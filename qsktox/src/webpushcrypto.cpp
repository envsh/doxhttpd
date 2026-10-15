#include "webpushcrypto.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/obj_mac.h>
#include <openssl/params.h>
#include <openssl/rand.h>

namespace {

const char* kKdfSalt = "fedlet/mobsub/v1";
const int kP256ScalarLen = 32;
const int kUncompressedLen = 65;

// P-256 group order n
const unsigned char kP256Order[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xBC, 0xE6, 0xFA, 0xAD, 0xA7, 0x17, 0x9E, 0x84,
    0xF3, 0xB9, 0xCA, 0xC2, 0xFC, 0x63, 0x25, 0x51
};

QByteArray hmacSha256(const QByteArray& key, const QByteArray& data)
{
    unsigned char out[EVP_MAX_MD_SIZE];
    unsigned int outLen = 0;
    HMAC(EVP_sha256(),
         key.constData(), key.size(),
         reinterpret_cast<const unsigned char*>(data.constData()),
         static_cast<size_t>(data.size()),
         out, &outLen);
    return QByteArray(reinterpret_cast<char*>(out), static_cast<int>(outLen));
}

QByteArray b64url(const QByteArray& data)
{
    return data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

bool isValidP256Scalar(const QByteArray& raw)
{
    if (raw.size() != kP256ScalarLen) {
        return false;
    }
    BIGNUM* d = BN_bin2bn(reinterpret_cast<const unsigned char*>(raw.constData()),
                          raw.size(), nullptr);
    if (!d) {
        return false;
    }
    bool ok = !BN_is_zero(d);
    if (ok) {
        BIGNUM* order = BN_bin2bn(kP256Order, sizeof(kP256Order), nullptr);
        ok = order != nullptr && BN_cmp(d, order) < 0;
        BN_free(order);
    }
    BN_free(d);
    return ok;
}

// EVP_PKEY_fromdata 只给私钥标量时并不会推导出公钥(ECDH/编码都拿不到点),
// 故直接用 EC_POINT_mul 计算 d*G 的未压缩编码(65B)。这与 Go ecdh 的
// NewPrivateKey(scalar).PublicKey().Bytes() 一致。
QByteArray ecP256PublicFromScalar(const QByteArray& scalar)
{
    QByteArray out;
    EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);
    if (!group) {
        return out;
    }
    BIGNUM* d = BN_bin2bn(reinterpret_cast<const unsigned char*>(scalar.constData()),
                          scalar.size(), nullptr);
    EC_POINT* point = EC_POINT_new(group);
    if (d && point &&
        EC_POINT_mul(group, point, d, nullptr, nullptr, nullptr) == 1) {
        unsigned char buf[kUncompressedLen];
        const size_t len = EC_POINT_point2oct(group, point, POINT_CONVERSION_UNCOMPRESSED,
                                              buf, sizeof(buf), nullptr);
        if (len == kUncompressedLen) {
            out = QByteArray(reinterpret_cast<char*>(buf), static_cast<int>(len));
        }
    }
    if (point) {
        EC_POINT_free(point);
    }
    if (d) {
        BN_free(d);
    }
    EC_GROUP_free(group);
    return out;
}

EVP_PKEY* ecP256PublicFromBytes(const QByteArray& pub)
{
    OSSL_PARAM params[3];
    params[0] = OSSL_PARAM_construct_utf8_string(
        OSSL_PKEY_PARAM_GROUP_NAME, const_cast<char*>("P-256"), 0);
    params[1] = OSSL_PARAM_construct_octet_string(
        OSSL_PKEY_PARAM_PUB_KEY, const_cast<char*>(pub.constData()),
        static_cast<size_t>(pub.size()));
    params[2] = OSSL_PARAM_construct_end();

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
    if (!ctx) {
        return nullptr;
    }
    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_fromdata_init(ctx) <= 0 ||
        EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) <= 0) {
        pkey = nullptr;
    }
    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

EVP_PKEY* genEcP256()
{
    return EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-256");
}

QByteArray ecPubEncoded(EVP_PKEY* pkey)
{
    unsigned char* buf = nullptr;
    size_t len = EVP_PKEY_get1_encoded_public_key(pkey, &buf);
    if (!buf) {
        return QByteArray();
    }
    QByteArray out(reinterpret_cast<char*>(buf), static_cast<int>(len));
    OPENSSL_free(buf);
    return out;
}

