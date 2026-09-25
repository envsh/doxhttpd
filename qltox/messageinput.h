#ifndef MESSAGEINPUT_H
#define MESSAGEINPUT_H

#include "compat34.h"
#ifdef QT3_BUILD
#include <qtimer.h>
#else
#include <QTimer>
#endif

class MessageInput : public QTextEdit {
    Q_OBJECT
public:
    MessageInput(QWidget* parent = 0);

    void setPlaceholderText(const QString& t);
    QString placeholderText() const;
    void clearPlaceholder();
    void saveToHistory(const QString& text);

signals:
    void sendRequested();
    void filePasteRequested(const QString& filePath, const QString& caption);
    void rawUpClicked();
    void rawDownClicked();
    void ctrlUpClicked();
    void ctrlDownClicked();

private slots:
    void onClickTimeout();

protected:
    void keyPressEvent(QKeyEvent* e);
    void keyReleaseEvent(QKeyEvent* e);
    void paintEvent(QPaintEvent* e);
    void focusInEvent(QFocusEvent* e);
    void focusOutEvent(QFocusEvent* e);
    void dragEnterEvent(QDragEnterEvent* e);
    void dropEvent(QDropEvent* e);

#ifdef QT3_BUILD
    // Qt3 fork QTextEdit 是 QScrollView 子类，拖放走 viewport 的 contents* 虚函数
    // （QScrollView::viewportEvent → contentsDragEnterEvent 等），widget 级
    // dragEnterEvent/dropEvent 不会被调用（Qt4 才经 viewportEvent 转发到 widget 级）
    void contentsDragEnterEvent(QDragEnterEvent* e);
    void contentsDragMoveEvent(QDragMoveEvent* e);
    void contentsDropEvent(QDropEvent* e);
    bool handleMimeSource(QMimeSource* src, int srcMode);
#else
    void insertFromMimeData(const QMimeData* source);
    bool handleMimeData(const QMimeData* data, int srcMode);
#endif

private:
    enum { kKindRawUp = 0, kKindRawDown = 1, kKindCtrlUp = 2,
           kKindCtrlDown = 3, kKindNone = -1 };
    static int s_pasteCounter;
    void handleArrowKey(QKeyEvent* e, bool pressed);
    int arrowKind(QKeyEvent* e) const;
    void emitClickSignal(int kind);
    void cancelPendingClick();
    QTimer* m_clickTimer;
    int m_pendingClick;
    bool m_held[4];
    QString m_placeholder;
    QStringList m_sentHistory;
    int m_historyIndex;
    QString m_savedDraft;
};

#endif
