#!/bin/bash
# ============================================================
# TreeOS · 把准备好的 rootfs 镜像写进 TF 卡的 p2 分区
#
# 在【电脑】上跑（需要 root）：
#
#   sudo bash install-to-p2.sh /dev/sda2 /media/$USER/EASYROMS/treeos-root.img
#
# 做的事：校验 → dd 写 p2 → 扩容到分区满 → 检查
# 不做的事：不碰 p1(BOOT) 和 p3(roms)
# ============================================================
set -euo pipefail

DEV=${1:-}
IMG=${2:-}
[ -b "$DEV" ] || { echo "用法: $0 <分区设备> <镜像文件>"; exit 1; }
[ -f "$IMG" ] || { echo "镜像不存在: $IMG"; exit 1; }
[ "$(id -u)" -eq 0 ] || { echo "请用 sudo"; exit 1; }

echo "目标分区: $DEV ($(lsblk -no SIZE "$DEV" | tr -d ' '))"
echo "镜像:     $IMG ($(du -h "$IMG" | cut -f1))"
echo
read -rp "确认写入？这会覆盖 $DEV 的全部内容 [输入 yes]: " ok
[ "$ok" = "yes" ] || { echo "已取消"; exit 1; }

# 只卸载目标分区；镜像可能就在同一张卡的另一个分区上，不能全卸
udisksctl unmount -b "$DEV" 2>/dev/null || true
umount "$DEV" 2>/dev/null || true
if mountpoint -q "$(dirname "$IMG")" 2>/dev/null; then
	echo "提示：镜像所在分区保持挂载（需要读取它）"
fi

echo "[1/3] 写入镜像"
dd if="$IMG" of="$DEV" bs=4M conv=fsync status=progress

echo "[2/3] 扩容文件系统到分区大小"
e2fsck -f "$DEV" || true
resize2fs "$DEV"

echo "[3/3] 完成"
lsblk -o NAME,SIZE,FSTYPE,LABEL "$DEV"
echo
echo "✅ p2 已是 TreeOS rootfs。开机前确认 BOOT 分区的 boot.ini / Image / dtb 对应要用的内核。"
