/*
 * The C game's routines against the ROM's on the emulated board (label rom):
 *   test_sim <galaxian.zip|folder>
 * Plays scripted games on the board (the attract mode, coins, one- and
 * two-player games to their end) and checks every ported routine each time
 * the ROM runs it: when the CPU enters the routine (by a CALL or RST, or by
 * a JP from outside it: a tail call), a model loaded from the board's
 * memory, latches and switches runs the C routine with the CPU's
 * registers; when the ROM's routine returns to its caller, the model must
 * equal the board (RAM bar the stack below the entry SP, tile and object
 * RAM, the latches) and the routine's output registers the CPU's. Calls an
 * NMI breaks into (the main loop's) are not checked; calls a C routine
 * reports reaching unported code are counted apart.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/rom.h"
#include "emu/board.h"
#include "sim/aliens.h"
#include "sim/attack.h"
#include "sim/coins.h"
#include "sim/demo.h"
#include "sim/flow.h"
#include "sim/frame.h"
#include "sim/game.h"
#include "sim/player.h"
#include "sim/screen.h"
#include "sim/shots.h"
#include "sim/sounds.h"
#include "sim/text.h"
#include "sim/swarm.h"
#include "sim/util.h"

#define FRAMES 60000

enum { OUT_A = 1, OUT_B = 2, OUT_C = 4, OUT_D = 8, OUT_E = 16, OUT_H = 32, OUT_L = 64 };
#define OUT_DE (OUT_D | OUT_E)
#define OUT_HL (OUT_H | OUT_L)

typedef void (*Adapter)(Game *g, Regs *r);
typedef struct {
    uint16_t addr;
    const char *name;
    Adapter fn;
    unsigned outputs;
    long checked, interrupted, unported, failed;
} Routine;

/* ---- the routines, through their registers ----------------------------------- */

static void a_random(Game *g, Regs *r) { r->b = game_rd(g, 0x401E), r->a = util_random(g); }
static void a_divide(Game *g, Regs *r) { util_divide(r); }
static void a_queue(Game *g, Regs *r) { util_queue(g, r->d, r->e); }
static void a_unpack(Game *g, Regs *r) { REGS_SET_DE(r, util_unpack_grid(g, REGS_DE(r))); }
static void a_pack(Game *g, Regs *r) { REGS_SET_DE(r, util_pack_grid(g, REGS_DE(r))); }
static void a_colours(Game *g, Regs *r) { util_colours(g, REGS_HL(r)); }
static void a_colours_start(Game *g, Regs *r) { util_colours(g, 0x1D71); }
static void a_scroll(Game *g, Regs *r) { swarm_scroll(g, r->a); }
static void a_scroll_home(Game *g, Regs *r) { swarm_scroll(g, 0); }
static void a_sway(Game *g, Regs *r) { swarm_sway(g); }
static void a_survey(Game *g, Regs *r) { swarm_survey(g); }

static void a_fire(Game *g, Regs *r) { shots_fire(g); }
static void a_move_shells(Game *g, Regs *r) { shots_move_shells(g); }
static void a_hit_swarm(Game *g, Regs *r) { shots_hit_swarm(g); }
static void a_hit_ship(Game *g, Regs *r) { shots_hit_ship(g); }
static void a_alien_sprites(Game *g, Regs *r) { aliens_sprites(g); }
static void a_aliens_step(Game *g, Regs *r) { aliens_step(g); }
static void a_home(Game *g, Regs *r) { aliens_home(g, r->ix); }
static void a_curve(Game *g, Regs *r) { aliens_curve(g, r->ix); }
static void a_aim(Game *g, Regs *r) { aliens_aim(g, r->ix); }
static void a_shoot(Game *g, Regs *r) { aliens_shoot(g, r->ix); }

