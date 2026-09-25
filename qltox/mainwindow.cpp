#include "mainwindow.h"
#include "CustomTitleBar.h"
#include "restapi.h"
#include "eventpoller.h"
#include "pureconsts.hpp"
#include "unknownparser.h"
#include "version.h"
#include "avatar_manager.h"
#include "translator.h"
#include "logindialog.h"
#include "conferenceinvitedialog.h"
#include "groupinvitedialog.h"
#include "friendinfodialog.h"
#include "memberlistdialog.h"
#include "storage.h"
#include "channel_db.h"
#include "cache_db.h"
#include "cache_fs.h"
#include "config.h"
#include "cJSON.h"
#include "msgdb_helper.h"
#include "jsonview.h"
#include "appsetup.h"
#include "assertf.h"
#include <qmessagebox.h>
#include <qtextcodec.h>
#include <qtextstream.h>
#include <qradiobutton.h>
#include <qinputdialog.h>
#include "placeholderlineedit.h"
#include "stickermanager.h"
#include "statisticsdialog.h"
#include "combinesearchwindow.h"
#include "sound.h"
#include "screenshotmanager.h"
#include "screenshotpreview.h"
#include <qfile.h>
#include <unistd.h>     // access()
#ifdef QT3_BUILD
#include <qurl.h>
#else
#include <QUrl>
#endif
#ifdef QT3_BUILD
#include <qdatetime.h>
#else
#include <QDateTime>
#endif
#ifdef QT3_BUILD
#include <qfileinfo.h>
#else
#include <QFileInfo>
#endif
#ifdef QT3_BUILD
#include <qdir.h>
#else
#include <QDir>
#endif
#include "toastwidget.h"
#include "sharedstatusbar.h"
#include "globaluiutil.h"
#include "photoviewer.h"
#include "media_shmem_cache.h"
#include <qtimer.h>
#include <qlabel.h>
#include "ConfigDialog.h"
#include <qpushbutton.h>
#include <qlineedit.h>
#include "plugin_manager_dialog.h"
#include <qevent.h>
#include "app_icon.xpm"
#ifndef QT3_BUILD
#include <QSignalMapper>
#endif
#ifdef __linux__
#include <malloc.h>
#endif

// 虚拟联系人 ID（使用 <-100 的负数避免与服务器 ID 及 "未选择" 哨兵值 -1 冲突）
static const int VIRTUAL_UNKNOWN_ID = -100;
static const int VIRTUAL_SYSEVENT_ID = -101;
static const int VIRTUAL_REDDIT_ID = -102;
static const int VIRTUAL_BOOKMARK_ID  = -103;
static const int VIRTUAL_AICHAT_ID    = -104;
static const int VIRTUAL_PASTEBIN_ID  = -105;
static const int VIRTUAL_TRANSLATE_ID = -106;

// sticonShowStatusMessage 使用的托盘实例（MainWindow 创建/销毁时维护，不导出）
static SystemTrayIcon* s_trayIcon = 0;

// ── 媒体本机播放辅助（worker 线程调用，不触碰 UI）──
static void urlIdPart(const std::string& mediaUrl, std::string* out) {
    size_t slash = mediaUrl.find_last_of('/');
    std::string id = (slash == std::string::npos) ? mediaUrl : mediaUrl.substr(slash + 1);
    size_t q = id.find('?');
    if (q != std::string::npos) { id = id.substr(0, q); }
    *out = id;
}

