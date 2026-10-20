#include "messageattribbar.h"
#include <QskPushButton.h>
#include <QskBox.h>
#include <QskBoxShapeMetrics.h>
#include <QskEvent.h>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QClipboard>
#include <QFontMetricsF>
#include <functional>

// ── kTags chip 标签编辑器(Qsk 版,手排 flow,无 Q_OBJECT) ─────────────────
// 行为对齐 qltox messageattribbar.cpp 的 TagEditor / TagInput

static const QChar kCnComma = QChar(0xff0c);   // 中文逗号 ，
static const QChar kAsciiComma = QChar(0x2c);  // ,
static const QChar kNewlineChar = QChar(0x0a); // \n

// 按宽度省略(超长 tag 不撑爆布局)
static QString elideTagText(const QString& s, int maxW, const QFont& f)
{
    QFontMetricsF fm(f);
    if (fm.horizontalAdvance(s) <= maxW) {
        return s;
    }
    return fm.elidedText(s, Qt::ElideRight, maxW);
}

class TagEditor;

// 标签输入框：对齐 qltox TagInput(Enter 拆分隔符提交 / 退格两步删除 /
// 粘贴拆分隔符 / 失焦提交 / IME 组合守卫)
class TagInput : public QskTextField
{
public:
    explicit TagInput(TagEditor* editor, QQuickItem* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;
    void inputMethodEvent(QInputMethodEvent* e) override;

private:
    TagEditor* m_e = nullptr;
    bool m_composing = false;
    void commitWhole();   // Enter 提交整段(含分隔符也拆分)
};

// 标签 chip 编辑器：手排 flow(chip = 圆角面板 + 文本 + 删除按钮) + 尾部输入框
class TagEditor : public QskControl
{
public:
    TagEditor(QQuickItem* parent = nullptr) : QskControl(parent)
    {
        setAutoLayoutChildren(false);

        m_edit = new TagInput(this, this);
        m_edit->setMaxLength(64);
        setFixedHeight(sizeHintHeight() + 6);
    }

    void setTags(const QStringList& tags);   // trim+去重保序;重建 chip
    bool addTag(const QString& t);           // true=成功添加
    void removeTag(const QString& t);
    QString text() const;                    // m_tags.join(",")
    void clearMark();
    void handleBackspaceEmpty();             // 两步删除:标记最后一个 / 删除标记的
    void relayout();                         // 对应 qltox TagEditor::relayout

    void setRelayoutCallback(const std::function<void()>& cb) { m_relayout = cb; }

protected:
    void updateLayout() override;
    void geometryChangeEvent(QskGeometryChangeEvent* e) override;

private:
    QStringList m_tags;
    QVector<QskBox*> m_chips;
    TagInput* m_edit = nullptr;
    int m_markIndex = -1;
    std::function<void()> m_relayout;

