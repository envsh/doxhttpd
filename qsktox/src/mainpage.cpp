#include "mainpage.h"
#include "pagemanager.h"
#include "menuoverlay.h"
#include "channellist.h"
#include "messagepage.h"
#include "pushstatusbar.h"
#include "pushhandler.h"
#include "messagestore.h"
#include "msgdb_bridge.h"
#include "virtualcontacts.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskMenu.h>
#include <QskBoxShapeMetrics.h>
#include <QskLabelData.h>
#include <QTimer>
#include <QCoreApplication>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTime>
#include <QDateTime>
#include "androidutils.h"
#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

static constexpr int FLAG_KEEP_SCREEN_ON = 0x80;

static void jniKeepScreenOn(bool on) {
#ifdef Q_OS_ANDROID
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([on]() {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid()) return;
        QJniObject window = activity.callObjectMethod(
            "getWindow", "()Landroid/view/Window;");
        if (!window.isValid()) return;
        if (on)
            window.callMethod<void>("addFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
        else
            window.callMethod<void>("clearFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
    }).waitForFinished();
#else
    Q_UNUSED(on)
#endif
}

namespace {

// timestamp 单位自适应：按数值位数判断 秒 / 毫秒 / 微秒 / 纳秒
qint64 epochSeconds(qint64 v)
{
    const qint64 a = (v < 0) ? -v : v;
    if (a >= 100000000000000000LL) {    // ≥1e17（约18-19位）：纳秒
        return v / 1000000000;
    }
    if (a >= 100000000000000LL) {   // ≥1e14（约16位）：微秒
        return v / 1000000;
    }
    if (a >= 100000000000LL) {      // ≥1e11（约13位）：毫秒
        return v / 1000;
    }
    return v;                       // 约10位：秒
}

} // namespace

MainPage::MainPage(QQuickItem* parent)
    : Page(parent)
{
}

void MainPage::onCreate(const QVariantMap& launchArgs, const QVariantMap&)
{
    registerMainPage(this);

    QString url = launchArgs.value("url").toString();
    if (!url.isEmpty()) {
        qDebug() << "[MainPage] connecting to:" << url;
    }

    // Restore keepScreenOn, defer JNI to after window is ready
    m_keepScreenOn = QSettings().value("keepScreenOn", true).toBool();
    QTimer::singleShot(50, this, [this]() { jniKeepScreenOn(m_keepScreenOn); });

    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);

    // ── TopBar ──
    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);
    topBar->setSpacing(8);

    // Left: Avatar (circular button with initial)
    auto* avatarBtn = new QskPushButton("U", topBar);
    avatarBtn->setPreferredSize(44, 44);
    avatarBtn->setBoxShapeHint(QskPushButton::Panel,
        QskBoxShapeMetrics(100, Qt::RelativeSize));

    // Center: Title
    auto* title = new QskTextLabel("qsktox", topBar);
    title->setAlignment(Qt::AlignCenter);
    title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);

    // Right: Options button
    auto* optionsBtn = new QskPushButton(QString::fromUtf8("⋮"), topBar);
    optionsBtn->setPreferredSize(44, 44);
    optionsBtn->setBoxShapeHint(QskPushButton::Panel,
        QskBoxShapeMetrics(8, Qt::AbsoluteSize));

    connect(optionsBtn, &QskAbstractButton::clicked, [this, optionsBtn]() {
        for (auto* old : findChildren<QskMenu*>())
            old->deleteLater();
        for (auto* old : findChildren<MenuOverlay*>())
            old->deleteLater();

        auto* menu = new QskMenu(this);
        menu->setModal(true);
        menu->addOption(QskLabelData(
            m_keepScreenOn ? QString::fromUtf8("✓ Keep Screen On")
                           : QString("  Keep Screen On")));
        menu->addSeparator();
        menu->addOption(QskLabelData("App Log"));
        menu->addSeparator();
        menu->addOption(QskLabelData("Settings"));
        menu->addSeparator();
        menu->addOption(QskLabelData("About"));
        menu->addSeparator();
        menu->addOption(QskLabelData("Logout"));

        QPointF btnPos = optionsBtn->mapToItem(this, QPointF(0, 0));
        qreal menuW = menu->implicitWidth();
        if (menuW <= 0) menuW = 200;
        menu->setOrigin(QPointF(
            btnPos.x() + optionsBtn->width() - menuW,
            btnPos.y() + optionsBtn->height()));

        connect(menu, &QskMenu::triggered, this, [this](int index) {
            if (index == 0) {
                m_keepScreenOn = !m_keepScreenOn;
                jniKeepScreenOn(m_keepScreenOn);
                emit keepScreenOnChanged(m_keepScreenOn);
            } else if (index == 2) {
                pageManager()->open("logs");
            } else if (index == 4) {
                pageManager()->open("settings");
            } else if (index == 6) {
                pageManager()->open("about");
            } else if (index == 8) {
                pageManager()->replace("login");
            }
            if (auto* m = qobject_cast<QskMenu*>(sender()))
                m->close();
        });

        {
            auto* overlay = new MenuOverlay(menu);
            connect(menu, &QObject::destroyed, overlay, &QObject::deleteLater);
        }

        menu->open();
    });

    // ── Push Status Bar ──
    new PushStatusBar(layout);

    // ── 虚拟频道播种（bot 类需先落库：messages.chanid 外键引用 channels）──
    {
        const QVector<VirtualContact>& vcs = virtualContacts();
        for (const auto& vc : vcs) {
            const QString proto = QString::fromUtf8(vc.type);
            const int kind = virtualSendKind(proto);
            if (kind != kVirtualBotAigpt
                && kind != kVirtualBotPaste
                && kind != kVirtualBotTranslate
                && kind != kVirtualBotWebsearch
                && kind != kVirtualBotTts) {
                continue;
            }
            MsgDbBridge::upsertChannel(proto, QString::number(vc.id),
                QString::fromUtf8(vc.name), QStringLiteral("online"),
                false, QString(), QString(),
                QDateTime::currentSecsSinceEpoch());
        }
    }

    // ── ChannelList (频道列表) ──
    m_channelList = new ChannelListWidget(layout);
    m_channelList->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    m_channelList->populateData();

    connect(m_channelList, &ChannelListWidget::rowClicked,
        this, [this](int row, const QString& chatName) {
            Q_UNUSED(chatName)
            openChatForRow(row);
        });

    connect(m_channelList, &ChannelListWidget::rowLongPressed,
        this, [this](int row, const QPointF& pos) {
            showChannelMenu(row, pos);
        });

    // ── 头像单击/长按菜单（长按与单击同菜单，区别于联系人行手势）──
    connect(m_channelList, &ChannelListWidget::avatarClicked,
        this, [this](int row, const QPointF& pos) {
            showContactAvatarMenu(row, pos);
        });
    connect(m_channelList, &ChannelListWidget::avatarLongPressed,
        this, [this](int row, const QPointF& pos) {
            showContactAvatarMenu(row, pos);
        });

    // ── Push message handler ──
    connect(PushHandler::instance(), &PushHandler::pushMessage,
        this, [this](const QByteArray& message, const QString& instance) {
            qDebug() << "[MainPage] push message received, size:" << message.size()
                     << "content:" << QString::fromUtf8(message).left(1900);

            QJsonDocument doc = QJsonDocument::fromJson(message);
            if (!doc.isObject()) {
                qWarning() << "[MainPage] invalid JSON payload";
                return;
            }
            QJsonObject obj = doc.object();

            QString chatId    = obj.value("chat_id").toString();
            QString protocol  = obj.value("protocol").toString();
            QString accountId = obj.value("account_id").toString();
            QString userId    = obj.value("user_id").toString();
            if (chatId.isEmpty()) {
                qWarning() << "[MainPage] chat_id is empty, skip"
                           << "protocol:" << (protocol.isEmpty()
                                   ? QString::fromUtf8("✗") : QString::fromUtf8("✓"))
                           << "account_id:" << (accountId.isEmpty()
                                   ? QString::fromUtf8("✗") : QString::fromUtf8("✓"))
                           << "user_id:" << (userId.isEmpty()
                                   ? QString::fromUtf8("✗") : QString::fromUtf8("✓"));
                return;
            }

            QString chatName = obj.value("chat_name").toString();
            QString chatType = obj.value("chat_type").toString();
            QString username = obj.value("usernick").toString();
            if (username.isEmpty()) {
                username = obj.value("username").toString();
            }
            QString usericon = obj.value("user_icon").toString();
            QStringList replyTos;
            {
                const QJsonArray rt = obj.value("reply_tos").toArray();
                for (const auto& v : rt) {
                    replyTos.append(v.toString());
                }
            }
            QStringList mentions;
            {
                const QJsonArray mv = obj.value("mentions").toArray();
                for (const auto& v : mv) {
                    mentions.append(v.toString());
                }
            }
            QString text = obj.value("text").toString();
            qint64 timestamp = obj.value("timestamp").toVariant().toLongLong();

            QString timeStr;
            if (timestamp > 0) {
                timeStr = QDateTime::fromSecsSinceEpoch(epochSeconds(timestamp)).toString("HH:mm");
            }

            static const QColor s_palette[] = {
                QColor("#4CAF50"), QColor("#2196F3"), QColor("#FF9800"),
                QColor("#E91E63"), QColor("#9C27B0"), QColor("#00BCD4"),
                QColor("#FF5722"), QColor("#795548"), QColor("#607D8B"),
            };
            QColor avatarColor = s_palette[qHash(chatId) % 9];
            QString avatarLetter = chatName.isEmpty()
                ? QString(chatId.at(0).toUpper())
                : QString(chatName.at(0));

            MessageItem msgItem;
            msgItem.sender = username;
            msgItem.content = text;
            msgItem.time = timeStr;
            msgItem.replyTos = replyTos;
            msgItem.mentions = mentions;
            msgItem.isSelf = false;
            msgItem.avatarUrl = usericon;
            {
                QString sname = username.isEmpty() ? chatName : username;
                if (sname.isEmpty()) sname = chatId;
                msgItem.avatarLetter = QString(sname.isEmpty() ? QChar(' ') : sname.at(0)).toUpper();
                static const QColor mp[] = {QColor("#4CAF50"),QColor("#2196F3"),QColor("#FF9800"),QColor("#E91E63"),QColor("#9C27B0"),QColor("#00BCD4"),QColor("#FF5722"),QColor("#795548"),QColor("#607D8B")};
                msgItem.avatarColor = mp[qHash(sname) % 9];
            }
            MessageStore::instance()->addMessage(protocol, chatId, msgItem);

            ChannelItem chItem;
            chItem.avatarLetter = avatarLetter;
            chItem.avatarColor = avatarColor;
            chItem.title = chatName.isEmpty() ? chatId : chatName;
            chItem.lastMessage = text;
            chItem.time = timeStr;
            chItem.chatId = chatId;
            chItem.chatType = chatType;
            chItem.protocol = protocol;
            chItem.chanId = MsgDbBridge::resolveChanid(protocol, chatId);
            // 排序键：与 DB 侧 updateChannelLastMessage 写入的 last_active 同源同值
            chItem.lastActive = QDateTime::currentSecsSinceEpoch();

            bool isCurrent = false;
            if (auto* pm = pageManager()) {
                if (auto* mp = qobject_cast<MessagePage*>(pm->currentPage())) {
                    isCurrent = (mp->chatId() == chatId
                                 && mp->protocol() == protocol);
                }
            }

            MsgDbBridge::onIncomingMessage(protocol, chatId, chItem.title,
                                           msgItem, isCurrent, nullptr, nullptr);

            chItem.unreadDelta = isCurrent ? 0 : 1;
            m_channelList->insertOrUpdateChannel(chatId, chItem);
        });

    // ── Toast label (initially hidden, overlaid on top) ──
    m_toastLabel = new QskTextLabel("", this);
    m_toastLabel->setAlignment(Qt::AlignCenter);
    m_toastLabel->setVisible(false);
    m_toastLabel->setZ(1000);
    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, this, [this]() {
        m_toastLabel->setVisible(false);
    });

    // ── BottomNav ──
    auto* navBar = new QskLinearBox(Qt::Horizontal, layout);
    navBar->setPanel(true);
    navBar->setFixedHeight(56); // 不能用preferedSize,会导致下半部分被裁切掉看不见
    navBar->setSpacing(0);

    struct TabDef { const char* icon; const char* label; };
    static const TabDef tabDefs[4] = {
        { "💬", "聊天" },
        { "👤", "联系人" },
        { "⚙", "设置" },
        { "🔔", "通知" },
    };

    for (int i = 0; i < 4; ++i) {
        auto* cell = new QskLinearBox(Qt::Vertical, navBar);
        cell->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
        cell->setSpacing(2);

        auto* icon = new QskPushButton(QString::fromUtf8(tabDefs[i].icon), cell);
        icon->setPreferredHeight(28);
        icon->setSizePolicy(QskSizePolicy::Preferred, QskSizePolicy::Preferred);
        icon->setBoxShapeHint(QskPushButton::Panel, QskBoxShapeMetrics(8, Qt::RelativeSize));

        auto* lbl = new QskTextLabel(QString::fromUtf8(tabDefs[i].label), cell);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setPreferredHeight(18);
        lbl->setSizePolicy(QskSizePolicy::Preferred, QskSizePolicy::Preferred);

        m_navTabs[i].btn = icon;
        m_navTabs[i].label = lbl;

        connect(icon, &QskAbstractButton::clicked, this, [this, i]() { setActiveTab(i); });
    }

    setActiveTab(0);

