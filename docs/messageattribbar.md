# 消息输入框属性组件列 MessageAttribBar

## 0. 改动摘要

新增组件 `qltox/messageattribbar.h` / `messageattribbar.cpp`：消息输入框底部一条**属性组件 flow 区**（combobox / checkbox / spinbox / lineedit / kTags），按当前联系人类型（`type_id`）动态重建，值以键值表形式暂存于会话内存。

当前阶段**只做 UI，不附加发送**（用户明确「restapi 不改」）。属性条插在输入区正下方（`mainLayout->addLayout(inputGrid)` 之后）。

## 1. 选取方案决策记录

| 要点 | 决策 |
|------|------|
| 发送途径 | **只做 UI 暂不发送**；restapi `ctxAllowedKeys()` 白名单**明确不改** |
| 值记忆 | 会话内存按 `type_id` 恢复（`ChatWidget::m_attrMemory`，key = `type+"_"+id`） |
| schema | 占位示例，键名/选项后续可调 |
| 命名 | `messageattribbar` → `MessageAttribBar`（原 attribbar 弃用） |
| kTags | 先 QLineEdit 纯文本（中英逗号拆分、trim、去重、join）；后升级 chip 流式（见 §6） |
| 类型覆盖 | 全部既有类型常量都要有对应档位，未知类型入 `kUnknownType` 档 |
| 隐藏组 | `kBookmarkType / kAichatType / kPastebinType / kTranslateType` → 空 defs（隐藏） |

## 2. 数据模型

```cpp
struct MessageAttrDef {
    int kind;         // kCombo / kCheck / kSpin / kLine / kTags
    QString key;      // 值键（返回表 / 记忆表用）
    QString label;    // 显示文案（CJK 必须 qFromUtf8）
    QStringList opts; // combo 选项
    QString defValue; // 默认值
};
// MessageAttribBar::values() -> QMap<QString,QString>（key -> 当前值）
// 值格式：combo=选中项、check="1"/"0"、spin=整数、line=文本、tags="a,b,c"（逗号 join）
```

`messageAttribBarDefsForType(const QString& type)` 返回该类型的属性定义表；空表 → 属性条隐藏。

## 3. 布局

- 手写 flow：label 定宽 + 控件右排，同行多列、超宽折行，`setFixedHeight` 按行数计算（`layoutItems`，`m_inLayout` 防重入）。
- defs 为空 → `setFixedHeight(0)` + hide。
- 位置：ChatWidget `mainLayout` 中 header / 未读横幅 / messageArea(stretch 1) / `m_ctxBar`（引用+提及，可隐藏）/ inputGrid / **attrBar**。
- Qt3/Qt4 双兼容：Qt3 `<qtimer.h>` vs Qt4 `<QTimer>`；QComboBox `insertItem+setCurrentItem` vs `addItem+setCurrentIndex`；QSpinBox `setMinValue/setMaxValue/setLineStep` vs `setRange/setSingleStep`；`QValueList` vs `QList`；CJK 均 `qFromUtf8`。
- `MessageAttribBar` 无 `Q_OBJECT`（规避 Qt3 moc 双分支问题）。

## 4. schema 全表（messageattribbar.cpp）

| 类型常量 | 实际档位（代码实现） |
|----------|------|
| friend | combo `presence` + line `note` + kTags |
| group | combo `priority` + check `sticky` + line `subject` + kTags |
| conference | combo `speak_mode` + check `record` + kTags |
| `kUnktoxFriendType` | combo `priority` + line `note` + kTags |
| `kUnktoxConferenceType` | combo `speak_mode` + check `record` + kTags |
| `kUnktoxGroupType` | combo `priority` + check `sticky` + kTags |
| `kGomuksRoomType` | check `notify` + combo `priority` + kTags |
| `kMtxliteRoomType` | check `notify` + combo `priority` + kTags |
| `kImapMailType` | check `mark_as_read` + combo `priority` + kTags |
| `kMisskeyType` | combo `publicity` + line `note` + kTags |
| `kFilesyncType` | line `sync_dir` + check `overwrite` |
| `kClipboardType` | check `keep_history` + kTags |
| `kSyseventType` | combo `level` + check `sound` |
| `kTopicType` | line `subreddit` + check `pinned` |
| `kToutiaoHotnewsType` 等 feed 五类 | spin `auto_refresh` + check `only_video` |
| `kUnknownType` | combo `priority` + line `note` + kTags |
| `kBookmarkType / kAichatType / kPastebinType / kTranslateType` | **空（隐藏）** |
| 未注册类型 | 空（隐藏） |

> 注意：`kToutiaoHotlistType` **不存在**，热榜复用 `kToutiaoHotnewsType`，勿引用。

## 5. 会话集成

