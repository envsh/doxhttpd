#!/usr/bin/env bash
# Build hjson-cpp (headers + libhjson) for the active MSYS2/MinGW toolchain.
# Usage: build-hjson.sh <dest-prefix>
# Idempotent: exits immediately when <dest>/lib/libhjson.a already exists.
set -euo pipefail

DEST="${1:?usage: build-hjson.sh <dest-prefix>}"

if [ -f "$DEST/lib/libhjson.a" ]; then
    echo "hjson already built: $DEST"
    exit 0
fi

mkdir -p "$DEST"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "cmake: $(cmake --version | head -n 1)"

git clone --depth 1 https://github.com/hjson/hjson-cpp.git "$TMP/src"

cmake -S "$TMP/src" -B "$TMP/b" -G "Unix Makefiles" \
    -DCMAKE_INSTALL_PREFIX="$DEST" \
    -DCMAKE_BUILD_TYPE=Release \
    -DHJSON_ENABLE_INSTALL=ON
cmake --build "$TMP/b" --parallel 1
cmake --install "$TMP/b"

if [ ! -f "$DEST/lib/libhjson.a" ]; then
    echo "ERROR: hjson build did not produce $DEST/lib/libhjson.a" >&2
    find "$DEST" -maxdepth 3 >&2
    exit 1
fi

echo "hjson -> $DEST"
