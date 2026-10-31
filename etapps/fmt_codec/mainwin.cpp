#include "compat34.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qtextedit.h>
#include <qpushbutton.h>
#include <qcombobox.h>
#include <qgroupbox.h>
#include <qlayout.h>

extern "C"
QWidget* createFmtCodecMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("格式化编解码"));
#else
    w->setWindowTitle(qFromUtf8("格式化编解码"));
#endif
    w->resize(640, 420);

    QVBoxLayout* mainLayout = new QVBoxLayout(w, 8, 6);

    QHBoxLayout* topLayout = new QHBoxLayout(0, 4, 4);
    QLabel* typeLabel = new QLabel(qFromUtf8("类型："), w);
    QComboBox* typeCombo = new QComboBox(w);
    typeCombo->insertItem(qFromUtf8("JSON"));
    typeCombo->insertItem(qFromUtf8("XML"));
    typeCombo->insertItem(qFromUtf8("YAML"));
    typeCombo->insertItem(qFromUtf8("TOML"));
    topLayout->addWidget(typeLabel);
    topLayout->addWidget(typeCombo);
    topLayout->addStretch(1);
    QPushButton* fmtBtn = new QPushButton(qFromUtf8("格式化"), w);
    QPushButton* minBtn = new QPushButton(qFromUtf8("压缩"), w);
    QPushButton* clearBtn = new QPushButton(qFromUtf8("清空"), w);
    topLayout->addWidget(fmtBtn);
    topLayout->addWidget(minBtn);
    topLayout->addWidget(clearBtn);
    mainLayout->addLayout(topLayout);

    QHBoxLayout* editLayout = new QHBoxLayout(0, 6, 6);

    QGroupBox* inBox = new QGroupBox(qFromUtf8("输入"), w);
    QVBoxLayout* inLayout = new QVBoxLayout(inBox, 4, 2);
    QTextEdit* inEdit = new QTextEdit(inBox);
    inEdit->setTextFormat(Qt::PlainText);
    inLayout->addWidget(inEdit);
    editLayout->addWidget(inBox);

    QGroupBox* outBox = new QGroupBox(qFromUtf8("输出"), w);
    QVBoxLayout* outLayout = new QVBoxLayout(outBox, 4, 2);
    QTextEdit* outEdit = new QTextEdit(outBox);
    outEdit->setTextFormat(Qt::PlainText);
    outEdit->setReadOnly(true);
    outLayout->addWidget(outEdit);
    editLayout->addWidget(outBox);

    mainLayout->addLayout(editLayout);
    return w;
}
