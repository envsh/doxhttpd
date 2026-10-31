#include "compat34.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qtextedit.h>
#include <qgroupbox.h>
#include <qlayout.h>

extern "C"
QWidget* createM3u8dlMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("M3U8 下载器"));
#else
    w->setWindowTitle(qFromUtf8("M3U8 下载器"));
#endif
    w->resize(560, 420);

    QVBoxLayout* mainLayout = new QVBoxLayout(w, 8, 6);

    QGroupBox* urlBox = new QGroupBox(qFromUtf8("下载地址"), w);
    QVBoxLayout* urlLayout = new QVBoxLayout(urlBox, 6, 4);
    QLineEdit* urlEdit = new QLineEdit(urlBox);
    urlEdit->setText(qFromUtf8("请输入 m3u8 地址"));
    urlLayout->addWidget(urlEdit);
    mainLayout->addWidget(urlBox);

    QGroupBox* dirBox = new QGroupBox(qFromUtf8("保存目录"), w);
    QHBoxLayout* dirLayout = new QHBoxLayout(dirBox, 6, 4);
    QLineEdit* dirEdit = new QLineEdit(dirBox);
    dirEdit->setText(qFromUtf8("./download"));
    QPushButton* browseBtn = new QPushButton(qFromUtf8("浏览..."), dirBox);
    dirLayout->addWidget(dirEdit);
    dirLayout->addWidget(browseBtn);
    mainLayout->addWidget(dirBox);

    QHBoxLayout* btnLayout = new QHBoxLayout(0, 4, 4);
    QPushButton* startBtn = new QPushButton(qFromUtf8("开始下载"), w);
    QPushButton* stopBtn = new QPushButton(qFromUtf8("停止"), w);
    btnLayout->addWidget(startBtn);
    btnLayout->addWidget(stopBtn);
    btnLayout->addStretch(1);
    mainLayout->addLayout(btnLayout);

    QGroupBox* logBox = new QGroupBox(qFromUtf8("日志"), w);
    QVBoxLayout* logLayout = new QVBoxLayout(logBox, 6, 4);
    QTextEdit* logEdit = new QTextEdit(logBox);
    logEdit->setTextFormat(Qt::PlainText);
    logEdit->setReadOnly(true);
    logLayout->addWidget(logEdit);
    mainLayout->addWidget(logBox);

    return w;
}
