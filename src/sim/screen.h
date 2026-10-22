/*
 * The main loop ($2000): between the NMIs it carries out the commands the
 * game queues ($08F2: a command and its argument in $40C0-$40FF, $40A1 the
 * next to take), and when there are none animates the swarm ($2067).
 *
 *   0 an alien drawn back in the swarm      4 a score cleared (3: all)
 *   1 an alien gone from the swarm          5 a score drawn (3: all)
 *   2 the ship's explosion, frame n         6 a line of text ($235C's table;
 *   3 a score added (and the high score)       +$80 erase, +$40 type it)
 *                                           7 the flags (0), credits (1),
 *                                             bonus (2), ships left (3)
 */
#ifndef GALAXIAN_SCREEN_H
#define GALAXIAN_SCREEN_H

#include "sim/game.h"

/* $2000: what the main loop does at power-on before its loop. */
void screen_start(Game *g);
/* $200A: take and carry out every queued command, then animate once (the
   main loop's work between two NMIs). */
void screen_run(Game *g);
/* One command. */
void screen_command(Game *g, uint8_t cmd, uint8_t arg);
/* $2067: the swarm's wing beats, a column a frame, and the blinking 1UP. */
void screen_idle(Game *g);
/* $22F1: a line of text. */
void screen_text(Game *g, uint8_t n);
/* $2231: a score drawn (0, 1 the players', 2 the high score, 3 all). */
void screen_score(Game *g, uint8_t n);
/* $22B3: the ships left, as icons from $539E (b ships, one fewer while a
   ship is in play). */
void screen_ships(Game *g, uint8_t b);

#endif
