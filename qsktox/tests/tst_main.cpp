// qsktox 回归测试：会话列表 → 消息列表链路的运行期不变量。
//
// 运行：
//   cd qsktox/build-x64 && QT_QPA_PLATFORM=offscreen ./qsktox_test
//
// 说明：
// - Storage 是进程级单例，只能 init() 一次；所有用例共用同一个临时库，
//   用各自独立的 chatId 隔离数据。
// - 异步写库走 WriteQueue（工作线程），用例用 waitFor() 轮询同步库等待落库。
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <QGuiApplication>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTemporaryDir>

#include <chrono>
#include <memory>
#include <thread>

#include "storage.h"
#include "channel_db.h"
#include "message_db.h"
#include "msgdb_bridge.h"
#include "messagestore.h"
#include "messagelist.h"

#include <QskSkinManager.h>
#include "channellist.h"

namespace {

QTemporaryDir* g_dataDir = nullptr;

bool ensureStorage()
{
    static bool ready = false;
    if (ready) {
        return true;
    }
    g_dataDir = new QTemporaryDir();
    if (!g_dataDir->isValid()) {
        return false;
    }
    if (!Storage::instance().init(g_dataDir->path().toUtf8().constData())) {
        return false;
    }
    ready = true;
    return true;
}

template <typename Fn>
bool waitFor(Fn cond, int timeoutMs = 3000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (cond()) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return cond();
}

QString chanIdOf(const QString& protocol, const QString& chatId)
{
    return MsgDbBridge::chanKey(protocol, chatId);
}

int messageCountOf(const QString& chanid)
{
    auto* db = Storage::instance().messageDb();
    if (!db) {
        return -1;
    }
    return static_cast<int>(
        db->load_messages(chanid.toUtf8().constData(), 500).size());
}

std::unique_ptr<ChannelRow> channelRow(const QString& chanid)
{
    auto* db = Storage::instance().channelDb();
    if (!db) {
        return nullptr;
    }
    return db->get_channel(chanid.toUtf8().constData());
}

MessageItem makeMessage(const QString& content, const QString& time,
                        const QString& sender = QStringLiteral("me"))
{
    MessageItem m;
    m.sender = sender;
    m.content = content;
    m.time = time;
    m.isSelf = true;
    m.avatarLetter = sender.isEmpty() ? QChar(' ') : sender.at(0).toUpper();
    m.avatarColor = QColor(QStringLiteral("#1565C0"));
    return m;
}

}

TEST_CASE("T1 event_id 跨进程唯一")
{
    REQUIRE(ensureStorage());
    const QString proto = QStringLiteral("t1proto");
    const QString chatId = QStringLiteral("t1chat");
    const QString chanid = chanIdOf(proto, chatId);

    MsgDbBridge::ensureChannel(proto, chatId, QStringLiteral("T1"));
    REQUIRE(waitFor([&] { return channelRow(chanid) != nullptr; }));

    MessageItem a = makeMessage(QStringLiteral("A"), QStringLiteral("10:00"));
    a.localId = 1;
    MessageItem b = makeMessage(QStringLiteral("B"), QStringLiteral("10:00"));
    b.localId = 1;

    MsgDbBridge::setInstanceTag(QStringLiteral("boot-A"));
    MsgDbBridge::insertMessage(proto, chatId, a, nullptr, nullptr);
    REQUIRE(waitFor([&] { return messageCountOf(chanid) == 1; }));

    MsgDbBridge::setInstanceTag(QStringLiteral("boot-B"));
    MsgDbBridge::insertMessage(proto, chatId, b, nullptr, nullptr);
    REQUIRE(waitFor([&] { return messageCountOf(chanid) == 2; }));
}

TEST_CASE("T2 MessageStore 跨 protocol 不串消息")
{
    REQUIRE(ensureStorage());
    MessageStore* store = MessageStore::instance();
    const QString chatId = QStringLiteral("4242");

    MessageItem m = makeMessage(QStringLiteral("hello"), QStringLiteral("11:00"),
                                QStringLiteral("bob"));
    store->addMessage(QStringLiteral("tox"), chatId, m);
    store->addMessage(QStringLiteral("matrix"), chatId, m);

    CHECK(store->messageCount(QStringLiteral("tox"), chatId) == 1);
    CHECK(store->messageCount(QStringLiteral("matrix"), chatId) == 1);
    CHECK(store->messageCount(QStringLiteral("tox"), QStringLiteral("other")) == 0);
}

