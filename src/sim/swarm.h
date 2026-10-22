/*
 * The swarm in formation: its sway and the survey of who is left.
 */
#ifndef GALAXIAN_SWARM_H
#define GALAXIAN_SWARM_H

#include "sim/game.h"

/* $0972: the swarm columns' scroll ($4028-$4038, every other byte) = a;
   $0363 is this with 0. */
void swarm_scroll(Game *g, uint8_t a);
/* $090D: the sway, a step every 4 frames between the limits, holding while
   the player's missile passes an occupied column. */
void swarm_sway(Game *g);
/* $098E: which rows and columns of the swarm are occupied, the sway's
   limits from them, and whether any alien is left (formation, flying). */
void swarm_survey(Game *g);

#endif
