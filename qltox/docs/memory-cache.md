# qltox 内存增长（80M → 260M）分析与改进方案

> 记录时间：2026-11-14
> 涉及项目：`qltox`（Qt3 优先、Qt4 同步）+ 共享库 `qlcomp`（identicon / emojiutil）+ `qlappcom`（avatar_manager）
> 现象：连续运行几天、新增数万条消息后，进程 RSS 由约 80 MB 涨到约 260 MB。

---

## 1. 结论摘要

不是单一泄漏，而是三类"按 peer 数 / 按聊天数只增不减"的持有物叠加：

| 类别 | 主要结构 | 增长驱动 |
|---|---|---|
| 图像缓存 | `AvatarManager::m_cache`、identicon 静态缓存、`EmojiRenderer::m_cache` | 不同发送者/头像/昵称数量 |
| 聊天缓冲 | `ChatBuffer::m_map`（每聊天 200 条，聊天条目永不删除） | 打开过的聊天数 |
| 生命周期缺陷 | `QMovie` 无析构、账户切换悬垂 history、动画索引错位 | 运行时长/操作次数 |

其中"消息文本"本身不是主要来源：最近 5 万条消息的纯文本仅约 **11.7 MB**。

---

## 2. 实测数据（本机 `~/.cache/toxhttpd/message.db`，只读查询）

| 指标 | 数值 |
|---|---|
| `messages` 最大 rowid | **815,097** |
| 最近 5 万条消息纯文本 | **11.7 MB** |
| 最近 5 万条涉及频道数 | **55** |
| 最近 5 万条不同发送者名 | **5,043** |
| 最近 5 万条不同公钥 / 不同头像 URL | 4,708 / **4,065** |
| `peers` 表 | **47,081** |

查询示例：

```sql
select max(rowid) from messages;
select count(*), sum(length(data)) from messages where rowid > 765097;
select count(distinct chanid) from messages where rowid > 765097;
select count(distinct sender_name), count(distinct sender_pubkey),
       count(distinct avatar_url) from messages where rowid > 765097;
select count(*) from peers;
```

**关键判断**：增长与"不同 peer 数"强相关，而不是与消息条数线性相关。

---

## 3. 代码层面的增长源（逐处核对）

| # | 结构 | 位置 | 增长键 | 单条成本（估算） | 原状态 |
|---|---|---|---|---|---|
| 1 | `AvatarManager::m_cache` | `qlappcom/avatar_manager.h` | 头像 URL | 42px ≈ 7.1 KB（+mask 0.2KB） | 从不清理 |
| 2 | identicon 静态缓存 | `qlcomp/identicon.cpp` | 昵称:尺寸 | 42px ≈ 7.1 KB | 从不清理，改名即新增 |
| 3 | `EmojiRenderer::m_cache` | `qlcomp/emojiutil.h` | (码点,尺寸) | 约 4 KB | 从不清理 |
| 4 | `peerInfoMap` | `qltox/mainwindow.h` | `friend_/group_/conf_N_M` | 0.3–0.5 KB/条 | 仅随对象销毁 |
| 5 | `ChatBuffer::m_map` | `qltox/chatbuffer.*` | 每个打开过的聊天 | 200 条 × ~1 KB | 每聊天上限 200，聊天条目永不删除 |
| 6 | 媒体缩略图 `scaledDisplay` | `qltox/chatbuffer.cpp` | 当前聊天 | ≤260px ≈ 90–270 KB | 离开聊天时已清理（累计风险低） |
| 7 | `QMovie`（动图/GIF 视频） | `qltox/chatview.cpp` | 播放过的动图 | 单条可达数十 MB（解码帧） | 无父对象、无析构 ⇒ 裁剪/销毁时泄漏 |
| 8 | `m_gifFrameUpdated` | `qltox/chatview.cpp` | 消息数 | 1 B/条 | 裁剪/前插后索引错位（功能 bug） |

