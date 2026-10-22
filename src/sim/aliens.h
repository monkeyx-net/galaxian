/*
 * The flying aliens: 8 slots of 32 bytes at $42B0 (+0 flying, +1 exploding,
 * +2 its state, +3/+4 x and y, +5 its heading, ...).
 */
#ifndef GALAXIAN_ALIENS_H
#define GALAXIAN_ALIENS_H

#include "sim/game.h"

/* $0CC3: each slot a step: its state ($0CE6's table: leaving the swarm,
   the arc out, aiming, the dive and its shots, round from the bottom,
   back into the swarm, the flagship's escorts' turns, the score table's
   parade) or, exploding, its explosion's ($10E8). */
void aliens_step(Game *g);
/* $1147: slot ix's place in the swarm: y from its row, x from its column
   and the sway. */
void aliens_home(Game *g, uint16_t ix);
/* $116B: the dive's curve: the slot's (+19,+1A) turned (+18 & 3) + 1 steps
   by its fractions (+1B,+1C). */
void aliens_curve(Game *g, uint16_t ix);
/* $11B0: the slot's heading (+5, -4..4) towards the player. */
void aliens_aim(Game *g, uint16_t ix);
/* $11E0: a free shell starts from the slot towards the player. */
void aliens_shoot(Game *g, uint16_t ix);

/* $0BBE: the slots as sprites in the object RAM's copy ($4060-$407F):
   position, picture by heading, colour; the first three a line lower
   (the hardware shows them a line late), mirrored in the cocktail's flip. */
void aliens_sprites(Game *g);

#endif
