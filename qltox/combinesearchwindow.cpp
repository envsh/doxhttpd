#include "combinesearchwindow.h"
#include "translator.h"
#include "compat34.h"
#include "placeholderlineedit.h"
#include "storage.h"
#include "channel_db.h"
#include "message_db.h"
#include "eventpoller.h"
#include "pureconsts.hpp"
#include <qlayout.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qstringlist.h>
#ifdef QT3_BUILD
#include <qevent.h>
#include <qapplication.h>
#else
#include <QKeyEvent>
#include <QApplication>
#endif
#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

static const int VIRTUAL_SEARCH_UNKNOWN_ID = -100;
static const int VIRTUAL_SEARCH_SYSEVENT_ID = -101;
static const int VIRTUAL_SEARCH_REDDIT_ID = -102;
static const int VIRTUAL_SEARCH_BOOKMARK_ID = -103;
static const int kTestRows = 5;

namespace {

ScrollArea* makeScrollArea(QWidget* page, QWidget*& inner) {
#ifdef QT3_BUILD
    QScrollView* scroll = new QScrollView(page);
#else
    QScrollArea* scroll = new QScrollArea(page);
#endif
    inner = new QWidget(scroll);
#ifdef QT3_BUILD
    scroll->addChild(inner);
    scroll->setResizePolicy(QScrollView::AutoOne);
#else
    scroll->setWidget(inner);
    scroll->setWidgetResizable(true);
#endif
    return scroll;
}

QLabel* sectionTitle(const QString& text, QWidget* host) {
    QLabel* l = new QLabel(text, host);
    QFont f = l->font();
    f.setBold(true);
    l->setFont(f);
#ifdef QT3_BUILD
    l->setPaletteForegroundColor(QColor(0x40, 0x40, 0x40));
#else
    l->setStyleSheet("color:#404040");
#endif
    return l;
}

QString typeLabel(const std::string& type) {
    if (type == "friend") { return _("friend"); }
    if (type == "conference") { return _("conference_item"); }
    if (type == "group") { return _("tabs.groups"); }
    if (type == kUnknownType) { return qFromUtf8("未知"); }
    if (type == kSyseventType) { return qFromUtf8("系统事件"); }
    if (type == kTopicType) { return qFromUtf8("主题"); }
    if (type == kBookmarkType) { return qFromUtf8("书签"); }
    return qFromUtf8(type.c_str());
}

QString trimStr(const QString& s) {
#ifdef QT3_BUILD
    return s.stripWhiteSpace();
#else
    return s.trimmed();
#endif
}

struct VirtualSeed {
    int id;
    const char* name;
    const char* type;
};

const VirtualSeed kVirtualSeeds[] = {
    { VIRTUAL_SEARCH_UNKNOWN_ID,  "Unknown",  kUnknownType },
    { VIRTUAL_SEARCH_SYSEVENT_ID, "Sysevent", kSyseventType },
    { VIRTUAL_SEARCH_REDDIT_ID,   "Reddit",   kTopicType },
    { VIRTUAL_SEARCH_BOOKMARK_ID, kBookmarkName, kBookmarkType },
};

const EventType34 SearchReadyType = toEventType34(QEvent::User + 107);

struct SearchMsgData {
    std::string chanName;
    std::string sender;
    std::string body;
    std::string time;
};

class SearchCompletedEvent : public CustomEventBase {
public:
    int seq;
    long long elapsedMs;
    std::vector<std::pair<std::string, std::string>> contacts;
    std::vector<SearchMsgData> messages;

    SearchCompletedEvent(int s, long long ms,
                         std::vector<std::pair<std::string, std::string>> c,
                         std::vector<SearchMsgData> m)
        : CustomEventBase(SearchReadyType), seq(s), elapsedMs(ms),
          contacts(std::move(c)), messages(std::move(m)) {}
};

}  // namespace

