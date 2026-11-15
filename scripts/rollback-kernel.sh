#!/bin/bash
# ============================================================
# TreeOS · 回滚到试装前的内核（在掌机上跑）
#
#   sudo bash rollback-kernel.sh
#
# 从 /roms/backup/kernel-<时间戳>/ 还原 Image + boot.ini + dtb。
# 掌机起不来时用不了这个脚本 —— 那就把 TF 卡插电脑，
# 把 backup 目录里的文件拷回 BOOT 分区（FAT32，电脑直接可写）。
# ============================================================
set -euo pipefail

[ "$(id -u)" -ne 0 ] && { echo "请用 sudo 运行"; exit 1; }

LAST=$(cat /roms/backup/.last-kernel-test 2>/dev/null || true)
BAK="/roms/backup/kernel-$LAST"

if [ -z "$LAST" ] || [ ! -d "$BAK" ]; then
	echo "找不到备份。/roms/backup/ 下有："
	ls -1 /roms/backup/ 2>/dev/null
	exit 1
fi

echo "从 $BAK 还原"
cp -v "$BAK/boot.ini" /boot/boot.ini
[ -f "$BAK/Image" ] && cp -v "$BAK/Image" /boot/Image
for f in "$BAK"/*.dtb; do [ -e "$f" ] && cp -v "$f" /boot/; done
rm -f /boot/rk3326-r36s-treeos.dtb
sync

echo "✅ 还原完成，重启生效：sudo reboot"