static void a_hit_flying(Game *g, Regs *r) { shots_hit_flying(g); }
static void a_crash(Game *g, Regs *r) { shots_crash(g); }
static void a_kill_flying(Game *g, Regs *r) { shots_kill_flying(g, r->ix); }
static void a_player_hit(Game *g, Regs *r) { player_hit(g); }
static void a_player_explode(Game *g, Regs *r) { player_explode(g); }
static void a_lone(Game *g, Regs *r) { attack_lone(g); }
static void a_side(Game *g, Regs *r) { attack_side(g); }
static void a_flagship(Game *g, Regs *r) { attack_flagship(g); }
static void a_rise(Game *g, Regs *r) { attack_rise(g); }
static void a_lone_timers(Game *g, Regs *r) { attack_lone_timers(g); }
static void a_flagship_timer(Game *g, Regs *r) { attack_flagship_timer(g); }
static void a_flagship_go(Game *g, Regs *r) { attack_flagship_go(g); }
static void a_shot_places(Game *g, Regs *r) { attack_shot_places(g); }
static void a_wave_over(Game *g, Regs *r) { attack_wave_over(g); }
static void a_next_wave(Game *g, Regs *r) { attack_next_wave(g); }
static void a_flagship_wait(Game *g, Regs *r) { attack_flagship_wait(g); }
static void a_6804(Game *g, Regs *r) { sounds_6804(g); }
static void a_swarm_sound(Game *g, Regs *r) { sounds_swarm(g); }

static void a_sounds(Game *g, Regs *r) { sounds_frame(g); }
static void a_lfo(Game *g, Regs *r) { sounds_lfo(g); }
static void a_coins_in(Game *g, Regs *r) { coins_in(g); }
static void a_coins_credit(Game *g, Regs *r) { coins_credit(g); }
static void a_lockout(Game *g, Regs *r) { coins_lockout(g); }
static void a_type(Game *g, Regs *r) { text_type(g); }
static void a_pilot(Game *g, Regs *r) { demo_pilot(g); }

static void a_player_move(Game *g, Regs *r) { player_move(g); }
static void a_missile(Game *g, Regs *r) { shots_missile(g); }
static void a_missile_done(Game *g, Regs *r) { shots_missile_done(g); }

static void a_cmd0(Game *g, Regs *r) { screen_command(g, 0, r->a); }
static void a_cmd1(Game *g, Regs *r) { screen_command(g, 1, r->a); }
static void a_cmd2(Game *g, Regs *r) { screen_command(g, 2, r->a); }
static void a_cmd3(Game *g, Regs *r) { screen_command(g, 3, r->a); }
static void a_cmd4(Game *g, Regs *r) { screen_command(g, 4, r->a); }
static void a_cmd5(Game *g, Regs *r) { screen_command(g, 5, r->a); }
static void a_cmd6(Game *g, Regs *r) { screen_command(g, 6, r->a); }
static void a_cmd7(Game *g, Regs *r) { screen_command(g, 7, r->a); }
static void a_idle(Game *g, Regs *r) { screen_idle(g); }
static void a_ships(Game *g, Regs *r) { screen_ships(g, r->b); }

static void a_nmi(Game *g, Regs *r) { frame_nmi(g); }
static void a_state(Game *g, Regs *r) { flow_state(g); }
static void a_play(Game *g, Regs *r) { flow_play(g); }
static void a_quiet(Game *g, Regs *r) { flow_quiet(g); }

