set -x

# for qt3
if [ x"$QTDIR" == x"" ]; then
    QTDIR=/opt/qt338sh
    export QTDIR
fi
mkdir -p build-qt3 && cd build-qt3
QMAKE_EXTRA=""
if [ x"$1" == x"asan" ]; then
    QMAKE_EXTRA="CONFIG+=asan"
fi
/opt/qt338sh/bin/qmake -makefile $QMAKE_EXTRA ../qltox.pro
if [ x"$1" == x"c" ]; then
	make clean
fi
make

ls -lh q*tox

# qmake-qt4 && make
