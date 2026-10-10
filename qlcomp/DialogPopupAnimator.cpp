#include "DialogPopupAnimator.h"
#include "StyleParams.h"

#include <qpainter.h>
#include <qcolor.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qpalette.h>

#if defined(Q_OS_LINUX)
#include <X11/Xlib.h>     // XInternAtom, XGetSelectionOwner, XChangeProperty
#include <X11/Xatom.h>    // XA_CARDINAL
// X11 的宏会污染 QEvent 的枚举值，必须 undef（与 compat34.cpp 一致）
#ifdef KeyPress
#undef KeyPress
#endif
#ifdef KeyRelease
#undef KeyRelease
#endif
#endif

namespace {
const int   kTickMs            = 16;
const int   kDefaultDurationMs = 220;
const float kStartScale        = 0.85f;

float easeOutCubic(float t) {
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

// 是否有 X 合成器（决定窗口淡入是否有效）。仅探测 Screen 0，结果缓存。
bool qHasCompositor() {
#if defined(Q_OS_LINUX)
    static int cached = -1;
    if (cached < 0) {
        cached = 0;
        Display* dpy = qX11Display();
        if (dpy) {
            Atom a = XInternAtom(dpy, "_NET_WM_CM_S0", False);
            if (a != None && XGetSelectionOwner(dpy, a) != None) {
                cached = 1;
            }
        }
    }
    return cached == 1;
#else
    return false;
#endif
}

// 覆盖层：固定尺寸子控件，在其内把快照从中心按比例放大后绘制。
// 单窗口绘制 → Qt 双缓冲 → 无合成器也平滑；不触碰顶层窗口几何。
class DialogScaleOverlay : public QWidget {
public:
    DialogScaleOverlay(QWidget* parent, const QPixmap& snap, const QColor& bg)
        : QWidget(parent), m_snap(snap), m_bg(bg), m_t(0.0f) {
#ifdef QT3_BUILD
        setBackgroundMode(Qt::NoBackground);
        setWFlags(Qt::WNoMousePropagation);
        m_img = m_snap.convertToImage();
#else
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
#endif
    }

    void setProgress(float eased) {
        m_t = eased;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) {
        QPainter p(this);
        p.fillRect(rect(), m_bg);
        const float s = kStartScale + (1.0f - kStartScale) * m_t;
        int w = (int)(width() * s + 0.5f);
        int h = (int)(height() * s + 0.5f);
        if (w < 1) { w = 1; }
        if (h < 1) { h = 1; }
        const QRect target((width() - w) / 2, (height() - h) / 2, w, h);
#ifdef QT3_BUILD
        QPixmap scaled(m_img.smoothScale(w, h));
        p.drawPixmap(target.topLeft(), scaled);
#else
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawPixmap(target, m_snap, m_snap.rect());
#endif
    }

private:
    QPixmap m_snap;
    QColor  m_bg;
    float   m_t;
#ifdef QT3_BUILD
    QImage  m_img;
#endif
};

} // namespace

bool DialogPopupAnimator::s_enabled = true;

DialogPopupAnimator::DialogPopupAnimator(QWidget* dialog, int durationMs)
    : QObject(dialog),
      m_dialog(dialog),
      m_overlay(0),
      m_timerId(0),
      m_durationMs(durationMs > 0 ? durationMs : kDefaultDurationMs),
      m_animating(false),
      m_fade(false) {
}

DialogPopupAnimator::~DialogPopupAnimator() {
    if (m_timerId != 0) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
}

DialogPopupAnimator* DialogPopupAnimator::install(QWidget* dialog, int durationMs) {
    if (!dialog) { return 0; }
    DialogPopupAnimator* a = new DialogPopupAnimator(dialog, durationMs);
    dialog->installEventFilter(a);
    return a;
}

void DialogPopupAnimator::setEnabled(bool on) {
    s_enabled = on;
}

bool DialogPopupAnimator::isEnabled() {
    return s_enabled;
}

bool DialogPopupAnimator::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::Show && obj == m_dialog && m_dialog->isTopLevel()) {
        if (s_enabled && (!g_activeParams || g_activeParams->compositingMode != JumpCut)) {
            startAnimation();
        }
    }
    return QObject::eventFilter(obj, event);
}

void DialogPopupAnimator::startAnimation() {
    if (m_animating) { return; }
    if (m_dialog->width() <= 0 || m_dialog->height() <= 0) { return; }

    const QPixmap snap = QPixmap::grabWidget(m_dialog);
    if (snap.isNull()) { return; }

#ifdef QT3_BUILD
    const QColor bg = m_dialog->palette().active().background();
#else
    const QColor bg = m_dialog->palette().color(QPalette::Window);
#endif
    m_overlay = new DialogScaleOverlay(m_dialog, snap, bg);
    m_overlay->setGeometry(0, 0, m_dialog->width(), m_dialog->height());
    m_overlay->raise();
    m_overlay->show();

    m_fade = qHasCompositor();
    if (m_fade) { setWindowAlpha(0.0f); }

    m_animating = true;
    m_clock.start();
    m_timerId = startTimer(kTickMs);
}

void DialogPopupAnimator::timerEvent(QTimerEvent* event) {
    if (event->timerId() != m_timerId) {
        QObject::timerEvent(event);
        return;
    }
    const float t = (float)m_clock.elapsed() / (float)m_durationMs;
    if (t >= 1.0f) {
        applyProgress(1.0f);
        finishAnimation();
        return;
    }
    applyProgress(t);
}

void DialogPopupAnimator::applyProgress(float t) {
    const float e = easeOutCubic(t);
    if (m_overlay) { static_cast<DialogScaleOverlay*>(m_overlay)->setProgress(e); }
    if (m_fade) { setWindowAlpha(e); }
}

void DialogPopupAnimator::finishAnimation() {
    if (m_timerId != 0) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    if (m_overlay) {
        m_overlay->hide();
        delete m_overlay;
        m_overlay = 0;
    }
    if (m_fade) {
        setWindowAlpha(1.0f);
        m_fade = false;
    }
    m_animating = false;
}

void DialogPopupAnimator::setWindowAlpha(float a) {
#ifdef QT3_BUILD
#if defined(Q_OS_LINUX)
    Display* dpy = qX11Display();
    if (!dpy) { return; }
    Atom atom = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
    if (atom == None) { return; }
    unsigned long value = (unsigned long)(a * 4294967295.0);
    XChangeProperty(dpy, (Window)m_dialog->winId(), atom, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char*)&value, 1);
    XFlush(dpy);
#endif
#else
    m_dialog->setWindowOpacity(a);
#endif
}
