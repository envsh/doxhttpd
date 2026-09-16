#!/usr/bin/env bash
set -u
OUT=/home/gzleo/aprog/toxhttpd/web/icons/SOURCES.md
META=/tmp/opencode/icons_meta.csv
{
cat <<'HEAD'
# 图标来源 (SOURCES)

本目录收录应用 / 协议 / 系统 / 浏览器 / 笔记 / 视频站 / 播放器 / 搜索引擎 / AI / 编程语言 / CPU / 汽车品牌 / 国家国旗 / 社区论坛的品牌图标，共 **430 项**。
每项含两种格式：`*.svg`（矢量，可无限缩放）与 `*.png`（64×64 透明底栅格图；`flags/` 国旗为 4:3 比例，PNG 为 64×48）。

## 分类与数量
| 分类 | 说明 | 数量 |
|---|---|---|
| messaging | 即时通讯 / 协议 | 20 |
| os | 操作系统（含版本变体） | 15 |
| browsers | 浏览器 | 11 |
| notes | 笔记应用 | 9 |
| video | 视频 / 直播站 | 13 |
| players | 视频播放器 | 9 |
| engines | 搜索引擎 | 15 |
| langs | 编程语言 | 26 |
| cpu | CPU / 芯片 | 9 |

| ai | AI / 大模型 | 13 |
| cars | 汽车品牌 | 26 |
| flags | 国家国旗 | 258 |
| community | 社区 / 论坛 | 6 |

## 命名与备注
- 一律小写英文单名（`matrix.svg`、`youtube.png`）。
- `oicq` = QQ 旧名，文件复制自 `messaging/qq.*`。
- `weixin` 取 WeChat 图形标；`twitter` 为经典蓝鸟（非 X 标）。
- `hackernews` 无官方独立矢量，取 simple-icons 的 `y-combinator`（HN 的橙色 Y 图形即出自 YC）。
- `windows` 为旧版旗帜、`windows-1` 为新版四格旗帜；`macos`（新）/ `mac-os-x`（旧）。
- 曾尝试收录但**无正规来源**而跳过的项：douyu、tieba、mercedes-benz、landrover（后两者为汽车品牌，SI/WVL/GB/logo.wine 等可达源均无矢量图）；iqiyi 曾无源，后通过 logo.wine 补齐。
- `gopher` 为非即时通讯类互联网协议，仍归入 messaging（即时通讯 / 协议）分类。
- `flags/` 以 ISO 3166-1 alpha-2 小写代码命名（含未分配 / 地区代码如 `hk` `mo` `tw`，以及 `gb-eng` `gb-sct` `gb-wls` `eu` `un` `xk` 等），PNG 保持 4:3（64×48）。来源 lipis/flag-icons（MIT），矢量来自其 `flags/4x3/`。

## 授权与版权说明
- **品牌商标归各自公司所有**，本目录收录仅用于界面识别 / 展示用途，不构成任何商标授权或关联关系。
- 来源许可（详见各表"来源许可"列）：simple-icons=CC0；SVG Logos (gilbarbara)=MIT；worldvectorlogo=免费公开下载；CoreUI 品牌图标 (iconify/cib)=MIT；logo.wine=按网站条款；Tox 官方仓库=CC-BY-SA-4.0。
- 单色图标来自 simple-icons，未着色（黑色字形，CSS 可用 `filter`/`fill` 改色）。

HEAD
# per-category tables
cat_cn(){
  case "$1" in
    messaging) echo "即时通讯 / 协议";; os) echo "操作系统";; browsers) echo "浏览器";;
    notes) echo "笔记应用";; video) echo "视频 / 直播站";; players) echo "视频播放器";; engines) echo "搜索引擎";; ai) echo "AI / 大模型";; langs) echo "编程语言";; cpu) echo "CPU / 芯片";; cars) echo "汽车品牌";; flags) echo "国家国旗";; community) echo "社区 / 论坛";;
  esac
}
lic_of(){
  case "$1" in
    *cdn.simpleicons.org*)   echo "CC0";;
    *worldvectorlogo*)       echo "免费公开下载";;
    *Tox/tox.chat*)          echo "CC-BY-SA-4.0";;
    *gilbarbara*)            echo "MIT (SVG Logos)";;
    *logo.wine*)             echo "logo.wine 条款";;
    *api.iconify.design*)    echo "MIT (CoreUI cib)";;
    *\(copy*)                echo "复制项（来源同原图标）";;
    *)                       echo "-";;
  esac
}
type_of(){
  case "$1" in
    *cdn.simpleicons.org*)   echo "单色";;
    *)                       echo "彩色";;
  esac
}
for cat in messaging os browsers notes video players engines ai langs cpu cars flags community; do
  cn=$(cat_cn "$cat")
  echo ""
  echo "## $cat/（$cn）"
  echo ""
  [ "$cat" = messaging ] && echo "| 文件 | 来源 URL | 类型 | 来源许可 |" || echo "| 文件 | 来源 URL | 类型 | 来源许可 |"
  echo "|---|---|---|---|"
  grep "^$cat|" "$META" | sort | while IFS='|' read -r c name url; do
    lic=$(lic_of "$url")
    typ=$(type_of "$url")
    echo "| $cat/$name.svg | \`$url\` | $typ | $lic |"
  done
done
echo ""
echo "## 下载与生成时间"
echo ""
echo "生成时间：$(date '+%Y-%m-%d %H:%M:%S')；PNG 由 \`rsvg-convert\` 栅格化（方图 -w 64 -h 64；国旗 -w 64 -h 48）。"
} > "$OUT"
