TEMPLATE = lib
TARGET = supervisor_mgr_ui
CONFIG += plugin qt
QT = core gui widgets

INCLUDEPATH += ../../qlcomp ../
SOURCES = main.cpp mainwin.cpp logic.cpp ../../qlcomp/compat34.cpp ../../qlcomp/qthooks.cpp ../../qlcomp/cJSON.c
HEADERS = logic.h
DESTDIR = ../../../qltox/plugins/uiapps

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