CombineSearch::CombineSearch(QWidget* parent)
    : QDialog(parent
#ifdef QT3_BUILD
          , nullptr, false, WDestructiveClose
#endif
          ) {
#ifndef QT3_BUILD
    setAttribute(Qt::WA_DeleteOnClose);
#endif
    qSetWindowTitle(this, _("combine_search.title"));
    resize(650, 550);
    setMinimumSize(650, 450);

    m_closed.store(false);
    m_canceled.store(false);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setMargin(8);
    root->setSpacing(6);

    QHBoxLayout* searchRow = new QHBoxLayout;
    m_input = new PlaceholderLineEdit(_("combine_search.placeholder"), this);
    m_input->setRealText("kernel");
    connect(m_input, SIGNAL(returnPressed()), this, SLOT(runSearch()));
    m_searchBtn = new QPushButton(_("combine_search.button"), this);
    connect(m_searchBtn, SIGNAL(clicked()), this, SLOT(runSearch()));
    m_cancelBtn = new QPushButton(_("combine_search.cancel"), this);
    m_cancelBtn->setEnabled(false);
    connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(onCancelClicked()));
    searchRow->addWidget(m_input, 1);
    searchRow->addWidget(m_searchBtn);
    searchRow->addWidget(m_cancelBtn);
    root->addLayout(searchRow);

    QHBoxLayout* tabRow = new QHBoxLayout;
    tabRow->setSpacing(0);
    m_tabContacts = new QPushButton(_("combine_search.tabs.contacts"), this);
    m_tabMessages = new QPushButton(_("combine_search.tabs.messages"), this);
    QPushButton* tabs[2] = { m_tabContacts, m_tabMessages };
    for (int i = 0; i < 2; ++i) {
        qSetCheckable(tabs[i], true);
        tabs[i]->setFixedHeight(26);
        connect(tabs[i], SIGNAL(clicked()), this, SLOT(onTabClicked()));
        tabRow->addWidget(tabs[i]);
        m_tabButtons.push_back(tabs[i]);
    }
    tabRow->addStretch(1);
    qSetChecked(m_tabContacts, true);
    root->addLayout(tabRow);

    m_stack = new StackedWidget(this);
    QWidget* contactsPage = new QWidget(m_stack);
    QWidget* messagesPage = new QWidget(m_stack);

    QWidget* inner = nullptr;
    ScrollArea* scroll = makeScrollArea(contactsPage, inner);
    (new QVBoxLayout(inner))->addStretch(1);
    (new QVBoxLayout(contactsPage))->addWidget(scroll);
    m_contactsInner = inner;
    m_contactsScroll = scroll;
    m_pages.push_back(contactsPage);
    m_stack->addWidget(contactsPage);

    m_viewMsg = new SearchListView(messagesPage);
    m_msgBar = new LimeScrollBar(Qt::Vertical, messagesPage);
    m_viewMsg->setScrollBar(m_msgBar);
    connect(m_msgBar, SIGNAL(valueChanged(int)), this, SLOT(onViewScroll(int)));
    QHBoxLayout* msgRow = new QHBoxLayout(messagesPage);
    msgRow->setSpacing(0);
    msgRow->addWidget(m_viewMsg, 1);
    msgRow->addWidget(m_msgBar);
    m_pages.push_back(messagesPage);
    m_stack->addWidget(messagesPage);

    root->addWidget(m_stack, 10);
    qStackSetCurrent(m_stack, contactsPage);

    QHBoxLayout* pagerRow = new QHBoxLayout;
    m_firstBtn = new QPushButton(_("combine_search.page_first"), this);
    m_prevBtn = new QPushButton(_("combine_search.page_prev"), this);
    m_nextBtn = new QPushButton(_("combine_search.page_next"), this);
    m_lastBtn = new QPushButton(_("combine_search.page_last"), this);
    connect(m_firstBtn, SIGNAL(clicked()), this, SLOT(goFirst()));
    connect(m_prevBtn, SIGNAL(clicked()), this, SLOT(goPrev()));
    connect(m_nextBtn, SIGNAL(clicked()), this, SLOT(goNext()));
    connect(m_lastBtn, SIGNAL(clicked()), this, SLOT(goLast()));
    m_pageLabel = new QLabel(this);
    pagerRow->addStretch(1);
    pagerRow->addWidget(m_firstBtn);
    pagerRow->addWidget(m_prevBtn);
    pagerRow->addWidget(m_pageLabel);
    pagerRow->addWidget(m_nextBtn);
    pagerRow->addWidget(m_lastBtn);
    pagerRow->addStretch(1);
    root->addLayout(pagerRow);

    m_statusLabel = new QLabel(this);
#ifdef QT3_BUILD
    m_statusLabel->setPaletteForegroundColor(QColor(0x70, 0x70, 0x70));
#else
    m_statusLabel->setStyleSheet("color:#707070");
