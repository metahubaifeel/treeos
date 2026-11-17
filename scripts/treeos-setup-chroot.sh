#!/bin/bash
# ============================================================
# TreeOS · Debian 用户态配置（在掌机上以 sudo 运行）
#
#   sudo bash treeos-setup-chroot.sh
#
# 前提：/roms/treeos-root 已经用 debootstrap 装好 trixie
# 作用：配 apt 源、装基础包、建用户、写 fstab、准备 chroot 环境
# 不动现有系统，不碰 p2
# ============================================================
set -euo pipefail

ROOT=${ROOT:-/mnt/treeos}
MIRROR=${MIRROR:-https://mirrors.tuna.tsinghua.edu.cn/debian}
SECURITY=${SECURITY:-https://mirrors.tuna.tsinghua.edu.cn/debian-security}

[ -d "$ROOT/etc" ] || { echo "找不到 $ROOT，先跑 debootstrap"; exit 1; }

cleanup() {
	for m in dev/pts dev proc sys; do
		mountpoint -q "$ROOT/$m" 2>/dev/null && umount "$ROOT/$m" || true
	done
}
trap cleanup EXIT

echo "[1/6] apt 源"
cat > "$ROOT/etc/apt/sources.list" <<EOF
deb $MIRROR trixie main contrib non-free non-free-firmware
deb $MIRROR trixie-updates main contrib non-free non-free-firmware
deb $SECURITY trixie-security main contrib non-free non-free-firmware
EOF
cp /etc/resolv.conf "$ROOT/etc/resolv.conf"
printf '#!/bin/sh\nexit 101\n' > "$ROOT/usr/sbin/policy-rc.d"
chmod +x "$ROOT/usr/sbin/policy-rc.d"

echo "[2/6] 挂载 chroot 环境"
mkdir -p "$ROOT/dev" "$ROOT/proc" "$ROOT/sys"
mount --bind /dev "$ROOT/dev"
mount --bind /dev/pts "$ROOT/dev/pts" 2>/dev/null || true
mount --bind /proc "$ROOT/proc"
mount --bind /sys "$ROOT/sys"

echo "[3/6] 安装基础包"
chroot "$ROOT" apt-get update
chroot "$ROOT" env DEBIAN_FRONTEND=noninteractive \
	apt-get install -y --no-install-recommends \
	systemd-sysv udev dbus sudo openssh-server \
	ca-certificates curl wget less vim-tiny htop \
	locales console-setup keyboard-configuration \
	alsa-utils \
	iproute2 wpasupplicant isc-dhcp-client \
	libdrm2 libegl1 libgles2 libgl1-mesa-dri mesa-utils \
	libinput10 libsdl2-2.0-0 libasound2t64 \
	fonts-dejavu-core \
	zram-tools \
	firmware-realtek

echo "[4/6] 用户与主机名"
chroot "$ROOT" bash -c '
	set -e
	echo treeos > /etc/hostname
	printf "127.0.0.1 localhost\n127.0.1.1 treeos\n" > /etc/hosts
	echo "Asia/Shanghai" > /etc/timezone
	ln -sf /usr/share/zoneinfo/Asia/Shanghai /etc/localtime
	# 主用户叫 tree（uid 1002 保持不变，存档权限不动）
	if ! id tree >/dev/null 2>&1; then
		groupadd -g 1002 tree
		useradd -m -u 1002 -g 1002 -s /bin/bash tree
	fi
	for g in adm audio video input; do adduser tree "$g" >/dev/null 2>&1 || true; done
	adduser tree sudo >/dev/null 2>&1 || true
	echo "tree:${TREEOS_PASSWORD:-tree}" | chpasswd
	echo "root:tree" | chpasswd
	# ArkOS 的老脚本到处写死 /home/ark，留个软链接兼容
	[ -e /home/tree ] && ln -sfn /home/tree /home/ark
	sed -i "s/^# *en_US.UTF-8/en_US.UTF-8/; s/^# *zh_CN.UTF-8/zh_CN.UTF-8/" /etc/locale.gen
	locale-gen >/dev/null 2>&1 || true
'

echo "[5/6] fstab"
cat > "$ROOT/etc/fstab" <<'EOF'
/dev/mmcblk0p2  /      ext4  defaults,noatime  0 1
/dev/mmcblk0p1  /boot  vfat  defaults,noatime  0 2
/dev/mmcblk0p3  /roms  exfat defaults,auto,umask=000,uid=1002,gid=1002,noatime,nofail  0 0
/roms/tools /opt/system/Tools none nofail,x-systemd.device-timeout=7,bind
EOF

echo "[5.5/6] 预置手柄配置（刷完开机不用配）"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$HERE/../config/es_input.cfg" ]; then
	mkdir -p "$ROOT/etc/emulationstation"
	cp -f "$HERE/../config/es_input.cfg" "$ROOT/etc/emulationstation/es_input.cfg"
	echo "  已写入 /etc/emulationstation/es_input.cfg"
fi
# ArkOS 的二进制度写死 /home/ark，必须有这个软链接
ln -sfn /home/tree "$ROOT/home/ark"

echo "[6/6] 清理"
rm -f "$ROOT/usr/sbin/policy-rc.d"
cleanup
trap - EXIT

echo
echo "✅ 完成。rootfs: $ROOT ($(du -sh "$ROOT" | cut -f1))"
echo "   下一步：搬 ArkOS 的菜单层和模拟器"
