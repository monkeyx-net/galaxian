/*
 * The ROM set loader against the user's galaxian set (label rom):
 *   test_rom <galaxian.zip or folder> <scratch dir>
 * Loads it as given, then from a folder of the same ROMs under other names
 * (matching is by CRC), then checks an empty folder fails with a message.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "assets/rom.h"

#ifdef _WIN32
#include <direct.h>
#define mkdir(dir, mode) _mkdir(dir)
#endif

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

static RomSet a, b;

static void write_rom(const char *dir, const char *name, const uint8_t *data, size_t len)
{
    char path[1200];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    CHECK(f && fwrite(data, 1, len, f) == len, "cannot write %s", path);
    if (f) fclose(f);
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: test_rom <galaxian.zip|folder> <scratch dir>\n");
        return 2;
    }
    char err[512];
    CHECK(!rom_load(&a, argv[1], err, sizeof err), "load %s: %s", argv[1], err);
    CHECK(rom_byte(&a, 0x0000) == a.program[0] && rom_byte(&a, 0x27FF) == a.program[0x27FF], "program ROM mapping");
    CHECK(rom_byte(&a, 0x2800) == -1 && rom_byte(&a, 0x4000) == -1, "RAM addresses read as ROM");

    /* the same ROMs under other names in a folder, with an unrelated file */
    mkdir(argv[2], 0755);
    char dir[1100];
    snprintf(dir, sizeof dir, "%s/renamed", argv[2]);
    mkdir(dir, 0755);
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof name, "prog%d.bin", (i * 3) % 5);
        write_rom(dir, name, a.program + i * 0x800, 0x800);
    }
    write_rom(dir, "plane_b.rom", a.gfx[1], sizeof a.gfx[1]);
    write_rom(dir, "plane_a.rom", a.gfx[0], sizeof a.gfx[0]);
    write_rom(dir, "colours.prom", a.prom, sizeof a.prom);
    uint8_t junk[0x800];
    memset(junk, 0x5A, sizeof junk);
    write_rom(dir, "unrelated.bin", junk, sizeof junk);
    CHECK(!rom_load(&b, dir, err, sizeof err), "load renamed folder: %s", err);
    CHECK(!memcmp(a.program, b.program, sizeof a.program) && !memcmp(a.gfx, b.gfx, sizeof a.gfx) &&
              !memcmp(a.prom, b.prom, sizeof a.prom),
          "renamed folder loaded different ROMs");

    snprintf(dir, sizeof dir, "%s/empty", argv[2]);
    mkdir(dir, 0755);
    err[0] = '\0';
    CHECK(rom_load(&b, dir, err, sizeof err) == -1 && strstr(err, "missing galmidw.u"),
          "empty folder: rc/err '%s'", err);
    CHECK(!b.loaded && rom_byte(&b, 0) == -1, "failed load left the set usable");

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    puts("rom: all checks passed");
    return 0;
}
