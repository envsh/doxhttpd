/* 来源：github.com/redeflesq/deepseek-pow c-impl/solve.c（上游无 LICENSE，按现状 vendor）
 * 本地适配：去 <math.h>（免 -lm）、辅助函数改 static、solve → dspow_solve、
 *           宏加 DSPOW_ 前缀、solve_result/solve 声明取自 dspow_solve.h
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "dspow_solve.h"

/* ============================================================
 *  Keccak-f[1600] — раунд-функция (f4)
 * ============================================================ */

static const uint64_t RC[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL,
    0x800000000000808AULL, 0x8000000080008000ULL,
    0x000000000000808BULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008AULL, 0x0000000000000088ULL,
    0x0000000080008009ULL, 0x000000008000000AULL,
    0x000000008000808BULL, 0x800000000000008BULL,
    0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800AULL, 0x800000008000000AULL,
    0x8000000080008081ULL, 0x8000000000008080ULL,
    0x0000000080000001ULL, 0x8000000080008008ULL
};

static inline uint64_t rotl64(uint64_t x, int n) {
    return (x << n) | (x >> (64 - n));
}

/* ДОСЛОВНАЯ транслитерация funcs/w2c_original_hash_v1/w2c_original_f4.c (реальный wasm2c/IDA декомпил, перечитан целиком напрямую).
 * Маппинг оффсет -> лейн сверен 1:1 (a2+192->A[24], a2+152->A[19], ..., a2->A[0]).
 * RC читается по v93[v94+1052184]; RC[0] расположен по абс. адресу 1052184, т.е. индекс=v94/8.
 * do-while: v94 пробегает 0,8,...,176 (23 значения) => используются RC[0..22], всего 23 раунда
 * (RC[23] действительно не используется в оригинале).
 */
