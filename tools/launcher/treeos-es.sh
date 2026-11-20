#!/bin/bash
# TreeOS ES launch wrapper
# 2026-11-20 P0: 先发停止标记，再等 splash 真正退出（最多 1s），不再固定 sleep 1；
# 每个阶段写 /dev/shm/treeos-boot-timing.log，时间基准是 /proc/uptime（单调秒）。
export LD_LIBRARY_PATH=/opt/treeos-compat/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export SDL_VIDEODRIVER=kmsdrm
export SDL_AUDIODRIVER=alsa
export SDL_ASSERT=always_ignore
export VLC_PLUGIN_PATH=/opt/treeos-compat/lib/vlc/plugins
export XDG_RUNTIME_DIR=/run/user/1002

now() { cut -d' ' -f1 /proc/uptime; }
log() { echo "$(now) $*" >> /dev/shm/treeos-boot-timing.log; }

log "es-script-start pid=$$"
touch /dev/shm/treeos-splash-stop
log "stop-touched"

pkill -x gptokeyb2 2>/dev/null || true
pkill -x gptokeyb 2>/dev/null || true
pkill -f padinput.py 2>/dev/null || true
pkill -f stickmouse.py 2>/dev/null || true
pkill -9 -x weston 2>/dev/null || true

# splash 每 200ms 检查一次停止标记；等它真正释放 DRM，最多 1s
for i in $(seq 1 20); do
  pgrep -x treeos-splash >/dev/null 2>&1 || break
  sleep 0.05
done
log "splash-exited"

log "exec-es-direct"
exec /usr/bin/emulationstation/emulationstation "$@"
