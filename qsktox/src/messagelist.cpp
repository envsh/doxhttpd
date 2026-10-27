#include "messagelist.h"
#include "messagestore.h"
#include "msgdb_bridge.h"
#include "avatarhost.h"
#include "avatar_manager.h"
#include "avatarutil.h"
#include "mediahost.h"
#include <private/qquicktaphandler_p.h>
#include <private/qquickpincharea_p.h>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QSet>
#include <QEasingCurve>
#include <QQuickWindow>
#include <QtMath>
#include <QskEvent.h>
#include <QTimer>
#include <QTextLayout>
#include <QTextOption>
#include <QImageReader>
#include <QMovie>
#include <QBuffer>

static constexpr qreal MIN_ROW_HEIGHT = 60;
static constexpr qreal NAME_HEIGHT = 20;
static constexpr qreal TIME_HEIGHT = 14;
static constexpr qreal BUBBLE_PADDING_H = 12;
static constexpr qreal BUBBLE_PADDING_V = 8;
static constexpr qreal SIDE_MARGIN = 12;
static constexpr qreal AVATAR_SIZE = 36;
static constexpr qreal AVATAR_SPACING = 8;
static constexpr qreal HDR_BTN_SIZE = 14;
static constexpr qreal HDR_BTN_GAP = 4;

// 卡片 header 右上角三钮：📋 source / 🌐 translate / 🔊 朗读。
// 本次空操作：仅绘制并存 Rect 锚点，不挂任何点击事件。
// 显示条件镜像 qltox chatview（chatview.cpp:1203-1214/1295-1306）：
// audio 类卡片整组不画；🔊 仅正文非空时画（m_speakRect 置空 Rect）。
// 左→右依 qltox 口径：source | translate | speak（speak 最贴行右缘）。

// ── 媒体消息分类（镜像 qltox etype 派发，chatview.cpp:4074/:2986 口径）──
// MsgMediaType 定义移至 messagelist.h（mediaBodyTapped 信号跨 TU 需要枚举可见）

static MsgMediaType msgMediaTypeOf(const MessageItem& item) {
    if (item.isAudio) {
        return MsgMediaType::Audio;
    }
    // push msgtype 兼容 "m.image"（Matrix CS）与 "image"（简化键）两形
    QString t = item.msgType;
    if (t.startsWith(QString::fromUtf8("m."))) {
        t = t.mid(2);
    }
    if (t == QString::fromUtf8("image")) {
        const bool gif = item.mediaMime.contains(QString::fromUtf8("image/gif"),
            Qt::CaseInsensitive)
            || item.mediaUrl.contains(QString::fromUtf8(".gif"), Qt::CaseInsensitive);
        return gif ? MsgMediaType::Gif : MsgMediaType::Image;
    }
    if (t == QString::fromUtf8("video")) {
        return MsgMediaType::Video;
    }
    if (t == QString::fromUtf8("file")) {
        return MsgMediaType::File;
    }
    return MsgMediaType::Text;
}

struct MediaDisplayMetrics {
    MsgMediaType type = MsgMediaType::Unknown;
    qreal bubbleW = 0.0;   // 媒体主体框宽（真绘图区 或 占位框）
    qreal bubbleH = 0.0;   // 主体框高
    bool  hasLocal = false;// localPath 预解码可用 → paint drawImage
    QString label;         // 占位标签：图片/GIF/视频/语音消息/文件
    QString subLabel;      // 视频/语音=时长 mm:ss；文件=大小
};

static QString mediaDurationLabel(int durationSec) {
    return QString::asprintf("%d:%02d", durationSec / 60, durationSec % 60);
}

static MediaDisplayMetrics measureMedia(const MessageItem& item, qreal availW) {
    MediaDisplayMetrics m;
    m.type = msgMediaTypeOf(item);
    if (m.type == MsgMediaType::Unknown || m.type == MsgMediaType::Text) {
        return m;
    }
    const qreal maxW = availW * 70.0 / 100.0;   // qltox width()*70/100 口径
    switch (m.type) {
    case MsgMediaType::Image:
    case MsgMediaType::Gif: {
        // Image/Gif：等比主体框（有原始宽高按比例，无则默认 4:3）
        qreal w = maxW;
        qreal h;
        if (item.mediaWidth > 0 && item.mediaHeight > 0) {
            h = w * qreal(item.mediaHeight) / qreal(item.mediaWidth);
        } else {
            h = w * 3.0 / 4.0;
        }
        if (h < 60.0) { h = 60.0; }
        if (h > 240.0) { h = 240.0; }
        m.bubbleW = w;
        m.bubbleH = h;
        m.label = (m.type == MsgMediaType::Gif)
            ? QString::fromUtf8("GIF 🖼") : QString::fromUtf8("图片 🖼");
        break;
    }
    case MsgMediaType::Video: {
        // qltox 口径：视频视觉区与图片同构（比例框 + ▶ 圆钮覆盖），非矮条
        qreal w = maxW;
        qreal h;
        if (item.mediaWidth > 0 && item.mediaHeight > 0) {
            h = w * qreal(item.mediaHeight) / qreal(item.mediaWidth);
        } else {
            h = w * 3.0 / 4.0;
        }
        if (h < 60.0) { h = 60.0; }
        if (h > 240.0) { h = 240.0; }
        m.bubbleW = w;
        m.bubbleH = h;
        m.label = QString::fromUtf8("视频");
        if (item.durationSec > 0) {
            m.subLabel = mediaDurationLabel(item.durationSec);
        }
        break;
    }
    case MsgMediaType::Audio: {
        // qltox paintAudioContent 口径：48px 图标块 + 时长，气泡加高
        m.bubbleW = (maxW < 220.0) ? maxW : 220.0;
        m.bubbleH = 72.0;
        m.label = QString::fromUtf8("语音 🔊");
        m.subLabel = mediaDurationLabel(item.durationSec);
        break;
    }
    case MsgMediaType::File: {
        m.bubbleW = (maxW < 240.0) ? maxW : 240.0;
        m.bubbleH = 84.0;
        m.label = item.fileName.isEmpty()
            ? QString::fromUtf8("文件") : item.fileName;
        m.subLabel = item.fileSizeStr;
        break;
    }
    default:
        break;
    }
    return m;
}
static void msgDrawHeaderButtons(QPainter* painter, MessageRowNode* node,
                                 qreal rightEdge, const MessageItem& item) {
    node->m_sourceRect = QRectF();
    node->m_translateRect = QRectF();
    node->m_speakRect = QRectF();
    if (item.isAudio) {
        return;
    }

    const qreal btnAreaW = HDR_BTN_SIZE * 3 + HDR_BTN_GAP * 2;
    const qreal btnY = (NAME_HEIGHT - HDR_BTN_SIZE) / 2.0;
    const qreal srcX = rightEdge - btnAreaW;
    const qreal trX = srcX + HDR_BTN_SIZE + HDR_BTN_GAP;
    const qreal spX = trX + HDR_BTN_SIZE + HDR_BTN_GAP;
    node->m_sourceRect = QRectF(srcX, btnY, HDR_BTN_SIZE, HDR_BTN_SIZE);
    node->m_translateRect = QRectF(trX, btnY, HDR_BTN_SIZE, HDR_BTN_SIZE);
    if (!item.content.isEmpty()) {
        node->m_speakRect = QRectF(spX, btnY, HDR_BTN_SIZE, HDR_BTN_SIZE);
    }

    painter->save();
    QFont iconFont;
    iconFont.setPixelSize(11);
    painter->setFont(iconFont);
    painter->setPen(QColor("#666"));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(node->m_sourceRect);
    painter->drawEllipse(node->m_translateRect);
    if (node->m_speakRect.isValid()) {
        painter->drawEllipse(node->m_speakRect);
    }
    painter->drawText(node->m_sourceRect, Qt::AlignCenter, QString::fromUtf8("📋"));
    painter->drawText(node->m_translateRect, Qt::AlignCenter, QString::fromUtf8("🌐"));
    if (node->m_speakRect.isValid()) {
        painter->drawText(node->m_speakRect, Qt::AlignCenter, QString::fromUtf8("🔊"));
    }
    painter->restore();
}