### 量化推演（与 180 MB 增量吻合）

- 头像缓存：4,065 × 7.3 KB ≈ **30 MB**
- identicon：~5,000 × 7.1 KB ≈ **36 MB**
- `peerInfoMap`：1–2 万条 ≈ **4–10 MB**（全量 47k → ~20 MB）
- `ChatBuffer`：55 个活跃聊天 × 200 条 × ~1 KB ≈ **10–15 MB**（聊天数上限 147 → ~30 MB）
- 固定项：back buffer（视口×4 ≈ 4 MB）、Qt `QPixmapCache`（Qt5 默认 10 MB）、堆碎片（`malloc_trim` 仅在切聊天/换账户调用）

合计约 **100–130 MB** 可解释增量，叠加碎片与图形后端差异（X11 pixmap 服务端 / macOS 客户端）可达 180 MB。

---

## 4. 已实施的改进

### 4.1 三个图像缓存统一上限 4096 条（FIFO 淘汰 + 日志）

| 缓存 | 文件 | 接口 | 淘汰日志 |
|---|---|---|---|
| Avatar | `qlappcom/avatar_manager.{h,cpp}` | `cacheCount()` / `cacheBytes()` / `kCacheMax=4096` | `[cache] avatar evict url=... count=... bytes=...` |
| Emoji | `qlcomp/emojiutil.{h,cpp}` | `cacheCount()` / `cacheBytes()` / `kCacheMax=4096` | `[cache] emoji evict U+xxxx size=... count=... bytes=...` |
| Identicon | `qlcomp/identicon.{h,cpp}` | `identiconCacheCount()` / `identiconCacheBytes()` / `kIdenticonCacheMax=4096` | `[cache] identicon evict key=... count=... bytes=...` |

实现要点：

- FIFO（插入顺序）淘汰，命中不刷新顺序，实现简单、无额外锁。
- 字节数按 `width * height * (depth/8)` 估算，随插入/淘汰增减，避免每次遍历求和。
- **identicon 的 key 逻辑未改动**（仍为 `seed:size`），仅加上限与日志。

### 4.2 诊断输出

1. `qltox/mainwindow.cpp`：每 5 分钟（300 次 1s 定时器）输出一行汇总：

   ```
   [cache] avatar count=... bytes=... | emoji count=... bytes=... | identicon count=... bytes=...
   ```

2. 统计窗口（`qltox/statisticsdialog.{h,cpp}`）：
   - 新增「缓存」页，显示 6 项：avatar/emoji/identicon 的条目数与大小；
   - 这 6 项在**切到该页时即时计算**（`updateCacheStats()`），页内“刷新”按钮直接重算，不依赖“计算”按钮；
   - 「总览」页保留原有 `cache.db` / `file_refs` 等数据库统计。

### 4.3 聊天缓冲与生命周期修复

| 编号 | 内容 | 文件 |
|---|---|---|
| B1 | 账户重置顺序修正：先 `chatWidget->setBuffer(nullptr)`，再 `m_chatbuf = ChatBuffer()`，消除 `m_history` 悬垂访问 | `qltox/mainwindow.cpp` |
| B2 | `ChatBuffer` 增加 LRU 顺序与 `evictIdle(chatId, type, keepN)`，切聊天时调用 | `qltox/chatbuffer.{h,cpp}`、`qltox/mainwindow.cpp` |
| B3 | `ChatHistory::trimOverflow()` 删除前先 `stopGifLikeAnimation()`，防止 `QMovie` 泄漏 | `qltox/chatbuffer.cpp` |
| B4 | `m_gifFrameUpdated` 与裁剪/前插同步（`onRemoveRange` 删除前 cnt 项、`onInsertRange` 前插 cnt 个 0），修复动画帧索引错位 | `qltox/chatview.cpp` |

