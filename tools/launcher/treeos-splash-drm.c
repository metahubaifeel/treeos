// TreeOS direct DRM/KMS boot splash
// 2026-11-20: 替代 SDL2 KMSDRM 版本。SDL 在 RK3326 上 SDL_CreateWindow 约 2.6s，
// 直接 dumb buffer + drmModeSetCrtc 通常几十毫秒。
// 输入：640x480 BGRA raw（1,228,800 字节）。
// 输出：/run/treeos-splash.log 记录各阶段 monotonic 毫秒；
//       /run/treeos-splash-first 只在第一帧上屏后写一次；
//       背光在第一帧上屏后才开到最大。
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static void logline(const char *fmt, ...) {
    FILE *l = fopen("/run/treeos-splash.log", "a");
    if (!l) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(l, fmt, ap);
    va_end(ap);
    fputc('\n', l);
    fclose(l);
}

static void backlight_max(void) {
    FILE *b = fopen("/sys/class/backlight/backlight/max_brightness", "r");
    if (!b) return;
    char m[16] = {0};
    if (fgets(m, sizeof m, b)) {
        FILE *w = fopen("/sys/class/backlight/backlight/brightness", "w");
        if (w) { fputs(m, w); fclose(w); }
    }
    fclose(b);
}

int main(int argc, char **argv) {
    double t0 = now_ms();
    if (argc < 2) return 2;

    FILE *f = fopen(argv[1], "rb");
    if (!f) return 3;
    unsigned char *src = malloc(640 * 480 * 4);
    if (!src) return 4;
    if (fread(src, 1, 640 * 480 * 4, f) != 640 * 480 * 4) { fclose(f); return 5; }
    fclose(f);
    logline("start t=%.1f file=%s", t0, argv[1]);

    int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) { logline("open card0 failed"); return 6; }
    logline("open t=%.1f (+%.1f)", now_ms(), now_ms() - t0);

    drmModeRes *res = drmModeGetResources(fd);
    if (!res) { logline("get resources failed"); return 7; }
    drmModeConnector *conn = NULL;
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *c = drmModeGetConnector(fd, res->connectors[i]);
        if (!c) continue;
        if (c->connection == DRM_MODE_CONNECTED && c->count_modes > 0 &&
            c->connector_type == DRM_MODE_CONNECTOR_DSI) {
            conn = c;
            break;
        }
        drmModeFreeConnector(c);
    }
    if (!conn) { logline("no connected DSI connector"); return 8; }
    drmModeModeInfo mode = conn->modes[0];
    for (int i = 0; i < conn->count_modes; i++) {
        if (conn->modes[i].hdisplay == 640 && conn->modes[i].vdisplay == 480) {
            mode = conn->modes[i];
            break;
        }
    }
    logline("connector %u mode %dx%d @%d", conn->connector_id, mode.hdisplay, mode.vdisplay, mode.vrefresh);

    drmModeEncoder *enc = NULL;
    if (conn->encoder_id) enc = drmModeGetEncoder(fd, conn->encoder_id);
    if (!enc) {
        for (int i = 0; i < conn->count_encoders && !enc; i++)
            enc = drmModeGetEncoder(fd, conn->encoders[i]);
    }
    if (!enc) { logline("no encoder"); return 9; }
    uint32_t crtc_id = enc->crtc_id;
    if (!crtc_id) {
        for (int i = 0; i < res->count_crtcs; i++) {
            if (enc->possible_crtcs & (1 << i)) { crtc_id = res->crtcs[i]; break; }
        }
    }
    if (!crtc_id) { logline("no crtc"); return 10; }
    logline("crtc %u t=%.1f (+%.1f)", crtc_id, now_ms(), now_ms() - t0);
    drmModeCrtc *old_crtc = drmModeGetCrtc(fd, crtc_id);

    struct drm_mode_create_dumb creq;
    memset(&creq, 0, sizeof creq);
    creq.width = 640;
    creq.height = 480;
    creq.bpp = 32;
    if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) { logline("create dumb failed"); return 11; }
    uint32_t fb_id = 0;
    if (drmModeAddFB(fd, 640, 480, 24, 32, creq.pitch, creq.handle, &fb_id)) {
        logline("addfb failed"); return 12;
    }
    struct drm_mode_map_dumb mreq;
    memset(&mreq, 0, sizeof mreq);
    mreq.handle = creq.handle;
    if (ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq)) { logline("map dumb failed"); return 13; }
    unsigned char *dst = mmap(NULL, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
    if (dst == MAP_FAILED) { logline("mmap failed"); return 14; }
    for (int y = 0; y < 480; y++) {
        const unsigned char *s = src + y * 640 * 4;
        unsigned char *d = dst + y * creq.pitch;
        for (int x = 0; x < 640; x++) {
            d[x * 4 + 0] = s[x * 4 + 0]; // B
            d[x * 4 + 1] = s[x * 4 + 1]; // G
            d[x * 4 + 2] = s[x * 4 + 2]; // R
            d[x * 4 + 3] = 0xff;         // X
        }
    }
    logline("copy t=%.1f (+%.1f)", now_ms(), now_ms() - t0);

    if (drmSetMaster(fd) != 0) logline("drmSetMaster failed (continuing)");
    if (drmModeSetCrtc(fd, crtc_id, fb_id, 0, 0, &conn->connector_id, 1, &mode) != 0) {
        logline("setcrtc failed");
        return 15;
    }
    double tp = now_ms();
    backlight_max();
    FILE *q = fopen("/run/treeos-splash-first", "w");
    if (q) { fprintf(q, "%.1f\n", tp); fclose(q); }
    logline("first-present t=%.1f (+%.1f) backlight=max", tp, tp - t0);

    while (access("/dev/shm/treeos-splash-stop", F_OK) != 0)
        usleep(100000);

    logline("stop t=%.1f (+%.1f)", now_ms(), now_ms() - t0);
    // 交接：先把同一张图写进 fbcon 的 /dev/fb0，再恢复进入前的 CRTC/fbcon。
    // 这样 splash 退出后、ES modeset 之前，fbcon 显示的是这张图而不是黑屏。
    {
        int fbfd = open("/dev/fb0", O_WRONLY);
        if (fbfd >= 0) {
            size_t off = 0, total = 640 * 480 * 4;
            while (off < total) {
                ssize_t n = write(fbfd, src + off, total - off);
                if (n <= 0) break;
                off += (size_t)n;
            }
            close(fbfd);
            logline("fb0-write %zu/%zu t=%.1f", off, total, now_ms());
        }
    }
    if (old_crtc) {
        if (old_crtc->mode_valid)
            drmModeSetCrtc(fd, crtc_id, old_crtc->buffer_id, old_crtc->x, old_crtc->y,
                           &conn->connector_id, 1, &old_crtc->mode);
        else
            drmModeSetCrtc(fd, crtc_id, 0, 0, 0, NULL, 0, NULL);
        drmModeFreeCrtc(old_crtc);
    }
    drmDropMaster(fd);
    munmap(dst, creq.size);
    drmModeRmFB(fd, fb_id);
    struct drm_mode_destroy_dumb dreq;
    memset(&dreq, 0, sizeof dreq);
    dreq.handle = creq.handle;
    ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
    close(fd);
    return 0;
}
