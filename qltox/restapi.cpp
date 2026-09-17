#include "restapi.h"
#include "limelog.h"
#include "cJSON.h"
#include <curl/curl.h>
#include <cstdlib>
#include <cctype>
#include <chrono>
#include <unistd.h>
#include <qapplication.h>
#include <sstream>
#include "seen_unknown.h"

// ── Static members ──
QObject* ToxAPI::s_target = nullptr;
std::string ToxAPI::s_baseUrl = "http://localhost:8181";
uint64_t ToxAPI::s_lastEventId = 0;
int ToxAPI::s_sendMsgSeq = 100;
bool ToxAPI::s_pollRunning = false;
bool ToxAPI::s_loadingAllData = false;
bool ToxAPI::s_reloadPending = false;
static bool s_useNdjson = true; // true=auto s/ Content-Type 分派; false=强制旧 JSON 数组
static const char* kEventTopic = "topic=reddit,hacknews,twitter,universal-connectivity";
static SeenUnknown s_seenUnknown;

// ── Helpers ──

// ── REST 响应错误提取 ──
// 优先 curl 错误；其次按候选 key 循环解析服务器错误字段，字符串直接取，
// 对象（如 {"error":{message,detail}}）下钻内部 message/detail；HTTP 码始终保留。
static std::string extractRespError(const HttpResponse& resp) {
    if (!resp.curlErrStr.empty())
        return resp.curlErrStr;
    std::string serverErr;
    cJSON* r = cJSON_Parse(resp.body.c_str());
    if (r) {
        static const char* kErrorKeys[] = {
            "error", "errmsg", "errMsg", "message", "detail", "title", "msg"
        };
        for (size_t i = 0; i < sizeof(kErrorKeys) / sizeof(kErrorKeys[0]) && serverErr.empty(); i++) {
            cJSON* e = cJSON_GetObjectItem(r, kErrorKeys[i]);
            if (!e) { continue; }
            if (cJSON_IsString(e)) {
                serverErr = cJSON_GetStringValue(e);
            } else if (cJSON_IsObject(e)) {
                cJSON* sub = cJSON_GetObjectItem(e, "message");
                if (sub && cJSON_IsString(sub)) {
                    serverErr = cJSON_GetStringValue(sub);
                } else {
                    sub = cJSON_GetObjectItem(e, "detail");
                    if (sub && cJSON_IsString(sub))
                        serverErr = cJSON_GetStringValue(sub);
                }
            }
        }
        cJSON_Delete(r);
    }
    if (!serverErr.empty())
        return "HTTP " + std::to_string(resp.httpCode) + ": " + serverErr;
    if (resp.httpCode != 200)
        return "HTTP " + std::to_string(resp.httpCode);
    return "unknown error";
}

static std::string jsonStr(cJSON* item) {
    return (item && cJSON_IsString(item)) ? std::string(cJSON_GetStringValue(item)) : "";
}

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// ── Sync helper write callback ──
static size_t syncWriteCb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

// ── LoadAllData chain ──
static const size_t maxBatchSize = 3;
struct LoadChain {
    AllDataLoadedEvent* result;
    int step = 0;
    std::vector<int> friendIds;
    size_t detailBatchIdx = 0;
    LoadChain() : result(new AllDataLoadedEvent()) {}
};

// ── Private ──

// ── 事件解析器 ──

static bool parseEventsJson(const std::string& body, uint64_t& lastId, std::vector<Event>& events) {
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) {
        const char* errPos = cJSON_GetErrorPtr();
        int offset = errPos ? (int)(errPos - body.c_str()) : -1;
        ALOG_WARN("parseEventsJson: cJSON error at offset %d: %.160s", offset, body.c_str() + (offset >= 0 ? offset : 0));
        return false;
    }
    if (!cJSON_IsArray(root)) {
        ALOG_WARN("parseEventsJson: not an array");
        cJSON_Delete(root);
        return false;
    }
    int count = cJSON_GetArraySize(root);
    for (int i = 0; i < count; ++i) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (!item) { continue; }
        Event e;
        cJSON* v = cJSON_GetObjectItem(item, "event_id");
        e.id = v ? (uint64_t)v->valuedouble : 0;
        e.type = jsonStr(cJSON_GetObjectItem(item, "event_type"));
        e.data = jsonStr(cJSON_GetObjectItem(item, "data"));
        e.timestamp = jsonStr(cJSON_GetObjectItem(item, "timestamp"));
        if (e.id == 0 || e.type.empty()) {
            char* raw = cJSON_PrintUnformatted(item);
            std::string lineStr = raw ? std::string(raw) : "{}";
            free(raw);
            static SeenUnknown s_seenUnknownJson;
            if (s_seenUnknownJson.insert(lineStr)) {
                Event synth;
                synth.id = 0;
                synth.type = "unknown";
                synth.data = lineStr;
                synth.timestamp = "";
                events.push_back(synth);
                ALOG_WARN("parseEventsJson: wrapped unparsed item as unknown");
            }
            continue;
        }
        events.push_back(e);
        if (e.id > lastId) { lastId = e.id; }
    }
    cJSON_Delete(root);
    return true;
}

static int parseEventsNdjson(const std::string& body, uint64_t& lastId, std::vector<Event>& events) {
    if (body.empty()) return 0;
    int errors = 0;
    size_t total = 0;
    std::istringstream stream(body);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        ++total;
        cJSON* item = cJSON_Parse(line.c_str());
        if (!item) {
            const char* errPos = cJSON_GetErrorPtr();
            int offset = errPos ? (int)(errPos - line.c_str()) : -1;
            ALOG_WARN("parseEventsNdjson: cJSON error at offset", offset, "line:", line);
            ++errors;
            continue;
        }
        Event e;
        cJSON* v = cJSON_GetObjectItem(item, "event_id");
        e.id = v ? (uint64_t)v->valuedouble : 0;
        e.type = jsonStr(cJSON_GetObjectItem(item, "event_type"));
        e.data = jsonStr(cJSON_GetObjectItem(item, "data"));
        e.timestamp = jsonStr(cJSON_GetObjectItem(item, "timestamp"));
        if (e.id == 0 || e.type.empty()) {
            if (s_seenUnknown.insert(line)) {
                Event synth;
                synth.id = 0;
                synth.type = "unknown";
                synth.data = line;
                synth.timestamp = "";
                events.push_back(synth);
                ALOG_WARN("parseEventsNdjson: wrapped unparsed line as unknown");
            }
            cJSON_Delete(item);
            continue;
        }
        events.push_back(e);
        if (e.id > lastId) { lastId = e.id; }
        cJSON_Delete(item);
    }
    return errors;
}

void ToxAPI::request(const HttpRequest& req, ApiCtx* ctx) {
    HttpRequest r = req;
    r.url = buildUrl(req.url);
    EventPoller::addRequest(r, onHttpDone, ctx);
}

void ToxAPI::request(ApiRequestType type, const HttpRequest& req) {
    request(req, new ApiCtx(type));
}

void ToxAPI::onHttpDone(const HttpResponse& resp, void* udata) {
    auto* ctx = static_cast<ApiCtx*>(udata);
    dispatchResult(ctx, resp);
    delete ctx;
}

// ── Public API ──

void ToxAPI::setEventTarget(QObject* target) { s_target = target; }
void ToxAPI::setBaseUrl(const std::string& url) { s_baseUrl = url; }

void ToxAPI::resetLastEventId() { s_lastEventId = 0; }

void ToxAPI::startPollEvent() {
    s_pollRunning = true;
    s_lastEventId = 0;
    pollEvents();
}

void ToxAPI::stopPollEvent() {
    s_pollRunning = false;
}

