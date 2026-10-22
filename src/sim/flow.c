#include "sim/flow.h"

#include "sim/aliens.h"
#include "sim/attack.h"
#include "sim/demo.h"
#include "sim/player.h"
#include "sim/shots.h"
#include "sim/sounds.h"
#include "sim/swarm.h"
#include "sim/util.h"

#define STATE 0x4005
#define STEP 0x400A
#define FRAME 0x425F

static void inc(Game *g, uint16_t a) { WR(a, RD(a) + 1); }

/* RST $10 n times from `at`, B = 0 meaning 256; returns where it stopped */
static uint16_t fill(Game *g, uint16_t at, uint8_t v, uint8_t b)
{
    int n = b ? b : 256;
    game_fill(g, at, v, n);
    return (uint16_t)(at + n);
}

/* $032E: $4009 counts down to the next step */
static void count_step(Game *g)
{
    WR(0x4009, RD(0x4009) - 1);
    if (!RD(0x4009)) inc(g, STEP);
}

/* $0336: $4008 counts frames, $4009 seconds, to the next step */
static void count_seconds(Game *g)
{
    WR(0x4008, RD(0x4008) - 1);
    if (RD(0x4008)) return;
    WR(0x4008, 0x3C);
    count_step(g);
}

/* $0593: the next step, the colours from $1D71 */
static void next_with_colours(Game *g)
{
    inc(g, STEP);
    util_colours(g, 0x1D71);
}

void flow_quiet(Game *g)
{
    game_fill(g, 0x6004, 1, 4);
    game_fill(g, 0x6800, 0, 8);
    game_fill(g, 0x7001, 0, 5);
    WR(0x7800, 0xFF);
}

/* ---- play ------------------------------------------------------------------- */

void flow_play(Game *g)
{
    player_move(g);           /* $0837 */
    shots_missile(g);         /* $0898 */
    shots_move_shells(g);     /* $0A74 */
    aliens_step(g);           /* $0CC3 */
    aliens_sprites(g);        /* $0BBE */
    shots_fire(g);            /* $0A32 */
    shots_hit_swarm(g);       /* $0B0B */
    shots_hit_ship(g);        /* $0B77 */
    shots_hit_flying(g);      /* $1227 */
    shots_crash(g);           /* $129E */
    shots_missile_done(g);    /* $08E5 */
    attack_flagship(g);       /* $140C */
    attack_lone(g);           /* $1344 */
    attack_side(g);           /* $13E1 */
    attack_rise(g);           /* $14F3 */
    player_hit(g);            /* $12ED */
    player_explode(g);        /* $1327 */
    sounds_6804(g);           /* $16A6 */
    attack_lone_timers(g);    /* $1515 */
    attack_flagship_timer(g); /* $1555 */
    attack_flagship_go(g);    /* $15C3 */
    attack_shot_places(g);    /* $15F4 */
    attack_wave_over(g);      /* $1621 */
    attack_next_wave(g);      /* $1637 */
    sounds_swarm(g);          /* $16B8 */
    attack_flagship_wait(g);  /* $1688 */
    demo_pilot(g);            /* $198E */
    if ((RD(0x4208) | RD(0x4200) | RD(0x4201)) & 1) return;
    if (!(RD(0x4225) & 1)) return;
    uint8_t any = 0;
    for (int i = 0; i < 14; i++) any |= RD(0x4260 + i * 5);
    if (any & 1) return;
    count_step(g);
}

/* $0614: the ship appears */
static void ship_appears(Game *g)
{
    WR(0x4009, RD(0x4009) - 1);
    if (RD(0x4009)) return;
    WR(0x4009, 0x0A);
    inc(g, STEP);
    game_wr16(g, 0x4200, 0x0001);
    WR(0x4202, 0x80);
    game_copy(g, 0x424A, 0x15E3, 0x10);
    WR(0x4058, 0);
    WR(0x405A, 0);
    util_queue(g, 7, 3);
    util_queue(g, 2, 0);
}

/* $0728: the game is over: the attract mode again */
static void game_over(Game *g)
{
    WR(STATE, 1);
    WR(0x4006, 0);
    WR(STEP, 0);
    flow_quiet(g);
    util_queue(g, 6, 0);
}

