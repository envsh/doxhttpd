#include "messagelist.h"
#include "messagestore.h"
#include "msgdb_bridge.h"
#include <private/qquicktaphandler_p.h>
#include <private/qquickpincharea_p.h>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QEasingCurve>
#include <QQuickWindow>
#include <QtMath>
#include <QskEvent.h>
#include <QTimer>
#include <QTextLayout>
#include <QTextOption>

static constexpr qreal MIN_ROW_HEIGHT = 60;
static constexpr qreal NAME_HEIGHT = 20;
static constexpr qreal TIME_HEIGHT = 14;
static constexpr qreal BUBBLE_PADDING_H = 12;
static constexpr qreal BUBBLE_PADDING_V = 8;
static constexpr qreal SIDE_MARGIN = 12;
static constexpr qreal AVATAR_SIZE = 36;
static constexpr qreal AVATAR_SPACING = 8;

static const QColor s_msgPalette[] = {
    QColor("#4CAF50"), QColor("#2196F3"), QColor("#FF9800"), QColor("#E91E63"), QColor("#9C27B0"), QColor("#00BCD4"), QColor("#FF5722"), QColor("#795548"), QColor("#607D8B"),
};
static constexpr int s_msgPaletteSize = sizeof(s_msgPalette) / sizeof(s_msgPalette[0]);

inline QString msgFirstLetter(const QString& s)
{
    if (s.isEmpty()) return QString(" ");
    return QString(s.at(0)).toUpper();
}

inline QColor msgColorForName(const QString& s)
{
    if (s.isEmpty()) return QColor("#607D8B");
    return s_msgPalette[qHash(s) % s_msgPaletteSize];
}

// ═══════════════════════════════════════════════════════════════════
// 120 条共享模拟消息数据（长短不一）
// ═══════════════════════════════════════════════════════════════════

