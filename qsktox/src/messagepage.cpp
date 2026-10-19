#include "messagepage.h"
#include "messagelist.h"
#include "msgenlargeoverlay.h"
#include "androidutils.h"
#include "menuoverlay.h"
#include "mozpushsender.h"
#include "virtualcontacts.h"
#include "virtualbothost.h"
#include "aigptbot.h"
#include "websearchbot.h"
#include "fanyibot.h"
#include "pasteuploader.h"
#include "ttsbot.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QClipboard>
#include <QskPushButton.h>
#include <QskTextField.h>
#include <QskBoxShapeMetrics.h>
#include <QskMenu.h>
#include <QskLabelData.h>
#include <QTime>

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

    // ── MessageList ──
    m_messageList = new MessageListWidget(layout);
    m_messageList->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    m_messageList->setChannel(chatId);

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
            Q_UNUSED(row)
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
            menu->addOption(QskLabelData(QString::fromUtf8("删除")));

            menu->setOrigin(pos);

            connect(menu, &QskMenu::triggered, this, [this](int index) {
                Q_UNUSED(index)
                if (auto* m = qobject_cast<QskMenu*>(QObject::sender()))
                    m->close();
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

    // ── Double click handler ──
    connect(m_messageList, &MessageListWidget::rowDoubleClicked,
        this, [this](int row) {
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

void MessagePage::onNewIntent(const QVariantMap& launchArgs)
{
    QString channelName = launchArgs.value("channelName").toString();
    QString chatId = launchArgs.value("chatId").toString();
    m_chatId = chatId;
    m_chatType = launchArgs.value("chatType").toString();
    m_protocol = launchArgs.value("protocol").toString();

    if (m_titleLabel) {
        m_titleLabel->setText(channelName);
    }
    if (m_messageList) {
        m_messageList->setChannel(chatId);
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
        m_input->setText("");
        return;
    }
    if (vk != kVirtualNone) {
        item.localId = VirtualBotHost::nextLocalId();
        item.sendState = MessageItem::SendSending;
        m_messageList->appendMessage(item);
        m_input->setText("");
        sendToVirtualBot(vk, item.localId, text);
        return;
    }

    const quint64 reqseq =
        MozpushSender::instance()->sendMessage(m_protocol, m_chatId, text);

    item.reqSeq = reqseq;
    if (reqseq == 0) {
        item.sendState = MessageItem::SendFailed;
        item.sendError = QString::fromUtf8("未配置推送端点");
    } else {
        item.sendState = MessageItem::SendSending;
    }

    m_messageList->appendMessage(item);
    m_input->setText("");
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
        return;
    }
    m_messageList->updateSendStateByLocalId(
        localId, MessageItem::SendSent, QString());

    MessageItem ack;
    ack.sender = QString::fromUtf8("我");
    ack.content = text;
    ack.time = QTime::currentTime().toString("HH:mm");
    ack.isSelf = true;
    ack.avatarLetter = QString::fromUtf8("我").at(0).toUpper();
    ack.avatarColor  = QColor("#1565C0");
    ack.sendState = MessageItem::SendSent;
    m_messageList->appendMessage(ack);
}

void MessagePage::onTtsDone(int chatId, qint64 localId, bool success,
                            const QString& caption, const QString& text,
                            const QString& errorMsg)
{
    if (QString::number(chatId) != m_chatId || m_protocol != QLatin1String("tts")) {
        return;
    }
    if (localId != 0) {
        m_messageList->updateSendStateByLocalId(
            localId,
            success ? MessageItem::SendSent : MessageItem::SendFailed,
            success ? QString() : errorMsg);
    }
    if (!success) {
        return;
    }
    MessageItem ack;
    ack.sender = QString::fromUtf8("我");
    ack.content = caption + QString::fromUtf8("\n") + text;
    ack.time = QTime::currentTime().toString("HH:mm");
    ack.isSelf = true;
    ack.avatarLetter = QString::fromUtf8("我").at(0).toUpper();
    ack.avatarColor  = QColor("#1565C0");
    ack.sendState = MessageItem::SendSent;
    m_messageList->appendMessage(ack);
}
