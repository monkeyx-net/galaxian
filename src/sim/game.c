#include "sim/game.h"

uint8_t game_rd(const Game *g, uint16_t a)
{
    switch (a >> 11) {
    case 0x0: case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: case 0x6: case 0x7:
        if (a >= GAME_DATA_SIZE || !g->data) return 0;
        if (g->data_read) g->data_read[a] = 1;
        return g->data[a];
    case 0x8: return g->ram[a & 0x3FF];
    case 0xA: return g->tiles[a & 0x3FF];
    case 0xB: return g->objects[a & 0xFF];
    case 0xC: return g->in0;
    case 0xD: return g->in1;
    case 0xE: return g->in2;
    default: return 0xFF;
    }
}

void game_wr(Game *g, uint16_t a, uint8_t v)
{
    if (g->on_write) g->on_write(g->user, a, v);
    switch (a >> 11) {
    case 0x8: g->ram[a & 0x3FF] = v; break;
    case 0xA: g->tiles[a & 0x3FF] = v; break;
    case 0xB: g->objects[a & 0xFF] = v; break;
    case 0xC: g->latch6000[a & 7] = v & 1; break;
    case 0xD: g->latch6800[a & 7] = v & 1; break;
    case 0xE: g->latch7000[a & 7] = v & 1; break;
    case 0xF: g->pitch = v; break;
    default: break;
    }
}

uint16_t game_rd16(const Game *g, uint16_t a) { return (uint16_t)(game_rd(g, a) | game_rd(g, (uint16_t)(a + 1)) << 8); }

void game_wr16(Game *g, uint16_t a, uint16_t v)
{
    game_wr(g, a, (uint8_t)v);
    game_wr(g, (uint16_t)(a + 1), (uint8_t)(v >> 8));
}

void game_fill(Game *g, uint16_t a, uint8_t v, int n)
{
    for (int i = 0; i < n; i++) game_wr(g, (uint16_t)(a + i), v);
}

void game_copy(Game *g, uint16_t dst, uint16_t src, int n)
{
    for (int i = 0; i < n; i++) game_wr(g, (uint16_t)(dst + i), game_rd(g, (uint16_t)(src + i)));
}

void game_unported(Game *g, uint16_t addr)
{
    if (!g->unported) g->unported = addr;
}
