# SharedStatusBar - 多窗口共享状态栏

## 目标

单个浮动状态栏实例，跟随当前活动窗口，在所有非模态窗口间共享。避免为每个窗口创建独立的 QStatusBar。

## 架构

```
SharedStatusBar : QWidget (singleton)
├── QStatusBar *m_bar           ← 内嵌状态栏
├── QLabel *m_iconLbl           ← 消息类型图标（彩色圆 i/!/x，外层布局，m_bar 左侧）
├── QTimer *m_iconTimer         ← 类型图标超时隐藏（单发，与消息超时同步）
├── QWidget *m_activeWindow    ← 当前追踪的顶层窗口
├── bool m_repositioning       ← 防重入守卫 (reposition 重入保护)
├── QTimer *m_debounceTimer    ← [Qt3 only] 失活 debounce 定时器 (100ms)
├── bool m_pendingHide         ← [Qt3 only] 是否在等待 debounce 超时
├── eventFilter(qApp)          ← 监听全局事件
├── reposition()               ← 跟踪窗口位置/大小
├── handleGripPress/Drag/Release ← sizegrip 拖动调整窗口大小
└── isInGripArea()             ← 判断鼠标是否在 sizegrip 区域
```

## 状态机

### 状态

```
                WinActivate(tw≠this)
   ┌──────────┐ ────────────────────────→ ┌───────────┐
   │  HIDDEN  │                           │ ATTACHED  │
   │          │ ←────────────────────────  │           │
   └──────────┘   AppDeactivate / Close    └─────┬─────┘
                                                  │
                                          Qt3 WinDeactivate
                                          (tw==m_activeWindow)
                                                  │
                                                  ▼
                                            ┌───────────┐
                                            │  PENDING  │ 100ms debounce
                                            │ (Qt3 only)│
                                            └─────┬─────┘
                                   ┌───────────────┴───────────────┐
                                   │                               │
                          WinActivate(any)                  Timer 超时
                                   │                               │
                                   ▼                               ▼
                             ┌───────────┐                  ┌──────────┐
                             │ ATTACHED  │                  │ HIDDEN   │
                             └───────────┘                  └──────────┘
```

### 事件表

| # | 事件 | 当前状态 | 动作 | 下一状态 |
|---|------|---------|------|---------|
| 1 | WinActivate(tw≠this) | HIDDEN | m_activeWindow=tw, reposition, show | ATTACHED |
| 2 | WinActivate(tw≠this) | ATTACHED | m_activeWindow=tw, reposition | ATTACHED |
| 3 | WinActivate(tw==this) | 任意 | 忽略 | 不变 |
| 4 | WinMove/Resize(tw==m_activeWindow) | ATTACHED | reposition | ATTACHED |
| 5 | Close(tw==m_activeWindow) | ATTACHED | m_activeWindow=null, hide | HIDDEN |
| 6 | **Qt3**: WinDeactivate(tw==m_activeWindow) | ATTACHED | 启动 100ms debounce timer | PENDING |
| 7 | **Qt3**: Timer 超时 | PENDING | hide | HIDDEN |
| 8 | **Qt3**: WinActivate(any) | PENDING | 停止 timer | ATTACHED |
| 9 | **Qt4**: AppDeactivate | ATTACHED | hide | HIDDEN |
| 10 | **Qt4**: AppActivate | HIDDEN | if m_activeWindow: reposition, show | ATTACHED |

### Qt3 vs Qt4 策略

Qt3 没有 `ApplicationActivate`/`ApplicationDeactivate` 事件，只能通过 `WindowActivate`/`WindowDeactivate` 模拟。

| | Qt3 | Qt4 |
|---|---|---|
| 切到外部应用 | WinDeactivate → 100ms debounce timer → timeout → hide | ApplicationDeactivate → hide |
| 同应用内切换窗口 | WinDeactivate → timer start → WinActivate → timer stop → reposition | WinActivate → reposition |
| 从外部切回 | WinActivate → reposition (timer 已取消) | ApplicationActivate → reposition |

**Qt3 点击 statusbar 闪烁修复**：点击 statusbar 时 MainWindow 失活，但 100ms 内无其他 WinActivate 才隐藏。点击 statusbar 后立即抬起鼠标触发 WinActivate 重置 timer。实测本应用内部切换低于 100ms，外部应用切换通常 >100ms。

## 事件流

### 窗口激活
```
WindowActivate → eventFilter
  → [Qt3] 停止 debounce timer, clear pendingHide
  → watched->isWidgetType() && topLevelWidget() != this
  → m_activeWindow = tw
  → reposition() → move() + resize() + show()
```

