#include "v2flag.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

#include <qimage.h>
#include <qpainter.h>
#include <qpen.h>
#include <qcolor.h>
#include <qfont.h>
#include <qmap.h>
#include <string.h>

struct V2FlagState {
    FT_Library lib;
    FT_Face face;
    hb_font_t* hb;
    bool ok;
    QMap<QString, QPixmap> cache;
    V2FlagState() : lib(0), face(0), hb(0), ok(false) {}
};

static V2FlagState& v2FlagState() {
    static V2FlagState s;
    static bool inited = false;
    if (inited) { return s; }
    inited = true;
    if (FT_Init_FreeType(&s.lib) != 0) { return s; }
    const char* paths[] = {
        "/usr/share/fonts/twemoji/twemoji.ttf",
        "/usr/share/fonts/noto/NotoColorEmoji.ttf",
        "/System/Library/Fonts/Apple Color Emoji.ttc",
        "/Library/Fonts/Apple Color Emoji.ttc",
        0
    };
    for (int i = 0; paths[i]; ++i) {
        if (FT_New_Face(s.lib, paths[i], 0, &s.face) == 0) {
            s.hb = hb_ft_font_create(s.face, NULL);
            s.ok = (s.hb != 0);
            return s;
        }
    }
    return s;
}

/* BGRA → QPixmap（qlcomp emojiutil 同款处理） */
static QPixmap v2BgraToPixmap(const unsigned char* bgra, int w, int h) {
    QImage img(w, h, 32);
#ifdef QT3_BUILD
    img.setAlphaBuffer(true);
#endif
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int off = (y * w + x) * 4;
            unsigned char b = bgra[off];
            unsigned char g = bgra[off + 1];
            unsigned char r = bgra[off + 2];
            unsigned char a = bgra[off + 3];
            img.setPixel(x, y, ((uint)a << 24) | ((uint)r << 16)
                | ((uint)g << 8) | (uint)b);
        }
    }
    QPixmap pm;
    pm.convertFromImage(img);
    return pm;
}

/* 无 emoji 字体时的兜底徽章（正常系统不会走到） */
static QPixmap v2FlagBadge(const QString& cc, int h) {
    /* 不透明底（Qt3 对 alpha QPixmap 用 QPainter 绘制在部分环境不生效） */
    QPixmap pm((int)(h * 1.45), h);
    pm.fill(QColor(60, 90, 140));
    QPainter p(&pm);
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPixelSize(h - 4);
    f.setBold(true);
    p.setFont(f);
    p.drawText(pm.rect(), Qt::AlignCenter, cc);
    p.end();
    return pm;
}

QPixmap v2FlagPixmap(const QString& cc, int height) {
    if (cc.length() != 2 || height <= 0) { return QPixmap(); }
    QString key = cc + "@" + QString::number(height);
    V2FlagState& s = v2FlagState();
    if (s.cache.contains(key)) { return s.cache[key]; }
    if (!s.ok) {
        QPixmap pm = v2FlagBadge(cc, height);
        s.cache.insert(key, pm);
        return pm;
    }
    /* 两个 regional indicator → HarfBuzz 合成国旗单字形 */
    uint32_t cps[2];
#ifdef QT3_BUILD
    cps[0] = 0x1F1E6 + (cc[0].upper().unicode() - 'A');
    cps[1] = 0x1F1E6 + (cc[1].upper().unicode() - 'A');
#else
    cps[0] = 0x1F1E6 + (cc[0].toUpper().unicode() - 'A');
    cps[1] = 0x1F1E6 + (cc[1].toUpper().unicode() - 'A');
#endif
    hb_buffer_t* b = hb_buffer_create();
    hb_buffer_add_utf32(b, cps, 2, 0, 2);
    hb_buffer_guess_segment_properties(b);
    hb_shape(s.hb, b, NULL, 0);
    unsigned int len = 0;
    hb_glyph_info_t* info = hb_buffer_get_glyph_infos(b, &len);
    unsigned int gid = (len == 1) ? info[0].codepoint : 0;
    hb_buffer_destroy(b);
    if (gid == 0) {
        QPixmap pm = v2FlagBadge(cc, height);
        s.cache.insert(key, pm);
        return pm;
    }
    if (FT_Set_Pixel_Sizes(s.face, 0, (FT_UInt)height) != 0) {
        int best = 0;
        int bestDist = 99999;
        for (int i = 0; i < s.face->num_fixed_sizes; ++i) {
            int d = s.face->available_sizes[i].height - height;
            if (d < 0) { d = -d; }
            if (d < bestDist) { bestDist = d; best = i; }
        }
        if (s.face->num_fixed_sizes > 0) { FT_Select_Size(s.face, best); }
    }
    QPixmap pm;
    if (FT_Load_Glyph(s.face, gid,
            FT_LOAD_COLOR | FT_LOAD_RENDER | FT_LOAD_NO_HINTING) == 0) {
        FT_GlyphSlot g = s.face->glyph;
        if (g->bitmap.buffer && g->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
            pm = v2BgraToPixmap(g->bitmap.buffer, g->bitmap.width, g->bitmap.rows);
        }
    }
    if (pm.isNull()) {
        pm = v2FlagBadge(cc, height);
    } else if (pm.height() != height) {
        QImage img = pm.convertToImage().smoothScale(
            height * pm.width() / pm.height(), height);
        pm.convertFromImage(img);
    }
    s.cache.insert(key, pm);
    return pm;
}
