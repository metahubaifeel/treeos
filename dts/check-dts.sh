#!/bin/bash
# ============================================================
# TreeOS · 不需要 kernel 树就能语法检查 rk3326-r36s.dts
#
#   bash check-dts.sh
#
# 做的事：拉 dt-bindings 头文件 → cpp 预处理 → dtc 编译 → 打印结果
# 不验证语义（能不能开机），只验证语法和引用能不能解析。
# ============================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="${TMPDIR:-/tmp}/treeos-dtscheck"
TAG="${KERNEL_TAG:-v6.12}"
MIRROR="${MIRROR:-https://cdn.jsdelivr.net/gh/torvalds/linux@$TAG}"
FALLBACK="https://raw.githubusercontent.com/torvalds/linux/$TAG"

mkdir -p "$WORK/inc" "$WORK/src"

# fetch <url-path> <dest>：主镜像失败就换 GitHub 原站
fetch() {
	local path="$1" dest="$2"
	mkdir -p "$(dirname "$dest")"
	[ -s "$dest" ] && return 0
	if curl -sS --retry 3 --retry-delay 1 --max-time 30 \
		-o "$dest" "$MIRROR/$path"; then
		[ -s "$dest" ] && ! head -c 40 "$dest" | grep -q "Couldn't find" && return 0
	fi
	curl -sS --retry 3 --retry-delay 1 --max-time 30 \
		-o "$dest" "$FALLBACK/$path" || true
	[ -s "$dest" ] || { echo "拉取失败：$path"; return 1; }
}

# ---------- dtc ----------
if command -v dtc >/dev/null 2>&1; then
	DTC=dtc
elif [ -x "$WORK/dtc/root/usr/bin/dtc" ]; then
	DTC="$WORK/dtc/root/usr/bin/dtc"
else
	echo "[1/4] 下载 dtc"
	mkdir -p "$WORK/dtc"
	(cd "$WORK/dtc" && apt-get download device-tree-compiler >/dev/null 2>&1 \
		&& dpkg-deb -x ./*.deb root)
	DTC="$WORK/dtc/root/usr/bin/dtc"
fi

# ---------- 源码 ----------
echo "[2/4] 准备 dts"
cp "$HERE/rk3326-r36s.dts" "$WORK/src/"
cp "$HERE"/upstream/rk3326-odroid-go.dtsi "$HERE"/upstream/rk3326.dtsi "$WORK/src/"
fetch arch/arm64/boot/dts/rockchip/px30.dtsi "$WORK/src/px30.dtsi"

# ---------- 头文件 ----------
echo "[3/4] 准备 dt-bindings"
for h in \
	dt-bindings/gpio/gpio.h \
	dt-bindings/input/input.h \
	dt-bindings/leds/common.h \
	dt-bindings/pinctrl/rockchip.h \
	dt-bindings/interrupt-controller/arm-gic.h \
	dt-bindings/interrupt-controller/irq.h \
	dt-bindings/phy/phy.h \
	dt-bindings/clock/px30-cru.h \
	dt-bindings/power/px30-power.h \
	dt-bindings/soc/rockchip,boot-mode.h \
	dt-bindings/thermal/thermal.h
do
	fetch "include/$h" "$WORK/inc/$h"
done
# input.h 里是 #include "linux-event-codes.h"
fetch include/uapi/linux/input-event-codes.h \
	"$WORK/inc/dt-bindings/input/linux-event-codes.h"

# ---------- 编译 ----------
echo "[4/4] 编译"
cd "$WORK"
gcc -E -nostdinc -I inc -I src -x assembler-with-cpp src/rk3326-r36s.dts -o out.dts.pp
"$DTC" -I dts -O dtb -o out.dtb out.dts.pp 2>&1 \
	| grep -vE "avoid_unnecessary_addr_size|graph_child_address|^$" || true

if [ -f out.dtb ]; then
	echo
	echo "✅ 通过：$WORK/out.dtb（$(stat -c%s out.dtb) 字节）"
	echo "   注意：这只证明语法和引用没问题，不证明能开机。"
else
	echo "❌ 编译失败"
	exit 1
fi