static void keccak_f1600(uint64_t A[25]) {
    uint64_t v5,v6,v7,v8,v9,v10,v11,v12,v13,v14,v15,v16=0,v17,v18,v19,v20;
    uint64_t v21,v22,v23,v24,v25,v26,v27,v28,v29,v30,v31,v32,v33,v34,v35;
    uint64_t v36,v37,v38,v39,v40,v41,v42,v43,v44,v45,v46,v47,v48,v49,v50;
    uint64_t v51,v52,v53,v54,v55,v56,v57,v58,v59,v60,v61,v62,v63,v64,v65;
    uint64_t v66,v67,v68,v69,v70,v71,v72,v73,v74,v75,v76,v77,v78,v79,v80;
    uint64_t v81,v82,v83,v84,v85,v86,v87,v88,v89,v90,v91,v92,v94,v98,v99,v100,v101;

    v5  = A[24]; v74 = A[19]; v78 = A[14]; v86 = A[9];
    v6  = A[4];  v7  = A[23]; v64 = A[18]; v76 = A[13]; v84 = A[8];
    v8  = A[3];  v9  = A[22]; v85 = A[17]; v10 = A[12]; v83 = A[7];
    v66 = A[2];  v11 = A[21]; v88 = A[16]; v12 = A[11]; v87 = A[6];
    v82 = A[0];  v13 = A[1];  v81 = A[20]; v14 = A[15]; v15 = A[10];
    v80 = A[5];

    do {
        v68 = v10;
        v94 = v16;
        v17 = v15 ^ v81 ^ v80 ^ v14 ^ v82;
        v71 = v66 ^ v9 ^ v85 ^ v83 ^ v10;
        v18 = v8 ^ v84 ^ v76 ^ v64 ^ v7;
        v19 = v17 ^ rotl64(v71, 1);
        v20 = v18 ^ rotl64(v17, 1);
        v91 = v5 ^ v20;
        v101 = v87 ^ v19;
        v89 = v11 ^ v19;
        v21 = v87 ^ v11 ^ v12 ^ v88;
        v22 = v6 ^ v78 ^ v5 ^ v74 ^ v86;
        v79 = v20 ^ v78;
        v61 = v20 ^ v6;
        v75 = v20 ^ v74;
        v23 = v19 ^ v12;
        v24 = v13 ^ v21;
        v25 = v19 ^ v13;
        v26 = v24 ^ rotl64(v18, 1);
        v27 = v22 ^ rotl64(v24, 1);
        v28 = v71 ^ rotl64(v22, 1);
        v29 = rotl64(v26 ^ v9, 61);
        v30 = rotl64(v86 ^ v20, 20);
        v100 = v28 ^ v84;
        v72 = v26 ^ v83;
        v31 = rotl64(v28 ^ v8, 28);
        v32 = rotl64(v88 ^ v19, 45);
        v86 = v29 ^ (v30 & ~v31);
        v33 = rotl64(v27 ^ v15, 3);
        v84 = v32 ^ (v31 & ~v29);
        v83 = v33 ^ (v29 & ~v32);
        v87 = v30 ^ (v32 & ~v33);
        v34 = v28 ^ v7;
        v77 = v28 ^ v76;
        v35 = v64 ^ v28;
        v36 = v26 ^ v66;
        v37 = v26 ^ v85;
        v67 = v68 ^ v26;
        v69 = v27 ^ v14;
        v38 = rotl64(v23, 10);
        v39 = rotl64(v34, 56);
        v65 = rotl64(v37, 15);
        v40 = rotl64(v61, 27);
        v85 = v38 ^ (v39 & ~v65);
        v41 = rotl64(v27 ^ v80, 36);
        v62 = v75;
        v88 = v41 ^ (v65 & ~v38);
        v90 = rotl64(v89, 2);
        v42 = rotl64(v36, 62);
        v80 = v31 ^ (v33 & ~v30);
        v73 = rotl64(v72, 6);
        v98 = v82 ^ v27;
        v74 = v39 ^ (v41 & ~v40);
        v99 = v40 ^ (v38 & ~v41);
        v43 = v40 & ~v39;
        v44 = rotl64(v27 ^ v81, 18);
        v45 = rotl64(v25, 1);
        v70 = rotl64(v69, 41);
        v46 = v79;
        v47 = rotl64(v62, 8);
        v78 = v44 ^ (v73 & ~v45);
        v48 = rotl64(v77, 25);
        v49 = v47 & ~v48;
        v76 = v47 ^ (v45 & ~v44);
        v50 = rotl64(v100, 55);
        v63 = v44 & ~v47;
        v51 = rotl64(v46, 39);
        v52 = v50 & ~v42;
        v11 = v50 ^ (v70 & ~v51);
        v53 = v42 & ~v90;
        v81 = v42 ^ (v51 & ~v50);
        v54 = v45 ^ (v48 & ~v73);
        v92 = rotl64(v91, 14);
        v55 = rotl64(v67, 43);
        v56 = rotl64(v35, 21);
        v66 = v55 ^ (v92 & ~v56);
        v57 = rotl64(v101, 44);
        v58 = v55 & ~v57;
        v59 = v55;
        v60 = v53;
        v12 = v73 ^ v49;
        v8  = v56 ^ (v98 & ~v92);
        v64 = v65 ^ v43;
        v5  = v90 ^ v52;
        v7  = v70 ^ v60;
        v13 = v57 ^ (v56 & ~v59);
        v15 = v54;
        v82 = v98 ^ RC[v94/8 + 1] ^ v58;
        v14 = v99;
        v9  = v51 ^ (v90 & ~v70);
        v6  = v92 ^ (v57 & ~v98);
        v10 = v48 ^ v63;
        v16 = v94 + 8;
    } while (v94 != 176);

    A[20] = v81; A[15] = v99; A[10] = v54; A[5]  = v80;
    A[21] = v11; A[16] = v88; A[11] = v12; A[6]  = v87;
    A[1]  = v13; A[22] = v9;  A[17] = v85; A[12] = v10;
    A[7]  = v83; A[2]  = v66; A[23] = v7;  A[18] = v64;
    A[13] = v76; A[8]  = v84; A[3]  = v8;  A[24] = v5;
    A[19] = v74; A[14] = v78; A[9]  = v86; A[4]  = v6;
    A[0]  = v82;
}


/* ============================================================
 *  Sha3-губка (f18 = absorb полных блоков, f13 = memcpy хвоста)
 * ============================================================ */

typedef struct {
    uint64_t state[25];
    uint8_t  buffer[DSPOW_KECCAK_RATE];
    uint32_t buffered;
} sha3_ctx;

static void sha3_init(sha3_ctx *ctx) {
    memset(ctx, 0, sizeof(*ctx));
}

static void absorb_block(sha3_ctx *ctx, const uint8_t *block) {
    const uint64_t *in = (const uint64_t *)block; // TODO: fix padding
    for (int i = 0; i < DSPOW_KECCAK_RATE / 8; i++)
        ctx->state[i] ^= in[i];
    keccak_f1600(ctx->state);
}

