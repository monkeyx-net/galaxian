/*
 * The C game's state: the board's memory as the ROM lays it out (RAM
 * $4000-$43FF, tile RAM $5000-$53FF, object RAM $5800-$58FF), the latches
 * it writes and the switches it reads. Routines are written against the
 * same addresses as the ROM's (game_rd / game_wr map them as the board
 * does, mirrors included), so their arithmetic and pointers are the ROM's
 * and the whole state compares byte for byte with the board's.
 *
 * `data` is the read-only data the ported routines use, at its ROM
 * addresses ($0000-$27FF); the game fills it from assets/, the tests from
 * the ROM set. Routines that take or return registers use Regs.
 */
#ifndef GALAXIAN_GAME_H
#define GALAXIAN_GAME_H

#include <stdint.h>

#define GAME_DATA_SIZE 0x2800

typedef struct {
    uint8_t a, f, b, c, d, e, h, l;
    uint16_t ix, iy;
} Regs;

#define REGS_BC(r) ((uint16_t)((r)->b << 8 | (r)->c))
#define REGS_DE(r) ((uint16_t)((r)->d << 8 | (r)->e))
#define REGS_HL(r) ((uint16_t)((r)->h << 8 | (r)->l))
#define REGS_SET_BC(r, v) ((r)->b = (uint8_t)((v) >> 8), (r)->c = (uint8_t)(v))
#define REGS_SET_DE(r, v) ((r)->d = (uint8_t)((v) >> 8), (r)->e = (uint8_t)(v))
#define REGS_SET_HL(r, v) ((r)->h = (uint8_t)((v) >> 8), (r)->l = (uint8_t)(v))

typedef struct Game {
    uint8_t ram[0x400];      /* $4000-$43FF (mirrored to $47FF) */
    uint8_t tiles[0x400];    /* $5000-$53FF (to $57FF) */
    uint8_t objects[0x100];  /* $5800-$58FF (to $5FFF) */
    uint8_t latch6000[8];    /* lamps, coin counter, the sound's LFO (bit 0 each) */
    uint8_t latch6800[8];    /* the sound */
    uint8_t latch7000[8];    /* NMI enable, stars, flip */
    uint8_t pitch;           /* $7800 */
    uint8_t in0, in1, in2;   /* the switches: IN0 $6000, IN1 $6800, the DIP switches $7000 */
    const uint8_t *data;     /* GAME_DATA_SIZE bytes indexed by ROM address */
    uint16_t unported;       /* the ROM address the C game last reached unported, or 0 */
    uint8_t *data_read;      /* optional, for tools: a flag per ROM byte the game reads */
    /* optional, for tests: called before each write */
    void (*on_write)(void *user, uint16_t addr, uint8_t value);
    void *user;
} Game;

/* A byte as the CPU reads it ($7800, the watchdog, reads $FF). */
uint8_t game_rd(const Game *g, uint16_t addr);
/* A byte as the CPU writes it (the latches keep bit 0). */
void game_wr(Game *g, uint16_t addr, uint8_t value);
/* Little-endian words, as the Z80 keeps them. */
uint16_t game_rd16(const Game *g, uint16_t addr);
void game_wr16(Game *g, uint16_t addr, uint16_t value);
/* `n` bytes of `value` from `addr` up (RST $10). */
void game_fill(Game *g, uint16_t addr, uint8_t value, int n);
/* LDIR: n bytes from src to dst, one at a time (so overlaps behave as the ROM's). */
void game_copy(Game *g, uint16_t dst, uint16_t src, int n);

/* Shorthands for the routines (g is the Game in scope). */
#define RD(a) game_rd(g, (uint16_t)(a))
#define WR(a, v) game_wr(g, (uint16_t)(a), (uint8_t)(v))

/* The C game reached ROM code not yet ported: remember where (the caller
   must leave the state unspecified). */
void game_unported(Game *g, uint16_t addr);

#endif
