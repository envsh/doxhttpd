#ifndef MESSAGE_PAGE_H
#define MESSAGE_PAGE_H

#include "page.h"

#include <QStringList>

class MessageListWidget;
class MessageAttribBar;
class QskTextField;
class QskPushButton;
class QskGeometryChangeEvent;

class MessagePage : public Page
{
    Q_OBJECT
public:
    MessagePage(QQuickItem* parent = nullptr);

    QString chatId() const { return m_chatId; }
    QString protocol() const { return m_protocol; }
    QString chanId() const { return m_chanId; }

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;
    void onNewIntent(const QVariantMap& launchArgs) override;
    void onPause() override;
    void geometryChangeEvent(QskGeometryChangeEvent*) override;

private:
    // 标题按钮 👤 前缀 + 超宽 elide（geometryChangeEvent / 入口回填重算）
    void updateTitleElide();
    void openChatMenu(QskPushButton* optionsBtn);
    void openAvatarMenu(int row, const QPointF& pos);
    void sendMessage();
    void sendToVirtualBot(int kind, qint64 localId, const QString& text);
    void onBotDone(int chatId, const QString& protocol, qint64 localId,
                   bool success, const QString& text,
                   const QString& providerUsed, const QString& errorMsg);
    void onTtsDone(int chatId, qint64 localId, bool success,
                   const QString& caption, const QString& text,
                   const QString& errorMsg,
                   const QString& remoteUrl, const QString& mime,
                   int durationSec, const QByteArray& audioData);
    static void onRowidBackfilled(void* ctx, const QString& protocol,
                                  const QString& chatId, qint64 localId,
                                  int64_t rowid);
    // 通知会话列表：本会话有新消息，需要上浮（排序键 last_active）
    void touchChannelInList();
    // 语音行内播放 tap 派发（P2）：tts:// 原链直接播；mxc 字节齐 →
    // AudioPlayer；字节缺 → download + pending 意图下载完自动播
    void toggleAudio(int row);
    // 文件系统打开（P4）：字节齐 → cache 落盘 + openFile/QDesktopServices；
    // 缺 → download + pending 意图（audio pendingPlay 同款），完成自动开
    void openFileFor(int row);

    MessageListWidget* m_messageList = nullptr;
    MessageAttribBar* m_attrBar = nullptr;
    QskTextField* m_input = nullptr;
    QskPushButton* m_titleBtn = nullptr;   // 👤 图标前缀 + elide + 点击 toast
    QStringList m_audioPendingUrls;   // 语音 pendingPlay 意图 → mediaUpdated 自动播
    QStringList m_filePendingUrls;    // 文件 pending 意图 → mediaUpdated 自动开
    QString m_chatId;
    QString m_chatType;
    QString m_protocol;
    QString m_chanId;
   QString m_channelName;
};

#endif
