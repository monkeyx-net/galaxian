#include "gfx/galaxian.h"

#include <stdio.h>
#include <string.h>

#define STAR_PERIOD GALAXIAN_STAR_PERIOD
#define STAR_STEPS_PER_LINE 512  /* two a pixel */
#define SPRITE_FIRST 17          /* no sprite is drawn before this pixel along a line */

/* the starfield generator's sequence: bit 7 a star, bits 0-5 its colour */
static uint8_t stars[STAR_PERIOD];
static int stars_ready;

static void make_stars(void)
{
    uint32_t sr = 0;
    for (int i = 0; i < STAR_PERIOD; i++) {
        int on = (sr & 0x1FE01) == 0x1FE00;
        stars[i] = (uint8_t)((on ? 0x80 : 0) | ((~sr & 0x1F8) >> 3));
        sr = (sr >> 1) | ((((sr >> 12) ^ ~sr) & 1) << 16);
    }
    stars_ready = 1;
}

/* a star's colour: two bits each of red (5-4), green (3-2), blue (1-0) */
static uint32_t star_colour(int c)
{
    static const uint8_t level[4] = { 0x00, 0xD6, 0xC2, 0xFF };
    return (uint32_t)level[c >> 4 & 3] << 16 | (uint32_t)level[c >> 2 & 3] << 8 | level[c & 3];
}

void galaxian_video_init(GalaxianVideo *v, const RomSet *rom)
{
    if (!stars_ready) make_stars();
    memset(v, 0, sizeof *v);
    v->gfx[0] = rom->gfx[0];
    v->gfx[1] = rom->gfx[1];
    /* red and green: 3 bits weighing 29, 62, 133; blue: 2 bits, 69, 148 */
    for (int i = 0; i < 32; i++) {
        uint8_t p = rom->prom[i];
        int r = (p & 1) * 29 + (p >> 1 & 1) * 62 + (p >> 2 & 1) * 133;
        int g = (p >> 3 & 1) * 29 + (p >> 4 & 1) * 62 + (p >> 5 & 1) * 133;
        int bl = (p >> 6 & 1) * 69 + (p >> 7 & 1) * 148;
        v->palette[i] = (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)bl;
    }
}

int galaxian_video_load(GalaxianVideo *v, const char *dir)
{
    if (!stars_ready) make_stars();
    memset(v, 0, sizeof *v);
    char path[1200], line[256];
    snprintf(path, sizeof path, "%s/gfx/tiles.txt", dir);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int tile = -1, row = 0, rows = 0;
    while (fgets(line, sizeof line, f)) {
        unsigned n;
        if (line[0] == '#') continue;
        if (sscanf(line, "tile %x", &n) == 1) {
            tile = (int)n, row = 0;
            continue;
        }
        if (tile < 0 || tile > 255 || row > 7 || strlen(line) < 8) continue;
        for (int col = 0; col < 8; col++) {
            int p = line[col] - '0';
            v->planes[0][tile * 8 + row] |= (uint8_t)((p >> 1 & 1) << (7 - col));
            v->planes[1][tile * 8 + row] |= (uint8_t)((p & 1) << (7 - col));
        }
        row++, rows++;
    }
    fclose(f);
    snprintf(path, sizeof path, "%s/gfx/palette.txt", dir);
    f = fopen(path, "r");
    if (!f) return -1;
    int colours = 0;
    while (colours < 32 && fgets(line, sizeof line, f)) {
        unsigned c;
        if (line[0] != '#' && sscanf(line, "%x", &c) == 1) v->palette[colours++] = c & 0xFFFFFF;
    }
    fclose(f);
    v->gfx[0] = v->planes[0];
    v->gfx[1] = v->planes[1];
    return rows == 256 * 8 && colours == 32 ? 0 : -1;
}

/* the 2-bit pixel of tile `code` at its line `row`, pixel `col` along it */
static int tile_pixel(const GalaxianVideo *v, int code, int row, int col)
{
    int a = (code & 0xFF) * 8 + (row & 7), bit = 7 - (col & 7);
    return (v->gfx[1][a] >> bit & 1) | (v->gfx[0][a] >> bit & 1) << 1; /* 1h is the high bit */
}

/* the 2-bit pixel of 16x16 sprite `code`: four tiles, 4 code to 4 code + 3 */
static int sprite_pixel(const GalaxianVideo *v, int code, int row, int col)
{
    int t = code * 4 + (row >> 3 & 1) * 2 + (col >> 3 & 1);
    return tile_pixel(v, t, row, col);
}

