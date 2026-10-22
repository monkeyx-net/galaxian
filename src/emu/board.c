#include "emu/board.h"

#include <string.h>

static uint8_t mem_read(void *ctx, uint16_t a)
{
    Board *b = ctx;
    switch (a >> 11) {
    case 0x0: case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: case 0x6: case 0x7:
        return a < ROM_PROGRAM_SIZE ? b->rom->program[a] : 0;
    case 0x8: return b->ram[a & 0x3FF];
    case 0xA: return b->video.tiles[a & 0x3FF];
    case 0xB: return b->video.objects[a & 0xFF];
    case 0xC: return b->in0;
    case 0xD: return b->in1;
    case 0xE: return b->in2;
    case 0xF: b->watchdog = 0; return 0xFF;
    default: return 0xFF; /* $4800-$4FFF, $8000-$FFFF */
    }
}

uint8_t board_peek(Board *b, uint16_t addr) { return (addr & 0xF800) == 0x7800 ? 0xFF : mem_read(b, addr); }

/* A sound latch changes at its sample in the frame; the frame's sound is made at its end. */
static void sound_event(Board *b, int64_t at, int latch, uint8_t v)
{
    if (b->sound_event_count == (int)(sizeof b->sound_events / sizeof b->sound_events[0])) return;
    int64_t n = (at - b->frame_start) / SOUND_CYCLES_PER_SAMPLE;
    b->sound_events[b->sound_event_count].sample = (int16_t)(n < 0 ? 0 : n >= BOARD_AUDIO_SAMPLES ? BOARD_AUDIO_SAMPLES - 1 : n);
    b->sound_events[b->sound_event_count].latch = (uint8_t)latch;
    b->sound_events[b->sound_event_count].value = v;
    b->sound_event_count++;
}

/* Draw the frame's visible lines that have started by `cycle`, from the
   video as it is: a write to the video then shows from the next line on. */
static void draw_lines_until(Board *b, int64_t cycle)
{
    while (b->next_line < BOARD_LINES && b->frame_start + (int64_t)b->next_line * BOARD_CYCLES_PER_LINE <= cycle) {
        int line = (BOARD_VBLANK_LINE + b->next_line) % BOARD_LINES;
        if (line >= GALAXIAN_FIRST_LINE && line <= GALAXIAN_LAST_LINE) galaxian_draw_line(&b->video, line);
        b->next_line++;
        if (b->line_hook && b->next_line < BOARD_LINES)
            b->line_hook(b, (BOARD_VBLANK_LINE + b->next_line) % BOARD_LINES);
    }
}

/* The starfield's generator is held clear while the stars are off; turned on,
   it starts from where the beam is: its origin goes back by the steps the
   frame has had so far (512 a line, 6 a CPU cycle along it). */
static void stars_on(Board *b, int64_t at)
{
    int64_t into = at - b->frame_start;
    int line = (int)((BOARD_VBLANK_LINE + into / BOARD_CYCLES_PER_LINE) % BOARD_LINES);
    long steps = (long)line * 512 + (long)(into % BOARD_CYCLES_PER_LINE) * 6;
    b->video.star_origin = ((GALAXIAN_STAR_PERIOD - steps) % GALAXIAN_STAR_PERIOD + GALAXIAN_STAR_PERIOD) %
                           GALAXIAN_STAR_PERIOD;
}

static void mem_write(void *ctx, uint16_t a, uint8_t v)
{
    Board *b = ctx;
    int64_t at = b->cpu.cycles + b->cpu.t;
    if (b->write_hook) b->write_hook(b, a, v);
    switch (a >> 11) {
    case 0x8: b->ram[a & 0x3FF] = v; break;
    case 0xA:
        draw_lines_until(b, at);
        b->video.tiles[a & 0x3FF] = v;
        break;
    case 0xB:
        draw_lines_until(b, at);
        b->video.objects[a & 0xFF] = v;
        break;
    case 0xC:
        b->latch6000[a & 7] = v & 1;
        if ((a & 7) >= 4) sound_event(b, at, SOUND_LFO0 + (a & 3), v);
        break;
    case 0xD:
        b->latch6800[a & 7] = v & 1;
        sound_event(b, at, SOUND_FS1 + (a & 7), v);
        break;
    case 0xE:
        switch (a & 7) {
        case 1: /* NMI enable: off holds the flip-flop clear */
            if (!(v & 1)) z80_set_nmi(&b->cpu, 0);
            break;
        case 4:
            draw_lines_until(b, at);
            if ((v & 1) && !b->video.stars_on) stars_on(b, at);
            b->video.stars_on = v & 1;
            break;
        case 6:
            draw_lines_until(b, at);
            b->video.flip_x = v & 1;
            break;
        case 7:
            draw_lines_until(b, at);
            b->video.flip_y = v & 1;
            break;
        default: break;
        }
        b->latch7000[a & 7] = v & 1;
        break;
    case 0xF:
        b->pitch = v;
        sound_event(b, at, SOUND_PITCH, v);
        break;
    default: break; /* ROM, nothing at $4800-$4FFF or $8000-$FFFF */
    }
}

void board_init(Board *b, const RomSet *rom)
{
    memset(b, 0, sizeof *b);
    b->rom = rom;
    galaxian_video_init(&b->video, rom);
    b->in2 = IN2_DEFAULT;
    b->cpu.ctx = b;
    b->cpu.read = mem_read;
    b->cpu.write = mem_write;
    z80_reset(&b->cpu);
    sound_reset(&b->sound);
}

/* The watchdog: the CPU and the latches start again (RAM keeps its contents). */
static void board_reset(Board *b)
{
    z80_reset(&b->cpu);
    memset(b->latch6000, 0, sizeof b->latch6000);
    memset(b->latch6800, 0, sizeof b->latch6800);
    memset(b->latch7000, 0, sizeof b->latch7000);
    b->pitch = 0;
    b->video.stars_on = b->video.flip_x = b->video.flip_y = 0;
    for (int i = 0; i < SOUND_LATCHES; i++) sound_write(&b->sound, i, 0);
    b->watchdog = 0;
}

static void make_sound(Board *b)
{
    int done = 0;
    for (int i = 0; i <= b->sound_event_count; i++) {
        int at = i < b->sound_event_count ? b->sound_events[i].sample : BOARD_AUDIO_SAMPLES;
        if (at > done) {
            sound_render(&b->sound, b->audio + done, at - done);
            done = at;
        }
        if (i < b->sound_event_count) sound_write(&b->sound, b->sound_events[i].latch, b->sound_events[i].value);
    }
    b->sound_event_count = 0;
}

void board_run_frame(Board *b)
{
    int64_t end = b->frame_start + BOARD_FRAME_CYCLES;
    b->next_line = 0;
    if (b->line_hook) b->line_hook(b, BOARD_VBLANK_LINE);
    /* vertical blank reaches the NMI a cycle before the line starts (as in
       MAME): an instruction ending at or after then is followed by the NMI */
    while (b->cpu.cycles < end - 1) {
        if (b->step_hook) b->step_hook(b);
        z80_step(&b->cpu);
    }
    draw_lines_until(b, end - 1);
    make_sound(b);
    b->frame_start = end;
    b->frame++;
    if (b->latch7000[1]) z80_set_nmi(&b->cpu, 1);
    if (++b->watchdog >= BOARD_WATCHDOG_FRAMES) board_reset(b);
    b->video.star_origin = (b->video.star_origin + GALAXIAN_STAR_PERIOD - 1) % GALAXIAN_STAR_PERIOD;
}