### 切换到外部应用
```
[Qt3] WinDeactivate(tw==m_activeWindow)
  → start debounce timer (100ms, single shot)
  → 100ms 内无 WinActivate → onDebounceTimeout() → hide()

[Qt4] ApplicationDeactivate → hide()
```

### 窗口移动/缩放
```
Move/Resize on m_activeWindow → eventFilter → reposition()
```

### 窗口关闭
```
Close on m_activeWindow → eventFilter
  → m_activeWindow = nullptr → hide()
```

### 首次创建
```
instance()
  → new SharedStatusBar()  ← 窗口标志: Frameless | StaysOnTop
  → qApp->activeWindow() 存在时立即跟随
  → 调用者 show()
```

## 窗口属性

| 属性 | Qt3 | Qt4 |
|------|-----|-----|
| 无边框 | `WStyle_NoBorder` | `Qt::FramelessWindowHint` |
| 置顶 | `WStyle_StaysOnTop` | `Qt::WindowStaysOnTopHint` |
| 工具窗口 | — | `Qt::Tool` |

## API

| 方法 | 说明 |
|------|------|
| `static SharedStatusBar *instance()` | singleton 获取（不自动 show） |
| `showMessage(msg, timeout)` | 临时消息（透传内嵌 QStatusBar，不带类型图标，历史 type=Info） |
| `showMessageTyped(msg, type, timeout)` | 临时消息 + 类型图标（实时区 m_iconLbl + 历史菜单均显示） |
| `clearMessage()` | 清除临时消息 |
| `addWidget(w, stretch)` | 添加左对齐 widget |
| `addPermanentWidget(w, stretch)` | 添加右对齐 widget（不隐藏） |
| `removeWidget(w)` | 移除已添加的 widget |

## 消息类型图标

- `enum StatusMessageType`（Info=1 / Warning=2 / Error=3），数值对齐 `SticonIcon` / `SystemTrayIcon::MessageIcon`。
- 图标 = 16×16 彩色圆 + 白色加粗字形：Info 蓝 `#009EFF` + `i`、Warning 黄 `#FFC107` + `!`、Error 红 `#D02020` + `x`（配色/字形与托盘气泡一致）。由 `makeTypePixmap()` 运行时绘制，Qt3/Qt4 通用。
- **实时区**：图标为外层布局的 `QLabel *m_iconLbl`（位于 `m_historyBtn` 与 `m_bar` 之间）。因在 `m_bar` 外层，Qt3 `message()` 只隐藏内嵌 widget、不会隐藏它，故消息显示时图标紧邻文本左侧。`m_iconTimer` 单发与消息超时同步隐藏；`clearMessage()` 一并隐藏。
- **历史菜单**：每项左侧插对应类型图标（Qt3 `insertItem(pixmap)` / Qt4 `addAction(QIcon)`）。
- 老 `showMessage(msg, timeout)` 走 `doMessage(..., typed=false)`（public 行为不变，仅图标隐藏）；`showMessageTyped` 走 `typed=true`。

## 与 QMainWindow::statusBar() 的差异

| | QMainWindow::statusBar() | SharedStatusBar |
|---|---|---|
| 实例数 | 每窗口一个 | 全局一个 |
| 资源占用 | 多倍（窗口数倍） | 单倍 |
| 窗口类型 | 内嵌在 QMainWindow layout 中 | 独立浮动 QWidget |
| 跨窗口共享 | 否 | 是 |
| 实现复杂度 | Qt 框架内置 | 自实现 eventFilter 追踪 |

## 状态转换

```
初始 (m_activeWindow=null) ──WindowActivate──→ 跟随窗口A (visible at A.bottom)
                                                   │
                    ┌──────────────────────────────┼──────────────────────┐
                    ▼                              ▼                      ▼
           用户点击窗口B                   用户切换到外部应用          用户关闭窗口A
                    │                              │                      │
       WindowActivate(B)                  WindowDeactivate(A)          Close(A)
                    │                              │                      │
       m_activeWindow=B                    !qApp->activeWindow()    m_activeWindow=null
       reposition()                        hide()                   hide()
```

## 已知待解决

1. **首次位置**：`instance()->show()` 时 m_activeWindow 可能为 null → 显示在 (0,0)，直到收到 WindowActivate
2. **Qt3 debounce 窗口期**：100ms 是硬编码值，非常慢的 WM 边缘情况下可能误隐藏
3. **窗口管理器兼容**：`WStyle_StaysOnTop` 在不同 WM 下行为可能有差异

## 文件

- `qlcomp/sharedstatusbar.h` — 类声明
- `qlcomp/sharedstatusbar.cpp` — 实现
