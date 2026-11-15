#!/bin/bash
# ============================================================
# TreeOS · 把 ArkOS 的菜单层搬进 Debian（在掌机上 sudo 运行）
#
#   sudo bash treeos-migrate-arkos.sh [/mnt/treeos]
#
# 原则：只搬"开机要用 + 玩要用"的，不搬 ArkOS 的历史包袱。
#   搬：ES 本体、主题、菜单脚本、核心、配置、兼容库、4.4 内核模块
#   不搬：PortMaster(1.9G)、cheats(313M)、database(218M)
# ============================================================
set -euo pipefail

ROOT=${1:-/mnt/treeos}
[ -d "$ROOT/etc" ] || { echo "目标 rootfs 不存在: $ROOT"; exit 1; }
[ "$(id -u)" -ne 0 ] && { echo "请用 sudo"; exit 1; }

COMPAT="$ROOT/opt/treeos-compat/lib"
mkdir -p "$COMPAT"
say() { echo "[TreeOS] $*"; }

# ---------------------------------------------------------------- 1. 小件
say "1/5 小程序与菜单脚本"
mkdir -p "$ROOT/opt/system" "$ROOT/usr/local" "$ROOT/usr/bin" "$ROOT/etc"
cp -a /usr/bin/emulationstation            "$ROOT/usr/bin/"        # 23 MB
cp -a /usr/local/bin                       "$ROOT/usr/local/"      # 30 MB
cp -a /opt/retroarch                       "$ROOT/opt/"            # 24 MB
cp -a /etc/emulationstation                "$ROOT/etc/"            # 主题 101 MB

# 菜单脚本：只搬根目录的 .sh / .xml / Advanced，不搬 Tools（PortMaster 1.9G）
find /opt/system -maxdepth 1 -type f \( -name '*.sh' -o -name '*.xml' -o -name '*.py' \) \
	-exec cp -a {} "$ROOT/opt/system/" \; 2>/dev/null || true
[ -d /opt/system/Advanced ] && cp -a /opt/system/Advanced "$ROOT/opt/system/"

# ---------------------------------------------------------------- 2. 模拟器配置
say "2/5 RetroArch 配置与核心"
mkdir -p "$ROOT/home/tree/.config"
for d in cores config autoconfig system assets overlay shaders filters playlists; do
	[ -e "/home/ark/.config/retroarch/$d" ] && mkdir -p "$ROOT/home/tree/.config/retroarch" \
		&& cp -a "/home/ark/.config/retroarch/$d" "$ROOT/home/tree/.config/retroarch/"
done
cp -an /home/ark/.config/retroarch/*.cfg "$ROOT/home/tree/.config/retroarch/" 2>/dev/null || true
[ -d /home/ark/.emulationstation ] && cp -a /home/ark/.emulationstation "$ROOT/home/tree/"

# ---------------------------------------------------------------- 3. 内核模块
say "3/5 厂商 4.4 内核模块（K1 启动要用）"
if [ -d /lib/modules/4.4.189 ]; then
	mkdir -p "$ROOT/lib/modules"
	cp -a /lib/modules/4.4.189 "$ROOT/lib/modules/"
	say "  $(du -sh /lib/modules/4.4.189 | cut -f1)"
fi

# ---------------------------------------------------------------- 4. 兼容库
say "4/5 旧库依赖闭包"
COL=$(mktemp)
printf '%s\n' /usr/bin/emulationstation/emulationstation /opt/retroarch/bin/retroarch > "$COL"
seen=" "
copy_deps() {
	local bin=$1 depth=$2
	[ "$depth" -gt 6 ] && return
	local libs
	libs=$(LD_TRACE_LOADED_OBJECTS=1 "$bin" 2>/dev/null | awk '/=> \//{print $3}' || true)
	for l in $libs; do
		case " $seen " in *" $l "*) continue;; esac
		seen="$seen$l "
		case "$(basename "$l")" in
			libc.so*|libm.so*|libpthread.so*|libdl.so*|librt.so*|ld-linux*|libgcc_s*|libstdc++*) continue;;
		esac
		[ -f "$l" ] || continue
		cp -a "$l" "$COMPAT/" 2>/dev/null || true
		copy_deps "$l" $((depth+1))
	done
}
while read -r b; do [ -e "$b" ] && copy_deps "$b" 0; done < "$COL"
rm -f "$COL"
for gl in /usr/local/lib/aarch64-linux-gnu/libEGL.so /lib/aarch64-linux-gnu/libMali.so \
	  /usr/local/lib/aarch64-linux-gnu/libGLESv2.so; do
	[ -e "$gl" ] && cp -a "$gl" "$COMPAT/" 2>/dev/null || true
done
say "  兼容库 $(ls "$COMPAT" | wc -l) 个，$(du -sh "$COMPAT" | cut -f1)"

cat > "$ROOT/etc/ld.so.conf.d/treeos-compat.conf" <<EOF
/opt/treeos-compat/lib
EOF

# ---------------------------------------------------------------- 5. 启动封装
say "5/5 启动封装"
cat > "$ROOT/usr/local/bin/treeos-es.sh" <<'EOF'
#!/bin/bash
export LD_LIBRARY_PATH=/opt/treeos-compat/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
exec /usr/bin/emulationstation/emulationstation.sh "$@"
EOF
chmod +x "$ROOT/usr/local/bin/treeos-es.sh"

chroot "$ROOT" sh -c 'chown -R 1002:1002 /home/tree /opt/system /opt/retroarch /usr/bin/emulationstation 2>/dev/null || true'

say "完成：$(du -sh "$ROOT" | cut -f1)（不含 PortMaster/cheats/database）"
