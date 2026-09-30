#include "identicon.h"
#include "compat34.h"
#include <qmap.h>
#include <qpainter.h>
#include <qbitmap.h>
#include <math.h>
#include <cstdint>

#ifdef QT3_BUILD
extern "C" {
#include "md5.h"
}
#else
#include <QCryptographicHash>
#endif

static void hslToRgb(double h, double s, double l,
                      int& r, int& g, int& b) {
    double c = (1 - fabs(2 * l - 1)) * s;
    double hp = h / 60.0;
    double x = c * (1 - fabs(fmod(hp, 2) - 1));
    double r1, g1, b1;
    if (hp < 1)      { r1 = c; g1 = x; b1 = 0; }
    else if (hp < 2) { r1 = x; g1 = c; b1 = 0; }
    else if (hp < 3) { r1 = 0; g1 = c; b1 = x; }
    else if (hp < 4) { r1 = 0; g1 = x; b1 = c; }
    else if (hp < 5) { r1 = x; g1 = 0; b1 = c; }
    else             { r1 = c; g1 = 0; b1 = x; }
    double m = l - c / 2;
    r = (int)((r1 + m) * 255 + 0.5);
    g = (int)((g1 + m) * 255 + 0.5);
    b = (int)((b1 + m) * 255 + 0.5);
}

static void colorFromDigest(const uint8_t digest[16],
                            int& r, int& g, int& b) {
    uint32_t hueSeed = (digest[12] << 8) | digest[13];
    double h = 180.0 + (hueSeed % 80);
    double s = 0.4 + (digest[14] >> 4) / 15.0 * 0.6;
    double l = 0.4 + (digest[14] & 0x0F) / 15.0 * 0.5;
    hslToRgb(h, s, l, r, g, b);
}

static void genPattern(const uint8_t digest[16],
                       int pat[5][3]) {
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 3; j++) {
            int idx = i * 3 + j;
            int byteIdx = idx / 2;
            int nibble = (idx % 2 == 0)
                ? (digest[byteIdx] >> 4)
                : (digest[byteIdx] & 0x0F);
            pat[i][j] = (nibble % 2 == 0) ? 1 : 0;
        }
    }
}

static void mirrorPat(int src[5][3], int dst[5][5]) {
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            dst[y][x] = (x < 3) ? src[y][x] : src[y][4 - x];
        }
    }
}

static QMap<QString, QPixmap>* s_idCachePtr() {
    static QMap<QString, QPixmap>* cache = 0;
    if (!cache)
        cache = new QMap<QString, QPixmap>();
    return cache;
}

QPixmap generateIdenticon(const QString& seed, int size) {
    QString key = seed + ":" + QString::number(size);
    QMap<QString, QPixmap>* cache = s_idCachePtr();
    QMap<QString, QPixmap>::const_iterator it = cache->find(key);
    if (it != cache->end()) {
        return *it;
    }

#ifdef QT3_BUILD
    MD5_CTX ctx;
    uint8_t digest[16];
    QByteArray data = seed.utf8();
    MD5_Init(&ctx);
    MD5_Update(&ctx, (const uint8_t*)data.data(), data.size());
    MD5_Final(digest, &ctx);
#else
    QByteArray digestBA = QCryptographicHash::hash(
        seed.toUtf8(), QCryptographicHash::Md5);
    const uint8_t* digest = (const uint8_t*)digestBA.data();
#endif

    int fgR, fgG, fgB;
    colorFromDigest(digest, fgR, fgG, fgB);

    int pat3[5][3];
    int pat[5][5];
    genPattern(digest, pat3);
    mirrorPat(pat3, pat);

    QPixmap pm(size, size);
    pm.fill(Qt::white);

    QPainter p(&pm);
    int cellW = size / 5;
    int rem = size % 5;
    QColor fg(fgR, fgG, fgB);
    for (int gy = 0; gy < 5; gy++) {
        for (int gx = 0; gx < 5; gx++) {
            if (pat[gy][gx]) {
                int x = gx * cellW + (gx < rem ? gx : rem);
                int y = gy * cellW + (gy < rem ? gy : rem);
                int w = cellW + (gx < rem ? 1 : 0);
                int h = cellW + (gy < rem ? 1 : 0);
                p.fillRect(x, y, w, h, fg);
            }
        }
    }
    p.end();

    QBitmap mask(size, size);
    mask.fill(Qt::color0);
    QPainter mp(&mask);
    mp.setBrush(Qt::color1);
    mp.setPen(Qt::NoPen);
    mp.drawEllipse(0, 0, size, size);
    mp.end();
    pm.setMask(mask);

    cache->insert(key, pm);
    return pm;
}