    int sizeHintHeight() const;
    QskBox* makeChip(const QString& t);
    void rebuildChips();
    void setMarkIndex(int i);
    void updateMarkVisual();
};

// ── TagInput 实现 ─────────────────────────────────────────────────────────

TagInput::TagInput(TagEditor* editor, QQuickItem* parent)
    : QskTextField(parent), m_e(editor)
{
    setPlaceholderText(QString::fromUtf8("标签1,标签2"));
}

void TagInput::commitWhole()
{
    if (m_composing) { return; }
    QString raw = text().trimmed();
    if (raw.isEmpty()) { return; }
    const bool hasDelim = raw.contains(kAsciiComma) || raw.contains(kCnComma)
                          || raw.contains(kNewlineChar);
    if (!hasDelim) {
        if (m_e->addTag(raw)) { setText(QString()); }
        return;
    }
    // 含分隔符:整段拆分后全部提交并清空
    QString cur;
    for (int k = 0; k < raw.length(); ++k) {
        const QChar c = raw[k];
        if (c == kAsciiComma || c == kCnComma || c == kNewlineChar) {
            m_e->addTag(cur);
            cur.truncate(0);
        } else {
            cur += c;
        }
    }
    if (!cur.trimmed().isEmpty()) { m_e->addTag(cur); }
    setText(QString());
}

void TagInput::keyPressEvent(QKeyEvent* e)
{
    const int key = e->key();
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        if (!m_composing) {
            commitWhole();
            e->accept();
        } else {
            QskTextField::keyPressEvent(e);   // 组合期:放行给输入法
        }
        return;
    }
    if (key == Qt::Key_Backspace && text().isEmpty()) {
        if (!m_composing) {
            m_e->handleBackspaceEmpty();
            e->accept();
        } else {
            QskTextField::keyPressEvent(e);
        }
        return;
    }
    if (key == Qt::Key_V && (e->modifiers() & Qt::ControlModifier)) {
        if (!m_composing) {
            const QString clip = QGuiApplication::clipboard()->text();
            if (!clip.isEmpty()) {
                // 对齐 qltox insert():除末段外全部提交,末段保留为草稿
                const bool hasDelim = clip.contains(kAsciiComma)
                        || clip.contains(kCnComma) || clip.contains(kNewlineChar);
                if (!hasDelim) {
                    const QString t = text() + clip;
                    setText(t);
                    setCursorPosition(t.length());
                } else {
                    const QString acc = text() + clip;
                    QStringList all;
                    QString cur;
                    for (int k = 0; k < acc.length(); ++k) {
                        const QChar c = acc[k];
                        if (c == kAsciiComma || c == kCnComma || c == kNewlineChar) {
                            all.append(cur);
                            cur.truncate(0);
                        } else {
                            cur += c;
                        }
                    }
                    all.append(cur);
                    const int n = all.size();
                    for (int i = 0; i < n - 1; ++i) { m_e->addTag(all[i]); }
                    const QString tail = (n > 0) ? all[n - 1] : QString();
                    if (tail.trimmed().isEmpty()) {
                        setText(QString());
                    } else {
                        setText(tail);
                        setCursorPosition(tail.length());
                    }
                }
            }
            e->accept();
            return;
        }
        QskTextField::keyPressEvent(e);
        return;
    }
    m_e->clearMark();
    QskTextField::keyPressEvent(e);
}

void TagInput::focusOutEvent(QFocusEvent* e)
{
    QskTextField::focusOutEvent(e);
    m_e->clearMark();
    if (!m_composing) {
        const QString raw = text();
        if (!raw.trimmed().isEmpty()) { commitWhole(); }
    }
}

void TagInput::inputMethodEvent(QInputMethodEvent* e)
{
    m_composing = !e->preeditString().isEmpty();
    if (!e->commitString().isEmpty()) { m_composing = false; }
    QskTextField::inputMethodEvent(e);
}

// ── TagEditor 实现 ────────────────────────────────────────────────────────

int TagEditor::sizeHintHeight() const
{
    if (m_edit) {
        return static_cast<int>(m_edit->sizeHint().height());
    }
    return 26;
}

QskBox* TagEditor::makeChip(const QString& t)
{
    auto* f = new QskBox(this);
    f->setAutoLayoutChildren(false);
    f->setPanel(true);

    auto* lb = new QskTextLabel(t, f);
    lb->setText(elideTagText(t, 120, lb->font()));
    const int textW = static_cast<int>(
        QFontMetricsF(lb->font()).horizontalAdvance(lb->text()));
    lb->setPreferredSize(textW + 8, sizeHintHeight());
    lb->setGeometry(4, 1, textW + 8, sizeHintHeight());

    auto* xb = new QskPushButton(QString::fromUtf8("×"), f);
    xb->setPreferredSize(16, sizeHintHeight());
    xb->setGeometry(4 + textW + 8, 1, 16, sizeHintHeight());
    connect(xb, &QskAbstractButton::clicked, this, [this, t]() {
        removeTag(t);
    });

    f->setPreferredSize(textW + 16 + 8, sizeHintHeight());
    return f;
}

void TagEditor::rebuildChips()
{
    for (auto* c : m_chips) {
        c->setParentItem(nullptr);
        c->deleteLater();
    }
    m_chips.clear();
    for (const QString& t : m_tags) {
        m_chips.append(makeChip(t));
    }
    m_markIndex = -1;
    updateMarkVisual();
}

void TagEditor::setTags(const QStringList& tags)
{
    m_tags.clear();
    for (const QString& raw : tags) {
        const QString s = raw.trimmed();
        if (s.isEmpty()) { continue; }
        if (!m_tags.contains(s)) { m_tags.append(s); }
    }
    rebuildChips();
    relayout();
    if (m_relayout) { m_relayout(); }
}

