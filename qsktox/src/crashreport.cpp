#include "crashreport.h"

#ifdef Q_OS_ANDROID

// ── 设计要点（对照 Breakpad / xCrash 网搜核对后的定稿）──
// 1. handler 主核严格 async-signal-safe：无 malloc、无 stdio（stdio 锁可能被
//    其它线程持有，fork 后子进程再碰会死锁）；只用 write/open/pread/read。
// 2. 崩溃现场 ucontext 在 handler 内 memcpy 到静态 g_ctx，collector 子进程
//    直接读取（fork 镜像，内容即崩溃瞬间状态）。
// 3. handler 结尾恢复 sigaction 保存的 oldact 并 return（不手动 raise）：
//    信号自动解除屏蔽被内核重投递 → 原链路（debuggerd）tombstone 照常生成。
// 4. fp-chain 式还原依赖 -fno-omit-frame-pointer（NDK arm64 release 默认
//    -fomit-frame-pointer，见 build.gradle/CMake 追加编译选项）；栈扫描兜底。
// 5. bionic aarch64 ucontext：uc_mcontext.regs[0..30] + 独立 .sp/.pc 字段。

#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/ucontext.h>

#include <cstdint>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfoList>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

namespace CrashReport {
namespace {

constexpr int kNSignals = 5;
const int kSignals[kNSignals] = { SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL };

constexpr int kMaxFrames = 96;
constexpr uint64_t kScanRange = 48 * 1024;
constexpr int kMaxSegs = 96;
constexpr int kPathLen = 160;
constexpr int kMapsBufSize = 256 * 1024;

struct ExecSeg {
    uint64_t start;
    uint64_t end;
    uint64_t fileOff;
    char path[kPathLen];
};

int g_logFd = -1;
struct sigaction g_oldAct[kNSignals];
bool g_hasOld[kNSignals] = {};
char g_altStack[16 * 1024] __attribute__((aligned(16)));
ucontext_t g_ctx; // 崩溃现场快照：handler 写入，collector 子进程读取
volatile sig_atomic_t g_active = 0;

// collector（子进程）专用，handler 内不触碰
ExecSeg g_segs[kMaxSegs];
int g_nSeg = 0;
char g_mapsBuf[kMapsBufSize];

// ── 最小格式化输出：只依赖 write()，循环处理短写 ──

void wStr(const char* s)
{
    if (g_logFd < 0 || !s) return;
    const size_t len = strlen(s);
    size_t off = 0;
    while (off < len) {
        const ssize_t n = write(g_logFd, s + off, len - off);
        if (n <= 0) return;
        off += (size_t)n;
    }
}

void wDec(uint64_t v)
{
    char buf[24];
    int pos = 23;
    buf[pos] = 0;
    do {
        buf[--pos] = char('0' + (v % 10));
        v /= 10;
    } while (v > 0);
    wStr(&buf[pos]);
}

void wHex(uint64_t v)
{
    char buf[24];
    int pos = 23;
    buf[pos] = 0;
    if (v == 0) {
        buf[--pos] = '0';
    }
    while (v > 0) {
        const int d = int(v & 0xf);
        buf[--pos] = char(d < 10 ? '0' + d : 'a' + d - 10);
        v >>= 4;
    }
    buf[--pos] = 'x';
    buf[--pos] = '0';
    wStr(&buf[pos]);
}

void wHex16(uint64_t v)
{
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; ++i) {
        const int d = int((v >> (60 - i * 4)) & 0xf);
        buf[2 + i] = char(d < 10 ? '0' + d : 'a' + d - 10);
    }
    buf[18] = 0;
    wStr(buf);
}

bool peek(int memFd, uint64_t addr, uint64_t* out)
{
    if (memFd < 0) return false;
    return pread(memFd, out, sizeof(uint64_t), (off_t)addr)
        == (ssize_t)sizeof(uint64_t);
}

int readProcFile(const char* path, char* buf, int cap)
{
    const int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    int total = 0;
    while (total < cap) {
        const ssize_t n = read(fd, buf + total, cap - total);
        if (n <= 0) break;
        total += int(n);
    }
    close(fd);
    return total;
}

// 只保留带路径的可执行段（perms 含 x），供 findSeg / backtrace 符号化
void parseMaps(char* buf, int len)
{
    g_nSeg = 0;
    char* line = buf;
    char* end = buf + len;
    while (line < end && g_nSeg < kMaxSegs) {
        char* nl = (char*)memchr(line, '\n', size_t(end - line));
        char* lineEnd = nl ? nl : end;
        *lineEnd = 0;

        unsigned long start = 0, endAddr = 0, fileOff = 0;
        unsigned long devMaj = 0, devMin = 0, inode = 0;
        char perms[8] = { 0 };
        int consumed = 0;
        if (sscanf(line, "%lx-%lx %4s %lx %lx:%lx %lu %n",
                   &start, &endAddr, perms, &fileOff, &devMaj, &devMin,
                   &inode, &consumed) >= 7 && perms[2] == 'x') {
            const char* path = line + consumed;
            while (*path == ' ') ++path;
            if (path[0] == '/') {
                const char* q = path;
                while (*q != 0 && q - path < kPathLen - 1) ++q;
                ExecSeg* seg = &g_segs[g_nSeg++];
                seg->start = start;
                seg->end = endAddr;
                seg->fileOff = fileOff;
                memcpy(seg->path, path, size_t(q - path));
                seg->path[q - path] = 0;
            }
        }
        line = nl ? nl + 1 : end;
    }
}

const ExecSeg* findSeg(uint64_t pc)
{
    for (int i = 0; i < g_nSeg; ++i) {
        if (pc >= g_segs[i].start && pc < g_segs[i].end) return &g_segs[i];
    }
    return nullptr;
}

// "#N pc <file-rel-addr> <lib>" —— file-rel-addr 直接可被 llvm-symbolizer
// 配对未 strip 的 .so 符号化
void wFrameLine(int idx, uint64_t pc)
{
    wStr("#");
    wDec(uint64_t(idx));
    wStr(" pc ");
    const ExecSeg* seg = findSeg(pc);
    if (seg) {
        wHex(pc - seg->start + seg->fileOff);
        wStr(" ");
        wStr(seg->path);
    } else {
        wHex(pc);
        wStr(" (unmapped)");
    }
    wStr("\n");
}

void dumpRegs()
{
    const mcontext_t* mc = &g_ctx.uc_mcontext;
    wStr("regs:\n");
    for (int i = 0; i < 31; ++i) {
        wStr("x");
        wDec(uint64_t(i));
        wStr("=");
        wHex(mc->regs[i]);
        wStr((i % 4 == 3) ? "\n" : "  ");
    }
    wStr("sp=");
    wHex(mc->sp);
    wStr(" pc=");
    wHex(mc->pc);
    wStr(" fp(x29)=");
    wHex(mc->regs[29]);
    wStr(" lr(x30)=");
    wHex(mc->regs[30]);
    wStr("\n");
}

// 两层还原：先 fp 链（x29→next_fp、lr@fp+8），再栈扫描兜底
void collectFrames(int memFd)
{
    const mcontext_t* mc = &g_ctx.uc_mcontext;
    const uint64_t pc = mc->pc;
    const uint64_t sp = mc->sp;
    uint64_t fp = mc->regs[29];
    const uint64_t lr = mc->regs[30];

    wStr("backtrace fp-chain:\n");
    int n = 0;
    wFrameLine(n++, pc);
    // pc 为野指针时，lr 是最近一个有效返回地址
    wFrameLine(n++, lr);
    while (n < kMaxFrames) {
        uint64_t nextFp = 0, nextLr = 0;
        if (!peek(memFd, fp, &nextFp)) break;
        if (!peek(memFd, fp + 8, &nextLr)) break;
        if (nextFp <= fp) break;
        if (nextLr == 0) break;
        wFrameLine(n++, nextLr);
        fp = nextFp;
    }

    // 兜底：从 sp 起扫描栈内存，命中代码段的值作为候选帧（有少量误报）
    wStr("stack-scan candidates:\n");
    const uint64_t base = sp & ~uint64_t(7);
    int m = 0;
    int failRun = 0;
    uint64_t prev = 0;
    for (uint64_t a = base; m < kMaxFrames && a < base + kScanRange; a += 8) {
        uint64_t v = 0;
        if (!peek(memFd, a, &v)) {
            if (++failRun > 16) break;
            continue;
        }
        failRun = 0;
        if (findSeg(v) && v != prev && v != pc && v != lr) {
            wFrameLine(m++, v);
            prev = v;
        }
    }
}

void dumpMemoryNear(int memFd, uint64_t addr)
{
    wStr("memory near pc:\n");
    const uint64_t start = (addr >= 128) ? addr - 128 : 0;
    for (uint64_t a = start; a < addr + 128; a += 8) {
        uint64_t v = 0;
        wHex(a);
        wStr(": ");
        if (peek(memFd, a, &v)) {
            wHex16(v);
            wStr("\n");
        } else {
            wStr("<unreadable>\n");
        }
    }
}

void dumpMapsRaw(char* buf, int len)
{
    wStr("maps:\n");
    size_t off = 0;
    while (off < size_t(len)) {
        const ssize_t n = write(g_logFd, buf + off, size_t(len) - off);
        if (n <= 0) return;
        off += size_t(n);
    }
    wStr("\n");
}

void collect()
{
    const int memFd = open("/proc/self/mem", O_RDONLY);

    dumpRegs();

    const int len = readProcFile("/proc/self/maps", g_mapsBuf, kMapsBufSize);
    if (len > 0) {
        parseMaps(g_mapsBuf, len);
        dumpMapsRaw(g_mapsBuf, len);
    }

    collectFrames(memFd);
    dumpMemoryNear(memFd, g_ctx.uc_mcontext.pc);

    if (memFd >= 0) close(memFd);
    wStr("=== END CRASH REPORT ===\n");
}

void handler(int sig, siginfo_t* info, void* uc)
{
    if (g_logFd >= 0 && g_active && uc) {
        g_active = 0;
        memcpy(&g_ctx, static_cast<ucontext_t*>(uc), sizeof(g_ctx));

        wStr("\n=== NATIVE CRASH ===\nsig=");
        wDec(uint64_t(sig));
        wStr(" code=");
        wDec(uint64_t(info->si_code));
        wStr(" fault-addr=");
        wHex(uint64_t(uintptr_t(info->si_addr)));
        wStr(" pid=");
        wDec(uint64_t(getpid()));
        wStr(" tid=");
        wDec(uint64_t(syscall(SYS_gettid)));
        wStr("\n");

        const pid_t child = fork();
        if (child == 0) {
            // collector 子进程：脱离 handler 的 async-signal-safe 限制；
            // 自身再崩只截止本进程（SIG_DFL），不影响父进程 tombstone
            sigset_t none;
            sigemptyset(&none);
            sigprocmask(SIG_SETMASK, &none, nullptr);
            for (int i = 0; i < kNSignals; ++i) {
                signal(kSignals[i], SIG_DFL);
            }
            collect();
            _exit(0);
        }
        if (child < 0) {
            wStr("fork failed; minimal regs:\n");
            dumpRegs();
        }
    }

    // 恢复原 handler 后返回：内核重投递交给 debuggerd 管道，tombstone 照常
    for (int i = 0; i < kNSignals; ++i) {
        if (kSignals[i] == sig) {
            if (g_hasOld[i]) sigaction(sig, &g_oldAct[i], nullptr);
            break;
        }
    }
}

} // namespace

