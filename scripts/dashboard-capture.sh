#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DASHBOARD_DIR="${ROOT_DIR}/dashboard"

DASHBOARD_PORT="${DASHBOARD_PORT:-3000}"
DASHBOARD_URL="${DASHBOARD_URL:-http://localhost:${DASHBOARD_PORT}}"
CAPTURE_WAIT_MS="${CAPTURE_WAIT_MS:-2000}"
OUTPUT_PATH="${DASHBOARD_SCREENSHOT:-${ROOT_DIR}/docs/generated/screenshots/dashboard.png}"

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing required tool: $1" >&2
    exit 1
  fi
}

wait_for_http() {
  local url="$1"
  local timeout="${2:-10}"
  local deadline=$((SECONDS + timeout))
  until curl -fsS "${url}" >/dev/null 2>&1; do
    if (( SECONDS >= deadline )); then
      return 1
    fi
    sleep 0.2
  done
}

require_cmd npm
require_cmd npx
require_cmd curl

if [[ ! -d "${DASHBOARD_DIR}/node_modules" ]]; then
  echo "Installing dashboard dependencies..."
  (cd "${DASHBOARD_DIR}" && npm install)
fi

if ! wait_for_http "${DASHBOARD_URL}" 15; then
  echo "Dashboard not reachable at ${DASHBOARD_URL}. Run scripts/dev-up.sh first." >&2
  exit 1
fi

mkdir -p "$(dirname "${OUTPUT_PATH}")"

(
  cd "${DASHBOARD_DIR}"
  npx playwright install chromium
  npx playwright screenshot "${DASHBOARD_URL}" "${OUTPUT_PATH}" \
    --wait-for-timeout="${CAPTURE_WAIT_MS}" \
    --full-page
)

echo "Saved dashboard screenshot to ${OUTPUT_PATH}"
