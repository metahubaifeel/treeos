#!/bin/bash
# TreeOS 内核构建（需要先准备源码树与工具链，见 .build/env.sh）
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="${SRC:-$HERE/../../.build/linux-6.12}"
[ -d "$SRC" ] || { echo "找不到内核源码树: $SRC"; exit 1; }

cp "$HERE/config-6.12-r36s" "$SRC/.config"
cp "$HERE/../dts/rk3326-r36s.dts" "$SRC/arch/arm64/boot/dts/rockchip/"
cp "$HERE/../dts/panel-r36s.c" "$SRC/drivers/gpu/drm/panel/" 2>/dev/null || true

cd "$SRC"
source "$HERE/../../.build/env.sh"
make olddefconfig
make -j"$(nproc)" Image rockchip/rk3326-r36s.dtb

cp arch/arm64/boot/Image "$HERE/Image-6.12-r36s"
cp arch/arm64/boot/dts/rockchip/rk3326-r36s.dtb "$HERE/rk3326-r36s.dtb"
sha256sum "$HERE/Image-6.12-r36s" "$HERE/rk3326-r36s.dtb" > "$HERE/SHA256SUMS"
echo "完成：$HERE"
