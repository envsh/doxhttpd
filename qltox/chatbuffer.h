#ifndef CHATBUFFER_H
#define CHATBUFFER_H

#include <deque>
#include <map>
#include <utility>
#include <string>
#include <cstdint>
#include "chatview.h"

class ChatBuffer;

class ChatHistoryObserver;

class ChatHistory {
    friend class ChatBuffer;
public:
    int size() const { return m_items.size(); }
    bool empty() const { return m_items.empty(); }

    const ChatElement& operator[](int i) const { return m_items[i]; }
    ChatElement& operator[](int i) { return m_items[i]; }

    const ChatElement& back() const { return m_items.back(); }
    ChatElement& back() { return m_items.back(); }

    using iterator = std::deque<ChatElement>::iterator;
    using const_iterator = std::deque<ChatElement>::const_iterator;
    iterator begin() { return m_items.begin(); }
    iterator end() { return m_items.end(); }
    const_iterator begin() const { return m_items.begin(); }
    const_iterator end() const { return m_items.end(); }

    int64_t oldestRowid = 0;
    int64_t newestRowid = 0;
    bool exhausted = false;
    bool loadedLatest50FromDB = false;
    bool loadedLastest50FromNet = false;
    static int getCapacity() { return m_capacity; }
    static ChatHistory kEmpty;

    void setObserver(ChatHistoryObserver* obs) { m_observer = obs; }

private:
    std::deque<ChatElement> m_items;
    ChatHistoryObserver* m_observer = nullptr;

    void append(const ChatElement& el);
    void prepend(const std::vector<ChatElement>& els);
    void trimOverflow();
    static int m_capacity;
    static int64_t s_nextLocalId;   // localId 分配器（进程内单调，跨裁剪稳定）
};

// ChatBuffer — 所有聊天的环形消息缓冲区。
//
// 设计约束：消息数据只进不出，永不从外部清除。
// - 所有消息通过 append() 流入，不存在 clear/remove/reset 操作。
// - 切换账户时通过析构整个 ChatBuffer 释放（m_chatbuf = ChatBuffer()）。
// - 视图重置用 ChatView::resetCanvas()，不影响 ChatHistory 数据。
// - 若上游出现"需要清空"的需求，说明上游设计有误，需重新设计机制而非添加 clear()。
class ChatBuffer {
public:
    ChatHistory& getOrCreate(int chatId, const std::string& chatType);
    void append(int chatId, const std::string& chatType, const ChatElement& el);
    void prepend(int chatId, const std::string& chatType, const std::vector<ChatElement>& els);
    ChatHistory* ptr(int chatId, const std::string& chatType);
    void clearDisplayCacheFor(int chatId, const std::string& chatType);
private:
    std::map<std::pair<int, std::string>, ChatHistory> m_map;
};

#endif
