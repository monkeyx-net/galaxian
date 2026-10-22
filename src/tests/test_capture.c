/*
 * The port's picture drawing against a MAME snapshot (label mame):
 *   test_capture <galaxian.zip|folder> <frame_N.txt> <N.png> <N>
 * Draws every line of frame N from the tile and object RAM and the latches
 * capture_probe.lua dumped at its end (gfx/galaxian.c), and compares the
 * picture with MAME's (224x768) pixel for pixel, bar the stars (where the
 * starfield's generator stands depends on when the game last turned the
 * stars on: test_emu checks them). Only for frames where nothing on screen
 * changes during the scan-out.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/png.h"
#include "assets/rom.h"
#include "gfx/galaxian.h"

static RomSet rom;
static GalaxianVideo video;

static int hex(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

static int hex_bytes(const char *p, uint8_t *out, int n)
{
    for (int i = 0; i < n; i++) {
        int hi = hex(p[2 * i]), lo = hex(p[2 * i + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (uint8_t)(hi << 4 | lo);
    }
    return 0;
}

/* a latch from the dump ("--": never written, 0) */
static int latch(const char *l, int i) { return hex(l[2 * i]) < 0 ? 0 : (hex(l[2 * i]) << 4 | hex(l[2 * i + 1])) & 1; }

/* A star's colour: every level 0, $C2, $D6 or $FF, which no colour of the
   palette has but black. */
static int is_star(const uint8_t *p)
{
    int lit = 0;
    for (int i = 0; i < 3; i++) {
        if (p[i] && p[i] != 0xC2 && p[i] != 0xD6 && p[i] != 0xFF) return 0;
        lit |= p[i];
    }
    return lit != 0;
}

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "usage: test_capture <rom> <frame_N.txt> <N.png> <N>\n");
        return 2;
    }
    char err[512];
    if (rom_load(&rom, argv[1], err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    galaxian_video_init(&video, &rom);
    FILE *f = fopen(argv[2], "r");
    if (!f) {
        fprintf(stderr, "cannot read %s\n", argv[2]);
        return 1;
    }
    static char line[4096];
    int have = 0;
    while (fgets(line, sizeof line, f)) {
        if (!strncmp(line, "tiles ", 6)) have |= !hex_bytes(line + 6, video.tiles, 0x400);
        else if (!strncmp(line, "objects ", 8)) have |= !hex_bytes(line + 8, video.objects, 0x100) << 1;
        else if (!strncmp(line, "latch ", 6) && strlen(line) >= 6 + 50) {
            const char *l = line + 6; /* $6000-$6007, $6800-$6807, $7000-$7007, $7800 */
            video.flip_x = (uint8_t)latch(l, 16 + 6);
            video.flip_y = (uint8_t)latch(l, 16 + 7);
            have |= 4;
        }
    }
    fclose(f);
    if (have != 7) {
        fprintf(stderr, "%s: not a capture frame\n", argv[2]);
        return 1;
    }
    for (int l = GALAXIAN_FIRST_LINE; l <= GALAXIAN_LAST_LINE; l++) galaxian_draw_line(&video, l);

    Image shot;
    const char *perr;
    if (png_load(argv[3], &shot, &perr)) {
        fprintf(stderr, "%s: %s\n", argv[3], perr);
        return 1;
    }
    if (shot.width != GALAXIAN_NATIVE_W || shot.height != GALAXIAN_NATIVE_H) {
        fprintf(stderr, "%s: %dx%d, not MAME's 224x768\n", argv[3], shot.width, shot.height);
        return 1;
    }
    long diff = 0;
    for (int y = 0; y < GALAXIAN_NATIVE_H; y++)
        for (int x = 0; x < GALAXIAN_NATIVE_W; x++) {
            const uint8_t *p = shot.rgba + ((size_t)y * (size_t)shot.width + (size_t)x) * 4;
            uint32_t want = (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2], got = video.native[y][x] & 0xFFFFFF;
            if (!got && is_star(p)) continue;
            if (got != want && diff++ < 6)
                fprintf(stderr, "  pixel %d,%d: %06X, MAME %06X\n", x, y, (unsigned)got, (unsigned)want);
        }
    image_free(&shot);
    printf("frame %s: %ld pixels differ from MAME's snapshot\n", argv[4], diff);
    return diff ? 1 : 0;
}