static const MessageItem s_mockMessages[] = {
    {"Alice", "大家好！今天天气不错 ☀️", "08:00", false},
    {"Bob", "是啊", "08:01", false},
    {"我", "走！下午有空的举手 🙋", "08:02", true},
    {"Charlie", "我我我！", "08:02", false},
    {"Alice", "那就下午两点公园见，大家别忘了带水和防晒霜，上次Dave就晒脱皮了哈哈", "08:03", false},
    {"Dave", "别提了…那次真的是惨痛教训，不过这次我会全副武装的，帽子防晒衣一个不落", "08:04", false},
    {"我", "我带点水果来，西瓜和葡萄", "08:05", true},
    {"Eve", "太好了", "08:06", false},
    {"Frank", "有没有人想打羽毛球？我可以带拍子，两副，够用", "08:10", false},
    {"我", "我可以打！", "08:11", true},
    {"Grace", "我也想打，三个人正好轮换，打累了还能去飞盘那边歇会儿", "08:12", false},
    {"Alice", "那我和Dave飞盘，你们三个羽毛球，完美分工！记得拍视频发群里", "08:13", false},
    {"Bob", "👍", "08:14", false},
    {"我", "下午见！", "08:15", true},
    {"Charlie", "等等，我中午请大家吃火锅怎么样？新开的那家，评价特别好，人均大概80块左右", "09:00", false},
    {"Dave", "真的吗！太好了 🍲", "09:01", false},
    {"Eve", "火锅！我的最爱！", "09:01", false},
    {"我", "那先吃火锅再运动，完美", "09:02", true},
    {"Frank", "在哪吃？", "09:03", false},
    {"Charlie", "学校旁边那家新开的，叫'蜀味轩'，我同事说锅底特别正宗，牛油锅底是招牌", "09:04", false},
    {"Grace", "听说那家很好吃，排队排很长，周末可能要等一两个小时", "09:05", false},
    {"Alice", "早点去应该不用排", "09:06", false},
    {"我", "我 11 点就到了", "09:07", true},
    {"Bob", "这么早？你是想先吃一顿吗 😂", "09:08", false},
    {"我", "哈哈哈不是，先占位子", "09:09", true},
    {"Charlie", "行，那 11 点门口集合，我来订位，8个人够吗？", "09:10", false},
    {"Dave", "收到 ✅", "09:11", false},
    {"Eve", "好", "09:11", false},
    {"Frank", "👍", "09:12", false},
    {"Grace", "今天真开心", "09:13", false},
    {"我", "对了，有人带充电宝吗？我手机快没电了，昨天忘了充", "09:30", true},
    {"Alice", "我带了两个，一个10000mAh一个20000mAh，够用", "09:31", false},
    {"Bob", "我还带了蓝牙音箱，可以放音乐 🎵 JBL的，音质还不错", "09:32", false},
    {"我", "太棒了！放点轻松的", "09:33", true},
    {"Charlie", "放周杰伦！", "09:34", false},
    {"Dave", "必须的", "09:34", false},
    {"Eve", "我喜欢听 lofi hip hop，放松", "09:35", false},
    {"Frank", "都行都行，有音乐就行", "09:36", false},
    {"Grace", "我来放吧，我的歌单很全，从流行到摇滚到古典都有", "09:37", false},
    {"我", "交给你了 🎧", "09:38", true},
    {"Alice", "今天有人拍照吗？", "10:00", false},
    {"Bob", "我带了相机，Canon 的，还带了三脚架，可以拍延时", "10:01", false},
    {"我", "太好了，帮我拍几张帅的", "10:02", true},
    {"Charlie", "哈哈", "10:03", false},
    {"Dave", "这嘴甜的 🍬", "10:04", false},
    {"Eve", "拍合照吧！", "10:05", false},
    {"Frank", "到时候发朋友圈，九宫格那种", "10:06", false},
    {"Grace", "我来修图！Lightroom和Snapseed双管齐下，保证每张都美美的", "10:07", false},
    {"我", "专业修图师上线了", "10:08", true},
    {"Alice", "那今天就是完美的一天", "10:10", false},
    {"Bob", "对了，下午可能有点热，记得涂防晒，SPF50以上的最好", "10:30", false},
    {"我", "好提醒！", "10:31", true},
    {"Charlie", "我带了遮阳伞", "10:32", false},
    {"Dave", "男生也要涂防晒哦，紫外线不分性别，皮肤老化的主要原因之一就是紫外线", "10:33", false},
    {"Eve", "说得对！", "10:33", false},
    {"Frank", "我带了帽子，渔夫帽", "10:34", false},
    {"Grace", "我带了太阳镜 😎", "10:35", false},
    {"Alice", "大家准备太充分了", "10:36", false},
    {"我", "毕竟是一年中最适合出去玩的季节，再过一个月就热得不行了", "10:37", true},
    {"Bob", "夏天就是好", "10:38", false},
    {"Charlie", "突然想到，有没有人会游泳？公园旁边好像有个露天泳池", "11:00", false},
    {"Dave", "公园里有湖吗？", "11:01", false},
    {"Eve", "有一个人工湖，但是不能游泳，水深大概两米左右，去年有人偷偷下去被罚款了", "11:02", false},
    {"我", "那就算了，安全第一", "11:03", true},
    {"Frank", "对对对，生命安全最重要", "11:04", false},
    {"Grace", "可以在湖边散步，风景很好，特别是傍晚的时候，夕阳倒映在湖面上特别美", "11:05", false},
    {"Alice", "还可以喂鱼 🐟", "11:06", false},
    {"Bob", "公园里有卖鱼食吗？", "11:07", false},
    {"我", "应该有小卖部", "11:08", true},
    {"Charlie", "我上次去看到有的，好像是那种颗粒状的，5块钱一包", "11:09", false},
    {"Dave", "那就带点面包也行", "11:10", false},
    {"Eve", "面包屑不能喂鱼吧？", "11:11", false},
    {"Grace", "好像可以的，但是不能太多，不然会污染水质", "11:12", false},
    {"我", "查了一下，确实可以喂面包，但官方建议用专门的鱼食，因为面包营养不够，长期喂会影响鱼的健康", "11:15", true},
    {"Frank", "那我带点全麦面包 🍞", "11:16", false},
    {"Alice", "全麦面包喂鱼？鱼也养生吗 😂", "11:17", false},
    {"Bob", "哈哈哈", "11:17", false},
    {"我", "鱼：谢谢，我最近在控制碳水", "11:18", true},
    {"Charlie", "笑死我了", "11:19", false},
    {"Dave", "你们太有才了", "11:20", false},
    {"Eve", "好期待下午的活动！", "11:30", false},
    {"Grace", "我也是！", "11:31", false},
    {"我", "话说有人带桌游吗？UNO狼人杀什么的", "11:35", true},
    {"Alice", "我带了 UNO！", "11:36", false},
    {"Bob", "UNO 永远的神", "11:37", false},
    {"Charlie", "还有狼人杀，我来当法官，上次我当法官特别公平公正公开", "11:38", false},
    {"Dave", "狼人杀太好玩了", "11:39", false},
    {"Frank", "我带了扑克牌，也可以斗地主", "11:40", false},
    {"我", "那今天就是桌游大会了", "11:41", true},
    {"Eve", "哈哈，运动+桌游，完美组合", "11:42", false},
    {"Grace", "先吃火锅，再运动，再桌游，这个流程太完美了，感觉可以玩一整天", "11:43", false},
    {"Alice", "今天安排得满满的", "11:44", false},
    {"Bob", "这才是生活！", "11:45", false},
    {"Charlie", "感觉像夏令营", "12:00", false},
    {"Dave", "对啊，好怀念小时候", "12:01", false},
    {"我", "长大了也要保持童心，这是我今年的座右铭", "12:02", true},
    {"Eve", "说得真好", "12:03", false},
    {"Grace", "我们就是一群大孩子", "12:04", false},
    {"Alice", "永远年轻，永远热泪盈眶", "12:05", false},
    {"Frank", "太文艺了 😂", "12:06", false},
    {"我", "好了不说了，准备出门！", "12:30", true},
    {"Bob", "冲冲冲！", "12:31", false},
    {"Charlie", "集合集合！", "12:32", false},
    {"Dave", "出发！", "12:33", false},
    {"Eve", "Let's go!", "12:33", false},
    {"Grace", "大家路上注意安全", "12:34", false},
    {"Alice", "到了在群里说一声", "12:35", false},
    {"Frank", "好的！", "12:35", false},
    {"我", "我已经在火锅店了，给你们占了位子，8号桌，在二楼靠窗的位置，风景很好", "12:50", true},
    {"Bob", "这么快！", "12:51", false},
    {"Charlie", "等我 5 分钟！", "12:52", false},
    {"Dave", "我也快到了", "12:53", false},
    {"Eve", "在路上了～", "12:54", false},
    {"Grace", "马上到！", "12:55", false},
    {"Alice", "我已经到了门口", "12:56", false},
    {"Frank", "来了来了", "12:57", false},
    {"我", "锅底选了鸳鸯锅，一半辣一半不辣，辣的那边我让老板加了特辣，不能吃辣的去另一边", "13:00", true},
    {"Bob", "明智之选！", "13:01", false},
    {"Charlie", "我要吃毛肚！听说他们家的毛肚是从重庆空运过来的，特别新鲜", "13:02", false},
    {"Dave", "我点虾滑", "13:03", false},
    {"Eve", "我要肥牛！", "13:03", false},
    {"Grace", "菌菇拼盘！养生局不能少了菌菇", "13:04", false},
    {"Alice", "蔬菜也不能少，要一份油麦菜和一份茼蒿", "13:05", false},
    {"Frank", "我点个鸭血", "13:06", false},
    {"我", "已经点了，大家看看还要加什么，菜单在桌上，别客气，今天我请客！", "13:07", true},
    {"Bob", "再加一份宽粉！", "13:08", false},
    {"Charlie", "还要豆皮！", "13:09", false},
    {"Dave", "冰粉！吃火锅必须配冰粉，解辣神器", "13:10", false},
    {"Eve", "同意！", "13:10", false},
    {"我", "已经加了，还有酸梅汤，一大壶，大家随便喝", "13:11", true},
    {"Grace", "你太贴心了", "13:12", false},
};