static Routine routines[] = {
    { 0x0066, "NMI", a_nmi, 0, 0, 0, 0, 0 },
    { 0x00E6, "state 0", a_state, 0, 0, 0, 0, 0 },
    { 0x0156, "state 1 (attract)", a_state, 0, 0, 0, 0, 0 },
    { 0x03F2, "state 2 (start)", a_state, 0, 0, 0, 0, 0 },
    { 0x0536, "state 3 (player one)", a_state, 0, 0, 0, 0, 0 },
    { 0x077B, "state 4 (player two)", a_state, 0, 0, 0, 0, 0 },
    { 0x0661, "play", a_play, 0, 0, 0, 0, 0 },
    { 0x1CB5, "quiet", a_quiet, 0, 0, 0, 0, 0 },
    { 0x003C, "random", a_random, OUT_A | OUT_B, 0, 0, 0, 0 },
    { 0x0048, "divide", a_divide, OUT_A | OUT_B | OUT_C | OUT_D, 0, 0, 0, 0 },
    { 0x08F2, "queue", a_queue, 0, 0, 0, 0, 0 },
    { 0x0646, "unpack grid", a_unpack, OUT_DE, 0, 0, 0, 0 },
    { 0x0764, "pack grid", a_pack, OUT_DE, 0, 0, 0, 0 },
    { 0x0598, "colours", a_colours, 0, 0, 0, 0, 0 },
    { 0x0595, "colours (start)", a_colours_start, 0, 0, 0, 0, 0 },
    { 0x0972, "swarm scroll", a_scroll, 0, 0, 0, 0, 0 },
    { 0x0363, "swarm scroll home", a_scroll_home, 0, 0, 0, 0, 0 },
    { 0x090D, "swarm sway", a_sway, 0, 0, 0, 0, 0 },
    { 0x098E, "swarm survey", a_survey, 0, 0, 0, 0, 0 },
    { 0x0837, "ship moves", a_player_move, 0, 0, 0, 0, 0 },
    { 0x0898, "missile", a_missile, 0, 0, 0, 0, 0 },
    { 0x08E5, "missile done", a_missile_done, 0, 0, 0, 0, 0 },
    { 0x0A32, "fire", a_fire, 0, 0, 0, 0, 0 },
    { 0x0A74, "move shells", a_move_shells, 0, 0, 0, 0, 0 },
    { 0x0B0B, "missile hits swarm", a_hit_swarm, 0, 0, 0, 0, 0 },
    { 0x0B77, "shells hit ship", a_hit_ship, 0, 0, 0, 0, 0 },
    { 0x0BBE, "alien sprites", a_alien_sprites, 0, 0, 0, 0, 0 },
    { 0x0CC3, "aliens step", a_aliens_step, 0, 0, 0, 0, 0 },
    { 0x1147, "alien home", a_home, 0, 0, 0, 0, 0 },
    { 0x116B, "alien curve", a_curve, 0, 0, 0, 0, 0 },
    { 0x11B0, "alien aim", a_aim, 0, 0, 0, 0, 0 },
    { 0x11E0, "alien shoots", a_shoot, 0, 0, 0, 0, 0 },
    { 0x1227, "missile hits flying", a_hit_flying, 0, 0, 0, 0, 0 },
    { 0x129E, "flying hits ship", a_crash, 0, 0, 0, 0, 0 },
    { 0x125E, "flying alien killed", a_kill_flying, 0, 0, 0, 0, 0 },
    { 0x12ED, "ship hit", a_player_hit, 0, 0, 0, 0, 0 },
    { 0x1327, "ship explodes", a_player_explode, 0, 0, 0, 0, 0 },
    { 0x1344, "lone attack", a_lone, 0, 0, 0, 0, 0 },
    { 0x13E1, "attack side", a_side, 0, 0, 0, 0, 0 },
    { 0x140C, "flagship attack", a_flagship, 0, 0, 0, 0, 0 },
    { 0x14F3, "aggression", a_rise, 0, 0, 0, 0, 0 },
    { 0x1515, "lone timers", a_lone_timers, 0, 0, 0, 0, 0 },
    { 0x1555, "flagship timer", a_flagship_timer, 0, 0, 0, 0, 0 },
    { 0x15C3, "flagship go", a_flagship_go, 0, 0, 0, 0, 0 },
    { 0x15F4, "shot places", a_shot_places, 0, 0, 0, 0, 0 },
    { 0x1621, "wave over", a_wave_over, 0, 0, 0, 0, 0 },
    { 0x1637, "next wave", a_next_wave, 0, 0, 0, 0, 0 },
    { 0x1688, "flagship wait", a_flagship_wait, 0, 0, 0, 0, 0 },
    { 0x16A6, "sound 6804", a_6804, 0, 0, 0, 0, 0 },
    { 0x16B8, "swarm sound", a_swarm_sound, 0, 0, 0, 0, 0 },
    { 0x16F5, "sounds", a_sounds, 0, 0, 0, 0, 0 },
    { 0x1898, "LFO", a_lfo, 0, 0, 0, 0, 0 },
    { 0x18C0, "typewriter", a_type, 0, 0, 0, 0, 0 },
    { 0x18EF, "coins in", a_coins_in, 0, 0, 0, 0, 0 },
    { 0x1931, "credits", a_coins_credit, 0, 0, 0, 0, 0 },
    { 0x197C, "coin lockout", a_lockout, 0, 0, 0, 0, 0 },
    { 0x198E, "demo pilot", a_pilot, 0, 0, 0, 0, 0 },
    { 0x2055, "draw alien", a_cmd0, 0, 0, 0, 0, 0 },
    { 0x205E, "erase alien", a_cmd1, 0, 0, 0, 0, 0 },
    { 0x2067, "swarm beats", a_idle, 0, 0, 0, 0, 0 },
    { 0x215F, "ship frame", a_cmd2, 0, 0, 0, 0, 0 },
    { 0x21A6, "add score", a_cmd3, 0, 0, 0, 0, 0 },
    { 0x21FE, "clear score", a_cmd4, 0, 0, 0, 0, 0 },
    { 0x2231, "draw score", a_cmd5, 0, 0, 0, 0, 0 },
    { 0x22B3, "ships left", a_ships, 0, 0, 0, 0, 0 },
    { 0x22F1, "text", a_cmd6, 0, 0, 0, 0, 0 },
    { 0x24B7, "flags, credits", a_cmd7, 0, 0, 0, 0, 0 },
};
#define ROUTINES ((int)(sizeof routines / sizeof routines[0]))

