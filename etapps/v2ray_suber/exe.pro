TEMPLATE = app
TARGET = v2ray_suber_ui
CONFIG += plugin qt
macx: CONFIG -= app_bundle
QT = core gui widgets

INCLUDEPATH += ../../qlcomp ../

# FreeType2 自动检测
FREETYPE_LIBS = $$system(pkg-config --libs freetype2 2>/dev/null)
FREETYPE_CFLAGS = $$system(pkg-config --cflags freetype2 2>/dev/null)
!isEmpty(FREETYPE_LIBS) {
    QMAKE_CXXFLAGS += $$FREETYPE_CFLAGS
    INCLUDEPATH += $$system(pkg-config --cflags-only-I freetype2 2>/dev/null|sed 's/-I//g')
    LIBS += $$FREETYPE_LIBS
    message("FreeType2: detected via pkg-config")
} else {
    macx {
        INCLUDEPATH += /opt/local/include/freetype2
        LIBS += -L/opt/local/lib
    } else {
        INCLUDEPATH += /usr/include/freetype2
    }
    LIBS += -lfreetype
    message("FreeType2: pkg-config not found, using fallback paths")
}

# HarfBuzz 自动检测
HARFBUZZ_LIBS = $$system(pkg-config --libs harfbuzz 2>/dev/null)
HARFBUZZ_CFLAGS = $$system(pkg-config --cflags harfbuzz 2>/dev/null)
!isEmpty(HARFBUZZ_LIBS) {
    QMAKE_CXXFLAGS += $$HARFBUZZ_CFLAGS
    INCLUDEPATH += $$system(pkg-config --cflags-only-I harfbuzz 2>/dev/null|sed 's/-I//g')
    LIBS += $$HARFBUZZ_LIBS
    message("HarfBuzz: detected via pkg-config")
} else {
    macx {
        INCLUDEPATH += /opt/local/include/harfbuzz
        LIBS += -L/opt/local/lib
    } else {
        INCLUDEPATH += /usr/include/harfbuzz
    }
    LIBS += -lharfbuzz
    message("HarfBuzz: pkg-config not found, using fallback paths")
}

SOURCES = exe.cpp main.cpp mainwin.cpp logic.cpp v2flag.cpp ../../qlcomp/compat34.cpp ../../qlcomp/qthooks.cpp ../../qlcomp/appsetup.cpp ../../qlcomp/cJSON.c ../../qlcomp/qsettings34.cpp
HEADERS = ../../qlcomp/appsetup.h logic.h v2flag.h ../../qlcomp/lambdaslot.h ../../qlcomp/qsettings34.h
# DESTDIR = ../../../qltox/plugins/uiapps

MOC_DIR = .
OBJECTS_DIR = .
QMAKE_CXXFLAGS += -std=c++11

!isEmpty(QT_VERSION) {
    message("Building for Qt4+")
} else {
    message("Building for Qt3")
    DEFINES += QT3_BUILD
    QMAKE_EXE = $$system(ps -p $PPID -o args= | head -1 | awk '{print $1}')
    QTDIR_AUTO = $$system(dirname $(dirname $$QMAKE_EXE))
    isEmpty(QTDIR) {
        QTDIR = $$QTDIR_AUTO
        INCLUDEPATH += $$QTDIR/include
        QMAKE_INCDIR_QT = $$QTDIR/include
        QMAKE_LIBDIR_QT = $$QTDIR/lib
        QMAKE_MOC = $$QTDIR/bin/moc
        QMAKE_UIC = $$QTDIR/bin/uic
        QMAKE_QMAKE = $$QMAKE_EXE
        QMAKE = $$QMAKE_EXE
    }
}
