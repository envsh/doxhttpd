#ifndef GLOBALUIUTIL_H
#define GLOBALUIUTIL_H

class QString;

// SharedStatusBar 显示状态栏消息的全局快捷函数（定义在 main.cpp）。
// 未开启 SharedStatusBar 时静默忽略，调用方无需 include sharedstatusbar.h。
extern void stbarShowStatusMessage(const QString &msg, int timeout = 0);

#endif // GLOBALUIUTIL_H