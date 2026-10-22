/*
 * Coins and credits.
 */
#ifndef GALAXIAN_COINS_H
#define GALAXIAN_COINS_H

#include "sim/game.h"

/* $18EF: a coin seen (pressed two frames, then released two) counts at
   $4004 (both chutes: two); the service switch's adds a credit; free play
   holds 9 credits. */
void coins_in(Game *g);
/* $1931: a counted coin pulses the coin counter ($6003) and becomes
   credits ($4002, up to 99) by the coinage ($4000); the coin sound and
   the credits redrawn. */
void coins_credit(Game *g);
/* $197C: the coin lockout ($6002) off from 9 credits. */
void coins_lockout(Game *g);

#endif