void ToxAPI::pollEvents() {
    EventPoller::addRequest(
        {buildUrl("/api/events?after=" + std::to_string(s_lastEventId) + "&" + kEventTopic),
         "GET", "", 35, {{"Accept", "application/x-ndjson"}}},
        onHttpDone, new ApiCtx(ApiPollEvents));
}

void ToxAPI::loadAllData() {
    if (s_loadingAllData) {
        s_reloadPending = true;
        return;
    }
    s_loadingAllData = true;
    auto* chain = new LoadChain();
    auto* ctx = new ApiCtx(ApiLoadAllData);
    ctx->ptr = chain;
    EventPoller::addRequest({buildUrl("/api/self"), "GET", "", 35}, onHttpDone, ctx);
}

bool ToxAPI::onLoadAllDataComplete() {
    s_loadingAllData = false;
    if (s_reloadPending) {
        s_reloadPending = false;
        return true;
    }
    return false;
}

void ToxAPI::getSelf() {
    request(ApiGetSelf, {"/api/self", "GET"});
}

void ToxAPI::getFriends() {
    request(ApiGetFriends, {"/api/friends", "GET"});
}

int ToxAPI::sendFriendMessage(int friendId, const std::string& message) {
    auto* ctx = new ApiCtx(ApiSendFriendMessage, friendId, message);
    ctx->sendmsgseq = ++s_sendMsgSeq;
    request({"/api/messages", "POST",
            "friend_id=" + std::to_string(friendId) + "&message=" + urlEncode(message), 90}, ctx);
    return ctx->sendmsgseq;
}

int ToxAPI::sendConferenceMessage(int conferenceId, const std::string& message) {
    auto* ctx = new ApiCtx(ApiSendConferenceMessage, conferenceId, message);
    ctx->sendmsgseq = ++s_sendMsgSeq;
    request({"/api/conference_messages", "POST",
            "conference_id=" + std::to_string(conferenceId) + "&message=" + urlEncode(message), 90}, ctx);
    return ctx->sendmsgseq;
}

int ToxAPI::sendGroupMessage(int groupId, const std::string& message) {
    auto* ctx = new ApiCtx(ApiSendGroupMessage, groupId, message);
    ctx->sendmsgseq = ++s_sendMsgSeq;
    request({"/api/group_messages", "POST",
            "group_number=" + std::to_string(groupId) + "&message=" + urlEncode(message), 90}, ctx);
    return ctx->sendmsgseq;
}

int ToxAPI::redactMessage(int chatId, const std::string& type, const std::string& messageId,
                          const std::string& idOverride) {
    std::string idStr = idOverride.empty() ? std::to_string(chatId) : idOverride;
    ALOG_INFO("redact request >>", "type=", type, "id=", idStr, "message_id=", messageId);
    auto* ctx = new ApiCtx(ApiRedactMessage, chatId, messageId, type);
    request({"/api/messages/redact", "POST",
            "type=" + type + "&id=" + urlEncode(idStr)
            + "&message_id=" + urlEncode(messageId), 90}, ctx);
    return 0;
}

// 可上送的 context key 白名单（扩展新字段时在 map 中 insert 一个 key 即可）
static const QMap<QString, bool>& ctxAllowedKeys() {
    static QMap<QString, bool> m;
    static bool initialized = false;
    if (!initialized) {
        m.insert("relates_to",   true); // not reply_to
        m.insert("mentions",   true);
        m.insert("visibility", true);
        initialized = true;
    }
    return m;
}

int ToxAPI::sendMessage(int chatId, const std::string& type, const std::string& message,
                          const std::string& idOverride,
                          const std::string& fileData,
                          const std::string& filename,
                          const QMap<QString,QString>& context) {
    auto* ctx = new ApiCtx(ApiSendMessage, chatId, message, type);
    ctx->sendmsgseq = ++s_sendMsgSeq;
    std::string idStr = idOverride.empty() ? std::to_string(chatId) : idOverride;

    // 遍历核对：白名单 key（ctxAllowedKeys）才上送，未知 key 记日志并忽略
    std::string extraFields;
    for (QMap<QString,QString>::const_iterator it = context.begin(); it != context.end(); ++it) {
        if (!ctxAllowedKeys().contains(it.key())) {
            qWarning("sendMessage: 忽略未知 context key: %s", qToUtf8(it.key()).data());
            continue;
        }
#ifdef QT3_BUILD
        const QString v = it.data();
#else
        const QString v = it.value();
#endif
        if (v.isEmpty()) continue;
        extraFields += "&" + urlEncode(std::string(qToUtf8(it.key()).data()))
                     + "=" + urlEncode(std::string(qToUtf8(v).data()));
    }

    if (fileData.empty()) {
        request({"/api/messages/send", "POST",
                "type=" + type + "&id=" + urlEncode(idStr)
                + "&message=" + urlEncode(message) + extraFields, 90}, ctx);
    } else {
        std::string boundary = "----toxhttpd" + std::to_string(time(nullptr));
        std::string body;
        auto af = [&](const std::string& n, const std::string& v) {
            body += "--" + boundary + "\r\n"
                    "Content-Disposition: form-data; name=\"" + n + "\"\r\n\r\n" + v + "\r\n";
        };
        af("type", type);
        af("id", idStr);
        af("message", message);
        af("filename", filename);
        for (QMap<QString,QString>::const_iterator it = context.begin(); it != context.end(); ++it) {
            if (!ctxAllowedKeys().contains(it.key())) {   // multipart 分支同样走 QMap 核对
                qWarning("sendMessage: 忽略未知 context key: %s", qToUtf8(it.key()).data());
                continue;
            }
#ifdef QT3_BUILD
            const QString v = it.data();
#else
            const QString v = it.value();
#endif
            if (v.isEmpty()) continue;
            af(std::string(qToUtf8(it.key()).data()), std::string(qToUtf8(v).data()));
        }
        body += "--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n"
                "Content-Type: application/octet-stream\r\n\r\n";
        body += fileData;
        body += "\r\n--" + boundary + "--\r\n";
        request({"/api/messages/send", "POST", body, 90,
            {{"Content-Type", "multipart/form-data; boundary=" + boundary}}}, ctx);
    }
    return ctx->sendmsgseq;
}

void ToxAPI::addFriend(const std::string& publicKey) {
    request(ApiAddFriend, {"/api/friends", "POST",
            "public_key=" + urlEncode(publicKey)});
}

void ToxAPI::deleteFriend(int friendId) {
    request(ApiDeleteFriend, {"/api/friend_delete", "POST",
            "friend_id=" + std::to_string(friendId)});
}

void ToxAPI::getGroupMembers(int groupId) {
    auto* ctx = new ApiCtx(ApiLoadGroupMembers, groupId, "group");
    request({"/api/group/members?group_number="
            + std::to_string(groupId), "GET", ""}, ctx);
}

void ToxAPI::getConferenceMembers(int confId) {
    auto* ctx = new ApiCtx(ApiLoadGroupMembers, confId, "conference");
    request({"/api/conference/members?conference_id="
            + std::to_string(confId), "GET", ""}, ctx);
}

void ToxAPI::getMessagesHistory(int contactId, const std::string& contactType) {
    auto* ctx = new ApiCtx(ApiLoadMessageHistory, contactId, contactType);
    request({"/api/messages/history?contact_id=" + std::to_string(contactId)
            + "&contact_type=" + contactType, "GET", ""}, ctx);
}

