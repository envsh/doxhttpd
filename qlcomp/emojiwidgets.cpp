#include "emojiwidgets.h"
#include "emojiutil.h"

#ifdef EMOJI_RENDER_QT34

// ============ EmojiLabel ============

#ifdef QT3_BUILD

EmojiLabel::EmojiLabel(QWidget* parent, const char* name)
    : QLabel(parent, name) {}
EmojiLabel::EmojiLabel(const QString& text, QWidget* parent, const char* name)
    : QLabel(text, parent, name) {}

void EmojiLabel::drawContents(QPainter* p) {
    QString t = text();
    if (!textHasEmoji(t)) { QLabel::drawContents(p); return; }
    QRect cr = contentsRect();
    int m = margin();
    QRect tr(cr.x() + m, cr.y() + m, cr.width() - 2 * m, cr.height() - 2 * m);
    EmojiRenderer::instance().drawText(*p, tr, t);
}

#else // QT4

EmojiLabel::EmojiLabel(QWidget* parent, Qt::WindowFlags f) : QLabel(parent, f) {}
EmojiLabel::EmojiLabel(const QString& text, QWidget* parent, Qt::WindowFlags f)
    : QLabel(text, parent, f) {}

void EmojiLabel::paintEvent(QPaintEvent* event) {
    QLabel::paintEvent(event);
    QString t = text();
    if (textHasEmoji(t) && !pixmap()) {
        QPainter p(this);
        p.setClipRect(event->rect());
        QRect tr = contentsRect().adjusted(margin(), margin(), -margin(), -margin());
        EmojiRenderer::instance().drawText(p, tr, t);
    }
}

#endif

// ============ EmojiPushButton ============

QString EmojiPushButton::text() const {
    return m_emojiText.isEmpty() ? QPushButton::text() : m_emojiText;
}

void EmojiPushButton::setText(const QString& text) {
    if (textHasEmoji(text)) {
        m_emojiText = text;
        QPushButton::setText(QString());
    } else {
        m_emojiText = QString();
        QPushButton::setText(text);
    }
}

#ifdef QT3_BUILD
#include <qstyle.h>

EmojiPushButton::EmojiPushButton(QWidget* parent, const char* name)
    : QPushButton(parent, name), m_emojiInset(0) { setText(QString()); }
EmojiPushButton::EmojiPushButton(const QString& text, QWidget* parent, const char* name)
    : QPushButton(parent, name), m_emojiInset(0) { setText(text); }

void EmojiPushButton::drawButtonLabel(QPainter* p) {
    if (!m_emojiText.isEmpty()) {
        QRect cr = rect();
        if (m_emojiInset > 0) {
            cr.addCoords(m_emojiInset, m_emojiInset, -m_emojiInset, -m_emojiInset);
            if (cr.width() < 4 || cr.height() < 4) { cr = rect(); }
        }
        QFont f = p->font();
        int px = cr.width() < cr.height() ? cr.width() : cr.height();
        if (px > 64) { px = 64; }
        if (px > 0) { f.setPixelSize(px); }
        p->setFont(f);
        QFontMetrics fm = p->fontMetrics();
        int es = px > 0 ? px : fm.height();
        if (es < 8) { es = 8; }
        if (es > 64) { es = 64; }
        int ew = fm.height();
        if (ew < es) { ew = es; }
        if (ew > cr.width()) ew = cr.width();
        QRect tr(cr.x() + (cr.width() - ew) / 2,
                 cr.y(), ew, cr.height());
        EmojiRenderer::instance().drawText(*p, tr, m_emojiText, es);
        return;
    }
    QPushButton::drawButtonLabel(p);
}

#else

EmojiPushButton::EmojiPushButton(QWidget* parent) : QPushButton(parent), m_emojiInset(0) {
    setAttribute(Qt::WA_LayoutUsesWidgetRect, true);
    setText(QString());
}
EmojiPushButton::EmojiPushButton(const QString& text, QWidget* parent)
    : QPushButton(parent), m_emojiInset(0) {
    setAttribute(Qt::WA_LayoutUsesWidgetRect, true);
    setText(text);
}

void EmojiPushButton::paintEvent(QPaintEvent* event) {
    if (m_emojiText.isEmpty()) { QPushButton::paintEvent(event); return; }
    QPainter p(this);
    QStyleOptionButton opt;
    opt.initFrom(this);
    opt.text = QString();
    style()->drawControl(QStyle::CE_PushButton, &opt, &p, this);
    QRect cr = contentsRect();
    if (m_emojiInset > 0) {
        cr.adjust(m_emojiInset, m_emojiInset, -m_emojiInset, -m_emojiInset);
        if (cr.width() < 4 || cr.height() < 4) { cr = contentsRect(); }
    }
    QFont f = p.font();
    int px = cr.width() < cr.height() ? cr.width() : cr.height();
    if (px > 64) { px = 64; }
    if (px > 0) { f.setPixelSize(px); }
    p.setFont(f);
    QFontMetrics fm = p.fontMetrics();
    int es = px > 0 ? px : fm.height();
    if (es < 8) { es = 8; }
    if (es > 64) { es = 64; }
    int ew = fm.height();
    if (ew < es) { ew = es; }
    if (ew > cr.width()) ew = cr.width();
    QRect tr(cr.x() + (cr.width() - ew) / 2,
             cr.y(), ew, cr.height());
    EmojiRenderer::instance().drawText(p, tr, m_emojiText, es);
}

