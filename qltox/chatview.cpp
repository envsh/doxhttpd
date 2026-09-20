#include "identicon.h"
#include "avatar_manager.h"
#include "chatview.h"
#include "chatbuffer.h"
#include "translate_util.h"
#include "config.h"
#include "photoviewer.h"
#include "LimeScrollBar.h"
#include "LimeStyle.h"
#ifdef EMOJI_RENDER_QT34
#include "emojiutil.h"
#endif
#include <algorithm>
#include <cstdlib>
#include <qimage.h>
#include <qtimer.h>
#include "translator.h"
#include "restapi.h"
#include "eventpoller.h"
#include "toastwidget.h"
#ifdef QT3_BUILD
#include <qpushbutton.h>
#include <qlayout.h>
#else
#include <QPushButton>
#include <QHBoxLayout>
#endif

// ── Media display sizing ──
static const int kMaxMediaDim = 260;
static const int kMinMediaH = 50;

static bool isSameSender(const ChatElement& a, const ChatElement& b) {
    return a.category == b.category
        && a.senderName == b.senderName;
}

QPixmap makeScaledThumb(const QPixmap& src, int mediaW, int mediaH, int maxContainW) {
    if (src.isNull()) return QPixmap();
    int dw, dh;
    if (mediaW > 0 && mediaH > 0) {
        double ratio = (double)mediaH / mediaW;
        dw = std::min(maxContainW, kMaxMediaDim);
        dh = (int)(dw * ratio);
        if (dh > kMaxMediaDim) { dh = kMaxMediaDim; dw = (int)(kMaxMediaDim / ratio); }
        if (dh < kMinMediaH)   { dh = kMinMediaH;   dw = std::min((int)(kMinMediaH / ratio), kMaxMediaDim); }
    } else {
        dw = std::min(maxContainW, kMaxMediaDim);
        dh = 200;
    }
#ifdef QT3_BUILD
    QImage img = src.convertToImage();
    QImage scaledImg = img.smoothScale(dw, dh, QImage::ScaleMin);
    QPixmap out;
    out.convertFromImage(scaledImg);
    return out;
#else
    return src.scaled(dw, dh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
#endif
}

#ifdef QT3_BUILD
#include <qpainter.h>
#include <qprocess.h>
#include <qregexp.h>
#include <qstringlist.h>
#include <qcursor.h>
#include <qclipboard.h>
#else
#include <QPainter>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QClipboard>
#include <QMenu>
#include <QApplication>
#include <QDialog>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QRegExp>
#include <QToolTip>
#endif

// ───── WebP fallback decode (dlopen libwebp) ─────
#include <dlfcn.h>
#include <cstdint>

bool isWebP(const std::string& d) {
    return d.size() >= 12 &&
           d[0]=='R'&&d[1]=='I'&&d[2]=='F'&&d[3]=='F' &&
           d[8]=='W'&&d[9]=='E'&&d[10]=='B'&&d[11]=='P';
}

QPixmap decodeWebP(const std::string& data) {
    if (!isWebP(data) || data.size() < 12) return QPixmap();
    void* lib = dlopen("libwebp.so", RTLD_LAZY);
    if (!lib) { qWarning("dlopen libwebp: %s", dlerror()); return QPixmap(); }
    using GetInfo  = int (*)(const uint8_t*, size_t, int*, int*);
    using DecodeRGB = uint8_t* (*)(const uint8_t*, size_t, int*, int*);
    auto gi   = (GetInfo)dlsym(lib, "WebPGetInfo");
    auto drgb = (DecodeRGB)dlsym(lib, "WebPDecodeRGB");
    if (!gi || !drgb) { dlclose(lib); return QPixmap(); }
    int w=0, imgH=0;
    if (!gi((const uint8_t*)data.data(), data.size(), &w, &imgH) || w<=0 || imgH<=0)
        { dlclose(lib); return QPixmap(); }
    uint8_t* rgb = drgb((const uint8_t*)data.data(), data.size(), &w, &imgH);
    if (!rgb) { dlclose(lib); return QPixmap(); }
    // Qt3/Qt4 no Format_RGB888; expand 24-bit RGB → 32-bit XRGB
#ifdef QT3_BUILD
    QImage img(w, imgH, 32);
#else
    QImage img(w, imgH, QImage::Format_RGB32);
#endif
    for (int y = 0; y < imgH; y++) {
        uint32_t* dst = (uint32_t*)img.scanLine(y);
        const uint8_t* src = rgb + y * w * 3;
        for (int x = 0; x < w; x++) {
            dst[x] = 0xFF000000 | (src[0] << 16) | (src[1] << 8) | src[2];
            src += 3;
        }
    }
    free(rgb); dlclose(lib);
#ifdef QT3_BUILD
    return QPixmap(img);
#else
    return QPixmap::fromImage(img);
#endif
}

#ifdef EMOJI_RENDER_QT34
// ───── Emoji icon helpers ─────
static uint32_t fileIconForName(const QString& name) {
    QString ext;
    int dot = -1;
    for (int i = 0; i < name.length(); i++) {
        if (name[i] == '.') { dot = i; }
    }
    if (dot >= 0) {
        for (int i = dot + 1; i < name.length(); i++) {
#ifdef QT3_BUILD
            ext += name[i].lower();
#else
            ext += name[i].toLower();
#endif
        }
    }
    if (ext == "jpg" || ext == "jpeg" || ext == "png" ||
        ext == "gif" || ext == "webp" || ext == "svg")
        return 0x1F5BC;   // 🖼
    if (ext == "mp3" || ext == "wav" || ext == "flac" ||
        ext == "ogg" || ext == "m4a" || ext == "wma")
        return 0x1F3B5;   // 🎵
    if (ext == "mp4" || ext == "mkv" || ext == "avi" ||
        ext == "mov" || ext == "webm")
        return 0x1F3AC;   // 🎬
    if (ext == "zip" || ext == "tar" || ext == "gz" ||
        ext == "bz2" || ext == "xz" || ext == "7z" || ext == "rar")
        return 0x1F4E6;   // 📦
    if (ext == "pdf")          return 0x1F4D5;   // 📕
    if (ext == "doc" || ext == "docx")  return 0x1F4DD;   // 📝
    if (ext == "xls" || ext == "xlsx")  return 0x1F4CA;   // 📊
    if (ext == "txt" || ext == "json" || ext == "xml" ||
        ext == "html" || ext == "js" || ext == "ts" ||
        ext == "css" || ext == "py" || ext == "go")
        return 0x1F4C4;   // 📄
    return 0x1F4C4;       // 📄 default
}

static void drawEmojiIcon(QPainter& p, const QRect& rect, uint32_t cp, const char* fallback) {
    QPixmap pm = EmojiRenderer::instance().renderEmoji(cp, rect.height() - 2);
    if (!pm.isNull()) {
        int cx = rect.x() + (rect.width() - pm.width()) / 2;
        int cy = rect.y() + (rect.height() - pm.height()) / 2;
        p.drawPixmap(cx, cy, pm);
    } else {
        p.drawText(rect, Qt::AlignCenter, qFromUtf8(fallback));
    }
}

// ───── Download button helper ─────
static QString formatFileSize(int bytes) {
    if (bytes <= 0) return QString();
    if (bytes < 1024) return QString::number(bytes) + qFromUtf8(" B");
    double kb = bytes / 1024.0;
    if (kb < 1024.0) {
        return QString::number(kb, 'f', 1) + qFromUtf8(" KB");
    }
    double mb = kb / 1024.0;
    return QString::number(mb, 'f', 1) + qFromUtf8(" MB");
}

static QString formatSpeed(int bps) {
    if (bps <= 0) { return QString(); }
    if (bps < 1024) { return QString::number(bps) + qFromUtf8(" B/s"); }
    double kb = bps / 1024.0;
    if (kb < 1024.0) { return QString::number(kb, 'f', 1) + qFromUtf8(" KB/s"); }
    double mb = kb / 1024.0;
    return QString::number(mb, 'f', 1) + qFromUtf8(" MB/s");
}

struct DownloadBarInfo {
    QRect downloadBtn;
    QRect retryBtn;
};

static DownloadBarInfo paintDownloadStatusBar(QPainter& p, const QRect& parentRect,
    const QFont& baseFont, const StyleParams::Palette& pal,
    ChatElement::DownloadState state, int fileSize,
    int progressPct = -1, int speedBps = 0)
{
    const int kPad = 8, btnH = 26, retryBtnH = 22;
    QFont bf = baseFont; bf.setPointSize(10); bf.setBold(true);
    QFontMetrics bfm(bf);

    // ── InProgress：顶部 4px 细线性进度条 + % 标签；底部按钮/状态仍照常绘制 ──
    if (state == ChatElement::InProgress) {
        int pct = progressPct;
        bool unknown = (pct < 0);
        if (pct < 0) { pct = 0; }
        if (pct > 100) { pct = 100; }

        // 标签宽度按 "100%" 全程预算，% 增长不会推动任何元素移位
        QString label = unknown ? qFromUtf8("…") : (QString::number(pct) + "%");
        int labelW = bfm.width("100%") + 6;
        QString speedText;
        int speedW = 0;
        if (speedBps > 0 && !unknown) {
            speedText = formatSpeed(speedBps);
            speedW = bfm.width(speedText);
        }
        int labelX = parentRect.right() - kPad - labelW
                   - (speedText.isEmpty() ? 0 : speedW + kPad);

        // 4px 条在预览区顶部，标签紧跟在条下方
        int barH = 4;
        int barTop = parentRect.top() + 2;
        int trackX = parentRect.x() + kPad;
        int trackRight = labelX - kPad;
        if (trackRight - trackX < 10) { trackRight = trackX + 10; }
        QRect track(trackX, barTop, trackRight - trackX, barH);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(70, 70, 70));          // 轨道底色
        p.drawRect(track);
        if (!unknown && pct >= 0) {
            QRect fillRect(track.x(), track.y(),
                           static_cast<int>((long long)track.width() * pct / 100), track.height());
            p.setBrush(pal.accent);
            p.drawRect(fillRect);
        }

        int labelY = barTop + barH + 2;   // 条下方 2px
        if (!unknown && pct >= 0) {
            p.setPen(pal.accent);
            p.setFont(bf);
            p.drawText(labelX, labelY, labelW, btnH,
                       Qt::AlignLeft | Qt::AlignVCenter, label);
        } else if (unknown) {
            p.setPen(pal.textMuted);
            p.setFont(bf);
            p.drawText(labelX, labelY, labelW, btnH,
                       Qt::AlignLeft | Qt::AlignVCenter, label);
        }
        if (!speedText.isEmpty()) {
            int sX = labelX - kPad - speedW;
            if (sX >= trackX + kPad) {   // 不覆盖进度条
                p.setPen(pal.textMuted);
                p.setFont(bf);
                p.drawText(sX, labelY, speedW, btnH,
                           Qt::AlignRight | Qt::AlignVCenter, speedText);
            }
        }
        p.setFont(baseFont);
    }

    // ── 其它状态：右→左文本/按钮排布 ──
    // ── Build text for each element ──
    QString statusText;
    QColor statusColor = pal.textMuted;
    if (state == ChatElement::NotRequested) {
        statusText = qFromUtf8("待下载");
    } else if (state == ChatElement::Completed) {
        statusText = qFromUtf8("✓ 已下载");
        statusColor = QColor(80, 180, 80);
    } else if (state == ChatElement::InProgress) {
        statusText = qFromUtf8("⏳ 下载中");
    } else if (state == ChatElement::Failed) {
        statusText = qFromUtf8("✗ 失败");
        statusColor = QColor(200, 50, 50);
    }

    QString dlText = qFromUtf8("⬇ 下载");
    QString sz;
    if (fileSize > 0) {
        sz = formatFileSize(fileSize);
    } else if (fileSize == UnkSize) {
        sz = qFromUtf8("0B");
    }
    if (!sz.isEmpty()) { dlText += qFromUtf8(" ") + sz; }

    QString errText;
    QString retryText;
    if (state == ChatElement::Failed) {
        errText = qFromUtf8("⚠ 下载失败");
        retryText = qFromUtf8("↻ 重试");
    }

    // ── Compute widths ──
    int statusW = statusText.isEmpty() ? 0 : bfm.width(statusText) + 8;
    int dlW     = bfm.width(dlText) + 20;
    int retryW  = retryText.isEmpty() ? 0 : bfm.width(retryText) + 16;
    int errW    = errText.isEmpty() ? 0 : bfm.width(errText);

    // ── Right-to-left layout ──
    int cursor = parentRect.right() - kPad;
    int barY   = parentRect.bottom() - btnH - 4;

    // 1. Status text (rightmost)
    if (!statusText.isEmpty()) {
        cursor -= statusW;
        QRect sr(cursor, barY, statusW, btnH);
        p.setPen(statusColor);
        p.setFont(baseFont);
        p.drawText(sr, Qt::AlignLeft | Qt::AlignVCenter, statusText);
        cursor -= kPad;
    }

    // 2. Download button
    cursor -= dlW;
    QRect dlBtnR(cursor, barY, dlW, btnH);
    p.setBrush(QColor(50, 50, 50));
    p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
    p.drawRoundRect(dlBtnR, 6, 6);
#else
    p.drawRoundedRect(dlBtnR, 6, 6);
#endif
    p.setPen(Qt::white);
    p.setFont(bf);
    p.drawText(dlBtnR, Qt::AlignCenter, dlText);
    cursor -= kPad;

    // 3. Retry button (only on failure)
    QRect retBtnR;
    if (!retryText.isEmpty()) {
        cursor -= retryW;
        int ry = barY + (btnH - retryBtnH) / 2;
        retBtnR = QRect(cursor, ry, retryW, retryBtnH);
        p.setBrush(QColor(50, 50, 50));
        p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
        p.drawRoundRect(retBtnR, 4, 3);
#else
        p.drawRoundedRect(retBtnR, 3, 3);
#endif
        p.setPen(Qt::white);
        QFont rf = baseFont; rf.setPointSize(9); p.setFont(rf);
        p.drawText(retBtnR, Qt::AlignCenter, retryText);
        cursor -= kPad;
    }

    // 4. Error text (only on failure)
    if (!errText.isEmpty()) {
        cursor -= errW;
        p.setPen(QColor(200, 50, 50));
        QFont ef = baseFont; ef.setPointSize(10); p.setFont(ef);
        p.drawText(cursor, barY, errW, btnH,
                   Qt::AlignLeft | Qt::AlignVCenter, errText);
    }

    p.setFont(baseFont);

    DownloadBarInfo info;
    info.downloadBtn = dlBtnR;
    info.retryBtn    = retBtnR;
    return info;
}

// ───── Media rendering helpers ─────

static int paintThumbnail(QPainter& p, const QRect& imgRect,
    const QPixmap& thumb, int mediaW, int mediaH,
    const QFont& baseFont, const QFontMetrics& fm,
    const StyleParams::Palette& pal,
    ChatElement::DownloadState state,
    QRect* downloadBtnOut = nullptr, QRect* retryBtnOut = nullptr,
    int fileSize = 0, int progressPct = -1, int speedBps = 0)
{
    int maxW = imgRect.width(), maxH = imgRect.height();
    int dw, dh;
    if (mediaW > 0 && mediaH > 0) {
        double ratio = (double)mediaH / mediaW;
        dw = std::min(maxW, kMaxMediaDim);
        dh = (int)(dw * ratio);
        if (dh > kMaxMediaDim) { dh = kMaxMediaDim; dw = (int)(kMaxMediaDim / ratio); }
        if (dh < kMinMediaH)   { dh = kMinMediaH;   dw = std::min((int)(kMinMediaH / ratio), kMaxMediaDim); }
    } else {
        dw = std::min(maxW, kMaxMediaDim);
        dh = 200;
    }
    if (!thumb.isNull()) {
        if (thumb.width() == dw && thumb.height() == dh) {
            int ox = imgRect.x() + (maxW - dw) / 2;
            int oy = imgRect.y();
            p.drawPixmap(ox, oy, thumb);
        } else {
#ifdef QT3_BUILD
        QImage img = thumb.convertToImage();
        QImage scaledImg = img.scale(dw, dh, QImage::ScaleMin);
        QPixmap scaled;
        scaled.convertFromImage(scaledImg);
#else
        QPixmap scaled = thumb.scaled(dw, dh, Qt::KeepAspectRatio, Qt::SmoothTransformation);
#endif
        int ox = imgRect.x() + (maxW - scaled.width()) / 2;
        int oy = imgRect.y();
        p.drawPixmap(ox, oy, scaled);
        }
    } else {
        p.setBrush(pal.hoverBg);
        p.setPen(Qt::NoPen);
        int px = imgRect.x() + (maxW - dw) / 2;
        int py = imgRect.y();
        QRect pr(px, py, dw, dh);
#ifdef QT3_BUILD
        p.drawRoundRect(pr, 4, 4);
#else
        p.drawRoundedRect(pr, 4, 4);
#endif
        p.setPen(pal.textMuted);
        QFont f = baseFont; f.setPointSize(11); p.setFont(f);
        QString dimText = (mediaW > 0 && mediaH > 0)
            ? QString("%1 × %2").arg(mediaW).arg(mediaH) : "?";
        p.drawText(pr, Qt::AlignCenter, dimText);
        p.setFont(baseFont);

    }
    // ── 状态栏（右下角右对齐）──
    DownloadBarInfo bi = paintDownloadStatusBar(p, imgRect, baseFont, pal,
                                                state, fileSize, progressPct, speedBps);
    if (downloadBtnOut) { *downloadBtnOut = bi.downloadBtn; }
    if (retryBtnOut)    { *retryBtnOut    = bi.retryBtn; }
    return dh;
}

// ───── 短动图（≤3s webm/mp4）───
// 写死默认自动：下载完成的短动画在消息列表内嵌循环预览；改 false 即关
static bool g_autoPreviewGifLikeVideo = true;

static bool isGifLikeVideo(const ChatElement& el) {
    if (el.etype != ChatElement::Video) { return false; }
    return el.gifLikeVideo || !el.gifPath.isEmpty();
}

static bool hasGifLiveMovie(const ChatElement& el) {
    return el.etype == ChatElement::Video && !el.gifPath.isEmpty();
}

static int wrappedLineCount(const QString& text, int maxW,
                            const QFontMetrics& fm, int emojiW);

