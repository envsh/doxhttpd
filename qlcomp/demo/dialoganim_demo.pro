TEMPLATE = app
TARGET = dialoganim_demo
CONFIG += console
CONFIG -= app_bundle

SOURCES = dialoganim_demo.cpp \
    ../DialogPopupAnimator.cpp \
    ../ThemeManager.cpp \
    ../LimeStyle.cpp \
    ../StyleParams.cpp \
    ../compat34.cpp

HEADERS = ../DialogPopupAnimator.h ../lambdaslot.h

INCLUDEPATH += ..

MOC_DIR = .
OBJECTS_DIR = .
QMAKE_CXXFLAGS += -std=c++11 -O0 -w

!isEmpty(QT_VERSION) {
    QT = core gui widgets
} else {
    DEFINES += QT3_BUILD
    INCLUDEPATH += $$(QTDIR)/include
    QMAKE_INCDIR_QT = $$(QTDIR)/include
    QMAKE_LIBDIR_QT = $$(QTDIR)/lib
}

unix:!macx: LIBS += -lX11
