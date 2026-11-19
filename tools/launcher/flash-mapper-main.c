#define _POSIX_C_SOURCE 200809L

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <dirent.h>
#include <time.h>
#include <math.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define PROFILE_DIR "/roms/ports/rufflehandheld/profiles/custom"
#define FONT_PATH "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc"
#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define VISIBLE 8

static char return_swf[PATH_MAX];
static int exit_to_es = 0;
static int init_ok = 0;
static int capture_row = -1;   /* 正在等待用户按住手柄键的鼠标行 */
static const char *capture_hit;/* 当前按住、等待进度条走满的键 */
static unsigned capture_t0;    /* 开始按住的时刻 */
static int capture_progress;   /* 0..100 */

static const char *stick_names[] = {
    "鼠标指针", "WASD", "方向键 ↑↓←→"
};

typedef struct {
    const char *key;
    const char *label;
    int vk;            /* 兼容字段：第一个键 */
    int vks[6];        /* 一个手柄键可以同时发多个键盘键 */
    int nvk;
} Mapping;

static Mapping maps[] = {
    {"south",         "A（右侧键）", 74},
    {"east",          "B（下方键）", 75},
    {"west",          "X（上方键）", 85},
    {"north",         "Y（左侧键）", 73},
    {"dpad_up",       "方向上",     87},
    {"dpad_down",     "方向下",     83},
    {"dpad_left",     "方向左",     65},
    {"dpad_right",    "方向右",     68},
    {"left_trigger",  "L1",         74},
    {"right_trigger", "R1",         75},
    {"left_trigger2", "L2",         85},
    {"right_trigger2","R2",         73},
    {"start",         "START",      13},
    {"select",        "SELECT",     27}
};

/* 用户实际改过的行才写回 profile，避免把模板默认值写死 */
static int maps_touched[COUNT(maps)];

typedef struct {
    const char *name;
    int vk;
    int row, col;
    int w;      /* 键帽宽度（格），默认 1 */
} Choice;

/* 真实键盘布局：QWERTY 三行 + 功能键行 + 数字行 */
static const Choice choices[] = {
    {"1",49,0,0,1},{"2",50,0,1,1},{"3",51,0,2,1},{"4",52,0,3,1},
    {"5",53,0,4,1},{"6",54,0,5,1},{"7",55,0,6,1},{"8",56,0,7,1},
    {"9",57,0,8,1},{"0",48,0,9,1},
    {"Q",81,1,0,1},{"W",87,1,1,1},{"E",69,1,2,1},{"R",82,1,3,1},
    {"T",84,1,4,1},{"Y",89,1,5,1},{"U",85,1,6,1},{"I",73,1,7,1},
    {"O",79,1,8,1},{"P",80,1,9,1},
    {"A",65,2,0,1},{"S",83,2,1,1},{"D",68,2,2,1},{"F",70,2,3,1},
    {"G",71,2,4,1},{"H",72,2,5,1},{"J",74,2,6,1},{"K",75,2,7,1},
    {"L",76,2,8,1},{"回车",13,2,9,1},
    {"Z",90,3,0,1},{"X",88,3,1,1},{"C",67,3,2,1},{"V",86,3,3,1},
    {"B",66,3,4,1},{"N",78,3,5,1},{"M",77,3,6,1},
    {"ESC",27,3,9,1},
    {"空格",32,4,0,5},{"↑",38,3,8,1},
    {"←",37,4,7,1},{"↓",40,4,8,1},{"→",39,4,9,1}
};

typedef struct {
    char *data;
    size_t len;
    int mapping;
} Line;

typedef struct {
    unsigned cp;
    SDL_Texture *texture;
    int w, h, xoff, yoff, advance;
} Glyph;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *tex_a, *tex_b, *tex_x, *tex_y;
static SDL_Texture *tex_du, *tex_dd, *tex_dl, *tex_dr;
static SDL_Texture *tex_ml, *tex_mr;
static SDL_GameController *pad;

static unsigned char *font_data;
static stbtt_fontinfo font;
static float font_scale;
static int baseline;
static Glyph glyphs[512];
static int glyph_count;

static Line *lines;
static size_t line_count;
static mode_t profile_mode = 0644;
static char profile_path[PATH_MAX];

static volatile sig_atomic_t interrupted;
static const SDL_Color white = {230, 235, 244, 255};
static const SDL_Color muted = {155, 168, 188, 255};
static const SDL_Color cyan  = {100, 218, 245, 255};

static void on_signal(int sig)
{
    (void)sig;
    interrupted = 1;
}

/* UTF-8 decoder: all UI strings are trusted UTF-8 literals. */
static unsigned next_cp(const char **text)
{
    const unsigned char *p = (const unsigned char *)*text;
    unsigned cp;
    int n;

    if (*p < 0x80) {
        *text += 1;
        return *p;
    }
    if ((*p & 0xe0) == 0xc0) {
        cp = *p & 31;
        n = 2;
    } else if ((*p & 0xf0) == 0xe0) {
        cp = *p & 15;
        n = 3;
    } else if ((*p & 0xf8) == 0xf0) {
        cp = *p & 7;
        n = 4;
    } else {
        *text += 1;
        return '?';
    }

    for (int i = 1; i < n; ++i) {
        if ((p[i] & 0xc0) != 0x80) {
            *text += 1;
            return '?';
        }
        cp = (cp << 6) | (p[i] & 63);
    }
    *text += n;
    return cp;
}

static int load_font(void)
{
    FILE *fp = fopen(FONT_PATH, "rb");
    if (!fp)
        return -1;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    long size = ftell(fp);
    if (size <= 0 || size > 64L * 1024 * 1024 ||
        fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    font_data = malloc((size_t)size);
    if (!font_data) {
        fclose(fp);
        return -1;
    }
    int ok = fread(font_data, 1, (size_t)size, fp) == (size_t)size;
    fclose(fp);
    if (!ok)
        return -1;

    int offset = stbtt_GetFontOffsetForIndex(font_data, 0);
    if (offset < 0 || !stbtt_InitFont(&font, font_data, offset))
        return -1;

    font_scale = stbtt_ScaleForPixelHeight(&font, 24.0f);
    int ascent;
    stbtt_GetFontVMetrics(&font, &ascent, NULL, NULL);
    baseline = (int)(ascent * font_scale + 0.5f);
    return 0;
}

static Glyph *get_glyph(unsigned cp)
{
    for (int i = 0; i < glyph_count; ++i)
        if (glyphs[i].cp == cp)
            return &glyphs[i];

    if (glyph_count == COUNT(glyphs))
        return NULL;

    Glyph *g = &glyphs[glyph_count++];
    memset(g, 0, sizeof(*g));
    g->cp = cp;

    int advance;
    stbtt_GetCodepointHMetrics(&font, (int)cp, &advance, NULL);
    g->advance = (int)(advance * font_scale + 0.5f);

    unsigned char *bitmap = stbtt_GetCodepointBitmap(
        &font, font_scale, font_scale, (int)cp,
        &g->w, &g->h, &g->xoff, &g->yoff);

    if (bitmap && g->w > 0 && g->h > 0) {
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
            0, g->w, g->h, 32, SDL_PIXELFORMAT_ARGB8888);

        if (surface) {
            for (int y = 0; y < g->h; ++y) {
                Uint32 *dst = (Uint32 *)(
                    (unsigned char *)surface->pixels + y * surface->pitch);
                for (int x = 0; x < g->w; ++x)
                    dst[x] = SDL_MapRGBA(surface->format, 255, 255, 255,
                                        bitmap[y * g->w + x]);
            }
            g->texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
            if (g->texture)
                SDL_SetTextureBlendMode(g->texture, SDL_BLENDMODE_BLEND);
        }
    }
    stbtt_FreeBitmap(bitmap, NULL);
    return g;
}

