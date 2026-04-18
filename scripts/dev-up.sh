#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
DEV_RUNTIME_DIR="${DEV_RUNTIME_DIR:-${BUILD_DIR}/dev}"
CACHE_BIN="${BUILD_DIR}/cache_server"

CACHE_RESP_PORT="${CACHE_RESP_PORT:-6379}"
CACHE_GRPC_PORT="${CACHE_GRPC_PORT:-50051}"
CACHE_METRICS_PORT="${CACHE_METRICS_PORT:-9100}"
CACHE_SHARD_COUNT="${CACHE_SHARD_COUNT:-64}"
CACHE_VIRTUAL_NODES="${CACHE_VIRTUAL_NODES:-128}"
CACHE_NODE_ID="${CACHE_NODE_ID:-local-dev}"
CACHE_SERVER_ARGS="${CACHE_SERVER_ARGS:-}"
CACHE_RESP_READY_TIMEOUT="${CACHE_RESP_READY_TIMEOUT:-10}"
CACHE_METRICS_READY_TIMEOUT="${CACHE_METRICS_READY_TIMEOUT:-10}"

DASHBOARD_PORT="${DASHBOARD_PORT:-3000}"
DASHBOARD_WS_URL="${DASHBOARD_WS_URL:-ws://localhost:8080/ws}"
DASHBOARD_SOURCE="${DASHBOARD_SOURCE:-LIVE}"
DASHBOARD_READY_TIMEOUT="${DASHBOARD_READY_TIMEOUT:-15}"

CACHE_PID_FILE="${DEV_RUNTIME_DIR}/cache_server.pid"
CACHE_LOG_FILE="${DEV_RUNTIME_DIR}/cache_server.log"
DASHBOARD_PID_FILE="${DEV_RUNTIME_DIR}/dashboard.pid"
DASHBOARD_LOG_FILE="${DEV_RUNTIME_DIR}/dashboard.log"
CACHE_STARTED=0
DASHBOARD_STARTED=0
CACHE_PID=""
DASHBOARD_PID=""

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing required tool: $1" >&2
    exit 1
  fi
}

is_running() {
  local pid="$1"
  if [[ -z "${pid}" ]]; then
    return 1
  fi
  kill -0 "${pid}" 2>/dev/null
}

pid_command_line() {
  local pid="$1"
  ps -p "${pid}" -o command= 2>/dev/null
}

pid_matches() {
  local pid="$1"
  local expected="$2"
  local cmdline
  cmdline="$(pid_command_line "${pid}")" || return 1
  if [[ -z "${cmdline}" ]]; then
    return 1
  fi
  [[ "${cmdline}" == *"${expected}"* ]]
}

read_pid_file() {
  local pid_file="$1"
  if [[ ! -f "${pid_file}" ]]; then
    return 1
  fi
  local pid
  pid="$(cat "${pid_file}")"
  if [[ ! "${pid}" =~ ^[0-9]+$ ]]; then
    rm -f "${pid_file}"
    return 1
  fi
  echo "${pid}"
}

validate_pid_file() {
  local label="$1"
  local pid_file="$2"
  local expected="$3"
  local pid
  pid="$(read_pid_file "${pid_file}")" || return 1
  if ! is_running "${pid}"; then
    echo "${label} not running (stale pid ${pid})."
    rm -f "${pid_file}"
    return 1
  fi
  if ! pid_matches "${pid}" "${expected}"; then
    echo "${label} pid ${pid} does not match expected command; cleaning pid file."
    rm -f "${pid_file}"
    return 1
  fi
  echo "${pid}"
}

stop_owned_process() {
  local label="$1"
  local pid="$2"
  local pid_file="$3"
  local expected="$4"

  if [[ -z "${pid}" ]]; then
    pid="$(read_pid_file "${pid_file}" || true)"
  fi
  if [[ -z "${pid}" ]]; then
    return
  fi
  if ! is_running "${pid}"; then
    rm -f "${pid_file}"
    return
  fi
  if ! pid_matches "${pid}" "${expected}"; then
    echo "${label} pid ${pid} does not match expected command; skipping stop."
    rm -f "${pid_file}"
    return
  fi
  echo "Stopping ${label} (pid ${pid})..."
  kill "${pid}"
  for _ in {1..40}; do
    if ! is_running "${pid}"; then
      rm -f "${pid_file}"
      echo "${label} stopped."
      return
    fi
    sleep 0.25
  done
  echo "${label} did not stop in time; killing."
  kill -9 "${pid}" 2>/dev/null || true
  rm -f "${pid_file}"
}

rollback_on_failure() {
  local exit_code=$?
  if [[ "${exit_code}" -eq 0 ]]; then
    return
  fi
  if [[ "${DASHBOARD_STARTED}" -eq 1 ]]; then
    stop_owned_process "dashboard" "${DASHBOARD_PID}" "${DASHBOARD_PID_FILE}" "${ROOT_DIR}/dashboard"
  fi
  if [[ "${CACHE_STARTED}" -eq 1 ]]; then
    stop_owned_process "cache server" "${CACHE_PID}" "${CACHE_PID_FILE}" "${CACHE_BIN}"
  fi
}

trap rollback_on_failure EXIT

