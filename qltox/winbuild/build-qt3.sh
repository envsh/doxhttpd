#!/usr/bin/env bash
# Build original Qt Windows Free Edition 3.3.5-4 (qtwin) for win32-g++ (x86/x64).
# Usage: build-qt3.sh <x86|x64> <dest-prefix> [xp-x86|xp-x64|win2k-x86]
# Override the source with QT3_SRC_URL / QT3_FALLBACK_URL.
set -euo pipefail

ARCH="${1:?usage: build-qt3.sh <x86|x64> <dest> [target-os]}"
DEST="${2:?usage: build-qt3.sh <x86|x64> <dest> [target-os]}"
OSS="${3:-xp-x86}"

QT3_SRC_URL="${QT3_SRC_URL:-https://downloads.sourceforge.net/project/qtwin/Unofficial%20Qtwin/qt-win-3.3.5-4/qt-win-free-3.3.5-4-src.tar.bz2}"
QT3_FALLBACK_URL="${QT3_FALLBACK_URL:-https://master.dl.sourceforge.net/project/qtwin/Unofficial%20Qtwin/qt-win-3.3.5-4/qt-win-free-3.3.5-4-src.tar.bz2}"

case "$ARCH" in
    x86) SPEC=win32-g++ ;;
    x64) SPEC=win32-g++-64 ;;
    *)
        echo "ERROR: unsupported arch '$ARCH' (x86|x64)" >&2
        exit 1
        ;;
esac

if [ -x "$DEST/bin/qmake.exe" ]; then
    echo "Qt3 already built: $DEST"
    exit 0
fi

SRC_ROOT="${SDK_ROOT:-$HOME/.win-sdk}/src"
mkdir -p "$SRC_ROOT" "$DEST"

# The 3.3.5-4 tarball unpacks into qt-3/, older ones into qt-win-free-*/,
# so locate the source by looking for configure.bat instead of guessing names.
qt3_src_dir() {
    for d in "$SRC_ROOT"/*; do
        if [ -d "$d" ] && [ -f "$d/configure.bat" ]; then
            printf '%s\n' "$d"
            return 0
        fi
    done
    return 1
}

SRC="$(qt3_src_dir || true)"

if [ -z "$SRC" ]; then
    TGZ="$SRC_ROOT/qt-win-free-3.3.5-4-src.tar.bz2"
    if [ ! -s "$TGZ" ]; then
        if ! curl -fL --retry 3 --retry-delay 2 --connect-timeout 20 "$QT3_SRC_URL" -o "$TGZ"; then
            curl -fL --retry 3 --retry-delay 2 --connect-timeout 20 "$QT3_FALLBACK_URL" -o "$TGZ"
        fi
    fi
    tar xjf "$TGZ" -C "$SRC_ROOT"
    SRC="$(qt3_src_dir || true)"
fi

if [ -z "$SRC" ] || [ ! -f "$SRC/configure.bat" ]; then
    echo "ERROR: Qt3 Windows source not found (no configure.bat under $SRC_ROOT)" >&2
    ls -l "$SRC_ROOT" >&2
    exit 1
fi

cd "$SRC"

# same bootstrap fix as Qt4: avoid ANSI GetComputerName with wchar_t buffer
if grep -q 'GetComputerName(name, &name_length)' qmake/project.cpp; then
    sed -i 's/GetComputerName(name, &name_length)/GetComputerNameW(name, \&name_length)/' qmake/project.cpp
fi

# Qt 3.3.5-4 ships win32-g++ (32-bit) only; derive the 64-bit spec from it.
if [ ! -d "mkspecs/$SPEC" ]; then
    cp -a mkspecs/win32-g++ "mkspecs/$SPEC"
fi
# configure aborts on -thread unless the spec defines QMAKE_LIBS_THREAD
# (AUTOTHREAD=never otherwise). MinGW threading is driven by -mthreads.
if ! grep -q '^QMAKE_LIBS_THREAD' "mkspecs/$SPEC/qmake.conf"; then
    echo "QMAKE_LIBS_THREAD =" >> "mkspecs/$SPEC/qmake.conf"
fi

MINOR=0
if [ "$ARCH" = "x64" ]; then
    MINOR=2
elif [ "$OSS" = "xp-x86" ]; then
    MINOR=1
fi
if ! grep -q "major-subsystem-version" "mkspecs/$SPEC/qmake.conf"; then
    echo "QMAKE_LFLAGS += -Wl,--major-subsystem-version,5 -Wl,--minor-subsystem-version,$MINOR -Wl,--major-os-version,5 -Wl,--minor-os-version,$MINOR" >> "mkspecs/$SPEC/qmake.conf"
fi

export QTDIR="$(cygpath -w "$SRC")"
export QMAKESPEC="$SPEC"

# configure.bat also forwards CFLAGS/CXXFLAGS to the qmake bootstrap as
# command-line variables; keep them compile-only (PE flags live in the mkspec).
export CFLAGS="-w -Wno-error -Wno-error=incompatible-pointer-types -Wno-error=implicit-function-declaration"
export CXXFLAGS="-std=gnu++98 -fpermissive -w -Wno-error"

# configure.bat aborts when sh.exe (MSYS/Cygwin) is on PATH; hand cmd.exe a
# PATH with only the MinGW toolchain and the Windows system directories.
MINGW_BIN_W="$(cygpath -w "$(dirname "$(command -v gcc)")")"
QT3_WIN_PATH="$MINGW_BIN_W;C:\\Windows\\system32;C:\\Windows"

cmd //c "set PATH=$QT3_WIN_PATH&& set QTDIR=$QTDIR&& set QMAKESPEC=$SPEC&& configure.bat -release -shared -thread -platform $SPEC -prefix $(cygpath -w "$DEST")"
mingw32-make -j1

if ! mingw32-make install; then
    echo "WARN: 'make install' unsupported, copying in-tree artifacts to $DEST" >&2
    for d in bin lib include plugins mkspecs translations; do
        if [ -d "$SRC/$d" ]; then
            mkdir -p "$DEST/$d"
            cp -a "$SRC/$d/." "$DEST/$d/"
        fi
    done
fi

if [ ! -x "$DEST/bin/qmake.exe" ]; then
    echo "ERROR: Qt3 install incomplete, no qmake.exe under $DEST" >&2
    find "$DEST" -maxdepth 2 >&2
    exit 1
fi

echo "Qt3 (Windows Free Edition 3.3.5-4) -> $DEST ($SPEC, PE 0x050$MINOR, target $OSS)"
