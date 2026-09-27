#include "mdtoolbar.h"
#include <qlayout.h>
#include <qdatetime.h>
#include <qcursor.h>

#ifdef QT3_BUILD
#include <qtextedit.h>
#include <qtooltip.h>
#else
#include <QTextEdit>
#endif

static void qComboAddItem(QComboBox* combo, const QString& text) {
#ifdef QT3_BUILD
    combo->insertItem(text);
#else
    combo->addItem(text);
#endif
}

static int qComboCurrentIndex(QComboBox* combo) {
#ifdef QT3_BUILD
    return combo->currentItem();
#else
    return combo->currentIndex();
#endif
}

static void qComboSetCurrentIndex(QComboBox* combo, int index) {
#ifdef QT3_BUILD
    combo->setCurrentItem(index);
#else
    combo->setCurrentIndex(index);
#endif
}

MdToolbar::MdToolbar(QWidget* parent)
    : QWidget(parent), m_editor(0) {
    QHBoxLayout* lay = new QHBoxLayout(this);
    qSetLayoutMargin(lay, 2);
    lay->setSpacing(2);

    lay->addWidget(makeBtn("undo", qFromUtf8("↩"), qFromUtf8("撤销")));
    lay->addWidget(makeBtn("redo", qFromUtf8("↪"), qFromUtf8("重做")));
    lay->addSpacing(8);

    EmojiPushButton* hBtn = makeBtn("heading", qFromUtf8("H▾"), qFromUtf8("标题"));
    lay->addWidget(hBtn);

#ifdef QT3_BUILD
    m_headingMenu = new QPopupMenu(hBtn);
    for (int i = 1; i <= 6; ++i) {
        m_headingMenu->insertItem(QString("H%1").arg(i), i - 1);
    }
#else
    m_headingMenu = new QMenu(hBtn);
    for (int i = 1; i <= 6; ++i) {
        m_headingMenu->addAction(QString("H%1").arg(i));
    }
#endif
    connect(hBtn, SIGNAL(clicked()), this, SLOT(onHeadingClicked()));

    lay->addWidget(makeBtn("bold", qFromUtf8("B"), qFromUtf8("粗体")));
    lay->addWidget(makeBtn("italic", qFromUtf8("I"), qFromUtf8("斜体")));
    lay->addWidget(makeBtn("underline", qFromUtf8("U"), qFromUtf8("下划线")));
    lay->addWidget(makeBtn("strike", qFromUtf8("S"), qFromUtf8("删除线")));
    lay->addWidget(makeBtn("code", qFromUtf8("⟨⟩"), qFromUtf8("行内代码")));
    lay->addWidget(makeBtn("codeblock", qFromUtf8("⌨"), qFromUtf8("代码块")));
    lay->addSpacing(8);

    lay->addWidget(makeBtn("quote", qFromUtf8("❝"), qFromUtf8("引用")));
    lay->addWidget(makeBtn("ul", qFromUtf8("•"), qFromUtf8("无序列表")));
    lay->addWidget(makeBtn("ol", qFromUtf8("1."), qFromUtf8("有序列表")));
    lay->addWidget(makeBtn("todo", qFromUtf8("☑"), qFromUtf8("任务列表")));
    lay->addWidget(makeBtn("hr", qFromUtf8("—"), qFromUtf8("水平线")));
    lay->addSpacing(8);

    lay->addWidget(makeBtn("link", qFromUtf8("🔗"), qFromUtf8("链接")));
    lay->addWidget(makeBtn("image", qFromUtf8("🖼"), qFromUtf8("图片")));
    lay->addWidget(makeBtn("table", qFromUtf8("⊞"), qFromUtf8("表格")));
    lay->addWidget(makeBtn("date", qFromUtf8("📅"), qFromUtf8("日期")));
    lay->addWidget(makeBtn("footnote", qFromUtf8("⌃"), qFromUtf8("脚注")));
    lay->addWidget(makeBtn("toc", qFromUtf8("≡"), qFromUtf8("目录")));
    lay->addSpacing(8);

    EmojiPushButton* previewBtn = makeBtn("preview", qFromUtf8("👁"), qFromUtf8("预览"));
    lay->addWidget(previewBtn);
    connect(previewBtn, SIGNAL(clicked()), this, SIGNAL(togglePreview()));

    lay->addStretch();

    QLabel* autoSaveLabel = new QLabel(qFromUtf8("⏱"), this);
    lay->addWidget(autoSaveLabel);

    m_autoSaveCombo = new QComboBox(this);
    qComboAddItem(m_autoSaveCombo, qFromUtf8("关闭"));
    for (int i = 1; i <= 10; ++i) {
        qComboAddItem(m_autoSaveCombo, qFromUtf8("%1 分钟").arg(i));
    }
    qComboSetCurrentIndex(m_autoSaveCombo, 0);
    m_autoSaveCombo->setFixedWidth(80);
    lay->addWidget(m_autoSaveCombo);
    connect(m_autoSaveCombo, SIGNAL(activated(int)), this, SLOT(onAutoSaveComboChanged(int)));
}

