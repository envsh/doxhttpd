#include "sharepickpage.h"

#include "pagemanager.h"
#include "msgdb_bridge.h"
#include "shareintentreceiver.h"
#include "virtualcontacts.h"

#include <QskLinearBox.h>
#include <QskPushButton.h>
#include <QskTextLabel.h>

SharePickPage::SharePickPage(QQuickItem* parent)
    : Page(parent)
{
}

void SharePickPage::onCreate(const QVariantMap& launchArgs, const QVariantMap&)
{
    setAutoLayoutChildren(true);
    rebuildFrom(launchArgs);
}

void SharePickPage::onNewIntent(const QVariantMap& launchArgs)
{
    // 二连分享：Transient 缓存命中走 onNewIntent（pagemanager.cpp 缓存分支），
    // 不清空重建就会显示上一份分享的 meta
    rebuildFrom(launchArgs);
}

void SharePickPage::rebuildFrom(const QVariantMap& launchArgs)
{
    if (m_layout) {
        m_layout->deleteLater();
        m_layout = nullptr;
    }
    m_layout = new QskLinearBox(Qt::Vertical, this);
    m_layout->setPanel(true);

    auto* layout = m_layout;
    layout->addStretch(1);

    const QVariantMap meta = launchArgs.value("shareMeta").toMap();
    const QString preview = meta.value("messageText").toString();
    const QString sourceApp = meta.value("sourceApp").toString();
    const QString sourceLine = sourceApp.isEmpty()
        ? QString() : QString::fromUtf8("来自 %1").arg(sourceApp);

    auto* head = new QskTextLabel(preview, layout);
    head->setAlignment(Qt::AlignCenter);
    if (!sourceLine.isEmpty()) {
        auto* sub = new QskTextLabel(sourceLine, layout);
        sub->setAlignment(Qt::AlignCenter);
    }

    layout->addSpacer(24, 0);

    auto* caption = new QskTextLabel(QString::fromUtf8("发送到…"), layout);
    caption->setAlignment(Qt::AlignCenter);

    // 简易联系人列表：channels 真实会话（排除虚拟协议），最近活跃在前
    // （loadChannels 内 ORDER BY last_active DESC）
    const QVector<MsgDbBridge::ChannelRecord> recs = MsgDbBridge::loadChannels();
    int shown = 0;
    for (const auto& rec : recs) {
        if (isVirtualProtocol(rec.protocol)) {
            continue;
        }
        const QString title = rec.name.isEmpty() ? rec.chatId : rec.name;
        // channellist.cpp:665-680 口径：真实会话 chatType=protocol、
        // chanId 兜底 protocol+'_'+chatId
        const QVariantMap args{
            { QStringLiteral("channelName"), title },
            { QStringLiteral("chatId"), rec.chatId },
            { QStringLiteral("chatType"), rec.protocol },
            { QStringLiteral("protocol"), rec.protocol },
            { QStringLiteral("chanId"), rec.chanId.isEmpty()
                ? rec.protocol + QLatin1Char('_') + rec.chatId : rec.chanId },
            { QStringLiteral("chanIcon"), rec.icon },
            { QStringLiteral("chanLetter"),
                QString(title.isEmpty() ? QChar(' ') : title.at(0)).toUpper() },
            { QStringLiteral("pendingSend"), preview },
        };
        ++shown;
        auto* btn = new QskPushButton(title, layout);
        btn->setPreferredWidth(340);
        btn->setPreferredHeight(52);
        connect(btn, &QskAbstractButton::clicked, this, [this, args]() {
            pageManager()->open("message", args);
        });
    }
    if (shown == 0) {
        auto* empty = new QskTextLabel(QString::fromUtf8("暂无联系人"), layout);
        empty->setAlignment(Qt::AlignCenter);
    }

    layout->addSpacer(20, 0);
    layout->addStretch(1);
}
