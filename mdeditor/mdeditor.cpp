#include "mdeditor.h"
#include "md_editor_icon.xpm"
#include <qlayout.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <qdatetime.h>
#include <qtimer.h>
#include <qmessagebox.h>
#ifdef QT3_BUILD
#include <qtextedit.h>
#else
#include <qtextdocument.h>
#include <qabstracttextdocumentlayout.h>
#include <QTextCursor>
#include <QScrollBar>
#include <QTextBlock>
#endif

MdEditor::MdEditor(QWidget* parent)
    : QWidget(parent
#ifdef QT3_BUILD
      , 0, WDestructiveClose
#endif
      ), m_previewVisible(true), m_modified(false),
      m_syncing(false) {
#ifndef QT3_BUILD
    setAttribute(Qt::WA_DeleteOnClose);
#endif
    qSetWindowTitle(this, qFromUtf8("Markdown 编辑器"));
    resize(1080, 630);

    {
        QPixmap pm(md_editor_icon);
#ifdef QT3_BUILD
        setIcon(pm);
#else
        setWindowIcon(QIcon(pm));
#endif
    }

    QVBoxLayout* mainLay = new QVBoxLayout(this);
    qSetLayoutMargin(mainLay, 4);
    mainLay->setSpacing(4);

    m_toolbar = new MdToolbar(this);
    mainLay->addWidget(m_toolbar, 0);

#ifdef QT3_BUILD
    m_splitter = new QSplitter(QSplitter::Horizontal, this);
#else
    m_splitter = new QSplitter(Qt::Horizontal, this);
#endif
    mainLay->addWidget(m_splitter, 1);

    m_editor = new QTextEdit(m_splitter);
#ifdef QT3_BUILD
    m_editor->setTextFormat(Qt::PlainText);
#else
    m_editor->setAcceptRichText(false);
#endif
    m_preview = new MdPreview(m_splitter);

#if 1  // 设为 0 关闭高亮，排查 crash
    m_highlighter = new MdHighlighter(m_editor);
#else
    m_highlighter = 0;
#endif
    m_toolbar->setEditor(m_editor);

    m_splitter->addWidget(m_editor);
    m_splitter->addWidget(m_preview);

    connect(m_toolbar, SIGNAL(undoRequested()), this, SLOT(onUndo()));
    connect(m_toolbar, SIGNAL(redoRequested()), this, SLOT(onRedo()));
    connect(m_toolbar, SIGNAL(insertHeading(int)), this, SLOT(onInsertHeading(int)));
    connect(m_toolbar, SIGNAL(insertBold()), this, SLOT(onInsertBold()));
    connect(m_toolbar, SIGNAL(insertItalic()), this, SLOT(onInsertItalic()));
    connect(m_toolbar, SIGNAL(insertUnderline()), this, SLOT(onInsertUnderline()));
    connect(m_toolbar, SIGNAL(insertStrikethrough()), this, SLOT(onInsertStrikethrough()));
    connect(m_toolbar, SIGNAL(insertInlineCode()), this, SLOT(onInsertInlineCode()));
    connect(m_toolbar, SIGNAL(insertCodeBlock()), this, SLOT(onInsertCodeBlock()));
    connect(m_toolbar, SIGNAL(insertQuote()), this, SLOT(onInsertQuote()));
    connect(m_toolbar, SIGNAL(insertUl()), this, SLOT(onInsertUl()));
    connect(m_toolbar, SIGNAL(insertOl()), this, SLOT(onInsertOl()));
    connect(m_toolbar, SIGNAL(insertTodo()), this, SLOT(onInsertTodo()));
    connect(m_toolbar, SIGNAL(insertLink()), this, SLOT(onInsertLink()));
    connect(m_toolbar, SIGNAL(insertImage()), this, SLOT(onInsertImage()));
    connect(m_toolbar, SIGNAL(insertTable()), this, SLOT(onInsertTable()));
    connect(m_toolbar, SIGNAL(insertHr()), this, SLOT(onInsertHr()));
    connect(m_toolbar, SIGNAL(insertDate()), this, SLOT(onInsertDate()));
    connect(m_toolbar, SIGNAL(insertFootnote()), this, SLOT(onInsertFootnote()));
    connect(m_toolbar, SIGNAL(insertToc()), this, SLOT(onInsertToc()));
    connect(m_toolbar, SIGNAL(togglePreview()), this, SLOT(onTogglePreview()));
    connect(m_editor, SIGNAL(textChanged()), this, SLOT(onTextChanged()));
    m_editor->installEventFilter(this);

#ifdef QT3_BUILD
    connect(m_editor, SIGNAL(contentsMoving(int, int)),
            this, SLOT(onEditorScrollChanged()));
#else
    connect(m_editor->verticalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(onEditorScrollChanged()));
#endif

    // 预览滚动条信号（双向同步）
    connect(m_preview->verticalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(onPreviewScrollChanged()));

    m_scrollSyncGuard = new QTimer(this);
#ifdef QT3_BUILD
    m_scrollSyncGuard->start(50, true);
    m_scrollSyncGuard->stop();
#else
    m_scrollSyncGuard->setSingleShot(true);
    m_scrollSyncGuard->setInterval(50);
#endif

    m_autoSaveTimer = new QTimer(this);
    connect(m_autoSaveTimer, SIGNAL(timeout()), this, SLOT(onAutoSaveTimeout()));
    connect(m_toolbar, SIGNAL(autoSaveIntervalChanged(int)), this, SLOT(onAutoSaveIntervalChanged(int)));
}

