#include "messagepage.h"
#include "messagelist.h"
#include "msgenlargeoverlay.h"
#include "androidutils.h"
#include "menuoverlay.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
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

    auto* title = new QskTextLabel(channelName, topBar);
    title->setAlignment(Qt::AlignCenter);
    title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);

    // ── MessageList ──
    m_messageList = new MessageListWidget(layout);
    m_messageList->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    m_messageList->setChannel(chatId);

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
                if (auto* m = qobject_cast<QskMenu*>(sender()))
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

    m_messageList->appendMessage(item);
    m_input->setText("");
}
