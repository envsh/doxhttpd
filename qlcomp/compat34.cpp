#include "compat34.h"
#include <stdlib.h>
#include <unistd.h>

#include "compatcore34.cpp"

// ========== 实现所需的 Qt 头文件 ==========
#include <qwidget.h>         // QWidget (setWindowTitle, setToolTip, newBoxLayout)
#include <qdir.h>            // QDir (qGetHomePath)
#include <qdatetime.h>       // QDateTime (qFmtTime, getCurrentTime)
#include <qapplication.h>    // QApplication::clipboard (LabelDblClickFilter)
#include <qclipboard.h>      // QClipboard (LabelDblClickFilter)
#include <qlayout.h>         // QLayout (qSetLayoutMargin)
#include <qpixmap.h>         // QPixmap (qGrabWholeScreen)
#include <qpaintdevice.h>    // QPaintDevice::x11Display
#ifdef QT3_BUILD
#include <qtooltip.h>        // QToolTip::add
#include <qfileinfo.h>       // QFileInfo (qAppDir)
#include <qwidgetlist.h>     // QWidgetList (topLevelWidgets)
#include <qimage.h>          // QImage (qX11SetWmIcon)
#include <X11/Xlib.h>        // Display, XChangeProperty, XFlush
#include <X11/Xatom.h>       // XA_CARDINAL (qX11SetWmIcon)
// X11 的宏会污染 QEvent 的枚举值, 必须 undef
#ifdef KeyPress
#undef KeyPress
#endif
#ifdef KeyRelease
#undef KeyRelease
#endif
#include <qdesktopwidget.h>  // QApplication::desktop (TipLabel::placeTip)
#include <qtimer.h>          // QTimer::singleShot (showTempTooltip)
#else
#include <QWheelEvent>       // QWheelEvent (qWheelDeltaY/qWheelIsHorizontal)
#include <QToolTip>          // QToolTip::showText (showTempTooltip)
#include <QTimer>            // QTimer::singleShot (showTempTooltip)
#if QT_VERSION < 0x060000
#include <QDesktopWidget>    // QApplication::desktop (qPrimaryScreenGeometry 等)
#else
#include <QGuiApplication>   // QGuiApplication::primaryScreen/screens
#include <QScreen>           // QScreen (geometry/availableGeometry/grabWindow)
#endif
#ifdef Q_OS_LINUX
#include <X11/Xlib.h>        // XOpenDisplay (qX11Display)
#endif
#endif

void qSetWindowTitle(QWidget* w, const QString& title) {
#ifdef QT3_BUILD
    w->setCaption(title);
#else
    w->setWindowTitle(title);
#endif
}

Display* qX11Display() {
#ifdef Q_OS_LINUX
    static Display* dpy = XOpenDisplay(nullptr);
    return dpy;
#else
    return 0;   // 非 X11 平台不可用（调用方均在 Q_OS_LINUX 守卫内）
#endif
}

#ifdef QT3_BUILD
static void qX11SetWmIcon(const QPixmap& pm) {
    QImage img = pm.convertToImage();
    if (img.isNull()) { return; }

    const int iconSize = 32;
    QImage scaled = img.smoothScale(iconSize, iconSize);

    const int dataSize = 2 + iconSize * iconSize;
    unsigned long* data = new unsigned long[dataSize];
    data[0] = (unsigned long)iconSize;
    data[1] = (unsigned long)iconSize;
    for (int y = 0; y < iconSize; ++y) {
        for (int x = 0; x < iconSize; ++x) {
            QRgb px = scaled.pixel(x, y);
            data[2 + y * iconSize + x] =
                ((unsigned long)qAlpha(px) << 24) |
                ((unsigned long)qRed(px) << 16) |
                ((unsigned long)qGreen(px) << 8) |
                (unsigned long)qBlue(px);
        }
    }

    Display* dpy = QPaintDevice::x11AppDisplay();
    if (!dpy) { delete[] data; return; }
    Atom netWmIcon = XInternAtom(dpy, "_NET_WM_ICON", False);
    QWidgetList* topWidgets = qApp->topLevelWidgets();
    for (uint i = 0; i < uint(topWidgets->count()); ++i) {
        WId wid = topWidgets->at(i)->winId();
        if (wid) {
            XChangeProperty(dpy, wid, netWmIcon, XA_CARDINAL, 32,
                          PropModeReplace, (unsigned char*)data, dataSize);
        }
    }
    XFlush(dpy);
    delete[] data;
}
#endif

