#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <dlfcn.h>
#include <cxxabi.h>
#include <execinfo.h>
#include <unistd.h>

#ifdef QT3_BUILD

static const char* demangleSym(const char* raw) {
    if (!raw) { return "??"; }
    const char* paren = strchr(raw, '(');
    const char* plus  = strchr(raw, '+');
    const char* name = raw;
    if (paren && plus && paren < plus) {
        size_t len = plus - paren - 1;
        char* buf = (char*)alloca(len + 1);
        strncpy(buf, paren + 1, len);
        buf[len] = '\0';
        name = buf;
    }
    int status;
    char* dem = abi::__cxa_demangle(name, nullptr, nullptr, &status);
    if (dem) {
        char* p = strchr(dem, '(');
        if (p) { *p = '\0'; }
    }
    return dem ? dem : name;
}

static const char* callerBacktrace() {
    void* stack[3];
    int n = backtrace(stack, 3);
    char** syms = backtrace_symbols(stack, n);
    const char* result = (syms && n >= 2) ? demangleSym(syms[2]) : "??";
    free(syms);
    return result;
}

static const char* callerRetaddr() {
    void* addr = __builtin_return_address(2);
    Dl_info info;
    if (dladdr(addr, &info) && info.dli_sname) {
        return demangleSym(info.dli_sname);
    }
    return "??";
}

#define USE_RETADDR

static void __attribute__((noinline)) logWithCaller(const char* label, const char* fmt, va_list ap) {
    char buf[4096];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);

    if ((size_t)n >= sizeof(buf)) {
        char marker[64];
        int ml = snprintf(marker, sizeof(marker), " [Truncated %d]", n);
        if (ml > 0 && (size_t)ml < sizeof(buf))
            strcpy(buf + sizeof(buf) - 1 - ml, marker);
    }

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char tbuf[16];
    strftime(tbuf, sizeof(tbuf), "%H:%M:%S", t);

#ifdef USE_RETADDR
    const char* caller = callerRetaddr();
#else
    const char* caller = callerBacktrace();
#endif

    static bool s_tty = isatty(fileno(stderr)) != 0;

    const char* labelColor = "";
    const char* dimOn  = "";
    const char* dimOff = "";
    if (s_tty) {
        if (strcmp(label, "DEBUG") == 0) labelColor = "\033[2m";
        else if (strcmp(label, "INFO") == 0) labelColor = "\033[1;32m";
        else if (strcmp(label, "WARN") == 0) labelColor = "\033[1;33m";
        else if (strcmp(label, "FATAL") == 0) labelColor = "\033[1;31m";
        dimOn  = "\033[2m";
        dimOff = "\033[0m";
    }

    fprintf(stderr, "%s[%s]%s %s[%s]\033[0m %s %s[from %s]%s\n",
            dimOn, tbuf, dimOff, labelColor, label, buf, dimOn, caller, dimOff);
}

void qDebug(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    logWithCaller("DEBUG", fmt, ap);
    va_end(ap);
}

void qWarning(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    logWithCaller("WARN", fmt, ap);
    va_end(ap);
}

void qFatal(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    logWithCaller("FATAL", fmt, ap);
    va_end(ap);
    abort();
}

void qInfo(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    logWithCaller("INFO", fmt, ap);
    va_end(ap);
}

#else  // ===== Qt4: 消息处理器 =====
#include <qglobal.h>
#include <cstdio>
#include <ctime>

static void qt4MsgHandler(QtMsgType type, const char* msg) {
    const char* level = "";
    switch (type) {
        case QtDebugMsg:    level = "DEBUG"; break;
        case QtWarningMsg:  level = "WARN";  break;
        case QtCriticalMsg: level = "ERROR"; break;
        case QtFatalMsg:    level = "FATAL"; break;
    }
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%H:%M:%S", t);
    fprintf(stderr, "[%s] [%s] %s\n", tbuf, level, msg);
    fflush(stderr);
    if (type == QtFatalMsg) abort();
}

struct Qt4MsgInit {
    Qt4MsgInit() {
		#if QT_VERSION < 0x050000
			qInstallMsgHandler(qt4MsgHandler);
		#elif QT_VERSION < 0x060000
			// TODO
			// qInstallMessageHandler(qt4MsgHandler);
		#endif
	}
};
static Qt4MsgInit g_qt4MsgInit;

#endif
