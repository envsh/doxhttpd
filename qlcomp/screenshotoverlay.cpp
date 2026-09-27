#include "screenshotoverlay.h"
#include "compat34.h"
#include "StyleParams.h"

#ifdef QT3_BUILD
#include <qapplication.h>
#include <qdesktopwidget.h>
#include <qimage.h>
#include <qcursor.h>
#else
#include <QApplication>
#if QT_VERSION < 0x060000
#include <QDesktopWidget>  // Qt6 移除 QDesktopWidget（改用 QScreen）
#endif
#include <QImage>
#include <QCursor>
#endif

#include <qkeysequence.h>

ScreenshotRegionSelector::ScreenshotRegionSelector(const QPixmap& background)
#ifdef QT3_BUILD
    : QWidget(nullptr, "region_selector")
#else
    : QWidget(nullptr)
#endif
    , m_background(background)
    , m_selecting(false)
    , m_hasSelection(false)
{
    QRect screenRect = qVirtualDesktopRect();
    setGeometry(screenRect);

    // 全屏无边框置顶窗口
#ifdef QT3_BUILD
    setWFlags(WStyle_Customize | WStyle_NoBorder | WStyle_StaysOnTop);
#else
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
#endif

    setMouseTracking(true);
#ifdef QT3_BUILD
    setCursor(CrossCursor);
#else
    setCursor(Qt::CrossCursor);
#endif

    grabKeyboard();
}

ScreenshotRegionSelector::~ScreenshotRegionSelector() {
}

void ScreenshotRegionSelector::mousePressEvent(QMouseEvent* e) {
#ifdef QT3_BUILD
    if (e->button() != LeftButton) { return; }
#else
    if (e->button() != Qt::LeftButton) { return; }
#endif
    m_startPoint = e->pos();
    m_selection = QRect(m_startPoint, QSize(0, 0));
    m_selecting = true;
    m_hasSelection = false;
    update();
}

void ScreenshotRegionSelector::mouseMoveEvent(QMouseEvent* e) {
    if (!m_selecting) { return; }
    m_selection = QRect(m_startPoint, e->pos());
    update();
}

void ScreenshotRegionSelector::mouseReleaseEvent(QMouseEvent* e) {
#ifdef QT3_BUILD
    if (e->button() != LeftButton) { return; }
#else
    if (e->button() != Qt::LeftButton) { return; }
#endif
    if (!m_selecting) { return; }
    m_selecting = false;
    m_selection = QRect(m_startPoint, e->pos());
    m_hasSelection = true;
    update();
}

void ScreenshotRegionSelector::mouseDoubleClickEvent(QMouseEvent*) {
    if (m_hasSelection && m_selection.width() > 0 && m_selection.height() > 0) {
        releaseKeyboard();
        emit regionSelected(m_selection, m_background);
        deleteLater();
    }
}

void ScreenshotRegionSelector::keyPressEvent(QKeyEvent* e) {
    int key = e->key();

    if (key == Qt::Key_Escape) {
        releaseKeyboard();
        emit cancelled();
        deleteLater();
    } else if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        if (m_hasSelection && m_selection.width() > 0 && m_selection.height() > 0) {
            releaseKeyboard();
            emit regionSelected(m_selection, m_background);
            deleteLater();
        }
    }
}

void ScreenshotRegionSelector::paintEvent(QPaintEvent*) {
    QPainter p(this);

    // 绘制截图背景
    p.drawPixmap(0, 0, m_background);

    if (!m_hasSelection && !m_selecting) {
        const StyleParams::Palette& pal = g_activeParams->light;
        p.setPen(pal.textMuted);
        QString hint("Click & drag to select region, Enter to confirm, Esc to cancel");
#ifdef QT3_BUILD
        p.drawText(rect(), AlignCenter, hint);
#else
        p.drawText(rect(), Qt::AlignCenter, hint);
#endif
        return;
    }

    // 归一化选区
#ifdef QT3_BUILD
    QRect sel = m_selection;
    sel.normalize();
#else
    QRect sel = m_selection.normalized();
#endif

    // 暗色遮罩：clip 排除选区，填充半透明黑
    p.save();
    {
        QRegion mask(rect());
        mask -= QRegion(sel);
        p.setClipRegion(mask);
    }

#ifdef QT3_BUILD
    // Qt3 QImage 默认为 32-bit depth with alpha; fill with qRgba
    QImage overlayImg(size().width(), size().height(), 32);
    overlayImg.fill(qRgba(0, 0, 0, 140));
    QPixmap overlayPm;
    overlayPm.convertFromImage(overlayImg);
    p.drawPixmap(0, 0, overlayPm);
#else
    p.fillRect(rect(), QColor(0, 0, 0, 140));
#endif

    p.restore();

    // 白色边框
#ifdef QT3_BUILD
    p.setPen(QPen(white, 2));
#else
    p.setPen(QPen(Qt::white, 2));
#endif
    p.drawRect(sel);

    // 尺寸标注
    QString sizeInfo;
#ifdef QT3_BUILD
    sizeInfo.sprintf("%d x %d", sel.width(), sel.height());
#else
    sizeInfo = QString("%1 x %2").arg(sel.width()).arg(sel.height());
#endif

    QFont infoFont = p.font();
    infoFont.setPointSize(infoFont.pointSize() + 2);
    p.setFont(infoFont);

    int labelX = sel.right() + 10;
    int labelY = sel.top() + infoFont.pointSize() + 4;

    if (labelX + 100 > width()) {
        labelX = sel.left() - 10 - 100;
        labelY = sel.top() + infoFont.pointSize() + 4;
    }
    if (labelY > height()) {
        labelY = sel.bottom() - 4;
    }
    if (labelX < 0) {
        labelX = 4;
    }

    QRect textRect(labelX, labelY - infoFont.pointSize() - 2, 200, infoFont.pointSize() + 8);

    // 标注背景
#ifdef QT3_BUILD
    QImage bgImg(textRect.size().width(), textRect.size().height(), 32);
    bgImg.fill(qRgba(0, 0, 0, 180));
    QPixmap bgPm;
    bgPm.convertFromImage(bgImg);
    p.drawPixmap(textRect.topLeft(), bgPm);
#else
    p.fillRect(textRect, QColor(0, 0, 0, 180));
#endif

#ifdef QT3_BUILD
    p.setPen(white);
    p.drawText(textRect, AlignCenter, sizeInfo);
#else
    p.setPen(Qt::white);
    p.drawText(textRect, Qt::AlignCenter, sizeInfo);
#endif
}
