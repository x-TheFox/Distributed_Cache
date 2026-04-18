#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
DEV_RUNTIME_DIR="${DEV_RUNTIME_DIR:-${BUILD_DIR}/dev}"

CACHE_PID_FILE="${DEV_RUNTIME_DIR}/cache_server.pid"
DASHBOARD_PID_FILE="${DEV_RUNTIME_DIR}/dashboard.pid"
CACHE_BIN="${BUILD_DIR}/cache_server"

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

pid_process_group() {
  local pid="$1"
  ps -p "${pid}" -o pgid= 2>/dev/null | tr -d ' '
}

is_process_group_running() {
  local pgid="$1"
  if [[ -z "${pgid}" ]]; then
    return 1
  fi
  kill -0 -- "-${pgid}" 2>/dev/null
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

stop_process() {
  local label="$1"
  local pid_file="$2"
  local expected="$3"

  if [[ ! -f "${pid_file}" ]]; then
    echo "${label} not running (no pid file)."
    return
  fi

  local pid
  pid="$(read_pid_file "${pid_file}" || true)"
  if [[ -z "${pid}" ]]; then
    echo "${label} not running (invalid pid file)."
    return
  fi
  if ! kill -0 "${pid}" 2>/dev/null; then
    echo "${label} not running (stale pid ${pid})."
    rm -f "${pid_file}"
    return
  fi
  if ! pid_matches "${pid}" "${expected}"; then
    echo "${label} pid ${pid} does not match expected command; cleaning pid file."
    rm -f "${pid_file}"
    return
  fi

  local pgid
  pgid="$(pid_process_group "${pid}")"
  local stop_group=0
  if [[ "${pgid}" =~ ^[0-9]+$ && "${pgid}" == "${pid}" ]]; then
    stop_group=1
  fi
  if [[ "${stop_group}" -eq 1 ]]; then
    echo "Stopping ${label} (process group ${pgid})..."
    kill -- "-${pgid}"
  else
    echo "Stopping ${label} (pid ${pid})..."
    kill "${pid}"
  fi
  for _ in {1..40}; do
    if [[ "${stop_group}" -eq 1 ]]; then
      if ! is_process_group_running "${pgid}"; then
        rm -f "${pid_file}"
        echo "${label} stopped."
        return
      fi
    elif ! kill -0 "${pid}" 2>/dev/null; then
      rm -f "${pid_file}"
      echo "${label} stopped."
      return
    fi
    sleep 0.25
  done

  echo "${label} did not stop in time; killing."
  if [[ "${stop_group}" -eq 1 ]]; then
    kill -9 -- "-${pgid}" 2>/dev/null || true
  else
    kill -9 "${pid}" 2>/dev/null || true
  fi
  rm -f "${pid_file}"
}

stop_process "cache server" "${CACHE_PID_FILE}" "${CACHE_BIN}"
stop_process "dashboard" "${DASHBOARD_PID_FILE}" "${ROOT_DIR}/dashboard"

echo "Logs retained under ${DEV_RUNTIME_DIR}."
