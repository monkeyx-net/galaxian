#include "sim/demo.h"

#include "sim/util.h"

/* $1A12: one object (y h, x l, its direction c) near the ship adds its weight to b */
static uint8_t weigh(Game *g, uint16_t ix, uint8_t h, uint8_t l, uint8_t c, uint8_t b)
{
    if (!(RD(ix) & 1)) return b;
    if (h < 0x80) return b;
    uint8_t a = (uint8_t)(h - 0x80), e = 0;
    if (a >= 0x34) {
        e = 1;
        if ((uint8_t)(a - 0x34) >= 0x34) return b;
    }
    a = (uint8_t)(RD(0x4202) - l - 0x40);
    if (a >= 0x80) return b;
    a = (uint8_t)((c & 0x80) | (a & 0x60));
    e = (uint8_t)((a >> 4 | a << 4) | e);
    return (uint8_t)(RD(0x1A45 + e) + b);
}

/* SRA: an arithmetic shift right */
static uint8_t sra(uint8_t a) { return (uint8_t)(a >> 1 | (a & 0x80)); }

void demo_pilot(Game *g)
{
    if ((uint8_t)(RD(0x425F) + 9) & 0x1F) return;
    if (!(RD(0x4007) & 1) || !(RD(0x4200) & 1)) return;
    uint8_t b = 0;
    for (uint16_t ix = 0x42D0; ix < 0x42D0 + 7 * 0x20; ix += 0x20)
        b = weigh(g, ix, RD(ix + 3), RD(ix + 4), RD(ix + 0x1A), b);
    for (uint16_t ix = 0x4260; ix < 0x4260 + 7 * 5; ix += 5) b = weigh(g, ix, RD(ix + 1), RD(ix + 3), RD(ix + 4), b);
    uint8_t a = (uint8_t)(RD(0x420E) + 0x80 - RD(0x4202));
    for (int i = 0; i < 5; i++) a = sra(a);
    uint8_t c = sra((uint8_t)(a + b));
    a = util_random(g) & 0x80 ? 0xFF : 1;
    a = (uint8_t)(a + c + 1);
    WR(0x423F, (a & 0x80) ? 8 : a >= 2 ? 4 : 0);
}
