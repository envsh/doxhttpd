#include "selfinfo.h"
#include "translator.h"
#include "editinfodialog.h"
#include "restapi.h"
#include "compat34.h"
#include "LimeStyle.h"
#include "toastwidget.h"
#include <qmessagebox.h>
#include <qclipboard.h>

SelfInfoWidget::SelfInfoWidget(QWidget* parent) : QWidget(parent), selfAddress("") {
    QBoxLayout* mainLayout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 6, 5);
    qSetMargins(mainLayout, 6, 6, 6, 6);
    
    // 头部区域：头像 + 信息
    QBoxLayout* headerLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    
    // 头像
    avatarLabel = new QLabel("?", this);
    avatarLabel->setFixedSize(40, 40);
    avatarLabel->setAlignment(Qt::AlignCenter);
    avatarLabel->setLineWidth(2);
    headerLayout->addWidget(avatarLabel);
    
    // 信息内容
    QBoxLayout* infoLayout = qNewBoxLayout(nullptr, QBoxLayout::TopToBottom, 0, 2);
    
    // 名称行：名称 + 状态标识
    QBoxLayout* nameRowLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    nameLabel = new QLabel(_("no_name"), this);
    nameLabel->setFont(QFont("Helvetica", 16, QFont::Bold));
    nameRowLayout->addWidget(nameLabel, 1);
    
    statusBadge = new QLabel(_("statuses.offline"), this);
    statusBadge->setAlignment(Qt::AlignCenter);
    statusBadge->setFixedSize(60, 20);
    nameRowLayout->addWidget(statusBadge);
    infoLayout->addLayout(nameRowLayout);
    
    // 状态消息
    statusMsgLabel = new QLabel(_("no_status"), this);
    statusMsgLabel->setFont(QFont("Helvetica", 12));
    infoLayout->addWidget(statusMsgLabel);
    
    headerLayout->addLayout(infoLayout, 1);
    mainLayout->addLayout(headerLayout);
    
    // 地址行（含切换账号按钮在最左侧）
    QBoxLayout* addrLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    switchBtn = new QPushButton(qFromUtf8("⇄"), this);
    switchBtn->setFixedSize(30, 25);
    connect(switchBtn, SIGNAL(clicked()), this, SLOT(onSwitchAccount()));
    addrLayout->addWidget(switchBtn);
    
    addressLabel = new QLabel("...", this);
    addressLabel->setFont(QFont("Monospace", 11));
    addrLayout->addWidget(addressLabel, 1);
    
    copyBtn = new EmojiPushButton(qFromUtf8("📋"), this);
    copyBtn->setFixedSize(50, 25);
    copyBtn->setEmojiInset(3);
    connect(copyBtn, SIGNAL(clicked()), this, SLOT(onCopyAddress()));
    addrLayout->addWidget(copyBtn);
    mainLayout->addLayout(addrLayout);
    
    // 操作按钮
    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    
    editBtn = new QPushButton(_("buttons.edit_info"), this);
    editBtn->setFixedHeight(24);
    connect(editBtn, SIGNAL(clicked()), this, SLOT(onEditInfo()));
    btnLayout->addWidget(editBtn);
    
    connectBtn = new QPushButton(_("buttons.connect_network"), this);
    connectBtn->setFixedHeight(24);
    connect(connectBtn, SIGNAL(clicked()), this, SLOT(onBootstrap()));
    btnLayout->addWidget(connectBtn);
    
    qrBtn = new QPushButton(_("buttons.qrcode"), this);
    qrBtn->setFixedHeight(24);
    connect(qrBtn, SIGNAL(clicked()), this, SLOT(onShowQRCode()));
    btnLayout->addWidget(qrBtn);
    
    mainLayout->addLayout(btnLayout);
}

