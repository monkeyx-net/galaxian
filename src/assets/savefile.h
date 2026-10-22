/*
 * The player's saved files (galaxian.hiscore: the high score, which the
 * arcade forgets at power-off; galaxian.cfg: the DIP switches). Directory: $GALAXIAN_SAVE_DIR, else
 * $XDG_DATA_HOME/galaxian, else (Windows) %APPDATA%/galaxian, else
 * ~/.local/share/galaxian. Files are written to a temp file and renamed, so a
 * power-off mid-write keeps the old one.
 */
#ifndef GALAXIAN_SAVEFILE_H
#define GALAXIAN_SAVEFILE_H

#include <stddef.h>

/* The full path of a saved file (static buffer). */
const char *save_path(const char *name);
/* Read exactly len bytes; returns 0, or -1 if missing or the wrong size. */
int save_load(const char *name, void *buf, size_t len);
/* Returns 0 on success. */
int save_store(const char *name, const void *buf, size_t len);

#endif
