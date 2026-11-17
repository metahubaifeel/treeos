#!/bin/bash
# 批量冒烟测试：逐个启动 Flash 游戏，记录退出码 / OOM / 存活时间。
# 用法（真机上，root）：
#   ES_SERVICE=treeos-emulationstation batch_test_flash.sh /tmp/list.txt /tmp/flash-test.csv
# list.txt 每行一个 swf 绝对路径。
# 约定：串行跑；结束确保游戏进程杀干净再恢复 ES；不改任何系统配置。
set -u

LIST="${1:-/tmp/list.txt}"
OUT="${2:-/tmp/flash-test.csv}"
ES_LAUNCH="${ES_LAUNCH:-/roms/ports/rufflehandheld/runtime/es-launch.sh}"
ROMROOT="${ROMROOT:-/roms}"
WAIT="${WAIT:-20}"
ES_SERVICE="${ES_SERVICE:-emulationstation}"

[ -f "$LIST" ] || { echo "no list: $LIST" >&2; exit 2; }
printf 'file,rc,oom,alive_sec,log_has_movie,log_panic\n' > "$OUT"

# 用字符类写法避免 pkill 匹配到自己（命令行里含 ruffle[-]native，但正则匹配的是 ruffle-native）
kill_games() {
    pkill -f 'ruffle[-]native' 2>/dev/null
    pkill -f 'rufflesa' 2>/dev/null
    pkill -f 'gptokeyb[2]' 2>/dev/null
    for _ in $(seq 1 20); do
        pgrep -f 'ruffle[-]native|gptokeyb[2]|rufflesa' >/dev/null 2>&1 || return 0
        sleep 0.5
    done
    pkill -9 -f 'ruffle[-]native' 2>/dev/null
    sleep 1
}

systemctl stop "$ES_SERVICE" 2>/dev/null
sleep 2

while IFS= read -r swf; do
    [ -n "$swf" ] || continue
    [ -f "$swf" ] || { echo "$swf,missing,,," >> "$OUT"; continue; }

    kill_games
    sleep 1

    c0=$(dmesg | grep -c 'Killed process')
    start=$(date +%s)
    timeout "$WAIT" /bin/bash "$ES_LAUNCH" --rom-root "$ROMROOT" "$swf" \
        >/tmp/bt.$$.log 2>&1 &
    pid=$!
    alive=0
    while kill -0 "$pid" 2>/dev/null; do
        sleep 1
        alive=$((alive + 1))
    done
    wait "$pid" 2>/dev/null
    rc=$?
    c1=$(dmesg | grep -c 'Killed process')
    movie=$(grep -c -E 'MOVIE STARTED|avm_trace' /tmp/bt.$$.log)
    panic=$(grep -c -i -E 'panicked|exit code: 101|ALSA.*Couldn' /tmp/bt.$$.log)
    echo "$swf,$rc,$((c1 - c0)),$alive,$movie,$panic" >> "$OUT"
    rm -f /tmp/bt.$$.log

    kill_games
    sleep 1
done < "$LIST"

# 收尾：确认干净后再恢复 ES，避免残留进程占 DRM 导致 ES failed
kill_games
if pgrep -f 'ruffle[-]native|gptokeyb[2]|rufflesa' >/dev/null 2>&1; then
    echo "WARN: 仍有游戏进程残留，暂不启动 ES" >&2
else
    systemctl reset-failed "$ES_SERVICE" 2>/dev/null
    systemctl start "$ES_SERVICE" 2>/dev/null
fi
echo "DONE -> $OUT"
