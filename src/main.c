/*
 * Galaxian native build: SDL setup and the fixed 60 Hz loop. See GALAXIAN_PORT_PLAN.md.
 * The game is the ROM's own code on the emulated board (emu/board.c), a
 * board frame per tick (--emulate; the game in C will be the default, plan
 * section 3). The port supplies video (with the enhanced presets), input,
 * sound (the board's model of the discrete circuit, emu/sound.c), the DIP
 * switches (galaxian.cfg in the save folder) and, later, high-score saving. --test-card shows the test card instead (ROM status, an
 * input tester and the graphics presets).
 *
 *   galaxian [--windowed] [--gfx preset] [--rom path] [--test-card] [--emulate]
 *            [--input script.txt] [--frames N] [--screenshot out.png]
 *   galaxian --check-rom <dir|zip>
 *   galaxian --dump-rom <dir|zip> <first> <last>
 *   galaxian --extract <dir|zip> [<asset dir>]
 *
 * --frames N stops after N updates; with --screenshot the last frame is
 * written as a PNG (use SDL_VIDEODRIVER=dummy to run headless).
 * --input replays scripted controls: lines of "<frame> [coin] [start]
 * [start2] [left] [right] [fire]", each holding from that frame until the
 * next line (live controls still quit).
 * --dump-rom prints "ADDR BYTE" lines (hex) for Z80 addresses first..last
 * in the program ROM, then "G:ADDR BYTE" for the graphics ROMs (1h at
 * $000, 1k at $800) and "P:ADDR BYTE" for the palette PROM: the format the
 * MAME probe writes (src/tests/mame/rom_map_probe.lua).
 * --extract writes the tiles, palette, font and previews from the ROM set to
 * the asset folder (default $GALAXIAN_ASSETS, else assets/ next to the
 * program; assets/extract.h). The game written in C will read only that
 * folder; the emulated board runs from the ROM set.
 */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/assets.h"
#include "assets/extract.h"
#include "assets/png.h"
#include "assets/rom.h"
#include "assets/savefile.h"
#include "emu/board.h"
#include "gfx/blit.h"
#include "gfx/debugfont.h"
#include "gfx/enhance.h"
#include "gfx/fb.h"
#include "gfx/galaxian.h"
#include "platform/audio.h"
#include "platform/input.h"
#include "platform/video.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define FRAME_HZ 60
/* Cap catch-up after a stall (suspend/resume, SD card hiccup). */
#define MAX_STEPS_PER_PRESENT 4
#define GFX_NOTICE_FRAMES 120
#define SETTINGS_FILE "galaxian.cfg"

#define WHITE 0xFFFFFFu
#define GREY 0x606060u
#define YELLOW 0xFFFF44u
#define CYAN 0x44FFFFu
#define GREEN 0x44FF44u
#define RED 0xFF4444u
#define ORANGE 0xFF8800u

static Framebuffer fb;     /* arcade resolution */
static Framebuffer fb_big; /* enhanced modes: the frame at the render scale */
static Framebuffer *shown; /* the frame last drawn */

typedef struct {
    unsigned long clock;
    InputFrame in;
    GfxSettings gfx;
    int gfx_notice;
    int coins;
    char rom_path[1024];
    char rom_error[256];
    char assets_path[1024]; /* the extracted assets, or "" */
    RomSet rom;
    int test_card;       /* --test-card, or no ROM: the card instead of the game */
    Board board;
    uint8_t dips;        /* IN2, and IN1's coinage bits */
    uint8_t coinage;
} Port;

static Port g;

/* ---- test card ------------------------------------------------------------ */

static void centred(Framebuffer *t, int y, const char *text, uint32_t rgb)
{
    debug_text(t, (FB_WIDTH - debug_text_width(text)) / 2.0, y, text, rgb);
}

static void lamp(Framebuffer *t, int x, int y, const char *label, int on)
{
    fill_rectf(t, x, y, 5, 5, on ? GREEN : GREY);
    debug_text(t, x + 8, y, label, on ? WHITE : GREY);
}

