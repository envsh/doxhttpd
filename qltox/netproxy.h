#ifndef NETPROXY_H
#define NETPROXY_H

#include <string>

// 全局代理配置（进程内单例，线程安全）。
//
// 用途：fanyibot/aigptbot/pasteuploader 三模块中「需要代理」的 provider
//   走用户配置的 HTTP/SOCKS 代理，其余直连。
//   HTTP 请求跑在 EventPoller 线程，而设置在 GUI 线程读写，故加锁。
//
// 语义：enabled=false 或 url 为空 = 对所有 provider 禁用代理（直连）。

// 设置代理（GUI 线程：启动时 / 设置保存后调用）
void netProxySet(bool enabled, const std::string& url);

// 对需要代理的 provider 返回代理 URL；否则返回空串（直连）。
// needsProxy=false 一律返回空串。
std::string netProxyFor(bool needsProxy);

#endif // NETPROXY_H
