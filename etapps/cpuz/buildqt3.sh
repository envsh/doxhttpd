#!/bin/bash
set -x

if [ x"$QTDIR" == x"" ]; then
    QTDIR=/opt/qt338sh
    export QTDIR
fi

cd "$(dirname "$0")"
mkdir -p build-qt3 && cd build-qt3
$QTDIR/bin/qmake -makefile ../plugin.pro

if [ x"$1" == x"c" ]; then
    make clean
fi
make -j1

ls -la ../../../qltox/plugins/uiapps/libcpuz_ui.so

### for exe

cd .. && mkdir -p build-exe && cd build-exe
$QTDIR/bin/qmake -makefile ../exe.pro
if [ x"$1" == x"c" ]; then
    make clean
fi
make -j1
ls -lha cpuz_ui