bool TagEditor::addTag(const QString& t)
{
    const QString s = t.trimmed();
    if (s.isEmpty()) { return false; }
    if (m_tags.contains(s)) { return false; }
    m_tags.append(s);
    rebuildChips();
    relayout();
    if (m_relayout) { m_relayout(); }
    return true;
}

void TagEditor::removeTag(const QString& t)
{
    const int idx = m_tags.indexOf(t);
    if (idx >= 0) { m_tags.removeAt(idx); }
    rebuildChips();
    relayout();
    if (m_relayout) { m_relayout(); }
}

QString TagEditor::text() const
{
    QString out;
    for (int i = 0; i < m_tags.size(); ++i) {
        if (i > 0) { out += kAsciiComma; }
        out += m_tags[i];
    }
    return out;
}

void TagEditor::clearMark()
{
    if (m_markIndex < 0) { return; }
    setMarkIndex(-1);
}

void TagEditor::setMarkIndex(int i)
{
    m_markIndex = i;
    updateMarkVisual();
}

void TagEditor::updateMarkVisual()
{
    // QskBox 无 FrameShape:以「是否显示面板底色」区分标记项
    // 标记项隐藏面板(去底色),其余常显面板
    for (int i = 0; i < m_chips.size(); ++i) {
        QskBox* f = m_chips[i];
        f->setPanel(i != m_markIndex);
    }
}

void TagEditor::handleBackspaceEmpty()
{
    if (m_tags.isEmpty()) { return; }
    if (m_markIndex < 0) {
        setMarkIndex(m_tags.size() - 1);
        return;
    }
    const QString t = m_tags[m_markIndex];
    removeTag(t);
}

void TagEditor::relayout()
{
    const int kW = (static_cast<int>(width()) > 24)
            ? static_cast<int>(width()) : 220;
    int x = 3;
    int y = 3;
    int rowH = 0;
    for (auto* f : m_chips) {
        const int fw = static_cast<int>(f->preferredSize().width()) + 6;
        if (x > 3 && x + fw > kW) {
            x = 3;
            y += rowH + 3;
            rowH = 0;
        }
        const int fh = static_cast<int>(f->preferredSize().height());
        if (fh > rowH) { rowH = fh; }
        f->setGeometry(x, y + (rowH - fh) / 2, fw, fh);
        x += fw;
    }
    const int kEditMinW = 90;
    const int eh = sizeHintHeight();
    if (eh > rowH) { rowH = eh; }
    int ew = kW - 6 - x;
    if (ew < kEditMinW) {
        x = 3;
        y += rowH + 3;
        rowH = eh;
        ew = kW - 6;
    }
    m_edit->setGeometry(x, y + (rowH - eh) / 2, ew, eh);
    setPreferredHeight(y + rowH + 3);
    setFixedHeight(y + rowH + 3);
}

void TagEditor::updateLayout()
{
    relayout();
    QskControl::updateLayout();
}

void TagEditor::geometryChangeEvent(QskGeometryChangeEvent* e)
{
    QskControl::geometryChangeEvent(e);
    relayout();
}

// ── MessageAttribBar ──────────────────────────────────────────────────────

MessageAttribBar::MessageAttribBar(QQuickItem* parent)
    : QskControl(parent)
{
    setAutoLayoutChildren(false);
    m_typeLabel = new QskTextLabel(this);
    m_typeLabel->setVisible(false);
    setVisible(false);
    setFixedHeight(0);
}

