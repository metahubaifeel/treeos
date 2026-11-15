#!/bin/bash
# 在电脑上运行：把 TreeOS 镜像写到读卡器里的卡
#   sudo bash flash-to-sd.sh <镜像> <块设备>
# 例：
#   sudo bash flash-to-sd.sh ~/下载/r36s/.build/out/images/treeos-0.1.img /dev/sda
set -euo pipefail

IMG=${1:?用法: $0 <镜像> <块设备>}
DEV=${2:?用法: $0 <镜像> <块设备>}
[ -f "$IMG" ] || { echo "镜像不存在: $IMG"; exit 1; }
[ -b "$DEV" ] || { echo "不是块设备: $DEV"; exit 1; }

echo "镜像: $IMG ($(du -h "$IMG" | cut -f1))"
echo "目标: $DEV ($(lsblk -no SIZE "$DEV" | tr -d ' '))"
lsblk -o NAME,SIZE,FSTYPE,LABEL "$DEV"

read -rp "确认擦除 $DEV 的全部内容？输入 yes: " ok
[ "$ok" = "yes" ] || { echo "已取消"; exit 1; }

# 卸载目标卡的所有分区
for p in ${DEV}*; do umount "$p" 2>/dev/null || true; done

echo "[1/2] 写入"
dd if="$IMG" of="$DEV" bs=4M conv=fsync status=progress
sync
echo "[2/2] 完成"
lsblk -o NAME,SIZE,FSTYPE,LABEL "$DEV"
echo "✅ 可以拔卡，插到掌机开机"