static void draw_scene(Framebuffer *t)
{
    char line[160];
    fb_clear(t, 0xFF000000u);
    fill_rectf(t, 4, 15, FB_WIDTH - 8, 2, ORANGE);
    fill_rectf(t, 4, FB_HEIGHT - 11, FB_WIDTH - 8, 2, ORANGE);
    fill_rectf(t, 4, 15, 2, FB_HEIGHT - 24, ORANGE);
    fill_rectf(t, FB_WIDTH - 6, 15, 2, FB_HEIGHT - 24, ORANGE);
    centred(t, 4, "GALAXIAN - NATIVE PORT - TEST CARD", YELLOW);

    if (g.rom.loaded) {
        snprintf(line, sizeof line, "ROM: %.46s", g.rom_path);
        centred(t, 26, line, GREEN);
        centred(t, 34, "MAME GALAXIAN SET OK", GREEN);
        snprintf(line, sizeof line, "FIRST BYTES $%02X $%02X $%02X", g.rom.program[0], g.rom.program[1],
                 g.rom.program[2]);
        centred(t, 42, line, GREEN);
    } else {
        centred(t, 26, g.rom_error[0] ? "ROM SET INCOMPLETE OR WRONG" : "NO ROM SET FOUND", RED);
        centred(t, 34, "PUT GALAXIAN.ZIP NEXT TO THE PROGRAM,", RED);
        centred(t, 42, "OR USE --ROM", RED);
    }
    if (g.assets_path[0]) {
        snprintf(line, sizeof line, "ASSETS: %.44s", g.assets_path);
        centred(t, 50, line, GREEN);
    } else {
        centred(t, 50, "NO ASSETS EXTRACTED YET", YELLOW);
    }

    const InputFrame *in = &g.in;
    snprintf(line, sizeof line, "CONTROLS%s", in->pads ? "  (PAD)" : "");
    debug_text(t, 16, 66, line, CYAN);
    lamp(t, 16, 78, "LEFT", in->move_x < 0);
    lamp(t, 64, 78, "RIGHT", in->move_x > 0);
    lamp(t, 116, 78, "FIRE", in->fire);
    lamp(t, 16, 92, "COIN", in->coin);
    lamp(t, 64, 92, "START", in->start);
    lamp(t, 116, 92, "2P START", in->start2);
    snprintf(line, sizeof line, "COINS %d", g.coins);
    debug_text(t, 16, 104, line, WHITE);

    /* something moving, to judge pacing and the enhanced modes */
    double phase = (double)(g.clock % 240) / 240.0;
    double bx = 16 + 180 * (phase < 0.5 ? phase * 2 : 2 - phase * 2);
    fill_rectf(t, 16, 140, 192, 1, GREY);
    fill_rectf(t, bx, 128, 12, 12, YELLOW);
    fill_rectf(t, bx + 3, 131, 6, 6, RED);
    for (int i = 0; i < 8; i++) {
        static const uint32_t bars[8] = { 0xFFFFFF, 0xFFFF00, 0x00FFFF, 0x00FF00, 0xFF00FF, 0xFF0000, 0x0000FF, 0x000000 };
        fill_rectf(t, 16 + i * 24, 150, 24, 20, bars[i]);
    }

    snprintf(line, sizeof line, "GRAPHICS: %s", gfx_describe(&g.gfx));
    centred(t, 184, line, g.gfx_notice > 0 ? YELLOW : WHITE);
    centred(t, 192, "SELECT+L1 / F3, F4 GLOW, F5 SCANLINES", GREY);
    snprintf(line, sizeof line, "FRAME %lu   RENDER %dX", g.clock, t->scale);
    centred(t, 204, line, GREY);
    centred(t, 220, "EXIT: HOLD SELECT+START OR ESC", GREY);
    centred(t, 232, "CONVERTED BY MONKEYX", GREY);
}

/* ---- the game ---------------------------------------------------------------- */

/* The DIP switches, as MAME's defaults unless galaxian.cfg says otherwise:
   lines "coinage 1c1c|2c1c|1c2c|free", "bonus 7000|10000|12000|20000",
   "lives 2|3". The file is written with the defaults if missing. */
static void settings_load(void)
{
    g.dips = IN2_DEFAULT;
    g.coinage = 0x00;
    FILE *f = fopen(save_path(SETTINGS_FILE), "r");
    if (!f) {
        static const char defaults[] = "# Galaxian's DIP switches (MAME's defaults)\n"
                                       "coinage 1c1c\n# 1c1c 2c1c 1c2c free\n"
                                       "bonus 7000\n# 7000 10000 12000 20000\n"
                                       "lives 3\n# 2 3\n";
        save_store(SETTINGS_FILE, defaults, sizeof defaults - 1);
        return;
    }
    char line[128], key[32], value[32];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || sscanf(line, "%31s %31s", key, value) != 2) continue;
        if (!strcmp(key, "coinage")) {
            static const char *const names[4] = { "1c1c", "2c1c", "1c2c", "free" };
            for (int i = 0; i < 4; i++)
                if (!strcmp(value, names[i])) g.coinage = (uint8_t)(i << 6);
        } else if (!strcmp(key, "bonus")) {
            static const char *const names[4] = { "7000", "10000", "12000", "20000" };
            for (int i = 0; i < 4; i++)
                if (!strcmp(value, names[i])) g.dips = (uint8_t)((g.dips & ~IN2_BONUS) | i);
        } else if (!strcmp(key, "lives")) {
            if (!strcmp(value, "2")) g.dips &= (uint8_t)~IN2_LIVES3;
            else if (!strcmp(value, "3")) g.dips |= IN2_LIVES3;
        }
    }
    fclose(f);
}