static constexpr int s_mockCount = sizeof(s_mockMessages) / sizeof(s_mockMessages[0]);

// ═══════════════════════════════════════════════════════════════════
// 计算单条消息行高
// ═══════════════════════════════════════════════════════════════════

// 用 QTextLayout 按真实排版算文本高度与行数, 供布局与绘制共用(单一真源)
struct MessageTextMetrics {
    int lines = 1;
    qreal height = 0.0;
};

static MessageTextMetrics measureMessageText(const QString& content,
                                             const QFont& msgFont,
                                             qreal textWidth)
{
    MessageTextMetrics m;
    if (textWidth <= 0.0) {
        QFontMetrics fm(msgFont);
        m.height = fm.height();
        return m;
    }

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    QTextLayout layout(content, msgFont);
    layout.setTextOption(textOption);
    layout.beginLayout();
    qreal y = 0.0;
    m.lines = 0;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(textWidth);
        line.setPosition(QPointF(0.0, y));
        y += line.height();
        ++m.lines;
    }
    layout.endLayout();
    if (m.lines < 1) {
        m.lines = 1;
    }
    m.height = y;
    return m;
}

static qreal calcRowHeight(const MessageItem& item, qreal availableWidth)
{
    QFont msgFont;
    msgFont.setPixelSize(16);

    const qreal maxBubbleWidth = availableWidth * 85.0 / 100.0;
    const qreal textWidth = maxBubbleWidth - BUBBLE_PADDING_H * 2;

    const MessageTextMetrics tm = measureMessageText(item.content, msgFont, textWidth);
    const qreal textHeight = tm.height;
    qreal bubbleHeight = textHeight + BUBBLE_PADDING_V * 2;
    if (bubbleHeight < 36) {
        bubbleHeight = 36;
    }

    return NAME_HEIGHT + bubbleHeight + TIME_HEIGHT + 4;
}

void rebuildLayout(MessageListWidget* widget)
{
    qreal viewW = widget->viewContentsRect().width();
    if (viewW <= 0) viewW = widget->width();

    int count = widget->messageCount();
    widget->m_rowHeights.resize(count);
    widget->m_rowYOffsets.resize(count + 1);
    widget->m_rowYOffsets[0] = 0;

    for (int i = 0; i < count; ++i) {
        widget->m_rowHeights[i] = calcRowHeight(widget->m_items[i], viewW);
        widget->m_rowYOffsets[i + 1] = widget->m_rowYOffsets[i] + widget->m_rowHeights[i];
    }
}

