/* One visual language for the overlay: rounded cards, key pills and a short
 * fade/slide when the page changes. Drawing only; no input semantics here. */
#include <math.h>

static int tw(TTF_Font *f, const char *s) {
    int w = 0;
    if (!s || !*s || TTF_SizeUTF8(f, s, &w, NULL) != 0) return 0;
    return (int)(w / ui_density + 0.5f);
}
static void ink(SDL_Renderer *r, int R, int G, int B, int A) {
    SDL_SetRenderDrawColor(r, (Uint8)R, (Uint8)G, (Uint8)B, (Uint8)(A * ui_alpha / 255));
}
/* Thin line-art glyphs, drawn from segments so no image assets ship.
 * Style: 1px stroke, round joins, monochrome — matching a 100-icon line set. */
static void px_line(SDL_Renderer *r, float x0, float y0, float x1, float y1) {
    SDL_RenderDrawLineF(r, x0, y0, x1, y1);
}
static void seg(SDL_Renderer *r, int x, int y, float x0, float y0, float x1, float y1) {
    px_line(r, ui_px(x) + x0 * ui_density, ui_px(y) + y0 * ui_density,
               ui_px(x) + x1 * ui_density, ui_px(y) + y1 * ui_density);
}
static void poly(SDL_Renderer *r, int x, int y, const float *pts, int n) {
    for (int i = 0; i < n; i += 2) {
        int j = (i + 2) % n;
        seg(r, x, y, pts[i], pts[i + 1], pts[j], pts[j + 1]);
    }
}
static void ring(SDL_Renderer *r, int x, int y, float cx, float cy, float rad, float from, float to) {
    float step = 6.0f;
    for (float a = from; a < to; a += step) {
        float b = a + step > to ? to : a + step;
        float x0 = cx + rad * cosf(a * 3.14159265f / 180), y0 = cy + rad * sinf(a * 3.14159265f / 180);
        float x1 = cx + rad * cosf(b * 3.14159265f / 180), y1 = cy + rad * sinf(b * 3.14159265f / 180);
        seg(r, x, y, x0, y0, x1, y1);
    }
}
static void icon(SDL_Renderer *r, int kind, int x, int y, int S) {
    static const float dpad[] = {5,0, 9,0, 9,5, 14,5, 14,9, 9,9, 9,14, 5,14, 5,9, 0,9, 0,5, 5,5};
    static const float floppy[] = {0,0, 14,0, 14,14, 0,14};
    static const float mouse[] = {4,0, 10,0, 10,4, 10,14, 4,14, 4,4};
    static const float tri[] = {5,4, 11,7, 5,10};
    switch (kind) {
    case 0: poly(r, x, y, dpad, 24); break;
    case 1:
        poly(r, x, y, mouse, 12);
        seg(r, x, y, 4, 5, 10, 5);
        seg(r, x, y, 7, 5, 7, 8);
        break;
    case 2:
        poly(r, x, y, floppy, 8);
        seg(r, x, y, 3, 0, 3, 4); seg(r, x, y, 11, 0, 11, 4); seg(r, x, y, 3, 4, 11, 4);
        seg(r, x, y, 3, 9, 3, 14); seg(r, x, y, 11, 9, 11, 14); seg(r, x, y, 3, 9, 11, 9);
        break;
    case 3:
        ring(r, x, y, 7, 7, 6.5f, 0, 360);
        poly(r, x, y, tri, 6);
        break;
    case 4:
        ring(r, x, y, 7, 8, 6, -60, 240);
        seg(r, x, y, 7, 1, 7, 8);
        break;
    }
    (void)S;
}
static const char *game_title(void) {
    const char *e = getenv("FLASH_GAME_TITLE");
    return e && *e ? e : "Flash 游戏";
}
