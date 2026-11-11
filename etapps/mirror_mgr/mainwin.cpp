#include "compat34.h"
#include "lambdaslot.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qgroupbox.h>
#include <qbuttongroup.h>
#ifdef QT3_BUILD
#include <qbutton.h>
#endif
#include <qlayout.h>

/* Qt3 QButtonGroup::insert / Qt4 addButton 差异封装 */
static void groupAdd(QButtonGroup* g, QWidget* b) {
#ifdef QT3_BUILD
    g->insert((QButton*)b);
#else
    g->addButton((QAbstractButton*)b);
#endif
}

extern "C"
QWidget* createMirrorMgrMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("镜像管理"));
#else
    w->setWindowTitle(qFromUtf8("镜像管理"));
#endif
    w->resize(480, 380);

    QVBoxLayout* mainLayout = (QVBoxLayout*)qNewBoxLayout(w, QBoxLayout::TopToBottom, 8, 6);

    QGroupBox* toolBox = new QGroupBox(qFromUtf8("工具"), w);
    QHBoxLayout* toolLayout = (QHBoxLayout*)qNewBoxLayout(toolBox, QBoxLayout::LeftToRight, 6, 4);
    QButtonGroup* toolGroup = new QButtonGroup(toolBox);
    QRadioButton* pipBtn = new QRadioButton(qFromUtf8("pip"), toolBox);
    QRadioButton* npmBtn = new QRadioButton(qFromUtf8("npm"), toolBox);
    QRadioButton* goBtn = new QRadioButton(qFromUtf8("go"), toolBox);
    groupAdd(toolGroup, pipBtn);
    groupAdd(toolGroup, npmBtn);
    groupAdd(toolGroup, goBtn);
    pipBtn->setChecked(true);
    toolLayout->addWidget(pipBtn);
    toolLayout->addWidget(npmBtn);
    toolLayout->addWidget(goBtn);
    toolLayout->addStretch(1);
    mainLayout->addWidget(toolBox);

    QGroupBox* mirrorBox = new QGroupBox(qFromUtf8("镜像源"), w);
    QVBoxLayout* mirrorLayout = (QVBoxLayout*)qNewBoxLayout(mirrorBox, QBoxLayout::TopToBottom, 6, 4);
    QButtonGroup* mirrorGroup = new QButtonGroup(mirrorBox);
    QRadioButton* officialBtn = new QRadioButton(qFromUtf8("官方源（默认）"), mirrorBox);
    QRadioButton* aliyunBtn = new QRadioButton(qFromUtf8("阿里云镜像"), mirrorBox);
    QRadioButton* tsinghuaBtn = new QRadioButton(qFromUtf8("清华大学镜像"), mirrorBox);
    QRadioButton* ustcBtn = new QRadioButton(qFromUtf8("中科大镜像"), mirrorBox);
    groupAdd(mirrorGroup, officialBtn);
    groupAdd(mirrorGroup, aliyunBtn);
    groupAdd(mirrorGroup, tsinghuaBtn);
    groupAdd(mirrorGroup, ustcBtn);
    officialBtn->setChecked(true);
    mirrorLayout->addWidget(officialBtn);
    mirrorLayout->addWidget(aliyunBtn);
    mirrorLayout->addWidget(tsinghuaBtn);
    mirrorLayout->addWidget(ustcBtn);
    mainLayout->addWidget(mirrorBox);

    QHBoxLayout* btnLayout = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 4, 4);
    QPushButton* applyBtn = new QPushButton(qFromUtf8("应用"), w);
    QPushButton* restoreBtn = new QPushButton(qFromUtf8("恢复默认"), w);
    btnLayout->addWidget(applyBtn);
    btnLayout->addWidget(restoreBtn);
    btnLayout->addStretch(1);
    mainLayout->addLayout(btnLayout);

    QLabel* statusLabel = new QLabel(qFromUtf8("状态：未修改"), w);
    mainLayout->addWidget(statusLabel);

    /* 主窗销毁日志：exe 与 qltox 插件双形态共用（destroyed→LambdaSlot） */
    LambdaSlot* logSlot = new LambdaSlot(0, [] {
        qWarning("[mainwin] destroyed: MirrorMgrMain");
    });
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(call()));
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(deleteLater()));

    return w;
}