void MessageAttribBar::rebuildChildren()
{
    for (const Item& it : m_items) {
        if (it.label) {
            it.label->setParentItem(nullptr);
            it.label->deleteLater();
        }
        if (it.control) {
            it.control->setParentItem(nullptr);
            it.control->deleteLater();
        }
    }
    m_items.clear();

    for (const MessageAttrDef& d : m_defs) {
        Item it;
        it.label = new QskTextLabel(d.label, this);
        it.label->setPreferredHeight(24);

        QskControl* ctl = nullptr;
        switch (d.kind) {
        case MessageAttrDef::kCombo: {
            auto* cb = new QskComboBox(this);
            QStringList display;
            it.wireOptions = d.options;
            for (int j = 0; j < d.options.size(); ++j) {
                display << (d.displayOptions.size() == d.options.size()
                        ? d.displayOptions[j] : d.options[j]);
            }
            cb->setOptions(display);
            cb->setFixedWidth(120);
            cb->setPreferredWidth(120);
            ctl = cb;
            break;
        }
        case MessageAttrDef::kCheck: {
            auto* ck = new QskCheckBox(this);
            ck->setChecked(false);
            ck->setFixedWidth(40);
            ctl = ck;
            break;
        }
        case MessageAttrDef::kSpin: {
            auto* sp = new QskSpinBox(d.spinMin, d.spinMax, d.spinStep, this);
            sp->setValue(d.defValue.toDouble());
            sp->setFixedWidth(90);
            sp->setPreferredWidth(90);
            ctl = sp;
            break;
        }
        case MessageAttrDef::kLine: {
            auto* le = new QskTextField(this);
            le->setFixedWidth(140);
            le->setPreferredWidth(140);
            ctl = le;
            break;
        }
        case MessageAttrDef::kTags: {
            auto* te = new TagEditor(this);
            te->setPreferredWidth(220);
            te->setFixedWidth(220);
            te->setRelayoutCallback([this]() { layoutItems(); });
            ctl = static_cast<QskControl*>(te);
            break;
        }
        default: {
            ctl = new QskControl(this);
            ctl->setFixedWidth(20);
            break;
        }
        }
        if (d.kind != MessageAttrDef::kTags) {
            ctl->setFixedHeight(24);
        }
        it.control = ctl;
        m_items.append(it);
    }
}

void MessageAttribBar::layoutItems()
{
    if (m_inLayout) {
        return;
    }
    if (m_items.isEmpty()) {
        setFixedHeight(0);
        setVisible(false);
        return;
    }
    if (isVisible() && width() <= 0) {
        return;
    }
    m_inLayout = true;
    const int kWrap = static_cast<int>(width()) > 24
            ? static_cast<int>(width()) : 24;
    int x = 4;
    int y = 2;
    int rowH = 0;

    if (m_typeLabel && m_typeLabel->isVisible()) {
        const int lh = static_cast<int>(m_typeLabel->sizeHint().height());
        const int tw = static_cast<int>(QFontMetricsF(m_typeLabel->font())
                .horizontalAdvance(m_typeLabel->text())) + 4;
        if (lh > rowH) {
            rowH = lh;
        }
        m_typeLabel->setGeometry(x, y + (rowH - lh) / 2, tw, lh);
        x += tw + 8;
    }

    for (const Item& it : m_items) {
        const int labelW = static_cast<int>(QFontMetricsF(it.label->font())
                .horizontalAdvance(it.label->text())) + 4;
        const int ctlW = static_cast<int>(it.control->preferredSize().width());
        const int total = labelW + 4 + ctlW;
        if (x > 4 && x + total > kWrap) {
            x = 4;
            y += rowH + 4;
            rowH = 0;
        }
        const int lh = 24;
        const int ch = static_cast<int>(it.control->preferredSize().height());
        if (lh > rowH) {
            rowH = lh;
        }
        if (ch > rowH) {
            rowH = ch;
        }
        it.label->setGeometry(x, y + (rowH - lh) / 2, labelW, lh);
        it.control->setGeometry(x + labelW + 4, y + (rowH - ch) / 2, ctlW, ch);
        x += total + 8;
    }
    const int h = y + rowH + 2;
    setFixedHeight(h);
    setPreferredHeight(h);
    m_inLayout = false;
}

void MessageAttribBar::setDefs(const MessageAttrDefList& defs)
{
    m_defs = defs;
    rebuildChildren();
    for (int i = 0; i < m_items.size(); ++i) {
        setItemValue(i, m_values.value(m_defs[i].key, m_defs[i].defValue));
    }
    setVisible(!m_items.isEmpty());
    layoutItems();
}

void MessageAttribBar::setTypeLabel(const QString& text)
{
    if (text.isEmpty()) {
        m_typeLabel->setText(QString());
        m_typeLabel->setVisible(false);
    } else {
        m_typeLabel->setText(text);
        m_typeLabel->setVisible(true);
    }
    layoutItems();
}

