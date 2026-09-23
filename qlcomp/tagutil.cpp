#include "tagutil.h"

#include <sstream>

std::vector<std::string> TagUtil::splitTags(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string item;
    while (std::getline(ss, item, sep)) {
        if (item.find_first_not_of(" \t\n\r") == std::string::npos) { continue; }
        out.push_back(item);
    }
    return out;
}

std::string TagUtil::normalizeTag(const std::string& t) {
    size_t start = t.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) { return ""; }
    size_t end = t.find_last_not_of(" \t\n\r");
    std::string out = t.substr(start, end - start + 1);

    // 剥离前导 '#'（可多个，避免 join 后再现 "##tag"）
    size_t p = 0;
    while (p < out.size() && out[p] == '#') { p++; }
    if (p >= out.size()) { return ""; }
    out = out.substr(p);

    // 连续空白折叠为单空格（保留中文等非空白字节）
    std::string collapsed;
    bool inWs = false;
    for (size_t i = 0; i < out.size(); i++) {
        const char c = out[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!inWs) { collapsed += ' '; }
            inWs = true;
        } else {
            collapsed += c;
            inWs = false;
        }
    }

    // 折叠后的首尾可能夹一个空格（如 "# 爱情" → " 爱情"），再裁一次
    size_t s2 = collapsed.find_first_not_of(" \t\n\r");
    size_t e2 = collapsed.find_last_not_of(" \t\n\r");
    if (s2 == std::string::npos) { return ""; }
    return collapsed.substr(s2, e2 - s2 + 1);
}

void TagUtil::dedupTags(std::vector<std::string>& tags) {
    std::vector<std::string> seen;
    for (size_t i = 0; i < tags.size(); i++) {
        bool dup = false;
        for (size_t j = 0; j < seen.size(); j++) {
            if (seen[j] == tags[i]) { dup = true; break; }
        }
        if (!dup) { seen.push_back(tags[i]); }
    }
    tags.swap(seen);
}

std::vector<std::string> TagUtil::mergeTags(const std::vector<std::string>& a,
                                            const std::vector<std::string>& b) {
    std::vector<std::string> out;
    for (size_t i = 0; i < a.size(); i++) {
        out.push_back(normalizeTag(a[i]));
    }
    for (size_t i = 0; i < b.size(); i++) {
        out.push_back(normalizeTag(b[i]));
    }
    std::vector<std::string> clean;
    for (size_t i = 0; i < out.size(); i++) {
        if (!out[i].empty()) { clean.push_back(out[i]); }
    }
    dedupTags(clean);
    return clean;
}

std::string TagUtil::joinTagsBare(const std::vector<std::string>& tags,
                                  const std::string& sep) {
    if (tags.empty()) { return ""; }
    std::ostringstream oss;
    for (size_t i = 0; i < tags.size(); i++) {
        if (i > 0) { oss << sep; }
        oss << tags[i];
    }
    return oss.str();
}

std::string TagUtil::joinTagsSharp(const std::vector<std::string>& tags,
                                   const std::string& sep) {
    if (tags.empty()) { return ""; }
    std::ostringstream oss;
    for (size_t i = 0; i < tags.size(); i++) {
        if (i > 0) { oss << sep; }
        oss << '#' << tags[i];
    }
    return oss.str();
}

std::string TagUtil::format(const std::vector<std::string>& rawTags) {
    std::vector<std::string> clean;
    for (size_t i = 0; i < rawTags.size(); i++) {
        const std::string n = normalizeTag(rawTags[i]);
        if (!n.empty()) { clean.push_back(n); }
    }
    dedupTags(clean);
    return joinTagsSharp(clean);
}

TagUtil::TagUtil() {}