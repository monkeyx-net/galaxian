/*
 * Extractor: reads the user's ROM set once and writes what the game draws
 * under <out>/ (GALAXIAN_PORT_PLAN.md, decision 1). After that the game in C
 * needs only <out>/, never the ROM. Nothing here is compiled in: the data
 * comes from the player's set. The sound is generated (emu/sound.c), so
 * nothing is extracted for it.
 *
 *   gfx/tiles.txt     the 256 tiles of graphics ROMs 1h and 1k, as the board
 *                     reads them: "tile NN" then 8 rows, a board line each,
 *                     of 8 pixels along it (0-3; 1h the high bit); a sprite
 *                     is the four tiles 4n to 4n + 3 (gfx/galaxian.h)
 *   gfx/palette.txt   the 32 colours of PROM 6l (RRGGBB, through the board's
 *                     resistor weights as MAME has them)
 *   gfx/font.txt      the game's characters: "c NN" per character, NN its
 *                     tile (the game's own character codes)
 *   gfx/<name>.png    previews, as the player sees them (not read by the
 *                     game): tiles, sprites, the attract mode's screens
 *   tables.txt        the program ROM's tables the game in C reads (sim/):
 *                     "# what" then "ADDR: bytes" lines, 16 bytes each, for
 *                     each range listed in extract.c (addresses only there)
 *   manifest.txt      written last: the folder is complete
 */
#ifndef GALAXIAN_EXTRACT_H
#define GALAXIAN_EXTRACT_H

#include "assets/rom.h"

#define EXTRACT_MANIFEST "manifest.txt"
#define EXTRACT_VERSION 2

/* Write everything to out_dir (created if needed). Returns 0, or -1 with a
   message in err; without manifest.txt the folder counts as missing. */
int extract_all(const RomSet *rom, const char *out_dir, char *err, int errlen);

/* 1 if the program ROM's byte at addr is in a table the extractor writes. */
int extract_is_table(unsigned addr);
/* Read tables.txt under dir into data (indexed by ROM address, the rest
   0). Returns 0, or -1 if missing or incomplete. */
int extract_load_tables(const char *dir, uint8_t *data, unsigned size);

/* The game's characters in tile order from code 0: the hex digits, a space,
   the alphabet and a dash ($00-$2B). */
#define EXTRACT_FONT_CHARS "0123456789ABCDEF ABCDEFGHIJKLMNOPQRSTUVWXYZ-"

#endif
