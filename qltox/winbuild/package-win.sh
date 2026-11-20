#!/usr/bin/env bash
# Collect the qltox binary, Qt runtime and language files, then assert a
# required-file manifest. Missing runtime is a hard failure (no silent success).
# Usage: package-win.sh <target> <xp-x86|xp-x64|win2k-x86>
set -euo pipefail

TARGET="${1:?usage: package-win.sh <target> <target-os>}"
OSS="${2:?usage: package-win.sh <target> <target-os>}"

DIST="dist/$TARGET"

case "$TARGET" in
    win32-qt4)       BUILD_DIR="qltox/build-win32-qt4-xp" ;;
    win64-qt4)       BUILD_DIR="qltox/build-win64-qt4-xp" ;;
    win32-qt3)       BUILD_DIR="qltox/build-win32-qt3-xp" ;;
    win32-qt3-win2k) BUILD_DIR="qltox/build-win32-qt3-win2k" ;;
    win64-qt3)       BUILD_DIR="qltox/build-win64-qt3-xp" ;;
    *)
        echo "ERROR: unknown target $TARGET" >&2
        exit 1
        ;;
esac

SRC="$BUILD_DIR"

mkdir -p "$DIST"

BIN="$SRC/qltox.exe"
if [ ! -f "$BIN" ]; then
    BIN="$SRC/release/qltox.exe"
fi
if [ ! -f "$BIN" ]; then
    echo "ERROR: qltox.exe not found under $SRC" >&2
    ls -l "$SRC" >&2
    exit 1
fi
cp -f "$BIN" "$DIST/"

# Qt runtime DLLs (Qt4: bin/*.dll, Qt3: bin or lib).
for d in "${QTDIR:-}/bin" "${QTDIR:-}/lib"; do
    if [ ! -d "$d" ]; then
        continue
    fi
    for f in "$d"/*.dll; do
        if [ -f "$f" ]; then
            cp -fu "$f" "$DIST/"
        fi
    done
done

# Qt plugins (platform/style/imageformats/codecs).
for p in "${QTDIR:-}/plugins" "${QTDIR:-}/lib/qt4/plugins" "${QTDIR:-}/lib/qt3/plugins"; do
    if [ -d "$p" ]; then
        mkdir -p "$DIST/plugins"
        cp -a "$p/." "$DIST/plugins/"
        break
    fi
done

# Language files.
if [ -d qltox/lang ]; then
    mkdir -p "$DIST/lang"
    for f in qltox/lang/*.json; do
        if [ -f "$f" ]; then
            cp -f "$f" "$DIST/lang/"
        fi
    done
fi

# Mandatory runtime manifest.
REQ="qltox.exe"
case "$TARGET" in
    win32-qt4|win64-qt4)
        REQ="$REQ QtCore4.dll QtGui4.dll"
        ;;
    win32-qt3|win32-qt3-win2k|win64-qt3)
        REQ="$REQ qt-mt3.dll"
        ;;
esac

MISSING=""
for f in $REQ; do
    if [ ! -f "$DIST/$f" ]; then
        MISSING="$MISSING $f"
    fi
done
if [ -n "$MISSING" ]; then
    echo "ERROR: missing required runtime files:$MISSING" >&2
    ls -l "$DIST" >&2
    exit 1
fi

echo "== target=$TARGET ($OSS) =="
echo "git_sha=${GITHUB_SHA:-local}"
echo "git_ref=${GITHUB_REF_NAME:-local}"
echo "built_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "qtdir=${QTDIR:-}"
echo "binaries_sha256:"
( cd "$DIST" && find . -type f \( -iname '*.exe' -o -iname '*.dll' \) -print0 \
    | sort -z | xargs -0 sha256sum )

echo "packaged $TARGET ($(ls -1 "$DIST" | wc -l) entries)"
