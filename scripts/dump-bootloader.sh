#!/bin/bash
# 从母卡导出前 16MB（Rockchip 引导程序区），供重做镜像使用
set -euo pipefail
OUT=${TREEOS_ROOT:-$HOME/r36s}/.build/out
DEV=${1:-/dev/sda}
[ -b "$DEV" ] || { echo "不是块设备: $DEV"; exit 1; }
mkdir -p "$OUT"
echo "从 $DEV 导出前 16MB..."
dd if="$DEV" of="$OUT/bootloader-16m.bin" bs=1M count=16 conv=fsync status=progress
fdisk -l "$DEV" > "$OUT/mother-card-fdisk.txt" 2>&1 || true
chown "$(id -u "${SUDO_USER:-$USER}")":"$(id -g "${SUDO_USER:-$USER}")" "$OUT/bootloader-16m.bin" "$OUT/mother-card-fdisk.txt"
ls -lh "$OUT/bootloader-16m.bin"
echo "✅ 完成"