/* The port's controls as the cabinet's switches: one panel, as the upright
   cabinet (both players use player 1's controls). */
static void board_inputs(const InputFrame *in)
{
    g.board.in0 = (uint8_t)((in->coin ? IN0_COIN1 : 0) | (in->move_x < 0 ? IN0_LEFT : 0) |
                            (in->move_x > 0 ? IN0_RIGHT : 0) | (in->fire ? IN0_FIRE : 0));
    g.board.in1 = (uint8_t)((in->start ? IN1_START1 : 0) | (in->start2 ? IN1_START2 : 0) | g.coinage);
    g.board.in2 = g.dips;
}

/* Classic: the arcade frame. HD: drawn at the render scale. FILTER: the arcade
   frame upscaled whole. Glow goes over whichever is shown. */
static void draw(int display_scale)
{
    const GfxSettings *gs = &g.gfx;
    const Scaler scaler = gs->smooth ? SCALER_SMOOTH : SCALER_CRISP;
    const int s = gfx_render_scale(gs, display_scale);
    shown = &fb;
    if (!g.test_card) { /* the board's picture, already in fb; only pixels, so HD upscales it like FILTER */
        if (g.gfx_notice > 0) {
            char text[80];
            snprintf(text, sizeof text, "GRAPHICS: %s", gfx_describe(&g.gfx));
            fill_rectf(&fb, 0, FB_HEIGHT - 9, FB_WIDTH, 9, 0x000000);
            centred(&fb, FB_HEIGHT - 7, text, YELLOW);
        }
        if (gs->mode != GFX_CLASSIC && !fb_init(&fb_big, s) && !enhance_filter_frame(&fb, &fb_big, scaler))
            shown = &fb_big;
        if (gs->glow) enhance_glow(shown);
        return;
    }
    if (gs->mode == GFX_HD && !fb_init(&fb_big, s) && !enhance_prepare(s, scaler)) {
        draw_scene(&fb_big);
        shown = &fb_big;
    } else {
        draw_scene(&fb);
        if (gs->mode == GFX_FILTER && !fb_init(&fb_big, s) && !enhance_filter_frame(&fb, &fb_big, scaler))
            shown = &fb_big;
    }
    if (gs->glow) enhance_glow(shown);
}

static void step(const InputFrame *in)
{
    static bool coin_was;
    g.clock++;
    g.in = *in;
    if (!g.test_card) {
        board_inputs(in);
        board_run_frame(&g.board);
        galaxian_to_fb(&g.board.video, &fb);
        /* a board frame's sound (1/60.606 s) plays in the tick's 1/60 s, as the game runs 1% slow */
        audio_stream(g.board.audio, BOARD_AUDIO_SAMPLES, BOARD_AUDIO_SAMPLES * FRAME_HZ);
    }
    if (in->coin && !coin_was) g.coins++;
    coin_was = in->coin;
    if (in->gfx_next || in->gfx_glow || in->gfx_scanlines) {
        if (in->gfx_next) gfx_next_preset(&g.gfx);
        if (in->gfx_glow) g.gfx.glow = !g.gfx.glow;
        if (in->gfx_scanlines) g.gfx.scanlines = !g.gfx.scanlines;
        g.gfx_notice = GFX_NOTICE_FRAMES;
    }
    if (g.gfx_notice > 0) g.gfx_notice--;
}

/* ---- tools ---------------------------------------------------------------- */

static int check_rom(const char *path)
{
    static RomSet rom;
    char err[512];
    if (rom_load(&rom, path, err, sizeof err)) {
        fprintf(stderr, "galaxian: %s\n", err);
        return 1;
    }
    printf("galaxian: %s: MAME galaxian set OK\n", path);
    return 0;
}

