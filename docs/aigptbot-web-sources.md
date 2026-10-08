# Aigptbot Web 接口参考来源核对 AigptbotWebSources

## 0. 概述

- 核对日期：**2026-10-08**
- 用途：为 qltox `aigptbot` 新增 4 个 web provider（`chatgpt-web` / `deepseek-web` / `gemini-web` / `grok-web`）提供接口依据、凭据提取方法与参考来源清单。
- 范围：仅文档记录。对应实现见 `qltox/aigptbot.h` / `aigptbot.cpp`（凭据常量为 `aigptbot.cpp` 顶部各 `kXxxWebCred` 空变量，填值后重编译）。

## 1. 四家接口核对表

| Provider | 端点（要点） | 凭据 | 反爬 | 响应格式 | 状态 |
|---|---|---|---|---|---|
| deepseek-web | `GET hif-leim.deepseek.com/query` → `POST /api/v0/chat/create_pow_challenge` → `POST /api/v0/chat_session/create` → `POST /api/v0/chat/completion`（详见 §6.1） | `Authorization: Bearer <userToken>`（localStorage） | App UA `DeepSeek/2.5.0 Android/35`（默认，可被全局 `kWebCredUA` 覆盖；桌面 Chrome UA 触发 AWS WAF 202）+ `X-Hif-Leim` 风控 + `X-Ds-Pow-Response` PoW（本地解） | SSE（p/o/BATCH 聚合，§6.1） | ✅ 当前有效 |
| gemini-web | GET 首页取 `SNlM0e` 作 `at` → `POST gemini.google.com/_/BardChatUi/data/assistant.lamda.BardFrontendService/StreamGenerate`（batchexecute `f.req` 表单，`?hl=&_reqid=&rt=c`） | Cookie `__Secure-1PSID` + `__Secure-1PSIDTS` | Google 按 TLS/JA3 指纹 429（2026-05 起 curl_cffi 全被拦，仅真浏览器可过；**qltox libcurl 无指纹伪装 → 已知限制，可能 429**） | `)]}'` 前缀 + 长度分帧 JSON | ✅ 当前有效 |
| grok-web | `POST grok.com/rest/app-chat/conversations/new`（及 `conversations/:id/message`） | Cookie `sso` + `sso-rw`（JWT） | `x-statsig-id` 本地生成挑战（算法见 §6.2，填 3 个常量启用）+ Cloudflare（`cf_clearance` 绑定 UA/IP，UA 须与 Cookie 源一致） | **NDJSON 流**（非标准 SSE，camelCase 字段） | ⚠️ 有效但易变 |
| chatgpt-web | sentinel `chat-requirements/prepare` → PoW → `finalize` → `POST /backend-api/f/conversation`（详见 §6.4） | accessToken（session 接口） | PoW（SHA-256 可解）+ **Turnstile**（普遍被卡） | SSE | ❌ 受限，实验性 |

## 2. 参考来源清单（时效性核对）

| Provider | 来源 | 活跃度证据 | 有效性结论 |
|---|---|---|---|
| deepseek | `github.com/NIyueeE/ds-free-api`（784★，Rust，母仓）、其 fork `github.com/Hucclp/deepseek-web-api`（API.md）、`github.com/alive2/deepseek-chat-api`、`github.com/sums001/Deepseek-API`、`github.com/Ametist298/deepseek-free-api` | 母仓 pushed **2026-10-08（核对当天）**，issue 均为近期 | ✅ 当前有效：PoW 路径 `/api/v0/chat/completion` 已被 PR #109 抓上游配置（`pow_header_paths`/`authed_pow_functions`）核对确认；auth = localStorage `userToken` |
| gemini | `github.com/HanaokaYuzu/Gemini-API`（gemini-webapi，3564★）、`github.com/Sophomoresty/gemini-web2api` | pushed 2026-08-27；最新 issue 2026-10-03/04 均为正常使用中的改进讨论（如 #364 `card_content` 解析），无"失效"报告 | ✅ 当前有效：`__Secure-1PSID`/`__Secure-1PSIDTS` + StreamGenerate；PR #361 指出 `__Secure-1PSID` 会变、库依赖 curl_cffi **TLS 指纹伪装** |
| grok | `github.com/imjustprism/grok-web-api`（Rust）、`github.com/ManojINaik/grokAPI`、`github.com/anojndr/grok-to-openai`、`github.com/mem0ai/grok3-api`、`pypi.org/project/grok-api`、`github.com/Sexlovr/grok-2-api`、`github.com/carzygod/grok2api` | grok-web-api pushed 2026-06-17；PR #7（2026-07-26）live 验证 | ⚠️ 有效但易变：响应为 NDJSON 流；`modelMode` wire 值为 `MODEL_MODE_*` 枚举（2026-07 Grok 4.5 升级曾致 403），免费账号仅 `fast` 模式可用；另有 `x-statsig-id` 挑战头与 WebSocket 网关路线并存（`anojndr/grok-to-openai`），实现时需实测确认 |
| chatgpt | `github.com/Octo-Lex/ChatGPT-Web2API`（协议参考）、`github.com/adam-s/toolkit`（TURNSTILE.md）、`gpt2agent` 实测报告（2026-09-08） | gpt2agent 实测：sentinel Turnstile 阶段 blocked | ❌ 受限：PoW 可解，Turnstile 第三方普遍被卡，`/backend-api/f/conversation` 返回 403 → 按既定决策标"实验性" |