static void text(int x, int y, SDL_Color color, const char *s)
{
    while (*s) {
        Glyph *g = get_glyph(next_cp(&s));
        if (!g)
            continue;
        if (g->texture) {
            SDL_Rect dst = {
                x + g->xoff, y + baseline + g->yoff, g->w, g->h
            };
            SDL_SetTextureColorMod(g->texture, color.r, color.g, color.b);
            SDL_RenderCopy(renderer, g->texture, NULL, &dst);
        }
        x += g->advance;
    }
}

static void rect(int x, int y, int w, int h, int r, int g, int b)
{
    SDL_Rect dst = {x, y, w, h};
    SDL_SetRenderDrawColor(renderer, (Uint8)r, (Uint8)g, (Uint8)b, 255);
    SDL_RenderFillRect(renderer, &dst);
}

static int text_w(const char *s)
{
    int w = 0;
    while (*s) {
        Glyph *g = get_glyph(next_cp(&s));
        if (g)
            w += g->advance;
    }
    return w;
}

static void text_c(int cx, int y, SDL_Color c, const char *s)
{
    text(cx - text_w(s) / 2, y, c, s);
}

static void disc(int cx, int cy, int r, int R, int G, int B)
{
    SDL_SetRenderDrawColor(renderer, (Uint8)R, (Uint8)G, (Uint8)B, 255);
    for (int dy = -r; dy <= r; ++dy) {
        int dx = (int)sqrt((double)(r * r - dy * dy));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void ring(int cx, int cy, int r, int R, int G, int B)
{
    SDL_SetRenderDrawColor(renderer, (Uint8)R, (Uint8)G, (Uint8)B, 255);
    int px = cx + r, py = cy;
    for (int i = 1; i <= 48; ++i) {
        double a = 6.283185307179586 * i / 48.0;
        int x = cx + (int)(r * cos(a));
        int y = cy + (int)(r * sin(a));
        SDL_RenderDrawLine(renderer, px, py, x, y);
        px = x; py = y;
    }
}

static SDL_Texture *load_png(const char *path)
{
    int w, h, n;
    unsigned char *pix = stbi_load(path, &w, &h, &n, 4);
    if (!pix)
        return NULL;
    SDL_Texture *t = SDL_CreateTexture(renderer,
                                       SDL_PIXELFORMAT_ABGR8888,
                                       SDL_TEXTUREACCESS_STATIC, w, h);
    if (t) {
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(t, NULL, pix, w * 4);
    }
    stbi_image_free(pix);
    return t;
}

/* 用 Kenney Input Prompts（CC0）现成图标，不再手画按钮 */
static void load_icons(void)
{
    static const char *dir = "/opt/flashmenu/icons/";
    char p[PATH_MAX];
#define LOAD(var, name) \
    do { snprintf(p, sizeof(p), "%s%s", dir, name); var = load_png(p); } while (0)
    LOAD(tex_a, "a.png");
    LOAD(tex_b, "b.png");
    LOAD(tex_x, "x.png");
    LOAD(tex_y, "y.png");
    LOAD(tex_du, "dpad_up.png");
    LOAD(tex_dd, "dpad_down.png");
    LOAD(tex_dl, "dpad_left.png");
    LOAD(tex_dr, "dpad_right.png");
    LOAD(tex_ml, "mouse_left.png");
    LOAD(tex_mr, "mouse_right.png");
#undef LOAD
}

static void blit_tex(SDL_Texture *t, int cx, int cy, int size,
                     int r, int g, int b)
{
    if (!t || size <= 0)
        return;
    SDL_Rect dst = {cx - size / 2, cy - size / 2, size, size};
    SDL_SetTextureColorMod(t, (Uint8)r, (Uint8)g, (Uint8)b);
    SDL_RenderCopy(renderer, t, NULL, &dst);
}

static SDL_Texture *face_tex(const char *key)
{
    if (!strcmp(key, "south")) return tex_a;
    if (!strcmp(key, "east")) return tex_b;
    if (!strcmp(key, "west")) return tex_x;
    if (!strcmp(key, "north")) return tex_y;
    return NULL;
}

static SDL_Texture *dpad_tex(const char *key)
{
    if (!strcmp(key, "dpad_up")) return tex_du;
    if (!strcmp(key, "dpad_down")) return tex_dd;
    if (!strcmp(key, "dpad_left")) return tex_dl;
    if (!strcmp(key, "dpad_right")) return tex_dr;
    return NULL;
}

static const struct {
    const char *key, *label;
    int r, g, b;
} kc[] = {
    {"south", "A", 205, 55, 55},
    {"east",  "B", 232, 192, 45},
    {"west",  "X", 45, 105, 215},
    {"north", "Y", 55, 170, 75},
};

static void dpad_icon(const char *key, int cx, int cy, int arm, int hi)
{
    int th = arm * 2 / 3;
    int R = hi ? 80 : 62, G = hi ? 200 : 72, B = hi ? 255 : 92;

    rect(cx - arm, cy - th / 2, arm * 2, th, 62, 72, 92);
    rect(cx - th / 2, cy - arm, th, arm * 2, 62, 72, 92);
    if (!strcmp(key, "dpad_up"))
        rect(cx - th / 2, cy - arm, th, arm * 2 / 3, R, G, B);
    else if (!strcmp(key, "dpad_down"))
        rect(cx - th / 2, cy + arm / 3, th, arm * 2 / 3, R, G, B);
    else if (!strcmp(key, "dpad_left"))
        rect(cx - arm, cy - th / 2, arm * 2 / 3, th, R, G, B);
    else if (!strcmp(key, "dpad_right"))
        rect(cx + arm / 3, cy - th / 2, arm * 2 / 3, th, R, G, B);
}

/* 手柄键小图标；hi=1 表示当前按住 */
static void key_icon(const char *key, int cx, int cy, int r, int hi)
{
    if (!key)
        return;
    for (int i = 0; i < COUNT(kc); ++i) {
        if (!strcmp(key, kc[i].key)) {
            SDL_Texture *t = face_tex(key);
            if (t)
                blit_tex(t, cx, cy, r * 2 + 4, kc[i].r, kc[i].g, kc[i].b);
            else {
                disc(cx, cy, r, kc[i].r, kc[i].g, kc[i].b);
                text_c(cx, cy - r + 3, (SDL_Color){25, 25, 25, 255}, kc[i].label);
            }
            if (hi)
                ring(cx, cy, r + 4, 255, 255, 255);
            return;
        }
    }
    if (!strncmp(key, "dpad_", 5)) {
        SDL_Texture *t = dpad_tex(key);
        if (t)
            blit_tex(t, cx, cy, r * 2 + 2, 255, 255, 255);
        else
            dpad_icon(key, cx, cy, r, hi);
        if (hi)
            ring(cx, cy, r + 5, 255, 255, 255);
        return;
    }
    const char *lab = NULL;
    if (!strcmp(key, "left_trigger")) lab = "L1";
    else if (!strcmp(key, "right_trigger")) lab = "R1";
    else if (!strcmp(key, "left_trigger2")) lab = "L2";
    else if (!strcmp(key, "right_trigger2")) lab = "R2";
    else if (!strcmp(key, "start")) lab = "ST";
    else if (!strcmp(key, "select")) lab = "SE";
    if (lab) {
        rect(cx - 16, cy - 11, 32, 22, hi ? 90 : 62, hi ? 190 : 72, hi ? 255 : 92);
        text_c(cx, cy - 12, (SDL_Color){235, 235, 235, 255}, lab);
    }
}

static void mouse_icon(int cx, int cy, int left_hi)
{
    SDL_Texture *t = left_hi ? tex_ml : tex_mr;
    if (t) {
        blit_tex(t, cx, cy, 30, 255, 255, 255);
        return;
    }
    rect(cx - 9, cy - 14, 18, 28, 55, 65, 85);
    rect(cx - 8, cy - 13, 7, 10,
         left_hi ? 90 : 45, left_hi ? 190 : 58, left_hi ? 255 : 78);
    rect(cx + 1, cy - 13, 7, 10,
         left_hi ? 45 : 90, left_hi ? 58 : 190, left_hi ? 78 : 255);
    rect(cx - 1, cy - 13, 2, 28, 20, 26, 38);
}

static void init_maps(void)
{
    for (int i = 0; i < COUNT(maps); ++i) {
        maps[i].vks[0] = maps[i].vk;
        maps[i].nvk = maps[i].vk ? 1 : 0;
    }
}

/* 把一组键码拼成 "74,32" */
static void key_codes(const Mapping *m, char *out, size_t n)
{
    size_t used = 0;
    out[0] = '\0';
    for (int i = 0; i < m->nvk && used + 1 < n; ++i)
        used += (size_t)snprintf(out + used, n - used, "%s%d",
                                 i ? "," : "", m->vks[i]);
}

static const char *vk_name(int vk)
{
    for (int i = 0; i < COUNT(choices); ++i)
        if (choices[i].vk == vk)
            return choices[i].name;
    return "自定义";
}

static int key_in(const Mapping *m, int vk)
{
    for (int i = 0; i < m->nvk; ++i)
        if (m->vks[i] == vk)
            return 1;
    return 0;
}

static void key_add(Mapping *m, int vk)
{
    if (vk <= 0 || key_in(m, vk) || m->nvk >= (int)(sizeof(m->vks) / sizeof(m->vks[0])))
        return;
    m->vks[m->nvk++] = vk;
    m->vk = m->vks[0];
}

static void key_del(Mapping *m, int vk)
{
    int j = 0;
    for (int i = 0; i < m->nvk; ++i)
        if (m->vks[i] != vk)
            m->vks[j++] = m->vks[i];
    m->nvk = j;
    m->vk = j ? m->vks[0] : 0;
}

static void key_toggle(Mapping *m, int vk)
{
    if (key_in(m, vk))
        key_del(m, vk);
    else
        key_add(m, vk);
}

/* 把一组键拼成 "J/空格/↑" */
static void key_names(const Mapping *m, char *out, size_t n)
{
    size_t used = 0;
    out[0] = '\0';
    for (int i = 0; i < m->nvk && used + 1 < n; ++i)
        used += (size_t)snprintf(out + used, n - used, "%s%s",
                                 used ? "/" : "", vk_name(m->vks[i]));
    if (!out[0])
        snprintf(out, n, "未绑定");
}

/* Match only an exact supported key followed by optional spaces and '='. */
static int mapping_index(const char *s, const char **value)
{
    while (isspace((unsigned char)*s))
        ++s;
    for (int i = 0; i < COUNT(maps); ++i) {
        size_t n = strlen(maps[i].key);
        if (strncmp(s, maps[i].key, n) != 0)
            continue;
        const char *p = s + n;
        while (*p == ' ' || *p == '\t')
            ++p;
        if (*p == '=') {
            if (value)
                *value = p + 1;
            return i;
        }
    }
    return -1;
}

/*
 * Keep every original line in memory.
 * Unrelated lines are written back byte for byte.
 * Duplicate supported keys are all rewritten to the selected value.
 */
static int load_profile(void)
{
    FILE *fp = fopen(profile_path, "rb");
    if (!fp)
        return errno == ENOENT ? 0 : -1;

    struct stat st;
    if (fstat(fileno(fp), &st) != 0) {
        fclose(fp);
        return -1;
    }
    profile_mode = st.st_mode & 0777;

    char *buf = NULL;
    size_t capacity = 0;
    ssize_t n;
    int result = 0;

    while ((n = getline(&buf, &capacity, fp)) >= 0) {
        Line *grown = realloc(lines, (line_count + 1) * sizeof(*lines));
        if (!grown) {
            errno = ENOMEM;
            result = -1;
            break;
        }
        lines = grown;

        char *copy = malloc((size_t)n + 1);
        if (!copy) {
            errno = ENOMEM;
            result = -1;
            break;
        }
        memcpy(copy, buf, (size_t)n + 1);

        const char *value = NULL;
        int index = mapping_index(copy, &value);
        lines[line_count++] = (Line){copy, (size_t)n, index};

        if (index >= 0) {
            Mapping *m = &maps[index];
            char *p = (char *)value;
            m->nvk = 0;
            while (*p) {
                char *end;
                errno = 0;
                long vk = strtol(p, &end, 10);
                if (end == p) {
                    ++p;
                    continue;
                }
                if (!errno && vk > 0 && vk <= 65535)
                    key_add(m, (int)vk);
                p = end;
                while (*p == ',' || *p == '|' || *p == ' ' || *p == '\t')
                    ++p;
            }
            m->vk = m->nvk ? m->vks[0] : 0;
        }
    }

    if (ferror(fp)) {
        errno = EIO;
        result = -1;
    }
    int saved_errno = errno;
    free(buf);
    if (fclose(fp) != 0 && result == 0)
        return -1;
    errno = saved_errno;
    return result;
}

static int mkdirs(const char *path)
{
    char tmp[PATH_MAX];
    if (snprintf(tmp, sizeof(tmp), "%s", path) >= (int)sizeof(tmp)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    for (char *p = tmp + 1;; ++p) {
        if (*p == '/' || *p == '\0') {
            char saved = *p;
            *p = '\0';

            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;

            struct stat st;
            if (stat(tmp, &st) != 0)
                return -1;
            if (!S_ISDIR(st.st_mode)) {
                errno = ENOTDIR;
                return -1;
            }

            *p = saved;
            if (!saved)
                break;
        }
    }
    return 0;
}

/* Same-directory temporary file + fsync + rename. */
static int save_profile(void)
{
    if (mkdirs(PROFILE_DIR) != 0)
        return -1;

    char temporary[PATH_MAX];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp.XXXXXX",
                 profile_path) >= (int)sizeof(temporary)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    int fd = mkstemp(temporary);
    if (fd < 0)
        return -1;
    if (fchmod(fd, profile_mode) != 0) {
        int e = errno;
        close(fd);
        unlink(temporary);
        errno = e;
        return -1;
    }

    FILE *fp = fdopen(fd, "wb");
    if (!fp) {
        int e = errno;
        close(fd);
        unlink(temporary);
        errno = e;
        return -1;
    }

    int seen[COUNT(maps)] = {0};
    int ok = 1;
    int ended_with_newline = 1;

    for (size_t i = 0; i < line_count && ok; ++i) {
        Line *line = &lines[i];
        if (line->mapping >= 0) {
            int k = line->mapping;
            char codes[64];
            seen[k] = 1;
            key_codes(&maps[k], codes, sizeof(codes));
            ok = fprintf(fp, "%s=%s\n", maps[k].key, codes) >= 0;
            ended_with_newline = 1;
        } else {
            ok = fwrite(line->data, 1, line->len, fp) == line->len;
            if (line->len)
                ended_with_newline = line->data[line->len - 1] == '\n';
        }
    }

    for (int i = 0; i < COUNT(maps) && ok; ++i) {
        if (seen[i] || !maps_touched[i])
            continue;
        char codes[64];
        key_codes(&maps[i], codes, sizeof(codes));
        if (!ended_with_newline)
            ok = fputc('\n', fp) != EOF;
        if (ok)
            ok = fprintf(fp, "%s=%s\n", maps[i].key, codes) >= 0;
        ended_with_newline = 1;
    }

    if (ok && fflush(fp) != 0)
        ok = 0;
    if (ok && fsync(fileno(fp)) != 0)
        ok = 0;

    int e = errno;
    if (fclose(fp) != 0) {
        ok = 0;
        e = errno;
    }
    if (!ok) {
        unlink(temporary);
        errno = e ? e : EIO;
        return -1;
    }

    if (rename(temporary, profile_path) != 0) {
        e = errno;
        unlink(temporary);
        errno = e;
        return -1;
    }

    /* Best effort directory sync after the successful rename. */
    int dirfd = open(PROFILE_DIR, O_RDONLY | O_DIRECTORY);
    if (dirfd >= 0) {
        if (fsync(dirfd) != 0)
            fprintf(stderr, "Directory fsync: %s\n", strerror(errno));
        close(dirfd);
    }
    return 0;
}

/* Prefer the named built-in controller; otherwise use the first controller. */
static void open_pad(void)
{
    if (pad)
        return;

    for (int pass = 0; pass < 2 && !pad; ++pass) {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (!SDL_IsGameController(i))
                continue;
            const char *name = SDL_GameControllerNameForIndex(i);
            if (pass == 0 && (!name || !strstr(name, "r36s")))
                continue;
            pad = SDL_GameControllerOpen(i);
            if (pad)
                break;
        }
    }
}

/* 当前按住的、可以绑成鼠标键的手柄键；SELECT 留给取消。 */
static const char *bind_candidate(void)
{
    if (!pad || !SDL_GameControllerGetAttached(pad))
        return NULL;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A)) return "south";
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B)) return "east";
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X)) return "west";
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y)) return "north";
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) return "l1";
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) return "r1";
    if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000) return "l2";
    if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) return "r2";
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START)) return "start";
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK)) return "select";
    return NULL;
}

