#!/bin/bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "Usage: build_pkg.sh <staging-root> <output-directory> [architecture]" >&2
    exit 2
fi

STAGE="$(cd "$1" && pwd)"
OUT="$(mkdir -p "$2" && cd "$2" && pwd)"
ARCH="${3:-$(uname -m)}"
VERSION="1.9.0"
COMPONENT="$OUT/HydrogenHttpd-${VERSION}-${ARCH}-component.pkg"
FINAL="$OUT/HydrogenHttpd-${VERSION}-macOS-${ARCH}.pkg"

pkgbuild \
    --root "$STAGE" \
    --identifier com.hydrogenhttpd.server \
    --version "$VERSION" \
    --scripts "$(cd "$(dirname "$0")/pkg-scripts" && pwd)" \
    --install-location / \
    "$COMPONENT"

productbuild \
    --package "$COMPONENT" \
    "$FINAL"

rm -f "$COMPONENT"
shasum -a 256 "$FINAL" > "$FINAL.sha256"
echo "$FINAL"
