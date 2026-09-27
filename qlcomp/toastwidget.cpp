#include "toastwidget.h"
#ifdef QT3_BUILD
#include <qpainter.h>
#else
#include <QPainter>
#endif

ToastWidget::ToastWidget(QWidget* parent, const QString& text, int durationMs)
    : QWidget(parent
#ifdef QT3_BUILD
      , 0, WStyle_StaysOnTop
#else
      , Qt::ToolTip | Qt::FramelessWindowHint
#endif
      ), m_text(text), m_timerId(0)
{
#ifndef QT3_BUILD
    setAttribute(Qt::WA_DeleteOnClose);
#endif
    setFixedSize(300, 36);
    m_timerId = startTimer(durationMs);
}

void ToastWidget::show(QWidget* parent, const QString& text, int durationMs) {
    ToastWidget* tw = new ToastWidget(parent, text, durationMs);
    tw->positionAtBottom();
    tw->QWidget::show();
}

void ToastWidget::positionAtBottom() {
    if (!parentWidget()) return;
    QRect pr = parentWidget()->rect();
#ifdef QT3_BUILD
    int x = (pr.width() - width()) / 2;
    int y = pr.height() - height() - 60;
    move(x, y);
#else
    QPoint pg = parentWidget()->mapToGlobal(QPoint(0, 0));
    int x = pg.x() + (pr.width() - width()) / 2;
    int y = pg.y() + pr.height() - height() - 60;
    move(x, y);
#endif
}

void ToastWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
#ifndef QT3_BUILD
    p.setRenderHint(QPainter::Antialiasing);
#endif
#ifdef QT3_BUILD
    QColor bg(0x1a, 0x1a, 0x1a);
#else
    QColor bg(0x1a, 0x1a, 0x1a, 0xE0);
#endif
    p.setBrush(bg);
    p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
    p.drawRoundRect(rect(), 25, 25);
#else
    p.drawRoundedRect(rect(), 25, 25);
#endif
    p.setPen(Qt::white);
    p.drawText(rect(), Qt::AlignCenter, m_text);
}

void ToastWidget::timerEvent(QTimerEvent*) {
    deleteLater();
}

void ToastWidget::mousePressEvent(QMouseEvent*) {
    deleteLater();
}
