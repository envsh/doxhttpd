# Supervisor Manager

## 简介
etapps UI App 插件：supervisord 控制管理（XML-RPC `/RPC2`）。双机制构建：`.so` 插件 + 独立 exe，遵循 `etapps/dummy/` 模式。

## 功能
- 连接区：`主机/端口/用户/密码`，密码框，配置自动保存到 `~/.config/supervisor_mgr/config.json`
- 进程列表：4 列 QTable（进程名 / 状态 / PID / 描述），`getAllProcessInfo` 获取
  - 状态色按官方状态码：RUNNING 绿 / STARTING·BACKOFF·STOPPING 橙 / EXITED 橙红 / FATAL 红 / STOPPED 灰 / UNKNOWN 浅灰
  - `state`（int 码）与 `statename`（字符串）字段严格按 supervisord XML-RPC 返回语义解析
- 操作：`启动` / `停止` / `重启`（`startProcess/stopProcess`，带 `wait=true`），选中行自动启用/禁用
  - supervisord 无单进程 `restartProcess` API → 重启为两阶段：`stopProcess` → 2s 延时 → `startProcess`，4.5s 后自动刷新
- 连接失败显式提示：双通道读取 stdout+stderr，匹配 11 种 curl 连接错误 + HTTP 401/404 + `<fault>`，
  状态栏红色显式提示、清空列表、禁用启停按钮（未开 RPC 的 supervisor 默认如此）
- 日志：多前缀 `[HH:MM:SS] [Supervisor] ...`，可清空
- XML-RPC：手写极简 XML 构造，curl `POST /RPC2`，`--user` 认证，`-sSk -i --connect-timeout 10 --max-time 30` 异步

## 构建
```bash
cd etapps/supervisor_mgr
./buildqt3.sh
```
产物：
- `../../../qltox/plugins/uiapps/libsupervisor_mgr_ui.so`
- `build-exe/supervisor_mgr_ui`

## 技术要点
- Qt3 优先，`#ifdef QT3_BUILD` 适配（Qt4 分支已用系统 qt4 头文件全源码语法级验证通过）
  - QProcess：Qt3 `processExited()/addArgument/start()/readStdout`；Qt4+ `finished(int,QProcess::ExitStatus)/start(program,args)/readAllStandardOutput`
    （Qt 官方规则：string-based connect 允许槽签名比信号短，故两端共用无参槽 `onProcDone()` + `sender()`）
  - 布局统一走 `qNewBoxLayout(parent, dir, border, spacing)`（compat34），不混用 Qt3 风格布局 ctor
  - 着色：Qt3 子类化 `QTableItem::paint` 改 `QColorGroup::Text`；Qt4 `QTableWidgetItem::setForeground(QBrush)`
- 进程数据：固定数组 `SupProcInfo procs[512] + procCount`（同 v2ray_suber 规范，不用列表类）
- C++11，无异常
- 全部中文 `qFromUtf8()`，控制体 `{}`
- `qlcomp/compat34.cpp`、`qlcomp/qthooks.cpp`、`qlcomp/cJSON.c`
- `plugin_create()` 只调用 `createSupervisorMgrMainWindow(parent)`
- dummy 零改动
