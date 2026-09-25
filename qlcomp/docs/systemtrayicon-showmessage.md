# SystemTrayIcon showMessage 气泡（Qt3/Qt4 双兼容）

> 关联：[systemtrayicon.md](./systemtrayicon.md)。本文记录托盘消息气泡的接口、Qt3 自绘实现与 Qt4 委托策略。

## 目标

让 `SystemTrayIcon` 提供 `showMessage()`（对齐 Qt4 `QSystemTrayIcon` 范式）：

- Qt4：**委托原生** `QSystemTrayIcon::showMessage`（外观随桌面系统，堆叠由系统管理）。
- Qt3：**自绘气泡 `TrayBubble`**，贴近托盘图标弹出，自带堆叠/排队/去重策略（Qt3 无原生托盘消息）。

接口对两端完全一致，调用方不感知分支。

## API

```cpp
enum MessageIcon { NoIcon = 0, Information = 1, Warning = 2, Critical = 3 }; // 数值同 Qt4

void showMessage(const QString& title, const QString& message,
                 MessageIcon icon = Information, int msecs = 10000);
void clearMessageQueue();                      // Qt3 清待显示队列；Qt4 no-op
static bool supportsMessages();                // Qt3/X11 恒 true（与托盘实现无关）；Qt4 原生静态方法

signals:
    void messageClicked();                     // 点击气泡后隐藏
```

- `msecs <= 0`：常显至点击（Qt3 语义；Qt4 原生由系统裁量）。
- `m_visible == false` 时 `showMessage` 直接丢弃（与托盘隐藏一致）。

## Qt4 分支

薄转发，`#endif` 隔离于 Qt3 之上：

```cpp
d->native->showMessage(title, message, (QSystemTrayIcon::MessageIcon)icon, msecs);
// activated / messageClicked 经原生信号中继：nativeMessageClicked() SLOT → emit
```

Qt4 原生气泡 z 序/外观随系统（StatusNotifierItem 下部分 DE 可能不显示，同原生限制）。

## Qt3 分支：TrayBubble

`class TrayBubble`（`systemtrayicon.cpp`，无 Q_OBJECT，用 `startTimer/timerEvent`，不新增 moc 文件）：
`QWidget(0, 0, WStyle_StaysOnTop | WStyle_Customize | WStyle_NoBorder | WStyle_Tool)`，`friend class TrayBubble` 授予 SystemTrayIcon 私有访问。
`WStyle_Tool` → X11 设 `_NET_WM_WINDOW_TYPE_UTILITY` + `SKIP_TASKBAR/SKIP_PAGER/ABOVE`（已验证），气泡不进任务栏/分页器，与 Qt4 原生、Qt5 QBalloonTip 一致。

### 堆叠策略（一次 `showMessage` 事件常见 2~20 条，需合并/排队）

1. **去重合并 ×N**：同 title+message → 计数 +1 显示 `×N`，延长显示计时。
2. **同源替换**：同 title 不同 message（状态流，如"连接中→已连接"）→ 就地替换，重置计时与布局。
3. **FIFO 队列**：不同源入队，上限 `MAX_QUEUE=3`，满丢最旧（防刷屏）。
4. **计时只在显示时启动**（空闲不启动定时器）；显示窗口关闭后 `COOLDOWN_MS=200` 冷却再晋升队头。
5. `clearMessageQueue()`：主窗激活时清空待显示队列（当前气泡保留）。

状态机：`TimerMode { None, Visible, Cooldown }`。`promoteNext()` 在排空/空闲情形自愈；点击/超时都走 `closeCurrentAndPromote()` → 空队直接隐藏，非空转 Cooldown 等晋升。

### 几何

- 固定宽 `W=300`（>`SharedStatusBar` 350px 跟随阈值前不干扰）；高度=正文自适应 + `ARROW_EXT=10` 箭头。
- `prepareAndPos()` 单步算：bodyH → 依 `trayIconGeometry()` 与 `availableGeometry(-1)` 决定箭头方向 `Arrow{None,Up,Down}` 与最终尺寸/位置。
- 图标在上半屏：气泡向下（ArrowDown，三角伸出底边）；下半屏：向上（ArrowUp）。无托盘几何回退桌面右下角（m=16）。

