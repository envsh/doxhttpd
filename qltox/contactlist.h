#ifndef CONTACTLIST_H
#define CONTACTLIST_H

#include "compat34.h"
#include "placeholderlineedit.h"
#include "LimeScrollBar.h"
#include <stdint.h>
#include <qwidget.h>
#include <qpoint.h>
#ifdef QT3_BUILD
#include <qdatetime.h>
#else
#include <QDateTime>
#endif
#include <map>
#include <vector>
#include <algorithm>
#include <memory>
#include <map>

extern const char* EMOJI_FRIEND;
extern const char* EMOJI_GROUP;
extern const char* EMOJI_CONFERENCE;
extern const char* EMOJI_SYSEVENT;
extern const char* EMOJI_UNKNOWN;
extern const char* EMOJI_TOPIC;
extern const char* EMOJI_MATRIX;
extern const char* EMOJI_MTXLITE;
extern const char* EMOJI_BOOKMARK;
extern const char* EMOJI_AICHAT;
extern const char* EMOJI_PASTEBIN;
extern const char* EMOJI_TRANSLATE;
extern const char* EMOJI_MOBPUSH;
extern const char* EMOJI_SNAP;

struct Contact {
    int id;
    QString name;
    QString type;
    QString status;
    QString chat_id;
    bool is_connected;
    QString lastMessage;
    QString lastMessageTime;
    QDateTime lastActive;
};

typedef QPtrList<Contact> ContactList;

struct RowData {
    int id;
    QString type;
    QString name;
    QString status;
    QString chatId;
    bool isConnected;
    int unread;
    QString lastMessage;
    QString timeStr;
    int index;
    uint lastActive = 0;
    int pinnedIndex = 0;
    QString nameUpper;
    QString truncatedName;
    QString truncatedMsg;
    int cachedWidth = 0;
};

class ContactListWidget;

struct RowPixmapCache {
    QPixmap pix;
    int width = 0;
    int rowId = -1;
    QString rowType;
};

class ContactListView : public QWidget {
public:
    ContactListView(ContactListWidget* widget);
    void setScrollBar(LimeScrollBar* sb) { m_scrollBar = sb; }
    int selectedIndex() const { return m_selIdx; }
    void setSelectedIndex(int idx);
    int scrollY() const { return m_scrollY; }
    void setScrollY(int y);
    void invalidateTruncation();
    void truncateRowData(RowData* rd, int w);
    QPixmap getCircularAvatar(uint32_t cp, int size);
protected:
    void paintEvent(QPaintEvent* e);
    void mousePressEvent(QMouseEvent* e);
    void wheelEvent(QWheelEvent* e);
    void resizeEvent(QResizeEvent* e);
private:
    int totalHeight() const;
    int yToRow(int y) const;
    ContactListWidget* m_widget;
    LimeScrollBar* m_scrollBar;
    int m_selIdx = -1;
    int m_selId = -1;
    QString m_selType;
    int m_scrollY = 0;
    int m_scrollDelta;
    std::map<uint32_t, QPixmap> m_circularAvatarCache;
    std::vector<RowPixmapCache> m_rowCache;
    int m_avatarSize = 0;
#ifdef QT3_BUILD
    QPixmap m_backBuffer;
#endif
public:
    int selectedId() const { return m_selId; }
    QString selectedType() const { return m_selType; }
    void renderRowToCache(int i, int w);
    void invalidateRowCache(int i);
    void invalidateAllCaches();
};

class ContactListData {
public:
    RowData* get(int id, const QString& type);
    RowData* addToEnd(std::unique_ptr<RowData> rd);
    void remove(int id, const QString& type);
    void adjustBySort(int idx);
    void sort(const std::vector<QString>& criteria);
    void freeze();
    void unfreeze(const std::vector<QString>& criteria);
    void clear();
    void updateFrom(int startIdx);
    int size() const { return m_rows.size(); }
    RowData* at(int i) { return m_rows[i]; }
    const RowData* at(int i) const { return m_rows[i]; }
private:
    static bool rowLess(const RowData* a, const RowData* b, const std::vector<QString>& criteria);
    std::map<std::pair<int,QString>, std::unique_ptr<RowData>> m_map;
    std::vector<RowData*> m_rows;
    std::vector<QString> m_criteria;
    bool m_frozen = false;
    std::vector<int> m_pendingAdjust;
};