## 3. 凭据手工提取指南

通用：登录目标站点 → F12 DevTools → Application（Chrome/Edge）/ Storage（Firefox）查看 Cookie 与 Local Storage；或 Network 面板右键请求 **Copy as cURL** 从 `-H 'cookie: ...'` / `authorization: ...` 抄取。填入 `qltox/aigptbot.cpp` 顶部对应 `kXxxWebCred` 空常量后重编译（Qt3 → Qt4 顺序，勿并行）。

注：**浏览器信息与凭据须同源**——grok 的 Cloudflare `cf_clearance`/`__cf_bm` 绑定 UA 与 IP，
因此 Copy as cURL 时应连带抓 `user-agent:` / `sec-ch-ua:` 的值，填到 `kWebCredUA`（或保持其空值、
用内置默认 Chrome/126，此时抓 cookie 的浏览器须同为该 UA）；grok 另抓 `x-statsig-id` 头/挑战常量
（见 §3.3/§6.2）。

### 3.1 deepseek-web → `kDeepseekWebCred`

1. 登录 `chat.deepseek.com`
2. DevTools → Application → Local Storage → `https://chat.deepseek.com`
3. 找 key **`userToken`**（值为 JSON，形如 `{"value":"<长串>","ttl":...}`）
4. 复制 `.value` 内的长串填入常量（**不带引号、不带 `Bearer` 前缀**，代码拼 `Bearer <值>`）

备选：Network 里任一 `/api/v0/*` 请求 → Request Headers → `authorization: Bearer xxx` 直接抄 `xxx`。

5.（可选）App 的 `X-Device-Id`（官方 App 设备唯一 id）填入 `kDeepseekDeviceIdCred`；不填则每次
   启动随机生成（服务端可能按设备绑定）。
6.（可选）若遇 AWS WAF 202：从浏览器 Cookie 抓 `aws-waf-token`（WAF 通关，3 天）与
   `smidV2`/`.thumbcache_*`（数美设备指纹）拼成 `name=value; name2=value2` 串填入 `kDeepseekWebCookie`。
7. UA：默认 `DeepSeek/2.5.0 Android/35`（绕 AWS WAF 必需）。仅当填了全局 `kWebCredUA` 为桌面
   Chrome UA 时才显式适配浏览器——但那样会触发 AWS WAF 202 challenge，需配合第 6 步一并处理。

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

注：登出/清缓存会使 cookie 轮换；另有 `x-statsig-id` 挑战头（算法与常量见 §6.2，非 cookie）。

4.（遇 403 时启用）浏览器控制台执行 §6.2 提取脚本，将输出的
   `CHALLENGE_HEADER_HEX` / `CHALLENGE_SUFFIX` / `CHALLENGE_TRAILER` 分别填入
   `kGrokChallengeHeaderHex` / `kGrokChallengeSuffix` / `kGrokChallengeTrailer`。
5. UA 一致性：`cf_clearance`/`__cf_bm` 绑定 UA+IP——Cookie 必须与 `kWebCredUA`（或内置默认
   Chrome/126）同源浏览器抓取；改 UA 后 cookie 需重抓。

### 3.4 chatgpt-web → `kChatgptWebCred`（实验性）

1. 登录 `chatgpt.com`
2. 浏览器地址栏打开 `https://chatgpt.com/api/auth/session`
3. 取 JSON 中 **`accessToken`** 字段值（`eyJ...` 开头）填入常量

