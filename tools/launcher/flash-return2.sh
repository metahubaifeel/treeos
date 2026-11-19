#!/bin/bash
# 设置界面保存后：按新 profile 重新编译会话，再回到增强运行时。
# 关键点：mapper 保存的是 profile，不改编译结果；不重新编译就还是旧映射。
set -u
SWF="${1:?usage: flash-return2.sh <swf>}"
SESS="${FLASH_SESSION_ENV%/*}"
if [ -n "${FLASH_SESSION_ENV:-}" ] && [ -d "$SESS" ]; then
  /usr/local/bin/flash-template "$SWF" "$SESS" >/dev/null 2>&1 || true
fi

BACKEND=""
if [ -n "${FLASH_SESSION_ENV:-}" ] && [ -f "$FLASH_SESSION_ENV" ]; then
  # shellcheck disable=SC1090
  . "$FLASH_SESSION_ENV"
fi

if [ "${BACKEND:-auto}" = "classic" ]; then  /usr/local/bin/stop-flash
  systemctl stop treeos-emulationstation 2>/dev/null
  sleep 1
  /bin/bash /roms/ports/rufflehandheld/runtime/es-launch.sh --rom-root /roms "$SWF"
  systemctl start treeos-emulationstation 2>/dev/null
  exit 0
fi

# 给 kmsdrm/DRM master 一点释放时间；否则 Weston 可能卡在 GL 初始化后拿不到输出
sleep 2
exec /usr/local/bin/treeos-flash2 "$SWF"
