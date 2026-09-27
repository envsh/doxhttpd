#include "floatingpill.h"

FloatingPill::FloatingPill() : m_count(0), m_hovered(false) {}

void FloatingPill::setCount(int n) {
    m_count = n;
}

void FloatingPill::paint(QPainter& p, const QRect& parentRect, const QColor& bgColor, const QColor& textColor) {
    if (m_count <= 0) return;

    QFont f = p.font();
    f.setPointSize(11);
    p.setFont(f);

    QString label = QString::fromUtf8("↓↓ %1").arg(m_count);
    int textW = qFontWidth(p.fontMetrics(), label);

    int btnH = 28;
    int btnW = textW + 22;
    int btnX = parentRect.width() - btnW - 12;
    int btnY = parentRect.height() - btnH - 10;
    m_rect = QRect(btnX, btnY, btnW, btnH);

#ifndef QT3_BUILD
    p.setRenderHint(QPainter::Antialiasing);
#endif

    // Shadow offset 3px
    QRect shadowRect(m_rect.x() + 3, m_rect.y() + 3, m_rect.width(), m_rect.height());
    p.setPen(Qt::NoPen);
#ifndef QT3_BUILD
    p.setBrush(QColor(0, 0, 0, 55));
#else
    p.setBrush(QColor(180, 180, 185));
#endif
#ifdef QT3_BUILD
    p.drawRoundRect(shadowRect, 40, 100);
#else
    p.drawRoundedRect(shadowRect, 40, 100);
#endif

    // Background matches chat area
    p.setBrush(bgColor);
#ifdef QT3_BUILD
    p.drawRoundRect(m_rect, 40, 100);
#else
    p.drawRoundedRect(m_rect, 40, 100);
#endif

    (void)m_hovered;
    p.setPen(textColor);
    p.drawText(m_rect, Qt::AlignCenter, label);
}

bool FloatingPill::handleClick(const QPoint& pos) {
    if (m_count > 0 && m_rect.contains(pos)) {
        if (m_onClicked) m_onClicked();
        return true;
    }
    return false;
}