// ── 媒体绘制辅助（paint / 几何命中共用的纯数学，无 GUI 对象）──

// 状态条大小后缀（mainpage fmtMediaSize 同口径 B/KB/MB/GB）
static QString mediaBarSizeText(qint64 bytes) {
    if (bytes <= 0) { return QString(); }
    if (bytes < 1024) { return QString::number((long long)bytes)
        + QString::fromUtf8("B"); }
    if (bytes < 1024*1024) { return QString::asprintf("%.1fKB", bytes/1024.0); }
    if (bytes < 1024LL*1024*1024) { return QString::asprintf("%.1fMB",
        bytes/1048576.0); }
    return QString::asprintf("%.1fGB", bytes/1073741824.0);
}

// 下载速率格式（qltox formatSpeed chatview.cpp:215-217 口径）；
// bps<=0 返回空（状态条不显示拖尾分隔符）
static QString mediaBarSpeedText(int bps) {
    if (bps <= 0) { return QString(); }
    const double kb = bps / 1024.0;
    if (kb < 1024.0) { return QString::asprintf("%.1fKB/s", kb); }
    const double mb = kb / 1024.0;
    if (mb < 1024.0) { return QString::asprintf("%.1fMB/s", mb); }
    return QString::asprintf("%.1fGB/s", mb / 1024.0);
}

// 媒体框底部的下载状态条几何（tap 命中与 paint 必须一致；isAudio 无条）
// 媒体主体框（row 内容坐标系；tap 本体命中与 paint 共用）
static QRectF msgMediaRect(const MessageItem& item, qreal viewW) {
    const MediaDisplayMetrics mm = measureMedia(item, viewW);
    if (mm.type == MsgMediaType::Unknown || mm.type == MsgMediaType::Text
        || mm.bubbleW <= 0) {
        return QRectF();
    }
    qreal bx;
    if (item.isSelf) {
        bx = viewW - (mm.bubbleW + BUBBLE_PADDING_H * 2) - SIDE_MARGIN;
    } else {
        bx = SIDE_MARGIN + AVATAR_SIZE + AVATAR_SPACING;
    }
    return QRectF(bx + BUBBLE_PADDING_H, NAME_HEIGHT + BUBBLE_PADDING_V,
        mm.bubbleW, mm.bubbleH);
}

static QRectF msgMediaBarRect(const MessageItem& item, qreal viewW) {
    const MediaDisplayMetrics mm = measureMedia(item, viewW);
    if (mm.type == MsgMediaType::Unknown || mm.type == MsgMediaType::Text
        || item.isAudio) {
        return QRectF();
    }
    // 复用本体框坐标（单一事实来源），不再重复 bx 数学
    const QRectF mediaRect = msgMediaRect(item, viewW);
    return QRectF(mediaRect.x(), mediaRect.bottom() - 22.0,
        mediaRect.width(), 22.0);
}

// 图片等比缩放目标 rect（修 drawImage 满框拉伸 bug：居中 KeepAspectRatio）
static QRectF mediaFitRect(const QImage& img, const QRectF& rect) {
    if (img.isNull() || rect.width() <= 0 || rect.height() <= 0) {
        return rect;
    }
    const qreal iw = img.width(), ih = img.height();
    qreal w = rect.width();
    qreal h = w * ih / iw;
    if (h > rect.height()) {
        h = rect.height();
        w = h * iw / ih;
    }
    return QRectF(rect.x() + (rect.width() - w) / 2.0,
                  rect.y() + (rect.height() - h) / 2.0, w, h);
}