static int grid_move(int cur, int dx, int dy)
{
    int r = choices[cur].row, c = choices[cur].col;
    if (dx) {
        int n = cur + dx;
        if (n >= 0 && n < COUNT(choices) && choices[n].row == r)
            return n;
        return cur;
    }
    int best = cur, bd = 99;
    for (int i = 0; i < COUNT(choices); ++i) {
        if (choices[i].row != r + dy)
            continue;
        int d = choices[i].col > c ? choices[i].col - c : c - choices[i].col;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

enum {
    INPUT_UP = 1,
    INPUT_DOWN = 2,
    INPUT_A = 4,
    INPUT_B = 8,
    INPUT_LEFT = 16,
    INPUT_RIGHT = 32,
    INPUT_FN = 64,
    INPUT_EXIT = 128
};

static unsigned read_input(void)
{
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    unsigned bits = 0;

    if (keys[SDL_SCANCODE_UP]) bits |= INPUT_UP;
    if (keys[SDL_SCANCODE_DOWN]) bits |= INPUT_DOWN;
    if (keys[SDL_SCANCODE_LEFT]) bits |= INPUT_LEFT;
    if (keys[SDL_SCANCODE_RIGHT]) bits |= INPUT_RIGHT;
    if (keys[SDL_SCANCODE_RETURN]) bits |= INPUT_A;
    if (keys[SDL_SCANCODE_ESCAPE]) bits |= INPUT_B;

    if (pad && SDL_GameControllerGetAttached(pad)) {
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP))
            bits |= INPUT_UP;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
            bits |= INPUT_DOWN;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
            bits |= INPUT_LEFT;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
            bits |= INPUT_RIGHT;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A))
            bits |= INPUT_A;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B))
            bits |= INPUT_B;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_GUIDE))
            bits |= INPUT_FN;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK) &&
            SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START))
            bits |= INPUT_EXIT;
    }
    return bits;
}


