#include "doctest.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>

extern "C" {
#include "../qlcomp/aes.c"
}

static std::string bytesFromHex(const char* hex) {
    std::string out;
    while (*hex && hex[1]) {
        char hi = *hex, lo = hex[1];
        auto cv = [](char c) -> unsigned char {
            if (c >= '0' && c <= '9') return (unsigned char)(c - '0');
            if (c >= 'a' && c <= 'f') return (unsigned char)(c - 'a' + 10);
            if (c >= 'A' && c <= 'F') return (unsigned char)(c - 'A' + 10);
            return 0;
        };
        out.push_back((char)((cv(hi) << 4) | cv(lo)));
        hex += 2;
    }
    return out;
}

static std::string toHex(const uint8_t* p, size_t n) {
    std::string out;
    char buf[4];
    for (size_t i = 0; i < n; i++) {
        sprintf(buf, "%02x", p[i]);
        out += buf;
    }
    return out;
}

TEST_CASE("AES - FIPS-197 single block vectors") {
    // FIPS-197 附录 C.1/C.3 为单块已知答案；CBC 下等价于零 IV 单块。
    std::string zeroIv = bytesFromHex("00000000000000000000000000000000");

    SUBCASE("AES-128: key 00..0f, pt 00..ef -> ct 69c4e0d86a7b0430d8cdb78070b4c55a") {
        std::string key = bytesFromHex("000102030405060708090a0b0c0d0e0f");
        std::string pt  = bytesFromHex("00112233445566778899aabbccddeeff");
        std::string ct  = bytesFromHex("69c4e0d86a7b0430d8cdb78070b4c55a");

        uint8_t buf[16];
        AES_ctx ctx;
        memcpy(buf, pt.data(), 16);
        CHECK(AES_init_ctx_iv(&ctx, (const uint8_t*)key.data(), 128, (const uint8_t*)zeroIv.data()) == 0);
        AES_CBC_encrypt_buffer(&ctx, buf, 16);
        CHECK(toHex(buf, 16) == toHex((const uint8_t*)ct.data(), 16));

        memcpy(buf, ct.data(), 16);
        CHECK(AES_init_ctx_iv(&ctx, (const uint8_t*)key.data(), 128, (const uint8_t*)zeroIv.data()) == 0);
        AES_CBC_decrypt_buffer(&ctx, buf, 16);
        CHECK(toHex(buf, 16) == toHex((const uint8_t*)pt.data(), 16));
    }

    SUBCASE("AES-256: key 00..1f, pt 00..ef -> ct 8ea2b7ca516745bfeafc49904b496089") {
        std::string key = bytesFromHex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
        std::string pt  = bytesFromHex("00112233445566778899aabbccddeeff");
        std::string ct  = bytesFromHex("8ea2b7ca516745bfeafc49904b496089");

        uint8_t buf[16];
        AES_ctx ctx;
        memcpy(buf, pt.data(), 16);
        CHECK(AES_init_ctx_iv(&ctx, (const uint8_t*)key.data(), 256, (const uint8_t*)zeroIv.data()) == 0);
        AES_CBC_encrypt_buffer(&ctx, buf, 16);
        CHECK(toHex(buf, 16) == toHex((const uint8_t*)ct.data(), 16));

        memcpy(buf, ct.data(), 16);
        CHECK(AES_init_ctx_iv(&ctx, (const uint8_t*)key.data(), 256, (const uint8_t*)zeroIv.data()) == 0);
        AES_CBC_decrypt_buffer(&ctx, buf, 16);
        CHECK(toHex(buf, 16) == toHex((const uint8_t*)pt.data(), 16));
    }
}

