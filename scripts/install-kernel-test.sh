#!/bin/bash
# ============================================================
# TreeOS · 内核试装（在掌机上跑）
#
#   sudo bash install-kernel-test.sh <Image> <dtb>
#
# 做的事：
#   1. 把 /boot 里的 Image / boot.ini / 当前 dtb 备份到 /roms/backup/
#   2. 装新 Image + 新 dtb
#   3. 写一份**不带 uInitrd** 的 boot.ini（旧 initrd 是 4.4 的，不能配 6.12）
#   4. 不重启 —— 重启由你决定
#
# 回滚：sudo bash rollback-kernel.sh
# ============================================================
set -euo pipefail

IMG="${1:-}"; DTB="${2:-}"
[ -f "$IMG" ] || { echo "用法: $0 <Image> <dtb>"; exit 1; }
[ -f "$DTB" ] || { echo "用法: $0 <Image> <dtb>"; exit 1; }
[ "$(id -u)" -ne 0 ] && { echo "请用 sudo 运行"; exit 1; }

TS=$(date +%Y%m%d-%H%M%S)
BAK="/roms/backup/kernel-$TS"
mkdir -p "$BAK"

echo "[1/4] 备份到 $BAK"
cp -v /boot/Image "$BAK/Image" 2>/dev/null || echo "  (没有 /boot/Image)"
cp -v /boot/boot.ini "$BAK/boot.ini"
for f in /boot/*.dtb; do cp -v "$f" "$BAK/"; done
echo "$TS" > /roms/backup/.last-kernel-test

echo "[2/4] 安装新 dtb"
DTBNAME="rk3326-r36s-treeos.dtb"
cp -v "$DTB" "/boot/$DTBNAME"
sync

echo "[3/4] 安装新 Image"
cp -v "$IMG" /boot/Image
sync

echo "[4/4] 写 boot.ini（不带 uInitrd）"
cp -v /boot/boot.ini /boot/boot.ini.vendor
cat > /boot/boot.ini <<EOF
odroidgoa-uboot-config

# 注意：mainline 没有 fiq-debugger，不要再写 console=/dev/ttyFIQ0
setenv bootargs "root=/dev/mmcblk0p2 rootwait rw fsck.repair=yes net.ifnames=0 console=tty0 loglevel=7"

setenv loadaddr "0x02000000"
setenv dtb_loadaddr "0x01f00000"

load mmc 1:1 \${loadaddr} Image
load mmc 1:1 \${dtb_loadaddr} $DTBNAME

booti \${loadaddr} - \${dtb_loadaddr}
EOF
sync

echo
echo "✅ 装好了。备份：$BAK"
echo "   重启试：sudo reboot"
echo "   起不来：把卡插电脑，用 $BAK 里的文件盖回去，或跑 rollback-kernel.sh"