void qSetAppIcon(const char** xpm) {
    QPixmap pm(xpm);
#ifdef QT3_BUILD
    if (QWidget* mw = qApp->mainWidget()) {
        mw->setIcon(pm);
    } else {
        QWidgetList* list = qApp->topLevelWidgets();
        if (list->count() == 0) {
            qWarning("qSetAppIcon: no top-level widgets found");
            return;
        }
        for (uint i = 0; i < uint(list->count()); ++i) {
            list->at(i)->setIcon(pm);
        }
    }
    qX11SetWmIcon(pm);
#else
    qApp->setWindowIcon(QIcon(pm));
#endif
}

void qSetMargins(QBoxLayout* layout, int left, int top, int right, int bottom) {
#ifdef QT3_BUILD
    layout->setMargin(top);
#else
    layout->setContentsMargins(left, top, right, bottom);
#endif
}

#ifdef QT3_BUILD
void qSetChecked(QPushButton* btn, bool checked) {
    btn->setOn(checked);
}
void qSetChecked(QCheckBox* btn, bool checked) {
    btn->setChecked(checked);
}
#else
void qSetChecked(QAbstractButton* btn, bool checked) {
    btn->setChecked(checked);
}
#endif

void qSetCheckable(QPushButton* btn, bool checkable) {
#ifdef QT3_BUILD
    btn->setToggleButton(checkable);
#else
    btn->setCheckable(checkable);
#endif
}

void qSetToolTip(QWidget* w, const QString& tip) {
#ifdef QT3_BUILD
    QToolTip::add(w, tip);
#else
    w->setToolTip(tip);
#endif
}


void qInsertHtml(QTextEdit* edit, const QString& html) {
#ifdef QT3_BUILD
    edit->append(html);  // Qt3 的 append 支持 HTML
#else
    edit->insertHtml(html);
#endif
}

void qClearTextEdit(QTextEdit* edit) {
#ifdef QT3_BUILD
    edit->clear();
#else
    edit->clear();
#endif
}

QBoxLayout* qNewBoxLayout(QWidget* parent, QBoxLayout::Direction dir, int border, int autoresize) {
#ifdef QT3_BUILD
    return new QBoxLayout(parent, dir, border, autoresize, 0);
#else
    QBoxLayout* layout = new QBoxLayout(dir, parent);
    if (border != 0) { layout->setContentsMargins(border, border, border, border); }
    if (autoresize != -1) { layout->setSpacing(autoresize); }
    return layout;
#endif
}


void qStackSetCurrent(StackedWidget* stack, QWidget* page) {
#ifdef QT3_BUILD
    stack->raiseWidget(page);
#else
    stack->setCurrentWidget(page);
#endif
}

// Qt3: 双击 QLabel 复制文本（闪烁 ✓ 反馈）
#ifdef QT3_BUILD
class LabelDblClickFilter : public QObject {
    QLabel* m_label;
    QString m_origText;
    int m_timerId;
public:
    LabelDblClickFilter(QObject* parent)
        : QObject(parent), m_label(nullptr), m_timerId(-1) {}

    bool eventFilter(QObject* obj, QEvent* event) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            QLabel* label = static_cast<QLabel*>(obj);
            if (label && !label->text().isEmpty()) {
                QApplication::clipboard()->setText(label->text());
                if (m_timerId == -1) {
                    m_label = label;
                    m_origText = label->text();
                    label->setText(m_origText + " ✓");
                    m_timerId = startTimer(1000);
                }
                return true;
            }
        }
        return QObject::eventFilter(obj, event);
    }

    void timerEvent(QTimerEvent* event) {
        if (event->timerId() == m_timerId) {
            killTimer(m_timerId);
            m_timerId = -1;
            if (m_label) {
                m_label->setText(m_origText);
                m_label = nullptr;
            }
        }
        QObject::timerEvent(event);
    }
};
#endif

void qSetLabelSelectable(QLabel* label) {
#ifndef QT3_BUILD
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
#else
    label->installEventFilter(new LabelDblClickFilter(label));
#endif
}

// ========== 活动窗口检测 ==========
bool qIsAppActive(const QWidget* widget) {
    if (widget) {
#ifdef QT3_BUILD
        return qApp->activeWindow() == widget->topLevelWidget();
#else
        return widget->isActiveWindow();
#endif
    }
    return qApp->activeWindow() != 0;
}

// Qt5 起 hasPendingEvents() 已 obsolete，Qt6 移除，且无等价 API
// （事件队列属 Qt 内部细节，官方不提供查询接口）。
// Qt6 下一律视作「无待处理事件」——调用方只用于首次绘制时序日志。
bool qHasPendingEvents() {
#if QT_VERSION < 0x060000
    return qApp->hasPendingEvents();
#else
    return false;
#endif
}

