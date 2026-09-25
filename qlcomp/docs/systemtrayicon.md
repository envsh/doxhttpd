# SystemTrayIcon - 系统托盘图标（Qt3/Qt4 双兼容）

## 目标

qlcomp 托盘图标组件，API 对齐 Qt4 `QSystemTrayIcon` 范式。Qt3 无原生托盘类（Qt 4.2 才引入），采用**原样引入 Psi 0.10 TrayIcon + 适配层**方案。首个使用场景：qltox 最小化到托盘。

## 架构

```
SystemTrayIcon : QObject            ← 适配层（systemtrayicon.h/cpp，类体内零 #ifdef）
├── [Qt4] QSystemTrayIcon *native   ← 原生薄封装（枚举值相同，cast 转发）
├── [Qt3] SystemTrayIconAdapter     ← 子类化 Psi TrayIcon（友元调 fireActivated）
│   └── TrayIcon (Psi 原样引入)
├── bool m_visible                  ← 自维护可见标志
└── activated(int)                  ← 统一信号
```

## 文件清单

| 文件 | 来源 | 角色 |
|------|------|------|
| `trayicon.h/cpp` | Psi 0.10-2（见下方来源与版本记录）原样拷入 | 公共逻辑（LGPL 2.1 头保留） |
| `trayicon_x11.cpp` | 同上 | freedesktop 协议实现 |
| `trayicon_win.cpp` | 同上 | Shell_NotifyIcon 实现 |
| `systemtrayicon.h/cpp` | 新写 | 适配层 |

mac 版 `trayicon_mac.cpp` 未引入（项目无 mac 构建目标），需要时从同源补抓并加 `macx` scope。

## 来源与版本记录

| 项 | 值 |
|----|-----|
| 上游项目 | Psi <https://psi-im.org> |
| 版本 | 0.10（Debian 修订版 **0.10-2**，Qt3 终版） |
| 文件 | trayicon.h / trayicon.cpp / trayicon_x11.cpp / trayicon_win.cpp |
| 来源 URL | <https://sources.debian.org/data/main/p/psi/0.10-2/src/tools/trayicon/> |
| 抓取日期 | 2026-08-21 |
| 许可证 | LGPL 2.1（原头注释完整保留，版权 Justin Karneges 2003 等） |
| 本地修改 | 无（原样拷贝），仅编译进 Qt3 构建 |
| 上游状态 | Psi 0.11+ 切换 Qt4 时整体删除此组件改用原生 QSystemTrayIcon；官方从未做 Qt3/Qt4 双兼容 |

> 升级提示：若需更新此组件，从 Debian 同路径取更高修订版；
> 不要期待上游新版本——0.11 起该目录已不存在。

## API 设计要点

- `PopupMenu` / `TrayActivationReason` typedef 在 `systemtrayicon.h` 内部定义（不改 compat34.h），参照 EmbeddedMenuBar.h 的 MenuWidget34 先例；`#ifdef` 均在类体之外（moc 安全）。
- 枚举 `ActivationReason` 数值与 Qt4 原生一致：Unknown=0, Context=1, DoubleClick=2, Trigger=3, MiddleClick=4。
- Qt4 分支通过 `SIGNAL(activated(QSystemTrayIcon::ActivationReason))` → slot `nativeActivated(TrayActivationReason)` 中继；typedef 保证字符串签名精确匹配。
- Qt3 分支无 cpp 内 Q_OBJECT 类（qlcomp 无此先例）：`SystemTrayIconAdapter` 无 Q_OBJECT，重写四个鼠标事件处理函数直接调 `fireActivated()`（friend 访问），不走 connect。
- `isSystemTrayAvailable()`：Qt3/X11 查 `_NET_SYSTEM_TRAY_S{screen}` selection owner；Qt4 用原生静态方法。

## 信号映射（Psi → Qt4 语义）

| 鼠标动作 | activated(reason) |
|----------|-------------------|
| 左键按下 | Trigger(3) |
| 中键按下 | MiddleClick(4) |
| 双击左键 | DoubleClick(2)（双击序列会先触发一次 Trigger，与 Qt4 原生行为一致） |
| 右键按下 | Context(1)，随后弹上下文菜单 |

