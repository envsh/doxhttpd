#include "sharedstatusbar.h"
#include "compatcore34.h"
#ifdef QT3_BUILD
#include <qpainter.h>
#include <qpen.h>
#include <qcursor.h>
#include <qdatetime.h>
#include <qpopupmenu.h>
#include <qtooltip.h>
#ifdef Q_OS_LINUX
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif
#else
#include <QPainter>
#include <QDateTime>
#include <QMenu>
#ifdef Q_OS_LINUX
#include <QX11Info>
#include <X11/Xlib.h>
#endif
#endif

SharedStatusBar *SharedStatusBar::s_instance = nullptr;

static const int GRIP_SIZE = 16;
static const int HISTORY_MAX = 30;
static const int HISTORY_TEXT_MAX = 140;
static const int HISTORY_MENU_MAX_PX = 560;

static QString elideTextForMenu(const QFontMetrics &fm, const QString &s, int maxPx)
{
    if (fm.width(s) <= maxPx) { return s; }
    const QString ell = QString(QChar(0x2026));
    QString r = s;
    while (!r.isEmpty() && fm.width(r) > maxPx - fm.width(ell)) {
        r = r.left(r.length() - 1);
    }
    if (r.isEmpty()) { r = s.left(1); }
    return r + ell;
}

SharedStatusBar::SharedStatusBar()
#ifdef QT3_BUILD
    : QWidget(nullptr, "sharedstatusbar",
              WStyle_Customize | WStyle_Tool | WStyle_NoBorder)
#else
    : QWidget(nullptr,
              Qt::Tool | Qt::FramelessWindowHint)
#endif
    , m_bar(nullptr)
    , m_activeWindow(nullptr)
    , m_dragging(false)
    , m_repositioning(false)
    , m_historyBtn(nullptr)
#ifdef QT3_BUILD
    , m_debounceTimer(nullptr)
    , m_pendingHide(false)
#endif
{
#ifdef QT3_BUILD
    m_bar = new QStatusBar(this, "innerbar");
    m_debounceTimer = new QTimer(this, "debounce");
    connect(m_debounceTimer, SIGNAL(timeout()),
            this, SLOT(onDebounceTimeout()));
#else
    m_bar = new QStatusBar(this);
    m_bar->setObjectName("innerbar");
    connect(qApp, SIGNAL(focusChanged(QWidget*,QWidget*)),
            this, SLOT(onFocusChanged(QWidget*,QWidget*)));
#endif
    m_bar->setSizeGripEnabled(false);
    setMouseTracking(true);

    // 历史消息按钮：下三角箭头在 LimeStyle / 当前字体下渲染不可见，
    // 现在在 "W" 前加空心下三角 ▽ 试验是否可渲染；
    // 点击弹出 30 条消息历史菜单
    m_historyBtn = new QToolButton(this);
    m_historyBtn->setText(qFromUtf8("▽ W"));
    m_historyBtn->setAutoRaise(true);
    // 按文本实际宽度自适应（△ 是测试符号，字体差异大，用固定宽易裁切）
    int btnW = m_historyBtn->fontMetrics().width(m_historyBtn->text()) + 12;
    m_historyBtn->setFixedSize(btnW, 18);
#ifdef QT3_BUILD
    QToolTip::add(m_historyBtn, qFromUtf8("查看消息历史"));
#else
    m_historyBtn->setToolTip(qFromUtf8("查看消息历史"));
#endif
    m_historyBtn->hide();
    connect(m_historyBtn, SIGNAL(clicked()),
            this, SLOT(onHistoryClicked()));

#ifdef QT3_BUILD
    QBoxLayout *lay = new QBoxLayout(this, QBoxLayout::LeftToRight, 0, 0);
#else
    QHBoxLayout *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
#endif
    lay->addWidget(m_historyBtn);
    lay->addWidget(m_bar, 1);

    qApp->installEventFilter(this);
}

