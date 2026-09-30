// qcrc64.cpp — CRC-64/XZ (ECMA-182 reflected variant, 同 xz 压缩器)
//
// 纯 C 核心照抄 xz 官方 src/liblzma/check/crc64_small.c：
//   作者 Lasse Collin，公版领域 (0BSD / public domain)。
//   poly=0xc96c5795d7870f42 (即标准 0x42f0e1eba9ea3693 的反射相)
//   check("123456789")=0x995dc9bbdf1939fa
//
// 注意：这是标准 CRC-64/XZ。现存 anystik/src/stickerstore.cpp 里的 CRC64
// 用的是一组非标准反射多项式(0x95ac9329ac4bc9b5)，check 不匹配任何标准体。
// 两者值不同，切勿混用；本文件按标准实现，anystik 暂未接入。

#include "qcrc64.h"

#include <qfile.h>
#ifdef QT3_BUILD
#include <qcstring.h>   // QCString (Qt3 的 QByteArray 等价)
#endif

static qcrc64_t s_crc64_table[256];
static bool s_crc64_table_ready = false;

static void qcrc64_init_table()
{
    static const qcrc64_t poly64 = UINT64_C(0xC96C5795D7870F42);
    for (size_t b = 0; b < 256; ++b) {
        qcrc64_t r = b;
        for (size_t i = 0; i < 8; ++i) {
            if (r & 1)
                r = (r >> 1) ^ poly64;
            else
                r >>= 1;
        }
        s_crc64_table[b] = r;
    }
}

qcrc64_t qcrc64(const uint8_t* buf, size_t size, qcrc64_t crc)
{
    if (!s_crc64_table_ready) {
        qcrc64_init_table();
        s_crc64_table_ready = true;
    }
    crc = ~crc;
    while (size != 0) {
        crc = s_crc64_table[*buf++ ^ (crc & 0xFF)] ^ (crc >> 8);
        --size;
    }
    return ~crc;
}

uint64_t qCrc64Str(const QString& s)
{
#ifdef QT3_BUILD
    QCString bytes = s.utf8();
    return qcrc64(reinterpret_cast<const uint8_t*>(bytes.data()),
                  bytes.length(), static_cast<qcrc64_t>(0));
#else
    const QByteArray bytes = s.toUtf8();
    return qcrc64(reinterpret_cast<const uint8_t*>(bytes.data()),
                  size_t(bytes.size()), static_cast<qcrc64_t>(0));
#endif
}

uint64_t qCrc64File(const QString& path)
{
    QFile f(path);
#ifdef QT3_BUILD
    if (!f.open(IO_ReadOnly))
#else
    if (!f.open(QIODevice::ReadOnly))
#endif
        return 0;

    qcrc64_t crc = static_cast<qcrc64_t>(0);
    char buf[65536];
    while (!f.atEnd()) {
#ifdef QT3_BUILD
        const int n = f.readBlock(buf, sizeof(buf));
#else
        const qint64 n = f.read(buf, sizeof(buf));
#endif
        if (n <= 0)
            break;
        crc = qcrc64(reinterpret_cast<const uint8_t*>(buf),
                     size_t(n), crc);
    }
    return crc;
}
