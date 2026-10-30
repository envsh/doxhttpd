#ifndef PEER_INFO_CACHE_H
#define PEER_INFO_CACHE_H

#include <QHash>
#include <QString>
#include "eventpoller.h"

// PeerInfoCache 复用 qltox 共享头 eventpoller.h::PeerInfo（同一类型、同一 merge 语义，
// 两个版本共用一处定义避免 ODR 冲突）。
// key 格式沿用 qltox —— "unknown_" + userId（mainwindow.cpp:2872 同款）。
// mergePeerInfo 移植 qltox metavars_cache.cpp:138（空值/零值不覆盖）。
// displayName 口径：nickname > userName（onReplyLinkActivated 对位）。
// 用户敲定：usr_presence 与 usr_status 本版本统一为 userStatus（push 用 usr_status）。
class PeerInfoCache
{
public:
    static PeerInfoCache* instance();

    void mergePeerInfo(const PeerInfo& src);

    PeerInfo lookup(const QString& userId) const;
    bool has(const QString& userId) const;

    // 根据 userId 获取显示名（nickname 优先，退 userName）
    QString displayName(const QString& userId) const;

private:
    PeerInfoCache() = default;
    QHash<QString, PeerInfo> m_map;   // "unknown_<userId>" key 对位
};

#endif // PEER_INFO_CACHE_H

