#include "sim/shots.h"

#include "sim/util.h"

#define SHELLS 0x4260 /* 14 of 5 bytes: on, y, x (16-bit), dx */

void shots_fire(Game *g)
{
    if (!(RD(0x4200) & 1) || (RD(0x4208) & 1)) return; /* the ship gone, a missile in flight */
    if (!(RD(0x4006) & 1)) { /* the demo fires every 32 frames */
        if (!(RD(0x425F) & 0x1F)) WR(0x4208, 1);
        return;
    }
    uint8_t now, was;
    if (RD(0x4018) & 1) now = RD(0x4011), was = RD(0x4014); /* the cocktail's second player */
    else now = RD(0x4010), was = RD(0x4013);
    if (!(now & (uint8_t)~was & 0x10)) return;
    WR(0x4208, 1);
    WR(0x41CC, 1);
}

void shots_move_shells(Game *g)
{
    uint16_t ix = SHELLS, iy = 0x4081;
    if (!(RD(0x425F) & 1)) { /* even frames: the odd shells, the first one moved on first */
        WR(ix + 1, RD(ix + 1) + 2);
        ix += 5;
    }
    for (int b = 7; b > 0; b--) {
        int keep = 0;
        if (RD(ix) & 1) {
            uint8_t y = (uint8_t)(RD(ix + 1) + 2);
            WR(ix + 1, y);
            if (y + 4 <= 0xFF) {
                uint8_t dx = RD(ix + 4);
                uint16_t d = (uint16_t)(((dx & 0x80) ? 0xFF00 : 0) | (uint8_t)(dx << 1));
                uint16_t x = (uint16_t)(game_rd16(g, ix + 2) + d);
                game_wr16(g, ix + 2, x);
                keep = (uint8_t)((x >> 8) + 0x10) >= 0x20;
            }
        }
        if (!keep) { /* gone, or off the screen's edge */
            WR(ix, 0);
            WR(ix + 1, 0);
            WR(ix + 3, 0);
        }
        uint8_t c = (uint8_t)~RD(ix + 3);
        if (RD(0x4018) & 1) { /* flipped */
            WR(iy + 2, RD(ix + 1) - 4);
            if (b >= 5) c--;
        } else {
            WR(iy + 2, (uint8_t)~RD(ix + 1) - 1);
            if (b >= 5) c++; /* the hardware shows the first three shells a line late */
        }
        WR(iy, c);
        ix += 5;
        WR(ix + 1, RD(ix + 1) + 2); /* the one not shown this frame moves on too */
        ix += 5;
        iy += 4;
    }
}

void shots_hit_swarm(Game *g)
{
    if (!(RD(0x4208) & 1)) return;
    uint8_t a = RD(0x4209);
    if (a >= 0x68 || a < 0x1E) return;
    a = (uint8_t)(a - 0x1E);
    int b = 6;
    for (;;) { /* the row: 7 lines of gap, 5 of alien */
        if (a < 7) return;
        a = (uint8_t)(a - 7);
        if (a < 5) break;
        a = (uint8_t)(a - 5);
        if (--b == 0) return;
    }
    uint8_t c = (uint8_t)(RD(0x420A) - RD(0x420E));
    if ((uint8_t)((c & 0x0F) - 2) >= 0x0B) return;
    b++;
    uint8_t cell = (uint8_t)((b << 4) | (c >> 4)); /* row b, column (x - sway) / 16 */
    uint16_t hl = (uint16_t)(0x4100 + cell);
    if (!(RD(hl) & 1)) return;
    WR(hl, 0);
    util_queue(g, 1, cell);
    WR(0x420B, 1);
    WR(0x42B1, 1); /* an explosion in the first flying slot */
    WR(0x42B2, 0);
    WR(0x42B3, RD(0x4209));
    WR(0x42B4, RD(0x420A));
    util_queue(g, 3, cell < 0x50 ? 0 : (uint8_t)(((cell & 0x70) >> 4) - 4));
}

