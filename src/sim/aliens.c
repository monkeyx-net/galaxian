#include "sim/aliens.h"

#include "sim/util.h"

#define SLOTS 0x42B0
#define SLOT_SIZE 0x20
#define SPRITES 0x4060

/* a slot's bytes */
enum {
    ON = 0x00, BOOM = 0x01, STATE = 0x02, Y = 0x03, X = 0x04, HEADING = 0x05, SIDE = 0x06, CELL = 0x07,
    XF = 0x09,  /* x's fraction while diving */
    PIC = 0x0F, COUNT = 0x10, TURNS = 0x11, BOOM_PIC = 0x12, PATH = 0x13, COLOUR = 0x16, LAPS = 0x17,
    CURVE = 0x18, AIM_X = 0x19, CURVE_L = 0x1A, CURVE_D = 0x1B, CURVE_E = 0x1C,
};
#define PATHS 0x1E00 /* the arcs out of the swarm: (dy, dx) pairs */

/* the sign of a byte, as the Z80's S flag */
static int negative(uint8_t v) { return v & 0x80; }

static void inc(Game *g, uint16_t a) { WR(a, RD(a) + 1); }
static void dec(Game *g, uint16_t a) { WR(a, RD(a) - 1); }

void aliens_home(Game *g, uint16_t ix)
{
    uint8_t row = RD(ix + CELL) & 0x70; /* row x 16 */
    WR(ix + Y, (uint8_t)-(uint8_t)((row >> 1) + (row >> 2)) + 0x7C);
    WR(ix + X, (uint8_t)(((RD(ix + CELL) & 0x0F) << 4) + 7) + RD(0x420E));
}

void aliens_curve(Game *g, uint16_t ix)
{
    int n = (RD(ix + CURVE) & 3) + 1;
    uint8_t h = RD(ix + AIM_X), l = RD(ix + CURVE_L), d = RD(ix + CURVE_D), e = RD(ix + CURVE_E);
    while (n--) {
        uint8_t c = h;
        if (l & 0x80) h--; /* l x 2, its carry */
        unsigned s = (uint8_t)(l << 1) + d;
        d = (uint8_t)s;
        uint8_t a = (uint8_t)(h + (s >> 8));
        h = a == 0x80 ? c : a;
        c = l;
        uint8_t m = (uint8_t)-h;
        if (m & 0x80) l--;
        s = (uint8_t)(m << 1) + e;
        e = (uint8_t)s;
        a = (uint8_t)(l + (s >> 8));
        l = a == 0x80 ? c : a;
    }
    WR(ix + AIM_X, h);
    WR(ix + CURVE_L, l);
    WR(ix + CURVE_D, d);
    WR(ix + CURVE_E, e);
}

/* $11D0: (A / D) as a heading: the quotient's top three bits (0..4). */
static uint8_t heading(uint8_t a, uint8_t d)
{
    Regs r = { 0 };
    r.a = a, r.d = d;
    util_divide(&r);
    a = r.c;
    if (negative(a)) a = 0x80;
    return a >> 5;
}

void aliens_aim(Game *g, uint16_t ix)
{
    uint8_t d = (uint8_t)(0xF0 - RD(ix + Y)), px = RD(0x4202), x = RD(ix + X);
    if (px >= x) WR(ix + HEADING, heading((uint8_t)(px - x), d));
    else WR(ix + HEADING, (uint8_t)-heading((uint8_t)(x - px), d));
}

/* $1218: a shell's slope: (A / D) plus a random 0..31, plus 6, at most $7F. */
static uint8_t slope(Game *g, uint8_t a, uint8_t d)
{
    Regs r = { 0 };
    r.a = a, r.d = d;
    util_divide(&r);
    a = (uint8_t)((util_random(g) & 0x1F) + r.c + 6);
    return negative(a) ? 0x7F : a;
}

void aliens_shoot(Game *g, uint16_t ix)
{
    uint16_t hl = 0x4260;
    int i = 0;
    while (i < 14 && (RD(hl) & 1)) hl += 5, i++;
    if (i == 14) return; /* none free */
    WR(hl, 1);
    WR(hl + 1, RD(ix + Y));
    uint8_t d = (uint8_t)(0xF0 - RD(hl + 1));
    WR(hl + 3, RD(ix + X));
    uint8_t px = RD(0x4202), x = RD(ix + X);
    if (px >= x) WR(hl + 4, slope(g, (uint8_t)(px - x), d));
    else WR(hl + 4, (uint8_t)-slope(g, (uint8_t)(x - px), d));
}