SharedStatusBar::~SharedStatusBar()
{
    qApp->removeEventFilter(this);
#ifdef QT3_BUILD
    delete m_debounceTimer;
#endif
}

SharedStatusBar *SharedStatusBar::instance()
{
    if (!s_instance) {
        s_instance = new SharedStatusBar();
        QWidget *aw = qApp->activeWindow();
        if (aw) {
            s_instance->m_activeWindow = aw;
            s_instance->reposition();
        } else {
            // Qt4 构造期间 activeWindow() 为 NULL，延迟重试
            QTimer::singleShot(0, s_instance, SLOT(retrack()));
        }
    }
    return s_instance;
}

bool SharedStatusBar::instanceExists()
{
    return s_instance != nullptr;
}

void SharedStatusBar::showMessage(const QString &msg, int timeout)
{
    if (!msg.isEmpty()) {
        StatusHistoryEntry e;
        e.timeStr = QTime::currentTime().toString("HH:mm:ss");
        e.text = msg;
        if (e.text.length() > HISTORY_TEXT_MAX) {
            e.text = e.text.left(HISTORY_TEXT_MAX - 1) + QString(QChar(0x2026));
        }
        m_history.prepend(e);
        while (m_history.size() > HISTORY_MAX) {
#ifdef QT3_BUILD
            m_history.remove(m_history.at(m_history.size() - 1));
#else
            m_history.removeAt(m_history.size() - 1);
#endif
        }
        m_historyBtn->show();
    }
#ifdef QT3_BUILD
    m_bar->message(msg, timeout);
#else
    m_bar->showMessage(msg, timeout);
#endif
}

void SharedStatusBar::onHistoryClicked()
{
    if (m_history.isEmpty()) {
        m_historyBtn->hide();
        return;
    }
    showHistoryMenu();
}

void SharedStatusBar::showHistoryMenu()
{
#ifdef QT3_BUILD
    QPopupMenu *menu = new QPopupMenu(this);
#else
    QMenu *menu = new QMenu(this);
#endif
    QFontMetrics fm(menu->font());
    for (int i = 0; i < m_history.size(); ++i) {
        const StatusHistoryEntry &e = m_history[i];
        QString item = e.timeStr + QString(" ") + e.text;
        item = elideTextForMenu(fm, item, HISTORY_MENU_MAX_PX);
#ifdef QT3_BUILD
        menu->insertItem(item);
#else
        menu->addAction(item);
#endif
    }
    connect(menu, SIGNAL(aboutToHide()), menu, SLOT(deleteLater()));
    QPoint pop = m_historyBtn->mapToGlobal(
        QPoint(0, m_historyBtn->height()));
    menu->popup(pop);
}

void SharedStatusBar::clearMessage()
{
#ifdef QT3_BUILD
    m_bar->clear();
#else
    m_bar->clearMessage();
#endif
}

void SharedStatusBar::addWidget(QWidget *w, int stretch)
{
    m_bar->addWidget(w, stretch);
}

void SharedStatusBar::addPermanentWidget(QWidget *w, int stretch)
{
#ifdef QT3_BUILD
    m_bar->addWidget(w, stretch, true);
#else
    m_bar->addPermanentWidget(w, stretch);
#endif
}

void SharedStatusBar::removeWidget(QWidget *w)
{
    m_bar->removeWidget(w);
}

