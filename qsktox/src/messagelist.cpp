#include "messagelist.h"
#include "messagestore.h"
#include "msgdb_bridge.h"
#include "avatarhost.h"
#include "avatar_manager.h"
#include "avatarutil.h"
#include "mediahost.h"
#include "translatehost.h"
#include "linkpreviewhost.h"
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
#include <QProcess>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QPointer>
#include <QFile>
#include <QDir>

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
// 引用摘要条行高（批2，TG 式正文上方灰条）
static constexpr qreal QUOTE_LINE_H = 22;

// 卡片 header 右上角三钮：📋 source / 🌐 translate / 🔊 朗读。
// 本次空操作：仅绘制并存 Rect 锚点，不挂任何点击事件。
// 显示条件镜像 qltox chatview（chatview.cpp:1203-1214/1295-1306）：
// audio 类卡片整组不画；🔊 仅正文非空时画（m_speakRect 置空 Rect）。
// 左→右依 qltox 口径：source | translate | speak（speak 最贴行右缘）。

// ── 媒体消息分类（镜像 qltox etype 派发，chatview.cpp:4074/:2986 口径）──
// MsgMediaType 定义移至 messagelist.h（mediaBodyTapped 信号跨 TU 需要枚举可见）

// ── 渲染文本归一 helpers（批2）──

// 引用摘要行（qltox " -- Re:" 语义的 TG 式正文上方灰条）：
// replyTos 原文 elide（服务端键④ logcat 实测前不做显示名解析）
static QString quoteLineFor(const MessageItem& item) {
    if (item.replyTos.isEmpty()) {
        return QString();
    }
    QString q = item.replyTos.first();
    if (q.length() > 48) {
        q = q.left(48) + QString::fromUtf8("…");
    }
    return QString::fromUtf8("↩ ") + q;
}

// mention 短称（qltox attachReplyLinks 简化对位）：@user:server → @user；
// 未命中正文 token 原样保留，零命中零行为
static QString bodyForPaint(const MessageItem& item) {
    QString body = item.content;
    for (const QString& tok : item.mentions) {
        if (tok.isEmpty()) {
            continue;
        }
        const int at = body.indexOf(tok);
        if (at < 0) {
            continue;
        }
        QString short_ = tok.section(QLatin1Char(':'), 0, 0);
        if (!short_.startsWith(QLatin1Char('@'))) {
            short_ = QLatin1Char('@') + short_;
        }
        body.replace(at, tok.length(), short_);
    }
    return body;
}

// ── giflike 短视频 helpers（批4，qltox :4468-4493 口径）──

// ffmpeg 探测缓存：QStandardPaths::findExecutable 优先，一次 -version
// 试跑确认可执行；空串 = 无 ffmpeg（Android 无二进位 → giflike 整链 no-op
// 退化为普通视频链）。首次调用后进程生命周期缓存。
static QString ffmpegPath() {
    static QString cached;
    static bool probed = false;
    if (probed) {
        return cached;
    }
    probed = true;
    cached = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (cached.isEmpty()) {
        return cached;
    }
    QProcess probe;
    probe.start(cached, QStringList() << QStringLiteral("-version"));
    probe.waitForFinished(3000);
    if (probe.exitCode() != 0) {
        cached = QString();
    }
    return cached;
}

// giflike 判定：1–3s webm/mp4 短视频 = 循环 GIF（qltox 同判据）
static bool isGifLikeVideo(const MessageItem& item) {
    if (item.durationSec < 1 || item.durationSec > 3) {
        return false;
    }
    const QString url = item.mediaUrl.toLower();
    return url.endsWith(QStringLiteral(".webm"))
        || url.endsWith(QStringLiteral(".mp4"));
}

