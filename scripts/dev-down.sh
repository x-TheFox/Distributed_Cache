#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
DEV_RUNTIME_DIR="${DEV_RUNTIME_DIR:-${BUILD_DIR}/dev}"

CACHE_PID_FILE="${DEV_RUNTIME_DIR}/cache_server.pid"
DASHBOARD_PID_FILE="${DEV_RUNTIME_DIR}/dashboard.pid"

stop_process() {
  local label="$1"
  local pid_file="$2"

  if [[ ! -f "${pid_file}" ]]; then
    echo "${label} not running (no pid file)."
    return
  fi

  local pid
  pid="$(cat "${pid_file}")"
  if ! kill -0 "${pid}" 2>/dev/null; then
    echo "${label} not running (stale pid ${pid})."
    rm -f "${pid_file}"
    return
  fi

  echo "Stopping ${label} (pid ${pid})..."
  kill "${pid}"
  for _ in {1..40}; do
    if ! kill -0 "${pid}" 2>/dev/null; then
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

stop_process "cache server" "${CACHE_PID_FILE}"
stop_process "dashboard" "${DASHBOARD_PID_FILE}"

echo "Logs retained under ${DEV_RUNTIME_DIR}."
