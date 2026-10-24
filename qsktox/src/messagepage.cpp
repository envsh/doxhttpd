#include "messagepage.h"
#include "messageattribbar.h"
#include "messagelist.h"
#include "msgenlargeoverlay.h"
#include "rawdatapopup.h"
#include "androidutils.h"
#include "menuoverlay.h"
#include "mozpushsender.h"
#include "msgdb_bridge.h"
#include "virtualcontacts.h"
#include "virtualbothost.h"
#include "aigptbot.h"
#include "websearchbot.h"
#include "fanyibot.h"
#include "pasteuploader.h"
#include "ttsbot.h"
#include "ttsaudio.h"
#include "channellist.h"
#include "mainpage.h"
#include "pagemanager.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QClipboard>
#include <QskPushButton.h>
#include <QskTextField.h>
#include <QskBoxShapeMetrics.h>
#include <QskMenu.h>
#include <QskLabelData.h>
#include <QTime>
#include <QDateTime>

MessagePage::MessagePage(QQuickItem* parent)
    : Page(parent)
{
}

void MessagePage::onCreate(const QVariantMap& launchArgs, const QVariantMap&)
{
    QString channelName = launchArgs.value("channelName").toString();
    QString chatId = launchArgs.value("chatId").toString();
    m_chatId = chatId;
    m_chatType = launchArgs.value("chatType").toString();
    m_protocol = launchArgs.value("protocol").toString();
    m_chanId = launchArgs.value("chanId").toString();

    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);

    // ── TopBar ──
    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);
    topBar->setSpacing(8);

    auto* backBtn = new QskPushButton(QString::fromUtf8("←"), topBar);
    backBtn->setPreferredSize(44, 44);
    backBtn->setBoxShapeHint(QskPushButton::Panel,
        QskBoxShapeMetrics(8, Qt::AbsoluteSize));
    connect(backBtn, &QskAbstractButton::clicked, this, &Page::finish);

    m_titleLabel = new QskTextLabel(channelName, topBar);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);

    auto* optionsBtn = new QskPushButton(QString::fromUtf8("⋮"), topBar);
    optionsBtn->setPreferredSize(44, 44);
    optionsBtn->setBoxShapeHint(QskPushButton::Panel,
        QskBoxShapeMetrics(8, Qt::AbsoluteSize));
    connect(optionsBtn, &QskAbstractButton::clicked, this, [this, optionsBtn]() {
        openChatMenu(optionsBtn);
    });

    // ── MessageList ──
    m_messageList = new MessageListWidget(layout);
    m_messageList->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    MsgDbBridge::ensureChannel(m_protocol, m_chatId, channelName);
    m_messageList->setChannel(m_protocol, chatId, m_chanId);

    // ── Avatar click menu ──
    connect(m_messageList, &MessageListWidget::avatarClicked,
        this, [this](int row, const QPointF& pos) {
            for (auto* old : findChildren<QskMenu*>())
                old->deleteLater();
            for (auto* old : findChildren<MenuOverlay*>())
                old->deleteLater();

            auto* menu = new QskMenu(this);
            menu->setModal(true);
            menu->addOption(QskLabelData(QString::fromUtf8("@提及")));
            menu->addOption(QskLabelData(QString::fromUtf8("发消息")));
            menu->addOption(QskLabelData(QString::fromUtf8("复制用户名")));
            menu->addSeparator();
            menu->addOption(QskLabelData(QString::fromUtf8("查看资料")));

            menu->setOrigin(pos);

            connect(menu, &QskMenu::triggered, this, [this, row](int index) {
                const MessageItem& mi = m_messageList->messageItem(row);
                const QString sender = mi.sender;
                if (index == 0) {
                    if (m_input && !sender.isEmpty()) {
                        m_input->setText(m_input->text() + "@" + sender + " ");
                        m_input->setFocus(true);
                    }
                } else if (index == 1) {
                    if (m_input && !sender.isEmpty()) {
                        m_input->setText("@" + sender + " ");
                        m_input->setFocus(true);
                    } else {
                        showAndroidToast(QString::fromUtf8("发消息功能暂未实现"));
                    }
                } else if (index == 2) {
                    QGuiApplication::clipboard()->setText(sender);
                    showAndroidToast(QString::fromUtf8("已复制"));
                } else if (index == 3) {
                    showAndroidToast(QString::fromUtf8("用户资料 - ") + sender);
                }
                if (auto* m = qobject_cast<QskMenu*>(QObject::sender()))
                    m->close();
            });

            {
                auto* overlay = new MenuOverlay(menu);
                connect(menu, &QObject::destroyed, overlay, &QObject::deleteLater);
            }

            menu->open();
        });

    // ── Long press menu ──
    connect(m_messageList, &MessageListWidget::rowLongPressed,
        this, [this](int row, const QPointF& pos) {
            for (auto* old : findChildren<QskMenu*>())
                old->deleteLater();
            for (auto* old : findChildren<MenuOverlay*>())
                old->deleteLater();

            auto* menu = new QskMenu(this);
            menu->setModal(true);
            menu->addOption(QskLabelData(QString::fromUtf8("复制")));
            menu->addOption(QskLabelData(QString::fromUtf8("转发")));
            menu->addOption(QskLabelData(QString::fromUtf8("回复")));
            menu->addSeparator();
            const int rawDataOption = menu->addOption(
                QskLabelData(QString::fromUtf8("查看原始数据")));
            menu->addSeparator();
            menu->addOption(QskLabelData(QString::fromUtf8("删除")));

            menu->setOrigin(pos);

            connect(menu, &QskMenu::triggered, this,
                [this, row, rawDataOption](int index) {
                if (index == rawDataOption) {
                    RawDataPopup::show(this, m_messageList->messageItem(row));
                }
                if (auto* m = qobject_cast<QskMenu*>(QObject::sender())) {
                    m->close();
                }
            });

            {
                auto* overlay = new MenuOverlay(menu);
                connect(menu, &QObject::destroyed, overlay, &QObject::deleteLater);
            }

            menu->open();
        });

    // ── InputBar ──
    auto* inputBar = new QskLinearBox(Qt::Horizontal, layout);
    inputBar->setPanel(true);
    inputBar->setFixedHeight(56);
    inputBar->setSpacing(8);

    m_input = new QskTextField(inputBar);
    m_input->setPlaceholderText(QString::fromUtf8("输入消息..."));
    m_input->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);

    auto* sendBtn = new QskPushButton(QString::fromUtf8("发送"), inputBar);
    sendBtn->setPreferredWidth(60);
    sendBtn->setBoxShapeHint(QskPushButton::Panel,
        QskBoxShapeMetrics(8, Qt::AbsoluteSize));
    connect(sendBtn, &QskAbstractButton::clicked, this, &MessagePage::sendMessage);

    // ── 属性组件条(输入区下方,随类型动态重建) ──
    m_attrBar = new MessageAttribBar(layout);
    m_attrBar->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    m_attrBar->setTypeLabel(m_protocol);
    m_attrBar->setDefs(messageAttribBarDefsForType(m_protocol));

    // ── Double click handler ──
    connect(m_messageList, &MessageListWidget::rowDoubleClicked,
        this, [this](int row) {
            const MessageItem& mi = m_messageList->messageItem(row);
            if (mi.isAudio && !mi.mediaUrl.isEmpty()) {
                TtsAudio::instance()->play(mi.mediaUrl);
                return;
            }
			// showAndroidToast(QString::fromUtf8("双击第 %1 条消息").arg(row + 1));
            for (auto* old : findChildren<MsgEnlargeOverlay*>())
                old->deleteLater();
            auto* overlay = new MsgEnlargeOverlay(this);
            overlay->show(m_messageList->messageItem(row));
            connect(overlay, &MsgEnlargeOverlay::closed,
                    overlay, &QObject::deleteLater);
        });

    // ── 发送状态回填（reqseq 唯一，仅匹配自己刚发出的那条）──
    connect(MozpushSender::instance(), &MozpushSender::messageSent,
        this, [this](quint64 reqseq, bool success, const QString& error) {
            m_messageList->setSendStatus(reqseq, success, error);
        });

    // ── 虚拟联系人 bot 结果 ──
    connect(VirtualBotHost::instance(), &VirtualBotHost::botDone,
        this, &MessagePage::onBotDone);
    connect(VirtualBotHost::instance(), &VirtualBotHost::ttsDone,
        this, &MessagePage::onTtsDone);

    // ── 点击失败消息重发 ──
    connect(m_messageList, &MessageListWidget::retryRequested,
        this, [this](int row) {
            const QString text = m_messageList->messageItem(row).content;
            const quint64 reqseq =
                MozpushSender::instance()->sendMessage(m_protocol, m_chatId, text);
            if (reqseq == 0) {
                m_messageList->updateSendState(
                    row, MessageItem::SendFailed, 0,
                    QString::fromUtf8("未配置推送端点"));
            } else {
                m_messageList->updateSendState(
                    row, MessageItem::SendSending, reqseq, QString());
            }
        });
}