static void sha3_update(sha3_ctx *ctx, const uint8_t *data, uint32_t len) {
    if (ctx->buffered) {
        uint32_t take = DSPOW_KECCAK_RATE - ctx->buffered;
        if (take > len) take = len;
        memcpy(ctx->buffer + ctx->buffered, data, take);
        ctx->buffered += take;
        data += take; len -= take;
        if (ctx->buffered == DSPOW_KECCAK_RATE) {
            absorb_block(ctx, ctx->buffer);
            ctx->buffered = 0;
        }
    }
    while (len >= DSPOW_KECCAK_RATE) {
        absorb_block(ctx, data);
        data += DSPOW_KECCAK_RATE;
        len  -= DSPOW_KECCAK_RATE;
    }
    if (len) {
        memcpy(ctx->buffer, data, len);
        ctx->buffered = len;
    }
}

static void sha3_final_copy(sha3_ctx ctx, uint8_t digest[DSPOW_DIGEST_LEN]) {
    uint8_t block[DSPOW_KECCAK_RATE];
    memset(block, 0, DSPOW_KECCAK_RATE);
    memcpy(block, ctx.buffer, ctx.buffered);
    block[ctx.buffered]    ^= 0x06;
    block[DSPOW_KECCAK_RATE - 1] ^= 0x80;
    absorb_block(&ctx, block);
    memcpy(digest, ctx.state, DSPOW_DIGEST_LEN);
}

/* ============================================================
 *  Вспомогательные функции
 * ============================================================ */

static int nonce_to_ascii(uint64_t nonce, char out[20]) {
    char tmp[20];
    int n = 0;
    if (nonce == 0) { out[0] = '0'; return 1; }
    while (nonce) { tmp[n++] = (char)('0' + (nonce % 10)); nonce /= 10; }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    return n;
}

static uint8_t *hex_decode(const char *hex, uint32_t len, uint32_t *out_len) {
    if (len & 1) return NULL;
    uint32_t n = len / 2;
    uint8_t *buf = (uint8_t *)malloc(n);
    if (!buf) return NULL;
    for (uint32_t i = 0; i < n; i++) {
        int hi = (unsigned char)hex[2*i];
        int lo = (unsigned char)hex[2*i + 1];
        int hv = (hi >= '0' && hi <= '9') ? hi - '0' : (hi | 0x20) - 'a' + 10;
        int lv = (lo >= '0' && lo <= '9') ? lo - '0' : (lo | 0x20) - 'a' + 10;
        if ((unsigned)hv > 15 || (unsigned)lv > 15) { free(buf); return NULL; }
        buf[i] = (uint8_t)((hv << 4) | lv);
    }
    *out_len = n;
    return buf;
}

/* ============================================================
 *  dspow_solve — основная функция（solve_result 取自 dspow_solve.h）
 * ============================================================ */

solve_result dspow_solve(const char *target_hex, uint32_t target_hex_len,
                         const uint8_t *prefix, uint32_t prefix_len,
                         double difficulty)
{
    solve_result res = { 0, 0.0 };

    /* 整数性检查（原版用 ceil(difficulty) != difficulty；去 math.h 后
     * 先转 uint64 再转回 double 比较，配合上方 <2^53 检查等价） */
    uint64_t rounded_u;
    if (difficulty >= 9.007199254740991e15
        || !(difficulty == difficulty)
        || difficulty <= 0.0) {
        return res;
    }
    rounded_u = (uint64_t)difficulty;
    if ((double)rounded_u != difficulty) {
        return res;
    }

    uint32_t target_len = 0;
    uint8_t *target = hex_decode(target_hex, target_hex_len, &target_len);
	
    if (!target) return res;
	
    uint64_t max_attempts;
    if (difficulty >= 9223372036854775808.0) max_attempts = UINT64_MAX;
    else max_attempts = (uint64_t)difficulty;

    if (max_attempts == 0 || target_len != DSPOW_DIGEST_LEN) {
        free(target);
        return res;
    }

    sha3_ctx base_ctx;
    sha3_init(&base_ctx);
    sha3_update(&base_ctx, prefix, prefix_len);

    uint64_t nonce;
    bool found = false;
    for (nonce = 0; nonce < max_attempts; nonce++) {
        char nonce_str[20];
        int  nonce_len = nonce_to_ascii(nonce, nonce_str);

        sha3_ctx ctx = base_ctx;
        sha3_update(&ctx, (const uint8_t *)nonce_str, nonce_len);

        uint8_t digest[DSPOW_DIGEST_LEN];
        sha3_final_copy(ctx, digest);

        if (memcmp(digest, target, DSPOW_DIGEST_LEN) == 0) {
            found = true;
            break;
        }
    }

    free(target);

    res.found = found;
	res.attempts = max_attempts;
    res.nonce = nonce;
	
    return res;
}