wait_for_port() {
  local port="$1"
  local timeout="${2:-10}"
  python3 - "${port}" "${timeout}" <<'PY'
import socket
import sys
import time

port = int(sys.argv[1])
timeout = float(sys.argv[2])
deadline = time.time() + timeout

while time.time() < deadline:
    sock = socket.socket()
    sock.settimeout(0.5)
    try:
        sock.connect(("127.0.0.1", port))
        sock.close()
        sys.exit(0)
    except Exception:
        time.sleep(0.1)
    finally:
        sock.close()
sys.exit(1)
PY
}

wait_for_http() {
  local url="$1"
  local timeout="${2:-10}"
  local deadline=$((SECONDS + timeout))
  until curl -fsS --connect-timeout 1 --max-time 1 "${url}" >/dev/null 2>&1; do
    if (( SECONDS >= deadline )); then
      return 1
    fi
    sleep 0.2
  done
}

ensure_cache_server() {
  local build_needed=0
  if [[ ! -x "${CACHE_BIN}" ]]; then
    build_needed=1
  fi
  if [[ "${build_needed}" -eq 0 ]]; then
    if [[ "${ROOT_DIR}/CMakeLists.txt" -nt "${CACHE_BIN}" ]]; then
      build_needed=1
    elif find "${ROOT_DIR}/src" "${ROOT_DIR}/cmake" -type f -newer "${CACHE_BIN}" -print -quit | grep -q .; then
      build_needed=1
    fi
  fi
  if [[ "${build_needed}" -eq 1 ]]; then
    echo "Building cache_server..."
    cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
    cmake --build "${BUILD_DIR}" --target cache_server
  fi
}

ensure_dashboard_deps() {
  if [[ ! -d "${ROOT_DIR}/dashboard/node_modules" ]]; then
    echo "Installing dashboard dependencies..."
    (cd "${ROOT_DIR}/dashboard" && npm install)
  fi
}

start_cache_server() {
  local existing_pid
  existing_pid="$(validate_pid_file "Cache server" "${CACHE_PID_FILE}" "${CACHE_BIN}" || true)"
  if [[ -n "${existing_pid}" ]]; then
    echo "Cache server already running (pid ${existing_pid})."
    CACHE_PID="${existing_pid}"
    CACHE_STARTED=0
    return
  fi

  echo "Starting cache server..."
  "${CACHE_BIN}" \
    --node-id="${CACHE_NODE_ID}" \
    --shard-count="${CACHE_SHARD_COUNT}" \
    --virtual-nodes="${CACHE_VIRTUAL_NODES}" \
    --resp-port="${CACHE_RESP_PORT}" \
    --grpc-port="${CACHE_GRPC_PORT}" \
    --metrics-port="${CACHE_METRICS_PORT}" \
    ${CACHE_SERVER_ARGS} \
    >"${CACHE_LOG_FILE}" 2>&1 &
  CACHE_PID=$!
  CACHE_STARTED=1
  echo "${CACHE_PID}" > "${CACHE_PID_FILE}"
}

start_dashboard() {
  local existing_pid
  existing_pid="$(validate_pid_file "Dashboard" "${DASHBOARD_PID_FILE}" "${ROOT_DIR}/dashboard" || true)"
  if [[ -n "${existing_pid}" ]]; then
    echo "Dashboard already running (pid ${existing_pid})."
    DASHBOARD_PID="${existing_pid}"
    DASHBOARD_STARTED=0
    return
  fi

  echo "Starting dashboard..."
  NEXT_PUBLIC_CLUSTER_WS_URL="${DASHBOARD_WS_URL}" \
    NEXT_PUBLIC_DASHBOARD_SOURCE="${DASHBOARD_SOURCE}" \
    npm --prefix "${ROOT_DIR}/dashboard" run dev -- --port "${DASHBOARD_PORT}" \
    >"${DASHBOARD_LOG_FILE}" 2>&1 &
  DASHBOARD_PID=$!
  DASHBOARD_STARTED=1
  echo "${DASHBOARD_PID}" > "${DASHBOARD_PID_FILE}"
}

require_cmd cmake
require_cmd npm
require_cmd node
require_cmd curl
require_cmd python3

mkdir -p "${DEV_RUNTIME_DIR}"

ensure_cache_server
ensure_dashboard_deps

start_cache_server
start_dashboard

if [[ "${CACHE_RESP_PORT}" != "0" ]]; then
  if ! wait_for_port "${CACHE_RESP_PORT}" "${CACHE_RESP_READY_TIMEOUT}"; then
    echo "Cache RESP port ${CACHE_RESP_PORT} did not become ready." >&2
    exit 1
  fi
fi

if [[ "${CACHE_METRICS_PORT}" != "0" ]]; then
  if ! wait_for_http "http://localhost:${CACHE_METRICS_PORT}/metrics" "${CACHE_METRICS_READY_TIMEOUT}"; then
    echo "Cache metrics endpoint did not become ready." >&2
    exit 1
  fi
fi

if ! wait_for_http "http://localhost:${DASHBOARD_PORT}" "${DASHBOARD_READY_TIMEOUT}"; then
  echo "Dashboard did not become ready." >&2
  exit 1
fi

echo ""
echo "Cache server PID: $(cat "${CACHE_PID_FILE}")"
echo "Dashboard PID:   $(cat "${DASHBOARD_PID_FILE}")"
echo "Runtime dir:     ${DEV_RUNTIME_DIR}"
echo "RESP endpoint:   redis://localhost:${CACHE_RESP_PORT}"
echo "Metrics:         http://localhost:${CACHE_METRICS_PORT}/metrics"
echo "Dashboard:       http://localhost:${DASHBOARD_PORT}"
