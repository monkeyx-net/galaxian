#include "assets/extract.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(dir, mode) _mkdir(dir)
#endif

#include "assets/png.h"
#include "emu/board.h"
#include "gfx/galaxian.h"

#define TILES 256
#define SPRITES 64
/* the attract mode's screens for the previews (the emulated board's frames) */
static const struct {
    long frame;
    const char *name;
} screens[] = { { 600, "game_over" }, { 1800, "score_table" }, { 3700, "demo" } };
#define SCREENS ((int)(sizeof screens / sizeof screens[0]))

/* ---- files ----------------------------------------------------------------- */

static int mkdir_p(const char *path)
{
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(tmp, 0755) && errno != EEXIST) return -1;
        *p = '/';
    }
    return mkdir(tmp, 0755) && errno != EEXIST ? -1 : 0;
}

/* Write `size` bytes to <dir>/<name> through a temp file, so a failure leaves no partial file. */
static int write_file(const char *dir, const char *name, const void *data, size_t size, char *err, int errlen)
{
    char path[1200], tmp[1210];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f || fwrite(data, 1, size, f) != size || fclose(f)) {
        if (f) fclose(f);
        remove(tmp);
        snprintf(err, (size_t)errlen, "cannot write %s", path);
        return -1;
    }
#ifdef _WIN32
    remove(path); /* Windows rename() does not replace */
#endif
    if (rename(tmp, path)) {
        remove(tmp);
        snprintf(err, (size_t)errlen, "cannot replace %s", path);
        return -1;
    }
    return 0;
}

/* A growing text buffer. */
typedef struct {
    char *s;
    size_t len, cap;
} Text;

static void put(Text *t, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void put(Text *t, const char *fmt, ...)
{
    va_list ap;
    for (;;) {
        va_start(ap, fmt);
        int n = vsnprintf(t->s ? t->s + t->len : NULL, t->s ? t->cap - t->len : 0, fmt, ap);
        va_end(ap);
        if (n < 0) return;
        if (t->s && t->len + (size_t)n < t->cap) {
            t->len += (size_t)n;
            return;
        }
        size_t cap = t->cap ? t->cap * 2 : 4096;
        while (cap < t->len + (size_t)n + 1) cap *= 2;
        char *s = realloc(t->s, cap);
        if (!s) abort();
        t->s = s;
        t->cap = cap;
    }
}

static int write_text(const char *dir, const char *name, Text *t, char *err, int errlen)
{
    int rc = write_file(dir, name, t->s ? t->s : "", t->len, err, errlen);
    free(t->s);
    memset(t, 0, sizeof *t);
    return rc;
}

/* An RGB picture (0xRRGGBB) as a PNG under <dir>/gfx. */
static int write_png(const char *dir, const char *name, const uint32_t *px, int w, int h, char *err, int errlen)
{
    uint8_t *rgba = malloc((size_t)w * (size_t)h * 4);
    if (!rgba) {
        snprintf(err, (size_t)errlen, "out of memory");
        return -1;
    }
    for (long i = 0; i < (long)w * h; i++) {
        rgba[i * 4] = (uint8_t)(px[i] >> 16);
        rgba[i * 4 + 1] = (uint8_t)(px[i] >> 8);
        rgba[i * 4 + 2] = (uint8_t)px[i];
        rgba[i * 4 + 3] = 0xFF;
    }
    char path[1200];
    snprintf(path, sizeof path, "%s/gfx/%s", dir, name);
    int rc = png_write(path, rgba, w, h);
    free(rgba);
    if (rc) snprintf(err, (size_t)errlen, "cannot write %s", path);
    return rc ? -1 : 0;
}

/* ---- graphics ---------------------------------------------------------------- */

/* Tile `t`'s pixel on its line `row`, `col` along the line (1h the high bit). */
static int tile_pixel(const RomSet *rom, int t, int row, int col)
{
    int a = t * 8 + row, bit = 7 - col;
    return (rom->gfx[0][a] >> bit & 1) << 1 | (rom->gfx[1][a] >> bit & 1);
}

/* A preview's colours: the four pixel values in grey. */
static const uint32_t grey[4] = { 0x000000, 0x555555, 0xAAAAAA, 0xFFFFFF };

static int extract_tiles(const RomSet *rom, const char *dir, char *err, int errlen)
{
    Text t = {0};
    put(&t, "# 256 tiles of 1h and 1k: 8 board lines of 8 pixels along the line (1h the high bit)\n");
    for (int n = 0; n < TILES; n++) {
        put(&t, "tile %02X\n", n);
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) put(&t, "%d", tile_pixel(rom, n, row, col));
            put(&t, "\n");
        }
    }
    char gfx[1100];
    snprintf(gfx, sizeof gfx, "%s/gfx", dir);
    if (write_text(gfx, "tiles.txt", &t, err, errlen)) return -1;

    /* previews as the player sees them: a board line is a column, right to left */
    static uint32_t px[128 * 128];
    for (int n = 0; n < TILES; n++)
        for (int row = 0; row < 8; row++)
            for (int col = 0; col < 8; col++)
                px[((n / 16) * 8 + col) * 128 + (n % 16) * 8 + 7 - row] = grey[tile_pixel(rom, n, row, col)];
    if (write_png(dir, "tiles.png", px, 128, 128, err, errlen)) return -1;
    for (int n = 0; n < SPRITES; n++)
        for (int row = 0; row < 16; row++)
            for (int col = 0; col < 16; col++) {
                int tile = n * 4 + (row >> 3) * 2 + (col >> 3);
                px[((n / 8) * 16 + col) * 128 + (n % 8) * 16 + 15 - row] = grey[tile_pixel(rom, tile, row & 7, col & 7)];
            }
    return write_png(dir, "sprites.png", px, 128, 128, err, errlen);
}