void MessageAttribBar::setValues(const QMap<QString,QString>& values)
{
    m_values = values;
    const QString pt = values.value(QString::fromUtf8("proto_type"));
    if (!pt.isEmpty()) {
        setTypeLabel(pt);
    }
    for (int i = 0; i < m_items.size(); ++i) {
        setItemValue(i, m_values.value(m_defs[i].key, m_defs[i].defValue));
    }
}

void MessageAttribBar::setItemValue(int i, const QString& v)
{
    const MessageAttrDef& d = m_defs[i];
    switch (d.kind) {
    case MessageAttrDef::kCombo: {
        auto* cb = static_cast<QskComboBox*>(m_items[i].control);
        int idx = m_items[i].wireOptions.indexOf(v);
        if (idx < 0) {
            idx = m_items[i].wireOptions.indexOf(d.defValue);
        }
        if (idx < 0) {
            idx = 0;
        }
        cb->setCurrentIndex(idx);
        break;
    }
    case MessageAttrDef::kCheck:
        static_cast<QskCheckBox*>(m_items[i].control)
            ->setChecked(v == "1" || v == "true");
        break;
    case MessageAttrDef::kSpin:
        static_cast<QskSpinBox*>(m_items[i].control)->setValue(v.toDouble());
        break;
    case MessageAttrDef::kLine:
        static_cast<QskTextField*>(m_items[i].control)->setText(v);
        break;
    case MessageAttrDef::kTags:
        static_cast<TagEditor*>(m_items[i].control)->setTags(v.split(QString::fromUtf8(",")));
        break;
    default:
        break;
    }
}

QString MessageAttribBar::itemValue(int i) const
{
    const MessageAttrDef& d = m_defs[i];
    switch (d.kind) {
    case MessageAttrDef::kCombo: {
        auto* cb = static_cast<QskComboBox*>(m_items[i].control);
        const int idx = cb->currentIndex();
        if (idx >= 0 && idx < m_items[i].wireOptions.size()) {
            return m_items[i].wireOptions[idx];
        }
        return QString();
    }
    case MessageAttrDef::kCheck:
        return static_cast<QskCheckBox*>(m_items[i].control)->isChecked()
                ? QString::fromLatin1("1") : QString::fromLatin1("0");
    case MessageAttrDef::kSpin:
        return QString::number(
            static_cast<int>(static_cast<QskSpinBox*>(m_items[i].control)->value()));
    case MessageAttrDef::kLine:
        return static_cast<QskTextField*>(m_items[i].control)->text();
    case MessageAttrDef::kTags:
        return static_cast<TagEditor*>(m_items[i].control)->text();
    }
    return QString();
}

QMap<QString,QString> MessageAttribBar::values() const
{
    QMap<QString,QString> out;
    if (!m_typeLabel->text().isEmpty()) {
        out.insert(QString::fromUtf8("proto_type"), m_typeLabel->text());
    }
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_defs[i].key == QString::fromLatin1("new_chat")) {
            continue;   // 一次性动作项，不持久化
        }
        const QString v = itemValue(i);
        if (m_defs[i].kind == MessageAttrDef::kCheck) {
            out.insert(m_defs[i].key, v);
        } else if (!v.isEmpty()) {
            out.insert(m_defs[i].key, v);
        }
    }
    return out;
}

void MessageAttribBar::clear()
{
    for (const Item& it : m_items) {
        if (it.label) {
            it.label->setParentItem(nullptr);
            it.label->deleteLater();
        }
        if (it.control) {
            it.control->setParentItem(nullptr);
            it.control->deleteLater();
        }
    }
    m_items.clear();
    m_defs.clear();
    m_typeLabel->setText(QString());
    m_typeLabel->setVisible(false);
    setFixedHeight(0);
    setVisible(false);
}

void MessageAttribBar::requestRelayout()
{
    layoutItems();
}

void MessageAttribBar::updateLayout()
{
    layoutItems();
    QskControl::updateLayout();
}

void MessageAttribBar::geometryChangeEvent(QskGeometryChangeEvent* e)
{
    QskControl::geometryChangeEvent(e);
    layoutItems();
}
