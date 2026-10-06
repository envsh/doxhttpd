#!/bin/bash
set -e
cd "$(dirname "$0")"

QTDIR=/opt/qt338sh
FT2=$(pkg-config --cflags freetype2)
CXX="g++ -std=c++11 -g -O0 -w -DQT3_BUILD -DEMOJI_RENDER_QT34"
QINC="-I. -I$QTDIR/include -I../qlcomp $FT2"

echo "=== 编译测试 ==="
$CXX -c test_main.cpp -o test_main.o
$CXX -c test_md5.cpp -I../qlcomp -o test_md5.o
$CXX -c test_emojiutil.cpp $QINC -o test_emojiutil.o
$CXX -c test_translate_util.cpp $QINC -o test_translate_util.o
$CXX -c test_compat34_time.cpp $QINC -o test_compat34_time.o
$CXX -c test_aes.cpp -I../qlcomp -o test_aes.o

echo "=== 链接测试 ==="
FT2_LIB=$(pkg-config --libs freetype2)
$CXX test_main.o test_md5.o test_emojiutil.o test_translate_util.o test_compat34_time.o test_aes.o \
    -o run_tests \
    -L$QTDIR/lib \
    -L/opt/vcpkg/installed/x64-linux-dynamic/lib \
    -Wl,-rpath,/opt/vcpkg/installed/x64-linux-dynamic/lib \
    -lqt-mt -lsqlite3 -lcurl -lX11 -luuid -lhjson -ldl $FT2_LIB

echo "=== 运行测试 ==="
LD_LIBRARY_PATH=$QTDIR/lib:/opt/vcpkg/installed/x64-linux-dynamic/lib ./run_tests