#endif
    root->addWidget(m_statusLabel);

    m_firstBtn->setEnabled(false);
    m_prevBtn->setEnabled(false);
    showPage(0, 0);
}

void CombineSearch::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QDialog::keyPressEvent(e);
}

int CombineSearch::activeTab() const {
#ifdef QT3_BUILD
    return m_tabMessages->isOn() ? 1 : 0;
#else
    return m_tabMessages->isChecked() ? 1 : 0;
#endif
}

void CombineSearch::onTabClicked() {
    int idx = activeTab();
    qStackSetCurrent(m_stack, m_pages[idx]);
    showPage(idx, m_curPage[idx]);
}

void CombineSearch::onViewScroll(int v) {
    m_viewMsg->scrollTo(v);
}

int CombineSearch::rowTotal(int tab) const {
    const std::vector<RowInfo>& rows = (tab == 0) ? m_contactRows : m_messageRows;
    if (rows.empty()) { return 1; }
    return ((int)rows.size() + 49) / 50;
}

void CombineSearch::renderSlice(int tab, int page) {
    const std::vector<RowInfo>& rows = (tab == 0) ? m_contactRows : m_messageRows;
    int total = rows.empty() ? 1 : ((int)rows.size() + 49) / 50;
    if (page < 0) { page = 0; }
    if (page >= total) { page = total - 1; }

    if (tab == 0) {
        QVBoxLayout* lay = static_cast<QVBoxLayout*>(m_contactsInner->layout());
        clearLayout(lay);
        if (rows.empty()) {
            lay->addWidget(new QLabel(_("combine_search.no_contacts"), m_contactsInner));
            lay->addStretch(1);
        } else {
            int off = page * 50;
            int end = std::min(off + 50, (int)rows.size());
            for (int i = off; i < end; ++i) {
                lay->addWidget(makeRow(rows[i].title, rows[i].detail, m_contactsInner));
            }
            lay->addStretch(1);
        }
#ifdef QT3_BUILD
        QScrollView* sv = m_contactsScroll;
        int w = sv->visibleWidth();
        int h = m_contactsInner->sizeHint().height();
        if (h < sv->visibleHeight()) { h = sv->visibleHeight() - 2; }
        sv->resizeContents(w, h);
        sv->moveChild(m_contactsInner, 0, 0);
        m_contactsInner->resize(w, h);
        m_contactsInner->show();
        m_contactsInner->update();
        sv->viewport()->update();
#endif
    } else {
        std::vector<RowInfo> slice;
        if (!rows.empty()) {
            int off = page * 50;
            int end = std::min(off + 50, (int)rows.size());
            for (int i = off; i < end; ++i) {
                slice.push_back(rows[i]);
            }
        }
        m_viewMsg->setRows(slice, _("combine_search.no_messages"));
    }

    m_pageLabel->setText(_A("combine_search.page_indicator",
                            QStringList() << QString::number(page + 1)
                                          << QString::number(total)));
    m_firstBtn->setEnabled(page > 0);
    m_prevBtn->setEnabled(page > 0);
    m_nextBtn->setEnabled(page < total - 1);
    m_lastBtn->setEnabled(page < total - 1);
}

void CombineSearch::showPage(int tab, int page) {
    renderSlice(tab, page);
}

void CombineSearch::goFirst() {
    int tab = activeTab();
    m_curPage[tab] = 0;
    showPage(tab, 0);
}

void CombineSearch::goPrev() {
    int tab = activeTab();
    if (m_curPage[tab] <= 0) { return; }
    --m_curPage[tab];
    showPage(tab, m_curPage[tab]);
}

void CombineSearch::goNext() {
    int tab = activeTab();
    if (m_curPage[tab] + 1 >= rowTotal(tab)) { return; }
    ++m_curPage[tab];
    showPage(tab, m_curPage[tab]);
}

void CombineSearch::goLast() {
    int tab = activeTab();
    m_curPage[tab] = rowTotal(tab) - 1;
    showPage(tab, m_curPage[tab]);
}

