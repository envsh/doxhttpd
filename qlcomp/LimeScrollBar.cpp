#include "LimeScrollBar.h"
#include "LimeStyle.h"
#include "ThemeManager.h"
#include <algorithm>

// ========== Helpers ==========

static QColor blendScrollBarColor(const QColor& bg, const QColor& fg,
                                  float ratio, CompositingMode mode) {
    switch (mode) {
    case AlphaBlend: {
#ifdef QT3_BUILD
        return lerpColor(bg, fg, ratio);
#else
        QColor c = fg;
        c.setAlphaF(ratio);
        return c;
#endif
    }
    case ColorBlend:
        return lerpColor(bg, fg, ratio);
    case JumpCut:
        return fg;
    }
    return fg;
}

const float LimeScrollBar::kFadeStep = 30.0f / kFadeDuration; // 30ms * 10 = 300ms
const float LimeScrollBar::kAlwaysFaintRatio = 0.4f;

// ========== Constructor ==========

LimeScrollBar::LimeScrollBar(Qt::Orientation orientation, QWidget* parent)
    : QScrollBar(orientation, parent)
    , m_animRatio(0.0f)
    , m_fadeState(Hidden)
    , m_hovered(false)
    , m_isVisible(false)
{
    m_fadeTimer = new QTimer(this);
    connect(m_fadeTimer, SIGNAL(timeout()), this, SLOT(onFadeIn()));

    m_hideTimer = new QTimer(this);
#ifdef QT3_BUILD
    m_hideTimer->start(kHideDelay, TRUE);
    m_hideTimer->stop();
#else
    m_hideTimer->setSingleShot(true);
#endif

    m_lastParams = g_activeParams;
}

// ========== Style update ==========

void LimeScrollBar::updateStyle() {
    const StyleParams* p = g_activeParams;
    if (!p) { return; }

    if (p->scrollbarMode == StyleParams::AlwaysFaint) {
        m_animRatio = kAlwaysFaintRatio;
        m_fadeState = Visible;
        m_isVisible = true;
        m_fadeTimer->stop();
        m_hideTimer->stop();
    } else {
        if (m_fadeState == Hidden || m_fadeState == FadeOut) {
            m_animRatio = 0.0f;
            m_isVisible = false;
        }
    }
    update();
}

// ========== Fade animation ==========

void LimeScrollBar::startFadeIn() {
    if (g_activeParams && g_activeParams->scrollbarMode == StyleParams::AlwaysFaint) {
        return;
    }

    if (g_activeParams && g_activeParams->compositingMode == JumpCut) {
        m_isVisible = true;
        m_fadeState = Visible;
        update();
        return;
    }

    disconnect(m_fadeTimer, 0, 0, 0);
    connect(m_fadeTimer, SIGNAL(timeout()), this, SLOT(onFadeIn()));
    m_fadeState = FadeIn;
    m_fadeTimer->start(kFadeStepMs);
}

void LimeScrollBar::startFadeOut() {
    if (g_activeParams && g_activeParams->scrollbarMode == StyleParams::AlwaysFaint) {
        return;
    }

    if (g_activeParams && g_activeParams->compositingMode == JumpCut) {
        m_isVisible = false;
        m_fadeState = Hidden;
        update();
        return;
    }

    disconnect(m_fadeTimer, 0, 0, 0);
    connect(m_fadeTimer, SIGNAL(timeout()), this, SLOT(onFadeOut()));
    m_fadeState = FadeOut;
    m_fadeTimer->start(kFadeStepMs);
}

void LimeScrollBar::onFadeIn() {
    m_animRatio += kFadeStep;
    if (m_animRatio >= 1.0f) {
        m_animRatio = 1.0f;
        m_fadeState = Visible;
        m_fadeTimer->stop();
    }
    update();
}

void LimeScrollBar::onFadeOut() {
    m_animRatio -= kFadeStep;
    if (m_animRatio <= 0.0f) {
        m_animRatio = 0.0f;
        m_fadeState = Hidden;
        m_fadeTimer->stop();
    }
    update();
}

void LimeScrollBar::onHideTimeout() {
    startFadeOut();
}

void LimeScrollBar::showTemporarily() {
    if (g_activeParams && g_activeParams->scrollbarMode == StyleParams::AlwaysFaint) {
        return;
    }

    if (m_fadeState == Hidden || m_fadeState == FadeOut) {
        startFadeIn();
    } else if (g_activeParams && g_activeParams->compositingMode == JumpCut) {
        m_isVisible = true;
        m_fadeState = Visible;
    }

    m_hideTimer->start(kHideDelay);
}

// ========== Color computation ==========