/* ---- 高级选项 ---- */
static int adv_i, adv_changed;
static char adv_backend[8]="auto", adv_quality[8]="low", adv_view[16]="auto",
            adv_stick[8]="left", adv_speed[4]="6",
            adv_template[24]="arrows_pointer", adv_click_l[8]="none", adv_click_r[8]="none";

/* 鼠标左右键挂在“按键映射”页最后两行，不放在高级页。 */
#define CLICK_ROWS 2
#define MAP_TOTAL (COUNT(maps) + CLICK_ROWS)
#define ROW_CLICK_L COUNT(maps)
#define ROW_CLICK_R (COUNT(maps) + 1)
static const char *click_names[] = {"none","south","east","west","north",
                                    "l1","r1","l2","r2","start","select"};
static const char *click_labels[] = {"无","A","B","X","Y",
                                     "L1","R1","L2","R2","START","SELECT"};

static const char *click_cn(const char *c)
{
    for (int i = 0; i < COUNT(click_names); ++i)
        if (!strcmp(c, click_names[i])) return click_labels[i];
    return "无";
}
static const char *click_of_row(int row)
{
    return row == ROW_CLICK_L ? adv_click_l : adv_click_r;
}
static const char *template_cn(const char *t)
{
    if (!strcmp(t, "arrows")) return "方向键";
    if (!strcmp(t, "wasd")) return "WASD";
    if (!strcmp(t, "pointer")) return "左摇杆当鼠标";
    if (!strcmp(t, "wasd_pointer")) return "WASD+右摇杆";
    return "方向键+右摇杆";
}
static const char *quality_cn(const char *q)
{
    if (!strcmp(q, "medium")) return "均衡";
    if (!strcmp(q, "high")) return "清晰";
    if (!strcmp(q, "best")) return "最高";
    return "流畅";
}
static const char *stick_cn(const char *s)
{
    if (!strcmp(s, "right")) return "右摇杆";
    if (!strcmp(s, "off")) return "关闭";
    return "左摇杆";
}

