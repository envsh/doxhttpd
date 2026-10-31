#include "compat34.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qtextedit.h>
#include <qpushbutton.h>
#include <qcombobox.h>
#include <qgroupbox.h>
#include <qlayout.h>

extern "C"
QWidget* createHashCalcMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("哈希计算器"));
#else
    w->setWindowTitle(qFromUtf8("哈希计算器"));
#endif
    w->resize(520, 380);

    QVBoxLayout* mainLayout = new QVBoxLayout(w, 8, 6);

    QGroupBox* inputBox = new QGroupBox(qFromUtf8("输入"), w);
    QVBoxLayout* inputLayout = new QVBoxLayout(inputBox, 6, 4);
    QTextEdit* inputEdit = new QTextEdit(inputBox);
    inputEdit->setTextFormat(Qt::PlainText);
    inputLayout->addWidget(inputEdit);
    mainLayout->addWidget(inputBox);

    QHBoxLayout* optLayout = new QHBoxLayout(0, 4, 4);
    QLabel* algoLabel = new QLabel(qFromUtf8("算法："), w);
    QComboBox* algoCombo = new QComboBox(w);
    algoCombo->insertItem(qFromUtf8("MD5"));
    algoCombo->insertItem(qFromUtf8("SHA1"));
    algoCombo->insertItem(qFromUtf8("SHA256"));
    algoCombo->insertItem(qFromUtf8("SHA512"));
    algoCombo->insertItem(qFromUtf8("Base64"));
    optLayout->addWidget(algoLabel);
    optLayout->addWidget(algoCombo);
    optLayout->addStretch(1);
    QPushButton* calcBtn = new QPushButton(qFromUtf8("计算"), w);
    QPushButton* clearBtn = new QPushButton(qFromUtf8("清空"), w);
    optLayout->addWidget(calcBtn);
    optLayout->addWidget(clearBtn);
    mainLayout->addLayout(optLayout);

    QGroupBox* resultBox = new QGroupBox(qFromUtf8("结果"), w);
    QVBoxLayout* resultLayout = new QVBoxLayout(resultBox, 6, 4);
    QLineEdit* resultEdit = new QLineEdit(resultBox);
    resultEdit->setReadOnly(true);
    resultLayout->addWidget(resultEdit);
    mainLayout->addWidget(resultBox);

    return w;
}