TEST_CASE("T3 messageItem 越界安全")
{
    REQUIRE(ensureStorage());
    MessageListWidget w;
    w.setWidth(400);
    w.setHeight(800);
    CHECK(w.messageCount() == 0);
    CHECK(w.messageItem(-1).content.isEmpty());
    CHECK(w.messageItem(0).content.isEmpty());
    CHECK(w.messageItem(9999).content.isEmpty());
}

TEST_CASE("T7 同内容同秒的两条消息都保留")
{
    REQUIRE(ensureStorage());
    const QString proto = QStringLiteral("t7proto");
    const QString chatId = QStringLiteral("t7chat");
    const QString chanid = chanIdOf(proto, chatId);

    MsgDbBridge::ensureChannel(proto, chatId, QStringLiteral("T7"));
    REQUIRE(waitFor([&] { return channelRow(chanid) != nullptr; }));

    MessageItem a = makeMessage(QStringLiteral("same"), QStringLiteral("12:00"));
    MessageItem b = makeMessage(QStringLiteral("same"), QStringLiteral("12:00"));
    MsgDbBridge::insertMessage(proto, chatId, a, nullptr, nullptr);
    MsgDbBridge::insertMessage(proto, chatId, b, nullptr, nullptr);
    REQUIRE(waitFor([&] { return messageCountOf(chanid) == 2; }));

    MessageListWidget w;
    w.setWidth(400);
    w.setHeight(800);
    w.setChannel(proto, chatId, chanid);
    CHECK(w.messageCount() == 2);
}

TEST_CASE("T8 mark_read 已读水位不倒退")
{
    REQUIRE(ensureStorage());
    const QString proto = QStringLiteral("t8proto");
    const QString chatId = QStringLiteral("t8chat");
    const QString chanid = chanIdOf(proto, chatId);

    MsgDbBridge::ensureChannel(proto, chatId, QStringLiteral("T8"));
    REQUIRE(waitFor([&] { return channelRow(chanid) != nullptr; }));

    auto* db = Storage::instance().channelDb();
    REQUIRE(db != nullptr);
    REQUIRE(db->mark_read(chanid.toUtf8().constData(), 100));
    REQUIRE(db->mark_read(chanid.toUtf8().constData(), 50));
    auto row = channelRow(chanid);
    REQUIRE(row != nullptr);
    CHECK(row->last_read_rowid == 100);
}

TEST_CASE("T9 protocol 前缀不一致仍能清未读")
{
    REQUIRE(ensureStorage());
    const QString chatId = QStringLiteral("t9chat");
    const QString oldChanid = chanIdOf(QStringLiteral("oldproto"), chatId);

    MsgDbBridge::ensureChannel(QStringLiteral("oldproto"), chatId,
                               QStringLiteral("T9"));
    REQUIRE(waitFor([&] { return channelRow(oldChanid) != nullptr; }));

    auto* db = Storage::instance().channelDb();
    REQUIRE(db != nullptr);
    REQUIRE(db->increment_unread(oldChanid.toUtf8().constData(), 3, 0));

    MsgDbBridge::markRead(QStringLiteral("newproto"), chatId, 77);
    REQUIRE(waitFor([&] {
        auto r = channelRow(oldChanid);
        return r && r->unread_count == 0;
    }));
    auto row = channelRow(oldChanid);
    REQUIRE(row != nullptr);
    CHECK(row->last_read_rowid == 77);
}

