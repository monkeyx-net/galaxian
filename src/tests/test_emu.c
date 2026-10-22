/*
 * The emulated board against MAME (label mame):
 *   test_emu <galaxian.zip|folder> <script> <capture dir> <trace dir>
 *            <trace from> <trace to> <frame>...
 * Runs the scripted game the probes ran (the same PROBE_SCRIPT string, e.g.
 * "300:coin 310: 400:start 410:") from power-on and checks it against MAME:
 *   - MAME's instruction trace of frames <trace from>-<trace to>
 *     (trace_probe.lua): every instruction at the same cycle
 *   - at each given frame, what capture_probe.lua dumped (capture
 *     dir/frame_<n>.txt) and MAME's snapshot (capture dir/<n>.png): RAM bar
 *     the stack, tile and object RAM, the latches, and the picture pixel for
 *     pixel (MAME's 224x768).
 * MAME's frame callback, where the probes dump, runs at the start of
 * vertical blank, as the board's frames end. Inputs a MAME probe sets in its
 * frame callback reach the game at the start of the frame after next.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/png.h"
#include "assets/rom.h"
#include "emu/board.h"

static RomSet rom;
static Board board;
static int stack_lo = 0x3C0, stack_hi = 0x400; /* RAM offsets not checked: the stack ($43C0-$43FF; it never goes below $43DA) */

/* the probe's script: from each frame on, these switches are held */
typedef struct {
    long frame;
    uint8_t in0, in1;
} Step;
static Step steps[4096];
static int nsteps;

static int parse_script(const char *text)
{
    const char *p = text;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (nsteps == (int)(sizeof steps / sizeof steps[0])) return -1;
        char *end;
        Step *s = &steps[nsteps++];
        memset(s, 0, sizeof *s);
        s->frame = strtol(p, &end, 10);
        if (end == p || *end != ':') return -1;
        p = end + 1;
        while (*p && *p != ' ') {
            size_t n = strcspn(p, ", ");
            static const struct { const char *name; int port; uint8_t bit; } names[] = {
                { "coin", 0, IN0_COIN1 }, { "start", 1, IN1_START1 }, { "start2", 1, IN1_START2 },
                { "left", 0, IN0_LEFT }, { "right", 0, IN0_RIGHT }, { "fire", 0, IN0_FIRE },
            };
            int ok = 0;
            for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
                if (strlen(names[i].name) == n && !strncmp(p, names[i].name, n)) {
                    *(names[i].port ? &s->in1 : &s->in0) |= names[i].bit;
                    ok = 1;
                }
            if (!ok) return -1;
            p += n;
            if (*p == ',') p++;
        }
    }
    return 0;
}

/* WRITE_LOG=addr: every write to that latch, with its frame and cycle */
static unsigned write_log_addr = 0x10000;

static void log_write(Board *b, uint16_t addr, uint8_t v)
{
    if ((addr & 0xF807) == write_log_addr)
        printf("write $%04X=%02X frame %ld cycle %lld\n", addr, v, (long)b->frame,
               (long long)(b->cpu.cycles - b->frame_start));
}

static long input_delay = 1; /* frames from a probe's callback to the game */

static FILE *object_log;
static long object_log_frame = -1;

static void script(Board *b, int line)
{
    if (object_log && b->frame + 1 == object_log_frame) { /* sprites and shells as each line starts */
        fprintf(object_log, "%d", line);
        for (int i = 0x40; i < 0x80; i++) fprintf(object_log, " %02X", b->video.objects[i]);
        fprintf(object_log, "\n");
    }
    if (line != BOARD_VBLANK_LINE) return;
    long f = (long)b->frame - input_delay; /* the probe's frame number whose inputs apply now */
    for (int i = 0; i < nsteps; i++)
        if (steps[i].frame == f) {
            b->in0 = steps[i].in0;
            b->in1 = steps[i].in1;
        }
}

/* ---- traces ---------------------------------------------------------------- */

typedef struct {
    int64_t *cycles;
    uint32_t *pc;
    long n, cap;
} Trace;