TEST_CASE("AES - PKCS7 multi-block CBC (openssl reference)") {
    // plaintext: "The quick brown fox jumps over the lazy dog. 0123456789\n"
    // 48 bytes -> PKCS7 pads a full 0x10 block -> 3 blocks of ciphertext
    std::string pt = bytesFromHex(
        "54686520517569636b2062726f776e20666f78206a756d7073206f76657220746865206c617a7920646f672e20303132333435363738390a");

    SUBCASE("AES-128") {
        std::string key = bytesFromHex("000102030405060708090a0b0c0d0e0f");
        std::string iv  = bytesFromHex("101112131415161718191a1b1c1d1e1f");
        std::string ct  = bytesFromHex(
            "9e51f3f395042089d1b6ee8d5b84891a31ed170aedcb1c745226767c19026dff4ee141c39a1e154304865ddb844b52888369567b2269bb4479b7f7325fc59219");

        uint8_t enc[48], dec[48];
        AES_ctx ctx;
        memcpy(enc, pt.data(), 48);
        CHECK(AES_init_ctx_iv(&ctx, (const uint8_t*)key.data(), 128, (const uint8_t*)iv.data()) == 0);
        AES_CBC_encrypt_buffer(&ctx, enc, 48);
        CHECK(toHex(enc, 48) == toHex((const uint8_t*)ct.data(), 48));

        memcpy(dec, ct.data(), 48);
        CHECK(AES_init_ctx_iv(&ctx, (const uint8_t*)key.data(), 128, (const uint8_t*)iv.data()) == 0);
        AES_CBC_decrypt_buffer(&ctx, dec, 48);
        CHECK(toHex(dec, 48) == toHex((const uint8_t*)pt.data(), 48));
    }

    SUBCASE("AES-256") {
        std::string key = bytesFromHex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
        std::string iv  = bytesFromHex("202122232425262728292a2b2c2d2e2f");
        std::string ct  = bytesFromHex(
            "81922f929058f3a3cb3acd92f2ac52325ed804a6f5a21e14c8e8dd2029856876d899935c4f61e99cc23c01a4dc2cfed0723f847f3eb6a34724ba9c761c01d445");

        uint8_t enc[48], dec[48];
        AES_ctx ctx;
        memcpy(enc, pt.data(), 48);
        CHECK(AES_init_ctx_iv(&ctx, (const uint8_t*)key.data(), 256, (const uint8_t*)iv.data()) == 0);
        AES_CBC_encrypt_buffer(&ctx, enc, 48);
        CHECK(toHex(enc, 48) == toHex((const uint8_t*)ct.data(), 48));

        memcpy(dec, ct.data(), 48);
        CHECK(AES_init_ctx_iv(&ctx, (const uint8_t*)key.data(), 256, (const uint8_t*)iv.data()) == 0);
        AES_CBC_decrypt_buffer(&ctx, dec, 48);
        CHECK(toHex(dec, 48) == toHex((const uint8_t*)pt.data(), 48));
    }
}

TEST_CASE("AES - roundtrip and API") {
    SUBCASE("AES-192 roundtrip multi-block") {
        uint8_t key[24], iv[16], buf[48];
        for (int i = 0; i < 24; i++) key[i] = (uint8_t)i;
        for (int i = 0; i < 16; i++) iv[i] = (uint8_t)(0x80 + i);
        for (int i = 0; i < 48; i++) buf[i] = (uint8_t)(i * 7 + 3);

        uint8_t orig[48];
        memcpy(orig, buf, 48);
        AES_ctx ctx;
        CHECK(AES_init_ctx_iv(&ctx, key, 192, iv) == 0);
        AES_CBC_encrypt_buffer(&ctx, buf, 48);
        CHECK(memcmp(buf, orig, 48) != 0);
        CHECK(AES_init_ctx_iv(&ctx, key, 192, iv) == 0);
        AES_CBC_decrypt_buffer(&ctx, buf, 48);
        CHECK(memcmp(buf, orig, 48) == 0);
    }

    SUBCASE("invalid key_bits rejected") {
        uint8_t key[32] = {0};
        uint8_t iv[16]  = {0};
        AES_ctx ctx;
        CHECK(AES_init_ctx_iv(&ctx, key, 100, iv) == -1);
        CHECK(AES_init_ctx_iv(&ctx, key, 0, iv) == -1);
    }

    SUBCASE("set_iv reuses key") {
        uint8_t key[16];
        std::string pt = bytesFromHex("00112233445566778899aabbccddeeff");
        char ivBytes[16];
        for (int i = 0; i < 16; i++) ivBytes[i] = (char)i;
        uint8_t enc[16];
        AES_ctx ctx;
        for (int i = 0; i < 16; i++) key[i] = (uint8_t)i;
        memcpy(enc, pt.data(), 16);
        CHECK(AES_init_ctx_iv(&ctx, key, 128, (const uint8_t*)ivBytes) == 0);
        AES_CBC_encrypt_buffer(&ctx, enc, 16);
        std::string ct1 = toHex(enc, 16);

        memcpy(enc, pt.data(), 16);
        CHECK(AES_init_ctx_iv(&ctx, key, 128, (const uint8_t*)ivBytes) == 0);
        AES_ctx_set_iv(&ctx, (const uint8_t*)ivBytes);
        AES_CBC_encrypt_buffer(&ctx, enc, 16);
        CHECK(toHex(enc, 16) == ct1);
    }
}