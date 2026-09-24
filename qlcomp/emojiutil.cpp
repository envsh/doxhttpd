#include "emojiutil.h"
#include "compat34.h"

#ifdef EMOJI_RENDER_QT34
#ifdef QT3_BUILD
#include <qimage.h>
#include <qfontinfo.h>
#else
#include <QImage>
#include <QFontInfo>
#endif
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

bool isEmojiChar(uint32_t cp) {
    if (cp >= 0x1F000 && cp <= 0x1FFFF) {
        if (cp >= 0x1F100 && cp <= 0x1FAFF) { return true; }
        return false;
    }
    return (cp == 0x00A9 || cp == 0x00AE ||
            cp == 0x203C || cp == 0x2049 ||
            cp == 0x2122 || cp == 0x2139 ||
            (cp >= 0x2194 && cp <= 0x2199) ||
            cp == 0x21A9 || cp == 0x21AA ||
            cp == 0x231A || cp == 0x231B ||
            (cp >= 0x23E9 && cp <= 0x23F3) ||
            (cp >= 0x23F8 && cp <= 0x23FA) ||
            cp == 0x24C2 ||
            cp == 0x25AA || cp == 0x25AB ||
            cp == 0x25B6 || cp == 0x25C0 ||
            (cp >= 0x25FB && cp <= 0x25FE) ||
            (cp >= 0x2600 && cp <= 0x27BF) ||
            (cp >= 0x2934 && cp <= 0x2935) ||
            (cp >= 0x2B05 && cp <= 0x2B07) ||
            cp == 0x2B1B || cp == 0x2B1C ||
            cp == 0x2B50 || cp == 0x2B55 ||
            cp == 0x3030 || cp == 0x303D ||
            cp == 0x3297 || cp == 0x3299);
}

int emojiCharWidth(const QFontMetrics& fm) {
    return fm.height();
}

std::vector<uint32_t> toCodepoints(const QString& text) {
    std::vector<uint32_t> cps;
    int n = text.length();
    for (int i = 0; i < n; i++) {
        ushort u = text[i].unicode();
        if (u >= 0xD800 && u <= 0xDBFF && i + 1 < n) {
            ushort l = text[i + 1].unicode();
            if (l >= 0xDC00 && l <= 0xDFFF) {
                cps.push_back(0x10000 + ((u - 0xD800) << 10) + (l - 0xDC00));
                i++;
                continue;
            }
        }
        cps.push_back(u);
    }
    return cps;
}

bool textHasEmoji(const QString& text) {
    auto cps = toCodepoints(text);
    for (size_t i = 0; i < cps.size(); i++) {
        if (isEmojiChar(cps[i])) { return true; }
    }
    return false;
}

#ifdef EMOJI_RENDER_QT34

static QPixmap scalePixmap(const QPixmap& pm, int w, int h) {
#ifdef QT3_BUILD
    QImage img = pm.convertToImage().smoothScale(w, h);
    QPixmap out;
    out.convertFromImage(img);
    return out;
#else
    return pm.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
#endif
}

static QPixmap rawBytesToQPixmap(const unsigned char* bgra, int w, int h) {
#ifdef QT3_BUILD
    QImage img(w, h, 32);
    img.setAlphaBuffer(true);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int off = (y * w + x) * 4;
            uint8_t b = bgra[off];
            uint8_t g = bgra[off + 1];
            uint8_t r = bgra[off + 2];
            uint8_t a = bgra[off + 3];
            img.setPixel(x, y, (a << 24) | (r << 16) | (g << 8) | b);
        }
    }
    QPixmap pm;
    pm.convertFromImage(img);
    return pm;
#else
    QImage raw(const_cast<uchar*>(bgra), w, h, w * 4, QImage::Format_ARGB32_Premultiplied);
    return QPixmap::fromImage(raw);
#endif
}