- `ChatWidget::setAttrForChat(const QString& type, int id)`：存旧值 `m_attrMemory[m_attrKey] = m_attrBar->values()` → 换 `m_attrKey = type+"_"+id` → 查 `messageAttribBarDefsForType`，空则 clear，否则 setDefs + 按记忆恢复（Qt3 `QMap::find/it.data`，Qt4 `QMap::value`）。
- MainWindow 三处挂钩：
  - `onContactSelected` 设置 currentChatId/currentChatType 后 → `setAttrForChat(type, id)`
  - 离开会话（群退，chatwidget 相关重置处）→ `setAttrForChat(QString(), -1)`
  - 切换账号重置处 → `setAttrForChat(QString(), -1)`
- 发送链路 `onSendClicked` / `resetPendingContext` **不改**（属性是持续 UI 状态）。

## 6. kTags chip 流式编辑

`kTags` = 标签 chip 编辑器（常见 tags 交互：回车/逗号添加一个 tag 成 chip，× 或退格删除，值=逗号 join）。全部实现在 `messageattribbar.cpp` 内部类（无 `Q_OBJECT`）：

```cpp
class TagEditor : public QWidget {   // chips + 尾部输入框, 内部流式, 超宽折行
    TagEditor(MessageAttribBar* owner, QWidget* parent = 0);
    void setTags(const QStringList& tags);   // 恢复/重建(trim+去重保序)
    bool addTag(const QString& t);           // true=成功;失败(空/重复)不丢草稿
    void removeTag(const QString& t);
    QString text() const;                    // m_tags.join(",")
    void clearMark();  void handleBackspaceEmpty();
    // resizeEvent 宽度变化重排; mousePressEvent 点击容器聚焦输入框
    // relayout(): 同行多 chip 流式, 超 220px 折行; setFixedHeight 算高
};
class TagInput : public QLineEdit {          // 输入框(无 Q_OBJECT)
    // keyPressEvent: Enter/Return 提交;空输入 Backspace 两步删;Ctrl+V 走 insert 拆分
    // focusOutEvent: 有草稿自动提交(addOnBlur)
    // insert(): 含 , / ，/ \n 的文本(输入或粘贴)拆分提交, 末段留作草稿(addOnPaste)
    // IME 守卫: Qt4 inputMethodEvent 追踪 preedit; Qt3 imStart/imCompose/imEnd
};
```

- **添加**：Enter/Return 提交整段（自动按 `,` / `，` / `\n` 拆分）；输入或粘贴含分隔符的文本 → `insert()` 拆分批量添加，末段保留为草稿（标准 web tag-input 约定）。`addTag` 失败（空/重复）时保留草稿不清空（不丢用户输入）。
- **IME 安全**：输入法组合期（中文拼音候选）内 Enter/逗号**不提交**（Qt4 `inputMethodEvent` 看 preedit/commitString；Qt3 `imStartEvent/imComposeEvent/imEndEvent`），避免误切。
- **删除-鼠标**：每个 chip = `QFrame(QLabel + ×QPushButton)`（QLabel 超长按宽度省略），`LambdaSlot(×, [this,t]{ removeTag(t); })` + `connect(×, SIGNAL(clicked()), slot, SLOT(call()))`（照抄 chatwidget.cpp:826-827 提及 chip 范式）。
- **删除-键盘（两步）**：输入框为空时 Backspace：第一次按**高亮**最后一个 chip（`QFrame::StyledPanel|Sunken`），第二次按删除该 chip；可连续两步删前一个。任何输入清除高亮。
- **blur**：输入框失焦时有草稿 → 自动提交（`addOnBlur`），并清高亮。
- **长度/显示**：单 tag `setMaxLength(64)`；chip 文本超宽 elide（Qt4 `QFontMetrics::elidedText`，Qt3 宽截断 + `…` 兜底）。
- **布局**：TagEditor 固定宽 220，内部流式同行多 chip、超宽折行——**不是每个值一行**；增删后 `relayout()` + `m_owner->requestRelayout()`（bar 重排，高度变化顶开后续行不重叠）。
- 值：`text()` = `m_tags.join(",")`；旧的逗号文本经 `setItemValue` split 后可正常恢复。

## 7. 不动清单

- 发送链路（`ChatWidget::onSendClicked` → `messageSent` → `MainWindow::onMessageSending` → `ToxAPI::sendMessage(..., context)`）。
- `restapi.cpp ctxAllowedKeys()` 白名单。
- `.pro` 无需新增文件（内部类自包含于 messageattribbar.cpp，仅登记已有的 messageattribbar.*）。

## 8. 待办 / 后续

- 真值如何附加到发送：待定（白名单字段 vs 拼入正文 vs 服务器消费）。
- 占位 schema 键名/选项可按产品需求调整。
- MessageInput 箭头键防抖（已完成）：一次 press+release = 一次 click，`kClickDebounceMs=120` > X11 连发 ~29ms；日志键名 `kArrowKindNames`。