## X11 dock 协议要点（trayicon_x11.cpp，源自 Psi/KDE3 成熟实现）

1. 窗口：`QWidget(0,"psidock",WRepaintNoErase)` + `setBackgroundMode(X11ParentRelative)`，固定 22×22
2. WM 标识：`XSetClassHint` + `XWMHints`（WithdrawnState 初始态）
3. 找托盘：`XGrabServer` → `XGetSelectionOwner("_NET_SYSTEM_TRAY_S{screen}")` → `XUngrabServer`
4. dock：ClientMessage `_NET_SYSTEM_TRAY_OPCODE` format=32 l[1]=0(REQUEST_DOCK) l[2]=winId()，XSendEvent 前后 trap X 错误
5. KDE 兼容属性：`KWM_DOCKWINDOW` + `_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR`
6. 收到 `ReparentNotify` 后才 `show()`
7. `enterEvent` 合成 FocusIn（修复托盘弹出菜单键盘焦点）
8. tooltip：`QToolTip::add/remove`

## 平台/版本矩阵

| | Qt3 | Qt4 |
|---|---|---|
| Linux/X11 | trayicon_x11.cpp | 原生 QSystemTrayIcon |
| Windows | trayicon_win.cpp（真实可用） | 原生 |

## 构建注册（qlite.pri）

```qmake
isEmpty(QT_VERSION) {
    QTCOMP_HDR += $$PWD/trayicon.h
    QTCOMP_CPP += $$PWD/trayicon.cpp
    unix:!macx { QTCOMP_CPP += $$PWD/trayicon_x11.cpp }
    win32      { QTCOMP_CPP += $$PWD/trayicon_win.cpp }
}
```

关键决策：

1. **必须用 `isEmpty(QT_VERSION)` 而非 `contains(DEFINES, QT3_BUILD)`**——qmake 顺序求值，qltox.pro 在 include(qlite.pri) 之后才 `DEFINES += QT3_BUILD`，include 时 DEFINES 里还没有该值。QT_VERSION 由 qmake 核心预置，任何位置可用。
2. **全部 Psi 文件只进 Qt3 构建**——trayicon.cpp 使用 qpopupmenu.h、TRUE/FALSE、双参 QObject 构造、非作用域 RightButton，在 Qt4 下全是编译错误。Qt4 分支零接触 Psi 文件。
3. X11 链接复用 qlite.pri 已有的 `unix:!macx: LIBS += -lXss -lX11`。

## 图标缩放

Psi 的托盘窗口固定 22×22 且 paintEvent 只居中不缩放，大图标会被裁剪成左上角。适配层在 setIcon 时统一预缩放：`convertToImage().smoothScale(22,22,QImage::ScaleMin)`。qltox 的 app_icon.xpm 为 64×64，依赖此逻辑。

## 已知限制（v1）

- showMessage 气泡：Qt4 原生 / Qt3 自绘 TrayBubble（贴托盘图标、去重×N、FIFO 上限 3、200ms 冷却），详见 [systemtrayicon-showmessage.md](./systemtrayicon-showmessage.md)。
- 托盘程序重启后不自动重注册（需重新 show 或重启应用）。
- 无托盘环境 setVisible(true) 静默失败（m_visible 保持 false，与实际状态一致）。

## qltox 集成

- mainwindow 成员 `SystemTrayIcon* m_tray`、slot `trayActivated(int)`、closeEvent override。
- ctor：用 app_icon.xpm 创建，tooltip=应用名，右键菜单（打开/退出）。
- closeEvent：托盘可见且非强制退出时 hide()+ignore；`isSystemTrayAvailable()==false` 时正常退出（否则应用不可达）。
- trayActivated：Trigger/DoubleClick → showNormal+activateWindow（用 compat34.h 的 qActivateWindow）。

## 许可证合规

trayicon*.{h,cpp} 保留 LGPL 2.1 版权头原文（Justin Karneges 等）；本目录其余代码不受影响。
