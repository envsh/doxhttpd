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
#include "emojiwidgets.h"
#include "emoji_picker.h"
#include "StyleParams.h"
#include "loadingbar.h"
#include <string>

class StickerPicker;

class ChatWidget : public QWidget {
    Q_OBJECT
public:
    ChatWidget(QWidget* parent = 0);
    static bool s_autoTranslateArg;
    
    void setHeaderText(const QString& text);

    int messageCount() const;
    ChatElement messageAt(int index) const;
    ChatElement& mutableMessageAt(int index);
    void updateElement(int msgIndex) { messageArea->updateElement(msgIndex); }
    void repaintMessageElement(int msgIndex) { messageArea->repaintMessageElement(msgIndex); }
    void relayout() { messageArea->relayout(); }
    void repaintMessages() { messageArea->update(); }
    void setBuffer(ChatHistory* hist);
    void retranslateUi();
    void showUnreadBanner(int count);
    void setAutoTranslateEnabled(bool enabled) { m_autoTranslateEnabled = enabled; }
    void resetPendingContext();                       // 会话切换/发送后清空待发扩展上下文
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
    void openFullSizeImage(int msgIndex, const QString& mediaUrl);
    void openMediaPlayer(int msgIndex);
    void resendMessage(int msgIndex);
    void requestRedactMessage(int msgIndex);
    void favoriteClicked(int msgIndex);
    void screenshotRequested();

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
    EmojiPushButton* emojiBtn;
    EmojiPushButton* fileBtn;
    EmojiPushButton* stickerBtn;
    EmojiPushButton* quickReplyBtn;
    EmojiPushButton* historyBtn;
    EmojiPushButton* screenshotBtn;
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