> 注意：当前代码中切聊天调用为 `evictIdle(id, typeStr, 88888)`（等效于暂不淘汰，便于观察日志/回归）；推荐值 **8**。调整该数字即可启用真正的空闲淘汰。

---

## 5. 后续可选项（尚未实施）

1. **`peerInfoMap` 上限**（如 2 万条）：超限淘汰最旧一批；miss 时由 `lookupPeerByKey` 走 DB 回填，功能安全，代价是少量查询。
2. **诊断增强**：5 分钟日志/统计页追加 RSS（`/proc/self/statm`）、`ChatBuffer` 聊天数与消息总数、`peerInfoMap` 条目数。
3. **`ChatHistory` 容量调优**：`m_capacity = 200` 可考虑降至 100–150（按聊天切换频率权衡）。
4. **`QPixmapCache` 显式上限**：Qt4/Qt5 默认值不同，可按需 `QPixmapCache::setCacheLimit()`。
5. **账户切换清理缓存**：`AvatarManager::clear()` 目前无调用者，换账户时调用可立即回收。

---

## 6. 验证方法与判据

| 项 | 方法 | 通过标准 |
|---|---|---|
| 缓存上限 | 滚动多个群聊/查看头像，观察 5 分钟日志与「缓存」页 | 三项均 ≤ 4096，长期不单调增长 |
| 淘汰日志 | 观察 stderr | 出现 `[cache] ... evict ... count/bytes`，频率与新增头像量相当 |
| 内存曲线 | 运行数天，对比 `/proc/PID/status` 的 `VmRSS` | 增量主要来自文本消息，预计 < 30 MB |
| B1 | 反复切换账户 5 次 | 不崩溃、无异常日志 |
| B2 | 打开 10+ 聊天 | 出现 `[chatbuf] evict ...`；重开被淘汰聊天正常加载 50 条、发送正常 |
| B3/B4 | 含动图聊天滚动触发裁剪 | 内存不随消息数增长，动画不串位 |

编译状态：

- Qt3：`bash buildqt3.sh` → 0 error（已通过）
- Qt4：`chatbuffer.cpp`、`chatview.cpp`、`mainwindow.cpp`、`statisticsdialog.cpp`、`avatar_manager.cpp`、`emojiutil.cpp`、`identicon.cpp` 语法检查通过

---

## 7. 风险与回退

| 改动 | 风险 | 回退 |
|---|---|---|
| 缓存 4096 上限 | 热点头像可能被淘汰并触发 DB/重绘开销 | 调大 `kCacheMax`；逻辑无副作用 |
| `evictIdle` | 已淘汰聊天收到延迟回执时元素找不到（消息停留“发送中”，重开恢复） | 调大 keepN 或改为 `88888`（当前值即等效关闭） |
| B3/B4 | 仅释放/同步逻辑，无行为变化 | 直接还原对应 diff |
| 统计页即时计算 | 仅 UI 读取，无副作用 | 还原 `statisticsdialog` 相关改动 |

---

## 8. 涉及文件清单

```
qlappcom/avatar_manager.h        # 缓存结构/LRU/统计接口
qlappcom/avatar_manager.cpp      # cachePut + FIFO 淘汰日志
qlcomp/emojiutil.h               # 缓存上限/统计接口
qlcomp/emojiutil.cpp             # cachePut + 淘汰日志
qlcomp/identicon.h               # 统计接口
qlcomp/identicon.cpp             # 上限 4096 + 淘汰日志（key 不变）
qltox/chatbuffer.h               # LRU 顺序 + evictIdle
qltox/chatbuffer.cpp             # getOrCreate LRU / evictIdle / trimOverflow 停动画
qltox/chatview.cpp               # onRemoveRange / onInsertRange 索引同步
qltox/mainwindow.cpp             # 账户切换顺序修复；5 分钟缓存日志；切聊天淘汰
qltox/statisticsdialog.h/.cpp    # 「缓存」页 6 项，切页即时计算
```
