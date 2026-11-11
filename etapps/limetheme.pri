# limetheme.pri — etapps 独立 exe 的默认深色主题
# 复用 qltox 的 LimeStyle / ThemeManager（qlcomp），用法：
#   include(../limetheme.pri)
#   QApplication app(argc, argv);
#   app.setStyle(new LimeStyle);
#   ThemeManager::setStyle("qtFusion", true);

LIMETHEME_DIR = $$PWD/../qlcomp

INCLUDEPATH += $$LIMETHEME_DIR

SOURCES += $$LIMETHEME_DIR/StyleParams.cpp \
           $$LIMETHEME_DIR/LimeStyle.cpp \
           $$LIMETHEME_DIR/LimeScrollBar.cpp \
           $$LIMETHEME_DIR/ThemeManager.cpp

HEADERS += $$LIMETHEME_DIR/StyleParams.h \
           $$LIMETHEME_DIR/LimeStyle.h \
           $$LIMETHEME_DIR/LimeScrollBar.h \
           $$LIMETHEME_DIR/ThemeManager.h
