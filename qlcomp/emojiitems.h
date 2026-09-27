#ifndef EMOJITEMS_H
#define EMOJITEMS_H

#include "compat34.h"

#ifdef QT3_BUILD
#include <qlistbox.h>
#include <qlistview.h>
#include <qtable.h>
#endif

#ifdef QT3_BUILD

#ifdef EMOJI_RENDER_QT34

class EmojiListBoxText : public QListBoxText {
public:
    EmojiListBoxText(QListBox* listbox, const QString& text = QString())
        : QListBoxText(listbox, text) {}
    EmojiListBoxText(const QString& text = QString())
        : QListBoxText(text) {}
    void paint(QPainter* p);
};

class EmojiListViewItem : public QListViewItem {
public:
    EmojiListViewItem(QListView* parent) : QListViewItem(parent) {}
    EmojiListViewItem(QListViewItem* parent) : QListViewItem(parent) {}
    EmojiListViewItem(QListView* parent, QListViewItem* after)
        : QListViewItem(parent, after) {}
    void paintCell(QPainter* p, const QColorGroup& cg,
                  int column, int width, int alignment);
};

class EmojiTableItem : public QTableItem {
public:
    EmojiTableItem(QTable* table, EditType et, const QString& text)
        : QTableItem(table, et, text) {}
    void paint(QPainter* p, const QColorGroup& cg,
               const QRect& cr, bool selected);
};

#else // !EMOJI_RENDER_QT34

typedef QListBoxText EmojiListBoxText;
typedef QListViewItem EmojiListViewItem;
typedef QTableItem EmojiTableItem;

#endif // EMOJI_RENDER_QT34

#endif // QT3_BUILD

#endif // EMOJITEMS_H
