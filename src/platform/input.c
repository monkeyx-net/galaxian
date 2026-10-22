#include "platform/input.h"

#include <SDL.h>

#define STICK_DEAD_ZONE 12000

static SDL_GameController *pads[MAX_PADS];
static int quit_hold;
static bool gfx_was_held[3], menu_was_held;
static bool quit_requested;

static void open_pad(int index)
{
    if (!SDL_IsGameController(index)) return;
    for (int i = 0; i < MAX_PADS; i++) {
        if (!pads[i]) {
            pads[i] = SDL_GameControllerOpen(index);
            return;
        }
    }
}

static void close_pad(SDL_JoystickID id)
{
    for (int i = 0; i < MAX_PADS; i++) {
        if (pads[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[i])) == id) {
            SDL_GameControllerClose(pads[i]);
            pads[i] = NULL;
        }
    }
}

void input_init(void)
{
    const char *db = SDL_getenv("GALAXIAN_GAMECONTROLLERDB");
    if (db) SDL_GameControllerAddMappingsFromFile(db);
    /* Pads present at start arrive as SDL_CONTROLLERDEVICEADDED events. */
}

static int clamp1(int v)
{
    return v < -1 ? -1 : v > 1 ? 1 : v;
}

static int axis_dir(Sint16 v)
{
    return v < -STICK_DEAD_ZONE ? -1 : v > STICK_DEAD_ZONE ? 1 : 0;
}

void input_poll(InputFrame *out)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            quit_requested = true;
            break;
        case SDL_CONTROLLERDEVICEADDED:
            open_pad(e.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            close_pad(e.cdevice.which);
            break;
        default:
            break;
        }
    }

    InputFrame in = {0};
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    in.move_x = k[SDL_SCANCODE_RIGHT] - k[SDL_SCANCODE_LEFT];
    in.move_y = k[SDL_SCANCODE_DOWN] - k[SDL_SCANCODE_UP];
    in.fire = k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_LCTRL] || k[SDL_SCANCODE_RCTRL];
    in.coin = k[SDL_SCANCODE_5];
    in.start = k[SDL_SCANCODE_1] || k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_KP_ENTER];
    in.start2 = k[SDL_SCANCODE_2];
    bool select_held = false, start_held = false, l1_held = false, r1_held = false;

    for (int i = 0; i < MAX_PADS; i++) {
        SDL_GameController *c = pads[i];
        if (!c) continue;
        in.pads++;
        int mx = axis_dir(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX))
               + SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
               - SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        if (mx) in.move_x = mx;
        int my = axis_dir(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY))
               + SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN)
               - SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP);
        if (my) in.move_y = my;
        select_held |= SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_BACK);
        start_held |= SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_START);
        l1_held |= SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        r1_held |= SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        in.fire |= SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_A) ||
                   SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_B);
    }
    in.move_x = clamp1(in.move_x);
    in.move_y = clamp1(in.move_y);

    /* Select+Start held 1 s quits; until then neither counts as coin/start. */
    if (select_held && start_held) {
        quit_hold++;
    } else {
        quit_hold = 0;
        in.coin |= select_held && !l1_held && !r1_held;
        if (start_held && l1_held) in.start2 = true;
        else in.start |= start_held;
    }

    bool gfx_held[3] = { (select_held && l1_held) || k[SDL_SCANCODE_F3], k[SDL_SCANCODE_F4], k[SDL_SCANCODE_F5] };
    in.gfx_next = gfx_held[0] && !gfx_was_held[0];
    in.gfx_glow = gfx_held[1] && !gfx_was_held[1];
    in.gfx_scanlines = gfx_held[2] && !gfx_was_held[2];
    for (int i = 0; i < 3; i++) gfx_was_held[i] = gfx_held[i];
    bool menu_held = (select_held && r1_held) || k[SDL_SCANCODE_F2];
    in.menu = menu_held && !menu_was_held;
    menu_was_held = menu_held;

    in.quit = quit_requested || quit_hold >= QUIT_HOLD_FRAMES || k[SDL_SCANCODE_ESCAPE];
    *out = in;
}

void input_shutdown(void)
{
    for (int i = 0; i < MAX_PADS; i++) {
        if (pads[i]) SDL_GameControllerClose(pads[i]);
        pads[i] = NULL;
    }
}
