#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: build_portable.sh <build-directory> <output-directory>" >&2
  exit 2
fi

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$(cd "$1" && pwd)"
OUT="$(mkdir -p "$2" && cd "$2" && pwd)"
STAGE="$ROOT/.portable-stage"
NAME="HydrogenHttpd-1.9.0-Linux-$(uname -m)"

rm -rf "$STAGE"
mkdir -p "$STAGE/$NAME"
DESTDIR="$STAGE/$NAME" cmake --install "$BUILD" --prefix /

tar -C "$STAGE" -czf "$OUT/$NAME.tar.gz" "$NAME"
sha256sum "$OUT/$NAME.tar.gz" > "$OUT/$NAME.tar.gz.sha256"
rm -rf "$STAGE"

echo "$OUT/$NAME.tar.gz"
