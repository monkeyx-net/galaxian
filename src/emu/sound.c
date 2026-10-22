#include "emu/sound.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <string.h>

/* Measured from MAME's output (GALAXIAN_PORT_PLAN.md section 2.1). */

/* the tone's DAC: weights of q0, q1^q2 and (q1&q2)^q3, by volume ($6806 + 2 x $6807) */
static const double tone_weight[4][3] = {
    { 1900, 2849, 0 }, { 1388, 6665, 0 }, { 1526, 2288, 3356 }, { 1177, 5651, 2591 },
};
/* the LFO's period in seconds, by its 4 bits */
static const double lfo_period[16] = {
    0.1640, 0.1727, 0.1837, 0.1941, 0.2119, 0.2264, 0.2451, 0.2644,
    0.3240, 0.3586, 0.4069, 0.4626, 0.5725, 0.6871, 0.8864, 1.1938,
};
static const double fs_high[3] = { 237.5, 332.0, 483.5 }, fs_low[3] = { 124.0, 169.5, 238.5 };
/* each is a 555: its low half lasts these samples whatever the frequency, the high half the rest */
static const double fs_low_time[3] = { 156.0, 109.5, 73.1 };
/* the volume bits load the mixer: the swarm's level by volume */
static const double swarm_gain[4] = { 1.0, 0.827, 0.933, 0.714 };
#define FS_LEVEL 4560.0    /* each square wave, peak to peak */
#define SWARM_LP 14.5      /* samples */
#define NOISE_SAMPLES 6    /* the noise steps at 8 kHz (every second line) */
#define HIT_LEVEL 52000.0  /* the noise's swing before the low-pass */
#define HIT_LP 147.0       /* samples */
#define HIT_RELEASE 0.71   /* seconds */
#define HIT_PEAK 152.0     /* Hz: a resonant high-pass after the low-pass */
#define HIT_PEAK_Q 2.4
#define HIT_ZERO_Q 2.3
#define FIRE_CLICK 10250.0 /* an edge, at full strength */
#define FIRE_DECAY 0.3128  /* of a click, a sample */
#define FIRE_RELEASE 0.19  /* seconds */
#define FIRE_REST 2500.0   /* Hz */
#define FIRE_TOP 4600.0    /* Hz, approached while held */
#define FIRE_RISE 0.07     /* seconds */
#define FIRE_FALL 0.10     /* seconds */
#define FIRE_JITTER 0.3    /* the noise bit moves the frequency by this much either way */
#define OUT_HP 0.99792     /* the output's high-pass, a sample (10 ms) */

void sound_reset(Sound *s)
{
    memset(s, 0, sizeof *s);
    /* the hit's filter, (s^2 + s/Qz) / (s^2 + s/Qp + 1) at HIT_PEAK, bilinear */
    double k = 1 / tan(M_PI * HIT_PEAK / SOUND_RATE), a0 = k * k + k / HIT_PEAK_Q + 1;
    s->hit_b[0] = (k * k + k / HIT_ZERO_Q) / a0;
    s->hit_b[1] = -2 * k * k / a0;
    s->hit_b[2] = (k * k - k / HIT_ZERO_Q) / a0;
    s->hit_a[0] = (2 - 2 * k * k) / a0;
    s->hit_a[1] = (k * k - k / HIT_PEAK_Q + 1) / a0;
    s->noise = 0;
    s->fire_rate = FIRE_REST;
    s->fire_level = 1;
}

void sound_write(Sound *s, int latch, uint8_t value)
{
    if (latch < 0 || latch >= SOUND_LATCHES) return;
    s->latch[latch] = latch == SOUND_PITCH ? value : value & 1;
}

static double decay(double seconds) { return exp(-1.0 / (seconds * SOUND_RATE)); }

