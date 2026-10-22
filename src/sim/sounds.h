/*
 * The sound requests the game makes each frame.
 */
#ifndef GALAXIAN_SOUNDS_H
#define GALAXIAN_SOUNDS_H

#include "sim/game.h"

/* $16A6: $41DF counts down, its bit 2 to $6804. */
void sounds_6804(Game *g);
/* $16B8: the swarm's sound: FS1-3 on as at least 1, 2, 3 aliens are left
   in the swarm; $4224 set with fewer than 4. */
void sounds_swarm(Game *g);

/* $16F5: the frame's tone: silence, then each source in turn (a tune
   starting, the dive's whine, the explosions' tunes, the tunes playing,
   the extra life, the coin, the fire), the last to sound winning; the
   volume to $6806/$6807, the pitch to $7800, the fire to $6805. */
void sounds_frame(Game *g);
/* $1898: the swarm sound's LFO ($421F) steps down every 256 frames
   ($41D0: back to 15) to $6004-$6007. */
void sounds_lfo(Game *g);

#endif
