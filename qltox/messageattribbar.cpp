#include "messageattribbar.h"
#include "pureconsts.hpp"   // 类型常量 kImapMailType / kGomuksRoomType / ...
#include "lambdaslot.h"
#include <qframe.h>
#include <qclipboard.h>
#ifdef QT3_BUILD
#include <qevent.h>          // QIMEvent
#include <qapplication.h>    // QApplication::clipboard()
#else
#include <QInputMethodEvent>
#include <QApplication>      // QApplication::clipboard()
#endif

// ── kTags chip 标签编辑器(无 Q_OBJECT,规避 Qt3 moc) ──────────────────
static const QChar kCnComma = QChar(0xff0c);   // 中文逗号 ，
static const QChar kAsciiComma = QChar(0x2c);  // ,
static const QChar kNewlineChar = QChar(0x0a); // \n

// 按宽度省略(超长 tag 不撑爆布局;Qt3 无 elidedText,手动截断+…)
static QString elideTagText(const QString& s, int maxW, const QFont& f) {
#ifdef QT3_BUILD
    QFontMetrics fm(f);
    if (fm.width(s) <= maxW) return s;
    QString out = s;
    const QString ell = qFromUtf8("…");
    while (!out.isEmpty() && fm.width(out + ell) > maxW) {
        out.truncate(out.length() - 1);
    }
    return out + ell;
#else
    QFontMetrics fm(f);
    if (fm.width(s) <= maxW) return s;
    return fm.elidedText(s, Qt::ElideRight, maxW);
#endif
}

class TagInput;

class TagEditor : public QWidget {
public:
    TagEditor(MessageAttribBar* owner, QWidget* parent);
    void setTags(const QStringList& tags);
    bool addTag(const QString& t);          // trim+去重保序;true=成功添加
    void removeTag(const QString& t);
    QString text() const;                   // m_tags.join(",")
    void clearMark();
    void handleBackspaceEmpty();            // 两步删除:标记最后一个 / 删除标记的
    int count() const { return m_tags.count(); }
protected:
    void resizeEvent(QResizeEvent* e);
    void showEvent(QShowEvent* e);
    void mousePressEvent(QMouseEvent* e);
private:
    QStringList m_tags;
    MessageAttribBar* m_owner;
#ifdef QT3_BUILD
    QValueList<QFrame*> m_chips;
#else
    QList<QFrame*> m_chips;
#endif
    TagInput* m_edit;
    int m_markIndex;
    QFrame* makeChip(const QString& t);
    void rebuildChips();
    void relayout();
    void setMarkIndex(int i);
    void updateMarkVisual();
};

class TagInput : public QLineEdit {
public:
    explicit TagInput(TagEditor* editor, QWidget* parent = 0);
protected:
    void keyPressEvent(QKeyEvent* e);
    void focusOutEvent(QFocusEvent* e);
    void insert(const QString& newText);
#ifdef QT3_BUILD
    void imStartEvent(QIMEvent* e);
    void imComposeEvent(QIMEvent* e);
    void imEndEvent(QIMEvent* e);
#else
    void inputMethodEvent(QInputMethodEvent* e);
#endif
private:
    TagEditor* m_e;
    bool m_composing;
    void commitWhole();   // Enter 提交整段(含分隔符也拆分)
};

TagEditor::TagEditor(MessageAttribBar* owner, QWidget* parent)
    : QWidget(parent), m_owner(owner), m_markIndex(-1) {
    m_edit = new TagInput(this, this);
    m_edit->setMaxLength(64);
    m_edit->setFixedHeight(m_edit->sizeHint().height());
    setFixedHeight(m_edit->sizeHint().height() + 6);
}

QFrame* TagEditor::makeChip(const QString& t) {
    QFrame* f = new QFrame(this);
    f->setFrameShape(QFrame::NoFrame);
    QHBoxLayout* hl = new QHBoxLayout(f);
    hl->setSpacing(2);
    qSetMargins(hl, 4, 1, 4, 1);
    QLabel* lb = new QLabel(elideTagText(t, 120, font()), f);
    QPushButton* xb = new QPushButton(qFromUtf8("×"), f);
    xb->setMinimumSize(16, 16);
    xb->setMaximumSize(18, 18);
    hl->addWidget(lb);
    hl->addWidget(xb);
    auto* slot = new LambdaSlot(xb, [this, t]() { removeTag(t); });
    connect(xb, SIGNAL(clicked()), slot, SLOT(call()));
    qSetToolTip(f, qFromUtf8("点击 × 删除该标签"));
    return f;
}

