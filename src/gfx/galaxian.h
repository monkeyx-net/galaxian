/*
 * Galaxian's video, drawn a line at a time from the board's tile and object
 * RAM (emu/board.c draws each line as the beam leaves it, so writes during
 * the frame land where they do on the board).
 *
 * The monitor is turned 90 degrees: a board line L (16-239 visible) is screen
 * column 239 - L, and the 256 pixels along it run down the screen. The
 * picture is kept as MAME shows it, at three times the board's pixel clock
 * along a line (`native`, 224 x 768 in screen orientation; the starfield's
 * generator runs at twice the pixel clock), and reduced to the port's
 * 224x256 framebuffer by galaxian_to_fb.
 *
 *   tiles     32x32 tiles of 8x8 (graphics ROMs 1h, 1k: two bitplanes, a
 *             byte per line, bit 7 first along it); tile RAM row r holds
 *             lines 8r-8r+7, column c pixels 8c-8c+7. Column c scrolls by
 *             object RAM 2c (along the line) and takes colour 2c+1
 *   sprites   8 of 16x16 (object RAM $40-$5F)
 *   shells    7 white and the yellow missile (object RAM $60-$7F)
 *   stars     a 17-bit LFSR, two steps a pixel and 512 a line, its origin
 *             moving back one step a frame (the board sets it when the
 *             stars are turned on); a star shows where bit 0 of the line
 *             differs from bit 3 of the pixel, colour from 6 of its bits
 *   palette   the 32-byte PROM: red and green 3 bits, blue 2 (resistor weights)
 */
#ifndef GALAXIAN_GFX_GALAXIAN_H
#define GALAXIAN_GFX_GALAXIAN_H

#include <stdint.h>

#include "assets/rom.h"
#include "gfx/fb.h"

#define GALAXIAN_FIRST_LINE 16
#define GALAXIAN_LAST_LINE 239
#define GALAXIAN_NATIVE_W 224
#define GALAXIAN_NATIVE_H 768 /* three per pixel along a line */
#define GALAXIAN_STAR_PERIOD 131071 /* the starfield generator's: 2^17 - 1 */

typedef struct {
    uint8_t tiles[0x400];
    uint8_t objects[0x100];
    uint8_t stars_on, flip_x, flip_y;
    long star_origin;         /* the generator's step at line 0's first pixel */
    const uint8_t *gfx[2];    /* 1h, 1k */
    uint32_t palette[32];     /* 0xRRGGBB */
    uint32_t native[GALAXIAN_NATIVE_H][GALAXIAN_NATIVE_W];
} GalaxianVideo;

void galaxian_video_init(GalaxianVideo *v, const RomSet *rom);
/* Draw board line `line` (16-239) into native. */
void galaxian_draw_line(GalaxianVideo *v, int line);
/* The picture at the port's resolution (fb scale 1): each pixel from its
   three along the line (a star on any of them shows). */
void galaxian_to_fb(const GalaxianVideo *v, Framebuffer *fb);

#endif
