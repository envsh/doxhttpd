#include "chatbuffer.h"
#include "assertf.h"
#ifdef __linux__
#include <malloc.h>
#endif

int ChatHistory::m_capacity = 200;
int64_t ChatHistory::s_nextLocalId = 1;

ChatHistory ChatHistory::kEmpty;

ChatHistory& ChatBuffer::getOrCreate(int chatId, const std::string& chatType) {
    return m_map[std::make_pair(chatId, chatType)];
}

void ChatBuffer::append(int chatId, const std::string& chatType, const ChatElement& el) {
    getOrCreate(chatId, chatType).append(el);
}

void ChatBuffer::prepend(int chatId, const std::string& chatType, const std::vector<ChatElement>& els) {
    getOrCreate(chatId, chatType).prepend(els);
}

ChatHistory* ChatBuffer::ptr(int chatId, const std::string& chatType) {
    auto it = m_map.find(std::make_pair(chatId, chatType));
    return (it != m_map.end()) ? &it->second : nullptr;
}

void ChatBuffer::clearDisplayCacheFor(int chatId, const std::string& chatType) {
    auto it = m_map.find(std::make_pair(chatId, chatType));
    if (it == m_map.end()) { return; }
    for (auto& el : it->second.m_items) {
        el.scaledDisplay = QPixmap();
    }
#ifdef __linux__
    malloc_trim(0);
#endif
}

void ChatHistory::append(const ChatElement& el) {
    ChatElement e = el;
    if (e.localId == 0) { e.localId = s_nextLocalId++; }   // 拷贝后分配，调用方对象不被改
    m_items.push_back(e);
    trimOverflow();
    if (el.dbRowid > newestRowid) {
        newestRowid = el.dbRowid;
    }
    if (m_observer) {
        m_observer->onInsertOne(m_items.size() - 1);
    }
}

void ChatHistory::prepend(const std::vector<ChatElement>& els) {
    int space = m_capacity - (int)m_items.size();
    if (space <= 0) { return; }
    int toInsert = std::min((int)els.size(), space);
    m_items.insert(m_items.begin(), els.end() - toInsert, els.end());
    for (int i = 0; i < toInsert; ++i) {
        if (m_items[i].localId == 0) { m_items[i].localId = s_nextLocalId++; }
    }
    for (int i = 0; i < toInsert; ++i) {
        const ChatElement& e = m_items[i];
        if (e.dbRowid > 0) {
            if (e.dbRowid > newestRowid) {
                newestRowid = e.dbRowid;
            }
            if (oldestRowid == 0 || e.dbRowid < oldestRowid) {
                oldestRowid = e.dbRowid;
            }
        }
    }
    if (m_observer && toInsert > 0) {
        m_observer->onInsertRange(0, toInsert);
    }
}

void ChatHistory::trimOverflow() {
    int excess = (int)m_items.size() - m_capacity;
    if (excess <= 0) { return; }
    m_items.erase(m_items.begin(), m_items.begin() + excess);
    if (m_observer) {
        m_observer->onRemoveRange(0, excess);
    }
}
