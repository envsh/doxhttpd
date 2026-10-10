#ifndef DIALOGPOPUPANIMATOR_H
#define DIALOGPOPUPANIMATOR_H

#include "compat34.h"

class QTimerEvent;
class QEvent;

// 对话框「打开动画」器：小→大 + 透明度淡入（macOS 风格近似实现）。
//
// 未来接入（一行，不改调用方流程）：
//     DialogPopupAnimator::install(&dialog);
//     dialog.exec();
//
// 设计要点：
//   - 不用 Q_OBJECT：用 eventFilter + timerEvent 驱动，规避 Qt3 moc 麻烦。
//   - 锚点=对话框中心；起点=终几何按 kStartScale 缩小。
//   - Qt4 用 QWidget::setWindowOpacity；Qt3 走 X11 _NET_WM_WINDOW_OPACITY
//     （无合成器时被 WM 忽略 → 退化为纯 geometry，不报错）。
//   - compositingMode == JumpCut（减动效）时跳过动画。
class DialogPopupAnimator : public QObject {
public:
    // 为 dialog 安装动画器；返回对象挂在 dialog 下，随其销毁自动释放。
    // durationMs <= 0 时使用默认时长。
    static DialogPopupAnimator* install(QWidget* dialog, int durationMs = 0);

protected:
    bool eventFilter(QObject* obj, QEvent* event);
    void timerEvent(QTimerEvent* event);

private:
    DialogPopupAnimator(QWidget* dialog, int durationMs);
    ~DialogPopupAnimator();

    void startAnimation();
    void finishAnimation();
    void applyProgress(float t);
    void setWindowAlpha(float a);

    QWidget* m_dialog;
    int      m_timerId;
    int      m_durationMs;
    int      m_elapsedMs;
    bool     m_animating;
    QRect    m_finalGeo;
    QRect    m_startGeo;
};

#endif // DIALOGPOPUPANIMATOR_H
