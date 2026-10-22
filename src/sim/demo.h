/*
 * The attract mode's demo: its pilot.
 */
#ifndef GALAXIAN_DEMO_H
#define GALAXIAN_DEMO_H

#include "sim/game.h"

/* $198E: every 32 frames of the demo, the pilot weighs the flying aliens
   and the first 7 shells near the ship ($1A45's table, $1A12) and the
   swarm's middle, with a random nudge, into its stick ($423F: 0 still,
   4 or 8 the two ways). */
void demo_pilot(Game *g);

#endif