QByteArray ecdhShared(EVP_PKEY* priv, EVP_PKEY* peer)
{
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(priv, nullptr);
    if (!ctx) {
        return QByteArray();
    }
    QByteArray secret;
    if (EVP_PKEY_derive_init(ctx) > 0 && EVP_PKEY_derive_set_peer(ctx, peer) > 0) {
        size_t len = 0;
        if (EVP_PKEY_derive(ctx, nullptr, &len) > 0 && len > 0) {
            secret.resize(static_cast<int>(len));
            size_t got = len;
            if (EVP_PKEY_derive(ctx, reinterpret_cast<unsigned char*>(secret.data()), &got) > 0) {
                secret.resize(static_cast<int>(got));
            } else {
                secret.clear();
            }
        }
    }
    EVP_PKEY_CTX_free(ctx);
    return secret;
}

QByteArray aes128gcmEncrypt(const QByteArray& key, const QByteArray& nonce,
                            const QByteArray& plaintext)
{
    QByteArray out;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return out;
    }
    int outLen = 0;
    int total = 0;
    bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr) == 1 &&
              EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, nonce.size(), nullptr) == 1 &&
              EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                  reinterpret_cast<const unsigned char*>(key.constData()),
                  reinterpret_cast<const unsigned char*>(nonce.constData())) == 1;
    if (ok) {
        out.resize(plaintext.size() + 16);
        ok = EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(out.data()), &outLen,
                 reinterpret_cast<const unsigned char*>(plaintext.constData()),
                 plaintext.size()) == 1;
        total = outLen;
    }
    if (ok) {
        ok = EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(out.data()) + total,
                 &outLen) == 1;
        total += outLen;
    }
    if (ok) {
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16,
                 reinterpret_cast<unsigned char*>(out.data()) + total) == 1;
        total += 16;
    }
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) {
        return QByteArray();
    }
    out.resize(total);
    return out;
}

QByteArray ecdsaDerToRaw(const QByteArray& der)
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(der.constData());
    ECDSA_SIG* sig = d2i_ECDSA_SIG(nullptr, &p, der.size());
    if (!sig) {
        return QByteArray();
    }
    const BIGNUM* r = nullptr;
    const BIGNUM* s = nullptr;
    ECDSA_SIG_get0(sig, &r, &s);
    QByteArray raw(64, '\0');
    BN_bn2binpad(r, reinterpret_cast<unsigned char*>(raw.data()), 32);
    BN_bn2binpad(s, reinterpret_cast<unsigned char*>(raw.data()) + 32, 32);
    ECDSA_SIG_free(sig);
    return raw;
}

} // namespace

QByteArray WebPushCrypto::hkdfSha256(const QByteArray& ikm, const QByteArray& salt,
                                     const QByteArray& info, int length)
{
    QByteArray effectiveSalt = salt;
    if (effectiveSalt.isEmpty()) {
        effectiveSalt = QByteArray(32, '\0');
    }
    const QByteArray prk = hmacSha256(effectiveSalt, ikm);
    QByteArray okm;
    QByteArray t;
    quint8 counter = 1;
    while (okm.size() < length) {
        QByteArray block = t;
        block.append(info);
        block.append(static_cast<char>(counter));
        t = hmacSha256(prk, block);
        okm.append(t);
        counter++;
    }
    return okm.left(length);
}

bool WebPushCrypto::deriveMobSubKeys(const QString& uaid,
                                     QByteArray* p256dhPub, QByteArray* authSecret)
{
    if (uaid.isEmpty()) {
        return false;
    }
    const QByteArray secret = uaid.toUtf8();
    const QByteArray salt(kKdfSalt);

    QByteArray pub;
    for (int i = 0; i < 1000; i++) {
        const QByteArray raw = hkdfSha256(secret, salt,
            "p256dh/" + QByteArray::number(i), kP256ScalarLen);
        if (!isValidP256Scalar(raw)) {
            continue;
        }
        pub = ecP256PublicFromScalar(raw);
        if (!pub.isEmpty()) {
            break;
        }
    }
    if (pub.size() != kUncompressedLen) {
        return false;
    }

    *p256dhPub = pub;
    *authSecret = hkdfSha256(secret, salt, "auth", 16);
    return true;
}

