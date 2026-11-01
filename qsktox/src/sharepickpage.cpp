#include "sharepickpage.h"

#include "pagemanager.h"
#include "msgdb_bridge.h"
#include "myscrollarea.h"
#include "shareintentreceiver.h"
#include "virtualcontacts.h"

#include <QDateTime>

#include <QskLinearBox.h>
#include <QskFontRole.h>
#include <QskTextOptions.h>
#include <QskTextLabel.h>
#include <QtMath>
#include <private/qquicktaphandler_p.h>

SharePickPage::SharePickPage(QQuickItem* parent)
    : Page(parent)
{
}

namespace {

// 行高 = channellist.cpp ROW_HEIGHT 同款口径（tap 行索引换算用）
constexpr qreal kRowHeight = 52.0;

} // namespace

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
    m_shareTargets.clear();
    m_layout = new QskLinearBox(Qt::Vertical, this);
    m_layout->setPanel(true);

    auto* layout = m_layout;
    layout->addSpacer(16, 0);

    const QVariantMap meta = launchArgs.value("shareMeta").toMap();
    const QString preview = meta.value("messageText").toString();
    const QString sourceApp = meta.value("sourceApp").toString();
    const QString mime = meta.value("mime").toString();
    const int imageCount = meta.value("imageCount").toInt();
    const qint64 totalBytes = meta.value("totalBytes").toLongLong();
    const qint64 receivedAt = meta.value("receivedAt").toLongLong();

    // ── 顶部固定元信息区（2-3 行，不参与滚动；滚动只属于联系人列表）──
    // 行1：类型/内容预览（文本原样；图片/文件为 emoji+文件名+大小的 messageText）
    auto* head = new QskTextLabel(preview, layout);
    head->setAlignment(Qt::AlignCenter);
    head->setFontRole(QskFontRole(QskFontRole::Subtitle, QskFontRole::High));
    head->setWrapMode(QskTextOptions::WordWrap);

    // 行2：类型分类（kind=image/text/file + mime + 个数 + 总大小）
    QString detailLine;
    const QString kindRaw = meta.value("kind").toString();
    if (kindRaw == QLatin1String("text")) {
        detailLine = QString::fromUtf8("文本");
    } else {
        detailLine = kindRaw == QLatin1String("image")
            ? QString::fromUtf8("图片") : QString::fromUtf8("文件");
        if (!mime.isEmpty()) {
            detailLine += QString::fromUtf8(" · %1").arg(mime);
        }
        if (imageCount > 1) {
            detailLine += QString::fromUtf8(" · %1 个").arg(imageCount);
        }
        if (totalBytes > 0) {
            detailLine += QString::fromUtf8(" · %1").arg(formatShareBytes(totalBytes));
        }
    }
    if (!detailLine.isEmpty()) {
        auto* detail = new QskTextLabel(detailLine, layout);
        detail->setAlignment(Qt::AlignCenter);
        detail->setFontRole(QskFontRole(QskFontRole::Caption, QskFontRole::Low));
        detail->setWrapMode(QskTextOptions::WordWrap);
    }

    // 行3：来源 app + 接收时间
    QString sourceLine;
    if (!sourceApp.isEmpty()) {
        sourceLine = QString::fromUtf8("来自 %1").arg(sourceApp);
    }
    if (receivedAt > 0) {
        if (!sourceLine.isEmpty()) {
            sourceLine += QString::fromUtf8(" · ");
        }
        sourceLine += QDateTime::fromMSecsSinceEpoch(receivedAt)
            .toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    }
    if (!sourceLine.isEmpty()) {
        auto* src = new QskTextLabel(sourceLine, layout);
        src->setAlignment(Qt::AlignCenter);
        src->setFontRole(QskFontRole(QskFontRole::Caption, QskFontRole::Low));
        src->setWrapMode(QskTextOptions::WordWrap);
    }

    layout->addSpacer(24, 0);

    auto* caption = new QskTextLabel(QString::fromUtf8("发送到…"), layout);
    caption->setAlignment(Qt::AlignCenter);

    // 简易联系人列表：channels 真实会话（排除虚拟协议），最近活跃在前
    // （loadChannels 内 ORDER BY last_active DESC）。
    // 超一屏可滚：MyScrollArea（rawdatapopup.cpp:81-102 先例）+
    // 内层 QskLinearBox 自动布局出 implicit 高度
    auto* scroll = new MyScrollArea(layout);
    scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_layout->setStretchFactor(scroll, 1);
    scroll->setItemResizable(true);

    auto* list = new QskLinearBox(Qt::Vertical, scroll);
    // 行高定宽 → tap 行索引换算需要零间距
    list->setSpacing(0);
    scroll->setScrolledItem(list);

    const QVector<MsgDbBridge::ChannelRecord> recs = MsgDbBridge::loadChannels();
    for (const auto& rec : recs) {
        if (isVirtualProtocol(rec.protocol)) {
            continue;
        }
        const QString title = rec.name.isEmpty() ? rec.chatId : rec.name;
        // channellist.cpp:665-680 口径：真实会话 chatType=protocol、
        // chanId 兜底 protocol+'_'+chatId
        QVariantMap args{
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
        m_shareTargets.append(std::move(args));
        auto* row = new QskTextLabel(title, list);
        row->setPreferredHeight(kRowHeight);
        row->setAlignment(Qt::AlignCenter);
    }

    // 行命中：QQuickTapHandler + DragThreshold（channellist.cpp:412-426 同款）——
    // 手势位移超过系统 press-to-drag 阈值即不再触发 singleTapped，
    // 滚动结束后抓离不及时也不会误触进入聊天
    auto* tapHandler = new QQuickTapHandler(list);
    tapHandler->setGesturePolicy(QQuickTapHandler::DragThreshold);
    connect(tapHandler, &QQuickTapHandler::singleTapped, this,
        [this](QEventPoint point, Qt::MouseButton) {
            const int row = qFloor(point.position().y() / kRowHeight);
            if (row < 0 || row >= m_shareTargets.size()) {
                return;
            }
            pageManager()->open("message", m_shareTargets[row]);
        });

    if (m_shareTargets.isEmpty()) {
        auto* empty = new QskTextLabel(QString::fromUtf8("暂无联系人"), list);
        empty->setAlignment(Qt::AlignCenter);
    }
}
