#ifndef PURECONSTS_HPP
#define PURECONSTS_HPP

// ── 联系人类型常量 ──
// C++11 constexpr（内部链接，每 TU 一份；消费方为内容比较/赋值，无指针同一性依赖）
constexpr const char* const kImapMailType       = "imap_mail";
constexpr const char* const kGomuksRoomType     = "gomuks_room";
constexpr const char* const kUnktoxFriendType   = "unktox_friend";
constexpr const char* const kUnktoxConferenceType = "unktox_conference";
constexpr const char* const kUnktoxGroupType    = "unktox_group";
constexpr const char* const kSyseventType       = "sysevent";
constexpr const char* const kTopicType          = "topic";
constexpr const char* const kFilesyncType       = "filesync";
constexpr const char* const kClipboardType      = "clipboard";
constexpr const char* const kUnknownType        = "unknown";
constexpr const char* const kBookmarkType       = "bookmark";
constexpr const char* const kAichatType         = "aichat";
constexpr const char* const kPastebinType       = "pastebin";
constexpr const char* const kTranslateType      = "translate";
constexpr const char* const kMisskeyType        = "misskey_note";
constexpr const char* const kToutiaoHotnewsType = "toutiao_hotnews";   // 头条热闻订阅流（type == chatId）
constexpr int kToutiaoHotnewsId = -107;                                // 头条热闻保留 id（-107，固定不 hash）
constexpr const char* const kZhihuNotifyType = "zhihu_notify";         // 知乎通知订阅流（type == chatId）
constexpr int kZhihuNotifyId = -108;                                   // 知乎通知保留 id（-108，固定不 hash）
constexpr const char* const kZhihuHotnewsType = "zhihu_hotnews";       // 知乎热闻订阅流（type == chatId）
constexpr int kZhihuHotnewsId = -109;                                  // 知乎热闻保留 id（-109，固定不 hash）
constexpr const char* const kBiliNotifyType = "bili_notify";           // 哔喱关注动态订阅流（type == chatId）
constexpr int kBiliNotifyId = -110;                                    // 哔喱通知保留 id（-110，固定不 hash）
constexpr const char* const kWeiboHotnewsType = "weibo_hotnews";       // 微博热闻订阅流（type == chatId == roomId）
constexpr int kWeiboHotnewsId = -111;                                  // 微博热闻保留 id（-111，固定不 hash）
constexpr const char* const kXiaohongshuNotifyType = "xiaohongshu_notify"; // 小红书通知订阅流（type == chatId == roomId）
constexpr int kXiaohongshuNotifyId = -112;                             // 小红书通知保留 id（-112，固定不 hash）
constexpr const char* const kCoolapkTimelineType = "coolapk_timeline";  // 酷安时线订阅流（type == chatId == roomId）
constexpr int kCoolapkTimelineId = -113;                                // 酷安时线保留 id（-113，固定不 hash）
constexpr const char* const kXiaohongshuRecommendType = "xiaohongshu_recommend"; // 小红书推荐流订阅流（type == chatId == roomId）
constexpr int kXiaohongshuRecommendId = -114;                                    // 小红书推荐流保留 id（-114，固定不 hash）
constexpr const char* const kXiaohongshuHotnewsType = "xiaohongshu_hotnews"; // 小红书热闻订阅流（type == chatId == roomId）
constexpr int kXiaohongshuHotnewsId = -115;                                // 小红书热闻保留 id（-115，固定不 hash）
constexpr int UnkSize = 0;                                             // 未知大小/未知尺寸哨兵（0 = 未知）

#endif // PURECONSTS_HPP