#ifndef MDHIGHLIGHTER_H
#define MDHIGHLIGHTER_H

#include "compat34.h"
#include <vector>

#ifdef QT3_BUILD
#include <qsyntaxhighlighter.h>
#include <qfont.h>
#include <qcolor.h>
#else
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#endif

#ifdef QT3_BUILD
class MdHighlighter : public QSyntaxHighlighter {
public:
    MdHighlighter(QTextEdit* parent = 0);
    int highlightParagraph(const QString& text, int);
private:
    struct Rule {
        Q36RegExp pattern;
        QFont font;
        QColor color;
        bool useFont;
    };
    std::vector<Rule> m_rules;
};
#else
class MdHighlighter : public QSyntaxHighlighter {
public:
    MdHighlighter(QTextEdit* parent = 0);
    void highlightBlock(const QString& text);
private:
    struct Rule {
        Q36RegExp pattern;
        QTextCharFormat format;
    };
    std::vector<Rule> m_rules;
};
#endif

#endif
