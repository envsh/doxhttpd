#pragma once

namespace CrashReport {

// 进程内 native crash handler（Android）：
// fork collector 子进程落文件（fp-chain + 栈扫描还原 backtrace + maps + regs），
// 主流程恢复原 handler 后返回，内核重投递 → debuggerd tombstone 照常生成。
// 非 Android 平台为空实现。
void install();

} // namespace CrashReport
