#!/bin/sh
# 在已挂载的 rootfs 根目录执行：sanitize.sh /mnt/rootfs
# 只删除个人数据与凭据；不修改驱动/系统文件。
set -eu
R=${1:?usage: sanitize.sh <mounted-rootfs>}
[ -d "$R/etc" ] || { echo "not a rootfs: $R" >&2; exit 1; }

rm -f "$R"/etc/NetworkManager/system-connections/* 2>/dev/null || true
rm -f "$R"/etc/ssh/ssh_host_* 2>/dev/null || true
rm -rf "$R"/root/.ssh "$R"/home/*/.ssh 2>/dev/null || true
rm -f "$R"/root/.*history "$R"/home/*/.*history 2>/dev/null || true
rm -rf "$R"/home/*/.emulationstation/gamelists 2>/dev/null || true
rm -rf "$R"/home/*/.config/retroarch/playlists 2>/dev/null || true
rm -rf "$R"/tmp/* "$R"/var/tmp/* 2>/dev/null || true
rm -rf "$R"/var/log/* 2>/dev/null || true
echo "sanitized: $R"
