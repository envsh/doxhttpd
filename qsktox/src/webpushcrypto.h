#ifndef WEBPUSHCRYPTO_H
#define WEBPUSHCRYPTO_H

#include <QByteArray>
#include <QString>

// WebPush (RFC8291 aes128gcm) + VAPID (RFC8292) sender-side crypto.
// Byte-compatible with webpush-go v1.4.0 / fedbridge deriveMobSubState.
namespace WebPushCrypto {

QByteArray hkdfSha256(const QByteArray& ikm, const QByteArray& salt,
                      const QByteArray& info, int length);

bool deriveMobSubKeys(const QString& uaid,
                      QByteArray* p256dhPub,
                      QByteArray* authSecret);

QByteArray encryptAes128gcm(const QByteArray& message,
                            const QByteArray& p256dhPub,
                            const QByteArray& authSecret);

QString vapidHeader(const QString& endpoint);

} // namespace WebPushCrypto

#endif
