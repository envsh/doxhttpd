#ifndef DIALOGPOPUPANIMATOR_H
#define DIALOGPOPUPANIMATOR_H

#include "compat34.h"
#include <qdatetime.h>

class QTimerEvent;
class QEvent;

// 对话框「打开动画」器：窗口几何不动，仅对内容快照做中心缩放（macOS 风格近似）。
//
// 接入（一行，不改调用方流程）：
//     DialogPopupAnimator::install(&dialog);
//     dialog.exec();
//
// 设计要点：
//   - 不用 Q_OBJECT：用 eventFilter + timerEvent 驱动，规避 Qt3 moc 麻烦。
//   - 不改顶层窗口几何：Show 时抓取内容快照，盖一个子控件在其上做「中心缩放」
//     （kStartScale→1.0）；单窗口内绘制 → Qt 双缓冲 → 无合成器也平滑。
//   - 计时用 QTime 测量真实流逝（非累计 tick），避免定时器抖动导致忽快忽慢。
//   - 淡入仅在有合成器时启用（Qt4 setWindowOpacity / Qt3 _NET_WM_WINDOW_OPACITY）；
//     无合成器则仅缩放。Qt3 无 QPainter::setOpacity，覆盖层不做逐帧 alpha。
//   - 完全关闭：setEnabled(false) 或 compositingMode==JumpCut 时直接跳过。
class DialogPopupAnimator : public QObject {
public:
    // 为 dialog 安装动画器；返回对象挂在 dialog 下，随其销毁自动释放。
    // durationMs <= 0 时使用默认时长。
    static DialogPopupAnimator* install(QWidget* dialog, int durationMs = 0);

    // 全局开关：false 时所有对话框均不播放动画。
    static void setEnabled(bool on);
    static bool isEnabled();

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
    QWidget* m_overlay;
    int      m_timerId;
    int      m_durationMs;
    bool     m_animating;
    bool     m_fade;
    QTime    m_clock;
    static bool s_enabled;
};

#endif // DIALOGPOPUPANIMATOR_H
