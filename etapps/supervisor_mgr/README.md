# Supervisor Manager

## 简介
etapps UI App 插件：supervisord 控制管理（XML-RPC `/RPC2`）。双机制构建：`.so` 插件 + 独立 exe，遵循 `etapps/dummy/` 模式。

## 功能
- 连接区：`主机/端口/用户/密码`，密码框，配置自动保存到 `~/.config/supervisor_mgr/config.json`
- 进程列表：`getAllProcessInfo` 获取，显示 `name/group:state [STATE] pid=...`
- 操作：`启动` / `停止` / `重启`（`startProcess/stopProcess/restartProcess`，均带 `wait=true`），操作后延时刷新
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
- Qt3 优先，`#ifdef QT3_BUILD` 适配
- C++11，无异常
- 全部中文 `qFromUtf8()`，控制体 `{}`
- `qlcomp/compat34.cpp`、`qlcomp/qthooks.cpp`、`qlcomp/cJSON.c`
- `plugin_create()` 只调用 `createSupervisorMgrMainWindow(parent)`
- dummy 零改动