// 媒体下载进度回调（poller 线程）：转发为 MediaDownloadProgressEvent
void ToxAPI::mediaProgressCb(long long received, long long total,
                             long long speedBps, void* udata) {
    auto* ctx = static_cast<ApiCtx*>(udata);
    if (!ctx || !s_target) { return; }
    auto* ev = new MediaDownloadProgressEvent();
    ev->chatId   = ctx->id;
    ev->chatType = ctx->str1;
    ev->msgIndex = ctx->n1;
    ev->received = received;
    ev->total    = total;
    ev->speedBps = speedBps;
    QApplication::postEvent(s_target, ev);
}

void ToxAPI::downloadMedia(int chatId, const std::string& chatType, int msgIndex,
                           const std::string& mxcUrl, int fileSize) {
    auto* ctx = new ApiCtx(ApiMediaDownload, chatId, chatType, mxcUrl, msgIndex);
    // 大文件放宽硬超时（按 50KB/s 估算，下限 90s），配停滞看门狗防止永久卡住
    int timeoutSec = 120;
    if (fileSize > 0) {
        int est = fileSize / 51200 + 1;
        if (est < 90) { est = 90; }
        timeoutSec = est;
    }
    HttpRequest req("/api/media_download?url=" + urlEncode(mxcUrl),
                    "GET", "", timeoutSec, {}, 25);
    req.progress = mediaProgressCb;
    request(req, ctx);
}

void ToxAPI::downloadAvatar(const std::string& mxcUrl) {
    auto* ctx = new ApiCtx(ApiAvatarDownload);
    ctx->str2 = mxcUrl;
    request({"/api/media_download?url=" + urlEncode(mxcUrl), "GET"}, ctx);
}

void ToxAPI::joinConference(int friendNumber, const std::string& cookie) {
    request(ApiJoinConference, {"/api/conferences/join", "POST",
            "friend_number=" + std::to_string(friendNumber) + "&cookie=" + cookie});
}

void ToxAPI::rejectConference(int friendNumber) {
    request(ApiRejectConference, {"/api/conferences/reject", "POST",
            "friend_number=" + std::to_string(friendNumber)});
}

void ToxAPI::ignoreConference(int friendNumber) {
    request(ApiIgnoreConference, {"/api/conferences/ignore", "POST",
            "friend_number=" + std::to_string(friendNumber)});
}

void ToxAPI::createConference() {
    request(ApiCreateConference, {"/api/conferences", "POST"});
}

void ToxAPI::leaveConference(int confId) {
    request(ApiLeaveConference, {"/api/conferences/leave", "POST",
            "conference_id=" + std::to_string(confId)});
}

void ToxAPI::inviteToConference(int friendId, int confId) {
    request(ApiInviteToConference, {"/api/conference_invite", "POST",
            "friend_id=" + std::to_string(friendId) + "&conference_id=" + std::to_string(confId)});
}

void ToxAPI::createGroup(const std::string& groupName, const std::string& creatorName,
                          const std::string& password, bool isPrivate) {
    std::string data = "group_name=" + urlEncode(groupName) + "&name=" + urlEncode(creatorName);
    if (!password.empty()) data += "&password=" + urlEncode(password);
    if (isPrivate) { data += "&privacy_state=private"; }
    request(ApiCreateGroup, {"/api/groups", "POST", data});
}

void ToxAPI::leaveGroup(int groupId) {
    request(ApiLeaveGroup, {"/api/groups/leave", "POST",
            "group_number=" + std::to_string(groupId)});
}

void ToxAPI::inviteToGroup(int friendId, int groupId) {
    request(ApiInviteToGroup, {"/api/groups/invite", "POST",
            "friend_id=" + std::to_string(friendId) + "&group_id=" + std::to_string(groupId)});
}

void ToxAPI::joinGroup(int friendNumber, const std::string& chatId,
                        const std::string& name, const std::string& password) {
    std::string data = "friend_number=" + std::to_string(friendNumber) + "&chat_id=" + chatId;
    if (!name.empty()) data += "&name=" + urlEncode(name);
    if (!password.empty()) data += "&password=" + urlEncode(password);
    request(ApiJoinGroup, {"/api/groups/join", "POST", data});
}

void ToxAPI::joinGroupByChatId(const std::string& chatId,
                                const std::string& name, const std::string& password) {
    std::string data = "chat_id=" + chatId;
    if (!name.empty()) data += "&name=" + urlEncode(name);
    if (!password.empty()) data += "&password=" + urlEncode(password);
    request(ApiJoinGroupByChatId, {"/api/groups/join", "POST", data});
}

void ToxAPI::setGroupSelfName(int groupId, const std::string& name) {
    request(ApiSetGroupSelfName, {"/api/groups/set-self-name", "POST",
            "group_number=" + std::to_string(groupId) + "&name=" + urlEncode(name)});
}

void ToxAPI::setGroupTopic(int groupId, const std::string& topic) {
    request(ApiSetGroupTopic, {"/api/groups/set-topic", "POST",
            "group_number=" + std::to_string(groupId) + "&topic=" + urlEncode(topic)});
}

void ToxAPI::setConferenceTitle(int conferenceId, const std::string& title) {
    request(ApiSetConferenceTitle, {"/api/conferences/set-title", "POST",
            "conference_id=" + std::to_string(conferenceId) + "&title=" + urlEncode(title)});
}

void ToxAPI::getRandomName() {
    request(ApiGetRandomName, {"/api/random-name", "GET"});
}

void ToxAPI::setSelfInfo(const std::string& name, const std::string& statusMessage) {
    std::string data;
    bool hasParams = false;
    if (!name.empty()) {
        data += "name=" + urlEncode(name);
        hasParams = true;
    }
    if (!statusMessage.empty()) {
        if (hasParams) { data += "&"; }
        data += "status_message=" + urlEncode(statusMessage);
        hasParams = true;
    }
    if (!hasParams) { return; }
    request(ApiSetSelfInfo, {"/api/self", "POST", data});
}

void ToxAPI::translate(const std::string& text, const std::string& toLang, int msgIndex, int chatId, const std::string& chatType) {
    auto* ctx = new ApiCtx(ApiTranslate, msgIndex, text, toLang);
    ctx->chatId = chatId;
    ctx->chatType = chatType;
    std::string postData = "{\"text\":\"" + jsonEscape(text) + "\",\"to\":\"" + toLang + "\"}";
    request({"/api/translate", "POST", postData}, ctx);
}

void ToxAPI::translateForSend(const std::string& text, const std::string& toLang) {
    auto* ctx = new ApiCtx(ApiTranslateForSend);
    ctx->str1 = text;
    ctx->str2 = toLang;
    std::string postData = "{\"text\":\"" + jsonEscape(text) + "\",\"to\":\"" + toLang + "\"}";
    request({"/api/translate", "POST", postData}, ctx);
}

void ToxAPI::lazyLoadFriendDetail(int friendId) {
    auto* ctx = new ApiCtx(ApiLoadFriendDetail, friendId);
    request({"/api/friend", "POST",
            "friend_ids=" + std::to_string(friendId)}, ctx);
}

std::string ToxAPI::urlEncode(const std::string& str) {
    CURL* curl = curl_easy_init();
    if (!curl) { return str; }
    char* encoded = curl_easy_escape(curl, str.c_str(), (int)str.length());
    std::string result = encoded ? encoded : str;
    if (encoded) { curl_free(encoded); }
    curl_easy_cleanup(curl);
    return result;
}

std::string ToxAPI::buildUrl(const std::string& endpoint) {
    return s_baseUrl + endpoint;
}

// ── Sync helpers ──