/* $0712: the same player again (the demo: its ship again, step $0E) */
static void again(Game *g)
{
    WR(STEP, RD(0x4006) & 1 ? 4 : 0x0E);
    WR(0x4009, 0x50);
}

/* $070D: the next step after a pause */
static void next_after_pause(Game *g)
{
    inc(g, STEP);
    WR(0x4009, 0x50);
}

/* $06D8 (player one, and the demo) / $07E8 (player two): a ship lost or
   the wave's end: the turn goes on, passes, or the game ends */
static void turn_over(Game *g, int two)
{
    uint16_t other = two ? 0x4195 : 0x41B5;
    if (RD(0x421D)) { /* ships left */
        if (!RD(other) || (!two && !RD(0x400E))) again(g);
        else next_after_pause(g);
        return;
    }
    if (RD(other) && (two || RD(0x400E))) { /* this player is out, the other plays on */
        inc(g, STEP);
        WR(0x4009, 0x82);
        if (!(RD(0x4006) & 1)) return;
        util_queue(g, 6, two ? 3 : 2);
        util_queue(g, 6, 0);
        return;
    }
    if (!(RD(0x4006) & 1)) next_after_pause(g);
    else game_over(g);
}

/* $073D / $0818: the turn passes to the other player, the swarm saved */
static void pass(Game *g, int two)
{
    WR(0x4009, RD(0x4009) - 1);
    if (RD(0x4009)) return;
    WR(STEP, 0);
    if (!two) {
        WR(0x4222, 0);
        WR(0x422B, 0);
    } else {
        WR(0x400D, 0);
        WR(STATE, 3);
    }
    uint16_t de = util_pack_grid(g, two ? 0x41A0 : 0x4180);
    game_copy(g, de, 0x4218, 8);
    if (!two) {
        WR(0x400D, 1);
        WR(STATE, 4);
    }
}

/* ---- the attract mode's steps ---------------------------------------------------- */

/* $0341: a parade alien for the score table ($4009: 4..1) into slot 4..7 */
static void parade_alien(Game *g)
{
    uint8_t b = (uint8_t)(RD(0x4009) - 1);
    uint16_t ix = (uint16_t)(0x4330 + (uint8_t)(b >> 3 | b << 5));
    WR(ix, 1);
    WR(ix + 1, 0);
    WR(ix + 2, 0x0D);
    WR(ix + 4, 0);
    WR(ix + 5, 0x0C);
    WR(ix + 7, b);
}

/* $03AF: three tiles from `src` down the screen from `de` (only its low byte steps) */
static uint16_t three(Game *g, uint16_t src, uint16_t *de)
{
    uint8_t e = (uint8_t)*de;
    for (int c = 0; c < 3; c++) {
        WR((*de & 0xFF00) | e, RD(src++));
        e = (uint8_t)(e - 0x20);
    }
    *de = (uint16_t)((*de & 0xFF00) | (uint8_t)(e + 0x62));
    return src;
}

/* $0367: the score table's points blink by the frame */
static void table_points(Game *g)
{
    uint8_t n = RD(0x4241);
    if (!n || !--n) return;
    uint8_t f = RD(FRAME);
    if (!(f & 0x3F)) { /* $03C0: blanked */
        uint16_t hl = 0x5193;
        for (; n; n--) {
            for (int c = 0; c < 3; c++, hl -= 0x20) WR(hl, 0x10);
            hl = (uint16_t)((hl & 0xFF00) | (uint8_t)(hl + 0x62));
        }
        return;
    }
    if ((f & 0x3F) != 0x20) return;
    uint8_t c = f >> 6;
    uint16_t src = (uint16_t)(0x039A + c * 3), de = 0x5193;
    three(g, src, &de);
    for (src = 0x03A6; --n;) src = three(g, src, &de);
}