// ═══════════════════════════════════════════════════════════════════
// MessageRowNode — 绘制单条消息气泡
// ═══════════════════════════════════════════════════════════════════

void MessageRowNode::setItem(const MessageItem& item)
{
    m_item = item;
}

void MessageRowNode::triggerUpdate(QQuickWindow* window, const QRectF& rect, const QSizeF& size)
{
    update(window, rect, size, nullptr);
}

void MessageRowNode::paint(QPainter* painter, const QSize& size, const void*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const int w = size.width();
    const int h = size.height();

    // ── 背景 ──
    painter->fillRect(0, 0, w, h, QColor("#1a1a2e"));

    // ── 发送者名字 ──
    QFont nameFont;
    nameFont.setPixelSize(13);
    painter->setFont(nameFont);

    // ── 气泡绘制 ──
    QFont msgFont;
    msgFont.setPixelSize(16);
    painter->setFont(msgFont);
    QFontMetrics msgFm(msgFont);

    const qreal maxBubbleWidth = w * 85.0 / 100.0;

    // 计算内容宽度
    QString displayContent = m_item.content;
    const qreal textWidth = maxBubbleWidth - BUBBLE_PADDING_H * 2;
    const MessageTextMetrics tm = measureMessageText(displayContent, msgFont, textWidth);
    qreal contentWidth = textWidth;
    const qreal naturalWidth = msgFm.horizontalAdvance(displayContent);
    if (naturalWidth < contentWidth) {
        contentWidth = naturalWidth;
    }
    qreal bubbleWidth = contentWidth + BUBBLE_PADDING_H * 2;
    if (bubbleWidth < 40) {
        bubbleWidth = 40;
    }

    // 行数和高度: 与布局端一致(measureMessageText)
    const qreal textHeight = tm.height;
    qreal bubbleHeight = textHeight + BUBBLE_PADDING_V * 2;
    if (bubbleHeight < 36) {
        bubbleHeight = 36;
    }

    if (m_item.isSelf) {
        // ── 自己的消息：右侧对齐，蓝色气泡 ──
        qreal bubbleX = w - bubbleWidth - SIDE_MARGIN;
        qreal bubbleY = NAME_HEIGHT;

        // 名字
        painter->setPen(QColor("#888"));
        QFontMetrics nameFm(nameFont);
        int nameW = nameFm.horizontalAdvance(m_item.sender);
        painter->drawText(QRectF(w - SIDE_MARGIN - nameW, 2, nameW, NAME_HEIGHT - 4),
            Qt::AlignLeft | Qt::AlignVCenter, m_item.sender);

        // 气泡背景
        painter->setBrush(QColor("#1565C0"));
        painter->setPen(Qt::NoPen);
        QPainterPath bubblePath;
        bubblePath.addRoundedRect(bubbleX, bubbleY, bubbleWidth, bubbleHeight, 10, 10);
        painter->drawPath(bubblePath);

        // 消息文字（支持多行）
        painter->setPen(Qt::white);
        const QRectF textRect(bubbleX + BUBBLE_PADDING_H, bubbleY + BUBBLE_PADDING_V,
            contentWidth, textHeight);
        QTextOption textOption;
        textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        QTextLayout textLayout(displayContent, msgFont);
        textLayout.setTextOption(textOption);
        textLayout.beginLayout();
        {
            qreal ty = 0.0;
            while (true) {
                QTextLine line = textLayout.createLine();
                if (!line.isValid()) {
                    break;
                }
                line.setLineWidth(contentWidth);
                line.setPosition(QPointF(0.0, ty));
                ty += line.height();
            }
        }
        textLayout.endLayout();
        textLayout.draw(painter, textRect.topLeft());

        // 时间
        QFont timeFont;
        timeFont.setPixelSize(11);
        painter->setFont(timeFont);
        painter->setPen(QColor("#666"));
        int timeW = QFontMetrics(timeFont).horizontalAdvance(m_item.time);
        painter->drawText(QRectF(bubbleX, bubbleY + bubbleHeight + 1, timeW, TIME_HEIGHT),
            Qt::AlignLeft | Qt::AlignVCenter, m_item.time);

        // 发送状态：⏳ 发送中 / ✓ 已发送 / ❗ 发送失败
        m_statusRect = QRectF(bubbleX + timeW + 6, bubbleY + bubbleHeight + 1,
            TIME_HEIGHT, TIME_HEIGHT);
        QString statusText;
        QColor statusColor(0x66, 0x66, 0x66);
        if (m_item.sendState == MessageItem::SendSending) {
            statusText = QString::fromUtf8("⏳");
        } else if (m_item.sendState == MessageItem::SendSent) {
            statusText = QString::fromUtf8("✓");
            statusColor = QColor(0x4C, 0xAF, 0x50);
        } else {
            statusText = QString::fromUtf8("❗");
            statusColor = QColor(0xE5, 0x39, 0x35);
        }
        painter->setPen(statusColor);
        painter->drawText(m_statusRect, Qt::AlignLeft | Qt::AlignVCenter, statusText);
    } else {
        // ── 他人的消息：左侧对齐，灰色气泡 + 头像 ──
        qreal bubbleY = NAME_HEIGHT;
        qreal avatarX = SIDE_MARGIN;
        qreal avatarY = bubbleY + (bubbleHeight - AVATAR_SIZE) / 2.0;
        if (avatarY < NAME_HEIGHT + 2) avatarY = NAME_HEIGHT + 2;
        qreal bubbleX = SIDE_MARGIN + AVATAR_SIZE + AVATAR_SPACING;

        // 头像背景
        QColor avatarBg = m_item.avatarColor.isValid() ? m_item.avatarColor : msgColorForName(m_item.sender);
        QString avatarLet = m_item.avatarLetter.isEmpty() ? msgFirstLetter(m_item.sender) : m_item.avatarLetter;
        painter->setBrush(avatarBg);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(QRectF(avatarX, avatarY, AVATAR_SIZE, AVATAR_SIZE));
        painter->setPen(Qt::white);
        QFont avatarFont;
        avatarFont.setPixelSize(16);
        avatarFont.setBold(true);
        painter->setFont(avatarFont);
        painter->drawText(QRectF(avatarX, avatarY, AVATAR_SIZE, AVATAR_SIZE), Qt::AlignCenter, avatarLet);

        // 名字
        painter->setFont(nameFont);
        painter->setPen(QColor("#888"));
        painter->drawText(QRectF(bubbleX, 2, 200, NAME_HEIGHT - 4),
            Qt::AlignLeft | Qt::AlignVCenter, m_item.sender);

        // 气泡背景
        painter->setBrush(QColor("#2a2a3e"));
        painter->setPen(Qt::NoPen);
        QPainterPath bubblePath2;
        bubblePath2.addRoundedRect(bubbleX, bubbleY, bubbleWidth, bubbleHeight, 10, 10);
        painter->drawPath(bubblePath2);

        // 消息文字（支持多行）
        painter->setPen(QColor("#ddd"));
        const QRectF textRect(bubbleX + BUBBLE_PADDING_H, bubbleY + BUBBLE_PADDING_V,
            contentWidth, textHeight);
        QTextOption textOption2;
        textOption2.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        QTextLayout textLayout2(displayContent, msgFont);
        textLayout2.setTextOption(textOption2);
        textLayout2.beginLayout();
        {
            qreal ty = 0.0;
            while (true) {
                QTextLine line = textLayout2.createLine();
                if (!line.isValid()) {
                    break;
                }
                line.setLineWidth(contentWidth);
                line.setPosition(QPointF(0.0, ty));
                ty += line.height();
            }
        }
        textLayout2.endLayout();
        textLayout2.draw(painter, textRect.topLeft());

        // 时间
        QFont timeFont;
        timeFont.setPixelSize(11);
        painter->setFont(timeFont);
        painter->setPen(QColor("#666"));
        int timeW = QFontMetrics(timeFont).horizontalAdvance(m_item.time);
        painter->drawText(QRectF(bubbleX + bubbleWidth - timeW, bubbleY + bubbleHeight + 1,
            timeW, TIME_HEIGHT),
            Qt::AlignRight | Qt::AlignVCenter, m_item.time);
    }
}