MdEditor::~MdEditor() {
    delete m_highlighter;
    m_highlighter = 0;
}

static QString mdRepeatChar(int count, char ch) {
    QString s;
    for (int i = 0; i < count; ++i) {
        s += ch;
    }
    return s;
}

void MdEditor::setMarkdown(const QString& markdown) {
    m_editor->blockSignals(true);
#ifdef QT3_BUILD
    m_editor->setText(markdown);
#else
    m_editor->setPlainText(markdown);
#endif
    m_editor->blockSignals(false);
    m_preview->setMarkdown(markdown);
}

QString MdEditor::toMarkdown() const {
#ifdef QT3_BUILD
    return m_editor->text();
#else
    return m_editor->toPlainText();
#endif
}

void MdEditor::loadSampleContent() {
    QString sample = qFromUtf8(
        "# Markdown 编辑器示例\n\n"
        "这是一段 **粗体** 文本，这是 *斜体*，这是 ~~删除线~~。\n\n"
        "## 功能列表\n\n"
        "- 实时预览\n"
        "- 语法高亮\n"
        "- 工具栏操作\n"
        "- 任务列表：\n"
        "  - [x] 粗体\n"
        "  - [x] 斜体\n"
        "  - [ ] 删除线\n\n"
        "## 代码示例\n\n"
        "```cpp\n"
        "int main() {\n"
        "    return 0;\n"
        "}\n"
        "```\n\n"
        "## 表格\n\n"
        "| 名称 | 版本 |\n"
        "|------|------|\n"
        "| Qt3  | 3.3.8 |\n"
        "| Qt4  | 4.8.7 |\n\n"
        "> 这是一段引用文本。\n\n"
        "---\n\n"
        "## 链接与图片\n\n"
        "[Qt 官网](https://www.qt.io)\n\n"
        "![图片](https://via.placeholder.com/150)\n\n"
        "## 脚注\n\n"
        "这是一段带有脚注的文本[^1]。\n\n"
        "[^1]: 这是脚注内容。\n\n"
        "示例结束。\n");
    setMarkdown(sample);
}

void MdEditor::closeEvent(QCloseEvent* e) {
    if (!promptIfModified()) {
        e->ignore();
        return;
    }
    // emit closed();
    m_editor->removeEventFilter(this);
    m_autoSaveTimer->stop();
    m_scrollSyncGuard->stop();
    QWidget::closeEvent(e);
    deleteLater(); // 不适用于close隐藏的情况!!!
}

void MdEditor::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        if (promptIfModified()) {
            m_modified = false;
            close();
        }
        return;
    }
#ifdef QT3_BUILD
    bool ctrl = (e->state() & Qt::ControlButton) != 0;
#else
    bool ctrl = (e->modifiers() & Qt::ControlModifier) != 0;
#endif
    if (ctrl && e->key() == Qt::Key_A) {
        m_editor->selectAll();
        return;
    }
    if (ctrl && e->key() == Qt::Key_S) {
        if (m_modified) {
            emit saveRequested();
            m_modified = false;
            qSetWindowTitle(this, qFromUtf8("Markdown 编辑器"));
        }
        return;
    }
    QWidget::keyPressEvent(e);
}

#ifdef QT3_BUILD

