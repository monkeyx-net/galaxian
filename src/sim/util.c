#include "sim/util.h"

uint8_t util_random(Game *g)
{
    uint8_t v = (uint8_t)(game_rd(g, 0x401E) * 5 + 1);
    game_wr(g, 0x401E, v);
    return v;
}

void util_divide(Regs *r)
{
    uint8_t a = r->a, c = 0, d = r->d;
    for (int i = 0; i < 8; i++) {
        int bit = a >= d;
        if (bit) a = (uint8_t)(a - d);
        int top = c >> 7; /* RL C: bit 7 goes to the carry, which RR D takes in at the top */
        c = (uint8_t)(c << 1 | bit);
        d = (uint8_t)(d >> 1 | top << 7);
    }
    r->a = a;
    r->b = 0;
    r->c = c;
    r->d = d;
}

void util_queue(Game *g, uint8_t cmd, uint8_t arg)
{
    uint8_t slot = game_rd(g, 0x40A0);
    uint16_t at = (uint16_t)(0x4000 | slot);
    if (!(game_rd(g, at) & 0x80)) return; /* the queue is full */
    game_wr(g, at, cmd);
    game_wr(g, (uint16_t)(0x4000 | (uint8_t)(slot + 1)), arg);
    slot = (uint8_t)(slot + 2);
    game_wr(g, 0x40A0, slot < 0xC0 ? 0xC0 : slot);
}

uint16_t util_unpack_grid(Game *g, uint16_t src)
{
    uint16_t at = 0x4100;
    for (int i = 0; i < 16; i++, src++)
        for (int bit = 0; bit < 8; bit++) game_wr(g, at++, game_rd(g, src) >> bit & 1);
    return src;
}

uint16_t util_pack_grid(Game *g, uint16_t dst)
{
    uint16_t at = 0x4100;
    for (int i = 0; i < 16; i++) {
        uint8_t v = 0;
        for (int bit = 0; bit < 8; bit++)
            if (game_rd(g, at++) & 1) v |= (uint8_t)(1 << bit);
        game_wr(g, dst++, v);
    }
    return dst;
}

void util_colours(Game *g, uint16_t src)
{
    for (int i = 0; i < 32; i++) game_wr(g, (uint16_t)(0x4021 + i * 2), game_rd(g, (uint16_t)(src + i)));
}