备选：Application → Cookies → `https://chatgpt.com` → `__Secure-next-auth.session-token`（由代码换取 session，首选 accessToken 直填）。

注：accessToken 有有效期，失效后重新访问 session 接口；且即使凭据有效仍可能被 Turnstile 拦截（见 §5）。

## 4. 对实施的映射

- 凭据/指纹常量：`aigptbot.cpp` 顶部 `kDeepseekWebCred` / `kGeminiWebCred` / `kGrokWebCred` /
  `kChatgptWebCred`，加 `kWebCredUA`（统一浏览器 UA）、`kDeepseekDeviceIdCred` / `kDeepseekWebCookie`
  （deepseek 可选）、`kGrokChallengeHeaderHex` / `kGrokChallengeSuffix` / `kGrokChallengeTrailer`
  （grok 挑战）。均 `""`，填值重编译；**不改 config.json、不做设置页**。
- `buildCandidates()` 中凭据为空 → `any`/`all` 跳过、点名单选报"未配置凭据"。
- 每条消息新会话（无状态，不缓存 session id）；内部可请求 SSE 但聚合后单次返回（`parseSseContent()` 聚合 delta），保持现有完成事件语义。
- 协议细节：见下方 §6（deepseek / grok / gemini / chatgpt 分节）。
- 哈希库：`qlcomp/obsd_sha2.c/.h`（OpenBSD SHA-2，供 chatgpt PoW）与 `qlcomp/dspow_solve.c/.h`（DeepSeekHashV1，供 deepseek PoW），经 `qlcomp/qlite.pri` 编入所有构建。
- 阶段划分：P0 架构（enum/`webKind` 字段/`sendWebHost()` 分派/哈希库）→ P1 deepseek-web → P2 gemini-web → P3 grok-web → P4 chatgpt-web（实验性，失败给明确报错）。
- 不改：Go server、`web/`、`messageattribbar.cpp`（provider 下拉动态生成，新 provider 自动出现）、设置对话框、`build.sh`。

## 5. 共性风险

1. **TLS/浏览器指纹（2026-10-08 网搜复核，逐家结论）**，qltox 用 libcurl 直连：
   - **grok**：Cloudflare `cf_clearance`/`__cf_bm` 绑定 UA+IP，UA 须与 Cookie 源浏览器一致；
     `x-statsig-id` 空串 2026-06 起被 403（`{"error":{"code":7,"message":"Request rejected by
     anti-bot rules."}}`，chenyme/grok2api issue #562）——需填 §6.2 挑战常量本地生成。
   - **gemini**：2026-05 起 Google 按 TLS JA3/JA4 指纹 429 所有 curl_cffi 请求（HanaokaYuzu/
     Gemini-API issue #323，含 valid cookie + impersonate="chrome"，仅真浏览器可过，已 closed 无解；
     另有异议称系 IP 信誉）→ 纯 libcurl 无指纹伪装，**已知限制**：被 429 时给出明确报错。
   - **chatgpt**：CF JA3 需 curl_cffi edge/Chrome 指纹才过 WAF（Kitjesen/chatgpt-to-api）；
     `/backend-api` 还有 Turnstile 不可本地解 → 维持实验性。
   - **deepseek**：App UA 绕 AWS WAF（桌面 Chrome UA 被 202 challenge 拦）；官方 cookie policy
     列 `aws-waf-token`（3 天，WAF 通关）与 `smidV2`/`.thumbcache_*`（数美设备指纹）。
2. **deepseek 风控封号**：参考来源 issue #112（4 账号 10 分钟~12 小时被封）、#109/#102（e2e 后禁言 3 天）。qltox 单发低频使用风险较低但非零，使用前须知。
3. **逆向接口随时可能变更**：§2 来源清单即后续排查起点（如 grok `modelMode` 403、gemini `__Secure-1PSID` 轮换）。
4. **凭据敏感**：cookie/token 等同账号密码，填入源码常量意味着本地明文保存，勿提交到公开仓库。
5. **指纹配置**：`kWebCred*` 常量空 ⇒ 各 provider 内置默认（deepseek=App UA，Web 端=Chrome/126）；
   非空 ⇒ 覆盖全部 provider（含 deepseek，文档提示 WAF 风险）。改 UA 后 grok/chatgpt 的 CF cookie
   需同源浏览器重抓。

## 6. 协议细节（实现依据，2026-10-08 核对）

