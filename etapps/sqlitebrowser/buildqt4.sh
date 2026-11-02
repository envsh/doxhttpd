#!/bin/bash
set -x

if [ x"$QTDIR" == x"" ]; then
    QTDIR=/opt/qt487sh
    export QTDIR
fi

cd "$(dirname "$0")"
mkdir -p build-qt4 && cd build-qt4
/opt/qt487sh/bin/qmake ../plugin.pro

if [ x"$1" == x"c" ]; then
    make clean
fi
make -j1

ls -la ../../../qltox/plugins4/uiapps/libsqlitebrowser_ui.so

### for exe

cd .. && mkdir -p build-exe4 && cd build-exe4
/opt/qt487sh/bin/qmake ../exe.pro
if [ x"$1" == x"c" ]; then
    make clean
fi
make -j1
ls -lha sqlitebrowser_ui
