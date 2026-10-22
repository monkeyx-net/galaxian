/*
 * The Galaxian board: a 3.072 MHz Z80, the 10 KB program ROM at $0000-$27FF
 * ($2800-$3FFF read 0), 1 KB of RAM at $4000 (mirrored to $47FF), 1 KB of
 * tile RAM at $5000 (to $57FF) and 256 bytes of object RAM at $5800 (to
 * $5FFF): per-column scroll and colour $5800-$583F, 8 sprites $5840-$585F,
 * shells and the missile $5860-$587F. Reads: IN0 $6000, IN1 $6800, the DIP
 * switches $7000, the watchdog $7800 (reads $FF); $4800-$4FFF and
 * $8000-$FFFF read $FF. Writes (each latch decodes A0-A2, mirrored over its
 * 2 KB): lamps and coin counter $6000-$6003, the sound's LFO $6004-$6007,
 * the sound $6800-$6807, NMI enable $7001, stars $7004, flip $7006/$7007,
 * the tone's pitch $7800 (emu/sound.c makes the sound from these). The
 * watchdog resets the board after 8 vertical blanks without a read of $7800.
 *
 * A frame is 264 lines of 192 CPU cycles (384 pixels at 6.144 MHz), from the
 * start of line 240 (vertical blank) to the next: the NMI flip-flop is set
 * there when enabled (MAME raises the NMI a cycle earlier), and writing 0 to
 * $7001 clears it. Frame k starts at CPU cycle 50688 k. Lines 16-239 are
 * visible; as MAME does, a line is drawn (gfx/galaxian.c) when a write to
 * the video first comes after its start (or at the frame's end), so it
 * shows the video as it was when the line began.
 */
#ifndef GALAXIAN_BOARD_H
#define GALAXIAN_BOARD_H

#include <stdint.h>

#include "assets/rom.h"
#include "emu/sound.h"
#include "emu/z80.h"
#include "gfx/fb.h"
#include "gfx/galaxian.h"

#define BOARD_LINES 264
#define BOARD_CYCLES_PER_LINE 192
#define BOARD_FRAME_CYCLES (BOARD_LINES * BOARD_CYCLES_PER_LINE) /* 50688 */
#define BOARD_VBLANK_LINE 240                                  /* frames start here */
#define BOARD_FRAME_HZ (3072000.0 / BOARD_FRAME_CYCLES)        /* 60.606 */
#define BOARD_AUDIO_SAMPLES (BOARD_FRAME_CYCLES / SOUND_CYCLES_PER_SAMPLE) /* 792 a frame at 48 kHz */
#define BOARD_WATCHDOG_FRAMES 8 /* vertical blanks without a read of $7800 reset the board */

/* Inputs, active high, as MAME's ports */
#define IN0_COIN1 0x01
#define IN0_COIN2 0x02
#define IN0_LEFT 0x04
#define IN0_RIGHT 0x08
#define IN0_FIRE 0x10
#define IN0_COCKTAIL 0x20
#define IN0_SERVICE_MODE 0x40
#define IN0_SERVICE 0x80
#define IN1_START1 0x01
#define IN1_START2 0x02
#define IN1_LEFT2 0x04
#define IN1_RIGHT2 0x08
#define IN1_FIRE2 0x10
#define IN1_COINAGE 0xC0     /* 00 1C/1C, 40 2C/1C, 80 1C/2C, C0 free play */
#define IN2_BONUS 0x03       /* 00 7000, 01 10000, 02 12000, 03 20000 */
#define IN2_LIVES3 0x04      /* clear: 2 lives */
#define IN2_DEFAULT IN2_LIVES3

typedef struct Board Board;

struct Board {
    Z80 cpu;
    const RomSet *rom;
    GalaxianVideo video;     /* tile and object RAM, the video latches, the palette */
    uint8_t ram[0x400];
    uint8_t latch6000[8];    /* lamps, coin counter, LFO */
    uint8_t latch6800[8];    /* the sound */
    uint8_t latch7000[8];    /* NMI enable, stars, flip */
    uint8_t pitch;           /* $7800 */
    uint8_t in0, in1, in2;
    int watchdog;            /* vertical blanks since $7800 was last read */
    /* the sound; the last frame's samples are in audio */
    Sound sound;
    int16_t audio[BOARD_AUDIO_SAMPLES];
    struct { int16_t sample; uint8_t latch, value; } sound_events[256];
    int sound_event_count;
    int64_t frame;           /* frames run */
    int64_t frame_start;     /* the cycle the frame started (line 240) */
    int next_line;           /* the next line to draw, in frame order */
    /* optional: called on every CPU write (tests trace RAM through this) */
    void (*write_hook)(Board *b, uint16_t addr, uint8_t value);
    /* optional: called at the start of each line (scripted input timing) */
    void (*line_hook)(Board *b, int line);
    /* optional: called before each CPU step (instruction traces) */
    void (*step_hook)(Board *b);
    void *user;
};

/* Power on. */
void board_init(Board *b, const RomSet *rom);
/* Run one frame, drawing the visible lines into b->video.native and making
   its BOARD_AUDIO_SAMPLES of sound in b->audio. */
void board_run_frame(Board *b);
/* A memory read as the CPU sees it (no side effects). */
uint8_t board_peek(Board *b, uint16_t addr);

#endif
