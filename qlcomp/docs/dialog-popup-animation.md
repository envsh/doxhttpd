# 对话框打开动画（DialogPopupAnimator）

## 背景

Qt Widgets 层没有 widget transform，**无法真正「缩放内容」**（QGraphicsProxyWidget
方案有任务栏归属错乱、无 `exec()`、缺合成器时发黑等硬伤，不采用）。
故采用业界通行近似：动画**顶层窗口 geometry** 从小放大，内容按终尺寸布局、被裁剪渐显，
叠加透明度淡入。视觉接近 macOS 对话框弹出。

## 用法（未来接入，各站点一行，不改流程）

```cpp
#include "DialogPopupAnimator.h"
...
DialogPopupAnimator::install(&dialog);   // 时长可选，默认 220ms
dialog.exec();
```

## 实现

- `DialogPopupAnimator : QObject`，**不用 `Q_OBJECT`**，以 `eventFilter` 捕获
  `QEvent::Show`、`startTimer/timerEvent` 逐帧驱动，规避 Qt3 moc。
- 锚点=对话框中心；起点=终几何按 `kStartScale=0.85` 缩小居中。
- 缓动=ease-out-cubic；帧间隔 `kTickMs=15`。
- 透明度：Qt4 `QWidget::setWindowOpacity`；Qt3 走 X11 `_NET_WM_WINDOW_OPACITY`
  （`qlcomp/compat34.h:qX11Display()`；无合成器时 WM 忽略 → 退化为纯 geometry）。
- 减动效：`g_activeParams->compositingMode == JumpCut` 时跳过（与
  `LimeScrollBar` 现有约定一致）。
- 无异常；`#ifdef QT3_BUILD` 分支。

## 验证

```
cd qlcomp/demo
bash buildqt3.sh     # 生成 build-qt3/dialoganim_demo
bash buildqt4.sh     # 生成 build-qt4/dialoganim_demo
./build-qt3/dialoganim_demo
```

> 注意：本组件为独立实现，**未接入任何现有对话框**（不改主流程）。
