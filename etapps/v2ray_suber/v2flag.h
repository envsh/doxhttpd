#ifndef V2FLAG_H
#define V2FLAG_H

#include <qstring.h>
#include <qpixmap.h>

/* 国家代码 → 国旗 pixmap
 * 参考 qlcomp 自绘机制：HarfBuzz 合成 regional-indicator 对 → FreeType 彩色渲染
 * 字体：twemoji.ttf → NotoColorEmoji.ttf（都没有时才画代码徽章兜底） */
QPixmap v2FlagPixmap(const QString& cc, int height);

#endif
