#include "mdhighlighter.h"

#ifdef QT3_BUILD

MdHighlighter::MdHighlighter(QTextEdit* parent)
    : QSyntaxHighlighter(parent) {
    QFont boldFont;
    boldFont.setBold(true);
    m_rules.push_back({Q36RegExp("^#{1,6}\\s+.*"), boldFont, Qt::darkBlue, true});
    m_rules.push_back({Q36RegExp("\\*\\*[^*]+\\*\\*"), boldFont, QColor(), true});
    m_rules.push_back({Q36RegExp("__[^_]+__"), boldFont, QColor(), true});

    QFont italicFont;
    italicFont.setItalic(true);
    m_rules.push_back({Q36RegExp("\\*[^*]+\\*"), italicFont, QColor(), true});
    m_rules.push_back({Q36RegExp("_[^_]+_"), italicFont, QColor(), true});

    QFont codeFont;
    codeFont.setFamily("monospace");
    m_rules.push_back({Q36RegExp("`[^`]+`"), codeFont, QColor(), true});

    m_rules.push_back({Q36RegExp("\\[[^\\]]+\\]\\([^)]+\\)"), QFont(), Qt::darkMagenta, false});
    m_rules.push_back({Q36RegExp("!\\[[^\\]]*\\]\\([^)]+\\)"), QFont(), Qt::darkGreen, false});
    m_rules.push_back({Q36RegExp("^>.*"), QFont(), Qt::gray, false});
    m_rules.push_back({Q36RegExp("^(-{3,}|\\*{3,}|_{3,})$"), QFont(), Qt::gray, false});
    m_rules.push_back({Q36RegExp("^\\s*[-*+]\\s+"), QFont(), Qt::darkRed, false});
    m_rules.push_back({Q36RegExp("^\\s*\\d+\\.\\s+"), QFont(), Qt::darkRed, false});
    m_rules.push_back({Q36RegExp("^\\s*-\\s+\\[[ x]\\]\\s+"), QFont(), Qt::darkCyan, false});
    m_rules.push_back({Q36RegExp("#[\\w]+"), QFont(), Qt::darkYellow, false});
}

int MdHighlighter::highlightParagraph(const QString& text, int) {
    for (size_t i = 0; i < m_rules.size(); ++i) {
        Rule& rule = m_rules[i];
        int index = rule.pattern.search(text);
        while (index >= 0) {
            int length = rule.pattern.matchedLength();
            if (rule.useFont) {
                setFormat(index, length, rule.font, rule.color);
            } else if (rule.color.isValid()) {
                setFormat(index, length, rule.color);
            }
            index = rule.pattern.search(text, index + length);
        }
    }
    return 0;
}

#else

MdHighlighter::MdHighlighter(QTextEdit* parent)
    : QSyntaxHighlighter(parent) {
    QTextCharFormat headingFmt;
    headingFmt.setFontWeight(QFont::Bold);
    headingFmt.setForeground(Qt::darkBlue);
    m_rules.push_back({Q36RegExp("^#{1,6}\\s+.*"), headingFmt});

    QTextCharFormat boldFmt;
    boldFmt.setFontWeight(QFont::Bold);
    m_rules.push_back({Q36RegExp("\\*\\*[^*]+\\*\\*"), boldFmt});
    m_rules.push_back({Q36RegExp("__[^_]+__"), boldFmt});

    QTextCharFormat italicFmt;
    italicFmt.setFontItalic(true);
    m_rules.push_back({Q36RegExp("\\*[^*]+\\*"), italicFmt});
    m_rules.push_back({Q36RegExp("_[^_]+_"), italicFmt});

    QTextCharFormat codeFmt;
    codeFmt.setFontFamily("monospace");
    codeFmt.setBackground(QColor(240, 240, 240));
    m_rules.push_back({Q36RegExp("`[^`]+`"), codeFmt});

    QTextCharFormat linkFmt;
    linkFmt.setForeground(Qt::darkMagenta);
    m_rules.push_back({Q36RegExp("\\[[^\\]]+\\]\\([^)]+\\)"), linkFmt});

    QTextCharFormat imageFmt;
    imageFmt.setForeground(Qt::darkGreen);
    m_rules.push_back({Q36RegExp("!\\[[^\\]]*\\]\\([^)]+\\)"), imageFmt});

    QTextCharFormat commentFmt;
    commentFmt.setForeground(Qt::gray);
    commentFmt.setFontItalic(true);
    m_rules.push_back({Q36RegExp("^>.*"), commentFmt});

    QTextCharFormat hrFmt;
    hrFmt.setForeground(Qt::gray);
    m_rules.push_back({Q36RegExp("^(-{3,}|\\*{3,}|_{3,})$"), hrFmt});

    QTextCharFormat listFmt;
    listFmt.setForeground(Qt::darkRed);
    m_rules.push_back({Q36RegExp("^\\s*[-*+]\\s+"), listFmt});
    m_rules.push_back({Q36RegExp("^\\s*\\d+\\.\\s+"), listFmt});

    QTextCharFormat todoFmt;
    todoFmt.setForeground(Qt::darkCyan);
    m_rules.push_back({Q36RegExp("^\\s*-\\s+\\[[ x]\\]\\s+"), todoFmt});

    QTextCharFormat tagFmt;
    tagFmt.setForeground(Qt::darkYellow);
    m_rules.push_back({Q36RegExp("#[\\w]+"), tagFmt});
}

void MdHighlighter::highlightBlock(const QString& text) {
    for (size_t i = 0; i < m_rules.size(); ++i) {
        Rule& rule = m_rules[i];
        int index = rule.pattern.indexIn(text);
        while (index >= 0) {
            int length = rule.pattern.matchedLength();
            setFormat(index, length, rule.format);
            index = rule.pattern.indexIn(text, index + length);
        }
    }
}

#endif
