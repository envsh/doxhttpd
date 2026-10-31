# LinkSwift（网盘直链下载助手）

## 简介

将 [LinkSwift](https://github.com/hmjz100/LinkSwift) 的网盘直链获取思路移植为 `qltox` 的 `etapps` UI App 插件。该插件以 Qt 界面形式集成网盘直链提取、认证信息配置及下载器推送流程，遵循本项目的 etapps 插件机制设计。

## 功能

- **平台选择**：支持百度网盘、阿里云盘、夸克网盘、UC 网盘、迅雷云盘、123 云盘、天翼云盘、中国移动云盘等主流网盘
- **分享链接输入**：支持输入网盘分享链接及提取码/密码
- **认证信息**：提供 Cookie/Token 填写区，用于需要登录态的平台
- **下载器设置**：支持 Aria2（JSON-RPC）、Motrix、IDM、AB Download Manager、BitComet、cURL
- **直链操作**：获取直链、复制全部、推送到下载器、清空
- **结果日志**：以只读文本框展示解析结果及状态日志

## 文件结构

```
etapps/link_swift/
├── plugin.pro    # 插件库构建配置（生成 liblink_swift_ui.so）
├── main.cpp      # 插件接口实现（plugin_name/version/description/create/destroy）
├── mainwin.cpp   # UI 构造入口（createLinkSwiftMainWindow）
├── exe.cpp       # 独立可执行文件入口
├── exe.pro       # 独立程序构建配置（生成 link_swift_ui）
├── buildqt3.sh   # Qt3 双段构建脚本（插件 + 独立 exe）
└── README.md     # 本文档
```

## 构建

严格遵循 `etapps/dummy/` 的双机制（插件 `.so` + 独立 `exe`）。

```bash
cd etapps/link_swift
./buildqt3.sh
```

也可直接调用 `etapps/build_all.sh` 统一编译所有插件。

## 产物

| 类型 | 路径 | 说明 |
|---|---|---|
| 插件库 | `../../../qltox/plugins/uiapps/liblink_swift_ui.so` | 由插件管理器加载的 UI App |
| 独立程序 | `build-exe/link_swift_ui` | 独立运行的 App 版本 |

## 设计约定

- **调用链**：`main.cpp: plugin_create()` 仅调用 `mainwin.cpp` 的 `createLinkSwiftMainWindow(parent)`，严禁反向调用
- **Qt 兼容**：使用 `compat34.h` 处理 Qt3/Qt4 差异，保留 Qt3 自动探测块
- **构建规范**：`MOC_DIR = .`、`OBJECTS_DIR = .`，`.o` 不放入 `SOURCES`，仅通过 `LIBS +=` 链接
- **代码风格**：C++11，不抛异常、不使用 try-catch
- **国际化**：所有中文字符串使用 `qFromUtf8()` 包装，所有控制体（`if/for/while`）必须使用 `{}`
- **接口遵循**：实现 `plugin_api.h` 定义的 C 接口（`plugin_name`、`plugin_version`、`plugin_description`、`plugin_create`、`plugin_destroy`）

## 使用说明

1. 在 `qltox` 主界面通过 **Etapps** 菜单启动 `LinkSwift`
2. 选择网盘平台，输入分享链接及提取码
3. 如需认证，在 Cookie/Token 区填写相关信息
4. 配置下载器（推荐 Aria2 RPC），点击 **获取直链**
5. 可直接 **复制全部** 或 **推送到下载器** 下载

## 说明

当前版本以 LinkSwift 的交互流程和 UI 骨架为主，重点呈现网盘直链提取、认证配置及下载器对接的操作路径。具体平台的解析逻辑可按需后续扩展。