QskHashValue MessageRowNode::hash(const void*) const
{
    return qHash(m_item.sender) ^ qHash(m_item.content)
        ^ qHash(m_item.time) ^ qHash(m_item.isSelf)
        ^ qHash(static_cast<int>(m_item.sendState))
        ^ qHash(m_item.mediaUrl) ^ qHash(m_item.audioMime)
        ^ qHash(m_item.isAudio) ^ m_item.durationSec;
}

// ═══════════════════════════════════════════════════════════════════
// MessageRowItem
// ═══════════════════════════════════════════════════════════════════

MessageRowItem::MessageRowItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void MessageRowItem::setMessageData(const MessageItem& item)
{
    m_item = item;
    m_dirty = true;
    update();
}

QSGNode* MessageRowItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* node = static_cast<MessageRowNode*>(oldNode);
    if (!node) {
        node = new MessageRowNode();
    }
    if (m_dirty) {
        node->setItem(m_item);
        m_dirty = false;
        node->triggerUpdate(window(), QRectF(QPointF(0, 0), QSizeF(width(), height())),
            QSizeF(width(), height()));
    }
    return node;
}

// ═══════════════════════════════════════════════════════════════════
// MessageListWidget — 虚拟渲染滚动列表（可变高度）
// ═══════════════════════════════════════════════════════════════════

