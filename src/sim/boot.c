#include "sim/boot.h"

#include "sim/screen.h"
#include "sim/sounds.h"
#include "sim/util.h"

void boot_power_on(Game *g)
{
    const uint8_t *data = g->data;
    uint8_t in0 = g->in0, in1 = g->in1, in2 = g->in2;
    *g = (Game){ 0 };
    g->data = data;
    g->in0 = in0, g->in1 = in1, g->in2 = in2;
    game_fill(g, 0x5000, 0x10, 0x400); /* $1A55, $1B5D */
    game_fill(g, 0x6004, 1, 4);        /* $1A7A */
    WR(0x7800, 0xFF);
    uint8_t a = 1; /* the RAM check's last pass ($1A98, C = 1): $2F more a byte, 1 more a page */
    for (int page = 0; page < 4; page++, a++)
        for (int i = 0; i < 0x100; i++) {
            a = (uint8_t)(a + 0x2F);
            if (page == 3 && i >= 0xA0) WR(0x4300 + i, a); /* what the set-up leaves of it */
        }
    game_fill(g, 0x40C0, 0xFF, 0x40);  /* $1B90: the main loop's queue empty */
    WR(0x4008, 0x20);
    WR(0x401A, 3);
    game_wr16(g, 0x40A0, 0xC0C0);
    WR(0x7004, 1);
    WR(0x7002, 1);
    WR(0x7003, 1);
    WR(0x7001, 1);
    screen_start(g);
}

/* $1CCF: a setting's line ($1CF6's table: text, where, length) */
static void setting(Game *g, uint8_t n)
{
    uint16_t at = (uint16_t)(0x1CF6 + (uint8_t)(n * 5));
    uint16_t text = game_rd16(g, at), to = game_rd16(g, (uint16_t)(at + 2));
    for (int b = RD(at + 4) ? RD(at + 4) : 256; b > 0; b--, to -= 0x20) WR(to, RD(text++) - 0x30);
}

/* mode 3: the object RAM's check; 32 frames, then OK */
static void check_objects(Game *g)
{
    uint8_t a = RD(0x401E);
    for (int i = 0; i < 0x100; i++, a = (uint8_t)(a + 0x2F)) WR(0x5800 + i, a);
    a = RD(0x401E);
    for (int i = 0; i < 0x100; i++, a = (uint8_t)(a + 0x2F))
        if (RD(0x5800 + i) != a) { /* $1C2C: the board is faulty */
            game_unported(g, 0x1C2C);
            return;
        }
    util_random(g);
    WR(0x4008, RD(0x4008) - 1);
    if (RD(0x4008)) return;
    game_fill(g, 0x5800, 0, 0x100);
    WR(0x4006, 1);
    WR(0x401A, 1);
    WR(0x6000, 1);
    WR(0x6001, 1);
    WR(0x6002, 1);
    WR(0x4226, 1);
    WR(0x425F, 1);
    WR(0x4238, 1);
    WR(0x5213, 0x1F);
    WR(0x51F3, 0x1B);
}

/* mode 1: the controls make their sounds, the settings shown, until the service switch is off */
static void settings(Game *g)
{
    sounds_frame(g);
    sounds_6804(g);
    uint8_t b = RD(0x6000), c = RD(0x6800);
    if (b & 0x83) WR(0x41C9, 1);
    if (c & 3) WR(0x41DF, 0x16);
    if ((b | c) & 0x0C) WR(0x41DF, 6);
    if ((b | c) & 0x10) WR(0x41CC, 1);
    uint8_t a = RD(0x6800);
    setting(g, (uint8_t)(a << 2 | a >> 6) & 3);
    setting(g, (RD(0x7000) & 3) + 4);
    setting(g, ((RD(0x7000) >> 2) & 1) + 8);
    if (RD(0x6000) & 0x40) return;
    WR(0x4006, 0);
    WR(0x401A, 2);
    game_wr16(g, 0x4008, 0x3010);
    game_wr16(g, 0x400B, 0x5000);
    game_fill(g, 0x6000, 0, 4);
    game_fill(g, 0x6004, 1, 4);
    game_fill(g, 0x6800, 0, 8);
    game_fill(g, 0x7001, 0, 5);
    WR(0x7800, 0xFF);
}

/* mode 2: the crosshatch, two rows a frame, held a while (and while the service switch is on) */
static void crosshatch(Game *g)
{
    if (RD(0x4008)) {
        uint16_t hl = game_rd16(g, 0x400B);
        for (int b = 0; b < 16; b++) WR(hl++, 0x30), WR(hl++, 0x32);
        for (int b = 0; b < 16; b++) WR(hl++, 0x34), WR(hl++, 0x36);
        game_wr16(g, 0x400B, hl);
        WR(0x4008, RD(0x4008) - 1);
        if (RD(0x4008)) return;
    }
    if (RD(0x4009)) {
        WR(0x4009, RD(0x4009) - 1);
        if (RD(0x4009)) return;
    }
    if (RD(0x6000) & 0x40) return;
    game_wr16(g, 0x400B, 0x5000);
    WR(0x4008, 0x20);
    WR(0x401A, 0);
    WR(0x4005, 0);
}

void boot_nmi(Game *g)
{
    switch (RD(0x401A)) {
    case 1: settings(g); return;
    case 2: crosshatch(g); return;
    case 3: check_objects(g); return;
    default: game_unported(g, 0x1BDA); return; /* the ROM starts again */
    }
}