/* the shots a diving alien may fire: at the y's listed from $4214, $19 apart, $4213 of them */
static void fire_maybe(Game *g, uint16_t ix)
{
    if (!(RD(0x4200) & 1)) return;
    aliens_aim(g, ix);
    if (RD(0x422B) & 1) return;
    uint8_t n = RD(0x4213), at = RD(0x4214), a = RD(ix + Y);
    do {
        if (a == at) {
            aliens_shoot(g, ix);
            return;
        }
        a = (uint8_t)(a + 0x19);
    } while (--n);
}

/* the escorts beside a flagship: how many (the next two slots) */
static void count_escorts(Game *g, uint16_t ix)
{
    uint8_t a = 0;
    if (RD(ix + 0x20) & 1) a++;
    if (RD(ix + 0x40) & 1) a++;
    WR(0x422A, a);
}

/* $0D39: the arc out starts */
static void arc_start(Game *g, uint16_t ix)
{
    WR(ix + COUNT, 3);
    WR(ix + TURNS, 0x0C);
    WR(ix + PATH, 0);
    inc(g, ix + STATE);
    WR(ix + HEADING, RD(ix + SIDE) & 1 ? 0xF4 : 0x0C);
}

/* 0: leaving the swarm */
static void s_leave(Game *g, uint16_t ix)
{
    WR(ix + LAPS, 0);
    WR(0x41C2, 1);
    aliens_home(g, ix);
    uint8_t cell = RD(ix + CELL);
    util_queue(g, 1, cell);
    uint8_t row2 = (uint8_t)((cell & 0x70) >> 3);
    uint16_t hl = (uint16_t)(0x1DD1 + row2);
    WR(ix + COLOUR, RD(hl));
    WR(ix + CURVE, RD(hl + 1));
    if (row2 == 0x0E) { /* a flagship */
        WR(ix + PIC, 0x18);
        count_escorts(g, ix);
    } else {
        WR(ix + PIC, 0);
    }
    arc_start(g, ix);
}

/* 1 and 11: the arc out, along the path */
static void s_arc(Game *g, uint16_t ix)
{
    uint8_t l = RD(ix + PATH);
    WR(ix + Y, RD(ix + Y) + RD(PATHS + l));
    l++;
    int left = RD(ix + SIDE) & 1;
    uint8_t x = left ? (uint8_t)(RD(ix + X) - RD(PATHS + l)) : (uint8_t)(RD(ix + X) + RD(PATHS + l));
    WR(ix + X, x);
    if ((uint8_t)(x + 7) < 0x0E) { /* off the side: round from the bottom */
        WR(ix + STATE, 5);
        return;
    }
    WR(ix + PATH, (uint8_t)(l + 1));
    dec(g, ix + COUNT);
    if (RD(ix + COUNT)) return;
    WR(ix + COUNT, 4);
    if (left) inc(g, ix + HEADING);
    else dec(g, ix + HEADING);
    dec(g, ix + TURNS);
    if (!RD(ix + TURNS)) inc(g, ix + STATE);
}

/* $0DDD: the dive's target: x ahead of the player (or the flagship's, for
   an escort), the curve reset; the next state */
static void target(Game *g, uint16_t ix, int follow)
{
    uint8_t a;
    if (follow && (RD(0x42D0) & 1)) {
        a = RD(0x42E9); /* the flagship's target */
    } else {
        uint8_t px = RD(0x4202), x = RD(ix + X);
        if (x >= px) {
            a = (uint8_t)(((uint8_t)(x - px) >> 1) + 0x10);
            if (a < 0x30) a = 0x30;
            if (a >= 0x70) a = 0x70;
        } else {
            a = (uint8_t)((((uint8_t)(x - px) >> 1) | 0x80) - 0x10);
            if (a >= 0xD0) a = 0xD0;
            if (a < 0x90) a = 0x90;
        }
    }
    WR(ix + AIM_X, a);
    WR(ix + XF, (uint8_t)(RD(ix + X) - a));
    WR(ix + CURVE_L, 0);
    WR(ix + CURVE_D, 0);
    WR(ix + CURVE_E, 0);
    inc(g, ix + STATE);
}