void install()
{
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation) + "/crash";
    QDir().mkpath(dir);

    // 仅保留最新 5 份报告
    const QFileInfoList old = QDir(dir).entryInfoList(
        QStringList{ "crash-*.log" }, QDir::Files, QDir::Time);
    for (int i = 5; i < old.size(); ++i) {
        QFile::remove(old[i].absoluteFilePath());
    }

    const QString path = dir + QString("/crash-%1-%2.log")
        .arg(QDateTime::currentSecsSinceEpoch())
        .arg(QCoreApplication::applicationPid());
    const QByteArray fileName = path.toUtf8();
    g_logFd = open(fileName.constData(), O_WRONLY | O_CREAT | O_APPEND, 0664);
    if (g_logFd < 0) {
        qWarning() << "[crashreport] open log failed:" << path;
        return;
    }
    wStr("[crashreport] installed\n");
    qDebug() << "[crashreport] log file:" << path;

    // 栈溢出类崩溃的 handler 也要有自己的栈：SA_ONSTACK + sigaltstack
    stack_t ss;
    ss.ss_sp = g_altStack;
    ss.ss_size = sizeof(g_altStack);
    ss.ss_flags = 0;
    sigaltstack(&ss, nullptr);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = handler;
    sigemptyset(&sa.sa_mask);
    for (int i = 0; i < kNSignals; ++i) {
        sigaddset(&sa.sa_mask, kSignals[i]);
    }
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;

    for (int i = 0; i < kNSignals; ++i) {
        if (sigaction(kSignals[i], &sa, &g_oldAct[i]) == 0) {
            g_hasOld[i] = true;
        }
    }
    g_active = 1;
    qDebug() << "[crashreport] handler installed";
}

} // namespace CrashReport

#else // !Q_OS_ANDROID

#include <QDebug>

namespace CrashReport {

void install()
{
    qDebug() << "[crashreport] not Android, skip";
}

} // namespace CrashReport

#endif