static Trace mame_main, ours_main;
static long trace_from, trace_to;

static void trace_add(Trace *t, int64_t cycles, uint32_t pc)
{
    if (t->n == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 1 << 16;
        t->cycles = realloc(t->cycles, (size_t)t->cap * sizeof *t->cycles);
        t->pc = realloc(t->pc, (size_t)t->cap * sizeof *t->pc);
    }
    t->cycles[t->n] = cycles;
    t->pc[t->n++] = pc;
}

/* MAME's trace: instruction lines are "cycles PPPP: ..." */
static int load_trace(Trace *t, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    static char line[512];
    while (fgets(line, sizeof line, f)) {
        char *p = line, *end;
        long long cycles = strtoll(p, &end, 10);
        if (end == p || *end != ' ') continue;
        p = end + 1;
        unsigned pc;
        char colon;
        if (sscanf(p, "%4x%c", &pc, &colon) == 2 && colon == ':' && strlen(p) > 5) trace_add(t, cycles, pc);
    }
    fclose(f);
    return 0;
}

static void main_step(Board *b)
{
    if (b->frame >= trace_from && b->frame < trace_to + 1) trace_add(&ours_main, b->cpu.cycles, b->cpu.pc);
}

/* MAME's instructions, cycle and address, are the board's. The board also
   lists the instruction an NMI is taken before (once per NMI). The traces
   start and stop at slightly different moments: only the cycles both cover
   are compared. */
static int compare_main(void)
{
    if (!mame_main.n || !ours_main.n) return -1;
    int64_t lo = mame_main.cycles[0] > ours_main.cycles[0] ? mame_main.cycles[0] : ours_main.cycles[0];
    int64_t hi = mame_main.cycles[mame_main.n - 1] < ours_main.cycles[ours_main.n - 1] ? mame_main.cycles[mame_main.n - 1]
                                                                                     : ours_main.cycles[ours_main.n - 1];
    long i = 0, j = 0, matched = 0, interrupts = 0;
    while (i < mame_main.n && mame_main.cycles[i] < lo) i++;
    while (j < ours_main.n && ours_main.cycles[j] < lo) j++;
    while (i < mame_main.n && mame_main.cycles[i] <= hi && j < ours_main.n) {
        if (mame_main.cycles[i] == ours_main.cycles[j] && mame_main.pc[i] == ours_main.pc[j]) {
            i++, j++, matched++;
        } else if (j + 1 < ours_main.n && mame_main.cycles[i] == ours_main.cycles[j + 1] &&
                   mame_main.pc[i] == ours_main.pc[j + 1]) {
            j++, interrupts++;
        } else {
            fprintf(stderr, "Z80: MAME runs $%04X at cycle %lld, the board $%04X at %lld (frame %lld)\n",
                    (unsigned)mame_main.pc[i], (long long)mame_main.cycles[i], (unsigned)ours_main.pc[j],
                    (long long)ours_main.cycles[j], (long long)(ours_main.cycles[j] / BOARD_FRAME_CYCLES));
            for (int k = -4; k <= 4; k++)
                if (i + k >= 0 && i + k < mame_main.n && j + k >= 0 && j + k < ours_main.n)
                    fprintf(stderr, "  %+d  MAME %lld $%04X   board %lld $%04X\n", k, (long long)mame_main.cycles[i + k],
                            (unsigned)mame_main.pc[i + k], (long long)ours_main.cycles[j + k], (unsigned)ours_main.pc[j + k]);
            return -1;
        }
    }
    printf("Z80: %ld instructions and %ld interrupts as MAME's trace, cycle for cycle\n", matched, interrupts);
    return matched > 1000 ? 0 : -1;
}

/* ---- captured frames ------------------------------------------------------- */