void MessagePage::openChatMenu(QskPushButton* optionsBtn)
{
    for (auto* old : findChildren<QskMenu*>())
        old->deleteLater();
    for (auto* old : findChildren<MenuOverlay*>())
        old->deleteLater();

    auto* menu = new QskMenu(this);
    menu->setModal(true);
    menu->addOption(QskLabelData(QString::fromUtf8("查看资料")));
    menu->addSeparator();
    menu->addOption(QskLabelData(QString::fromUtf8("搜索聊天记录")));
    menu->addSeparator();
    menu->addOption(QskLabelData(QString::fromUtf8("静音")));
    menu->addSeparator();
    menu->addOption(QskLabelData(QString::fromUtf8("滚动到底部")));

    // 右对齐按钮、悬于其下（仿 MainPage options 菜单）
    QPointF btnPos = optionsBtn->mapToItem(this, QPointF(0, 0));
    qreal menuW = menu->implicitWidth();
    if (menuW <= 0) menuW = 200;
    menu->setOrigin(QPointF(
        btnPos.x() + optionsBtn->width() - menuW,
        btnPos.y() + optionsBtn->height()));

    connect(menu, &QskMenu::triggered, this, [this](int index) {
        if (index == 0) {
            showAndroidToast(QString::fromUtf8("名称: ") + m_titleLabel->text()
                + QString::fromUtf8("\nID: ") + m_chatId
                + QString::fromUtf8("\n协议: ") + m_protocol);
        } else if (index == 2) {
            showAndroidToast(QString::fromUtf8("搜索聊天记录 (coming soon)"));
        } else if (index == 4) {
            showAndroidToast(QString::fromUtf8("静音 (coming soon)"));
        } else if (index == 6) {
            if (m_messageList) {
                m_messageList->scrollToBottom();
            }
        }
        if (auto* m = qobject_cast<QskMenu*>(QObject::sender())) {
            m->close();
        }
    });

    {
        auto* overlay = new MenuOverlay(menu);
        connect(menu, &QObject::destroyed, overlay, &QObject::deleteLater);
    }
    menu->open();
}