// 大文件超时放宽（qltox restapi:496 口径：50KB/s 估算 + 15s 余量；
// 大小未知保持默认 35s 兜底）；上限 1h 防呆
static int mediaDownloadTimeout(qint64 sizeBytes) {
    if (sizeBytes <= 0) {
        return 0;
    }
    const qint64 est = 15 + sizeBytes / (50 * 1024);
    return (int)qMin<qint64>(est, 3600);
}

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
    if (t == QString::fromUtf8("audio")) {
        return MsgMediaType::Audio;   // 显式 msgtype 直判（isAudio 兜底之外）
    }
    // Misskey 桥 MIME 归一 + 兜底（qltox unknownparser:845 口径）：
    // msgtype 缺失/非标 → MIME 前缀 → URL 扩展名 → file；
    // mediaUrl 空 → Text。与 mainpage isAudio 判据（mainpage.cpp:361）
    // 互不冲突：audio tile 行内播链共用同一 source url。
    if (!item.mediaUrl.isEmpty()) {
        const QString mime = item.mediaMime.toLower();
        const QString url = item.mediaUrl.toLower();
        if (mime.startsWith(QStringLiteral("image/"))
            || url.endsWith(QStringLiteral(".gif"))) {
            return url.endsWith(QStringLiteral(".gif"))
                ? MsgMediaType::Gif : MsgMediaType::Image;
        }
        static const char* videoExts[] = {".webm", ".mp4", ".mkv",
            ".mov", ".m4v", ".avi"};
        if (mime.startsWith(QStringLiteral("video/"))) {
            return MsgMediaType::Video;
        }
        for (const char* ext : videoExts) {
            if (url.endsWith(QString::fromUtf8(ext))) {
                return MsgMediaType::Video;
            }
        }
        static const char* audioExts[] = {".mp3", ".ogg", ".m4a",
            ".wav", ".opus", ".flac"};
        if (mime.startsWith(QStringLiteral("audio/"))) {
            return MsgMediaType::Audio;
        }
        for (const char* ext : audioExts) {
            if (url.endsWith(QString::fromUtf8(ext))) {
                return MsgMediaType::Audio;
            }
        }
        return MsgMediaType::File;   // 媒体 url 兜底（qltox file 口径）
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
        // 三态：下载中 ⏳（audio 无状态条，pending 视觉只能挂 tile）/
        // 播放中 ⏸（暂停后 flag 归 false）/ 其余 🔊
        QString audioIcon = QString::fromUtf8("🔊");
        if (item.downloadState == MediaDlInProgress) {
            audioIcon = QString::fromUtf8("⏳");
        } else if (item.mediaPlaying) {
            audioIcon = QString::fromUtf8("⏸");
        }
        p->drawText(iconR, Qt::AlignCenter, audioIcon);
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

// ── 自动翻译/链接预览扩展块（qltox 翻译/预览渲染移植口径）──
// 译文块（TransDone && showTranslation）：小 2pt、muted、🌐 前缀；
// 预览卡（LpReady）：圆角底三行（site/title/desc），与译文互斥
//（显示译文时不画卡，qltox chatview.cpp:2157 口径）。

static constexpr qreal TR_GAP = 4;
static constexpr qreal LP_CARD_PAD = 6;

static void drawWrappedText(QPainter* painter, const QString& text,
                            const QFont& font, const QRectF& rect);

static qreal translateBlockHeight(const MessageItem& item, qreal textWidth)
{
    if (item.transState != MessageItem::TransDone
        || !item.showTranslation || item.translatedText.isEmpty()) {
        return 0.0;
    }
    QFont trFont;
    trFont.setPixelSize(14);
    const MessageTextMetrics m =
        measureMessageText(item.translatedText, trFont, textWidth);
    return TR_GAP + m.height;
}

static qreal linkPreviewCardHeight(const MessageItem& item)
{
    if (item.lpState != MessageItem::LpReady
        || item.showTranslation || item.lpUrl.isEmpty()) {
        return 0.0;
    }
    qreal h = LP_CARD_PAD;
    if (!item.lpSite.isEmpty()) {
        h += 14;
    }
    if (!item.lpTitle.isEmpty()) {
        h += 18;
    }
    if (!item.lpDesc.isEmpty()) {
        h += 14;
    }
    if (h <= LP_CARD_PAD) {
        return 0.0;
    }
    return h + LP_CARD_PAD;
}

static void paintTranslateBlock(QPainter* painter, const MessageItem& item,
                                qreal x, qreal y, qreal width)
{
    if (width <= 0.0) {
        return;
    }
    QFont trFont;
    trFont.setPixelSize(14);
    painter->save();
    painter->setFont(trFont);
    painter->setPen(QColor("#9aa8c0"));
    // 🌐 前缀（qltox 译文行口径，qFromUtf8 不适用 Qt6：qsktox 源码 Qt6
    // 按源编码 UTF-8 读取；qskcommit 口径用 QStringLiteral/原始字面量）
    const QString shown = QStringLiteral("🌐 ") + item.translatedText;
    const qreal trH = translateBlockHeight(item, width) - TR_GAP;
    drawWrappedText(painter, shown, trFont, QRectF(x, y, width, trH));
    painter->restore();
}

static void paintLinkPreviewCard(QPainter* painter, const MessageItem& item,
                                 qreal x, qreal y, qreal width)
{
    const qreal cardH = linkPreviewCardHeight(item);
    if (cardH <= 0.0 || width <= 0.0) {
        return;
    }
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0x23, 0x23, 0x36)); // 微异于气泡底 #2a2a3e
    painter->drawRoundedRect(QRectF(x, y, width, cardH), 8, 8);
    const qreal inner = width - LP_CARD_PAD * 2;
    qreal cy = y + LP_CARD_PAD;
    if (!item.lpSite.isEmpty()) {
        QFont siteFont;
        siteFont.setPixelSize(11);
        QFontMetrics fmS(siteFont);
        painter->setFont(siteFont);
        painter->setPen(QColor("#8a97b5"));
        painter->drawText(QRectF(x + LP_CARD_PAD, cy, inner, 14),
            Qt::AlignLeft | Qt::AlignVCenter,
            fmS.elidedText(item.lpSite, Qt::ElideRight, (int)inner));
        cy += 14;
    }
    if (!item.lpTitle.isEmpty()) {
        QFont titleFont;
        titleFont.setPixelSize(14);
        QFontMetrics fmT(titleFont);
        painter->setFont(titleFont);
        painter->setPen(QColor("#e6e9f0"));
        painter->drawText(QRectF(x + LP_CARD_PAD, cy, inner, 18),
            Qt::AlignLeft | Qt::AlignVCenter,
            fmT.elidedText(item.lpTitle, Qt::ElideRight, (int)inner));
        cy += 18;
    }
    if (!item.lpDesc.isEmpty()) {
        QFont descFont;
        descFont.setPixelSize(12);
        QFontMetrics fmD(descFont);
        painter->setFont(descFont);
        painter->setPen(QColor("#9aa8c0"));
        painter->drawText(QRectF(x + LP_CARD_PAD, cy, inner, 14),
            Qt::AlignLeft | Qt::AlignVCenter,
            fmD.elidedText(item.lpDesc, Qt::ElideRight, (int)inner));
    }
    painter->restore();
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
        // caption（=content 非空时）作媒体上方正文行（批5a 移顶部），计入行高
        if (!item.content.isEmpty()) {
            const MessageTextMetrics cm =
                measureMessageText(bodyForPaint(item), msgFont, mm.bubbleW);
            bubbleHeight += cm.height + BUBBLE_PADDING_V;
        }
    } else {
        // 批2：caption 行内短称后计量（mention token 逐位替换不改语序）
        const MessageTextMetrics tm =
            measureMessageText(bodyForPaint(item), msgFont, textWidth);
        bubbleHeight = tm.height + BUBBLE_PADDING_V * 2;
        // 批2：reply 引用摘要条（TG 式正文上方灰条）计入行高
        if (!quoteLineFor(item).isEmpty()) {
            bubbleHeight += QUOTE_LINE_H;
        }
        // 翻译/预览扩展块计入行高（仅文本行；媒体+caption 行不套）
        bubbleHeight += translateBlockHeight(item, textWidth);
        bubbleHeight += linkPreviewCardHeight(item);
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

    // 计算内容宽度（批2：mention 短称替换后渲染/计量同源）
    QString displayContent = bodyForPaint(m_item);
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
            // 批5a：caption 移顶（qltox paintMediaContent:494 口径），
            // 媒体框整体下移 caption 高度
            const qreal captionH = displayContent.isEmpty() ? 0.0 :
                measureMessageText(displayContent, msgFont, mm.bubbleW).height
                    + BUBBLE_PADDING_V;
            const QRectF mediaRect(bubbleX + BUBBLE_PADDING_H,
                bubbleY + BUBBLE_PADDING_V + captionH, mm.bubbleW, mm.bubbleH);
            paintMediaBody(painter, m_item, m_mediaImage, mm, mediaRect,
                Qt::white, QColor(0xff, 0xff, 0xff, 0x99), msgFont);
            // caption 正文行（顶部，content 非空），同款换行绘制
            if (!displayContent.isEmpty()) {
                painter->setPen(Qt::white);
                painter->setFont(msgFont);
                drawWrappedText(painter, displayContent, msgFont,
                    QRectF(bubbleX + BUBBLE_PADDING_H,
                        bubbleY + BUBBLE_PADDING_V,
                        mm.bubbleW, captionH - BUBBLE_PADDING_V));
            }
        } else {
            // 批2：reply 引用摘要条（正文上方灰条）+ 正文下移一行
            const QString quote = quoteLineFor(m_item);
            // 消息文字（支持多行）
            painter->setPen(Qt::white);
            const QRectF textRect(bubbleX + BUBBLE_PADDING_H,
                bubbleY + BUBBLE_PADDING_V + (quote.isEmpty() ? 0.0 : QUOTE_LINE_H),
                contentWidth, textHeight);
            if (!quote.isEmpty()) {
                painter->setPen(QColor("#7f8fb0"));
                painter->drawText(QRectF(bubbleX + BUBBLE_PADDING_H,
                    bubbleY + BUBBLE_PADDING_V, contentWidth, QUOTE_LINE_H),
                    Qt::AlignLeft | Qt::AlignVCenter, quote);
                painter->setPen(Qt::white);
            }
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
            // 翻译/预览扩展块（qltox 口径：译文双行同气泡；显示译文时
            // 不画预览卡；本批卡片无点击跳转）
            {
                qreal extY = textRect.y() + textHeight;
                const qreal trH = translateBlockHeight(m_item, textWidth);
                if (trH > 0.0) {
                    paintTranslateBlock(painter, m_item,
                        bubbleX + BUBBLE_PADDING_H, extY, textWidth);
                    extY += trH;
                }
                if (linkPreviewCardHeight(m_item) > 0.0) {
                    paintLinkPreviewCard(painter, m_item,
                        bubbleX + BUBBLE_PADDING_H, extY, textWidth);
                }
            }

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
            // 批5a：caption 移顶（同 self 口径），媒体框整体下移
            const qreal captionH2 = displayContent.isEmpty() ? 0.0 :
                measureMessageText(displayContent, msgFont, mm.bubbleW).height
                    + BUBBLE_PADDING_V;
            const QRectF mediaRect(bubbleX + BUBBLE_PADDING_H,
                bubbleY + BUBBLE_PADDING_V + captionH2, mm.bubbleW, mm.bubbleH);
            paintMediaBody(painter, m_item, m_mediaImage, mm, mediaRect,
                QColor("#ddd"), QColor(0x8f, 0xa0, 0xc0), msgFont);
            // caption 正文行（顶部，content 非空），同款换行绘制
            if (!displayContent.isEmpty()) {
                painter->setPen(QColor("#ddd"));
                painter->setFont(msgFont);
                drawWrappedText(painter, displayContent, msgFont,
                    QRectF(bubbleX + BUBBLE_PADDING_H,
                        bubbleY + BUBBLE_PADDING_V,
                        mm.bubbleW, captionH2 - BUBBLE_PADDING_V));
            }
        } else {
            // 批2：reply 引用摘要条（正文上方灰条）+ 正文下移一行
            const QString quote = quoteLineFor(m_item);
            // 消息文字（支持多行）
            painter->setPen(QColor("#ddd"));
            if (!quote.isEmpty()) {
                painter->setPen(QColor("#9aa8c0"));
                painter->drawText(QRectF(bubbleX + BUBBLE_PADDING_H,
                    bubbleY + BUBBLE_PADDING_V, contentWidth, QUOTE_LINE_H),
                    Qt::AlignLeft | Qt::AlignVCenter, quote);
                painter->setPen(QColor("#ddd"));
            }
            const QRectF textRect(bubbleX + BUBBLE_PADDING_H,
                bubbleY + BUBBLE_PADDING_V + (quote.isEmpty() ? 0.0 : QUOTE_LINE_H),
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
            // 同上：翻译/预览扩展块（他人消息分支）
            {
                qreal extY2 = textRect.y() + textHeight;
                const qreal trH2 = translateBlockHeight(m_item, textWidth);
                if (trH2 > 0.0) {
                    paintTranslateBlock(painter, m_item,
                        bubbleX + BUBBLE_PADDING_H, extY2, textWidth);
                    extY2 += trH2;
                }
                if (linkPreviewCardHeight(m_item) > 0.0) {
                    paintLinkPreviewCard(painter, m_item,
                        bubbleX + BUBBLE_PADDING_H, extY2, textWidth);
                }
            }
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
        ^ qHash(m_item.mediaUrl)
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
        ^ qHash(m_item.mediaPlaying ? 1 : 0)
        ^ qHash(static_cast<qulonglong>(m_item.fileSizeBytes))
        ^ m_mediaTick
        ^ qHash(m_item.transState)
        ^ qHash(m_item.translatedText)
        ^ (m_item.showTranslation ? 0x5555u : 0u)
        ^ qHash(m_item.lpState) ^ qHash(m_item.lpUrl)
        ^ qHash(m_item.lpTitle) ^ qHash(m_item.lpDesc)
        ^ qHash(m_item.lpSite);
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
    // 批4：giflike 短视频（1–3s webm/mp4）→ 循环 GIF 行内播（qltox 口径）
    const bool gifLike = (mt == MsgMediaType::Video) && isGifLikeVideo(m_item);
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
    if (gifLike) {
        // giflike：伪源 "@gif@" 键（setupGifMovie 内剥离后读 CacheDb 字节）
        const QString pseudo = m_item.mediaUrl + QStringLiteral("@gif@");
        if (!MediaHost::instance()->getGifLikeBytes(m_item.mediaUrl).empty()) {
            m_mediaSrc = pseudo;
            setupGifMovie(pseudo);
            m_mediaImage = m_gifMovie ? m_gifMovie->currentImage() : QImage();
            m_dirty = true;
            return;
        }
        // 转码未产出（未下载完/无 ffmpeg）→ 下落常规视频缩略图链
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
        // miss（尚未下载完成）→ 不启动动画，保持占位静帧。
        // 批4：giflike 伪源 "@gif@" 后缀 → 剥离后读 @gif@ 键字节
        std::vector<uint8_t> bytes;
        if (src.endsWith(QStringLiteral("@gif@"))) {
            bytes = MediaHost::instance()->getGifLikeBytes(
                src.left(src.size() - 5));
        } else {
            bytes = MediaHost::instance()->getBytes(src);
        }
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

    // 自动翻译/链接预览扫描尾随防抖（见 h 内注释：进入动画窗口外单次）
    m_autoScanTimer.setSingleShot(true);
    m_autoScanTimer.setInterval(350);
    connect(&m_autoScanTimer, &QTimer::timeout, this, [this]() {
        scanAutoFeatures(m_lastScanMin, m_lastScanMax);
    });

    m_contentView = new QQuickItem(this);
    setScrolledItem(m_contentView);

    // ── 自动翻译 / 链接预览回填接线（回程 GUI 信号 → 行内字段 → 重排）──
    connect(TranslateHost::instance(), &TranslateHost::translateDone, this,
        [this](qint64 key, bool ok, const QString& text, const QString& err) {
        for (int i = 0; i < m_items.size(); ++i) {
            MessageItem& it = m_items[i];
            if (it.translateKey != key
                || it.transState != MessageItem::TransInFlight) {
                continue;
            }
            if (ok && !text.isEmpty()) {
                it.translatedText = text;
                it.translateError = QString();
                it.transState = MessageItem::TransDone;
                it.showTranslation = true;
                // 持久面：translations 表缓存（msgRowid<=0 不落）
                MsgDbBridge::translationSave(it.dbRowid, QStringLiteral("zh"),
                    text);
            } else {
                // 失败：UI 静默（host 侧已控制台日志）；置终态防扫描重试
                it.translateError = err;
                it.transState = MessageItem::TransDone;
                it.showTranslation = false;
            }
            if (MessageRowItem* rowItem = m_visibleRows.value(i)) {
                rowItem->setMessageDataNoUpdate(m_items[i]);
            }
            refreshLayoutAndView();
            break;
        }
    });
    connect(LinkPreviewHost::instance(), &LinkPreviewHost::linkReady, this,
        [this](const QString& urlKey, const LinkPreviewData& data) {
        for (int i = 0; i < m_items.size(); ++i) {
            MessageItem& it = m_items[i];
            if (it.lpUrl != urlKey
                || it.lpState != MessageItem::LpScheduled) {
                continue;
            }
            if (data.ok) {
                it.lpTitle = data.title;
                it.lpDesc = data.description;
                it.lpSite = data.siteName.isEmpty()
                    ? data.url : data.siteName;
                it.lpState = MessageItem::LpReady;
            } else {
                it.lpState = MessageItem::LpFailed;
            }
            if (MessageRowItem* rowItem = m_visibleRows.value(i)) {
                rowItem->setMessageDataNoUpdate(m_items[i]);
            }
            refreshLayoutAndView();
            break;
        }
    });

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
            // 🌐 头钮命中（首个接线头钮）：Done 行切 showTranslation；
            // 锚点数学与 msgDrawHeaderButtons 同源（行顶 header 带）
            if (m_items[row].transState == MessageItem::TransDone) {
                const auto vrTap = viewContentsRect();
                const QPointF cPos(localPos.x() - vrTap.left(),
                    localPos.y() - vrTap.top() + scrollPos().y());
                const qreal btnAreaT = HDR_BTN_SIZE * 3 + HDR_BTN_GAP * 2;
                const qreal srcXh = m_contentView->width() - SIDE_MARGIN - btnAreaT;
                const QRectF trRect(
                    srcXh + HDR_BTN_SIZE + HDR_BTN_GAP,
                    m_rowYOffsets[row] + (NAME_HEIGHT - HDR_BTN_SIZE) / 2.0,
                    HDR_BTN_SIZE, HDR_BTN_SIZE);
                if (trRect.contains(cPos)) {
                    m_items[row].showTranslation = !m_items[row].showTranslation;
                    refreshLayoutAndView();
                    return;
                }
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
                    // 批5b：大文件（>1MB 状态条手动档）按 qltox 50KB/s 估算放权超时
                    MediaHost::instance()->download(rowUrl,
                        mediaDownloadTimeout(m_items[row].fileSizeBytes));
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

    // push 编辑（m.replace）命中：MessageStore 内存桶 mutate 后转发信号，
    // 本列表按 msgId 原地更新行（批3）
    connect(MessageStore::instance(), &MessageStore::messageContentUpdated,
        this, [this](const QString& protocol, const QString& chatId,
                     const QString& msgId, const QString& newContent) {
            if (m_chatId.isEmpty() || chatId != m_chatId) {
                return;
            }
            if (!m_chanProtocol.isEmpty() && !protocol.isEmpty()
                && protocol != m_chanProtocol) {
                return;
            }
            replaceRowContent(msgId, newContent);
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
    // 下载完成（mediaUpdated，Task A 首发信号）→ Completed；
    // 批4：顺带泵 giflike 待转码任务（源字节此刻已落 CacheDb）
    connect(MediaHost::instance(), &MediaHost::mediaUpdated,
        this, [this, applyMediaRow](const QString& url) {
            applyMediaRow(url, MediaDlCompleted, 100, 0);
            pumpGifLikeJobs();
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
            int matched = 0;
            for (int idx = 0; idx < m_items.size(); ++idx) {
                if (m_items[idx].mediaUrl != url
                    && m_items[idx].thumbUrl != url) {
                    continue;
                }
                ++matched;
                m_items[idx].errorMsg = errorInfo;
                qWarning("[media-fail] url=%s idx=%d msgType=%s mime=%s err=%s",
                         qPrintable(url), idx,
                         qPrintable(m_items[idx].msgType),
                         qPrintable(m_items[idx].mediaMime),
                         qPrintable(errorInfo));
                if (m_visibleRows.contains(idx)) {
                    queueVisibleRefresh(idx);
                }
            }
            if (matched == 0) {
                qWarning("[media-fail] unmatched url=%s err=%s",
                         qPrintable(url), qPrintable(errorInfo));
            }
            if (m_isScrolling) {
                m_scrollIdleTimer.start();
            }
        });
}

void MessageListWidget::setAudioPlaying(const QString& url, bool on)
{
    for (int idx = 0; idx < m_items.size(); ++idx) {
        const bool want = (m_items[idx].mediaUrl == url) && on;
        if (m_items[idx].mediaPlaying == want) {
            continue;
        }
        m_items[idx].mediaPlaying = want;
        if (m_visibleRows.contains(idx)) {
            queueVisibleRefresh(idx);
        }
    }
    if (m_isScrolling) {
        m_scrollIdleTimer.start();
    }
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
        MediaHost::instance()->download(item.mediaUrl,
                                        mediaDownloadTimeout(item.fileSizeBytes));
        // 批4：giflike 待转码键登记（泵在 mediaUpdated / 本函数尾触发）
        if (isGifLikeVideo(item) && !m_gifLikeJobs.contains(item.mediaUrl)
            && !m_gifLikeRunning.contains(item.mediaUrl)) {
            m_gifLikeJobs.insert(item.mediaUrl);
        }
    }
    pumpGifLikeJobs();
}

void MessageListWidget::pumpGifLikeJobs()
{
    if (m_gifLikeJobs.isEmpty()) {
        return;
    }
    // 无 ffmpeg（Android 无二进位）→ giflike 整链 no-op，任务刺消退化为
    // 常规视频链（refreshMedia giflike miss 分支自然走缩略图）
    const QString ff = ffmpegPath();
    if (ff.isEmpty()) {
        m_gifLikeJobs.clear();
        return;
    }
    for (auto it = m_gifLikeJobs.begin(); it != m_gifLikeJobs.end();) {
        const QString url = *it;
        if (m_gifLikeRunning.contains(url)) {
            ++it;
            continue;
        }
        // 归属消息（取判据字段）；行不在列表（切频道回收）→ 刺消
        MessageItem srcItem;
        bool found = false;
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items[i].mediaUrl == url) {
                srcItem = m_items[i];
                found = true;
                break;
            }
        }
        if (!found || !isGifLikeVideo(srcItem)) {
            it = m_gifLikeJobs.erase(it);
            continue;
        }
        const std::vector<uint8_t> src = MediaHost::instance()->getBytes(url);
        if (src.empty()) {
            // 源未落 CacheDb → 留任务，等 mediaUpdated 泵再入
            ++it;
            continue;
        }
        // 临时输入/输出（tempPath 一文件一 hash，完成后清理）
        const QString tmpDir = QDir::tempPath();
        const QString tag =
            QString::number(qHash(url), 16);
        const QString inPath = tmpDir + QStringLiteral("/qsk_giflike_%1%2")
            .arg(tag,
                 url.endsWith(QStringLiteral(".mp4"), Qt::CaseInsensitive)
                    ? QStringLiteral(".mp4") : QStringLiteral(".webm"));
        const QString outPath = tmpDir + QStringLiteral("/qsk_giflike_%1.gif")
            .arg(tag);
        QFile in(inPath);
        if (!in.open(QIODevice::WriteOnly)) {
            it = m_gifLikeJobs.erase(it);
            continue;
        }
        const QByteArray srcBytes(
            reinterpret_cast<const char*>(src.data()), (int)src.size());
        if (in.write(srcBytes) != srcBytes.size()) {
            in.close();
            QFile::remove(inPath);
            it = m_gifLikeJobs.erase(it);
            continue;
        }
        in.close();
        // 转码：palettegen/paletteuse 单命令（split 一次取两抽），fps=12，
        // 宽 ≤320（qltox :4487 口径）；-an 去音轨。argv 直传不走 shell。
        QProcess* proc = new QProcess(this);
        connect(proc,
            (void (QProcess::*)(int, QProcess::ExitStatus))&QProcess::finished,
            this, [this, proc, url, inPath, outPath](int code,
                QProcess::ExitStatus) {
                proc->deleteLater();
                m_gifLikeRunning.remove(url);
                m_gifLikeProcs.remove(url);
                QFile::remove(inPath);
                QFile gifOut(outPath);
                if (code == 0 && gifOut.size() > 0
                    && gifOut.open(QIODevice::ReadOnly)) {
                    const QByteArray gif = gifOut.readAll();
                    gifOut.close();
                    MediaHost::instance()->storeGifLike(
                        url, std::string(gif.constData(), gif.size()));
                }
                QFile::remove(outPath);
                pumpGifLikeJobs();
            });
        m_gifLikeRunning.insert(url);
        m_gifLikeProcs.insert(url, proc);
        proc->start(ff, QStringList()
            << QStringLiteral("-y")
            << QStringLiteral("-i") << inPath
            << QStringLiteral("-filter_complex")
            << QStringLiteral("fps=12,scale=min(iw\\,320):-2:flags=lanczos,"
                              "split[s0][s1];[s0]palettegen=stats_mode=diff[p];"
                              "[s1][p]paletteuse=dither=bayer:bayer_scale=5")
            << QStringLiteral("-an") << outPath);
        if (proc->error() == QProcess::FailedToStart) {
            // 起进程失败（探测后再失效）：清并退化视频链
            m_gifLikeRunning.remove(url);
            m_gifLikeProcs.remove(url);
            proc->deleteLater();
            QFile::remove(inPath);
            QFile::remove(outPath);
        }
        it = m_gifLikeJobs.erase(it);
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

void MessageListWidget::refreshLayoutAndView()
{
    rebuildLayout(this);
    if (m_contentView) {
        m_contentView->setSize(QSizeF(m_contentView->width(),
            m_rowYOffsets.isEmpty() ? 0.0 : m_rowYOffsets.last()));
    }
    updateVisibleRows();
    update();
}

// ── 首条 URL 提取（qltox extractLinks 首元裁剪版，chatview.cpp:2939 口径：
// 尾标点/括号剔除；仅取首个，预览语义 = 一行一卡）──
static QString firstPreviewUrlFor(const QString& text)
{
    static const QRegularExpression re(
        QStringLiteral("(https?://[^\\s<>\"']+)"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch()) {
        return QString();
    }
    QString u = m.captured(1);
    while (!u.isEmpty()) {
        const QChar c = u.at(u.size() - 1);
        if (c == '.' || c == ',' || c == ';' || c == ':' || c == '!'
            || c == '?' || c == ')' || c == ']' || c == '}') {
            u.chop(1);
        } else {
            break;
        }
    }
    return u;
}

void MessageListWidget::scanAutoFeatures(int rowMin, int rowMax)
{
    if (m_rowHeights.size() != m_items.size()
        || m_rowYOffsets.size() != m_items.size() + 1) {
        return;
    }
    if (m_items.isEmpty() || rowMin > rowMax) {
        return;
    }
    // 触发区间 = 可见行（updateVisibleRows 同一 ±2 边带口径，qltox
    // paintEvent 只对绘出行触发的对位）；越界 clamp 收口
    const int lo = qBound(0, rowMin, m_items.size() - 1);
    const int hi = rowMax >= m_items.size() ? m_items.size() - 1 : rowMax;
    if (lo > hi) {
        return;
    }
    QPointer<MessageListWidget> guard(this);
    for (int i = lo; i <= hi; ++i) {
        const MsgMediaType mt = msgMediaTypeOf(m_items[i]);
        const bool isTextRow = (mt == MsgMediaType::Text
            || mt == MsgMediaType::Unknown);
        if (!isTextRow || m_items[i].content.isEmpty()) {
            continue;
        }
        // ── 自动翻译（决策 1=A：默认开，toLang=中文，仅他人文本消息）──
        if (TranslateHost::enabled() && !m_items[i].isSelf
            && m_items[i].transState == MessageItem::TransNone
            && TranslateHost::needsTranslation(m_items[i].content)) {
            m_items[i].transState = MessageItem::TransScheduled;
            MessageItem& item = m_items[i];
            // 定位键：DB 行优先（DB 加载行 localId=0）
            item.translateKey = item.dbRowid > 0 ? item.dbRowid : item.localId;
            MsgDbBridge::translationLookup(item.dbRowid, QStringLiteral("zh"),
                [guard, row = i](std::unique_ptr<TranslationRow> tr) {
                if (!guard || row < 0 || row >= guard->m_items.size()) {
                    return;
                }
                MessageItem& it = guard->m_items[row];
                if (it.transState != MessageItem::TransScheduled) {
                    return;
                }
                // 缓存命中 → 免请求直接 Done 显示（重启后重灌口）
                const QString cached =
                    tr ? QString::fromUtf8(tr->translated_text.c_str())
                       : QString();
                if (!cached.isEmpty()) {
                    it.translatedText = cached;
                    it.translateError = QString();
                    it.transState = MessageItem::TransDone;
                    it.showTranslation = true;
                    guard->refreshLayoutAndView();
                } else {
                    it.transState = MessageItem::TransInFlight;
                    TranslateHost::instance()->requestTranslate(
                        it.translateKey, it.content);
                }
            });
        }
        // ── 自动链接预览（文本行首条 URL）──
        if (LinkPreviewHost::enabled()
            && m_items[i].lpState == MessageItem::LpNone) {
            const QString url = firstPreviewUrlFor(m_items[i].content);
            if (!url.isEmpty()) {
                m_items[i].lpState = MessageItem::LpScheduled;
                m_items[i].lpUrl = linkPreviewNormalizeUrl(url);
                LinkPreviewHost::instance()->requestPreview(m_items[i].lpUrl);
            }
        }
    }
}

bool MessageListWidget::replaceRowContent(const QString& msgId,
                                          const QString& newContent)
{
    if (msgId.isEmpty()) {
        return false;
    }
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].msgId != msgId) {
            continue;
        }
        m_items[i].content = newContent;
        // 行高重算（内容变化不改行高结构；rebuildLayout 全量廉推 ≤80 行）
        rebuildLayout(this);
        if (MessageRowItem* rowItem = m_visibleRows.value(i)) {
            rowItem->setMessageDataNoUpdate(m_items[i]);
        }
        update();
        return true;
    }
    return false;
}

void MessageListWidget::setChannel(const QString& protocol, const QString& chatId,
                                   const QString& chanId)
{
    m_scrollIdleTimer.stop();
    m_coalesceTimer.stop();
    m_autoScanTimer.stop();
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

    // 自动翻译 / 链接预览触发扫描：尾随防抖（进入动画/连滚窗口外单次，
    // m_lastScanMin/Max 记最近可见区间）
    m_lastScanMin = rowMin;
    m_lastScanMax = rowMax;
    m_autoScanTimer.start();
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
