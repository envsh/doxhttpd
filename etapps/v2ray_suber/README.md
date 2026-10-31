# V2Ray Suber（V2Ray 订阅管理器）

## 简介

将 [v2rayN](https://github.com/2dust/v2rayN)（Windows/Linux）与 [V2rayU](https://github.com/yanue/V2rayU)（macOS）的**订阅管理 + 核心（v2ray-core）下载安装与启停管理**思路，移植为 qltox 的 etapps UI App 插件。

- 参考 v2rayN/v2rayNG/V2rayU 的“订阅 → 节点 → 核心 → 连接”主流程
- 本插件内置 v2ray-core 下载安装与一键启停，并通过本地 SOCKS 端口接入
- 插件机制严格遵循 `etapps/dummy/` 双构建模式（`.so` 插件 +独立 exe）

## 核心版本说明（重要）

- **v2ray-core 版本固定为 `v5.35.0`**，不自动检测新版本、不升级
- 下载源通过 **gh-proxy.com** 加速前缀下发，提升国内环境可用性：

```
https://gh-proxy.com/https://github.com/v2fly/v2ray-core/releases/download/v5.35.0/v2ray-linux-64.zip
```

- 目前支持 Linux x86_64（`v2ray-linux-64.zip`）、aarch64（`v2ray-linux-arm64-v8a.zip`）两种包

## 功能

### 1. 订阅管理
- 添加订阅（校验 `http://` / `https://` 前缀）
- 删除订阅（选中条目后删除）
- 更新订阅（curl 下载 → base64 解码 → 解析节点列表）
- 订阅列表持久化存储于 `~/.config/v2ray_suber/subscriptions.txt`
- 最近一次订阅原始内容保存于 `~/.config/v2ray_suber/sub_last.txt`

### 2. 节点管理
- 订阅协议解析：
  - `vmess://`（base64 JSON，pq 解析 uuid、addr、port、net、path、host、tls、ps）
  - `vless://`（URI 方式解析 uuid@addr:port + 查询参数 type/path/host/security）
  - `trojan://`（URI 解析 password@addr:port + 查询参数）
  - `ss://`（仅展示原始链接，暂不支持一键生成配置）
- 节点列表展示：名称 / 协议 / 地址端口
- 复制节点原始分享链接（QClipboard）
- 选中节点后一键 START/STOP

### 3. v2ray-core 下载安装与管理（固定 v5.35.0）
- 状态检测：未安装 / 已安装 / 运行中
- 下载流程：
  1. 构造 gh-proxy.com 加速 URL（锁定 v5.35.0 包）
  2. `curl -L -o` 由 QProcess 后台执行，不阻塞 UI
  3. `unzip -o` 解压至 `~/.config/v2ray_suber/core/`
  4. `chmod 0755` 设置 `v2ray` 可执行
  5. 清理临时 zip
- 启停管理：
  - `启动核心`：选中节点生成 `config_run.json` → `QProcess` 执行 `v2ray run -c config_run.json`
  - `停止核心`：发送 `tryTerminate()`，由 UI 状态栏同步显示
- 运行日志同步到“日志”标签页

### 4. 本地接入
- SOCKS 监听：`127.0.0.1:10808`（config 默认入口）
- 路由未细化（简化版），适合测试性连接

## 文件结构

```
etapps/v2ray_suber/
├── plugin.pro     # 插件库构建配置 → libv2ray_suber_ui.so
├── main.cpp       # plugin_name/version/description/create/destroy
├── mainwin.cpp    # UI 构造入口 createV2raySuberMainWindow（4 个 Tab）
├── logic.h        # V2RayLogic（Q_OBJECT 类，承载订阅/核心逻辑）
├── logic.cpp      # 订阅解析、核心下载/启停、配置生成
├── exe.cpp        # 独立 exe 入口
├── exe.pro        # 独立程序构建配置
├── buildqt3.sh    # dummy 两段式构建脚本（Qt3）
└── README.md      # 本文档
```

## 构建

严格遵循 `etapps/dummy/` 的双机制（插件 `.so` + 独立 `exe`）。

```bash
cd etapps/v2ray_suber
./buildqt3.sh
```

首次完整流程：
1. 输入订阅 URL → 点击“添加”
2. 选中订阅 → 点击“更新选中订阅”（节点进入“节点”列表）
3. 切到“核心”页 → 点击“下载安装”（等待下载/解压/权限设置）
4. 回到“节点”页 → 选中节点 → 点击“启动核心”
5. 任选 SOCKS 客户端指向 `127.0.0.1:10808`

## 产物

| 类型 | 路径 | 说明 |
|---|---|---|
| 插件库 | `../../../qltox/plugins/uiapps/libv2ray_suber_ui.so` | 由 qltox 插件管理器加载 |
| 独立程序 | `build-exe/v2ray_suber_ui` | 独立运行版本 |

## 运行时数据目录

```
~/.config/v2ray_suber/
├── subscriptions.txt      # 订阅 URL 列表（每行一条）
├── sub_last.txt           # 最近一次订阅原始内容
├── v2ray-core.zip         # 下载中的临时包（成功后删除）
└── core/                  # v2ray-core 安装目录
    ├── v2ray              # 可执行文件（0755）
    ├── geoip.dat / geosite.dat（由发布包带出）
    └── config_run.json    # 上一次启动时生成的运行配置
```

> config_run.json 内容为简化版 v2ray 配置（单 inbound SOCKS:10808 + 单 outbound 节点），写入到 `core/` 目录。

## UI 结构

| Tab | 控件 | 说明 |
|---|---|---|
| 订阅 | `QLineEdit` + 增删更按钮 + `QListBox` | 订阅 URL 管理与刷新 |
| 节点 | `QListBox` + 复制按钮 | 展示解析节点，支持选中→启动核心 |
| 核心 | 状态 `QLabel` + 下载/启动/停止按钮 | 核心 v5.35.0 管理入口 |
| 日志 | `QTextEdit`（只读追加） | 输出订阅、下载、核心运行日志 |

## 技术要点

### 1. 订阅解析
- base64 全文可能使用了多行、去空忽略的策略（custom `b64Decode`）
- `vmess://` 用 `cJSON`（qlcomp 内置）解析 JSON
- `vless://`/`trojan://` 用手写 URI 切分（`@`、`:`、`/`、`?`、`#` 定位）
- 还原 URL 参数到 V2Node 结构体字段（net/path/host/tls）

### 2. 核心（v2ray-core）管理
- `QProcess` 后台执行 `curl` / `unzip` / `v2ray run`
- 无 try-catch / 抛异常；所有错误通过返回值或日志传递
- 核心 zip 下载前判断是否已存在（`QFile::exists`）
- 核心可执行文件 `chmod 0755` 由 `access(..., X_OK)` 状态检测

### 3. UI 兼容
- 所有中文基于 `qFromUtf8()`（compatcore34.h）
- 控制体均使用 `{}`；C++11 标准、不抛异常
- Qt3 直接实现（dummy 机制暂按 Qt3 主线）；后续 Qt4 兼容需另行适配 `QListBox→QListWidget`、`QProcess` 参数 API、`QDir::homeDirPath` 等
- `logic.h/logic.cpp` 单独抽出逻辑类（有 Q_OBJECT，qmake 自动 moc）
- `mainwin.cpp` 中通过 `connect(btn, SIGNAL(clicked()), logic, SLOT(...))` 把按钮事件路由到 logic

### 4. dummy 机制约束
- `main.cpp: plugin_create()` 仅调用 `createV2raySuberMainWindow()`（调用链单向）
- `plugin.pro` 的 `DESTDIR` 输出到 `qltox/plugins/uiapps/`
- `exe.pro` 的 `TARGET = v2ray_suber_ui`，输出 exe
- `MOC_DIR = .` / `OBJECTS_DIR = .`
- `SOURCES` 不放 `.o`，`cJSON.c` **直接作为源码包含**（无 .o 链接）
- 不修改 `etapps/dummy/`、`etapps/dummy_noui/` 及既有插件

## 使用建议
- 若订阅链接是“明文 YAML/原始 JSON”，第一次解析失败，先点一次“更新选中订阅”刷新
- 若 gh-proxy.com 暂不可用，可临时修改 `logic.cpp` 中 `V2CORE_GH_PREFIX` 或采用外置下载
- 若核心启动立刻退，切到“日志”页查看 v2ray 输出，常见原因是节点信息/auth 缺失
- `v2ray-core` 由 gh-proxy 加速下载，必要时可手工从 GitHub Releases 下载后解压到 `~/.config/v2ray_suber/core/`

## 安全与合规
- 本插件仅用于学习与研究目的
- 订阅与节点信息仅保存在本机 `~/.config/v2ray_suber/`
- 使用请遵守你所在国家/地区的法律法规

## TODO
- [ ] v2ray-core 更多协议 / 传输方式（mKcp、grpc、httpUpgrade）
- [ ] 节点延迟测试（ping / tcp connect）
- [ ] 订阅手动更新[^2]手动更新全部
- [ ] 路由/分流设置（自定义路由规则）
- [ ] 系统代理环境变量写入
- [ ] Qt4 完整适配

[^2]: 当前实现为“只有选中或第一个订阅”逐个更新。
