set -x

cd "$(dirname "$0")"

QTDIR=/opt/qt338sh
export QTDIR

mkdir -p build-qt3 && cd build-qt3

$QTDIR/bin/qmake ../config_demo.pro && make
$QTDIR/bin/qmake ../frameless_demo.pro && make
$QTDIR/bin/qmake ../dialoganim_demo.pro && make