class ContactListWidget : public QWidget {
    Q_OBJECT
    friend class ContactListView;
public:
    explicit ContactListWidget(QWidget* parent = 0);

    void setContacts(ContactList& contacts);
    void clear();
    void updateFriendName(int friendId, const QString& newName);
    void updateFriendConnectionStatus(int friendId, const QString& newStatus);
    void updateContact(int id, const QString& type, const QString& name,
                       const QString& chatId, const QString& status);
    void addContact(Contact* c);
    void removeContact(int id, const QString& type);
    void updateContactLastMessage(int id, const QString& type, const QString& msg,
                                  const QString& timeStr = QString());
    bool isFriendLoaded(int friendId);
    void retranslateUi();

    void incrementUnread(int id, const QString& type, int count = 1);
    void resetUnread(int id, const QString& type);
    int unreadCount(int id, const QString& type) const;
    int totalUnread() const { return m_totalUnread; }
    void seedTotalUnread(int n) { m_totalUnread = n; }

    void beginBatch();
    void endBatch();
    void togglePin(int id, const QString& type);

    int itemHeight() const { return m_itemHeight; }
    bool matchesFilter(const RowData& rd) const {
        if (m_searchText.isEmpty()) return true;
        return rd.nameUpper.contains(qToUpper(m_searchText));
    }
    void handleContactSelected(int id, const QString& type, const QString& name) {
        emit contactSelected(id, type, name);
    }
    void setViewScrollY(int y);

signals:
    void contactSelected(int id, const QString& type, const QString& name);
    void viewInfoRequested(int id, const QString& type);
    void deleteOrLeaveRequested(int id, const QString& type);
    void inviteToConferenceRequested(int friendId);
    void inviteToGroupRequested(int friendId);
    void viewMembersRequested(int id, const QString& type);
    void renameNickRequested(int groupId, const QString& groupName);
    void setGroupTopicRequested(int groupId);
    void setConferenceTitleRequested(int conferenceId);
    void unreadCountChanged(int total);
    void globalSearchRequested();

private slots:
    void onSearchTextChanged(const QString& text);
    void onSortMenuClicked();
    void onJoinGroupClicked();
    void onAddFriendClicked();
    void onCreateConferenceClicked();
    void onCreateGroupClicked();
    void onPlusMenuClicked();
    void onScrollChanged(int value);

public:
    void showContextMenuAt(int id, const QString& type, const QString& name, const QPoint& globalPos);

private:
    void refreshView();
    void resolveSelection();
    void updateScrollBar();

    ContactListView* m_view;
    ContactListData m_list;
    LimeScrollBar* m_scrollBar;
    int m_batchLevel = 0;
    int contextItemId;
    QString contextItemType;
    PlaceholderLineEdit* addInput;
    QPushButton* addBtn;
    QPushButton* confBtn;
    QPushButton* groupBtn;
    PlaceholderLineEdit* joinGroupInput;
    QPushButton* joinGroupBtn;

    std::map<std::pair<int, std::string>, int> m_unreadCounts;
    int m_totalUnread = 0;
    std::map<std::pair<int, std::string>, QString> m_lastMessages;
    std::map<std::pair<int, std::string>, QString> m_lastMessageTimes;
    std::map<std::pair<int, std::string>, int> m_pinnedIndices;

    PlaceholderLineEdit* searchInput;
    QLabel* countLabel;
    QPushButton* sortBtn;
    QPushButton* plusBtn;
    QString m_searchText;
    int m_itemHeight = 60;
    std::vector<QString> m_sortCriteria;
};

#endif // CONTACTLIST_H
