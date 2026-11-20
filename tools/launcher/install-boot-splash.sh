#!/bin/bash
# Install the TreeOS boot splash handoff into a running card or an offline rootfs.
#
#   sudo ./install-boot-splash.sh              # running system (/)
#   sudo ./install-boot-splash.sh /mnt/rootfs  # mounted card rootfs
#
# Files it installs:
#   /usr/local/bin/treeos-splash        direct DRM/KMS splash (prebuilt aarch64)
#   /usr/local/bin/treeos-splash-run    picks raw, records /run/treeos-splash-image
#   /usr/local/bin/treeos-es.sh         stop splash, wait, exec ES directly
#   /etc/systemd/system/treeos-splash.service
#   /usr/bin/emulationstation/emulationstation  TreeOS 0.1 + early boot splash
#   /usr/local/share/treeos-boot/png/*.png      21 matching loading images
#
# It does NOT create the raw images. They must already exist in
# /usr/local/share/treeos-boot/*.raw (the image build already ships them).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
WORKSPACE="$(cd "$HERE/../../.." && pwd)"
ROOT="${1:-}"
RUNNING=0
if [ -z "$ROOT" ]; then
    ROOT="/"
    RUNNING=1
fi
ROOT="${ROOT%/}"

if [ "$(id -u)" -ne 0 ]; then
    echo "run as root" >&2
    exit 1
fi

if [ -z "${ES_BIN:-}" ]; then
    for p in "$HERE/../es/emulationstation-treeos-boot-splash" \
             "$WORKSPACE/10_掌机游戏工程/tools/es/emulationstation-treeos-boot-splash"; do
        [ -x "$p" ] && { ES_BIN="$p"; break; }
    done
fi
if [ -z "${PNG_DIR:-}" ]; then
    for p in "$HERE/../../assets/boot/png" \
             "$WORKSPACE/11_TreeOS/brand/boot"; do
        [ -d "$p" ] && { PNG_DIR="$p"; break; }
    done
fi
[ -n "${ES_BIN:-}" ] && [ -x "$ES_BIN" ] || { echo "missing ES binary; set ES_BIN or build tools/es" >&2; exit 1; }
[ -n "${PNG_DIR:-}" ] && [ -d "$PNG_DIR" ] || { echo "missing PNG dir; set PNG_DIR" >&2; exit 1; }

echo "root: ${ROOT:-/}"

install -D -m 755 "$HERE/treeos-splash-drm" "$ROOT/usr/local/bin/treeos-splash"
install -D -m 755 "$HERE/treeos-splash-run" "$ROOT/usr/local/bin/treeos-splash-run"
install -D -m 755 "$HERE/treeos-es.sh"      "$ROOT/usr/local/bin/treeos-es.sh"
install -D -m 644 "$HERE/treeos-splash.service" "$ROOT/etc/systemd/system/treeos-splash.service"

if [ -f "$ROOT/usr/bin/emulationstation/emulationstation" ]; then
    cp -a "$ROOT/usr/bin/emulationstation/emulationstation" \
          "$ROOT/usr/bin/emulationstation/emulationstation.bak.$(date +%s)"
fi
install -D -m 755 "$ES_BIN" "$ROOT/usr/bin/emulationstation/emulationstation"

install -d -m 755 "$ROOT/usr/local/share/treeos-boot/png"
cp -f "$PNG_DIR"/boot-*.png "$ROOT/usr/local/share/treeos-boot/png/"

# Retire the old fb0-holder service; the new service owns the splash.
rm -f "$ROOT/etc/systemd/system/sysinit.target.wants/treeos-bootlogo.service"
install -d -m 755 "$ROOT/etc/systemd/system/sysinit.target.wants"
ln -sfn /etc/systemd/system/treeos-splash.service \
        "$ROOT/etc/systemd/system/sysinit.target.wants/treeos-splash.service"

if [ "$RUNNING" -eq 1 ]; then
    systemctl daemon-reload
    systemctl disable treeos-bootlogo.service 2>/dev/null || true
    systemctl enable treeos-splash.service
    echo "installed. reboot (cold power cycle on R36S) to test."
else
    echo "installed into $ROOT. Rebuild/unmount the card, then cold boot."
fi
