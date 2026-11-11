#!/bin/bash
set -x

if [ x"$QTDIR" == x"" ]; then
    QTDIR=/opt/qt338sh
    export QTDIR
fi

cd "$(dirname "$0")"
mkdir -p build-qt3 && cd build-qt3
$QTDIR/bin/qmake -makefile ../plugin.pro

# generate moc for Qt3
if [ -f $QTDIR/bin/moc ]; then
    $QTDIR/bin/moc ../logic.h -o logic.moc 2>/dev/null || true
fi

if [ x"$1" == x"c" ]; then
    make clean
fi
make -j1

ls -la ../../../qltox/plugins/uiapps/libsupervisor_mgr_ui.so

### for exe

cd .. && mkdir -p build-exe && cd build-exe 
$QTDIR/bin/qmake -makefile ../exe.pro
if [ -f $QTDIR/bin/moc ]; then
    $QTDIR/bin/moc ../logic.h -o logic.moc 2>/dev/null || true
fi
if [ x"$1" == x"c" ]; then
    make clean
fi
make -j1
ls -lha supervisor_mgr_ui