static void paintMediaContent(QPainter& p, const QRect& bubbleRect,
    ChatElement::ElementType etype, bool gifLikeVideo,
    const QPixmap& fullImage, const QString& caption,
    int mediaWidth, int mediaHeight, int durationSec,
    QMovie* movie,
    const QFont& baseFont, const QFontMetrics& fm,
    int emojiW, const StyleParams::Palette& pal,
    ChatElement::DownloadState state,
    QRect* downloadBtnOut = nullptr, QRect* retryBtnOut = nullptr,
    int fileSize = 0, int progressPct = -1, int speedBps = 0)
{
    const int kBubbleHPad = 12, kBubbleVPad = 8, kPad = 8;

    // ── 顶部文字区（标题）──
    int capW = bubbleRect.width() - 2*kBubbleHPad;
    if (capW < 20) { capW = 20; }
    QFont cf = baseFont; cf.setPointSize(10);
    QFontMetrics cfm(cf);
    int nLines = caption.isEmpty() ? 0 : wrappedLineCount(caption, capW, cfm, emojiW);
    int captionH = nLines * cfm.lineSpacing();

    // ── 图片区：位于文字块下方，不与被文字占用 ──
    QRect imgRect(bubbleRect.x() + kBubbleHPad,
                  bubbleRect.y() + kBubbleVPad + captionH,
                  bubbleRect.width() - 2*kBubbleHPad,
                  bubbleRect.height() - 2*kBubbleVPad - captionH);
    if (imgRect.height() < 1) { imgRect.setHeight(1); }

    // Thumbnail / GIF frame
    QPixmap frame;
    if (etype == ChatElement::Gif || gifLikeVideo) {
#ifndef QT3_BUILD
        if (movie && movie->isValid())
            frame = movie->currentPixmap();
#endif
    }
    const QPixmap& src = !frame.isNull() ? frame : fullImage;
    paintThumbnail(p, imgRect, src, mediaWidth, mediaHeight, baseFont, fm, pal, state, downloadBtnOut, retryBtnOut, fileSize, progressPct, speedBps);

    // GIF badge
    if (etype == ChatElement::Gif || gifLikeVideo) {
        int bw = 30, bh = 16;
        QRect badgeR(imgRect.x() + 4, imgRect.y() + 4, bw, bh);
#ifdef QT3_BUILD
        p.setBrush(QColor(50, 50, 50));
#else
        p.setBrush(QColor(0,0,0,180));
#endif
        p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
        p.drawRoundRect(badgeR, 4, 3);
#else
        p.drawRoundedRect(badgeR, 3, 3);
#endif
        p.setPen(Qt::white);
        QFont bf = baseFont; bf.setPointSize(9); bf.setBold(true); p.setFont(bf);
        p.drawText(badgeR, Qt::AlignCenter,
                   etype == ChatElement::Gif ? qFromUtf8("GIF") : qFromUtf8("动图"));
        p.setFont(baseFont);
    }

    // Video overlay
    if (etype == ChatElement::Video) {
        int btnSize = 40;
        QRect playRect(imgRect.center().x() - btnSize/2,
                       imgRect.center().y() - btnSize/2,
                       btnSize, btnSize);
#ifdef QT3_BUILD
        p.setBrush(QColor(40, 40, 40));
#else
        p.setBrush(QColor(0,0,0,160));
#endif
        p.setPen(Qt::NoPen);
        p.drawEllipse(playRect);
        // Play triangle via lines + flood fill
        {
#ifdef QT3_BUILD
            QPointArray pts(3);
            pts.setPoint(0, playRect.left() + 14, playRect.top() + 10);
            pts.setPoint(1, playRect.left() + 14, playRect.bottom() - 10);
            pts.setPoint(2, playRect.right() - 10, playRect.center().y());
#else
            QPolygon pts(3);
            pts.setPoint(0, playRect.left() + 14, playRect.top() + 10);
            pts.setPoint(1, playRect.left() + 14, playRect.bottom() - 10);
            pts.setPoint(2, playRect.right() - 10, playRect.center().y());
#endif
            p.setBrush(Qt::white);
            p.setPen(Qt::NoPen);
            p.drawPolygon(pts);
        }

        if (durationSec > 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d:%02d", durationSec / 60, durationSec % 60);
            QString dur = qFromUtf8(buf);
            int bw = fm.width(dur) + 12, bh = fm.lineSpacing() + 4;
            QRect badgeR(imgRect.right() - bw - 4, imgRect.bottom() - bh - 4, bw, bh);
#ifdef QT3_BUILD
            p.setBrush(QColor(50, 50, 50));
#else
            p.setBrush(QColor(0,0,0,180));
#endif
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(badgeR, 4, 3);
#else
            p.drawRoundedRect(badgeR, 3, 3);
#endif
            p.setPen(Qt::white);
            QFont bf = baseFont; bf.setPointSize(10); p.setFont(bf);
            p.drawText(badgeR, Qt::AlignCenter, dur);
            p.setFont(baseFont);
        }
    }

    // Caption（消息顶部，0.9 不透明度，接近普通文本消息）
    if (nLines > 0) {
        QColor capCol;
#ifdef QT3_BUILD
        // Qt3 QColor 无 alpha，向气泡底色混合 10% 实现 0.9 不透明度观感
        {
            QColor fg = pal.textPrimary, bg = pal.baseBg;
            capCol = QColor((fg.red()*230 + bg.red()*25) / 255,
                            (fg.green()*230 + bg.green()*25) / 255,
                            (fg.blue()*230 + bg.blue()*25) / 255);
        }
#endif
#ifndef QT3_BUILD
        capCol = pal.textPrimary;
        capCol.setAlpha(230);
#endif
        p.setPen(capCol);
        p.setFont(cf);
        QRect capRect(bubbleRect.x() + kBubbleHPad, bubbleRect.y() + kBubbleVPad,
                      capW, captionH);
#ifdef QT3_BUILD
        p.drawText(capRect, Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop, caption);
#else
        p.drawText(capRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, caption);
#endif
        p.setFont(baseFont);
    }
}

static void paintAudioContent(QPainter& p, const QRect& bubbleRect,
    const QString& caption, int durationSec,
    const QFont& baseFont, const QFontMetrics& fm,
    const StyleParams::Palette& pal)
{
    const int kBubbleHPad = 12, kBubbleVPad = 8, kPad = 8;
    int iconSize = 48;
    int iconX = bubbleRect.x() + kBubbleHPad;
    int iconY = bubbleRect.y() + kBubbleVPad;

    p.setBrush(pal.surfaceBg);
    p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
    p.drawRoundRect(iconX, iconY, iconSize, iconSize, 8, 8);
#else
    p.drawRoundedRect(iconX, iconY, iconSize, iconSize, 8, 8);
#endif
    uint32_t cp = fileIconForName(caption);
    QPixmap pm = EmojiRenderer::instance().renderEmoji(cp, iconSize - 4);
    if (!pm.isNull()) {
        int cx = iconX + (iconSize - pm.width()) / 2;
        int cy = iconY + (iconSize - pm.height()) / 2;
        p.drawPixmap(cx, cy, pm);
    } else {
        QFont f = baseFont; f.setPointSize(24); p.setFont(f);
        p.setPen(pal.textMuted);
        p.drawText(iconX, iconY, iconSize, iconSize, Qt::AlignCenter, qFromUtf8("🎵"));
        p.setFont(baseFont);
    }

    int textX = iconX + iconSize + kPad;
    int textW = bubbleRect.right() - kBubbleHPad - textX;
    if (textW < 20) { textW = 20; }
    if (!caption.isEmpty()) {
        p.setPen(pal.textPrimary);
        QFont f = baseFont; f.setPointSize(12); p.setFont(f);
        p.drawText(textX, iconY, textW, fm.lineSpacing(),
                   Qt::AlignLeft | Qt::AlignVCenter, caption);
        p.setFont(baseFont);
    }
    if (durationSec > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%02d", durationSec / 60, durationSec % 60);
        QString dur = qFromUtf8(buf);
        p.setPen(pal.textMuted);
        QFont f = baseFont; f.setPointSize(10); p.setFont(f);
        p.drawText(textX, iconY + fm.lineSpacing() + kPad/2,
                   textW, fm.lineSpacing(),
                   Qt::AlignLeft | Qt::AlignVCenter, dur);
        p.setFont(baseFont);
    }
}

#endif

static QString formatAdaptiveMessageTime(const QString& timeStr);

// 连续同发送者消息：在气泡右下角绘制时间戳。
//           ┌──────────────────────┐
//           │ 连续消息不显示头像     │
//           │ 和姓名，但时间仍可见。  │
//           │                10:30 │
//           └──────────────────────┘
// 文本消息：时间覆盖最后一行文末（Telegram 旧版风格）
// 媒体消息：时间放在气泡右下空白处，不重叠主要内容。
static void drawGroupedTime(QPainter& p, const QFont& baseFont,
                            const QFontMetrics& fm, const QRect& bubbleRect,
                            const QString& time, const StyleParams::Palette& pal)
{
    QString at = formatAdaptiveMessageTime(time);
    QFont f = baseFont;
    f.setPointSize(10);
    p.setFont(f);
    p.setPen(pal.textMuted);
    int tw = fm.width(at);
    p.drawText(bubbleRect.right() - tw - ChatView::kBubbleHPad,
               bubbleRect.bottom() - ChatView::kBubbleVPad - fm.lineSpacing(),
               tw, fm.lineSpacing(),
               Qt::AlignRight | Qt::AlignVCenter, at);
    p.setFont(baseFont);
}


// 根据与当前时间差格式化时间字符串。
// "hh:mm" 格式保持原样，完整日期格式做相对格式化。
static QString formatAdaptiveMessageTime(const QString& timeStr) {
    if (timeStr.length() == 5 && timeStr[2] == ':') {
        return timeStr;
    }
#ifdef QT3_BUILD
    int y  = timeStr.mid(0, 4).toInt();
    int mo = timeStr.mid(5, 2).toInt();
    int d  = timeStr.mid(8, 2).toInt();
    int h  = timeStr.mid(11, 2).toInt();
    int mi = timeStr.mid(14, 2).toInt();
    int s  = timeStr.mid(17, 2).toInt();
    QDate dtDate(y, mo, d);
    QTime dtTime(h, mi, s);
    if (!dtDate.isValid() || !dtTime.isValid()) { return timeStr; }
    QDateTime dt(dtDate, dtTime);
    QDateTime now = QDateTime::currentDateTime();
    int secs = dt.secsTo(now);
#else
    QDateTime dt = QDateTime::fromString(timeStr, "yyyy-MM-dd hh:mm:ss");
    if (!dt.isValid()) { return timeStr; }
    QDateTime now = QDateTime::currentDateTime();
    qint64 secs = dt.secsTo(now);
#endif
    
    // h:m:s or m-d h:m:s or y-m-d h:m:s
    int sublen = timeStr.length();
    if (dt.date() == now.date()) { sublen = 8; }
    else if (dt.date().year() == now.date().year()) { sublen = 14; }
    else {}
    if (1) { return timeStr.right(sublen); }
    
    if (secs < 0) { secs = 0; }
    if (secs < 60) { return qFromUtf8("刚刚"); }
    if (secs < 3600) { return QString::number(secs / 60) + qFromUtf8(" 分钟前"); }
    QDate today = now.date();
    QDate msgDate = dt.date();
    if (msgDate == today) { return dt.toString("HH:mm"); }
    if (msgDate == today.addDays(-1)) { return qFromUtf8("昨天"); }
    if (msgDate.daysTo(today) < 7) {
        static const char* wd[] = { "周一", "周二", "周三", "周四", "周五", "周六", "周日" };
        return qFromUtf8(wd[msgDate.dayOfWeek() - 1]);
    }
    if (msgDate.year() == today.year()) { return dt.toString("M月d日"); }
    return dt.toString("yyyy年M月d日");
}

// 按显示宽度对文本软/硬换行计数（含 emoji 宽度），空串返回 0，非空至少 1 行。
static int wrappedLineCount(const QString& text, int maxW,
                            const QFontMetrics& fm, int emojiW) {
    if (text.isEmpty()) { return 0; }
    int lineCount = 0;
#ifdef EMOJI_RENDER_QT34
    auto cps = toCodepoints(text);
    int tLen = (int)cps.size();
    int pos = 0;
    while (pos < tLen) {
        if (cps[pos] == '\n') { lineCount++; pos++; continue; }
        int lineWidth = 0, lastSpace = -1, end = pos;
        while (end < tLen && cps[end] != '\n') {
            int cw = isEmojiChar(cps[end]) ? emojiW : fm.width(QChar(cps[end]));
            lineWidth += cw;
            if (cps[end] == ' ') { lastSpace = end; }
            if (lineWidth >= maxW) {
                if (lastSpace > pos && end - pos > 10) { end = lastSpace + 1; }
                break;
            }
            end++;
        }
        lineCount++;
        pos = end;
    }
#else
    int tLen = text.length();
    int pos = 0;
    while (pos < tLen) {
        if (text[pos] == '\n') { lineCount++; pos++; continue; }
        int lineWidth = 0, lastSpace = -1, end = pos;
        while (end < tLen && text[end] != '\n') {
            lineWidth += fm.width(text[end]);
            if (text[end].isSpace()) { lastSpace = end; }
            if (lineWidth >= maxW) {
                if (lastSpace > pos && end - pos > 10) { end = lastSpace + 1; }
                break;
            }
            end++;
        }
        lineCount++;
        pos = end;
    }
#endif
    if (lineCount < 1) { lineCount = 1; }
    return lineCount;
}

// ───── ChatElement methods ─────

int ChatElement::calcHeight(int viewWidth, const QFontMetrics& fm, int emojiW, const QFont& baseFont) {
    if (redacted) {
        ChatElement tmp = *this;
        tmp.etype = Text;
        tmp.redacted = false;                     // 防递归
        tmp.messageText = qFromUtf8("🗑 ") + _("context.msg_redacted") + qFromUtf8("：") + messageText;
        tmp.showTranslation = false;
        tmp.caption = QString();
        tmp.mediaUrl = QString();
        tmp.fileName = QString();
        return tmp.calcHeight(viewWidth, fm, emojiW, baseFont);
    }
    switch (etype) {
    case Text: {
        if (viewWidth <= 0) { viewWidth = 400; }

        int kAvatarSize = ChatView::kAvatarSize;
        int kPad = ChatView::kPad;
        int kBubbleHPad = ChatView::kBubbleHPad;
        int kBubbleVPad = ChatView::kBubbleVPad;
        int kMsgSpacing = ChatView::kMsgSpacing;

        int contentW = viewWidth - 3 * kPad - kAvatarSize;
        int bubbleW = (contentW * 80) / 100;
        if (bubbleW < 100) { bubbleW = contentW; }
        int bubbleTextWidth = bubbleW - 2 * kBubbleHPad;
        if (bubbleTextWidth < 20) { bubbleTextWidth = 20; }

        int lineCount = 0;
#ifdef EMOJI_RENDER_QT34
        auto cps = toCodepoints(messageText);
        int tLen = (int)cps.size();
        int pos = 0;
        while (pos < tLen) {
            if (cps[pos] == '\n') { lineCount++; pos++; continue; }
            int lineWidth = 0, lastSpace = -1, end = pos;
            while (end < tLen && cps[end] != '\n') {
                int cw = isEmojiChar(cps[end]) ? emojiW : fm.width(QChar(cps[end]));
                lineWidth += cw;
                if (cps[end] == ' ') { lastSpace = end; }
                if (lineWidth >= bubbleTextWidth) {
                    if (lastSpace > pos && end - pos > 10) {
                        end = lastSpace + 1;
                    }
                    break;
                }
                end++;
            }
            lineCount++;
            pos = end;
        }
#else
        int textLen = messageText.length();
        for (int i = 0; i < textLen; ) {
            if (messageText[i] == '\n') {
                lineCount++;
                i++;
                continue;
            }
            int lineWidth = 0;
            int lastSpace = -1;
            int end = i;
            while (end < textLen && messageText[end] != '\n') {
                lineWidth += fm.width(messageText[end]);
                if (messageText[end].isSpace()) lastSpace = end;
                if (lineWidth >= bubbleTextWidth) {
                    if (lastSpace > i && end - i > 10) {
                        end = lastSpace + 1;
                    }
                    break;
                }
                end++;
            }
            lineCount++;
            i = end;
        }
#endif
        if (lineCount < 1) { lineCount = 1; }

        int textHeight = lineCount * fm.lineSpacing();
        int minBubbleH = 2 * kBubbleVPad + fm.lineSpacing();
        int bubbleHeight = 2 * kBubbleVPad + std::max(textHeight, fm.lineSpacing());
        if (bubbleHeight < minBubbleH) { bubbleHeight = minBubbleH; }

        if (showTranslation && !translatedText.isEmpty()) {
            QFont sfmFont = baseFont;
            int pt = std::max(sfmFont.pointSize() - 2, 8);
            sfmFont.setPointSize(pt);
            QFontMetrics sfm(sfmFont);

            int transLineCount = 0;
            int tLen2 = translatedText.length();
            int tPos = 0;
            while (tPos < tLen2) {
                if (translatedText[tPos] == '\n') { transLineCount++; tPos++; continue; }
                int lineWidth = 0;
                int lastSpace = -1;
                int end = tPos;
                while (end < tLen2 && translatedText[end] != '\n') {
                    lineWidth += sfm.width(translatedText[end]);
                    if (translatedText[end].isSpace()) lastSpace = end;
                    if (lineWidth >= bubbleTextWidth) {
                        if (lastSpace > tPos && end - tPos > 10) {
                            end = lastSpace + 1;
                        }
                        break;
                    }
                    end++;
                }
                transLineCount++;
                tPos = end;
            }
            if (transLineCount < 1) { transLineCount = 1; }
            bubbleHeight += kBubbleVPad / 2 + transLineCount * sfm.lineSpacing() + kBubbleVPad + 7;
        }

        if (!firstInGroup) {
            return bubbleHeight + kPad / 2 + kMsgSpacing;
        }
        int headerHeight = fm.lineSpacing() + kPad;
        int contentHeight = kPad + headerHeight + bubbleHeight;
        int avatarTotal = kPad + kAvatarSize;
        return std::max(contentHeight, avatarTotal) + kMsgSpacing;
    }
    case Image:
    case Gif:
    case Video: {
        int kAvatarSize = ChatView::kAvatarSize;
        int kPad = ChatView::kPad;
        int kBubbleHPad = ChatView::kBubbleHPad;
        int kBubbleVPad = ChatView::kBubbleVPad;

        int contentW = viewWidth - 3 * kPad - kAvatarSize;
        int bubbleW = (contentW * 80) / 100;
        if (bubbleW < 100) { bubbleW = contentW; }
        int imgMaxW = bubbleW - 2 * kBubbleHPad;
        int imgDispW, imgDispH;
        if (mediaWidth > 0 && mediaHeight > 0) {
            double ratio = (double)mediaHeight / mediaWidth;
            imgDispW = std::min(imgMaxW, kMaxMediaDim);
            imgDispH = (int)(imgDispW * ratio);
            if (imgDispH > kMaxMediaDim) {
                imgDispH = kMaxMediaDim;
                imgDispW = (int)(kMaxMediaDim / ratio);
            }
            if (imgDispH < kMinMediaH) {
                imgDispH = kMinMediaH;
                imgDispW = std::min((int)(kMinMediaH / ratio), kMaxMediaDim);
            }
        } else {
            imgDispW = std::min(imgMaxW, kMaxMediaDim);
            imgDispH = 200;
        }
        QFont cfnt = baseFont; cfnt.setPointSize(10);
        QFontMetrics cfm(cfnt);
        int capW = bubbleW - 2 * kBubbleHPad;
        if (capW < 20) { capW = 20; }
        int captionH = caption.isEmpty() ? 0
            : wrappedLineCount(caption, capW, cfm, emojiW) * cfm.lineSpacing();
        int kDLBtnH = 30;
        int bubbleH = 2 * kBubbleVPad + imgDispH + captionH + kDLBtnH;
        if (!firstInGroup) {
            return bubbleH + kPad / 2 + 8;
        }
        int headerHeight = fm.lineSpacing() + kPad;
        int contentHeight = kPad + headerHeight + bubbleH;
        int avatarTotal = kPad + kAvatarSize;
        return std::max(contentHeight, avatarTotal) + 8;
    }
    case Audio: {
        int kAvatarSize = ChatView::kAvatarSize;
        int kPad = ChatView::kPad;
        int kBubbleHPad = ChatView::kBubbleHPad;
        int kBubbleVPad = ChatView::kBubbleVPad;

        int contentW = viewWidth - 3 * kPad - kAvatarSize;
        int bubbleW = (contentW * 80) / 100;
        if (bubbleW < 100) { bubbleW = contentW; }
        int iconSize = 48;
        int textW = bubbleW - 2 * kBubbleHPad - iconSize - kPad;
        if (textW < 20) { textW = 20; }
        int nameLines = 0;
        QString displayText = caption;
        {
            int tLen = displayText.length(), pos = 0;
            while (pos < tLen) {
                if (displayText[pos] == '\n') { nameLines++; pos++; continue; }
                int lineW = 0, lastSpace = -1, end = pos;
                while (end < tLen && displayText[end] != '\n') {
                    lineW += fm.width(displayText[end]);
                    if (displayText[end] == ' ') { lastSpace = end; }
                    if (lineW >= textW) {
                        if (lastSpace > pos && end - pos > 3) { end = lastSpace + 1; }
                        break;
                    }
                    end++;
                }
                nameLines++; pos = end;
            }
        }
        if (nameLines < 1) { nameLines = 1; }
        int textH = nameLines * fm.lineSpacing() + fm.lineSpacing() + kPad/2;
        int kDLBtnH = 30;
        int bubbleH = 2 * kBubbleVPad + std::max(textH + kPad, iconSize) + kDLBtnH;
        if (bubbleH < 2 * kBubbleVPad + iconSize + kDLBtnH) { bubbleH = 2 * kBubbleVPad + iconSize + kDLBtnH; }
        if (!firstInGroup) {
            return bubbleH + kPad / 2 + 8;
        }
        int headerHeight = fm.lineSpacing() + kPad;
        int contentHeight = kPad + headerHeight + bubbleH;
        int avatarTotal = kPad + kAvatarSize;
        return std::max(contentHeight, avatarTotal) + 8;
    }
    case File: {
        int kAvatarSize = ChatView::kAvatarSize;
        int kPad = ChatView::kPad;
        int kBubbleHPad = ChatView::kBubbleHPad;
        int kBubbleVPad = ChatView::kBubbleVPad;
        int kMsgSpacing = ChatView::kMsgSpacing;

        if (viewWidth <= 0) { viewWidth = 400; }

        int contentW = viewWidth - 3 * kPad - kAvatarSize;
        int bubbleW = (contentW * 80) / 100;
        if (bubbleW < 100) { bubbleW = contentW; }
        int innerW = bubbleW - 2 * kBubbleHPad;

        int iconSize = 48;
        int textW = innerW - iconSize - ChatView::kPad;
        if (textW < 20) { textW = 20; }

        int nameLines = 0;
        QString displayText = !caption.isEmpty() ? caption
                             : !fileName.isEmpty() ? fileName
                             : messageText;
        {
            int tLen = displayText.length();
            int pos = 0;
            while (pos < tLen) {
                if (displayText[pos] == '\n') { nameLines++; pos++; continue; }
                int lineWidth = 0, lastSpace = -1, end = pos;
                while (end < tLen && displayText[end] != '\n') {
                    lineWidth += fm.width(displayText[end]);
                    if (displayText[end] == ' ') { lastSpace = end; }
                    if (lineWidth >= textW) {
                        if (lastSpace > pos && end - pos > 3) {
                            end = lastSpace + 1;
                        }
                        break;
                    }
                    end++;
                }
                nameLines++;
                pos = end;
            }
        }
        if (nameLines < 1) { nameLines = 1; }

        int subLines = messageText.isEmpty() ? 0 : 1;
        int sizeLines = 1;
        int textHeight = nameLines * fm.lineSpacing()
                       + (subLines > 0 ? kPad/2 + subLines * fm.lineSpacing() : 0)
                       + (sizeLines > 0 ? kPad/2 + sizeLines * fm.lineSpacing() : 0);

        int kDLBtnH = 30;
        int bubbleHeight = 2 * kBubbleVPad
                         + std::max(textHeight + kPad, iconSize) + kDLBtnH;
        if (bubbleHeight < 2 * kBubbleVPad + iconSize + kDLBtnH) {
            bubbleHeight = 2 * kBubbleVPad + iconSize + kDLBtnH;
        }

        if (!firstInGroup) {
            return bubbleHeight + kPad / 2 + kMsgSpacing;
        }
        int headerHeight = fm.lineSpacing() + kPad;
        int contentHeight = kPad + headerHeight + bubbleHeight;
        int avatarTotal = kPad + kAvatarSize;
        return std::max(contentHeight, avatarTotal) + kMsgSpacing;
    }
    }
    return 0;
}

