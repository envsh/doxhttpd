# Restman（REST 请求管理器）

## 简介

etapps UI App 插件：轻量 REST 客户端（Postman 思路的极简版）。

- 发送 HTTP 请求（`QProcess`+`curl` 异步实现，全程不阻塞 UI 线程）
- **历史记录：自动保留最近 30 条**（超出移除最旧条），持久化于本地 JSON
- **Bookmark 收藏：保存某次请求的完整参数**——method / url / query / headers / body 类型与内容 / auth 类型与凭据全部字段一并保存，双击（选中+加载）完全还原到编辑区
- 插件机制严格遵循 `etapps/dummy/` 双构建模式（`.so` 插件 + 独立 exe）

## 功能

### 请求编辑区
- Method：`GET/POST/PUT/PATCH/DELETE/HEAD/OPTIONS`
- URL：`QLineEdit`
- Query：多行 `key=value`，发送时自动拼接到 URL（`?`/`&` 智能判断）
- Headers：多行 `Key: Value`
- Body：三型开关——`无` / `原始(raw)`（默认补 `Content-Type: application/json`，可用 Headers 显式覆盖）/ `表单(urlencoded)`（补 `x-www-form-urlencoded`）
- Auth：`无` / `Basic`（`curl --user user:pass`）

### 发送与响应
- curl 参数：`-sSk -i --connect-timeout 10 --max-time 30 -X <METHOD> [-H ...] [--data ...] [--user ...] <URL>`
- `-i` 取回响应头；按空行拆分状态行/响应头/Body，分别展示
- 响应区：`状态：<HTTP 状态行>  <耗时>ms`，`响应 Body`、`响应 Headers`、`日志/原始` 三页
- 失败提示：超时/拒绝/解析失败 + `curl exit=` 错误码中文映射
- `-k` 默认携带（内网/自签证书场景）

### 历史（上限 30）
- 每次发送即入历史（与最近一条完全相同则跳过）
- 上限 30 条，超出自动移除最旧条
- 持久化：`~/.config/restman/history.json`
- 按钮：加载（还原完整参数到编辑区）/ 删除 / 清空

### 收藏（Bookmark，含完整参数）
- 点击“保存收藏”保存**当前编辑区全部字段**（method/url/query/headers/bodyType/bodyText/authType/authUser/authPass/时间戳）
- 容量上限 256 条（到达后替换最旧）
- 持久化：`~/.config/restman/bookmarks.json`
- 按钮：加载 / 删除
- “加载”后**所有编辑控件完全还原**（含 Body 类型与内容、Auth 凭据），可直接再次“发送”

## 文件结构

```
etapps/restman/
├── plugin.pro     # 插件库输出 librestman_ui.so
├── main.cpp       # plugin_name/description/version/create
├── mainwin.cpp    # UI 构造 createRestmanMainWindow
├── logic.h        # RestmanLogic（Q_OBJECT）
├── logic.cpp      # 快照/JSON 持久化/curl 异步发送/响应解析
├── exe.cpp        # 独立 exe 入口
├── exe.pro
├── buildqt3.sh    # 两段式构建（照抄 dummy）
└── README.md
```

## 数据存储

```
~/.config/restman/
├── history.json    # 最近 30 条（自动维护）
└── bookmarks.json  # 收藏（含完整参数）
```

JSON 结构（两者同构，数组元素字段齐全，向后兼容缺字段）：

```json
{
  "history": [
    { "method":"POST", "url":"http://...", "query":"...", "headers":"...",
      "bodyType":"raw", "bodyText":"{...}", "authType":"none",
      "authUser":"", "authPass":"", "createdAt":"2026-10-31 12:34:56" }
  ]
}
```

## 构建

```bash
cd etapps/restman
./buildqt3.sh
```

产物：
- `../../../qltox/plugins/uiapps/librestman_ui.so`
- `build-exe/restman_ui`

## 技术要点

- 快照结构 `RestManSnap`（histArr[30]/bmArr[256] 定长数组，Qt3/Qt4 通用、无异常）
- JSON 用 `qlcomp/cJSON`（`cJSON_Print` 生成缩进文件，`cJSON_Parse` 读取，缺字段容错）
- 全部中文 `qFromUtf8()`、控制体 `{}`、Qt3 优先
- qthooks.cpp 已纳入编译（qlcomp 日志钩子，与 qltox 风格一致）
- `plugin_create() → createRestmanMainWindow()` 单向调用链（dummy 机制）
