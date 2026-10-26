#include "rawdatapopup.h"

#include "androidutils.h"
#include "multilinetextedit.h"
#include "myscrollarea.h"
#include "rawdatautil.h"

#include <QskBox.h>
#include <QskBoxShapeMetrics.h>
#include <QskFontRole.h>
#include <QskGradient.h>
#include <QskLinearBox.h>
#include <QskPushButton.h>
#include <QskTextLabel.h>
#include <QskTextOptions.h>

#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>

namespace {

QRectF popupParentRect(QQuickItem* parent)
{
    if (!parent) {
        return QRectF();
    }
    if (auto* window = parent->window()) {
        return QRectF(QPointF(0, 0), window->size());
    }
    return QRectF(-parent->x(), -parent->y(),
        parent->width(), parent->height());
}

QString subtitleText(const MessageItem& item)
{
    QString text = item.sender;
    if (!item.time.isEmpty()) {
        if (!text.isEmpty()) {
            text += QStringLiteral(" · ");
        }
        text += item.time;
    }
    return text;
}

} // namespace

RawDataPopup::RawDataPopup(const MessageItem& item, QQuickItem* parent)
    : QskPopup(parent)
    , m_item(item)
{
    setModal(true);
    setOverlay(true);
    setPopupFlag(QskPopup::DeleteOnClose, false);
    setPolishOnResize(true);
    setPolishOnParentResize(true);

    m_panel = new QskBox(this);
    m_panel->setBoxShapeHint(QskBox::Panel,
        QskBoxShapeMetrics(14, Qt::AbsoluteSize));

    QskGradient panelGradient = m_panel->fillGradient();
    if (panelGradient.isValid()) {
        panelGradient.setAlpha(qRound(0.92 * 255.0));
        m_panel->setFillGradient(panelGradient);
    }

    m_layout = new QskLinearBox(Qt::Vertical, m_panel);
    m_layout->setMargins(18);
    m_layout->setSpacing(12);
    m_layout->setSizePolicy(
        QskSizePolicy::MinimumExpanding, QskSizePolicy::Constrained);

    m_titleLabel = new QskTextLabel(QString::fromUtf8("原始数据"), m_layout);
    m_titleLabel->setFontRole(
        QskFontRole(QskFontRole::Subtitle, QskFontRole::High));
    m_titleLabel->setAlignment(Qt::AlignCenter);

    m_subLabel = new QskTextLabel(subtitleText(item), m_layout);
    m_subLabel->setFontRole(
        QskFontRole(QskFontRole::Caption, QskFontRole::Low));
    m_subLabel->setAlignment(Qt::AlignCenter);
    m_subLabel->setWrapMode(QskTextOptions::WordWrap);

    m_scroll = new MyScrollArea(m_layout);
    m_scroll->setSizePolicy(
        QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_layout->setStretchFactor(m_scroll, 1);

    m_edit = new MultiLineTextEdit(m_scroll);
    m_edit->setReadOnly(true);
    m_edit->setContentSized(true);
    m_scroll->setScrolledItem(m_edit);

    auto* buttonBox = new QskLinearBox(Qt::Horizontal, m_layout);
    buttonBox->setSpacing(10);

    m_copyButton = new QskPushButton(QString::fromUtf8("拷贝"), buttonBox);
    m_refreshButton = new QskPushButton(QString::fromUtf8("刷新"), buttonBox);
    m_closeButton = new QskPushButton(QString::fromUtf8("关闭"), buttonBox);

    QskPushButton* const buttons[] = {
        m_copyButton, m_refreshButton, m_closeButton
    };
    for (auto* button : buttons) {
        button->setBoxShapeHint(QskPushButton::Panel,
            QskBoxShapeMetrics(8, Qt::AbsoluteSize));
        button->setPreferredHeight(36);
        button->setSizePolicy(
            QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    }

    connect(m_copyButton, &QskPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(m_text);
        showAndroidToast(QString::fromUtf8("已复制"));
    });
    connect(m_refreshButton, &QskPushButton::clicked,
        this, &RawDataPopup::refresh);
    connect(m_closeButton, &QskPushButton::clicked,
        this, &RawDataPopup::close);

    refresh();
}

void RawDataPopup::show(QQuickItem* parent, const MessageItem& item)
{
    auto* popup = new RawDataPopup(item, parent);
    connect(popup, &QskPopup::closed, popup, &QObject::deleteLater);

    QTimer::singleShot(0, popup, [popup]() {
        popup->open();
        popup->updateGeometry();
    });
}

void RawDataPopup::refresh()
{
    m_text = buildRawDataText(m_item);
    if (m_edit) {
        m_edit->setText(m_text);
    }
}

void RawDataPopup::updateLayout()
{
    updateGeometry();
    m_layout->setGeometry(layoutRect());
}

void RawDataPopup::keyPressEvent(QKeyEvent* event)
{
    // Esc（桌面）自关闭，防止沿焦点链冒泡触发页面回退。
    // Android back 键在 main.cpp BackButtonFilter（应用级前置 filter）处理。
    if (event->key() == Qt::Key_Escape) {
        close();
        event->accept();
        return;
    }
    QskPopup::keyPressEvent(event);
}

void RawDataPopup::updateGeometry()
{
    const QRectF parentRect = popupParentRect(parentItem());
    if (parentRect.isEmpty()) {
        return;
    }

    const qreal width = qBound(320.0, parentRect.width() * 0.94, 640.0);
    const qreal height = parentRect.height() * 0.9;
    if (width <= 0.0 || height <= 0.0) {
        return;
    }

    QRectF popupRect(0, 0, width, height);
    popupRect.moveCenter(parentRect.center());
    setGeometry(popupRect);
    m_panel->setGeometry(QRectF(0, 0, width, height));
}