void TagEditor::rebuildChips() {
    for (int i = 0; i < m_chips.size(); ++i) { delete m_chips[i]; }
    m_chips.clear();
    for (int i = 0; i < m_tags.count(); ++i) { m_chips.append(makeChip(m_tags[i])); }
    m_markIndex = -1;
    updateMarkVisual();
}

void TagEditor::setTags(const QStringList& tags) {
    m_tags.clear();
    for (int i = 0; i < tags.count(); ++i) {
        QString s = qTrim(tags[i]);
        if (s.isEmpty()) { continue; }
        bool dup = false;
        for (int j = 0; j < m_tags.count(); ++j) {
            if (m_tags[j] == s) { dup = true; break; }
        }
        if (!dup) { m_tags.append(s); }
    }
    rebuildChips();
    relayout();
    m_owner->requestRelayout();
}

bool TagEditor::addTag(const QString& t) {
    QString s = qTrim(t);
    if (s.isEmpty()) { return false; }
    for (int i = 0; i < m_tags.count(); ++i) {
        if (m_tags[i] == s) { return false; }
    }
    m_tags.append(s);
    rebuildChips();
    relayout();
    m_owner->requestRelayout();
    return true;
}

void TagEditor::removeTag(const QString& t) {
#ifdef QT3_BUILD
    for (QStringList::Iterator it = m_tags.begin(); it != m_tags.end(); ++it) {
        if (*it == t) { m_tags.remove(it); break; }
    }
#else
    int idx = m_tags.indexOf(t);
    if (idx >= 0) { m_tags.removeAt(idx); }
#endif
    rebuildChips();
    relayout();
    m_owner->requestRelayout();
}

QString TagEditor::text() const {
    QString out;
    for (int i = 0; i < m_tags.count(); ++i) {
        if (i > 0) { out += kAsciiComma; }
        out += m_tags[i];
    }
    return out;
}

void TagEditor::clearMark() {
    if (m_markIndex < 0) { return; }
    setMarkIndex(-1);
}

void TagEditor::setMarkIndex(int i) {
    m_markIndex = i;
    updateMarkVisual();
}

void TagEditor::updateMarkVisual() {
    for (int i = 0; i < m_chips.size(); ++i) {
        QFrame* f = m_chips[i];
        if (i == m_markIndex) {
            f->setFrameShape(QFrame::StyledPanel);
            f->setFrameShadow(QFrame::Sunken);
        } else {
            f->setFrameShape(QFrame::NoFrame);
            f->setFrameShadow(QFrame::Plain);
        }
    }
}

void TagEditor::handleBackspaceEmpty() {
    if (m_tags.isEmpty()) { return; }
    if (m_markIndex < 0) {
        setMarkIndex(m_tags.count() - 1);
        return;
    }
    QString t = m_tags[m_markIndex];
    removeTag(t);
}

void TagEditor::relayout() {
    const int kW = (width() > 24) ? width() : 220;
    int x = 3, y = 3;
    int rowH = 0;
    for (int i = 0; i < m_chips.size(); ++i) {
        QFrame* f = m_chips[i];
        int fw = f->sizeHint().width() + 6;
        if (x > 3 && x + fw > kW) {
            x = 3;
            y += rowH + 3;
            rowH = 0;
        }
        int fh = f->sizeHint().height();
        if (fh > rowH) { rowH = fh; }
        f->move(x, y + (rowH - fh) / 2);
        x += fw;
    }
    const int kEditMinW = 90;
    const int eh = m_edit->sizeHint().height();
    if (eh > rowH) { rowH = eh; }
    int ew = kW - 6 - x;
    if (ew < kEditMinW) {
        x = 3;
        y += rowH + 3;
        rowH = eh;
        ew = kW - 6;
    }
    m_edit->move(x, y + (rowH - eh) / 2);
    m_edit->setFixedWidth(ew);
    m_edit->setFixedHeight(eh);
    setFixedHeight(y + rowH + 3);
}

