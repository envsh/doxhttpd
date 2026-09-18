#include "memberlistdialog.h"
#include "restapi.h"
#include "translator.h"
#include "compat34.h"
#include <qstring.h>
#include <qfont.h>
#ifdef QT3_BUILD
#include <qlistbox.h>
#else
#include <qlistwidget.h>
#endif

MemberListDialog::MemberListDialog(QWidget* parent) : QDialog(parent), listWidget(nullptr) {
    qSetWindowTitle(this, _("member_list.title"));
    resize(550, 350);
    
    QBoxLayout* mainLayout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 10, 10);
    
    // 成员列表
#ifdef QT3_BUILD
    QListBox* lb = new QListBox(this);
    listWidget = lb;
    mainLayout->addWidget(lb);
#else
    QListWidget* lw = new QListWidget(this);
    listWidget = lw;
    mainLayout->addWidget(lw);
#endif
    
    // 关闭按钮
    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    btnLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
    QPushButton* closeBtn = new QPushButton(_("buttons.close"), this);
    closeBtn->setFixedSize(80, 30);
    connect(closeBtn, SIGNAL(clicked()), this, SLOT(onClose()));
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);
}

void MemberListDialog::setMembers(const std::vector<PeerInfo>& members) {
    QFont monoFont("monospace", 10);
    if (!monoFont.exactMatch())
        monoFont.setStyleHint(QFont::TypeWriter);
    
#ifdef QT3_BUILD
    QListBox* lb = (QListBox*)listWidget;
    lb->setFont(monoFont);
    lb->clear();
    // 添加表头
    lb->insertItem(QString(" #  | %1 | %2 | %3 | %4          | %5")
        .arg(_("member_list.table.name"), -19)
        .arg(_("member_list.table.role"), -9)
        .arg(_("member_list.table.connection"), -5)
        .arg(_("member_list.table.ip"), -16)
        .arg(_("member_list.table.public_key")));
    for (const auto& peer : members) {
        QString ip = qFromUtf8(peer.peerIp);
        if (ip.isEmpty()) ip = "--";
        QString role = qFromUtf8(peer.roleStr);
        if (role.isEmpty()) role = "member";
        QString conn = qFromUtf8(peer.statusStr);
        if (conn.isEmpty()) conn = "--";
        QString pk = qFromUtf8(peer.publicKey).left(16);
        QString line = QString("%1 | %2 | %3 | %4 | %5 | %6")
            .arg(QString::number(peer.peerNumber), -3)
            .arg(qFromUtf8(peer.userName), -19)
            .arg(role, -9)
            .arg(conn, -5)
            .arg(ip, -16)
            .arg(pk, -15);
        lb->insertItem(line);
    }
#else
    QListWidget* lw = (QListWidget*)listWidget;
    lw->setFont(monoFont);
    lw->clear();
    // 添加表头
    new QListWidgetItem(QString(" #  | %1 | %2 | %3 | %4          | %5")
        .arg(_("member_list.table.name"), -19)
        .arg(_("member_list.table.role"), -9)
        .arg(_("member_list.table.connection"), -5)
        .arg(_("member_list.table.ip"), -16)
        .arg(_("member_list.table.public_key")), lw);
    for (const auto& peer : members) {
        QString ip = qFromUtf8(peer.peerIp);
        if (ip.isEmpty()) ip = "--";
        QString role = qFromUtf8(peer.roleStr);
        if (role.isEmpty()) role = "member";
        QString conn = qFromUtf8(peer.statusStr);
        if (conn.isEmpty()) conn = "--";
        QString pk = qFromUtf8(peer.publicKey).left(16);
        QString line = QString("%1 | %2 | %3 | %4 | %5 | %6")
            .arg(QString::number(peer.peerNumber), -3)
            .arg(qFromUtf8(peer.userName), -19)
            .arg(role, -9)
            .arg(conn, -5)
            .arg(ip, -16)
            .arg(pk, -15);
        new QListWidgetItem(line, lw);
    }
#endif
}

void MemberListDialog::setDialogTitle(const QString& title) {
    qSetWindowTitle(this, title);
}

void MemberListDialog::onClose() {
    accept();
}