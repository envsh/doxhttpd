#pragma once
#include "eventpoller.h"          // PeerInfo / ContactData
#include "channel_db.h"           // PeerRow
#include <map>
#include <string>
#include <vector>
#include <memory>

// ── 客户端元变量缓存：peer + 联系人 ──
// peer key 格式: "friend_N", "group_N_M", "conference_N_M", "unknown_*"
struct PeerKey {
    std::string chanid;
    int peerNum = 0;
    bool valid = false;
};

PeerKey parsePeerKey(const std::string& key);

// Row→PeerInfo 转换 + 写入 map + 可选 avatar 下载；返回 iterator，失败返回 end()
std::map<std::string, PeerInfo>::iterator
loadRowToMap(std::map<std::string, PeerInfo>& m,
             const std::string& key,
             std::unique_ptr<PeerRow> row);

bool addPeerToDb(const std::string& key, const PeerInfo& pi);
void updatePeerInDb(const std::string& key, const PeerInfo& pi);
PeerInfo& getOrCreatePeerEntry(std::map<std::string, PeerInfo>& m, const std::string& key);
void mergePeerInfo(PeerInfo& dst, const PeerInfo& src);

// 查找 peer：内存命中直接返回；miss 时按 key parsePeerKey + channelDb 回填。仍 miss 返回 end()
std::map<std::string, PeerInfo>::iterator
lookupPeerByKey(std::map<std::string, PeerInfo>& m, const std::string& key);

// ── 联系人缓存 helpers（m_accumulatedContactData，保持 vector + (id,type) 匹配）──
// 按 (id,type) 原地覆盖已存在项，否则追加；返回 true=新增，false=已存在被覆盖
bool upsertContact(std::vector<ContactData>& vec, const ContactData& cd);

// 按 (id,type) 查找，未命中返回 0（不新增、不改动容器）
const ContactData* findContact(const std::vector<ContactData>& vec,
                               int id, const std::string& type);

// 虚拟类型判定（chatId 为字符串的流式/桥接类型）
bool isVirtualChatType(const std::string& type);

// 虚拟类型的 chatId 覆盖值：仅当 isVirtualChatType 且命中时返回 cd.chatId，否则空串
std::string contactChatIdOverride(const std::vector<ContactData>& vec,
                                  int id, const std::string& type);