/* ---- the board ----------------------------------------------------------------- */

static RomSet rom;
static Board board;

/* The scripted runs: the controls by frame (the attract mode first, then
   cycles of coins and a start, one player then two, playing with every
   control), or an autopilot that hunts the aliens and dodges the shells,
   with its ships topped up ($421D) until near the end, to reach the later
   waves. */
static int autopilot;
static long top_up_until;
static int npending;
static void interrupt_pending(void);

static void pilot(Board *b)
{
    const uint8_t *ram = b->ram;
    int x = ram[0x202], target = -1, best_y = -1;
    for (int s = 0; s < 8; s++) { /* the lowest diving alien */
        const uint8_t *slot = ram + 0x2B0 + s * 0x20;
        if ((slot[0] & 1) && slot[3] > best_y && slot[3] < 0xD0) best_y = slot[3], target = slot[4];
    }
    if (target < 0) /* else a column of the swarm */
        for (int c = 0; c < 10 && target < 0; c++) {
            int col = (int)((b->frame / 600 + c) % 10);
            for (int r = 0; r < 6; r++)
                if (ram[0x123 + r * 16 + col] & 1) target = ((col + 3) << 4) + 7 + ram[0x20E];
        }
    int dodge = 0;
    for (int i = 0; i < 14; i++) { /* a shell close above */
        const uint8_t *sh = ram + 0x260 + i * 5;
        if ((sh[0] & 1) && sh[1] > 0x98 && sh[1] < 0xE0 && abs(sh[3] - x) < 14) dodge = sh[3] >= x ? -1 : 1;
    }
    int dir = dodge ? dodge : target < 0 ? 0 : (target & 0xFF) > x + 2 ? 1 : (target & 0xFF) < x - 2 ? -1 : 0;
    if (dir > 0) b->in0 |= IN0_LEFT; /* left adds to x */
    if (dir < 0) b->in0 |= IN0_RIGHT;
    if (b->frame & 1) b->in0 |= IN0_FIRE;
    if (b->frame < top_up_until && ram[0x21D] < 3) {
        b->ram[0x21D] = 3;
        interrupt_pending(); /* changed under the calls in flight */
    }
}

