#include "sim/text.h"

void text_type(Game *g)
{
    if (!(RD(0x40B0) & 1)) return;
    uint16_t counter = game_rd16(g, 0x40B1);
    if (!(RD(counter) & 7)) {
        uint16_t from = game_rd16(g, 0x40B3);
        uint8_t ch = RD(from);
        if (ch != 0x3F) {
            game_wr16(g, 0x40B3, (uint16_t)(from + 1));
            uint16_t to = game_rd16(g, 0x40B5);
            WR(to, ch - 0x30);
            game_wr16(g, 0x40B5, (uint16_t)(to - 0x20));
        }
    }
    WR(counter, RD(counter) - 1);
    if (!RD(counter)) WR(0x40B0, 0);
}
