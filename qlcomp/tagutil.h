#ifndef TAGUTIL_H
#define TAGUTIL_H

#include <string>
#include <vector>

// tags 通用处理（对标 TagsProcessor/TagMerger 操作集），纯 std 无 Qt 依赖，可跨项目复用
class TagUtil {
public:
    // split/parse: 按分隔符拆分（istringstream+getline），跳过空白 token
    static std::vector<std::string> splitTags(const std::string& s, char sep = ',');
    // normalize: trim + 剥离一个前导 '#'（防 "##tag"）+ 连续空白折叠为单空格
    static std::string normalizeTag(const std::string& t);
    // dedup: 去重保序（保留首次出现）
    static void dedupTags(std::vector<std::string>& tags);
    // merge: 并集（a 全部 + b 新增），先归一化再保序去重，不修改入参
    static std::vector<std::string> mergeTags(const std::vector<std::string>& a,
                                              const std::vector<std::string>& b);
    // join-bare: 直接拼接，无前缀；sep 只在元素间（无尾随分隔符）；空返回 ""
    static std::string joinTagsBare(const std::vector<std::string>& tags,
                                    const std::string& sep = ", ");
    // join-sharp: 每项加 "#" 前缀后拼接（用于 Tags: #a, #b 展示）
    static std::string joinTagsSharp(const std::vector<std::string>& tags,
                                     const std::string& sep = ", ");
    // format: 便捷入口 = normalize → dedup → joinTagsSharp 全流程
    static std::string format(const std::vector<std::string>& rawTags);
private:
    TagUtil();   // 纯静态，禁实例化
};

#endif // TAGUTIL_H