// 媒体主体统一绘制（self/peer 共用；渲染线程安全：painter 数学与文本）：
// 真图(等比) / 占位(W×H 或 label) + GIF 徽章 / ▶ 圆钮 / 语音 tile + 下载状态条
static void paintMediaBody(QPainter* p, const MessageItem& item,
                           const QImage& mediaImage,
                           const MediaDisplayMetrics& mm,
                           const QRectF& mediaRect,
                           const QColor& textColor, const QColor& subColor,
                           const QFont& baseFont)
{
    // ── Audio：48px 图标块 + 大号时长 + 文件名副行（qltox paintAudioContent）──
    if (mm.type == MsgMediaType::Audio) {
        const qreal iconSize = 48.0;
        const QRectF iconR(mediaRect.x(), mediaRect.y() + 12.0,
            iconSize, iconSize);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(255, 255, 255, 26));
        p->drawRoundedRect(iconR, 8, 8);
        QFont af = baseFont;
        af.setPixelSize(24);
        p->setFont(af);
        p->setPen(textColor);
        p->drawText(iconR, Qt::AlignCenter, QString::fromUtf8("🔊"));
        p->setFont(baseFont);
        QFont df = baseFont;
        df.setPixelSize(15);
        df.setBold(true);
        p->setFont(df);
        const QString dur = (item.durationSec > 0)
            ? mediaDurationLabel(item.durationSec)
            : QString::fromUtf8("--:--");
        p->drawText(QRectF(iconR.right() + 12.0, mediaRect.y() + 14.0,
            mediaRect.right() - iconR.right() - 12.0, 22.0),
            Qt::AlignLeft | Qt::AlignVCenter, dur);
        p->setFont(baseFont);
        if (!item.fileName.isEmpty()) {
            p->setPen(subColor);
            p->drawText(QRectF(iconR.right() + 12.0, mediaRect.y() + 36.0,
                mediaRect.right() - iconR.right() - 12.0, 16.0),
                Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                item.fileName);
        }
        return;
    }

    // ── 主体：真图（等比居中）else 占位（有宽高 → "W × H"，否则类型 label）──
    if (!mediaImage.isNull()) {
        p->drawImage(mediaFitRect(mediaImage, mediaRect), mediaImage);
    } else {
        QString center = mm.label;
        if (item.mediaWidth > 0 && item.mediaHeight > 0
            && (mm.type == MsgMediaType::Image || mm.type == MsgMediaType::Gif)) {
            center = QStringLiteral("%1 × %2")
                .arg(QString::number((int)item.mediaWidth),
                     QString::number((int)item.mediaHeight));
        }
        p->setPen(textColor);
        p->setFont(baseFont);
        p->drawText(mediaRect, Qt::AlignCenter | Qt::TextSingleLine, center);
        if (!mm.subLabel.isEmpty()) {
            p->setPen(subColor);
            // 上移到状态条（22px）上方 2px 处：视频时长/文件大小不再被状态
            // 条遮住半截（占位分支只会出现在带状态条的非 Audio 行）
            p->drawText(QRectF(mediaRect.x(), mediaRect.bottom() - 42.0,
                mediaRect.width(), 18.0),
                Qt::AlignCenter | Qt::TextSingleLine, mm.subLabel);
        }
    }

    // ── GIF 徽章（左上角；qltox paintMediaContent :521-539 口径）──
    if (mm.type == MsgMediaType::Gif) {
        const QRectF badgeR(mediaRect.x() + 4.0, mediaRect.y() + 4.0,
            30.0, 16.0);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 0, 0, 150));
        p->drawRoundedRect(badgeR, 3, 3);
        p->setPen(Qt::white);
        QFont bf = baseFont;
        bf.setPixelSize(9);
        bf.setBold(true);
        p->setFont(bf);
        p->drawText(badgeR, Qt::AlignCenter, QStringLiteral("GIF"));
        p->setFont(baseFont);
    }

    // ── 视频 ▶ 圆钮覆盖（真图/占位都画；qltox :542-556 口径）──
    if (mm.type == MsgMediaType::Video) {
        const qreal btn = 40.0;
        const QRectF playR(mediaRect.center().x() - btn / 2.0,
            mediaRect.center().y() - btn / 2.0, btn, btn);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 0, 0, 160));
        p->drawEllipse(playR);
        QPainterPath tri;
        tri.moveTo(playR.left() + 14.0, playR.top() + 10.0);
        tri.lineTo(playR.left() + 14.0, playR.bottom() - 10.0);
        tri.lineTo(playR.right() - 10.0, playR.center().y());
        tri.closeSubpath();
        p->setBrush(Qt::white);
        p->drawPath(tri);
    }

    // ── 下载状态条（媒体框底部 22px，isAudio 无条；NotRequested/Failed 可点）──
    if (item.isAudio) {
        return;
    }
    const QRectF barRect(mediaRect.x(), mediaRect.bottom() - 22.0,
        mediaRect.width(), 22.0);
    p->fillRect(barRect, QColor(0, 0, 0, 140));
    // InProgress：条内半透明白填充（宽度=pct）——qltox「顶部 4px 线性进度条」
    // 在 qsk 状态条内的同义落位（22px 全高，bottom bar 不再另分顶条）
    if (item.downloadState == MediaDlInProgress && item.downloadProgress >= 0
        && item.downloadProgress <= 100) {
        p->fillRect(QRectF(barRect.x(), barRect.y(),
            barRect.width() * item.downloadProgress / 100.0, barRect.height()),
            QColor(255, 255, 255, 38));
    }
    QString barText;
    QColor barColor = Qt::white;
    switch (item.downloadState) {
    case MediaDlInProgress:
        barText = (item.downloadProgress >= 0)
            ? (QString::fromUtf8("⬇⏳ ") + QString::number(item.downloadProgress)
               + QStringLiteral("%"))
            : QString::fromUtf8("⏳ …");
        {
            const QString spd = mediaBarSpeedText(item.downloadSpeedBps);
            if (!spd.isEmpty()) {
                barText += QString::fromUtf8(" · ") + spd;
            }
        }
        break;
    case MediaDlCompleted:
        barText = QString::fromUtf8("✓ 已下载");
        barColor = QColor(0x4C, 0xAF, 0x50);
        break;
    case MediaDlFailed:
        // 详情直出（qltox tooltip 文案 chatview.cpp:3706-3708 的触摸端改版；
        // 详情空保持旧文案；单行 TextSingleLine 已兜裁剪）
        barText = item.errorMsg.isEmpty()
            ? QString::fromUtf8("✗ 失败 · 点按重试")
            : QString::fromUtf8("✗ ") + item.errorMsg;
        barColor = QColor(0xE5, 0x39, 0x35);
        break;
    default: {
        barText = QString::fromUtf8("⬇ 下载");
        QString sz = item.fileSizeStr;
        if (sz.isEmpty()) {
            sz = mediaBarSizeText(item.fileSizeBytes);
        }
        if (!sz.isEmpty()) {
            barText += QLatin1Char(' ') + sz;
        }
        break;
    }
    }
    QFont bf = baseFont;
    bf.setPixelSize(10);
    bf.setBold(true);
    p->setFont(bf);
    p->setPen(barColor);
    p->drawText(barRect.adjusted(0, 0, -8.0, 0),
        Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, barText);
    p->setFont(baseFont);
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

    const MediaDisplayMetrics mm = measureMedia(item, availableWidth);
    qreal bubbleHeight;
    if (mm.type != MsgMediaType::Unknown && mm.type != MsgMediaType::Text) {
        bubbleHeight = mm.bubbleH + BUBBLE_PADDING_V * 2;
        // caption（=content 非空时）作媒体下正文行，计入行高
        if (!item.content.isEmpty()) {
            const MessageTextMetrics cm =
                measureMessageText(item.content, msgFont, mm.bubbleW);
            bubbleHeight += cm.height + BUBBLE_PADDING_V;
        }
    } else {
        const MessageTextMetrics tm =
            measureMessageText(item.content, msgFont, textWidth);
        bubbleHeight = tm.height + BUBBLE_PADDING_V * 2;
    }
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

MessageRowNode::MessageRowNode()
{
    setRenderHint( QskPaintedNode::Raster );
}

void MessageRowNode::setItem(const MessageItem& item,
                             const QImage& avatarImage, quint32 avatarTick,
                             const QImage& mediaImage, quint32 mediaTick)
{
    m_item = item;
    m_avatarImage = avatarImage;
    m_avatarTick = avatarTick;
    m_mediaImage = mediaImage;
    m_mediaTick = mediaTick;
}

void MessageRowNode::triggerUpdate(QQuickWindow* window, const QRectF& rect, const QSizeF& size)
{
    update(window, rect, size, nullptr);
}

