#include "compatcore34.h"
#include <stdlib.h>
#include <unistd.h>
#ifndef QT3_BUILD
#include <QProcess>
#endif

// ========== 实现所需的 Qt 头文件 ==========
#include <qdir.h>            // QDir (qGetHomePath, qAppDir, qCurrDir, qMkdir)
#include <qdatetime.h>       // QDateTime (qFmtTime, getCurrentTime)

// ========== EventType34 ==========
EventType34 toEventType34(int raw) {
#ifdef QT3_BUILD
    return raw;
#else
    return static_cast<QEvent::Type>(raw);
#endif
}

// 时间字符串解析：支持 "yyyy-MM-dd hh:mm:ss" 和 ISO8601 格式。
// 返回完整 "yyyy-MM-dd hh:mm:ss"，剥离毫秒与时区后缀（"Z" 或 "+08:00"）。
QString qFormatTime(const QString& createdAt) {
    // 如果是 ISO8601 格式（包含 'T'），使用 qFormatISO8601 处理
    if (createdAt.contains('T')) {
        return qFormatISO8601(createdAt);
    }
    
#ifdef QT3_BUILD
    // Qt3: 手动解析，split 后返回完整日期时间
    QStringList parts = QStringList::split(" ", createdAt);
    if (parts.count() >= 2) {
        return parts[0] + " " + parts[1];  // 完整 "yyyy-MM-dd hh:mm:ss"
    }
    return QString();
#else
    // Qt4: 使用 QDateTime
    QDateTime dt = QDateTime::fromString(createdAt, "yyyy-MM-dd hh:mm:ss");
    if (dt.isValid()) {
        return dt.toString("yyyy-MM-dd hh:mm:ss");
    }
    return QString();
#endif
}

// 解析 ISO8601 时间字符串，返回完整日期时间 "yyyy-MM-dd hh:mm:ss"。
// 剥离了小数秒（如 ".123"）与时区后缀（"Z" / "+08:00"），只保留事件记录的时间部分。
QString qFormatISO8601(const QString& iso8601Str) {
    if (iso8601Str.isEmpty()) { return QString(); }
    
#ifdef QT3_BUILD
    // Qt3: 手动解析 ISO8601 "yyyy-MM-ddThh:mm:ssZ" 或 "yyyy-MM-ddThh:mm:ss+08:00"
    QString str = iso8601Str;
    // 移除 'T'
    int tPos = str.find('T');
    if (tPos >= 0) {
        str = str.left(tPos) + " " + str.mid(tPos + 1);
    }
    // 移除时区后缀：'+' 或 '-'（排除日期中的 '-')
    int zonePos = str.find('+');
    if (zonePos < 0) { zonePos = str.find('-', 10); }
    if (zonePos >= 0) {
        str = str.left(zonePos);
    }
    // 移除 'Z' 后缀（UTC 标识）
    if (str.right(1) == "Z") {
        str = str.left(str.length() - 1);
    }
    // 移除秒后的小数点
    int dotPos = str.find('.');
    if (dotPos >= 0) {
        str = str.left(dotPos);
    }
    // 现在格式应为 "yyyy-MM-dd hh:mm:ss"，返回完整时间
    QStringList parts = QStringList::split(" ", str);
    if (parts.count() >= 2) {
        return parts[0] + " " + parts[1];  // 完整 "yyyy-MM-dd hh:mm:ss"
    }
    return QString();
#else
    // Qt4: 使用 QDateTime 解析 ISO8601
    QDateTime dt = QDateTime::fromString(iso8601Str, Qt::ISODate);
    if (!dt.isValid()) {
        // 尝试移除时区中的 ':'
        QString cleaned = iso8601Str;
        int plusPos = cleaned.indexOf('+');
        int minusPos = cleaned.lastIndexOf('-', cleaned.length() - 1);
        int tzPos = (plusPos > 0) ? plusPos : ((minusPos > 10) ? minusPos : -1);
        if (tzPos > 0) {
            cleaned = cleaned.left(tzPos);
        }
        cleaned.replace(".", "");
        dt = QDateTime::fromString(cleaned, "yyyy-MM-ddTHh:mm:ss");
    }
    if (dt.isValid()) {
        return dt.toString("yyyy-MM-dd hh:mm:ss");
    }
    return QString();
#endif
}

// QString API 兼容
QString qTrim(const QString& s) {
#ifdef QT3_BUILD
    return s.stripWhiteSpace();
#else
    return s.trimmed();
#endif
}

