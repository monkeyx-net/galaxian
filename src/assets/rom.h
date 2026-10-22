/*
 * The user's Galaxian ROM set (MAME `galaxian`, Namco set 1, Midway
 * program ROMs), read from a folder or a zip. Files are matched by CRC-32,
 * so any naming works (MAME's current names, older sets, FBNeo).
 *
 * The Z80 sees the five 2 KB program ROMs at $0000-$27FF; $2800-$3FFF is
 * empty (reads $FF on the board's open bus; see emu/board.c). The two
 * graphics ROMs (1h, 1k) are the two bitplanes of 256 8x8 tiles (and the
 * same data read as 64 16x16 sprites); the 32-byte PROM (6l) is the palette.
 */
#ifndef GALAXIAN_ROM_H
#define GALAXIAN_ROM_H

#include <stddef.h>
#include <stdint.h>

#define ROM_PROGRAM_SIZE 0x2800 /* $0000-$27FF */
#define ROM_GFX_SIZE 0x800      /* each bitplane */
#define ROM_PROM_SIZE 0x20

typedef struct {
    uint8_t program[ROM_PROGRAM_SIZE];
    uint8_t gfx[2][ROM_GFX_SIZE]; /* 1h, 1k */
    uint8_t prom[ROM_PROM_SIZE];  /* 6l */
    int loaded;
} RomSet;

/* Load the set from a folder or a zip. Returns 0, or -1 with a message in err. */
int rom_load(RomSet *rom, const char *src, char *err, int errlen);
/* Byte at a Z80 address in the program ROM ($0000-$27FF); -1 elsewhere. */
int rom_byte(const RomSet *rom, unsigned addr);

/* Where the ROM set is: $GALAXIAN_ROM, else galaxian.zip or a galaxian/ folder next to
   the program or in the current folder (development builds: also in the
   source tree's roms/). Returns 0 if found. */
int rom_locate(const char *argv0, char *out, size_t outlen);

#endif