void MessagePage::onPause()
{
    // 页面即将被覆盖/关闭：停止消息列表淡入动画，避免隐藏页上的
    // setOpacity 与场景图状态不一致导致白屏
    if (m_messageList) {
        m_messageList->stopFadeIn();
    }
}

void MessagePage::onNewIntent(const QVariantMap& launchArgs)
{
    QString channelName = launchArgs.value("channelName").toString();
    QString chatId = launchArgs.value("chatId").toString();
    m_chatId = chatId;
    m_chatType = launchArgs.value("chatType").toString();
    m_protocol = launchArgs.value("protocol").toString();
    m_chanId = launchArgs.value("chanId").toString();

    if (m_titleLabel) {
        m_titleLabel->setText(channelName);
    }
    if (m_messageList) {
        m_messageList->setChannel(m_protocol, chatId, m_chanId);
        if (!isVirtualProtocol(m_protocol)) {
            // 切换频道时上报已读：用当前列表的最大 rowid（历史+刚加载的都已展示）
            int64_t lr = m_messageList->lastRowid();
            if (lr > 0) {
                MsgDbBridge::markRead(m_protocol, m_chatId, lr);
            }
            if (auto* pm = pageManager()) {
                if (auto* mp = qobject_cast<MainPage*>(pm->findPage("main"))) {
                    if (mp->channelList()) {
                        mp->channelList()->markLocallyRead(m_protocol, m_chatId, m_chanId);
                    }
                }
            }
        }
    }
    if (m_attrBar) {
        m_attrBar->setTypeLabel(m_protocol);
        m_attrBar->setDefs(messageAttribBarDefsForType(m_protocol));
    }
}