void TagEditor::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    relayout();
}

void TagEditor::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    relayout();
}

void TagEditor::mousePressEvent(QMouseEvent* e) {
    m_edit->setFocus();
    QWidget::mousePressEvent(e);
}

TagInput::TagInput(TagEditor* editor, QWidget* parent)
    : QLineEdit(parent), m_e(editor), m_composing(false) {
}

void TagInput::commitWhole() {
    if (m_composing) { return; }
    QString raw = qTrim(text());
    if (raw.isEmpty()) { return; }
    bool hasDelim = raw.contains(kAsciiComma) || raw.contains(kCnComma)
                    || raw.contains(kNewlineChar);
    if (!hasDelim) {
        if (m_e->addTag(raw)) { clear(); }
        return;
    }
    // 含分隔符:整段拆分后全部提交并清空
    QString cur;
    for (int k = 0; k < raw.length(); ++k) {
        QChar c = raw[k];
        if (c == kAsciiComma || c == kCnComma || c == kNewlineChar) {
            m_e->addTag(cur);
            cur.truncate(0);
        } else {
            cur += c;
        }
    }
    if (!qTrim(cur).isEmpty()) { m_e->addTag(cur); }
    clear();
}

void TagInput::keyPressEvent(QKeyEvent* e) {
    const int key = e->key();
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        if (!m_composing) {
            commitWhole();
            e->accept();
        } else {
            QLineEdit::keyPressEvent(e);   // 组合期:放行给输入法
        }
        return;
    }
    if (key == Qt::Key_Backspace && text().isEmpty()) {
        if (!m_composing) {
            m_e->handleBackspaceEmpty();
            e->accept();
        } else {
            QLineEdit::keyPressEvent(e);
        }
        return;
    }
    if (key == Qt::Key_V) {
        bool ctrl = false;
#ifdef QT3_BUILD
        ctrl = (e->state() & ControlButton);
#else
        ctrl = (e->modifiers() & Qt::ControlModifier);
#endif
        if (ctrl) {
            if (!m_composing) {
                QString clip = QApplication::clipboard()->text();
                if (!clip.isEmpty()) { insert(clip); }
            } else {
                QLineEdit::keyPressEvent(e);
                return;
            }
            e->accept();
            return;
        }
    }
    m_e->clearMark();
    QLineEdit::keyPressEvent(e);
}

void TagInput::insert(const QString& newText) {
    if (m_composing || newText.isEmpty()) {
        QLineEdit::insert(newText);
        return;
    }
    bool hasDelim = newText.contains(kAsciiComma) || newText.contains(kCnComma)
                    || newText.contains(kNewlineChar);
    if (!hasDelim) {
        QLineEdit::insert(newText);
        return;
    }
    // 拆分:除末段外全部提交,末段保留为草稿(若以分隔符结尾则清空)
    QString acc = text() + newText;
    QStringList all;
    QString cur;
    for (int k = 0; k < acc.length(); ++k) {
        QChar c = acc[k];
        if (c == kAsciiComma || c == kCnComma || c == kNewlineChar) {
            all.append(cur);
            cur.truncate(0);
        } else {
            cur += c;
        }
    }
    all.append(cur);
    int n = all.count();
    for (int i = 0; i < n - 1; ++i) { m_e->addTag(all[i]); }
    QString tail = (n > 0) ? all[n - 1] : QString();
    if (qTrim(tail).isEmpty()) {
        clear();
    } else {
        setText(tail);
        setCursorPosition(tail.length());
    }
}

void TagInput::focusOutEvent(QFocusEvent* e) {
    QLineEdit::focusOutEvent(e);
    m_e->clearMark();
    if (!m_composing) {
        QString raw = text();
        if (!qTrim(raw).isEmpty()) { commitWhole(); }
    }
}