bool ToxAPI::syncRequest(const std::string& endpoint,
                          const std::string& method,
                          std::string& outBody,
                          const std::string& data,
                          int timeoutSec) {
    CURL* curl = curl_easy_init();
    if (!curl) { return false; }
    std::string url = buildUrl(endpoint);
    ALOG_INFO(">>", method, url);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, syncWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outBody);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeoutSec);
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)data.size());
    }
    auto syncStart = std::chrono::steady_clock::now();
    CURLcode res = curl_easy_perform(curl);
    auto syncEnd = std::chrono::steady_clock::now();
    auto syncElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(syncEnd - syncStart).count();
    long httpCode = 0;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    }
    ALOG_INFO("<<", httpCode, url, syncElapsedMs, "ms", outBody.size(), "bytes");
    curl_easy_cleanup(curl);
    return (httpCode == 200 && !outBody.empty());
}

FriendInfo friendInfoFromPeer(const PeerInfo& peer, int id) {
    FriendInfo info;
    info.id = id;
    info.name = peer.name;
    info.statusStr = peer.statusStr;
    info.statusText = peer.statusText;
    info.userStatus = peer.userStatus;
    info.iconUrl = peer.iconUrl;
    info.publicKey = peer.publicKey;
    info.peerIp = peer.peerIp;
    info.lastSeen = peer.lastSeen;
    return info;
}

std::vector<GroupInfo> ToxAPI::getGroupsSync() {
    std::vector<GroupInfo> groups;
    std::string body;
    if (!syncRequest("/api/groups", "GET", body))
        return groups;
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) { return groups; }
    cJSON* arr = cJSON_GetObjectItem(root, "groups");
    if (arr && cJSON_IsArray(arr)) {
        int n = cJSON_GetArraySize(arr);
        for (int i = 0; i < n; ++i) {
            cJSON* item = cJSON_GetArrayItem(arr, i);
            if (!item) { continue; }
            GroupInfo g;
            cJSON* v = cJSON_GetObjectItem(item, "groupNumber");
            if (v) { g.groupNumber = v->valueint; }
            g.groupName = jsonStr(cJSON_GetObjectItem(item, "groupName"));
            g.chatId = jsonStr(cJSON_GetObjectItem(item, "chatId"));
            v = cJSON_GetObjectItem(item, "isConnected");
            g.isConnected = v ? (v->valueint == 1) : false;
            g.statusText = jsonStr(cJSON_GetObjectItem(item, "statusText"));
            cJSON* mc = cJSON_GetObjectItem(item, "memberCount");
            if (mc) { g.memberCount = mc->valueint; }
            groups.push_back(g);
        }
    }
    cJSON_Delete(root);
    return groups;
}

std::vector<ConferenceInfo> ToxAPI::getConferencesSync() {
    std::vector<ConferenceInfo> conferences;
    std::string body;
    if (!syncRequest("/api/conferences", "GET", body))
        return conferences;
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) { return conferences; }
    cJSON* arr = cJSON_GetObjectItem(root, "conferences");
    if (arr && cJSON_IsArray(arr)) {
        int n = cJSON_GetArraySize(arr);
        for (int i = 0; i < n; ++i) {
            cJSON* item = cJSON_GetArrayItem(arr, i);
            if (!item) { continue; }
            ConferenceInfo c;
            cJSON* v = cJSON_GetObjectItem(item, "conferenceNumber");
            if (v) { c.conferenceNumber = v->valueint; }
            c.conferenceName = jsonStr(cJSON_GetObjectItem(item, "conferenceName"));
            c.chatId = jsonStr(cJSON_GetObjectItem(item, "chatId"));
            v = cJSON_GetObjectItem(item, "isConnected");
            c.isConnected = v ? (v->valueint == 1) : false;
            c.statusText = jsonStr(cJSON_GetObjectItem(item, "statusText"));
            cJSON* mc = cJSON_GetObjectItem(item, "memberCount");
            if (mc) { c.memberCount = mc->valueint; }
            conferences.push_back(c);
        }
    }
    cJSON_Delete(root);
    return conferences;
}

static std::vector<PeerInfo> parseMembersResponse(const std::string& body) {
    std::vector<PeerInfo> members;
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) { return members; }
    cJSON* selfPpItem = cJSON_GetObjectItem(root, "selfPeerNumber");
    int selfPeerNumber = selfPpItem ? selfPpItem->valueint : -1;
    cJSON* membersItem = cJSON_GetObjectItem(root, "members");
    if (membersItem && cJSON_IsArray(membersItem)) {
        int n = cJSON_GetArraySize(membersItem);
        for (int i = 0; i < n; ++i) {
            cJSON* item = cJSON_GetArrayItem(membersItem, i);
            if (!item) { continue; }
            PeerInfo info;
            cJSON* v;
            v = cJSON_GetObjectItem(item, "peerNumber");
            if (v) { info.peerNumber = v->valueint; }
            info.name = jsonStr(cJSON_GetObjectItem(item, "name"));
            v = cJSON_GetObjectItem(item, "status");
            if (v) { info.status = v->valueint; }
            info.statusStr = jsonStr(cJSON_GetObjectItem(item, "statusStr"));
            info.statusText = jsonStr(cJSON_GetObjectItem(item, "statusText"));
            info.iconUrl = jsonStr(cJSON_GetObjectItem(item, "iconUrl"));
            v = cJSON_GetObjectItem(item, "role");
            if (v) { info.role = v->valueint; }
            info.roleStr = jsonStr(cJSON_GetObjectItem(item, "roleStr"));
            info.publicKey = jsonStr(cJSON_GetObjectItem(item, "publicKey"));
            info.peerIp = jsonStr(cJSON_GetObjectItem(item, "peerIp"));
            info.isSelf = (info.peerNumber == selfPeerNumber);
            members.push_back(info);
        }
    }
    cJSON_Delete(root);
    return members;
}

std::vector<PeerInfo> ToxAPI::getConferenceMembersSync(int confId) {
    std::string body;
    if (!syncRequest("/api/conference/members?conference_id=" + std::to_string(confId), "GET", body))
        return std::vector<PeerInfo>();
    return parseMembersResponse(body);
}

std::vector<PeerInfo> ToxAPI::getGroupMembersSync(int groupId) {
    std::string body;
    if (!syncRequest("/api/group/members?group_number=" + std::to_string(groupId), "GET", body))
        return std::vector<PeerInfo>();
    return parseMembersResponse(body);
}

std::string ToxAPI::getRandomNameSync() {
    std::string body;
    if (!syncRequest("/api/random-name", "GET", body))
        return "";
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) { return ""; }
    std::string name = jsonStr(cJSON_GetObjectItem(root, "name"));
    cJSON_Delete(root);
    return name;
}

int ToxAPI::addFriendSync(const std::string& publicKey) {
    std::string body;
    if (!syncRequest("/api/friends", "POST", body,
                     "public_key=" + urlEncode(publicKey)))
        return -1;
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) { return -1; }
    cJSON* v = cJSON_GetObjectItem(root, "friend_id");
    int id = v ? v->valueint : -1;
    cJSON_Delete(root);
    return id;
}

int ToxAPI::createConferenceSync() {
    std::string body;
    if (!syncRequest("/api/conferences", "POST", body))
        return -1;
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) { return -1; }
    cJSON* v = cJSON_GetObjectItem(root, "conference_id");
    int id = v ? v->valueint : -1;
    cJSON_Delete(root);
    return id;
}

int ToxAPI::createGroupSync(const std::string& groupName, const std::string& creatorName,
                             const std::string& password, bool isPrivate) {
    std::string data = "group_name=" + urlEncode(groupName) + "&name=" + urlEncode(creatorName);
    if (!password.empty()) data += "&password=" + urlEncode(password);
    if (isPrivate) { data += "&privacy_state=private"; }
    std::string body;
    if (!syncRequest("/api/groups", "POST", body, data))
        return -1;
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) { return -1; }
    cJSON* v = cJSON_GetObjectItem(root, "group_number");
    int id = v ? v->valueint : -1;
    cJSON_Delete(root);
    return id;
}