参考源缓存（本机 `/tmp/opencode/`，实现期直接查阅）：
`ds_api.md`（deepseek 抓包参考）、`dspow.go`（PoW 端到端）、`dspow_solve.c/h`（C 求解器）、
`gem_client.py`/`gem_constants2.py`/`gem_parsing.py`（gemini-webapi 参考库）、
`grok_client.rs`/`grok_challenge.rs`/`grok_streaming.rs`/`grok_readme.md`（grok-web-api）、
`cgpt_proto.md`（ChatGPT-Web2API）。

### 6.1 deepseek-web

**步骤状态机**（`sendDeepseek` + `Session::step/wv`，每步失败即 `webFail` 切候选）：

1. `GET https://hif-leim.deepseek.com/query`（无鉴权，客户端拟态头）→
   `data.biz_data.value` 存 `wv["hif"]`（TTL 600s；本网络 `hif-dliq` NXDOMAIN，只发
   `X-Hif-Leim`，与官方行为一致，见 ds_api.md §0.4）。
2. `POST /api/v0/chat/create_pow_challenge` body `{"target_path":"/api/v0/chat/completion"}`
   → `data.biz_data.challenge{algorithm,challenge,salt,signature,difficulty,expire_at,target_path}`。
3. 本地解 PoW（见下）→ `wv["pow"]`。
4. `POST /api/v0/chat_session/create` body `{}` → `data.biz_data.chat_session.id`。
5. `POST /api/v0/chat/completion`（SSE）：

```json
{ "chat_session_id": "...", "parent_message_id": null, "model_type": "default",
  "prompt": "<仅最新一条用户消息>", "ref_file_ids": [],
  "thinking_enabled": false, "search_enabled": false,
  "source": "input", "action": null, "preempt": false }
```

- `model_type` 取值：抓包见 `"default"`，文档注 `expert`（默认）→ **TODO 实测**。

**请求头**（所有业务请求）：
`User-Agent: <webUA>`（默认 `DeepSeek/2.5.0 Android/35`，被全局 `kWebCredUA` 覆盖前；桌面 Chrome UA 被 AWS WAF 202 challenge 拦）、
`Authorization: Bearer <userToken>`、`X-Client-Version: 2.5.0`、`X-Client-Platform: android`、
`X-Client-Locale: zh_CN`、`X-Client-Bundle-Id: com.deepseek.chat`、`X-Device-Id: <uuid>`、
`X-Device-Model: ""`、`X-Client-Timezone-Offset: 28800`；
completion 另加 `X-Ds-Pow-Response` + `X-Hif-Leim`。

**PoW（DeepSeekHashV1，本地 `qlcomp/dspow_solve.c`）**：

- `prefix = "<salt>_<expire_at>_"`（`expire_at` 为响应中的毫秒时间戳原样拼接）；
- 搜索 `nonce ∈ [0, difficulty)`（实测 difficulty≈144000，~百毫秒级），使
  `DeepSeekHashV1(prefix + 十进制 nonce) == challenge`（32 字节 == 64 hex）；
- 算法 = SHA3-256 变体：Keccak-f[1600] **跳过 round0**（用 RC[1..23]，23 轮）、rate=136、
  padding `0x06…0x80`；
- 成功后 header 值 = `base64(JSON{algorithm,challenge,salt,answer,signature,target_path})`
  （**不含** difficulty/expire_at，见 `dspow.go BuildPowHeader`）；
- challenge 不可凭空伪造（与 salt/expire_at 有签名绑定，ds_api.md §3 实测）。

**SSE 事件聚合**（`event: ready` / `update_session` / `title` / `close` + 无 event 的操作符行）：

- 操作符：`{"p":路径,"v":值}`（默认 SET）、`"o":"APPEND"`（追加）、`"o":"BATCH","v":[…]`
  （递归，子项 `p` 前置父路径）；路径 `response/fragments/-1/content` = 最后片段内容；
- 初始快照 `{"v":{response…}}`；`fragments[].type=="THINK"` 的内容丢弃（brief/不启用思考）；
- **结束信号**：`response/status` = `FINISHED`（成功）/ `INCOMPLETE`（中止，按失败处理）；
  BATCH 内 `quasi_status` 同理；`event: hint` + `type:"error"` → 取 `message` 报错。

### 6.2 grok-web（x-statsig-id 本地生成挑战，已实现）