QByteArray qToUtf8(const QString& s) {
#ifdef QT3_BUILD
    return s.utf8();
#else
    return s.toUtf8();
#endif
}

QString qFromUtf8(const std::string& s) {
    return QString::fromUtf8(s.c_str());
}

QString qFromUtf8(const char* s) {
    return QString::fromUtf8(s);
}

QString qFromUtf8(const char* data, int size) {
    return QString::fromUtf8(data, size);
}

QByteArray qToLocal8Bit(const QString& s) {
#ifdef QT3_BUILD
    return s.local8Bit();
#else
    return s.toLocal8Bit();
#endif
}

int qLastIndexOf(const QString& s, const QString& str) {
#ifdef QT3_BUILD
    return s.findRev(str);
#else
    return s.lastIndexOf(str);
#endif
}

QString qToUpper(const QString& s) {
#ifdef QT3_BUILD
    return s.upper();
#else
    return s.toUpper();
#endif
}

QStringList qSplit(const QString& str, const QString& sep) {
#ifdef QT3_BUILD
    return QStringList::split(sep, str);
#else
    return str.split(sep);
#endif
}

bool qOpenReadOnly(QFile& file) {
#ifdef QT3_BUILD
    return file.open(IO_ReadOnly);
#else
    return file.open(QIODevice::ReadOnly | QIODevice::Text);
#endif
}

bool qOpenWriteOnly(QFile& file) {
#ifdef QT3_BUILD
    return file.open(IO_WriteOnly);
#else
    return file.open(QIODevice::WriteOnly | QIODevice::Text);
#endif
}

QString qGetHomePath() {
#ifdef QT3_BUILD
    return QDir::homeDirPath();
#else
    return QDir::homePath();
#endif
}

#ifdef QT3_BUILD
#include <qfileinfo.h>       // QFileInfo (qAppDir QT3 路径)
#else
#include <qcoreapplication.h> // QCoreApplication::applicationDirPath
#endif

QString qAppDir() {
#ifdef QT3_BUILD
    char exePath[1024];
    int len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len != -1) {
        exePath[len] = '\0';
        QFileInfo fi;
        fi.setFile(QString(exePath));
        return fi.dirPath(true);
    }
    return ".";
#else
    return QCoreApplication::applicationDirPath();
#endif
}

QString qCurrDir() {
#ifdef QT3_BUILD
    return QDir::currentDirPath();
#else
    return QDir::currentPath();
#endif
}

QString qElideChars(const QString& text, int maxLen,
                    ElidePos pos, const QString& ellipsis) {
    if (text.length() <= maxLen) return text;
    int ellen = ellipsis.length();
    if (maxLen <= ellen) return ellipsis.left(maxLen);
    int keep = maxLen - ellen;
    switch (pos) {
    case ElideRight:
        return text.left(keep) + ellipsis;
    case ElideLeft:
        return ellipsis + text.right(keep);
    case ElideMiddle: {
        int left = keep / 2 + keep % 2;
        int right = keep / 2;
        return text.left(left) + ellipsis + text.right(right);
    }
    }
    return text;
}

QString qFmtTime(uint timestamp) {
#ifdef QT3_BUILD
    QDateTime dt;
    dt.setTime_t(timestamp);
    return dt.toString("yyyy-MM-dd hh:mm:ss");
#else
#if QT_VERSION >= 0x060000
	return QDateTime::fromSecsSinceEpoch(timestamp).toString("yyyy-MM-dd hh:mm:ss");
#else
    return QDateTime::fromTime_t(timestamp).toString("yyyy-MM-dd hh:mm:ss");
#endif
#endif
}

QString getCurrentTime() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
}

void qSleepMs(unsigned long ms) {
    usleep(ms * 1000);
}

QByteArray base64Decode(const std::string& b64) {
    if (b64.empty()) return QByteArray();
#ifdef QT3_BUILD
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char rev[256] = {};
    for (int i = 0; i < 64; ++i) rev[(unsigned char)alphabet[i]] = i;
    rev[(unsigned char)'-'] = 62;
    rev[(unsigned char)'_'] = 63;

    int len = (int)b64.size();
    int maxOut = ((len + 3) / 4) * 3;
    QByteArray out;
    out.resize(maxOut);
    int wp = 0;
    unsigned char buf[4];
    int bufPos = 0;

    for (int i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)b64[i];
        if (c == '=') {
            break;
        }
        unsigned char val = rev[c];
        if (val == 0 && c != 'A') {
            if (c != '\n' && c != '\r' && c != ' ' && c != '\t') {
                qWarning("base64Decode: skipping invalid char 0x%02x at pos %d",
                         (unsigned char)b64[i], i);
                continue;
            }
            continue;
        }
        buf[bufPos++] = val;
        if (bufPos == 4) {
            out[wp++] = (char)((buf[0] << 2) | (buf[1] >> 4));
            out[wp++] = (char)((buf[1] << 4) | (buf[2] >> 2));
            out[wp++] = (char)((buf[2] << 6) | buf[3]);
            bufPos = 0;
        }
    }
    if (bufPos > 0) {
        for (int i = bufPos; i < 4; ++i) buf[i] = 0;
        if (bufPos >= 2)
            out[wp++] = (char)((buf[0] << 2) | (buf[1] >> 4));
        if (bufPos >= 3)
            out[wp++] = (char)((buf[1] << 4) | (buf[2] >> 2));
    }
    out.resize(wp);
    return out;