/* 2: aim */
static void s_aim(Game *g, uint16_t ix)
{
    inc(g, ix + Y);
    target(g, ix, (RD(ix + CELL) & 0x70) == 0x60);
}

/* 3: the dive, shooting */
static void s_dive(Game *g, uint16_t ix)
{
    inc(g, ix + Y);
    aliens_curve(g, ix);
    uint8_t x = (uint8_t)(RD(ix + XF) + RD(ix + AIM_X));
    WR(ix + X, x);
    if ((uint8_t)(x + 7) < 0x0E) {
        WR(ix + STATE, RD(ix + STATE) + 2);
        return;
    }
    if (RD(ix + Y) + 0x48 > 0xFF) {
        inc(g, ix + STATE);
        return;
    }
    fire_maybe(g, ix);
}

/* 4: below the player, on to the bottom */
static void s_bottom(Game *g, uint16_t ix)
{
    uint8_t y = (uint8_t)((RD(0x425F) & 1) + 1 + RD(ix + Y));
    WR(ix + Y, y);
    if ((uint8_t)(y - 6) < 3) {
        inc(g, ix + STATE);
        return;
    }
    aliens_curve(g, ix);
    uint8_t a = RD(ix + AIM_X);
    unsigned s = a + RD(ix + XF);
    if (negative(a) ? s > 0xFF : s <= 0xFF) WR(ix + X, (uint8_t)s);
    else inc(g, ix + STATE);
}

/* 5: round from the bottom to the top: back to the swarm, or (late in a
   wave) straight down again */
static void s_top(Game *g, uint16_t ix)
{
    WR(ix + Y, 8);
    inc(g, ix + LAPS);
    WR(ix + HEADING, 0);
    if ((RD(ix + CELL) & 0x70) == 0x70) { /* a flagship */
        if (!RD(0x422A)) { /* alone: it leaves the screen */
            WR(ix + ON, 0);
            uint8_t a = (uint8_t)(RD(0x421E) + 1);
            WR(0x421E, a >= 3 ? 2 : a);
            return;
        }
        count_escorts(g, ix);
    }
    if ((RD(0x4200) & 1) && (RD(0x4224) || RD(0x4221))) {
        uint8_t c = RD(ix + X) >> 1;
        WR(ix + X, (uint8_t)((util_random(g) & 0x1F) + c + 0x20));
        WR(ix + COUNT, 0x28);
        inc(g, ix + STATE);
    }
    inc(g, ix + STATE);
}

/* 6: back into its place in the swarm */
static void s_home(Game *g, uint16_t ix)
{
    uint8_t b = (uint8_t)(RD(ix + Y) + 1);
    aliens_home(g, ix);
    uint8_t a = (uint8_t)(RD(ix + Y) - b);
    WR(ix + Y, b);
    if (!a) {
        WR(ix + ON, 0);
        uint8_t cell = RD(ix + CELL);
        WR(0x4100 + cell, 1);
        util_queue(g, 0, cell);
        return;
    }
    if (a >= 0x19 || (a & 1)) return;
    if (RD(ix + SIDE) & 1) dec(g, ix + HEADING);
    else inc(g, ix + HEADING);
}

/* 7: drifting towards the player */
static void s_drift(Game *g, uint16_t ix)
{
    inc(g, ix + Y);
    uint8_t a1 = (uint8_t)(RD(0x4202) - RD(ix + X));
    uint8_t a2 = (uint8_t)-a1;
    int c = a1 != 0; /* NEG's carry */
    uint8_t e = (uint8_t)(a2 << 1 | c);
    c = a2 >> 7;
    uint8_t d = c ? 0xFF : 0;
    uint8_t e2 = (uint8_t)(e << 1 | c);
    c = e >> 7;
    d = (uint8_t)(d << 1 | c);
    uint16_t hl = (uint16_t)((RD(ix + X) << 8 | RD(ix + XF)) - (d << 8 | e2));
    WR(ix + X, hl >> 8);
    WR(ix + XF, (uint8_t)hl);
    dec(g, ix + COUNT);
    if (!RD(ix + COUNT)) inc(g, ix + STATE);
}

/* $0F7B: a dive again, slowly */
static void dive_again(Game *g, uint16_t ix)
{
    target(g, ix, 0);
    WR(ix + CURVE, 3);
    WR(ix + COUNT, 0x64);
}

