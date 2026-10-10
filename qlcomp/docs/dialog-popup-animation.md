# 对话框「打开动画」组件（DialogPopupAnimator）

## 目标

macOS 风格对话框弹出：内容从小放大 + 透明度淡入。**关键约束：本机（xfwm4）无合成器**，
`setWindowOpacity` / `_NET_WM_WINDOW_OPACITY` 均被忽略，且逐帧 resize 顶层窗口会抖动。

## 方案：固定窗口 + 内容快照缩放（无需合成器）

不做 widget transform（QGraphicsProxyWidget 方案有任务栏归属错乱、无 `exec()`、
缺合成器发黑等硬伤，不采用），改为：

1. `Show` 时用 `QPixmap::grabWidget()` 抓取对话框内容快照。
2. 在一个**尺寸不变的子控件**（`DialogScaleOverlay`）里把快照从中心按
   `kStartScale=0.85`→`1.0` 放大绘制；单窗口绘制 → Qt 双缓冲 → 平滑。
   - Qt4：`QPainter::SmoothPixmapTransform`；Qt3：`QImage::smoothScale`。
   - 覆盖层不用 `Q_OBJECT`（规避 Qt3 moc）。
3. 动画结束删除覆盖层，露出真实内容；**全程不改顶层窗口几何**。

## 计时与淡入

- 计时用 `QTime::elapsed()` 测量**真实流逝**（非累计 tick），帧间隔 `kTickMs=16`，
  ease-out-cubic，默认时长 220ms。
- 淡入仅在有合成器时启用：`qHasCompositor()` 探测 `_NET_WM_CM_S0`
  （`qlcomp/compat34.h:qX11Display()`）。有则 Qt4 `setWindowOpacity` /
  Qt3 `_NET_WM_WINDOW_OPACITY`；无则仅缩放（Qt3 无 `QPainter::setOpacity`，
  覆盖层不做逐帧 alpha）。

## 完全关闭

```cpp
DialogPopupAnimator::setEnabled(false);   // 全局开关，默认 true
DialogPopupAnimator::isEnabled();
```
`g_activeParams->compositingMode == JumpCut` 时同样跳过。
入口默认关闭：`qltox/main.cpp` 在 macOS 上启动即 `setEnabled(false)`（macOS 自带窗口动效）。

## 用法（各站点一行，不改流程）

```cpp
#include "DialogPopupAnimator.h"
...
DialogPopupAnimator::install(&dialog);   // 时长可选，默认 220ms
dialog.exec();
```

## 验证

```
cd qltox
bash buildqt3.sh     # 生成 qltox
bash buildqt4.sh     # 生成 q4tox
```

> 已接入：`mainwindow.cpp`（13 处弹窗）+ `webcredsdialog.cpp`（2 处）。
> 返回对象挂在对话框下，随其销毁自动释放。