EmojiPushButton* MdToolbar::makeBtn(const QString& id, const QString& display, const QString& tip) {
    EmojiPushButton* btn = new EmojiPushButton(display, this);
    m_btnIds.insert(btn, id);
#ifdef QT3_BUILD
    QToolTip::add(btn, tip);
#else
    btn->setToolTip(tip);
#endif
    btn->setFixedSize(28, 26);
    btn->setFlat(true);
    connect(btn, SIGNAL(clicked()), this, SLOT(onButtonClicked()));
    return btn;
}

void MdToolbar::setEditor(QTextEdit* editor) {
    m_editor = editor;
}

void MdToolbar::onButtonClicked() {
    const QObject* obj = sender();
    if (!obj) {
        return;
    }
    QPushButton* btn = (QPushButton*)obj;
#ifdef QT3_BUILD
    QMap<QPushButton*, QString>::iterator it = m_btnIds.find(btn);
    QString id = (it != m_btnIds.end()) ? it.data() : QString();
#else
    QString id = m_btnIds.value(btn);
#endif

    if (id == "undo") {
        emit undoRequested();
    } else if (id == "redo") {
        emit redoRequested();
    } else if (id == "bold") {
        emit insertBold();
    } else if (id == "italic") {
        emit insertItalic();
    } else if (id == "underline") {
        emit insertUnderline();
    } else if (id == "strike") {
        emit insertStrikethrough();
    } else if (id == "code") {
        emit insertInlineCode();
    } else if (id == "codeblock") {
        emit insertCodeBlock();
    } else if (id == "quote") {
        emit insertQuote();
    } else if (id == "ul") {
        emit insertUl();
    } else if (id == "ol") {
        emit insertOl();
    } else if (id == "todo") {
        emit insertTodo();
    } else if (id == "hr") {
        emit insertHr();
    } else if (id == "link") {
        emit insertLink();
    } else if (id == "image") {
        emit insertImage();
    } else if (id == "table") {
        emit insertTable();
    } else if (id == "date") {
        onDateClicked();
    } else if (id == "footnote") {
        emit insertFootnote();
    } else if (id == "toc") {
        emit insertToc();
    }
}

void MdToolbar::onHeadingClicked() {
#ifdef QT3_BUILD
    int id = m_headingMenu->exec(QCursor::pos());
    if (id >= 0) {
        emit insertHeading(id + 1);
    }
#else
    QAction* act = m_headingMenu->exec(QCursor::pos());
    if (act) {
        QString text = act->text();
        int level = text.mid(1).toInt();
        if (level >= 1 && level <= 6) {
            emit insertHeading(level);
        }
    }
#endif
}

void MdToolbar::onDateClicked() {
    emit insertDate();
}

void MdToolbar::onAutoSaveComboChanged(int index) {
    emit autoSaveIntervalChanged(index);
}