QWidget* CombineSearch::makeRow(const QString& title, const QString& detail, QWidget* host) {
    QWidget* row = new QWidget(host);
    QVBoxLayout* lay = new QVBoxLayout(row);
    lay->setMargin(6);
    lay->setSpacing(2);

    QLabel* t = new QLabel(qElideChars(title, 80, ElideRight), row);
    QFont f = t->font();
    f.setBold(true);
    t->setFont(f);

    QLabel* d = new QLabel(qElideChars(detail, 120, ElideRight), row);
#ifdef QT3_BUILD
    d->setPaletteForegroundColor(QColor(0x70, 0x70, 0x70));
#else
    d->setStyleSheet("color:#707070");
#endif

    lay->addWidget(t);
    lay->addWidget(d);
    return row;
}

void CombineSearch::clearLayout(QLayout* lay) {
    if (!lay) { return; }
#ifdef QT3_BUILD
    QLayoutIterator it = lay->iterator();
    while (it.current()) {
        QLayoutItem* cur = it.current();
        QWidget* w = cur->widget();
        it.deleteCurrent();
        if (w) { delete w; }
    }
#else
    QLayoutItem* it;
    while ((it = lay->takeAt(0))) {
        if (it->widget()) { delete it->widget(); }
        delete it;
    }
#endif
}

void CombineSearch::runSearch() {
    if (m_searching) { return; }

    m_searching = true;
    m_canceled.store(false);
    ++m_searchSeq;
    int seq = m_searchSeq;

    m_contacts.clear();
    m_messages.clear();
    m_contactRows.clear();
    m_messageRows.clear();

    QString query = trimStr(m_input->text());
    std::string qquery;
    if (!query.isEmpty()) {
        QByteArray qb = qToUtf8(query);
        qquery.assign(qb.data(), (size_t)qb.size());
    }

    m_statusLabel->setText(_("combine_search.searching"));
    m_searchBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);
    m_firstBtn->setEnabled(false);
    m_prevBtn->setEnabled(false);
    m_nextBtn->setEnabled(false);
    m_lastBtn->setEnabled(false);
    m_curPage[0] = 0;
    m_curPage[1] = 0;
    showPage(activeTab(), 0);

    std::thread t([this, seq, qquery] {
        std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();

        std::vector<ContactHit> contactHits;
        std::vector<MessageHit> messageHits;

        if (!qquery.empty()) {
            QString q = qFromUtf8(qquery.c_str());
            QString upperQ = qToUpper(q);

            std::map<std::pair<int, std::string>, QString> nameMap;
            {
                auto channels = Storage::instance().channelDb()->load_all_channels();
                for (const auto& row : channels) {
                    auto sep = row.chanid.rfind('_');
                    if (sep == std::string::npos) { continue; }
                    std::string type = row.chanid.substr(0, sep);
                    int id = 0;
                    try {
                        id = std::stoi(row.chanid.substr(sep + 1));
                    } catch (...) {
                        continue;
                    }
                    QString name = qFromUtf8(row.name.c_str());
                    nameMap[std::make_pair(id, type)] = name;
                    if (qToUpper(name).contains(upperQ)) {
                        ContactHit h;
                        h.id = id;
                        h.type = type;
                        h.name = name;
                        contactHits.push_back(h);
                    }
                }
            }
            for (const VirtualSeed& s : kVirtualSeeds) {
                QString name = qFromUtf8(s.name);
                if (qToUpper(name).contains(upperQ)) {
                    ContactHit h;
                    h.id = s.id;
                    h.type = s.type;
                    h.name = name;
                    contactHits.push_back(h);
                }
            }
            if (m_canceled.load()) { return; }

            std::vector<int64_t> ids = Storage::instance().messageDb()->search_messages(
                qquery.c_str(), 100);
            int n = 0;
            for (int64_t rowid : ids) {
                std::unique_ptr<MessageRow> row = Storage::instance().messageDb()->get_message(rowid);
                if (!row) { continue; }
                auto sep = row->chanid.rfind('_');
                if (sep == std::string::npos) { continue; }
                std::string type = row->chanid.substr(0, sep);
                int id = 0;
                try {
                    id = std::stoi(row->chanid.substr(sep + 1));
                } catch (...) {
                    continue;
                }
                MessageHit h;
                h.id = id;
                h.type = type;
                std::map<std::pair<int, std::string>, QString>::const_iterator it =
                    nameMap.find(std::make_pair(id, type));
                h.chanName = (it != nameMap.end()) ? it->second : qFromUtf8(row->chanid.c_str());
                h.sender = qFromUtf8(row->sender_name.c_str());
                h.body = qFromUtf8(row->data.c_str());
                h.time = qFromUtf8(row->time_text.c_str());
                messageHits.push_back(h);
                if (++n % 25 == 0 && m_canceled.load()) { return; }
            }
        }
        if (m_canceled.load()) { return; }

        std::vector<std::pair<std::string, std::string>> contacts;
        contacts.reserve(contactHits.size());
        for (const auto& c : contactHits) {
            contacts.push_back(std::make_pair(std::string(qToUtf8(c.name).data()),
                                              c.type));
        }
        std::vector<SearchMsgData> messages;
        messages.reserve(messageHits.size());
        for (const auto& m : messageHits) {
            SearchMsgData d;
            d.chanName = std::string(qToUtf8(m.chanName).data());
            d.sender = std::string(qToUtf8(m.sender).data());
            d.body = std::string(qToUtf8(m.body).data());
            d.time = std::string(qToUtf8(m.time).data());
            messages.push_back(std::move(d));
        }

        long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();

        if (m_canceled.load() || m_closed.load()) { return; }

        QApplication::postEvent(this, new SearchCompletedEvent(
            seq, elapsed, std::move(contacts), std::move(messages)));
    });
    t.detach();
}