static void controls(Board *b, int line)
{
    if (line != BOARD_VBLANK_LINE) return;
    long f = b->frame, p = f % 20000;
    b->in0 = b->in1 = 0;
    if (f < 3000) return;
    if ((p >= 300 && p < 310) || (p >= 330 && p < 340) || (p >= 360 && p < 370)) b->in0 |= IN0_COIN1;
    if (p >= 400 && p < 410) b->in1 |= (f / 20000) & 1 ? IN1_START2 : IN1_START1;
    if (autopilot) {
        pilot(b);
        return;
    }
    int m = (int)((f / 23) % 5);
    if (m == 1) b->in0 |= IN0_LEFT;
    if (m == 3) b->in0 |= IN0_RIGHT;
    if ((f / 7) % 3 == 0) b->in0 |= IN0_FIRE;
}

static void load_model(Game *g, const Board *b)
{
    memset(g, 0, sizeof *g);
    memcpy(g->ram, b->ram, sizeof g->ram);
    memcpy(g->tiles, b->video.tiles, sizeof g->tiles);
    memcpy(g->objects, b->video.objects, sizeof g->objects);
    memcpy(g->latch6000, b->latch6000, 8);
    memcpy(g->latch6800, b->latch6800, 8);
    memcpy(g->latch7000, b->latch7000, 8);
    g->pitch = b->pitch;
    g->in0 = b->in0, g->in1 = b->in1, g->in2 = b->in2;
    g->data = rom.program;
}

static void cpu_regs(const Z80 *c, Regs *r)
{
    r->a = c->a, r->f = c->f, r->b = c->b, r->c = c->c, r->d = c->d, r->e = c->e, r->h = c->h, r->l = c->l;
    r->ix = (uint16_t)(c->ixh << 8 | c->ixl), r->iy = (uint16_t)(c->iyh << 8 | c->iyl);
}

/* ---- calls in flight ------------------------------------------------------------- */

typedef struct {
    Routine *rt;
    uint16_t ret, sp;
    int interrupted;
    Game model;
    Regs regs;
} Pending;

static Pending pending[32];

static void interrupt_pending(void)
{
    for (int i = 0; i < npending; i++) pending[i].interrupted = 1;
}
static uint16_t prev_pc, prev_sp;
static long shown;

static int compare(Pending *p)
{
    Game *g = &p->model;
    Regs r = p->regs;
    p->rt->fn(g, &r);
    if (g->unported) {
        p->rt->unported++;
        return 0;
    }
    int bad = 0;
    char where[96];
    snprintf(where, sizeof where, "%s ($%04X) at frame %ld", p->rt->name, p->rt->addr, (long)board.frame);
#define DIFF(what, addr, ours, rom)                                                                      \
    do {                                                                                                  \
        if ((ours) != (rom)) {                                                                            \
            if (shown++ < 40) fprintf(stderr, "%s: %s $%04X: %02X, the ROM %02X\n", where, what, addr, ours, rom); \
            bad = 1;                                                                                      \
        }                                                                                                 \
    } while (0)
    unsigned stack_lo = 0x43C0, stack_hi = p->sp; /* the stack below the entry SP: the ROM's scratch */
    for (unsigned a = 0; a < 0x400; a++)
        if (a + 0x4000 < stack_lo || a + 0x4000 >= stack_hi) DIFF("RAM", a + 0x4000, g->ram[a], board.ram[a]);
    for (unsigned a = 0; a < 0x400; a++) DIFF("tiles", a + 0x5000, g->tiles[a], board.video.tiles[a]);
    for (unsigned a = 0; a < 0x100; a++) DIFF("objects", a + 0x5800, g->objects[a], board.video.objects[a]);
    for (unsigned i = 0; i < 8; i++) {
        DIFF("latch", 0x6000 + i, g->latch6000[i], board.latch6000[i]);
        DIFF("latch", 0x6800 + i, g->latch6800[i], board.latch6800[i]);
        DIFF("latch", 0x7000 + i, g->latch7000[i], board.latch7000[i]);
    }
    DIFF("latch", 0x7800, g->pitch, board.pitch);
    Regs cpu;
    cpu_regs(&board.cpu, &cpu);
    unsigned o = p->rt->outputs;
    if (o & OUT_A) DIFF("register A", 0, r.a, cpu.a);
    if (o & OUT_B) DIFF("register B", 0, r.b, cpu.b);
    if (o & OUT_C) DIFF("register C", 0, r.c, cpu.c);
    if (o & OUT_D) DIFF("register D", 0, r.d, cpu.d);
    if (o & OUT_E) DIFF("register E", 0, r.e, cpu.e);
    if (o & OUT_H) DIFF("register H", 0, r.h, cpu.h);
    if (o & OUT_L) DIFF("register L", 0, r.l, cpu.l);
    if (bad) p->rt->failed++;
    else p->rt->checked++;
    return bad;
}

