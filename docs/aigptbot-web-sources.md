# Aigptbot Web 接口参考来源核对 AigptbotWebSources

## 0. 概述

- 核对日期：**2026-10-08**
- 用途：为 qltox `aigptbot` 新增 4 个 web provider（`chatgpt-web` / `deepseek-web` / `gemini-web` / `grok-web`）提供接口依据、凭据提取方法与参考来源清单。
- 范围：仅文档记录。对应实现见 `qltox/aigptbot.h` / `aigptbot.cpp`（凭据常量为 `aigptbot.cpp` 顶部各 `kXxxWebCred` 空变量，填值后重编译）。

## 1. 四家接口核对表

| Provider | 端点（要点） | 凭据 | 反爬 | 响应格式 | 状态 |
|---|---|---|---|---|---|
| deepseek-web | `POST chat.deepseek.com/api/v0/chat_session/create` → `POST /api/v0/chat/completion` | `Authorization: Bearer <userToken>`（localStorage） | `x-ds-pow-response` PoW（可本地解） | SSE | ✅ 当前有效 |
| gemini-web | GET 首页取 `SNlM0e` 作 `at` → `POST gemini.google.com/_/LabsTailwindUi/data/assistant.lamda.BardFrontendService/StreamGenerate`（batchexecute `f.req` 表单） | Cookie `__Secure-1PSID` + `__Secure-1PSIDTS` | Google 可能校验 TLS 指纹（参考库用 curl_cffi） | 长度分帧 JSON | ✅ 当前有效 |
| grok-web | `POST grok.com/rest/app-chat/conversations/new`（及 `conversations/:id/message`） | Cookie `sso` + `sso-rw`（JWT） | `x-statsig-id` JS 挑战（需本地复现签名） | **NDJSON 流**（非标准 SSE） | ⚠️ 有效但易变 |
| chatgpt-web | sentinel `chat-requirements/prepare`→`finalize` → `POST chatgpt.com/backend-api/f/conversation` | accessToken（session 接口） | PoW（SHA-256 可解）+ **Turnstile**（普遍被卡） | SSE | ❌ 受限，实验性 |

## 2. 参考来源清单（时效性核对）

| Provider | 来源 | 活跃度证据 | 有效性结论 |
|---|---|---|---|
| deepseek | `github.com/NIyueeE/ds-free-api`（784★，Rust，母仓）、其 fork `github.com/Hucclp/deepseek-web-api`（API.md）、`github.com/alive2/deepseek-chat-api`、`github.com/sums001/Deepseek-API`、`github.com/Ametist298/deepseek-free-api` | 母仓 pushed **2026-10-08（核对当天）**，issue 均为近期 | ✅ 当前有效：PoW 路径 `/api/v0/chat/completion` 已被 PR #109 抓上游配置（`pow_header_paths`/`authed_pow_functions`）核对确认；auth = localStorage `userToken` |
| gemini | `github.com/HanaokaYuzu/Gemini-API`（gemini-webapi，3564★）、`github.com/Sophomoresty/gemini-web2api` | pushed 2026-08-27；最新 issue 2026-10-03/04 均为正常使用中的改进讨论（如 #364 `card_content` 解析），无"失效"报告 | ✅ 当前有效：`__Secure-1PSID`/`__Secure-1PSIDTS` + StreamGenerate；PR #361 指出 `__Secure-1PSID` 会变、库依赖 curl_cffi **TLS 指纹伪装** |
| grok | `github.com/imjustprism/grok-web-api`（Rust）、`github.com/ManojINaik/grokAPI`、`github.com/anojndr/grok-to-openai`、`github.com/mem0ai/grok3-api`、`pypi.org/project/grok-api`、`github.com/Sexlovr/grok-2-api`、`github.com/carzygod/grok2api` | grok-web-api pushed 2026-06-17；PR #7（2026-07-26）live 验证 | ⚠️ 有效但易变：响应为 NDJSON 流；`modelMode` wire 值为 `MODEL_MODE_*` 枚举（2026-07 Grok 4.5 升级曾致 403），免费账号仅 `fast` 模式可用；另有 `x-statsig-id` 挑战头与 WebSocket 网关路线并存（`anojndr/grok-to-openai`），实现时需实测确认 |
| chatgpt | `github.com/Octo-Lex/ChatGPT-Web2API`（协议参考）、`github.com/adam-s/toolkit`（TURNSTILE.md）、`gpt2agent` 实测报告（2026-09-08） | gpt2agent 实测：sentinel Turnstile 阶段 blocked | ❌ 受限：PoW 可解，Turnstile 第三方普遍被卡，`/backend-api/f/conversation` 返回 403 → 按既定决策标"实验性" |

## 3. 凭据手工提取指南

