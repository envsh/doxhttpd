#ifndef EMOJIWIDGETS_H
#define EMOJIWIDGETS_H

#include "compat34.h"
#include "emojiutil.h"

#ifdef QT3_BUILD
#include <qtoolbutton.h>
#include <qgroupbox.h>
#else
#include <QToolButton>
#include <QGroupBox>
#include <QPaintEvent>
#endif

class EmojiLabel : public QLabel {
    Q_OBJECT
public:
#ifdef QT3_BUILD
    EmojiLabel(QWidget* parent = 0, const char* name = 0);
    EmojiLabel(const QString& text, QWidget* parent = 0, const char* name = 0);
#else
    EmojiLabel(QWidget* parent = 0, Qt::WindowFlags f = 0);
    EmojiLabel(const QString& text, QWidget* parent = 0, Qt::WindowFlags f = 0);
#endif
protected:
#ifdef QT3_BUILD
    void drawContents(QPainter* p);
#else
    void paintEvent(QPaintEvent* event);
#endif
};

class EmojiPushButton : public QPushButton {
    Q_OBJECT
public:
#ifdef QT3_BUILD
    EmojiPushButton(QWidget* parent = 0, const char* name = 0);
    EmojiPushButton(const QString& text, QWidget* parent = 0, const char* name = 0);
#else
    EmojiPushButton(QWidget* parent = 0);
    EmojiPushButton(const QString& text, QWidget* parent = 0);
#endif
    void setText(const QString& text);
    QString text() const;
    void setEmojiInset(int px) { m_emojiInset = px > 0 ? px : 0; }
protected:
#ifdef QT3_BUILD
    void drawButtonLabel(QPainter* p);
#else
    void paintEvent(QPaintEvent* event);
#endif
private:
    QString m_emojiText;
    int m_emojiInset;
};

class EmojiToolButton : public QToolButton {
    Q_OBJECT
public:
#ifdef QT3_BUILD
    EmojiToolButton(QWidget* parent = 0, const char* name = 0);
#else
    EmojiToolButton(QWidget* parent = 0);
#endif
protected:
#ifdef QT3_BUILD
    void drawButtonLabel(QPainter* p);
#else
    void paintEvent(QPaintEvent* event);
#endif
};

class EmojiCheckBox : public QCheckBox {
    Q_OBJECT
public:
#ifdef QT3_BUILD
    EmojiCheckBox(QWidget* parent = 0, const char* name = 0);
    EmojiCheckBox(const QString& text, QWidget* parent = 0, const char* name = 0);
#else
    EmojiCheckBox(QWidget* parent = 0);
    EmojiCheckBox(const QString& text, QWidget* parent = 0);
#endif
protected:
#ifdef QT3_BUILD
    void drawButtonLabel(QPainter* p);
#else
    void paintEvent(QPaintEvent* event);
#endif
};

class EmojiGroupBox : public QGroupBox {
    Q_OBJECT
public:
#ifdef QT3_BUILD
    EmojiGroupBox(QWidget* parent = 0, const char* name = 0);
    EmojiGroupBox(const QString& title, QWidget* parent = 0, const char* name = 0);
#else
    EmojiGroupBox(QWidget* parent = 0);
    EmojiGroupBox(const QString& title, QWidget* parent = 0);
#endif
protected:
    void paintEvent(QPaintEvent* event);
};

class EmojiLineEdit : public QLineEdit {
    Q_OBJECT
public:
#ifdef QT3_BUILD
    EmojiLineEdit(QWidget* parent = 0, const char* name = 0);
#else
    EmojiLineEdit(QWidget* parent = 0);
#endif
protected:
#ifdef QT3_BUILD
    void drawContents(QPainter* p);
#else
    void paintEvent(QPaintEvent* event);
#endif
};

#endif
