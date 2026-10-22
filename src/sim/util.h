/*
 * The ROM's small helpers, in C (each mirrors its routine's arithmetic and
 * order of stores).
 */
#ifndef GALAXIAN_UTIL_H
#define GALAXIAN_UTIL_H

#include "sim/game.h"

/* $003C: the random number at $401E steps (x 5 + 1) and is returned. */
uint8_t util_random(Game *g);
/* $0048: D / E as the ROM divides (8 steps of shift and subtract): the
   quotient in C, D left as the routine leaves it; A as it leaves it. */
void util_divide(Regs *r);
/* $08F2: queue the main loop's command `cmd` with `arg` (DE) at $40A0's
   slot, if that slot is free (bit 7). */
void util_queue(Game *g, uint8_t cmd, uint8_t arg);
/* $0646: unpack 16 bytes of bits from `src` into one byte each (0 or 1)
   at $4100-$417F; returns the address after the last byte read. */
uint16_t util_unpack_grid(Game *g, uint16_t src);
/* $0764: pack $4100-$417F (bit 0 of each byte) into 16 bytes at `dst`;
   returns the address after them. */
uint16_t util_pack_grid(Game *g, uint16_t dst);
/* $0598: the 32 bytes at `src` into every other byte from $4021 (the
   columns' colours in the object RAM's copy); $0595 does it from $1D71. */
void util_colours(Game *g, uint16_t src);

#endif