QByteArray WebPushCrypto::encryptAes128gcm(const QByteArray& message,
                                           const QByteArray& p256dhPub,
                                           const QByteArray& authSecret)
{
    if (p256dhPub.size() != kUncompressedLen || authSecret.size() != 16) {
        return QByteArray();
    }

    QByteArray salt(16, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(salt.data()), salt.size()) != 1) {
        return QByteArray();
    }

    EVP_PKEY* asKey = genEcP256();
    if (!asKey) {
        return QByteArray();
    }
    const QByteArray asPub = ecPubEncoded(asKey);
    EVP_PKEY* peer = ecP256PublicFromBytes(p256dhPub);
    if (asPub.size() != kUncompressedLen || !peer) {
        EVP_PKEY_free(asKey);
        EVP_PKEY_free(peer);
        return QByteArray();
    }

    const QByteArray shared = ecdhShared(asKey, peer);
    EVP_PKEY_free(asKey);
    EVP_PKEY_free(peer);
    if (shared.size() != 32) {
        return QByteArray();
    }

    QByteArray prkInfo("WebPush: info\x00", 14);
    prkInfo.append(p256dhPub);
    prkInfo.append(asPub);
    const QByteArray ikm = hkdfSha256(shared, authSecret, prkInfo, 32);
    const QByteArray cek = hkdfSha256(ikm, salt,
        QByteArray("Content-Encoding: aes128gcm\x00", 28), 16);
    const QByteArray nonce = hkdfSha256(ikm, salt,
        QByteArray("Content-Encoding: nonce\x00", 24), 12);

    const quint32 rs = 4096;
    const int recordLength = static_cast<int>(rs) - 16;
    const int headerLen = 16 + 4 + 1 + asPub.size();
    const int padLen = recordLength - headerLen;

    QByteArray data = message;
    data.append(static_cast<char>(0x02));
    if (data.size() > padLen) {
        return QByteArray();
    }
    data.append(QByteArray(padLen - data.size(), '\0'));

    const QByteArray ct = aes128gcmEncrypt(cek, nonce, data);
    if (ct.isEmpty()) {
        return QByteArray();
    }

    QByteArray body = salt;
    body.append(static_cast<char>((rs >> 24) & 0xFF));
    body.append(static_cast<char>((rs >> 16) & 0xFF));
    body.append(static_cast<char>((rs >> 8) & 0xFF));
    body.append(static_cast<char>(rs & 0xFF));
    body.append(static_cast<char>(asPub.size()));
    body.append(asPub);
    body.append(ct);
    return body;
}

QString WebPushCrypto::vapidHeader(const QString& endpoint)
{
    const QUrl url(endpoint);
    if (!url.isValid() || url.host().isEmpty()) {
        return QString();
    }
    QString aud = url.scheme() + "://" + url.host();
    if (url.port() != -1) {
        aud += ":" + QString::number(url.port());
    }

    const QJsonObject header{
        {"typ", "JWT"},
        {"alg", "ES256"}
    };
    const QJsonObject claims{
        {"aud", aud},
        {"exp", static_cast<double>(QDateTime::currentSecsSinceEpoch() + 12 * 3600)},
        {"sub", "mailto:mobsub@localhost"}
    };
    const QByteArray headerSeg = b64url(QJsonDocument(header).toJson(QJsonDocument::Compact));
    const QByteArray claimsSeg = b64url(QJsonDocument(claims).toJson(QJsonDocument::Compact));
    const QByteArray signingInput = headerSeg + "." + claimsSeg;

    EVP_PKEY* key = genEcP256();
    if (!key) {
        return QString();
    }

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    QByteArray der;
    if (mdctx &&
        EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, key) == 1) {
        size_t sigLen = 0;
        if (EVP_DigestSign(mdctx, nullptr, &sigLen,
                reinterpret_cast<const unsigned char*>(signingInput.constData()),
                signingInput.size()) == 1) {
            der.resize(static_cast<int>(sigLen));
            if (EVP_DigestSign(mdctx, reinterpret_cast<unsigned char*>(der.data()), &sigLen,
                    reinterpret_cast<const unsigned char*>(signingInput.constData()),
                    signingInput.size()) == 1) {
                der.resize(static_cast<int>(sigLen));
            } else {
                der.clear();
            }
        }
    }
    if (mdctx) {
        EVP_MD_CTX_free(mdctx);
    }

    const QByteArray rawSig = ecdsaDerToRaw(der);
    const QByteArray pub = ecPubEncoded(key);
    EVP_PKEY_free(key);
    if (rawSig.size() != 64 || pub.size() != kUncompressedLen) {
        return QString();
    }

    const QByteArray jwt = signingInput + "." + b64url(rawSig);
    return QStringLiteral("vapid t=%1, k=%2")
        .arg(QString::fromLatin1(jwt), QString::fromLatin1(b64url(pub)));
}
