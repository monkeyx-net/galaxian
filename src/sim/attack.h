/*
 * The swarm's attacks: when and who leaves it to dive (lone aliens,
 * flagships with their escorts), the aggression rising through a wave,
 * the wave's end and the next.
 */
#ifndef GALAXIAN_ATTACK_H
#define GALAXIAN_ATTACK_H

#include "sim/game.h"

/* $1344: a lone alien leaves the swarm ($4228 set), from the side $4215,
   into a free slot among the last few. */
void attack_lone(Game *g);
/* $13E1: the side the next attack comes from ($4215), by the sway. */
void attack_side(Game *g);
/* $140C: a flagship leaves ($4229 set) with up to two escorts from the row below. */
void attack_flagship(Game *g);
/* $14F3: the aggression ($421A) rises every 20 seconds of play, to 7. */
void attack_rise(Game *g);
/* $1515: the lone attacks' timers ($424A-, reloaded from $15E3). */
void attack_lone_timers(Game *g);
/* $1555: the flagship's timer ($4245, $4246): when it comes. */
void attack_flagship_timer(Game *g);
/* $15C3: the flagship's countdown ($422F) to $4229. */
void attack_flagship_go(Game *g);
/* $15F4: where the diving aliens may shoot ($4213 how many places, $4214
   the first), by the rows left. */
void attack_shot_places(Game *g);
/* $1621: the swarm and the slots empty: the wave ends ($4222). */
void attack_wave_over(Game *g);
/* $1637: after a pause, the next wave: the swarm again, the flagships that
   got away back in it. */
void attack_next_wave(Game *g);
/* $1688: the wait after a flagship is shot ($422B, $422C). */
void attack_flagship_wait(Game *g);

#endif
