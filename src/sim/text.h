/*
 * Text: the typewriter that types a line a letter at a time.
 */
#ifndef GALAXIAN_TEXT_H
#define GALAXIAN_TEXT_H

#include "sim/game.h"

/* $18C0: while typing ($40B0), every 8th count of the line's column's
   scroll byte ($40B1 points at it) the next letter of $40B3's text goes
   to the screen at $40B5 ($3F ends it); typing ends when the count runs out. */
void text_type(Game *g);

#endif
