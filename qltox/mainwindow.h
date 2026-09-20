#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <vector>
#include <utility>
#include "compat34.h"
#include <qmainwindow.h>
#include <qsplitter.h>
#include <qwidget.h>
#include "selfinfo.h"
#include "contactlist.h"
#include "chatwidget.h"
#include "chatbuffer.h"
#include <qtimer.h>
#include "desktoplyrics.h"
#include "restapi.h"
#include "translator.h"
#include "FramelessHelper.h"
#include "friendinfodialog.h"
#include "screenshotmanager.h"
#include "screenshotpreview.h"
#include "memberlistdialog.h"
#include "ConfigDialog.h"
#include "sleepblocker.h"
#include "systemtrayicon.h"
#include "plugin_loader.h"
#include "EmbeddedMenuBar.h"
#include "mdeditor.h"
#include <thread>
#include <vector>
#ifdef QT3_BUILD
#include <qmap.h>
#else
#include <QMap>
#endif

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    
    void customEvent(CustomEventBase* event);
    
protected:
    bool event(QEvent* event) override;
    // 托盘可见时拦截关闭 → 隐藏到托盘；无托盘或强制退出时正常关闭
    void closeEvent(QCloseEvent* event) override;
protected slots:
    void onFirstPaintComplete();
    void initLoadPlugins();
    
    public slots:
    void onContactSelected(int id, const QString& type, const QString& name);
    void onMessageSending(const QString& message, const QMap<QString,QString>& context);
    void onFileSendRequested(const QString& filePath, const QString& caption);
    void onLanguageChanged(const QString& langCode);
    void retranslateUi();
    void handleEvents(const EventList& events);
    void onViewInfoRequested(int id, const QString& type);
    void onChatPeerInfoRequested(int peerNumber, const QString& senderName);
    void onDeleteOrLeaveRequested(int id, const QString& type);
    void onInviteToConferenceRequested(int friendId);
    void onInviteToGroupRequested(int friendId);
    void onGroupInviteReceived(int friendNumber, const QString& chatId);
    void onViewMembersRequested(int id, const QString& type);
    void onRenameNickRequested(int groupId, const QString& groupName);
    void onSetGroupTopicRequested(int groupId);
    void onSetConferenceTitleRequested(int conferenceId);
    void onSwitchAccount();
    void clearLyricsHint();
    void showClockOnLyrics();
    void loadMessageHistory();
    void onTranslateRequested(int msgIndex, const QString& text, const QString& targetLang);
    void onTranslateForSendRequested(const QString& text, const QString& targetLang);
    void onSourceClicked(int msgIndex);
    void onRetryClicked(int msgIndex, const QString& mediaUrl, const QString& source);
    void onResendMessage(int msgIndex);
    void onRequestRedactMessage(int msgIndex);
    void onOpenFullSizeImage(int msgIndex, const QString& mediaUrl);
    void onOpenMediaPlayer(int msgIndex);
    void onFavoriteClicked(int msgIndex);
    void onChatScreenshotRequested();
    void renderHistoryMessages(const std::vector<HistoryMessage>& messages);
    void openSettings();
    void openStickerManager();
    void openStatistics();
    void openCombineSearch();
    void onMenu1Stub();
    void onMenu2Stub();
    void onAboutApp();
    void onScreenshotRequested();
    void onScreenshotReady(const QString& filePath);
    void onScreenshotCancelled();
    void openHomePage();
    void onSettingsSaved(const SettingsChangedMap& changed);
    void onEtappActivated(int index);
    void onEtappCloseAll();
    void openPluginManager();
    void openMdEditor();
    void quitApp();                 // 真正退出应用（绕过 closeEvent 托盘拦截）
    void trayShowMainWindow();      // 从托盘恢复并激活主窗口
    void trayActivated(int reason); // 托盘点击：Trigger/DoubleClick 恢复窗口
    void updateTrayBadge(int total);
    
private:
    // 媒体本机播放：实体化缓存字节 → 探测/转码（worker） → 播放
    void scheduleMediaPlayback(int msgIndex, bool pendingPlay,
                               const std::vector<uint8_t>* preloaded = nullptr,
                               int chatIdOverride = -1,
                               const QString& chatTypeOverride = QString());
    void runMediaPostproc(int msgIndex, bool pendingPlay,
                          int chatId, const std::string& chatType, int etype,
                          const std::string& mediaUrl,
                          std::vector<uint8_t> data);
    void launchPlayer(const QString& filePath, bool gifLike);
    void handleMediaPostproc(MediaDownloadEvent* e);
    FramelessHelper* framelessHelper;
    std::string selfPubkey;  // 自己的公钥（地址前64字符）
    std::map<std::string, PeerInfo> peerInfoMap;  // 会议/群组 peer info 缓存: "conf_N_M" / "group_N_M"
    
    QSplitter* splitter;
    QWidget* sidebarWidget;  // 左侧边栏容器
    SelfInfoWidget* selfInfoWidget;
    ContactListWidget* contactListWidget;
    ChatWidget* chatWidget;
    
    int currentChatId;
    QString currentChatType;

    // loading ID 常量
    static const int kLoadAll = 1;
    static const int kLoadFriend = 2;
    static const int kLoadMessages = 3;
    static const int kLoadMembers = 4;
    static const int kLoadSendMsg = 5;
    static const int kLoadRedactMsg = 6;

    bool m_firstPaintLogged = false;
    int m_paintCounter = 0;

    // 增量累积的 contacts（值类型，避免指针所有权混乱）
    std::vector<ContactData> m_accumulatedContactData;

    // 消息缓存：每个联系人的 ChatHistory，ChatBuffer 统一管理
    ChatBuffer m_chatbuf;

    ScreenshotManager* m_screenshotMgr;
    DesktopLyrics* m_lyrics;
    QTimer* m_msgTimer;
    QTimer* m_clockTimer;
    bool m_hintActive;
    QColor m_savedPlayedColor;
    SleepBlocker* m_sleepBlocker;
    MenuWidget34* m_etappsMenu;
    SystemTrayIcon* m_tray;   // 系统托盘（关闭时最小化到托盘）
    bool m_forceQuit;         // 退出菜单/托盘退出置 true，绕过 closeEvent 拦截

#ifdef QT3_BUILD
    QMap<int, int> m_etappItemToIndex;
#endif
};

#endif // MAINWINDOW_H