void MessagePage::sendMessage()
{
    if (!m_input || !m_messageList) {
        return;
    }
    QString text = m_input->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    MessageItem item;
    item.sender = QString::fromUtf8("我");
    item.content = text;
    item.time = QTime::currentTime().toString("HH:mm");
    item.isSelf = true;
    item.avatarLetter = QString::fromUtf8("我").at(0).toUpper();
    item.avatarColor  = QColor("#1565C0");

    VirtualSendKind vk = virtualSendKind(m_protocol);
    if (vk == kVirtualLocalNoop) {
        item.sendState = MessageItem::SendSent;
        m_messageList->appendMessage(item);
        MsgDbBridge::insertMessage(m_protocol, m_chatId, item, nullptr, nullptr);
        MsgDbBridge::upsertChannel(m_protocol, m_chatId, m_chatId,
            QStringLiteral("online"), false, text, item.time,
            QDateTime::currentSecsSinceEpoch());
        touchChannelInList();
        m_input->setText("");
        return;
    }
    if (vk != kVirtualNone) {
        item.localId = VirtualBotHost::nextLocalId();
        item.sendState = MessageItem::SendSending;
        m_messageList->appendMessage(item);
        MsgDbBridge::insertMessage(m_protocol, m_chatId, item,
                                   &MessagePage::onRowidBackfilled, this);
        touchChannelInList();
        m_input->setText("");
        sendToVirtualBot(vk, item.localId, text);
        return;
    }

    QString sendErr;
    const quint64 reqseq = MozpushSender::instance()->sendMessage(
        m_protocol, m_chatId, text, QMap<QString,QString>(), &sendErr);

    item.reqSeq = reqseq;
    if (reqseq == 0) {
        item.sendState = MessageItem::SendFailed;
        item.sendError = sendErr.isEmpty()
            ? QString::fromUtf8("未配置推送端点") : sendErr;
    } else {
        item.sendState = MessageItem::SendSending;
    }

    m_messageList->appendMessage(item);
    MsgDbBridge::insertMessage(m_protocol, m_chatId, item, nullptr, nullptr);
    touchChannelInList();
    m_input->setText("");
}

void MessagePage::onRowidBackfilled(void* ctx, const QString& protocol,
                                    const QString& chatId, qint64 localId,
                                    int64_t rowid)
{
    Q_UNUSED(protocol)
    Q_UNUSED(chatId)
    auto* self = static_cast<MessagePage*>(ctx);
    if (self && self->m_messageList) {
        self->m_messageList->backfillRowid(localId, rowid);
    }
}

void MessagePage::touchChannelInList()
{
    // 走 pageManager 找 MainPage，不直接持有 ChannelListWidget（避免循环依赖）
    auto* pm = pageManager();
    if (!pm) {
        return;
    }
    auto* mp = qobject_cast<MainPage*>(pm->findPage("main"));
    if (!mp) {
        return;
    }
    if (!mp->channelList()) {
        return;
    }
    mp->channelList()->markChannelActive(m_protocol, m_chatId,
        (qint64)QDateTime::currentSecsSinceEpoch());
}

void MessagePage::sendToVirtualBot(int kind, qint64 localId, const QString& text)
{
    const QByteArray tb = text.toUtf8();
    const QByteArray cb = m_protocol.toUtf8();
    const int chatId = m_chatId.toInt();
    const std::string typeStr(cb.constData(), cb.size());

    if (kind == kVirtualBotAigpt) {
        AigptbotRequest req;
        req.text     = std::string(tb.constData(), tb.size());
        req.provider = "any";
        req.model    = "";
        req.brief    = false;
        req.thinking = false;
        req.search   = false;
        req.newChat  = false;
        req.localId  = (long long)localId;
        req.chatId   = chatId;
        req.chatType = typeStr;
        aigptbotChatStart(req, VirtualBotHost::instance());
        return;
    }
    if (kind == kVirtualBotWebsearch) {
        WebsearchRequest req;
        req.text     = std::string(tb.constData(), tb.size());
        req.backend  = "any";
        req.lang     = "zh";
        req.localId  = (long long)localId;
        req.chatId   = chatId;
        req.chatType = typeStr;
        websearchStart(req, VirtualBotHost::instance());
        return;
    }
    if (kind == kVirtualBotTranslate) {
        FanyibotRequest req;
        req.text     = std::string(tb.constData(), tb.size());
        req.toLang   = "中文";
        req.engine   = "any";
        req.localId  = (long long)localId;
        req.chatId   = chatId;
        req.chatType = typeStr;
        fanyibotTranslateStart(req, VirtualBotHost::instance());
        return;
    }
    if (kind == kVirtualBotPaste) {
        PasteRequest req;
        req.text     = std::string(tb.constData(), tb.size());
        req.kind     = kPasteKindText;
        req.provider = "any";
        req.expire   = "auto";
        req.localId  = (long long)localId;
        req.chatId   = chatId;
        req.chatType = typeStr;
        pasteUploadStart(req, VirtualBotHost::instance());
        return;
    }
    if (kind == kVirtualBotTts) {
        TtsbotRequest req;
        req.text     = std::string(tb.constData(), tb.size());
        req.provider = "edge";
        req.voice    = "zh-CN-XiaoxiaoNeural";
        req.lang     = "zh-CN";
        req.rate     = "+0%";
        req.localId  = (long long)localId;
        req.chatId   = chatId;
        req.chatType = typeStr;
        ttsbotSynthStart(req, VirtualBotHost::instance());
        return;
    }
}