static bool isGifLikeCandidate(const std::string& mediaUrl) {
    std::string id;
    urlIdPart(mediaUrl, &id);   // 剥离 ?query 后再判扩展名
    std::string lower;
    for (size_t i = 0; i < id.size(); ++i) {
        char c = id[i];
        lower += (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    return lower.rfind(".webm") == lower.size() - 5
        || lower.rfind(".mp4")  == lower.size() - 4;
}

// ffprobe 输出的时长（Qt3 无 QString::trimmed）
static double fromDurationStr(const QString& s) {
#ifdef QT3_BUILD
    return s.stripWhiteSpace().toDouble();
#else
    return s.trimmed().toDouble();
#endif
}

// 媒体 URL 是否真 GIF（扩展名 .gif）
static bool isGifUrl(const std::string& mediaUrl) {
    std::string id;
    urlIdPart(mediaUrl, &id);
    std::string lower;
    for (size_t i = 0; i < id.size(); ++i) {
        char c = id[i];
        lower += (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    return lower.rfind(".gif") == lower.size() - 4;
}

// ffmpeg 抽代表帧（长视频本地预览图，worker 线程调用）
static bool extractVideoFrame(const std::string& srcFile, const std::string& outPath,
                              int seekSec = 1) {
    QString err;
    QStringList a;
    a << "-y" << "-ss" << QString::number(seekSec)
      << "-i" << qFromUtf8(srcFile)
      << "-vf" << "scale='min(320,iw)':-2"
      << "-frames:v" << "1"
      << "-f" << "image2" << "-update" << "1" << "-c:v" << "png"
      << qFromUtf8(outPath);
    if (qRuncmdCaptureOuterr("ffmpeg", a, &err) != 0) {
        qWarning("extractVideoFrame failed: %s", qToUtf8(err).data());
        return false;
    }
    return true;
}

// 抽帧预览的独立缓存 key（与媒体原字节区分，避免 download 阶段的视频字节占位）
static QString thumbCacheKey(const QString& mxcUrl) {
    return QString("@thumb@") + mxcUrl;
}

// ── media thumbnail 辅助函数（从原始字节解码 → 缩放到显示尺寸）──
static QPixmap decodeRawToThumb(const char* data, int len, int mediaW, int mediaH, int maxW) {
    TimePoint _t0 = timeNow();
    QPixmap tmp;
    tmp.loadFromData((const uchar*)data, len);
    if (tmp.isNull()) {
        std::string s(data, len);
        tmp = decodeWebP(s);
    }
    if (tmp.isNull()) {
        long long _el = elapsedMs(_t0); if (_el >= 50) qWarning("SLOW [hangui] decodeRawToThumb took %lldms", _el);
        return QPixmap();
    }
    long long _el = elapsedMs(_t0); if (_el >= 50) qWarning("SLOW [hangui] decodeRawToThumb took %lldms", _el);
    return makeScaledThumb(tmp, mediaW, mediaH, maxW);
}

// ── peer 持久化 helpers ──
// key 格式: "friend_N", "group_N_M", "conference_N_M", "unknown_*"
struct PeerKey {
    std::string chanid;
    int peerNum = 0;
    bool valid = false;
};
static PeerKey parsePeerKey(const std::string& key) {
    PeerKey ret;
    if (key.compare(0, 7, "friend_") == 0) {
        ret.chanid = "friend:" + key.substr(7);
        ret.peerNum = std::stoi(key.substr(7));
        ret.valid = true;
    } else if (key.compare(0, 6, "group_") == 0) {
        size_t us = key.find('_', 6);
        if (us == std::string::npos) { return ret; }
        ret.chanid = "group:" + key.substr(6, us - 6);
        ret.peerNum = std::stoi(key.substr(us + 1));
        ret.valid = true;
    } else if (key.compare(0, 11, "conference_") == 0) {
        size_t us = key.find('_', 11);
        if (us == std::string::npos) { return ret; }
        ret.chanid = "conference:" + key.substr(11, us - 11);
        ret.peerNum = std::stoi(key.substr(us + 1));
        ret.valid = true;
    } else if (key.compare(0, 8, "unknown_") == 0) {
        ret.chanid = "unknown:" + key.substr(8);
        ret.peerNum = 0;
        ret.valid = true;
    }
    return ret;
}

// Row→PeerInfo 转换 + 写入 peerInfoMap + 可选 avatar 下载
// 返回 map iterator，失败返回 end()
static std::map<std::string, PeerInfo>::iterator
loadRowToMap(std::map<std::string, PeerInfo>& m,
             const std::string& key,
             std::unique_ptr<PeerRow> row)
{
    if (!row) return m.end();
    PeerInfo pi;
    pi.peerNumber = row->peer_number;
    pi.publicKey  = row->public_key;
    pi.userName       = row->name;
    pi.nickname   = row->nickname;
    pi.iconUrl    = row->avatar_url;
    pi.statusText = row->status_text;
    pi.statusStr  = row->status_str;
    pi.userStatus = row->user_status;
    pi.peerIp     = row->peer_ip;
    pi.role       = row->role;
    pi.roleStr    = row->role_str;
    pi.isSelf     = row->is_self;
    pi.lastSeen   = (time_t)row->last_seen;
    pi.status     = row->status;
    auto result = m.insert({key, pi});
    if (result.second && !pi.iconUrl.empty()) {
        QString mxc = qFromUtf8(pi.iconUrl);
        if (AvatarManager::inst().requestDownload(mxc)) {
            ToxAPI::downloadAvatar(pi.iconUrl);
        }
    }
    return result.first;
}

static bool addPeerToDb(const std::string& key, const PeerInfo& pi) {
    PeerKey pk = parsePeerKey(key);
    if (!pk.valid) { return false; }
    if (pi.nickname.empty() && pi.iconUrl.empty()) {
        qWarning("addPeerToDb: skip, key=%s", key.c_str());
        return false;
    }
    PeerRow row;
    row.chanid = pk.chanid;
    row.peer_number = pk.peerNum;
    row.public_key = pi.publicKey;
    row.name = pi.userName;
    row.nickname = pi.nickname;
    row.avatar_url = pi.iconUrl;
    row.status_text = pi.statusText;
    row.status_str = pi.statusStr;
    row.user_status = pi.userStatus;
    row.peer_ip = pi.peerIp;
    row.role = pi.role;
    row.role_str = pi.roleStr;
    row.is_self = pi.isSelf;
    row.last_seen = (int64_t)pi.lastSeen;
    row.status = pi.status;
    auto* async = Storage::instance().channelDbAsync();
    if (async) {
        async->add_peer(std::move(row), nullptr);
    }
    return true;
}

static void updateContactDb(const std::string& chanid, const std::string& name,
                            const std::string& status, int is_connected,
                            const std::string& iconUrl = "",
                            const std::string& pubkey = "",
                            const std::string& protoType = "tox") {
    qDebug("updateContactDb: chanid=%s name=%s status=%s is_connected=%d",
           chanid.c_str(), name.c_str(), status.c_str(), is_connected);
    auto* async = Storage::instance().channelDbAsync();
    if (!async) return;
    ChannelRow row;
    row.chanid       = chanid;
    row.proto_type   = protoType;
    row.name         = name;
    row.status       = status;
    row.is_connected = is_connected;
    row.icon_url     = iconUrl;
    row.pubkey       = pubkey;
    async->update_contact_channel(std::move(row), nullptr);
}

static std::string chanidStr(int id, const QString& type) {
    return std::string(qToUtf8(type).data()) + "_" + std::to_string(id);
}

static void db_writeMarkRead(int id, const QString& type, int64_t lastReadRowid) {
    auto* async = Storage::instance().channelDbAsync();
    if (!async) return;
    async->mark_read(chanidStr(id, type), lastReadRowid, nullptr);
}

static void db_writeUnreadIncrement(int id, const QString& type, int64_t msgRowid = 0) {
    auto* async = Storage::instance().channelDbAsync();
    if (!async) return;
    async->increment_unread(chanidStr(id, type), 1, msgRowid, nullptr);
}

static void db_writeLastMessage(int id, const QString& type,
                                const QString& msg, const QString& timeStr,
                                int64_t rowid = 0) {
    auto* async = Storage::instance().channelDbAsync();
    if (!async) return;
    ChannelUpdate upd;
    upd.hasLastMessageText = true;
    upd.last_message_text = std::string(qToUtf8(msg).data());
    upd.hasLastMessageTime = true;
    upd.last_message_time = std::string(qToUtf8(timeStr).data());
    upd.hasLastActive = true;
    upd.last_active = (int64_t)::time(0);
    if (rowid > 0) {
        upd.hasLastMessageRowid = true;
        upd.last_message_rowid = rowid;
    }
    async->update_channel(chanidStr(id, type), std::move(upd), nullptr);
}

static void updatePeerInDb(const std::string& key, const PeerInfo& pi) {
    PeerKey pk = parsePeerKey(key);
    if (!pk.valid) { return; }
    if (pk.chanid.compare(0, 8, "unknown:") == 0) { return; } // unknown 不走部分更新
    PeerRow row;
    row.chanid = pk.chanid;
    row.peer_number = pk.peerNum;
    row.public_key = pi.publicKey;
    row.name = pi.userName;
    row.nickname = pi.nickname;
    row.avatar_url = pi.iconUrl;
    row.status_text = pi.statusText;
    row.status_str = pi.statusStr;
    row.user_status = pi.userStatus;
    row.peer_ip = pi.peerIp;
    row.role = pi.role;
    row.role_str = pi.roleStr;
    row.is_self = pi.isSelf;
    row.last_seen = (int64_t)pi.lastSeen;
    row.status = pi.status;
    auto* async = Storage::instance().channelDbAsync();
    if (async) {
        async->update_peer(std::move(row), nullptr);
    }
}

static PeerInfo& getOrCreatePeerEntry(std::map<std::string, PeerInfo>& m, const std::string& key) {
    auto it = m.find(key);
    if (it == m.end()) {
        it = m.insert({key, PeerInfo()}).first;
    }
    return it->second;
}

static void mergePeerInfo(PeerInfo& dst, const PeerInfo& src) {
    if (src.peerNumber != 0) dst.peerNumber = src.peerNumber;
    if (!src.userName.empty()) dst.userName = src.userName;
    if (!src.nickname.empty()) dst.nickname = src.nickname;
    if (src.status != 0) dst.status = src.status;
    if (!src.statusStr.empty()) dst.statusStr = src.statusStr;
    if (!src.statusText.empty()) dst.statusText = src.statusText;
    if (!src.iconUrl.empty()) dst.iconUrl = src.iconUrl;
    if (src.role != 0) dst.role = src.role;
    if (!src.roleStr.empty()) dst.roleStr = src.roleStr;
    if (!src.publicKey.empty()) dst.publicKey = src.publicKey;
    if (src.isSelf) dst.isSelf = true;
    if (!src.peerIp.empty()) dst.peerIp = src.peerIp;
    if (!src.userStatus.empty()) dst.userStatus = src.userStatus;
    if (src.lastSeen != 0) dst.lastSeen = src.lastSeen;
}

static QString timenowhm() {
#ifdef QT3_BUILD
    return QTime::currentTime().toString("hh:mm");
#else
    return QTime::currentTime().toString("HH:mm");
#endif
}

// 读取保存的语言设置
static QString loadSavedLanguage() {
    QString home = qGetHomePath();
    QFile file(home + "/.q3tox_lang");
    if (qOpenReadOnly(file)) {
        QTextStream stream(&file);
        QString lang = qTrim(stream.readLine());
        file.close();
        if (!lang.isEmpty()) return lang;
    }
    return "zh-CN"; // 默认简体
}

static void playNotificationSound() {
    const char* paths[] = {
        "sound/notification.s16le.pcm",
        "web/sound/notification.s16le.pcm",
        "../web/sound/notification.s16le.pcm"
    };
    for (int i = 0; i < 3; i++) {
        if (QFile::exists(paths[i])) {
            playSoundNopcm(paths[i]);
            return;
        }
    }
}

static bool unknownShouldPlaySound(const std::string& type,
                                   const std::string& message,
                                   const std::vector<std::string>& selfNames)
{
    if (type == kUnktoxFriendType || type == kUnktoxConferenceType || type == kUnktoxGroupType)
        return true;
    if (type == kImapMailType)
        return true;

    std::string lowerMsg = message;
    for (size_t i = 0; i < lowerMsg.size(); i++) { lowerMsg[i] = tolower((unsigned char)lowerMsg[i]); }
    for (size_t ni = 0; ni < selfNames.size(); ++ni) {
        std::string lowerName = selfNames[ni];
        for (size_t i = 0; i < lowerName.size(); i++) { lowerName[i] = tolower((unsigned char)lowerName[i]); }
        if (lowerMsg.find("@" + lowerName) != std::string::npos)
            return true;
    }
    if (lowerMsg.find("funami.tech") != std::string::npos)
        return true;
    if (lowerMsg.find("P5N4wp1ga06A") != std::string::npos)
		return true;

    return false;
}

static QString formatElapsedMs(int64_t ms) {
    if (ms < 1000) {
        return QString::number((int)ms) + "ms";
    }
    return QString::number((int)(ms / 1000)) + "." + QString::number((int)((ms % 1000) / 100)) + "s";
}

static void initDemoWidgets() {
    // Left side (addWidget)
    QLabel *leftLabel = new QLabel(qFromUtf8("Left"), SharedStatusBar::instance());
    SharedStatusBar::instance()->addWidget(leftLabel);

    QPushButton *leftBtn = new QPushButton(qFromUtf8("LBtn"), SharedStatusBar::instance());
    leftBtn->setFixedWidth(36);
    SharedStatusBar::instance()->addWidget(leftBtn);

    // Right side permanent (addPermanentWidget)
    QLabel *rightLabel = new QLabel(qFromUtf8("Right"), SharedStatusBar::instance());
    SharedStatusBar::instance()->addPermanentWidget(rightLabel);

    QPushButton *rightBtn = new QPushButton(qFromUtf8("RBtn"), SharedStatusBar::instance());
    rightBtn->setFixedWidth(36);
    SharedStatusBar::instance()->addPermanentWidget(rightBtn);

    QLineEdit *demoEdit = new QLineEdit(SharedStatusBar::instance());
    demoEdit->setFixedWidth(100);
#ifndef QT3_BUILD
    demoEdit->setPlaceholderText(qFromUtf8("input..."));
#endif
    SharedStatusBar::instance()->addPermanentWidget(demoEdit);

    SharedStatusBar::instance()->showMessage(
        qFromUtf8("Demo: 左端 widgets 隐藏中..."), 3000);
}

MainWindow::MainWindow(QWidget* parent) 
    : QMainWindow(parent
#ifdef QT3_BUILD
        , "mainwindow", Qt::WType_TopLevel | Qt::WStyle_Customize | Qt::WSubWindow | Qt::WStyle_MinMax | Qt::WStyle_SysMenu | Qt::WStyle_NoBorder
#else
        , Qt::FramelessWindowHint
#endif
    ), 
    currentChatId(-1), currentChatType("") {
    qWarning("MainWindow: constructor started");
    m_sleepBlocker = nullptr;
    m_tray = nullptr;
    m_forceQuit = false;
    
    // 设置窗口
    qSetWindowTitle(this, _("app_title"));
    setGeometry(100, 50, 1100, 680);

    // 初始化本地存储
    QString dataDir = qGetHomePath() + "/.cache/toxhttpd";
    Storage::instance().init(
#ifdef QT3_BUILD
        dataDir.utf8()
#else
        dataDir.toUtf8().constData()
#endif
    );

    // 设置 UTF-8 编解码器
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    
    // 底部状态栏浮窗
    SharedStatusBar::instance()->show();
    initDemoWidgets();
    
    QWidget* centralContainer = new QWidget(this);
    QBoxLayout* mainLayout = qNewBoxLayout(centralContainer, QBoxLayout::TopToBottom, 0, 0);

    // 主分割器（左右布局）
    splitter = new QSplitter(Qt::Horizontal, centralContainer);
    splitter->setOpaqueResize(true);
    
    // ===== 左侧边栏 =====
    QWidget* sidebar = new QWidget(splitter);
    QBoxLayout* sidebarLayout = qNewBoxLayout(sidebar, QBoxLayout::TopToBottom, 0, 0);
    sidebarLayout->setSpacing(0);
    qSetMargins(sidebarLayout, 0, 0, 0, 0);
    
    // 个人信息区
    selfInfoWidget = new SelfInfoWidget(sidebar);
    sidebarLayout->addWidget(selfInfoWidget);
    
    // 联系人列表
    contactListWidget = new ContactListWidget(sidebar);
    sidebarLayout->addWidget(contactListWidget, 1); // stretch
    
    splitter->addWidget(sidebar);
    sidebarWidget = sidebar;  // 保存 sidebar 指针
    
    // ===== 右侧聊天区 =====
    chatWidget = new ChatWidget(splitter);
    splitter->addWidget(chatWidget);
    
    // 设置 splitter 比例：左 1/3 (367px)，右 2/3 (733px)
    // 窗口固定 1100px，所以直接算像素值
    int leftWidth = 367;   // 1100 / 3 ≈ 367
    int rightWidth = 733;  // 1100 - 367 = 733
    
#ifdef QT3_BUILD
    QValueList<int> sizes;
    sizes << leftWidth << rightWidth;
    splitter->setSizes(sizes);
#else
    QList<int> sizes;
    sizes << leftWidth << rightWidth;
    splitter->setSizes(sizes);
#endif

    CustomTitleBar* titleBar = new CustomTitleBar(centralContainer);
    mainLayout->addWidget(titleBar, 0);
    mainLayout->addWidget(splitter, 1);
    setCentralWidget(centralContainer);

    // 连接信号槽
    connect(selfInfoWidget, SIGNAL(switchAccountRequested()),
            this, SLOT(onSwitchAccount()));
    connect(contactListWidget, SIGNAL(contactSelected(int, const QString&, const QString&)), 
            this, SLOT(onContactSelected(int, const QString&, const QString&)));
    connect(contactListWidget, SIGNAL(viewInfoRequested(int, const QString&)),
            this, SLOT(onViewInfoRequested(int, const QString&)));
    connect(contactListWidget, SIGNAL(deleteOrLeaveRequested(int, const QString&)),
            this, SLOT(onDeleteOrLeaveRequested(int, const QString&)));
    connect(contactListWidget, SIGNAL(viewMembersRequested(int, const QString&)),
            this, SLOT(onViewMembersRequested(int, const QString&)));
    connect(contactListWidget, SIGNAL(renameNickRequested(int, const QString&)),
            this, SLOT(onRenameNickRequested(int, const QString&)));
    connect(contactListWidget, SIGNAL(setGroupTopicRequested(int)),
            this, SLOT(onSetGroupTopicRequested(int)));
    connect(contactListWidget, SIGNAL(setConferenceTitleRequested(int)),
            this, SLOT(onSetConferenceTitleRequested(int)));
    connect(contactListWidget, SIGNAL(inviteToConferenceRequested(int)),
            this, SLOT(onInviteToConferenceRequested(int)));
    connect(chatWidget, SIGNAL(messageSent(const QString&, const QMap<QString,QString>&)),
            this, SLOT(onMessageSending(const QString&, const QMap<QString,QString>&)));
    connect(chatWidget, SIGNAL(languageChanged(const QString&)), 
            this, SLOT(onLanguageChanged(const QString&)));
    /*
     * Qt3 SIGNAL/SLOT 空格问题：见 chatwidget.cpp 注释
     * (connect() 不归一化空格，多参数 SIGNAL/SLOT 逗号后空格致静默失败)
     */
    connect(chatWidget, SIGNAL(translateRequested(int, const QString&, const QString&)),
            this, SLOT(onTranslateRequested(int, const QString&, const QString&)));
    connect(chatWidget, SIGNAL(translateForSendRequested(const QString&, const QString&)),
            this, SLOT(onTranslateForSendRequested(const QString&, const QString&)));
    connect(chatWidget, SIGNAL(sourceClicked(int)), this, SLOT(onSourceClicked(int)));
    connect(chatWidget, SIGNAL(retryClicked(int, const QString&, const QString&)), this, SLOT(onRetryClicked(int, const QString&, const QString&)));
    connect(chatWidget, SIGNAL(resendMessage(int)), this, SLOT(onResendMessage(int)));
    connect(chatWidget, SIGNAL(requestRedactMessage(int)), this, SLOT(onRequestRedactMessage(int)));
    connect(chatWidget, SIGNAL(openFullSizeImage(int, const QString&)),
            this, SLOT(onOpenFullSizeImage(int, const QString&)));
    connect(chatWidget, SIGNAL(openMediaPlayer(int)),
            this, SLOT(onOpenMediaPlayer(int)));
    connect(chatWidget, SIGNAL(favoriteClicked(int)), this, SLOT(onFavoriteClicked(int)));
    connect(chatWidget, SIGNAL(peerInfoRequested(int,const QString&,const QString&)),
            this, SLOT(onChatPeerInfoRequested(int,const QString&,const QString&)));
    connect(chatWidget, SIGNAL(screenshotRequested()), this, SLOT(onChatScreenshotRequested()));
    connect(chatWidget, SIGNAL(fileSendRequested(const QString&, const QString&)),
            this, SLOT(onFileSendRequested(const QString&, const QString&)));
    connect(&Translator::instance(), SIGNAL(languageChanged()), this, SLOT(retranslateUi()));
    
    // 启动事件轮询引擎
    EventPoller::start();
    ToxAPI::setEventTarget(this);
    setRowidEventTarget(this);
    ToxAPI::startPollEvent();

    // 桌面歌词（消息发送/结果提示）
    m_lyrics = new DesktopLyrics();
    m_lyrics->setFontSize(24);
    m_lyrics->showLyrics();

    m_msgTimer = new QTimer(this);
#ifdef QT3_BUILD
    connect(m_msgTimer, SIGNAL(timeout()), this, SLOT(clearLyricsHint()));
#else
    m_msgTimer->setSingleShot(true);
    connect(m_msgTimer, SIGNAL(timeout()), this, SLOT(clearLyricsHint()));
#endif

    m_hintActive = false;
    m_savedPlayedColor = m_lyrics->playedColor();

    // 时钟显示（无提示时显示当前时间）
    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, SIGNAL(timeout()), this, SLOT(showClockOnLyrics()));
    m_clockTimer->start(1000);
    showClockOnLyrics();

    // 从本地 SQLite 加载联系人列表（秒开），然后异步从服务器刷新
    {
        auto channels = Storage::instance().channelDb()->load_all_channels();
        ContactList list;
        m_accumulatedContactData.clear();

        for (const auto& row : channels) {
            // 解析 chanid: "friend_5" → type="friend", id=5
            auto sep = row.chanid.rfind('_');
            if (sep == std::string::npos) { continue; }
            std::string type = row.chanid.substr(0, sep);
            int id = std::stoi(row.chanid.substr(sep + 1));

            ContactData cd;
            cd.id = id; cd.type = type; cd.name = row.name;
            cd.status = row.status; cd.chatId = row.pubkey;
            cd.isConnected = row.is_connected != 0; cd.iconUrl = row.icon_url;
            m_accumulatedContactData.push_back(cd);

            Contact* c = new Contact();
            c->id = id; c->name = qFromUtf8(cd.name);
            c->type = qFromUtf8(cd.type); c->status = qFromUtf8(cd.status);
            c->chat_id = qFromUtf8(cd.chatId); c->is_connected = cd.isConnected;
            list.append(c);
        }

        // 追加虚拟联系人（不查 DB，纯硬编码 UI 占位符）
        auto addSeed = [&](int id, const char* name, const char* type) {
            Contact* c = new Contact();
            c->id = id; c->name = name; c->type = type;
            c->status = "online"; c->chat_id = ""; c->is_connected = false;
            list.append(c);
        };
        addSeed(VIRTUAL_UNKNOWN_ID, "Unknown", kUnknownType);
        addSeed(VIRTUAL_SYSEVENT_ID, "Sysevent", kSyseventType);
        addSeed(VIRTUAL_REDDIT_ID, "Reddit", kTopicType);
        addSeed(VIRTUAL_BOOKMARK_ID, "Bookmark", kBookmarkType);
        addSeed(VIRTUAL_AICHAT_ID, "AI Chat", kAichatType);
        addSeed(VIRTUAL_PASTEBIN_ID, "Paste Bin", kPastebinType);
        addSeed(VIRTUAL_TRANSLATE_ID, "Translate", kTranslateType);
        contactListWidget->setContacts(list);
    }
    
    // 异步加载初始数据
    qWarning("MainWindow: requesting initial data load (async)");
    ToxAPI::loadAllData();
    chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
    
    // 设置 frameless 窗口
    framelessHelper = new FramelessHelper(this);
    framelessHelper->setup(this);
    titleBar->connectFramelessHelper(framelessHelper);

    // ≡ 应用菜单：切换 menubar 显隐
    QObject::connect(titleBar, SIGNAL(appMenuClicked()), titleBar, SLOT(toggleMenu()));

    EmbeddedMenuBar* mb = titleBar->menuBar();

    MenuWidget34* file = mb->addMenu(qFromUtf8("文件(&F)"));
    EmbeddedMenuBar::addItem(file, qFromUtf8("新建\tCtrl+N"), this, SLOT(onMenu1Stub()));
    EmbeddedMenuBar::addSeparator(file);
    EmbeddedMenuBar::addItem(file, qFromUtf8("退出\tCtrl+Q"), this, SLOT(quitApp()));

    MenuWidget34* edit = mb->addMenu(qFromUtf8("编辑(&E)"));
    EmbeddedMenuBar::addItem(edit, qFromUtf8("撤销\tCtrl+Z"), this, SLOT(onMenu1Stub()));
    EmbeddedMenuBar::addItem(edit, qFromUtf8("重做\tCtrl+Shift+Z"), this, SLOT(onMenu1Stub()));

    MenuWidget34* tool = mb->addMenu(qFromUtf8("工具(&T)"));
    EmbeddedMenuBar::addItem(tool, qFromUtf8("统计(&T)..."), this, SLOT(openStatistics()));
    EmbeddedMenuBar::addItem(tool, qFromUtf8("日志(&L)..."), this, SLOT(onMenu1Stub()));
    EmbeddedMenuBar::addItem(tool, qFromUtf8("贴纸管理器(&S)..."), this, SLOT(openStickerManager()));
    EmbeddedMenuBar::addItem(tool, qFromUtf8("设置(&S)...\tCtrl+,"), this, SLOT(openSettings()));
    EmbeddedMenuBar::addItem(tool, qFromUtf8("Markdown编辑器(&M)...\tCtrl+M"), this, SLOT(openMdEditor()));

    // ── Etapps 菜单 ──
    m_etappsMenu = mb->addMenu(qFromUtf8("Etapps(&E)"));
    EmbeddedMenuBar::addItem(m_etappsMenu, qFromUtf8("插件管理..."), this, SLOT(openPluginManager()));
    EmbeddedMenuBar::addItem(m_etappsMenu, qFromUtf8("关闭全部窗口(&C)"), this, SLOT(onEtappCloseAll()));
    EmbeddedMenuBar::addSeparator(m_etappsMenu);

    MenuWidget34* help = mb->addMenu(qFromUtf8("帮助(&H)"));
    EmbeddedMenuBar::addItem(help, _("menu.homepage"), this, SLOT(openHomePage()));
    EmbeddedMenuBar::addItem(help, _("menu.aboutqt"), qApp, SLOT(aboutQt()));
    EmbeddedMenuBar::addItem(help, qFromUtf8("关于(&A)..."), this, SLOT(onAboutApp()));

    mb->finalize();

    // ── 系统托盘：关闭最小化到托盘，左键/双击恢复 ──
    if (SystemTrayIcon::isSystemTrayAvailable()) {
        m_tray = new SystemTrayIcon(QPixmap(app_icon), this);
        s_trayIcon = m_tray;
        m_tray->setToolTip(_("app_title"));
        connect(m_tray, SIGNAL(activated(int)), this, SLOT(trayActivated(int)));
        connect(m_tray, SIGNAL(messageClicked()), this, SLOT(trayShowMainWindow()));

        PopupMenu* trayMenu = new PopupMenu(this);
        EmbeddedMenuBar::addItem(trayMenu, qFromUtf8("打开主窗口"), this, SLOT(trayShowMainWindow()));
        EmbeddedMenuBar::addSeparator(trayMenu);
        EmbeddedMenuBar::addItem(trayMenu, qFromUtf8("退出"), this, SLOT(quitApp()));
        m_tray->setContextMenu(trayMenu);

        connect(contactListWidget, SIGNAL(unreadCountChanged(int)),
                this, SLOT(updateTrayBadge(int)));
        connect(contactListWidget, SIGNAL(globalSearchRequested()),
                this, SLOT(openCombineSearch()));
        if (0) {
            int n = (int)Storage::instance().channelDb()->totalUnread();
            contactListWidget->seedTotalUnread(n);
            updateTrayBadge(n);
        }

        m_tray->show();
    }

    // ── Screenshot 截图模块 ──
    {
        m_screenshotMgr = ScreenshotManager::instance();
        m_screenshotMgr->setHideWindow(this);

        // Config: 截图前隐藏主窗口
        QString hideVal = Config::value("screenshot_hide_window");
        m_screenshotMgr->setAutoHide(hideVal.isEmpty() || hideVal == "true");

        // 「工具」菜单加截图项
        // 快捷键 Ctrl+Shift+S 通过菜单文本中的 \t 自动注册
        EmbeddedMenuBar::addItem(tool, qFromUtf8("截图(&S)...\tCtrl+Shift+S"),
            this, SLOT(onScreenshotRequested()));

        // 截图完成 → 弹出预览
        connect(m_screenshotMgr, SIGNAL(screenshotReady(const QString&)),
                this, SLOT(onScreenshotReady(const QString&)));

        // 截图取消 → 静默忽略
        connect(m_screenshotMgr, SIGNAL(cancelled()),
                this, SLOT(onScreenshotCancelled()));
    }
}

MainWindow::~MainWindow() {
    s_trayIcon = 0;
    ToxAPI::stopPollEvent();
    EventPoller::stop();
    Storage::instance().close();
}

// 托盘气泡通知：仅显示提示气泡，不触碰 SharedStatusBar（状态栏各管各的）。
void sticonShowStatusMessage(const QString &msg, SticonIcon iconType, int timeout)
{
    if (s_trayIcon && s_trayIcon->isVisible() && SystemTrayIcon::supportsMessages())
        s_trayIcon->showMessage(msg, QString(),
            (SystemTrayIcon::MessageIcon)iconType, timeout);
}

// 环形缓冲裁剪/prepend 会移动索引：以 mediaUrl 身份定位真实目标。
// 命中且 url 一致走原索引；越界或 url 不符则按 url 扫描；url 为空回退 msgIndex。
static ChatElement* resolveMediaElement(ChatHistory* target, const std::string& mxcUrl,
                                        int msgIndex, int* outIndex) {
    if (outIndex) { *outIndex = -1; }
    if (!target) { return nullptr; }
    if (msgIndex >= 0 && msgIndex < target->size()
        && ((*target)[msgIndex].mediaUrl == qFromUtf8(mxcUrl) || mxcUrl.empty())) {
        if (outIndex) { *outIndex = msgIndex; }
        return &(*target)[msgIndex];
    }
    if (mxcUrl.empty()) { return nullptr; }
    QString want = qFromUtf8(mxcUrl);
    for (int i = 0; i < target->size(); ++i) {
        if ((*target)[i].mediaUrl == want) {
            if (outIndex) { *outIndex = i; }
            return &(*target)[i];
        }
    }
    return nullptr;
}

void MainWindow::customEvent(CustomEventBase* event) {
    TimePoint _t0 = timeNow();
    // 事件轮询结果
    if (event->type() == EventListReadyType) {
        EventListEvent* e = static_cast<EventListEvent*>(event);
        handleEvents(e->events);
        return;
    }

    // 媒体下载完成
    if (event->type() == MediaDownloadReadyType) {
        MediaDownloadEvent* e = static_cast<MediaDownloadEvent*>(event);
        if (e->postprocDone) { handleMediaPostproc(e); return; }

        // 不依赖当前激活聊天：字节缓存、DB、状态都落到目标聊天的 buffer 元素上，
        // 仅在目标聊天正是当前聊天时才刷新视图。
        ChatHistory* target = m_chatbuf.ptr(e->chatId, e->chatType);
        int realIdx = -1;
        ChatElement* elp = resolveMediaElement(target, e->mxcUrl, e->msgIndex, &realIdx);
        bool isCurrent = (currentChatId == e->chatId
                          && currentChatType == qFromUtf8(e->chatType));

        if (e->success) {
            if (elp) {
                bool isImgLike = (elp->etype == ChatElement::Image
                                  || elp->etype == ChatElement::Gif);
                bool sizeOk = (elp->fileSize <= 0)
                              || ((int)e->rawData.size() == elp->fileSize);
                QPixmap thumb = decodeRawToThumb((const char*)e->rawData.data(), e->rawData.size(),
                    elp->mediaWidth, elp->mediaHeight, chatWidget->width() * 70 / 100);
                if (sizeOk && (!isImgLike || !thumb.isNull())) {
                    elp->scaledDisplay = thumb;
                    elp->cachedWidth = -1;

                    // Cache raw bytes (JPEG/WebP), not QPixmap
                    MediaShmemCache::inst().putThumb(qFromUtf8(e->mxcUrl), (const char*)e->rawData.data(), e->rawData.size());

                    {
                        std::string key = mediaCacheKey("file", qFromUtf8(e->mxcUrl));
                        const auto& rd = e->rawData;
                        std::vector<uint8_t> data(rd.begin(), rd.end());
                        Storage::instance().cacheDbAsync()->storeMedia(
                            std::move(key), std::move(data), "", 2, nullptr);
                    }

                    elp->downloadState = ChatElement::Completed;
                    elp->downloadProgress = 100;
                    elp->downloadSpeedBps = 0;
                    bool elPendingPlay = elp->pendingPlay;
                    bool isVideo = (elp->etype == ChatElement::Video);
                    bool isGifElem = (elp->etype == ChatElement::Gif);
                    if (isVideo || isGifElem || elPendingPlay) {
                        std::vector<uint8_t> procData(e->rawData.begin(), e->rawData.end());
                        scheduleMediaPlayback(realIdx, elPendingPlay, &procData,
                                              e->chatId, qFromUtf8(e->chatType));
                    }
                } else {
                    qWarning("Media download corrupt: chat=%d/%s idx=%d want=%d got=%d",
                             e->chatId, e->chatType.c_str(), realIdx,
                             elp->fileSize, (int)e->rawData.size());
                    elp->downloadState = ChatElement::Failed;
                    elp->downloadProgress = 0;
                    elp->downloadSpeedBps = 0;
                    elp->mediaUrl = qFromUtf8(e->mxcUrl);
                    elp->mediaErrorMsg = qFromUtf8("数据校验失败（大小不匹配/解码失败）");
                }
                if (isCurrent) {
                    chatWidget->updateElement(realIdx);
                    stbarShowStatusMessage(qFromUtf8("图片下载失败（数据校验失败）"), SticonWarning, 4000);
                    sticonShowStatusMessage(qFromUtf8("图片下载失败（数据校验失败）"), SticonWarning, 4000);
                }
            }
        } else {
            qWarning("Media download failed: chat=%d/%s idx=%d err=%s",
                     e->chatId, e->chatType.c_str(), realIdx, e->errorInfo.c_str());
            if (elp) {
                elp->downloadState = ChatElement::Failed;
                elp->downloadProgress = 0;
                elp->downloadSpeedBps = 0;
                elp->mediaUrl = qFromUtf8(e->mxcUrl);
                elp->mediaErrorMsg = qFromUtf8(e->errorInfo);
                if (isCurrent) {
                    chatWidget->updateElement(realIdx);
                    const QString failMsg = qFromUtf8("图片下载失败：") + qFromUtf8(e->errorInfo);
                    stbarShowStatusMessage(failMsg, SticonWarning, 4000);
                    sticonShowStatusMessage(failMsg, SticonWarning, 4000);
                }
            }
        }
        return;
    }

    // 媒体下载进度（100ms 节流，仅更新 % 并局部重绘，不动布局缓存）
    if (event->type() == MediaDownloadProgressType) {
        MediaDownloadProgressEvent* e = static_cast<MediaDownloadProgressEvent*>(event);
        ChatHistory* target = m_chatbuf.ptr(e->chatId, e->chatType);
        int realIdx = -1;
        ChatElement* elp = resolveMediaElement(target, e->mxcUrl, e->msgIndex, &realIdx);
        if (elp) {
            ChatElement& el = *elp;
            int pct = -1;   // -1=未知长度，UI 显示不确定进度
            if (e->total > 0) {
                long long p = e->received * 100LL / e->total;
                if (p < 0) { p = 0; }
                if (p > 100) { p = 100; }
                pct = (int)p;
                if (pct < el.downloadProgress) { pct = el.downloadProgress; }   // 百分比只增不减
            }
            el.downloadProgress = pct;
            el.downloadSpeedBps = (int)e->speedBps;
            if (currentChatId == e->chatId
                && currentChatType == qFromUtf8(e->chatType)) {
                chatWidget->repaintMessageElement(realIdx);
            }
        }
        return;
    }

    // 头像下载完成
    if (event->type() == AvatarDownloadReadyType) {
        AvatarDownloadEvent* e = static_cast<AvatarDownloadEvent*>(event);
        QString key = qFromUtf8(e->mxcUrl);
        if (e->success) {
            if (!e->pixmap.isNull()) {
                AvatarManager::inst().store(key, e->pixmap, ChatView::kAvatarSize);
                chatWidget->repaintMessages();
            } else {
                qWarning("AvatarManager: success but null pixmap for [%s]",
                         e->mxcUrl.c_str());
            }
        } else {
            qWarning("AvatarManager: download failed for [%s] reason: [%s]",
                     e->mxcUrl.c_str(), e->errorInfo.c_str());
            AvatarManager::inst().removePending(key);
            stbarShowStatusMessage(qFromUtf8("头像下载失败"), SticonWarning, 3000);
            sticonShowStatusMessage(qFromUtf8("头像下载失败"), SticonWarning, 3000);
        }
        return;
    }

    // 磁盘缓存加载完成（双击查看原图）
    if (event->type() == DiskLoadReadyType) {
        DiskLoadEvent* e = static_cast<DiskLoadEvent*>(event);
        if (!e->success) {
            qWarning("DiskLoad: cache miss for msg %d url=%s",
                     e->msgIndex, e->mediaUrl.c_str());
            return;
        }
        QPixmap pix;
        QByteArray origData;
        {
            const auto& rd = e->rawData;
            // Qt3 QByteArray 缺少 (const char*,int) 构造，故用 resize+memcpy 深拷贝。
            // 内存安全性已核对：
            // - resize(n)+全量覆盖：无未初始化字节
            // - 值拷贝独立于事件生命周期，无悬垂风险；QGArray 引用计数自动释放
            // - empty 保护规避 memcpy(dst,nullptr,0) 的标准 UB
            // - (int) 截断仅理论风险（需 >2GB 图片，消息媒体不可能达到）
            origData.resize((int)rd.size());
            if (!rd.empty()) {
                memcpy(origData.data(), rd.data(), rd.size());
            }
            std::string rawStr(rd.begin(), rd.end());
            if (!pix.loadFromData((const uchar*)rawStr.data(), rawStr.size())) {
                if (isWebP(rawStr))
                    pix = decodeWebP(rawStr);
            }
        }
        if (pix.isNull()) { return; }
        PhotoViewer* pv = new PhotoViewer(this, pix, origData,
                                          qFromUtf8(e->mediaUrl.c_str())); // 原始字节+媒体URL带入
        pv->show();
        return;
    }

    // 数据库插入完成 → 回填内存元素的 rowid（翻译缓存写库需要）
    if (event->type() == RowidBackfillReadyType) {
        RowidBackfillEvent* e = static_cast<RowidBackfillEvent*>(event);
        ChatHistory* h = m_chatbuf.ptr(e->chatId, e->chatType);
        if (h) {
            QString evid = qFromUtf8(e->eventId);
            for (int i = 0; i < h->size(); ++i) {
                if ((*h)[i].messageId == evid) {
                    (*h)[i].dbRowid = e->rowid;
                    break;
                }
            }
        }
        return;
    }

    // 数据加载完成
    if (event->type() == ApiResultReadyType) {
        ApiResultEvent* e = static_cast<ApiResultEvent*>(event);
        
        // 增量数据到达（逐步展示）
        if (e->type == ApiLoadPartialData) {
            PartialDataEvent* evt = static_cast<PartialDataEvent*>(event);
            
            if (evt->loadedMask & PartialDataEvent::kSelf) {
                selfInfoWidget->updateInfo(
                    qFromUtf8(evt->selfName),
                    qFromUtf8(evt->selfStatusMsg),
                    qFromUtf8(evt->selfConnStatus),
                    qFromUtf8(evt->selfAddress));
                std::string addr = evt->selfAddress;
                if (addr.length() >= 64) {
                    selfPubkey = addr.substr(0, 64);
                }
            }
            
            if (evt->loadedMask & PartialDataEvent::kContacts) {
                for (const auto& cd : evt->contacts) {
                    // 按 (id, type) 匹配，原地更新已有条目（如好友占位→真实数据）
                    bool updated = false;
                    for (auto& existing : m_accumulatedContactData) {
                        if (existing.id == cd.id && existing.type == cd.type) {
                            existing = cd;
                            updated = true;
                            break;
                        }
                    }
                    if (!updated) {
                        m_accumulatedContactData.push_back(cd);
                    }
                    
                    if (cd.type == "friend") {
                        std::string key = "friend_" + std::to_string(cd.id);
                        auto& entry = getOrCreatePeerEntry(peerInfoMap, key);
                        entry.userName = cd.name;
                        entry.peerNumber = cd.id;
                        entry.publicKey = cd.chatId;
                        entry.iconUrl = cd.iconUrl;
                        {
                            QString mxc = qFromUtf8(cd.iconUrl);
                            if (AvatarManager::inst().requestDownload(mxc)) {
                                ToxAPI::downloadAvatar(cd.iconUrl);
                            }
                        }
                        entry.isSelf = false;
                        if (cd.status == "tcp") {
                            entry.status = 1;
                            entry.statusStr = "tcp";
                        } else if (cd.status == "udp") {
                            entry.status = 2;
                            entry.statusStr = "udp";
                        } else {
                            entry.status = 0;
                            entry.statusStr = "none";
                        }
                        addPeerToDb(key, entry);
                        updateContactDb(cd.type + "_" + std::to_string(cd.id),
                                        cd.name, cd.status, cd.isConnected ? 1 : 0,
                                        cd.iconUrl, cd.chatId, cd.type);
                    }
                }
                ContactList cl;
                for (const auto& cd : m_accumulatedContactData) {
                    Contact* c = new Contact();
                    c->id = cd.id;
                    c->name = qFromUtf8(cd.name);
                    c->type = qFromUtf8(cd.type);
                    c->status = qFromUtf8(cd.status);
                    c->chat_id = qFromUtf8(cd.chatId);
                    c->is_connected = cd.isConnected;
                    cl.append(c);
                }
                // 追加虚拟联系人（始终在列表底部）
                {
                    Contact* c = new Contact();
                    c->id = VIRTUAL_UNKNOWN_ID;
                    c->name = "Unknown";
                    c->type = kUnknownType;
                    c->status = "online";
                    c->chat_id = "";
                    c->is_connected = false;
                    cl.append(c);
                    updateContactDb(std::string(kUnknownType) + "_" + std::to_string(VIRTUAL_UNKNOWN_ID),
                                    "Unknown", "online", -1, "", "", kUnknownType);
                }
                {
                    Contact* c = new Contact();
                    c->id = VIRTUAL_SYSEVENT_ID;
                    c->name = "Sysevent";
                    c->type = kSyseventType;
                    c->status = "online";
                    c->chat_id = "";
                    c->is_connected = false;
                    cl.append(c);
                    updateContactDb(std::string(kSyseventType) + "_" + std::to_string(VIRTUAL_SYSEVENT_ID),
                                    "Sysevent", "online", -1, "", "", kSyseventType);
                }
                // 追加固定虚拟联系人
                {
                    Contact* c = new Contact();
                    c->id = VIRTUAL_REDDIT_ID;
                    c->name = "Reddit";
                    c->type = kTopicType;
                    c->status = "online";
                    c->chat_id = "";
                    c->is_connected = false;
                    cl.append(c);
                    updateContactDb(std::string(kTopicType) + "_" + std::to_string(VIRTUAL_REDDIT_ID),
                                    "Reddit", "online", -1, "", "", kTopicType);
                }
                {
                    Contact* c = new Contact();
                    c->id = VIRTUAL_BOOKMARK_ID;
                    c->name = "Bookmark";
                    c->type = kBookmarkType;
                    c->status = "online";
                    c->chat_id = "";
                    c->is_connected = false;
                    cl.append(c);
                    updateContactDb(std::string(kBookmarkType) + "_" + std::to_string(VIRTUAL_BOOKMARK_ID),
                                    "Bookmark", "online", -1, "", "", kBookmarkType);
                }
                {
                    Contact* c = new Contact();
                    c->id = VIRTUAL_AICHAT_ID;
                    c->name = "AI Chat";
                    c->type = kAichatType;
                    c->status = "online";
                    c->chat_id = "";
                    c->is_connected = false;
                    cl.append(c);
                    updateContactDb(std::string(kAichatType) + "_" + std::to_string(VIRTUAL_AICHAT_ID),
                                    "AI Chat", "online", -1, "", "", kAichatType);
                }
                {
                    Contact* c = new Contact();
                    c->id = VIRTUAL_PASTEBIN_ID;
                    c->name = "Paste Bin";
                    c->type = kPastebinType;
                    c->status = "online";
                    c->chat_id = "";
                    c->is_connected = false;
                    cl.append(c);
                    updateContactDb(std::string(kPastebinType) + "_" + std::to_string(VIRTUAL_PASTEBIN_ID),
                                    "Paste Bin", "online", -1, "", "", kPastebinType);
                }
                {
                    Contact* c = new Contact();
                    c->id = VIRTUAL_TRANSLATE_ID;
                    c->name = "Translate";
                    c->type = kTranslateType;
                    c->status = "online";
                    c->chat_id = "";
                    c->is_connected = false;
                    cl.append(c);
                    updateContactDb(std::string(kTranslateType) + "_" + std::to_string(VIRTUAL_TRANSLATE_ID),
                                    "Translate", "online", -1, "", "", kTranslateType);
                }
                contactListWidget->setContacts(cl);
            }
            return;
        }
        
        if (e->type == ApiLoadAllData) {
            qWarning("MainWindow: initial data load complete");
            chatWidget->loadingBar()->hideLoading(kLoadAll);
            if (ToxAPI::onLoadAllDataComplete()) {
                chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
                ToxAPI::loadAllData();
            }
            return;
        }

        // 懒加载好友详情结果
        if (e->type == ApiLoadFriendDetail) {
            FriendDetailEvent* evt = static_cast<FriendDetailEvent*>(event);
            chatWidget->loadingBar()->hideLoading(kLoadFriend);
            if (evt->success) {
                contactListWidget->updateContact(
                    evt->friendId, "friend",
                    qFromUtf8(evt->name),
                    qFromUtf8(evt->publicKey),
                    qFromUtf8(evt->statusStr));
                std::string key = "friend_" + std::to_string(evt->friendId);
                auto& entry = getOrCreatePeerEntry(peerInfoMap, key);
                entry.userName = evt->name;
                entry.peerNumber = evt->friendId;
                entry.publicKey = evt->publicKey;
                entry.iconUrl = evt->iconUrl;
                {
                    QString mxc = qFromUtf8(evt->iconUrl);
                    if (AvatarManager::inst().requestDownload(mxc)) {
                        ToxAPI::downloadAvatar(evt->iconUrl);
                    }
                }
                entry.statusText = evt->statusText;
                int s = 0;
                if (evt->statusStr == "tcp") { s = 1; }
                else if (evt->statusStr == "udp") s = 2;
                entry.status = s;
                entry.statusStr = evt->statusStr;
                entry.peerIp = evt->peerIp;
                entry.userStatus = evt->userStatus;
                entry.lastSeen = evt->lastSeen;
                addPeerToDb(key, entry);
            }
            return;
        }
        
        // 自身信息更新结果
        if (e->type == ApiGetSelf) {
            SelfInfoResultEvent* evt = static_cast<SelfInfoResultEvent*>(event);
            if (evt->success) {
                selfInfoWidget->updateInfo(
                    qFromUtf8(evt->name),
                    qFromUtf8(evt->statusMsg),
                    qFromUtf8(evt->connStatus),
                    qFromUtf8(evt->address));
                if (evt->address.length() >= 64) {
                    selfPubkey = evt->address.substr(0, 64);
                }
            }
            return;
        }
        
        // 消息发送结果
        if (e->type == ApiSendFriendMessage || e->type == ApiSendConferenceMessage || e->type == ApiSendGroupMessage || e->type == ApiSendMessage) {
            MessageSentResultEvent* evt = static_cast<MessageSentResultEvent*>(event);
            chatWidget->loadingBar()->hideLoading(kLoadSendMsg);
            QString targetName;
            for (const auto& cd : m_accumulatedContactData) {
                if (cd.id == evt->chatId && cd.type == evt->chatType) {
                    targetName = qFromUtf8(cd.name);
                    break;
                }
            }
            if (targetName.isEmpty())
                targetName = qFromUtf8(evt->chatType) + " " + QString::number(evt->chatId);
            {
                bool found = false;
                for (int i = chatWidget->messageCount() - 1; i >= 0; i--) {
                    ChatElement& el = chatWidget->mutableMessageAt(i);
                    if (el.category == "self" && el.sendmsgseq == evt->sendmsgseq) {
                    if (evt->success) {
                        el.sendState = ChatElement::SendSent;
                        el.messageId = qFromUtf8(evt->messageId);
                    } else {
                            el.sendState = ChatElement::SendFailed;
                            el.sendErrorMsg = qFromUtf8(evt->errorMessage);
                        }
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    for (int i = chatWidget->messageCount() - 1; i >= 0; i--) {
                        ChatElement& el = chatWidget->mutableMessageAt(i);
                        if (el.category == "self" && el.sendState == ChatElement::SendSending) {
                            if (evt->success) {
                                el.sendState = ChatElement::SendSent;
                            } else {
                                el.sendState = ChatElement::SendFailed;
                                el.sendErrorMsg = qFromUtf8(evt->errorMessage);
                            }
                            break;
                        }
                    }
                }
            }
            if (!evt->success) {
                ToastWidget::show(chatWidget, _("send_failed").arg(targetName)
                .arg(formatElapsedMs(evt->elapsedMs)).arg(qFromUtf8(evt->errorMessage)), 8000);
                const QString failText = _("send_failed").arg(targetName)
                .arg(formatElapsedMs(evt->elapsedMs))
                .arg(qFromUtf8(evt->errorMessage));
                stbarShowStatusMessage(failText, SticonCritical, 5000);
                sticonShowStatusMessage(failText, SticonCritical, 5000);
                m_lyrics->setPlayedColor(QColor(0xFF,0x44,0x44));
                m_lyrics->setLrcText(qFromUtf8("[00:00.000]发送失败"));
                m_lyrics->setPosition(0);   // setLrcText 只装载不定位，需显式定位首行才渲染
                m_hintActive = true;
#ifdef QT3_BUILD
                m_msgTimer->start(5000, true);
#else
                m_msgTimer->start(5000);
#endif
            }
            else {
                ToastWidget::show(chatWidget, _("send_success").arg(targetName).arg(formatElapsedMs(evt->elapsedMs)), 2000);
                const QString okText = _("send_success").arg(targetName)
                .arg(formatElapsedMs(evt->elapsedMs));
                stbarShowStatusMessage(okText, SticonInfo, 2000);
                sticonShowStatusMessage(okText, SticonInfo, 2000);
                m_lyrics->setPlayedColor(QColor(0x00,0xB4,0xD8));
                m_lyrics->setLrcText(qFromUtf8("[00:00.000]已发送"));
                m_lyrics->setPosition(0);   // setLrcText 只装载不定位，需显式定位首行才渲染
                m_hintActive = true;
#ifdef QT3_BUILD
                m_msgTimer->start(5000, true);
#else
                m_msgTimer->start(5000);
#endif
            }
            chatWidget->repaintMessages();
            return;
        }
        
        // 消息撤回结果
        if (e->type == ApiRedactMessage) {
            RedactResultEvent* evt = static_cast<RedactResultEvent*>(event);
            chatWidget->loadingBar()->hideLoading(kLoadRedactMsg);
            if (evt->success) {
                int64_t dbRowid = 0;
                for (int i = chatWidget->messageCount() - 1; i >= 0; i--) {
                    ChatElement& el = chatWidget->mutableMessageAt(i);
                    if (el.messageId != qFromUtf8(evt->messageId)) { continue; }
                    el.redacted = true;
                    el.caption = QString();
                    el.mediaUrl = QString();
                    el.fileName = QString();
                    el.showTranslation = false;
                    if (el.dbRowid > 0 && dbRowid == 0) { dbRowid = el.dbRowid; }
                }
                chatWidget->relayout();
                chatWidget->repaintMessages();
                if (dbRowid > 0) {
                    qDebug("redact: 更新 DB rowid=%lld messageId=%s chatId=%d chatType=%s",
                           (long long)dbRowid, evt->messageId.c_str(), evt->chatId,
                           qFromUtf8(evt->chatType).data());
                    MessageUpdate upd;
                    upd.hasRedacted = true;
                    upd.redacted = 1;
                    Storage::instance().messageDbAsync()->update_message(dbRowid, upd, [](bool){});
                } else {
                    qWarning("redact: 无已回填 rowid，跳过 DB 更新 msgId=%s chatId=%d chatType=%s",
                             evt->messageId.c_str(), evt->chatId,
                             qFromUtf8(evt->chatType).data());
                }
                ToastWidget::show(chatWidget, _("redact_success").arg(formatElapsedMs(evt->elapsedMs)), 2500);
            } else {
                ToastWidget::show(chatWidget, _("redact_failed").arg(qFromUtf8(evt->errorMessage)), 4000);
            }
            return;
        }
        
        // 会议加入结果 → 重新加载联系人
        if (e->type == ApiJoinConference) {
            chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
            ToxAPI::loadAllData();
            return;
        }
        
        // 成员列表加载完成
        if (e->type == ApiLoadGroupMembers) {
            MembersLoadedEvent* evt = static_cast<MembersLoadedEvent*>(event);
            chatWidget->loadingBar()->hideLoading(kLoadMembers);
            if (evt->contactId == currentChatId && evt->contactType == std::string(qToUtf8(currentChatType).data())) {
                qWarning("MembersLoaded: %d members for %s %d", (int)evt->members.size(), evt->contactType.c_str(), evt->contactId);
                for (const auto& m : evt->members) {
                    std::string key = evt->contactType + "_" + std::to_string(evt->contactId) + "_" + std::to_string(m.peerNumber);
                    addPeerToDb(key, m);
                    // 没有 nickname 和 avatar 的数据没必要缓存
                    if (m.nickname.empty() && m.iconUrl.empty()) {
                        continue;
                    }
                    mergePeerInfo(getOrCreatePeerEntry(peerInfoMap, key), m);
                }
            }
            return;
        }
        
        // 历史消息加载完成
        if (e->type == ApiLoadMessageHistory) {
            MessageHistoryLoadedEvent* evt = static_cast<MessageHistoryLoadedEvent*>(event);
            chatWidget->loadingBar()->hideLoading(kLoadMessages);
            if (!evt->success) {
                ToastWidget::show(chatWidget, _("load_messages_failed").arg(formatElapsedMs(evt->elapsedMs)), 8000);
                return;
            }
            if (evt->messages.empty()) {
                ToastWidget::show(chatWidget, _("load_messages_empty").arg(formatElapsedMs(evt->elapsedMs)), 2000);
                return;
            }
            if (evt->contactId == currentChatId && evt->contactType == std::string(qToUtf8(currentChatType).data())) {
                qWarning("MessageHistoryLoaded: %d messages for %s %d", (int)evt->messages.size(), evt->contactType.c_str(), evt->contactId);
                renderHistoryMessages(evt->messages);
            }
            return;
        }
        
        // 翻译结果
        if (e->type == ApiTranslate) {
            TranslateResultEvent* tev = static_cast<TranslateResultEvent*>(event);
            if (tev->chatId != currentChatId
                || tev->chatType != std::string(qToUtf8(currentChatType).data())) {
                return;
            }
            chatWidget->onTranslateResult(tev->msgIndex, tev->success,
                qFromUtf8(tev->translatedText.data(), (int)tev->translatedText.size()),
                qFromUtf8(tev->errorMessage.data(), (int)tev->errorMessage.size()));
            if (!tev->success) {
                const QString trFail = qFromUtf8("翻译失败：") + qFromUtf8(tev->errorMessage);
                stbarShowStatusMessage(trFail, SticonWarning, 8000);
                sticonShowStatusMessage(trFail, SticonWarning, 8000);
            }
            return;
        }

        // Send EN 翻译结果
        if (e->type == ApiTranslateForSend) {
            TranslateForSendResultEvent* tev = static_cast<TranslateForSendResultEvent*>(event);
            if (tev->success) {
                chatWidget->loadingBar()->showLoading(kLoadSendMsg,
                    _("sending_message"));
                std::string type = std::string(qToUtf8(currentChatType).data());
                std::string idOverride;
                if (type == kGomuksRoomType || type == kMtxliteRoomType
        || type == kUnktoxConferenceType
                    || type == kUnktoxFriendType || type == kUnktoxGroupType
                    || type == kMisskeyType || type == kImapMailType) {
                    for (const auto& cd : m_accumulatedContactData) {
                        if (cd.id == currentChatId && cd.type == type) {
                            idOverride = cd.chatId;
                            break;
                        }
                    }
                }
                int sendmsgseq = ToxAPI::sendMessage(currentChatId, type, tev->translatedText, idOverride);
                (void)sendmsgseq;
            } else {
                chatWidget->loadingBar()->hideLoading(kLoadSendMsg);
                ToastWidget::show(chatWidget, "翻译失败", 8000);
                stbarShowStatusMessage(qFromUtf8("翻译失败"), SticonWarning, 8000);
                sticonShowStatusMessage(qFromUtf8("翻译失败"), SticonWarning, 8000);
            }
            return;
        }
    }
    long long _el = elapsedMs(_t0); if (_el >= 200) qWarning("SLOW [hangui] customEvent took %lldms", _el);
}

bool MainWindow::event(QEvent* event) {
    bool ret = QMainWindow::event(event);
#ifdef QT3_BUILD
    bool windowActivated = (event->type() == QEvent::WindowActivate);
#else
    bool windowActivated = (event->type() == QEvent::ActivationChange);
#endif
    if (m_tray && windowActivated && qApp->activeWindow() == this) {
        m_tray->clearMessageQueue();   // 主窗激活时清空待显示气泡
    }
    if (!m_firstPaintLogged && event->type() == QEvent::Paint) {
        m_paintCounter++;		
	}
    if (!m_firstPaintLogged) {
		if (event->type() == QEvent::Paint) {
			if (!qApp->hasPendingEvents() || m_paintCounter > 5) {
				m_firstPaintLogged = true;
				QTimer::singleShot(0, this, SLOT(onFirstPaintComplete()));
			}
		} else {
			if (!qApp->hasPendingEvents() && m_paintCounter > 0) {
				m_firstPaintLogged = true;
				QTimer::singleShot(0, this, SLOT(onFirstPaintComplete()));
			}			
		}
    }
    return ret;
}

// 关闭拦截：托盘可用时点 X / Alt+F4 不退出，改为隐藏到托盘（最小化到托盘）。
// 真正退出必须走 quitApp()（先置 m_forceQuit 再 close）；
// 托盘不可见（无托盘环境）时才允许直接关闭，避免窗口藏起来后应用不可达。
void MainWindow::closeEvent(QCloseEvent* event) {
    if (!m_forceQuit && m_tray && m_tray->isVisible()) {
        hide();
        event->ignore();
        return;
    }
    event->accept();
}

// 真正退出应用：「文件→退出」菜单与托盘右键菜单的共同入口。
// 先置 m_forceQuit 绕过 closeEvent 的托盘拦截，再走正常 close 流程
// （触发窗口关闭、子控件析构等清理逻辑）。
void MainWindow::quitApp() {
    m_forceQuit = true;
    close();
}

// 从托盘恢复主窗口：托盘左键/双击、托盘菜单「打开主窗口」共用。
// show + raise + qActivateWindow 三步，保证被遮挡或最小化状态下都能置前。
void MainWindow::trayShowMainWindow() {
    show();
    raise();
    qActivateWindow(this);
}

// 托盘图标点击响应。reason 为 SystemTrayIcon::ActivationReason：
// Trigger(左键)/DoubleClick(双击) → 恢复主窗口；
// Context(右键) 由 SystemTrayIcon 内部弹上下文菜单，无需处理；
// MiddleClick(中键) 暂无动作。
void MainWindow::trayActivated(int reason) {
    if (reason == SystemTrayIcon::Trigger || reason == SystemTrayIcon::DoubleClick) {
        trayShowMainWindow();
    }
}

// 协议订阅流类型 → 头部 emoji（与 contactlist.cpp typeToEmojiCp 保持一致）。
// 用 qFromUtf8 包装，避免 QString(const char*) 的 Latin-1 乱码（AGENTS.md 规范）。
static QString protoStreamEmoji(const QString& type) {
    if (type == kToutiaoHotnewsType)       return qFromUtf8("📰");
    if (type == kZhihuNotifyType
        || type == kZhihuHotnewsType)      return qFromUtf8("📘");
    if (type == kBiliNotifyType)           return qFromUtf8("📺");
    if (type == kWeiboHotnewsType)         return qFromUtf8("🔥");
    if (type == kXiaohongshuRecommendType) return qFromUtf8("📕");
    if (type == kXiaohongshuNotifyType)    return qFromUtf8("🔔");
    if (type == kXiaohongshuHotnewsType)   return qFromUtf8("📈");
    if (type == kCoolapkTimelineType)      return qFromUtf8("📱");
    if (type == kMisskeyType)              return qFromUtf8("🐱");
    return QString();
}

void MainWindow::updateTrayBadge(int total) {
    if (m_tray && m_tray->isVisible()) {
        m_tray->setBadgeCount(total);
    }
}

void MainWindow::onFirstPaintComplete() {
    QString status = qApp->hasPendingEvents() ? QString::fromUtf8("true") : QString::fromUtf8("false");
    QString msg = QString::fromUtf8("=== UI 首次绘制完成 === pending=") + status;
#ifdef QT3_BUILD
    qWarning("%s", msg.utf8().data());
#else
    qWarning("%s", msg.toUtf8().constData());
#endif

    initLoadPlugins();
}

void MainWindow::initLoadPlugins() {
    PluginLoader::scanPlugins();
#ifdef QT3_BUILD
    for (int i = 0; i < PluginLoader::uiappCount(); i++) {
        const PluginInfo& p = PluginLoader::uiappAt(i);
        if (!p.enabled) continue;
        int id = m_etappsMenu->insertItem(p.name);
        m_etappItemToIndex[id] = i;
    }
    connect(m_etappsMenu, SIGNAL(activated(int)), this, SLOT(onEtappActivated(int)));
#else
    {
        QSignalMapper* etappMapper = new QSignalMapper(this);
        for (int i = 0; i < PluginLoader::uiappCount(); i++) {
            const PluginInfo& p = PluginLoader::uiappAt(i);
            if (!p.enabled) continue;
            QAction* action = m_etappsMenu->addAction(p.name);
            etappMapper->setMapping(action, i);
            connect(action, SIGNAL(triggered()), etappMapper, SLOT(map()));
        }
        connect(etappMapper, SIGNAL(mapped(int)), this, SLOT(onEtappActivated(int)));
    }
#endif
}

void MainWindow::onContactSelected(int id, const QString& type, const QString& name) {
    chatWidget->resetPendingContext();
    TimePoint _t0 = timeNow();
    // 如果已经是当前选中的聊天对象，不重新加载（避免清空消息）
    if (id == currentChatId && type == currentChatType) {
        qWarning("onContactSelected: same contact, ignoring");
        return;
    }
    
    // 清理当前聊天的 scaledDisplay（即将离开的）
    if (currentChatId >= 0) {
        m_chatbuf.clearDisplayCacheFor(currentChatId,
            std::string(qToUtf8(currentChatType).data()));
    }
    
    // 好友未加载详情 → 懒加载
    if (type == "friend" && !contactListWidget->isFriendLoaded(id)) {
        qWarning("onContactSelected: friend %d not loaded, lazy loading", id);
        currentChatId = -1; // reset so next click won't be ignored
        chatWidget->loadingBar()->showLoading(kLoadFriend, _("loading_contact"));
        ToxAPI::lazyLoadFriendDetail(id);
        return;
    }
    
    qWarning("onContactSelected: id=%d, type=%s", id, qToUtf8(type).data());
    currentChatId = id;
    currentChatType = type;
    chatWidget->setAttrForChat(type, id);
    int prevUnread = contactListWidget->unreadCount(id, type);
    contactListWidget->resetUnread(id, type);
    
    QString headerText;
    QString emoji;
    if (type == "friend") {
        emoji = EMOJI_FRIEND;
        headerText = emoji + " " + name;
    } else if (type == "group") {
        emoji = EMOJI_GROUP;
        headerText = emoji + " " + name;
    } else if (type == "conference") {
        emoji = EMOJI_CONFERENCE;
        headerText = emoji + " " + name;
    } else if (type == kUnknownType) {
        emoji = EMOJI_UNKNOWN;
        headerText = emoji + " " + name;
    } else if (type == kSyseventType) {
        emoji = EMOJI_SYSEVENT;
        headerText = emoji + " " + name;
    } else if (type == kTopicType) {
        emoji = EMOJI_TOPIC;
        headerText = emoji + " " + name;
    } else if (type == kBookmarkType) {
        emoji = EMOJI_BOOKMARK;
        headerText = emoji + " " + name;
    } else if (type == kAichatType) {
        emoji = EMOJI_AICHAT;
        headerText = emoji + " " + name;
    } else if (type == kPastebinType) {
        emoji = EMOJI_PASTEBIN;
        headerText = emoji + " " + name;
    } else if (type == kTranslateType) {
        emoji = EMOJI_TRANSLATE;
        headerText = emoji + " " + name;
    } else if (type == kGomuksRoomType) {
        emoji = EMOJI_MATRIX;
        headerText = emoji + " " + name;
    } else if (type == kMtxliteRoomType) {
        emoji = EMOJI_MTXLITE;
        headerText = emoji + " " + name;
    } else if (type == kUnktoxFriendType) {
        emoji = EMOJI_FRIEND;
        headerText = emoji + " " + name;
    } else if (type == kUnktoxConferenceType) {
        emoji = EMOJI_CONFERENCE;
        headerText = emoji + " " + name;
    } else if (type == kUnktoxGroupType) {
        emoji = EMOJI_GROUP;
        headerText = emoji + " " + name;
    } else if (type == kImapMailType) {
        emoji = "E";
        headerText = emoji + " " + name;
    } else if (!protoStreamEmoji(type).isEmpty()) {
        headerText = protoStreamEmoji(type) + " " + name;
    }
    
    chatWidget->setHeaderText(headerText);
    if (prevUnread > 0)
        chatWidget->showUnreadBanner(prevUnread);
    
    std::string typeStr = std::string(qToUtf8(type).data());
    ChatHistory& hist = m_chatbuf.getOrCreate(id, typeStr);
    chatWidget->setBuffer(&hist);

    if (!hist.loadedLatest50FromDB) {
        auto* db = Storage::instance().messageDb();
        assertf(db, "Storage::messageDb() returned null, chanid=%s_%d",
                typeStr.c_str(), id);
        std::string chanid = typeStr + "_" + std::to_string(id);
        std::vector<MessageRow> rows = db->load_messages(chanid.c_str(), 50);
        if (!rows.empty()) {
            std::vector<ChatElement> els;
            els.reserve(rows.size());
            for (auto& row : rows) {
                ChatElement el = msgRowToElement(row);
                // 已恢复动画源/本地文件的消息不显示下载覆盖（内容仍在缓存盘/DB）
                if (el.etype == ChatElement::Video && !el.gifPath.isEmpty()) {
                    el.downloadState = ChatElement::Completed;
                    el.downloadProgress = 100;
                } else if ((el.etype == ChatElement::Video || el.etype == ChatElement::Audio)
                           && !el.mediaUrl.isEmpty()) {
                    std::string fkey = mediaCacheKey("file", el.mediaUrl);
                    std::string fpath = Storage::instance().dataDir() + "/"
                                      + makeCacheFsPath(fkey.c_str());
                    if (::access(fpath.c_str(), F_OK) == 0) {
                        el.downloadState = ChatElement::Completed;
                        el.downloadProgress = 100;
                    }
                }
                els.push_back(el);
            }
            m_chatbuf.prepend(id, typeStr, els);
            chatWidget->relayout();
        }
        hist.loadedLatest50FromDB = true;
    }

    // ── 收藏集内存加载：聊天切换时一次性读全量收藏，右键菜单零查库 ──
    if (id >= 0) {
        auto* db = Storage::instance().messageDb();
        if (db) {
            std::string chanid = typeStr + "_" + std::to_string(id);
            std::vector<int64_t> favs =
                db->list_bookmark_rowids(chanid.c_str());
            chatWidget->setFavRowids(favs);
        }
    }

    // 写入读游标：取最新消息的 rowid
    if (hist.newestRowid > 0) {
        db_writeMarkRead(id, type, hist.newestRowid);
    }

    // ── HTTP fallback ──
    // TEMP: 暂注释，先验证 DB-on-load 路径。
    //
    // if (!hist.loadedLastest50FromNet && hist.empty() && id >= 0
    //     && type != kGomuksRoomType && type != kUnktoxFriendType
    //     && type != kUnktoxConferenceType && type != kUnktoxGroupType
    //     && type != kImapMailType && type != kFilesyncType
    //     && type != kClipboardType && type != kBookmarkType
    //     && type != kAichatType && type != kPastebinType
    //     && type != kTranslateType) {
    //     hist.loadedLastest50FromNet = true;
    //     chatWidget->loadingBar()->showLoading(kLoadMessages, _("loading_messages"));
    //     ToxAPI::getMessagesHistory(id, typeStr);
    // }
    
    // 异步预加载成员列表到 peerInfoMap 缓存（虚拟联系人跳过）
    if (id < 0) return;
    if (type == "group") {
        chatWidget->loadingBar()->showLoading(kLoadMembers, _("loading_members"));
        ToxAPI::getGroupMembers(id);
    } else if (type == "conference") {
        chatWidget->loadingBar()->showLoading(kLoadMembers, _("loading_members"));
        ToxAPI::getConferenceMembers(id);
    }
    long long _el = elapsedMs(_t0); if (_el >= 200) qWarning("SLOW [hangui] onContactSelected(%d,%s) took %lldms", id, qToUtf8(type).data(), _el);
}

// ── 虚拟联系人消息截留 stub ──
static void handleBookmarkMessage(const QString&) {}
static void handleAichatMessage(const QString&) {}
static void handlePastebinMessage(const QString&) {}
static void handleTranslateMessage(const QString&) {}

void MainWindow::onMessageSending(const QString& message, const QMap<QString,QString>& context) {
    // ── 统一发送 API ──
    // 注释掉下面这行可切回旧的三条独立端点
#define USE_UNIFIED_SEND_API
    qWarning("onMessageSending: id=%d type=%s msg=[%.8192s]",
             currentChatId, qToUtf8(currentChatType).data(), qToUtf8(message).data());
    if (currentChatId == -1 || currentChatType.isEmpty()) {
        QMessageBox::warning(this, _("select_chat_first"), _("select_chat_first"));
        return;
    }

    int sendmsgseq = 0;
#ifdef USE_UNIFIED_SEND_API
    std::string type = std::string(qToUtf8(currentChatType).data());
    if (type.empty()) {
        qWarning("Empty chat type");
        return;
    }

    // 虚拟类型使用 chatId 字符串（如 gomuks room ID）而非 numeric contactId
    std::string idOverride;
    if (type == kGomuksRoomType || type == kMtxliteRoomType
        || type == kUnktoxConferenceType
        || type == kUnktoxFriendType || type == kUnktoxGroupType
        || type == kMisskeyType || type == kImapMailType) {
        for (const auto& cd : m_accumulatedContactData) {
            if (cd.id == currentChatId && cd.type == type) {
                idOverride = cd.chatId;
                break;
            }
        }
    }

    if (type == kBookmarkType) {
        handleBookmarkMessage(message);
    } else if (type == kAichatType) {
        handleAichatMessage(message);
    } else if (type == kPastebinType) {
        handlePastebinMessage(message);
    } else if (type == kTranslateType) {
        handleTranslateMessage(message);
    } else {
        chatWidget->loadingBar()->showLoading(kLoadSendMsg, _("sending_message"));
        m_lyrics->setPlayedColor(QColor(0xBB,0xBB,0xBB));
        m_lyrics->setLrcText(qFromUtf8("[00:00.000]发送中..."));
        m_lyrics->setPosition(0);   // setLrcText 只装载不定位，需显式定位首行才渲染
        m_hintActive = true;
#ifdef QT3_BUILD
        m_msgTimer->start(5000, true);
#else
        m_msgTimer->start(5000);
#endif
        sendmsgseq = ToxAPI::sendMessage(currentChatId, type, std::string(qToUtf8(message)), idOverride, "", "", context);
    }
#else
    // ✅ 改为异步请求
    ApiRequestType reqType;
    if (currentChatType == "friend") {
        reqType = ApiSendFriendMessage;
    } else if (currentChatType == "group") {
        reqType = ApiSendGroupMessage;
    } else if (currentChatType == "conference") {
        reqType = ApiSendConferenceMessage;
    } else {
        qWarning("Unknown chat type: %s", qToUtf8(currentChatType).data());
        return;
    }

    chatWidget->loadingBar()->showLoading(kLoadSendMsg, _("sending_message"));
    if (reqType == ApiSendFriendMessage) {
        sendmsgseq = ToxAPI::sendFriendMessage(currentChatId, std::string(qToUtf8(message)));
    } else if (reqType == ApiSendConferenceMessage) {
        sendmsgseq = ToxAPI::sendConferenceMessage(currentChatId, std::string(qToUtf8(message)));
    } else if (reqType == ApiSendGroupMessage) {
        sendmsgseq = ToxAPI::sendGroupMessage(currentChatId, std::string(qToUtf8(message)));
    }
#endif

    // 乐观更新：先写入 buffer，再显示在界面
    {
        std::string typeStr = std::string(qToUtf8(currentChatType).data());
        ChatElement el;
        el.messageText = message;
        el.category = "self";
        el.senderName = "Me";
        el.peerNumber = -1;
        el.time = getCurrentTime();
        m_chatbuf.append(currentChatId, typeStr, el);
        db_writeMessage(currentChatId, typeStr, el);
        ChatHistory& hist = m_chatbuf.getOrCreate(currentChatId, typeStr);
        ChatElement& backEl = hist.back();
        backEl.sendState = ChatElement::SendSending;
        backEl.sendmsgseq = sendmsgseq;
        if (type == kBookmarkType || type == kAichatType
            || type == kPastebinType || type == kTranslateType) {
            backEl.sendState = ChatElement::SendSent;
        }
    }
    contactListWidget->updateContactLastMessage(currentChatId, currentChatType, message, timenowhm());
    db_writeLastMessage(currentChatId, currentChatType, message, timenowhm());
}

void MainWindow::handleEvents(const EventList& events) {
    TimePoint _t0 = timeNow();
    contactListWidget->beginBatch();
    for (size_t i = 0; i < events.size(); ++i) {
        const Event& e = events[i];
        qWarning("Event: %s", e.type.c_str());
        
        if (e.type == "friend_message") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* friendIdItem = cJSON_GetObjectItem(root, "friend_id");
                cJSON* messageItem = cJSON_GetObjectItem(root, "message");
                if (friendIdItem && messageItem) {
                    int friendId = friendIdItem->valueint;
                    QString message = qFromUtf8(cJSON_GetStringValue(messageItem));
                    ChatElement el;
                    el.messageText = message;
                    el.category = "other";
                    el.peerNumber = friendId;
                    el.time = getCurrentTime();
                    {
                        auto fit = peerInfoMap.find("friend_" + std::to_string(friendId));
                    }
                    m_chatbuf.append(friendId, "friend", el);
                    QString friendTimeStr = timenowhm();
                    bool friendNotCurrent = !(friendId == currentChatId && currentChatType == "friend");
                    db_writeMessage(friendId, "friend", el,
                        [friendId, message, friendTimeStr, friendNotCurrent](int64_t rowid) {
                            db_writeLastMessage(friendId, "friend", message, friendTimeStr, rowid);
                            if (friendNotCurrent) {
                                db_writeUnreadIncrement(friendId, "friend", rowid);
                            }
                        });
                    if (friendNotCurrent) {
                        contactListWidget->incrementUnread(friendId, "friend");
                    }
                    contactListWidget->updateContactLastMessage(friendId, "friend", message, timenowhm());
                    if (!qIsAppActive())
                        playNotificationSound();
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "conference_invite") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* friendNumberItem = cJSON_GetObjectItem(root, "friend_number");
                cJSON* cookieItem = cJSON_GetObjectItem(root, "cookie");
                if (friendNumberItem && cookieItem) {
                    QString friendNumber = QString::number(friendNumberItem->valueint);
                    QString cookie = qFromUtf8(cJSON_GetStringValue(cookieItem));
                    
                     ConferenceInviteDialog dialog(friendNumber, cookie, this);
                      dialog.exec();
                      
                      if (dialog.getResult() == ConferenceInviteDialog::Accept) {
                         ToxAPI::joinConference(friendNumber.toInt(), std::string(qToUtf8(cookie)));
                         
                         QMessageBox::information(this, _("conference_joined"), 
                                                         _("conference_joined").arg(dialog.getCookie()));
                     } else if (dialog.getResult() == ConferenceInviteDialog::Reject) {
                         ToxAPI::rejectConference(friendNumber.toInt());
                     }
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "conference_message") {
            qWarning("Processing conference_message event");
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* confNumberItem = cJSON_GetObjectItem(root, "conference_number");
                cJSON* messageItem = cJSON_GetObjectItem(root, "message");
                cJSON* peerNumberItem = cJSON_GetObjectItem(root, "peer_number");
                cJSON* peerNameItem = cJSON_GetObjectItem(root, "peer_name");
                
                if (confNumberItem && messageItem) {
                    int confNumber = confNumberItem->valueint;
                    QString message = qFromUtf8(cJSON_GetStringValue(messageItem));
                    int peerNumber = peerNumberItem ? peerNumberItem->valueint : -1;
                    
                    std::string key = "conference_" + std::to_string(confNumber) + "_" + std::to_string(peerNumber);
                    auto it = peerInfoMap.find(key);

                    if (it == peerInfoMap.end()) {
                        qWarning("conference_message: peer record load, key=%s", key.c_str());
                        PeerKey pk = parsePeerKey(key);
                        if (pk.valid) {
                            auto row = Storage::instance().channelDb()->get_chan_peer(
                                pk.chanid.c_str(), pk.peerNum);
                            it = loadRowToMap(peerInfoMap, key, std::move(row));
                        }
                        // 不创建空条目
                    } else {
                        // 第二层：key 已存在，逐个检查缺失字段
                        bool needsNick = it->second.nickname.empty();
                        bool needsAvatar = it->second.iconUrl.empty();
                        if (needsNick || needsAvatar) {
                            qWarning("conference_message: peer field fill, key=%s nick=%s avatar=%s",
                                     key.c_str(), needsNick?"miss":"hit", needsAvatar?"miss":"hit");
                            PeerKey pk = parsePeerKey(key);
                            if (pk.valid) {
                                auto row = Storage::instance().channelDb()->get_chan_peer(
                                    pk.chanid.c_str(), pk.peerNum);
                                if (row) {
                                    if (needsNick && !row->nickname.empty()) {
                                        it->second.nickname = row->nickname;
                                    }
                                    if (needsAvatar && !row->avatar_url.empty()) {
                                        it->second.iconUrl = row->avatar_url;
                                        QString mxc = qFromUtf8(it->second.iconUrl);
                                        if (AvatarManager::inst().requestDownload(mxc)) {
                                            ToxAPI::downloadAvatar(it->second.iconUrl);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (it != peerInfoMap.end()) {
                        if (peerNameItem && cJSON_IsString(peerNameItem)) {
                            it->second.userName = std::string(cJSON_GetStringValue(peerNameItem));
                            it->second.peerNumber = peerNumber;
                        }
                        updatePeerInDb(key, it->second);
                    }

                    bool confNotCurrent = !(confNumber == currentChatId && currentChatType == "conference");

                    {
                        ChatElement el;
                        el.messageText = message;
                        el.category = "other";
                        el.peerNumber = peerNumber;
                        el.time = getCurrentTime();
                        if (it != peerInfoMap.end()) {
                            el.senderName = qFromUtf8(it->second.userName);
                            if (!it->second.nickname.empty())
                                el.senderNickname = qFromUtf8(it->second.nickname);
                        } else if (peerNameItem && cJSON_IsString(peerNameItem)) {
                            el.senderName = qFromUtf8(cJSON_GetStringValue(peerNameItem));
                        }
                        m_chatbuf.append(confNumber, "conference", el);
                        QString confTimeStr = timenowhm();
                        db_writeMessage(confNumber, "conference", el,
                            [confNumber, message, confTimeStr, confNotCurrent](int64_t rowid) {
                                db_writeLastMessage(confNumber, "conference", message, confTimeStr, rowid);
                                if (confNotCurrent) {
                                    db_writeUnreadIncrement(confNumber, "conference", rowid);
                                }
                            });
                    }
                    if (confNotCurrent) {
                        contactListWidget->incrementUnread(confNumber, "conference");
                    }
                    contactListWidget->updateContactLastMessage(confNumber, "conference", message, timenowhm());
                    if (!qIsAppActive())
                        playNotificationSound();
                } else {
                    qWarning("conference_message: missing confNumber or message");
                }
                cJSON_Delete(root);
            } else {
                qWarning("conference_message: failed to parse JSON: %s", e.data.c_str());
            }
        } else if (e.type == "self_connection_status") {
            chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
            ToxAPI::loadAllData();
        } else if (e.type == "group_invite") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* friendNumberItem = cJSON_GetObjectItem(root, "friend_number");
                cJSON* chatIdItem = cJSON_GetObjectItem(root, "chat_id");
                if (friendNumberItem && chatIdItem) {
                    int friendNumber = friendNumberItem->valueint;
                    QString chatId = qFromUtf8(cJSON_GetStringValue(chatIdItem));
                    onGroupInviteReceived(friendNumber, chatId);
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "group_message") {
            qWarning("Processing group_message event");
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* groupNumberItem = cJSON_GetObjectItem(root, "group_number");
                cJSON* messageItem = cJSON_GetObjectItem(root, "message");
                cJSON* peerNumberItem = cJSON_GetObjectItem(root, "peer_number");
                cJSON* peerNameItem = cJSON_GetObjectItem(root, "peer_name");
                
                if (groupNumberItem && messageItem) {
                    int groupNumber = groupNumberItem->valueint;
                    QString message = qFromUtf8(cJSON_GetStringValue(messageItem));
                    int peerNumber = peerNumberItem ? peerNumberItem->valueint : -1;
                    
                    std::string key = "group_" + std::to_string(groupNumber) + "_" + std::to_string(peerNumber);
                    auto it = peerInfoMap.find(key);

                    if (it == peerInfoMap.end()) {
                        qWarning("group_message: peer record load, key=%s", key.c_str());
                        PeerKey pk = parsePeerKey(key);
                        if (pk.valid) {
                            auto row = Storage::instance().channelDb()->get_chan_peer(
                                pk.chanid.c_str(), pk.peerNum);
                            it = loadRowToMap(peerInfoMap, key, std::move(row));
                        }
                        // 不创建空条目
                    } else {
                        // 第二层：key 已存在，逐个检查缺失字段
                        bool needsNick = it->second.nickname.empty();
                        bool needsAvatar = it->second.iconUrl.empty();
                        if (needsNick || needsAvatar) {
                            qWarning("group_message: peer field fill, key=%s nick=%s avatar=%s",
                                     key.c_str(), needsNick?"miss":"hit", needsAvatar?"miss":"hit");
                            PeerKey pk = parsePeerKey(key);
                            if (pk.valid) {
                                auto row = Storage::instance().channelDb()->get_chan_peer(
                                    pk.chanid.c_str(), pk.peerNum);
                                if (row) {
                                    if (needsNick && !row->nickname.empty()) {
                                        it->second.nickname = row->nickname;
                                    }
                                    if (needsAvatar && !row->avatar_url.empty()) {
                                        it->second.iconUrl = row->avatar_url;
                                        QString mxc = qFromUtf8(it->second.iconUrl);
                                        if (AvatarManager::inst().requestDownload(mxc)) {
                                            ToxAPI::downloadAvatar(it->second.iconUrl);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (it != peerInfoMap.end()) {
                        if (peerNameItem && cJSON_IsString(peerNameItem)) {
                            it->second.userName = std::string(cJSON_GetStringValue(peerNameItem));
                            it->second.peerNumber = peerNumber;
                        }
                        updatePeerInDb(key, it->second);
                    }

                    bool groupNotCurrent = !(groupNumber == currentChatId && currentChatType == "group");

                    {
                        ChatElement el;
                        el.messageText = message;
                        el.category = "other";
                        el.peerNumber = peerNumber;
                        el.time = getCurrentTime();
                        if (it != peerInfoMap.end()) {
                            el.senderName = qFromUtf8(it->second.userName);
                            el.ipAddress = qFromUtf8(it->second.peerIp);
                            if (!it->second.nickname.empty())
                                el.senderNickname = qFromUtf8(it->second.nickname);
                        } else if (peerNameItem && cJSON_IsString(peerNameItem)) {
                            el.senderName = qFromUtf8(cJSON_GetStringValue(peerNameItem));
                        }
                        m_chatbuf.append(groupNumber, "group", el);
                        QString groupTimeStr = timenowhm();
                        db_writeMessage(groupNumber, "group", el,
                            [groupNumber, message, groupTimeStr, groupNotCurrent](int64_t rowid) {
                                db_writeLastMessage(groupNumber, "group", message, groupTimeStr, rowid);
                                if (groupNotCurrent) {
                                    db_writeUnreadIncrement(groupNumber, "group", rowid);
                                }
                            });
                    }
                    if (groupNotCurrent) {
                        contactListWidget->incrementUnread(groupNumber, "group");
                    }
                    contactListWidget->updateContactLastMessage(groupNumber, "group", message, timenowhm());
                    if (!qIsAppActive())
                        playNotificationSound();
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "conference_peer_name") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* confNumberItem = cJSON_GetObjectItem(root, "conference_number");
                cJSON* peerNumberItem = cJSON_GetObjectItem(root, "peer_number");
                cJSON* nameItem = cJSON_GetObjectItem(root, "name");
                if (confNumberItem && peerNumberItem && nameItem && cJSON_IsString(nameItem)) {
                    std::string key = "conference_" + std::to_string(confNumberItem->valueint)
                        + "_" + std::to_string(peerNumberItem->valueint);
                    auto it = peerInfoMap.find(key);
                    if (it == peerInfoMap.end()) {
                        PeerKey pk = parsePeerKey(key);
                        if (pk.valid) {
                            auto row = Storage::instance().channelDb()->get_chan_peer(
                                pk.chanid.c_str(), pk.peerNum);
                            it = loadRowToMap(peerInfoMap, key, std::move(row));
                        }
                    }
                    if (it != peerInfoMap.end()) {
                        it->second.userName = std::string(cJSON_GetStringValue(nameItem));
                        it->second.peerNumber = peerNumberItem->valueint;
                    }
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "group_peer_name") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* groupNumberItem = cJSON_GetObjectItem(root, "group_number");
                cJSON* peerNumberItem = cJSON_GetObjectItem(root, "peer_number");
                cJSON* nameItem = cJSON_GetObjectItem(root, "name");
                if (groupNumberItem && peerNumberItem && nameItem && cJSON_IsString(nameItem)) {
                    std::string key = "group_" + std::to_string(groupNumberItem->valueint)
                        + "_" + std::to_string(peerNumberItem->valueint);
                    auto it = peerInfoMap.find(key);
                    if (it == peerInfoMap.end()) {
                        PeerKey pk = parsePeerKey(key);
                        if (pk.valid) {
                            auto row = Storage::instance().channelDb()->get_chan_peer(
                                pk.chanid.c_str(), pk.peerNum);
                            it = loadRowToMap(peerInfoMap, key, std::move(row));
                        }
                    }
                    if (it != peerInfoMap.end()) {
                        it->second.userName = std::string(cJSON_GetStringValue(nameItem));
                        it->second.peerNumber = peerNumberItem->valueint;
                    }
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "friend_name") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* fid = cJSON_GetObjectItem(root, "friend_id");
                cJSON* nameItem = cJSON_GetObjectItem(root, "name");
                if (fid && nameItem && cJSON_IsString(nameItem)) {
                    int friendId = fid->valueint;
                    std::string newName = cJSON_GetStringValue(nameItem);
                    std::string key = "friend_" + std::to_string(friendId);
                    auto it = peerInfoMap.find(key);
                    if (it != peerInfoMap.end()) {
                        it->second.userName = newName;
                        updatePeerInDb(key, it->second);
                    }
                    contactListWidget->updateFriendName(friendId, qFromUtf8(newName));
                    updateContactDb("friend_" + std::to_string(friendId), newName, "", -1);
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "friend_status") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* fid = cJSON_GetObjectItem(root, "friend_id");
                cJSON* statusItem = cJSON_GetObjectItem(root, "status");
                if (fid && statusItem) {
                    int friendId = fid->valueint;
                    int s = statusItem->valueint;
                    std::string key = "friend_" + std::to_string(friendId);
                    auto it = peerInfoMap.find(key);
                    if (it != peerInfoMap.end()) {
                        it->second.status = s;
                        updatePeerInDb(key, it->second);
                    }
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "friend_connection_status") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* fid = cJSON_GetObjectItem(root, "friend_id");
                cJSON* statusItem = cJSON_GetObjectItem(root, "status");
                if (fid && statusItem && cJSON_IsString(statusItem)) {
                    int friendId = fid->valueint;
                    std::string statusStr = cJSON_GetStringValue(statusItem);
                    std::string key = "friend_" + std::to_string(friendId);
                    auto it = peerInfoMap.find(key);
                    if (it != peerInfoMap.end()) {
                        it->second.status = (statusStr == "udp") ? 2 : (statusStr == "tcp") ? 1 : 0;
                        it->second.statusStr = statusStr;
                        updatePeerInDb(key, it->second);
                    }
                    contactListWidget->updateFriendConnectionStatus(friendId, qFromUtf8(statusStr));
                    updateContactDb("friend_" + std::to_string(friendId), "", "",
                                    (statusStr == "udp" || statusStr == "tcp") ? 1 : 0);
                }
                cJSON_Delete(root);
            }
        } else if (e.data.find("\"Type\":\"event.Evt") != std::string::npos) {
            // System event with Type field — 归入 sysevent
            {
                ChatElement msg;
                msg.messageText = qFromUtf8("[" + e.type + "]\n" + e.data);
                msg.category = "other";
                msg.senderName = "Sysevent";
                msg.peerNumber = VIRTUAL_SYSEVENT_ID;
                msg.time = getCurrentTime();
                qWarning("Cache PUSH to sysevent %d: type=%s (event.Evt)", VIRTUAL_SYSEVENT_ID, e.type.c_str());
                m_chatbuf.append(VIRTUAL_SYSEVENT_ID, kSyseventType, msg);
                db_writeMessage(VIRTUAL_SYSEVENT_ID, kSyseventType, msg);
                if (!(currentChatId == VIRTUAL_SYSEVENT_ID && currentChatType == kSyseventType)) {
                    contactListWidget->incrementUnread(VIRTUAL_SYSEVENT_ID, kSyseventType);
                    db_writeUnreadIncrement(VIRTUAL_SYSEVENT_ID, kSyseventType);
                }
            }
        } else if (!e.type.empty() && e.type[0] == '_') {
            // System event — 始终缓存到 Sysevent，正在查看时也追加到界面
            {
                ChatElement msg;
                if (e.type == "_server_restart") {
                    msg.messageText = "[Server restart detected]";
                } else {
                    msg.messageText = qFromUtf8("[" + e.type + "]\n" + e.data);
                }
                msg.category = "other";
                msg.senderName = "Sysevent";
                msg.peerNumber = VIRTUAL_SYSEVENT_ID;
                msg.time = getCurrentTime();
                qWarning("Cache PUSH to sysevent %d: type=%s", VIRTUAL_SYSEVENT_ID, e.type.c_str());
                m_chatbuf.append(VIRTUAL_SYSEVENT_ID, kSyseventType, msg);
                db_writeMessage(VIRTUAL_SYSEVENT_ID, kSyseventType, msg);
                if (!(currentChatId == VIRTUAL_SYSEVENT_ID && currentChatType == kSyseventType)) {
                    contactListWidget->incrementUnread(VIRTUAL_SYSEVENT_ID, kSyseventType);
                    db_writeUnreadIncrement(VIRTUAL_SYSEVENT_ID, kSyseventType);
                }
            }
        } else {
            ParseResult pr = UnknownParser::parse(e.type, e.data);

            // ── 更新 peers ──
            if (!pr.peers.empty()) {
                for (const auto& p : pr.peers) {
                    std::string key = "unknown_" + p.publicKey;
                    addPeerToDb(key, p);
                    // 没有 nickname 和 avatar 的数据没必要缓存
                    if (p.nickname.empty() && p.iconUrl.empty()) {
                        continue;
                    }
                    mergePeerInfo(getOrCreatePeerEntry(peerInfoMap, key), p);
                }
                for (const auto& p : pr.peers) {
                    QString mxc = qFromUtf8(p.iconUrl);
                    if (AvatarManager::inst().requestDownload(mxc)) {
                        ToxAPI::downloadAvatar(p.iconUrl);
                    }
                }
            }

            // ── 更新 contacts ──
            if (!pr.contacts.empty()) {
                for (const auto& cd : pr.contacts) {
                    bool updated = false;
                    for (auto& existing : m_accumulatedContactData) {
                        if (existing.id == cd.id && existing.type == cd.type) {
                            existing = cd;
                            updated = true;
                            break;
                        }
                    }
                    if (!updated) {
                        m_accumulatedContactData.push_back(cd);

                        Contact* c = new Contact();
                        c->id = cd.id;
                        c->name = qFromUtf8(cd.name);
                        c->type = qFromUtf8(cd.type);
                        c->status = qFromUtf8(cd.status);
                        c->chat_id = qFromUtf8(cd.chatId);
                        c->is_connected = cd.isConnected;
                        contactListWidget->addContact(c);
                    } else {
                        contactListWidget->updateContact(cd.id, qFromUtf8(cd.type),
                            qFromUtf8(cd.name), qFromUtf8(cd.chatId), qFromUtf8(cd.status));
                    }
                    updateContactDb(cd.type + "_" + std::to_string(cd.id),
                                    cd.name, cd.status, cd.isConnected ? 1 : 0,
                                    cd.iconUrl, cd.chatId, cd.type);
                }
            }

            // ── 消息处理 ──
            if (!pr.messages.empty()) {
                for (const auto& hm : pr.messages) {
                    ChatElement msg;
                    msg.messageText = qFromUtf8(hm.message);
                    msg.category = "other";
msg.time = hm.created_at.empty() ? getCurrentTime()
                                                     : qFormatTime(qFromUtf8(hm.created_at));

                    QString senderLabel;
                    QString userName;
                    QString avatarMxc;
                    if (!hm.sender_pubkey.empty()) {
                        for (const auto& p : pr.peers) {
                            if (p.publicKey == hm.sender_pubkey) {
                                userName = qFromUtf8(p.userName);
                                senderLabel = !p.nickname.empty()
                                    ? qFromUtf8(p.nickname) : QString();
                                avatarMxc = qFromUtf8(p.iconUrl);
                                break;
                            }
                        }
                        if (senderLabel.isEmpty()) {
                            std::string key = "unknown_" + hm.sender_pubkey;
                            auto it = peerInfoMap.find(key);
                            if (it == peerInfoMap.end()) {
                                PeerKey pk = parsePeerKey(key);
                                if (pk.valid) {
                                    auto row = Storage::instance().channelDb()->get_chan_peer(
                                        pk.chanid.c_str(), pk.peerNum);
                                    it = loadRowToMap(peerInfoMap, key, std::move(row));
                                }
                            }
                            if (it != peerInfoMap.end()) {
                                userName = qFromUtf8(it->second.userName);
                                senderLabel = !it->second.nickname.empty()
                                    ? qFromUtf8(it->second.nickname) : QString();
                                avatarMxc = qFromUtf8(it->second.iconUrl);
                            }
                        }
                    }
                    msg.senderName = userName;
                    msg.senderNickname = senderLabel;
                    msg.peerNumber = (int)hm.sender_number;
                    msg.avatarUrl = avatarMxc;
                    msg.messageId = qFromUtf8(hm.eventId);
                    msg.redacted = hm.redacted;

                    int chatId = VIRTUAL_REDDIT_ID;
                    std::string chatType = kTopicType;
                    for (const auto& cd : pr.contacts) {
                        if (cd.chatId == hm.roomId) {
                            chatId = cd.id;
                            chatType = cd.type;
                            break;
                        }
                    }

                    if (chatType == kFilesyncType) {
                        msg.etype = ChatElement::File;
                        QString raw = qFromUtf8(hm.message);
                        int colonPos = -1;
                        for (int i = 0; i < raw.length() - 1; i++) {
                            if (raw[i] == ':' && raw[i+1] == ' ') {
                                colonPos = i; break;
                            }
                        }
                        if (colonPos >= 0) {
                            msg.messageText = raw.left(colonPos);
                            msg.fileName    = raw.mid(colonPos + 2);
                        } else {
                            msg.messageText = QString();
                            msg.fileName    = raw;
                        }
                        int slashPos = -1;
                        for (int i = 0; i < msg.fileName.length(); i++) {
                            if (msg.fileName[i] == '/') { slashPos = i; }
                        }
                        msg.caption = (slashPos >= 0)
                            ? msg.fileName.mid(slashPos + 1) : msg.fileName;
                        msg.fileSize = hm.fileSize;
                    } else if (hm.msgtype == "image") {
                        bool isGif = (hm.mediaMime.find("image/gif") != std::string::npos)
                                  || isGifUrl(hm.mediaUrl);
                        msg.etype       = isGif ? ChatElement::Gif : ChatElement::Image;
                        msg.caption     = qFromUtf8(hm.message);
                        msg.messageText = qFromUtf8(hm.message);
                        msg.mediaWidth  = hm.mediaWidth;
                        msg.mediaHeight = hm.mediaHeight;
                        msg.mediaUrl    = qFromUtf8(hm.mediaUrl);
                        msg.fileSize    = hm.fileSize;
                    } else if (hm.msgtype == "video") {
                        msg.etype       = ChatElement::Video;
                        msg.caption     = qFromUtf8(hm.message);
                        msg.messageText = qFromUtf8(hm.message);
                        msg.mediaWidth  = hm.mediaWidth;
                        msg.mediaHeight = hm.mediaHeight;
                        msg.durationSec = hm.duration / 1000;
                        msg.mediaUrl    = qFromUtf8(hm.mediaUrl);
                        msg.fileSize    = hm.fileSize;
                    } else if (hm.msgtype == "audio") {
                        msg.etype       = ChatElement::Audio;
                        msg.caption     = qFromUtf8(hm.message);
                        msg.messageText = qFromUtf8(hm.message);
                        msg.durationSec = hm.duration / 1000;
                        msg.mediaUrl    = qFromUtf8(hm.mediaUrl);
                        msg.fileSize    = hm.fileSize;
                    } else if (hm.msgtype == "file") {
                        msg.etype       = ChatElement::File;
                        msg.fileName    = qFromUtf8(hm.message);
                        msg.caption     = qFromUtf8(hm.message);
                        msg.mediaUrl    = qFromUtf8(hm.mediaUrl);
                        msg.fileSize    = hm.fileSize;
                    }
                
                    qWarning("Cache PUSH to %s %d: sender=%s",
                             chatType.c_str(), chatId, qToUtf8(msg.senderName).data());
                    contactListWidget->updateContactLastMessage(
                        chatId, qFromUtf8(chatType), msg.messageText, timenowhm());
                    db_writeLastMessage(chatId, qFromUtf8(chatType), msg.messageText, timenowhm());
                    m_chatbuf.append(chatId, chatType, msg);
                    db_writeMessage(chatId, chatType, msg);
                    if (currentChatId == chatId && currentChatType == qFromUtf8(chatType)) {
                        int newIdx = m_chatbuf.getOrCreate(chatId, chatType).size() - 1;
                        if (!hm.mediaUrl.empty() && hm.msgtype != "file") {
                            QString mxc = qFromUtf8(hm.mediaUrl);
                            ChatElement& el = m_chatbuf.getOrCreate(chatId, chatType)[newIdx];

                            QByteArray rawBytes = MediaShmemCache::inst().getThumb(mxc);
                            if (!rawBytes.isEmpty()) {
                                el.scaledDisplay = decodeRawToThumb(rawBytes.data(), rawBytes.size(),
                                    el.mediaWidth, el.mediaHeight, chatWidget->width() * 70 / 100);
                                el.downloadState = ChatElement::Completed;
                                el.downloadProgress = 100;
                                el.cachedWidth = -1;
                                chatWidget->updateElement(newIdx);
                            } else {
                                auto dbData = Storage::instance().cacheDb()->loadMedia(
                                    mediaCacheKey("file", mxc).c_str());
                                if (!dbData.empty()) {
                                    MediaShmemCache::inst().putThumb(mxc, (const char*)dbData.data(), dbData.size());
                                    el.scaledDisplay = decodeRawToThumb((const char*)dbData.data(), dbData.size(),
                                        el.mediaWidth, el.mediaHeight, chatWidget->width() * 70 / 100);
                                    el.downloadState = ChatElement::Completed;
                                    el.downloadProgress = 100;
                                    el.cachedWidth = -1;
                                    chatWidget->updateElement(newIdx);
                                } else {
                                    if (hm.fileSize <= 0 || (hm.fileSize > 0 && hm.fileSize < 1048576)) {
                                        el.downloadState = ChatElement::InProgress;
                                        el.downloadProgress = 0;
                                        ToxAPI::downloadMedia(chatId, chatType, newIdx, hm.mediaUrl, hm.fileSize);
                                    }
                                }
                            }
                        }
                    } else {
                        contactListWidget->incrementUnread(chatId, qFromUtf8(chatType));
                        db_writeUnreadIncrement(chatId, qFromUtf8(chatType));
                    }

                    // 声音通知
                    if (!qIsAppActive()) {
                        std::vector<std::string> names;
                        QString self = selfInfoWidget->selfName();
                        if (!self.isEmpty())
                            names.push_back(qToUtf8(self).data());
                        if (unknownShouldPlaySound(chatType, hm.message, names))
                            playNotificationSound();
                    }
                }
            } else if (pr.handled && pr.contactName == "reddit") {
                ChatElement msg;
                msg.category = "other";
                msg.time = getCurrentTime();
                msg.messageText = pr.messageText;
                msg.senderName = pr.senderName;
                msg.peerNumber = VIRTUAL_REDDIT_ID;
                qWarning("Cache PUSH to reddit %d: sender=%s",
                         VIRTUAL_REDDIT_ID, qToUtf8(msg.senderName).data());
                m_chatbuf.append(VIRTUAL_REDDIT_ID, kTopicType, msg);
                QString redditTimeStr = timenowhm();
                QString redditMsgText = msg.messageText;
                bool redditNotCurrent = !(currentChatId == VIRTUAL_REDDIT_ID && currentChatType == kTopicType);
                db_writeMessage(VIRTUAL_REDDIT_ID, kTopicType, msg,
                    [redditTimeStr, redditMsgText, redditNotCurrent](int64_t rowid) {
                        db_writeLastMessage(VIRTUAL_REDDIT_ID, kTopicType, redditMsgText, redditTimeStr, rowid);
                        if (redditNotCurrent) {
                            db_writeUnreadIncrement(VIRTUAL_REDDIT_ID, kTopicType, rowid);
                        }
                    });
                contactListWidget->updateContactLastMessage(
                    VIRTUAL_REDDIT_ID, kTopicType, msg.messageText, timenowhm());
                if (redditNotCurrent) {
                    contactListWidget->incrementUnread(VIRTUAL_REDDIT_ID, kTopicType);
                }
            } else {
                ChatElement msg;
                msg.category = "other";
                msg.time = getCurrentTime();
                if (pr.handled) {
                    msg.messageText = pr.messageText;
                    msg.senderName = pr.senderName;
                } else {
                    msg.messageText = qFromUtf8("[" + e.type + "]\n" + e.data);
                    msg.senderName = "Unknown";
                }
                msg.peerNumber = VIRTUAL_UNKNOWN_ID;
                qWarning("Cache PUSH to unknown %d: type=%s, handled=%d, sender=%s",
                         VIRTUAL_UNKNOWN_ID, e.type.c_str(), pr.handled, qToUtf8(msg.senderName).data());
                m_chatbuf.append(VIRTUAL_UNKNOWN_ID, kUnknownType, msg);
                QString unkTimeStr = timenowhm();
                QString unkMsgText = msg.messageText;
                bool unkNotCurrent = !(currentChatId == VIRTUAL_UNKNOWN_ID && currentChatType == kUnknownType);
                db_writeMessage(VIRTUAL_UNKNOWN_ID, kUnknownType, msg,
                    [unkTimeStr, unkMsgText, unkNotCurrent](int64_t rowid) {
                        db_writeLastMessage(VIRTUAL_UNKNOWN_ID, kUnknownType, unkMsgText, unkTimeStr, rowid);
                        if (unkNotCurrent) {
                            db_writeUnreadIncrement(VIRTUAL_UNKNOWN_ID, kUnknownType, rowid);
                        }
                    });
                contactListWidget->updateContactLastMessage(
                    VIRTUAL_UNKNOWN_ID, kUnknownType, msg.messageText, timenowhm());
                if (unkNotCurrent) {
                    contactListWidget->incrementUnread(VIRTUAL_UNKNOWN_ID, kUnknownType);
                }
            }
        }
    }
    contactListWidget->endBatch();
    long long _el = elapsedMs(_t0); if (_el >= 500) qWarning("SLOW [hangui] handleEvents(%zu events) took %lldms", events.size(), _el);
}

void MainWindow::onLanguageChanged(const QString& langCode) {
    Translator::instance().loadLanguage(langCode);
    QtappSetup::installQtTranslations(langCode);
}

void MainWindow::retranslateUi() {
    qSetWindowTitle(this, _("app_title"));
    
    // 更新子控件
    if (selfInfoWidget) { selfInfoWidget->retranslateUi(); }
    if (contactListWidget) { contactListWidget->retranslateUi(); }
    if (chatWidget) { chatWidget->retranslateUi(); }
    
    if (currentChatId == -1) {
        chatWidget->setHeaderText(_("select_chat_object"));
    } else {
        QString headerText;
        if (currentChatType == "friend") {
            headerText = _("chat_with_friend").arg(QString::number(currentChatId));
        } else if (currentChatType == "group") {
            headerText = _("group") + " " + QString::number(currentChatId);
        } else if (currentChatType == "conference") {
            headerText = _("conference_item") + " " + QString::number(currentChatId);
        } else if (currentChatType == kUnknownType) {
            headerText = QString(_("unknown")) + " " + QString::number(currentChatId);
        } else if (currentChatType == kSyseventType) {
            headerText = QString("System Events") + " " + QString::number(currentChatId);
        } else if (currentChatType == kBookmarkType) {
            headerText = QString("Bookmark") + " " + QString::number(currentChatId);
        } else if (currentChatType == kAichatType) {
            headerText = QString("AI Chat") + " " + QString::number(currentChatId);
        } else if (currentChatType == kPastebinType) {
            headerText = QString("Paste Bin") + " " + QString::number(currentChatId);
        } else if (currentChatType == kTranslateType) {
            headerText = QString("Translate") + " " + QString::number(currentChatId);
        } else if (!protoStreamEmoji(currentChatType).isEmpty()) {
            QString label;
            std::string type = std::string(qToUtf8(currentChatType).data());
            for (const auto& cd : m_accumulatedContactData) {
                if (cd.id == currentChatId && cd.type == type) {
                    label = qFromUtf8(cd.name);
                    break;
                }
            }
            if (label.isEmpty()) { label = currentChatType; }
            headerText = protoStreamEmoji(currentChatType) + " " + label;
        }
        chatWidget->setHeaderText(headerText);
    }
}

void MainWindow::onViewInfoRequested(int id, const QString& type) {
    FriendInfoDialog dialog(this);
    
    if (type == "friend") {
        std::string key = "friend_" + std::to_string(id);
        auto it = peerInfoMap.find(key);
        if (it == peerInfoMap.end()) {
            PeerKey pk = parsePeerKey(key);
            if (pk.valid) {
                auto row = Storage::instance().channelDb()->get_chan_peer(
                    pk.chanid.c_str(), pk.peerNum);
                it = loadRowToMap(peerInfoMap, key, std::move(row));
            }
        }
        if (it != peerInfoMap.end()) {
            if (it->second.statusText.empty())
                ToxAPI::lazyLoadFriendDetail(id);
            dialog.setInfo(friendInfoFromPeer(it->second, id));
        } else {
            dialog.setInfo(id, _("no_name"), type);
        }
    } else if (type == "conference") {
        dialog.setTitle(_("modals.conference_info_title"));
        std::vector<ConferenceInfo> conferences = ToxAPI::getConferencesSync();
        bool isConnected = false;
        QString chatId = "";
        QString statusText = "";
        int memberCount = 0;
        for (size_t i = 0; i < conferences.size(); ++i) {
            if (conferences[i].conferenceNumber == (uint32_t)id) {
                isConnected = conferences[i].isConnected;
                chatId = qFromUtf8(conferences[i].chatId);
                statusText = qFromUtf8(conferences[i].statusText);
                memberCount = conferences[i].memberCount;
                break;
            }
        }
        dialog.setInfo(id, _("conference_item") + " " + QString::number(id), type,
                       statusText, QString(), "", isConnected, chatId);
        dialog.setPeerCount(memberCount);
    } else if (type == "group") {
        dialog.setTitle(_("modals.group_info_title"));
        std::vector<GroupInfo> groups = ToxAPI::getGroupsSync();
        bool isConnected = false;
        QString chatId = "";
        QString statusText = "";
        int memberCount = 0;
        for (size_t i = 0; i < groups.size(); ++i) {
            if (groups[i].groupNumber == (uint32_t)id) {
                isConnected = groups[i].isConnected;
                chatId = qFromUtf8(groups[i].chatId);
                statusText = qFromUtf8(groups[i].statusText);
                memberCount = groups[i].memberCount;
                break;
            }
        }
        dialog.setInfo(id, _("group_item") + " " + QString::number(id), type,
                       statusText, QString(), "", isConnected, chatId);
        dialog.setPeerCount(memberCount);
    }
    
    dialog.exec();
}

void MainWindow::onChatPeerInfoRequested(int peerNumber, const QString& senderName,
                                         const QString& senderPubkey) {
    FriendInfoDialog dialog(this);
    std::string key;
    if (currentChatType == "friend") {
        key = "friend_" + std::to_string(peerNumber);
    } else if (currentChatType == "unknown") {
        if (!senderPubkey.isEmpty()) {
            key = "unknown_" + std::string(qToUtf8(senderPubkey).data());
        }
    } else {
        key = std::string(qToUtf8(currentChatType).data())
            + "_" + std::to_string(currentChatId)
            + "_" + std::to_string(peerNumber);
    }
    auto it = peerInfoMap.find(key);
    if (it == peerInfoMap.end() && !key.empty()) {
        PeerKey pk = parsePeerKey(key);
        if (pk.valid) {
            auto row = Storage::instance().channelDb()->get_chan_peer(
                pk.chanid.c_str(), pk.peerNum);
            it = loadRowToMap(peerInfoMap, key, std::move(row));
        }
    }
    if (it != peerInfoMap.end()) {
        QString title = it->second.nickname.empty()
            ? qFromUtf8(it->second.userName) : qFromUtf8(it->second.nickname);
        if (title.isEmpty()) { title = senderName; }
        dialog.setTitle(title);
        dialog.setInfo(friendInfoFromPeer(it->second, peerNumber));
    } else {
        dialog.setTitle(senderName.isEmpty() ? _("no_name") : senderName);
        dialog.setInfo(peerNumber,
                       senderName.isEmpty() ? _("no_name") : senderName,
                       currentChatType.isEmpty() ? QString("friend") : currentChatType);
    }
    dialog.exec();
}

void MainWindow::onViewMembersRequested(int id, const QString& type) {
    std::vector<PeerInfo> members;
    QString title;
    
    if (type == "conference") {
        members = ToxAPI::getConferenceMembersSync(id);
        title = _("member_list.title.conference").arg(QString::number(id));
    } else if (type == "group") {
        members = ToxAPI::getGroupMembersSync(id);
        title = _("member_list.title.group").arg(QString::number(id));
    } else {
        return;
    }
    
    MemberListDialog dialog(this);
    dialog.setDialogTitle(title);
    dialog.setMembers(members);
    dialog.exec();
}

void MainWindow::onRenameNickRequested(int groupId, const QString& groupName) {
    std::string globalName = qToUtf8(selfInfoWidget->selfName()).data();

    // Scan peerInfoMap for own peer in this group
    std::string currentGroupNick;
    std::string prefix = "group_" + std::to_string(groupId) + "_";
    for (const auto& pair : peerInfoMap) {
        if (pair.second.isSelf && pair.first.find(prefix) == 0) {
            currentGroupNick = pair.second.userName;
            break;
        }
    }
    if (currentGroupNick.empty()) {
        currentGroupNick = globalName;
    }

    QDialog dialog(this);
    qSetWindowTitle(&dialog, _("rename.title") + QString(" - ") + groupName);
    dialog.resize(380, 220);

    QBoxLayout* layout = qNewBoxLayout(&dialog, QBoxLayout::TopToBottom, 10, 10);

    // Current nickname (from cache or fallback to global)
    std::string displayName = currentGroupNick.empty() ? "--" : currentGroupNick;
    QLabel* currentLabel = new QLabel(
        _("rename.current_nick") + QString(": ") + qFromUtf8(displayName), &dialog);
    layout->addWidget(currentLabel);

    // Use global nick
    QRadioButton* selfRadio = new QRadioButton(
        _("rename.use_self_nick") + QString(" (%1)").arg(qFromUtf8(globalName)), &dialog);
    selfRadio->setChecked(true);
    layout->addWidget(selfRadio);

    // Custom nick
    QRadioButton* customRadio = new QRadioButton(_("rename.custom_nick"), &dialog);
    layout->addWidget(customRadio);

    PlaceholderLineEdit* nameEdit = new PlaceholderLineEdit(_("rename.enter_nick"), &dialog);
    nameEdit->setText(qFromUtf8(currentGroupNick));
    nameEdit->setEnabled(false);
    layout->addWidget(nameEdit);

    // Random nick
    QRadioButton* randomRadio = new QRadioButton(_("rename.random_nick"), &dialog);
    layout->addWidget(randomRadio);

    QLabel* randomLabel = new QLabel(
        qFromUtf8(ToxAPI::getRandomNameSync()), &dialog);
    layout->addWidget(randomLabel);

    // Buttons
    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    btnLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));

    QPushButton* confirmBtn = new QPushButton(_("rename.confirm"), &dialog);
    QPushButton* cancelBtn = new QPushButton(_("buttons.cancel"), &dialog);
    btnLayout->addWidget(confirmBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    QObject::connect(confirmBtn, SIGNAL(clicked()), &dialog, SLOT(accept()));
    QObject::connect(cancelBtn, SIGNAL(clicked()), &dialog, SLOT(reject()));

    if (dialog.exec() == QDialog::Accepted) {
        std::string name;
        if (selfRadio->isChecked()) {
            name = globalName;
        } else if (customRadio->isChecked()) {
            name = qToUtf8(nameEdit->text()).data();
            if (name.empty()) {
                QMessageBox::warning(this, _("rename.failed"), _("rename.name_empty"));
                return;
            }
        } else if (randomRadio->isChecked()) {
            name = qToUtf8(randomLabel->text()).data();
            if (name.empty()) {
                QMessageBox::warning(this, _("rename.failed"), _("rename.name_empty"));
                return;
            }
        }
        if (!name.empty()) {
            ToxAPI::setGroupSelfNameSync(groupId, name);
        }
    }
}

void MainWindow::onSetGroupTopicRequested(int groupId) {
    auto groups = ToxAPI::getGroupsSync();
    std::string currentTopic;
    for (const auto& g : groups) {
        if (g.groupNumber == groupId) {
            currentTopic = g.statusText;
            break;
        }
    }

    bool ok = false;
    QString topic;
#ifdef QT3_BUILD
    topic = QInputDialog::getText(_("set_topic.title"), _("set_topic.prompt"),
                                  QLineEdit::Normal, qFromUtf8(currentTopic), &ok, this);
#else
    topic = QInputDialog::getText(this, _("set_topic.title"), _("set_topic.prompt"),
                                  QLineEdit::Normal, qFromUtf8(currentTopic), &ok);
#endif
    if (ok && !topic.isEmpty()) {
        if (ToxAPI::setGroupTopicSync(groupId, qToUtf8(topic).data())) {
            chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
            ToxAPI::loadAllData();
        } else {
            QMessageBox::warning(this, _("error"), _("set_topic.failed"));
        }
    }
}

void MainWindow::onSetConferenceTitleRequested(int conferenceId) {
    auto conferences = ToxAPI::getConferencesSync();
    std::string currentTitle;
    for (const auto& c : conferences) {
        if (c.conferenceNumber == conferenceId) {
            currentTitle = c.statusText;
            break;
        }
    }

    bool ok = false;
    QString title;
#ifdef QT3_BUILD
    title = QInputDialog::getText(_("set_title.title"), _("set_title.prompt"),
                                  QLineEdit::Normal, qFromUtf8(currentTitle), &ok, this);
#else
    title = QInputDialog::getText(this, _("set_title.title"), _("set_title.prompt"),
                                  QLineEdit::Normal, qFromUtf8(currentTitle), &ok);
#endif
    if (ok && !title.isEmpty()) {
        if (ToxAPI::setConferenceTitleSync(conferenceId, qToUtf8(title).data())) {
            chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
            ToxAPI::loadAllData();
        } else {
            QMessageBox::warning(this, _("error"), _("set_title.failed"));
        }
    }
}

void MainWindow::onDeleteOrLeaveRequested(int id, const QString& type) {
    QString confirmMsg;
    if (type == "friend") {
        confirmMsg = _("confirm_delete_friend").arg(QString::number(id));
    } else if (type == "conference") {
        confirmMsg = _("confirm_leave_conference").arg(QString::number(id));
    } else if (type == "group") {
        confirmMsg = _("confirm_leave_group").arg(QString::number(id));
    } else {
        return;
    }
    
#ifdef QT3_BUILD
    int result = QMessageBox::question(this, _("confirm"), confirmMsg,
                                       QMessageBox::Yes, QMessageBox::No);
    if (result == QMessageBox::Yes) {
#else
    if (QMessageBox::question(this, _("confirm"), confirmMsg,
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
#endif
        bool success = false;
        
        if (type == "friend") {
            success = ToxAPI::deleteFriendSync(id);
            if (success) {
                QMessageBox::information(this, _("friend_deleted"), _("friend_deleted"));
            } else {
                QMessageBox::warning(this, _("error"), _("delete_failed"));
            }
        } else if (type == "conference") {
            success = ToxAPI::leaveConferenceSync(id);
            if (success) {
                QMessageBox::information(this, _("conference_leave_success"), _("conference_leave_success"));
            }
        } else if (type == "group") {
            success = ToxAPI::leaveGroupSync(id);
            if (success) {
                QMessageBox::information(this, _("group_leave_success"), _("group_leave_success"));
            }
        }
        
        if (success) {
            // 如果是当前聊天对象，重置视图状态
            if (id == currentChatId && type == currentChatType) {
                currentChatId = -1;
                currentChatType = "";
                chatWidget->setAttrForChat(QString(), -1);
                chatWidget->resetPendingContext();
                chatWidget->setHeaderText(_("select_chat_object"));
                chatWidget->setBuffer(nullptr);
            }
            // 重新加载联系人列表
            chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
            ToxAPI::loadAllData();
        }
    }
}

void MainWindow::onInviteToConferenceRequested(int friendId) {
    // 获取会议列表
    std::vector<ConferenceInfo> conferences = ToxAPI::getConferencesSync();
    
    if (conferences.empty()) {
        QMessageBox::warning(this, _("no_conference"), _("no_conference"));
        return;
    }
    
    // 创建选择对话框
    QDialog dialog(this);
    qSetWindowTitle(&dialog, _("select_conference"));
    dialog.resize(300, 150);
    
    QBoxLayout* layout = qNewBoxLayout(&dialog, QBoxLayout::TopToBottom, 10, 10);
    
    QLabel* label = new QLabel(_("select_conference"), &dialog);
    layout->addWidget(label);
    
    QComboBox* confCombo = new QComboBox(&dialog);
    // Qt3: 用单独的数组存储ID
    std::vector<int> confIds;
    for (uint i = 0; i < conferences.size(); ++i) {
        const auto& conf = conferences[i];
        QString displayName;
        if (!conf.conferenceName.empty()) {
            displayName = qFromUtf8(conf.conferenceName);
        } else {
            displayName = QString(_("conference_item")) + " " + QString::number(conf.conferenceNumber);
        }
#ifdef QT3_BUILD
        confCombo->insertItem(displayName);
        confIds.push_back(conf.conferenceNumber);
#else
        confCombo->addItem(displayName, QVariant(conf.conferenceNumber));
#endif
    }
    layout->addWidget(confCombo);
    
    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    btnLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
    
    QPushButton* inviteBtn = new QPushButton(_("buttons.add"), &dialog);
    connect(inviteBtn, SIGNAL(clicked()), &dialog, SLOT(accept()));
    btnLayout->addWidget(inviteBtn);
    
    QPushButton* cancelBtn = new QPushButton(_("buttons.cancel"), &dialog);
    connect(cancelBtn, SIGNAL(clicked()), &dialog, SLOT(reject()));
    btnLayout->addWidget(cancelBtn);
    
    layout->addLayout(btnLayout);
    
    if (dialog.exec() == QDialog::Accepted) {
        int confId = -1;
#ifdef QT3_BUILD
        // Qt3: 从单独的ID数组获取
        int idx = confCombo->currentItem();
        if (idx >= 0 && idx < (int)confIds.size()) {
            confId = confIds[idx];
        }
#else
        // Qt4: 从 QVariant 获取存储的ID
        QVariant data = confCombo->itemData(confCombo->currentIndex());
        confId = data.toInt();
#endif
        if (confId == -1) { return; }
        bool success = ToxAPI::inviteToConferenceSync(friendId, confId);
        if (success) {
            QMessageBox::information(this, _("invite_success"), _("invite_success"));
        } else {
            QMessageBox::warning(this, _("invite_failed"), _("invite_failed"));
        }
    }
}

void MainWindow::onInviteToGroupRequested(int friendId) {
    // 获取群组列表
    std::vector<GroupInfo> groups = ToxAPI::getGroupsSync();
    
    if (groups.empty()) {
        QMessageBox::warning(this, _("no_group"), _("no_group"));
        return;
    }
    
    // 创建选择对话框
    QDialog dialog(this);
    qSetWindowTitle(&dialog, _("select_group"));
    dialog.resize(300, 150);
    
    QBoxLayout* layout = qNewBoxLayout(&dialog, QBoxLayout::TopToBottom, 10, 10);
    
    QLabel* label = new QLabel(_("select_group"), &dialog);
    layout->addWidget(label);
    
    QComboBox* groupCombo = new QComboBox(&dialog);
    // Qt3: 用单独的数组存储ID
    std::vector<int> groupIds;
    for (uint i = 0; i < groups.size(); ++i) {
        const auto& grp = groups[i];
        QString displayName;
        if (!grp.groupName.empty()) {
            displayName = qFromUtf8(grp.groupName);
        } else {
            displayName = QString(_("group_item")) + " " + QString::number(grp.groupNumber);
        }
#ifdef QT3_BUILD
        groupCombo->insertItem(displayName);
        groupIds.push_back(grp.groupNumber);
#else
        groupCombo->addItem(displayName, QVariant(grp.groupNumber));
#endif
    }
    layout->addWidget(groupCombo);
    
    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    btnLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
    
    QPushButton* inviteBtn = new QPushButton(_("buttons.add"), &dialog);
    connect(inviteBtn, SIGNAL(clicked()), &dialog, SLOT(accept()));
    btnLayout->addWidget(inviteBtn);
    
    QPushButton* cancelBtn = new QPushButton(_("buttons.cancel"), &dialog);
    connect(cancelBtn, SIGNAL(clicked()), &dialog, SLOT(reject()));
    btnLayout->addWidget(cancelBtn);
    
    layout->addLayout(btnLayout);
    
    if (dialog.exec() == QDialog::Accepted) {
        int groupId = -1;
#ifdef QT3_BUILD
        // Qt3: 从单独的ID数组获取
        int idx = groupCombo->currentItem();
        if (idx >= 0 && idx < (int)groupIds.size()) {
            groupId = groupIds[idx];
        }
#else
        // Qt4: 从 QVariant 获取存储的ID
        QVariant data = groupCombo->itemData(groupCombo->currentIndex());
        groupId = data.toInt();
#endif
        bool success = ToxAPI::inviteToGroupSync(friendId, groupId);
        if (success) {
            QMessageBox::information(this, _("invite_success"), _("invite_success"));
        } else {
            QMessageBox::warning(this, _("invite_failed"), _("invite_failed"));
        }
    }
}

void MainWindow::onGroupInviteReceived(int friendNumber, const QString& chatId) {
    GroupInviteDialog dialog(QString::number(friendNumber), chatId, this);
    if (dialog.exec() == QDialog::Accepted) {
        if (dialog.getResult() == GroupInviteDialog::Accept) {
            bool success = ToxAPI::joinGroupSync(friendNumber, qToUtf8(chatId).data(),
                                                  "", qToUtf8(dialog.getPassword()).data());
            if (success) {
                QMessageBox::information(this, _("group.joined"),
                                        _A("group.joined", QStringList() << chatId));
                chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
                ToxAPI::loadAllData();
            } else {
                QMessageBox::warning(this, _("group.join_failed"), _("group.join_failed"));
            }
        } else if (dialog.getResult() == GroupInviteDialog::Reject) {
            QMessageBox::information(this, _("group_rejected"), _("group_rejected"));
        }
    }
}

void MainWindow::clearLyricsHint() {
    m_hintActive = false;
    m_lyrics->setPlayedColor(m_savedPlayedColor);
    showClockOnLyrics();
}

void MainWindow::showClockOnLyrics()
{
    if (m_hintActive) return;

    static const char* weekDays[] = {
        "星期一","星期二","星期三","星期四","星期五","星期六","星期日"
    };
    time_t raw = time(0);
    struct tm* now = localtime(&raw);
    char tzBuf[16] = {};
    strftime(tzBuf, sizeof(tzBuf), "%z", now);
    QDate d = QDate::currentDate();
    QTime t = QTime::currentTime();
    QString ts = d.toString("yyyy-MM-dd") + " "
        + t.toString("HH:mm:ss") + " "
        + tzBuf + " "
        + qFromUtf8(weekDays[d.dayOfWeek() - 1]);
    m_lyrics->setLrcText("[00:00.000]" + ts);
    m_lyrics->setPosition(0);
}

void MainWindow::onSwitchAccount() {
    ToxAPI::stopPollEvent();

    LoginDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        ToxAPI::startPollEvent();
        chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
        ToxAPI::loadAllData();
        return;
    }

    ToxAPI::setBaseUrl(dialog.selectedUrl());
    ToxAPI::resetLastEventId();

    // 清空 UI 状态
    currentChatId = -1;
    currentChatType = "";
    chatWidget->setAttrForChat(QString(), -1);
    selfPubkey.clear();
    contactListWidget->clear();
    m_chatbuf = ChatBuffer();
#ifdef __linux__
    malloc_trim(0);
#endif
    chatWidget->setBuffer(nullptr);
    chatWidget->setHeaderText("");

    // 重新加载数据
    ToxAPI::startPollEvent();
    chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
    ToxAPI::loadAllData();
}

void MainWindow::renderHistoryMessages(const std::vector<HistoryMessage>& messages) {
    if (currentChatId == -1 || currentChatType.isEmpty()) return;
    std::string typeStr = std::string(qToUtf8(currentChatType).data());
    ChatHistory& hist = m_chatbuf.getOrCreate(currentChatId, typeStr);

    for (const auto& msg : messages) {
        bool isSelf  = (msg.sender_pubkey == selfPubkey);
        QString senderLabel;
        QString senderNickname;
        QString ipAddress;
        QString avatarUrl;
        
        if (isSelf) {
            senderLabel = "Me";
        } else {
            if (currentChatType == "friend") {
                std::string key = "friend_" + std::to_string(currentChatId);
                auto it = peerInfoMap.find(key);
                if (it == peerInfoMap.end()) {
                    PeerKey pk = parsePeerKey(key);
                    if (pk.valid) {
                        auto row = Storage::instance().channelDb()->get_chan_peer(
                            pk.chanid.c_str(), pk.peerNum);
                        it = loadRowToMap(peerInfoMap, key, std::move(row));
                    }
                }
                if (it != peerInfoMap.end()) {
                    senderLabel = qFromUtf8(it->second.userName);
                    if (!it->second.nickname.empty()) {
                        QString n = qFromUtf8(it->second.nickname);
                        senderNickname = n;
                    }
                    avatarUrl = qFromUtf8(it->second.iconUrl);
                }
            } else if (currentChatType == "unknown") {
                std::string key = "unknown_" + msg.sender_pubkey;
                auto it = peerInfoMap.find(key);
                if (it == peerInfoMap.end()) {
                    PeerKey pk = parsePeerKey(key);
                    if (pk.valid) {
                        auto row = Storage::instance().channelDb()->get_chan_peer(
                            pk.chanid.c_str(), pk.peerNum);
                        it = loadRowToMap(peerInfoMap, key, std::move(row));
                    }
                }
                if (it != peerInfoMap.end()) {
                    senderLabel = qFromUtf8(it->second.userName);
                    if (!it->second.nickname.empty()) {
                        QString n = qFromUtf8(it->second.nickname);
                        senderNickname = n;
                    }
                    ipAddress = qFromUtf8(it->second.peerIp);
                    avatarUrl = qFromUtf8(it->second.iconUrl);
                }
            } else {
                std::string key = std::string(qToUtf8(currentChatType).data())
                    + "_" + std::to_string(currentChatId)
                    + "_" + std::to_string(msg.sender_number);
                auto it = peerInfoMap.find(key);
                if (it == peerInfoMap.end()) {
                    PeerKey pk = parsePeerKey(key);
                    if (pk.valid) {
                        auto row = Storage::instance().channelDb()->get_chan_peer(
                            pk.chanid.c_str(), pk.peerNum);
                        it = loadRowToMap(peerInfoMap, key, std::move(row));
                    }
                }
                if (it != peerInfoMap.end()) {
                    senderLabel = qFromUtf8(it->second.userName);
                    if (!it->second.nickname.empty()) {
                        QString n = qFromUtf8(it->second.nickname);
                        senderNickname = n;
                    }
                    ipAddress = qFromUtf8(it->second.peerIp);
                    avatarUrl = qFromUtf8(it->second.iconUrl);
                }
            }
        }
        
        QString timeStr = qFormatTime(qFromUtf8(msg.created_at));
        
        ChatElement el;
        el.messageText    = qFromUtf8(msg.message);
        el.category       = isSelf ? "self" : "other";
        el.senderName     = senderLabel;
        el.senderNickname = senderNickname;
        el.peerNumber     = isSelf ? -1 : (currentChatType == "friend" ? currentChatId : (int)msg.sender_number);
        el.senderPubkey   = qFromUtf8(msg.sender_pubkey);
        el.time           = timeStr;
        el.avatarUrl      = avatarUrl;
        el.ipAddress      = ipAddress;
        el.messageId      = qFromUtf8(msg.eventId);

        for (const auto& s : msg.relatesTos)
            el.relatesTos.append(qFromUtf8(s));
        for (const auto& s : msg.mentions)
            el.mentions.append(qFromUtf8(s));

        if (!msg.mediaUrl.empty()) {
            el.mediaUrl    = qFromUtf8(msg.mediaUrl);
            el.mediaWidth  = msg.mediaWidth;
            el.mediaHeight = msg.mediaHeight;
            el.fileSize    = msg.fileSize;
        }

        if (msg.msgtype == "image") {
            bool isGif = (msg.mediaMime.find("image/gif") != std::string::npos)
                       || isGifUrl(msg.mediaUrl);
            el.etype = isGif ? ChatElement::Gif : ChatElement::Image;
        } else if (msg.msgtype == "video") {
            el.etype       = ChatElement::Video;
            el.durationSec = msg.duration / 1000;
        } else if (msg.msgtype == "audio") {
            el.etype       = ChatElement::Audio;
            el.durationSec = msg.duration / 1000;
        } else if (msg.msgtype == "file") {
            el.etype = ChatElement::File;
        }

        m_chatbuf.append(currentChatId, typeStr, el);
        db_writeMessage(currentChatId, typeStr, el);
    }
    chatWidget->setBuffer(&hist);
}

void MainWindow::loadMessageHistory() {
    if (currentChatId == -1 || currentChatType.isEmpty()) {
        qWarning("loadMessageHistory: no contact selected");
        return;
    }
    
    ToxAPI::getMessagesHistory(currentChatId, std::string(qToUtf8(currentChatType).data()));
}

void MainWindow::onTranslateRequested(int msgIndex, const QString& text, const QString& targetLang) {
    ToxAPI::translate(std::string(qToUtf8(text)), std::string(qToUtf8(targetLang)),
                      msgIndex, currentChatId,
                      std::string(qToUtf8(currentChatType)));
}

void MainWindow::onTranslateForSendRequested(const QString& text, const QString& targetLang) {
    ToxAPI::translateForSend(std::string(qToUtf8(text)), std::string(qToUtf8(targetLang)));
}

void MainWindow::onRetryClicked(int msgIndex, const QString& mediaUrl, const QString& /*source*/) {
    if (msgIndex < 0 || msgIndex >= chatWidget->messageCount()) { return; }
    if (currentChatId == 0 || currentChatId < -10000) { return; }

    std::string mxc = std::string(qToUtf8(mediaUrl).data());
    std::string typeStr = std::string(qToUtf8(currentChatType).data());
    int realIdx = -1;
    ChatElement* elp = resolveMediaElement(m_chatbuf.ptr(currentChatId, typeStr),
                                           mxc, msgIndex, &realIdx);
    if (!elp) { return; }
    if (realIdx != msgIndex) { msgIndex = realIdx; }
    ChatElement& el = *elp;
    if (el.downloadState == ChatElement::InProgress) { return; }   // 已在下载，防重复
    QByteArray rawBytes = MediaShmemCache::inst().getThumb(mediaUrl);
    if (!rawBytes.isEmpty()) {
        el.scaledDisplay = decodeRawToThumb(rawBytes.data(), rawBytes.size(),
            el.mediaWidth, el.mediaHeight, chatWidget->width() * 70 / 100);
        el.downloadState = ChatElement::Completed;
        el.downloadProgress = 100;
        el.cachedWidth = -1;
        chatWidget->updateElement(msgIndex);
        if (el.pendingPlay) { scheduleMediaPlayback(msgIndex, true); }
        return;
    }
    auto dbData = Storage::instance().cacheDb()->loadMedia(
        mediaCacheKey("file", mediaUrl).c_str());
    if (!dbData.empty()) {
        MediaShmemCache::inst().putThumb(mediaUrl, (const char*)dbData.data(), dbData.size());
        el.scaledDisplay = decodeRawToThumb((const char*)dbData.data(), dbData.size(),
            el.mediaWidth, el.mediaHeight, chatWidget->width() * 70 / 100);
        el.downloadState = ChatElement::Completed;
        el.downloadProgress = 100;
        el.cachedWidth = -1;
        chatWidget->updateElement(msgIndex);
        if (el.pendingPlay) { scheduleMediaPlayback(msgIndex, true); }
        return;
    }

    chatWidget->updateElement(msgIndex);
    el.downloadState = ChatElement::InProgress;
    el.downloadProgress = 0;
    el.downloadSpeedBps = 0;
    ToxAPI::downloadMedia(currentChatId, std::string(qToUtf8(currentChatType).data()),
                          msgIndex, std::string(qToUtf8(mediaUrl).data()), el.fileSize);
}

void MainWindow::onResendMessage(int msgIndex) {
    if (msgIndex < 0 || msgIndex >= chatWidget->messageCount()) { return; }
    if (currentChatId == -1 || currentChatType.isEmpty()) { return; }
    QString msgText = chatWidget->messageAt(msgIndex).messageText;
    if (msgText.isEmpty()) { return; }
#ifdef USE_UNIFIED_SEND_API
    std::string type = std::string(qToUtf8(currentChatType).data());
    std::string idOverride;
    if (type == kGomuksRoomType || type == kMtxliteRoomType
        || type == kUnktoxConferenceType
        || type == kUnktoxFriendType || type == kUnktoxGroupType
        || type == kMisskeyType || type == kImapMailType) {
        for (const auto& cd : m_accumulatedContactData) {
            if (cd.id == currentChatId && cd.type == type) {
                idOverride = cd.chatId; break;
            }
        }
    }
    if (type == kBookmarkType) {
        handleBookmarkMessage(msgText);
    } else if (type == kAichatType) {
        handleAichatMessage(msgText);
    } else if (type == kPastebinType) {
        handlePastebinMessage(msgText);
    } else if (type == kTranslateType) {
        handleTranslateMessage(msgText);
    } else {
        int sendmsgseq = ToxAPI::sendMessage(currentChatId, type, std::string(qToUtf8(msgText)), idOverride);
        (void)sendmsgseq;
    }
#else
    ToxAPI::sendMessage(currentChatId, "friend", std::string(qToUtf8(msgText)));
#endif
}

void MainWindow::onRequestRedactMessage(int msgIndex) {
    if (msgIndex < 0 || msgIndex >= chatWidget->messageCount()) { return; }
    if (currentChatId == -1 || currentChatType.isEmpty()) { return; }
    const ChatElement& el = chatWidget->messageAt(msgIndex);
    // TEMP(临时): 全放开，回滚时取消注释。
    // if (el.category != "self" || el.messageId.isEmpty()) { return; }
    std::string type = std::string(qToUtf8(currentChatType).data());
    std::string msgId = std::string(qToUtf8(el.messageId).data());
    // 虚拟类型使用 chatId 字符串（如 gomuks room ID）而非 numeric contactId
    std::string idOverride;
    if (type == kGomuksRoomType || type == kMtxliteRoomType
        || type == kUnktoxConferenceType
        || type == kUnktoxFriendType || type == kUnktoxGroupType
        || type == kMisskeyType || type == kImapMailType) {
        for (const auto& cd : m_accumulatedContactData) {
            if (cd.id == currentChatId && cd.type == type) {
                idOverride = cd.chatId;
                break;
            }
        }
    }
    chatWidget->loadingBar()->showLoading(kLoadRedactMsg, _("redacting_message"));
    ToxAPI::redactMessage(currentChatId, type, msgId, idOverride);
}

void MainWindow::onOpenFullSizeImage(int msgIndex, const QString& mediaUrl) {
    if (msgIndex < 0 || msgIndex >= chatWidget->messageCount()) { return; }
    std::string key = mediaCacheKey("file", mediaUrl);
    Storage::instance().cacheDbAsync()->loadMedia(
        key,
        [this, msgIndex, mediaUrl](std::vector<uint8_t> data, std::string) {
            auto* ev = new DiskLoadEvent();
            ev->msgIndex = msgIndex;
            ev->mediaUrl = std::string(qToUtf8(mediaUrl).data());
            if (data.empty()) {
                ev->success = false;
                ev->errorInfo = "cache miss";
            } else {
                ev->success = true;
                ev->rawData = std::move(data);
            }
            QApplication::postEvent(this, ev);
        });
}

void MainWindow::onOpenMediaPlayer(int msgIndex) {
    if (msgIndex < 0 || msgIndex >= chatWidget->messageCount()) { return; }
    if (currentChatId < 0) { return; }

    ChatElement& el = chatWidget->mutableMessageAt(msgIndex);
    if (el.etype != ChatElement::Video && el.etype != ChatElement::Audio) { return; }

    if (el.pendingPlay) { return; }   // 防重入：已等待自动播放
    if (el.downloadState == ChatElement::InProgress) {
        el.pendingPlay = true;   // 下载完成后自动交给播放器
        return;
    }
    if (el.downloadState == ChatElement::Completed) {
        el.pendingPlay = true;
        scheduleMediaPlayback(msgIndex, true);
        return;
    }
    // 未下载/失败 → 触发下载 + 待播放
    el.pendingPlay = true;
    onRetryClicked(msgIndex, el.mediaUrl, "open_player");
}

void MainWindow::scheduleMediaPlayback(int msgIndex, bool pendingPlay,
                                       const std::vector<uint8_t>* preloaded,
                                       int chatIdOverride, const QString& chatTypeOverride) {
    if (msgIndex < 0) { return; }
    int chatId;
    QString chatType;
    const ChatElement* el = nullptr;
    if (chatIdOverride >= 0) {
        chatId = chatIdOverride;
        chatType = chatTypeOverride;
        ChatHistory* t = m_chatbuf.ptr(chatId, std::string(qToUtf8(chatType).data()));
        if (!t || msgIndex >= t->size()) { return; }
        el = &(*t)[msgIndex];
    } else {
        if (msgIndex >= chatWidget->messageCount() || currentChatId < 0) { return; }
        chatId = currentChatId;
        chatType = currentChatType;
        el = &chatWidget->mutableMessageAt(msgIndex);
    }
    if (el->etype != ChatElement::Video && el->etype != ChatElement::Audio
        && el->etype != ChatElement::Gif) { return; }

    std::string mediaUrl = std::string(qToUtf8(el->mediaUrl).data());
    int etype = (int)el->etype;

    if (preloaded) {
        // 已下载完成回调里有原始字节，直接后台处理
        std::vector<uint8_t> data = *preloaded;
        std::thread worker([this, msgIndex, pendingPlay, chatId, chatType, etype, mediaUrl, data]() {
            runMediaPostproc(msgIndex, pendingPlay, chatId,
                std::string(qToUtf8(chatType).data()), etype, mediaUrl, data);
        });
        worker.detach();
        return;
    }

    std::string key = mediaCacheKey("file", qFromUtf8(mediaUrl));
    Storage::instance().cacheDbAsync()->loadMedia(
        key,
        [this, msgIndex, pendingPlay, chatId, chatType, etype, mediaUrl](
            std::vector<uint8_t> data, std::string) {
            runMediaPostproc(msgIndex, pendingPlay, chatId,
                std::string(qToUtf8(chatType).data()), etype, mediaUrl, data);
        });
}

void MainWindow::runMediaPostproc(int msgIndex, bool pendingPlay,
                                  int chatId, const std::string& chatType, int etype,
                                  const std::string& mediaUrl,
                                  std::vector<uint8_t> data) {
    auto* ev = new MediaDownloadEvent();
    ev->chatId = chatId;
    ev->chatType = chatType;
    ev->msgIndex = msgIndex;
    ev->mxcUrl = mediaUrl;
    ev->pendingPlay = pendingPlay;
    ev->postprocDone = true;

    if (data.empty()) {
        ev->success = false;
        ev->errorInfo = "cache miss";
        QApplication::postEvent(this, ev);
        return;
    }

    // 实体化原文件（相同 key 已落盘则跳过，兼容 >1MB 文件下载时的 put_ref 路径）
    std::string base = Storage::instance().dataDir();
    std::string fkey = mediaCacheKey("file", qFromUtf8(mediaUrl));
    std::string fpath = base + "/" + makeCacheFsPath(fkey.c_str());
    if (::access(fpath.c_str(), F_OK) != 0) {
        if (!writeCacheFile(fpath, data.data(), data.size())) {
            ev->success = false;
            ev->errorInfo = "write file failed";
            QApplication::postEvent(this, ev);
            return;
        }
    }
    ev->localFile = fpath;

    // 视频/真GIF 后处理：短片→转GIF；长视频→本地抽帧预览；真GIF→实体即 gifPath
    if (etype == (int)ChatElement::Video) {
        QString out, err;
        int durationMs = 0;
        if (qRuncmdCaptureOuterr("ffprobe", QStringList()
                << "-v" << "error"
                << "-show_entries" << "format=duration"
                << "-of" << "default=noprint_wrappers=1:nokey=1"
                << qFromUtf8(fpath), &out) == 0) {
            durationMs = (int)(fromDurationStr(out) * 1000.0);
        }
        ev->durationSec = durationMs / 1000;
        if (durationMs >= 1000 && durationMs <= 3000 && isGifLikeCandidate(mediaUrl)) {
            ev->isShortGif = true;
            std::string urlId;
            urlIdPart(mediaUrl, &urlId);
            std::string gkey = "gifplay_" + urlId;
            std::string gpath = base + "/" + makeCacheFsPath(gkey.c_str());
            if (::access(gpath.c_str(), F_OK) == 0) {
                ev->gifFile = gpath;   // 转码产物存在，幂等跳过
            } else if (qRuncmdCaptureOuterr("ffmpeg", QStringList()
                        << "-y" << "-i" << qFromUtf8(fpath)
                        << "-vf" << "fps=12,scale='min(320,iw)':-2"
                        << "-f" << "gif"
                        << qFromUtf8(gpath), &err) == 0) {
                ev->gifFile = gpath;
            }
        } else {
            // 长视频（或非 webm/mp4 短片）：本地抽帧做预览；已缓存则跳过
            if (MediaShmemCache::inst().getThumb(thumbCacheKey(qFromUtf8(mediaUrl))).isEmpty()) {
                std::string urlId;
                urlIdPart(mediaUrl, &urlId);
                std::string tkey = "thumb_" + urlId;
                std::string tpath = base + "/" + makeCacheFsPath(tkey.c_str());
                int seekSec = (durationMs >= 1000) ? 1 : 0;
                if (extractVideoFrame(fpath, tpath, seekSec)) {
                    ev->thumbFile = tpath;
                }
            }
        }
    }

    ev->success = true;
    QApplication::postEvent(this, ev);
}

void MainWindow::handleMediaPostproc(MediaDownloadEvent* e) {
    if (!e->success) {
        qWarning("Media postproc failed: chat=%d/%s idx=%d err=%s",
                 e->chatId, e->chatType.c_str(), e->msgIndex, e->errorInfo.c_str());
        return;
    }
    ChatHistory* target = m_chatbuf.ptr(e->chatId, e->chatType);
    if (!target || e->msgIndex < 0 || e->msgIndex >= target->size()) { return; }
    ChatElement& el = (*target)[e->msgIndex];
    bool isCurrent = (e->chatId == currentChatId
                      && qFromUtf8(e->chatType) == currentChatType);
    if (e->durationSec > 0) { el.durationSec = e->durationSec; }
    if (e->isShortGif) { el.gifLikeVideo = true; }
    if (!qFromUtf8(e->gifFile).isEmpty()) {
        el.gifPath = qFromUtf8(e->gifFile);
    } else if (el.etype == ChatElement::Gif && !qFromUtf8(e->localFile).isEmpty()) {
        el.gifPath = qFromUtf8(e->localFile);   // 真 GIF：原文件即动画源
    }
    if (!e->thumbFile.empty()) {
        std::vector<uint8_t> thumb = readCacheFile(e->thumbFile);
        if (!thumb.empty()) {
            MediaShmemCache::inst().putThumb(thumbCacheKey(el.mediaUrl),
                (const char*)thumb.data(), (int)thumb.size());
            MediaShmemCache::inst().putThumb(el.mediaUrl,
                (const char*)thumb.data(), (int)thumb.size());
            el.scaledDisplay = decodeRawToThumb((const char*)thumb.data(), (int)thumb.size(),
                el.mediaWidth, el.mediaHeight, chatWidget->width() * 70 / 100);
            el.cachedWidth = -1;
        }
        removeCacheFile(e->thumbFile);
    }
    if (!el.mediaUrl.isEmpty()) {
        db_writeMessage(e->chatId, e->chatType, el);
    }
    if (isCurrent) {
        chatWidget->updateElement(e->msgIndex);
    }

    if (e->pendingPlay) {
        el.pendingPlay = false;
        launchPlayer(qFromUtf8(e->localFile), el.gifLikeVideo);
    }
}

void MainWindow::launchPlayer(const QString& filePath, bool gifLike) {
    if (filePath.isEmpty()) { return; }
    QStringList args;
    if (gifLike) { args << "--loop-file=inf" << "--no-audio"; }
    args << filePath;
#if !defined(Q_OS_MACX)
    args << "-vo=xv"; // TODO check x11?
#endif

    bool ok = false;
    if (QFile::exists("/usr/bin/mpv")) { ok = qStartProcessDetached("mpv", args); }
    if (!ok) { ok = qStartProcessDetached("mpv", args); }   // PATH 兜底
#if defined(Q_OS_MACX)
    if (!ok) {
        QStringList iargs;
        iargs << "-a" << "IINA" << filePath;
        ok = qStartProcessDetached("open", iargs);
    }
#endif
    if (!ok) {
        qWarning("launchPlayer: no player for [%s], fallback system default",
                 qToUtf8(filePath).data());
#ifdef QT3_BUILD
        qOpenUrl(QString("file://") + filePath);
#else
        qOpenUrl(QUrl::fromLocalFile(filePath).toString());
#endif
    }
}

void MainWindow::onFileSendRequested(const QString& filePath, const QString& caption) {
    if (currentChatId == -1 || currentChatType.isEmpty()) { return; }

    std::string fileType = std::string(qToUtf8(currentChatType).data());
    std::string fileIdOverride;
    if (fileType == kGomuksRoomType || fileType == kMtxliteRoomType
        || fileType == kUnktoxConferenceType
        || fileType == kUnktoxFriendType || fileType == kUnktoxGroupType
        || fileType == kMisskeyType || fileType == kImapMailType) {
        for (const auto& cd : m_accumulatedContactData) {
            if (cd.id == currentChatId && cd.type == fileType) {
                fileIdOverride = cd.chatId;
                break;
            }
        }
    }

    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) { return; }

#ifdef QT3_BUILD
    long sz = fi.size();
    const long kMaxFileSize = 5 * 1024 * 1024;
#else
    qint64 sz = fi.size();
    const qint64 kMaxFileSize = 5 * 1024 * 1024;
#endif
    if (sz > kMaxFileSize) {
        QMessageBox::warning(this, _("file_too_large"),
            _("file_size_limit").arg((int)(kMaxFileSize / 1024 / 1024)));
        return;
    }
    if (sz == 0) {
        QMessageBox::warning(this, _("file_empty"), _("file_empty"));
        return;
    }

    QFile f(filePath);
#ifdef QT3_BUILD
    if (!f.open(IO_ReadOnly)) { return; }
#else
    if (!f.open(QIODevice::ReadOnly)) { return; }
#endif
    QByteArray data = f.readAll();
    f.close();

    QString fn = filePath;
#ifdef QT3_BUILD
    int slash = fn.findRev('/');
#else
    int slash = fn.lastIndexOf('/');
#endif
    if (slash >= 0) fn = fn.mid(slash + 1);

    ChatElement el;
    el.etype = ChatElement::File;
    el.category = "self";
    el.senderName = "Me";
    el.time = getCurrentTime();
    el.localPath = filePath;
    el.fileName = fn;
    el.fileSize = (int)sz;
    el.sendState = ChatElement::SendSending;
    std::string typeStr = std::string(qToUtf8(currentChatType).data());
    {
        m_chatbuf.append(currentChatId, typeStr, el);
        db_writeMessage(currentChatId, typeStr, el);
    }
    int sendmsgseq = ToxAPI::sendMessage(currentChatId, fileType,
                         std::string(qToUtf8(caption).data()), fileIdOverride,
                         std::string(data.data(), data.size()),
                         std::string(qToUtf8(fn)));
    if (sendmsgseq > 0) {
        ChatHistory& hist = m_chatbuf.getOrCreate(currentChatId, typeStr);
        if (!hist.empty()) {
            hist.back().sendmsgseq = sendmsgseq;
        }
    }
}

void MainWindow::onFavoriteClicked(int msgIndex) {
    if (msgIndex < 0 || msgIndex >= chatWidget->messageCount()) { return; }
    if (currentChatId < 0) { return; }
    const ChatElement& el = chatWidget->messageAt(msgIndex);
    int64_t rowid = el.dbRowid;
    if (rowid <= 0) {
        qWarning("favorite: rowid not ready, skip chat=%d idx=%d", currentChatId, msgIndex);
        return;
    }
    std::string chanid = std::string(qToUtf8(currentChatType).data())
                       + "_" + std::to_string(currentChatId);
    if (chatWidget->isFavRowid(rowid)) {
        chatWidget->setFavRowid(rowid, false);
        Storage::instance().messageDbAsync()->remove_bookmark(
            rowid, [rowid](bool ok) { qWarning("favorite remove rowid=%lld ok=%d", (long long)rowid, ok); });
    } else {
        chatWidget->setFavRowid(rowid, true);
        Storage::instance().messageDbAsync()->add_bookmark(
            rowid, chanid, "", [rowid](bool ok) { qWarning("favorite add rowid=%lld ok=%d", (long long)rowid, ok); });
    }
}

void MainWindow::onChatScreenshotRequested() {
#if defined(Q_OS_MAC) || defined(Q_OS_MACX) || defined(Q_OS_DARWIN)
    QString shotPath = QDir::tempPath()
        + QString("/qltox_shot_%1.png").arg(QString::number(QDateTime::currentMSecsSinceEpoch()));
    bool ok = qStartProcessDetached(QString::fromLatin1("screencapture"),
                                    QStringList() << "-i" << shotPath);
    if (!ok) {
        qWarning("onChatScreenshotRequested: screencapture 启动失败，回退 open -a Screenshot");
        qStartProcessDetached(QString::fromLatin1("open"), QStringList() << "-a" << "Screenshot");
    }
#else
    if (!qStartProcessDetached(QString::fromLatin1("xfce4-screenshooter"), QStringList() << "-r")) {
        qWarning("onChatScreenshotRequested: xfce4-screenshooter 启动失败");
    }
#endif
}

void MainWindow::onSourceClicked(int msgIndex) {
    if (msgIndex < 0 || msgIndex >= chatWidget->messageCount()) { return; }

    ChatElement msg = chatWidget->messageAt(msgIndex);
    QDialog* dlg = new QDialog(this);
    dlg->resize(650, 520);
    qSetWindowTitle(dlg, qFromUtf8("Source"));
    QBoxLayout* lay = qNewBoxLayout(dlg, QBoxLayout::TopToBottom, 0, 4);
    qSetMargins(lay, 4, 4, 4, 4);

    JsonViewWidget* jv = new JsonViewWidget(dlg);
    jv->setJson(msg.messageText);
    lay->addWidget(jv, 1);

    QPushButton* closeBtn = new QPushButton(_("buttons.close"), dlg);
    connect(closeBtn, SIGNAL(clicked()), dlg, SLOT(accept()));
    lay->addWidget(closeBtn, 0, Qt::AlignRight);

#ifdef QT3_BUILD
    // QDialog::finished(int) 在 Qt3 不存在；dialog 是 MainWindow 子对象，自动清理
#else
    QObject::connect(dlg, SIGNAL(finished(int)), dlg, SLOT(deleteLater()));
#endif
    dlg->show();
}

void MainWindow::openSettings() {
    ConfigDialog* dlg = new ConfigDialog(qFromUtf8("设置"), this);
    dlg->setSettingsFile(ConfigDialog::defaultSettingsFile());

    CategoryPage* general = new CategoryPage(qFromUtf8("常规"), dlg);
    StringConfigItem* serverUrl = new StringConfigItem(
        "server/baseUrl", "http://localhost:8181",
        qFromUtf8("服务器地址"), general);
    general->addLabeledControl(serverUrl->label(), serverUrl->lineEdit());
    general->addStretch();
    dlg->addCategory(qFromUtf8("常规"), general);
    dlg->registerConfigItem(serverUrl);

    CategoryPage* appearance = new CategoryPage(qFromUtf8("界面"), dlg);
    QStringList langs;
    langs << qFromUtf8("zh-CN") << qFromUtf8("zh-TW") << qFromUtf8("en-US");
    SelectConfigItem* langItem = new SelectConfigItem(
        "lang/code", "zh-CN",
        qFromUtf8("语言"), langs, appearance);
    appearance->addLabeledControl(langItem->label(), langItem->comboBox());
    appearance->addStretch();
    dlg->addCategory(qFromUtf8("界面"), appearance);
    dlg->registerConfigItem(langItem);

    CategoryPage* notify = new CategoryPage(qFromUtf8("通知"), dlg);
    BoolConfigItem* msgNotif = new BoolConfigItem(
        "notify/enabled", true,
        qFromUtf8("消息通知"), notify);
    notify->addWidget(msgNotif->checkBox());
    BoolConfigItem* soundNotif = new BoolConfigItem(
        "notify/sound", true,
        qFromUtf8("声音"), notify);
    notify->addWidget(soundNotif->checkBox());
    notify->addStretch();
    dlg->addCategory(qFromUtf8("通知"), notify);
    dlg->registerConfigItem(msgNotif);
    dlg->registerConfigItem(soundNotif);

    CategoryPage* chat = new CategoryPage(qFromUtf8("聊天"), dlg);
    IntConfigItem* pageSize = new IntConfigItem(
        "chat/pageSize", 200,
        qFromUtf8("每页消息数"), 50, 500, chat);
    chat->addLabeledControl(pageSize->label(), pageSize->spinBox());
    chat->addStretch();
    dlg->addCategory(qFromUtf8("聊天"), chat);
    dlg->registerConfigItem(pageSize);

    // --- 网络 ---
    CategoryPage* network = new CategoryPage(qFromUtf8("网络"), dlg);
    IntConfigItem* timeout = new IntConfigItem(
        "server/timeout", 30,
        qFromUtf8("服务器超时(秒)"), 5, 120, network);
    network->addLabeledControl(timeout->label(), timeout->spinBox());
    IntConfigItem* retryInterval = new IntConfigItem(
        "server/retryInterval", 5,
        qFromUtf8("连接重试间隔(秒)"), 1, 60, network);
    network->addLabeledControl(retryInterval->label(), retryInterval->spinBox());
    IntConfigItem* httpParallel = new IntConfigItem(
        "http_request_parral", 1,
        qFromUtf8("HTTP请求并行数"), 1, 5, network);
    network->addLabeledControl(httpParallel->label(), httpParallel->spinBox());
    network->addStretch();
    dlg->addCategory(qFromUtf8("网络"), network);
    dlg->registerConfigItem(timeout);
    dlg->registerConfigItem(retryInterval);
    dlg->registerConfigItem(httpParallel);

    // --- 其他 ---
    CategoryPage* other = new CategoryPage(qFromUtf8("其他"), dlg);
    QStringList logLevels;
    logLevels << qFromUtf8("debug") << qFromUtf8("info")
              << qFromUtf8("warning") << qFromUtf8("error");
    SelectConfigItem* logLevel = new SelectConfigItem(
        "log/level", "info",
        qFromUtf8("日志级别"), logLevels, other);
    other->addLabeledControl(logLevel->label(), logLevel->comboBox());
    BoolConfigItem* autoConnect = new BoolConfigItem(
        "startup/autoConnect", true,
        qFromUtf8("启动时自动连接"), other);
    other->addWidget(autoConnect->checkBox());
    BoolConfigItem* sortbylastmsg = new BoolConfigItem(
        "sortbylastmsg", false,
        qFromUtf8("联系人排序最新消息优先"), other);
    other->addWidget(sortbylastmsg->checkBox());
    QStringList toLangs;
    toLangs << qFromUtf8("zh-CN") << qFromUtf8("zh-TW") << qFromUtf8("en-US");
    SelectConfigItem* translateToLang = new SelectConfigItem(
        "translate/tolang", "zh-CN",
        qFromUtf8("翻译目标语言"), toLangs, other);
    other->addLabeledControl(translateToLang->label(), translateToLang->comboBox());
    IntConfigItem* fontSize = new IntConfigItem(
        "chat/fontSize", 14,
        qFromUtf8("消息字体大小"), 8, 32, other);
    other->addLabeledControl(fontSize->label(), fontSize->spinBox());

    // ── 系统行为 ──
    BoolConfigItem* sleepBlocker = new BoolConfigItem(
        "sleepblocker/enabled", true,
        qFromUtf8("阻止屏保/锁屏"), other);
    other->addWidget(sleepBlocker->checkBox());

    // ── 富媒体显示 ──
    BoolConfigItem* dispImg = new BoolConfigItem(
        "media/displayImage", true,
        qFromUtf8("显示图片"), other);
    other->addWidget(dispImg->checkBox());
    BoolConfigItem* dispFile = new BoolConfigItem(
        "media/displayFile", false,
        qFromUtf8("显示文件"), other);
    other->addWidget(dispFile->checkBox());
    BoolConfigItem* dispGif = new BoolConfigItem(
        "media/displayGif", true,
        qFromUtf8("显示GIF"), other);
    other->addWidget(dispGif->checkBox());
    BoolConfigItem* dispVid = new BoolConfigItem(
        "media/displayVideo", false,
        qFromUtf8("显示视频"), other);
    other->addWidget(dispVid->checkBox());

    // ── 富媒体自动下载 ──
    BoolConfigItem* dlImg = new BoolConfigItem(
        "media/downloadImage", true,
        qFromUtf8("自动下载图片"), other);
    other->addWidget(dlImg->checkBox());
    BoolConfigItem* dlFile = new BoolConfigItem(
        "media/downloadFile", false,
        qFromUtf8("自动下载文件"), other);
    other->addWidget(dlFile->checkBox());
    BoolConfigItem* dlGif = new BoolConfigItem(
        "media/downloadGif", true,
        qFromUtf8("自动下载GIF"), other);
    other->addWidget(dlGif->checkBox());
    BoolConfigItem* dlVid = new BoolConfigItem(
        "media/downloadVideo", false,
        qFromUtf8("自动下载视频"), other);
    other->addWidget(dlVid->checkBox());

    other->addStretch();
    dlg->addCategory(qFromUtf8("其他"), other);
    dlg->registerConfigItem(logLevel);
    dlg->registerConfigItem(autoConnect);
    dlg->registerConfigItem(sortbylastmsg);
    dlg->registerConfigItem(translateToLang);
    dlg->registerConfigItem(fontSize);
    dlg->registerConfigItem(dispImg);
    dlg->registerConfigItem(dispFile);
    dlg->registerConfigItem(dispGif);
    dlg->registerConfigItem(dispVid);
    dlg->registerConfigItem(dlImg);
    dlg->registerConfigItem(dlFile);
    dlg->registerConfigItem(dlGif);
    dlg->registerConfigItem(dlVid);
    dlg->registerConfigItem(sleepBlocker);

    connect(dlg, SIGNAL(settingsSaved(SettingsChangedMap)), this, SLOT(onSettingsSaved(SettingsChangedMap)));
    dlg->loadSettings();
    dlg->show();
}

void MainWindow::onSettingsSaved(const SettingsChangedMap& changed) {
    if (changed.isEmpty()) { return; }
    qWarning("Settings changed (%d keys):", changed.size());
    for (auto it = changed.begin(); it != changed.end(); ++it) {
#ifdef QT3_BUILD
        qWarning("  %s => %s", (const char*)it.key().utf8(),
                 (const char*)it.data().toString().utf8());
#else
        qWarning("  %s => %s", qPrintable(it.key()),
                 qPrintable(it.value().toString()));
#endif
    }
    if (changed.contains("sleepblocker/enabled")) {
#ifdef QT3_BUILD
        bool enabled = (changed["sleepblocker/enabled"].toString() == "true");
#else
        bool enabled = (changed.value("sleepblocker/enabled").toString() == "true");
#endif
        if (enabled && !m_sleepBlocker) {
            m_sleepBlocker = new SleepBlocker();
        } else if (!enabled && m_sleepBlocker) {
            delete m_sleepBlocker;
            m_sleepBlocker = nullptr;
        }
    }
}

void MainWindow::openStickerManager() {
    StickerManager* mgr = new StickerManager(_("stickermanager.title"), nullptr);
    StickerDbSyncInterface* db = Storage::instance().stickerDb();
    if (db) { mgr->setStickerDb(db); }
    mgr->loadData();
    mgr->show();
}

void MainWindow::openStatistics() {
    StatisticsDialog* dlg = new StatisticsDialog(this);
    dlg->show();
}

void MainWindow::openCombineSearch() {
    CombineSearch* dlg = new CombineSearch(this);
    dlg->show();
}

void MainWindow::onScreenshotRequested() {
    // Default to region capture (most common).
    // Options: captureFullScreen(), captureActiveWindow(), captureRegion()
    m_screenshotMgr->captureRegion();
}

void MainWindow::onScreenshotReady(const QString& filePath) {
    // 弹出预览对话框，用户确认后发送到当前聊天
    ScreenshotPreviewDialog* dlg = new ScreenshotPreviewDialog(filePath, this);
    connect(dlg, SIGNAL(sendRequested(const QString&)),
            this, SLOT(onFileSendRequested(const QString&)));
    connect(dlg, SIGNAL(cancelled()), dlg, SLOT(close()));
    dlg->show();
}

void MainWindow::onScreenshotCancelled() {
    // 截图取消 — 静默忽略
}

void MainWindow::onMenu1Stub() {
    qWarning("onMenu1Stub: not implemented yet");
}

void MainWindow::onMenu2Stub() {
    qWarning("onMenu2Stub: not implemented yet");
}

void MainWindow::onAboutApp() {
    QString text = QString(
        "<h3>qltox %1</h3>"
        "<p>A lightweight Tox client based on doxhttpd.</p>"
        "<p>Qt: %2 | Build: %3</p>"
        "<p><a href='https://github.com/envsh/doxhttpd'>https://github.com/envsh/doxhttpd</a></p>"
    ).arg(APP_VERSION_FULL).arg(qVersion()).arg(__DATE__);
    QMessageBox::about(this, "About qldox", text);
}

void MainWindow::openHomePage() {
    qOpenUrl("https://github.com/envsh/doxhttpd");
}

void MainWindow::onEtappActivated(int index) {
#ifdef QT3_BUILD
    QMap<int, int>::const_iterator it = m_etappItemToIndex.find(index);
    if (it == m_etappItemToIndex.end()) return;
    int pluginIndex = it.data();
#else
    int pluginIndex = index;
#endif
    if (pluginIndex >= 0 && pluginIndex < PluginLoader::uiappCount()) {
        PluginLoader::createUiApp(pluginIndex, 0);
    }
}

void MainWindow::onEtappCloseAll() {
    PluginLoader::closeAllUiApps();
}

void MainWindow::openPluginManager() {
    PluginManagerDialog* dlg = new PluginManagerDialog(this);
    dlg->show();
}

void MainWindow::openMdEditor() {
    MdEditor* editor = new MdEditor(0);
    editor->loadSampleContent();
    editor->show();
}