#ifdef QT3_BUILD
void TagInput::imStartEvent(QIMEvent* e) {
    m_composing = true;
    QWidget::imStartEvent(e);
}
void TagInput::imComposeEvent(QIMEvent* e) {
    m_composing = true;
    QWidget::imComposeEvent(e);
}
void TagInput::imEndEvent(QIMEvent* e) {
    m_composing = false;
    QWidget::imEndEvent(e);
}
#else
void TagInput::inputMethodEvent(QInputMethodEvent* e) {
    m_composing = !e->preeditString().isEmpty();
    if (!e->commitString().isEmpty()) { m_composing = false; }
    QLineEdit::inputMethodEvent(e);
}
#endif

static QString mapValue(const QString& key, const QMap<QString,QString>& m,
                        const QString& defValue) {
#ifdef QT3_BUILD
    QMap<QString,QString>::ConstIterator it = m.find(key);
    if (it != m.end()) { return it.data(); }
    return defValue;
#else
    return m.value(key, defValue);
#endif
}

void MessageAttribBar::layoutItems() {
    if (m_inLayout) return;
    if (m_items.size() == 0) { setFixedHeight(0); hide(); return; }
    if (!isVisible()) return;   // 隐藏期 width() 是布局前残留值,用它换行会导致首次显示时先乱排版后收缩;showEvent/resizeEvent 拿到真实宽度会重排
    m_inLayout = true;
    const int kWrap = width() > 24 ? width() : 24;
    int x = 4, y = 2;
    int rowH = 0;
    if (m_typeLabel && !m_typeLabel->isHidden()) {
        int lh = m_typeLabel->height();
        if (lh <= 0) {
            lh = m_typeLabel->sizeHint().height();
            if (lh < 16) { lh = 16; }
        }
        if (lh > rowH) { rowH = lh; }
        m_typeLabel->setGeometry(x, y + (rowH - lh) / 2, m_typeLabel->width(), lh);
        x += m_typeLabel->width() + 8;
    }
    for (int i = 0; i < m_items.size(); ++i) {
        const Item& it = m_items[i];
        int total = it.label->width() + 4 + it.control->width();
        if (x > 4 && x + total > kWrap) {
            x = 4;
            y += rowH + 4;
            rowH = 0;
        }
        int lh = it.label->height();
        if (lh <= 0) {
            lh = it.label->sizeHint().height();
            if (lh < 16) { lh = 16; }
        }
        int ch = it.control->height();
        if (ch <= 0) {
            ch = it.control->sizeHint().height();
            if (ch < 16) { ch = 16; }
        }
        if (lh > rowH) { rowH = lh; }
        if (ch > rowH) { rowH = ch; }
        it.label->setGeometry(x, y + (rowH - lh) / 2, it.label->width(), lh);
        it.control->setGeometry(x + it.label->width() + 4, y + (rowH - ch) / 2, it.control->width(), ch);
        x += total + 8;
    }
    setFixedHeight(y + rowH + 2);
    m_inLayout = false;
}

void MessageAttribBar::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    layoutItems();
}

void MessageAttribBar::requestRelayout() {
    layoutItems();
}

void MessageAttribBar::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    layoutItems();
}