// ========== 布局外边距 ==========
// Qt6 移除了 QLayout::setMargin()，统一改用 contentsMargins（Qt4.6+ 已有）。
void qSetLayoutMargin(QLayout* layout, int m) {
    if (!layout) {
        return;
    }
#if QT_VERSION >= 0x040600
    layout->setContentsMargins(m, m, m, m);
#else
    layout->setMargin(m);
#endif
}

// ========== 滚轮事件 ==========
// Qt6 移除了 QWheelEvent::delta() / orientation()。
// delta() 在 Qt5 中的实现就是 angleDelta().y()（同为 1/8 度单位），
// orientation() 是比较 pixelDelta() 两轴绝对值 —— 这里逐位复刻，非近似。
int qWheelDeltaY(QWheelEvent* e) {
#if QT_VERSION < 0x060000
    return e->delta();
#else
    return e->angleDelta().y();
#endif
}

bool qWheelIsHorizontal(QWheelEvent* e) {
#if QT_VERSION < 0x060000
    return e->orientation() == Qt::Horizontal;
#else
    QPoint pd = e->pixelDelta();
    return qAbs(pd.x()) > qAbs(pd.y());
#endif
}

// Qt5 起 QWheelEvent::x()/y() 已 deprecated，Qt6 移除。
// pos()（Qt6 为 position()）同为控件局部坐标，逐位等价。
QPoint qWheelPos(QWheelEvent* e) {
#if QT_VERSION < 0x060000
    return e->pos();
#else
    return e->position().toPoint();
#endif
}

// ========== 屏幕几何 / 整屏截图 ==========
// Qt6 移除了 QApplication::desktop() 与 QDesktopWidget，改用 QScreen。
QRect qPrimaryScreenGeometry() {
#if QT_VERSION < 0x060000
    return QApplication::desktop()->screenGeometry();
#else
    return QGuiApplication::primaryScreen()->geometry();
#endif
}

QRect qPrimaryAvailableGeometry() {
#if QT_VERSION < 0x060000
    return QApplication::desktop()->availableGeometry(-1);
#else
    return QGuiApplication::primaryScreen()->availableGeometry();
#endif
}

// QDesktopWidget::rect() 是所有屏幕矩形的并集，Qt6 无直接等价 API，手工求并集
QRect qVirtualDesktopRect() {
#if QT_VERSION < 0x060000
    return QApplication::desktop()->rect();
#else
    QRect r;
    const QList<QScreen*> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        r = r.isNull() ? screens[i]->geometry() : r.united(screens[i]->geometry());
    }
    return r;
#endif
}

QRect qScreenGeometryFor(QWidget* w) {
#if QT_VERSION < 0x060000
    return QApplication::desktop()->screenGeometry(QApplication::desktop()->screenNumber(w));
#else
    return w->screen()->geometry();
#endif
}

// QPixmap::grabWindow 在 Qt6 中已移除，等价能力迁到 QScreen::grabWindow
QPixmap qGrabWholeScreen() {
#if QT_VERSION < 0x060000
    QDesktopWidget* desktop = QApplication::desktop();
    QRect r = desktop->rect();
    return QPixmap::grabWindow(desktop->winId(), r.x(), r.y(), r.width(), r.height());
#else
    QScreen* screen = QGuiApplication::primaryScreen();
    QRect r = screen->geometry();
    return screen->grabWindow(0, r.x(), r.y(), r.width(), r.height());
#endif
}

// ========== URL 打开兼容 ==========
// qOpenUrl 实现见 compatcore34.cpp（兼容到 kBuiltinSources / 跨项目复用）

#ifdef QT3_BUILD
namespace {

class TipLabel : public QLabel {
public:
    static TipLabel* instance;

    TipLabel()
        : QLabel(0, "tip_label",
            Qt::WStyle_Customize | Qt::WStyle_Tool
            | Qt::WStyle_NoBorder | Qt::WStyle_StaysOnTop)
        , m_widget(0)
        , m_expireTimerId(0)
        , m_hideTimerId(0)
    {
        delete instance;
        instance = this;

        QPalette pal;
        pal.setColor(QPalette::Active, QColorGroup::Background, QColor(255, 255, 220));
        pal.setColor(QPalette::Disabled, QColorGroup::Background, QColor(255, 255, 220));
        pal.setColor(QPalette::Inactive, QColorGroup::Background, QColor(255, 255, 220));
        pal.setColor(QPalette::Active, QColorGroup::Foreground, Qt::black);
        pal.setColor(QPalette::Disabled, QColorGroup::Foreground, Qt::black);
        pal.setColor(QPalette::Inactive, QColorGroup::Foreground, Qt::black);
        setPalette(pal);
        setMargin(3);
        setFrameStyle(QFrame::Box | QFrame::Plain);
        setLineWidth(1);
        setAlignment(AlignLeft);
        setIndent(1);
        qApp->installEventFilter(this);
        setMouseTracking(true);
    }