bool ToxAPI::deleteFriendSync(int friendId) {
    std::string body;
    return syncRequest("/api/friend_delete", "POST", body,
                       "friend_id=" + std::to_string(friendId));
}

bool ToxAPI::leaveConferenceSync(int confId) {
    std::string body;
    return syncRequest("/api/conferences/leave", "POST", body,
                       "conference_id=" + std::to_string(confId));
}

bool ToxAPI::leaveGroupSync(int groupId) {
    std::string body;
    return syncRequest("/api/groups/leave", "POST", body,
                       "group_number=" + std::to_string(groupId));
}

bool ToxAPI::setSelfInfoSync(const std::string& name, const std::string& statusMessage) {
    std::string data;
    bool hasParams = false;
    if (!name.empty()) {
        data += "name=" + urlEncode(name);
        hasParams = true;
    }
    if (!statusMessage.empty()) {
        if (hasParams) { data += "&"; }
        data += "status_message=" + urlEncode(statusMessage);
        hasParams = true;
    }
    if (!hasParams) { return true; }
    std::string body;
    return syncRequest("/api/self", "POST", body, data);
}

bool ToxAPI::joinGroupSync(int friendNumber, const std::string& chatId,
                            const std::string& name, const std::string& password) {
    std::string data = "friend_number=" + std::to_string(friendNumber) + "&chat_id=" + chatId;
    if (!name.empty()) data += "&name=" + urlEncode(name);
    if (!password.empty()) data += "&password=" + urlEncode(password);
    std::string body;
    return syncRequest("/api/groups/join", "POST", body, data);
}

bool ToxAPI::inviteToConferenceSync(int friendId, int confId) {
    std::string body;
    return syncRequest("/api/conference_invite", "POST", body,
                       "friend_id=" + std::to_string(friendId)
                       + "&conference_id=" + std::to_string(confId));
}

bool ToxAPI::inviteToGroupSync(int friendId, int groupId) {
    std::string body;
    return syncRequest("/api/groups/invite", "POST", body,
                       "friend_id=" + std::to_string(friendId)
                       + "&group_id=" + std::to_string(groupId));
}

bool ToxAPI::setGroupSelfNameSync(int groupId, const std::string& name) {
    std::string body;
    return syncRequest("/api/groups/set-self-name", "POST", body,
                       "group_number=" + std::to_string(groupId)
                       + "&name=" + urlEncode(name));
}

bool ToxAPI::setGroupTopicSync(int groupId, const std::string& topic) {
    std::string body;
    return syncRequest("/api/groups/set-topic", "POST", body,
                       "group_number=" + std::to_string(groupId)
                       + "&topic=" + urlEncode(topic));
}

bool ToxAPI::setConferenceTitleSync(int conferenceId, const std::string& title) {
    std::string body;
    return syncRequest("/api/conferences/set-title", "POST", body,
                       "conference_id=" + std::to_string(conferenceId)
                       + "&title=" + urlEncode(title));
}

bool ToxAPI::joinGroupByChatIdSync(const std::string& chatId,
                                    const std::string& name, const std::string& password) {
    std::string data = "chat_id=" + chatId;
    if (!name.empty()) data += "&name=" + urlEncode(name);
    if (!password.empty()) data += "&password=" + urlEncode(password);
    std::string body;
    return syncRequest("/api/groups/join", "POST", body, data);
}

// ── dispatchResult ──