void MessageAttribBar::rebuildChildren() {
    for (int i = 0; i < m_items.size(); ++i) {
        delete m_items[i].label;
        delete m_items[i].control;
    }
    m_items.clear();
    for (int i = 0; i < m_defs.size(); ++i) {
        const MessageAttrDef& d = m_defs[i];
        Item it;
        it.kind = (int)d.kind;
        it.label = new QLabel(d.label, this);
        it.label->setFixedWidth(QFontMetrics(it.label->font()).width(d.label));
        it.label->setFixedHeight(QFontMetrics(it.label->font()).height() + 4);
        QWidget* ctl = 0;
        switch (d.kind) {
        case MessageAttrDef::kCombo: {
            QComboBox* cb = new QComboBox(this);
            for (int j = 0; j < d.options.count(); ++j) {
#ifdef QT3_BUILD
                cb->insertItem(d.options[j]);
#else
                cb->addItem(d.options[j]);
#endif
            }
            cb->setFixedWidth(120);
            ctl = cb;
            break;
        }
        case MessageAttrDef::kCheck: {
            QCheckBox* ck = new QCheckBox(this);
            qSetChecked(ck, false);
            int cw = ck->sizeHint().width();
            if (cw < 24) { cw = 24; }
            ck->setFixedWidth(cw);
            ctl = ck;
            break;
        }
        case MessageAttrDef::kSpin: {
            QSpinBox* sp = new QSpinBox(this);
#ifdef QT3_BUILD
            sp->setMinValue(d.spinMin);
            sp->setMaxValue(d.spinMax);
            sp->setLineStep(d.spinStep);
#else
            sp->setRange(d.spinMin, d.spinMax);
            sp->setSingleStep(d.spinStep);
#endif
            sp->setValue(d.defValue.toInt());
            sp->setFixedWidth(90);
            ctl = sp;
            break;
        }
        case MessageAttrDef::kLine: {
            QLineEdit* le = new QLineEdit(this);
            le->setFixedWidth(140);
            ctl = le;
            break;
        }
        case MessageAttrDef::kTags: {
            TagEditor* te = new TagEditor(this, this);
            te->setFixedWidth(220);
            ctl = te;
            break;
        }
        default:
            ctl = new QWidget(this);
            ctl->setFixedWidth(20);
            break;
        }
        if (d.kind != MessageAttrDef::kTags) {
            int ch = ctl->sizeHint().height();
            if (ch < 16) { ch = 16; }
            ctl->setFixedHeight(ch);
        }
        it.control = ctl;
        if (!d.hint.isEmpty()) {
            qSetToolTip(it.label, d.hint);
            qSetToolTip(it.control, d.hint);
        }
        it.label->show();
        it.control->show();
        m_items.append(it);
    }
}

void MessageAttribBar::setDefs(const MessageAttrDefList& defs) {
    m_defs = defs;
    rebuildChildren();
    for (int i = 0; i < m_items.size(); ++i) {
        setItemValue(i, mapValue(m_defs[i].key, m_values, m_defs[i].defValue));
    }
    show();
    layoutItems();
}

void MessageAttribBar::setValues(const QMap<QString,QString>& values) {
    m_values = values;
#ifdef QT3_BUILD
    {
        QMap<QString,QString>::ConstIterator it = values.find(qFromUtf8("proto_type"));
        if (it != values.end() && !it.data().isEmpty()) { setTypeLabel(it.data()); }
    }
#else
    {
        QString pt = values.value(qFromUtf8("proto_type"));
        if (!pt.isEmpty()) { setTypeLabel(pt); }
    }
#endif
    for (int i = 0; i < m_items.size(); ++i) {
        setItemValue(i, mapValue(m_defs[i].key, m_values, m_defs[i].defValue));
    }
}

void MessageAttribBar::setItemValue(int i, const QString& v) {
    const MessageAttrDef& d = m_defs[i];
    switch (d.kind) {
    case MessageAttrDef::kCombo: {
        QComboBox* cb = static_cast<QComboBox*>(m_items[i].control);
        int idx = -1;
        for (int j = 0; j < d.options.count(); ++j) {
            if (d.options[j] == v) { idx = j; break; }
        }
        if (idx < 0) {
            for (int j = 0; j < d.options.count(); ++j) {
                if (d.options[j] == d.defValue) { idx = j; break; }
            }
        }
        if (idx < 0) { idx = 0; }
#ifdef QT3_BUILD
        cb->setCurrentItem(idx);
#else
        cb->setCurrentIndex(idx);
#endif
        break;
    }
    case MessageAttrDef::kCheck:
        qSetChecked(static_cast<QCheckBox*>(m_items[i].control),
                    (v == "1" || v == "true"));
        break;
    case MessageAttrDef::kSpin:
        static_cast<QSpinBox*>(m_items[i].control)->setValue(v.toInt());
        break;
    case MessageAttrDef::kLine:
        static_cast<QLineEdit*>(m_items[i].control)->setText(v);
        break;
    case MessageAttrDef::kTags: {
        QStringList saved = qSplit(v, qFromUtf8(","));
        static_cast<TagEditor*>(m_items[i].control)->setTags(saved);
        break;
    }
    default:
        break;
    }
}

