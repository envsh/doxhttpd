set -x

cd "$(dirname "$0")"

mkdir -p build-qt4 && cd build-qt4

qmake-qt4 ../config_demo.pro && make
qmake-qt4 ../frameless_demo.pro && make
qmake-qt4 ../dialoganim_demo.pro && make