void CombineSearch::onCancelClicked() {
    m_searching = false;
    m_canceled.store(true);
    m_searchBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
    m_statusLabel->setText(_("combine_search.cancelled"));
    m_firstBtn->setEnabled(false);
    m_prevBtn->setEnabled(false);
    m_nextBtn->setEnabled(false);
    m_lastBtn->setEnabled(false);
    m_curPage[0] = 0;
    m_curPage[1] = 0;
    showPage(activeTab(), 0);
}

void CombineSearch::customEvent(CustomEventBase* event) {
    if (event->type() == SearchReadyType) {
        SearchCompletedEvent* e = static_cast<SearchCompletedEvent*>(event);
        if (m_canceled.load() || e->seq != m_searchSeq) { return; }

        m_contactRows.clear();
        for (size_t i = 0; i < e->contacts.size(); ++i) {
            RowInfo r;
            r.title = qFromUtf8(e->contacts[i].first.c_str());
            r.detail = typeLabel(e->contacts[i].second);
            m_contactRows.push_back(r);
        }
        m_messageRows.clear();
        for (size_t i = 0; i < e->messages.size(); ++i) {
            RowInfo r;
            QString sender = qFromUtf8(e->messages[i].sender.c_str());
            if (sender.isEmpty()) {
                sender = qFromUtf8("未知");
            }
            r.title = qFromUtf8("[") + sender + qFromUtf8("] ")
                      + qFromUtf8(e->messages[i].body.c_str());
            r.detail = qFromUtf8(e->messages[i].chanName.c_str())
                       + qFromUtf8(" · ") + qFromUtf8(e->messages[i].time.c_str());
            m_messageRows.push_back(r);
        }
        for (int i = 0; i < kTestRows; ++i) {
            RowInfo r;
            r.title = qFromUtf8("TEST-") + QString::number(i + 1)
                      + qFromUtf8(" 测试数据 ") + QString::number(i + 1);
            r.detail = qFromUtf8("test_chan") + qFromUtf8(" 渲染/滚动/分页验证用行");
            m_messageRows.push_back(r);
        }

        int totalHit = (int)(m_contactRows.size() + m_messageRows.size()) - kTestRows;
        m_statusLabel->setText(_A("combine_search.result_summary",
                                  QStringList() << QString::number(totalHit)
                                                << QString::number(e->elapsedMs)));

        m_searching = false;
        m_searchBtn->setEnabled(true);
        m_cancelBtn->setEnabled(false);

        bool hasMsg = (m_messageRows.size() > (size_t)kTestRows);
        bool hasContacts = (!m_contactRows.empty());
        int targetTab = (hasMsg || !hasContacts) ? 1 : 0;
        if (targetTab == 1) {
            qSetChecked(m_tabMessages, true);
        } else {
            qSetChecked(m_tabContacts, true);
        }
        qStackSetCurrent(m_stack, m_pages[targetTab]);
        m_curPage[0] = 0;
        m_curPage[1] = 0;
        showPage(targetTab, 0);
        return;
    }
    QDialog::customEvent(event);
}

void CombineSearch::closeEvent(QCloseEvent* e) {
    m_closed.store(true);
    m_canceled.store(true);
    QApplication::removePostedEvents(this);
    QDialog::closeEvent(e);
}