#ifndef LIMESCROLLBAR_H
#define LIMESCROLLBAR_H

#include "compat34.h"
#include "StyleParams.h"

#ifdef QT3_BUILD
#include <qscrollbar.h>
#include <qtimer.h>
#include <qpainter.h>
#include <qevent.h>
#else
#include <QScrollBar>
#include <QTimer>
#include <QPainter>
#include <QPaintEvent>
#include <QEvent>
#endif

class LimeScrollBar : public QScrollBar {
    Q_OBJECT
public:
    LimeScrollBar(Qt::Orientation orientation, QWidget* parent = 0);
    void showTemporarily();
    void updateStyle();

protected:
    void paintEvent(QPaintEvent* event);
    void enterEvent(qEnterEventType* event);
    void leaveEvent(QEvent* event);
#ifdef QT3_BUILD
    void valueChange();
#else
    void sliderChange(SliderChange change);
#endif

private slots:
    void onFadeIn();
    void onFadeOut();
    void onHideTimeout();

private:
    void startFadeIn();
    void startFadeOut();
    QColor sliderColor(bool hovered) const;

    enum FadeState { Hidden, FadeIn, Visible, FadeOut };

    float m_animRatio;
    FadeState m_fadeState;
    bool m_hovered;
    bool m_isVisible;
    const StyleParams* m_lastParams;

    QTimer* m_fadeTimer;
    QTimer* m_hideTimer;

    static const int kFadeDuration = 300;
    static const int kHideDelay = 3000;
    static const int kFadeStepMs = 30;
    static const float kFadeStep;
    static const float kAlwaysFaintRatio;
};

#endif