MessageListWidget::MessageListWidget(QQuickItem* parent)
    : MyScrollArea(parent)
{
    setFlickableOrientations(Qt::Vertical);
    setItemResizable(false);

    m_contentView = new QQuickItem(this);
    setScrolledItem(m_contentView);

    // ── QQuickTapHandler — 单击、长按检测 ──
    auto* tapHandler = new QQuickTapHandler(m_contentView);
    tapHandler->setGesturePolicy(QQuickTapHandler::DragThreshold);
    connect(tapHandler, &QQuickTapHandler::singleTapped, this,
        [this](QEventPoint point, Qt::MouseButton) {
            QPointF localPos = point.position();
            int row = rowFromContentY(localPos.y());
            if (row < 0 || row >= m_items.size()) return;
            if (m_items[row].isSelf
                && m_items[row].sendState == MessageItem::SendFailed) {
                Q_EMIT retryRequested(row);
                return;
            }
            if (!m_items[row].isSelf) {
                const qreal avatarX = SIDE_MARGIN;
                const qreal avatarSize = AVATAR_SIZE;
                qreal avatarY = m_rowYOffsets[row] + NAME_HEIGHT + (m_rowHeights[row] - avatarSize) / 2.0;
                if (avatarY < NAME_HEIGHT + 2) avatarY = NAME_HEIGHT + 2;
                if (localPos.x() >= avatarX && localPos.x() <= avatarX + avatarSize &&
                    localPos.y() >= avatarY && localPos.y() <= avatarY + avatarSize) {
                    Q_EMIT avatarClicked(row, mapToScene(localPos));
                    return;
                }
            }
            Q_EMIT rowClicked(row);
        });
    // 双击改由 MyScrollArea::childMouseEventFilter 检测
    // connect(tapHandler, &QQuickTapHandler::doubleTapped, this,
    //     [this](QEventPoint point, Qt::MouseButton) {
    //         QPointF localPos = mapFromScene(point.scenePosition());
    //         int row = rowFromPosition(localPos);
    //         if (row >= 0 && row < m_items.size()) {
    //             Q_EMIT rowDoubleClicked(row);
    //         }
    //     });
    connect(tapHandler, &QQuickTapHandler::longPressed, this,
        [this]() {
            // 双击 guard：检测到双击后 500ms 内抑制 longPressed，防止 Android 上
            // QQuickTapHandler 的 passiveGrab 失败导致 longPressTimer 误触发
            if (isDoubleTapGuardActive())
                return;
            QPointF pos = lastTouchScenePos();
            QPointF localPos = mapFromScene(pos);
            int row = rowFromPosition(localPos);
            if (row >= 0 && row < m_items.size()) {
                Q_EMIT rowLongPressed(row, pos);
            }
        });

    // ── MyScrollArea 双击检测 ──
    connect(this, &MyScrollArea::doubleTapped, this,
        [this](QPointF scenePos) {
            QPointF localPos = mapFromScene(scenePos);
            int row = rowFromPosition(localPos);
            if (row >= 0 && row < m_items.size()) {
                Q_EMIT rowDoubleClicked(row);
            }
        });

    connect(this, &QskScrollBox::scrollPosChanged,
        this, &MessageListWidget::updateVisibleRows);
}

MessageListWidget::~MessageListWidget()
{
    for (auto* row : std::as_const(m_visibleRows)) {
        delete row;
    }
    m_visibleRows.clear();
}

void MessageListWidget::populateMessages()
{
    if (m_fadeAnimator) {
        m_fadeAnimator->stop();
        delete m_fadeAnimator;
        m_fadeAnimator = nullptr;
    }
    setOpacity(1.0);

    m_items.clear();
    for (int i = 0; i < s_mockCount; ++i) {
        m_items.append(s_mockMessages[i]);
    }

    rebuildLayout(this);

    qreal totalH = m_rowYOffsets.isEmpty() ? 0 : m_rowYOffsets.last();
    qreal viewW = viewContentsRect().width();
    if (viewW <= 0) viewW = width();
    m_contentView->setSize(QSizeF(viewW, totalH));
    update();
    updateVisibleRows();

    // 滚动到底部
    qreal viewH = viewContentsRect().height();
    if (totalH > viewH) {
        setScrollPos(QPointF(0, totalH - viewH));
    }

    QTimer::singleShot(100, this, [this]() {
        if (window()) {
            m_fadeAnimator = new MessageListAnimator(this, this);
            m_fadeAnimator->setWindow(window());
            m_fadeAnimator->start();
        }
    });
}

void MessageListWidget::stopFadeIn()
{
    if (m_fadeAnimator) {
        m_fadeAnimator->stop();
        delete m_fadeAnimator;
        m_fadeAnimator = nullptr;
    }
    setOpacity(1.0);
}

