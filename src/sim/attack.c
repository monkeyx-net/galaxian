#include "sim/attack.h"

#include "sim/util.h"

/* CPDR / CPIR for 1 over n bytes from `at`: the address after the match's
   (before it, for CPDR), or 0 if none matched or the match was the last
   byte (the ROM returns on PO as on NZ). */
static uint16_t find(Game *g, uint16_t at, int n, int step)
{
    int z = 0;
    do {
        z = RD(at) == 1;
        at = (uint16_t)(at + step);
        n--;
    } while (n && !z);
    return z && n ? at : 0;
}

/* $139A: down the columns from l (d rows each, e to step on), for an
   alien to take: it leaves into slot ix */
static void take(Game *g, uint16_t ix, uint8_t l, uint8_t d, uint8_t e, uint8_t c)
{
    for (;;) {
        uint8_t a = l;
        for (int b = d; b > 0; b--) {
            if (RD(0x4100 + l) & 1) {
                WR(0x4100 + l, 0);
                WR(ix + 7, l);
                WR(ix, 1);
                WR(ix + 2, 0);
                util_queue(g, 1, l);
                return;
            }
            a = (uint8_t)(l - 0x10);
            l = a;
        }
        l = (uint8_t)(a + e);
        if (!--c) return;
    }
}

void attack_lone(Game *g)
{
    if (!(RD(0x4228) & 1)) return;
    WR(0x4228, 0);
    if (RD(0x4220) & 1) return;
    unsigned s = RD(0x421A) + RD(0x421B);
    uint8_t a = (uint8_t)((s >> 1) | (s > 0xFF ? 0x80 : 0));
    if (a >= 4) a = 3;
    int b = a + 1;
    uint16_t hl = 0x4391;
    for (;;) { /* a slot neither flying nor exploding, from the last */
        uint8_t v = RD(hl);
        hl--;
        if (!(v | RD(hl))) break;
        hl = (uint16_t)(hl - 31);
        if (!--b) return;
    }
    uint16_t ix = hl;
    uint8_t side = RD(0x4215);
    WR(ix + 6, side);
    uint16_t at;
    uint8_t e;
    if (!side) { /* from the right: the rightmost column with aliens */
        at = find(g, 0x41FC, 10, -1);
        if (!at) return;
        e = 0x3F;
        at++;
    } else {
        at = find(g, 0x41F3, 10, 1);
        if (!at) return;
        e = 0x41;
        at--;
    }
    uint8_t c = (uint8_t)(10 - ((side ? at - 0x41F3 : 0x41FC - at) + 1)); /* BC left by the search */
    uint8_t l = (uint8_t)at;
    if (RD(0x41EF) & 1) take(g, ix, (uint8_t)((l & 0x0F) + 0x50), 4, e, c);
    else take(g, ix, (uint8_t)((l & 0x0F) + 0x60), 5, (uint8_t)(e + 0x10), c);
}

void attack_side(Game *g)
{
    uint16_t hl = game_rd16(g, 0x420E);
    uint8_t h = (uint8_t)(hl >> 8), l = (uint8_t)hl, e = RD(0x4210), d = RD(0x4211);
    if (h & 0x80) {
        if ((uint8_t)(l - d) < 0x1C) {
            WR(0x4215, 0);
            return;
        }
    } else if ((uint8_t)(e - l) < 0x1C) {
        WR(0x4215, 1);
        return;
    }
    WR(0x4215, util_random(g) & 1);
}

/* $145C: the alien at l leaves into slot ix */
static void leave(Game *g, uint16_t ix, uint8_t l, uint8_t side)
{
    WR(0x4100 + l, 0);
    WR(ix, 1);
    WR(ix + 2, 0);
    WR(ix + 6, side);
    WR(ix + 7, l);
    util_queue(g, 1, l);
}

/* $1446: a flagship-row alien without escorts leaves into the last free slot */
static void lone_flagship(Game *g, uint8_t l, uint8_t side)
{
    for (uint16_t ix = 0x4390; ix > 0x4390 - 4 * 0x20; ix -= 0x20)
        if (!(RD(ix) | RD(ix + 1))) {
            leave(g, ix, l, side);
            return;
        }
}

/* $148E / $149B: the escorts from the row below (l down, b of them looked at) */
static void escorts(Game *g, uint8_t l, int step)
{
    uint16_t iy = 0x42F0;
    int c = 2;
    for (int b = 3; b > 0; b--, l = (uint8_t)(l + step)) {
        if (!(RD(0x4100 + l) & 1)) continue;
        if (!(RD(iy) & 1) && !(RD(iy + 1) & 1)) {
            WR(0x4100 + l, 0);
            WR(iy, 1);
            WR(iy + 2, 0);
            WR(iy + 6, RD(0x42D0 + 6));
            WR(iy + 7, l);
            util_queue(g, 1, l);
        }
        iy += 0x20;
        if (!--c) b = 1;
    }
}

void attack_flagship(Game *g)
{
    if ((RD(0x4220) & 1) || !(RD(0x4200) & 1) || !(RD(0x4229) & 1)) return;
    WR(0x4229, 0);
    if ((RD(0x42D0) | RD(0x42D1)) & 1) return;
    uint8_t side = RD(0x4215);
    if (!(side & 1)) {
        for (uint8_t l = 0x79; l > 0x75; l--) /* the flagships, from the right */
            if (RD(0x4100 + l) & 1) {
                leave(g, 0x42D0, l, side);
                escorts(g, (uint8_t)(l - 0x0F), -1);
                return;
            }
        for (uint8_t l = 0x6A; l > 0x66; l--) /* none: a red alien alone */
            if (RD(0x4100 + l) & 1) {
                lone_flagship(g, l, side);
                return;
            }
    } else {
        for (uint8_t l = 0x76; l < 0x7A; l++)
            if (RD(0x4100 + l) & 1) {
                leave(g, 0x42D0, l, side);
                escorts(g, (uint8_t)(l - 0x11), 1);
                return;
            }
        for (uint8_t l = 0x65; l < 0x69; l++)
            if (RD(0x4100 + l) & 1) {
                lone_flagship(g, l, side);
                return;
            }
    }
}

