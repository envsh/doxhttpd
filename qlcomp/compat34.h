#ifndef COMPAT34_H
#define COMPAT34_H

#include "compatcore34.h"

// ========== 平台特有头文件 ==========
#ifdef QT3_BUILD
#include <qpushbt.h>         // QPushButton
#include <qmenubar.h>        // QMenuBar
#include <qpopupmenu.h>      // QPopupMenu
#include <qwidgetstack.h>    // QWidgetStack
#include <qptrlist.h>        // Qt3 原生 QPtrList<T>
#include <qobjectlist.h>     // Qt3 QObjectList（完整定义，用于 children()）
#if QT_VERSION < 0x060000
#include <qtextcodec.h>      // QTextCodec
#endif
#else
#include <qpushbutton.h>     // QPushButton
#include <QMenuBar>          // QMenuBar
#include <qmenu.h>           // QMenu
#include <qstackedwidget.h>  // QStackedWidget
#include <qstyleoption.h>    // QStyleOption* (Qt4 only)
#if QT_VERSION < 0x060000
#include <QTextCodec>        // QTextCodec（Qt6 移除，未装 Qt5Compat）
#endif
#include <qevent.h>          // QMouseEvent 等完整定义
#endif

// ========== 核心 Qt 头文件（被 qlite.pri 继承，无法前向声明） ==========
#include <qlayout.h>         // QBoxLayout, QBoxLayout::Direction
#include <qwidget.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qtextedit.h>
#include <qcheckbox.h>
#include <qfontmetrics.h>

// ========== 前向声明（仅用于指针/引用参数） ==========
class QAbstractButton;
struct _XDisplay;
typedef _XDisplay Display;         // 与 X11/Xlib.h 同型，重复 typedef 合法

// ========== Widgets 兼容函数声明 ==========

void qSetWindowTitle(QWidget* w, const QString& title);
void qSetAppIcon(const char** xpm);
Display* qX11Display();

inline int qFontWidth(const QFontMetrics& fm, const QString& text) {
#if QT_VERSION >= 0x050b00
    return fm.horizontalAdvance(text);   // Qt5.11+ / Qt6
#else
    return fm.width(text);                // Qt3/Qt4
#endif
}
#if QT_VERSION < 0x050500
/// qInfo 实现见 qthooks.cpp (与 qDebug/qWarning/qFatal 的 hook 放一起)
void qInfo(const char* fmt, ...);
#endif
void qSetMargins(QBoxLayout* layout, int left, int top, int right, int bottom);

#ifdef QT3_BUILD
void qSetChecked(QPushButton* btn, bool checked);
void qSetChecked(QCheckBox* btn, bool checked);
#else
void qSetChecked(QAbstractButton* btn, bool checked);
#endif

void qSetCheckable(QPushButton* btn, bool checkable);
void qSetToolTip(QWidget* w, const QString& tip);
void qInsertHtml(QTextEdit* edit, const QString& html);
void qClearTextEdit(QTextEdit* edit);
QBoxLayout* qNewBoxLayout(QWidget* parent, QBoxLayout::Direction dir, int border = 0, int autoresize = -1);

// ========== StackedWidget 兼容 ==========
#ifdef QT3_BUILD
typedef QWidgetStack StackedWidget;
#else
typedef QStackedWidget StackedWidget;
#endif

void qStackSetCurrent(StackedWidget* stack, QWidget* page);
void qSetLabelSelectable(QLabel* label);

// ========== Scroll 兼容 ==========
#ifdef QT3_BUILD
#include <qscrollview.h>     // QScrollView
typedef QScrollView ScrollArea;
#else
#include <qscrollarea.h>     // QScrollArea
typedef QScrollArea ScrollArea;
#endif

// ========== 活动窗口检测 ==========
bool qIsAppActive(const QWidget* widget = 0);

// Qt6 移除了 QApplication::hasPendingEvents()，统一走 compat 包装
bool qHasPendingEvents();

// ========== 布局外边距（Qt6 移除了 QLayout::setMargin）==========
void qSetLayoutMargin(QLayout* layout, int m);

// ========== 滚轮事件（Qt6 移除了 QWheelEvent::delta/orientation）==========
class QWheelEvent;
int  qWheelDeltaY(QWheelEvent* e);
bool qWheelIsHorizontal(QWheelEvent* e);
QPoint qWheelPos(QWheelEvent* e);

// ========== enter 事件参数类型 ==========
// Qt6 把 QWidget::enterEvent 的参数从 QEvent* 收窄为 QEnterEvent*；
// leaveEvent 到 Qt6 仍是 QEvent*，不需处理。
// 定义在类体外（而非类内 #ifdef），避免 Qt3 moc 同时展开两个分支。
#if QT_VERSION >= 0x060000
typedef QEnterEvent qEnterEventType;
#else
typedef QEvent     qEnterEventType;
#endif

// ========== 屏幕几何（Qt6 移除了 QApplication::desktop/QDesktopWidget）==========
class QPixmap;
QRect qPrimaryScreenGeometry();       // 主屏 geometry
QRect qPrimaryAvailableGeometry();    // 主屏可用区（不含任务栏等）
QRect qVirtualDesktopRect();          // 所有屏幕矩形并集
QRect qScreenGeometryFor(QWidget* w); // w 所在屏幕 geometry
QPixmap qGrabWholeScreen();           // 整屏截图

void showTempTooltip(QWidget* parent, const QRect& btnRect, const QString& text, int timeoutMs = 3000);

void qActivateWindow(QWidget* w);

#endif  // COMPAT34_H