void SelfInfoWidget::updateInfo(const QString& name, const QString& statusMsg,
                               const QString& connStatus, const QString& address) {
    // 更新头像
    QString displayName = name.isEmpty() ? _("no_name") : name;
    QString initial = qToUpper(displayName.left(1));
    avatarLabel->setText(initial);
    
    if (!name.isEmpty()) {
        QPalette apal(currentPalette().accent, currentPalette().accentText);
        avatarLabel->setPalette(apal);
    } else {
        avatarLabel->setPalette(QPalette());
    }
    
    // 更新名称
    nameLabel->setText(displayName);
    
    // 更新状态标识
    QString statusText = (connStatus == "offline") ? _("statuses.offline") :
                        (connStatus == "tcp") ? _("statuses.tcp") :
                        _("statuses.udp");
    statusBadge->setText(statusText);
    
    if (connStatus == "offline") {
        QPalette opal;
#ifdef QT3_BUILD
        opal.setColor(QPalette::Active, QColorGroup::Background, currentPalette().textMuted);
        opal.setColor(QPalette::Active, QColorGroup::Foreground, currentPalette().textPrimary);
        opal.setColor(QPalette::Disabled, QColorGroup::Background, currentPalette().textMuted);
        opal.setColor(QPalette::Disabled, QColorGroup::Foreground, currentPalette().textPrimary);
        opal.setColor(QPalette::Inactive, QColorGroup::Background, currentPalette().textMuted);
        opal.setColor(QPalette::Inactive, QColorGroup::Foreground, currentPalette().textPrimary);
#else
        opal.setColor(QPalette::Window, currentPalette().textMuted);
        opal.setColor(QPalette::WindowText, currentPalette().textPrimary);
#endif
        statusBadge->setPalette(opal);
#ifndef QT3_BUILD
        statusBadge->setAutoFillBackground(true);
#endif
    } else {
        QPalette cpal;
#ifdef QT3_BUILD
        cpal.setColor(QPalette::Active, QColorGroup::Background, currentPalette().hoverBg);
        cpal.setColor(QPalette::Active, QColorGroup::Foreground, currentPalette().textPrimary);
        cpal.setColor(QPalette::Disabled, QColorGroup::Background, currentPalette().hoverBg);
        cpal.setColor(QPalette::Disabled, QColorGroup::Foreground, currentPalette().textPrimary);
        cpal.setColor(QPalette::Inactive, QColorGroup::Background, currentPalette().hoverBg);
        cpal.setColor(QPalette::Inactive, QColorGroup::Foreground, currentPalette().textPrimary);
#else
        cpal.setColor(QPalette::Window, currentPalette().hoverBg);
        cpal.setColor(QPalette::WindowText, currentPalette().textPrimary);
#endif
        statusBadge->setPalette(cpal);
#ifndef QT3_BUILD
        statusBadge->setAutoFillBackground(true);
#endif
    }
    
    // 更新状态消息
    statusMsgLabel->setText(statusMsg.isEmpty() ? _("no_status") : statusMsg);
    
    // 更新地址
    selfAddress = address;
    if (address.length() > 20) {
        QString shortAddr = address.left(8) + "..." + address.right(8);
        addressLabel->setText(shortAddr);
    } else {
        addressLabel->setText(address);
    }
    // 设置工具提示
    qSetToolTip(addressLabel, address);
}

void SelfInfoWidget::onEditInfo() {
    // 从现有控件读取当前值
    QString currentName = nameLabel->text();
    QString currentStatus = statusMsgLabel->text();
    
    // 如果是默认值，传递空字符串（让对话框显示为空）
    if (currentName == _("no_name") || currentName == "未设置名称") {
        currentName = "";
    }
    if (currentStatus == _("no_status") || currentStatus == "无状态消息") {
        currentStatus = "";
    }
    
    EditInfoDialog dialog(currentName, currentStatus, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.getName();
        QString status = dialog.getStatusMessage();
        
        ToxAPI::setSelfInfo(
            std::string(qToUtf8(name).data()),
            std::string(qToUtf8(status).data())
        );
    }
}

void SelfInfoWidget::onBootstrap() {
    QMessageBox::information(this, "Bootstrap", _("connecting_network"));
    
    ToxAPI::getSelf();
}

void SelfInfoWidget::onShowQRCode() {
    if (selfAddress.isEmpty()) {
        QMessageBox::warning(this, "QR Code", _("please_wait"));
        return;
    }
    
    // Qt3 没有QUrl，简单进行URL编码
    QString encodedAddr = selfAddress;
    encodedAddr.replace("%", "%25");
    encodedAddr.replace(" ", "%20");
    encodedAddr.replace("!", "%21");
    encodedAddr.replace("\"", "%22");
    encodedAddr.replace("#", "%23");
    encodedAddr.replace("$", "%24");
    encodedAddr.replace("&", "%26");
    encodedAddr.replace("'", "%27");
    encodedAddr.replace("(", "%28");
    encodedAddr.replace(")", "%29");
    encodedAddr.replace("*", "%2A");
    encodedAddr.replace("+", "%2B");
    encodedAddr.replace(",", "%2C");
    encodedAddr.replace("/", "%2F");
    encodedAddr.replace(":", "%3A");
    encodedAddr.replace(";", "%3B");
    encodedAddr.replace("=", "%3D");
    encodedAddr.replace("?", "%3F");
    encodedAddr.replace("@", "%40");
    encodedAddr.replace("[", "%5B");
    encodedAddr.replace("]", "%5D");
    
    QString url = QString("https://api.qrserver.com/v1/create-qr-code/?size=200x200&data=%1")
                  .arg(encodedAddr);
    
    QMessageBox::information(this, "QR Code", _("tox_id_copied") + "\n\nURL: " + url);
}

void SelfInfoWidget::retranslateUi() {
    // 更新按钮文字
    if (switchBtn) { switchBtn->setText(qFromUtf8("⇄")); }
    if (editBtn) { editBtn->setText(_("buttons.edit_info")); }
    if (connectBtn) { connectBtn->setText(_("buttons.connect_network")); }
    if (qrBtn) { qrBtn->setText(_("buttons.qrcode")); }
    if (copyBtn) { copyBtn->setText(qFromUtf8("📋")); }
    
    // 更新状态标签（如果当前显示的是默认值）
    QString currentStatus = statusBadge->text();
    // 重新获取状态并更新???
    if (currentStatus == "TCP" || currentStatus == "UDP") {
		// statusBadge->setText(currentStatus);
	} else {
		statusBadge->setText(_("statuses.offline"));
	}
}

void SelfInfoWidget::onSwitchAccount() {
    emit switchAccountRequested();
}

void SelfInfoWidget::onCopyAddress() {
    if (!selfAddress.isEmpty()) {
        QApplication::clipboard()->setText(selfAddress);
        ToastWidget::show(this, _("tox_id_copied"));
    }
}
