#ifndef FRIENDINFODIALOG_H
#define FRIENDINFODIALOG_H

#include "compat34.h"
#include "restapi.h"
#include <qstring.h>
#include <qdialog.h>

class FriendInfoDialog : public QDialog {
    Q_OBJECT
public:
    explicit FriendInfoDialog(QWidget* parent = nullptr);
    
    void setInfo(int id, const QString& name, const QString& type,
                 const QString& status = QString(),
                 const QString& userStatus = QString(),
                 const QString& connection = QString(),
                 bool isConnected = false,
                 const QString& publicKey = QString());
    void setInfo(const FriendInfo& info);
    void setTitle(const QString& title);
    void setLastSeen(const QString& text);
    void setPeerCount(int count);
    void setAvatar(int id, const QString& name, const QString& mxcUrl);

private slots:
    void onClose();

private:
    QLabel* avatarLabel;
    QLabel* statusLineLabel;
    QLabel* titleLabel;
    QLabel* idLabel;
    QLabel* nameLabel;
    QLabel* typeLabel;
    QLabel* statusLabel;
    QLabel* userStatusLabel;
    QLabel* connLabel;
    QLabel* connectedLabel;
    QLabel* pkLabel;
    QLabel* ipLabel;
    QLabel* lastSeenLabel;
    QLabel* peerCountLabel;
};

#endif // FRIENDINFODIALOG_H
