#include "DialogPopupAnimator.h"
#include "StyleParams.h"

#ifdef QT3_BUILD
#include <X11/Xlib.h>     // XInternAtom, XChangeProperty, XFlush
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
const int   kDefaultDurationMs = 220;
const int   kTickMs            = 15;
const float kStartScale        = 0.85f;

float easeOutCubic(float t) {
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

int lerpInt(int a, int b, float t) {
    return a + (int)((b - a) * t + 0.5f);
}
}

DialogPopupAnimator::DialogPopupAnimator(QWidget* dialog, int durationMs)
    : QObject(dialog)
    , m_dialog(dialog)
    , m_timerId(0)
    , m_durationMs(durationMs > 0 ? durationMs : kDefaultDurationMs)
    , m_elapsedMs(0)
    , m_animating(false) {}

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

bool DialogPopupAnimator::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::Show && obj == m_dialog && m_dialog->isTopLevel()) {
        // 减动效：直接跳过动画
        if (!g_activeParams || g_activeParams->compositingMode != JumpCut) {
            startAnimation();
        }
    }
    return QObject::eventFilter(obj, event);
}

void DialogPopupAnimator::startAnimation() {
    if (m_animating) { return; }

    m_finalGeo = m_dialog->geometry();
    if (m_finalGeo.width() <= 0 || m_finalGeo.height() <= 0) { return; }

    int w = (int)(m_finalGeo.width() * kStartScale + 0.5f);
    int h = (int)(m_finalGeo.height() * kStartScale + 0.5f);
    if (w < 1) { w = 1; }
    if (h < 1) { h = 1; }
    const int cx = m_finalGeo.x() + m_finalGeo.width() / 2;
    const int cy = m_finalGeo.y() + m_finalGeo.height() / 2;
    m_startGeo = QRect(cx - w / 2, cy - h / 2, w, h);

    m_elapsedMs = 0;
    m_animating = true;
    m_dialog->setGeometry(m_startGeo);
    setWindowAlpha(0.0f);
    m_timerId = startTimer(kTickMs);
}

void DialogPopupAnimator::timerEvent(QTimerEvent* event) {
    if (event->timerId() != m_timerId) {
        QObject::timerEvent(event);
        return;
    }
    m_elapsedMs += kTickMs;
    float t = (float)m_elapsedMs / (float)m_durationMs;
    if (t >= 1.0f) {
        applyProgress(1.0f);
        finishAnimation();
        return;
    }
    applyProgress(t);
}

void DialogPopupAnimator::applyProgress(float t) {
    const float e = easeOutCubic(t);
    m_dialog->setGeometry(QRect(
        lerpInt(m_startGeo.x(),      m_finalGeo.x(),      e),
        lerpInt(m_startGeo.y(),      m_finalGeo.y(),      e),
        lerpInt(m_startGeo.width(),  m_finalGeo.width(),  e),
        lerpInt(m_startGeo.height(), m_finalGeo.height(), e)));
    setWindowAlpha(e);
}

void DialogPopupAnimator::finishAnimation() {
    if (m_timerId != 0) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    m_animating = false;
    m_dialog->setGeometry(m_finalGeo);
    setWindowAlpha(1.0f);
}

void DialogPopupAnimator::setWindowAlpha(float a) {
    if (a < 0.0f) { a = 0.0f; }
    if (a > 1.0f) { a = 1.0f; }
#ifdef QT3_BUILD
    // Qt3 无 setWindowOpacity：走 X11 _NET_WM_WINDOW_OPACITY
    Display* dpy = qX11Display();
    if (!dpy) { return; }
    Atom atom = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
    if (atom == None) { return; }
    unsigned long value = (unsigned long)(a * 4294967295.0);
    XChangeProperty(dpy, (Window)m_dialog->winId(), atom, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char*)&value, 1);
    XFlush(dpy);
#else
    m_dialog->setWindowOpacity(a);
#endif
}