void MessageListWidget::appendMessage(const MessageItem& item)
{
    m_items.append(item);

    qreal viewW = viewContentsRect().width();
    if (viewW <= 0) viewW = width();
    qreal prevTotal = m_rowYOffsets.isEmpty() ? 0 : m_rowYOffsets.last();
    qreal h = calcRowHeight(item, viewW);
    m_rowHeights.append(h);
    m_rowYOffsets.append(prevTotal + h);

    qreal totalH = m_rowYOffsets.last();
    m_contentView->setSize(QSizeF(viewW, totalH));
    update();

    // 滚动到底部
    qreal viewH = viewContentsRect().height();
    setScrollPos(QPointF(0, qMax(0.0, totalH - viewH)));

    updateVisibleRows();
}

void MessageListWidget::setSendStatus(quint64 reqseq, bool success, const QString& error)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items[i].isSelf || m_items[i].reqSeq != reqseq) {
            continue;
        }
        m_items[i].sendState = success ? MessageItem::SendSent : MessageItem::SendFailed;
        m_items[i].sendError = success ? QString() : error;
        auto it = m_visibleRows.find(i);
        if (it != m_visibleRows.end()) {
            (*it)->setMessageData(m_items[i]);
        }
        return;
    }
}

void MessageListWidget::updateSendState(int row, MessageItem::SendState state,
                                        quint64 reqseq, const QString& error)
{
    if (row < 0 || row >= m_items.size()) {
        return;
    }
    m_items[row].sendState = state;
    m_items[row].reqSeq = reqseq;
    m_items[row].sendError = error;
    auto it = m_visibleRows.find(row);
    if (it != m_visibleRows.end()) {
        (*it)->setMessageData(m_items[row]);
    }
}

bool MessageListWidget::updateSendStateByLocalId(qint64 localId,
                                                 MessageItem::SendState state,
                                                 const QString& error)
{
    if (localId == 0) {
        return false;
    }
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].localId != localId) {
            continue;
        }
        m_items[i].sendState = state;
        m_items[i].sendError = error;
        auto it = m_visibleRows.find(i);
        if (it != m_visibleRows.end()) {
            (*it)->setMessageData(m_items[i]);
        }
        return true;
    }
    return false;
}

int64_t MessageListWidget::rowidForLocalId(qint64 localId) const
{
    if (localId == 0) {
        return 0;
    }
    for (const auto& it : m_items) {
        if (it.localId == localId) {
            return it.dbRowid;
        }
    }
    return 0;
}

int64_t MessageListWidget::lastRowid() const
{
    int64_t maxRowid = 0;
    for (const auto& it : m_items) {
        if (it.dbRowid > maxRowid) {
            maxRowid = it.dbRowid;
        }
    }
    return maxRowid;
}

void MessageListWidget::backfillRowid(qint64 localId, int64_t rowid)
{
    if (localId == 0 || rowid <= 0) {
        return;
    }
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].localId == localId) {
            m_items[i].dbRowid = rowid;
            return;
        }
    }
}

void MessageListWidget::updateSendStateByRowid(int64_t rowid,
                                               MessageItem::SendState state)
{
    if (rowid <= 0) {
        return;
    }
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].dbRowid != rowid) {
            continue;
        }
        m_items[i].sendState = state;
        auto it = m_visibleRows.find(i);
        if (it != m_visibleRows.end()) {
            (*it)->setMessageData(m_items[i]);
        }
        return;
    }
}

void MessageListWidget::setChannel(const QString& protocol, const QString& chatId)
{
    if (m_fadeAnimator) {
        m_fadeAnimator->stop();
        delete m_fadeAnimator;
        m_fadeAnimator = nullptr;
    }
    setOpacity(1.0);

    m_chatId = chatId;
    m_chanProtocol = protocol;
    m_items.clear();
    m_rowHeights.clear();
    m_rowYOffsets.clear();
    for (auto* row : std::as_const(m_visibleRows)) {
        row->deleteLater();
    }
    m_visibleRows.clear();

    if (m_chatId.isEmpty()) {
        populateMessages();
        return;
    }

    // 优先从 SQLite 加载历史（chanid 需 protocol）
    QVector<MessageItem> msgs;
    if (!m_chanProtocol.isEmpty()) {
        msgs = MsgDbBridge::loadMessages(m_chanProtocol, m_chatId, 50);
    }
    if (msgs.isEmpty()) {
        msgs = MessageStore::instance()->getMessages(m_chatId);
    }
    for (const auto& msg : msgs) {
        m_items.append(msg);
    }

    rebuildLayout(this);

    qreal totalH = m_rowYOffsets.isEmpty() ? 0 : m_rowYOffsets.last();
    qreal viewW = viewContentsRect().width();
    if (viewW <= 0) viewW = width();
    m_contentView->setSize(QSizeF(viewW, totalH));
    update();
    updateVisibleRows();

    qreal viewH = viewContentsRect().height();
    if (totalH > viewH) {
        setScrollPos(QPointF(0, totalH - viewH));
    }
}