static int cfg_get(const char *key, char *out, size_t n, const char *def)
{
    char cmd[512]; FILE *fp;
    snprintf(cmd, sizeof(cmd), "/usr/local/bin/flashcfg get '%s' %s 2>/dev/null", profile_path, key);
    fp = popen(cmd, "r"); if (!fp) { snprintf(out,n,"%s",def); return 0; }
    if (!fgets(out, (int)n, fp)) snprintf(out,n,"%s",def);
    pclose(fp);
    char *nl = strchr(out,'\n'); if (nl) *nl=0;
    if (!out[0]) snprintf(out,n,"%s",def);
    return 0;
}
static void cfg_set(const char *key, const char *val)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "/usr/local/bin/flashcfg set '%s' %s '%s' 2>/dev/null", profile_path, key, val);
    if (system(cmd) != 0) fprintf(stderr, "flashcfg set %s failed\n", key);
}
static void adv_cycle(int row, int dir)
{
    static const char *q[]={"low","medium","high","best"};
    static const char *b[]={"auto","enhanced","classic"};
    static const char *s[]={"left","right","off"};
    int i;
    if (row==0) { for(i=0;i<3;i++) if(!strcmp(adv_backend,b[i])) break; snprintf(adv_backend,8,"%s",b[(i+dir+3)%3]); }
    else if (row==1) { for(i=0;i<4;i++) if(!strcmp(adv_quality,q[i])) break; snprintf(adv_quality,8,"%s",q[(i+dir+4)%4]); }
    else if (row==2) { static const char *v[]={"auto","640x480","480x360","550x400"};
        for(i=0;i<4;i++) if(!strcmp(adv_view,v[i])) break;
        snprintf(adv_view,16,"%s",v[(i+dir+4)%4]); }
    else if (row==3) { for(i=0;i<3;i++) if(!strcmp(adv_stick,s[i])) break; snprintf(adv_stick,8,"%s",s[(i+dir+3)%3]); }
    else if (row==4) { int v=atoi(adv_speed)+dir; if(v<1)v=1; if(v>10)v=10; snprintf(adv_speed,4,"%d",v); }
    else if (row==5) { static const char *t[]={"arrows","wasd","pointer","arrows_pointer","wasd_pointer"};
        for(i=0;i<5;i++) if(!strcmp(adv_template,t[i])) break;
        snprintf(adv_template,24,"%s",t[(i+dir+5)%5]); }
    else if (row==6) { for(i=0;i<COUNT(click_names);i++) if(!strcmp(adv_click_l,click_names[i])) break;
        snprintf(adv_click_l,8,"%s",click_names[(i+dir+COUNT(click_names))%COUNT(click_names)]); }
    else if (row==7) { for(i=0;i<COUNT(click_names);i++) if(!strcmp(adv_click_r,click_names[i])) break;
        snprintf(adv_click_r,8,"%s",click_names[(i+dir+COUNT(click_names))%COUNT(click_names)]); }
    adv_changed=1;
}
static void adv_load(void)
{
    cfg_get("runtime.backend", adv_backend, sizeof adv_backend, "auto");
    cfg_get("video.quality", adv_quality, sizeof adv_quality, "low");
    cfg_get("video.viewport", adv_view, sizeof adv_view, "auto");
    cfg_get("input.mouse_stick", adv_stick, sizeof adv_stick, "left");
    cfg_get("input.mouse_speed", adv_speed, sizeof adv_speed, "6");
    cfg_get("control.template", adv_template, sizeof adv_template, "arrows_pointer");
    cfg_get("control.mouse_left", adv_click_l, sizeof adv_click_l, "none");
    cfg_get("control.mouse_right", adv_click_r, sizeof adv_click_r, "none");
}
static void adv_save(void)
{
    cfg_set("runtime.backend", adv_backend);
    cfg_set("video.quality", adv_quality);
    cfg_set("video.viewport", adv_view);
    cfg_set("input.mouse_stick", adv_stick);
    cfg_set("input.mouse_speed", adv_speed);
    cfg_set("control.template", adv_template);
    cfg_set("control.mouse_left", adv_click_l);
    cfg_set("control.mouse_right", adv_click_r);
    /* 用户在设置页看过并保存过，就算确认过，首次运行不再重复询问 */
    cfg_set("control.confirmed", "1");
    adv_changed=0;
}

/* 绑定页：独立整屏，避免任何键同时承担“返回/取消” */
static void draw_capture(void)
{
    char buf[160];
    const char *hit = capture_hit;
    SDL_SetRenderDrawColor(renderer, 8, 10, 16, 255);
    SDL_RenderClear(renderer);

    text(28, 20, white, capture_row == ROW_CLICK_L ?
                       "按键映射 · 鼠标左键" : "按键映射 · 鼠标右键");
    text(28, 62, cyan, capture_row == ROW_CLICK_L ? "设置：鼠标左键" : "设置：鼠标右键");
    rect(28, 96, 584, 2, 45, 57, 76);

    text(28, 112, white, "请按住你要绑定的手柄键");
    text(28, 148, muted, "约 2 秒，进度条走满即绑定；FN 取消");

    /* 左：十字键 */
    dpad_icon(hit && !strncmp(hit, "dpad_", 5) ? hit : "", 152, 250, 52, 0);
    if (hit && !strncmp(hit, "dpad_", 5))
        dpad_icon(hit, 152, 250, 52, 1);
    text_c(152, 318, muted, "方向键");

    /* 右：ABXY（上 X、左 Y、右 A、下 B，和机器一致） */
    key_icon("west",  420, 202, 26, hit && !strcmp(hit, "west"));
    key_icon("north", 362, 252, 26, hit && !strcmp(hit, "north"));
    key_icon("south", 478, 252, 26, hit && !strcmp(hit, "south"));
    key_icon("east",  420, 302, 26, hit && !strcmp(hit, "east"));
    text_c(420, 340, muted, "ABXY");

    /* L/R/START/SELECT 被按住时，在中间补一个图标 */
    if (hit && (strstr(hit, "trigger") || !strcmp(hit, "start") ||
                !strcmp(hit, "select")))
        key_icon(hit, 300, 210, 20, 1);

    if (hit)
        snprintf(buf, sizeof(buf), "当前按住：手柄 %s", click_cn(hit));
    else
        snprintf(buf, sizeof(buf), "当前按住：无");
    text(28, 366, (SDL_Color){255, 220, 80, 255}, buf);

    rect(48, 408, 544, 20, 28, 36, 52);
    int w = 544 * (capture_progress > 100 ? 100 : capture_progress) / 100;
    if (w > 0)
        rect(48, 408, w, 20, 80, 200, 255);
    snprintf(buf, sizeof(buf), "%d%%", capture_progress);
    text(300, 434, muted, buf);

    SDL_RenderPresent(renderer);
}

