#!/usr/bin/env bash
set -euo pipefail

if [ ! -x build/hydrogen_httpd ]; then
  echo "Binary not found. Run scripts/build.sh first."
  exit 1
fi

./build/hydrogen_httpd server.conf
