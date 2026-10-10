#include "metavars_cache.h"
#include "compat34.h"
#include "restapi.h"
#include "avatar_manager.h"
#include "storage.h"
#include "pureconsts.hpp"
#include <ctime>

// 解析 peer key → {chanid, peerNum}。
// "friend_N"→"friend:N"/N；"group_N_M"→"group:N"/M；"conference_N_M"→"conference:N"/M；
// "unknown_PUBKEY"→"unknown:PUBKEY"/0。无法识别时 valid=false。
PeerKey parsePeerKey(const std::string& key) {
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

// 把 DB 行(PeerRow)转换并写入内存 map；row 为空返回 end()。
// 仅在"新插入"且带 iconUrl 时触发头像下载。
std::map<std::string, PeerInfo>::iterator
loadRowToMap(std::map<std::string, PeerInfo>& m,
             const std::string& key,
             std::unique_ptr<PeerRow> row) {
    if (!row) return m.end();
    PeerInfo pi;
    pi.peerNumber  = row->peer_number;
    pi.publicKey   = row->public_key;
    pi.userName    = row->name;
    pi.nickname    = row->nickname;
    pi.iconUrl     = row->avatar_url;
    pi.statusText  = row->status_text;
    pi.statusStr   = row->status_str;
    pi.userStatus  = row->user_status;
    pi.peerIp      = row->peer_ip;
    pi.role        = row->role;
    pi.roleStr     = row->role_str;
    pi.isSelf      = row->is_self;
    pi.lastSeen    = (time_t)row->last_seen;
    pi.status      = row->status;
    auto result = m.insert({key, pi});
    if (result.second && !pi.iconUrl.empty()) {
        QString mxc = qFromUtf8(pi.iconUrl);
        if (AvatarManager::inst().requestDownload(mxc)) {
            ToxAPI::downloadAvatar(pi.iconUrl);
        }
    }
    return result.first;
}

// 新增 peer 到 DB（异步写入）。key 非法、或 nickname/iconUrl 均空时跳过返回 false。
bool addPeerToDb(const std::string& key, const PeerInfo& pi) {
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

// 部分更新 peer 到 DB（异步）。unknown 类型不做部分更新（直接返回）。
void updatePeerInDb(const std::string& key, const PeerInfo& pi) {
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

// 取 key 对应的 PeerInfo 引用；不存在则插入默认值后返回。
PeerInfo& getOrCreatePeerEntry(std::map<std::string, PeerInfo>& m, const std::string& key) {
    auto it = m.find(key);
    if (it == m.end()) {
        it = m.insert({key, PeerInfo()}).first;
    }
    return it->second;
}

// 把 src 的非空/非零字段合并覆盖到 dst（缺失字段保留 dst 原值）。
void mergePeerInfo(PeerInfo& dst, const PeerInfo& src) {
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

std::map<std::string, PeerInfo>::iterator
lookupPeerByKey(std::map<std::string, PeerInfo>& m, const std::string& key) {
    auto it = m.find(key);
    if (it != m.end()) { return it; }
    PeerKey pk = parsePeerKey(key);
    if (!pk.valid) { return m.end(); }
    auto row = Storage::instance().channelDb()->get_chan_peer(pk.chanid.c_str(), pk.peerNum);
    return loadRowToMap(m, key, std::move(row));
}

// ── 联系人缓存 helpers（作用于 MainWindow::m_accumulatedContactData）──

// 按 (id, type) upsert：已存在则原地整体覆盖（如好友占位→真实数据），
// 否则追加到尾。返回 true=新增，false=命中已有项被覆盖。
bool upsertContact(std::vector<ContactData>& vec, const ContactData& cd) {
    for (auto& existing : vec) {
        // (id, type) 唯一标识一个联系人/会话槽位
        if (existing.id == cd.id && existing.type == cd.type) {
            existing = cd;
            return false;
        }
    }
    vec.push_back(cd);
    return true;
}

// 按 (id, type) 只读查找：命中返回指向容器内元素的指针，未命中返回 0。
// 不新增、不改动容器，故调用方须在同一逻辑帧内使用该指针。
const ContactData* findContact(const std::vector<ContactData>& vec,
                               int id, const std::string& type) {
    for (const auto& cd : vec) {
        if (cd.id == id && cd.type == type) {
            return &cd;
        }
    }
    return 0;
}

// 虚拟类型判定：这类会话的 chatId 是字符串（如 gomuks room ID），
// 不能直接用 numeric contactId 当 id，需以 cd.chatId 覆盖。
bool isVirtualChatType(const std::string& type) {
    return type == kGomuksRoomType || type == kMtxliteRoomType
        || type == kUnktoxConferenceType
        || type == kUnktoxFriendType || type == kUnktoxGroupType
        || type == kMisskeyType || type == kImapMailType;
}

// 取虚拟类型的 chatId 覆盖值：非虚拟类型或未命中均返回空串
// （调用方原样透传给 ToxAPI 的 idOverride 参数，空串即表示用 numeric id）。
std::string contactChatIdOverride(const std::vector<ContactData>& vec,
                                  int id, const std::string& type) {
    if (!isVirtualChatType(type)) { return std::string(); }
    const ContactData* c = findContact(vec, id, type);
    return c ? c->chatId : std::string();
}