/* $0B8D: one shell (ix) against the ship. */
static void hit_ship(Game *g, uint16_t ix)
{
    if (!(RD(ix) & 1)) return;
    uint8_t a = (uint8_t)(RD(ix + 1) + 0x1F);
    if (a >= 5) {
        if ((uint8_t)(a - 5) >= 9) return;
        if ((uint8_t)(RD(0x4202) - RD(ix + 3) + 5) >= 0x0B) return;
    } else if ((uint8_t)(RD(0x4202) - RD(ix + 3) + 2) >= 5) {
        return;
    }
    WR(ix, 0);
    WR(0x4204, 1);
}

void shots_hit_ship(Game *g)
{
    if (!(RD(0x4200) & 1)) return;
    for (int i = 0; i < 14; i++) hit_ship(g, (uint16_t)(SHELLS + i * 5));
}

void shots_kill_flying(Game *g, uint16_t ix)
{
    WR(ix, 0);
    WR(ix + 1, 1);
    WR(ix + 2, 0);
    uint8_t e = 4, a = RD(ix + 7);
    for (int b = 3; b > 0; b--) { /* the score by row: 4 the blue rows, 5 purple, 6 red */
        if (a < 0x50) {
            util_queue(g, 3, e);
            return;
        }
        e++;
        a = (uint8_t)(a - 0x10);
    }
    game_wr16(g, 0x422B, 0xF001);
    a = RD(0x422A);
    if (a == 2 && !(RD(ix + 0x20) & 1) && !(RD(ix + 0x40) & 1)) a = 3; /* both escorts shot first */
    WR(0x422D, a);
    util_queue(g, 3, (uint8_t)(a + e));
}

void shots_hit_flying(Game *g)
{
    if (!(RD(0x4208) & 1)) return;
    for (uint16_t ix = 0x42D0; ix < 0x42D0 + 7 * 0x20; ix += 0x20) {
        if (!(RD(ix) & 1)) continue;
        if ((uint8_t)(RD(ix + 3) - RD(0x4209) + 2) >= 6) continue;
        if ((uint8_t)(RD(ix + 4) - RD(0x420A) + 5) >= 0x0C) continue;
        WR(0x420B, 1);
        shots_kill_flying(g, ix);
    }
}

void shots_crash(Game *g)
{
    if (!(RD(0x4200) & 1)) return;
    for (uint16_t ix = 0x42D0; ix < 0x42D0 + 7 * 0x20; ix += 0x20) {
        if (!(RD(ix) & 1)) continue;
        uint8_t a = (uint8_t)(RD(ix + 3) + 0x21);
        if (a >= 5) {
            if ((uint8_t)(a - 5) >= 0x0C) continue;
            if ((uint8_t)(RD(0x4202) - RD(ix + 4) + 0x0A) >= 0x15) continue;
        } else if ((uint8_t)(RD(0x4202) - RD(ix + 4) + 7) >= 0x0F) {
            continue;
        }
        WR(0x4204, 1);
        shots_kill_flying(g, ix);
    }
}

void shots_missile(Game *g)
{
    if (RD(0x4208) & 1) {
        uint8_t y = (uint8_t)(RD(0x4209) - 4);
        WR(0x4209, y);
        if ((uint8_t)(y - 0x0E) < 4) WR(0x420B, 1);
    } else {
        WR(0x4209, 0xDC);
        WR(0x420A, RD(0x4200) & 1 ? RD(0x4202) : 0);
    }
    uint8_t y = RD(0x4209), x = RD(0x420A);
    if (RD(0x4018) & 1) WR(0x409F, y - 1);
    else WR(0x409F, (uint8_t)~y + 0xFC);
    WR(0x409D, (uint8_t)~x);
}

void shots_missile_done(Game *g)
{
    if (!(RD(0x420B) & 1)) return;
    WR(0x420B, 0);
    WR(0x4208, 0);
}
