#!/usr/bin/env bash
# Build Qt 4.8.7 for win32-g++ (x86|x64) from source.
# Usage: build-qt4.sh <x86|x64> <dest-prefix> [xp-x86|xp-x64]
# Never downloads a prebuilt SDK; the source tarball is cached under $SDK_ROOT/src.
set -euo pipefail

ARCH="${1:?usage: build-qt4.sh <x86|x64> <dest> [target-os]}"
DEST="${2:?usage: build-qt4.sh <x86|x64> <dest> [target-os]}"
OSS="${3:-xp-x86}"

QT4_GIT_URL="${QT4_GIT_URL:-https://github.com/qt/qt.git}"
QT4_GIT_REF="${QT4_GIT_REF:-v4.8.7}"

if [ -x "$DEST/bin/qmake.exe" ]; then
    echo "Qt4 already built: $DEST"
    exit 0
fi

SRC_ROOT="${SDK_ROOT:-$HOME/.win-sdk}/src"
mkdir -p "$SRC_ROOT" "$DEST"

SRC="$SRC_ROOT/qt-4.8.7"
if [ ! -d "$SRC" ]; then
    git clone --depth 1 --branch "$QT4_GIT_REF" "$QT4_GIT_URL" "$SRC"
fi
if [ ! -x "$SRC/configure" ]; then
    echo "ERROR: unexpected Qt4 source layout at $SRC (no configure script)" >&2
    ls -l "$SRC" >&2
    exit 1
fi

cd "$SRC"

# qmake bootstrap ignores the UNICODE define from CFLAGS/CXXFLAGS and then
# passes wchar_t* to GetComputerName(); pin the wide API explicitly.
if grep -q 'GetComputerName(name, &name_length)' qmake/project.cpp; then
    sed -i 's/GetComputerName(name, &name_length)/GetComputerNameW(name, \&name_length)/' qmake/project.cpp
fi

SPEC=win32-g++
MINOR=1
if [ "$ARCH" = x64 ]; then
    SPEC=win32-g++-64
    MINOR=2
    if [ ! -d "mkspecs/$SPEC" ]; then
        cp -a mkspecs/win32-g++ "mkspecs/$SPEC"
    fi
fi

# Qt 4.8.7 predates C++11; modern GCC defaults to C++17 and rejects its code.
if ! grep -q "std=gnu++98" "mkspecs/$SPEC/qmake.conf"; then
    echo "QMAKE_CXXFLAGS += -std=gnu++98" >> "mkspecs/$SPEC/qmake.conf"
fi
# Qt 4.8.7 assumes a UNICODE build; the stock spec only sets UNICODE and the
# tools/ subprojects may not inherit it reliably. Add both, in DEFINES (that is
# what Qt4 Makefiles turn into -D flags) and in the compiler flags.
if ! grep -q "_UNICODE" "mkspecs/$SPEC/qmake.conf"; then
    echo "DEFINES += UNICODE _UNICODE" >> "mkspecs/$SPEC/qmake.conf"
    echo "QMAKE_CFLAGS += -DUNICODE -D_UNICODE" >> "mkspecs/$SPEC/qmake.conf"
    echo "QMAKE_CXXFLAGS += -DUNICODE -D_UNICODE" >> "mkspecs/$SPEC/qmake.conf"
fi
# Qt4 decides the 'win32' scope from Option::target_mode, which is taken from
# TARGET_PLATFORM (qmake/project.cpp), not from QMAKE_PLATFORM (that is Qt5).
if ! grep -q "^TARGET_PLATFORM" "mkspecs/$SPEC/qmake.conf"; then
    echo "TARGET_PLATFORM = win32" >> "mkspecs/$SPEC/qmake.conf"
fi
# GCC 14 promotes several C diagnostics to errors by default (C23 behaviour),
# which Qt 4.8.7's C sources do not satisfy.
if ! grep -q "incompatible-pointer-types" "mkspecs/$SPEC/qmake.conf"; then
    echo "QMAKE_CXXFLAGS += -fpermissive -Wno-error" >> "mkspecs/$SPEC/qmake.conf"
    echo "QMAKE_CFLAGS += -Wno-error -Wno-error=incompatible-pointer-types -Wno-error=implicit-function-declaration" >> "mkspecs/$SPEC/qmake.conf"
fi
if ! grep -q "major-subsystem-version" "mkspecs/$SPEC/qmake.conf"; then
    echo "QMAKE_LFLAGS += -Wl,--major-subsystem-version,5 -Wl,--minor-subsystem-version,$MINOR -Wl,--major-os-version,5 -Wl,--minor-os-version,$MINOR" >> "mkspecs/$SPEC/qmake.conf"
fi

# configure forwards CFLAGS/CXXFLAGS to the qmake bootstrap as command-line
# variables, which replaces its built-in flags (-DUNICODE, -DQT_NODLL, ...).
# Keep UNICODE here, otherwise GetComputerName() maps to the ANSI variant.
export CFLAGS="-DUNICODE -D_UNICODE -Wno-error=incompatible-pointer-types -Wno-error=implicit-function-declaration"
export CXXFLAGS="-DUNICODE -D_UNICODE -std=gnu++98 -fpermissive -w -Wno-error"

./configure -opensource -confirm-license -release -shared \
    -platform "$SPEC" -prefix "$DEST" \
    -no-webkit -no-script -no-scripttools -no-declarative -no-qt3support \
    -nomake demos -nomake examples -nomake docs
make -j1
make install

if [ ! -x "$DEST/bin/qmake.exe" ]; then
    echo "ERROR: Qt4 install incomplete, no qmake.exe under $DEST" >&2
    find "$DEST" -maxdepth 2 >&2
    exit 1
fi

echo "Qt4 -> $DEST (mkspec $SPEC, PE 0x050$MINOR, target $OSS)"