Token = 70 字节，base64（**无 padding**）后作为 `x-statsig-id` 头；另发 `x-xai-request-id`
（UUID v4）。70 字节构成（`grok_challenge.rs`，MIT）：

```
header[49] ‖ u32le(counter) ‖ sha256("<METHOD>!<PATH>!<counter><SUFFIX>")[0..16] ‖ trailer[1]
```

- `counter = unix_now − 1682924400`（2023-05-01 UTC）；`PATH` 为请求 URL path
  （如 `/rest/app-chat/conversations/new`）；
- 全部 70 字节 XOR 同一个随机字节后 base64；
- 三个常量（`CHALLENGE_HEADER_HEX` 49B hex / `CHALLENGE_SUFFIX` / `CHALLENGE_TRAILER` 默认 `3`）
  为构建期烘焙值，grok 发版可能更换；提取方式（grok-web-api README，Void 扩展，浏览器控制台）：

```js
var m=Void.findByProps("chatApi"),p=m.chatApi.configuration.middleware[0].pre,r=Math.random,d=Date.now,g=crypto.subtle.digest.bind(crypto.subtle),h;Math.random=()=>0;Date.now=()=>1e12;crypto.subtle.digest=async(a,b)=>{h=new TextDecoder().decode(b);return g(a,b)};var s=await p({url:"https://grok.com/rest/app-chat/x",init:{method:"POST",headers:{}}});Math.random=r;Date.now=d;crypto.subtle.digest=g;var t=new Uint8Array([...atob(s.init.headers["x-statsig-id"])].map(c=>c.charCodeAt(0)));console.log(`CHALLENGE_HEADER_HEX=${[...t.slice(0,49)].map(b=>b.toString(16).padStart(2,"0")).join("")}\nCHALLENGE_SUFFIX=${h.split("!").slice(2).join("!").replace(/^-?\d+/,"")}\nCHALLENGE_TRAILER=${t[69]}`)
```

**实现状态（2026-10-08）**：`grokStatsigChallenge()`（`qltox/aigptbot.cpp`）按上文构造
70 字节 token，整体 XOR 随机字节后 base64 **无 padding** 作 `x-statsig-id` 头；仅在
`kGrokChallengeHeaderHex` 等三项常量齐备时启用，未填则发空串（2026-06 起被 403
`code:7 anti-bot rules`，证据见 §5.1/chenyme issue #562）。grok 发版会更换烘焙常量，
若已填但仍 403 → 重新执行上方脚本提取。

**请求**（须 Cookie 与 UA 同源；常量齐备时附 `x-statsig-id`）：

- `POST https://grok.com/rest/app-chat/conversations/new`，body `{"message":"<text>"}`
  （brief 加 `"forceConcise":true`）；
- 头：`Cookie: sso=…; sso-rw=…`、`Content-Type: application/json`、`Accept: */*`、
  `Origin/Referer: https://grok.com`、`sec-fetch-dest: empty`、`sec-fetch-mode: cors`、
  `sec-fetch-site: same-origin`；
- 响应 **NDJSON 每行一个 JSON**（非 SSE，camelCase）：
  `result.response`(首帧) / `result.payload`(后续).`token`、`isThinking`（true 期间为思考
  输出）、`isSoftStop`（true = 结束）、`result.conversation.conversationId`、
  `error.message`（出错）；文本 = 各帧 `token` 依序拼接（`isThinking` 帧跳过或另存）。

### 6.3 gemini-web

1. `GET https://gemini.google.com/app`（带 Cookie）→ 从 HTML 提取 `"SNlM0e"` 作 `at`
   （另有 `cfb2h`/`FdrFJe`/`TuX5cc`(hl)，首版只用 `at`）。
2. `POST https://gemini.google.com/_/BardChatUi/data/assistant.lamda.BardFrontendService/StreamGenerate?hl=<lang>&_reqid=<n>&rt=c`（`_reqid` 从 10000–99999 随机起步，每次 +100000）。

**请求头**：`Content-Type: application/x-www-form-urlencoded;charset=utf-8`、
`Referer: https://gemini.google.com/`、`X-Same-Domain: 1`、
`x-goog-ext-525005358-jspb: ["<大写 UUID>",1]`（与 inner[59] 同值）+ Cookie。

**表单**：`at=<SNlM0e>&f.req=[null, "<inner JSON 字符串>"]`（外层数组 2 元：null + 内层
JSON 字符串）。inner = **81 元素稀疏数组**（`gem_client.py`，null 即缺省）：