/* 8: near the middle: turn back up, else dive again */
static void s_middle(Game *g, uint16_t ix)
{
    inc(g, ix + Y);
    if ((uint8_t)(RD(ix + Y) - 0x60) >= 0x40 || (uint8_t)(RD(ix + X) - 0x60) >= 0x40) {
        dive_again(g, ix);
        return;
    }
    WR(ix + STATE, RD(ix + STATE) + 2);
    WR(ix + COUNT, 3);
    WR(ix + TURNS, 0x0C);
    WR(ix + HEADING, 0);
    WR(ix + PATH, 0);
    WR(ix + SIDE, RD(0x4202) >= RD(ix + X) ? 0 : 1);
}

/* 9: the second dive, homing on the player late in a wave */
static void s_dive2(Game *g, uint16_t ix)
{
    inc(g, ix + Y);
    aliens_curve(g, ix);
    uint8_t laps = RD(ix + LAPS);
    if (laps > 4 || (laps == 4 && (RD(0x425F) & 1))) {
        if (RD(0x4202) >= RD(ix + XF)) inc(g, ix + XF);
        else dec(g, ix + XF);
    }
    uint8_t x = (uint8_t)(RD(ix + XF) + RD(ix + AIM_X));
    WR(ix + X, x);
    if ((uint8_t)(x + 7) < 0x0E) {
        WR(ix + STATE, 5);
        return;
    }
    if (RD(ix + Y) + 0x40 > 0xFF) {
        WR(ix + STATE, 4);
        return;
    }
    dec(g, ix + COUNT);
    if (!RD(ix + COUNT)) {
        dec(g, ix + STATE);
        return;
    }
    fire_maybe(g, ix);
}

/* 10: the arc reversed, up again */
static void s_unarc(Game *g, uint16_t ix)
{
    uint8_t l = RD(ix + PATH);
    WR(ix + Y, RD(ix + Y) - RD(PATHS + l));
    l++;
    int left = RD(ix + SIDE) & 1;
    if (left) WR(ix + X, RD(ix + X) + RD(PATHS + l));
    else WR(ix + X, RD(ix + X) - RD(PATHS + l));
    WR(ix + PATH, (uint8_t)(l + 1));
    dec(g, ix + COUNT);
    if (RD(ix + COUNT)) return;
    WR(ix + COUNT, 4);
    if (left) inc(g, ix + HEADING);
    else dec(g, ix + HEADING);
    dec(g, ix + TURNS);
    if (RD(ix + TURNS)) return;
    inc(g, ix + STATE);
    WR(ix + COUNT, 3);
    WR(ix + TURNS, 0x0C);
    WR(ix + HEADING, left ? 0xF4 : 0x0C);
    WR(ix + PATH, 0);
}

/* 12: dive again */
static void s_again(Game *g, uint16_t ix)
{
    inc(g, ix + Y);
    WR(ix + STATE, 8);
    dive_again(g, ix);
}

/* 13: the score table's parade: into place */
static void s_parade(Game *g, uint16_t ix)
{
    uint8_t b = (uint8_t)~RD(ix + CELL) & 3;
    WR(ix + COLOUR, b + 1);
    WR(ix + Y, (uint8_t)(((b + 1) << 4) + 0x8C));
    WR(ix + COUNT, 0x18);
    inc(g, ix + STATE);
    WR(ix + PIC, b ? 0 : 0x18);
}

/* 14: the parade moves in, then shows its score */
static void s_parade_in(Game *g, uint16_t ix)
{
    inc(g, ix + X);
    dec(g, ix + COUNT);
    if (RD(ix + COUNT)) return;
    util_queue(g, 6, (uint8_t)(RD(ix + CELL) + 0x4B));
    inc(g, ix + STATE);
}

/* 15: the parade's last step */
static void s_parade_end(Game *g, uint16_t ix)
{
    if ((uint8_t)(RD(ix + X) - 0xC8) < 5) return;
    inc(g, ix + X);
}

