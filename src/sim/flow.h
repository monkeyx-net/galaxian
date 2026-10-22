/*
 * The game's flow: the NMI's states ($4005) and their steps ($400A).
 *
 *   0  the screen cleared a row a frame, the settings read ($00E6)
 *   1  the attract mode ($0156): GAME OVER, the swarm, the score table's
 *      text typed and its parade, the demo; a credit leads to 2
 *   2  PUSH START BUTTON ($03F2); a start leads to 3
 *   3  player one's turn ($0536), 4 player two's ($077B): the screen
 *      cleared, the player's swarm, PLAYER ONE / TWO, the ship, play
 *      ($0661: every routine of a frame), a ship lost, the turn's end
 */
#ifndef GALAXIAN_FLOW_H
#define GALAXIAN_FLOW_H

#include "sim/game.h"

/* $0661: a frame of play (the demo's too): every routine in its order,
   then the step ends once the ship, its missile, the flying aliens and
   the shells are all gone and $4009 has counted down. */
void flow_play(Game *g);
/* The state $4005's frame ($00C6's RST $28). */
void flow_state(Game *g);
/* $1CB5: the sound latches back to rest, the NMI and stars off. */
void flow_quiet(Game *g);

#endif
