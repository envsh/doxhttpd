#ifndef EMOJIUTIL_H
#define EMOJIUTIL_H

#include <stdint.h>
#include <vector>
#include <string>
#include <qstring.h>
#include <qfontmetrics.h>
#include <qpainter.h>

bool isEmojiChar(uint32_t codepoint);
int emojiCharWidth(const QFontMetrics& fm);
std::vector<uint32_t> toCodepoints(const QString& text);

#ifdef EMOJI_RENDER_QT34
#include <map>

bool textHasEmoji(const QString& text);

class EmojiRenderer {
public:
    static EmojiRenderer& instance();
    void drawText(QPainter& p, const QRect& textRect, const QString& text);
    void drawText(QPainter& p, const QRect& textRect, const QString& text, int emojiSize);
    QPixmap renderEmoji(uint32_t codepoint, int size);
private:
    EmojiRenderer();
    ~EmojiRenderer();
    QPixmap loadEmoji(uint32_t codepoint, int size);
    void* m_lib;
    void* m_face;
    bool m_ok;
    std::string m_fontPath;
    std::map<uint64_t, QPixmap> m_cache;
};
#endif

#endif
