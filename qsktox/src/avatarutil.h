#ifndef AVATAR_UTIL_H
#define AVATAR_UTIL_H

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QRectF>
#include <QString>

// 消息头像调色板：按发送者名稳定取色。实时推送与 DB 往返必须同源，
// 否则同一个人在历史消息里会换色（messagelist / msgdb_bridge 共用）。
inline QColor msgAvatarColorFor(const QString& name)
{
    static const QColor palette[] = {
        QColor("#4CAF50"), QColor("#2196F3"), QColor("#FF9800"),
        QColor("#E91E63"), QColor("#9C27B0"), QColor("#00BCD4"),
        QColor("#FF5722"), QColor("#795548"), QColor("#607D8B"),
    };
    static constexpr int paletteSize =
        (int)(sizeof(palette) / sizeof(palette[0]));
    if (name.isEmpty()) {
        return QColor("#607D8B");
    }
    return palette[qHash(name) % paletteSize];
}

inline QString msgAvatarLetterFor(const QString& name)
{
    if (name.isEmpty()) {
        return QString(" ");
    }
    return QString(name.at(0)).toUpper();
}

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
