/*
 * ROM routines against the C ones from random states (label rom):
 *   test_routines <galaxian.zip|folder>
 * For each routine, cases built from random memory (the bytes the routine
 * reads given plausible values: a slot's state among its table's, positions
 * near the ship, the swarm's cells 0 or 1, ...): the ROM's runs on the
 * emulated Z80 from its entry until it returns (a sentinel return address),
 * the C one on a model with the same memory; then RAM (bar the stack page),
 * tile and object RAM and the latches must be equal. For the paths the
 * scripted play of `sim` rarely reaches (the escorts' turns, a wave's end,
 * the score table's parade, the tunes, every command). Cases where the ROM
 * runs away (no return in 2 million cycles) or the C reports unported code
 * are counted apart.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/rom.h"
#include "emu/board.h"
#include "sim/aliens.h"
#include "sim/attack.h"
#include "sim/flow.h"
#include "sim/game.h"
#include "sim/player.h"
#include "sim/screen.h"
#include "sim/shots.h"
#include "sim/sounds.h"
#include "sim/swarm.h"

#define CASES 20000
#define SENTINEL 0x2800 /* a return address in no ROM: the routine is done */

static RomSet rom;
static Board board;
static Game model;
static uint32_t seed = 12345;

static uint32_t rnd(void)
{
    seed = seed * 1103515245u + 12345u;
    return seed >> 8;
}
static uint8_t r8(void) { return (uint8_t)rnd(); }
static int chance(int n) { return (int)(rnd() % 100) < n; }

/* ---- the states the cases start from ------------------------------------------- */

static void random_ram(void)
{
    for (int i = 0; i < 0x3C0; i++) board.ram[i] = r8();
    for (int i = 0; i < 0x400; i++) board.video.tiles[i] = r8();
    for (int i = 0; i < 0x100; i++) board.video.objects[i] = r8();
    for (int i = 0; i < 8; i++) {
        board.latch6000[i] = r8() & 1;
        board.latch6800[i] = r8() & 1;
        board.latch7000[i] = r8() & 1;
    }
    board.pitch = r8();
    board.in0 = r8() & 0x1F, board.in1 = r8() & 0x1F, board.in2 = r8() & 0x0F;
}

/* the swarm: cells 0 or 1 */
static void random_swarm(int density)
{
    for (int i = 0; i < 0x80; i++) board.ram[0x100 + i] = chance(density);
}

/* flags: bytes 0 or 1 */
static void flags(const uint16_t *addrs, int n, int p)
{
    for (int i = 0; i < n; i++) board.ram[addrs[i] - 0x4000] = chance(p);
}

static void random_slots(int max_state)
{
    for (int s = 0; s < 8; s++) {
        uint8_t *slot = board.ram + 0x2B0 + s * 0x20;
        slot[0] = chance(60);
        slot[1] = chance(20);
        slot[2] = slot[1] ? (uint8_t)(rnd() % 4) : (uint8_t)(rnd() % (unsigned)max_state);
        slot[3] = r8();
        slot[4] = r8();
        slot[5] = (uint8_t)((int)(rnd() % 24) - 12);
        slot[6] = r8() & 1;
        slot[7] = (uint8_t)(((rnd() % 6) + 2) << 4 | ((rnd() % 10) + 3));
        slot[0x10] = (uint8_t)(rnd() % 6 + 1);
        slot[0x11] = (uint8_t)(rnd() % 13 + 1);
        slot[0x13] = (uint8_t)(rnd() % 0x60) & 0xFE;
        slot[0x17] = (uint8_t)(rnd() % 7);
    }
}

static void random_shells(void)
{
    for (int i = 0; i < 14; i++) {
        uint8_t *sh = board.ram + 0x260 + i * 5;
        sh[0] = chance(50);
        sh[1] = (uint8_t)(0xA0 + rnd() % 0x60);
        sh[3] = (uint8_t)(board.ram[0x202] + (int)(rnd() % 32) - 16);
    }
}

