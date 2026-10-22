/*
 * Run-time asset folder. Nothing ROM-derived is compiled into the binary
 * (GALAXIAN_PORT_PLAN.md, decision 1): what the game draws and plays comes from
 * the user's ROM set, extracted to assets/ (and later, sounds rendered from
 * the sound ROM).
 *
 * Search order: $GALAXIAN_ASSETS, <exe dir>/assets, then the source tree
 * (development builds only).
 */
#ifndef GALAXIAN_ASSETS_H
#define GALAXIAN_ASSETS_H

#include <stddef.h>

/* First asset dir holding `rel`, e.g. "sounds/fire.wav". Returns 0 if found. */
int assets_find(const char *argv0, const char *rel, char *out, size_t outlen);

#endif