#ifdef Q_OS_ANDROID
    // auto* safeSpacer = new QskLinearBox(Qt::Horizontal, layout);
    // safeSpacer->setFixedHeight(30);
#endif
}

void MainPage::onNewIntent(const QVariantMap& launchArgs)
{
    QString url = launchArgs.value("url").toString();
    if (!url.isEmpty()) {
        qDebug() << "[MainPage] onNewIntent, url:" << url;
    }
}

void MainPage::onPause()
{
    if (m_channelList) {
        m_channelList->stopFadeIn();
    }
}

void MainPage::onResume()
{
    Page::onResume();
    if (m_channelList) {
        m_channelList->reloadUnreadFromDb();
    }
}

void MainPage::setActiveTab(int index)
{
    m_activeTab = index;
    const QColor activeColor(255, 255, 255);
    const QColor inactiveColor(160, 160, 160);

    for (int i = 0; i < 4; ++i) {
        bool selected = (i == index);
        m_navTabs[i].btn->setEmphasis(selected
            ? QskPushButton::HighEmphasis
            : QskPushButton::LowEmphasis);
        m_navTabs[i].label->setTextColor(selected ? activeColor : inactiveColor);
    }

    if (index == 2) {
        pageManager()->open("settings");
    } else if (index != 0) {
        static const char* names[] = { nullptr, "联系人", nullptr, "通知" };
        if (names[index]) {
            showToast(QString::fromUtf8(names[index]) + QString::fromUtf8(" (coming soon)"));
        }
    }
}

