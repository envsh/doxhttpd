#ifndef AVATAR_UTIL_H
#define AVATAR_UTIL_H

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QRectF>
#include <QString>

// 字母圆块头像（无真实头像/identicon 时兜底；messagelist/channellist 共用）。
// color 非法时回退 #607D8B。
inline void drawLetterAvatar(QPainter* painter, const QRectF& rect,
                             const QColor& color, const QString& letter)
{
    const QColor bg = color.isValid() ? color : QColor("#607D8B");
    painter->setBrush(bg);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(rect);
    painter->setPen(Qt::white);
    QFont avatarFont;
    avatarFont.setPixelSize(16);
    avatarFont.setBold(true);
    painter->setFont(avatarFont);
    painter->drawText(rect, Qt::AlignCenter, letter);
}

#endif // AVATAR_UTIL_H
