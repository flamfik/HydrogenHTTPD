#!/usr/bin/env bash
set -euo pipefail

URL="${1:-http://127.0.0.1:8080/}"
REQUESTS="${REQUESTS:-12000}"
CONCURRENCY="${CONCURRENCY:-100}"

if ! command -v ab >/dev/null 2>&1; then
  echo "ApacheBench (ab) is not installed." >&2
  exit 1
fi

exec ab -n "$REQUESTS" -c "$CONCURRENCY" "$URL"