void MainPage::showToast(const QString& msg, int durationMs) {
    m_toastLabel->setText(msg);
    m_toastLabel->setVisible(true);
    m_toastTimer->start(durationMs);
}

void MainPage::openChatForRow(int row)
{
    if (row < 0 || !m_channelList || row >= m_channelList->count()) {
        return;
    }
    QString chatId = m_channelList->chatIdForRow(row);
    QString chatType = m_channelList->chatTypeForRow(row);
    QString protocol = m_channelList->protocolForRow(row);
    QString chanId = m_channelList->chanIdForRow(row);
    m_channelList->markLocallyRead(protocol, chatId, chanId);
    pageManager()->open("message",
        {{"channelIndex", row},
         {"channelName", m_channelList->channelItem(row).title},
         {"chatId", chatId}, {"chatType", chatType},
         {"protocol", protocol}, {"chanId", chanId}});
}

void MainPage::showContactAvatarMenu(int row, const QPointF& pos)
{
    for (auto* old : findChildren<QskMenu*>()) {
        old->deleteLater();
    }
    for (auto* old : findChildren<MenuOverlay*>()) {
        old->deleteLater();
    }

    auto* menu = new QskMenu(this);
    menu->setModal(true);
    menu->addOption(QskLabelData(QString::fromUtf8("查看资料")));
    menu->addOption(QskLabelData(QString::fromUtf8("发消息")));
    menu->addOption(QskLabelData(QString::fromUtf8("置顶")));
    menu->addOption(QskLabelData(QString::fromUtf8("静音")));

    menu->setOrigin(pos);

    connect(menu, &QskMenu::triggered, this, [this, row](int index) {
        if (index == 0) {
            if (row >= 0 && m_channelList && row < m_channelList->count()) {
                const auto& ch = m_channelList->channelItem(row);
                QString info = QString::fromUtf8("标题: ") + ch.title +
                               QString::fromUtf8("\nID: ") + ch.chatId +
                               QString::fromUtf8("\n最后消息: ") + ch.lastMessage +
                               QString::fromUtf8("\n时间: ") + ch.time +
                               QString::fromUtf8("\n未读: ") + QString::number(ch.unreadCount);
                showToast(info, 4000);
            } else {
                showToast(QString::fromUtf8("暂无详细信息"));
            }
        } else if (index == 1) {
            openChatForRow(row);
        }
        // index 2/3（置顶/静音）：占位，与行菜单现状一致待做实
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

void MainPage::showChannelMenu(int row, const QPointF& pos) {
    Q_UNUSED(pos)

    for (auto* old : findChildren<QskMenu*>())
        old->deleteLater();
    for (auto* old : findChildren<MenuOverlay*>())
        old->deleteLater();

    auto* menu = new QskMenu(this);
    menu->setModal(true);
    menu->addOption(QskLabelData("Mute"));
    menu->addOption(QskLabelData("Pin"));
    menu->addOption(QskLabelData("Delete"));
    menu->addSeparator();
    menu->addOption(QskLabelData(QString::fromUtf8("详细信息")));

    menu->setOrigin(pos);

    connect(menu, &QskMenu::triggered, this, [this, row](int index) {
        if (index == 3) {
            if (row >= 0 && m_channelList && row < m_channelList->count()) {
                const auto& ch = m_channelList->channelItem(row);
                QString info = QString::fromUtf8("标题: ") + ch.title +
                               QString::fromUtf8("\nID: ") + ch.chatId +
                               QString::fromUtf8("\n最后消息: ") + ch.lastMessage +
                               QString::fromUtf8("\n时间: ") + ch.time +
                               QString::fromUtf8("\n未读: ") + QString::number(ch.unreadCount);
                showToast(info, 4000);
            } else {
                showToast(QString::fromUtf8("暂无详细信息"));
            }
        }
        if (auto* m = qobject_cast<QskMenu*>(QObject::sender()))
            m->close();
    });

    {
        auto* overlay = new MenuOverlay(menu);
        connect(menu, &QObject::destroyed, overlay, &QObject::deleteLater);
    }
    menu->open();
}
void MainPage::handleShareIntent(const QString& action, const QString& mimeType,
                                   const QString& text, const QString& urisJson)
{
    qDebug() << "[MainPage] handleShareIntent:" << action << mimeType;

    if (action == "android.intent.action.SEND") {
        if (mimeType == "text/plain" && !text.isEmpty()) {
            showAndroidToast("Shared text: " + text);
        } else {
            QJsonDocument doc = QJsonDocument::fromJson(urisJson.toUtf8());
            QJsonArray arr = doc.array();
            if (!arr.isEmpty()) {
                showAndroidToast("Shared file: " + mimeType + "\n" + arr[0].toString());
            } else {
                showAndroidToast("Shared: " + mimeType);
            }
        }
    } else if (action == "android.intent.action.SEND_MULTIPLE") {
        QJsonDocument doc = QJsonDocument::fromJson(urisJson.toUtf8());
        int count = doc.array().size();
        showAndroidToast(QString("Shared %1 files").arg(count));
    }
}

void MainPage::setKeepScreenOn(bool on) {
    if (m_keepScreenOn == on) return;
    m_keepScreenOn = on;
}