#endif

// ============ EmojiToolButton ============

#ifdef QT3_BUILD

EmojiToolButton::EmojiToolButton(QWidget* parent, const char* name)
    : QToolButton(parent, name) {}

void EmojiToolButton::drawButtonLabel(QPainter* p) {
    QString t = text();
    if (!textHasEmoji(t)) { QToolButton::drawButtonLabel(p); return; }
    EmojiRenderer::instance().drawText(*p, rect(), t);
}

#else

EmojiToolButton::EmojiToolButton(QWidget* parent) : QToolButton(parent) {}

void EmojiToolButton::paintEvent(QPaintEvent* event) {
    QToolButton::paintEvent(event);
    if (textHasEmoji(text())) {
        QPainter p(this);
        EmojiRenderer::instance().drawText(p, rect(), text());
    }
}

#endif

// ============ EmojiCheckBox ============

#ifdef QT3_BUILD

EmojiCheckBox::EmojiCheckBox(QWidget* parent, const char* name)
    : QCheckBox(parent, name) {}
EmojiCheckBox::EmojiCheckBox(const QString& text, QWidget* parent, const char* name)
    : QCheckBox(text, parent, name) {}

void EmojiCheckBox::drawButtonLabel(QPainter* p) {
    QString t = text();
    if (!textHasEmoji(t)) { QCheckBox::drawButtonLabel(p); return; }
    EmojiRenderer::instance().drawText(*p, rect(), t);
}

#else

EmojiCheckBox::EmojiCheckBox(QWidget* parent) : QCheckBox(parent) {}
EmojiCheckBox::EmojiCheckBox(const QString& text, QWidget* parent)
    : QCheckBox(text, parent) {}

void EmojiCheckBox::paintEvent(QPaintEvent* event) {
    if (!textHasEmoji(text())) { QCheckBox::paintEvent(event); return; }
    QPainter p(this);
    QStyleOptionButton opt;
    opt.initFrom(this);
    opt.text = QString();
    style()->drawControl(QStyle::CE_CheckBox, &opt, &p, this);
    QRect cr = style()->subElementRect(QStyle::SE_CheckBoxContents, &opt, this);
    EmojiRenderer::instance().drawText(p, cr, text());
}

#endif

// ============ EmojiGroupBox ============

#ifdef QT3_BUILD

EmojiGroupBox::EmojiGroupBox(QWidget* parent, const char* name)
    : QGroupBox(parent, name) {}
EmojiGroupBox::EmojiGroupBox(const QString& title, QWidget* parent, const char* name)
    : QGroupBox(title, parent, name) {}

void EmojiGroupBox::paintEvent(QPaintEvent* event) {
    QGroupBox::paintEvent(event);
    QString t = title();
    if (!textHasEmoji(t)) return;
    QPainter p(this);
    p.setClipRect(event->rect());
    QFontMetrics fm = p.fontMetrics();
    int indent = 10;
    int w = rect().width();
    QRect tr(indent, 0, w - 2 * indent, fm.height());
    EmojiRenderer::instance().drawText(p, tr, t);
}

#else

EmojiGroupBox::EmojiGroupBox(QWidget* parent) : QGroupBox(parent) {}
EmojiGroupBox::EmojiGroupBox(const QString& title, QWidget* parent)
    : QGroupBox(title, parent) {}

void EmojiGroupBox::paintEvent(QPaintEvent* event) {
    QGroupBox::paintEvent(event);
    if (textHasEmoji(title())) {
        QPainter p(this);
        QFontMetrics fm = p.fontMetrics();
        QRect tr(10, 0, fm.width(title()), fm.height());
        EmojiRenderer::instance().drawText(p, tr, title());
    }
}

#endif

// ============ EmojiLineEdit ============

#ifdef QT3_BUILD

EmojiLineEdit::EmojiLineEdit(QWidget* parent, const char* name)
    : QLineEdit(parent, name) {}

void EmojiLineEdit::drawContents(QPainter* p) {
    QString t = text();
    if (!textHasEmoji(t)) { QLineEdit::drawContents(p); return; }
    EmojiRenderer::instance().drawText(*p, rect(), t);
}

#else

EmojiLineEdit::EmojiLineEdit(QWidget* parent) : QLineEdit(parent) {}

void EmojiLineEdit::paintEvent(QPaintEvent* event) {
    QLineEdit::paintEvent(event);
    if (textHasEmoji(text())) {
        QPainter p(this);
        p.setClipRect(event->rect());
        EmojiRenderer::instance().drawText(p, rect(), text());
    }
}

#endif

#endif // EMOJI_RENDER_QT34