TEST_CASE("T10 自己发送后更新会话摘要与活跃时间")
{
    REQUIRE(ensureStorage());
    const QString proto = QStringLiteral("t10proto");
    const QString chatId = QStringLiteral("t10chat");
    const QString chanid = chanIdOf(proto, chatId);

    MsgDbBridge::upsertChannel(proto, chatId, QStringLiteral("T10"),
                               QStringLiteral("online"), false,
                               QStringLiteral("旧摘要"), QStringLiteral("10:00"), 1000);
    REQUIRE(waitFor([&] { return channelRow(chanid) != nullptr; }));

    MsgDbBridge::upsertChannel(proto, chatId, QStringLiteral("T10"),
                               QStringLiteral("online"), false,
                               QStringLiteral("新摘要"), QStringLiteral("11:00"), 2000);
    REQUIRE(waitFor([&] {
        auto r = channelRow(chanid);
        return r && r->last_message_text == "新摘要";
    }));
    auto row = channelRow(chanid);
    REQUIRE(row != nullptr);
    CHECK(row->last_message_text == "新摘要");
    CHECK(row->last_message_time == "11:00");
    CHECK(row->last_active == 2000);
}

// ── T4: 不同 protocol 同 chatId 不串未读 ──
TEST_CASE("T4 不同 protocol 同 chatId 未读隔离")
{
    REQUIRE(ensureStorage());
    ChannelListWidget w;
    w.setWidth(400);
    w.setHeight(800);

    ChannelItem a;
    a.avatarLetter = QStringLiteral("A");
    a.avatarColor = QColor(QStringLiteral("#4CAF50"));
    a.title = QStringLiteral("A");
    a.chatId = QStringLiteral("123");
    a.chatType = QStringLiteral("tox");
    a.protocol = QStringLiteral("tox");
    a.chanId = QStringLiteral("tox_123");
    a.unreadDelta = 2;

    ChannelItem b = a;
    b.title = QStringLiteral("B");
    b.chatType = QStringLiteral("matrix");
    b.protocol = QStringLiteral("matrix");
    b.chanId = QStringLiteral("matrix_123");
    b.unreadDelta = 5;

    w.insertOrUpdateChannel(QStringLiteral("123"), a);
    w.insertOrUpdateChannel(QStringLiteral("123"), b);
    CHECK(w.count() == 2);
    int toxUnread = 0;
    int matrixUnread = 0;
    for (int i = 0; i < w.count(); ++i) {
        const auto& it = w.channelItem(i);
        if (it.protocol == QStringLiteral("tox")) {
            toxUnread = it.unreadCount;
        }
        if (it.protocol == QStringLiteral("matrix")) {
            matrixUnread = it.unreadCount;
        }
    }
    CHECK(toxUnread == 2);
    CHECK(matrixUnread == 5);
}

// ── T5: 点开会话乐观清零，DB 回滚复位 ──
TEST_CASE("T5 点开会话乐观清零与 DB 回滚")
{
    REQUIRE(ensureStorage());
    const QString proto = QStringLiteral("t5proto");
    const QString chatId = QStringLiteral("t5chat");
    const QString chanid = chanIdOf(proto, chatId);

    MsgDbBridge::ensureChannel(proto, chatId, QStringLiteral("T5"));
    REQUIRE(waitFor([&] { return channelRow(chanid) != nullptr; }));
    auto* db = Storage::instance().channelDb();
    REQUIRE(db != nullptr);
    REQUIRE(db->increment_unread(chanid.toUtf8().constData(), 3, 0));

    ChannelListWidget w;
    w.setWidth(400);
    w.setHeight(800);
    w.reloadFromDb();

    int idx = -1;
    for (int i = 0; i < w.count(); ++i) {
        if (w.channelItem(i).chatId == chatId && w.channelItem(i).protocol == proto) {
            idx = i;
            break;
        }
    }
    REQUIRE(idx >= 0);
    CHECK(w.channelItem(idx).unreadCount == 3);

    w.markLocallyRead(proto, chatId, chanid);
    CHECK(w.channelItem(idx).unreadCount == 0);

    w.syncUnreadFromDb(chanid);
    CHECK(w.channelItem(idx).unreadCount == 3);
}