QColor LimeScrollBar::sliderColor(bool hovered) const {
    bool dark = ThemeManager::isDarkMode();
    const StyleParams::Palette* pal = nullptr;
    if (g_activeParams) {
        pal = dark ? &g_activeParams->dark : &g_activeParams->light;
    }
    static StyleParams::Palette fallback;
    if (!pal) { pal = &fallback; }

    QColor fg = hovered ? pal->scrollbarHover : pal->scrollbarSlider;
    QColor bg = pal->windowBg;

    float ratio = 0.0f;
    CompositingMode mode = ColorBlend;

    if (g_activeParams) {
        mode = g_activeParams->compositingMode;
    }

    if (g_activeParams && g_activeParams->scrollbarMode == StyleParams::AlwaysFaint) {
        ratio = hovered ? 1.0f : kAlwaysFaintRatio;
    } else {
        ratio = m_animRatio;
    }

    return blendScrollBarColor(bg, fg, ratio, mode);
}

// ========== PaintEvent ==========

void LimeScrollBar::paintEvent(QPaintEvent* event) {
    if (g_activeParams != m_lastParams) {
        updateStyle();
        m_lastParams = g_activeParams;
    }

    QPainter p(this);
#ifndef QT3_BUILD
    p.setRenderHint(QPainter::Antialiasing);
#endif
    p.setClipRect(event->rect());

    // always clear background first (prevents smearing)
#ifdef QT3_BUILD
    p.eraseRect(event->rect());
#else
    p.fillRect(event->rect(), palette().color(backgroundRole()));
#endif

    if (g_activeParams && g_activeParams->scrollbarMode == StyleParams::AlwaysFaint) {
        // AlwaysFaint: always draw
    } else if (g_activeParams && g_activeParams->compositingMode == JumpCut) {
        if (!m_isVisible) { return; }
    } else {
        if (m_fadeState == Hidden && m_animRatio <= 0.0f) { return; }
    }

    // compute thumb rect
    int sliderSize = 8;
    if (g_activeParams) { sliderSize = g_activeParams->scrollbarWidth; }

#ifdef QT3_BUILD
    int min = minValue();
    int max = maxValue();
    int val = value();
#else
    int min = minimum();
    int max = maximum();
    int val = value();
#endif

    if (min >= max) { return; }

    int totalLen, widgetSize;
    if (orientation() == Qt::Vertical) {
        totalLen = height();
        widgetSize = width();
    } else {
        totalLen = width();
        widgetSize = height();
    }

    int margin = 4;
    int trackLen = totalLen - 2 * margin;
    float thumbRatio = (float)(max - min) > 0 ? (float)pageStep() / (float)(max - min + pageStep()) : 1.0f;
    int thumbLen = std::max(20, (int)(trackLen * thumbRatio));
    int availLen = trackLen - thumbLen;
    int thumbPos = (availLen > 0)
        ? margin + (int)((float)(val - min) / (float)(max - min) * availLen)
        : margin;

    QRect sliderRect;
    if (orientation() == Qt::Vertical) {
        sliderRect = QRect(
            (widgetSize - sliderSize) / 2,
            thumbPos,
            sliderSize,
            thumbLen
        );
    } else {
        sliderRect = QRect(
            thumbPos,
            (widgetSize - sliderSize) / 2,
            thumbLen,
            sliderSize
        );
    }

    QColor color = sliderColor(m_hovered);

#ifndef QT3_BUILD
    if (g_activeParams && g_activeParams->compositingMode == AlphaBlend) {
        p.setOpacity((float)color.alphaF());
        color.setAlpha(255);
    }
#endif

    p.setBrush(color);
    p.setPen(Qt::NoPen);
    int rad = std::min(sliderSize / 2, 4);
#ifdef QT3_BUILD
    p.drawRoundRect(sliderRect, rad * 200 / std::max(sliderRect.width(), 1),
                    rad * 200 / std::max(sliderRect.height(), 1));
#else
    p.drawRoundedRect(sliderRect, rad, rad);
#endif
}

// ========== Events ==========

void LimeScrollBar::enterEvent(qEnterEventType* event) {
    m_hovered = true;
    m_hideTimer->stop();

    if (g_activeParams && g_activeParams->scrollbarMode == StyleParams::AlwaysFaint) {
        update();
    } else {
        startFadeIn();
    }

    QScrollBar::enterEvent(event);
}

void LimeScrollBar::leaveEvent(QEvent* event) {
    m_hovered = false;

    if (g_activeParams && g_activeParams->scrollbarMode == StyleParams::AlwaysFaint) {
        update();
    } else {
        m_hideTimer->start(kHideDelay);
    }

    QScrollBar::leaveEvent(event);
}

#ifdef QT3_BUILD
void LimeScrollBar::valueChange() {
    QScrollBar::valueChange();
    showTemporarily();
}
#else
void LimeScrollBar::sliderChange(SliderChange change) {
    QScrollBar::sliderChange(change);
    if (change == SliderValueChange) {
        showTemporarily();
    }
}
#endif
