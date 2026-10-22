/*
 * A frame of the C game: the NMI's work ($0066) and then the main loop's
 * (sim/screen.h), as the board does them between two vertical blanks.
 */
#ifndef GALAXIAN_FRAME_H
#define GALAXIAN_FRAME_H

#include "sim/game.h"

/* $0066: the NMI: the object RAM's copy out, the switches' history, the
   frame's counters and sounds, the state's frame (or a boot mode's). */
void frame_nmi(Game *g);
/* A frame: the NMI, then the main loop's commands and animation. Returns
   0, or -1 if it reached code not yet ported (g->unported). */
int frame_run(Game *g);

#endif