| 下标 | 值 |
|---|---|
| 0 | `[prompt, 0, null, null, null, null, 0]`（message_content） |
| 1 | `["zh_CN"]`（语言） |
| 2 | `DEFAULT_METADATA = ["","","",null,null,null,null,null,null,""]` |
| 6 | `[1]` |
| 7 | `1`（streaming 标志） |
| 10 / 11 | `1` / `0` |
| 17 / 18 | `[[0]]` / `0` |
| 27 | `1` |
| 30 / 41 | `[4]` / `[1]` |
| 53 | `0` |
| 59 | 大写 UUID（= x-goog-ext 头） |
| 61 / 68 / 79 / 80 | `[]` / `1` / `1` / `1`（80=2 时启用 extended thinking） |

**响应解析**：

- 整体前缀 `)]}'`（防 XSSI）后是**长度分帧**：`<十进制长度>\n<帧>`，长度按
  **UTF-16 code unit** 计（非字节）——流式时按 unit 扫描裁帧（`gem_parsing.py`）；
- 每帧 = JSON 数组，取 `part[2]`（JSON 字符串）再 parse → `part_json[4]` = candidates；
- 正文 = `cand[1][0]`（跨帧累积为全文，取最后非空帧）；卡片占位回落 `cand[22][0]`；
- 思考 = `cand[37][0][0]`；
- `ARTIFACTS_RE` = `https?://googleusercontent\.com/(?:\w+/)+\d+\n*` 从正文剔除；
- 错误码 = `part[5][2][0][1][0]`（reject 码 `part[5][0]`，7=未认证）→ 报错。

**已知限制（2026-10-08 网搜）**：Google 2026-05 起按 TLS JA3/JA4 指纹 429 非浏览器客户端
（curl_cffi 即便 `impersonate="chrome"` + 有效 cookie 也被拦，仅真实浏览器可过；Gemini-API
issue #323 已 closed 无解）。qltox 的 libcurl 无法伪装指纹，运行时可能 429——设计上明确
报错并降级下一候选即可，不引入 curl-impersonate/CDP。

### 6.4 chatgpt-web（实验性）

1. `POST https://chatgpt.com/backend-api/sentinel/chat-requirements/prepare` body `{}`
   → `{prepare_token, turnstile{required,dx}, proofofwork{required,seed,difficulty(hex)},
   so{required,collector_dx,snapshot_dx}}`（头需 accessToken）。
2. PoW（本地 `qlcomp/obsd_sha2.c`）：`sha256(seed的ASCII + 十进制counter)`，前 3 字节
   （作为大端 uint24）`< parseInt(difficulty, 16)` 即通过（实测 seed=0.559…、difficulty=
   `0689f6` → ~30 次内解出）。
3. `POST …/sentinel/chat-requirements/finalize` body
   `{prepare_token, turnstile, proofofwork{seed,difficulty,answer}, so}`
   → **预期失败**：Turnstile/so 为浏览器交互/加密 blob，本地不可解 → 失败时给明确
   "ChatGPT 被 Turnstile 拦截（实验性）"错误，降级下一候选。
4. `POST https://chatgpt.com/backend-api/f/conversation`（头 `x-openai-sentinel-chat-requirements-token`
   等）→ **TODO 实测**：finalize 成功时的 token 字段名、完整 body、默认模型、SSE 与
   OpenAI `choices[].delta.content` 是否同构。

### 6.5 来源与许可

| 内容 | 来源 | 许可 |
|---|---|---|
| `qlcomp/obsd_sha2.c/.h` | openbsd/src `sys/crypto/sha2.c/h` v1.21 | ISC + Aaron D. Gifford BSD-3（文件头保留）；本地仅类型/字节序/擦除等机械适配 |
| `qlcomp/dspow_solve.c/.h` | `redeflesq/deepseek-pow` c-impl/solve.c | **上游无 LICENSE**（仅 DISCLAIMER）→ 经确认按现状 vendor，改动限于改名/static/去 math.h，见文件头注释 |
| grok 挑战算法 | `imjustprism/grok-web-api` | MIT |
| gemini 协议 | `HanaokaYuzu/Gemini-API`（gemini-webapi） | 见上游仓库 |
| chatgpt 协议 | `Octo-Lex/ChatGPT-Web2API` | 见上游仓库 |
| deepseek 协议 | `NIyueeE/ds-free-api` 及 ds_api.md 抓包参考 | 见上游仓库 |
