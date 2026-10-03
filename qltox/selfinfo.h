#ifndef SELFINFO_H
#define SELFINFO_H

#include "compat34.h"
#include "emojiwidgets.h"
#include <qwidget.h>

class SelfInfoWidget : public QWidget {
    Q_OBJECT
public:
    explicit SelfInfoWidget(QWidget* parent = 0);
    
    void updateInfo(const QString& name, const QString& statusMsg,
                    const QString& connStatus, const QString& address);
    QString selfName() const { return nameLabel ? nameLabel->text() : QString(); }
    void retranslateUi();
    
private slots:
    void onEditInfo();
    void onBootstrap();
    void onShowQRCode();
    void onCopyAddress();
    void onSwitchAccount();
    
signals:
    void switchAccountRequested();
    
private:
    QLabel* avatarLabel;
    QLabel* nameLabel;
    QLabel* statusBadge;
    QLabel* statusMsgLabel;
    QLabel* addressLabel;
    EmojiPushButton* copyBtn;
    QPushButton* switchBtn;
    QPushButton* editBtn;
    QPushButton* connectBtn;
    QPushButton* qrBtn;

    /// 「连接网络」按钮 tooltip：当前连接的服务器地址(ToxAPI base url)
    void updateConnectTooltip();

    QString selfAddress;
};

#endif // SELFINFO_H
