#ifndef VIRTUAL_CONTACTS_H
#define VIRTUAL_CONTACTS_H

#include <QString>
#include <QVector>

// 虚拟联系人（合成频道）静态契约 —— 与 qltox 对齐（qltox/mainwindow.cpp:103-113）
struct VirtualContact {
    int         id;
    const char* type;    // = protocol，发送分类键
    const char* name;    // 频道显示名
    const char* emoji;   // 头像字符
};

const QVector<VirtualContact>& virtualContacts();

// 发送路径分类：
//   None        = 普通频道（走 push）
//   LocalNoop   = 本地即时置 Sent（不发送）
//   Bot*        = P1 预留（客户端直连 bot）
enum VirtualSendKind {
    kVirtualNone = 0,
    kVirtualLocalNoop,
    kVirtualBotAigpt,
    kVirtualBotWebsearch,
    kVirtualBotTranslate,
    kVirtualBotPaste,
    kVirtualBotTts,
};

VirtualSendKind virtualSendKind(const QString& protocol);
bool isVirtualProtocol(const QString& protocol);

#endif // VIRTUAL_CONTACTS_H
