#include "../DialogPopupAnimator.h"
#include "../lambdaslot.h"

#ifdef QT3_BUILD
#include <qapplication.h>
#include <qwidget.h>
#include <qdialog.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qlayout.h>
#else
#include <QApplication>
#include <QWidget>
#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#endif

static void openAnimatedDialog(QWidget* parent) {
#ifdef QT3_BUILD
    QDialog dlg(parent, "animated", true);
#else
    QDialog dlg(parent);
    dlg.setWindowTitle("Animated Dialog");
#endif
    dlg.resize(420, 260);

    QBoxLayout* layout = qNewBoxLayout(&dlg, QBoxLayout::TopToBottom, 16, 16);
    QLabel* label = new QLabel("Opened with scale + fade animation.", &dlg);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label, 1);
    QPushButton* closeBtn = new QPushButton("Close", &dlg);
    layout->addWidget(closeBtn, 0);
    QObject::connect(closeBtn, SIGNAL(clicked()), &dlg, SLOT(accept()));

    DialogPopupAnimator::install(&dlg);
    dlg.exec();
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QWidget win;
#ifdef QT3_BUILD
    win.setCaption("Dialog Popup Animation Demo");
    win.resize(300, 120);
    QBoxLayout* layout = qNewBoxLayout(&win, QBoxLayout::TopToBottom, 16, 16);
#else
    win.setWindowTitle("Dialog Popup Animation Demo");
    win.resize(300, 120);
    QVBoxLayout* layout = new QVBoxLayout(&win);
    layout->setContentsMargins(16, 16, 16, 16);
#endif

    QPushButton* openBtn = new QPushButton("Open Animated Dialog", &win);
    layout->addWidget(openBtn);

    LambdaSlot* slot = new LambdaSlot(openBtn, [&win]() { openAnimatedDialog(&win); });
    QObject::connect(openBtn, SIGNAL(clicked()), slot, SLOT(call()));

    win.show();
    return app.exec();
}