static int extract_palette(const RomSet *rom, const char *dir, char *err, int errlen)
{
    static GalaxianVideo video;
    galaxian_video_init(&video, rom);
    Text t = {0};
    put(&t, "# the 32 colours of PROM 6l: tile and sprite colour c uses 4c-4c+3\n");
    for (int i = 0; i < 32; i++) put(&t, "%06X\n", (unsigned)video.palette[i]);
    char gfx[1100];
    snprintf(gfx, sizeof gfx, "%s/gfx", dir);
    return write_text(gfx, "palette.txt", &t, err, errlen);
}

static int extract_font(const char *dir, char *err, int errlen)
{
    static const char chars[] = EXTRACT_FONT_CHARS;
    Text t = {0};
    put(&t, "# the game's characters: tile, character (the letters are $11-$2A; $0A-$0F are hex digits)\n");
    for (int i = 0; chars[i]; i++) put(&t, "%02X %s\n", i, chars[i] == ' ' ? "space" : (char[2]){ chars[i], 0 });
    char gfx[1100];
    snprintf(gfx, sizeof gfx, "%s/gfx", dir);
    return write_text(gfx, "font.txt", &t, err, errlen);
}

/* ---- the attract mode's screens ------------------------------------------------ */

static Board board;
static Framebuffer frame;

static int extract_screens(const RomSet *rom, const char *dir, char *err, int errlen)
{
    board_init(&board, rom);
    if (fb_init(&frame, 1)) {
        snprintf(err, (size_t)errlen, "out of memory");
        return -1;
    }
    static uint32_t px[FB_WIDTH * FB_HEIGHT];
    int rc = 0;
    for (int i = 0; i < SCREENS && !rc; i++) {
        while (board.frame < screens[i].frame) board_run_frame(&board);
        galaxian_to_fb(&board.video, &frame);
        for (int y = 0; y < FB_HEIGHT; y++)
            for (int x = 0; x < FB_WIDTH; x++) px[y * FB_WIDTH + x] = fb_row(&frame, y)[x] & 0xFFFFFF;
        char name[64];
        snprintf(name, sizeof name, "%s.png", screens[i].name);
        rc = write_png(dir, name, px, FB_WIDTH, FB_HEIGHT, err, errlen);
    }
    fb_free(&frame);
    return rc;
}

int extract_all(const RomSet *rom, const char *out_dir, char *err, int errlen)
{
    char path[1100];
    if (!rom->loaded) {
        snprintf(err, (size_t)errlen, "no ROM set loaded");
        return -1;
    }
    snprintf(path, sizeof path, "%s/%s", out_dir, EXTRACT_MANIFEST);
    remove(path); /* until the end, the folder is incomplete */
    snprintf(path, sizeof path, "%s/gfx", out_dir);
    if (mkdir_p(path)) {
        snprintf(err, (size_t)errlen, "cannot create %s", path);
        return -1;
    }
    if (extract_tiles(rom, out_dir, err, errlen) || extract_palette(rom, out_dir, err, errlen) ||
        extract_font(out_dir, err, errlen) || extract_screens(rom, out_dir, err, errlen))
        return -1;
    Text t = {0};
    put(&t, "galaxian assets %d\n", EXTRACT_VERSION);
    return write_text(out_dir, EXTRACT_MANIFEST, &t, err, errlen);
}