void MessageListWidget::updateVisibleRows()
{
    if (!m_contentView || m_items.isEmpty()) {
        return;
    }

    const qreal scrollY = scrollPos().y();
    const qreal viewH = viewContentsRect().height();

    // 二分查找可见行范围
    auto lowerBound = [](const QVector<qreal>& offsets, qreal val) -> int {
        int lo = 0, hi = offsets.size() - 1;
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (offsets[mid] < val) lo = mid + 1;
            else hi = mid;
        }
        return lo;
    };
    auto upperBound = [](const QVector<qreal>& offsets, qreal val) -> int {
        int lo = 0, hi = offsets.size() - 1;
        while (lo < hi) {
            int mid = (lo + hi + 1) / 2;
            if (offsets[mid] > val) hi = mid - 1;
            else lo = mid;
        }
        return lo;
    };

    int rowMin = qMax(0, upperBound(m_rowYOffsets, scrollY) - 2);
    int rowMax = qMin(m_items.size() - 1,
        lowerBound(m_rowYOffsets, scrollY + viewH) + 2);
    if (rowMin > rowMax) rowMin = rowMax;

    // 回收超出范围的行
    auto it = m_visibleRows.begin();
    while (it != m_visibleRows.end()) {
        if (it.key() < rowMin || it.key() > rowMax) {
            it.value()->deleteLater();
            it = m_visibleRows.erase(it);
        } else {
            ++it;
        }
    }

    // 创建新进入可见范围的行
    for (int i = rowMin; i <= rowMax; ++i) {
        if (!m_visibleRows.contains(i)) {
            auto* row = new MessageRowItem(m_contentView);
            row->setX(0);
            row->setY(m_rowYOffsets[i]);
            row->setWidth(m_contentView->width());
            row->setHeight(m_rowHeights[i]);
            row->setMessageData(m_items[i]);
            m_visibleRows[i] = row;
        }
    }
}

void MessageListWidget::geometryChangeEvent(QskGeometryChangeEvent* event)
{
    MyScrollArea::geometryChangeEvent(event);
    if (event->isResized() && m_contentView && !m_rebuildingLayout) {
        m_rebuildingLayout = true;
        rebuildLayout(this);
        qreal totalH = m_rowYOffsets.isEmpty() ? 0 : m_rowYOffsets.last();
        qreal viewW = viewContentsRect().width();
        if (viewW <= 0) viewW = width();
        m_contentView->setSize(QSizeF(viewW, totalH));
        for (auto* row : std::as_const(m_visibleRows)) {
            row->setWidth(viewW);
        }
        updateVisibleRows();
        qreal viewH = viewContentsRect().height();
        if (totalH > viewH) {
            setScrollPos(QPointF(0, totalH - viewH));
        }
        m_rebuildingLayout = false;
    }
}

int MessageListWidget::rowFromPosition(const QPointF& localPos) const
{
    if (m_rowYOffsets.isEmpty()) return -1;
    const auto vr = viewContentsRect();
    qreal contentY = localPos.y() - vr.top() + scrollPos().y();

    // 二分查找
    int lo = 0, hi = m_items.size() - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (contentY < m_rowYOffsets[mid]) {
            hi = mid - 1;
        } else if (contentY >= m_rowYOffsets[mid + 1]) {
            lo = mid + 1;
        } else {
            return mid;
        }
    }
    return -1;
}

int MessageListWidget::rowFromContentY(qreal contentY) const
{
    if (m_rowYOffsets.isEmpty()) return -1;

    int lo = 0, hi = m_items.size() - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (contentY < m_rowYOffsets[mid]) {
            hi = mid - 1;
        } else if (contentY >= m_rowYOffsets[mid + 1]) {
            lo = mid + 1;
        } else {
            return mid;
        }
    }
    return -1;
}

// ═══════════════════════════════════════════════════════════════════
// MessageListAnimator
// ═══════════════════════════════════════════════════════════════════

MessageListAnimator::MessageListAnimator(QQuickItem* target, QObject* parent)
    : QskAnimator()
    , m_target(target)
{
    Q_UNUSED(parent)
    setDuration(800);
    setEasingCurve(QEasingCurve::OutCubic);

    if (m_target) {
        m_target->setOpacity(0.0);
    }
}

void MessageListAnimator::advance(qreal value)
{
    if (!m_target) {
        return;
    }
    // 目标已不可见则停止动画并复位，避免在隐藏页上反复 setOpacity
    if (!m_target->isVisible()) {
        stop();
        m_target->setOpacity(1.0);
        return;
    }
    m_target->setOpacity(value);
}

#include "moc_messagelist.cpp"
