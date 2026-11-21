# Boot splash handoff (0.1.3)

Status: **visual issue closed on the test unit** (cold boot shows the image continuously;
only a ~0.2s modeset flicker remains). Shipped in the 0.1.3 preview.

## Timeline

```
0–5s    U-Boot/kernel, backlight off, no userspace splash   ← known limitation
5.05s   treeos-splash first frame (direct DRM/KMS)
11.88s  splash stop: write the same image to /dev/fb0, restore fbcon, drop DRM master
13.85s  ES binary starts
16.42s  ES window created
16.68s  ES early boot splash redraws the same PNG (+0.26s)
17.51s  ES loading screen first frame
```

The fb0 content was verified byte-identical to the selected raw image.

## Design

1. `treeos-splash.service` does **not** wait for `systemd-udevd`; `treeos-splash-run`
   waits for `/dev/dri/card0` itself.
2. `treeos-splash-drm.c` uses a dumb buffer + `drmModeSetCrtc` directly (no SDL).
   SDL's `SDL_CreateWindow` costs ~2.7s on RK3326; direct DRM takes 15–50ms.
3. On stop it writes the same 640×480 BGRA frame to `/dev/fb0`, restores the previous
   CRTC/fbcon, then drops DRM master. fbcon keeps showing the image instead of black.
4. `treeos-es.sh` stops the splash, waits for DRM release, and execs the ES binary
   directly (no ArkOS/BaRT wrapper; saves ~3s).
5. ES draws the same PNG immediately after `Renderer::init` (`TreeOS early boot splash`)
   and again in `renderLoadingScreen`, full-screen.

## Files

- `tools/launcher/treeos-splash-drm.c` / `treeos-splash-drm` (prebuilt aarch64)
- `tools/launcher/treeos-splash-run`
- `tools/launcher/treeos-splash.service`
- `tools/launcher/treeos-es.sh`
- `tools/launcher/install-boot-splash.sh`
- `tools/es/treeos-boot-splash.patch` (Window.cpp + main.cpp only)
- `tools/es/treeos-es-working-tree.patch` (full 5-file diff)
- `tools/es/emulationstation-treeos-boot-splash` (prebuilt, TreeOS 0.1)
- `assets/boot/png/boot-01.png … boot-21.png`

ES source base: `metahubaifeel/EmulationStation-fcamod`, commit `e9b60e7`.
Do **not** build from `/home/tree/es-gl` or `/home/tree/es-build`; those are upstream
ES 2.13 and lose TreeOS features.

## Install

```bash
# running card
sudo tools/launcher/install-boot-splash.sh

# offline rootfs
sudo tools/launcher/install-boot-splash.sh /mnt/rootfs
```

The script installs the splash, unit, ES binary and PNGs, and retires the old
`treeos-bootlogo.service` enable link. The raw images must already exist in
`/usr/local/share/treeos-boot/*.raw`.

## Known limitations

- 0–5s U-Boot/kernel: no backlight, no userspace image. U-Boot logo/PWM backlight or
  a minimal initramfs is a separate project.
- ES window init is ~2.3s and `window.init` another ~1.7s; the image covers them, but
  entering ES takes ~17–18s.
- Hot reboot can hang in U-Boot; test with a cold power cycle.