static void play_state(void)
{
    random_ram();
    random_swarm(60);
    random_slots(16);
    random_shells();
    static const uint16_t f[] = { 0x4200, 0x4201, 0x4204, 0x4208, 0x420B, 0x4220, 0x4221, 0x4224, 0x4225, 0x4226,
                                  0x4228, 0x4229, 0x422B, 0x422E, 0x4222, 0x4006, 0x4007, 0x4018, 0x41EF, 0x4238 };
    flags(f, (int)(sizeof f / sizeof f[0]), 50);
    board.ram[0x202] = (uint8_t)(0x17 + rnd() % 0xD2);
    board.ram[0x209] = r8(), board.ram[0x20A] = r8();
    board.ram[0x213] = (uint8_t)(rnd() % 4 + 1);
    board.ram[0x21A] = (uint8_t)(rnd() % 9), board.ram[0x21B] = (uint8_t)(rnd() % 9);
    board.ram[0x21D] = (uint8_t)(rnd() % 7);
    board.ram[0x22A] = (uint8_t)(rnd() % 3);
    board.ram[0x20D] = r8() & 1;
    board.ram[0x20E] = (uint8_t)((int)(rnd() % 48) - 24), board.ram[0x20F] = board.ram[0x20E] & 0x80 ? 0xFF : 0;
    board.ram[0x210] = 0x22 + (uint8_t)(rnd() % 0x50), board.ram[0x211] = (uint8_t)(0xE0 - rnd() % 0x50);
    board.ram[0x0A0] = (uint8_t)(0xC0 + (rnd() % 32) * 2);
    for (int i = 0; i < 0x40; i++) board.ram[0xC0 + i] = chance(70) ? 0xFF : r8() & 7;
}

static void sound_state(void)
{
    random_ram();
    static const uint16_t f[] = { 0x41C2, 0x41C7, 0x41C9, 0x41CC, 0x41CD, 0x41CF, 0x41D0, 0x41D1, 0x41D2,
                                  0x4006, 0x4007, 0x4226 };
    flags(f, (int)(sizeof f / sizeof f[0]), 40);
    board.ram[0x1D6] = (uint8_t)(rnd() % 4 + 1);
    board.ram[0x1DF] = chance(30) ? (chance(50) ? 6 : 0x16) : (uint8_t)(rnd() % 20);
    static const uint16_t tunes[] = { 0x1E68, 0x1EBD, 0x1EDF };
    uint16_t t = (uint16_t)(tunes[rnd() % 3] + rnd() % 16);
    board.ram[0x1D3] = (uint8_t)t, board.ram[0x1D4] = (uint8_t)(t >> 8);
}

static void screen_state(void)
{
    random_ram();
    random_swarm(60);
    board.ram[0x00D] = r8() & 1, board.ram[0x00E] = r8() & 1;
    board.ram[0x007] = r8() & 1, board.ram[0x006] = r8() & 1;
    board.ram[0x0AC] = chance(10) ? 0xFF : (uint8_t)(rnd() % 0x30);
    board.ram[0x002] = (uint8_t)(rnd() % 0x70);
    board.ram[0x21C] = (uint8_t)(rnd() % 0x40);
    board.ram[0x21D] = (uint8_t)(rnd() % 8);
    for (int i = 0; i < 9; i++) board.ram[0x0A2 + i] = (uint8_t)((rnd() % 10) << 4 | (rnd() % 10)); /* BCD scores */
}

static void flow_state_random(void)
{
    play_state();
    static const uint8_t steps[5] = { 1, 19, 4, 8, 8 };
    uint8_t s = (uint8_t)(rnd() % 5);
    board.ram[0x005] = s;
    board.ram[0x00A] = (uint8_t)(rnd() % steps[s]);
    board.ram[0x008] = (uint8_t)(rnd() % 3 + 1), board.ram[0x009] = (uint8_t)(rnd() % 3 + 1);
    board.ram[0x00B] = 0x00, board.ram[0x00C] = 0x50 + (uint8_t)(rnd() % 3);
    board.ram[0x002] = (uint8_t)(rnd() % 3);
    board.ram[0x241] = (uint8_t)(rnd() % 6);
    board.ram[0x019] = (uint8_t)(rnd() % 3);
    board.ram[0x195] = (uint8_t)(rnd() % 3), board.ram[0x1B5] = (uint8_t)(rnd() % 3);
}

/* ---- running both --------------------------------------------------------------- */

typedef struct {
    const char *name;
    uint16_t entry;
    void (*state)(void);
    void (*c)(Game *g, uint8_t a);
    int arg; /* A's values: 0 any, else up to this */
    long checked, failed, skipped;
} Case;

