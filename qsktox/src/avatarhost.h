#ifndef AVATAR_HOST_H
#define AVATAR_HOST_H

#include <QObject>
#include <QString>
#include <QSet>

// 头像下载宿主：mxc:// 按 Matrix 标准转 https 直连（不经后端），
// https:// 直链直接请求。下载完成落 AvatarManager 缓存，发 avatarUpdated。
class AvatarHost : public QObject
{
    Q_OBJECT
public:
    static AvatarHost* instance();

    // 触发一次头像下载（内部去重：在途则跳过）。
    void download(const QString& avatarUrl);

Q_SIGNALS:
    void avatarUpdated();

protected:
    void customEvent(QEvent* event) override;

private:
    AvatarHost();
    QSet<QString> m_pending;
};

#endif // AVATAR_HOST_H
