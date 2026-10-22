/*
 * The player's ship: hit, exploding.
 */
#ifndef GALAXIAN_PLAYER_H
#define GALAXIAN_PLAYER_H

#include "sim/game.h"

/* $12ED: the ship was hit ($4204): it explodes ($4201), the aliens calm
   down a step ($421A, $421D), the hit sound in a game. */
void player_hit(Game *g);
/* $1327: the explosion's frames (the main loop draws them), then quiet. */
void player_explode(Game *g);

/* $0837: the ship's x ($4202, $17-$E9) by the stick (the cocktail's
   second player's on IN1, the demo's pilot's $423F); the ship is tiles in
   columns 26-29, placed by their scroll (colour 6, 7 exploding). */
void player_move(Game *g);

#endif