void galaxian_draw_line(GalaxianVideo *v, int line)
{
    /* the tiles and sprites of the line as the flip has it (line 255 - L,
       the pixels along it the other way round); the first three sprites
       show a line late in that count, the first three shells a line late
       on the beam; no sprite is drawn before pixel SPRITE_FIRST in the count */
    uint32_t tiles[256 * 3], sprites[256 * 3], shells[256 * 3];
    memset(tiles, 0, sizeof tiles);
    memset(sprites, 0, sizeof sprites);
    memset(shells, 0, sizeof shells);
    int vl = v->flip_y ? 255 - line : line;
    for (int h = 0; h < 256; h++) {
        int col = h >> 3;
        int ls = (vl + v->objects[col * 2]) & 0xFF;
        int code = v->tiles[(ls >> 3) * 32 + col];
        int p = tile_pixel(v, code, ls, h);
        if (p) tiles[h * 3] = tiles[h * 3 + 1] = tiles[h * 3 + 2] = v->palette[(v->objects[col * 2 + 1] & 7) * 4 + p];
    }
    for (int i = 7; i >= 0; i--) {
        const uint8_t *s = v->objects + 0x40 + i * 4;
        int row = (vl - (0xF0 - s[0]) - (i < 3)) & 0xFF;
        if (row >= 16) continue;
        int code = s[1] & 0x3F;
        if (s[1] & 0x80) row = 15 - row;
        for (int k = 0; k < 16; k++) {
            int h = s[3] + 1 + k;
            if (h >= 256) break;
            if (h < SPRITE_FIRST) continue; /* no sprite before this pixel (in the flip's count) */
            int p = sprite_pixel(v, code, row, s[1] & 0x40 ? 15 - k : k);
            if (p) sprites[h * 3] = sprites[h * 3 + 1] = sprites[h * 3 + 2] = v->palette[(s[2] & 7) * 4 + p];
        }
    }
    /* shells, and the missile (the last): the flip moves their lines but
       not their pixels along a line (the game mirrors those itself) */
    for (int i = 0; i < 8; i++) {
        const uint8_t *s = v->objects + 0x60 + i * 4;
        int at = 255 - s[1];
        if (v->flip_y) at = 255 - at;
        if (((at + (i < 3)) & 0xFF) != line) continue;
        uint32_t c = i == 7 ? 0xFFFF00 : 0xFFFFFF;
        for (int k = 0; k < 4; k++) {
            int h = 251 - s[3] + k;
            if (h >= 0 && h < 256) shells[h * 3] = shells[h * 3 + 1] = shells[h * 3 + 2] = c;
        }
    }
    /* the stars, never flipped, where the rest is black */
    uint32_t px[256 * 3];
    memset(px, 0, sizeof px);
    if (v->stars_on) {
        long at = (v->star_origin + (long)line * STAR_STEPS_PER_LINE) % STAR_PERIOD;
        for (int s = 0; s < 512; s++) {
            uint8_t st = stars[at];
            if (++at == STAR_PERIOD) at = 0;
            if (!(st & 0x80) || !((line ^ (s >> 4)) & 1)) continue;
            uint32_t c = star_colour(st & 0x3F);
            if (s & 1) px[(s >> 1) * 3 + 1] = px[(s >> 1) * 3 + 2] = c;
            else px[(s >> 1) * 3] = c;
        }
    }
    for (int y = 0; y < 256 * 3; y++) {
        int from = v->flip_x ? 256 * 3 - 1 - y : y;
        uint32_t c = tiles[from];
        if (sprites[from]) c = sprites[from];
        if (shells[y]) c = shells[y];
        if (c) px[y] = c;
    }
    int x = GALAXIAN_LAST_LINE - line;
    for (int y = 0; y < GALAXIAN_NATIVE_H; y++) v->native[y][x] = px[y];
}

void galaxian_to_fb(const GalaxianVideo *v, Framebuffer *fb)
{
    for (int y = 0; y < FB_HEIGHT; y++) {
        uint32_t *row = fb_row(fb, y);
        const uint32_t *a = v->native[y * 3], *b = v->native[y * 3 + 1], *c = v->native[y * 3 + 2];
        for (int x = 0; x < FB_WIDTH; x++) row[x] = 0xFF000000u | (b[x] ? b[x] : a[x] ? a[x] : c[x]);
    }
}
