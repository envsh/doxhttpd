#ifndef GLOBALUIUTIL_H
#define GLOBALUIUTIL_H

class QString;

// sticonShowStatusMessage 的 iconType 取值（数值对齐 SystemTrayIcon::MessageIcon：1/2/3）
enum SticonIcon {
	SticonInfo     = 1,
	SticonWarning  = 2,
	SticonCritical = 3
};

// SharedStatusBar 显示状态栏消息的全局快捷函数（定义在 main.cpp）。
// 未开启 SharedStatusBar 时静默忽略，调用方无需 include sharedstatusbar.h。
extern void stbarShowStatusMessage(const QString &msg, int timeout = 0);

// 新增：托盘气泡通知（独立功能，不触碰 SharedStatusBar。定义在 mainwindow.cpp）。
extern void sticonShowStatusMessage(const QString &msg, SticonIcon iconType, int timeout);

#endif // GLOBALUIUTIL_H