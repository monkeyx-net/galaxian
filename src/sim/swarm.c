#include "sim/swarm.h"

#define GRID 0x4100        /* the swarm: a byte per alien, 0 or 1; rows of 16 */
#define ROWS_FLAGS 0x41E8  /* a byte per row: any alien in it */
#define COLUMN_FLAGS 0x41F0 /* a byte per column */
#define LIMITS 0x4210      /* the sway's right and left limits */
#define OFFSET 0x420E      /* the sway: a signed 16-bit offset */
#define DIRECTION 0x420D   /* 0 right, 1 left */
#define FRAME 0x425F       /* counts down a frame at a time */

void swarm_scroll(Game *g, uint8_t a)
{
    for (int i = 0; i < 9; i++) game_wr(g, (uint16_t)(0x4028 + i * 2), a);
}

void swarm_sway(Game *g)
{
    /* the player's missile near the swarm, in line with an occupied column:
       the swarm holds still */
    if (game_rd(g, 0x4208) & 1) {
        uint8_t y = game_rd(g, 0x4209);
        if ((uint8_t)(y - 0x22) < 0x50) {
            uint8_t b = (uint8_t)(game_rd(g, 0x420A) - game_rd(g, OFFSET));
            if ((uint8_t)((b + 2) & 0x0F) < 3 && (game_rd(g, (uint16_t)(COLUMN_FLAGS + (b >> 4))) & 1)) {
                swarm_scroll(g, (uint8_t)-game_rd(g, OFFSET));
                return;
            }
        }
    }
    uint16_t at = game_rd16(g, OFFSET);
    uint8_t right = game_rd(g, LIMITS), left = game_rd(g, LIMITS + 1);
    uint8_t h = (uint8_t)(at >> 8), l = (uint8_t)at;
    if (!game_rd(g, DIRECTION)) {
        if (!(h & 0x80) && l >= right) {
            game_wr(g, DIRECTION, 1);
            return;
        }
        if (game_rd(g, FRAME) & 3) return;
        at++;
    } else {
        if ((h & 0x80) && l < left) {
            game_wr(g, DIRECTION, 0);
            return;
        }
        if (game_rd(g, FRAME) & 3) return;
        at--;
    }
    game_wr16(g, OFFSET, at);
    swarm_scroll(g, (uint8_t)-(uint8_t)at);
}

void swarm_survey(Game *g)
{
    uint16_t d = ROWS_FLAGS;
    game_wr(g, d++, 0);
    game_wr(g, d++, 0);
    uint16_t at = GRID + 0x23; /* 6 rows of 10 */
    for (int row = 0; row < 6; row++, at = (uint16_t)(at + 16)) {
        uint8_t any = 0;
        for (int col = 0; col < 10; col++) any |= game_rd(g, (uint16_t)(at + col));
        game_wr(g, d++, any);
    }
    game_wr(g, d++, 0);
    game_wr(g, d++, 0);
    game_wr(g, d++, 0);
    for (int col = 0; col < 10; col++) {
        uint8_t any = 0;
        for (int row = 0; row < 6; row++) any |= game_rd(g, (uint16_t)(GRID + 0x23 + col + row * 16));
        game_wr(g, d++, any);
    }
    /* the limits: from the outermost occupied columns */
    uint8_t e = 0x22, dd = 0xE0;
    for (int i = 0, x = 0x22; i < 10; i++, x += 0x10)
        if (game_rd(g, (uint16_t)(COLUMN_FLAGS + 0x0C - i)) & 1) {
            e = (uint8_t)x;
            break;
        }
    for (int i = 0, x = 0xE0; i < 10; i++, x -= 0x10)
        if (game_rd(g, (uint16_t)(COLUMN_FLAGS + 3 + i)) & 1) {
            dd = (uint8_t)x;
            break;
        }
    game_wr(g, LIMITS, e);
    game_wr(g, LIMITS + 1, dd);
    /* none left: in the top four rows (0x4221), in the swarm (0x4220) */
    uint8_t any = 0;
    for (int i = 0; i < 4; i++) any |= game_rd(g, (uint16_t)(0x41EA + i));
    game_wr(g, 0x4221, any ^ 1);
    any |= game_rd(g, 0x41EE) | game_rd(g, 0x41EF);
    game_wr(g, 0x4220, any ^ 1);
    /* the flying aliens' slots */
    any = 0;
    for (int i = 0; i < 7; i++) any |= game_rd(g, (uint16_t)(0x42D0 + i * 0x20));
    game_wr(g, 0x4226, any ^ 1);
    for (int i = 0; i < 8; i++) any |= game_rd(g, (uint16_t)(0x42B1 + i * 0x20));
    game_wr(g, 0x4225, any ^ 1);
}