bool SharedStatusBar::eventFilter(QObject *watched, QEvent *event)
{
    if (m_repositioning) return false;

    // Show：widget 变为可见时触发（兜底，不受 Qt::Tool 限制）
    if (event->type() == QEvent::Show) {
        if (!watched->isWidgetType()) return false;
        QWidget *tw = static_cast<QWidget*>(watched)->topLevelWidget();
        if (!tw || tw == this) return false;
#ifdef QT3_BUILD
        if (tw->inherits("QLabel")) return false;
        if (tw->inherits("QPopupMenu")) return false;
        if (tw->inherits("DesktopLyrics")) return false;
        if (tw->inherits("ScreenshotRegionSelector")) return false;
        if (tw->inherits("ScreenshotPreviewDialog")) return false;
#else
        // 注意：不能用 windowFlags() & (Qt::ToolTip | Qt::Popup) 判断，
        // 因为 Qt::Popup(0x09)、Qt::ToolTip(0x0d)、Qt::Dialog(0x03) 都含
        // Qt::Window(0x01) 位，位AND对所有窗口返回非零。须用 Mask 提取类型后比较。
        if (((tw->windowFlags() & Qt::WindowType_Mask) == Qt::ToolTip) || ((tw->windowFlags() & Qt::WindowType_Mask) == Qt::Popup)) return false;
        if (tw->inherits("DesktopLyrics")) return false;
        if (tw->inherits("ScreenshotRegionSelector")) return false;
        if (tw->inherits("ScreenshotPreviewDialog")) return false;
#endif
        if (tw->width() < 350) {
            return false;
        }
        if (minimumWidth() > tw->width()) {
            return false;
        }
        m_activeWindow = tw;
        installEventFiltersOnMyselfTopLevelWidgets();
        reposition();
        return false;
    }

#ifndef QT3_BUILD
    // ApplicationActivate：应用被激活时触发（watched 是 qApp，兜底）
    if (event->type() == QEvent::ApplicationActivate) {
        installEventFiltersOnMyselfTopLevelWidgets();
        QWidget *aw = qApp->activeWindow();
        if (!aw || aw == this) return false;
        if (aw->inherits("DesktopLyrics")) return false;
        if (aw->inherits("ScreenshotRegionSelector")) return false;
        if (aw->inherits("ScreenshotPreviewDialog")) return false;
        if (aw->width() < 350) return false;
        if (minimumWidth() > aw->width()) return false;
        m_activeWindow = aw;
        reposition();
        return false;
    }
#endif

    if (event->type() == QEvent::WindowActivate) {
        // 同应用内窗口切换：停止 Qt3 debounce 定时器，直接跟随
#ifdef QT3_BUILD
        if (m_pendingHide) {
            m_pendingHide = false;
            m_debounceTimer->stop();
        }
#endif
        if (!watched->isWidgetType()) return false;
        QWidget *tw = static_cast<QWidget*>(watched)->topLevelWidget();
        if (!tw || tw == this) return false;
#ifdef QT3_BUILD
        // TipLabel 是顶层 QLabel — 跳过，不跟踪
        if (tw->inherits("QLabel")) return false;
        if (tw->inherits("QPopupMenu")) return false;
        if (tw->inherits("DesktopLyrics")) return false;
        if (tw->inherits("ScreenshotRegionSelector")) return false;
        if (tw->inherits("ScreenshotPreviewDialog")) return false;
#else
        // Qt4 tooltips/popups 不跟踪
        if (((tw->windowFlags() & Qt::WindowType_Mask) == Qt::ToolTip) || ((tw->windowFlags() & Qt::WindowType_Mask) == Qt::Popup)) return false;
        if (tw->inherits("DesktopLyrics")) return false;
        if (tw->inherits("ScreenshotRegionSelector")) return false;
        if (tw->inherits("ScreenshotPreviewDialog")) return false;
#endif
		// 窗口宽或高小于 350px 时不跟随
		if (tw->width() < 350) {
			return false;
		}
        // 状态栏最小宽度大于活动窗口宽度时不跟随
        if (minimumWidth() > tw->width()) {
            return false;
        }
        // 状态栏可见且新窗口不遮挡当前位置时，不跟随
        if (isVisible() && !geometry().intersects(tw->frameGeometry())) {
            // return false;
        }
        m_activeWindow = tw;
        reposition();
        return false;
    }

#ifdef QT3_BUILD
    // Qt3 无 ApplicationDeactivate 事件 → 用 debounce timer 模拟
    if (event->type() == QEvent::WindowDeactivate) {
        if (!watched->isWidgetType()) return false;
        QWidget *tw = static_cast<QWidget*>(watched)->topLevelWidget();
        if (tw == m_activeWindow && !m_pendingHide) {
            m_pendingHide = true;
            m_debounceTimer->start(100, true); // single shot
        }
        return false;
    }
#else
    // Qt4 有 ApplicationDeactivate 事件（不隐藏，上层窗口覆盖时 bar 仍需显示）
    if (event->type() == QEvent::ApplicationDeactivate) {
        return false;
    }
#endif

    if (event->type() == QEvent::Hide) {
        if (watched == m_activeWindow && watched->isWidgetType()) {
            QWidget *w = static_cast<QWidget*>(watched);
            if (w->isTopLevel()) {
                QTimer::singleShot(50, this, SLOT(retrack()));
                return false;
            }
        }
        return false;
    }

    if (event->type() == QEvent::Close && watched == m_activeWindow) {
        m_activeWindow = nullptr;
        return false;
    }
    if ((event->type() == QEvent::Move ||
         event->type() == QEvent::Resize) &&
        watched == m_activeWindow) {
        reposition();
        return false;
    }
    return false;
}

