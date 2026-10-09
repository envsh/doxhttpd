#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include "compat34.h"
#ifdef QT3_BUILD
#include <qmap.h>
#include <qstringlist.h>
#else
#include <QMap>
#include <QStringList>
#include <QList>
#endif
#include <qwidget.h>
#ifdef QT3_BUILD
#include <qptrlist.h>
typedef QPtrList<QWidget> ChipWidgetList;      // Qt3：存 QWidget（非指针参数类型）
#else
typedef QList<QWidget*> ChipWidgetList;        // Qt4：存 QWidget*
#endif
#include <qwidget.h>
#include <qlabel.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include "chatview.h"
#include "messageinput.h"
#include "messageattribbar.h"
#include "recorder.h"
#include "emojiwidgets.h"
#include "emoji_picker.h"
#include "StyleParams.h"
#include "loadingbar.h"
#include "EmbeddedMenuBar.h"
#include <vector>
#include <string>

class StickerPicker;

// 外观三件套（语言/风格/深色）弹出菜单的文案引用。
// ⚠ 定义在类体之外：类体内 #ifdef 会让 Qt3 moc v26 同时处理两个分支而重复生成。
// ⚠ 菜单文案不重建、只就地改写，原因（Qt3 实测约束，同 qlstik/mainwindow.h:15-21）：
//   * QMenuData::clear()（Qt3）会保留旧分隔线 → 反复重建会堆积分隔线；
//   * QPopupMenu::insertItem()（Qt3）返回的是**分配 id**（非位置），
//     而 Qt4+ 的 QAction* 才是稳定句柄。
// 故构建时记下每项的 id（Qt3）或 QAction*（Qt4+），retranslateUi() 据此改写。
struct AppearanceMenuItemRef {
    const char* key;      // 翻译键（UTF-8）
    void* owner;          // 所属菜单（Qt3 changeItem 用；Qt4+ 仅留档）
    int id;               // Qt3 insertItem 返回的分配 id
    void* action;         // Qt4+ QAction*
    int group;            // 0=语言 1=风格 2=深色；-1 = 不打勾
    int index;            // 组内序号（语言/风格=索引，深色 0=开 1=关）
    AppearanceMenuItemRef() : key(0), owner(0), id(-1), action(0), group(-1), index(-1) {}
};

// ── 外观三件套的共用构件 ──
// ChatWidget 顶部与 MainWindow 标题栏各建一套（彼此独立维护文案与 tooltip），
// 这几个构件跨两个 .cpp 共用，故不设为 static。
// 用 QPushButton 而非 QToolButton：双端都能挂菜单（Qt3 setPopup / Qt4+ setMenu），
// 且 Qt3 QToolButton 的 popup 不响应单击。
QPushButton* makeAppearanceMenuButton(QWidget* parent, const QString& glyph);
void attachAppearanceMenu(QPushButton* btn, MenuWidget34* menu);
// 三个语言项固定用语言名自身（简体中文/繁體中文/English），不参与翻译
const char* const* appearanceLangLabelKeys();
// 当前语言代码 → 索引（0..2），认不出按 0
int appearanceLangIndex(const QString& langCode);
// 加一项并留档；out 为宿主容器的 m_appearanceItems
void addAppearanceMenuItem(std::vector<AppearanceMenuItemRef>* out, MenuWidget34* menu,
                           const QObject* receiver, const char* slot, const char* key,
                           int group, int index);

class ChatWidget : public QWidget {
    Q_OBJECT
public:
    ChatWidget(QWidget* parent = 0);
    ~ChatWidget();
    static bool s_autoTranslateArg;
    
    void setHeaderText(const QString& text);