static int dump_rom(const char *path, const char *first, const char *last)
{
    static RomSet rom;
    char err[512];
    if (rom_load(&rom, path, err, sizeof err)) {
        fprintf(stderr, "galaxian: %s\n", err);
        return 1;
    }
    unsigned a = (unsigned)strtoul(first, NULL, 16), b = (unsigned)strtoul(last, NULL, 16);
    for (unsigned addr = a; addr <= b && addr <= 0xFFFF; addr++) {
        int v = rom_byte(&rom, addr);
        if (v >= 0) printf("%04X %02X\n", addr, v);
    }
    for (unsigned i = 0; i < 2 * ROM_GFX_SIZE; i++) printf("G:%03X %02X\n", i, rom.gfx[i / ROM_GFX_SIZE][i % ROM_GFX_SIZE]);
    for (unsigned i = 0; i < ROM_PROM_SIZE; i++) printf("P:%02X %02X\n", i, rom.prom[i]);
    return 0;
}

static int extract(const char *argv0, const char *rom_path, const char *out_dir)
{
    static RomSet rom;
    char err[512], dir[1024];
    if (out_dir) snprintf(dir, sizeof dir, "%s", out_dir);
    else if (getenv("GALAXIAN_ASSETS")) snprintf(dir, sizeof dir, "%s", getenv("GALAXIAN_ASSETS"));
    else {
        snprintf(dir, sizeof dir, "%s", argv0);
        char *slash = strrchr(dir, '/');
#ifdef _WIN32
        char *bslash = strrchr(dir, '\\');
        if (bslash > slash) slash = bslash;
#endif
        if (slash) snprintf(slash, sizeof dir - (size_t)(slash - dir), "/assets");
        else snprintf(dir, sizeof dir, "assets");
    }
    if (rom_load(&rom, rom_path, err, sizeof err) || extract_all(&rom, dir, err, sizeof err)) {
        fprintf(stderr, "galaxian: %s\n", err);
        return 1;
    }
    printf("galaxian: assets extracted to %s\n", dir);
    return 0;
}

static int save_screenshot(const char *path)
{
    static uint8_t rgba[FB_HEIGHT * FB_MAX_SCALE * FB_WIDTH * FB_MAX_SCALE * 4];
    const Framebuffer *f = shown ? shown : &fb;
    for (int y = 0; y < f->h; y++)
        for (int x = 0; x < f->w; x++) {
            uint32_t p = fb_row(f, y)[x];
            uint8_t *o = rgba + ((long)y * f->w + x) * 4;
            o[0] = (uint8_t)(p >> 16);
            o[1] = (uint8_t)(p >> 8);
            o[2] = (uint8_t)p;
            o[3] = 0xFF;
        }
    if (png_write(path, rgba, f->w, f->h)) {
        fprintf(stderr, "galaxian: cannot write %s\n", path);
        return -1;
    }
    return 0;
}

/* ---- scripted input ------------------------------------------------------- */

typedef struct {
    long frame;
    InputFrame in;
} ScriptLine;

static ScriptLine script[4096];
static int script_len, script_pos;

static int load_script(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "galaxian: cannot open %s\n", path);
        return -1;
    }
    char line[256];
    while (script_len < (int)(sizeof script / sizeof script[0]) && fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        ScriptLine *l = &script[script_len];
        memset(l, 0, sizeof *l);
        char *tok = strtok(line, " \t\n");
        if (!tok) continue;
        l->frame = strtol(tok, NULL, 10);
        while ((tok = strtok(NULL, " \t\n"))) {
            if (!strcmp(tok, "coin")) l->in.coin = true;
            else if (!strcmp(tok, "start")) l->in.start = true;
            else if (!strcmp(tok, "start2")) l->in.start2 = true;
            else if (!strcmp(tok, "left")) l->in.move_x = -1;
            else if (!strcmp(tok, "right")) l->in.move_x = 1;
            else if (!strcmp(tok, "fire")) l->in.fire = true;
            else fprintf(stderr, "galaxian: %s: unknown token '%s'\n", path, tok);
        }
        script_len++;
    }
    fclose(f);
    return 0;
}

/* Replace live controls with the script line in force at `frame` (quit stays live). */
static void apply_script(InputFrame *in, long frame)
{
    while (script_pos + 1 < script_len && script[script_pos + 1].frame <= frame) script_pos++;
    bool quit = in->quit;
    if (script[script_pos].frame <= frame) *in = script[script_pos].in;
    else memset(in, 0, sizeof *in);
    in->quit = quit;
}

/* ---- loop ----------------------------------------------------------------- */

/* Loop state, static so the browser can run one pass per animation frame. */
static long max_frames = -1;
static const char *screenshot;
static Uint64 step_ticks, last, acc;
static long frames;
static int rc;

