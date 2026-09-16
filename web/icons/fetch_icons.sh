#!/usr/bin/env bash
set -u
ICONS=/home/gzleo/aprog/toxhttpd/web/icons
META=/tmp/opencode/icons_meta.csv
MANIFEST=/tmp/opencode/icons_manifest.txt
: > "$META"
fail=0
ok=0

dl() {
  local url="$1" out="$2" turl="$3"
  for attempt in 1 2; do
    curl -sfL --retry 2 -m 30 "$url" -o "$out" && [ -s "$out" ] && return 0
    sleep 1
  done
  if [ -n "$turl" ]; then
    curl -sfL --retry 2 -m 30 "$turl" -o "$out" && [ -s "$out" ] && return 0
  fi
  return 1
}

while IFS='|' read -r cat name url; do
  [ -z "$cat" ] && continue
  dir="$ICONS/$cat"
  mkdir -p "$dir"
  svg="$dir/$name.svg"
  url="$url"
  # fallback for iconify misses -> simple-icons
  si=$(printf '%s' "$url" | grep -q 'api.iconify.design' && echo "https://cdn.simpleicons.org/$([ "$name" = weixin ] && echo wechat || echo "$name")" || echo "")
  if dl "$url" "$svg" "$si"; then
    ok=$((ok+1))
    echo "$cat|$name|$url" >> "$META"
  else
    fail=$((fail+1))
    echo "FAIL: $cat/$name <- $url"
  fi
done < "$MANIFEST"

# oicq = copy of qq
cp "$ICONS/messaging/qq.svg" "$ICONS/messaging/oicq.svg" && { ok=$((ok+1)); echo "messaging|oicq|(copy of messaging/qq.svg)" >> "$META"; } || echo "FAIL: oicq copy"
echo "---"
echo "OK=$ok FAIL=$fail"