    int messageCount() const;
    ChatElement messageAt(int index) const;
    ChatElement& mutableMessageAt(int index);
    void updateElement(int msgIndex) { messageArea->updateElement(msgIndex); }
    int indexOfLocalId(int64_t localId) { return messageArea->indexOfLocalId(localId); }
    void repaintMessageElement(int msgIndex) { messageArea->repaintMessageElement(msgIndex); }
    void relayout() { messageArea->relayout(); }
    void repaintMessages() { messageArea->update(); }
    void setBuffer(ChatHistory* hist);
    void retranslateUi();
    void showUnreadBanner(int count);
    void setAutoTranslateEnabled(bool enabled) { m_autoTranslateEnabled = enabled; }
    void resetPendingContext();                       // 会话切换/发送后清空待发扩展上下文
    void setAttrForChat(const QString& type, int id); // 切换联系人时重建底部属性条
    // 仅属性栏可下发取值(白名单过滤)。失败重试兜底用：不含待发引用/提及
    // ——那属于用户正在编辑的新消息，混入旧消息重发是错的。
    QMap<QString,QString> attribContext() const;
    // 首发用：待发引用/提及(m_pendingCtx) + 属性栏取值。
    QMap<QString,QString> currentSendContext() const;
    void setFavRowids(const std::vector<int64_t>& favRowids) { messageArea->setFavRowids(favRowids); }
    bool isFavRowid(int64_t rowid) const { return messageArea->isFavRowid(rowid); }
    void setFavRowid(int64_t rowid, bool fav) { messageArea->setFavRowid(rowid, fav); }
    
signals:
    void messageSent(const QString& message, const QMap<QString,QString>& context);
    void languageChanged(const QString& langCode);
    void fileSendRequested(const QString& filePath, const QString& caption = QString());
    void translateRequested(int msgIndex, const QString& text, const QString& targetLang);
    void translateForSendRequested(const QString& text, const QString& targetLang);
    void sourceClicked(int msgIndex);
    void retryClicked(int msgIndex, const QString& mediaUrl, const QString& source);
    void downloadNeeded(int msgIndex, const QString& mediaUrl);
    void openFullSizeImage(int msgIndex, const QString& mediaUrl);
    void openMediaPlayer(int msgIndex);
    void resendMessage(int msgIndex);
    void requestRedactMessage(int msgIndex);
    void favoriteClicked(int msgIndex);
    void screenshotRequested();
    void peerInfoRequested(int peerNumber, const QString& senderName, const QString& pubkey);
    void replyLinkActivated(const QString& peerId, const QString& name);

private slots:
    void onSendClicked();
    void onUilangChanged(int index);
    void onTranslateTolangChanged(int index);
    void onThemeToggled(bool checked);
    void onStyleChanged(int index);
    void onEmojiClicked();
    void onEmojiInsert(const QString& emoji);
    void onFileClicked();
    void onFilePaste(const QString& filePath, const QString& caption);
    void onStickerClicked();
    void onQuickReplyClicked();
    void onSendEnClicked();
    void onTranslateClicked(int msgIndex);
    void onAutoTranslateRequested(int msgIndex, const QString& text, const QString& toLang);
    void onMentionClicked(const QString& senderName, const QString& nickName);
    void onReplyRequested(int msgIndex);
    void onEditRequested(int msgIndex);
    void onDeleteRequested(int msgIndex);
    void onRedactRequested(int msgIndex);
    void onForwardRequested(int msgIndex);
    void onScreenshotClicked();
    void onPasteClicked();
    void onRecordAudioClicked();
    void onRecordScreenClicked();
    void onRecordFinished(const QString& file, int exitCode);
    void onReplyStripClose();
    void hideUnreadBanner();

public slots:
    void onTranslateResult(int msgIndex, bool success, const QString& translatedText, const QString& errorMessage);