/* Step for the time elapsed, then draw and present; false = stop. */
static bool run_once(void)
{
    Uint64 now = SDL_GetPerformanceCounter();
    acc += now - last;
    last = now;
    if (acc > step_ticks * MAX_STEPS_PER_PRESENT)
        acc = step_ticks * MAX_STEPS_PER_PRESENT;
    /* Headless runs step once per loop so the frame count is exact. */
    if (max_frames >= 0 && acc < step_ticks) acc = step_ticks;

    while (acc >= step_ticks) {
        InputFrame in;
        input_poll(&in);
#ifndef __EMSCRIPTEN__
        if (in.quit) return false; /* a browser tab has nothing to quit to */
#endif
        if (script_len) apply_script(&in, frames + 1);
        step(&in);
        frames++;
        acc -= step_ticks;
        if (max_frames >= 0 && frames >= max_frames) break;
    }
    if (max_frames < 0 || frames >= max_frames) draw(video_display_scale());
    if (max_frames < 0) video_present(shown, g.gfx.scanlines); /* blocks on vsync */
    if (max_frames >= 0 && frames >= max_frames) {
        if (screenshot && save_screenshot(screenshot)) rc = 1;
        return false;
    }
    return true;
}

#ifdef __EMSCRIPTEN__
static void web_frame(void) { run_once(); }
#endif

int main(int argc, char **argv)
{
    bool windowed = false;
    const char *rom_arg = NULL;
    if (argc == 3 && !strcmp(argv[1], "--check-rom")) return check_rom(argv[2]);
    if (argc == 5 && !strcmp(argv[1], "--dump-rom")) return dump_rom(argv[2], argv[3], argv[4]);
    if ((argc == 3 || argc == 4) && !strcmp(argv[1], "--extract")) return extract(argv[0], argv[2], argc == 4 ? argv[3] : NULL);
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--windowed")) windowed = true;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) max_frames = strtol(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) screenshot = argv[++i];
        else if (!strcmp(argv[i], "--rom") && i + 1 < argc) rom_arg = argv[++i];
        else if (!strcmp(argv[i], "--test-card")) g.test_card = 1;
        else if (!strcmp(argv[i], "--emulate")) {} /* the only game so far */
        else if (!strcmp(argv[i], "--input") && i + 1 < argc) {
            if (load_script(argv[++i])) return 2;
        }
        else if (!strcmp(argv[i], "--gfx") && i + 1 < argc) {
            if (gfx_parse(argv[++i], &g.gfx)) {
                fprintf(stderr, "galaxian: unknown --gfx preset '%s' (one of: %s)\n", argv[i], gfx_preset_names());
                return 2;
            }
        } else {
            fprintf(stderr,
                    "usage: %s [--windowed] [--gfx preset] [--rom path] [--test-card] [--emulate]\n"
                    "          [--input script.txt] [--frames N] [--screenshot out.png]\n"
                    "       %s --check-rom <dir|zip>\n"
                    "       %s --dump-rom <dir|zip> <first> <last>\n"
                    "       %s --extract <dir|zip> [<asset dir>]\n",
                    argv[0], argv[0], argv[0], argv[0]);
            return 2;
        }
    }

    if (fb_init(&fb, 1)) {
        fprintf(stderr, "galaxian: out of memory\n");
        return 1;
    }
    if (rom_arg) snprintf(g.rom_path, sizeof g.rom_path, "%s", rom_arg);
    else rom_locate(argv[0], g.rom_path, sizeof g.rom_path);
    if (g.rom_path[0] && rom_load(&g.rom, g.rom_path, g.rom_error, sizeof g.rom_error))
        fprintf(stderr, "galaxian: %s\n", g.rom_error);
    if (assets_find(argv[0], EXTRACT_MANIFEST, g.assets_path, sizeof g.assets_path)) g.assets_path[0] = '\0';
    if (!g.rom.loaded) g.test_card = 1; /* nothing to run: show why */
    if (!g.test_card) {
        settings_load();
        board_init(&g.board, &g.rom);
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }
#ifdef __EMSCRIPTEN__
    windowed = true; /* the page's canvas */
#endif
    if (!video_init(windowed)) {
        SDL_Quit();
        return 1;
    }
    input_init();
    if (max_frames < 0) audio_init(argv[0]); /* headless runs stay silent */

    step_ticks = SDL_GetPerformanceFrequency() / FRAME_HZ;
    last = SDL_GetPerformanceCounter();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(web_frame, 0, 1); /* never returns */
#endif
    while (run_once()) {}

    audio_shutdown();
    input_shutdown();
    video_shutdown();
    SDL_Quit();
    enhance_clear_images();
    fb_free(&fb_big);
    fb_free(&fb);
    return rc;
}