/* the attract mode's step at $400A ($0164's table) */
static void attract_step(Game *g)
{
    uint16_t hl;
    switch (RD(STEP)) {
    case 0: /* $018C: GAME OVER */
        util_queue(g, 7, 1);
        util_queue(g, 6, 0);
        WR(0x4007, 1);
        WR(0x7004, 1);
        WR(0x7002, 1);
        WR(0x7003, 1);
        inc(g, STEP);
        WR(0x4019, 0);
        WR(0x400D, 0);
        WR(0x400E, 0);
        WR(0x4006, 0);
        game_wr16(g, 0x4008, 0x1060);
        return;
    case 1: /* $01BE */
        WR(0x4019, 1);
        count_seconds(g);
        return;
    case 2:
    case 8: /* $01C6: the swarm cleared */
        fill(g, 0x4100, 0, 0x80);
        WR(FRAME, 0);
        WR(0x4224, 0);
        game_wr16(g, 0x400B, 0x5002);
        WR(0x4009, 0x20);
        inc(g, STEP);
        return;
    case 3: /* $01E1: the screen cleared a row a frame, then the score table's colours */
        hl = fill(g, game_rd16(g, 0x400B), 0x10, 0x1C);
        game_wr16(g, 0x400B, (uint16_t)(hl + 4));
        WR(0x4009, RD(0x4009) - 1);
        if (RD(0x4009)) return;
        inc(g, STEP);
        game_wr16(g, 0x4008, 0x0440);
        fill(g, 0x4200, 0, 0x30);
        WR(0x7006, 0);
        WR(0x7007, 0);
        WR(0x4018, 0);
        WR(0x4238, 1);
        util_colours(g, 0x1DB1);
        return;
    case 4: /* $0218: the score table's lines typed */
        swarm_scroll(g, 0);
        WR(0x4008, RD(0x4008) - 1);
        if (RD(0x4008)) return;
        WR(0x4008, 0x50);
        util_queue(g, 6, (uint8_t)(RD(0x4009) + 6));
        WR(0x4009, RD(0x4009) - 1);
        if (RD(0x4009)) return;
        inc(g, STEP);
        game_wr16(g, 0x4008, 0x0420);
        fill(g, 0x42B0, 0, 0);
        WR(0x4241, 0);
        return;
    case 5: /* $023F: the parade comes on */
        swarm_scroll(g, 0);
        aliens_sprites(g);
        aliens_step(g);
        table_points(g);
        WR(0x4008, RD(0x4008) - 1);
        if (RD(0x4008)) return;
        WR(0x4008, 0xD2);
        parade_alien(g);
        inc(g, 0x4241);
        WR(0x4009, RD(0x4009) - 1);
        if (RD(0x4009)) return;
        WR(0x4009, 0xD2);
        inc(g, STEP);
        WR(0x4058, 0);
        return;
    case 6: /* $0267 */
        swarm_scroll(g, 0);
        aliens_sprites(g);
        aliens_step(g);
        table_points(g);
        WR(0x4009, RD(0x4009) - 1);
        if (RD(0x4009)) return;
        inc(g, STEP);
        WR(0x4058, 0);
        game_wr16(g, 0x4008, 0x1140);
        inc(g, 0x4241);
        util_queue(g, 6, 0x0F);
        return;
    case 7: /* $028E */
        swarm_scroll(g, 0);
        aliens_sprites(g);
        aliens_step(g);
        table_points(g);
        count_seconds(g);
        return;
    case 9: /* $029D: cleared again */
        swarm_scroll(g, 0);
        hl = fill(g, game_rd16(g, 0x400B), 0x10, 0x1C);
        game_wr16(g, 0x400B, (uint16_t)(hl + 4));
        WR(0x4009, RD(0x4009) - 1);
        if (RD(0x4009)) return;
        inc(g, STEP);
        fill(g, 0x42B0, 0, 0);
        fill(g, 0x4060, 0, 0x40);
        game_wr16(g, 0x4008, 0x0440);
        util_colours(g, 0x1D71);
        util_queue(g, 6, 0);
        return;
    case 10: /* $02D1 */
        util_queue(g, 7, 1);
        util_queue(g, 6, 0);
        inc(g, STEP);
        game_wr16(g, 0x4008, 0x1060);
        return;
    case 11:
    case 17: /* $032E */
        count_step(g);
        return;
    case 12: /* $02E8 */
        fill(g, 0x4100, 0, 0x80);
        WR(FRAME, 0);
        WR(0x4238, 0);
        WR(0x4009, 0x40);
        next_with_colours(g);
        return;
    case 13: { /* $02FD: the demo's swarm */
        uint16_t de = util_unpack_grid(g, 0x051B);
        game_copy(g, 0x4218, de, 8);
        WR(FRAME, 0);
        WR(0x421D, 1);
        inc(g, STEP);
        WR(0x400B, 0x96);
        game_wr16(g, 0x4245, 0x0640);
        return;
    }
    case 14: ship_appears(g); return;
    case 15: flow_play(g); return;
    case 16: turn_over(g, 0); return;
    case 18: /* $0322 */
        WR(STEP, 1);
        game_wr16(g, 0x4008, 0x0303);
        return;
    default:
        game_unported(g, 0x0163);
        return;
    }
}