// caption/正文统一换行绘制助手（paint 内两处媒体 caption 复用；
// 渲染线程安全，仅 QTextLayout/drawText，无 QPixmap）
static void drawWrappedText(QPainter* painter, const QString& text, const QFont& font,
                            const QRectF& rect)
{
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    QTextLayout layout(text, font);
    layout.setTextOption(option);
    layout.beginLayout();
    {
        qreal ty = 0.0;
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid()) {
                break;
            }
            line.setLineWidth(rect.width());
            line.setPosition(QPointF(0.0, ty));
            ty += line.height();
        }
    }
    layout.endLayout();
    layout.draw(painter, rect.topLeft());
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

    const MediaDisplayMetrics mm = measureMedia(m_item, w);
    const bool isMedia = (mm.type != MsgMediaType::Unknown
                          && mm.type != MsgMediaType::Text);

    // 行数和高度: 与布局端一致(calcRowHeight 同款双口径)
    const qreal textHeight = tm.height;
    qreal bubbleHeight;
    if (isMedia) {
        bubbleHeight = mm.bubbleH + BUBBLE_PADDING_V * 2;
        if (!displayContent.isEmpty()) {
            const MessageTextMetrics cm =
                measureMessageText(displayContent, msgFont, mm.bubbleW);
            bubbleHeight += cm.height + BUBBLE_PADDING_V;
        }
    } else {
        bubbleHeight = textHeight + BUBBLE_PADDING_V * 2;
    }
    if (bubbleHeight < 36) {
        bubbleHeight = 36;
    }

    if (m_item.isSelf) {
        // ── 自己的消息：右侧对齐，蓝色气泡 ──
        qreal bubbleX;
        qreal bubbleY = NAME_HEIGHT;
        if (isMedia) {
            bubbleX = w - (mm.bubbleW + BUBBLE_PADDING_H * 2) - SIDE_MARGIN;
        } else {
            bubbleX = w - bubbleWidth - SIDE_MARGIN;
        }

        // 名字（右上角让出三钮区域，超宽省略）
        painter->setPen(QColor("#888"));
        QFontMetrics nameFm(nameFont);
        int nameW = nameFm.horizontalAdvance(m_item.sender);
        QString shownName = m_item.sender;
        const qreal btnAreaW = HDR_BTN_SIZE * 3 + HDR_BTN_GAP * 2;
        const qreal maxNameW = w - SIDE_MARGIN * 2 - btnAreaW - 4;
        if (nameW > maxNameW) {
            shownName = nameFm.elidedText(shownName, Qt::ElideRight, (int)maxNameW);
            nameW = nameFm.horizontalAdvance(shownName);
        }
        painter->drawText(QRectF(w - SIDE_MARGIN - nameW, 2, nameW, NAME_HEIGHT - 4),
            Qt::AlignLeft | Qt::AlignVCenter, shownName);

        // header 三钮（空操作，仅绘制）
        msgDrawHeaderButtons(painter, this, w - SIDE_MARGIN, m_item);

        // 气泡背景
        painter->setBrush(QColor("#1565C0"));
        painter->setPen(Qt::NoPen);
        QPainterPath bubblePath;
        bubblePath.addRoundedRect(bubbleX, bubbleY,
            isMedia ? (mm.bubbleW + BUBBLE_PADDING_H * 2) : bubbleWidth,
            bubbleHeight, 10, 10);
        painter->drawPath(bubblePath);

        if (isMedia) {
            const QRectF mediaRect(bubbleX + BUBBLE_PADDING_H,
                bubbleY + BUBBLE_PADDING_V, mm.bubbleW, mm.bubbleH);
            paintMediaBody(painter, m_item, m_mediaImage, mm, mediaRect,
                Qt::white, QColor(0xff, 0xff, 0xff, 0x99), msgFont);
            // caption 正文行（content 非空），同款换行绘制
            if (!displayContent.isEmpty()) {
                painter->setPen(Qt::white);
                painter->setFont(msgFont);
                drawWrappedText(painter, displayContent, msgFont,
                    QRectF(bubbleX + BUBBLE_PADDING_H,
                        mediaRect.bottom() + BUBBLE_PADDING_V,
                        mm.bubbleW, bubbleHeight - mm.bubbleH - BUBBLE_PADDING_V * 2));
            }
        } else {
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
        }

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

        // 头像：GUI 线程已预解析为 QImage（真实头像 / identicon 均在此）。
        // paint() 运行在场景图渲染线程，禁止再访问 AvatarManager/QPixmap，
        // 否则 Android 等无 ThreadedPixmaps 能力的平台会拿到 null QPixmap，
        // 永远退化成字母圆块。无头像时才画字母圆块兜底。
        const QRectF avatarRect(avatarX, avatarY, AVATAR_SIZE, AVATAR_SIZE);
        if (!m_avatarImage.isNull()) {
            painter->drawImage(avatarRect, m_avatarImage);
        } else {
            const QColor avatarBg = m_item.avatarColor.isValid()
                ? m_item.avatarColor : msgAvatarColorFor(m_item.sender);
            const QString avatarLet = m_item.avatarLetter.isEmpty()
                ? msgAvatarLetterFor(m_item.sender) : m_item.avatarLetter;
            drawLetterAvatar(painter, avatarRect, avatarBg, avatarLet);
        }

        // 名字
        painter->setFont(nameFont);
        painter->setPen(QColor("#888"));
        painter->drawText(QRectF(bubbleX, 2, 200, NAME_HEIGHT - 4),
            Qt::AlignLeft | Qt::AlignVCenter, m_item.sender);

        // header 三钮（空操作，仅绘制；行右缘锚点，与 self 口径一致）
        msgDrawHeaderButtons(painter, this, w - SIDE_MARGIN, m_item);

        // 气泡背景
        painter->setBrush(QColor("#2a2a3e"));
        painter->setPen(Qt::NoPen);
        QPainterPath bubblePath2;
        bubblePath2.addRoundedRect(bubbleX, bubbleY,
            isMedia ? (mm.bubbleW + BUBBLE_PADDING_H * 2) : bubbleWidth,
            bubbleHeight, 10, 10);
        painter->drawPath(bubblePath2);

        if (isMedia) {
            const QRectF mediaRect(bubbleX + BUBBLE_PADDING_H,
                bubbleY + BUBBLE_PADDING_V, mm.bubbleW, mm.bubbleH);
            paintMediaBody(painter, m_item, m_mediaImage, mm, mediaRect,
                QColor("#ddd"), QColor(0x8f, 0xa0, 0xc0), msgFont);
            // caption 正文行（content 非空），同款换行绘制
            if (!displayContent.isEmpty()) {
                painter->setPen(QColor("#ddd"));
                painter->setFont(msgFont);
                drawWrappedText(painter, displayContent, msgFont,
                    QRectF(bubbleX + BUBBLE_PADDING_H,
                        mediaRect.bottom() + BUBBLE_PADDING_V,
                        mm.bubbleW, bubbleHeight - mm.bubbleH - BUBBLE_PADDING_V * 2));
            }
        } else {
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
        }

        // 时间
        QFont timeFont;
        timeFont.setPixelSize(11);
        painter->setFont(timeFont);
        painter->setPen(QColor("#666"));
        int timeW = QFontMetrics(timeFont).horizontalAdvance(m_item.time);
        painter->drawText(QRectF(bubbleX + (isMedia
                ? (mm.bubbleW + BUBBLE_PADDING_H * 2) : bubbleWidth) - timeW,
            bubbleY + bubbleHeight + 1, timeW, TIME_HEIGHT),
            Qt::AlignRight | Qt::AlignVCenter, m_item.time);
    }
}

QskHashValue MessageRowNode::hash(const void*) const
{
    return qHash(m_item.sender) ^ qHash(m_item.content)
        ^ qHash(m_item.time) ^ qHash(m_item.isSelf)
        ^ qHash(m_item.avatarUrl) ^ m_item.peerNumber
        ^ qHash(static_cast<int>(m_item.sendState))
        ^ qHash(m_item.mediaUrl) ^ qHash(m_item.audioMime)
        ^ qHash(m_item.isAudio) ^ m_item.durationSec
        ^ qHash(m_item.avatarLetter)
        ^ qHash(static_cast<uint>(m_item.avatarColor.rgb()))
        ^ m_avatarTick
        ^ qHash(m_item.msgType)
        ^ qHash(m_item.mediaMime)
        ^ qHash(m_item.fileName)
        ^ qHash(m_item.fileSizeStr)
        ^ static_cast<uint>(m_item.mediaWidth)
        ^ static_cast<uint>(m_item.mediaHeight)
        ^ qHash(m_item.thumbUrl)
        ^ qHash(m_item.downloadState)
        ^ qHash(m_item.downloadProgress)
        ^ qHash(static_cast<qulonglong>(m_item.fileSizeBytes))
        ^ m_mediaTick;
}

// ═══════════════════════════════════════════════════════════════════
// MessageRowItem
// ═══════════════════════════════════════════════════════════════════

MessageRowItem::MessageRowItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void MessageRowItem::refreshAvatar()
{
    // 必须在 GUI 线程执行：AvatarManager::get() 返回 QPixmap，且无缓存时
    // 会生成 identicon（内部构造 QPixmap）。Qt 仅在支持 ThreadedPixmaps 的
    // 平台允许非 GUI 线程使用 QPixmap，Android/EGLFS 不支持 → null。
    m_avatarImage = QImage();
    if (!m_item.avatarUrl.isEmpty()) {
        const QPixmap pm = AvatarManager::inst().get(
            m_item.avatarUrl, m_item.sender, m_item.peerNumber, (int)AVATAR_SIZE);
        if (!pm.isNull()) {
            m_avatarImage = pm.toImage();
        }
    }
    ++m_avatarTick;
    m_dirty = true;
}