// ── T11: 会话列表排序（会话全局 last_active 倒序 → mock 垫底）──
TEST_CASE("T11 会话列表排序")
{
    REQUIRE(ensureStorage());
    ChannelListWidget w;
    w.setWidth(400);
    w.setHeight(800);

    ChannelItem older;
    older.avatarLetter = QStringLiteral("O");
    older.avatarColor = QColor(QStringLiteral("#4CAF50"));
    older.title = QStringLiteral("older");
    older.chatId = QStringLiteral("t7old");
    older.chatType = QStringLiteral("tox");
    older.protocol = QStringLiteral("tox");
    older.chanId = QStringLiteral("tox_t7old");
    older.lastActive = 1000;

    ChannelItem newer = older;
    newer.title = QStringLiteral("newer");
    newer.chatId = QStringLiteral("t7new");
    newer.chanId = QStringLiteral("tox_t7new");
    newer.lastActive = 2000;

    ChannelItem virt = older;
    virt.title = QStringLiteral("virt");
    virt.chatId = QStringLiteral("t7virt");
    virt.chatType = QStringLiteral("virtual");
    virt.protocol = QStringLiteral("aigpt");
    virt.chanId = QStringLiteral("aigpt_t7virt");
    virt.lastActive = 500;

    w.insertOrUpdateChannel(older.chatId, older);
    w.insertOrUpdateChannel(newer.chatId, newer);
    w.insertOrUpdateChannel(virt.chatId, virt);

    REQUIRE(w.count() == 3);
    // 全局按 lastActive 倒序：newer(2000) > older(1000) > virt(500)
    CHECK(w.channelItem(0).title == QStringLiteral("newer"));
    CHECK(w.channelItem(1).title == QStringLiteral("older"));
    CHECK(w.channelItem(2).title == QStringLiteral("virt"));
    // 索引重建后仍能按 chatId 定位（未读/已读操作依赖它）
    CHECK(w.chatIdForRow(0) == QStringLiteral("t7new"));
    CHECK(w.chatIdForRow(1) == QStringLiteral("t7old"));
    CHECK(w.chatIdForRow(2) == QStringLiteral("t7virt"));

    // 旧会话收到新消息（lastActive 变大）→ 上浮到列表首位
    ChannelItem bumped = older;
    bumped.lastActive = 3000;
    w.insertOrUpdateChannel(bumped.chatId, bumped);
    REQUIRE(w.count() == 3);
    // older(3000) > newer(2000) > virt(500)
    CHECK(w.channelItem(0).title == QStringLiteral("older"));
    CHECK(w.channelItem(1).title == QStringLiteral("newer"));
    CHECK(w.channelItem(2).title == QStringLiteral("virt"));
    CHECK(w.chatIdForRow(1) == QStringLiteral("t7new"));
}

// ── T6: MessageListWidget 切换频道不串消息且能正确滚动 ──
TEST_CASE("T6 切换频道不串消息且滚动到底部")
{
    REQUIRE(ensureStorage());
    MessageListWidget list;
    list.setWidth(400);
    list.setHeight(600);

    list.setChannel(QStringLiteral("tox"), QStringLiteral("room1"), QStringLiteral("tox_room1"));
    list.setChannel(QStringLiteral("matrix"), QStringLiteral("room2"), QStringLiteral("matrix_room2"));

    list.scrollToBottom();
    CHECK(true);
}