void ChatElement::paint(QPainter& p, int y, int viewWidth, bool isSelected,
                        const std::vector<QRect>& selRects,
                        const QFontMetrics& fm, int emojiW,
                        const QFont& baseFont, const StyleParams::Palette& pal) {
    if (redacted) {
        ChatElement tmp = *this;
        tmp.etype = Text;
        tmp.redacted = false;                     // 防递归
        tmp.messageText = qFromUtf8("🗑 ") + _("context.msg_redacted") + qFromUtf8("：") + messageText;
        tmp.showTranslation = false;
        tmp.caption = QString();
        tmp.mediaUrl = QString();
        tmp.fileName = QString();
        tmp.paint(p, y, viewWidth, isSelected, selRects, fm, emojiW, baseFont, pal);
        return;
    }
    switch (etype) {
    case Text: {
        QFont f = baseFont;
        int headerH = fm.lineSpacing();
        QRect textRect;
        QRect bubbleRect;

        const int hdrBtnSize = 18;
        const int hdrBtnGap = 4;
        int hdrBtnCnt = 2;
        int hdrBtnAreaW = hdrBtnCnt * hdrBtnSize + (hdrBtnCnt - 1) * hdrBtnGap;

        int kAvatarSize = ChatView::kAvatarSize;
        int kPad = ChatView::kPad;
        int kBubbleHPad = ChatView::kBubbleHPad;
        int kBubbleVPad = ChatView::kBubbleVPad;
        int kBubbleRadius = ChatView::kBubbleRadius;
        int kMsgSpacing = ChatView::kMsgSpacing;

        if (category == "self") {
            int contentRight = viewWidth - 2 * kPad - kAvatarSize;
            int contentLeft = kPad;
            int bubbleMaxW = contentRight - contentLeft;
            int bubbleW = std::min(bubbleMaxW, (bubbleMaxW * 80) / 100);

            if (firstInGroup) {
                int ax = viewWidth - kPad - kAvatarSize;
                QPixmap av = AvatarManager::inst().get(avatarUrl, senderName, peerNumber, kAvatarSize);
                p.drawPixmap(ax, y + kPad, av);

                int hdrTextRight = contentRight - hdrBtnAreaW - kPad / 2;

                f.setPointSize(11);
                p.setFont(f);
                p.setPen(pal.textMuted);
                QString displayName;
                if (!senderNickname.isEmpty())
                    displayName = senderNickname;
                else if (!senderName.isEmpty())
                    displayName = senderName;
                else if (peerNumber >= 0)
                    displayName = QString("Peer %1").arg(peerNumber);
                else
                    displayName = "?";
                displayName = qElideChars(displayName, 23, ElideMiddle);
                int nameW = fm.width(displayName) + fm.width("  ");
                p.drawText(hdrTextRight - nameW, y + kPad, nameW, headerH, Qt::AlignRight | Qt::AlignVCenter, displayName);

                int ipW = 0;
                if (!ipAddress.isEmpty()) {
                    f.setPointSize(10);
                    p.setFont(f);
                    p.setPen(QColor(130, 140, 150));
                    ipW = fm.width(ipAddress);
                    p.drawText(hdrTextRight - nameW - ipW - kPad/2, y + kPad,
                               ipW, headerH, Qt::AlignLeft | Qt::AlignVCenter, ipAddress);
                }

                f.setPointSize(10);
                p.setFont(f);
                p.setPen(pal.textMuted);
                QString at = formatAdaptiveMessageTime(time);
                p.drawText(hdrTextRight - nameW - ipW - kPad/2 - fm.width(at) - kPad/2, y + kPad,
                           fm.width(at), headerH, Qt::AlignRight | Qt::AlignVCenter, at);

                // send status
                if (category == "self") {
                    int statusX = hdrTextRight - nameW - ipW - kPad/2 - fm.width(at) - kPad/2 - 20;
                    resendIconRect = QRect(statusX, y + kPad + (headerH - hdrBtnSize) / 2,
                                            hdrBtnSize, hdrBtnSize);
                    if (sendState == SendSending) {
                        drawEmojiIcon(p, resendIconRect, 0x23F3, "⏳");
                    } else if (sendState == SendSent) {
                        drawEmojiIcon(p, resendIconRect, 0x2713, "✓");
                    } else if (sendState == SendFailed) {
                        drawEmojiIcon(p, resendIconRect, 0x2757, "❗");
                    }
                }

                p.setFont(baseFont);

                if (etype != ChatElement::File) {
                    int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
                    int btnX0 = contentRight - hdrBtnAreaW;
                    sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
                    translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
                    p.setPen(pal.textMuted);
                    p.setBrush(Qt::NoBrush);
                    p.drawEllipse(sourceBtnRect);
                    drawEmojiIcon(p, sourceBtnRect, 0x1F4CB, "📋");
                    p.drawEllipse(translateBtnRect);
                    drawEmojiIcon(p, translateBtnRect, 0x1F310, "🌐");
                }
            }

            int bubbleX = contentRight - bubbleW;
            int bubbleY = firstInGroup ? (y + kPad + headerH + kPad) : (y + kPad / 2);
            int bubbleH = firstInGroup ? (height - (kPad + headerH + kPad) - kMsgSpacing)
                                       : (height - kPad / 2 - kMsgSpacing);
            bubbleRect = QRect(bubbleX, bubbleY, bubbleW, bubbleH);
            textRect = QRect(bubbleRect.x() + kBubbleHPad, bubbleRect.y() + kBubbleVPad,
                             bubbleRect.width() - 2 * kBubbleHPad, bubbleRect.height() - 2 * kBubbleVPad);

            p.setBrush(pal.baseBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(bubbleRect, 4, 4);
#else
            p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif
            // send failed retry — use header ❗ instead
        } else {
            int contentX = 2 * kPad + kAvatarSize;
            int contentW = viewWidth - kPad - contentX;
            int bubbleW = (contentW * 80) / 100;
            if (bubbleW < 100) { bubbleW = contentW; }
            int bubbleRight = contentX + bubbleW;

            if (firstInGroup) {
                int ax = kPad;
                QPixmap av = AvatarManager::inst().get(avatarUrl, senderName, peerNumber, kAvatarSize);
                p.drawPixmap(ax, y + kPad, av);

                int hdrTextRight = bubbleRight - hdrBtnAreaW - kPad / 2;

                f.setPointSize(11);
                p.setFont(f);
                p.setPen(pal.textMuted);
                QString displayName;
                if (!senderNickname.isEmpty())
                    displayName = senderNickname;
                else if (!senderName.isEmpty())
                    displayName = senderName;
                else if (peerNumber >= 0)
                    displayName = QString("Peer %1").arg(peerNumber);
                else
                    displayName = "?";
                displayName = qElideChars(displayName, 23, ElideMiddle);
                int maxNameW = hdrTextRight - contentX;
                if (maxNameW < 20) { maxNameW = 20; }
                p.drawText(contentX, y + kPad, maxNameW, headerH, Qt::AlignLeft | Qt::AlignVCenter, displayName);

                f.setPointSize(10);
                p.setFont(f);
                int ipEnd = contentX + fm.width(displayName) + kPad;
                if (!ipAddress.isEmpty()) {
                    p.setPen(QColor(130, 140, 150));
                    int ipW = fm.width(ipAddress);
                    int ipMax = hdrTextRight - ipEnd;
                    if (ipMax > ipW) { ipMax = ipW; }
                    if (ipMax > 0) {
                        p.drawText(ipEnd, y + kPad, ipMax, headerH,
                                   Qt::AlignLeft | Qt::AlignVCenter, ipAddress);
                    }
                    ipEnd += fm.width(ipAddress) + kPad/2;
                }

                p.setPen(pal.textMuted);
                int timeX = ipEnd;
                int timeMaxW = hdrTextRight - timeX;
                if (timeMaxW > 0) {
                    p.drawText(timeX, y + kPad, timeMaxW, headerH, Qt::AlignLeft | Qt::AlignVCenter, formatAdaptiveMessageTime(time));
                }

                p.setFont(baseFont);

                if (etype != ChatElement::File) {
                    int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
                    int btnX0 = bubbleRight - hdrBtnAreaW;
                    sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
                    translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
                    p.setPen(pal.textMuted);
                    p.setBrush(Qt::NoBrush);
                    p.drawEllipse(sourceBtnRect);
                    drawEmojiIcon(p, sourceBtnRect, 0x1F4CB, "📋");
                    p.drawEllipse(translateBtnRect);
                    drawEmojiIcon(p, translateBtnRect, 0x1F310, "🌐");
                }
            }

            int bubbleY = firstInGroup ? (y + kPad + headerH + kPad) : (y + kPad / 2);
            int bubbleH = firstInGroup ? (height - (kPad + headerH + kPad) - kMsgSpacing)
                                       : (height - kPad / 2 - kMsgSpacing);
            if (bubbleH < 30) { bubbleH = 30; }
            bubbleRect = QRect(contentX, bubbleY, bubbleW, bubbleH);
            textRect = QRect(bubbleRect.x() + kBubbleHPad, bubbleRect.y() + kBubbleVPad,
                             bubbleRect.width() - 2 * kBubbleHPad, bubbleRect.height() - 2 * kBubbleVPad);

            p.setBrush(pal.baseBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(bubbleRect, 4, 4);
#else
            p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif
        }

        // Selection highlight
        if (isSelected && !selRects.empty()) {
            QColor selColor = lerpColor(pal.baseBg, pal.accent, 0.25f);
            p.setPen(Qt::NoPen);
            p.setBrush(selColor);
            for (size_t ri = 0; ri < selRects.size(); ++ri) {
                QRect r = selRects[ri];
#ifdef QT3_BUILD
                r.moveBy(0, y);
#else
                r.translate(0, y);
#endif
                p.drawRect(r);
            }
        }

        // Draw text
        p.setPen(pal.textPrimary);
        p.setFont(baseFont);
#ifdef EMOJI_RENDER_QT34
        EmojiRenderer::instance().drawText(p, textRect, messageText);
#else
#ifdef QT3_BUILD
        p.drawText(textRect, Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop, messageText);
#else
        p.drawText(textRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, messageText);
#endif
#endif

        // Translated text
        if (showTranslation && !translatedText.isEmpty()) {
            int origLineCount = 0;
            int textW = textRect.width();
            if (textW < 20) { textW = 20; }
#ifdef EMOJI_RENDER_QT34
            auto cps = toCodepoints(messageText);
            int tLen = (int)cps.size();
            int pos = 0;
            while (pos < tLen) {
                if (cps[pos] == '\n') { origLineCount++; pos++; continue; }
                int lineWidth = 0, lastSpace = -1, end = pos;
                while (end < tLen && cps[end] != '\n') {
                    int cw = isEmojiChar(cps[end]) ? emojiW : fm.width(QChar(cps[end]));
                    lineWidth += cw;
                    if (cps[end] == ' ') { lastSpace = end; }
                    if (lineWidth >= textW) {
                        if (lastSpace > pos && end - pos > 10) {
                            end = lastSpace + 1;
                        }
                        break;
                    }
                    end++;
                }
                origLineCount++;
                pos = end;
            }
#else
            int tLen3 = messageText.length();
            int pos = 0;
            while (pos < tLen3) {
                if (messageText[pos] == '\n') { origLineCount++; pos++; continue; }
                int lineWidth = 0, lastSpace = -1, end = pos;
                while (end < tLen3 && messageText[end] != '\n') {
                    lineWidth += fm.width(messageText[end]);
                    if (messageText[end].isSpace()) lastSpace = end;
                    if (lineWidth >= textW) {
                        if (lastSpace > pos && end - pos > 10) {
                            end = lastSpace + 1;
                        }
                        break;
                    }
                    end++;
                }
                origLineCount++;
                pos = end;
            }
#endif
            if (origLineCount < 1) { origLineCount = 1; }

            int origTextEndY = textRect.y() + origLineCount * fm.lineSpacing();
            int transY = origTextEndY + kBubbleVPad / 2;
            QRect transRect(textRect.x(), transY,
                            textRect.width(), bubbleRect.bottom() - kBubbleVPad - transY);
            if (transRect.height() > 0) {
                p.setPen(pal.textMuted);
                QFont smallFont = baseFont;
                int pt2 = std::max(smallFont.pointSize() - 2, 8);
                smallFont.setPointSize(pt2);
                p.setFont(smallFont);
                QString displayText = QString::fromUtf8("🌐 ") + translatedText;
#ifdef EMOJI_RENDER_QT34
                EmojiRenderer::instance().drawText(p, transRect, displayText);
#else
#  ifdef QT3_BUILD
                p.drawText(transRect, Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop, displayText);
#  else
                p.drawText(transRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, displayText);
#  endif
#endif
                p.setFont(baseFont);
            }
        }
        if (!firstInGroup) { drawGroupedTime(p, baseFont, fm, bubbleRect, time, pal); }
        break;
    }
    case Image:
    case Video:
    case Gif: {
        QFont f = baseFont;
        int headerH = fm.lineSpacing();
        QRect bubbleRect;

        const int hdrBtnSize = 18, hdrBtnGap = 4;
        int hdrBtnAreaW = 2 * hdrBtnSize + hdrBtnGap;

        int kAvatarSize = ChatView::kAvatarSize;
        int kPad = ChatView::kPad;
        int kBubbleHPad = ChatView::kBubbleHPad;
        int kBubbleVPad = ChatView::kBubbleVPad;
        int kBubbleRadius = ChatView::kBubbleRadius;
        int kMsgSpacing = ChatView::kMsgSpacing;

        if (category == "self") {
            int contentRight = viewWidth - 2 * kPad - kAvatarSize;
            int contentLeft = kPad;
            int bubbleMaxW = contentRight - contentLeft;
            int bubbleW = std::min(bubbleMaxW, (bubbleMaxW * 80) / 100);

            if (firstInGroup) {
                int ax = viewWidth - kPad - kAvatarSize;
                QPixmap av = AvatarManager::inst().get(avatarUrl, senderName, peerNumber, kAvatarSize);
                p.drawPixmap(ax, y + kPad, av);
                int hdrTextRight = contentRight - kPad / 2;

                f.setPointSize(11); p.setFont(f);
                p.setPen(pal.textMuted);
                QString dname = !senderNickname.isEmpty() ? senderNickname
                              : !senderName.isEmpty() ? senderName
                              : (peerNumber >= 0 ? QString("Peer %1").arg(peerNumber) : "?");
                dname = qElideChars(dname, 23, ElideMiddle);
                int nameW = fm.width(dname);
                p.drawText(hdrTextRight - nameW, y + kPad, nameW, headerH,
                           Qt::AlignRight | Qt::AlignVCenter, dname);
                int ipW = 0;
                if (!ipAddress.isEmpty()) {
                    f.setPointSize(10); p.setFont(f);
                    p.setPen(QColor(130, 140, 150));
                    ipW = fm.width(ipAddress);
                    p.drawText(hdrTextRight - nameW - ipW - kPad/2, y + kPad,
                               ipW, headerH, Qt::AlignLeft | Qt::AlignVCenter, ipAddress);
                }
                QString at = formatAdaptiveMessageTime(time);
                f.setPointSize(10); p.setFont(f);
                p.setPen(pal.textMuted);
                p.drawText(hdrTextRight - nameW - ipW - kPad/2 - fm.width(at) - kPad/2,
                           y + kPad, fm.width(at), headerH,
                           Qt::AlignRight | Qt::AlignVCenter, at);
                p.setFont(baseFont);

                if (etype != ChatElement::File) {
                    int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
                    int btnX0 = contentRight - hdrBtnAreaW;
                    sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
                    translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
                    p.setPen(pal.textMuted); p.setBrush(Qt::NoBrush);
                    p.drawEllipse(sourceBtnRect);
                    drawEmojiIcon(p, sourceBtnRect, 0x1F4CB, "📋");
                    p.drawEllipse(translateBtnRect);
                    drawEmojiIcon(p, translateBtnRect, 0x1F310, "🌐");
                }
            }

            int bubbleX = contentRight - bubbleW;
            int bubbleY = firstInGroup ? (y + kPad + headerH + kPad) : (y + kPad / 2);
            int bubbleH = firstInGroup ? (height - (kPad + headerH + kPad) - kMsgSpacing)
                                       : (height - kPad / 2 - kMsgSpacing);
            if (bubbleH < 30) { bubbleH = 30; }
            bubbleRect = QRect(bubbleX, bubbleY, bubbleW, bubbleH);
            p.setBrush(pal.baseBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(bubbleRect, 4, 4);
#else
            p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif
        } else {
            int contentX = 2 * kPad + kAvatarSize;
            int contentW = viewWidth - kPad - contentX;
            int bubbleW = (contentW * 80) / 100;
            if (bubbleW < 100) { bubbleW = contentW; }
            int bubbleRight = contentX + bubbleW;

            if (firstInGroup) {
                int ax = kPad;
                QPixmap av = AvatarManager::inst().get(avatarUrl, senderName, peerNumber, kAvatarSize);
                p.drawPixmap(ax, y + kPad, av);
                int hdrTextRight = bubbleRight - kPad / 2;

                f.setPointSize(11); p.setFont(f);
                p.setPen(pal.textMuted);
                QString dname = !senderNickname.isEmpty() ? senderNickname
                              : !senderName.isEmpty() ? senderName
                              : (peerNumber >= 0 ? QString("Peer %1").arg(peerNumber) : "?");
                dname = qElideChars(dname, 23, ElideMiddle);
                int maxNameW = hdrTextRight - contentX;
                if (maxNameW < 20) { maxNameW = 20; }
                p.drawText(contentX, y + kPad, maxNameW, headerH,
                           Qt::AlignLeft | Qt::AlignVCenter, dname);
                f.setPointSize(10); p.setFont(f);
                int ipEnd = contentX + fm.width(dname) + kPad;
                if (!ipAddress.isEmpty()) {
                    p.setPen(QColor(130, 140, 150));
                    int ipW = fm.width(ipAddress);
                    int ipMax = hdrTextRight - ipEnd;
                    if (ipMax > ipW) { ipMax = ipW; }
                    if (ipMax > 0) {
                        p.drawText(ipEnd, y + kPad, ipMax, headerH,
                                   Qt::AlignLeft | Qt::AlignVCenter, ipAddress);
                    }
                    ipEnd += fm.width(ipAddress) + kPad/2;
                }
                p.setPen(pal.textMuted);
                int timeMaxW = hdrTextRight - ipEnd;
                if (timeMaxW > 0) {
                    p.drawText(ipEnd, y + kPad, timeMaxW, headerH,
                               Qt::AlignLeft | Qt::AlignVCenter, formatAdaptiveMessageTime(time));
                }
                p.setFont(baseFont);

                if (etype != ChatElement::File) {
                    int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
                    int btnX0 = bubbleRight - hdrBtnAreaW;
                    sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
                    translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
                    p.setPen(pal.textMuted); p.setBrush(Qt::NoBrush);
                    p.drawEllipse(sourceBtnRect);
                    drawEmojiIcon(p, sourceBtnRect, 0x1F4CB, "📋");
                    p.drawEllipse(translateBtnRect);
                    drawEmojiIcon(p, translateBtnRect, 0x1F310, "🌐");
                }
            }

            int bubbleY = firstInGroup ? (y + kPad + headerH + kPad) : (y + kPad / 2);
            int bubbleH = firstInGroup ? (height - (kPad + headerH + kPad) - kMsgSpacing)
                                       : (height - kPad / 2 - kMsgSpacing);
            if (bubbleH < 30) { bubbleH = 30; }
            bubbleRect = QRect(contentX, bubbleY, bubbleW, bubbleH);
            p.setBrush(pal.baseBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(bubbleRect, 4, 4);
#else
            p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif
        }

        // Selection highlight
        if (isSelected && !selRects.empty()) {
            QColor selColor = lerpColor(pal.baseBg, pal.accent, 0.25f);
            p.setPen(Qt::NoPen);
            p.setBrush(selColor);
            for (size_t ri = 0; ri < selRects.size(); ++ri) {
                QRect r = selRects[ri];
#ifdef QT3_BUILD
                r.moveBy(0, y);
#else
                r.translate(0, y);
#endif
                p.drawRect(r);
            }
        }

        {
            int kBubbleHPad = 12, kBubbleVPad = 8;
            thumbnailRect = QRect(bubbleRect.x() + kBubbleHPad, bubbleRect.y() + kBubbleVPad,
                                  bubbleRect.width() - 2*kBubbleHPad, bubbleRect.height() - 2*kBubbleVPad);
        }
paintMediaContent(p, bubbleRect, etype, isGifLikeVideo(*this),
                          scaledDisplay, caption,
                          mediaWidth, mediaHeight, durationSec, movie,
                          baseFont, fm, emojiW, pal, downloadState,
                          &downloadBtnRect, &retryBtnRect, fileSize, downloadProgress,
                          downloadSpeedBps);
        if (!firstInGroup) { drawGroupedTime(p, baseFont, fm, bubbleRect, time, pal); }
        break;
    }
    case Audio: {
        QFont f = baseFont;
        int headerH = fm.lineSpacing();
        QRect bubbleRect;

        const int hdrBtnSize = 18, hdrBtnGap = 4;
        int hdrBtnAreaW = 2 * hdrBtnSize + hdrBtnGap;

        int kAvatarSize = ChatView::kAvatarSize;
        int kPad = ChatView::kPad;
        int kBubbleHPad = ChatView::kBubbleHPad;
        int kBubbleVPad = ChatView::kBubbleVPad;
        int kBubbleRadius = ChatView::kBubbleRadius;
        int kMsgSpacing = ChatView::kMsgSpacing;

        if (category == "self") {
            int contentRight = viewWidth - 2 * kPad - kAvatarSize;
            int contentLeft = kPad;
            int bubbleMaxW = contentRight - contentLeft;
            int bubbleW = std::min(bubbleMaxW, (bubbleMaxW * 80) / 100);

            if (firstInGroup) {
                int ax = viewWidth - kPad - kAvatarSize;
                QPixmap av = AvatarManager::inst().get(avatarUrl, senderName, peerNumber, kAvatarSize);
                p.drawPixmap(ax, y + kPad, av);
                int hdrTextRight = contentRight - kPad / 2;

                f.setPointSize(11); p.setFont(f);
                p.setPen(pal.textMuted);
                QString dname = !senderNickname.isEmpty() ? senderNickname
                              : !senderName.isEmpty() ? senderName
                              : (peerNumber >= 0 ? QString("Peer %1").arg(peerNumber) : "?");
                dname = qElideChars(dname, 23, ElideMiddle);
                int nameW = fm.width(dname);
                p.drawText(hdrTextRight - nameW, y + kPad, nameW, headerH,
                           Qt::AlignRight | Qt::AlignVCenter, dname);
                int ipW = 0;
                if (!ipAddress.isEmpty()) {
                    f.setPointSize(10); p.setFont(f);
                    p.setPen(QColor(130, 140, 150));
                    ipW = fm.width(ipAddress);
                    p.drawText(hdrTextRight - nameW - ipW - kPad/2, y + kPad,
                               ipW, headerH, Qt::AlignLeft | Qt::AlignVCenter, ipAddress);
                }
                QString at = formatAdaptiveMessageTime(time);
                f.setPointSize(10); p.setFont(f);
                p.setPen(pal.textMuted);
                p.drawText(hdrTextRight - nameW - ipW - kPad/2 - fm.width(at) - kPad/2,
                           y + kPad, fm.width(at), headerH,
                           Qt::AlignRight | Qt::AlignVCenter, at);
                p.setFont(baseFont);

                if (etype != ChatElement::File) {
                    int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
                    int btnX0 = contentRight - hdrBtnAreaW;
                    sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
                    translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
                    p.setPen(pal.textMuted); p.setBrush(Qt::NoBrush);
                    p.drawEllipse(sourceBtnRect);
                    drawEmojiIcon(p, sourceBtnRect, 0x1F4CB, "📋");
                    p.drawEllipse(translateBtnRect);
                    drawEmojiIcon(p, translateBtnRect, 0x1F310, "🌐");
                }
            }

            int bubbleX = contentRight - bubbleW;
            int bubbleY = firstInGroup ? (y + kPad + headerH + kPad) : (y + kPad / 2);
            int bubbleH = firstInGroup ? (height - (kPad + headerH + kPad) - kMsgSpacing)
                                       : (height - kPad / 2 - kMsgSpacing);
            if (bubbleH < 30) { bubbleH = 30; }
            bubbleRect = QRect(bubbleX, bubbleY, bubbleW, bubbleH);
            p.setBrush(pal.baseBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(bubbleRect, 4, 4);
#else
            p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif
        } else {
            int contentX = 2 * kPad + kAvatarSize;
            int contentW = viewWidth - kPad - contentX;
            int bubbleW = (contentW * 80) / 100;
            if (bubbleW < 100) { bubbleW = contentW; }
            int bubbleRight = contentX + bubbleW;

            if (firstInGroup) {
                int ax = kPad;
                QPixmap av = AvatarManager::inst().get(avatarUrl, senderName, peerNumber, kAvatarSize);
                p.drawPixmap(ax, y + kPad, av);
                int hdrTextRight = bubbleRight - kPad / 2;

                f.setPointSize(11); p.setFont(f);
                p.setPen(pal.textMuted);
                QString dname = !senderNickname.isEmpty() ? senderNickname
                              : !senderName.isEmpty() ? senderName
                              : (peerNumber >= 0 ? QString("Peer %1").arg(peerNumber) : "?");
                dname = qElideChars(dname, 23, ElideMiddle);
                int maxNameW = hdrTextRight - contentX;
                if (maxNameW < 20) { maxNameW = 20; }
                p.drawText(contentX, y + kPad, maxNameW, headerH,
                           Qt::AlignLeft | Qt::AlignVCenter, dname);
                f.setPointSize(10); p.setFont(f);
                int ipEnd = contentX + fm.width(dname) + kPad;
                if (!ipAddress.isEmpty()) {
                    p.setPen(QColor(130, 140, 150));
                    int ipW = fm.width(ipAddress);
                    int ipMax = hdrTextRight - ipEnd;
                    if (ipMax > ipW) { ipMax = ipW; }
                    if (ipMax > 0) {
                        p.drawText(ipEnd, y + kPad, ipMax, headerH,
                                   Qt::AlignLeft | Qt::AlignVCenter, ipAddress);
                    }
                    ipEnd += fm.width(ipAddress) + kPad/2;
                }
                p.setPen(pal.textMuted);
                int timeMaxW = hdrTextRight - ipEnd;
                if (timeMaxW > 0) {
                    p.drawText(ipEnd, y + kPad, timeMaxW, headerH,
                               Qt::AlignLeft | Qt::AlignVCenter, formatAdaptiveMessageTime(time));
                }
                p.setFont(baseFont);

                if (etype != ChatElement::File) {
                    int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
                    int btnX0 = bubbleRight - hdrBtnAreaW;
                    sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
                    translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
                    p.setPen(pal.textMuted); p.setBrush(Qt::NoBrush);
                    p.drawEllipse(sourceBtnRect);
                    drawEmojiIcon(p, sourceBtnRect, 0x1F4CB, "📋");
                    p.drawEllipse(translateBtnRect);
                    drawEmojiIcon(p, translateBtnRect, 0x1F310, "🌐");
                }
            }

            int bubbleY = firstInGroup ? (y + kPad + headerH + kPad) : (y + kPad / 2);
            int bubbleH = firstInGroup ? (height - (kPad + headerH + kPad) - kMsgSpacing)
                                       : (height - kPad / 2 - kMsgSpacing);
            if (bubbleH < 30) { bubbleH = 30; }
            bubbleRect = QRect(contentX, bubbleY, bubbleW, bubbleH);
            p.setBrush(pal.baseBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(bubbleRect, 4, 4);
#else
            p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif
        }

        if (isSelected && !selRects.empty()) {
            QColor selColor = lerpColor(pal.baseBg, pal.accent, 0.25f);
            p.setPen(Qt::NoPen);
            p.setBrush(selColor);
            for (size_t ri = 0; ri < selRects.size(); ++ri) {
                QRect r = selRects[ri];
#ifdef QT3_BUILD
                r.moveBy(0, y);
#else
                r.translate(0, y);
#endif
                p.drawRect(r);
            }
        }

        paintAudioContent(p, bubbleRect, caption, durationSec, baseFont, fm, pal);
        thumbnailRect = bubbleRect;   // 双击命中区（复用媒体字段）
        {
            DownloadBarInfo bi = paintDownloadStatusBar(p, bubbleRect, baseFont, pal,
                downloadState, fileSize, downloadProgress, downloadSpeedBps);
            downloadBtnRect = bi.downloadBtn;
            retryBtnRect    = bi.retryBtn;
        }
        if (!firstInGroup) { drawGroupedTime(p, baseFont, fm, bubbleRect, time, pal); }
        break;
    }
    case File: {
        QFont f = baseFont;
        int headerH = fm.lineSpacing();
        QRect bubbleRect;

        const int hdrBtnSize = 18;
        const int hdrBtnGap = 4;
        int hdrBtnAreaW = 2 * hdrBtnSize + hdrBtnGap;

        int kAvatarSize = ChatView::kAvatarSize;
        int kPad = ChatView::kPad;
        int kBubbleHPad = ChatView::kBubbleHPad;
        int kBubbleVPad = ChatView::kBubbleVPad;
        int kBubbleRadius = ChatView::kBubbleRadius;
        int kMsgSpacing = ChatView::kMsgSpacing;

        if (category == "self") {
            int contentRight = viewWidth - 2 * kPad - kAvatarSize;
            int contentLeft = kPad;
            int bubbleMaxW = contentRight - contentLeft;
            int bubbleW = std::min(bubbleMaxW, (bubbleMaxW * 80) / 100);

            if (firstInGroup) {
                int ax = viewWidth - kPad - kAvatarSize;
                QPixmap av = AvatarManager::inst().get(avatarUrl, senderName, peerNumber, kAvatarSize);
                p.drawPixmap(ax, y + kPad, av);
                int hdrTextRight = contentRight - kPad / 2;

                // sender name
                f.setPointSize(11);
                p.setFont(f);
                p.setPen(pal.textMuted);
                QString dname = !senderNickname.isEmpty() ? senderNickname
                              : !senderName.isEmpty() ? senderName
                              : (peerNumber >= 0 ? QString("Peer %1").arg(peerNumber) : "?");
                dname = qElideChars(dname, 23, ElideMiddle);
                int nameW = fm.width(dname);
                p.drawText(hdrTextRight - nameW, y + kPad, nameW, headerH,
                           Qt::AlignRight | Qt::AlignVCenter, dname);

                // IP
                int ipW = 0;
                if (!ipAddress.isEmpty()) {
                    f.setPointSize(10); p.setFont(f);
                    p.setPen(QColor(130, 140, 150));
                    ipW = fm.width(ipAddress);
                    p.drawText(hdrTextRight - nameW - ipW - kPad/2, y + kPad,
                               ipW, headerH, Qt::AlignLeft | Qt::AlignVCenter, ipAddress);
                }
                // time
                QString at = formatAdaptiveMessageTime(time);
                f.setPointSize(10); p.setFont(f);
                p.setPen(pal.textMuted);
                p.drawText(hdrTextRight - nameW - ipW - kPad/2 - fm.width(at) - kPad/2,
                           y + kPad, fm.width(at), headerH,
                           Qt::AlignRight | Qt::AlignVCenter, at);
                 // send status
                resendIconRect = QRect(hdrTextRight - nameW - ipW - kPad/2 - fm.width(at) - kPad/2 - 20,
                                       y + kPad + (headerH - hdrBtnSize) / 2,
                                       hdrBtnSize, hdrBtnSize);
                if (sendState == SendSending) {
                    drawEmojiIcon(p, resendIconRect, 0x23F3, "⏳");
                } else if (sendState == SendSent) {
                    drawEmojiIcon(p, resendIconRect, 0x2713, "✓");
                } else if (sendState == SendFailed) {
                    drawEmojiIcon(p, resendIconRect, 0x2757, "❗");
                }
                p.setFont(baseFont);

                // header buttons
                if (etype != ChatElement::File) {
                    int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
                    int btnX0 = contentRight - hdrBtnAreaW;
                    sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
                    translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
                    p.setPen(pal.textMuted); p.setBrush(Qt::NoBrush);
                    p.drawEllipse(sourceBtnRect);
                    drawEmojiIcon(p, sourceBtnRect, 0x1F4CB, "📋");
                    p.drawEllipse(translateBtnRect);
                    drawEmojiIcon(p, translateBtnRect, 0x1F310, "🌐");
                }
            }

            // bubble background
            int bubbleX = contentRight - bubbleW;
            int bubbleY = firstInGroup ? (y + kPad + headerH + kPad) : (y + kPad / 2);
            int bubbleH = firstInGroup ? (height - (kPad + headerH + kPad) - kMsgSpacing)
                                       : (height - kPad / 2 - kMsgSpacing);
            if (bubbleH < 30) { bubbleH = 30; }
            bubbleRect = QRect(bubbleX, bubbleY, bubbleW, bubbleH);
            p.setBrush(pal.baseBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(bubbleRect, 4, 4);
#else
            p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif

            // ── file content ──
            int iconSize = 48;
            int iconX = bubbleRect.x() + kBubbleHPad;
            int iconY = bubbleRect.y() + kBubbleVPad;
            p.setBrush(pal.surfaceBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(iconX, iconY, iconSize, iconSize, 8, 8);
#else
            p.drawRoundedRect(iconX, iconY, iconSize, iconSize, 8, 8);
#endif
            uint32_t fileCp = fileIconForName(!caption.isEmpty() ? caption : fileName);
            QPixmap pm = EmojiRenderer::instance().renderEmoji(fileCp, iconSize - 4);
            if (!pm.isNull()) {
                int cx = iconX + (iconSize - pm.width()) / 2;
                int cy = iconY + (iconSize - pm.height()) / 2;
                p.drawPixmap(cx, cy, pm);
            } else {
                f.setPointSize(24);
                p.setFont(f);
                p.setPen(pal.textMuted);
                p.drawText(iconX, iconY, iconSize, iconSize, Qt::AlignCenter, qFromUtf8("📄"));
                p.setFont(baseFont);
            }

            int textX = iconX + iconSize + kPad;
            int textW = bubbleRect.right() - kBubbleHPad - textX;
            if (textW < 20) { textW = 20; }
            QString displayName = !caption.isEmpty() ? caption
                                : !fileName.isEmpty() ? fileName
                                : messageText;
            if (!displayName.isEmpty()) {
                p.setPen(pal.textPrimary);
                f.setPointSize(12);
                p.setFont(f);
#ifdef QT3_BUILD
                // Qt3 手动 elide
                QString elide = displayName;
                if (fm.width(elide) > textW) {
                    for (int ei = elide.length(); ei > 0; ei--) {
                        if (fm.width(elide.left(ei) + "...") <= textW) {
                            elide = elide.left(ei) + "..."; break;
                        }
                    }
                    if (fm.width(elide) > textW) { elide = "..."; }
                }
                p.drawText(textX, iconY, textW, fm.lineSpacing(),
                           Qt::AlignLeft | Qt::AlignVCenter, elide);
#else
                p.drawText(textX, iconY, textW, fm.lineSpacing(),
                           Qt::AlignLeft | Qt::AlignVCenter,
                           fm.elidedText(displayName, Qt::ElideRight, textW));
#endif
                p.setFont(baseFont);
            }
            if (!messageText.isEmpty() && messageText != displayName) {
                p.setPen(pal.textMuted);
                f.setPointSize(10);
                p.setFont(f);
                p.drawText(textX, iconY + fm.lineSpacing() + kPad/2, textW, fm.lineSpacing(),
                           Qt::AlignLeft | Qt::AlignVCenter, messageText);
                p.setFont(baseFont);
            }
            {
                QString sz = formatFileSize(fileSize);
                if (sz.isEmpty()) { sz = qFromUtf8("0 B"); }
                int baseY = iconY + fm.lineSpacing() + kPad/2;
                if (!messageText.isEmpty() && messageText != displayName)
                    baseY += fm.lineSpacing() + kPad/2;
                p.setPen(pal.textMuted);
                f.setPointSize(10); p.setFont(f);
                p.drawText(textX, baseY, textW, fm.lineSpacing(),
                           Qt::AlignLeft | Qt::AlignVCenter, sz);
                p.setFont(baseFont);
            }
        } else {
            int contentX = 2 * kPad + kAvatarSize;
            int contentW = viewWidth - kPad - contentX;
            int bubbleW = (contentW * 80) / 100;
            if (bubbleW < 100) { bubbleW = contentW; }
            int bubbleRight = contentX + bubbleW;

            if (firstInGroup) {
                int ax = kPad;
                QPixmap av = AvatarManager::inst().get(avatarUrl, senderName, peerNumber, kAvatarSize);
                p.drawPixmap(ax, y + kPad, av);
                int hdrTextRight = bubbleRight - kPad / 2;

                // sender name
                f.setPointSize(11); p.setFont(f);
                p.setPen(pal.textMuted);
                QString dname = !senderNickname.isEmpty() ? senderNickname
                              : !senderName.isEmpty() ? senderName
                              : (peerNumber >= 0 ? QString("Peer %1").arg(peerNumber) : "?");
                dname = qElideChars(dname, 23, ElideMiddle);
                int maxNameW = hdrTextRight - contentX;
                if (maxNameW < 20) { maxNameW = 20; }
                p.drawText(contentX, y + kPad, maxNameW, headerH,
                           Qt::AlignLeft | Qt::AlignVCenter, dname);

                // IP
                f.setPointSize(10); p.setFont(f);
                int ipEnd = contentX + fm.width(dname) + kPad;
                if (!ipAddress.isEmpty()) {
                    p.setPen(QColor(130, 140, 150));
                    int ipW = fm.width(ipAddress);
                    int ipMax = hdrTextRight - ipEnd;
                    if (ipMax > ipW) { ipMax = ipW; }
                    if (ipMax > 0) {
                        p.drawText(ipEnd, y + kPad, ipMax, headerH,
                                   Qt::AlignLeft | Qt::AlignVCenter, ipAddress);
                    }
                    ipEnd += fm.width(ipAddress) + kPad/2;
                }
                // time
                p.setPen(pal.textMuted);
                int timeMaxW = hdrTextRight - ipEnd;
                if (timeMaxW > 0) {
                    p.drawText(ipEnd, y + kPad, timeMaxW, headerH,
                               Qt::AlignLeft | Qt::AlignVCenter, formatAdaptiveMessageTime(time));
                }
                p.setFont(baseFont);

                // header buttons
                if (etype != ChatElement::File) {
                    int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
                    int btnX0 = bubbleRight - hdrBtnAreaW;
                    sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
                    translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
                    p.setPen(pal.textMuted); p.setBrush(Qt::NoBrush);
                    p.drawEllipse(sourceBtnRect);
                    drawEmojiIcon(p, sourceBtnRect, 0x1F4CB, "📋");
                    p.drawEllipse(translateBtnRect);
                    drawEmojiIcon(p, translateBtnRect, 0x1F310, "🌐");
                }
            }

            // bubble background
            int bubbleY = firstInGroup ? (y + kPad + headerH + kPad) : (y + kPad / 2);
            int bubbleH = firstInGroup ? (height - (kPad + headerH + kPad) - kMsgSpacing)
                                       : (height - kPad / 2 - kMsgSpacing);
            if (bubbleH < 30) { bubbleH = 30; }
            bubbleRect = QRect(contentX, bubbleY, bubbleW, bubbleH);
            p.setBrush(pal.baseBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(bubbleRect, 4, 4);
#else
            p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif

            // ── file content ──
            int iconSize = 48;
            int iconX = bubbleRect.x() + kBubbleHPad;
            int iconY = bubbleRect.y() + kBubbleVPad;
            p.setBrush(pal.surfaceBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(iconX, iconY, iconSize, iconSize, 8, 8);
#else
            p.drawRoundedRect(iconX, iconY, iconSize, iconSize, 8, 8);
#endif
            uint32_t fileCp = fileIconForName(!caption.isEmpty() ? caption : fileName);
            QPixmap pm = EmojiRenderer::instance().renderEmoji(fileCp, iconSize - 4);
            if (!pm.isNull()) {
                int cx = iconX + (iconSize - pm.width()) / 2;
                int cy = iconY + (iconSize - pm.height()) / 2;
                p.drawPixmap(cx, cy, pm);
            } else {
                f.setPointSize(24);
                p.setFont(f);
                p.setPen(pal.textMuted);
                p.drawText(iconX, iconY, iconSize, iconSize, Qt::AlignCenter, qFromUtf8("📄"));
                p.setFont(baseFont);
            }

            int textX = iconX + iconSize + kPad;
            int textW = bubbleRect.right() - kBubbleHPad - textX;
            if (textW < 20) { textW = 20; }
            QString displayName = !caption.isEmpty() ? caption
                                : !fileName.isEmpty() ? fileName
                                : messageText;
            if (!displayName.isEmpty()) {
                p.setPen(pal.textPrimary);
                f.setPointSize(12);
                p.setFont(f);
#ifdef QT3_BUILD
                QString elide = displayName;
                if (fm.width(elide) > textW) {
                    for (int ei = elide.length(); ei > 0; ei--) {
                        if (fm.width(elide.left(ei) + "...") <= textW) {
                            elide = elide.left(ei) + "..."; break;
                        }
                    }
                    if (fm.width(elide) > textW) { elide = "..."; }
                }
                p.drawText(textX, iconY, textW, fm.lineSpacing(),
                           Qt::AlignLeft | Qt::AlignVCenter, elide);
#else
                p.drawText(textX, iconY, textW, fm.lineSpacing(),
                           Qt::AlignLeft | Qt::AlignVCenter,
                           fm.elidedText(displayName, Qt::ElideRight, textW));
#endif
                p.setFont(baseFont);
            }
            if (!messageText.isEmpty() && messageText != displayName) {
                p.setPen(pal.textMuted);
                f.setPointSize(10);
                p.setFont(f);
                p.drawText(textX, iconY + fm.lineSpacing() + kPad/2, textW, fm.lineSpacing(),
                           Qt::AlignLeft | Qt::AlignVCenter, messageText);
                p.setFont(baseFont);
            }
            {
                QString sz = formatFileSize(fileSize);
                if (sz.isEmpty()) { sz = qFromUtf8("0 B"); }
                int baseY = iconY + fm.lineSpacing() + kPad/2;
                if (!messageText.isEmpty() && messageText != displayName)
                    baseY += fm.lineSpacing() + kPad/2;
                p.setPen(pal.textMuted);
                f.setPointSize(10); p.setFont(f);
                p.drawText(textX, baseY, textW, fm.lineSpacing(),
                           Qt::AlignLeft | Qt::AlignVCenter, sz);
                p.setFont(baseFont);
            }
        }

        // ── selection highlight ──
        if (isSelected && !selRects.empty()) {
            QColor selColor = lerpColor(pal.baseBg, pal.accent, 0.25f);
            p.setPen(Qt::NoPen);
            p.setBrush(selColor);
            for (size_t ri = 0; ri < selRects.size(); ++ri) {
                QRect r = selRects[ri];
#ifdef QT3_BUILD
                r.moveBy(0, y);
#else
                r.translate(0, y);
#endif
                p.drawRect(r);
            }
        }
        // 下载按钮
        {
            DownloadBarInfo bi = paintDownloadStatusBar(p, bubbleRect, baseFont, pal,
                downloadState, fileSize, downloadProgress, downloadSpeedBps);
            downloadBtnRect = bi.downloadBtn;
            retryBtnRect    = bi.retryBtn;
        }
        if (!firstInGroup) { drawGroupedTime(p, baseFont, fm, bubbleRect, time, pal); }
        break;
    }
    }
}

void ChatElement::startGifLikeAnimation(QWidget* parent, int msgIndex) {
    if (movie) { return; }
    if (gifPath.isEmpty()) { return; }
    bool isGif = (etype == Gif) || isGifLikeVideo(*this);
    if (!isGif) { return; }
    if (etype == Video && !hasGifLiveMovie(*this)) { return; }
    if (etype == Video && !g_autoPreviewGifLikeVideo) { return; }
#ifndef QT3_BUILD
    movie = new QMovie(gifPath);
    {
        auto* slot = new LambdaSlot(movie, [parent, msgIndex]() {
            static_cast<ChatView*>(parent)->onGifFrameUpdated(msgIndex);
        });
        QObject::connect(movie, SIGNAL(updated(const QRect&)), slot, SLOT(call()));
    }
    movie->start();
#else
    Q_UNUSED(parent);
    Q_UNUSED(msgIndex);
    // Qt3 QMovie API differs; GIF animation deferred to Phase 4
#endif
}

void ChatElement::stopGifLikeAnimation() {
    if (!movie) { return; }
#ifndef QT3_BUILD
    movie->stop();
#endif
    delete movie;
    movie = nullptr;
}

// ───── ChatView ─────

// Block index helpers — binary-search accelerated Y-position lookup.
void ChatView::rebuildBlocks() {
    m_blocks.clear();
    int absY = kPad;
    int n = m_history->size();
    for (int i = 0; i < n; i += kBlockSize) {
        m_blocks.push_back({absY});
        int end = std::min(i + kBlockSize, n);
        for (int j = i; j < end; ++j) {
            absY += (*m_history)[j].height;
        }
    }
}

void ChatView::_appendToBlocks(int elementHeight) {
    if (m_blocks.empty()) {
        m_blocks.push_back({kPad});
        return;
    }
    int lastIdx = (int)m_blocks.size() - 1;
    int blkCnt = (int)m_history->size() - 1 - lastIdx * kBlockSize;
    if (blkCnt >= kBlockSize) {
        m_blocks.push_back({m_totalHeight + kPad});
    }
}

void ChatView::_prependToBlocks(int count) {
    if (count <= 0 || m_blocks.empty()) { rebuildBlocks(); return; }
    int absY = kPad;
    std::vector<MsgBlock> front;
    for (int i = 0; i < count; i += kBlockSize) {
        front.push_back({absY});
        int end = std::min(i + kBlockSize, count);
        for (int j = i; j < end; ++j) {
            absY += (*m_history)[j].height;
        }
    }
    int added = absY - kPad;
    for (auto& blk : m_blocks) {
        blk.cumulativeHeight += added;
    }
    m_blocks.insert(m_blocks.begin(), front.begin(), front.end());
}

void ChatView::_updateBlockFor(int idx, int oldHeight) {
    int newHeight = (*m_history)[idx].height;
    int diff = newHeight - oldHeight;
    if (diff == 0) { return; }
    int blk = blockForIndex(idx);
    if (blk < 0) { rebuildBlocks(); return; }
    for (size_t i = (size_t)(blk + 1); i < m_blocks.size(); ++i) {
        m_blocks[i].cumulativeHeight += diff;
    }
    m_totalHeight += diff;
}

void ChatView::_removeFromBlocks(int idx) {
    if (m_blocks.empty() || idx < 0) { return; }
    int n = (int)m_history->size();
    m_blocks.clear();
    m_totalHeight = 0;
    int absY = kPad;
    for (int i = 0; i < n; i += kBlockSize) {
        m_blocks.push_back({absY});
        int end = std::min(i + kBlockSize, n);
        for (int j = i; j < end; ++j) {
            absY += (*m_history)[j].height;
        }
    }
    m_totalHeight = absY - kPad;
}

int ChatView::blockForIndex(int msgIndex) const {
    if (m_blocks.empty() || msgIndex < 0) { return -1; }
    int lo = 0, hi = (int)m_blocks.size();
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (mid * kBlockSize <= msgIndex) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo - 1;
}

int ChatView::msgAbsY(int msgIndex) const {
    if (msgIndex < 0 || msgIndex >= (int)m_history->size()) { return -1; }
    int blockIdx = blockForIndex(msgIndex);
    if (blockIdx < 0) { return kPad; }
    int absY = m_blocks[blockIdx].cumulativeHeight;
    int blockStart = blockIdx * kBlockSize;
    int blockEnd = std::min(blockStart + kBlockSize, (int)m_history->size());
    for (int i = blockStart; i < msgIndex && i < blockEnd; ++i) {
        absY += (*m_history)[i].height;
    }
    return absY;
}

QRect ChatView::avatarRectFor(int msgIndex) const {
    if (msgIndex < 0 || msgIndex >= (int)m_history->size()) { return QRect(); }
    const ChatElement& el = (*m_history)[msgIndex];
    // 头像仅在组首消息绘制（与 paint 一致，self 靠右/其他靠左）
    if (!el.firstInGroup) { return QRect(); }
    int ax = (el.category == "self")
        ? (width() - kPad - kAvatarSize) : kPad;
    int ay = msgAbsY(msgIndex) - m_scrollPos + kPad;
    return QRect(ax, ay, kAvatarSize, kAvatarSize);
}

int ChatView::findByAbsY(int absY) const {
    if (m_blocks.empty()) { return -1; }
    if (absY < kPad) { return 0; }
    if (absY >= m_totalHeight) { return -1; }
    int lo = 0, hi = (int)m_blocks.size();
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (m_blocks[mid].cumulativeHeight <= absY) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    int blockIdx = lo - 1;
    int absPos = m_blocks[blockIdx].cumulativeHeight;
    int start = blockIdx * kBlockSize;
    int end = std::min(start + kBlockSize, (int)m_history->size());
    for (int i = start; i < end; ++i) {
        int h = (*m_history)[i].height;
        if (absY >= absPos && absY < absPos + h) { return i; }
        absPos += h;
    }
    return end - 1;
}

// 刷新策略：updateFull() 用于可视结构变化全量重绘，
// updateRect() 用于脏矩形可精确定位的局部刷新。
void ChatView::updateFull() {
    QWidget::update();
}

QRect ChatView::messageRect(int msgIndex) const {
    if (msgIndex < 0 || msgIndex >= (int)m_history->size()) { return QRect(); }
    int y = msgAbsY(msgIndex) - m_scrollPos;
    return QRect(0, y, contentWidth(), (*m_history)[msgIndex].height);
}

void ChatView::updateRect(const QRect& r) {
    if (r.isEmpty()) {
        qWarning("updateRect: empty rect (w=%d h=%d) — should use updateFull()",
                 r.width(), r.height());
        return;
    }
    QWidget::update(r);
}

ChatView::ChatView(QWidget* parent)
    : QWidget(parent
#ifdef QT3_BUILD
      , nullptr, WNoAutoErase
#endif
      )
    , m_history(&ChatHistory::kEmpty)
    , m_clickCount(0), m_clickMsgIndex(-1)
    , m_selMsgIndex(-1), m_selStart(0), m_selEnd(0), m_selecting(false)
    , m_fm(font()), m_emojiW(0), m_bmpW(NULL), m_scrollDelta(0)
{
    m_totalHeight = 0;
    m_scrollPos = 0;
    for (int i = 0; i < 128; i++) {
        m_ascW[i] = (uint8_t)std::min(m_fm.width(QChar(i)), 255);
    }
    m_emojiW = emojiCharWidth(m_fm);

    m_vScrollBar = new LimeScrollBar(Qt::Vertical, this);
#ifdef QT3_BUILD
    m_vScrollBar->setSteps(10, 50);
#else
    m_vScrollBar->setSingleStep(10);
    m_vScrollBar->setPageStep(50);
#endif
    connect(m_vScrollBar, SIGNAL(valueChanged(int)), this, SLOT(onScrollChanged(int)));

#ifdef QT3_BUILD
    setFocusPolicy(QWidget::StrongFocus);
#else
    setFocusPolicy(Qt::StrongFocus);
#endif
    setMouseTracking(true);
    m_scrollDownPill.setCallback([this]() {
        m_scrollDownPill.setCount(0);
        scrollToBottom();
    });
#ifndef QT3_BUILD
    setAttribute(Qt::WA_OpaquePaintEvent);
#endif

    m_animTimer = new QTimer(this);
    QObject::connect(m_animTimer, SIGNAL(timeout()), this, SLOT(onAnimTick()));
    m_animTimer->start(200);
}

ChatView::~ChatView() {
    m_animTimer->stop();
    resetCanvas();
    delete[] m_bmpW;
}



void ChatView::scrollBottomIfNeeded() {
    TimePoint _t0 = timeNow();
    if (!m_history || m_history->empty()) { return; }

    // Skip redundant work when observer already laid out the last element
    {
        int _w = contentWidth();
        if (_w <= 0) { _w = 400; }
        ChatElement& _el = (*m_history)[m_history->size() - 1];
        if (_el.cachedWidth == _w) { return; }
    }

    if (m_blocks.empty()) {
        m_blocks.push_back({kPad});
    } else {
        int lastBlockStart = ((int)m_blocks.size() - 1) * kBlockSize;
        int lastBlockCount = (int)m_history->size() - lastBlockStart;
        if (lastBlockCount >= kBlockSize) {
            m_blocks.push_back({m_totalHeight});
        }
    }

    int curVal = m_vScrollBar->value();
#ifdef QT3_BUILD
    int oldMax = m_vScrollBar->maxValue();
#else
    int oldMax = m_vScrollBar->maximum();
#endif
    bool atBottom = (oldMax - curVal) <= 20;

    int w = contentWidth();
    if (w <= 0) { w = 400; }
    m_gifFrameUpdated.push_back(0);
    int lastIdx = (int)m_history->size() - 1;
    ChatElement& el = (*m_history)[lastIdx];
    if (m_history->size() >= 2) {
        el.firstInGroup = !isSameSender((*m_history)[lastIdx - 1], el);
    }
    el.height = el.calcHeight(w, m_fm, m_emojiW, font());
    el.cachedWidth = (short)w;
    m_totalHeight += el.height;

    int vpH = height();
    int maxScroll = std::max(0, m_totalHeight - vpH);
    m_vScrollBar->setRange(0, maxScroll);
    m_vScrollBar->setPageStep(vpH);

    if (atBottom) {
        m_scrollPos = maxScroll;
        m_vScrollBar->blockSignals(true);
        m_vScrollBar->setValue(maxScroll);
        m_vScrollBar->blockSignals(false);
        m_vScrollBar->showTemporarily();
        updateFull();
    } else {
        m_scrollDownPill.setCount(m_scrollDownPill.count() + 1);
        updateRect(QRect(width() - 150, height() - 40, 150, 40));
    }
    long long _el = elapsedMs(_t0); if (_el >= 30) qWarning("SLOW [hangui] scrollBottomIfNeeded took %lldms", _el);
}

void ChatView::resetCanvas() {
    if (m_history) {
        for (auto& el : *m_history) { el.stopGifLikeAnimation(); }
    }
    m_gifFrameUpdated.clear();
    m_blocks.clear();
    m_totalHeight = 0;
    m_scrollPos = 0;
    m_vScrollBar->setRange(0, 0);
    m_selMsgIndex = -1;
    m_selStart = m_selEnd = 0;
    m_scrollDownPill.setCount(0);
    m_evictLow = 0;
    m_evictHigh = 0;
    updateFull();
}

void ChatView::setBuffer(ChatHistory* hist) {
    m_scrollUpdatePending = false;
    if (m_history && m_history != &ChatHistory::kEmpty) {
        m_history->setObserver(nullptr);
    }
    resetCanvas();
    m_history = hist ? hist : &ChatHistory::kEmpty;
    if (m_history && m_history != &ChatHistory::kEmpty) {
        m_history->setObserver(this);
        m_gifFrameUpdated.assign(m_history->size(), 0);
    }
    relayout();
    scrollToBottom();
    updateFull();
}

void ChatView::scrollToBottom() {
#ifdef QT3_BUILD
    int maxScroll = m_vScrollBar->maxValue();
#else
    int maxScroll = m_vScrollBar->maximum();
#endif
    m_vScrollBar->setValue(maxScroll);
    m_scrollPos = maxScroll;
}

void ChatView::scheduleScrollUpdate() {
    if (!m_scrollUpdatePending) {
        m_scrollUpdatePending = true;
        QTimer::singleShot(0, this, SLOT(flushScrollUpdate()));
    }
}

void ChatView::flushScrollUpdate() {
    m_scrollUpdatePending = false;
    if (!m_history) { return; }
    _updateScrollState();
}

void ChatView::_updateScrollState() {
    int vpH = height();
    int maxScroll = std::max(0, m_totalHeight - vpH);
    int curVal = m_vScrollBar->value();
    int oldMax;
#ifdef QT3_BUILD
    oldMax = m_vScrollBar->maxValue();
#else
    oldMax = m_vScrollBar->maximum();
#endif
    bool atBottom = (oldMax - curVal) <= 20;
    m_vScrollBar->setRange(0, maxScroll);
    m_vScrollBar->setPageStep(vpH);
    if (atBottom) {
        m_scrollPos = maxScroll;
        m_vScrollBar->blockSignals(true);
        m_vScrollBar->setValue(maxScroll);
        m_vScrollBar->blockSignals(false);
        m_vScrollBar->showTemporarily();
        updateFull();
    } else {
        m_scrollDownPill.setCount(m_scrollDownPill.count() + 1);
        updateRect(QRect(width() - 150, height() - 40, 150, 40));
    }
}

// ───── ChatHistoryObserver ─────
void ChatView::onInsertOne(size_t index) {
    if (!m_history || index >= m_history->size()) { return; }
    int w = contentWidth();
    if (w <= 0) { w = 400; }
    m_gifFrameUpdated.push_back(0);
    int i = (int)index;
    ChatElement& el = (*m_history)[i];
    if (i > 0) {
        el.firstInGroup = !isSameSender((*m_history)[i - 1], el);
    }
    el.height = el.calcHeight(w, m_fm, m_emojiW, font());
    el.cachedWidth = (short)w;
    m_totalHeight += el.height;
    _appendToBlocks(el.height);
    scheduleScrollUpdate();
}

void ChatView::onInsertRange(size_t start, size_t cnt) {
    if (!m_history || cnt == 0 || start != 0) { return; }
    int w = contentWidth();
    if (w <= 0) { w = 400; }
    int addedH = 0;
    for (size_t i = 0; i < cnt; ++i) {
        ChatElement& el = (*m_history)[i];
        el.height = el.calcHeight(w, m_fm, m_emojiW, font());
        el.cachedWidth = (short)w;
        if (i > 0) {
            el.firstInGroup = !isSameSender((*m_history)[i - 1], el);
        }
        addedH += el.height;
    }
    m_totalHeight += addedH;
    _prependToBlocks((int)cnt);
    m_evictLow += (int)cnt;
    m_evictHigh += (int)cnt;
    int vpH = height();
    int maxScroll = std::max(0, m_totalHeight - vpH);
    m_scrollPos += addedH;
    m_vScrollBar->setRange(0, maxScroll);
    m_vScrollBar->setPageStep(vpH);
    m_vScrollBar->setValue(m_scrollPos);
    m_vScrollBar->showTemporarily();
    updateFull();
}

void ChatView::onUpdateOne(size_t index) {
    if (!m_history || index >= m_history->size()) { return; }
    int i = (int)index;
    int oldH = (*m_history)[i].height;
    int w = contentWidth();
    if (w <= 0) { w = 400; }
    ChatElement& el = (*m_history)[i];
    el.cachedWidth = -1;
    el.height = el.calcHeight(w, m_fm, m_emojiW, font());
    el.cachedWidth = (short)w;
    int diff = el.height - oldH;
    if (diff != 0) {
        _updateBlockFor(i, oldH);
    }
    QRect r = messageRect(i);
    if (r.isValid()) { updateRect(r); }
}

void ChatView::onUpdateRange(size_t start, size_t cnt) {
    if (!m_history) { return; }
    int w = contentWidth();
    for (size_t i = start; i < start + cnt && i < m_history->size(); ++i) {
        ChatElement& el = (*m_history)[i];
        int oldH = el.height;
        el.cachedWidth = -1;
        el.height = el.calcHeight(w, m_fm, m_emojiW, font());
        el.cachedWidth = (short)w;
        if (el.height != oldH) {
            _updateBlockFor((int)i, oldH);
        }
    }
    scheduleScrollUpdate();
    QWidget::update();
}

void ChatView::onRemoveOne(size_t idx) {
    if (!m_history) { return; }
    _removeFromBlocks((int)idx);
    int vpH = height();
    int maxScroll = std::max(0, m_totalHeight - vpH);
    m_vScrollBar->setRange(0, maxScroll);
    m_vScrollBar->setPageStep(vpH);
    if (m_scrollPos > maxScroll) {
        m_scrollPos = maxScroll;
        m_vScrollBar->setValue(maxScroll);
    }
    updateFull();
}

void ChatView::onRemoveRange(size_t start, size_t cnt) {
    if (!m_history || cnt == 0) { return; }
    _removeFromBlocks((int)start);
    if (start == 0) {
        m_evictLow = std::max(0, m_evictLow - (int)cnt);
        m_evictHigh = std::max(0, m_evictHigh - (int)cnt);
    }
    int vpH = height();
    int maxScroll = std::max(0, m_totalHeight - vpH);
    m_vScrollBar->setRange(0, maxScroll);
    m_vScrollBar->setPageStep(vpH);
    if (m_scrollPos > maxScroll) {
        m_scrollPos = maxScroll;
        m_vScrollBar->setValue(maxScroll);
    }
    updateFull();
}

ChatElement& ChatView::messageAt(int index) {
    return (*m_history)[index];
}

int ChatView::messageCount() const {
    return m_history ? (int)m_history->size() : 0;
}

void ChatView::setFavRowids(const std::vector<int64_t>& favRowids) {
    m_favRowids.clear();
    for (int64_t r : favRowids) { if (r > 0) m_favRowids.insert(r, 1); }
}

bool ChatView::isFavRowid(int64_t rowid) const {
    return m_favRowids.contains(rowid);
}

void ChatView::setFavRowid(int64_t rowid, bool fav) {
    if (fav) m_favRowids.insert(rowid, 1); else m_favRowids.remove(rowid);
}

void ChatView::updateElement(int msgIndex) {
    if (!m_history || msgIndex < 0 || msgIndex >= (int)m_history->size()) { return; }
    ChatElement& el = (*m_history)[msgIndex];
    int oldH = el.height;
    int w = contentWidth();
    if (w <= 0) { w = 400; }
    if (el.cachedWidth == w) {
        QWidget::update();
        return;
    }
    el.cachedWidth = -1;
    el.height = el.calcHeight(w, m_fm, m_emojiW, font());
    el.cachedWidth = (short)w;
    if (el.height != oldH) {
        _updateBlockFor(msgIndex, oldH);
    }
    int vpH = height();
    int maxScroll = std::max(0, m_totalHeight - vpH);
    m_vScrollBar->setRange(0, maxScroll);
    m_vScrollBar->setPageStep(vpH);
    QWidget::update();
}

int ChatView::contentWidth() const {
    int sbw = m_vScrollBar->sizeHint().width();
    return width() - sbw;
}

// 进度类轻量刷新：只标脏该元素所在行，不做高度重算/布局更新
void ChatView::repaintMessageElement(int msgIndex) {
    if (!m_history || msgIndex < 0 || msgIndex >= (int)m_history->size()) { return; }
    QRect r = messageRect(msgIndex);
    if (r.isEmpty()) { return; }
    QWidget::update(r);
}

void ChatView::relayout() {
    int w = contentWidth();
    if (w <= 0) { w = 400; }
    if (!m_history) { return; }

    m_totalHeight = kPad;
    for (size_t i = 0; i < m_history->size(); ++i) {
        ChatElement& el = (*m_history)[i];
        if (el.cachedWidth != w) {
            el.height = el.calcHeight(w, m_fm, m_emojiW, font());
            el.cachedWidth = (short)w;
        }
        m_totalHeight += el.height;
    }

    int vpH = height();
    int maxScroll = std::max(0, m_totalHeight - vpH);
#ifdef QT3_BUILD
    int oldMax = m_vScrollBar->maxValue();
    int oldVal = m_vScrollBar->value();
#else
    int oldMax = m_vScrollBar->maximum();
    int oldVal = m_vScrollBar->value();
#endif
    rebuildBlocks();
    // block signals to prevent double scroll from setRange/setValue
    // https://doc.qt.io/qt-6/qabstractslider.html#valueChanged
    m_vScrollBar->blockSignals(true);
    m_vScrollBar->setRange(0, maxScroll);
    m_vScrollBar->setPageStep(vpH);
    if (oldVal == oldMax && oldMax > 0) {
        m_vScrollBar->setValue(maxScroll);
    } else if (oldMax == 0 && maxScroll > 0) {
        // 首次加载：历史为空→有内容，自动滚底
        m_vScrollBar->setValue(maxScroll);
    }
    m_vScrollBar->blockSignals(false);
    m_scrollPos = m_vScrollBar->value();
    updateFull();
}

int ChatView::charWidth(uint32_t cp) {
    if (cp < 128) return m_ascW[cp];
    if (cp < 0x10000) {
        if (!m_bmpW) m_bmpW = new uint8_t[65536]();
        if (!m_bmpW[cp]) {
            m_bmpW[cp] = (uint8_t)std::min(m_fm.width(QChar((ushort)cp)), 255);
        }
        return m_bmpW[cp];
    }
    return m_fm.width(QChar((ushort)cp));
}

// Extract URLs from text
std::vector<LinkSpan> ChatView::extractLinks(const QString& text) {
    std::vector<LinkSpan> spans;
#ifdef QT3_BUILD
    QRegExp urlRe("https?://[^\\s<>\"']+", false);
#else
    QRegExp urlRe("https?://[^\\s<>\"']+", Qt::CaseInsensitive);
#endif
    int pos = 0;
    while (true) {
#ifdef QT3_BUILD
        pos = urlRe.search(text, pos);
#else
        pos = urlRe.indexIn(text, pos);
#endif
        if (pos == -1) { break; }
        int len = urlRe.matchedLength();
        QString url = text.mid(pos, len);
        while (len > 0 && (url.right(1) == "." || url.right(1) == "," ||
               url.right(1) == ";" || url.right(1) == ":" ||
               url.right(1) == "!" || url.right(1) == "?" ||
               url.right(1) == ")" || url.right(1) == "]" ||
               url.right(1) == "}" || url.right(1) == ">")) {
            len--;
            url = url.left(len);
        }
        LinkSpan span;
        span.start = pos;
        span.end = pos + len;
        span.url = url;
        spans.push_back(span);
        pos += len;
    }
    return spans;
}

// Find word boundaries around a character position
static void wordBoundaries(const QString& text, int pos, int& start, int& end) {
    int len = text.length();
    if (len == 0) { start = end = 0; return; }
    start = pos;
    end = pos;
    if (pos < 0) { pos = 0; }
    if (pos >= len) { pos = len - 1; }
    // If at whitespace, move to nearest non-space
    while (start >= 0 && text[start].isSpace()) start--;
    while (end < len && text[end].isSpace()) end++;
    if (start < 0) { start = end = 0; return; }
    if (end >= len) { end = len; }
    // Expand left
    while (start > 0 && !text[start - 1].isSpace()) start--;
    // Expand right
    while (end < len && !text[end].isSpace()) end++;
}

// Find line boundaries around a character position
static void lineBoundaries(const QString& text, int pos, int& start, int& end) {
    int len = text.length();
    if (len == 0) { start = end = 0; return; }
    start = pos;
    end = pos;
    if (pos < 0) { pos = 0; }
    if (pos >= len) { pos = len - 1; }
    // Expand left to line start
    while (start > 0 && text[start - 1] != '\n') { start--; }
    // Expand right to line end
    while (end < len && text[end] != '\n') { end++; }
}

// Find which message is at given y coordinate (relative to widget top)
int ChatView::findMessageAtY(int y) const {
    return findByAbsY(m_scrollPos + y);
}

// Get character position in a message from local coordinates
int ChatView::charPosAt(int msgIndex, int localX, int localY) {
    if (msgIndex < 0 || msgIndex >= (int)m_history->size()) return -1;
    const ChatElement& msg = (*m_history)[msgIndex];
    // 带图片预览：文本在气泡顶部的 caption（10pt），几何与 paintMediaContent 一致
    bool mediaTop = (msg.etype == ChatElement::Image ||
                     msg.etype == ChatElement::Gif ||
                     msg.etype == ChatElement::Video) && !msg.caption.isEmpty();
    const QString& text = mediaTop ? msg.caption : msg.messageText;
    int textLen = text.length();
    if (textLen == 0) { return 0; }

    QFont mf = font();
    QFontMetrics fm(mf);
    int headerH = fontMetrics().lineSpacing();
    int viewW = contentWidth();
    if (mediaTop) {
        mf.setPointSize(10);
        fm = QFontMetrics(mf);
    }

    // Compute bubble text width (same as calcMessageHeight)
    int contentW = viewW - 3 * kPad - kAvatarSize;
    int bubbleW = (contentW * 80) / 100;
    if (bubbleW < 100) { bubbleW = contentW; }
    int bubbleTextWidth = bubbleW - 2 * kBubbleHPad;
    if (bubbleTextWidth < 20) { bubbleTextWidth = 20; }

    // Compute text area position
    int areaX, areaY;
    if (msg.category == "self") {
        int contentRight = viewW - 2 * kPad - kAvatarSize;
        int contentLeft = kPad;
        int bubbleMaxW = contentRight - contentLeft;
        int bubbleW2 = std::min(bubbleMaxW, (bubbleMaxW * 80) / 100);
        int bubbleX = contentRight - bubbleW2;
        areaX = bubbleX + kBubbleHPad;
    } else {
        int contentX = 2 * kPad + kAvatarSize;
        int contentW2 = viewW - kPad - contentX;
        int bubbleW3 = (contentW2 * 80) / 100;
        if (bubbleW3 < 100) { bubbleW3 = contentW2; }
        areaX = contentX + kBubbleHPad;
    }
    areaY = (msg.firstInGroup ? kPad + headerH + kPad : kPad / 2) + kBubbleVPad;

    localX -= areaX;
    localY -= areaY;
    if (localX < 0) { localX = 0; }
    if (localX >= bubbleTextWidth) { return -1; }
    if (localY < 0) { return -1; }

    // Compute line breaks
    std::vector<int> lineStarts;
    lineStarts.push_back(0);
    int pos = 0;
    while (pos < textLen) {
        if (text[pos] == '\n') {
            pos++;
            lineStarts.push_back(pos);
            continue;
        }
        int lineWidth = 0, lastSpace = -1, end = pos;
        while (end < textLen && text[end] != '\n') {
            lineWidth += fm.width(text[end]);
            if (text[end].isSpace()) lastSpace = end;
            if (lineWidth >= bubbleTextWidth) {
                if (lastSpace > pos && end - pos > 10) {
                    end = lastSpace + 1;
                }
                break;
            }
            end++;
        }
        pos = end;
        if (pos < textLen) { lineStarts.push_back(pos); }
    }

    int lineHeight = fm.lineSpacing();
    if (localY >= (int)lineStarts.size() * lineHeight) { return -1; }
    int lineIndex = localY / lineHeight;
    if (lineIndex >= (int)lineStarts.size())
        lineIndex = (int)lineStarts.size() - 1;
    if (lineIndex < 0) { lineIndex = 0; }

    int lineStart = lineStarts[lineIndex];
    int lineEnd = (lineIndex + 1 < (int)lineStarts.size()) ?
                  lineStarts[lineIndex + 1] : textLen;

    // Find character at x position
    int xOffset = 0;
    for (int i = lineStart; i < lineEnd; i++) {
        int chW = fm.width(text[i]);
        if (localX <= xOffset + chW / 2) {
            return i;
        }
        xOffset += chW;
    }
    return lineEnd - 1;
}

// Get rectangles for selection in a message
std::vector<QRect> ChatView::selectionRects(int msgIndex) {
    std::vector<QRect> rects;
    if (msgIndex != m_selMsgIndex) { return rects; }
    int start = std::min(m_selStart, m_selEnd);
    int end = std::max(m_selStart, m_selEnd);
    if (start == end) { return rects; }

    const ChatElement& msg = (*m_history)[msgIndex];
    int textLen = msg.messageText.length();
    if (start >= textLen || end <= 0) { return rects; }

    // Compute text rectangle
    QFont f = font();
    QFontMetrics fm(f);
    int viewW = contentWidth();
    int headerH = fm.lineSpacing();

    QRect textRect;
    if (msg.category == "self") {
        int contentRight = viewW - 2 * kPad - kAvatarSize;
        int contentLeft = kPad;
        int bubbleMaxW = contentRight - contentLeft;
        int bubbleW = std::min(bubbleMaxW, (bubbleMaxW * 80) / 100);
        int bubbleX = contentRight - bubbleW;
        int bubbleY = kPad + headerH + kPad; // relative to message top
        textRect = QRect(bubbleX + kBubbleHPad, bubbleY + kBubbleVPad,
                         bubbleW - 2 * kBubbleHPad, msg.height - (2*kPad + headerH + kMsgSpacing) - 2*kBubbleVPad);
    } else {
        int contentX = 2 * kPad + kAvatarSize;
        int contentW = viewW - kPad - contentX;
        int bubbleW = (contentW * 80) / 100;
        if (bubbleW < 100) { bubbleW = contentW; }
        textRect = QRect(contentX + kBubbleHPad, kPad + headerH + kPad + kBubbleVPad,
                         bubbleW - 2 * kBubbleHPad, msg.height - (2*kPad + headerH + kMsgSpacing) - 2*kBubbleVPad);
    }

    // Get line breaks
    int contentW = viewW - 3 * kPad - kAvatarSize;
    int bubbleW2 = (contentW * 80) / 100;
    if (bubbleW2 < 100) { bubbleW2 = contentW; }
    int bubbleTextWidth = bubbleW2 - 2 * kBubbleHPad;
    if (bubbleTextWidth < 20) { bubbleTextWidth = 20; }

    // Get line breaks using word-wrap
    std::vector<int> lineStarts;
    lineStarts.push_back(0);
    int cpos = 0;
    while (cpos < textLen) {
        if (msg.messageText[cpos] == '\n') {
            cpos++;
            lineStarts.push_back(cpos);
            continue;
        }
        int lineWidth = 0, lastSpace = -1, end = cpos;
        while (end < textLen && msg.messageText[end] != '\n') {
            lineWidth += fm.width(msg.messageText[end]);
            if (msg.messageText[end].isSpace()) lastSpace = end;
            if (lineWidth >= bubbleTextWidth) {
                if (lastSpace > cpos && end - cpos > 10) {
                    end = lastSpace + 1;
                }
                break;
            }
            end++;
        }
        cpos = end;
        if (cpos < textLen) { lineStarts.push_back(cpos); }
    }

    int lineHeight = fm.lineSpacing();
    for (size_t li = 0; li < lineStarts.size(); li++) {
        int lineStart = lineStarts[li];
        int lineEnd = (li+1 < lineStarts.size()) ? lineStarts[li+1] : textLen;
        int selStartInLine = std::max(start, lineStart);
        int selEndInLine = std::min(end, lineEnd);
        if (selStartInLine < selEndInLine) {
            int x1 = 0, x2 = 0;
            for (int i = lineStart; i < selStartInLine; i++) {
                x1 += fm.width(msg.messageText[i]);
            }
            for (int i = lineStart; i < selEndInLine; i++) {
                x2 += fm.width(msg.messageText[i]);
            }
            QRect selRect(textRect.x() + x1, textRect.y() + li * lineHeight, x2 - x1, lineHeight);
            rects.push_back(selRect);
        }
    }
    return rects;
}

QString ChatView::selectedText() const {
    if (m_selMsgIndex < 0 || m_selMsgIndex >= (int)m_history->size()) return QString();
    int start = std::min(m_selStart, m_selEnd);
    int end = std::max(m_selStart, m_selEnd);
    if (start == end) { return QString(); }
    return (*m_history)[m_selMsgIndex].messageText.mid(start, end - start);
}

void ChatView::selectWordAt(int msgIndex, int charPos) {
    if (msgIndex < 0 || msgIndex >= (int)m_history->size()) return;
    int oldIdx = m_selMsgIndex;
    const QString& text = (*m_history)[msgIndex].messageText;
    int start, end;
    wordBoundaries(text, charPos, start, end);
    m_selMsgIndex = msgIndex;
    m_selStart = start;
    m_selEnd = end;
    m_selecting = false;
    QRect dirty = messageRect(oldIdx);
#ifdef QT3_BUILD
    dirty = dirty.unite(messageRect(msgIndex));
#else
    dirty = dirty.united(messageRect(msgIndex));
#endif
    if (!dirty.isEmpty()) updateRect(dirty);
}

void ChatView::selectLineAt(int msgIndex, int charPos) {
    if (msgIndex < 0 || msgIndex >= (int)m_history->size()) return;
    int oldIdx = m_selMsgIndex;
    const QString& text = (*m_history)[msgIndex].messageText;
    int start, end;
    lineBoundaries(text, charPos, start, end);
    m_selMsgIndex = msgIndex;
    m_selStart = start;
    m_selEnd = end;
    m_selecting = false;
    QRect dirty = messageRect(oldIdx);
#ifdef QT3_BUILD
    dirty = dirty.unite(messageRect(msgIndex));
#else
    dirty = dirty.united(messageRect(msgIndex));
#endif
    if (!dirty.isEmpty()) updateRect(dirty);
}

void ChatView::copySelectedText() {
    QString text = selectedText();
    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
    }
}

void ChatView::copyFullMessage(int msgIndex) {
    if (msgIndex >= 0 && msgIndex < (int)m_history->size()) {
        QApplication::clipboard()->setText((*m_history)[msgIndex].messageText);
    }
}

// ── 原始数据 dump：把元素字段转成对齐文本（"查看原始数据"窗口用）──
static QString buildRawDataText(const ChatElement& el) {
    QString etypeStr;
    switch (el.etype) {
        case ChatElement::Text:   etypeStr = "Text";   break;
        case ChatElement::Image:  etypeStr = "Image";  break;
        case ChatElement::File:   etypeStr = "File";   break;
        case ChatElement::Video:  etypeStr = "Video";  break;
        case ChatElement::Gif:    etypeStr = "Gif";    break;
        case ChatElement::Audio:  etypeStr = "Audio";  break;
        default:                  etypeStr = "Unknown"; break;
    }
    QString sendStr;
    switch (el.sendState) {
        case ChatElement::SendSending: sendStr = "SendSending"; break;
        case ChatElement::SendSent:    sendStr = "SendSent";    break;
        case ChatElement::SendFailed:  sendStr = "SendFailed";  break;
        default:                       sendStr = "?";           break;
    }
    QString text;
    text += QString("dbRowid:        %1\n").arg(qFromUtf8(std::to_string((long long)el.dbRowid).c_str()));
    text += QString("redacted:       %1\n").arg(el.redacted ? "true" : "false");
    text += QString("etype:          %1\n").arg(etypeStr);
    text += QString("senderName:     %1\n").arg(el.senderName);
    text += QString("senderNickname: %1\n").arg(el.senderNickname);
    text += QString("peerNumber:     %1\n").arg(el.peerNumber);
    text += QString("category:       %1\n").arg(el.category);
    text += QString("time:           %1\n").arg(el.time);
    text += QString("ipAddress:      %1\n").arg(el.ipAddress);
    text += QString("messageText:    %1\n").arg(el.messageText);
    text += QString("avatarUrl:      %1\n").arg(el.avatarUrl);
    text += QString("mediaUrl:       %1\n").arg(el.mediaUrl);
    text += QString("caption:        %1\n").arg(el.caption);
    text += QString("fileName:       %1\n").arg(el.fileName);
    text += QString("fileSize:       %1\n").arg(el.fileSize);
    text += QString("localPath:      %1\n").arg(el.localPath);
    text += QString("durationSec:    %1\n").arg(el.durationSec);
    text += QString("sendState:      %1\n").arg(sendStr);
    text += QString("sendmsgseq:     %1\n").arg(el.sendmsgseq);
    text += QString("messageId:      %1\n").arg(el.messageId);
    text += QString("sendErrorMsg:   %1\n").arg(el.sendErrorMsg);
    text += QString("translatedText: %1\n").arg(el.translatedText);
    text += QString("translateError: %1\n").arg(el.translateError);
    text += QString("showTranslation:%1\n").arg(el.showTranslation);
    text += QString("firstInGroup:   %1\n").arg(el.firstInGroup);
    return text;
}

// Qt3/4 QTextEdit 显示纯文本统一入口
static void setTePlain(QTextEdit* te, const QString& t) {
#ifdef QT3_BUILD
    te->setText(t);
#else
    te->setPlainText(t);
#endif
}

void ChatView::showRawData(int msgIndex) {
    if (msgIndex < 0 || msgIndex >= (int)m_history->size()) return;
#ifdef QT3_BUILD
    QDialog dlg(this, "rawdata", true);
#else
    QDialog dlg(this);
#endif
    qSetWindowTitle(&dlg, qFromUtf8("原始数据"));
    QVBoxLayout* lay = new QVBoxLayout(&dlg);
    lay->setMargin(0);
    QTextEdit* te = new QTextEdit(&dlg);
    te->setReadOnly(true);
    lay->addWidget(te);

    QHBoxLayout* btns = new QHBoxLayout;
    btns->addStretch(1);
    QPushButton* updateBtn = new QPushButton(qFromUtf8("更新"), &dlg);
    QPushButton* copyBtn   = new QPushButton(qFromUtf8("拷贝"), &dlg);
    btns->addWidget(updateBtn);
    btns->addWidget(copyBtn);
    btns->addStretch(1);
    lay->addLayout(btns);

    setTePlain(te, buildRawDataText((*m_history)[msgIndex]));
    auto* updSlot = new LambdaSlot(updateBtn, [this, te, msgIndex]() {
        if (msgIndex < 0 || msgIndex >= (int)m_history->size()) { return; }
        setTePlain(te, buildRawDataText((*m_history)[msgIndex]));
    });
    connect(updateBtn, SIGNAL(clicked()), updSlot, SLOT(call()));
    auto* copySlot = new LambdaSlot(copyBtn, [te]() {
#ifdef QT3_BUILD
        QApplication::clipboard()->setText(te->text());
#else
        QApplication::clipboard()->setText(te->toPlainText());
#endif
    });
    connect(copyBtn, SIGNAL(clicked()), copySlot, SLOT(call()));
    dlg.resize(600, 480);
    dlg.exec();
}

void ChatView::wheelEvent(QWheelEvent* event) {
#ifndef QT3_BUILD
    // Qt4 macOS sends separate events for horizontal/vertical; ignore horizontal
    if (event->orientation() != Qt::Vertical) {
        event->ignore();
        return;
    }
#endif
    // Accumulate delta (macOS smooth scrolling: small values, many events)
    m_scrollDelta += event->delta();
    int steps = m_scrollDelta / 120;
    if (steps == 0) { return; }
    m_scrollDelta -= steps * 120;

#ifdef QT3_BUILD
    int step = m_vScrollBar->lineStep();
    int maxVal = m_vScrollBar->maxValue();
#else
    int step = m_vScrollBar->singleStep();
    int maxVal = m_vScrollBar->maximum();
#endif
    int delta = m_scrollPos + (steps > 0 ? -step * 5 : step * 5);
    delta = std::max(0, std::min(delta, maxVal));
    m_scrollPos = delta;
    m_vScrollBar->blockSignals(true);
    m_vScrollBar->setValue(delta);
    m_vScrollBar->blockSignals(false);
    update();
    event->accept();
}

void ChatView::keyPressEvent(QKeyEvent* event) {
    int val = m_vScrollBar->value();
    // Ctrl+C to copy selected text
#ifdef QT3_BUILD
    if (event->key() == Qt::Key_C && (event->state() & Qt::ControlButton)) {
#else
    if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
#endif
        copySelectedText();
        return;
    }
    switch (event->key()) {
#ifdef QT3_BUILD
    case Qt::Key_PageUp:     val -= m_vScrollBar->pageStep(); break;
    case Qt::Key_PageDown:   val += m_vScrollBar->pageStep(); break;
    case Qt::Key_Up:         val -= m_vScrollBar->lineStep(); break;
    case Qt::Key_Down:       val += m_vScrollBar->lineStep(); break;
    case Qt::Key_Home:       val  = m_vScrollBar->minValue(); break;
    case Qt::Key_End:        val  = m_vScrollBar->maxValue(); break;
#else
    case Qt::Key_PageUp:     val -= m_vScrollBar->pageStep(); break;
    case Qt::Key_PageDown:   val += m_vScrollBar->pageStep(); break;
    case Qt::Key_Up:         val -= m_vScrollBar->singleStep(); break;
    case Qt::Key_Down:       val += m_vScrollBar->singleStep(); break;
    case Qt::Key_Home:       val  = m_vScrollBar->minimum(); break;
    case Qt::Key_End:        val  = m_vScrollBar->maximum(); break;
#endif
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    m_vScrollBar->setValue(val);
    event->accept();
}

void ChatView::mousePressEvent(QMouseEvent* event) {
    if (m_scrollDownPill.handleClick(event->pos())) {
        return;
    }
    if (event->button() == Qt::LeftButton) {
        int msgIndex = findMessageAtY(event->y());
        if (msgIndex >= 0 && msgIndex < (int)m_history->size()) {
            // Check translate button click
            if ((*m_history)[msgIndex].etype != ChatElement::File
                && (*m_history)[msgIndex].translateBtnRect.contains(event->pos())) {
                if ((*m_history)[msgIndex].transState != TransState::InFlight) {
                    emit translateClicked(msgIndex);
                }
                return;
            }

            // Check source button click
            if ((*m_history)[msgIndex].etype != ChatElement::File
                && (*m_history)[msgIndex].sourceBtnRect.contains(event->pos())) {
                emit sourceClicked(msgIndex);
                return;
            }

            // Check download button click (handled in release, prevent text selection)
            if (!(*m_history)[msgIndex].downloadBtnRect.isNull() &&
                (*m_history)[msgIndex].downloadBtnRect.contains(event->pos())) {
                return;
            }
            // Check retry button click
            if (!(*m_history)[msgIndex].retryBtnRect.isNull() &&
                (*m_history)[msgIndex].retryBtnRect.contains(event->pos())) {
                return;
            }
            // Check resend via status icon
            if ((*m_history)[msgIndex].category == "self"
                && (*m_history)[msgIndex].sendState == ChatElement::SendFailed
                && (*m_history)[msgIndex].resendIconRect.contains(event->pos())) {
                return;
            }

            // Compute local Y relative to message
            int curY = kPad - m_scrollPos;
            for (int i = 0; i < msgIndex; i++) { curY += (*m_history)[i].height; }
            int localY = event->y() - curY;
            int localX = event->x();
            int charPos = charPosAt(msgIndex, localX, localY);
            if (charPos >= 0) {
                // Triple-click detection
                QTime now = QTime::currentTime();
                if (msgIndex == m_clickMsgIndex && m_clickCount == 2 &&
                    m_clickTime.msecsTo(now) <= QApplication::doubleClickInterval()) {
                    selectLineAt(msgIndex, charPos);
                    m_clickCount = 0;
                    return;
                }
                m_clickCount = 1;
                m_clickMsgIndex = msgIndex;
                m_clickTime = now;

                // Check if clicked on a URL
                {
                    const ChatElement& cel = (*m_history)[msgIndex];
                    const QString& linkSrc = (cel.etype == ChatElement::Image ||
                                              cel.etype == ChatElement::Gif ||
                                              cel.etype == ChatElement::Video)
                                          ? cel.caption : cel.messageText;
                    auto links = extractLinks(linkSrc);
                    for (const LinkSpan& link : links) {
                        qWarning("  PRESS link [%d,%d): %s",
                                 link.start, link.end, qToUtf8(link.url).data());
                        if (charPos >= link.start && charPos < link.end) {
                            qOpenUrl(link.url);
                            return;
                        }
                    }
                }
                // Start selection
                int oldIdx = m_selMsgIndex;
                m_selMsgIndex = msgIndex;
                m_selStart = charPos;
                m_selEnd = charPos;
                m_selecting = true;
                updateRect(messageRect(oldIdx));
            }
        } else {
            // Click outside messages, clear selection
            if (m_selMsgIndex != -1) {
                int oldIdx = m_selMsgIndex;
                m_selMsgIndex = -1;
                updateRect(messageRect(oldIdx));
            }
            m_clickCount = 0;
        }
    }
    QWidget::mousePressEvent(event);
}

void ChatView::mouseMoveEvent(QMouseEvent* event) {
    bool overPill = m_scrollDownPill.count() > 0
        && m_scrollDownPill.rect().contains(event->pos());
    if (overPill != m_scrollDownPill.isHovered()) {
        m_scrollDownPill.setHovered(overPill);
        updateRect(m_scrollDownPill.rect());
    }
    if (overPill) {
        setCursor(QCursor(Qt::PointingHandCursor));
        QWidget::mouseMoveEvent(event);
        return;
    }
    if (m_selecting && m_selMsgIndex >= 0 && m_selMsgIndex < (int)m_history->size()) {
        int msgIndex = findMessageAtY(event->y());
        if (msgIndex == m_selMsgIndex) {
            int msgY = msgAbsY(msgIndex) - m_scrollPos;
            int localY = event->y() - msgY;
            int localX = event->x();
            int charPos = charPosAt(msgIndex, localX, localY);
            if (charPos >= 0) {
                m_selEnd = charPos;
                updateRect(QRect(0, msgY, width(), (*m_history)[m_selMsgIndex].height));
            }
        }
    }
    // Set cursor based on whether over URL
    int msgIndex = findMessageAtY(event->y());
    if (msgIndex >= 0 && msgIndex < (int)m_history->size()) {
        // Check header action buttons first
        if ((*m_history)[msgIndex].translateBtnRect.contains(event->pos())) {
            const auto& el = (*m_history)[msgIndex];
            QString stateStr;
            switch (el.transState) {
            case TransState::None:      stateStr = qFromUtf8("None"); break;
            case TransState::Scheduled: stateStr = qFromUtf8("Scheduled"); break;
            case TransState::InFlight:  stateStr = qFromUtf8("InFlight"); break;
            case TransState::Done:      stateStr = qFromUtf8("Done"); break;
            }
            QString tip = el.translateError.isEmpty()
                ? qFromUtf8("Translate")
                : el.translateError;
            tip += qFromUtf8(" [") + stateStr + qFromUtf8("]");
            showTempTooltip(this, (*m_history)[msgIndex].translateBtnRect, tip);
            setCursor(QCursor(Qt::PointingHandCursor));
            QWidget::mouseMoveEvent(event);
            return;
        }
        if ((*m_history)[msgIndex].sourceBtnRect.contains(event->pos())) {
            showTempTooltip(this, (*m_history)[msgIndex].sourceBtnRect, qFromUtf8("Source"));
            setCursor(QCursor(Qt::PointingHandCursor));
            QWidget::mouseMoveEvent(event);
            return;
        }
        // Nickname tooltip: 显示 nickname 时，hover 名字区域显示 username
        {
            const auto& item = (*m_history)[msgIndex];
            if (!item.senderNickname.isEmpty() && !item.senderName.isEmpty()
                && item.senderNickname != item.senderName) {
                int msgY = msgAbsY(msgIndex) - m_scrollPos;
                QFont nf;
                nf.setPointSize(11);
                QFontMetrics nfm(nf);
                int headerH = nfm.lineSpacing();
                int halfW = (width() - 2*kPad) / 3;
                QRect nameRect;
                if (item.category == "self") {
                    nameRect = QRect(width() - kPad - halfW, msgY + kPad, halfW, headerH);
                } else {
                    nameRect = QRect(kPad, msgY + kPad, halfW, headerH);
                }
                if (nameRect.contains(event->pos())) {
                    showTempTooltip(this, nameRect, item.senderName);
                }
            }
        }

        // Debug: hover avatar to see identicon seed info
        {
            const auto& item = (*m_history)[msgIndex];
            QRect avatarRect = avatarRectFor(msgIndex);
            if (!avatarRect.isNull() && avatarRect.contains(event->pos())) {
                QString s = item.senderName.isEmpty()
                    ? QString::number(item.peerNumber)
                    : QString::number(item.peerNumber) + "|" + item.senderName;
                showTempTooltip(this, avatarRect,
                    "seed: " + s
                    + "\nname: " + item.senderName
                    + "\nnick: " + item.senderNickname
                    + "\npeer: " + QString::number(item.peerNumber)
                    + "\nurl: " + item.avatarUrl);
            }
        }

        // Send error tooltip
        {
            const auto& item = (*m_history)[msgIndex];
            if (item.sendState == ChatElement::SendFailed
                && !item.sendErrorMsg.isEmpty()
                && item.resendIconRect.contains(event->pos())) {
                    QString tip = qFromUtf8("错误：") + item.sendErrorMsg + qFromUtf8("。（点击重新发送）");
                    showTempTooltip(this, item.resendIconRect, tip);
            }
        }

        // Media download error tooltip
        {
            const auto& item = (*m_history)[msgIndex];
            if (item.downloadState == ChatElement::Failed
                && !item.mediaErrorMsg.isEmpty()
                && !item.retryBtnRect.isNull()
                && item.retryBtnRect.contains(event->pos())) {
                QString tip = qFromUtf8("媒体下载错误：") + item.mediaErrorMsg
                            + qFromUtf8("（点击重试）");
                showTempTooltip(this, item.retryBtnRect, tip);
            }
        }

            // Resend icon cursor
            {
                const auto& item = (*m_history)[msgIndex];
                if (item.sendState == ChatElement::SendFailed
                    && item.category == "self"
                    && item.resendIconRect.contains(event->pos())) {
                    setCursor(QCursor(Qt::PointingHandCursor));
                    QWidget::mouseMoveEvent(event);
                    return;
                }
            }
        // ... compute charPos for link detection
        int curY = msgAbsY(msgIndex) - m_scrollPos;
        int localY = event->y() - curY;
        int localX = event->x();
        int charPos = charPosAt(msgIndex, localX, localY);
        if (charPos >= 0) {
            const ChatElement& cel = (*m_history)[msgIndex];
            const QString& linkSrc = (cel.etype == ChatElement::Image ||
                                      cel.etype == ChatElement::Gif ||
                                      cel.etype == ChatElement::Video)
                                  ? cel.caption : cel.messageText;
            auto links = extractLinks(linkSrc);
            for (const LinkSpan& link : links) {
                if (charPos >= link.start && charPos < link.end) {
                    setCursor(QCursor(Qt::PointingHandCursor));
                    QWidget::mouseMoveEvent(event);
                    return;
                }
            }
        }
        // Over text area, set I-beam cursor
#ifdef QT3_BUILD
        setCursor(QCursor(Qt::IbeamCursor));
#else
        setCursor(QCursor(Qt::IBeamCursor));
#endif
    } else {
        setCursor(QCursor(Qt::ArrowCursor));
    }

    QWidget::mouseMoveEvent(event);
}

void ChatView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int msgIndex = findMessageAtY(event->y());
        if (msgIndex >= 0 && msgIndex < (int)m_history->size()) {
            ChatElement& el = (*m_history)[msgIndex];
            // Send failed retry
            if (el.sendState == ChatElement::SendFailed && el.category == "self"
                && !el.retryBtnRect.isNull() && el.retryBtnRect.contains(event->pos())) {
                el.sendState = ChatElement::SendSending;
                updateRect(el.resendIconRect);
                emit resendMessage(msgIndex);
                return;
            }
            // Resend via status icon
            if (el.category == "self"
                && el.sendState == ChatElement::SendFailed
                && el.resendIconRect.contains(event->pos())) {
                el.sendState = ChatElement::SendSending;
                updateRect(el.resendIconRect);
                emit resendMessage(msgIndex);
                return;
            }
            // Download button (always)
            if (!el.downloadBtnRect.isNull() &&
                el.downloadBtnRect.contains(event->pos())) {
                emit retryClicked(msgIndex, el.mediaUrl, qFromUtf8("manual_always"));
                return;
            }
            // Retry button (only when failed)
            if (!el.retryBtnRect.isNull() &&
                el.retryBtnRect.contains(event->pos())) {
                emit retryClicked(msgIndex, el.mediaUrl, qFromUtf8("manual_error"));
                return;
            }
        }
        if (m_selecting) {
            m_selecting = false;
            // If start and end are same, clear selection
            if (m_selStart == m_selEnd) {
                int oldIdx = m_selMsgIndex;
                m_selMsgIndex = -1;
                updateRect(messageRect(oldIdx));
            }
        }
        // Retry via status bar button replaces old thumbnail-area retry
    }
    QWidget::mouseReleaseEvent(event);
}

void ChatView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int msgIndex = findMessageAtY(event->y());
        if (msgIndex >= 0 && msgIndex < (int)m_history->size()) {
            // 双击下载/重试按钮时不打开 PhotoViewer、不进入选择
            if (!(*m_history)[msgIndex].downloadBtnRect.isNull() &&
                (*m_history)[msgIndex].downloadBtnRect.contains(event->pos())) {
                return;
            }
            if (!(*m_history)[msgIndex].retryBtnRect.isNull() &&
                (*m_history)[msgIndex].retryBtnRect.contains(event->pos())) {
                return;
            }
            // 双击头像：查看该发送者的好友信息（self 也发放，由上层显示或退化）
            QRect avatarRectForMsg = avatarRectFor(msgIndex);
            if (!avatarRectForMsg.isNull() && avatarRectForMsg.contains(event->pos())) {
                emit peerInfoRequested((*m_history)[msgIndex].peerNumber,
                                       (*m_history)[msgIndex].senderName);
                return;
            }
            // 双击缩略图：
            //   Image/Gif → 原图查看器；Video/Audio → 本机播放器
            if (((*m_history)[msgIndex].etype == ChatElement::Image ||
                 (*m_history)[msgIndex].etype == ChatElement::Gif) &&
                !(*m_history)[msgIndex].scaledDisplay.isNull() &&
                (*m_history)[msgIndex].thumbnailRect.contains(event->pos())) {
                emit openFullSizeImage(msgIndex, (*m_history)[msgIndex].mediaUrl);
                return;
            }
            if (((*m_history)[msgIndex].etype == ChatElement::Video ||
                 (*m_history)[msgIndex].etype == ChatElement::Audio) &&
                (*m_history)[msgIndex].thumbnailRect.contains(event->pos())) {
                emit openMediaPlayer(msgIndex);
                return;
            }
        }
        if (msgIndex >= 0) {
            int msgY = kPad - m_scrollPos;
            for (int i = 0; i < msgIndex; i++) { msgY += (*m_history)[i].height; }
            int localY = event->y() - msgY;
            int localX = event->x();
            int charPos = charPosAt(msgIndex, localX, localY);
            if (charPos >= 0) {
                selectWordAt(msgIndex, charPos);
                m_clickCount = 2;
                m_clickMsgIndex = msgIndex;
                m_clickTime = QTime::currentTime();
                return;
            }
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

static void openSearchInBrowser(QWidget* parent, bool ok, const QString& sel, const QString& baseUrl) {
    if (!ok) {
        ToastWidget::show(parent, _("search.invalid_selection"), 2500);
        return;
    }
    #ifdef QT3_BUILD
    std::string enc = ToxAPI::urlEncode(std::string(qToUtf8(sel).data()));
#else
    std::string enc = ToxAPI::urlEncode(std::string(qToUtf8(sel).constData()));
#endif
    qOpenUrl(baseUrl + qFromUtf8(enc.c_str()));
}

void ChatView::contextMenuEvent(QContextMenuEvent* event) {
    int msgIndex = findMessageAtY(event->y());
    // 检测是否在名字区域上
    bool onName = false;
    QString displayName;
    if (msgIndex >= 0 && msgIndex < (int)m_history->size()) {
        const ChatElement& item = (*m_history)[msgIndex];
        displayName = item.senderNickname.isEmpty() ? item.senderName : item.senderNickname;
        int msgY = kPad - m_scrollPos;
        for (int i = 0; i < msgIndex; i++) { msgY += (*m_history)[i].height; }
        QFont nf;
        nf.setPointSize(11);
        QFontMetrics nfm(nf);
        int headerH = nfm.lineSpacing();
        QRect nameRect(kPad, msgY + kPad, width() - 2*kPad, headerH);
        onName = item.firstInGroup && nameRect.contains(event->pos());
    }
#ifdef QT3_BUILD
    QPopupMenu menu(this);
#else
    QMenu menu(this);
#endif
    bool canRetry = (msgIndex >= 0 && msgIndex < (int)m_history->size()
                     && (*m_history)[msgIndex].sendState == ChatElement::SendFailed
                     && (*m_history)[msgIndex].category == "self");
    bool canRedact = (msgIndex >= 0 && msgIndex < (int)m_history->size()
                      && (*m_history)[msgIndex].category == "self"
                      && !(*m_history)[msgIndex].messageId.isEmpty());
    // TEMP(临时): 所有消息均显示"撤回"。回滚时删除下面这行即可恢复原检测。
    canRedact = true;
#ifdef QT3_BUILD
    QString searchSel = selectedText().stripWhiteSpace();
#else
    QString searchSel = selectedText().trimmed();
#endif
    bool searchOk = (searchSel.length() >= 3 && searchSel.length() <= 60);
    // Copy full message
#ifdef QT3_BUILD
    // Qt3 QMenuData::insertItem() 自动生成的 ID 是负数（-1, -2, ...），
    // 不能用 >= 0 判断是否插入成功，所以用 bool 标志追踪，哨兵值用 0。
    int copyMsgId = menu.insertItem(_("context.copy_message"));
    bool hasShowRaw = (msgIndex >= 0);
    int showRawId = hasShowRaw ? menu.insertItem(qFromUtf8("查看原始数据")) : 0;
    bool hasRetry = canRetry;
    int retryMsgId = hasRetry ? menu.insertItem(qFromUtf8("重发")) : 0;
    int selectAllId = menu.insertItem(_("context.select_all"));
    bool hasSource = false, hasTranslate = false;
    int sourceMsgId = 0, translateMsgId = 0;
    bool hasFav = false, hasForward = false;
    int favMsgId = 0, forwardMsgId = 0;
    if (msgIndex >= 0 && (*m_history)[msgIndex].etype != ChatElement::File) {
        hasSource = true;
        sourceMsgId = menu.insertItem(qFromUtf8("查看原文"));
        hasTranslate = true;
        translateMsgId = menu.insertItem(qFromUtf8("翻译"));
        QString favLabel;
        if ((*m_history)[msgIndex].dbRowid > 0)
            favLabel = isFavRowid((*m_history)[msgIndex].dbRowid)
                       ? qFromUtf8("取消收藏") : qFromUtf8("收藏");
        else
            favLabel = qFromUtf8("收藏/取消");
        hasFav = true;
        favMsgId = menu.insertItem(favLabel);
        hasForward = true;
        forwardMsgId = menu.insertItem(qFromUtf8("转发"));
    }
    QPopupMenu searchSubMenu(this);
    int searchGoogleId = searchSubMenu.insertItem("Google");
    int searchBingId = searchSubMenu.insertItem("Bing");
    int searchDuckId = searchSubMenu.insertItem("DuckDuckGo");
    int searchYandexId = searchSubMenu.insertItem("Yandex");
    menu.insertItem(_("context.search_selected"), &searchSubMenu);
    bool hasNick = onName;
    int copyNickId = 0, mentionId = 0, viewInfoId = 0;
    if (hasNick) {
        copyNickId = menu.insertItem(qFromUtf8("复制昵称"));
        mentionId = menu.insertItem(qFromUtf8("@ TA"));
        viewInfoId = menu.insertItem(qFromUtf8("查看好友信息"));
    }
    bool hasMsgActions = (msgIndex >= 0 && msgIndex < (int)m_history->size());
    int replyMsgId = 0, editMsgId = 0, deleteMsgId = 0, redactMsgId = 0;
    if (hasMsgActions) {
        replyMsgId = menu.insertItem(_("context.reply"));
        editMsgId = menu.insertItem(_("context.edit_message"));
        menu.insertSeparator();
        deleteMsgId = menu.insertItem(_("context.delete_message"));
        if (canRedact) {
            redactMsgId = menu.insertItem(_("context.redact_message"));
        }
    }
#else
    QAction* copyMsgAction = menu.addAction(_("context.copy_message"));
    QAction* showRawAction = msgIndex >= 0 ? menu.addAction(qFromUtf8("查看原始数据")) : nullptr;
    QAction* retryMsgAction = canRetry ? menu.addAction("重发") : nullptr;
    QAction* selectAllAction = menu.addAction(_("context.select_all"));
    QAction* sourceMsgAction = nullptr;
    QAction* translateMsgAction = nullptr;
    QAction* favMsgAction = nullptr;
    QAction* forwardMsgAction = nullptr;
    if (msgIndex >= 0 && (*m_history)[msgIndex].etype != ChatElement::File) {
        sourceMsgAction = menu.addAction("查看原文");
        translateMsgAction = menu.addAction("翻译");
        QString favLabel;
        if ((*m_history)[msgIndex].dbRowid > 0)
            favLabel = isFavRowid((*m_history)[msgIndex].dbRowid)
                       ? qFromUtf8("取消收藏") : qFromUtf8("收藏");
        else
            favLabel = qFromUtf8("收藏/取消");
        favMsgAction = menu.addAction(favLabel);
        forwardMsgAction = menu.addAction(qFromUtf8("转发"));
    }
    QMenu* searchSubMenu = menu.addMenu(_("context.search_selected"));
    QAction* searchGoogleAction = searchSubMenu->addAction("Google");
    QAction* searchBingAction = searchSubMenu->addAction("Bing");
    QAction* searchDuckAction = searchSubMenu->addAction("DuckDuckGo");
    QAction* searchYandexAction = searchSubMenu->addAction("Yandex");
    QAction* copyNickAction = nullptr;
    QAction* mentionAction = nullptr;
    QAction* viewInfoAction = nullptr;
    if (onName) {
        copyNickAction = menu.addAction(qFromUtf8("复制昵称"));
        mentionAction = menu.addAction(qFromUtf8("@ TA"));
        viewInfoAction = menu.addAction(qFromUtf8("查看好友信息"));
    }
    QAction* replyMsgAction = nullptr;
    QAction* editMsgAction = nullptr;
    QAction* deleteMsgAction = nullptr;
    QAction* redactMsgAction = nullptr;
    if (msgIndex >= 0 && msgIndex < (int)m_history->size()) {
        replyMsgAction = menu.addAction(_("context.reply"));
        editMsgAction = menu.addAction(_("context.edit_message"));
        menu.addSeparator();
        deleteMsgAction = menu.addAction(_("context.delete_message"));
        if (canRedact) {
            redactMsgAction = menu.addAction(_("context.redact_message"));
        }
    }
#endif
#ifdef QT3_BUILD
    int choice = menu.exec(event->globalPos());
    if (choice == copyMsgId) {
        copyFullMessage(msgIndex);
    } else if (hasShowRaw && choice == showRawId) {
        showRawData(msgIndex);
    } else if (hasRetry && choice == retryMsgId) {
        (*m_history)[msgIndex].sendState = ChatElement::SendSending;
        updateRect((*m_history)[msgIndex].resendIconRect);
        emit resendMessage(msgIndex);
    } else if (choice == selectAllId) {
        // Select all text in all messages
        m_selMsgIndex = 0;
        m_selStart = 0;
        m_selEnd = m_history->empty() ? 0 : m_history->back().messageText.length();
        updateFull();
    } else if (hasSource && choice == sourceMsgId) {
        emit sourceClicked(msgIndex);
    } else if (hasTranslate && choice == translateMsgId) {
        emit translateClicked(msgIndex);
    } else if (hasFav && choice == favMsgId) {
        emit favoriteClicked(msgIndex);
    } else if (hasForward && choice == forwardMsgId) {
        emit forwardClicked(msgIndex);
    } else if (hasNick && choice == copyNickId) {
        QApplication::clipboard()->setText(displayName);
    } else if (hasNick && choice == mentionId) {
        emit mentionClicked((*m_history)[msgIndex].senderName, displayName);
    } else if (hasNick && choice == viewInfoId) {
        emit peerInfoRequested((*m_history)[msgIndex].peerNumber,
                               (*m_history)[msgIndex].senderName);
    } else if (hasMsgActions && choice == replyMsgId) {
        emit replyRequested(msgIndex);
    } else if (hasMsgActions && choice == editMsgId) {
        emit editRequested(msgIndex);
    } else if (hasMsgActions && choice == deleteMsgId) {
        emit deleteRequested(msgIndex);
    } else if (canRedact && choice == redactMsgId) {
        emit redactRequested(msgIndex);
    } else if (choice == searchGoogleId) {
        openSearchInBrowser(this, searchOk, searchSel, "https://www.google.com/search?q=");
    } else if (choice == searchBingId) {
        openSearchInBrowser(this, searchOk, searchSel, "https://www.bing.com/search?q=");
    } else if (choice == searchDuckId) {
        openSearchInBrowser(this, searchOk, searchSel, "https://duckduckgo.com/?q=");
    } else if (choice == searchYandexId) {
        openSearchInBrowser(this, searchOk, searchSel, "https://yandex.com/search/?text=");
    }
#else
    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == copyMsgAction) {
        copyFullMessage(msgIndex);
    } else if (chosen == showRawAction) {
        showRawData(msgIndex);
    } else if (canRetry && chosen == retryMsgAction) {
        (*m_history)[msgIndex].sendState = ChatElement::SendSending;
        updateRect((*m_history)[msgIndex].resendIconRect);
        emit resendMessage(msgIndex);
    } else if (chosen == selectAllAction) {
        m_selMsgIndex = 0;
        m_selStart = 0;
        m_selEnd = m_history->empty() ? 0 : m_history->back().messageText.length();
        updateFull();
    } else if (sourceMsgAction && chosen == sourceMsgAction) {
        emit sourceClicked(msgIndex);
    } else if (translateMsgAction && chosen == translateMsgAction) {
        emit translateClicked(msgIndex);
    } else if (favMsgAction && chosen == favMsgAction) {
        emit favoriteClicked(msgIndex);
    } else if (forwardMsgAction && chosen == forwardMsgAction) {
        emit forwardClicked(msgIndex);
    } else if (chosen == copyNickAction) {
        QApplication::clipboard()->setText(displayName);
    } else if (chosen == mentionAction) {
        emit mentionClicked((*m_history)[msgIndex].senderName, displayName);
    } else if (chosen == viewInfoAction) {
        emit peerInfoRequested((*m_history)[msgIndex].peerNumber,
                               (*m_history)[msgIndex].senderName);
    } else if (replyMsgAction && chosen == replyMsgAction) {
        emit replyRequested(msgIndex);
    } else if (editMsgAction && chosen == editMsgAction) {
        emit editRequested(msgIndex);
    } else if (deleteMsgAction && chosen == deleteMsgAction) {
        emit deleteRequested(msgIndex);
    } else if (redactMsgAction && chosen == redactMsgAction) {
        emit redactRequested(msgIndex);
    } else if (chosen == searchGoogleAction) {
        openSearchInBrowser(this, searchOk, searchSel, "https://www.google.com/search?q=");
    } else if (chosen == searchBingAction) {
        openSearchInBrowser(this, searchOk, searchSel, "https://www.bing.com/search?q=");
    } else if (chosen == searchDuckAction) {
        openSearchInBrowser(this, searchOk, searchSel, "https://duckduckgo.com/?q=");
    } else if (chosen == searchYandexAction) {
        openSearchInBrowser(this, searchOk, searchSel, "https://yandex.com/search/?text=");
    }
#endif
}

void ChatView::manageGifLikeAnimations() {
    if (!m_history) { return; }
    int viewBottom = m_scrollPos + height();
    int first = findByAbsY(m_scrollPos);
    if (first < 0) { first = 0; }
    int absY = msgAbsY(first);
    int y = absY - m_scrollPos;
    for (size_t i = first; i < m_history->size(); ++i) {
        int h = (*m_history)[i].height;
        bool visible = (absY + h > m_scrollPos) && (absY < viewBottom);
        bool isGifLike = ((*m_history)[i].etype == ChatElement::Gif)
                      || isGifLikeVideo((*m_history)[i]);
        if (isGifLike) {
            if (visible) {
                if (!(*m_history)[i].movie) {
                    (*m_history)[i].startGifLikeAnimation(this, i);
                }
                if ((*m_history)[i].movie && i < m_gifFrameUpdated.size() && m_gifFrameUpdated[i]) {
                    m_gifFrameUpdated[i] = 0;
                    update(QRect(0, y, width(), h));
                }
            } else {
                (*m_history)[i].stopGifLikeAnimation();
            }
        }
        if (absY > viewBottom) { break; }
        absY += h;
        y += h;
    }
}

void ChatView::onAnimTick() {
    manageGifLikeAnimations();
}

void ChatView::onGifFrameUpdated(int msgIndex) {
    if (msgIndex >= 0 && msgIndex < (int)m_gifFrameUpdated.size()) {
        m_gifFrameUpdated[msgIndex] = 1;
    }
}

std::pair<int,int> ChatView::visibleMessageRange() const {
    int top = m_scrollPos;
    int bottom = top + height();
    int first = findByAbsY(top);
    if (first < 0) { return {-1, -1}; }
    int absY = msgAbsY(first);
    int last = first;
    while (last + 1 < (int)m_history->size() && absY + (*m_history)[last].height < bottom) {
        absY += (*m_history)[last].height;
        last++;
    }
    return {first, last};
}

void ChatView::paintEvent(QPaintEvent* event) {
    if (!m_history) { return; }
    TimePoint _t0 = timeNow();
    if (m_backBuffer.size() != size()) {
        m_backBuffer = QPixmap(size());
    }
    QPainter bp(&m_backBuffer);
    bp.setClipRect(event->rect());
    bp.fillRect(event->rect(), currentPalette().windowBg);

    int viewW = contentWidth();
    int vpH = height();

    std::vector<QRect> selRects;
    if (m_selMsgIndex >= 0 && m_selMsgIndex < (int)m_history->size()) {
        selRects = selectionRects(m_selMsgIndex);
    }

    int first = findByAbsY(m_scrollPos);
    if (first < 0) { first = 0; }
    int absY = msgAbsY(first);
    int y = absY - m_scrollPos;
    for (size_t i = first; i < m_history->size(); ++i) {
        int h = (*m_history)[i].height;
        if (y + h >= 0 && y <= vpH) {
            (*m_history)[i].paint(bp, y, viewW, ((int)i == m_selMsgIndex), selRects,
                             m_fm, m_emojiW, font(), currentPalette());
            ChatElement& el = (*m_history)[i];
            if (el.downloadState == ChatElement::NotRequested
                && el.scaledDisplay.isNull()
                && !el.mediaUrl.isEmpty()
                && (el.etype == ChatElement::Image || el.etype == ChatElement::Gif
                    || el.etype == ChatElement::Video)
                && el.fileSize >= UnkSize && el.fileSize < 1048576)
            {
                emit retryClicked((int)i, el.mediaUrl, qFromUtf8("autopaint"));
            }

            if (el.etype == ChatElement::Text
                && !el.messageText.isEmpty()
                && el.transState == TransState::None)
            {
                if (!el.needsTranslateComputed) {
                    el.needsTranslateResult = needsAutoTranslate(el.messageText, Config::value("translate_tolang"));
                    el.needsTranslateComputed = true;
                }
                if (el.needsTranslateResult) {
                    el.transState = TransState::Scheduled;
                    qWarning("ChatView: auto-trigger translate msgIndex=%d lang=%s text=[%.8192s]",
                             (int)i, qToUtf8(Config::value("translate_tolang")).data(),
                             qToUtf8(el.messageText).data());
                    emit autoTranslateRequested((int)i, el.messageText, Config::value("translate_tolang"));
                }
            }
        }
        if (y > vpH) { break; }
        y += h;
        absY += h;
    }
    m_scrollDownPill.paint(bp, rect(), currentPalette().windowBg, currentPalette().textPrimary);
    bp.end();

    QPainter p(this);
    p.drawPixmap(0, 0, m_backBuffer);
    long long _el = elapsedMs(_t0); if (_el >= 50) qWarning("SLOW [hangui] paintEvent took %lldms", _el);
}

void ChatView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    m_backBuffer = QPixmap();
    int sbw = m_vScrollBar->sizeHint().width();
    m_vScrollBar->setGeometry(width() - sbw, 0, sbw, height());
    relayout();
}

void ChatView::onScrollChanged(int value) {
    m_scrollPos = value;
    update();
#ifdef QT3_BUILD
    int maxVal = m_vScrollBar->maxValue();
#else
    int maxVal = m_vScrollBar->maximum();
#endif
    if (maxVal - value <= 20 && m_scrollDownPill.count() > 0) {
        m_scrollDownPill.setCount(0);
        updateRect(m_scrollDownPill.rect());
    }
    m_vScrollBar->showTemporarily();
    flushEviction();
}

void ChatView::flushEviction() {
    if (!m_history || m_history->empty()) { return; }
    int sz = (int)m_history->size();
    if (m_evictLow < 0) { m_evictLow = 0; }
    if (m_evictLow >= sz) { m_evictLow = sz - 1; }
    if (m_evictHigh < 0) { m_evictHigh = 0; }
    if (m_evictHigh >= sz) { m_evictHigh = sz - 1; }
    auto [first, last] = visibleMessageRange();
    if (first < 0) { return; }
    int nearLow = std::max(0, first - 30);
    int nearHigh = std::min(sz - 1, last + 30);
    while (m_evictLow < nearLow) {
        (*m_history)[m_evictLow].scaledDisplay = QPixmap();
        m_evictLow++;
    }
    while (m_evictHigh > nearHigh) {
        (*m_history)[m_evictHigh].scaledDisplay = QPixmap();
        m_evictHigh--;
    }
    m_evictLow = nearLow;
    m_evictHigh = nearHigh;
}
