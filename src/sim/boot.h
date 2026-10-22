/*
 * Power-on: the state the ROM's set-up leaves (after its RAM, tile RAM and
 * ROM checks, frames 1-87 on the board), and the NMI's boot modes ($401A:
 * 3 the object RAM's check, 1 the settings shown while the service switch
 * is on, 2 the crosshatch) before the game's states.
 */
#ifndef GALAXIAN_BOOT_H
#define GALAXIAN_BOOT_H

#include "sim/game.h"

/* The board as the set-up leaves it when the first NMI comes ($1BCA, then
   the main loop's start $2000; the checks' last pattern left in $43A0-$43FF). */
void boot_power_on(Game *g);
/* $1BCD: the NMI during the boot modes. */
void boot_nmi(Game *g);

#endif
