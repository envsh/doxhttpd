#include "peerinfocache.h"

namespace {

// qltox peerInfoMap key 同款（mainwindow.cpp:2872
// "unknown_" + publicKey）
QString keyFor(const QString& userId)
{
    return QStringLiteral("unknown_") + userId;
}

} // namespace

PeerInfoCache* PeerInfoCache::instance()
{
    static PeerInfoCache* s_instance = nullptr;
    if (!s_instance) {
        s_instance = new PeerInfoCache();
    }
    return s_instance;
}

void PeerInfoCache::mergePeerInfo(const PeerInfo& src)
{
    if (src.publicKey.empty()) {
        return;
    }
    const std::string key = keyFor(src.publicKey.c_str()).toStdString();
    PeerInfo& dst = m_map[QString::fromStdString(key)];
    // 移植 qltox metavars_cache.cpp:138 mergePeerInfo
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

PeerInfo PeerInfoCache::lookup(const QString& userId) const
{
    return m_map.value(keyFor(userId));
}

bool PeerInfoCache::has(const QString& userId) const
{
    return m_map.contains(keyFor(userId));
}

QString PeerInfoCache::displayName(const QString& userId) const
{
    const PeerInfo pi = lookup(userId);
    if (!pi.nickname.empty()) {
        return QString::fromStdString(pi.nickname);
    }
    return QString::fromStdString(pi.userName);
}
