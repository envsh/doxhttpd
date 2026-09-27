#include "searchlistview.h"
#include "LimeStyle.h"
#include "compat34.h"
#include <qpixmap.h>
#include <qpainter.h>
#include <algorithm>

SearchListView::SearchListView(QWidget* parent)
    : QWidget(parent) {}

int SearchListView::totalHeight() const {
    return m_rowHeight * (int)m_rows.size();
}

void SearchListView::clampScrollY() {
    int total = totalHeight();
    int view = height();
    if (total <= view) {
        m_scrollY = 0;
    } else if (m_scrollY > total - view) {
        m_scrollY = total - view;
    }
    if (m_scrollY < 0) { m_scrollY = 0; }
}

void SearchListView::setRows(const std::vector<RowInfo>& rows, const QString& emptyText) {
    m_rows = rows;
    m_emptyText = emptyText;
    m_scrollY = 0;
    if (m_bar) {
        m_bar->blockSignals(true);
        m_bar->setRange(0, std::max(0, totalHeight() - height()));
        m_bar->setValue(0);
        m_bar->blockSignals(false);
    }
    update();
}

void SearchListView::scrollTo(int v) {
    if (v < 0) { v = 0; }
    m_scrollY = v;
    clampScrollY();
    if (m_bar) {
        m_bar->blockSignals(true);
        m_bar->setValue(m_scrollY);
        m_bar->blockSignals(false);
    }
    update();
}

void SearchListView::render(QPainter& p) {
    p.fillRect(rect(), currentPalette().windowBg);
    clampScrollY();
    int h = m_rowHeight;
    int viewH = height();
    int sz = (int)m_rows.size();
    if (sz == 0) {
        p.setPen(currentPalette().textMuted);
        p.drawText(rect(), Qt::AlignCenter, m_emptyText);
        return;
    }
    int first = m_scrollY / h;
    int last = (m_scrollY + viewH - 1) / h;
    if (last >= sz) { last = sz - 1; }
    for (int i = first; i <= last; ++i) {
        int y = i * h - m_scrollY;
        const RowInfo& r = m_rows[i];

        QFont tf = p.font();
        tf.setBold(true);
        p.setFont(tf);
        p.setPen(currentPalette().textPrimary);
        p.drawText(QRect(8, y + 4, width() - 16 - 8, 20), Qt::AlignLeft | Qt::AlignVCenter,
                   qElideChars(r.title, width() - 28, ElideRight));

        QFont df = p.font();
        df.setBold(false);
        df.setPointSize((int)(df.pointSize() * 0.92));
        p.setFont(df);
        p.setPen(currentPalette().textMuted);
        p.drawText(QRect(8, y + 22, width() - 16 - 8, 18), Qt::AlignLeft | Qt::AlignVCenter,
                   qElideChars(r.detail, width() - 28, ElideRight));

        p.setPen(currentPalette().border);
        p.drawLine(0, y + h - 1, width(), y + h - 1);
    }
}

void SearchListView::paintEvent(QPaintEvent*) {
#ifdef QT3_BUILD
    if (m_backBuffer.isNull() || m_backBuffer.size() != size()) {
        m_backBuffer = QPixmap(size());
    }
    {
        QPainter bp(&m_backBuffer);
        render(bp);
    }
    bitBlt(this, 0, 0, &m_backBuffer);
#else
    QPainter p(this);
    render(p);
#endif
}

void SearchListView::wheelEvent(QWheelEvent* e) {
    int step = 88;
    m_scrollY -= qWheelDeltaY(e) / 120 * step;
    clampScrollY();
    scrollTo(m_scrollY);
}

void SearchListView::resizeEvent(QResizeEvent*) {
    clampScrollY();
    if (m_bar) {
        m_bar->blockSignals(true);
        m_bar->setRange(0, std::max(0, totalHeight() - height()));
        m_bar->setValue(m_scrollY);
        m_bar->blockSignals(false);
    }
    update();
}