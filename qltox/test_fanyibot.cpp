#include "doctest.h"

#include <qapplication.h>
#include <qmutex.h>
#include <qthread.h>

#include "fanyibot.h"
#include "fanyibot.cpp"

static std::string bytesFromHex(const char* h) {
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') { return c - '0'; }
        if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
        if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
        return 0;
    };
    std::string out;
    for (size_t i = 0; h[i] && h[i + 1]; i += 2) {
        out += (char)((val(h[i]) << 4) | val(h[i + 1]));
    }
    return out;
}

static std::string toHex(const std::string& b) {
    static const char* hex = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < b.size(); i++) {
        out += hex[(unsigned char)b[i] >> 4];
        out += hex[(unsigned char)b[i] & 0xF];
    }
    return out;
}

static std::string b64urlFromChars(char a, char b) {
    std::string s;
    s += a;
    s += b;
    return s;
}

TEST_CASE("fanyibotEngines 基本结构") {
    const auto& v = fanyibotEngines();
    CHECK(v.size() == 5);
    CHECK(std::string(v[0].name) == "msedge");
    CHECK(std::string(v[1].name) == "google");
    CHECK(std::string(v[2].name) == "youdao");
    CHECK(std::string(v[3].name) == "yandex");
    CHECK(std::string(v[4].name) == "deepl");
    CHECK(v[4].unsupported == true);
}

TEST_CASE("fanyibotLangCode 映射") {
    CHECK(fanyibotLangCode(kFanyibotMsedge, "中文") == "zh-Hans");
    CHECK(fanyibotLangCode(kFanyibotGoogle, "中文") == "zh-CN");
    CHECK(fanyibotLangCode(kFanyibotYoudao, "中文") == "zh-CHS");
    CHECK(fanyibotLangCode(kFanyibotYandex, "中文") == "zh");
    CHECK(fanyibotLangCode(kFanyibotDeepl, "English").empty());
}

TEST_CASE("buildCandidates all 固定顺序") {
    FanyibotRequest r;
    r.engine = "all";
    r.toLang = "English";
    std::string reason;
    auto c = buildCandidates(r, reason);
    CHECK(reason.empty());
    CHECK(c.size() == 4);
}

TEST_CASE("buildCandidates any 非空") {
    FanyibotRequest r;
    r.engine = "any";
    r.toLang = "English";
    std::string reason;
    auto c = buildCandidates(r, reason);
    CHECK(reason.empty());
    CHECK(c.size() == 4);
}

TEST_CASE("parseMsedge 数组形/对象形") {
    CHECK(parseMsedge("[{\"translations\":[{\"text\":\"你好\"}]}]") == "你好");
    CHECK(parseMsedge("{\"translations\":[{\"text\":\"hi\"}]}") == "hi");
}

TEST_CASE("parseGoogle 基本") {
    CHECK(parseGoogle("[[[\"hi\",\"hello\"]]]") == "hi");
}

TEST_CASE("parseYoudaoV2 基本") {
    CHECK(parseYoudaoV2("{\"translateResult\":[[{\"tgt\":\"你好\"}]]}") == "你好");
    CHECK(parseYoudaoV2("{\"translateResult\":[[{\"tgt\":\"abc\"}]]}") == "abc");
    CHECK(parseYoudaoV2("{\"translateResult\":[[{\"tgt\":\"hi\"}],[{\"tgt\":\" there\"}]]}")
          == "hi there");
    CHECK(parseYoudaoV2("{\"translateResult\":[[{\"t\":\"old\"}]]}") == "old");
    CHECK(parseYoudaoV2("{\"translation\":[{\"t\":\"legacy\"}]}") == "legacy");
    CHECK(parseYoudaoV2("{\"code\":50}").empty());
}

TEST_CASE("youdaoResultCode") {
    CHECK(youdaoResultCode("{\"code\":0}") == 0);
    CHECK(youdaoResultCode("{\"code\":50}") == 50);
    CHECK(youdaoResultCode("not json") == 0);
}

TEST_CASE("youdaoKeyParse") {
    std::string sec, a, iv, ck;
    CHECK(youdaoKeyParse(
        "{\"secretKey\":\"s\",\"aesKey\":\"ak\",\"aesIv\":\"iv\",\"cookie\":\"ck\"}",
        sec, a, iv, ck));
    CHECK(sec == "s");
    CHECK(a == "ak");
    CHECK(iv == "iv");
    CHECK(ck == "ck");

    sec = a = iv = ck = std::string();
    CHECK_FALSE(youdaoKeyParse("{\"secretKey\":\"s\",\"aesKey\":\"\"}", sec, a, iv, ck));
}

TEST_CASE("youdaoExtractCookie") {
    std::string c;
    youdaoExtractCookie(c, "OUTFOX_SEARCH_USER_ID=100@127.0.0.1; Path=/; HttpOnly"
                            "\nOUTFOX_SEARCH_USER_ID_NEW=200@127.0.0.1; Path=/");
    CHECK(c == "OUTFOX_SEARCH_USER_ID=100@127.0.0.1; Path=/; HttpOnly");

    c = std::string();
    youdaoExtractCookie(c, "");
    CHECK(c.empty());
}

TEST_CASE("scrapeYandexSid") {
    CHECK(scrapeYandexSid("var t={srv:1,sid: '1234567890abcdef.12345',no:2}")
          == "1234567890abcdef.12345");
    CHECK(scrapeYandexSid("no sid here").empty());
}

TEST_CASE("parseYandex 基本") {
    CHECK(parseYandex("{\"code\":200,\"text\":[\"hi\"]}") == "hi");
}

TEST_CASE("base64UrlDecode") {
    CHECK(base64UrlDecode("aGVsbG8=") == "hello");
    CHECK(base64UrlDecode("aGVsbG8") == "hello");
    CHECK(base64UrlDecode(b64urlFromChars('-', '_')) == bytesFromHex("fb"));
    CHECK(base64UrlDecode(b64urlFromChars('_', '-')) == bytesFromHex("ff"));
    CHECK(base64UrlDecode("") == "");
}

TEST_CASE("md5Raw") {
    CHECK(toHex(md5Raw("abc")) == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(md5Raw("secret").size() == 16);
}

TEST_CASE("AES-128-CBC 单块 FIPS-197 已知答案") {
    uint8_t key[16], in[16], out[16];
    for (int i = 0; i < 16; i++) { key[i] = (uint8_t)i; }
    const uint8_t expect[16] = { 0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
                                 0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff };
    const uint8_t ct[16] = { 0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,
                             0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a };
    aesDecryptBlock(key, ct, out);
    for (int i = 0; i < 16; i++) { CHECK(out[i] == expect[i]); }
}

TEST_CASE("AES-128-CBC PKCS7" ) {
    std::string key;
    for (int i = 0; i < 16; i++) { key += (char)i; }
    const std::string iv(16, '\0');
    const std::string single =
        aes128CbcDecrypt(key, iv, bytesFromHex("493e03c05f98d3c43fc46bbeff24ab50"));
    CHECK(single == "hello\n");
    const std::string multi =
        aes128CbcDecrypt(key, iv,
                         bytesFromHex("e2228e7dafd51dc06aa04110722be127164ece0e7684af8215b6e02f84f3a29837bfb07ca8e9b81966582efe3b02ccb7"));
    CHECK(multi == "the quick brown fox jumps over the lazy dog\n");
    CHECK(aes128CbcDecrypt(key, iv, std::string(15, 'x')).empty());
    CHECK(aes128CbcDecrypt(std::string(8, 'k'), iv, std::string(16, 0)).empty());
}