/* aes.h - AES-128/192/256，CBC 模式加解密（运行时 key 长度）
 *
 * 改编自 kokke/tiny-AES-c（commit 2385675），仓库以 The Unlicense 发布：
 *    https://github.com/kokke/tiny-AES-c
 * 见 qlcomp/docs/deprefs.md 的来源说明。
 *
 * 本地改动（尽量小）：
 *   - 密钥长度由编译期宏（AES128/AES192/AES256）改为运行时参数 key_bits，
 *     轮数 Nr 存入 AES_ctx，KeyExpansion/Cipher/InvCipher 以 Nr 变量运行；
 *   - 只保留 CBC 模式（上游 ECB/CTR 分支移除）。
 */
#ifndef AES_H
#define AES_H

#include <stdint.h>
#include <stddef.h>

/* AES 块长固定为 128 位（16 字节），与密钥长度无关 */
#define AES_BLOCKLEN 16

/* 最大密钥扩展空间：AES-256 为 60 字 = 240 字节 */
#define AES_keyExpSize 240

struct AES_ctx
{
  uint8_t RoundKey[AES_keyExpSize];
  uint8_t Iv[AES_BLOCKLEN];
  uint8_t Nr;   /* 轮数：AES-128=10 / AES-192=12 / AES-256=14 */
};

/* 初始化解密/加密上下文。key_bits 仅接受 128 / 192 / 256；
 * 非法值返回 -1，成功返回 0。
 * iv 长度必须为 AES_BLOCKLEN。 */
int AES_init_ctx_iv(struct AES_ctx* ctx, const uint8_t* key, int key_bits,
                    const uint8_t* iv);

/* 替换 IV（连续多次调用同一 key 时使用） */
void AES_ctx_set_iv(struct AES_ctx* ctx, const uint8_t* iv);

/* CBC 加解密缓冲区。length 必须是 AES_BLOCKLEN 的整数倍；
 * 加解密均可原地进行（buf 既当输入也当输出）。
 * 尾块填充（如 PKCS7/零填充）由调用方负责。 */
void AES_CBC_encrypt_buffer(struct AES_ctx* ctx, uint8_t* buf, size_t length);
void AES_CBC_decrypt_buffer(struct AES_ctx* ctx, uint8_t* buf, size_t length);

#endif /* AES_H */