void MdEditor::wrapSelection(const QString& prefix, const QString& suffix) {
    QString sel = m_editor->selectedText();
    if (!sel.isEmpty()) {
        m_editor->insert(prefix + sel + suffix);
    } else {
        m_editor->insert(prefix + "text" + suffix);
    }
}

void MdEditor::insertAtCursor(const QString& text) {
    m_editor->insert(text);
}

void MdEditor::insertBlockPrefix(const QString& prefix) {
    m_editor->insert(prefix);
}

#else

void MdEditor::wrapSelection(const QString& prefix, const QString& suffix) {
    QTextCursor cursor = m_editor->textCursor();
    if (cursor.hasSelection()) {
        QString sel = cursor.selectedText();
        cursor.insertText(prefix + sel + suffix);
    } else {
        cursor.insertText(prefix + "text" + suffix);
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, suffix.length());
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 4);
        m_editor->setTextCursor(cursor);
    }
}

void MdEditor::insertAtCursor(const QString& text) {
    QTextCursor cursor = m_editor->textCursor();
    cursor.insertText(text);
    m_editor->setTextCursor(cursor);
}

void MdEditor::insertBlockPrefix(const QString& prefix) {
    QTextCursor cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.insertText(prefix);
    m_editor->setTextCursor(cursor);
}

#endif

void MdEditor::onUndo() {
    m_editor->undo();
}

void MdEditor::onRedo() {
    m_editor->redo();
}

void MdEditor::onInsertHeading(int level) {
    insertBlockPrefix(mdRepeatChar(level, '#') + " ");
}

void MdEditor::onInsertBold() {
    wrapSelection("**", "**");
}

void MdEditor::onInsertItalic() {
    wrapSelection("*", "*");
}

void MdEditor::onInsertUnderline() {
    wrapSelection("__", "__");
}

void MdEditor::onInsertStrikethrough() {
    wrapSelection("~~", "~~");
}

void MdEditor::onInsertInlineCode() {
    wrapSelection("`", "`");
}

void MdEditor::onInsertCodeBlock() {
    insertAtCursor("```\ncode\n```");
}

void MdEditor::onInsertQuote() {
    insertBlockPrefix("> ");
}

void MdEditor::onInsertUl() {
    insertBlockPrefix("- ");
}

void MdEditor::onInsertOl() {
    insertBlockPrefix("1. ");
}

void MdEditor::onInsertTodo() {
    insertBlockPrefix("- [ ] ");
}

void MdEditor::onInsertLink() {
    wrapSelection("[", "](url)");
}

void MdEditor::onInsertImage() {
    wrapSelection("![alt](", ")");
}

void MdEditor::onInsertTable() {
    insertAtCursor("| Header 1 | Header 2 | Header 3 |\n"
                   "|----------|----------|----------|\n"
                   "| Cell 1   | Cell 2   | Cell 3   |\n");
}

void MdEditor::onInsertHr() {
    insertAtCursor("\n---\n\n");
}

void MdEditor::onInsertDate() {
    QString date = QDate::currentDate().toString("yyyy-MM-dd");
    insertAtCursor(date);
}

void MdEditor::onInsertFootnote() {
    wrapSelection("[^", "]");
}

void MdEditor::onInsertToc() {
    insertAtCursor("[toc]\n\n");
}

void MdEditor::onTogglePreview() {
    m_previewVisible = !m_previewVisible;
    if (m_previewVisible) {
        m_preview->show();
    } else {
        m_preview->hide();
    }
}

void MdEditor::onTextChanged() {
    if (!m_modified) {
        m_modified = true;
        qSetWindowTitle(this, qFromUtf8("* Markdown 编辑器"));
    }
#ifdef QT3_BUILD
    m_preview->setMarkdown(m_editor->text());
#else
    m_preview->setMarkdown(m_editor->toPlainText());
#endif
    m_scrollMap.clear();
    QTimer::singleShot(0, this, SLOT(onEditorScrollChanged()));
}

void MdEditor::onAutoSaveIntervalChanged(int minutes) {
    if (minutes <= 0) {
        m_autoSaveTimer->stop();
    } else {
        m_autoSaveTimer->start(minutes * 60 * 1000);
    }
}

bool MdEditor::promptIfModified() {
    if (!m_modified) {
        return true;
    }
    int ret = QMessageBox::question(
        this, qFromUtf8("确认关闭"),
        qFromUtf8("文档已修改，是否放弃更改？"),
        QMessageBox::Yes, QMessageBox::No);
    return (ret == QMessageBox::Yes);
}

