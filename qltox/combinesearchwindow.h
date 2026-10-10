#ifndef COMBINESEARCHWINDOW_H
#define COMBINESEARCHWINDOW_H

#include "compat34.h"
#include "searchlistview.h"
#include <qdialog.h>
#include <atomic>
#include <string>
#include <vector>

class QPushButton;
class QLabel;
class LimeScrollBar;
class PlaceholderLineEdit;

// 联系人类型 → 显示名（friend/conference/group/未知/unk/虚拟/订阅流全覆盖），
// 供 FriendInfoDialog 类型行与搜索窗类型标签共用，未知回退原始类型。
QString contactTypeLabel(const QString& type);

class CombineSearch : public QDialog {
    Q_OBJECT
public:
    explicit CombineSearch(QWidget* parent = nullptr);
    bool isSearching() const { return m_searching; }

protected:
    void keyPressEvent(QKeyEvent* e);
    void closeEvent(QCloseEvent* e);
    void customEvent(CustomEventBase* event);

private slots:
    void runSearch();
    void onCancelClicked();
    void onTabClicked();
    void onViewScroll(int v);
    void goFirst();
    void goPrev();
    void goNext();
    void goLast();

private:
    struct ContactHit {
        int id;
        std::string type;
        QString name;
        QString typeLabel;
    };
    struct MessageHit {
        int id;
        std::string type;
        QString chanName;
        QString sender;
        QString body;
        QString time;
    };
    typedef SearchListView::RowInfo RowInfo;

    QWidget* makeRow(const QString& title, const QString& detail, QWidget* host);
    void clearLayout(QLayout* lay);
    int activeTab() const;
    int rowTotal(int tab) const;
    void renderSlice(int tab, int page);
    void showPage(int tab, int page);

    PlaceholderLineEdit* m_input = nullptr;
    QPushButton* m_searchBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QPushButton* m_tabContacts = nullptr;
    QPushButton* m_tabMessages = nullptr;
    QPushButton* m_firstBtn = nullptr;
    QPushButton* m_prevBtn = nullptr;
    QPushButton* m_nextBtn = nullptr;
    QPushButton* m_lastBtn = nullptr;
    QLabel* m_pageLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    StackedWidget* m_stack = nullptr;
    QWidget* m_contactsInner = nullptr;
    ScrollArea* m_contactsScroll = nullptr;
    SearchListView* m_viewMsg = nullptr;
    LimeScrollBar* m_msgBar = nullptr;
    std::vector<QWidget*> m_pages;
    std::vector<QPushButton*> m_tabButtons;
    std::vector<ContactHit> m_contacts;
    std::vector<MessageHit> m_messages;
    std::vector<RowInfo> m_contactRows;
    std::vector<RowInfo> m_messageRows;
    int m_curPage[2] = {0, 0};
    std::atomic<bool> m_closed;
    std::atomic<bool> m_canceled;
    bool m_searching = false;
    int m_searchSeq = 0;
};

#endif