void ToxAPI::dispatchResult(ApiCtx* ctx, const HttpResponse& resp) {
    if (!s_target) { return; }
    int type = ctx->type;

    switch (type) {

    case ApiPollEvents: {
        bool restartDetected = false;
        auto it = resp.headers.find("x-server-next-id");
        if (it != resp.headers.end()) {
            char* endptr = nullptr;
            uint64_t serverNextId = std::strtoull(it->second.c_str(), &endptr, 10);
            if (endptr != it->second.c_str() && serverNextId <= s_lastEventId) {
                restartDetected = true;
                s_lastEventId = 0;
            }
        }

        if (resp.httpCode == 0 && !resp.curlErrStr.empty()) {
            ALOG_WARN("Event poll error:", resp.curlErrStr);
            if (s_pollRunning) {
                qSleepMs(2000);
                EventPoller::addRequest(
                    {buildUrl("/api/events?after=" + std::to_string(s_lastEventId) + "&" + kEventTopic),
                     "GET", "", 35, {{"Accept", "application/x-ndjson"}}},
                    onHttpDone, new ApiCtx(ApiPollEvents));
            }
            break;
        }

        if (resp.httpCode != 200) {
            ALOG_WARN("!! event poll non-200:", resp.httpCode);
            if (s_pollRunning) {
                qSleepMs(2000);
                EventPoller::addRequest(
                    {buildUrl("/api/events?after=" + std::to_string(s_lastEventId) + "&" + kEventTopic),
                     "GET", "", 35, {{"Accept", "application/x-ndjson"}}},
                    onHttpDone, new ApiCtx(ApiPollEvents));
            }
            break;
        }

        bool useNdjson = s_useNdjson;

        std::vector<Event> events;
        if (restartDetected) {
            Event e;
            e.id = 0; e.type = "_server_restart"; e.data = ""; e.timestamp = "";
            events.push_back(e);
        }

        if (useNdjson) {
            int parseErrors = parseEventsNdjson(resp.body, s_lastEventId, events);
            if (parseErrors > 0) {
                ALOG_WARN("parseEventsNdjson:", parseErrors, "/", events.size() + parseErrors, "lines failed");
            }
        } else {
            if (!parseEventsJson(resp.body, s_lastEventId, events))
                ALOG_WARN("parseEventsJson: parse failed");
        }

        QApplication::postEvent(s_target, new EventListEvent(events));

        if (s_pollRunning) {
            EventPoller::addRequest(
                {buildUrl("/api/events?after=" + std::to_string(s_lastEventId) + "&" + kEventTopic),
                 "GET", "", 35, {{"Accept", "application/x-ndjson"}}},
                onHttpDone, new ApiCtx(ApiPollEvents));
        }
        break;
    }

    case ApiGetSelf: {
        auto* ev = new SelfInfoResultEvent();
        ev->elapsedMs = resp.elapsedMs;
        if (resp.httpCode != 200 || resp.body.empty()) {
            ev->success = false;
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* root = cJSON_Parse(resp.body.c_str());
        if (!root) { ev->success = false; QApplication::postEvent(s_target, ev); break; }
        ev->success = true;
        ev->name = jsonStr(cJSON_GetObjectItem(root, "name"));
        ev->statusMsg = jsonStr(cJSON_GetObjectItem(root, "status_message"));
        ev->connStatus = jsonStr(cJSON_GetObjectItem(root, "connection_status"));
        ev->address = jsonStr(cJSON_GetObjectItem(root, "address"));
        cJSON_Delete(root);
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiGetFriends: {
        auto* ev = new FriendsResultEvent();
        ev->elapsedMs = resp.elapsedMs;
        if (resp.httpCode != 200 || resp.body.empty()) {
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* root = cJSON_Parse(resp.body.c_str());
        if (!root) { QApplication::postEvent(s_target, ev); break; }
        cJSON* arr = cJSON_GetObjectItem(root, "friends");
        if (arr && cJSON_IsArray(arr)) {
            int n = cJSON_GetArraySize(arr);
            for (int i = 0; i < n; ++i) {
                cJSON* item = cJSON_GetArrayItem(arr, i);
                if (item) { ev->friendIds.push_back(item->valueint); }
            }
        }
        cJSON_Delete(root);
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiSendMessage:
    case ApiSendFriendMessage:
    case ApiSendConferenceMessage:
    case ApiSendGroupMessage: {
        auto* ev = new MessageSentResultEvent((ApiRequestType)type);
        ev->elapsedMs = resp.elapsedMs;
        ev->success = (resp.httpCode == 200 && !resp.body.empty());
        if (!ev->success) {
            ev->errorMessage = extractRespError(resp);
        } else if (!resp.body.empty()) {
            cJSON* root = cJSON_Parse(resp.body.c_str());
            if (root) {
                cJSON* mid = cJSON_GetObjectItem(root, "message_id");
                if (mid && cJSON_IsNumber(mid)) {
                    ev->messageId = std::to_string((uint64_t)mid->valuedouble);
                } else if (mid && cJSON_IsString(mid)) {
                    ev->messageId = jsonStr(mid);
                }
                cJSON_Delete(root);
            }
        }
        ev->chatId = ctx->id;
        ev->message = ctx->str1;
        ev->sendmsgseq = ctx->sendmsgseq;
        switch (ctx->type) {
            case ApiSendMessage: ev->chatType = ctx->str2; break;
            case ApiSendFriendMessage: ev->chatType = "friend"; break;
            case ApiSendConferenceMessage: ev->chatType = "conference"; break;
            case ApiSendGroupMessage: ev->chatType = "group"; break;
        }
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiRedactMessage: {
        auto* ev = new RedactResultEvent();
        ev->elapsedMs = resp.elapsedMs;
        ev->success = (resp.httpCode == 200);
        if (!ev->success) {
            ev->errorMessage = extractRespError(resp);
        }
        ev->chatId = ctx->id;
        ev->messageId = ctx->str1;
        ev->chatType = ctx->str2;
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiAddFriend:
    case ApiDeleteFriend:
    case ApiCreateConference:
    case ApiLeaveConference:
    case ApiInviteToConference:
    case ApiRejectConference:
    case ApiIgnoreConference:
    case ApiCreateGroup:
    case ApiLeaveGroup:
    case ApiInviteToGroup:
    case ApiJoinGroup:
    case ApiJoinGroupByChatId:
    case ApiSetGroupSelfName:
    case ApiSetGroupTopic:
    case ApiSetConferenceTitle:
        break; // fire-and-forget
    case ApiSetSelfInfo: {
        auto* ev = new SelfInfoResultEvent();
        ev->elapsedMs = resp.elapsedMs;
        if (resp.httpCode != 200 || resp.body.empty()) {
            ev->success = false;
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* root = cJSON_Parse(resp.body.c_str());
        if (!root) { ev->success = false; QApplication::postEvent(s_target, ev); break; }
        ev->success = true;
        ev->name = jsonStr(cJSON_GetObjectItem(root, "name"));
        ev->statusMsg = jsonStr(cJSON_GetObjectItem(root, "status_message"));
        ev->connStatus = jsonStr(cJSON_GetObjectItem(root, "connection_status"));
        ev->address = jsonStr(cJSON_GetObjectItem(root, "address"));
        cJSON_Delete(root);
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiJoinConference: {
        auto* ev = new ApiResultEvent(ApiJoinConference);
        ev->elapsedMs = resp.elapsedMs;
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiGetRandomName: {
        auto* ev = new ApiResultEvent(ApiGetRandomName);
        ev->elapsedMs = resp.elapsedMs;
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiLoadFriendDetail: {
        auto* ev = new FriendDetailEvent();
        ev->elapsedMs = resp.elapsedMs;
        ev->friendId = ctx->id;
        if (resp.httpCode != 200 || resp.body.empty()) {
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* root = cJSON_Parse(resp.body.c_str());
        if (!root || !cJSON_IsArray(root) || cJSON_GetArraySize(root) == 0) {
            if (root) { cJSON_Delete(root); }
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* item = cJSON_GetArrayItem(root, 0);
        if (!item) {
            cJSON_Delete(root);
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* err = cJSON_GetObjectItem(item, "error");
        if (err && cJSON_IsString(err) && strlen(cJSON_GetStringValue(err)) > 0) {
            cJSON_Delete(root);
            QApplication::postEvent(s_target, ev);
            break;
        }
        ev->success = true;
        ev->name = jsonStr(cJSON_GetObjectItem(item, "name"));
        ev->publicKey = jsonStr(cJSON_GetObjectItem(item, "publicKey"));
        ev->statusStr = jsonStr(cJSON_GetObjectItem(item, "statusStr"));
        ev->statusText = jsonStr(cJSON_GetObjectItem(item, "statusText"));
        ev->iconUrl = jsonStr(cJSON_GetObjectItem(item, "iconUrl"));
        ev->userStatus = jsonStr(cJSON_GetObjectItem(item, "userStatus"));
        ev->peerIp = jsonStr(cJSON_GetObjectItem(item, "peerIp"));
        cJSON* ls = cJSON_GetObjectItem(item, "lastSeen");
        if (ls) { ev->lastSeen = (uint64_t)ls->valuedouble; }
        cJSON_Delete(root);
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiLoadGroupMembers: {
        auto* ev = new MembersLoadedEvent();
        ev->elapsedMs = resp.elapsedMs;
        ev->contactId = ctx->id;
        ev->contactType = ctx->str1;
        if (resp.httpCode != 200 || resp.body.empty()) {
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* root = cJSON_Parse(resp.body.c_str());
        if (!root) { QApplication::postEvent(s_target, ev); break; }

        cJSON* selfPpItem = cJSON_GetObjectItem(root, "selfPeerNumber");
        int selfPeerNumber = selfPpItem ? selfPpItem->valueint : -1;

        cJSON* membersItem = cJSON_GetObjectItem(root, "members");
        if (membersItem && cJSON_IsArray(membersItem)) {
            int n = cJSON_GetArraySize(membersItem);
            for (int i = 0; i < n; ++i) {
                cJSON* item = cJSON_GetArrayItem(membersItem, i);
                if (!item) { continue; }
                PeerInfo info;
                cJSON* v;
                v = cJSON_GetObjectItem(item, "peerNumber");
                if (v) { info.peerNumber = v->valueint; }
                info.name = jsonStr(cJSON_GetObjectItem(item, "name"));
                v = cJSON_GetObjectItem(item, "status");
                if (v) { info.status = v->valueint; }
                info.statusStr = jsonStr(cJSON_GetObjectItem(item, "statusStr"));
                info.statusText = jsonStr(cJSON_GetObjectItem(item, "statusText"));
                info.iconUrl = jsonStr(cJSON_GetObjectItem(item, "iconUrl"));
                v = cJSON_GetObjectItem(item, "role");
                if (v) { info.role = v->valueint; }
                info.roleStr = jsonStr(cJSON_GetObjectItem(item, "roleStr"));
                info.publicKey = jsonStr(cJSON_GetObjectItem(item, "publicKey"));
                info.peerIp = jsonStr(cJSON_GetObjectItem(item, "peerIp"));
                info.isSelf = (info.peerNumber == selfPeerNumber);
                ev->members.push_back(info);
            }
        }
        cJSON_Delete(root);
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiLoadMessageHistory: {
        auto* ev = new MessageHistoryLoadedEvent();
        ev->elapsedMs = resp.elapsedMs;
        ev->contactId = ctx->id;
        ev->contactType = ctx->str1;
        if (resp.httpCode != 200 || resp.body.empty()) {
            ev->errorMessage = "NETWORK_ERROR";
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* root = cJSON_Parse(resp.body.c_str());
        if (!root) {
            ev->errorMessage = "PARSE_ERROR";
            QApplication::postEvent(s_target, ev);
            break;
        }
        ev->success = true;
        cJSON* msgsItem = cJSON_GetObjectItem(root, "messages");
        if (msgsItem && cJSON_IsArray(msgsItem)) {
            int n = cJSON_GetArraySize(msgsItem);
            for (int i = 0; i < n; ++i) {
                cJSON* msg = cJSON_GetArrayItem(msgsItem, i);
                if (!msg) { continue; }
                HistoryMessage hm;
                cJSON* v = cJSON_GetObjectItem(msg, "rowid");
                if (v) { hm.rowid = (int64_t)v->valuedouble; }
                hm.message = jsonStr(cJSON_GetObjectItem(msg, "message"));
                hm.sender_pubkey = jsonStr(cJSON_GetObjectItem(msg, "sender_pubkey"));
                v = cJSON_GetObjectItem(msg, "sender_number");
                if (v) { hm.sender_number = (uint32_t)v->valuedouble; }
                hm.direction = jsonStr(cJSON_GetObjectItem(msg, "direction"));
                hm.created_at = jsonStr(cJSON_GetObjectItem(msg, "created_at"));
                hm.eventId = jsonStr(cJSON_GetObjectItem(msg, "event_id"));
                ev->messages.push_back(hm);
            }
        }
        cJSON_Delete(root);
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiTranslate: {
        auto* ev = new TranslateResultEvent();
        ev->elapsedMs = resp.elapsedMs;
        ev->chatId = ctx->chatId;
        ev->chatType = ctx->chatType;
        ev->msgIndex = ctx->id;
        if (resp.httpCode != 200 || resp.body.empty()) {
            ev->errorMessage = "NETWORK_ERROR: cannot connect to server";
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* root = cJSON_Parse(resp.body.c_str());
        if (!root) {
            ev->errorMessage = "PARSE_ERROR: invalid server response";
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* t = cJSON_GetObjectItem(root, "translated_text");
        if (t && cJSON_IsString(t)) {
            ev->success = true;
            ev->translatedText = cJSON_GetStringValue(t);
        } else {
            std::string err = jsonStr(cJSON_GetObjectItem(root, "error"));
            std::string code = jsonStr(cJSON_GetObjectItem(root, "code"));
            ev->errorMessage = (code.empty() ? "UNKNOWN" : code) + ": " + (err.empty() ? "translate failed" : err);
        }
        cJSON_Delete(root);
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiTranslateForSend: {
        auto* ev = new TranslateForSendResultEvent();
        ev->elapsedMs = resp.elapsedMs;
        if (resp.httpCode != 200 || resp.body.empty()) {
            ev->errorMessage = "NETWORK_ERROR: cannot connect to server";
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* root = cJSON_Parse(resp.body.c_str());
        if (!root) {
            ev->errorMessage = "PARSE_ERROR: invalid server response";
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* t = cJSON_GetObjectItem(root, "translated_text");
        if (t && cJSON_IsString(t)) {
            ev->success = true;
            ev->translatedText = cJSON_GetStringValue(t);
        } else {
            std::string err = jsonStr(cJSON_GetObjectItem(root, "error"));
            std::string code = jsonStr(cJSON_GetObjectItem(root, "code"));
            ev->errorMessage = (code.empty() ? "UNKNOWN" : code) + ": " + (err.empty() ? "translate failed" : err);
        }
        cJSON_Delete(root);
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiMediaDownload: {
        auto* ev = new MediaDownloadEvent();
        ev->chatId = ctx->id;
        ev->chatType = ctx->str1;
        ev->mxcUrl = ctx->str2;
        ev->msgIndex = ctx->n1;
        if (resp.httpCode != 200 || resp.body.empty() || !resp.curlErrStr.empty()) {
            ev->success = false;
            ev->errorInfo = resp.curlErrStr.empty()
                ? "HTTP " + std::to_string(resp.httpCode)
                : resp.curlErrStr;
            QApplication::postEvent(s_target, ev);
            break;
        }
        ev->success = true;
        ev->rawData = resp.body;
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiAvatarDownload: {
        auto* ev = new AvatarDownloadEvent();
        ev->mxcUrl = ctx->str2;
        if (resp.httpCode != 200 || resp.body.empty()) {
            ev->success = false;
            ev->errorInfo = "HTTP " + std::to_string(resp.httpCode);
        } else if (!ev->pixmap.loadFromData(
                       (const uchar*)resp.body.data(), resp.body.size())) {
            ev->success = false;
            ev->errorInfo = "pixmap load failed";
        } else {
            ev->success = true;
        }
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiLoadAllData: {
        auto* chain = static_cast<LoadChain*>(ctx->ptr);
        if (!chain) { break; }

        switch (chain->step) {
        case 0: { // self -> partial self info
            if (resp.httpCode == 200 && !resp.body.empty()) {
                cJSON* root = cJSON_Parse(resp.body.c_str());
                if (root) {
                    chain->result->selfName = jsonStr(cJSON_GetObjectItem(root, "name"));
                    chain->result->selfStatusMsg = jsonStr(cJSON_GetObjectItem(root, "status_message"));
                    chain->result->selfConnStatus = jsonStr(cJSON_GetObjectItem(root, "connection_status"));
                    chain->result->selfAddress = jsonStr(cJSON_GetObjectItem(root, "address"));
                    cJSON_Delete(root);
                }
            }
            auto* partial = new PartialDataEvent();
            partial->elapsedMs = resp.elapsedMs;
            partial->loadedMask = PartialDataEvent::kSelf;
            partial->selfName = chain->result->selfName;
            partial->selfStatusMsg = chain->result->selfStatusMsg;
            partial->selfConnStatus = chain->result->selfConnStatus;
            partial->selfAddress = chain->result->selfAddress;
            QApplication::postEvent(s_target, partial);
            chain->step = 1;
            auto* next = new ApiCtx(ApiLoadAllData); next->ptr = chain;
            EventPoller::addRequest({buildUrl("/api/friends"), "GET", "", 35}, onHttpDone, next);
            break;
        }
        case 1: { // friend ids -> partial contacts + skip detail batch
            std::vector<ContactData> batch;
            if (resp.httpCode == 200 && !resp.body.empty()) {
                cJSON* root = cJSON_Parse(resp.body.c_str());
                if (root) {
                    cJSON* arr = cJSON_GetObjectItem(root, "friends");
                    if (arr && cJSON_IsArray(arr)) {
                        int n = cJSON_GetArraySize(arr);
                        for (int i = 0; i < n; ++i) {
                            int fid = cJSON_GetArrayItem(arr, i)->valueint;
                            chain->friendIds.push_back(fid);
                            ContactData cd;
                            cd.id = fid;
                            cd.type = "friend";
                            cd.name = "";
                            cd.status = "offline";
                            cd.chatId = "";
                            chain->result->contacts.push_back(cd);
                            batch.push_back(cd);
                        }
                    }
                    cJSON_Delete(root);
                }
            }
            if (!batch.empty()) {
                auto* partial = new PartialDataEvent();
                partial->elapsedMs = resp.elapsedMs;
                partial->loadedMask = PartialDataEvent::kContacts;
                partial->contacts = batch;
                QApplication::postEvent(s_target, partial);
            }
            if (!chain->friendIds.empty()) {
                chain->step = 2;
                chain->detailBatchIdx = 0;
                size_t total = chain->friendIds.size();
                size_t end = maxBatchSize < total ? maxBatchSize : total;
                std::string idsStr;
                for (size_t i = 0; i < end; ++i) {
                    if (i > 0) { idsStr += ","; }
                    idsStr += std::to_string(chain->friendIds[i]);
                }
                chain->detailBatchIdx = end;
                auto* next = new ApiCtx(ApiLoadAllData); next->ptr = chain;
                std::string postData = "friend_ids=" + idsStr;
                EventPoller::addRequest({buildUrl("/api/friend"), "POST", postData, 35}, onHttpDone, next);
            } else {
                chain->step = 3;
                auto* next = new ApiCtx(ApiLoadAllData); next->ptr = chain;
                EventPoller::addRequest({buildUrl("/api/groups"), "GET", "", 35}, onHttpDone, next);
            }
            break;
        }
        case 2: { // friend detail batch -> replace placeholders with real data
            std::vector<ContactData> batch;
            if (resp.httpCode == 200 && !resp.body.empty()) {
                cJSON* root = cJSON_Parse(resp.body.c_str());
                if (root) {
                    if (cJSON_IsArray(root)) {
                        int n = cJSON_GetArraySize(root);
                        for (int i = 0; i < n; ++i) {
                            cJSON* item = cJSON_GetArrayItem(root, i);
                            if (!item) { continue; }
                            cJSON* err = cJSON_GetObjectItem(item, "error");
                            if (err && cJSON_IsString(err) && strlen(cJSON_GetStringValue(err)) > 0) { continue; }
                            ContactData cd;
                            cJSON* v = cJSON_GetObjectItem(item, "friendId");
                            if (!v) { continue; }
                            cd.id = v->valueint;
                            cd.type = "friend";
                            cd.name = jsonStr(cJSON_GetObjectItem(item, "name"));
                            cd.chatId = jsonStr(cJSON_GetObjectItem(item, "publicKey"));
                            cd.iconUrl = jsonStr(cJSON_GetObjectItem(item, "iconUrl"));
                            std::string s = jsonStr(cJSON_GetObjectItem(item, "statusStr"));
                            if (s == "tcp" || s == "udp") {
                                cd.status = s;
                                cd.isConnected = true;
                            } else {
                                cd.status = "offline";
                                cd.isConnected = false;
                            }
                            for (auto& rc : chain->result->contacts) {
                                if (rc.id == cd.id && rc.type == "friend") {
                                    rc = cd;
                                    break;
                                }
                            }
                            batch.push_back(cd);
                        }
                    }
                    cJSON_Delete(root);
                }
            }
            if (!batch.empty()) {
                auto* partial = new PartialDataEvent();
                partial->elapsedMs = resp.elapsedMs;
                partial->loadedMask = PartialDataEvent::kContacts;
                partial->contacts = batch;
                QApplication::postEvent(s_target, partial);
            }
            size_t total = chain->friendIds.size();
            if (chain->detailBatchIdx < total) {
                size_t end = chain->detailBatchIdx + maxBatchSize;
                if (end > total) { end = total; }
                std::string idsStr;
                for (size_t i = chain->detailBatchIdx; i < end; ++i) {
                    if (i > chain->detailBatchIdx) { idsStr += ","; }
                    idsStr += std::to_string(chain->friendIds[i]);
                }
                chain->detailBatchIdx = end;
                auto* next = new ApiCtx(ApiLoadAllData); next->ptr = chain;
                std::string postData = "friend_ids=" + idsStr;
                EventPoller::addRequest({buildUrl("/api/friend"), "POST", postData, 35}, onHttpDone, next);
            } else {
                chain->step = 3;
                auto* next = new ApiCtx(ApiLoadAllData); next->ptr = chain;
                EventPoller::addRequest({buildUrl("/api/groups"), "GET", "", 35}, onHttpDone, next);
            }
            break;
        }
        case 3: { // groups -> partial contacts
            std::vector<ContactData> batch;
            if (resp.httpCode == 200 && !resp.body.empty()) {
                cJSON* root = cJSON_Parse(resp.body.c_str());
                if (root) {
                    cJSON* arr = cJSON_GetObjectItem(root, "groups");
                    if (arr && cJSON_IsArray(arr)) {
                        int n = cJSON_GetArraySize(arr);
                        for (int i = 0; i < n; ++i) {
                            cJSON* item = cJSON_GetArrayItem(arr, i);
                            if (!item) { continue; }
                            ContactData cd;
                            cJSON* v = cJSON_GetObjectItem(item, "groupNumber");
                            if (v) { cd.id = v->valueint; }
                            cd.name = jsonStr(cJSON_GetObjectItem(item, "groupName"));
                            cd.chatId = jsonStr(cJSON_GetObjectItem(item, "chatId"));
                            v = cJSON_GetObjectItem(item, "isConnected");
                            cd.isConnected = v ? (v->valueint == 1) : false;
                             cd.statusText = jsonStr(cJSON_GetObjectItem(item, "statusText"));
                             v = cJSON_GetObjectItem(item, "memberCount");
                             if (v) { cd.memberCount = v->valueint; }
                             cd.type = "group";
                            chain->result->contacts.push_back(cd);
                            batch.push_back(cd);
                        }
                    }
                    cJSON_Delete(root);
                }
            }
            if (!batch.empty()) {
                auto* partial = new PartialDataEvent();
                partial->elapsedMs = resp.elapsedMs;
                partial->loadedMask = PartialDataEvent::kContacts;
                partial->contacts = batch;
                QApplication::postEvent(s_target, partial);
            }
            chain->step = 4;
            auto* next = new ApiCtx(ApiLoadAllData); next->ptr = chain;
            EventPoller::addRequest({buildUrl("/api/conferences"), "GET", "", 35}, onHttpDone, next);
            break;
        }
        case 4: { // conferences -> partial contacts + all done
            std::vector<ContactData> batch;
            if (resp.httpCode == 200 && !resp.body.empty()) {
                cJSON* root = cJSON_Parse(resp.body.c_str());
                if (root) {
                    cJSON* arr = cJSON_GetObjectItem(root, "conferences");
                    if (arr && cJSON_IsArray(arr)) {
                        int n = cJSON_GetArraySize(arr);
                        for (int i = 0; i < n; ++i) {
                            cJSON* item = cJSON_GetArrayItem(arr, i);
                            if (!item) { continue; }
                            ContactData cd;
                            cJSON* v = cJSON_GetObjectItem(item, "conferenceNumber");
                            if (v) { cd.id = v->valueint; }
                            cd.name = jsonStr(cJSON_GetObjectItem(item, "conferenceName"));
                            cd.chatId = jsonStr(cJSON_GetObjectItem(item, "chatId"));
                            v = cJSON_GetObjectItem(item, "isConnected");
                            cd.isConnected = v ? (v->valueint == 1) : false;
                             cd.statusText = jsonStr(cJSON_GetObjectItem(item, "statusText"));
                             v = cJSON_GetObjectItem(item, "memberCount");
                             if (v) { cd.memberCount = v->valueint; }
                             cd.type = "conference";
                            chain->result->contacts.push_back(cd);
                            batch.push_back(cd);
                        }
                    }
                    cJSON_Delete(root);
                }
            }
            if (!batch.empty()) {
                auto* partial = new PartialDataEvent();
                partial->elapsedMs = resp.elapsedMs;
                partial->loadedMask = PartialDataEvent::kContacts;
                partial->contacts = batch;
                QApplication::postEvent(s_target, partial);
            }
            chain->result->success = true;
            chain->result->elapsedMs = resp.elapsedMs;
            QApplication::postEvent(s_target, chain->result);
            delete chain;
            break;
        }
        }
        break;
    }

    }
}
