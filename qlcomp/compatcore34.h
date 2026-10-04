#ifndef COMPATCORE34_H
#define COMPATCORE34_H

// ========== 跨 Qt3/Qt4 通用头文件 ==========
#include <qstring.h>         // QString, QByteArray, QStringList
#include <ctime>             // timespec, clock_gettime

// Qt 4.4 起官方提供 QT_VERSION_CHECK；Qt 3.5 没有（只有 QT_VERSION_STR + QT_VERSION）。
// 按官方同一定义补齐，Qt4/5/6 由 #ifndef 自动跳过。
#ifndef QT_VERSION_CHECK
#define QT_VERSION_CHECK(major, minor, patch) ((major<<16)|(minor<<8)|(patch))
#endif
#ifdef QT3_BUILD
#include <qobjectlist.h>     // Qt3 QObjectList（完整定义，用于 children()）
#include <qevent.h>          // QCustomEvent (Qt3)
#else
#include <qcoreevent.h>      // QEvent::Type + QEvent (Qt4+, 纯 QtCore)
#endif
#include <qlist.h>           // QList — for QPtrList
#include <qfile.h>           // QFile
#if QT_VERSION >= 0x050000
#include <QMap>               // QMap — for qMapValue
#include <QVariant>           // QVariant — for qMapValue(QMap<QString,QVariant>)
#else
#include <qmap.h>             // QMap
#include <qvariant.h>         // QVariant
#endif

// ========== 事件类型兼容 ==========
#ifdef QT3_BUILD
typedef int EventType34;
#else
typedef QEvent::Type EventType34;
#endif

EventType34 toEventType34(int raw);

// ========== 事件基类 ==========
#ifdef QT3_BUILD
typedef QCustomEvent CustomEventBase;
#else
typedef QEvent CustomEventBase;
#endif

// ========== 兼容函数声明 ==========

QString qTrim(const QString& s);
QByteArray qToUtf8(const QString& s);
QString qFromUtf8(const std::string& s);
QString qFromUtf8(const char* s);
QString qFromUtf8(const char* data, int size);
QByteArray qToLocal8Bit(const QString& s);
int qLastIndexOf(const QString& s, const QString& str);
QString qToUpper(const QString& s);
QStringList qSplit(const QString& str, const QString& sep);
/// split 包装：skipEmpty=true 丢弃空项，false（默认）保留空项
QStringList qStrSplit(const QString& str, const QString& sep, bool skipEmpty = false);

bool qOpenReadOnly(QFile& file);
bool qOpenWriteOnly(QFile& file);
QString qGetHomePath();
QString qAppDir();
QString qCurrDir();
bool qMkdir(const QString& path, bool recursive = true);

enum ElidePos {
    ElideRight,
    ElideLeft,
    ElideMiddle
};
QString qElideChars(const QString& text, int maxLen,
                    ElidePos pos = ElideMiddle,
                    const QString& ellipsis = "...");

QString qFormatTime(const QString& createdAt);
QString qFormatISO8601(const QString& iso8601Str);
QString qFmtTime(uint timestamp);
QString getCurrentTime();

// 系统打开 URL（非 QT3: QDesktopServices::openUrl；QT3: 平台 shell 命令）
void qOpenUrl(const QString& url);

// 同步运行命令并捕获 stdout+stderr；返回退出码（-1=启动失败）
int qRuncmdCaptureOuterr(const QString& program, const QStringList& args, QString* outErr);

// 分离启动外部进程（播放器等）；返回是否成功启动
bool qStartProcessDetached(const QString& program, const QStringList& args);

// ========== QTimer / QMap 兼容 ==========
class QTimer;   // 指针参数，前向声明即可（实现处 include QTimer 头）
// 启动定时器。Qt3 start(msec, sshot) 第二参是「单次」，Qt4+ start(msec) 是重复触发
//（单次须先 setSingleShot(true)）；这里统一成 repeat 语义，两端行为一致。
void qTimerStart(QTimer* t, int msec, bool repeat = true);
// QMap 取值：各代 const_iterator 都支持 operator*（Qt3 const T& / Qt4 const T / Qt6 const T&），
// 实现无需 #ifdef。按实际用到的键值类型重载，缺键返回 def。
QString qMapValue(const QMap<QString,QString>& m, const QString& key, const QString& def = QString());
QVariant qMapValue(const QMap<QString,QVariant>& m, const QString& key, const QVariant& def = QVariant());
int qMapValue(const QMap<int,int>& m, int key, int def = 0);

// ========== QPtrList 兼容（Qt4 模拟） ==========
#ifndef QT3_BUILD
template<typename T>
class QPtrList : public QList<T*> {
public:
    void append(T* item) { QList<T*>::append(item); }
    bool isEmpty() const { return QList<T*>::isEmpty(); }
    int count() const { return QList<T*>::count(); }
};
#endif

