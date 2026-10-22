/*
 * The C game on its own against the board (label rom):
 *   test_frame <galaxian.zip|folder> [frames]
 * The C power-on (sim/boot.h) must equal the board when the ROM's set-up is
 * done (its first NMI due); from there each frame the C game (sim/frame.h:
 * the NMI's work, then the main loop's) runs with the switches the board
 * reads, and at the frame's end every byte of RAM (bar the stack,
 * $43C0-$43FF), tile and object RAM and the latches must be the board's.
 * The main loop races the NMI on the board: the work an NMI breaks into (a
 * pass of the swarm's animation, with the last frame's column, or a
 * command) finishes after it, over what the NMI did, which the C game,
 * doing a frame's main-loop work whole after the NMI, cannot know. As
 * Defender's frame test is given the board's band split, this test is
 * given that race: what the board's main loop writes between the NMI's
 * return and its dispatcher ($200A) is written to the C model after its
 * NMI. Tile RAM is compared by picture ($10, $2C-$2F, $40, $42 and $CA are
 * all blank; the game never reads tile RAM back).
 * The controls: the attract mode, coins, a one-player game played by an
 * autopilot to its end, a two-player game, as test_sim's runs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/rom.h"
#include "emu/board.h"
#include "sim/boot.h"
#include "sim/frame.h"
#include "sim/game.h"
#include "sim/screen.h"

static RomSet rom;
static Board board;
static Game cg;

/* ---- the race between the NMI and the main loop ------------------------------- */

typedef struct {
    uint8_t ram[0x400], tiles[0x400], objects[0x100];
} Memory;

static Memory after_nmi;          /* the board when its NMI returned */
static uint16_t back_pc, back_sp; /* where the NMI returns to */
static int resuming;              /* the NMI has returned; the main loop not yet at its dispatcher */
static struct { uint16_t addr; uint8_t value; } resumed[4096];
static int nresumed;

static void snapshot(Memory *m)
{
    memcpy(m->ram, board.ram, sizeof m->ram);
    memcpy(m->tiles, board.video.tiles, sizeof m->tiles);
    memcpy(m->objects, board.video.objects, sizeof m->objects);
}

static void record(uint16_t addr, uint8_t was, uint8_t now)
{
    if (was != now && nresumed < (int)(sizeof resumed / sizeof resumed[0])) {
        resumed[nresumed].addr = addr;
        resumed[nresumed].value = now;
        nresumed++;
    }
}

static void race(Board *b)
{
    Z80 *c = &b->cpu;
    if (c->nmi_pending) { /* the NMI comes next: where it will return to */
        back_pc = c->pc;
        back_sp = c->sp;
        nresumed = 0;
        resuming = 0;
    } else if (c->pc == back_pc && c->sp == back_sp && !resuming) {
        snapshot(&after_nmi);
        resuming = 1;
    }
    if (resuming && c->pc == 0x200A) {
        resuming = 0;
        for (unsigned a = 0; a < 0x3C0; a++) record((uint16_t)(0x4000 + a), after_nmi.ram[a], b->ram[a]);
        for (unsigned a = 0; a < 0x400; a++) record((uint16_t)(0x5000 + a), after_nmi.tiles[a], b->video.tiles[a]);
        for (unsigned a = 0; a < 0x100; a++) record((uint16_t)(0x5800 + a), after_nmi.objects[a], b->video.objects[a]);
    }
}

/* the controls by frame, and an autopilot hunting aliens and dodging shells */
static void pilot(Board *b)
{
    const uint8_t *ram = b->ram;
    int x = ram[0x202], target = -1, best_y = -1;
    for (int s = 0; s < 8; s++) {
        const uint8_t *slot = ram + 0x2B0 + s * 0x20;
        if ((slot[0] & 1) && slot[3] > best_y && slot[3] < 0xD0) best_y = slot[3], target = slot[4];
    }
    if (target < 0)
        for (int c = 0; c < 10 && target < 0; c++) {
            int col = (int)((b->frame / 600 + c) % 10);
            for (int r = 0; r < 6; r++)
                if (ram[0x123 + r * 16 + col] & 1) target = ((col + 3) << 4) + 7 + ram[0x20E];
        }
    int dodge = 0;
    for (int i = 0; i < 14; i++) {
        const uint8_t *sh = ram + 0x260 + i * 5;
        if ((sh[0] & 1) && sh[1] > 0x98 && sh[1] < 0xE0 && abs(sh[3] - x) < 14) dodge = sh[3] >= x ? -1 : 1;
    }
    int dir = dodge ? dodge : target < 0 ? 0 : (target & 0xFF) > x + 2 ? 1 : (target & 0xFF) < x - 2 ? -1 : 0;
    if (dir > 0) b->in0 |= IN0_LEFT;
    if (dir < 0) b->in0 |= IN0_RIGHT;
    if (b->frame & 1) b->in0 |= IN0_FIRE;
}

