# deprefs - 第三方依赖来源清单（dependency references）

qlcomp 内的第三方/外部代码来源、许可与本地改动记录。纯自制代码（如
`limelog`、`compat34`、`emoji*` 等）不在本表。

## AES（qlcomp/aes.c + aes.h）

| 项 | 值 |
|----|----|
| 上游 | [kokke/tiny-AES-c](https://github.com/kokke/tiny-AES-c) |
| 版本 | commit `2385675`（2024-10-04） |
| 许可 | The Unlicense（public domain 声明），仓库 `unlicense.txt` / `README`: "All material in this repository is in the public domain." |
| 加入日期 | 2026-10-06 |

### 上游能力

ECC/CTR/CBC 三种模式、AES-128/192/256，密钥长度由编译期宏
`AES128/AES192/AES256` 决定；已按 NIST SP 800-38A 官方向量验证（单文件
`aes.c/aes.h`，纯 C，无依赖）。

### 本地改动

1. **密钥长度运行时化**：`Nk/Nr` 由编译期宏改为 `AES_ctx.Nr` 字段，
   `AES_init_ctx_iv` 接收 `key_bits`（128/192/256）换算 `Nr = key_bits/32 + 6`，
   `KeyExpansion/Cipher/InvCipher` 以运行时 `Nr` 迭代。
2. **只保留 CBC**：移除上游 ECB/CTR 模式分支（`AES_init_ctx`/ECB/CTR API 一并去掉）。
3. `if (i % Nk == 4)`（上游 `#if AES256` 的二次 SubWord）改为无条件保留——对
   `Nk=4/6` 该条件恒为假，行为与上游等价。
4. 其余（Sbox/Invsbox/Rcon/轮变换/CBC 拼接）与上游逐行一致。

### API

| 函数 | 说明 |
|------|------|
| `int AES_init_ctx_iv(ctx, key, key_bits, iv)` | 初始化解密/加密上下文；key_bits∈{128,192,256}，非法返回 -1，成功返回 0 |
| `void AES_ctx_set_iv(ctx, iv)` | 替换 IV（同 key 多次调用时） |
| `void AES_CBC_encrypt_buffer(ctx, buf, len)` | CBC 加密，len 须为 16 的倍数，原地可 |
| `void AES_CBC_decrypt_buffer(ctx, buf, len)` | CBC 解密，len 须为 16 的倍数，原地可 |

尾部填充（PKCS7/零填充）由调用方负责。

### 验证

- `qlcomp/test_aes.cpp`（注册于 `build_tests.sh`）：
  - FIPS-197 AES-128 单块：key `000102…0f`，iv 全零，ct `69c4e0d86a7b0430d8cdb78070b4c55a`
    → pt `00112233445566778899aabbccddeeff`；
  - FIPS-197 AES-256 单块：key `000102…1f`，iv 全零，ct `8ea2b7ca516745bfeafc49904b496089`
    → pt `001122…eef`；
  - AES-128/AES-256 + 多块 + PKCS7 回环。
- 本地实现与 fanyibot 内联解密结果可交叉对照（后者未更换，行为不变）。

## 其他第三方组件

| 组件 | 来源 | 许可 | 本地调整 |
|------|------|------|---------|
| `md5.c / md5.h` | RFC 1321，RSA Data Security, Inc. | 公共领域（文件头声明） | 少量调整以适配 C99 与固定宽度类型；ACME 文档算法未用 FIPS 模式 |
| `cJSON.c / cJSON.h` | Dave Gamble and cJSON contributors | MIT | 原样引入 |

## 约束

- 引入新三方代码时：保留原许可头，禁止移除版权声明；在本文件登记后再投入使用。
- 有价值的依赖优先以单文件纯 C 形态并入（参照 `md5.c`/`aes.c`），避免新增库链接。