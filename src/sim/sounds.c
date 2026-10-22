#include "sim/sounds.h"

void sounds_6804(Game *g)
{
    if (RD(0x4007) & 1) return;
    uint8_t a = RD(0x41DF);
    if (!a) return;
    WR(0x6804, (uint8_t)(a >> 2 | a << 6));
    WR(0x41DF, a - 1);
}

void sounds_swarm(Game *g)
{
    if (RD(0x4007) & 1) return;
    uint8_t a = 1;
    for (int row = 0; row < 6; row++)
        for (int col = 0; col < 10; col++) a = (uint8_t)(a + RD(0x4123 + row * 16 + col));
    int b = 3;
    uint16_t hl = 0x6800;
    while (b) {
        if (!--a) break;
        WR(hl, 1);
        hl++, b--;
    }
    for (; b; b--) WR(hl++, 0);
    WR(0x4224, a < 2);
}

/* $1747: the start tune ($41D1 set) */
static void tune_start(Game *g)
{
    if (RD(0x41D1) != 1) return;
    WR(0x41D1, 0);
    WR(0x41D2, 1);
    WR(0x41D6, 1);
    game_wr16(g, 0x41D3, 0x1E68);
}

/* $176C: a tune's channel (its flag at `at`): the pitch, and on each
   note's end the next (a byte: the pitch from $17A9 by bits 0-4, the
   length from $17C8 by bits 5-7; $E0 ends it) */
static void tune(Game *g, uint16_t at)
{
    if (!RD(at)) return;
    WR(0x41C0, 2);
    WR(0x41C1, RD(0x41D5));
    uint8_t a = (uint8_t)(RD(0x41D6) - 1);
    if (!a) {
        uint16_t hl = game_rd16(g, 0x41D3);
        uint8_t b = RD(hl);
        if (b == 0xE0) {
            WR(at, 0);
            return;
        }
        game_wr16(g, 0x41D3, (uint16_t)(hl + 1));
        WR(0x41D5, RD(0x17A9 + (b & 0x1F)));
        a = RD(0x17C8 + (b >> 5));
    }
    WR(0x41D6, a);
}

/* $17D0: an alien's dive ($41C2 set): a whine falling while any flies */
static void dive(Game *g)
{
    if (!(RD(0x4006) & 1)) return;
    if (RD(0x41C2) == 1) {
        WR(0x41C2, 0);
        game_wr16(g, 0x41C3, 0xA002);
        return;
    }
    if (RD(0x4226) & 1) return;
    if (!(RD(0x425F) & 1)) {
        uint8_t a = RD(0x41C4);
        if (a < 0x60) WR(0x41C3, RD(0x41C3) + 1);
        if (a) WR(0x41C4, a - 1);
    }
    uint8_t a;
    if (!(RD(0x41C3) & 3)) a = 0x60;
    else if (!(RD(0x41C3) & 1)) a = RD(0x41C4);
    else {
        unsigned s = RD(0x41C4) + 0x60;
        a = (uint8_t)((s >> 1) | (s > 0xFF ? 0x80 : 0));
    }
    WR(0x41C1, a);
}

/* $1819: the explosions' tunes ($41DF: 6 an alien's, $16 a flagship's) */
static void explosion_tunes(Game *g)
{
    if (!(RD(0x4006) & 1)) return;
    uint8_t a = RD(0x41DF);
    if (a == 6) {
        if (RD(0x41CD) & 1) return;
        WR(0x41CF, 1);
        WR(0x41D6, 1);
        game_wr16(g, 0x41D3, 0x1EBD);
    } else if (a == 0x16) {
        WR(0x41CF, 0);
        WR(0x41CD, 1);
        WR(0x41D6, 1);
        game_wr16(g, 0x41D3, 0x1EDF);
    }
}

/* $184F: the extra life's warble ($41C7 starts it, $41C8 counts) */
static void extra_life(Game *g)
{
    if (RD(0x41C7) & 1) {
        game_wr16(g, 0x41C7, 0x8000);
        return;
    }
    uint8_t a = RD(0x41C8);
    if (!a) return;
    WR(0x41C8, --a);
    WR(0x41C1, (a & 4) ? 0x80 : 0xFF);
    WR(0x41C0, 1);
}

/* $1876: the coin's rising tone ($41C9 starts it) */
static void coin(Game *g)
{
    if (RD(0x41C9) == 1) {
        WR(0x41C9, 0);
        game_wr16(g, 0x41CA, 0x0020);
        return;
    }
    if (!RD(0x41CA)) return;
    WR(0x41CA, RD(0x41CA) - 1);
    uint8_t a = (uint8_t)(RD(0x41CB) + 4);
    WR(0x41CB, a);
    WR(0x41C1, a);
    WR(0x41C0, 0);
}

/* $1723: the fire sound, 8 frames ($41CC starts it) */
static void fire(Game *g)
{
    uint8_t a = (uint8_t)(RD(0x41CC) - 1);
    if (!a) {
        WR(0x41CC, 0);
        WR(0x41CE, 8);
        return;
    }
    a = RD(0x41CE);
    if (a) {
        WR(0x41CE, a - 1);
        a = RD(0x4007) ^ 1;
    }
    WR(0x6805, a);
}

void sounds_frame(Game *g)
{
    WR(0x41C0, 0);
    WR(0x41C1, 0xFF);
    tune_start(g);
    dive(g);
    explosion_tunes(g);
    tune(g, 0x41D2);
    tune(g, 0x41CF);
    tune(g, 0x41CD);
    extra_life(g);
    coin(g);
    fire(g);
    uint8_t v = RD(0x41C0);
    WR(0x6806, v);
    WR(0x6807, (uint8_t)(v >> 1 | v << 7));
    WR(0x7800, RD(0x41C1));
}

void sounds_lfo(Game *g)
{
    uint8_t a;
    if (RD(0x41D0)) {
        WR(0x41D0, 0);
        a = 0x0F;
    } else {
        if (RD(0x425F) != 0xFF) return;
        a = RD(0x421F);
        if (!a) return;
        a--;
    }
    WR(0x421F, a);
    for (int i = 0; i < 4; i++, a = (uint8_t)(a >> 1 | a << 7)) WR(0x6004 + i, a);
}