    LoadingBar* loadingBar() { return m_loadingBar; }

private:
    LoadingBar* m_loadingBar;
    QLabel* headerText;
    QLabel* m_unreadBanner;
    QString m_baseHeader;
    void scrollBottomIfNeeded();
    void updateHeaderCount();
    // 若消息已缓存翻译，直接应用并返回 true（不发网络请求）
    bool applyCachedTranslation(int msgIndex, const QString& toLang);
    QComboBox* langSelector;
    QComboBox* m_styleSelector;
    QCheckBox* themeCheckBox;
    ChatView* messageArea;
    MessageInput* inputEdit;
    MessageAttribBar* m_attrBar;
    QMap<QString, QMap<QString,QString> > m_attrMemory;   // key = type_id
    QString m_attrKey;
    EmojiPushButton* emojiBtn;
    EmojiPushButton* fileBtn;
    EmojiPushButton* stickerBtn;
    EmojiPushButton* quickReplyBtn;
    EmojiPushButton* historyBtn;
    EmojiPushButton* screenshotBtn;
    EmojiPushButton* pasteBtn;      // 输入框左侧第一列上：粘贴（与 Ctrl+V 同路径）
    EmojiPushButton* typeIconBtn;   // 输入框左侧第一列下：当前联系人类型图标（纯指示器，不接 slot）
    EmojiPushButton* recordAudioBtn;   // 输入框左侧第二列上：录音发送
    EmojiPushButton* recordScreenBtn;  // 输入框左侧第二列下：录屏发送
    MediaRecorder* m_recorder;         // 归属本窗口，析构回收
    int m_recKind;                     // MediaRecorder::kNone/kAudio/kScreen
    void onRecordToggle(int kind, EmojiPushButton* me, EmojiPushButton* other);
    void updateChatTypeIcon(const QString& type);

    // ── 外观三件套（按钮 + 弹出菜单）──
    // 旧的 langSelector / m_styleSelector / themeCheckBox 仍按原样构造并接线，
    // 但已 hide()：它们的「当前值」全部走 ThemeManager / Config，无需读控件状态。
    void updateAppearanceTooltips();      // 按钮 tooltip =「功能: 当前值」
    void refreshAppearanceMenuTexts();    // 切换语言后就地改写菜单项文案
    void updateAppearanceMenuChecks();    // 当前值打勾（三组各自单选）
    QPushButton* m_langBtn;
    QPushButton* m_styleBtn;
    QPushButton* m_darkBtn;
    MenuWidget34* m_langMenu;
    MenuWidget34* m_styleMenu;
    MenuWidget34* m_darkMenu;
    std::vector<AppearanceMenuItemRef> m_appearanceItems;

    QString m_currentChatType;
    QPushButton* sendBtn;
    QPushButton* m_sendEnBtn;
    EmojiPicker* emojiPicker;
    StickerPicker* m_stickerPicker = nullptr;
    bool m_autoTranslateEnabled = false;
    // 待发送扩展上下文（key=wire 表单字段名）。key 必须在白名单内：
    //   relates_to —— 引用目标（逗号分隔）
    //   mentions   —— 提及目标（逗号分隔）
    //   visibility —— 可见性
    // 必须及时清理：onSendClicked 发送后 clear()；切换会话/清空输入等时机也应 clear()。
    // 残留的引用/提及会被误带到下一条消息。
    QMap<QString,QString> m_pendingCtx;
    // ── 待发扩展上下文的可见指示（输入区上方 m_ctxBar，纯增量，不动 >> /@ 输入行为）──
    QWidget* m_ctxBar;                                // 容器（初始 hide）
    QWidget* m_replyRow;                              // 引用条行
    QLabel*  m_replyStrip;                            // "↩ 回复: <名>"
    QLabel*  m_replySnippet;                          // 单行摘要（自截断，Qt3 无 elide）
    QPushButton* m_replyCloseBtn;                     // ✕
    QWidget* m_chipRow;                               // 提及 chip 行
    ChipWidgetList m_chipWidgets;                    // 当前 chip 列表（用于重建）
    QString m_replyDisplayName;
    QString m_replySnippetText;
    QString m_replyMentionedName;                     // MSC3952：回复捆绑进 mentions 的显示名，✕ 时一并移除
    QStringList m_pendingMentionDisplay;              // senderName 去重列表（chip 渲染用）
    void updateReplyStrip();
    void updateMentionChips();
    void updateCtxBarVisibility();
    void onChipClose(const QString& senderName);      // 由 LambdaSlot 调用
};

#endif // CHATWIDGET_H
