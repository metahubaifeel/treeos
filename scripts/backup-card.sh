#!/bin/bash
# ============================================================
# TreeOS 整卡备份（母卡 -> 压缩镜像）
#
#   sudo bash backup-card.sh /dev/sdX
#
# 产物：.backup/treeos-mothercard-YYYYMMDD-HHMM.img.gz
# 恢复：gunzip -c xxx.img.gz | sudo dd of=/dev/sdX bs=16M status=progress
# 注意：恢复会覆盖目标卡全部内容，先确认设备号！
# ============================================================
set -euo pipefail

DEV="${1:-}"
[ -n "$DEV" ] || { echo "用法: sudo bash backup-card.sh /dev/sdX"; exit 1; }
[ -b "$DEV" ] || { echo "不是块设备: $DEV"; exit 1; }

OUTDIR=/home/amd/下载/r36s/.backup
mkdir -p "$OUTDIR"
OUT="$OUTDIR/treeos-mothercard-$(date +%Y%m%d-%H%M).img.gz"

echo "源卡: $DEV"
lsblk "$DEV"
echo "输出: $OUT"
read -r -p "确认开始？(yes/no) " ok
[ "$ok" = "yes" ] || exit 1

dd if="$DEV" bs=64M status=progress | gzip -1 > "$OUT"
sync
ls -lh "$OUT"
sha256sum "$OUT" | tee "$OUT.sha256"