static uint8_t executed[0x2800]; /* COVER=file: the instructions run, for reading the ROM */

static void step(Board *b)
{
    Z80 *c = &b->cpu;
    if (c->pc < sizeof executed) executed[c->pc] = 1;
    /* returns: every call whose caller is back */
    while (npending && c->pc == pending[npending - 1].ret && c->sp == (uint16_t)(pending[npending - 1].sp + 2)) {
        Pending *p = &pending[--npending];
        if (p->interrupted) p->rt->interrupted++;
        else compare(p);
    }
    /* entries: a CALL or RST to a routine, or a JP from outside it */
    for (int i = 0; i < ROUTINES; i++) {
        Routine *rt = &routines[i];
        if (c->pc != rt->addr) continue;
        uint8_t op = board_peek(b, prev_pc);
        int called = c->sp == (uint16_t)(prev_sp - 2);
        int jumped = c->sp == prev_sp && (op == 0xC3 || (op & 0xC7) == 0xC2 || op == 0x18 || (op & 0xE7) == 0x20 || op == 0xE9) &&
                     (prev_pc < rt->addr || prev_pc >= rt->addr + 0x80);
        if (!called && !jumped) continue;
        if (npending == (int)(sizeof pending / sizeof pending[0])) break;
        Pending *p = &pending[npending++];
        p->rt = rt;
        p->sp = c->sp;
        p->ret = (uint16_t)(board_peek(b, c->sp) | board_peek(b, (uint16_t)(c->sp + 1)) << 8);
        p->interrupted = 0;
        load_model(&p->model, b);
        cpu_regs(c, &p->regs);
    }
    /* an NMI taken now breaks into every call in flight (a call just made included) */
    if (c->nmi_pending)
        for (int i = 0; i < npending; i++) pending[i].interrupted = 1;
    prev_pc = c->pc;
    prev_sp = c->sp;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: test_sim <galaxian.zip|folder>\n");
        return 2;
    }
    char err[512];
    if (rom_load(&rom, argv[1], err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    for (autopilot = 0; autopilot < 2; autopilot++) {
        board_init(&board, &rom);
        board.no_video = 1;
        board.line_hook = controls;
        board.step_hook = step;
        npending = 0;
        top_up_until = autopilot ? FRAMES - 8000 : 0;
        while (board.frame < FRAMES) board_run_frame(&board);
    }
    if (getenv("COVER")) {
        FILE *f = fopen(getenv("COVER"), "w");
        for (unsigned a = 0; f && a < sizeof executed; a++) fprintf(f, "%04X %c\n", a, executed[a] ? 'X' : '.');
        if (f) fclose(f);
    }
    int failed = 0;
    for (int i = 0; i < ROUTINES; i++) {
        Routine *rt = &routines[i];
        printf("%-24s $%04X: %7ld checked, %ld failed, %ld interrupted, %ld unported\n", rt->name, rt->addr, rt->checked,
               rt->failed, rt->interrupted, rt->unported);
        if (rt->failed || !rt->checked) failed = 1;
    }
    return failed;
}
