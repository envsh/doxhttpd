#include "compat34.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qgroupbox.h>
#include <qbuttongroup.h>
#include <qlayout.h>

extern "C"
QWidget* createMirrorMgrMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("镜像管理"));
#else
    w->setWindowTitle(qFromUtf8("镜像管理"));
#endif
    w->resize(480, 380);

    QVBoxLayout* mainLayout = new QVBoxLayout(w, 8, 6);

    QGroupBox* toolBox = new QGroupBox(qFromUtf8("工具"), w);
    QHBoxLayout* toolLayout = new QHBoxLayout(toolBox, 6, 4);
    QButtonGroup* toolGroup = new QButtonGroup(toolBox);
    QRadioButton* pipBtn = new QRadioButton(qFromUtf8("pip"), toolBox);
    QRadioButton* npmBtn = new QRadioButton(qFromUtf8("npm"), toolBox);
    QRadioButton* goBtn = new QRadioButton(qFromUtf8("go"), toolBox);
    toolGroup->insert(pipBtn);
    toolGroup->insert(npmBtn);
    toolGroup->insert(goBtn);
    pipBtn->setChecked(true);
    toolLayout->addWidget(pipBtn);
    toolLayout->addWidget(npmBtn);
    toolLayout->addWidget(goBtn);
    toolLayout->addStretch(1);
    mainLayout->addWidget(toolBox);

    QGroupBox* mirrorBox = new QGroupBox(qFromUtf8("镜像源"), w);
    QVBoxLayout* mirrorLayout = new QVBoxLayout(mirrorBox, 6, 4);
    QButtonGroup* mirrorGroup = new QButtonGroup(mirrorBox);
    QRadioButton* officialBtn = new QRadioButton(qFromUtf8("官方源（默认）"), mirrorBox);
    QRadioButton* aliyunBtn = new QRadioButton(qFromUtf8("阿里云镜像"), mirrorBox);
    QRadioButton* tsinghuaBtn = new QRadioButton(qFromUtf8("清华大学镜像"), mirrorBox);
    QRadioButton* ustcBtn = new QRadioButton(qFromUtf8("中科大镜像"), mirrorBox);
    mirrorGroup->insert(officialBtn);
    mirrorGroup->insert(aliyunBtn);
    mirrorGroup->insert(tsinghuaBtn);
    mirrorGroup->insert(ustcBtn);
    officialBtn->setChecked(true);
    mirrorLayout->addWidget(officialBtn);
    mirrorLayout->addWidget(aliyunBtn);
    mirrorLayout->addWidget(tsinghuaBtn);
    mirrorLayout->addWidget(ustcBtn);
    mainLayout->addWidget(mirrorBox);

    QHBoxLayout* btnLayout = new QHBoxLayout(0, 4, 4);
    QPushButton* applyBtn = new QPushButton(qFromUtf8("应用"), w);
    QPushButton* restoreBtn = new QPushButton(qFromUtf8("恢复默认"), w);
    btnLayout->addWidget(applyBtn);
    btnLayout->addWidget(restoreBtn);
    btnLayout->addStretch(1);
    mainLayout->addLayout(btnLayout);

    QLabel* statusLabel = new QLabel(qFromUtf8("状态：未修改"), w);
    mainLayout->addWidget(statusLabel);

    return w;
}