void sound_render(Sound *s, int16_t *out, int n)
{
    const double swarm_k = 1 - exp(-1.0 / SWARM_LP), hit_k = 1 - exp(-1.0 / HIT_LP);
    const double hit_rel = decay(HIT_RELEASE), fire_rel = decay(FIRE_RELEASE);
    const double fire_up = 1 - decay(FIRE_RISE), fire_down = 1 - decay(FIRE_FALL);
    const uint8_t *l = s->latch;
    for (int i = 0; i < n; i++) {
        /* tone: 32 ticks of 1.536 MHz a sample, the level where the counter is */
        for (int t = 0; t < 32; t++) {
            if (s->tone_count == 0xFF) {
                s->tone_count = l[SOUND_PITCH];
                s->tone_step = (s->tone_step + 1) & 15;
            } else {
                s->tone_count++;
            }
        }
        int q = s->tone_step, q0 = q & 1, q1 = q >> 1 & 1, q2 = q >> 2 & 1, q3 = q >> 3 & 1;
        const double *w = tone_weight[l[SOUND_VOL1] | l[SOUND_VOL2] << 1];
        double x = w[0] * q0 + w[1] * (q1 ^ q2) + w[2] * ((q1 & q2) ^ q3);

        /* swarm */
        int lfo = l[SOUND_LFO0] | l[SOUND_LFO1] << 1 | l[SOUND_LFO2] << 2 | l[SOUND_LFO3] << 3;
        s->lfo_phase += 1.0 / (lfo_period[lfo] * SOUND_RATE);
        if (s->lfo_phase >= 1) s->lfo_phase -= 1;
        double sq = 0;
        for (int k = 0; k < 3; k++) {
            /* phase 0..1 the high half, 1..2 the low half */
            double period = SOUND_RATE / (fs_high[k] - s->lfo_phase * (fs_high[k] - fs_low[k]));
            double high = period - fs_low_time[k];
            s->fs_phase[k] += s->fs_phase[k] < 1 ? 1 / high : 1 / fs_low_time[k];
            if (s->fs_phase[k] >= 2) s->fs_phase[k] -= 2;
            if (l[SOUND_FS1 + k] && s->fs_phase[k] < 1) sq += FS_LEVEL;
        }
        s->swarm_lp += (sq * swarm_gain[l[SOUND_VOL1] | l[SOUND_VOL2] << 1] - s->swarm_lp) * swarm_k;
        x += s->swarm_lp;

        /* the noise generator: a 17-bit shift register */
        if (--s->noise_wait <= 0) {
            s->noise_wait = NOISE_SAMPLES;
            s->noise = (s->noise >> 1) | ((((s->noise >> 12) ^ ~s->noise) & 1) << 16);
        }
        int bit = s->noise & 1;

        /* hit */
        s->hit_env = l[SOUND_HIT] ? 1 : s->hit_env * hit_rel;
        s->hit_lp += ((bit ? HIT_LEVEL : -HIT_LEVEL) * s->hit_env - s->hit_lp) * hit_k;
        double h = s->hit_b[0] * s->hit_lp + s->hit_x[0] * s->hit_b[1] + s->hit_x[1] * s->hit_b[2] -
                   s->hit_a[0] * s->hit_y[0] - s->hit_a[1] * s->hit_y[1];
        s->hit_x[1] = s->hit_x[0], s->hit_x[0] = s->hit_lp;
        s->hit_y[1] = s->hit_y[0], s->hit_y[0] = h;
        x += h;

        /* fire */
        if (l[SOUND_FIRE]) {
            s->fire_env = 1;
            s->fire_rate += (FIRE_TOP - s->fire_rate) * fire_up;
        } else {
            s->fire_env *= fire_rel;
            s->fire_rate += (FIRE_REST - s->fire_rate) * fire_down;
        }
        s->fire_click *= FIRE_DECAY;
        s->fire_phase += 2 * s->fire_rate * (1 + (bit ? FIRE_JITTER : -FIRE_JITTER)) / SOUND_RATE;
        if (s->fire_phase >= 1) { /* an edge of the oscillator */
            s->fire_phase -= 1;
            s->fire_level = -s->fire_level;
            s->fire_click += s->fire_level * FIRE_CLICK * s->fire_env;
        }
        x += s->fire_click;

        /* the output's high-pass */
        s->hp_out = OUT_HP * (s->hp_out + x - s->hp_in);
        s->hp_in = x;
        double y = s->hp_out;
        out[i] = (int16_t)(y > 32767 ? 32767 : y < -32768 ? -32768 : y);
    }
}