bool MdEditor::eventFilter(QObject* obj, QEvent* e) {
    if (obj == m_editor && e->type() == QEvent::KeyPress) {
        QKeyEvent* ke = (QKeyEvent*)e;
#ifdef QT3_BUILD
        bool ctrl = (ke->state() & Qt::ControlButton) != 0;
#else
        bool ctrl = (ke->modifiers() & Qt::ControlModifier) != 0;
#endif
        if (ke->key() == Qt::Key_Escape) {
            if (promptIfModified()) {
                m_modified = false;
                close();
            }
            return true;
        }
        if (ctrl && ke->key() == Qt::Key_A) {
            m_editor->selectAll();
            return true;
        }
        if (ctrl && ke->key() == Qt::Key_S) {
            if (m_modified) {
                emit saveRequested();
                m_modified = false;
                qSetWindowTitle(this, qFromUtf8("Markdown 编辑器"));
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, e);
}

void MdEditor::onAutoSaveTimeout() {
    if (m_modified) {
        emit saveRequested();
    }
}

void MdEditor::onEditorScrollChanged() {
    if (m_syncing || m_scrollSyncGuard->isActive()) {
        return;
    }
    m_syncing = true;

    // 重建 scrollmap（如果需要）
    if (m_scrollMap.size() == 0) {
        buildScrollMap();
    }

    QScrollBar* esb = m_editor->verticalScrollBar();
    QScrollBar* psb = m_preview->verticalScrollBar();

#ifdef QT3_BUILD
    int editorMax = esb->maxValue();
    int previewMax = psb->maxValue();
#else
    int editorMax = esb->maximum();
    int previewMax = psb->maximum();
#endif

    if (editorMax <= 0 || previewMax <= 0 || m_scrollMap.size() == 0) {
        m_syncing = false;
        return;
    }

    qreal editorVal = esb->value();
    qreal frac = 0.0;

    if (editorVal <= 0) {
        frac = 0.0;
    } else if (editorVal >= editorMax) {
        frac = 1.0;
    } else {
        int idx = findEntryByEditorY(editorVal);
        if (idx >= 0 && idx < (int)m_scrollMap.size() - 1) {
            const ScrollEntry& a = m_scrollMap[idx];
            const ScrollEntry& b = m_scrollMap[idx + 1];
            qreal editorSpan = b.editorY - a.editorY;
            if (editorSpan > 0) {
                qreal lineFrac = (editorVal - a.editorY) / editorSpan;
                if (lineFrac < 0.0) lineFrac = 0.0;
                if (lineFrac > 1.0) lineFrac = 1.0;
                qreal previewY = a.previewY + lineFrac * (b.previewY - a.previewY);
                if (previewMax > 0) {
                    frac = previewY / previewMax;
                }
            }
        } else {
            // 回退到简单百分比
            frac = (qreal)editorVal / (qreal)editorMax;
        }
    }

    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;

    int target = (int)(frac * previewMax + 0.5);
    if (target != psb->value()) {
        psb->setValue(target);
    }

    m_syncing = false;
#ifdef QT3_BUILD
    m_scrollSyncGuard->start(50, true);
#else
    m_scrollSyncGuard->start();
#endif
}

void MdEditor::onPreviewScrollChanged() {
    if (m_syncing || m_scrollSyncGuard->isActive()) {
        return;
    }
    m_syncing = true;

    // 重建 scrollmap（如果需要）
    if (m_scrollMap.size() == 0) {
        buildScrollMap();
    }

    QScrollBar* esb = m_editor->verticalScrollBar();
    QScrollBar* psb = m_preview->verticalScrollBar();

#ifdef QT3_BUILD
    int editorMax = esb->maxValue();
    int previewMax = psb->maxValue();
#else
    int editorMax = esb->maximum();
    int previewMax = psb->maximum();
#endif

    if (editorMax <= 0 || previewMax <= 0 || m_scrollMap.size() == 0) {
        m_syncing = false;
        return;
    }

    // 预览滚动 → 编辑器滚动（反向同步）
    qreal previewVal = psb->value();

    int bestIdx = findEntryByPreviewY(previewVal);
    if (bestIdx < 0) {
        m_syncing = false;
        return;
    }

    // 线性插值计算 editorY
    qreal targetEditorY = m_scrollMap[bestIdx].editorY;
    if (bestIdx < (int)m_scrollMap.size() - 1) {
        const ScrollEntry& a = m_scrollMap[bestIdx];
        const ScrollEntry& b = m_scrollMap[bestIdx + 1];
        qreal previewSpan = b.previewY - a.previewY;
        if (previewSpan > 0) {
            qreal lineFrac = (previewVal - a.previewY) / previewSpan;
            if (lineFrac < 0.0) lineFrac = 0.0;
            if (lineFrac > 1.0) lineFrac = 1.0;
            targetEditorY = a.editorY + lineFrac * (b.editorY - a.editorY);
        }
    }

    // 映射到编辑器滚动条
    qreal editorFrac = 0.0;
    if (editorMax > 0) {
        editorFrac = targetEditorY / editorMax;
    }
    if (editorFrac < 0.0) editorFrac = 0.0;
    if (editorFrac > 1.0) editorFrac = 1.0;

    int target = (int)(editorFrac * editorMax + 0.5);
    if (target != esb->value()) {
        esb->setValue(target);
    }

    m_syncing = false;
#ifdef QT3_BUILD
    m_scrollSyncGuard->start(50, true);
#else
    m_scrollSyncGuard->start();
#endif
}

void MdEditor::buildScrollMap() {
    m_scrollMap.clear();

#ifdef QT3_BUILD
    int editorCount = m_editor->paragraphs();
    int previewCount = m_preview->paragraphs();
    int maxCount = editorCount;
    if (previewCount > maxCount) {
        maxCount = previewCount;
    }

    for (int i = 0; i < maxCount; ++i) {
        ScrollEntry entry;
        entry.editorLine = i;
        entry.editorY = 0;
        entry.previewY = 0;

        if (i < editorCount) {
            QRect r = m_editor->paragraphRect(i);
            entry.editorY = r.y();
        } else {
            QRect r = m_editor->paragraphRect(editorCount - 1);
            entry.editorY = r.y() + r.height();
        }

        if (i < previewCount) {
            QRect r = m_preview->paragraphRect(i);
            entry.previewY = r.y();
        } else {
            QRect r = m_preview->paragraphRect(previewCount - 1);
            entry.previewY = r.y() + r.height();
        }

        m_scrollMap.push_back(entry);
    }
#else
    QTextDocument* editorDoc = m_editor->document();
    QTextDocument* previewDoc = m_preview->document();

    if (!editorDoc || !previewDoc) {
        return;
    }

    QAbstractTextDocumentLayout* editorLayout =
        qobject_cast<QAbstractTextDocumentLayout*>(editorDoc->documentLayout());
    QAbstractTextDocumentLayout* previewLayout =
        qobject_cast<QAbstractTextDocumentLayout*>(previewDoc->documentLayout());

    if (!editorLayout || !previewLayout) {
        return;
    }

    QTextBlock editorBlock = editorDoc->firstBlock();
    QTextBlock previewBlock = previewDoc->firstBlock();

    qreal editorY = 0;
    qreal previewY = 0;

    while (editorBlock.isValid() && previewBlock.isValid()) {
        qreal editorHeight = editorLayout->blockBoundingRect(editorBlock).height();
        qreal previewHeight = previewLayout->blockBoundingRect(previewBlock).height();

        ScrollEntry entry;
        entry.editorLine = editorBlock.blockNumber();
        entry.editorY = editorY;
        entry.previewY = previewY;
        m_scrollMap.append(entry);

        editorY += editorHeight;
        previewY += previewHeight;
        editorBlock = editorBlock.next();
        previewBlock = previewBlock.next();
    }

    if (previewBlock.isValid()) {
        ScrollEntry entry;
        entry.editorLine = editorDoc->blockCount() - 1;
        entry.editorY = editorY;
        entry.previewY = previewY;
        m_scrollMap.append(entry);
    }
#endif
}

int MdEditor::findEntryByEditorY(qreal y) const {
    if (m_scrollMap.empty()) {
        return -1;
    }

    int lo = 0;
    int hi = (int)m_scrollMap.size() - 1;

    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (m_scrollMap[mid].editorY <= y) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }

    return lo;
}

int MdEditor::findEntryByPreviewY(qreal y) const {
    if (m_scrollMap.empty()) {
        return -1;
    }

    int lo = 0;
    int hi = (int)m_scrollMap.size() - 1;

    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (m_scrollMap[mid].previewY <= y) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }

    return lo;
}
