#ifndef RAWDATAPOPUP_H
#define RAWDATAPOPUP_H

#include "messagelist.h"

#include <QskPopup.h>
#include <QString>

class QskBox;
class QskLinearBox;
class QskTextLabel;
class QskPushButton;
class MyScrollArea;
class MultiLineTextEdit;

class RawDataPopup : public QskPopup
{
    Q_OBJECT
public:
    static void show(QQuickItem* parent, const MessageItem& item);

protected:
    void updateLayout() override;

private:
    RawDataPopup(const MessageItem& item, QQuickItem* parent);
    void updateGeometry();
    void refresh();

    MessageItem m_item;
    QString m_text;
    QskBox* m_panel = nullptr;
    QskLinearBox* m_layout = nullptr;
    QskTextLabel* m_titleLabel = nullptr;
    QskTextLabel* m_subLabel = nullptr;
    MyScrollArea* m_scroll = nullptr;
    MultiLineTextEdit* m_edit = nullptr;
    QskPushButton* m_copyButton = nullptr;
    QskPushButton* m_refreshButton = nullptr;
    QskPushButton* m_closeButton = nullptr;
};

#endif // RAWDATAPOPUP_H