void MessageRowItem::refreshMedia()
{
    // 媒体真绘源（GUI 线程解码，与 avatar 的 QImage+tick 体系同构）：
    //   Image/Gif：mediaUrl（图片类字节可 QImage decode）
    //   Video：thumbUrl（CS info.thumbnail_url）优先，缺则 mxc /thumbnail 端点
    //   File/Audio：不绘图（非 image 字节）
    //   localPath 本地文件兜底（P2 直读）
    ++m_mediaTick;
    m_mediaImage = QImage();
    QString showUrl;
    const MsgMediaType mt = msgMediaTypeOf(m_item);
    if (mt == MsgMediaType::Gif) {
        // GIF 动画（qltox QMovie 对位）：源 = mediaUrl（优先）else localPath；
        // 首帧先行（start 后 currentImage），CacheDb miss（尚未下载）时
        // 退化为占位，与静帧口径一致
        const QString src = !m_item.mediaUrl.isEmpty()
            ? m_item.mediaUrl : m_item.localPath;
        m_mediaSrc = src;
        setupGifMovie(src);
        m_mediaImage = m_gifMovie ? m_gifMovie->currentImage() : QImage();
        m_dirty = true;
        return;
    }
    if (m_gifMovie) {
        // 行数据变更为非 GIF → 释放动画资源
        stopGifMovie();
    }
    if (mt == MsgMediaType::Video) {
        if (!m_item.thumbUrl.isEmpty()) {
            showUrl = m_item.thumbUrl;
        } else if (m_item.mediaWidth > 0 && m_item.mediaHeight > 0) {
            showUrl = MediaHost::thumbnailUrl(m_item.mediaUrl,
                (int)m_item.mediaWidth, (int)m_item.mediaHeight);
        }
    } else if (mt == MsgMediaType::Image || mt == MsgMediaType::Gif) {
        showUrl = m_item.mediaUrl;
    }
    m_mediaSrc = showUrl.isEmpty() ? m_item.localPath : showUrl;
    if (!showUrl.isEmpty()) {
        m_mediaImage = MediaHost::instance()->get(showUrl);
        if (!m_mediaImage.isNull()) {
            m_dirty = true;
            return;
        }
    }
    if (!m_item.localPath.isEmpty()
        && (mt == MsgMediaType::Image || mt == MsgMediaType::Gif
            || mt == MsgMediaType::Video)) {
        QImageReader reader(m_item.localPath);
        reader.setAutoTransform(true);
        m_mediaImage = reader.read();
    }
    m_dirty = true;
}

MessageRowItem::~MessageRowItem()
{
    // QMovie/QBuffer 均以 this 为父，Qt 自动清理；此处显式 stop 防
    // deleteLater 延迟窗口内继续刷帧
    if (m_gifMovie) {
        m_gifMovie->stop();
    }
}

void MessageRowItem::setupGifMovie(const QString& src)
{
    if (src.isEmpty()) {
        stopGifMovie();
        return;
    }
    if (m_gifMovie && m_gifSrc == src) {
        return;   // 同源已在播
    }
    stopGifMovie();
    m_gifMovie = new QMovie(this);
    if (src.contains(QStringLiteral("://"))) {
        // URL 源：CacheDb 同步读字节（QMovie 不支持网络直拾），
        // miss（尚未下载完成）→ 不启动动画，保持占位静帧
        const std::vector<uint8_t> bytes = MediaHost::instance()->getBytes(src);
        if (bytes.empty()) {
            m_gifMovie->deleteLater();
            m_gifMovie = nullptr;
            m_gifSrc.clear();
            return;
        }
        m_gifData = QByteArray(
            reinterpret_cast<const char*>(bytes.data()), (int)bytes.size());
        m_gifBuf = new QBuffer(this);
        m_gifBuf->setData(m_gifData);
        m_gifBuf->open(QIODevice::ReadOnly);
        m_gifMovie->setDevice(m_gifBuf);
    } else {
        // 本地文件源（localPath 直读）
        m_gifMovie->setFileName(src);
    }
    connect(m_gifMovie, &QMovie::frameChanged, this, [this](int) {
        m_mediaImage = m_gifMovie->currentImage();
        ++m_mediaTick;
        m_dirty = true;
        update();
    });
    m_gifMovie->start();
}

void MessageRowItem::stopGifMovie()
{
    if (m_gifMovie) {
        m_gifMovie->stop();
        m_gifMovie->deleteLater();
        m_gifMovie = nullptr;
    }
    if (m_gifBuf) {
        m_gifBuf->deleteLater();
        m_gifBuf = nullptr;
    }
    m_gifData = QByteArray();
    m_gifSrc = QString();
}

void MessageRowItem::setMessageData(const MessageItem& item)
{
    const bool avatarChanged = (item.avatarUrl != m_item.avatarUrl);
    const bool mediaChanged = (item.localPath != m_item.localPath
        || item.mediaUrl != m_item.mediaUrl);
    const bool needMedia = mediaChanged || !item.mediaUrl.isEmpty();
    m_item = item;
    if (avatarChanged) {
        m_avatarResolved = false;
    }
    if (!m_avatarResolved) {
        refreshAvatar();
        m_avatarResolved = true;
    }
    if (needMedia) {
        // mediaUrl 行每次刷新都回查 MediaHost：下载完成（mediaUpdated）前
        // 该行可能已被回收重建，命中与否只差一次内存/CacheDb 查询
        refreshMedia();
    }
    m_dirty = true;
    update();
}

void MessageRowItem::setMessageDataNoUpdate(const MessageItem& item)
{
    const bool avatarChanged = (item.avatarUrl != m_item.avatarUrl);
    const bool mediaChanged = (item.localPath != m_item.localPath
        || item.mediaUrl != m_item.mediaUrl);
    const bool needMedia = mediaChanged || !item.mediaUrl.isEmpty();
    m_item = item;
    if (avatarChanged) {
        m_avatarResolved = false;
    }
    if (!m_avatarResolved) {
        refreshAvatar();
        m_avatarResolved = true;
    }
    if (needMedia) {
        refreshMedia();
    }
    m_dirty = true;
    // 合并刷新：不触发 update()，避免向上传播到 QskScrollArea
}