/* $03D7: a credit ends the attract mode */
static void credit_check(Game *g)
{
    if (!RD(0x4002)) return;
    inc(g, STATE);
    WR(0x4007, 0);
    WR(STEP, 0);
    WR(0x41C2, 0);
    WR(0x41DF, 0);
    WR(0x40B0, 0);
}

/* ---- PUSH START BUTTON ------------------------------------------------------------- */

/* $0473: the start buttons' lamps blink with the credits */
static void lamps(Game *g)
{
    if (!(RD(FRAME) & 0x20)) {
        WR(0x6000, 0);
        WR(0x6001, 0);
        return;
    }
    uint8_t n = RD(0x4002);
    if (!n) return;
    WR(0x6000, 1);
    if (--n) WR(0x6001, 1);
}

static void start_step(Game *g)
{
    uint16_t hl;
    switch (RD(STEP)) {
    case 0: /* $0408 */
        util_colours(g, 0x1D91);
        fill(g, 0x4060, 0, 0x40);
        hl = fill(g, 0x4260, 0, 0);
        fill(g, hl, 0, 0x50);
        WR(0x4238, 0);
        WR(0x40B0, 0);
        game_wr16(g, 0x400B, 0x5002);
        WR(0x4009, 0x10);
        inc(g, STEP);
        return;
    case 1: /* $0430 */
        WR(0x4019, RD(0x4019) - 1);
        if (RD(0x4019)) {
            lamps(g);
            return;
        }
        inc(g, STEP);
        fill(g, 0x4100, 0, 0x80);
        return;
    case 2: /* $0443: two rows a frame cleared, then PUSH START BUTTON */
        hl = fill(g, game_rd16(g, 0x400B), 0x10, 0x1C);
        hl = fill(g, (uint16_t)(hl + 4), 0x10, 0x1C);
        game_wr16(g, 0x400B, (uint16_t)(hl + 4));
        WR(0x4009, RD(0x4009) - 1);
        if (RD(0x4009)) return;
        inc(g, STEP);
        WR(0x7006, 0);
        WR(0x7007, 0);
        WR(0x4018, 0);
        util_queue(g, 7, 2);
        util_queue(g, 6, 1);
        lamps(g);
        return;
    case 3: lamps(g); return;
    default: game_unported(g, 0x03FF); return;
    }
}

/* $0492: a start button with credits: the game begins */
static void start_check(Game *g)
{
    uint8_t in = RD(0x4011);
    if (in & 1) { /* one player */
        if (!RD(0x4002)) {
            WR(STATE, 1);
            return;
        }
        WR(0x4002, RD(0x4002) - 1);
        fill(g, 0x41A0, 0, 0x20);
        game_wr16(g, 0x400D, 0x0000);
    } else {
        if (!(in & 2) || RD(0x4002) < 2) return;
        WR(0x4002, RD(0x4002) - 2);
        game_copy(g, 0x41A0, 0x051B, 0x20);
        if (RD(0x401F) & 1) WR(0x41B5, 3);
        game_wr16(g, 0x400D, 0x0100);
    }
    game_copy(g, 0x4180, 0x051B, 0x20);
    if (RD(0x401F) & 1) WR(0x4195, 3);
    WR(STEP, 0);
    WR(STATE, 3);
    WR(0x4006, 1);
    WR(0x41D1, 1);
    util_queue(g, 6, 4);
    util_queue(g, 4, 0);
    util_queue(g, 4, 1);
}

/* ---- a player's turn ------------------------------------------------------------- */

