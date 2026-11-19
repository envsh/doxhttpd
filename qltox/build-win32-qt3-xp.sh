#!/usr/bin/env bash
# Build qltox for Windows XP SP3 x86 (0x0501) with Qt3 (tde-qt338sh Windows port).
# BLOCKED until Windows x86 tde-qt338sh toolchain + XP-compatible deps are available.
set -euo pipefail
set -x

: "${QTDIR:?QTDIR must point to Windows x86 tde-qt338sh XP-compatible SDK}"
: "${XP_DEPS_PREFIX:=}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build-win32-qt3-xp"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

export PATH="$QTDIR/bin:${XP_DEPS_PREFIX:+$XP_DEPS_PREFIX/bin:}$PATH"
export PKG_CONFIG_PATH="${XP_DEPS_PREFIX:+$XP_DEPS_PREFIX/lib/pkgconfig:}${PKG_CONFIG_PATH:-}"

QMAKE_BIN="${XP_TARGET_QMAKE:-$QTDIR/bin/qmake}"
if [ ! -x "$QMAKE_BIN" ] && [ -x "$QTDIR/bin/qmake.exe" ]; then
  QMAKE_BIN="$QTDIR/bin/qmake.exe"
elif [ ! -x "$QMAKE_BIN" ]; then
  QMAKE_BIN="$(command -v qmake 2>/dev/null || echo "$QTDIR/bin/qmake")"
fi
"$QMAKE_BIN" -makefile CONFIG+=xpcompat XP_TARGET_ARCH=x86 XP_TARGET_OS=winxp ../qltox.pro

if [[ "${1:-}" == "c" ]]; then
  make clean
fi
# Do not parallelize builds.
make -j1

if [[ -f release/qltox.exe && ! -f qltox.exe ]]; then
  cp -f release/qltox.exe ./qltox.exe
fi
ls -lh qltox.exe release/qltox.exe 2>/dev/null || true