static void c_aliens(Game *g, uint8_t a) { aliens_step(g); }
static void c_sprites(Game *g, uint8_t a) { aliens_sprites(g); }
static void c_lone(Game *g, uint8_t a) { attack_lone(g); }
static void c_flagship(Game *g, uint8_t a) { attack_flagship(g); }
static void c_timers(Game *g, uint8_t a) { attack_lone_timers(g); }
static void c_ftimer(Game *g, uint8_t a) { attack_flagship_timer(g); }
static void c_next_wave(Game *g, uint8_t a) { attack_next_wave(g); }
static void c_hit_swarm(Game *g, uint8_t a) { shots_hit_swarm(g); }
static void c_hit_ship(Game *g, uint8_t a) { shots_hit_ship(g); }
static void c_hit_flying(Game *g, uint8_t a) { shots_hit_flying(g); }
static void c_crash(Game *g, uint8_t a) { shots_crash(g); }
static void c_shells(Game *g, uint8_t a) { shots_move_shells(g); }
static void c_sway(Game *g, uint8_t a) { swarm_sway(g); }
static void c_survey(Game *g, uint8_t a) { swarm_survey(g); }
static void c_sounds(Game *g, uint8_t a) { sounds_frame(g); }
static void c_swarm_sound(Game *g, uint8_t a) { sounds_swarm(g); }
static void c_player(Game *g, uint8_t a) { player_move(g); }
static void c_idle(Game *g, uint8_t a) { screen_idle(g); }
static void c_cmd(Game *g, uint8_t a, int cmd) { screen_command(g, (uint8_t)cmd, a); }
static void c_cmd0(Game *g, uint8_t a) { c_cmd(g, a, 0); }
static void c_cmd1(Game *g, uint8_t a) { c_cmd(g, a, 1); }
static void c_cmd2(Game *g, uint8_t a) { c_cmd(g, a, 2); }
static void c_cmd3(Game *g, uint8_t a) { c_cmd(g, a, 3); }
static void c_cmd4(Game *g, uint8_t a) { c_cmd(g, a, 4); }
static void c_cmd5(Game *g, uint8_t a) { c_cmd(g, a, 5); }
static void c_cmd6(Game *g, uint8_t a) { c_cmd(g, a, 6); }
static void c_cmd7(Game *g, uint8_t a) { c_cmd(g, a, 7); }
static void c_state(Game *g, uint8_t a) { flow_state(g); }

static Case cases[] = {
    { "aliens step", 0x0CC3, play_state, c_aliens, 0, 0, 0, 0 },
    { "alien sprites", 0x0BBE, play_state, c_sprites, 0, 0, 0, 0 },
    { "lone attack", 0x1344, play_state, c_lone, 0, 0, 0, 0 },
    { "flagship attack", 0x140C, play_state, c_flagship, 0, 0, 0, 0 },
    { "lone timers", 0x1515, play_state, c_timers, 0, 0, 0, 0 },
    { "flagship timer", 0x1555, play_state, c_ftimer, 0, 0, 0, 0 },
    { "next wave", 0x1637, play_state, c_next_wave, 0, 0, 0, 0 },
    { "missile hits swarm", 0x0B0B, play_state, c_hit_swarm, 0, 0, 0, 0 },
    { "shells hit ship", 0x0B77, play_state, c_hit_ship, 0, 0, 0, 0 },
    { "missile hits flying", 0x1227, play_state, c_hit_flying, 0, 0, 0, 0 },
    { "flying hits ship", 0x129E, play_state, c_crash, 0, 0, 0, 0 },
    { "move shells", 0x0A74, play_state, c_shells, 0, 0, 0, 0 },
    { "swarm sway", 0x090D, play_state, c_sway, 0, 0, 0, 0 },
    { "swarm survey", 0x098E, play_state, c_survey, 0, 0, 0, 0 },
    { "ship moves", 0x0837, play_state, c_player, 0, 0, 0, 0 },
    { "sounds", 0x16F5, sound_state, c_sounds, 0, 0, 0, 0 },
    { "swarm sound", 0x16B8, play_state, c_swarm_sound, 0, 0, 0, 0 },
    { "swarm beats", 0x2067, screen_state, c_idle, 0, 0, 0, 0 },
    { "draw alien", 0x2055, screen_state, c_cmd0, 0x80, 0, 0, 0 },
    { "erase alien", 0x205E, screen_state, c_cmd1, 0x80, 0, 0, 0 },
    { "ship frame", 0x215F, screen_state, c_cmd2, 6, 0, 0, 0 },
    { "add score", 0x21A6, screen_state, c_cmd3, 11, 0, 0, 0 },
    { "clear score", 0x21FE, screen_state, c_cmd4, 5, 0, 0, 0 },
    { "draw score", 0x2231, screen_state, c_cmd5, 5, 0, 0, 0 },
    { "text", 0x22F1, screen_state, c_cmd6, 0x11, 0, 0, 0 },
    { "flags, credits", 0x24B7, screen_state, c_cmd7, 5, 0, 0, 0 },
    { "a state's frame", 0x00C6, flow_state_random, c_state, 0, 0, 0, 0 },
};
#define CASE_TYPES ((int)(sizeof cases / sizeof cases[0]))