QSGNode* MessageRowItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* node = static_cast<MessageRowNode*>(oldNode);
    if (!node) {
        node = new MessageRowNode();
    }
    if (m_dirty) {
        node->setItem(m_item, m_avatarImage, m_avatarTick,
            m_mediaImage, m_mediaTick);
        m_dirty = false;
    }
    // 必须无条件调用：QskPaintedNode::update() 是唯一创建/更新 QSGImageNode 的
    // 入口，且 rect 为空（宽 0）时会直接删掉 imageNode → 整行不渲染。
    // 只在 m_dirty 时调用 → 行尺寸变化（宽度变化导致重排）后不再重绘，
    // 表现为消息列表空白/错位。ChannelRowItem 的写法就是对的。
    node->triggerUpdate(window(), QRectF(QPointF(0, 0), QSizeF(width(), height())),
        QSizeF(width(), height()));
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
            if (!m_items[row].isSelf
                && isAvatarHit(row, localPos.x(), localPos.y())) {
                Q_EMIT avatarClicked(row, mapToScene(localPos));
                return;
            }
            // 媒体下载状态条命中：NotRequested/Failed → 发起/重试
            //（qltox startMediaDownload mw:4114 口径；防重复由 MediaHost
            //  m_pending/m_files/m_cache 与行状态双保险）
            {
                const int rowState = m_items[row].downloadState;
                const bool rowIsAudio = m_items[row].isAudio;
                const QString rowUrl = m_items[row].mediaUrl;
                if (!rowIsAudio && !rowUrl.isEmpty()
                    && (rowState == MediaDlNotRequested
                        || rowState == MediaDlFailed)
                    && msgMediaBarRect(m_items[row], m_contentView->width())
                        .contains(localPos)) {
                    m_items[row].downloadState = MediaDlInProgress;
                    m_items[row].downloadProgress = 0;
                    m_items[row].downloadSpeedBps = 0;
                    m_items[row].errorMsg = QString();
                    if (MessageRowItem* rowItem = m_visibleRows.value(row)) {
                        rowItem->setMessageDataNoUpdate(m_items[row]);
                        rowItem->update();
                    }
                    MediaHost::instance()->download(rowUrl);
                    return;
                }
            }
            // 媒体本体命中：单击即开（触摸端无 qltox 双击依赖；状态条段
            // 未命中才走这里——Completed 状态下点条带 = 开预览）
            {
                const int bodyMt = static_cast<int>(
                    msgMediaTypeOf(m_items[row]));
                if (bodyMt != static_cast<int>(MsgMediaType::Unknown)
                    && bodyMt != static_cast<int>(MsgMediaType::Text)
                    && msgMediaRect(m_items[row], m_contentView->width())
                        .contains(localPos)) {
                    Q_EMIT mediaBodyTapped(row, bodyMt);
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
                const auto vr = viewContentsRect();
                const qreal contentX = localPos.x() - vr.left() + 0.0;
                const qreal contentY = localPos.y() - vr.top()
                    + scrollPos().y();
                // 长按头像：弹头像菜单（与单击头像同款行为）
                if (isAvatarHit(row, contentX, contentY)) {
                    Q_EMIT avatarLongPressed(row, pos);
                    return;
                }
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

    m_scrollIdleTimer.setSingleShot(true);
    m_scrollIdleTimer.setInterval(160);
    connect(&m_scrollIdleTimer, &QTimer::timeout, this, &MessageListWidget::flushPendingVisibleRefresh);

    m_coalesceTimer.setSingleShot(true);
    connect(&m_coalesceTimer, &QTimer::timeout, this, &MessageListWidget::flushCoalescedRefresh);

    connect(this, &QskScrollBox::scrollPosChanged,
        this, &MessageListWidget::onScrollPosChangedForTracking);

    // 淡入计时器：构造时连接一次（成员槽）。
    // 不能在 populateMessages() 里反复 connect lambda + Qt::UniqueConnection ——
    // UniqueConnection 只支持成员函数槽（Qt 源码 qobject.h 里有断言），
    // lambda 每次 connect 都会新增一条连接 → 计时器触发多次 → 建多个动画器。
    m_fadeTimer.setSingleShot(true);
    connect(&m_fadeTimer, &QTimer::timeout, this, &MessageListWidget::onFadeTimeout);

    // 当前打开的消息列表实时追加新收到的该频道消息：
    // MessageStore 收到 push 后发 messageAdded，此处按 protocol + m_chatId 过滤
    // （protocol 必须参与：不同 protocol 的 chatId 可能相同），
    // 命中即复用 appendMessage（追加、算行高、更新尺寸、滚到底）
    connect(MessageStore::instance(), &MessageStore::messageAdded,
        this, [this](const QString& protocol, const QString& chatId,
                     const MessageItem& item) {
            if (m_chatId.isEmpty() || chatId != m_chatId) {
                return;
            }
            if (!m_chanProtocol.isEmpty() && !protocol.isEmpty()
                && protocol != m_chanProtocol) {
                return;
            }
            appendMessage(item);
        });

    // 头像下载完成 → 刷新可见行（重绘）
    connect(AvatarHost::instance(), &AvatarHost::avatarUpdated,
        this, [this]() {
            for (auto it = m_visibleRows.begin(); it != m_visibleRows.end(); ++it) {
                const int idx = it.key();
                // 没有头像 url 的行不需要因下载完成而重绘
                if (idx < 0 || idx >= m_items.size()
                    || m_items[idx].avatarUrl.isEmpty()) {
                    continue;
                }
                it.value()->refreshAvatar();
                queueVisibleRefresh(idx);
            }
            if (m_isScrolling) {
                m_scrollIdleTimer.start();
            }
        });

    // 媒体行统一更新：url 命中（mediaUrl/thumbUrl）→ 全行落状态（不可见行
    // 也不丢）+ 可见行走合并刷新（与 avatarUpdated 同款节流口径）
    auto applyMediaRow = [this](const QString& url, int state, int pct, int speed) {
        if (url.isEmpty()) {
            return;
        }
        for (int idx = 0; idx < m_items.size(); ++idx) {
            if (m_items[idx].mediaUrl != url && m_items[idx].thumbUrl != url) {
                continue;
            }
            if (state >= 0) {
                m_items[idx].downloadState = state;
                m_items[idx].downloadSpeedBps = speed;   // 状态切换即刻归零速率
                if (state == MediaDlInProgress) {
                    m_items[idx].errorMsg = QString();   // 重试再发起：清旧错误
                }
            } else {
                // 纯进度更新（state<0）：速率随进度泵实时刷新
                m_items[idx].downloadSpeedBps = speed;
            }
            m_items[idx].downloadProgress = pct;
            if (m_visibleRows.contains(idx)) {
                queueVisibleRefresh(idx);
            }
        }
        if (m_isScrolling) {
            m_scrollIdleTimer.start();
        }
    };
    // 下载完成（mediaUpdated，Task A 首发信号）→ Completed
    connect(MediaHost::instance(), &MediaHost::mediaUpdated,
        this, [applyMediaRow](const QString& url) {
            applyMediaRow(url, MediaDlCompleted, 100, 0);
        });
    // 状态迁移（InProgress/Completed/Failed）
    connect(MediaHost::instance(), &MediaHost::mediaStateChanged,
        this, [applyMediaRow](const QString& url, int state) {
            applyMediaRow(url, state,
                state == MediaDlCompleted ? 100 : 0, 0);
        });
    // 进度（state<0 → 只更进度 + 速率，不覆盖状态）
    connect(MediaHost::instance(), &MediaHost::mediaProgress,
        this, [applyMediaRow](const QString& url, int pct, int speedBps) {
            applyMediaRow(url, -1, pct, speedBps);
        });
    // 失败详情（mediaFailed，qltox mediaErrorMsg 链）：独立 lambda，
    // 专走 errorMsg 填充（mediaFailed 必伴随 mediaStateChanged(Failed)）
    connect(MediaHost::instance(), &MediaHost::mediaFailed,
        this, [this](const QString& url, const QString& errorInfo) {
            for (int idx = 0; idx < m_items.size(); ++idx) {
                if (m_items[idx].mediaUrl != url
                    && m_items[idx].thumbUrl != url) {
                    continue;
                }
                m_items[idx].errorMsg = errorInfo;
                if (m_visibleRows.contains(idx)) {
                    queueVisibleRefresh(idx);
                }
            }
            if (m_isScrolling) {
                m_scrollIdleTimer.start();
            }
        });
}

MessageListWidget::~MessageListWidget()
{
    m_scrollIdleTimer.stop();
    m_coalesceTimer.stop();
    m_pendingVisibleRefresh.clear();
    m_coalescedRefresh.clear();
    m_isScrolling = false;
    m_flushPending = false;
    for (auto* row : std::as_const(m_visibleRows)) {
        delete row;
    }
    m_visibleRows.clear();
}

void MessageListWidget::queueVisibleRefresh(int idx)
{
    if (idx < 0 || idx >= m_items.size()) {
        return;
    }
    if (!m_visibleRows.contains(idx)) {
        return;
    }
    m_coalescedRefresh.insert(idx);
    if (!m_coalesceTimer.isActive()) {
        m_coalesceTimer.start(0);
    }
}

void MessageListWidget::onScrollPosChangedForTracking()
{
    m_isScrolling = true;
    m_scrollIdleTimer.start();
    updateVisibleRows();
}

void MessageListWidget::forceClipRefresh()
{
    if (!isVisible() || !window() || !window()->isExposed()) {
        return;
    }
    if (m_contentView) {
        m_contentView->polish();
        m_contentView->update();
    }
    this->polish();
    this->update();
    if (window()) {
        window()->update();
    }
}

void MessageListWidget::flushPendingVisibleRefresh()
{
    m_isScrolling = false;
    if (!isVisible() || !window() || !window()->isExposed()) {
        m_pendingVisibleRefresh.clear();
        return;
    }
    if (m_pendingVisibleRefresh.isEmpty()) {
        if (m_coalescedRefresh.isEmpty()) {
            forceClipRefresh();
        }
        return;
    }
    QVector<MessageRowItem*> toUpdate;
    toUpdate.reserve(m_pendingVisibleRefresh.size());
    for (int idx : std::as_const(m_pendingVisibleRefresh)) {
        if (idx < 0 || idx >= m_items.size()) {
            continue;
        }
        auto it = m_visibleRows.find(idx);
        if (it == m_visibleRows.end() || !it.value()) {
            continue;
        }
        it.value()->setMessageDataNoUpdate(m_items[idx]);
        toUpdate.append(it.value());
    }
    m_pendingVisibleRefresh.clear();
    for (auto* row : toUpdate) {
        row->update();
    }
    forceClipRefresh();
}

void MessageListWidget::flushCoalescedRefresh()
{
    if (!isVisible() || !window() || !window()->isExposed()) {
        m_coalescedRefresh.clear();
        return;
    }
    if (m_coalescedRefresh.isEmpty()) {
        return;
    }
    QVector<MessageRowItem*> toUpdate;
    toUpdate.reserve(m_coalescedRefresh.size());
    for (int idx : std::as_const(m_coalescedRefresh)) {
        if (idx < 0 || idx >= m_items.size()) {
            continue;
        }
        auto it = m_visibleRows.find(idx);
        if (it == m_visibleRows.end() || !it.value()) {
            continue;
        }
        it.value()->setMessageDataNoUpdate(m_items[idx]);
        toUpdate.append(it.value());
    }
    m_coalescedRefresh.clear();
    for (auto* row : toUpdate) {
        row->update();
    }
    forceClipRefresh();
}

void MessageListWidget::populateMessages()
{
    m_fadeTimer.stop();
    m_scrollIdleTimer.stop();
    m_coalesceTimer.stop();
    m_pendingVisibleRefresh.clear();
    m_coalescedRefresh.clear();
    m_isScrolling = false;
    m_flushPending = false;
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
    // 淡入：用成员计时器而非 singleShot，stopFadeIn() 才能真正取消
    // （页面已切走时启动动画会把列表置为全透明 → 白屏）
    m_fadeTimer.stop();
    m_fadeTimer.start(100);
}

void MessageListWidget::onFadeTimeout()
{
    if (!window() || !isVisible()) {
        return;
    }
    m_fadeAnimator = new MessageListAnimator(this, this);
    m_fadeAnimator->setWindow(window());
    m_fadeAnimator->start();

    // 兜底：动画时长 800ms，若未被驱动推进则强制复位透明度
    QTimer::singleShot(1000, this, [this]() {
        if (m_fadeAnimator) {
            m_fadeAnimator->stop();
            delete m_fadeAnimator;
            m_fadeAnimator = nullptr;
        }
        setOpacity(1.0);
    });
}

void MessageListWidget::stopFadeIn()
{
    m_fadeTimer.stop();
    m_scrollIdleTimer.stop();
    m_coalesceTimer.stop();
    m_pendingVisibleRefresh.clear();
    m_coalescedRefresh.clear();
    m_isScrolling = false;
    m_flushPending = false;
    if (m_fadeAnimator) {
        m_fadeAnimator->stop();
        delete m_fadeAnimator;
        m_fadeAnimator = nullptr;
    }
    setOpacity(1.0);
}

void MessageListWidget::kickMediaPrefetch(const MessageItem& item)
{
    AvatarHost::instance()->download(item.avatarUrl);
    // 视频缩略：thumbUrl 优先（CS info.thumbnail_url），mxc + 宽高时构造
    // /_matrix/media/v3/thumbnail（spec 标准端点，不加自定义 API）
    if (!item.thumbUrl.isEmpty()) {
        MediaHost::instance()->download(item.thumbUrl);
    } else if (item.msgType.contains(QString::fromUtf8("video"))
               && !item.mediaUrl.isEmpty()
               && item.mediaWidth > 0 && item.mediaHeight > 0) {
        const QString tu = MediaHost::thumbnailUrl(item.mediaUrl,
            (int)item.mediaWidth, (int)item.mediaHeight);
        if (!tu.isEmpty()) {
            MediaHost::instance()->download(tu);
        }
    }
    // 自动预取（qltox chatview.cpp:4238 口径）：image 类恒自动；
    // video/file 本体 <1MB 自动（大文件等状态条手动 tap，不接播放/抽帧）。
    // 未知大小（=0）同过门槛：qltox fileSize UnkSize=-1 亦通过同款门槛
    QString mt = item.msgType;
    if (mt.startsWith(QString::fromUtf8("m."))) {
        mt = mt.mid(2);
    }
    if (mt == QString::fromUtf8("image")
        || item.mediaMime.startsWith(QString::fromUtf8("image/"))) {
        MediaHost::instance()->autoDownload(item.msgType, item.mediaMime,
                                            item.mediaUrl);
    } else if (!item.mediaUrl.isEmpty()
               && (item.fileSizeBytes <= 0
                   || item.fileSizeBytes < 1048576)
               && (mt == QString::fromUtf8("video")
                   || mt == QString::fromUtf8("file"))) {
        MediaHost::instance()->download(item.mediaUrl);
    }
}

void MessageListWidget::appendMessage(const MessageItem& item)
{
    m_items.append(item);
    kickMediaPrefetch(item);

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
            queueVisibleRefresh(i);
            if (m_isScrolling) {
                m_scrollIdleTimer.start();
            }
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
        queueVisibleRefresh(row);
        if (m_isScrolling) {
            m_scrollIdleTimer.start();
        }
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
            queueVisibleRefresh(i);
            if (m_isScrolling) {
                m_scrollIdleTimer.start();
            }
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
            queueVisibleRefresh(i);
            if (m_isScrolling) {
                m_scrollIdleTimer.start();
            }
        }
        return;
    }
}