/* $05DC / $07CC's queue: the scores, PLAYER ONE (TWO), HIGH SCORE, ships, flags */
static void turn_texts(Game *g, int two)
{
    if (!two && !(RD(0x400E) & 1)) util_queue(g, 5, 0);
    else util_queue(g, 5, 3);
    if (!two) util_queue(g, 5, 2);
    util_queue(g, 6, two ? 3 : 2);
    util_queue(g, 6, 4);
    util_queue(g, 7, 3);
    util_queue(g, 7, 0);
}

static void turn_step(Game *g, int two)
{
    uint16_t hl;
    switch (RD(STEP)) {
    case 0: /* $0550 */
        WR(0x6000, 0);
        WR(0x6001, 0);
        fill(g, 0x4100, 0, 0x80);
        WR(FRAME, 0);
        hl = fill(g, 0x4200, 0, 0x17);
        fill(g, (uint16_t)(hl + 1), 0, 0x18);
        fill(g, 0x4260, 0, 0x46);
        WR(0x4226, 1);
        inc(g, STEP);
        WR(0x4009, 0x20);
        game_wr16(g, 0x400B, 0x5000);
        return;
    case 1: /* $0583: a row a frame */
        hl = fill(g, game_rd16(g, 0x400B), 0x10, 0x20);
        game_wr16(g, 0x400B, hl);
        WR(0x4009, RD(0x4009) - 1);
        if (RD(0x4009)) return;
        next_with_colours(g);
        return;
    case 2: { /* $05A5 / $0795: the player's swarm */
        uint16_t de = util_unpack_grid(g, two ? 0x41A0 : 0x4180);
        game_copy(g, 0x4218, de, 8);
        WR(FRAME, 0);
        WR(0x4220, 0);
        if (!two) {
            WR(0x7006, 0);
            WR(0x7007, 0);
            WR(0x4018, 0);
        } else if (RD(0x400F)) { /* the cocktail table turns for player two */
            uint8_t a = RD(0x400F);
            WR(0x4018, a);
            WR(0x7006, a);
            WR(0x7007, a);
        }
        inc(g, STEP);
        WR(0x4009, 0x96);
        game_wr16(g, 0x4245, two ? 0x0830 : 0x0640);
        if (!(RD(0x4006) & 1)) return;
        turn_texts(g, two);
        return;
    }
    case 3: /* $0605 */
        WR(0x4009, RD(0x4009) - 1);
        if (RD(0x4009)) return;
        WR(0x4009, 0x14);
        inc(g, STEP);
        util_queue(g, 6, 0x82);
        return;
    case 4: ship_appears(g); return;
    case 5: flow_play(g); return;
    case 6: turn_over(g, two); return;
    case 7: pass(g, two); return;
    default: game_unported(g, two ? 0x0784 : 0x053F); return;
    }
}

/* ---- state 0 --------------------------------------------------------------------- */

static void state0(Game *g)
{
    uint16_t hl = fill(g, game_rd16(g, 0x400B), 0x10, 0x20);
    game_wr16(g, 0x400B, hl);
    WR(0x4008, RD(0x4008) - 1);
    if (RD(0x4008)) return;
    WR(0x4007, 1);
    WR(0x4006, 0);
    WR(STATE, 1);
    WR(STEP, 0);
    uint8_t a = RD(0x4011);
    WR(0x4000, (uint8_t)(a << 2 | a >> 6) & 3);
    WR(0x401F, (RD(0x4012) & 4) >> 2);
    util_unpack_grid(g, 0x051B);
    WR(0x400F, (RD(0x4010) & 0x20) >> 5);
    WR(0x40AC, RD(0x0152 + (RD(0x7000) & 3)));
    util_colours(g, 0x1D71);
    WR(0x5340, 1);
    WR(0x5320, 0x25);
    WR(0x5300, 0x20);
    util_queue(g, 6, 4);
    util_queue(g, 5, 3);
}

void flow_state(Game *g)
{
    switch (RD(STATE)) {
    case 0: state0(g); return;
    case 1:
        swarm_sway(g);
        swarm_survey(g);
        attract_step(g);
        credit_check(g);
        return;
    case 2:
        swarm_sway(g);
        swarm_survey(g);
        start_step(g);
        start_check(g);
        return;
    case 3:
    case 4:
        swarm_sway(g);
        swarm_survey(g);
        turn_step(g, RD(STATE) == 4);
        return;
    default: game_unported(g, 0x00CD); return;
    }
}