#ifdef QT3_BUILD
void SharedStatusBar::onDebounceTimeout()
{
    // 100ms 内没有新的 WindowActivate → 真切换到外部应用 → 隐藏
    m_pendingHide = false;
    // hide();
}

void SharedStatusBar::onFocusChanged(QWidget *, QWidget *) {}
void SharedStatusBar::installEventFiltersOnMyselfTopLevelWidgets() {}
#else
void SharedStatusBar::installEventFiltersOnMyselfTopLevelWidgets()
{
    QWidgetList widgets = QApplication::topLevelWidgets();
    for (int i = 0; i < widgets.size(); ++i) {
        QWidget *w = widgets.at(i);
        if (w == this) continue;
        if (w->inherits("DesktopLyrics")) continue;
        if (w->inherits("ScreenshotRegionSelector")) continue;
        if (w->inherits("ScreenshotPreviewDialog")) continue;
        if (((w->windowFlags() & Qt::WindowType_Mask) == Qt::ToolTip) || ((w->windowFlags() & Qt::WindowType_Mask) == Qt::Popup)) continue;
        w->installEventFilter(this);
    }
}

void SharedStatusBar::onFocusChanged(QWidget *, QWidget *now)
{
    if (!now) return;
    QWidget *tw = now->topLevelWidget();
    if (!tw || tw == this) return;
    if (tw->inherits("DesktopLyrics")) return;
    if (tw->inherits("ScreenshotRegionSelector")) return;
    if (tw->inherits("ScreenshotPreviewDialog")) return;
    if (((tw->windowFlags() & Qt::WindowType_Mask) == Qt::ToolTip) || ((tw->windowFlags() & Qt::WindowType_Mask) == Qt::Popup)) return;
    if (tw->width() < 350) return;
    if (minimumWidth() > tw->width()) return;
    tw->installEventFilter(this);
    m_activeWindow = tw;
    reposition();
}
#endif

void SharedStatusBar::retrack()
{
    QWidget *aw = qApp->activeWindow();
    if (!aw || aw == this) {
        m_activeWindow = nullptr;
        // activeWindow() 未就绪时持续重试，直到找到活动窗口
        if (!m_activeWindow) {
            QTimer::singleShot(50, this, SLOT(retrack()));
        }
        return;
    }
    if (aw->inherits("ScreenshotRegionSelector") || aw->inherits("ScreenshotPreviewDialog")) {
        return;
    }
    if (minimumWidth() > aw->width()) { return; }
    if (aw != m_activeWindow) {
        m_activeWindow = aw;
        installEventFiltersOnMyselfTopLevelWidgets();
        reposition();
    }
}

