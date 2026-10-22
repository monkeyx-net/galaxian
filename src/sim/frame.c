#include "sim/frame.h"

#include "sim/boot.h"
#include "sim/coins.h"
#include "sim/flow.h"
#include "sim/screen.h"
#include "sim/sounds.h"
#include "sim/text.h"

void frame_nmi(Game *g)
{
    WR(0x7001, 0);
    if (RD(0x401A)) {
        boot_nmi(g);
    } else {
        game_copy(g, 0x5800, 0x4020, 0x80);
        WR(0x4016, RD(0x4015));
        WR(0x4015, RD(0x4013));
        WR(0x4013, RD(0x4010));
        WR(0x4014, RD(0x4011));
        WR(0x4012, RD(0x7000));
        WR(0x4011, RD(0x6800));
        WR(0x4010, RD(0x6000));
        if (RD(0x4010) & 0x40) { /* the service switch: the ROM starts again */
            game_unported(g, 0x0000);
            return;
        }
        WR(0x425F, RD(0x425F) - 1);
        coins_in(g);
        coins_credit(g);
        coins_lockout(g);
        sounds_frame(g);
        sounds_lfo(g);
        text_type(g);
        flow_state(g);
    }
    WR(0x7001, 1);
}

int frame_run(Game *g)
{
    frame_nmi(g);
    if (g->unported) return -1;
    screen_run(g);
    return g->unported ? -1 : 0;
}