void MessageListWidget::setChannel(const QString& protocol, const QString& chatId,
                                   const QString& chanId)
{
    m_scrollIdleTimer.stop();
    m_coalesceTimer.stop();
    m_pendingVisibleRefresh.clear();
    m_coalescedRefresh.clear();
    m_isScrolling = false;
    m_flushPending = false;
    if (m_fadeAnimator) {
        m_fadeAnimator->stop();
        delete m_fadeAnimator;
        m_fadeAnimator = nullptr;
    }
    setOpacity(1.0);

    m_chatId = chatId;
    m_chanProtocol = protocol;
    m_chanId = chanId;
    m_items.clear();
    m_rowHeights.clear();
    m_rowYOffsets.clear();
    for (auto* row : std::as_const(m_visibleRows)) {
        // 先隐藏再延迟删除：否则换频道瞬间旧行仍绘制在视口里（残影）
        row->setVisible(false);
        row->deleteLater();
    }
    m_visibleRows.clear();
    if (m_contentView) {
        m_contentView->setSize(QSizeF(0, 0));
    }

    if (m_chatId.isEmpty() && m_chanId.isEmpty()) {
        populateMessages();
        return;
    }

    // 优先从 SQLite 加载历史：
    // 1) 有权威 chanid（channels 表主键）时直接按 chanid 读
    // 2) 否则按 protocol + "_" + chatId 组装
    // 3) 都为空时按 chatId 在 channels 表中反查 chanid 再读
    QVector<MessageItem> msgs;
    if (!m_chanId.isEmpty()) {
        msgs = MsgDbBridge::loadMessagesByChanid(m_chanId, 50);
    } else if (!m_chanProtocol.isEmpty()) {
        msgs = MsgDbBridge::loadMessages(m_chanProtocol, m_chatId, 50);
    }
    if (msgs.isEmpty() && !m_chatId.isEmpty()) {
        // 兜底：protocol 缺失时从 chanid 前缀反推（与下方 store 合并同一口径），
        // 避免把别的 protocol 的同 chatId 会话消息加载进来。
        QString fallbackProto = m_chanProtocol;
        if (fallbackProto.isEmpty() && !m_chanId.isEmpty()) {
            fallbackProto = m_chanId.section(QLatin1Char('_'), 0, 0);
        }
        msgs = MsgDbBridge::loadMessagesByChatId(fallbackProto, m_chatId, 50);
    }
    {
        // 合并 MessageStore（进程内实时消息）。
        // 去重只按 DB rowid：旧的 sender|content|time 去重会把「同人同内容
        // 同秒」的两条真实消息误删一条。
        QString storeProto = m_chanProtocol;
        if (storeProto.isEmpty() && !m_chanId.isEmpty()) {
            // chanid 形如 "<proto>_<chatId>"
            storeProto = m_chanId.section(QLatin1Char('_'), 0, 0);
        }
        const QList<MessageItem> storeMsgs =
            MessageStore::instance()->getMessages(storeProto, m_chatId);
        if (!storeMsgs.isEmpty()) {
            QSet<qint64> seenRowid;
            for (const auto& m : msgs) {
                if (m.dbRowid > 0) {
                    seenRowid.insert(m.dbRowid);
                }
            }
            for (const auto& m : storeMsgs) {
                // 无 rowid 的消息只存在于内存，没有可比较的身份，一律保留
                if (m.dbRowid > 0 && seenRowid.contains(m.dbRowid)) {
                    continue;
                }
                msgs.append(m);
            }
        }
    }
    for (const auto& msg : msgs) {
        m_items.append(msg);
        kickMediaPrefetch(msg);
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
    } else {
        setScrollPos(QPointF(0, 0));
    }

    // 若异步落库（MsgDbBridge::onIncomingMessage）尚未完成导致列表仍为空，
    // 短延迟后重试一次加载
    if (m_items.isEmpty() && !m_chatId.isEmpty()) {
        const QString p = m_chanProtocol;
        const QString c = m_chatId;
        const QString cid = m_chanId;
        QTimer::singleShot(200, this, [this, p, c, cid]() {
            if (p == m_chanProtocol && c == m_chatId && cid == m_chanId
                && m_items.isEmpty()) {
                setChannel(p, c, cid);
            }
        });
    }
}