/* page: 0 main menu, 1 controller mapping, 2 VK chooser. */
static void draw_ui(int page, int menu, int selected, int choice,
                    int stick, int dirty, const char *status)
{
    if (capture_row >= 0) {
        draw_capture();
        return;
    }
    const char *menu_names[] = {
        "按键映射", "高级选项",
        return_swf[0] ? "保存并返回游戏" : "保存并退出",
        "退出游戏（返回菜单）"
    };
    char buf[160];

    SDL_SetRenderDrawColor(renderer, 16, 21, 31, 255);
    SDL_RenderClear(renderer);

    const char *titles[] = {"Flash 启动器设置", "按键映射", "选择键盘键", "高级选项"};
    text(28, 20, white, titles[page >= 0 && page <= 3 ? page : 0]);
    snprintf(buf, sizeof(buf), "%s%s",
             strrchr(profile_path, '/') + 1, dirty ? "  *" : "");
    text(28, 55, muted, buf);
    rect(28, 91, 584, 2, 45, 57, 76);

    if (page == 0) {
        for (int i = 0; i < 4; ++i) {
            int y = 122 + i * 58;
            if (i == menu)
                rect(28, y - 9, 584, 54, 42, 71, 104);
            text(46, y, white, menu_names[i]);
        }
        text(28, 374, muted, "星号表示映射已修改；保存失败不会退出");

    } else if (page == 1) {
        int first = (selected / VISIBLE) * VISIBLE;
        int total = MAP_TOTAL;

        for (int row = 0; row < VISIBLE && first + row < total; ++row) {
            int i = first + row;
            int y = 108 + row * 36;
            if (i == selected)
                rect(28, y - 3, 584, 34, 42, 71, 104);
            if (i < COUNT(maps)) {
                char names[96];
                key_icon(maps[i].key, 56, y + 14, 14, 0);
                text(84, y, white, maps[i].label);
                key_names(&maps[i], names, sizeof(names));
                text(355, y, cyan, names);
            } else {
                mouse_icon(56, y + 14, i == ROW_CLICK_L);
                text(84, y, white, i == ROW_CLICK_L ? "鼠标左键" : "鼠标右键");
                snprintf(buf, sizeof(buf), "手柄 %s", click_cn(click_of_row(i)));
                text(355, y, (SDL_Color){255, 220, 80, 255}, buf);
            }
        }
        if (capture_row >= 0) {
            snprintf(buf, sizeof(buf), "按住 %s 不松，进度满即绑定（轻点 B 取消）",
                     capture_hit ? click_cn(capture_hit) : "手柄键");
            text(28, 386, muted, buf);
            rect(28, 414, 584, 10, 28, 36, 52);
            int w = 584 * (capture_progress > 100 ? 100 : capture_progress) / 100;
            if (w > 0)
                rect(28, 414, w, 10, 80, 200, 255);
        } else {
            if (selected >= ROW_CLICK_L)
                snprintf(buf, sizeof(buf), "A 进绑定页，按住手柄键 2 秒；左右可换  共 %d 项", total);
            else
                snprintf(buf, sizeof(buf), "A 进键盘页可多选；最下两行是鼠标左右键  共 %d 项", total);
            text(28, 402, muted, buf);
        }
    } else if (page == 2) {
        /* 键盘布局页 */
        static const int stagger[5] = {0, 0, 0, 0, 0};
        for (int i = 0; i < COUNT(choices); ++i) {
            int w = (choices[i].w ? choices[i].w : 1) * 60 - 4;
            int x = 26 + choices[i].col * 60 + stagger[choices[i].row];
            int y = 100 + choices[i].row * 56;
            int chosen = key_in(&maps[selected], choices[i].vk);
            if (i == choice)
                rect(x - 3, y - 3, w + 6, 48, 42, 71, 104);
            else if (chosen)
                rect(x - 3, y - 3, w + 6, 48, 46, 92, 70);
            else
                rect(x - 3, y - 3, w + 6, 48, 28, 36, 52);
            text(x + 7, y + 4, white, choices[i].name);
            if (chosen)
                rect(x + w - 13, y + 3, 9, 9, 110, 230, 140);
            {
                const char *sn[] = {"A","B","X","Y","↑","↓","←","→","L1","R1","L2","R2","ST","SE"};
                char lab[48] = "";
                for (int m = 0; m < COUNT(maps); ++m)
                    if (key_in(&maps[m], choices[i].vk)) {
                        if (lab[0]) strncat(lab, "/", sizeof(lab)-strlen(lab)-1);
                        strncat(lab, sn[m], sizeof(lab)-strlen(lab)-1);
                    }
                if (lab[0]) text(x + 7, y + 22, (SDL_Color){255,220,80,255}, lab);
            }
        }
        snprintf(buf, sizeof(buf), "为 %s 选键：A 添加/取消，长按 A 移除，B 返回", maps[selected].label);
        text(28, 400, muted, buf);
    } else {
        /* 高级选项页 */
        const char *labels[]={"运行方式","画面清晰度","画面尺寸","鼠标摇杆（经典）","鼠标速度","控制方案"};
        const char *backend_show = !strcmp(adv_backend,"auto") ? "自动" :
                                   (!strcmp(adv_backend,"classic")||!strcmp(adv_backend,"a")) ? "经典" : "增强";
        char view_show[24];
        if (!strcmp(adv_view, "auto")) snprintf(view_show, sizeof view_show, "自动");
        else snprintf(view_show, sizeof view_show, "%s", adv_view);
        const char *vals[]={backend_show, quality_cn(adv_quality), view_show,
                            stick_cn(adv_stick), adv_speed, template_cn(adv_template)};
        const char *flag[]={"需重启","需重启","需重启","仅经典","仅经典","需重启"};
        for (int i=0;i<COUNT(labels);i++){
            int y=90+i*38;
            if (i==adv_i) rect(28,y-8,584,46,42,71,104);
            text(44,y,white,labels[i]);
            text(300,y,cyan,vals[i]);
            text(480,y,muted,flag[i]);
        }
        text(28,404,muted,"左右改值；“控制方案”决定手柄怎么对应键盘和鼠标");
    }
    rect(28, 434, 584, 1, 45, 57, 76);
    if (status[0])
        text(28, 462, cyan, status);
    else if (!pad)
        text(28, 446, muted, "未连接手柄；键盘：方向 / 回车 / ESC");
    else
        text(28, 446, muted, "方向上下选择   A 确定   B 返回");

    SDL_RenderPresent(renderer);
}

