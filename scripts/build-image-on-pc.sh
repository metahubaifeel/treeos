#!/bin/bash
# ============================================================
# 在电脑上构建 TreeOS 可刷写镜像（需要 root）
#
#   sudo bash build-image-on-pc.sh
#
# 依赖：卡已插在电脑上并挂载到 /media/$SUDO_USER/EASYROMS
# 产物：/media/.../treeos-image/treeos-0.1.img（7 GB）
# ============================================================
set -euo pipefail

USER_HOME=$(getent passwd "${SUDO_USER:-$USER}" | cut -d: -f6)
BASE=$(ls -d /media/*/EASYROMS 2>/dev/null | head -1)
[ -n "$BASE" ] || { echo "找不到挂载的 EASYROMS 分区，请先插卡并挂载"; exit 1; }
D="$BASE/treeos-image"
IMG="/tmp/treeos-0.1.img"   # 先在 ext4 上构建，最后再拷到卡

ROOTFS_TGZ="${ROOTFS_TGZ:-/tmp/rootfs-clean.tgz}"
[ -f "$ROOTFS_TGZ" ] || { echo "找不到 $ROOTFS_TGZ（先在电脑上打包 rootfs）"; exit 1; }
[ -f "$D/Image" ] || { echo "找不到 Image"; exit 1; }

echo "源目录: $D"
echo "镜像:   $IMG"

echo "[1/6] 创建 7G 镜像文件"
rm -f "$IMG"
truncate -s 7G "$IMG"

echo "[2/6] 分区"
parted -s "$IMG" mklabel msdos
parted -s "$IMG" mkpart primary fat32 1MiB 257MiB
parted -s "$IMG" mkpart primary ext4 257MiB 5377MiB
parted -s "$IMG" mkpart primary 5377MiB 100%
parted -s "$IMG" set 1 boot on

echo "[3/6] 映射 loop"
LOOP=$(losetup -f --show -P "$IMG")
echo "loop=$LOOP"
sleep 1

echo "[4/6] 格式化"
mkfs.vfat -F 32 -n BOOT "${LOOP}p1" >/dev/null
mkfs.ext4 -q -L treeos-root "${LOOP}p2"
mkfs.exfat -n EASYROMS "${LOOP}p3" >/dev/null

echo "[5/6] 写入内容"
M=$(mktemp -d)
mkdir -p "$M/p1" "$M/p2" "$M/p3"
mount "${LOOP}p1" "$M/p1"
cp "$D/Image" "$D/boot.ini" "$D/rk3326-r36s-treeos.dtb" "$D/logo.bmp" "$M/p1/"
mount "${LOOP}p2" "$M/p2"
tar -xzpf "$ROOTFS_TGZ" -C "$M/p2"
mount "${LOOP}p3" "$M/p3"
mkdir -p "$M/p3/backup"
sync

echo "[6/6] 收尾"
umount "$M/p1" "$M/p2" "$M/p3"
rmdir "$M/p1" "$M/p2" "$M/p3" "$M" 2>/dev/null || true
losetup -d "$LOOP"
sync

echo
echo "✅ 镜像完成：$IMG"
ls -lh "$IMG"
echo
echo "复制到卡上（可能要几分钟）..."
cp -f "$IMG" "$D/treeos-0.1.img"
sync
ls -lh "$D/treeos-0.1.img"
echo "下一步：压缩（可选）"
echo "  xz -T0 -3 -k $IMG"
