#include "compat34.h"
#include "lambdaslot.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qpushbutton.h>
#ifdef QT3_BUILD
#include <qlistbox.h>
#else
#include <qlistwidget.h>
/* Qt4 无 QListBox：用 QListWidget 包装同名 API */
class QListBox : public QListWidget {
public:
    explicit QListBox(QWidget* parent = 0) : QListWidget(parent) {}
    void insertItem(const QString& t) { addItem(t); }
    QString text(int i) const {
        QListWidgetItem* it = item(i);
        return it ? it->text() : QString();
    }
    int currentItem() const { return currentRow(); }
    void setCurrentItem(int i) { setCurrentRow(i); }
    void removeItem(int i) { QListWidgetItem* it = takeItem(i); delete it; }
};
#endif
#include <qgroupbox.h>
#include <qlayout.h>

extern "C"
QWidget* createBookmarkMgrMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("书签管理"));
#else
    w->setWindowTitle(qFromUtf8("书签管理"));
#endif
    w->resize(560, 420);

    QVBoxLayout* mainLayout = (QVBoxLayout*)qNewBoxLayout(w, QBoxLayout::TopToBottom, 8, 6);

    QGroupBox* listBox = new QGroupBox(qFromUtf8("书签列表"), w);
    QVBoxLayout* listLayout = (QVBoxLayout*)qNewBoxLayout(listBox, QBoxLayout::TopToBottom, 6, 4);
    QListBox* listBoxCtrl = new QListBox(listBox);
    listLayout->addWidget(listBoxCtrl);
    mainLayout->addWidget(listBox);

    QHBoxLayout* btnLayout = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 4, 4);
    QPushButton* addBtn = new QPushButton(qFromUtf8("添加"), w);
    QPushButton* delBtn = new QPushButton(qFromUtf8("删除"), w);
    QPushButton* openBtn = new QPushButton(qFromUtf8("打开"), w);
    QPushButton* refreshBtn = new QPushButton(qFromUtf8("刷新"), w);
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(delBtn);
    btnLayout->addWidget(openBtn);
    btnLayout->addWidget(refreshBtn);
    btnLayout->addStretch(1);
    mainLayout->addLayout(btnLayout);

    /* 主窗销毁日志：exe 与 qltox 插件双形态共用（destroyed→LambdaSlot） */
    LambdaSlot* logSlot = new LambdaSlot(0, [] {
        qWarning("[mainwin] destroyed: BookmarkMgrMain");
    });
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(call()));
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(deleteLater()));

    return w;
}