EmojiRenderer::EmojiRenderer() : m_lib(0), m_face(0), m_ok(false) {
    if (FT_Init_FreeType((FT_Library*)&m_lib) != 0) {
        fprintf(stderr, "EmojiRenderer: FT_Init_FreeType failed\n");
        return;
    }
    fprintf(stderr, "EmojiRenderer: FT_Init_FreeType OK\n");
    const char* paths[] = {
        "/System/Library/Fonts/Apple Color Emoji.ttc",
        "/Library/Fonts/Apple Color Emoji.ttc",
        "/usr/share/fonts/twemoji/twemoji.ttf",
        "/usr/share/fonts/noto/NotoColorEmoji.ttf",
        0
    };
    for (int i = 0; paths[i]; i++) {
        if (FT_New_Face((FT_Library)m_lib, paths[i], 0, (FT_Face*)&m_face) == 0) {
            m_ok = true;
            m_fontPath = paths[i];
            fprintf(stderr, "EmojiRenderer: loaded font = %s\n", paths[i]);
            return;
        }
        fprintf(stderr, "EmojiRenderer: FT_New_Face failed for %s\n", paths[i]);
    }
    fprintf(stderr, "EmojiRenderer: no emoji font found\n");
}

EmojiRenderer::~EmojiRenderer() {
    if (m_face) { FT_Done_Face((FT_Face)m_face); }
    if (m_lib) { FT_Done_FreeType((FT_Library)m_lib); }
}

EmojiRenderer& EmojiRenderer::instance() {
    static EmojiRenderer inst;
    return inst;
}

QPixmap EmojiRenderer::loadEmoji(uint32_t codepoint, int size) {
    uint64_t key = ((uint64_t)codepoint << 32) | (uint32_t)size;
    auto it = m_cache.find(key);
    if (it != m_cache.end()) { return it->second; }

    QPixmap pm;
    FT_Face face = (FT_Face)m_face;

    if (FT_Set_Pixel_Sizes(face, 0, size) != 0) {
        int best = 0, bestDist = 99999;
        for (int i = 0; i < face->num_fixed_sizes; i++) {
            int d = abs((int)face->available_sizes[i].height - size);
            if (d < bestDist) { bestDist = d; best = i; }
        }
        fprintf(stderr, "EmojiRenderer: set size failed for U+%04X size=%d, "
                "using strike %d (%dx%d) + custom scale\n",
                codepoint, size, best,
                face->available_sizes[best].width,
                face->available_sizes[best].height);
        FT_Select_Size(face, best);
    }

    if (FT_Load_Char(face, codepoint, FT_LOAD_COLOR) != 0) {
        fprintf(stderr, "EmojiRenderer: FT_Load_Char failed for U+%04X\n", codepoint);
        m_cache[key] = pm;
        return pm;
    }

    FT_GlyphSlot slot = face->glyph;
    if (slot && slot->bitmap.buffer && slot->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
        pm = rawBytesToQPixmap(slot->bitmap.buffer, slot->bitmap.width, slot->bitmap.rows);
    }

    if (!pm.isNull() && (pm.width() != size || pm.height() != size)) {
#ifdef QT3_BUILD
        QImage img = pm.convertToImage().smoothScale(size, size);
        pm.convertFromImage(img);
#else
        pm = pm.scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
#endif
    }

    m_cache[key] = pm;
    return pm;
}

QPixmap EmojiRenderer::renderEmoji(uint32_t codepoint, int size) {
    if (!m_ok) { return QPixmap(); }
    return loadEmoji(codepoint, size);
}

static bool setupEmojiFallbackFont(QPainter& p) {
    static const char* fams[] = {
        "Apple Color Emoji",
        "Noto Color Emoji",
        "Noto Emoji",
        "Segoe UI Emoji",
        "Twemoji Mozilla",
        0
    };
    QFont base = p.font();
    for (int i = 0; fams[i]; i++) {
        QFont f = base;
        f.setFamily(qFromUtf8(fams[i]));
        QFontInfo info(f);
        if (info.family() != qFromUtf8(fams[i])) { continue; }
        p.setFont(f);
        return true;
    }
    return false;
}

void EmojiRenderer::drawText(QPainter& p, const QRect& textRect, const QString& text) {
    drawText(p, textRect, text, 0);
}

