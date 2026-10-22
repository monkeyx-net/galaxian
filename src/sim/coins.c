#include "sim/coins.h"

#include "sim/util.h"

/* $194F: a credit more */
static void credit(Game *g)
{
    uint8_t a = RD(0x4002);
    if (a == 0x63) return;
    if (a > 0x63) {
        WR(0x4002, 0x63);
        return;
    }
    WR(0x4002, a + 1);
    WR(0x41C9, 1);
    util_queue(g, 7, 1);
}

void coins_in(Game *g)
{
    if (RD(0x4000) == 3) { /* free play */
        game_wr16(g, 0x4001, 0x0900);
        return;
    }
    uint8_t a = (uint8_t)(~(RD(0x4010) | RD(0x4013)) & RD(0x4015) & RD(0x4016));
    if (a & 0x80) { /* the service coin */
        if (RD(0x4002) >= 0x63) return;
        WR(0x4002, RD(0x4002) + 1);
        WR(0x41C9, 1);
        util_queue(g, 7, 1);
        return;
    }
    a &= 3;
    if (!a) return;
    WR(0x4004, RD(0x4004) + 1);
    if ((a & 1) && (a & 2)) WR(0x4004, RD(0x4004) + 1);
}

void coins_credit(Game *g)
{
    uint8_t a = RD(0x4003);
    if (a) { /* the counter's pulse */
        WR(0x6003, (uint8_t)(a >> 3 | a << 5));
        WR(0x4003, a - 1);
        return;
    }
    if (!RD(0x4004)) return;
    WR(0x4004, RD(0x4004) - 1);
    WR(0x4003, 0x0F);
    switch (RD(0x4000)) {
    case 3: return;
    case 1: /* 2 coins 1 credit */
        if (RD(0x4001) & 1) {
            WR(0x4001, 0);
            credit(g);
        } else {
            WR(0x4001, 1);
        }
        return;
    case 2: /* 1 coin 2 credits */
        credit(g);
        credit(g);
        return;
    default:
        credit(g);
        return;
    }
}

void coins_lockout(Game *g) { WR(0x6002, RD(0x4002) < 9 ? 1 : 0); }