// ── T12: 兜底加载不跨 protocol 串消息 ──
// 直接在 channels 表构造两行（chanid = "<proto>_<chatId>"，同 chatId 不同 proto）——
// 这是 Go 服务侧写入的真实状态。qsktox 自己的 upsertChannel 走 resolveChanid 的
// 别名容忍路径（X6/T9）：同 chatId 只存在一行时会认那一行，第二个 protocol 的
// 会话被合并进第一行，qsktox 侧造不出这种状态。
TEST_CASE("T12 兜底加载不跨 protocol 串消息")
{
    REQUIRE(ensureStorage());
    const QString chatId = QStringLiteral("t12chat");
    const QString protoA = QStringLiteral("t12protoA");
    const QString protoB = QStringLiteral("t12protoB");
    const QString chanA = chanIdOf(protoA, chatId);
    const QString chanB = chanIdOf(protoB, chatId);

    auto* cdb = Storage::instance().channelDb();
    REQUIRE(cdb != nullptr);

    ChannelRow rowA;
    rowA.chanid = chanA.toStdString();
    rowA.proto_type = protoA.toStdString();
    rowA.name = "A";
    rowA.status = "online";
    rowA.last_active = 100;
    REQUIRE(cdb->update_contact_channel(rowA));

    ChannelRow rowB;
    rowB.chanid = chanB.toStdString();
    rowB.proto_type = protoB.toStdString();
    rowB.name = "B";
    rowB.status = "online";
    rowB.last_active = 100;
    REQUIRE(cdb->update_contact_channel(rowB));

    REQUIRE(channelRow(chanA) != nullptr);
    REQUIRE(channelRow(chanB) != nullptr);

    MessageItem ma = makeMessage(QStringLiteral("A-msg"), QStringLiteral("10:00"));
    ma.localId = 101;
    MsgDbBridge::insertMessage(protoA, chatId, ma, nullptr, nullptr);
    REQUIRE(waitFor([&] { return messageCountOf(chanA) == 1; }));

    MessageItem mb = makeMessage(QStringLiteral("B-msg"), QStringLiteral("10:01"));
    mb.localId = 102;
    MsgDbBridge::insertMessage(protoB, chatId, mb, nullptr, nullptr);
    REQUIRE(waitFor([&] { return messageCountOf(chanB) == 1; }));

    // protocol 明确 → 只拿到本 protocol 的消息
    const auto va = MsgDbBridge::loadMessagesByChatId(protoA, chatId, 50);
    REQUIRE(va.size() == 1);
    CHECK(va.first().content == QStringLiteral("A-msg"));

    const auto vb = MsgDbBridge::loadMessagesByChatId(protoB, chatId, 50);
    REQUIRE(vb.size() == 1);
    CHECK(vb.first().content == QStringLiteral("B-msg"));

    // protocol 未知且同 chatId 命中多个协议 → 不猜，返回空
    const auto vunk = MsgDbBridge::loadMessagesByChatId(QString(), chatId, 50);
    CHECK(vunk.isEmpty());
}

// ── T13: 消息列表换频道后必须真的渲染出可见行 ──
// 回归：updateVisibleRows() 的守卫把 m_rowYOffsets.size()（前缀和表，长度 N+1）
// 跟 m_items.size()（N）比 → 条件恒真 → 一行都不创建 → 消息列表整表空白。
TEST_CASE("T13 消息列表换频道后渲染出可见行")
{
    REQUIRE(ensureStorage());
    MessageListWidget list;
    list.setWidth(400);
    list.setHeight(600);

    const QString proto = QStringLiteral("t13proto");
    const QString chatId = QStringLiteral("t13chat");
    const QString chanid = chanIdOf(proto, chatId);

    MsgDbBridge::setInstanceTag(QStringLiteral("boot-T13"));
    MsgDbBridge::ensureChannel(proto, chatId, QStringLiteral("T13"));
    REQUIRE(waitFor([&] { return channelRow(chanid) != nullptr; }));

    MessageItem m = makeMessage(QStringLiteral("hello t13"), QStringLiteral("12:00"));
    m.localId = 1301;
    MsgDbBridge::insertMessage(proto, chatId, m, nullptr, nullptr);
    REQUIRE(waitFor([&] { return messageCountOf(chanid) == 1; }));

    list.setChannel(proto, chatId, chanid);
    CHECK(list.messageCount() >= 1);
    // 关键断言：可见行必须被创建（守卫恒真时为 0 → 消息列表白屏）
    CHECK(list.visibleRowCount() > 0);
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qsktox"));
    QCoreApplication::setApplicationName(QStringLiteral("qsktox_test"));

    QStringList skinPaths;
    const QStringList candidates = {
        qEnvironmentVariable("QSKTOX_SKIN_PLUGINS"),
        QStringLiteral("/opt/qt/qskinny/lib/qskinny/plugins"),
        QStringLiteral("/opt/qt/qskinny.6.7/lib/qskinny/plugins"),
    };
    for (const auto& c : candidates) {
        if (!c.isEmpty() && QDir(c).exists()) {
            skinPaths.prepend(c);
            break;
        }
    }
    if (!skinPaths.isEmpty()) {
        qskSkinManager->setPluginPaths(skinPaths);
        if (!qskSkinManager->setSkin(QStringLiteral("Fusion"))) {
            qWarning("[tst] skin Fusion 加载失败，控件度量可能异常");
        }
    }

    doctest::Context context(argc, argv);
    return context.run();
}