QString MessageAttribBar::itemValue(int i) const {
    const MessageAttrDef& d = m_defs[i];
    switch (d.kind) {
    case MessageAttrDef::kCombo: {
        QComboBox* cb = static_cast<QComboBox*>(m_items[i].control);
#ifdef QT3_BUILD
        int idx = cb->currentItem();
#else
        int idx = cb->currentIndex();
#endif
        if (idx >= 0 && idx < d.options.count()) { return d.options[idx]; }
        return QString();
    }
    case MessageAttrDef::kCheck:
        return static_cast<QCheckBox*>(m_items[i].control)->isChecked()
                ? QString::fromLatin1("1") : QString::fromLatin1("0");
    case MessageAttrDef::kSpin:
        return QString::number(static_cast<QSpinBox*>(m_items[i].control)->value());
    case MessageAttrDef::kLine:
        return static_cast<QLineEdit*>(m_items[i].control)->text();
    case MessageAttrDef::kTags:
        return static_cast<TagEditor*>(m_items[i].control)->text();
    }
    return QString();
}

QMap<QString,QString> MessageAttribBar::values() const {
    QMap<QString,QString> out;
    if (!m_typeLabel->text().isEmpty()) {
        out.insert(qFromUtf8("proto_type"), m_typeLabel->text());
    }
    for (int i = 0; i < m_items.size(); ++i) {
        QString v = itemValue(i);
        if (m_defs[i].kind == MessageAttrDef::kCheck) {
            out.insert(m_defs[i].key, v);
        } else if (!v.isEmpty()) {
            out.insert(m_defs[i].key, v);
        }
    }
    return out;
}

void MessageAttribBar::clear() {
    for (int i = 0; i < m_items.size(); ++i) {
        delete m_items[i].label;
        delete m_items[i].control;
    }
    m_items.clear();
    m_defs.clear();
    m_typeLabel->setText(QString());
    m_typeLabel->hide();
    setFixedHeight(0);
    hide();
}

MessageAttribBar::MessageAttribBar(QWidget* parent)
    : QWidget(parent), m_inLayout(false) {
    m_typeLabel = new QLabel(this);
    m_typeLabel->hide();
    setFixedHeight(0);
    hide();
}

void MessageAttribBar::setTypeLabel(const QString& text) {
    if (text.isEmpty()) {
        m_typeLabel->setText(QString());
        m_typeLabel->hide();
        layoutItems();
        return;
    }
    m_typeLabel->setText(text);
    m_typeLabel->setFixedWidth(QFontMetrics(m_typeLabel->font()).width(text) + 4);
    m_typeLabel->setFixedHeight(QFontMetrics(m_typeLabel->font()).height() + 4);
    m_typeLabel->show();
    layoutItems();
}

// ── 占位 schema：按联系人类型返回属性集(键名/选项后续按需调整) ──
static MessageAttrDef comboDef(const QString& key, const QString& label,
                               const QStringList& options, const QString& def) {
    MessageAttrDef d;
    d.key = key; d.label = label; d.kind = MessageAttrDef::kCombo;
    d.options = options; d.defValue = def;
    return d;
}
static MessageAttrDef checkDef(const QString& key, const QString& label) {
    MessageAttrDef d;
    d.key = key; d.label = label; d.kind = MessageAttrDef::kCheck;
    d.defValue = QString::fromLatin1("0");
    return d;
}
static MessageAttrDef lineDef(const QString& key, const QString& label, const QString& hint) {
    MessageAttrDef d;
    d.key = key; d.label = label; d.kind = MessageAttrDef::kLine; d.hint = hint;
    return d;
}
static MessageAttrDef tagsDef(const QString& key, const QString& label) {
    MessageAttrDef d;
    d.key = key; d.label = label; d.kind = MessageAttrDef::kTags;
    d.hint = qFromUtf8("标签1,标签2");
    return d;
}
static MessageAttrDef spinDef(const QString& key, const QString& label,
                              int mn, int mx, int step, int def) {
    MessageAttrDef d;
    d.key = key; d.label = label; d.kind = MessageAttrDef::kSpin;
    d.spinMin = mn; d.spinMax = mx; d.spinStep = step;
    d.defValue = QString::number(def);
    return d;
}