    ~TipLabel() {
        instance = 0;
    }

    void showTip(QWidget* parent, const QRect& btnRect,
                 const QString& text, int timeoutMs) {
        if (text.isEmpty()) {
            hideTip();
            return;
        }

        QRect screenRect(parent->mapToGlobal(btnRect.topLeft()), btnRect.size());

        bool changed = (text != QLabel::text())
                    || (parent != m_widget)
                    || (m_btnRect != btnRect);

        if (changed) {
            setText(text);
            m_widget = parent;
            m_btnRect = btnRect;
            m_trackingRect = screenRect;
            adjustSize();
            placeTip();
            if (!isVisible())
                show();
            restartExpireTimer(timeoutMs);
        } else {
            m_trackingRect = screenRect;
            restartExpireTimer(timeoutMs);
        }
    }

    void hideTipImmediately() {
        if (m_hideTimerId) { killTimer(m_hideTimerId); m_hideTimerId = 0; }
        if (m_expireTimerId) { killTimer(m_expireTimerId); m_expireTimerId = 0; }
        close();
        m_btnRect = QRect();
        m_trackingRect = QRect();
        m_widget = 0;
    }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        switch (event->type()) {
        case QEvent::MouseMove: {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (!m_btnRect.isNull() && !m_trackingRect.contains(me->globalPos()))
                hideTip();
            break;
        }
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::Wheel:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            hideTipImmediately();
            break;
        case QEvent::Leave:
            hideTip();
            break;
        default:
            break;
        }
        return false;
    }

    void timerEvent(QTimerEvent* e) override {
        if (e->timerId() == m_hideTimerId) {
            killTimer(m_hideTimerId);
            m_hideTimerId = 0;
            hideTipImmediately();
        } else if (e->timerId() == m_expireTimerId) {
            killTimer(m_expireTimerId);
            m_expireTimerId = 0;
            hideTip();
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (!m_btnRect.isNull()) {
            QPoint pos = e->globalPos();
            if (m_widget)
                pos = m_widget->mapFromGlobal(pos);
            if (!m_btnRect.contains(pos))
                hideTip();
        }
        QLabel::mouseMoveEvent(e);
    }

private:
    void placeTip() {
        QPoint center = m_widget->mapToGlobal(m_btnRect.center());
        QPoint pos2(center.x() - width() / 2, center.y() - height() - 4);

        QRect screen = QApplication::desktop()->screenGeometry(center);
        if (pos2.x() < screen.x())
            pos2.setX(screen.x() + 2);
        if (pos2.x() + width() > screen.x() + screen.width())
            pos2.setX(screen.x() + screen.width() - width() - 2);
        if (pos2.y() < screen.y())
            pos2.setY(screen.y() + 2);

        move(pos2);
    }

    void restartExpireTimer(int timeoutMs) {
        if (m_expireTimerId) killTimer(m_expireTimerId);
        if (m_hideTimerId) { killTimer(m_hideTimerId); m_hideTimerId = 0; }
        m_expireTimerId = startTimer(timeoutMs > 0 ? timeoutMs : 3000);
    }

    void hideTip() {
        if (!m_hideTimerId)
            m_hideTimerId = startTimer(300);
    }

    QWidget* m_widget;
    QRect m_btnRect;
    QRect m_trackingRect;
    int m_expireTimerId;
    int m_hideTimerId;
};

TipLabel* TipLabel::instance = 0;

} // namespace
#endif

void showTempTooltip(QWidget* parent, const QRect& btnRect,
                     const QString& text, int timeoutMs) {
#ifdef QT3_BUILD
    if (text.isEmpty()) {
        if (TipLabel::instance)
            TipLabel::instance->hideTipImmediately();
        return;
    }
    if (!TipLabel::instance)
        new TipLabel();
    TipLabel::instance->showTip(parent, btnRect, text, timeoutMs);
#else
    QToolTip::showText(parent->mapToGlobal(btnRect.center()), text, parent, btnRect);
#endif
}

void qActivateWindow(QWidget* w) {
    w->show();
    w->raise();
    w->setFocus();
#ifdef QT3_BUILD
    w->setActiveWindow();
#else
    w->activateWindow();
#endif
}

