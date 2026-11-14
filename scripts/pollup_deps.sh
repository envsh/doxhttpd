
set -euo pipefail
set -x

pwd

git clone --depth 1 https://github.com/qxcomp/qxcomp.git ../qxcomp

git clone --depth 1 https://github.com/qxcomp/cxvendor.git ../cxvendor

# git clone https://github.com/qxcomp/droidcomp.git ../droidcomp

git clone --depth 1 https://github.com/kitech/touse.git ../touse

git clone --depth 1 https://github.com/kitech/anystik.git ../anystik

# vendor 是指向 ../cxvendor/vendor/ 的软链接：缺了会在 qmake 里静默跳过 gumbo.pri，
# 表现为 "gumbo.h not found" 或链接 undefined reference，所以这里直接失败
test -d ../cxvendor/vendor/gumbo || {
    echo "FATAL: ../cxvendor/vendor/gumbo missing"; exit 1; }
test -f ../cxvendor/vendor/gumbo/gumbo.h || {
    echo "FATAL: gumbo.h missing in vendor tree"; exit 1; }
test -d ../anystik/qlappcom || {
    echo "FATAL: ../anystik/qlappcom missing"; exit 1; }
test -d ../qxcomp/qlcomp || {
    echo "FATAL: ../qxcomp/qlcomp missing"; exit 1; }