void attack_rise(Game *g)
{
    if (!(RD(0x4200) & 1) || (RD(0x422B) & 1)) return;
    WR(0x4218, RD(0x4218) - 1);
    if (RD(0x4218)) return;
    WR(0x4218, 0x3C);
    WR(0x4219, RD(0x4219) - 1);
    if (RD(0x4219)) return;
    WR(0x4219, 0x14);
    uint8_t a = RD(0x421A);
    if (a == 7) return;
    WR(0x421A, a > 7 ? 7 : a + 1);
}

void attack_lone_timers(Game *g)
{
    if (!(RD(0x4200) & 1) || (RD(0x4220) & 1) || (RD(0x422B) & 1)) return;
    uint8_t a = RD(0x421B);
    if (a < 2) a = 0;
    int b = ((a + RD(0x421A)) & 0x0F) + 1;
    uint16_t hl = 0x424A, de = 0x15E3;
    WR(hl, RD(hl) - 1);
    if (RD(hl)) {
        WR(0x4228, 0);
        return;
    }
    uint8_t c = 0;
    WR(hl, RD(de));
    while (b--) {
        hl++, de++;
        WR(hl, RD(hl) - 1);
        if (!RD(hl)) {
            WR(hl, RD(de));
            c++;
        }
    }
    if (c) WR(0x4228, 1);
}

void attack_flagship_timer(Game *g)
{
    if (!(RD(0x4200) & 1) || !(RD(0x41EF) & 1) || (RD(0x422B) & 1)) return;
    uint8_t a;
    if (!(RD(0x4006) & 1)) { /* the demo */
        WR(0x4245, RD(0x4245) - 1);
        if (RD(0x4245)) return;
        WR(0x4245, 0x3C);
        WR(0x4246, RD(0x4246) - 1);
        if (RD(0x4246)) return;
        WR(0x4246, 5);
        WR(0x422F, 0x5A);
        WR(0x424A, 0x2D);
        WR(0x422E, 1);
        return;
    }
    WR(0x4245, RD(0x4245) - 1);
    if (RD(0x4245)) return;
    WR(0x4245, 0x3C);
    if (RD(0x4221) & 1) {
        a = 2;
    } else {
        WR(0x4246, RD(0x4246) - 1);
        if (RD(0x4246)) return;
        WR(0x4246, RD(0x4246) + 1);
        uint8_t c = (RD(0x4177) + RD(0x4178)) & 3;
        a = (uint8_t)(RD(0x421A) + RD(0x421B));
        if (!a) return;
        a = (uint8_t)((uint8_t)~((uint8_t)(a >> 2 | a << 6) & 3) + 0x0A - c);
        WR(0x4246, a);
    }
    a = (uint8_t)(a << 2 | a >> 6);
    WR(0x422F, a);
    a = (uint8_t)(a << 1 | a >> 7);
    WR(0x424A, a);
    WR(0x422E, 1);
}

void attack_flagship_go(Game *g)
{
    if (!(RD(0x422E) & 1)) return;
    WR(0x422F, RD(0x422F) - 1);
    if (RD(0x422F)) return;
    WR(0x422E, 0);
    if (!(RD(0x4200) & 1) || !(RD(0x41EF) & 1)) return;
    WR(0x4229, 1);
}

void attack_shot_places(Game *g)
{
    uint8_t e = 1, d = 0x84;
    if (RD(0x421B)) e = 2, d = 0x9D;
    uint16_t hl = 0x41E8;
    for (int b = 4; b > 0; b--) {
        if (RD(hl) & 1) break;
        hl++;
        if (RD(hl) & 1) break;
        hl++;
        e++;
    }
    WR(0x4213, e);
    WR(0x4214, d);
}

void attack_wave_over(Game *g)
{
    if (!(RD(0x4220) & 1) || !(RD(0x4225) & 1) || (RD(0x4222) & 1)) return;
    game_wr16(g, 0x4222, 0x0001);
}

void attack_next_wave(Game *g)
{
    if (!(RD(0x4222) & 1)) return;
    WR(0x4223, RD(0x4223) - 1);
    if (RD(0x4223)) return;
    WR(0x4222, 0);
    util_unpack_grid(g, 0x051B);
    WR(0x421A, 0);
    WR(0x425F, 0);
    game_wr16(g, 0x420E, 0x0001);
    uint8_t a = RD(0x421B);
    if (a < 7) a++;
    else a = 7;
    WR(0x421B, a);
    WR(0x421C, RD(0x421C) + 1);
    util_queue(g, 7, 0);
    a = RD(0x421E);
    if (!a) return;
    WR(0x4177, 1); /* the flagships that got away come back */
    WR(0x421E, --a);
    if (!a) return;
    WR(0x4178, 1);
    WR(0x421E, 0);
}

void attack_flagship_wait(Game *g)
{
    if (!(RD(0x422B) & 1)) return;
    if (!RD(0x4224) && !RD(0x4221) && !(RD(0x4226) & 1)) return;
    WR(0x422C, RD(0x422C) - 1);
    if (RD(0x422C)) return;
    WR(0x422B, 0);
}
