/*
 * The player's missile and the aliens' shells: firing, moving, what they hit.
 */
#ifndef GALAXIAN_SHOTS_H
#define GALAXIAN_SHOTS_H

#include "sim/game.h"

/* $0A32: fire the player's missile ($4208) on a new press of fire (the
   cocktail's second player on IN1), or every 32 frames in the demo. */
void shots_fire(Game *g);
/* $0A74: move the 14 shells ($4260, 5 bytes each: on, y, x 16-bit, dx)
   down two lines a frame and write half of them, alternate frames, to the
   7 hardware shells ($4081-$409B in the object RAM's copy). */
void shots_move_shells(Game *g);
/* $0B0B: the missile against the swarm: the alien it reaches is killed
   (its grid byte cleared, the main loop told), its explosion started. */
void shots_hit_swarm(Game *g);
/* $0B77: the shells against the player's ship: one that reaches it is
   removed and the ship hit ($4204). */
void shots_hit_ship(Game *g);

/* $1227: the missile against the flying aliens (slots 1-7): one it hits
   explodes ($125E) and scores. */
void shots_hit_flying(Game *g);
/* $129E: the flying aliens against the ship: both go ($4204). */
void shots_crash(Game *g);
/* $125E: slot ix explodes and its score is queued (a flagship's by how
   many escorts were left; the flagship's wait $422B starts). */
void shots_kill_flying(Game *g, uint16_t ix);

/* $0898: the missile ($08BC: up 4 lines a frame; done at line $10, else
   waiting on the ship) as the 8th hardware shell ($409D, $409F). */
void shots_missile(Game *g);
/* $08E5: a missile done ($420B) is gone. */
void shots_missile_done(Game *g);

#endif
