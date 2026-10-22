/*
 * Galaxian's discrete sound, modelled from its behaviour as MAME 0.289
 * plays it (each source measured from MAME's output with one latch changed
 * at a time; GALAXIAN_PORT_PLAN.md section 2.1). Generated, so nothing
 * comes from the ROM set. Samples are made at 48 kHz, 64 CPU cycles each:
 * 792 a frame, in MAME's output units (the same levels, clipped to 16 bits).
 *
 *   tone ($7800 pitch, $6806/$6807 volume): an 8-bit counter loaded with
 *        the pitch steps a 4-bit counter at 1.536 MHz / (256 - pitch); a
 *        3-input DAC of q0, q1^q2 and (q1&q2)^q3, its weights set by the two
 *        volume bits ($FF: no steps, silence)
 *   swarm ($6800-$6802 FS1-3, $6004-$6007 the LFO's 4 bits): three 555s
 *        gliding down from 237, 332 and 483 Hz to 124, 169 and 238 Hz and
 *        back, once per LFO period (1.20 s at 15 to 0.16 s at 0), each with
 *        a fixed low half, through a low-pass (0.3 ms); quieter with the
 *        volume bits set
 *   hit ($6803): the noise generator (stepped at 8 kHz) through a 3 ms
 *        low-pass and a resonant high-pass (152 Hz), full at once, fading
 *        out with 0.71 s when released
 *   fire ($6805): a noise-jittered oscillator (2.9 kHz rising to 4.4 kHz
 *        while held, falling to 2.5 kHz after) whose edges come through as
 *        clicks, fading out with 0.17 s when released
 * all through the output's high-pass (10 ms). $6804 makes no sound.
 */
#ifndef GALAXIAN_SOUND_H
#define GALAXIAN_SOUND_H

#include <stdint.h>

#define SOUND_RATE 48000
#define SOUND_CYCLES_PER_SAMPLE 64 /* 3.072 MHz / 48 kHz */
#define SOUND_MAX_SAMPLES 1024

/* the latches, as the board numbers them */
enum {
    SOUND_LFO0, SOUND_LFO1, SOUND_LFO2, SOUND_LFO3, /* $6004-$6007 */
    SOUND_FS1, SOUND_FS2, SOUND_FS3, SOUND_HIT,     /* $6800-$6803 */
    SOUND_UNUSED, SOUND_FIRE, SOUND_VOL1, SOUND_VOL2, /* $6804-$6807 */
    SOUND_PITCH,                                    /* $7800 */
    SOUND_LATCHES
};

typedef struct {
    uint8_t latch[SOUND_LATCHES];
    /* tone */
    uint8_t tone_count;     /* the 8-bit counter */
    uint8_t tone_step;      /* the 4-bit counter */
    /* swarm */
    double lfo_phase;       /* 0..1 down the glide */
    double fs_phase[3];     /* 0..1 the high half, 1..2 the low half */
    double swarm_lp;
    /* noise */
    uint32_t noise;         /* the shift register */
    int noise_wait;         /* samples to its next step */
    double hit_env, hit_lp;
    double hit_b[3], hit_a[2], hit_x[2], hit_y[2]; /* the hit's resonant high-pass */
    double fire_env, fire_rate, fire_phase, fire_click;
    int fire_level;
    /* the output's high-pass */
    double hp_in, hp_out;
} Sound;

void sound_reset(Sound *s);
/* A latch's new value (bit 0 for the 1-bit latches, all 8 for the pitch). */
void sound_write(Sound *s, int latch, uint8_t value);
/* Make n samples (mono, 16-bit). */
void sound_render(Sound *s, int16_t *out, int n);

#endif
