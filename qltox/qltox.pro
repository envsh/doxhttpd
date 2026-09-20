TEMPLATE = app
TARGET = qltox
QT = core gui widgets
CONFIG += moc
CONFIG += sdk_no_version_check

VERSION = 0.1.0

GIT_COMMIT = $$system(git -C $$PWD rev-parse --short=7 HEAD 2>/dev/null)
isEmpty(GIT_COMMIT): GIT_COMMIT = unknown
DEFINES += GIT_COMMIT=$$GIT_COMMIT

GIT_DIRTY = $$system(git -C $$PWD status --porcelain 2>/dev/null)
!isEmpty(GIT_DIRTY): DEFINES += GIT_DIRTY

QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.7

SOURCES = main.cpp mainwindow.cpp storage.cpp restapi.cpp eventpoller.cpp \
             chatwidget.cpp chatview.cpp chatbuffer.cpp contactlist.cpp selfinfo.cpp \
             cJSON.c editinfodialog.cpp conferenceinvitedialog.cpp groupinvitedialog.cpp \
             friendinfodialog.cpp memberlistdialog.cpp logindialog.cpp \
             imageinputconfirm.cpp messageinput.cpp messageattribbar.cpp sound.c loadingbar.cpp \
              unknownparser.cpp photoviewer.cpp avatar_manager.cpp \
               media_shmem_cache.cpp \
               channel_db.cpp message_db.cpp \
                pending_db.cpp cache_db.cpp cache_fs.cpp \
                translate_util.cpp \
                 config.cpp \
                 msgdb_helper.cpp \
                 translation_cache.cpp \
                 statisticsdialog.cpp \
                 combinesearchwindow.cpp \
                 searchlistview.cpp \
                 sticker_db.cpp stickerpicker.cpp stickermanager.cpp \
                 seen_unknown.cpp \
                 ./vendor/barrust_bloom/bloom.c \
                 ../etapps/plugin_loader.cpp ../etapps/plugin_manager_dialog.cpp

HEADERS = mainwindow.h storage.h restapi.h eventpoller.h \
             chatwidget.h chatview.h chatbuffer.h contactlist.h selfinfo.h \
             editinfodialog.h conferenceinvitedialog.h groupinvitedialog.h \
             friendinfodialog.h memberlistdialog.h logindialog.h \
              imageinputconfirm.h messageinput.h messageattribbar.h sound.h loadingbar.h \
               unknownparser.h photoviewer.h avatar_manager.h \
               media_shmem_cache.h \
               channel_db.h message_db.h \
                pending_db.h cache_db.h cache_fs.h \
                translate_util.h \
                 config.h version.h \
                 msgdb_helper.h \
                 translation_cache.h \
                 statisticsdialog.h \
                 combinesearchwindow.h \
                 searchlistview.h \
                 sticker_db.h stickerpicker.h stickermanager.h \
                 seen_unknown.h \
                 ./vendor/bloom/bloom_filter.hpp \
                 ./vendor/barrust_bloom/bloom.h \
                 ../etapps/plugin_loader.h ../etapps/plugin_manager_dialog.h

# FORMS += MediumMsgEditor.ui — 已弃用。Qt3/Qt4 uic XML schema 不兼容，
# Qt4 uic 拒绝 <vbox>/<hbox> 等 Qt3 标签，且 Qt4 环境无 Qt3Support 库。
include(../qlcomp/qlite.pri)
include(../mdeditor/mdeditor.pri)

# 使 qltox/ 中的 #include "compat34.h" 能找到 qlcomp/
INCLUDEPATH += ../qlcomp
INCLUDEPATH += ../etapps
INCLUDEPATH += ./vendor/bloom ./vendor/barrust_bloom
macx {
    INCLUDEPATH += /opt/vcpkg/installed/x64-osx-dynamic/include
    LIBS += -L/opt/vcpkg/installed/x64-osx-dynamic/lib -Wl,-rpath,/opt/vcpkg/installed/x64-osx-dynamic/lib -lhjson
} else {
    INCLUDEPATH += /opt/vcpkg/installed/x64-linux-dynamic/include
    LIBS += -L/opt/vcpkg/installed/x64-linux-dynamic/lib -Wl,-rpath,/opt/vcpkg/installed/x64-linux-dynamic/lib -lhjson
}

# moc 处理
MOC_DIR = .
OBJECTS_DIR = .

# C++11 标准
QMAKE_CXXFLAGS += -O0 -std=c++11
QMAKE_CFLAGS += -O0 -std=c11
QMAKE_CXXFLAGS += -fstack-protector-strong
QMAKE_CFLAGS += -fstack-protector-strong

# asan double memory usage!!!
contains(CONFIG, asan) {
    QMAKE_CXXFLAGS += -fsanitize=address
    QMAKE_CFLAGS += -fsanitize=address
    QMAKE_LFLAGS += -fsanitize=address
}

!win32: QMAKE_LFLAGS += -rdynamic

