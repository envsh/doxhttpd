set -e fail
set -x

export PKG_CONFIG_PATH=/opt/vcpkg/installed/x64-osx/lib/pkgconfig
QMAKE_EXTRA=""
if [ x"$1" == x"asan" ]; then
    QMAKE_EXTRA="CONFIG+=asan"
fi

# macports binary qt4 installation
QMAKE=${QMAKE:-/opt/local/libexec/qt4/bin/qmake}
mkdir -p build-osx4 && cd build-osx4
"$QMAKE" -r $QMAKE_EXTRA ../
make

# package
mkdir -p qltox.app/Contents/Resources
QT_TRANS=$("$QMAKE" -query QT_INSTALL_TRANSLATIONS)
cp -f "$QT_TRANS/qt_zh_CN.qm" "$QT_TRANS/qt_zh_TW.qm" qltox.app/Contents/Resources/ 2>/dev/null || true
tar zcf ../qltox-qt4-osx-x64.tar.gz qltox.app/
ls -lh ../qltox-*.gz
ls -lh qltox.app/Contents/MacOS/
