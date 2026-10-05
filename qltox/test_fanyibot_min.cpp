#include "doctest.h"
#include "fanyibot.h"
#include "fanyibot.cpp"

TEST_CASE("fanyibotEngines") {
    const auto& v = fanyibotEngines();
    CHECK(v.size() == 5);
}

TEST_CASE("fanyibotLangCode 基本") {
    CHECK(fanyibotLangCode(kFanyibotMsedge, "中文") == "zh-Hans");
    CHECK(fanyibotLangCode(kFanyibotGoogle, "中文") == "zh-CN");
    CHECK(fanyibotLangCode(kFanyibotYoudao, "中文") == "zh-CHS");
    CHECK(fanyibotLangCode(kFanyibotYandex, "中文") == "zh");
}

TEST_CASE("buildCandidates") {
    FanyibotRequest r;
    r.engine = "all";
    r.toLang = "English";
    std::string reason;
    auto c = buildCandidates(r, reason);
    CHECK(reason.empty());
    CHECK(c.size() == 4);
}

TEST_CASE("parseMsedge") {
    CHECK(parseMsedge("{\"translations\":[{\"text\":\"你好\"}]}") == "你好");
}

TEST_CASE("parseGoogle") {
    CHECK(parseGoogle("[[[\"hi\",\"hello\"]]]") == "hi");
}

TEST_CASE("parseYandex") {
    CHECK(parseYandex("{\"code\":200,\"text\":[\"hi\"]}") == "hi");
}