void MessagePage::onBotDone(int chatId, const QString& protocol, qint64 localId,
                            bool success, const QString& text,
                            const QString& providerUsed, const QString& errorMsg)
{
    Q_UNUSED(providerUsed)
    if (QString::number(chatId) != m_chatId || protocol != m_protocol) {
        return;
    }
    if (!success) {
        m_messageList->updateSendStateByLocalId(
            localId, MessageItem::SendFailed, errorMsg);
        MsgDbBridge::updateSendStateByRowid(
            m_messageList->rowidForLocalId(localId), MessageItem::SendFailed);
        return;
    }
    m_messageList->updateSendStateByLocalId(
        localId, MessageItem::SendSent, QString());
    MsgDbBridge::updateSendStateByRowid(
        m_messageList->rowidForLocalId(localId), MessageItem::SendSent);

    MessageItem ack;
    ack.sender = QString::fromUtf8("我");
    ack.content = text;
    ack.time = QTime::currentTime().toString("HH:mm");
    ack.isSelf = true;
    ack.avatarLetter = QString::fromUtf8("我").at(0).toUpper();
    ack.avatarColor  = QColor("#1565C0");
    ack.sendState = MessageItem::SendSent;
    m_messageList->appendMessage(ack);
    MsgDbBridge::insertMessage(m_protocol, m_chatId, ack, nullptr, nullptr);
    MsgDbBridge::upsertChannel(m_protocol, m_chatId, m_chatId,
        QStringLiteral("online"), false, text, ack.time,
        QDateTime::currentSecsSinceEpoch());
    touchChannelInList();
}

void MessagePage::onTtsDone(int chatId, qint64 localId, bool success,
                            const QString& caption, const QString& text,
                            const QString& errorMsg,
                            const QString& remoteUrl, const QString& mime,
                            int durationSec, const QByteArray& audioData)
{
    if (QString::number(chatId) != m_chatId || m_protocol != QLatin1String("tts")) {
        return;
    }
    if (localId != 0) {
        m_messageList->updateSendStateByLocalId(
            localId,
            success ? MessageItem::SendSent : MessageItem::SendFailed,
            success ? QString() : errorMsg);
        MsgDbBridge::updateSendStateByRowid(
            m_messageList->rowidForLocalId(localId),
            success ? MessageItem::SendSent : MessageItem::SendFailed);
    }
    if (!success) {
        return;
    }

    QString key;
    if (!audioData.isEmpty()) {
        key = QStringLiteral("tts://local_%1_%2")
                  .arg(QDateTime::currentMSecsSinceEpoch())
                  .arg(localId);
    } else if (!remoteUrl.isEmpty()) {
        key = QStringLiteral("tts://") + remoteUrl;
    }
    if (!key.isEmpty()) {
        TtsAudio::instance()->store(key, audioData, mime, remoteUrl);
    }

    MessageItem ack;
    ack.sender = QString::fromUtf8("我");
    ack.content = caption + QString::fromUtf8("\n") + text;
    ack.time = QTime::currentTime().toString("HH:mm");
    ack.isSelf = true;
    ack.avatarLetter = QString::fromUtf8("我").at(0).toUpper();
    ack.avatarColor  = QColor("#1565C0");
    ack.sendState = MessageItem::SendSent;
    ack.isAudio = !key.isEmpty();
    ack.mediaUrl = key;
    ack.audioMime = mime;
    ack.durationSec = durationSec;
    m_messageList->appendMessage(ack);
    MsgDbBridge::insertMessage(m_protocol, m_chatId, ack, nullptr, nullptr);
    MsgDbBridge::upsertChannel(m_protocol, m_chatId, m_chatId,
        QStringLiteral("online"), false, text, ack.time,
        QDateTime::currentSecsSinceEpoch());
    touchChannelInList();
}