static int hex(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

static int hex_bytes(const char *p, uint8_t *out, int n)
{
    for (int i = 0; i < n; i++) {
        int hi = hex(p[2 * i]), lo = hex(p[2 * i + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (uint8_t)(hi << 4 | lo);
    }
    return 0;
}

/* Count the differences from a probe dump; print the first few. */
static long compare_dump(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cannot read %s\n", path);
        return -1;
    }
    static char line[4096];
    static uint8_t ram[0x400], tiles[0x400], objects[0x100];
    int have = 0;
    char latch[64] = "";
    while (fgets(line, sizeof line, f)) {
        if (!strncmp(line, "ram ", 4)) have |= !hex_bytes(line + 4, ram, 0x400);
        else if (!strncmp(line, "tiles ", 6)) have |= !hex_bytes(line + 6, tiles, 0x400) << 1;
        else if (!strncmp(line, "objects ", 8)) have |= !hex_bytes(line + 8, objects, 0x100) << 2;
        else if (!strncmp(line, "latch ", 6)) {
            snprintf(latch, sizeof latch, "%.50s", line + 6);
            have |= 8;
        }
    }
    fclose(f);
    if (have != 15) {
        fprintf(stderr, "%s: not a capture frame\n", path);
        return -1;
    }
    long diff = 0, loose = 0;
    int shown = 0;
#define DIFF(count, what, addr, ours, mame)                                                              \
    do {                                                                                                 \
        if ((ours) != (mame)) {                                                                          \
            if (shown++ < 10) fprintf(stderr, "  %s $%04X: %02X, MAME %02X\n", what, addr, ours, mame); \
            count++;                                                                                     \
        }                                                                                                \
    } while (0)
    for (int a = 0; a < 0x400; a++) {
        if (a >= stack_lo && a < stack_hi) DIFF(loose, "stack", 0x4000 + a, board.ram[a], ram[a]);
        else DIFF(diff, "RAM", 0x4000 + a, board.ram[a], ram[a]);
    }
    for (int a = 0; a < 0x400; a++) DIFF(diff, "tiles", 0x5000 + a, board.video.tiles[a], tiles[a]);
    for (int a = 0; a < 0x100; a++) DIFF(diff, "objects", 0x5800 + a, board.video.objects[a], objects[a]);
    /* latches as the probe writes them: "--" never written */
    for (int i = 0; i < 25; i++) {
        int m = hex(latch[2 * i]) < 0 ? -1 : hex(latch[2 * i]) << 4 | hex(latch[2 * i + 1]);
        int ours = i < 8 ? board.latch6000[i] : i < 16 ? board.latch6800[i - 8] : i < 24 ? board.latch7000[i - 16] : board.pitch;
        if (m < 0) m = 0;
        if (i < 24) m &= 1;
        DIFF(diff, "latch", i, ours, m);
    }
    if (loose) printf("  (%ld bytes of the stack differ: not checked)\n", loose);
    return diff;
}

static long compare_shot(const char *path)
{
    Image shot;
    const char *err;
    if (png_load(path, &shot, &err)) {
        fprintf(stderr, "%s: %s\n", path, err);
        return -1;
    }
    long diff = 0;
    if (shot.width != GALAXIAN_NATIVE_W || shot.height != GALAXIAN_NATIVE_H) {
        fprintf(stderr, "%s: %dx%d, not MAME's 224x768\n", path, shot.width, shot.height);
        image_free(&shot);
        return -1;
    }
    for (int y = 0; y < GALAXIAN_NATIVE_H; y++)
        for (int x = 0; x < GALAXIAN_NATIVE_W; x++) {
            const uint8_t *p = shot.rgba + ((size_t)y * (size_t)shot.width + (size_t)x) * 4;
            uint32_t want = (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2], got = board.video.native[y][x] & 0xFFFFFF;
            if (got == want) continue;
            if (diff++ < 6)
                fprintf(stderr, "  pixel %d,%d (line %d, pixel %d.%d): %06X, MAME %06X\n", x, y, GALAXIAN_LAST_LINE - x,
                        y / 3, y % 3, (unsigned)got, (unsigned)want);
        }
    image_free(&shot);
    return diff;
}

int main(int argc, char **argv)
{
    if (argc < 7) {
        fprintf(stderr, "usage: test_emu <rom> <script> <capture dir> <trace dir> <trace from> <trace to> <frame>...\n");
        return 2;
    }
    char err[512];
    if (rom_load(&rom, argv[1], err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    if (parse_script(argv[2])) {
        fprintf(stderr, "bad script '%s'\n", argv[2]);
        return 2;
    }
    if (getenv("OBJECT_LOG")) { /* OBJECT_LOG=frame: object RAM line by line, to object.log */
        object_log_frame = atol(getenv("OBJECT_LOG"));
        object_log = fopen("object.log", "w");
    }
    if (getenv("INPUT_DELAY")) input_delay = atol(getenv("INPUT_DELAY"));
    char path[1200];
    trace_from = strtol(argv[5], NULL, 10);
    trace_to = strtol(argv[6], NULL, 10);
    int with_trace = trace_to > trace_from;
    if (with_trace) {
        snprintf(path, sizeof path, "%s/main.tr", argv[4]);
        if (load_trace(&mame_main, path)) {
            fprintf(stderr, "cannot read the trace in %s\n", argv[4]);
            return 1;
        }
    }
    board_init(&board, &rom);
    board.line_hook = script;
    if (getenv("WRITE_LOG")) {
        write_log_addr = (unsigned)strtoul(getenv("WRITE_LOG"), NULL, 16);
        board.write_hook = log_write;
    }
    if (with_trace) board.step_hook = main_step;

    long last = trace_to;
    for (int i = 7; i < argc; i++) {
        long fr = strtol(argv[i], NULL, 10);
        if (fr > last) last = fr;
    }
    int failed = 0;
    while (board.frame < last) {
        board_run_frame(&board);
        if (getenv("LATCH_LOG")) { /* the latches' changes, frame by frame */
            static uint8_t was[25];
            uint8_t now[25];
            memcpy(now, board.latch6000, 8), memcpy(now + 8, board.latch6800, 8), memcpy(now + 16, board.latch7000, 8);
            now[24] = board.pitch;
            if (memcmp(was, now, sizeof now)) {
                printf("frame %ld latches", (long)board.frame);
                for (int i = 0; i < 25; i++) printf(" %02X", now[i]);
                printf("\n");
                memcpy(was, now, sizeof now);
            }
        }
        /* the stack: from SP down, as far as calls go in this game */
        for (int i = 7; i < argc; i++) {
            if (strtol(argv[i], NULL, 10) != board.frame) continue;
            snprintf(path, sizeof path, "%s/frame_%ld.txt", argv[3], (long)board.frame);
            long d = compare_dump(path);
            snprintf(path, sizeof path, "%s/%ld.png", argv[3], (long)board.frame);
            long p = compare_shot(path);
            if (getenv("DUMP_DIR")) { /* the board's picture, to look at a difference */
                static uint8_t rgba[GALAXIAN_NATIVE_H * GALAXIAN_NATIVE_W * 4];
                for (int y = 0; y < GALAXIAN_NATIVE_H; y++)
                    for (int x = 0; x < GALAXIAN_NATIVE_W; x++) {
                        uint32_t c = board.video.native[y][x];
                        uint8_t *o = rgba + ((size_t)y * GALAXIAN_NATIVE_W + (size_t)x) * 4;
                        o[0] = (uint8_t)(c >> 16), o[1] = (uint8_t)(c >> 8), o[2] = (uint8_t)c, o[3] = 0xFF;
                    }
                snprintf(path, sizeof path, "%s/ours_%ld.png", getenv("DUMP_DIR"), (long)board.frame);
                png_write(path, rgba, GALAXIAN_NATIVE_W, GALAXIAN_NATIVE_H);
            }
            printf("frame %ld: %ld bytes and %ld pixels differ from MAME\n", (long)board.frame, d, p);
            if (d || p) failed = 1;
        }
    }
    if (with_trace && compare_main()) failed = 1;
    return failed;
}