void SharedStatusBar::paintEvent(QPaintEvent *)
{
#ifdef QT3_BUILD
    QPainter p(this);
    p.setPen(QPen(Qt::gray, 1));
#else
    QPainter p(this);
    p.setPen(QColor(160, 160, 160));
#endif
    int x = width() - GRIP_SIZE;
    int y = height() - GRIP_SIZE;
    for (int i = 0; i < 4; i++) {
        int x1 = x + GRIP_SIZE - 3 - i * 4;
        int y1 = y + GRIP_SIZE - i * 4;
        p.drawLine(x1,     y1 + 4, x1 + 4, y1);
        p.drawLine(x1 + 4, y1 + 4, x1 + 8, y1);
    }
}

bool SharedStatusBar::isInGripArea(const QPoint &localPos) const
{
    return localPos.x() >= width() - GRIP_SIZE
        && localPos.y() >= height() - GRIP_SIZE;
}

void SharedStatusBar::handleGripPress(const QPoint &globalPos)
{
    if (!isVisible() || !m_activeWindow) { return; }
    m_dragging = true;
    m_dragStartGlobal = globalPos;
    m_windowStartGeo = m_activeWindow->geometry();
}

void SharedStatusBar::handleGripDrag(const QPoint &globalPos)
{
    if (!m_dragging || !m_activeWindow) { return; }
    int dx = globalPos.x() - m_dragStartGlobal.x();
    int dy = globalPos.y() - m_dragStartGlobal.y();
    QRect g = m_windowStartGeo;
    g.setWidth ((g.width() + dx) > m_activeWindow->minimumWidth()
                ? g.width() + dx
                : m_activeWindow->minimumWidth());
    g.setHeight((g.height() + dy) > m_activeWindow->minimumHeight()
                ? g.height() + dy
                : m_activeWindow->minimumHeight());
    m_activeWindow->setGeometry(g);
}

void SharedStatusBar::handleGripRelease()
{
    m_dragging = false;
}

void SharedStatusBar::reposition()
{
    if (m_repositioning || !m_activeWindow) return;
    m_repositioning = true;

    if (!m_activeWindow->isVisible()) {
        m_repositioning = false;
        return;
    }
    QPoint bottomLeft = m_activeWindow->mapToGlobal(QPoint(0, m_activeWindow->height()));
    move(bottomLeft.x(), bottomLeft.y() + 1);
    int barH = m_bar->sizeHint().height();
    if (barH < 20) { barH = 20; }
    int winW = m_activeWindow->width();
    resize(winW, barH);
    if (!isVisible()) { show(); raise(); }
    if (minimumWidth() > winW) { m_repositioning = false; return; }

#ifdef Q_OS_LINUX
    {
#ifdef QT3_BUILD
        Display *dpy = QPaintDevice::x11Display();
#else
        Display *dpy = QX11Info::display();
#endif
        XSetTransientForHint(dpy, winId(), m_activeWindow->winId());
        XFlush(dpy);
    }
#endif

    m_repositioning = false;
}

bool SharedStatusBar::event(QEvent *e)
{
    if (e->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent*>(e);
        if (me->button() == Qt::LeftButton && isInGripArea(me->pos())) {
            handleGripPress(me->globalPos());
            return true;
        }
    }
    if (e->type() == QEvent::MouseMove) {
        QMouseEvent *me = static_cast<QMouseEvent*>(e);
        if (m_dragging) {
            handleGripDrag(me->globalPos());
            return true;
        }
#ifdef QT3_BUILD
        setCursor(QCursor(isInGripArea(me->pos())
                          ? SizeFDiagCursor : ArrowCursor));
#else
        setCursor(isInGripArea(me->pos())
                  ? Qt::SizeFDiagCursor : Qt::ArrowCursor);
#endif
    }
    if (e->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent*>(e);
        if (m_dragging && me->button() == Qt::LeftButton) {
            handleGripRelease();
            return true;
        }
    }
    return QWidget::event(e);
}