# Qt 版本检测：必须用同一个 .pro 文件
# 方法：用 system() 调用 qmake -v，检测输出是否包含 "Qt 3" 或 "4."
# Qt3 qmake (1.07a) 不支持 QT_VERSION 变量
# Qt4 qmake 支持 QT_VERSION
# 检测逻辑：如果 QT_VERSION 不为空，则是 Qt4
!isEmpty(QT_VERSION) {
    # Qt4: QT_VERSION 不为空（如 "4.8.7"）
    message("Building for Qt4+ - QT3_BUILD not defined")
} else {
    # Qt3: QT_VERSION 为空
    message("Building for Qt3 - adding QT3_BUILD")
    DEFINES += QT3_BUILD
    # *nix/bsd/mac/wsl
    QMAKE_EXE = $$system(ps -p $PPID -o args= | head -1 | awk '{print $1}')
    QTDIR_AUTO = $$system(dirname $(dirname $$QMAKE_EXE))
    # message("qmake path: $$QMAKE_QMAKE , $$QMAKE_EXE, $$QTDIR_AUTO")
    isEmpty(QTDIR) {
		message("Auto QTDIR ... $$QTDIR_AUTO")
		# QMAKE_EXTRA_VARIABLES += QTDIR # not works
		QTDIR = $$QTDIR_AUTO # var not env
		# override Makefile prefixed with $(QTDIR)
		INCLUDEPATH += $$QTDIR/include
		QMAKE_INCDIR_QT    = $$QTDIR/include
		QMAKE_LIBDIR_QT    = $$QTDIR/lib
		QMAKE_MOC          = $$QTDIR/bin/moc
		QMAKE_UIC          = $$QTDIR/bin/uic
		QMAKE_QMAKE        = $$QMAKE_EXE
		QMAKE        = $$QMAKE_EXE # subdirs use this!!!
		QMAKE_LRELEASE     = $$QTDIR/bin/lrelease # no works
    }
}

# 包含路径
# INCLUDEPATH += /opt/qt338sh/include /opt/qt338sh/include/qt3

# PKG_CONFIG_PATH=/opt/vcpkg/installed/.../lib/pkgconfig qmake
QMAKE_CXXFLAGS += $$system(pkg-config --cflags openal 2>/dev/null)
INCLUDEPATH += $$system(pkg-config --cflags-only-I openal 2>/dev/null|sed 's/-I//g')
LIBS += $$system(pkg-config --libs openal 2>/dev/null)

# FreeType2 自动检测
FREETYPE_LIBS = $$system(pkg-config --libs freetype2 2>/dev/null)
!isEmpty(FREETYPE_LIBS) {
    QMAKE_CXXFLAGS += $$system(pkg-config --cflags freetype2 2>/dev/null)
    LIBS += -lcurl -lopenal -ldl $$FREETYPE_LIBS
    message("FreeType2: detected via pkg-config")
} else {
    INCLUDEPATH += /usr/include/freetype2
    LIBS += -lcurl -lfreetype
    message("FreeType2: pkg-config not found, using fallback paths")
}

# SQLite 依赖检测
SQLITE_CFLAGS = $$system(pkg-config --cflags sqlite3 2>/dev/null)
SQLITE_LIBS = $$system(pkg-config --libs sqlite3 2>/dev/null)
isEmpty(SQLITE_LIBS) {
    error("sqlite3 not found. Install libsqlite3-dev (Debian) or libsqlite3x-devel (RHEL)")
}
message("SQLite: $$SQLITE_LIBS")
INCLUDEPATH += $$SQLITE_CFLAGS
LIBS += $$SQLITE_LIBS
DEFINES += HAVE_SQLITE

# 库路径和链接
# QMAKE_LIBDIR_FLAGS += -L/opt/qt338sh/lib

# Emoji rendering with FreeType (color emoji fonts):
# Uncomment the next line to enable EMOJI_RENDER_QT34
DEFINES += EMOJI_RENDER_QT34
isEmpty(QT_VERSION) {
    # Qt3: 手动指定库
    # LIBS += $$(QMAKE_LIBS_QT_THREAD) -lqt-mt
} else {
    # Qt4: Qt 库由 qmake 自动处理
    message("Linking with Qt4 libraries")
}

# 截图模块 (screenshotmanager.cpp) 直接调用 XGetInputFocus 等 X11 API，
# 需要显式链接 -lX11。libQtGui.so 虽是共享库且有 DT_NEEDED 指向 libX11，
# 但 GNU ld --as-needed 策略下，链接器不会自动搜索传递依赖来解析应用
# 目标文件中的符号，因此必须显式 -lX11。
# — 添加 screenshot 模块前无此需要，因为没有代码直接调用 X11 API。
unix:!macx: LIBS += -lX11

# dlopen/dlsym/dlclose/dlerror for plugin loading
LIBS += -ldl

# 安装
target.path = /usr/local/bin
INSTALLS += target

# 翻译文件
translation.path = lang
translation.files = lang/*.json
INSTALLS += translation

# 应用图标
QMAKE_ICON = app_icon.icns
RC_FILE = app_icon.rc