static void controls(Board *b, int line)
{
    if (line != BOARD_VBLANK_LINE) return;
    long f = b->frame, p = f % 20000;
    b->in0 = b->in1 = 0;
    if (f < 3000) return;
    if ((p >= 300 && p < 310) || (p >= 330 && p < 340) || (p >= 360 && p < 370)) b->in0 |= IN0_COIN1;
    if (p >= 400 && p < 410) b->in1 |= (f / 20000) & 1 ? IN1_START2 : IN1_START1;
    pilot(b);
}

/* the blank tiles: one picture */
static int blank(uint8_t t) { return t == 0x10 || (t >= 0x2C && t <= 0x2F) || t == 0x40 || t == 0x42 || t == 0xCA; }

/* Count the differences; print the first few. */
static long compare(const char *when)
{
    long diff = 0;
#define DIFF(what, addr, ours, theirs)                                                                     \
    do {                                                                                                   \
        if ((ours) != (theirs)) {                                                                          \
            if (diff++ < 12) fprintf(stderr, "%s: %s $%04X: %02X, the board %02X\n", when, what, addr, ours, theirs); \
        }                                                                                                  \
    } while (0)
    for (unsigned a = 0; a < 0x3C0; a++) DIFF("RAM", 0x4000 + a, cg.ram[a], board.ram[a]);
    for (unsigned a = 0; a < 0x400; a++)
        if (!blank(cg.tiles[a]) || !blank(board.video.tiles[a])) DIFF("tiles", 0x5000 + a, cg.tiles[a], board.video.tiles[a]);
    for (unsigned a = 0; a < 0x100; a++) DIFF("objects", 0x5800 + a, cg.objects[a], board.video.objects[a]);
    for (unsigned i = 0; i < 8; i++) {
        DIFF("latch", 0x6000 + i, cg.latch6000[i], board.latch6000[i]);
        DIFF("latch", 0x6800 + i, cg.latch6800[i], board.latch6800[i]);
        DIFF("latch", 0x7000 + i, cg.latch7000[i], board.latch7000[i]);
    }
    DIFF("latch", 0x7800, cg.pitch, board.pitch);
    return diff;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: test_frame <galaxian.zip|folder> [frames]\n");
        return 2;
    }
    char err[512];
    if (rom_load(&rom, argv[1], err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    long frames = argc > 2 ? atol(argv[2]) : 60000;
    board_init(&board, &rom);
    board.no_video = 1;
    board.line_hook = controls;
    while (!board.cpu.nmi_pending && board.frame < 1000) board_run_frame(&board);
    cg.data = rom.program;
    cg.in0 = board.in0, cg.in1 = board.in1, cg.in2 = board.in2;
    boot_power_on(&cg);
    char when[64];
    snprintf(when, sizeof when, "power-on (frame %ld)", (long)board.frame);
    long d = compare(when);
    printf("power-on: %ld bytes differ from the board's when its set-up is done (frame %ld)\n", d, (long)board.frame);
    int failed = d != 0;
    long matched = 0;
    board.step_hook = race;
    long races = 0, ram_races = 0;
    while (!failed && board.frame < frames) {
        board_run_frame(&board);
        cg.in0 = board.in0, cg.in1 = board.in1, cg.in2 = board.in2;
        frame_nmi(&cg);
        if (nresumed) races++;
        for (int i = 0; i < nresumed; i++)
            if (resumed[i].addr < 0x5000) {
                ram_races++;
                if (getenv("RACES")) fprintf(stderr, "frame %ld: the main loop wrote RAM $%04X after the NMI\n", (long)board.frame, resumed[i].addr);
                break;
            }
        for (int i = 0; i < nresumed; i++) game_wr(&cg, resumed[i].addr, resumed[i].value);
        nresumed = 0;
        if (!cg.unported) screen_run(&cg);
        if (cg.unported) {
            fprintf(stderr, "frame %ld: the C game reached unported code ($%04X)\n", (long)board.frame, cg.unported);
            failed = 1;
            break;
        }
        snprintf(when, sizeof when, "frame %ld", (long)board.frame);
        if (compare(when)) {
            failed = 1;
            fprintf(stderr, "  state $4005 %d step $400A %d, $425F %02X, queue at $40A1 %02X; the board's PC $%04X\n",
                    board.ram[5], board.ram[0xA], board.ram[0x25F], board.ram[0xA1], board.cpu.pc);
        }
        else matched++;
    }
    printf("frame: %ld frames of the C game alone equal the board's%s (%ld with an NMI breaking into the main loop's work, %ld of them its RAM)\n",
           matched, failed ? " (then a difference)" : "", races, ram_races);
    return failed;
}
