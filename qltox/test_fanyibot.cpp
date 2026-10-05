#include "doctest.h"
#include "fanyibot.h"
#include "fanyibot.cpp"

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

TEST_CASE("parseMsedge 基本") {
    CHECK(parseMsedge("{\"translations\":[{\"text\":\"你好\"}]}") == "你好");
}

TEST_CASE("parseGoogle 基本") {
    CHECK(parseGoogle("[[[\"hi\",\"hello\"]]]") == "hi");
}

TEST_CASE("parseYoudao 基本") {
    CHECK(parseYoudao("{\"translateResult\":[[{\"t\":\"你好\"}]]}") == "你好");
}

TEST_CASE("parseYandex 基本") {
    CHECK(parseYandex("{\"code\":200,\"text\":[\"hi\"]}") == "hi");
}