/* the explosion's states ($10E8) */
static void boom(Game *g, uint16_t ix)
{
    switch (RD(ix + STATE)) {
    case 0:
        WR(ix + COUNT, 4);
        WR(ix + TURNS, 4);
        WR(ix + BOOM_PIC, 0x1C);
        inc(g, ix + STATE);
        WR(0x41DF, RD(ix + CELL) < 0x70 ? 7 : 0x17);
        return;
    case 1:
        dec(g, ix + COUNT);
        if (RD(ix + COUNT)) return;
        WR(ix + COUNT, 4);
        inc(g, ix + BOOM_PIC);
        dec(g, ix + TURNS);
        if (RD(ix + TURNS)) return;
        if (RD(ix + CELL) < 0x70) {
            WR(ix + BOOM, 0);
            return;
        }
        WR(ix + COUNT, 0x32); /* a flagship: its score shown a while */
        WR(ix + BOOM_PIC, RD(0x422D) + 0x20);
        inc(g, ix + STATE);
        return;
    case 2:
        dec(g, ix + COUNT);
        if (!RD(ix + COUNT)) WR(ix + BOOM, 0);
        return;
    case 3:
        return;
    default: /* past the table: what the ROM would jump to */
        game_unported(g, 0x10E7);
        return;
    }
}

static void (*const states[16])(Game *g, uint16_t ix) = {
    s_leave, s_arc, s_aim, s_dive, s_bottom, s_top, s_home, s_drift,
    s_middle, s_dive2, s_unarc, s_arc, s_again, s_parade, s_parade_in, s_parade_end,
};

/* $0CD6 */
static void step(Game *g, uint16_t ix)
{
    if (RD(ix + BOOM) & 1) {
        boom(g, ix);
        return;
    }
    if (!(RD(ix + ON) & 1)) return;
    uint8_t s = RD(ix + STATE);
    if (s < 16) states[s](g, ix);
    else game_unported(g, 0x0CE5);
}

void aliens_step(Game *g)
{
    for (uint16_t ix = SLOTS; ix < SLOTS + 8 * SLOT_SIZE; ix += SLOT_SIZE) step(g, ix);
}

/* $0C20: slot ix as sprite iy, its y drawn c lines up. */
static void sprite(Game *g, uint16_t ix, uint16_t iy, uint8_t c)
{
    if (RD(ix) & 1) {
        WR(iy + 2, RD(ix + 0x16));
        WR(iy + 3, RD(ix + 3) - 8);
        WR(iy, (uint8_t)~RD(ix + 4) - c);
        uint8_t a = RD(ix + 5), pic = RD(ix + 0x0F);
        for (;;) { /* the heading, -12..11, picks the picture and its flips */
            if (!negative(a)) {
                if (!negative((uint8_t)(a - 6))) {
                    if (!negative((uint8_t)(a - 0x0C))) {
                        a = (uint8_t)(a - 0x18);
                        continue;
                    }
                    WR(iy + 1, (uint8_t)((uint8_t)(~a + 0x1E) | 0x80) + pic);
                    WR(iy, RD(iy) + 1);
                    return;
                }
                WR(iy + 1, (uint8_t)((uint8_t)(a + 0x11) | 0xC0) + pic);
                WR(iy + 3, RD(iy + 3) + 1);
                WR(iy, RD(iy) + 1);
                return;
            }
            if (negative((uint8_t)(a - 0xFA))) {
                if (negative((uint8_t)(a - 0xF4))) {
                    a = (uint8_t)(a + 0x18);
                    continue;
                }
                WR(iy + 1, (uint8_t)(a + 0x1D) + pic);
                return;
            }
            WR(iy + 1, (uint8_t)((uint8_t)(~a + 0x12) | 0x40) + pic);
            WR(iy + 3, RD(iy + 3) + 1);
            return;
        }
    }
    if (RD(ix + 1) & 1) { /* exploding */
        WR(iy + 2, 7);
        WR(iy + 3, RD(ix + 3) - 8);
        WR(iy, (uint8_t)~RD(ix + 4) - c);
        WR(iy + 1, RD(ix + 0x12));
        return;
    }
    WR(iy + 3, 0xF8);
    WR(iy, 0xF8);
}

void aliens_sprites(Game *g)
{
    int flipped = RD(0x4018) & 1;
    uint8_t c = flipped ? 9 : 7;
    uint16_t ix = SLOTS, iy = SPRITES;
    for (int i = 0; i < 8; i++, ix += SLOT_SIZE, iy += 4) {
        if (i == 3) c = flipped ? (uint8_t)(c - 1) : (uint8_t)(c + 1);
        sprite(g, ix, iy, c);
    }
}
