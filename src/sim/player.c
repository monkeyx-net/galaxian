#include "sim/player.h"

#include "sim/util.h"

void player_hit(Game *g)
{
    if (!(RD(0x4204) & 1)) return;
    WR(0x4204, 0);
    game_wr16(g, 0x4200, 0x0100);
    game_wr16(g, 0x4205, 0x040A);
    util_queue(g, 2, 5);
    uint8_t a = RD(0x421A);
    if (a) a--;
    WR(0x421A, a);
    WR(0x421D, RD(0x421D) - 1);
    if (RD(0x421D) >= 6) WR(0x421D, 5);
    if (RD(0x4006) & 1) WR(0x6803, 1);
}

void player_explode(Game *g)
{
    if (!(RD(0x4201) & 1)) return;
    WR(0x4205, RD(0x4205) - 1);
    if (RD(0x4205)) return;
    WR(0x4205, 0x0A);
    util_queue(g, 2, RD(0x4206));
    WR(0x4206, RD(0x4206) - 1);
    if (RD(0x4206)) return;
    WR(0x4201, 0);
    WR(0x6803, 0);
}

/* $086B: the ship's columns' scroll a, colour c */
static void columns(Game *g, uint8_t a, uint8_t c)
{
    for (int i = 0; i < 4; i++) {
        WR(0x4054 + i * 2, a);
        WR(0x4055 + i * 2, c);
    }
}

void player_move(Game *g)
{
    if (!(RD(0x4200) & 1)) {
        if (RD(0x4201) & 1) columns(g, (uint8_t)~RD(0x4202) + 0x80, 7);
        else {
            WR(0x4202, 0);
            columns(g, (uint8_t)~RD(0x4202) + 0x80, 6);
        }
        return;
    }
    uint8_t b = !(RD(0x4006) & 1) ? RD(0x423F) : (RD(0x4018) & 1) ? RD(0x4011) : RD(0x4010);
    if ((b & 8) && RD(0x4202) >= 0x17) WR(0x4202, RD(0x4202) - 1);
    if ((b & 4) && RD(0x4202) < 0xE9) WR(0x4202, RD(0x4202) + 1);
    columns(g, (uint8_t)~RD(0x4202) + 0x80, 6);
}