static char stick_path[PATH_MAX];

static int load_stick(void)
{
    char path[PATH_MAX], buf[16] = {0};
    snprintf(path, sizeof(path), "%s", profile_path);
    char *dot = strrchr(path, '.');
    if (dot) strcpy(dot, ".stick");
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    if (fgets(buf, sizeof(buf), fp)) { }
    fclose(fp);
    if (!strncmp(buf, "wasd", 4)) return 1;
    if (!strncmp(buf, "arrows", 6)) return 2;
    return 0;
}

static void save_stick(int mode)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s", profile_path);
    char *dot = strrchr(path, '.');
    if (dot) strcpy(dot, ".stick");
    FILE *fp = fopen(path, "w");
    if (!fp) return;
    fputs(mode == 1 ? "wasd" : mode == 2 ? "arrows" : "mouse", fp);
    fclose(fp);
}

static int valid_stem(const char *stem)
{
    size_t n = strlen(stem);
    if (!n || n > 48 || !strcmp(stem, ".") || !strcmp(stem, ".."))
        return 0;

    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)stem[i];
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
               c == '_' || c == '-' || c == '.'))
            return 0;
    }
    return 1;
}

static void cleanup(void)
{
    if (pad)
        SDL_GameControllerClose(pad);
    for (int i = 0; i < glyph_count; ++i)
        if (glyphs[i].texture)
            SDL_DestroyTexture(glyphs[i].texture);
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);

    SDL_Quit();
    free(font_data);
    for (size_t i = 0; i < line_count; ++i)
        free(lines[i].data);
    free(lines);
}