static void load_model(Game *g)
{
    memset(g, 0, sizeof *g);
    memcpy(g->ram, board.ram, sizeof g->ram);
    memcpy(g->tiles, board.video.tiles, sizeof g->tiles);
    memcpy(g->objects, board.video.objects, sizeof g->objects);
    memcpy(g->latch6000, board.latch6000, 8);
    memcpy(g->latch6800, board.latch6800, 8);
    memcpy(g->latch7000, board.latch7000, 8);
    g->pitch = board.pitch;
    g->in0 = board.in0, g->in1 = board.in1, g->in2 = board.in2;
    g->data = rom.program;
}

static long compare(const Case *c, long n)
{
    long diff = 0;
#define DIFF(what, addr, ours, theirs)                                                                     \
    do {                                                                                                   \
        if ((ours) != (theirs) && diff++ < 6)                                                              \
            fprintf(stderr, "%s, case %ld: %s $%04X: %02X, the ROM %02X\n", c->name, n, what, addr, ours, theirs); \
    } while (0)
    for (unsigned a = 0; a < 0x3C0; a++) DIFF("RAM", 0x4000 + a, model.ram[a], board.ram[a]);
    for (unsigned a = 0; a < 0x400; a++) DIFF("tiles", 0x5000 + a, model.tiles[a], board.video.tiles[a]);
    for (unsigned a = 0; a < 0x100; a++) DIFF("objects", 0x5800 + a, model.objects[a], board.video.objects[a]);
    for (unsigned i = 0; i < 8; i++) {
        DIFF("latch", 0x6000 + i, model.latch6000[i], board.latch6000[i]);
        DIFF("latch", 0x6800 + i, model.latch6800[i], board.latch6800[i]);
        DIFF("latch", 0x7000 + i, model.latch7000[i], board.latch7000[i]);
    }
    DIFF("latch", 0x7800, model.pitch, board.pitch);
    return diff;
}

/* The ROM's routine from its entry, A = a, until it returns to the sentinel. */
static int run_rom(uint16_t entry, uint8_t a)
{
    Z80 *c = &board.cpu;
    c->sp = 0x4400;
    c->sp -= 2;
    board.ram[0x3FE] = SENTINEL & 0xFF, board.ram[0x3FF] = SENTINEL >> 8;
    if (entry == 0x00C6) { /* a state's frame: the NMI pushes its tail ($00D8) and jumps through RST $28 */
        c->sp = 0x43FE;
        c->pc = 0x00CA;
    } else {
        c->pc = entry;
    }
    c->a = a, c->e = a;
    c->iff1 = c->iff2 = 0;
    c->nmi_pending = 0, c->nmi_line = 0;
    int64_t limit = c->cycles + 2000000;
    while (c->pc != SENTINEL) {
        if (c->pc == 0x00D8 && entry == 0x00C6) return 0; /* the state's frame is done */
        if (c->cycles > limit || c->pc == 0x0000) return -1;
        z80_step(c);
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: test_routines <galaxian.zip|folder>\n");
        return 2;
    }
    char err[512];
    if (rom_load(&rom, argv[1], err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    board_init(&board, &rom);
    board.no_video = 1;
    int failed = 0;
    for (int t = 0; t < CASE_TYPES; t++) {
        Case *c = &cases[t];
        for (long n = 0; n < CASES; n++) {
            c->state();
            uint8_t a = c->arg ? (uint8_t)(rnd() % (unsigned)c->arg) : r8();
            if (c->entry == 0x2055 || c->entry == 0x205E) a = (uint8_t)(((rnd() % 6) + 2) << 4 | (rnd() % 16));
            load_model(&model);
            c->c(&model, a);
            if (run_rom(c->entry, a) || model.unported) {
                c->skipped++;
                continue;
            }
            if (compare(c, n)) {
                c->failed++;
                failed = 1;
            } else {
                c->checked++;
            }
        }
        printf("%-20s $%04X: %5ld cases equal, %ld differ, %ld skipped\n", c->name, c->entry, c->checked, c->failed,
               c->skipped);
    }
    return failed;
}
