#ifndef DSPOW_SOLVE_H
#define DSPOW_SOLVE_H

/* DeepSeekHashV1 PoW 求解器（DeepSeek web 端 x-ds-pow-response）
 *
 * 来源：https://github.com/redeflesq/deepseek-pow  c-impl/solve.c|.h
 * 上游无 LICENSE 文件（仅 DISCLAIMER）—— 经确认按现状 vendor，仅改动：
 *   - 宏/函数名加 dspow_ 前缀避免冲突；solve → dspow_solve
 *   - 内部辅助函数改 static；.c 去掉 <math.h>（避免 -lm）
 *   - 头文件加 extern "C"
 * 算法本身（Keccak-f[1600] 跳过 round0、23 轮变体、rate=136、padding 0x06+0x80）
 * 为 DeepSeekHashV1 规范实现，见 docs/aigptbot-web-sources.md §6.1。
 */

#include <stdint.h>

#define DSPOW_KECCAK_RATE   136   /* SHA3-256 rate, byte (1088 bit) */
#define DSPOW_DIGEST_LEN    32    /* SHA3-256 -> 32 bytes */

/**
 * Result of a nonce search.
 */
typedef struct {
    int      found;      /**< 1 if a matching nonce was found, 0 otherwise */
    double   attempts;   /**< actual number of attempts performed (equals max_attempts) */
    uint64_t nonce;      /**< the nonce that produced the target digest (only valid if found == 1) */
} solve_result;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Brute-force a SHA3-256 nonce.
 *
 * Iterates nonce from 0 to difficulty-1, computing SHA3-256(prefix || decimal_nonce)
 * using a **custom**, self-contained Keccak-f[1600] implementation (not OpenSSL or similar).
 * If the resulting digest equals `target_hex`, the search stops and returns the winning nonce.
 *
 * @param target_hex      hexadecimal string of the 32‑byte target digest (64 characters)
 * @param target_hex_len  length of `target_hex` (must be 64)
 * @param prefix          binary data prepended to the nonce
 * @param prefix_len      length of `prefix` in bytes
 * @param difficulty      maximum number of attempts; must be a positive integer representable
 *                        as uint64_t (fractional values are rejected, values > 2^63-1 are clamped
 *                        to UINT64_MAX if the double is >= 9.22e18)
 * @return solve_result with `found` set to 1 if a matching nonce was found, otherwise 0.
 */
solve_result dspow_solve(const char *target_hex, uint32_t target_hex_len,
                         const uint8_t *prefix, uint32_t prefix_len,
                         double difficulty);

#ifdef __cplusplus
}
#endif

#endif /* DSPOW_SOLVE_H */