int main(int argc, char **argv)
{
    if (argc < 2 || !valid_stem(argv[1])) {
        fprintf(stderr,
                "Usage: %s <stem> [swf]\n"
                "stem: 1..48 ASCII letters/digits/._-; no .profile suffix needed\n",
                argv[0]);
        return 2;
    }

    snprintf(profile_path, sizeof(profile_path),
             "%s/%s.profile", PROFILE_DIR, argv[1]);
    init_maps();
    adv_load();
    if (argc > 2) snprintf(return_swf, sizeof(return_swf), "%s", argv[2]);

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    /*
     * Set only when not already supplied.
     * SDL_VIDEODRIVER=dummy can therefore be used for limited testing.
     */
    if (!getenv("SDL_VIDEODRIVER"))
        SDL_setenv("SDL_VIDEODRIVER", "kmsdrm", 1);

    if (load_profile() != 0) {
        fprintf(stderr, "Read profile %s: %s\n",
                profile_path, strerror(errno));
        goto done;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        goto done;
    }

    SDL_GameControllerAddMappingsFromFile(
        "/roms/tools/PortMaster/gamecontrollerdb.txt");
    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    window = SDL_CreateWindow(
        "Flash key mapper",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        640, 480, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        goto done;
    }

    /* Accelerated rendering first, software fallback second. */
    renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        goto done;
    }

    SDL_SetRenderDrawColor(renderer, 16, 21, 31, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    if (SDL_RenderSetLogicalSize(renderer, 640, 480) != 0) {
        fprintf(stderr, "Logical size: %s\n", SDL_GetError());
        goto done;
    }
    if (load_font() != 0) {
        fprintf(stderr, "Cannot load font: %s\n", FONT_PATH);
        goto done;
    }
    load_icons();

    open_pad();
    init_ok = 1;

    int page = 0, menu = 0, selected = 0, choice = 0;
    int stick = load_stick(), dirty = 0, redraw = 1, running = 1;
    unsigned previous = 0;
    Uint32 repeat_at = 0;
    int previous_direction = 0;
    unsigned key_t0 = 0;
    int key_held = 0, key_fired = 0;
    char status[128] = "";

    while (running && !interrupted) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT)
                running = 0;
            else if (ev.type == SDL_CONTROLLERDEVICEADDED) {
                open_pad();
                redraw = 1;
            } else if (ev.type == SDL_CONTROLLERDEVICEREMOVED) {
                if (pad && !SDL_GameControllerGetAttached(pad)) {
                    SDL_GameControllerClose(pad);
                    pad = NULL;
                    open_pad();
                    previous = 0;
                    redraw = 1;
                }
            } else if (ev.type == SDL_WINDOWEVENT)
                redraw = 1;
        }

        if (!running)
            break;

        unsigned input = read_input();
        unsigned pressed = input & ~previous;
        previous = input;

        /* 鼠标键绑定页：按住任意手柄键 3 秒即绑定，按任何键都不会退出本页 */
        if (capture_row >= 0) {
            const Uint8 *k = SDL_GetKeyboardState(NULL);
            unsigned now = SDL_GetTicks();
            const char *hit = bind_candidate();
            int cancelled = k[SDL_SCANCODE_ESCAPE] ||
                (pad && SDL_GameControllerGetButton(
                           pad, SDL_CONTROLLER_BUTTON_GUIDE)) ||
                (pad && SDL_GameControllerGetButton(
                           pad, SDL_CONTROLLER_BUTTON_START) &&
                 SDL_GameControllerGetButton(
                           pad, SDL_CONTROLLER_BUTTON_BACK));

            if (!cancelled) {
                if (!hit) {
                    capture_hit = NULL;
                    capture_progress = 0;
                } else {
                    if (!capture_hit || strcmp(capture_hit, hit)) {
                        capture_hit = hit;
                        capture_t0 = now;
                    }
                    unsigned held = now - capture_t0;
                    capture_progress = held >= 2000 ? 100 : (int)(held * 100 / 2000);
                    if (held >= 2000) {
                        if (capture_row == ROW_CLICK_L)
                            snprintf(adv_click_l, sizeof(adv_click_l), "%s", hit);
                        else
                            snprintf(adv_click_r, sizeof(adv_click_r), "%s", hit);
                        dirty = 1;
                        snprintf(status, sizeof(status), "已绑定：鼠标%s = 手柄 %s",
                                 capture_row == ROW_CLICK_L ? "左键" : "右键",
                                 click_cn(hit));
                        capture_row = -1;
                        capture_hit = NULL;
                        capture_progress = 0;
                    }
                }
            } else {
                capture_row = -1;
                capture_hit = NULL;
                capture_progress = 0;
                snprintf(status, sizeof(status), "已取消");
            }

            redraw = 1; /* 绑定页需要连续刷新进度条 */
            if (running) {
                draw_ui(page, menu, selected, choice, stick, dirty, status);
                redraw = 0;
            }
            SDL_Delay(12);
            continue;
        }

        /* 键盘页：A 短按添加/取消，长按 A 移除；B 返回 */
        if (page == 2) {
            if (input & INPUT_A) {
                if (!key_held) {
                    key_held = 1;
                    key_t0 = SDL_GetTicks();
                    key_fired = 0;
                } else if (!key_fired && SDL_GetTicks() - key_t0 >= 800) {
                    if (key_in(&maps[selected], choices[choice].vk)) {
                        key_del(&maps[selected], choices[choice].vk);
                        maps_touched[selected] = 1;
                        dirty = 1;
                    }
                    key_fired = 1;
                    redraw = 1;
                }
            } else if (key_held) {
                key_held = 0;
                if (!key_fired) {
                    key_toggle(&maps[selected], choices[choice].vk);
                    maps_touched[selected] = 1;
                    dirty = 1;
                }
                redraw = 1;
            }
        }

        int direction =
            !!(input & INPUT_DOWN) - !!(input & INPUT_UP);
        int step = 0;
        int dx = 0;
        if (page == 2 || page == 3 || (page == 1 && selected >= ROW_CLICK_L)) {
            unsigned rx = pressed & (INPUT_LEFT | INPUT_RIGHT);
            if (rx & INPUT_RIGHT) dx = 1;
            else if (rx & INPUT_LEFT) dx = -1;
        }
        Uint32 now = SDL_GetTicks();

        if (direction != previous_direction) {
            previous_direction = direction;
            if (direction) {
                step = direction;
                repeat_at = now + 350;
            }
        } else if (direction && (Sint32)(now - repeat_at) >= 0) {
            step = direction;
            repeat_at = now + 100;
        }

        if (page == 3) {
            if (dx) { adv_cycle(adv_i, dx); redraw=1; }
            if (step) { adv_i = (adv_i + step + 6) % 6; redraw=1; }
        } else if (page == 2) {
            if (dx)
                choice = grid_move(choice, dx, 0);
            if (step)
                choice = grid_move(choice, 0, step);
            if (dx || step) { status[0] = '\0'; redraw = 1; }
        } else if (page == 1 && selected >= ROW_CLICK_L) {
            if (dx) {
                adv_cycle(selected == ROW_CLICK_L ? 6 : 7, dx);
                dirty = 1;
            }
            if (step)
                selected = (selected + step + MAP_TOTAL) % MAP_TOTAL;
            if (dx || step) { status[0] = '\0'; redraw = 1; }
        } else if (step) {
            int *index = page == 0 ? &menu : (page == 3 ? &adv_i : &selected);
            int total = page == 0 ? 4 : (page == 3 ? 6 : MAP_TOTAL);
            *index = (*index + step + total) % total;
            status[0] = '\0';
            redraw = 1;
        }

        /* Select+Start：直接退出到 ES，不保存 */
        if (pressed & INPUT_EXIT) {
            exit_to_es = 1;
            running = 0;
            redraw = 1;
        } else if (pressed & INPUT_FN) {
            if (save_profile() == 0) { adv_save(); running = 0; }
            else snprintf(status, sizeof(status), "保存失败");
            redraw = 1;
        } else if (pressed & INPUT_B) {
            if (page == 3) page = 0;
            else if (page > 0) --page;
            status[0] = '\0';
            redraw = 1;
        } else if (pressed & INPUT_A) {
            status[0] = '\0';

            if (page == 0) {
                if (menu == 0) {
                    page = 1;
                } else if (menu == 1) {
                    page = 3;
                } else if (menu == 3) {
                    exit_to_es = 1;
                    running = 0;
                } else {
                    if (save_profile() == 0) {
                        adv_save();
                        running = 0;
                    } else {
                        fprintf(stderr, "Save %s: %s\n",
                                profile_path, strerror(errno));
                        snprintf(status, sizeof(status),
                                 "保存失败，详情见日志；可再次尝试");
                    }
                }
            } else if (page == 1) {
                if (selected >= ROW_CLICK_L) {
                    capture_row = selected;
                    capture_hit = NULL;
                    capture_progress = 0;
                    snprintf(status, sizeof(status),
                             "请按住你要绑定的手柄键（约 2 秒）");
                } else {
                    choice = 0;
                    for (int i = 0; i < COUNT(choices); ++i)
                        if (choices[i].vk == maps[selected].vk)
                            choice = i;
                    page = 2;
                    /* 进入页面的那一下 A 不算一次选择 */
                    key_held = 1;
                    key_fired = 1;
                    key_t0 = SDL_GetTicks();
                }
            } else if (page == 2) {
                /* A 由上面的按住/切换逻辑处理 */
            }
            redraw = 1;
        }

        if (redraw && running) {
            draw_ui(page, menu, selected, choice, stick, dirty, status);
            redraw = 0;
        }

        /* Idle without continuously rendering or consuming a CPU core. */
        SDL_Delay(12);
    }

done:
    /*
     * Release DRM/GBM resources before starting ES.
     * Startup failures and SIGINT/SIGTERM also return to ES without saving.
     */
    cleanup();
    fflush(NULL);

    /* 等 Ruffle 完全退出再启动 ES，避免争 DRM master 导致 ES 重启循环 */
    for (int i = 0; i < 50; ++i) {
        int busy = 0;
        DIR *dp = opendir("/proc");
        if (dp) {
            struct dirent *de;
            while ((de = readdir(dp))) {
                if (!isdigit((unsigned char)de->d_name[0])) continue;
                char path[64], comm[32] = {0};
                snprintf(path, sizeof(path), "/proc/%s/comm", de->d_name);
                FILE *f = fopen(path, "r");
                if (!f) continue;
                if (fgets(comm, sizeof(comm), f)) { }
                fclose(f);
                if (!strncmp(comm, "ruffle-native", 13)) { busy = 1; break; }
            }
            closedir(dp);
        }
        if (!busy) break;
        { struct timespec ts = {0, 100000000L}; nanosleep(&ts, NULL); }
    }

    if (!exit_to_es && init_ok && return_swf[0]) {
        const char *ret = getenv("FLASH_RETURN_SH");
        if (!ret || !ret[0]) ret = "/usr/local/bin/flash-return.sh";
        execl(ret, ret, return_swf, (char *)NULL);
    }
    execlp("systemctl", "systemctl", "start",
           "treeos-emulationstation", (char *)NULL);
    fprintf(stderr, "exec systemctl failed: %s\n", strerror(errno));
    return 1;
}