MessageAttrDefList messageAttribBarDefsForType(const QString& type) {
    MessageAttrDefList defs;
    QStringList pri;  pri << "low" << "normal" << "high";
    QStringList pre;  pre << "normal" << "away" << "busy";
    QStringList sm;   sm  << "auto" << "ptt" << "muted";
    QStringList lv;   lv  << "info" << "warning" << "error";
    if (type == "friend") {
        defs << comboDef("presence", qFromUtf8("状态"), pre, "normal");
        defs << lineDef("note", qFromUtf8("备注"), qFromUtf8("给该好友的备注"));
        defs << tagsDef("tags", qFromUtf8("标签"));
    } else if (type == "group") {
        defs << comboDef("priority", qFromUtf8("优先级"), pri, "normal");
        defs << checkDef("sticky", qFromUtf8("置顶"));
        defs << lineDef("subject", qFromUtf8("主题"), QString());
        defs << tagsDef("tags", qFromUtf8("标签"));
    } else if (type == "conference") {
        defs << comboDef("speak_mode", qFromUtf8("发言"), sm, "auto");
        defs << checkDef("record", qFromUtf8("录音"));
        defs << tagsDef("tags", qFromUtf8("标签"));
    } else if (type == kUnktoxFriendType) {
        defs << comboDef("priority", qFromUtf8("优先级"), pri, "normal");
        defs << lineDef("note", qFromUtf8("备注"), qFromUtf8("未知协议好友备注"));
        defs << tagsDef("tags", qFromUtf8("标签"));
    } else if (type == kUnktoxConferenceType) {
        defs << comboDef("speak_mode", qFromUtf8("发言"), sm, "auto");
        defs << checkDef("record", qFromUtf8("录音"));
        defs << tagsDef("tags", qFromUtf8("标签"));
    } else if (type == kUnktoxGroupType) {
        defs << comboDef("priority", qFromUtf8("优先级"), pri, "normal");
        defs << checkDef("sticky", qFromUtf8("置顶"));
        defs << tagsDef("tags", qFromUtf8("标签"));
    } else if (type == kGomuksRoomType) {
        defs << checkDef("notify", qFromUtf8("提醒"));
        defs << comboDef("priority", qFromUtf8("优先级"), pri, "normal");
        defs << tagsDef("tags", qFromUtf8("标签"));
    } else if (type == kImapMailType) {
        defs << checkDef("mark_as_read", qFromUtf8("自动已读"));
        defs << comboDef("priority", qFromUtf8("优先级"), pri, "normal");
        defs << tagsDef("tags", qFromUtf8("标签"));
    } else if (type == kMisskeyType) {
        defs << comboDef("publicity", qFromUtf8("可见性"), pre, "normal");
        defs << lineDef("note", qFromUtf8("备注"), QString());
        defs << tagsDef("tags", qFromUtf8("标签"));
    } else if (type == kFilesyncType) {
        defs << lineDef("sync_dir", qFromUtf8("同步目录"), qFromUtf8("如 /data/sync"));
        defs << checkDef("overwrite", qFromUtf8("覆盖"));
    } else if (type == kClipboardType) {
        defs << checkDef("keep_history", qFromUtf8("保留历史"));
        defs << tagsDef("tags", qFromUtf8("标签"));
    } else if (type == kSyseventType) {
        defs << comboDef("level", qFromUtf8("级别"), lv, "info");
        defs << checkDef("sound", qFromUtf8("声音"));
    } else if (type == kTopicType) {
        defs << lineDef("subreddit", qFromUtf8("分区"), QString());
        defs << checkDef("pinned", qFromUtf8("置顶"));
    } else if (type == kToutiaoHotnewsType
               || type == kZhihuHotnewsType
               || type == kZhihuNotifyType
               || type == kBiliNotifyType
               || type == kWeiboHotnewsType
               || type == kXiaohongshuNotifyType
               || type == kCoolapkTimelineType) {
        defs << spinDef("auto_refresh", qFromUtf8("刷新间隔"), 30, 3600, 30, 300);
        defs << checkDef("only_video", qFromUtf8("仅视频"));
    } else if (type == kUnknownType) {
        defs << comboDef("priority", qFromUtf8("优先级"), pri, "normal");
        defs << lineDef("note", qFromUtf8("备注"), qFromUtf8("给该会话的备注"));
        defs << tagsDef("tags", qFromUtf8("标签"));
    }
    // kBookmarkType / kAichatType / kPastebinType / kTranslateType / 其余未注册类型 → 空(隐藏)
    return defs;
}