void qSleepMs(unsigned long ms);

// ========== Q36RegExp（Qt3/Qt4: QRegExp；Qt5.11+/Qt6: QRegularExpression）==========
#if QT_VERSION >= 0x050000
#include <QRegularExpression>
#else
#include <qregexp.h>
#endif

class Q36RegExp {
public:
    explicit Q36RegExp(const QString& pattern = QString(), bool caseSensitive = true) {
#if QT_VERSION >= 0x050000
        QRegularExpression::PatternOptions opts =
            caseSensitive ? QRegularExpression::NoPatternOption
                          : QRegularExpression::CaseInsensitiveOption;
        m_re = QRegularExpression(pattern, opts);
#elif QT_VERSION >= 0x040000
        // Qt4 把 Qt3 的 bool caseSensitive 换成了 Qt::CaseSensitivity 枚举
        m_re = QRegExp(pattern, caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
#else
        m_re = QRegExp(pattern, caseSensitive);
#endif
    }
    int search(const QString& text, int offset = 0) { return doIndexIn(text, offset); }
    int indexIn(const QString& text, int offset = 0) { return doIndexIn(text, offset); }
    int matchedLength() const {
#if QT_VERSION >= 0x050000
        return m_last.capturedLength(0);
#else
        return m_re.matchedLength();
#endif
    }
private:
    // 三个分支语义一致：返回匹配起始位置，未匹配返回 -1
    // （Qt3 的 search() 就是 Qt4 改名的 indexIn()）
    int doIndexIn(const QString& text, int offset) {
#if QT_VERSION >= 0x050000
        m_last = m_re.match(text, offset);
        return m_last.hasMatch() ? m_last.capturedStart() : -1;
#elif QT_VERSION >= 0x040000
        return m_re.indexIn(text, offset);
#else
        return m_re.search(text, offset);
#endif
    }
#if QT_VERSION >= 0x050000
    QRegularExpression m_re;
    QRegularExpressionMatch m_last;
#else
    QRegExp m_re;
#endif
};

QByteArray base64Decode(const std::string& b64);

// Qt5/Qt6 编码转换兼容函数
// Qt5: QTextCodec::codecForName + toUnicode
// Qt6: QStringDecoder（需要 ICU 支持）
QString qToUnicode(const QByteArray& data, const char* codecName);

// ── 计时工具（类 Go time.Since）──
using TimePoint = struct timespec;

inline TimePoint timeNow() {
    TimePoint tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return tp;
}

inline long long elapsedMs(const TimePoint& start) {
    TimePoint now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start.tv_sec) * 1000LL
         + (now.tv_nsec - start.tv_nsec) / 1000000LL;
}

// boottime 时钟：CLOCK_BOOTTIME 含系统休眠时间，用于看门狗（休眠恢复后立即判停滞）
inline TimePoint timeNowBoot() {
    TimePoint tp;
#ifndef CLOCK_BOOTTIME
    clock_gettime(CLOCK_MONOTONIC, &tp);   // 非 Linux：回退，行为同现状
#else
    clock_gettime(CLOCK_BOOTTIME, &tp);    // Linux：含休眠时间
#endif
    return tp;
}

inline long long elapsedMsBoot(const TimePoint& start) {
    TimePoint now;
#ifndef CLOCK_BOOTTIME
    clock_gettime(CLOCK_MONOTONIC, &now);
#else
    clock_gettime(CLOCK_BOOTTIME, &now);
#endif
    return (now.tv_sec - start.tv_sec) * 1000LL
         + (now.tv_nsec - start.tv_nsec) / 1000000LL;
}

// boottime 时钟上的"从现在起 delayMs 后"时刻（非阻塞延迟调度用）
inline TimePoint timeFromNowBoot(int delayMs) {
    TimePoint tp = timeNowBoot();
    tp.tv_sec += delayMs / 1000;
    tp.tv_nsec += (long)(delayMs % 1000) * 1000000L;
    if (tp.tv_nsec >= 1000000000L) { tp.tv_sec += 1; tp.tv_nsec -= 1000000000L; }
    return tp;
}

inline std::string timeSince(const TimePoint& start) {
    TimePoint now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long long ns = (now.tv_sec - start.tv_sec) * 1000000000LL
                 + (now.tv_nsec - start.tv_nsec);
    if (ns < 1000)       return std::to_string(ns) + "ns";
    if (ns < 1000000)    return std::to_string(ns / 1000) + "us";
    auto ms = ns / 1000000;
    if (ms < 1000)       return std::to_string(ms) + "ms";
    auto sec = ms / 1000;
    if (sec < 60)        return std::to_string(sec) + "." + std::to_string((ms % 1000) / 100) + "s";
    return std::to_string(sec / 60) + "m" + std::to_string(sec % 60) + "s";
}

#endif  // COMPATCORE34_H
