/*
 * Controls, sampled once per 60 Hz frame. Galaxian's upright cabinet has
 * one control panel (the players take turns): a 2-way left/right joystick
 * and a fire button. Pads go through SDL_GameController
 * (SDL_GAMECONTROLLERCONFIG / gamecontrollerdb.txt); every connected pad
 * drives the panel.
 *
 *              pad                          keyboard
 *   left/right left stick, D-pad            Left Right
 *              (turned with the screen: rotate 90 or 270, the device on its side)
 *   fire       A or B                       Space or Ctrl
 *   coin       Select                       5
 *   start      Start (1P), L1+Start (2P)    1 or Enter / 2
 *   graphics   Select+L1                    F3 (F4 glow, F5 scanlines)
 *   screen     the operator menu            F6 scale mode, F7 rotation
 *   operator menu Select+R1               F2 (up/down, left/right in it)
 *   quit       hold Select+Start 1 s        Esc
 */
#ifndef GALAXIAN_INPUT_H
#define GALAXIAN_INPUT_H

#include <stdbool.h>

/* Frames Select+Start must be held to quit (1 s). */
#define QUIT_HOLD_FRAMES 60
#define MAX_PADS 2

typedef struct {
    int move_x;          /* -1 left, 0, 1 right (the arcade's, the screen turned) */
    int move_y;          /* -1 up, 0, 1 down (the operator menu) */
    bool fire;
    bool coin;
    bool start;          /* 1-player start */
    bool start2;         /* 2-player start */
    bool quit;           /* Select+Start held QUIT_HOLD_FRAMES, Esc, or window close */
    bool gfx_next;       /* Select+L1 or F3, edge-triggered: next graphics preset */
    bool gfx_glow;       /* F4, edge-triggered */
    bool gfx_scanlines;  /* F5, edge-triggered */
    bool screen_mode;    /* F6, edge-triggered: the next scale mode */
    bool screen_rotate;  /* F7, edge-triggered: the next rotation */
    bool menu;           /* Select+R1 or F2, edge-triggered: the operator menu */
    int pads;            /* game controllers connected */
} InputFrame;

void input_init(void);
/* The screen's rotation (0, 90, 270 clockwise): the directions turn with it. */
void input_set_rotation(int degrees);
/* Pump SDL events and build this frame's input. */
void input_poll(InputFrame *out);
void input_shutdown(void);

#endif
