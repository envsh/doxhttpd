#include "doctest.h"
#include "../qlcomp/compat34.cpp"

TEST_CASE("qFormatTime - 日期时间字符串") {
    CHECK(qFormatTime("2025-01-15 14:30:00") == "2025-01-15 14:30:00");
}

TEST_CASE("qFormatTime - ISO8601") {
    CHECK(qFormatTime("2025-01-15T14:30:00Z") == "2025-01-15 14:30:00");
    CHECK(qFormatTime("2025-01-15T14:30:00+08:00") == "2025-01-15 14:30:00");
}

TEST_CASE("qFormatISO8601 - 基础") {
    SUBCASE("空字符串") {
        CHECK(qFormatISO8601("") == "");
    }

    SUBCASE("UTC时间") {
        CHECK(qFormatISO8601("2025-01-15T14:30:00Z") == "2025-01-15 14:30:00");
    }

    SUBCASE("带时区") {
        CHECK(qFormatISO8601("2025-01-15T14:30:00+08:00") == "2025-01-15 14:30:00");
    }

    SUBCASE("带毫秒") {
        CHECK(qFormatISO8601("2025-01-15T14:30:00.123Z") == "2025-01-15 14:30:00");
    }
}

TEST_CASE("qFmtTime - Unix时间戳") {
    CHECK(qFmtTime(1705320600) == "2024-01-15 20:10:00");
}
