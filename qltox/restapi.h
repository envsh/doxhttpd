#ifndef API_H
#define API_H

#include <string>
#include <vector>
#include <cstdint>
#include "eventpoller.h"
#ifdef QT3_BUILD
#include <qmap.h>
#else
#include <QMap>
#endif

struct FriendInfo {
    int id;
    std::string name;
    std::string statusStr;
    std::string userStatus;
    std::string statusText;
    std::string iconUrl;
    std::string publicKey;
    std::string peerIp;
    uint64_t lastSeen = 0;
};

FriendInfo friendInfoFromPeer(const PeerInfo& peer, int id);

struct GroupInfo {
    int groupNumber = 0;
    std::string groupName;
    std::string chatId;
    bool isConnected = false;
    std::string statusText;
    int memberCount = 0;
};

struct ConferenceInfo {
    int conferenceNumber = 0;
    std::string conferenceName;
    std::string chatId;
    bool isConnected = false;
    std::string statusText;
    int memberCount = 0;
};

class ToxAPI {
public:
    static void setEventTarget(QObject* target);
    static void setBaseUrl(const std::string& url);
    static void resetLastEventId();

    static void startPollEvent();
    static void stopPollEvent();
    static void loadAllData();
    static bool onLoadAllDataComplete();

    static void getSelf();
    static void getFriends();
    // sendMessage 扩展字段 context：落入 POST body 表单字段，不进入 URL。
    // key 必须在白名单内（见 restapi.cpp ctxAllowedKeys）：relates_to / mentions / visibility。
    // 多值用逗号拼接；白名单外的 key 在 sendMessage 遍历核对时记日志并跳过。
    static int  sendMessage(int chatId, const std::string& type, const std::string& message,
                             const std::string& idOverride = "",
                             const std::string& fileData = "",
                             const std::string& filename = "",
                             const QMap<QString,QString>& context = QMap<QString,QString>());
    static int  sendFriendMessage(int friendId, const std::string& message);
    static int  sendConferenceMessage(int conferenceId, const std::string& message);
    static int  sendGroupMessage(int groupId, const std::string& message);
    static int  redactMessage(int chatId, const std::string& type, const std::string& messageId,
                              const std::string& idOverride = "");
    static void addFriend(const std::string& publicKey);
    static void deleteFriend(int friendId);
    static void getGroupMembers(int groupId);
    static void getConferenceMembers(int confId);
    static void getMessagesHistory(int contactId, const std::string& contactType);
    static void joinConference(int friendNumber, const std::string& cookie);
    static void rejectConference(int friendNumber);
    static void ignoreConference(int friendNumber);
    static void createConference();
    static void leaveConference(int confId);
    static void inviteToConference(int friendId, int confId);
    static void createGroup(const std::string& groupName, const std::string& creatorName,
                           const std::string& password = "", bool isPrivate = false);
    static void leaveGroup(int groupId);
    static void inviteToGroup(int friendId, int groupId);
    static void joinGroup(int friendNumber, const std::string& chatId,
                         const std::string& name = "", const std::string& password = "");
    static void joinGroupByChatId(const std::string& chatId,
                                  const std::string& name = "", const std::string& password = "");
    static void setGroupSelfName(int groupId, const std::string& name);
    static void getRandomName();
    static void setSelfInfo(const std::string& name, const std::string& statusMessage);
    static void translate(const std::string& text, const std::string& toLang, int msgIndex, int chatId, const std::string& chatType);
    static void translateForSend(const std::string& text, const std::string& toLang);
    static void lazyLoadFriendDetail(int friendId);
    static std::string urlEncode(const std::string& str);

    // Sync helper methods for dialog contexts
    static std::vector<GroupInfo> getGroupsSync();
    static std::vector<ConferenceInfo> getConferencesSync();
    static std::vector<PeerInfo> getConferenceMembersSync(int confId);
    static std::vector<PeerInfo> getGroupMembersSync(int groupId);
    static std::string getRandomNameSync();
    static int addFriendSync(const std::string& publicKey);
    static int createConferenceSync();
    static int createGroupSync(const std::string& groupName, const std::string& creatorName,
                               const std::string& password = "", bool isPrivate = false);
    static bool deleteFriendSync(int friendId);
    static bool leaveConferenceSync(int confId);
    static bool leaveGroupSync(int groupId);
    static bool setGroupTopicSync(int groupId, const std::string& topic);
    static bool setConferenceTitleSync(int conferenceId, const std::string& title);
    static void setGroupTopic(int groupId, const std::string& topic);
    static void setConferenceTitle(int conferenceId, const std::string& title);
    static void downloadMedia(int chatId, const std::string& chatType, int msgIndex, const std::string& mxcUrl);
    static void downloadAvatar(const std::string& mxcUrl);
    static bool setSelfInfoSync(const std::string& name, const std::string& statusMessage);
    static bool joinGroupSync(int friendNumber, const std::string& chatId,
                              const std::string& name = "", const std::string& password = "");
    static bool inviteToConferenceSync(int friendId, int confId);
    static bool inviteToGroupSync(int friendId, int groupId);
    static bool setGroupSelfNameSync(int groupId, const std::string& name);
    static bool joinGroupByChatIdSync(const std::string& chatId,
                                      const std::string& name = "", const std::string& password = "");

private:
    struct ApiCtx {
        int type;
        int id = 0;
        int sendmsgseq = 0;
        std::string str1;
        std::string str2;
        int n1 = 0;
        int step = 0;
        ApiCtx() {}
        ApiCtx(int t) : type(t) {}
        ApiCtx(int t, int i) : type(t), id(i) {}
        ApiCtx(int t, int i, const std::string& s) : type(t), id(i), str1(s) {}
        ApiCtx(int t, int i, const std::string& s1, const std::string& s2) : type(t), id(i), str1(s1), str2(s2) {}
        ApiCtx(int t, int i, const std::string& s1, const std::string& s2, int n) : type(t), id(i), str1(s1), str2(s2), n1(n) {}
        int chatId = -1;
        std::string chatType;
        // TODO: 以上两个 ad-hoc 字段应替换为通用机制：
        // 在 ApiCtx 和 ApiResultEvent 基类各加 QVariant cbNum/cbStr，
        // dispatchResult 自动复制，customEvent 用 cbNum.toInt()/cbStr.toString() 做过期检查。
        // 适用场景：ApiTranslate、ApiLoadGroupMembers、ApiLoadMessageHistory、ApiMediaDownload。
        void* ptr = nullptr;
    };
    static void onHttpDone(const HttpResponse& resp, void* udata);
    static void dispatchResult(ApiCtx* ctx, const HttpResponse& resp);
    static std::string buildUrl(const std::string& endpoint);
    static void pollEvents();
    static void request(ApiRequestType type, const HttpRequest& req);
    static void request(const HttpRequest& req, ApiCtx* ctx);
    static bool syncRequest(const std::string& endpoint,
                            const std::string& method,
                            std::string& outBody,
                            const std::string& data = "",
                            int timeoutSec = 35);
    static void mediaProgressCb(long long received, long long total, void* udata);

    static QObject* s_target;
    static std::string s_baseUrl;
    static uint64_t s_lastEventId;
    static int s_sendMsgSeq;
    static bool s_pollRunning;
    static bool s_loadingAllData;
    static bool s_reloadPending;
};

#endif