#else
    return QByteArray::fromBase64(QByteArray(b64.data(), (int)b64.size()));
#endif
}

bool qMkdir(const QString& path, bool recursive) {
#ifdef QT3_BUILD
    if (recursive) {
        // QDir::mkdirs 在旧版 Qt3 不可用，手动逐级创建
        QString p = path;
        if (p.endsWith("/")) { p.truncate(p.length() - 1); }
        int slashPos = 0;
        while ((slashPos = p.find("/", slashPos + 1)) != -1) {
            QString sub = p.left(slashPos);
            QDir().mkdir(sub);
        }
        return QDir().mkdir(p);
    }
    return QDir().mkdir(path);
#else
    if (recursive) return QDir().mkpath(path);
    return QDir().mkdir(path);
#endif
}

#if QT_VERSION >= 0x060000
#include <qstringconverter.h>

QString qToUnicode(const QByteArray& data, const char* codecName) {
    QStringDecoder decoder(codecName);
    if (decoder.isValid()) {
        return decoder(data);
    }
    return QStringDecoder(QStringDecoder::Utf8)(data);
}

#else  // Qt3/Qt4/Qt5
#include <qtextcodec.h>

QString qToUnicode(const QByteArray& data, const char* codecName) {
    QTextCodec* codec = QTextCodec::codecForName(codecName);
    if (codec) {
        return codec->toUnicode(data);
    }
    return QString::fromUtf8(data);
}

#endif

// ========== URL 打开兼容 ==========
#ifdef QT3_BUILD
void qOpenUrl(const QString& url) {
#if defined(Q_OS_WIN32)
    system(QString("cmd /c start \"\" \"" + url + "\"").local8Bit().data());
#elif defined(Q_OS_MACX)
    system(QString("open \"" + url + "\"").local8Bit().data());
#else
    system(QString("xdg-open '" + url + "'").local8Bit().data());
#endif
}
#else
#include <QDesktopServices>
#include <QUrl>
void qOpenUrl(const QString& url) {
    QDesktopServices::openUrl(QUrl(url));
}
#endif

// ========== 进程/命令兼容 ==========
// shell 单引号转义（QT3 走 popen/system 时需要）
static QString shellQuote(const QString& s) {
    QString t = s;
    t.replace("'", "'\\''");
    return QString("'") + t + QString("'");
}

int qRuncmdCaptureOuterr(const QString& program, const QStringList& args, QString* outErr) {
#ifdef QT3_BUILD
    QString cmd = program;
    for (int i = 0; i < args.size(); i++) { cmd += " " + shellQuote(args[i]); }
    FILE* f = ::popen(cmd.local8Bit().data(), "r");
    if (!f) { return -1; }
    std::string buf;
    char tmp[4096];
    size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0) { buf.append(tmp, n); }
    int rc = ::pclose(f);
    if (outErr) { *outErr = qFromUtf8(buf.data(), (int)buf.size()); }
    return rc;
#else
    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForStarted(3000)) { return -1; }
    proc.waitForFinished(-1);
    QByteArray all = proc.readAllStandardOutput();
    all += proc.readAllStandardError();
    if (outErr) { *outErr = QString::fromUtf8(all.constData(), all.size()); }
    return proc.exitCode();
#endif
}

bool qStartProcessDetached(const QString& program, const QStringList& args) {
#ifdef QT3_BUILD
    QString cmd = program;
    for (int i = 0; i < args.size(); i++) { cmd += " " + shellQuote(args[i]); }
    QString full = cmd + " >/dev/null 2>&1 &";
    int rc = ::system(full.local8Bit().data());
    return rc != -1;
#else
    return QProcess::startDetached(program, args);
#endif
}
