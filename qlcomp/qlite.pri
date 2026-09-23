
# usage: include(/path/to/qlite.pri)

# todo self depends setting

# LimeStyle theme system
LIME_STYLE_H = $$PWD/StyleParams.h $$PWD/LimeStyle.h $$PWD/LimeScrollBar.h
LIME_STYLE_CPP = $$PWD/StyleParams.cpp $$PWD/LimeStyle.cpp $$PWD/LimeScrollBar.cpp

QTCOMP_CPP = $$PWD/limelog.cpp $$PWD/appsetup.cpp $$PWD/compat34.cpp $$PWD/translator.cpp \
			$$PWD/emojiutil.cpp $$PWD/emojiwidgets.cpp $$PWD/emojiitems.cpp \
			$$PWD/emoji_picker.cpp $$PWD/ThemeManager.cpp $$PWD/placeholderlineedit.cpp $$PWD/searchline.cpp $$PWD/toastwidget.cpp \
			$$PWD/FramelessHelper.cpp $$PWD/CustomTitleBar.cpp $$PWD/ConfigDialog.cpp \
			$$PWD/sharedstatusbar.cpp \
			$$PWD/qthooks.cpp \
			$$PWD/jsonview.cpp \
			$$PWD/floatingpill.cpp \
			$$PWD/generic_slot.cpp \
			$$PWD/md5.c $$PWD/identicon.cpp \
			$$PWD/qcrc64.cpp \
			$$PWD/hjson_wrap.cpp \
			$$PWD/desktoplyrics.cpp \
			$$PWD/assertf.cpp \
			$$PWD/screenshotmanager.cpp \
			$$PWD/screenshotoverlay.cpp \
			$$PWD/screenshotpreview.cpp \
			$$PWD/sleepblocker.cpp \
			$$PWD/systemtrayicon.cpp \
			$$PWD/tagutil.cpp

QTCOMP_HDR = $$PWD/limelog.h $$PWD/appsetup.h $$PWD/appsetup_c.h $$PWD/translator.h $$PWD/compat34.h \
			$$PWD/emojiutil.h $$PWD/emojiwidgets.h $$PWD/emojiitems.h \
			$$PWD/emoji_picker.h $$PWD/ThemeManager.h $$PWD/placeholderlineedit.h $$PWD/searchline.h $$PWD/toastwidget.h \
			$$PWD/FramelessHelper.h $$PWD/CustomTitleBar.h $$PWD/EmbeddedMenuBar.h $$PWD/ConfigDialog.h \
			$$PWD/generic_slot_base.h \
			$$PWD/lambdaslot.h \
			$$PWD/sharedstatusbar.h \
			$$PWD/jsonview.h \
			$$PWD/floatingpill.h \
			$$PWD/md5.h $$PWD/identicon.h \
			$$PWD/qcrc64.h \
			$$PWD/hjson_wrap.h \
			$$PWD/desktoplyrics.h \
			$$PWD/assertf.h \
			$$PWD/screenshotmanager.h \
			$$PWD/screenshotoverlay.h \
			$$PWD/screenshotpreview.h \
			$$PWD/sleepblocker.h \
			$$PWD/systemtrayicon.h \
			$$PWD/tagutil.h

# SystemTrayIcon 平台依赖：Qt3 分支用 Psi TrayIcon（qpopupmenu.h 等 Qt3 专属 API，
# 不能进 Qt4 构建）；Qt4 分支用原生 QSystemTrayIcon，无需平台文件。
# 注意：qltox.pro 在 include(qlite.pri) 之后才 DEFINES += QT3_BUILD，
# 所以这里不能用 contains(DEFINES, QT3_BUILD)，必须用 isEmpty(QT_VERSION) 自判。
# 此块必须在下方 HEADERS/SOURCES += 之前——$$ 展开发生在赋值行，后追加不生效。
isEmpty(QT_VERSION) {
    QTCOMP_HDR += $$PWD/trayicon.h
    QTCOMP_CPP += $$PWD/trayicon.cpp
    unix:!macx { QTCOMP_CPP += $$PWD/trayicon_x11.cpp }
    win32      { QTCOMP_CPP += $$PWD/trayicon_win.cpp }
}

HEADERS += $$LIME_STYLE_H $$QTCOMP_HDR
SOURCES += $$LIME_STYLE_CPP $$QTCOMP_CPP

# SleepBlocker 平台依赖
unix:!macx: LIBS += -lXss -lX11
macx: LIBS += -framework IOKit