### 视觉

- 深底圆角体（`drawRoundRect(…,15,15)`）+ 分类图标 **48px**（`ICON_SIZE`，对齐 Qt5 `QBalloonTip`）：`drawEllipse` 底色圆（蓝 i / 黄 `!` / 红 `x`，NoIcon 不画）贴近左边距 10px、垂直居中于气泡体；白色粗体字符**独立字号** `ICON_CHAR_SIZE=36`（比图标小一圈）。文本左界 `contentLeft()=10+48+10=68` 与绘制 x 一致。
- 标题/内容用气泡**基础字体**（`font()`）：标题粗体白、正文常规 #DDDDDD——与图标字符字号**各自独立**，绘制时从 `font()` 取字体（勿用 `p.font()`，painter 字体已因图标字符被污染）。`×N` 用 `qFromUtf8("×")`（纯 ASCII 限制）黄字右上角。
- 标题粗体白，正文常规 #DDDDDD；`×N` 用 `qFromUtf8("×")`（纯 ASCII 限制）黄字右上角。
- 透明圆角+箭头镂空：`buildMask()` 用 `QBitmap` 同形再 `QPainter` 刷 `Qt::color1`/`Qt::color0` 后 `setMask(bm)`（Qt3 无 QPainterPath 的等价物）。

### 换行

`wrapText()`：空格分词，单词或连续 CJK 超 `maxW` 时逐字切分（`QFontMetrics::width`，Qt3 无 `elidedText`）。

## 托盘几何 `TrayIcon::trayIconGeometry()`

`trayicon.h` 新增公开方法（Psi 原样文件需最小改动；头注释保留）：

- `trayicon_x11.cpp`：`XTranslateCoordinates(d, d->winId(), XDefaultRootWindow, …)` 得表屏坐标 + `XGetWindowAttributes` 取 22×22 尺寸；`!d` 或失败返回空 `QRect()`。
- `trayicon_win.cpp`：stub 返回 `QRect()`（Windows 不回退调用方，气泡落桌面右下角）。

## 改动文件

| 文件 | 改动 |
|------|------|
| `systemtrayicon.h` | MessageIcon/showMessage/clearMessageQueue/supportsMessages/messageClicked/nativeMessageClicked/fireMessageClicked/friend TrayBubble |
| `systemtrayicon.cpp` | Qt3：TrayBubble 全实现 + Private 前置（tray 改 `TrayIcon*`）；Qt4：原生转发 + messageClicked 中继 |
| `trayicon.h` | `QRect trayIconGeometry() const;` |
| `trayicon_x11.cpp` | X11 实现 |
| `trayicon_win.cpp` | stub |

`qlite.pri`/`qltox.pro` 无改动（TrayBubble 无 Q_OBJECT，moc 安全；`Q_OBJECT` 类型均在类体外 `#ifdef`）。

## 验证计划

- 双构建：`buildqt3.sh` → `buildqt4.sh`（串行）。
- Qt3 冒烟：单条弹出贴托盘；连发 5 条验去重 ×N/队列上限丢最旧/200ms 冷却；点击触发 `messageClicked` 并消失；主窗激活清队列。
- Qt4 冒烟：原生气泡出现；点击触发 `messageClicked`。

## 后续接线（qltox，已完成）

- `mainwindow.cpp` ctor 托盘区：`connect(m_tray, SIGNAL(messageClicked()), this, SLOT(trayShowMainWindow()))` —— 点击气泡恢复主窗。
- `MainWindow::event()`：`WindowActivate`（Qt3）/`ActivationChange`（Qt4）且 `qApp->activeWindow()==this` 时调 `m_tray->clearMessageQueue()` —— 主窗激活清空待显示队列。
- 冒烟（独立程序，非 qltox 内）：见 `验证计划`；几何/点击/队列晋升均已程序化验证。