通用：登录目标站点 → F12 DevTools → Application（Chrome/Edge）/ Storage（Firefox）查看 Cookie 与 Local Storage；或 Network 面板右键请求 **Copy as cURL** 从 `-H 'cookie: ...'` / `authorization: ...` 抄取。填入 `qltox/aigptbot.cpp` 顶部对应 `kXxxWebCred` 空常量后重编译（Qt3 → Qt4 顺序，勿并行）。

### 3.1 deepseek-web → `kDeepseekWebCred`

1. 登录 `chat.deepseek.com`
2. DevTools → Application → Local Storage → `https://chat.deepseek.com`
3. 找 key **`userToken`**（值为 JSON，形如 `{"value":"<长串>","ttl":...}`）
4. 复制 `.value` 内的长串填入常量（**不带引号、不带 `Bearer` 前缀**，代码拼 `Bearer <值>`）

备选：Network 里任一 `/api/v0/*` 请求 → Request Headers → `authorization: Bearer xxx` 直接抄 `xxx`。

### 3.2 gemini-web → `kGeminiWebCred`

1. 登录 `gemini.google.com`
2. DevTools → Application → Cookies → `https://gemini.google.com`
3. 分别复制 **`__Secure-1PSID`** 和 **`__Secure-1PSIDTS`** 的值
4. 填入格式（两段 cookie 对拼接，代码直接作 Cookie 头）：

```
__Secure-1PSID=<值1>; __Secure-1PSIDTS=<值2>
```

备选：Network 里任一首页/StreamGenerate 请求 → Copy as cURL → 抄完整 cookie 头。

注：`at`（SNlM0e）由代码运行时自动获取，无需手工提取；`__Secure-1PSID` 会不定期变化（参考来源 PR #361），失效后需重新提取。

### 3.3 grok-web → `kGrokWebCred`

1. 登录 `grok.com` 并发送一条消息
2. Network → 过滤 `conversations/new` → 右键 **Copy as cURL**
3. 从 `-H 'cookie: ...'` 抄完整 cookie 串填入常量（至少含 **`sso`** 与 **`sso-rw`**，通常为同一 JWT）

或 Application → Cookies → `https://grok.com` 分别复制 `sso`、`sso-rw` 后拼成：

```
sso=<值1>; sso-rw=<值2>
```

注：登出/清缓存会使 cookie 轮换；另有 `x-statsig-id` 挑战头由代码复现（非 cookie，见 §5 风险）。

### 3.4 chatgpt-web → `kChatgptWebCred`（实验性）

1. 登录 `chatgpt.com`
2. 浏览器地址栏打开 `https://chatgpt.com/api/auth/session`
3. 取 JSON 中 **`accessToken`** 字段值（`eyJ...` 开头）填入常量

备选：Application → Cookies → `https://chatgpt.com` → `__Secure-next-auth.session-token`（由代码换取 session，首选 accessToken 直填）。

注：accessToken 有有效期，失效后重新访问 session 接口；且即使凭据有效仍可能被 Turnstile 拦截（见 §5）。

## 4. 对实施的映射

- 凭据空常量：`aigptbot.cpp` 顶部 `kDeepseekWebCred` / `kGeminiWebCred` / `kGrokWebCred` / `kChatgptWebCred`（`""`，用户填值重编译；**不改 config.json、不做设置页**）。
- `buildCandidates()` 中凭据为空 → `any`/`all` 跳过、点名单选报"未配置凭据"。
- 每条消息新会话（无状态，不缓存 session id）；内部可请求 SSE 但聚合后单次返回（`parseSseContent()` 聚合 delta），保持现有完成事件语义。
- 阶段划分：P0 架构（enum/`webKind` 字段/`sendWebHost()` 状态机/sha256.cpp）→ P1 deepseek-web → P2 gemini-web → P3 grok-web → P4 chatgpt-web（实验性，失败给明确报错）。
- 不改：Go server、`web/`、`messageattribbar.cpp`（provider 下拉动态生成，新 provider 自动出现）、设置对话框、`build.sh`。

## 5. 共性风险

1. **TLS/浏览器指纹**：四家均可能校验浏览器指纹（Cloudflare `cf_clearance`、Google TLS/JA3 指纹、grok `x-statsig-id` 挑战）。qltox 用 libcurl 直连，指纹与 Chrome 不同，可能被拦——实现阶段需实测的变量。
2. **deepseek 风控封号**：参考来源 issue #112（4 账号 10 分钟~12 小时被封）、#109/#102（e2e 后禁言 3 天）。qltox 单发低频使用风险较低但非零，使用前须知。
3. **逆向接口随时可能变更**：§2 来源清单即后续排查起点（如 grok `modelMode` 403、gemini `__Secure-1PSID` 轮换）。
4. **凭据敏感**：cookie/token 等同账号密码，填入源码常量意味着本地明文保存，勿提交到公开仓库。