void EmojiRenderer::drawText(QPainter& p, const QRect& textRect, const QString& text, int emojiSize) {
    if (!m_ok) {
        bool hasNewline = false;
        for (int i = 0; i < text.length(); i++) {
            if (text[i] == QChar('\n')) { hasNewline = true; break; }
        }
        if (!hasNewline && text.length() <= 8) {
            if (setupEmojiFallbackFont(p)) {
                QFont f = p.font();
                int fs = emojiSize > 0 ? emojiSize
                                       : (textRect.width() < textRect.height()
                                              ? textRect.width() : textRect.height());
                if (fs < 8) { fs = 8; }
                if (fs > 64) { fs = 64; }
                f.setPixelSize(fs);
                p.setFont(f);
#ifdef QT3_BUILD
                p.drawText(textRect, Qt::AlignCenter | Qt::DontClip, text);
#else
                p.drawText(textRect, Qt::AlignCenter | Qt::TextDontClip, text);
#endif
                return;
            }
        }
#ifdef QT3_BUILD
        p.drawText(textRect, Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop, text);
#else
        p.drawText(textRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, text);
#endif
        return;
    }

    auto cps = toCodepoints(text);
    QFontMetrics fm = p.fontMetrics();
    int lh = emojiSize > 0 ? emojiSize : fm.lineSpacing();
    if (lh <= 0) { lh = fm.height(); }
    if (lh <= 0) { lh = 16; }
    int maxW = textRect.width();
    int x = textRect.x();
    int y = textRect.y();
    int n = (int)cps.size();
    int i = 0;

    while (i < n) {
        if (cps[i] == '\n') { x = textRect.x(); y += lh; i++; continue; }

        int lineWidth = 0;
        int lastSpace = -1;
        int lineEnd = i;
        for (int j = i; j < n && cps[j] != '\n'; j++) {
            int cw = isEmojiChar(cps[j]) ? (emojiSize > 0 ? lh : fm.height())
                                         : fm.width(QChar((ushort)cps[j]));
            lineWidth += cw;
            if (cps[j] == ' ') { lastSpace = j; }
            if (lineWidth >= maxW) {
                if (lastSpace > i && j - i > 10) { lineEnd = lastSpace + 1; }
                break;
            }
            lineEnd = j + 1;
        }
        if (lineEnd <= i) { lineEnd = i + 1; }

        int lx = x;
        for (int j = i; j < lineEnd; j++) {
            if (isEmojiChar(cps[j])) {
                QPixmap pm = loadEmoji(cps[j], lh);
                if (!pm.isNull()) {
                    int pw = pm.width();
                    int ph = pm.height();
                    if (pw > textRect.width()) { pw = textRect.width(); }
                    if (ph > textRect.height()) { ph = textRect.height(); }
                    if (pm.width() != pw || pm.height() != ph) {
                        if (pw > 0 && ph > 0) {
                            pm = scalePixmap(pm, pw, ph);
                        }
                    }
                    int ey = y + (lh - pm.height()) / 2;
                    p.drawPixmap(lx, ey, pm);
                    lx += (emojiSize > 0 ? pm.width() : fm.height());
                } else {
                    if (cps[j] <= 0xFFFF) {
                        p.drawText(lx, y + fm.ascent(), QChar((ushort)cps[j]));
                    }
                    lx += (emojiSize > 0 ? lh : fm.height());
                }
            } else {
                int s = j;
                QString seg;
                while (j < lineEnd && !isEmojiChar(cps[j])) {
                    if (cps[j] <= 0xFFFF) {
                        seg += QChar((ushort)cps[j]);
                    }
                    else {
                        uint32_t cp = cps[j] - 0x10000;
                        seg += QChar((ushort)(0xD800 + (cp >> 10)));
                        seg += QChar((ushort)(0xDC00 + (cp & 0x3FF)));
                    }
                    j++;
                }
                j--;
                p.drawText(lx, y + fm.ascent(), seg);
                lx += fm.width(seg);
            }
        }

        y += lh;
        i = lineEnd;
    }
}

#endif
