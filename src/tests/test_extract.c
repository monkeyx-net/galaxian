/*
 * The extractor against the user's galaxian set (label rom):
 *   test_extract <galaxian.zip or folder> <scratch dir>
 * Two extractions are byte-identical and hold every file; the graphics and
 * palette read back from them (gfx/galaxian.c) are the ROM set's; the font
 * matches what the attract mode draws on the emulated board ("WE ARE THE
 * GALAXIANS": each letter's tile code and its pixels on screen); and with
 * no ROM set nothing is written, not even a manifest.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "assets/extract.h"
#include "assets/rom.h"
#include "emu/board.h"
#include "gfx/galaxian.h"

static int failures;
#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        if (!(cond)) {                                                      \
            failures++;                                                     \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);            \
            fprintf(stderr, __VA_ARGS__);                                   \
            fputc('\n', stderr);                                            \
        }                                                                   \
    } while (0)

static const char *const files[] = {
    "manifest.txt", "gfx/tiles.txt", "gfx/palette.txt", "gfx/font.txt",
    "gfx/tiles.png", "gfx/sprites.png", "gfx/game_over.png", "gfx/score_table.png", "gfx/demo.png",
};
#define FILES ((int)(sizeof files / sizeof files[0]))

static RomSet rom, none;
static GalaxianVideo from_rom, from_assets;
static Board board;

static long read_all(const char *path, char **out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    *out = malloc((size_t)n + 1);
    if (fread(*out, 1, (size_t)n, f) != (size_t)n) n = -1;
    fclose(f);
    return n;
}

/* The tile code of a character in the font file. */
static int font_code(const char *dir, char ch)
{
    char path[1200], line[256];
    snprintf(path, sizeof path, "%s/gfx/font.txt", dir);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int code = -1;
    while (fgets(line, sizeof line, f)) {
        unsigned n;
        char c[16];
        if (line[0] == '#' || sscanf(line, "%x %15s", &n, c) != 2) continue;
        if ((ch == ' ' && !strcmp(c, "space")) || (c[0] == ch && !c[1])) code = (int)n; /* the last: letters over hex digits */
    }
    fclose(f);
    return code;
}

/* The attract mode's "WE ARE THE GALAXIANS", found in tile RAM down a column,
   each letter as the font says, drawn on screen as its glyph. */
static void check_font(const char *dir)
{
    static const char text[] = "GALAXIANS";
    int codes[sizeof text];
    for (int k = 0; text[k]; k++) {
        codes[k] = font_code(dir, text[k]);
        CHECK(codes[k] >= 0, "font.txt has no '%c'", text[k]);
    }
    board_init(&board, &rom);
    int col = -1, row = -1;
    while (board.frame < 3000 && col < 0) {
        board_run_frame(&board);
        for (int c = 0; c < 32 && col < 0; c++)
            for (int r = 8; r < 32 && col < 0; r++) {
                int k = 0;
                while (text[k] && board.video.tiles[(r - k) * 32 + c] == codes[k]) k++;
                if (!text[k]) col = c, row = r;
            }
    }
    CHECK(col >= 0, "the attract mode never showed \"%s\"", text);
    if (col < 0) return;
    board_run_frame(&board); /* the next frame shows it whole */
    CHECK(board.video.objects[col * 2] == 0, "the text's column scrolls");
    int colour = board.video.objects[col * 2 + 1] & 7, bad = 0, lit = 0;
    for (int k = 0; text[k]; k++)
        for (int y = 0; y < 8; y++)    /* the tile's lines */
            for (int x = 0; x < 8; x++) { /* pixels along them */
                int line = (row - k) * 8 + y, a = codes[k] * 8 + y;
                int p = (from_assets.gfx[0][a] >> (7 - x) & 1) << 1 | (from_assets.gfx[1][a] >> (7 - x) & 1);
                if (!p) continue;
                lit++;
                uint32_t want = from_assets.palette[colour * 4 + p];
                for (int third = 0; third < 3; third++)
                    if ((board.video.native[(col * 8 + x) * 3 + third][GALAXIAN_LAST_LINE - line] & 0xFFFFFF) != want) bad++;
            }
    CHECK(lit > 50 && !bad, "\"%s\" on screen: %d of %d pixels differ from the font", text, bad, lit * 3);
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: test_extract <galaxian.zip|folder> <scratch dir>\n");
        return 2;
    }
    char err[512], a[1100], b[1100], c[1100];
    if (rom_load(&rom, argv[1], err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    snprintf(a, sizeof a, "%s/a", argv[2]);
    snprintf(b, sizeof b, "%s/b", argv[2]);
    snprintf(c, sizeof c, "%s/none", argv[2]);
    CHECK(!extract_all(&rom, a, err, sizeof err), "extract: %s", err);
    CHECK(!extract_all(&rom, b, err, sizeof err), "extract again: %s", err);
    for (int i = 0; i < FILES; i++) {
        char pa[1200], pb[1200], *da = NULL, *db = NULL;
        snprintf(pa, sizeof pa, "%s/%s", a, files[i]);
        snprintf(pb, sizeof pb, "%s/%s", b, files[i]);
        long na = read_all(pa, &da), nb = read_all(pb, &db);
        CHECK(na > 0, "%s missing", pa);
        CHECK(na == nb && na > 0 && !memcmp(da, db, (size_t)na), "%s differs between two extractions", files[i]);
        free(da), free(db);
    }

    galaxian_video_init(&from_rom, &rom);
    CHECK(!galaxian_video_load(&from_assets, a), "cannot read the assets back");
    CHECK(!memcmp(from_assets.planes, rom.gfx, sizeof rom.gfx), "the tiles read back are not the ROM's");
    CHECK(!memcmp(from_assets.palette, from_rom.palette, sizeof from_rom.palette), "the palette read back is not the PROM's");
    check_font(a);

    err[0] = '\0';
    CHECK(extract_all(&none, c, err, sizeof err) == -1 && err[0], "extract without a ROM set did not fail");
    char path[1200];
    struct stat st;
    snprintf(path, sizeof path, "%s/%s", c, EXTRACT_MANIFEST);
    CHECK(stat(path, &st) != 0, "a manifest without a ROM set");

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    puts("extract: all checks passed");
    return 0;
}