void MessageListWidget::scrollToBottom()
{
    if (!m_contentView) {
        return;
    }
    qreal totalH = m_rowYOffsets.isEmpty() ? 0 : m_rowYOffsets.last();
    qreal viewH = viewContentsRect().height();
    if (viewH <= 0) {
        viewH = height();
    }
    if (totalH > viewH) {
        setScrollPos(QPointF(0, totalH - viewH));
    } else {
        setScrollPos(QPointF(0, 0));
    }
    updateVisibleRows();
}

void MessageListWidget::updateVisibleRows()
{
    if (!m_contentView || m_items.isEmpty()) {
        return;
    }
    // 布局缓存与数据必须一一对应，否则下面的下标访问会越界
    // 注意：m_rowYOffsets 是「前缀和」表，长度 = 消息数 + 1（末位为总高），
    // 不能直接跟 m_items.size() 比 —— 否则条件恒真，updateVisibleRows() 每次
    // 都提前返回，一行都不渲染 → 消息列表整表空白。
    if (m_rowYOffsets.size() != m_items.size() + 1
        || m_rowHeights.size() != m_items.size()) {
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
            MessageRowItem* doomed = it.value();
            it = m_visibleRows.erase(it);
            // 先隐藏再延迟删除：旧行在事件循环处理 deleteLater 前仍会绘制
            doomed->setVisible(false);
            doomed->deleteLater();
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
        } else {
            MessageRowItem* row = m_visibleRows[i];
            row->setY(m_rowYOffsets[i]);
            row->setWidth(m_contentView->width());
            row->setHeight(m_rowHeights[i]);
            if (m_isScrolling) {
                // 滚动中延迟视觉刷新，避开 QskScrollArea ClipItem ContentUpdateMask defer
                continue;
            }
            // 已在合并队列中的行交给 flushCoalescedRefresh 统一刷新
            if (m_coalescedRefresh.contains(i) || m_pendingVisibleRefresh.contains(i)) {
                continue;
            }
            row->setMessageDataNoUpdate(m_items[i]);
            row->update();
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

bool MessageListWidget::isAvatarHit(int row, qreal contentX, qreal contentY) const
{
    if (row < 0 || row >= m_items.size() || m_items[row].isSelf) {
        return false;
    }
    // 与 paint() 同口径：头像在气泡区域内垂直居中
    const qreal bubbleH = m_rowHeights[row] - NAME_HEIGHT - TIME_HEIGHT - 4;
    const qreal avatarX = SIDE_MARGIN;
    const qreal avatarSize = AVATAR_SIZE;
    qreal avatarY = m_rowYOffsets[row] + NAME_HEIGHT + (bubbleH - avatarSize) / 2.0;
    if (avatarY < NAME_HEIGHT + 2) avatarY = NAME_HEIGHT + 2;
    return contentX >= avatarX && contentX <= avatarX + avatarSize
        && contentY >= avatarY && contentY <= avatarY + avatarSize;
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
}

void MessageListAnimator::setup()
{
    // 动画真正开始时才把透明度降到 0：
    // 若在构造函数里就置 0，一旦动画未被驱动推进（窗口隐藏等），
    // 列表会一直保持全透明 —— 表现为白屏。
    if (m_target && m_target->isVisible()) {
        m_target->setOpacity(0.0);
    }
}

void MessageListAnimator::advance(qreal value)
{
    if (!m_target) {
        return;
    }
    // 目标已不可见或无 window 则停止动画并复位，避免在隐藏页上反复 setOpacity
    if (!m_target->isVisible()) {
        stop();
        m_target->setOpacity(1.0);
        return;
    }
    if (!m_target->window()) {
        stop();
        m_target->setOpacity(1.0);
        return;
    }
    if (value >= 1.0) {
        m_target->setOpacity(1.0);
        stop();
        return;
    }
    m_target->setOpacity(value);
}

void MessageListAnimator::done()
{
    if (m_target) {
        m_target->setOpacity(1.0);
    }
}

#include "moc_messagelist.cpp"
