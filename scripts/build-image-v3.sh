#!/bin/bash
# ============================================================
# TreeOS 镜像构建 v3（2026-11-15）
#   - 内含：内核 #14（uinput）/ Debian RetroArch 1.20 / padmerge
#           虚拟键盘 / CJK 字体 / 存档与核心修复
#   - p1 从 16 MiB 开始，前 16MB 写入母卡 u-boot
#   - p3 类型 0x07（exFAT）
#
#   sudo bash build-image-v3.sh
# ============================================================
set -euo pipefail

D=${TREEOS_ROOT:-$HOME/r36s}/.build/out
IMG=/tmp/treeos-v3.img
BOOTLOADER="$D/bootloader-16m.bin"
ROOTFS=/tmp/rootfs-v3.tgz
BOOTSRC="$D/bootfiles"

[ -f "$BOOTLOADER" ] || { echo "缺引导程序: $BOOTLOADER"; exit 1; }
[ -f "$ROOTFS" ] || { echo "缺 rootfs 包: $ROOTFS（先在电脑上打包）"; exit 1; }

echo "[1/7] 建 16 GiB 空镜像（rootfs 实际 5.0G，必须留足）"
rm -f "$IMG"
truncate -s 16384M "$IMG"

echo "[2/7] 写入引导程序（前 16 MiB）"
dd if="$BOOTLOADER" of="$IMG" bs=1M count=16 conv=notrunc status=none

echo "[3/7] 分区（p1 从 16 MiB 开始）"
parted -s "$IMG" mklabel msdos
parted -s "$IMG" mkpart primary fat32 16MiB 272MiB
parted -s "$IMG" mkpart primary ext4 272MiB 11520MiB
parted -s "$IMG" mkpart primary 11520MiB 100%
parted -s "$IMG" set 1 boot on
python3 - "$IMG" <<'PY'
import sys
p = sys.argv[1]
with open(p, 'r+b') as f:
    f.seek(446 + 2 * 16 + 4)
    f.write(b'\x07')
PY

echo "[4/7] 映射 loop"
LOOP=$(losetup -f --show -P "$IMG")
echo "  loop=$LOOP"
sleep 1

echo "[5/7] 格式化"
mkfs.vfat -F 32 -n BOOT "${LOOP}p1" >/dev/null
mkfs.ext4 -q -L treeos-root "${LOOP}p2"
mkfs.exfat -n EASYROMS "${LOOP}p3" >/dev/null

echo "[6/7] 写入内容"
M=$(mktemp -d); mkdir -p "$M/p1" "$M/p2" "$M/p3"
mount "${LOOP}p1" "$M/p1"
for f in Image boot.ini rk3326-r36s-treeos.dtb logo.bmp; do
    [ -f "$BOOTSRC/$f" ] && cp "$BOOTSRC/$f" "$M/p1/"
done
mount "${LOOP}p2" "$M/p2"
tar -xzpf "$ROOTFS" -C "$M/p2"
mount "${LOOP}p3" "$M/p3"
mkdir -p "$M/p3/backup"
sync

echo "[7/7] 收尾"
umount "$M/p1" "$M/p2" "$M/p3"
rmdir "$M/p1" "$M/p2" "$M/p3" "$M" 2>/dev/null || true
losetup -d "$LOOP"
sync

echo
echo "✅ 镜像完成：$IMG"
ls -lh "$IMG"
sha256sum "$IMG"
fdisk -l "$IMG" | tail -4
