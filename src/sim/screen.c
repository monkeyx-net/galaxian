#include "sim/screen.h"

#include <stddef.h>

#define QUEUE_TAKE 0x40A1
#define FRAME 0x425F

/* ADD / ADC and DAA, as the Z80 does them: a BCD byte sum and its carry. */
static uint8_t add_daa(uint8_t a, uint8_t b, int *carry)
{
    unsigned sum = a + b + (unsigned)*carry;
    int h = (a & 0x0F) + (b & 0x0F) + *carry > 0x0F, c = sum > 0xFF;
    uint8_t r = (uint8_t)sum, diff = 0;
    if (c || r > 0x99) diff = 0x60, c = 1;
    if (h || (r & 0x0F) > 9) diff |= 0x06;
    *carry = c;
    return (uint8_t)(r + diff);
}

static uint8_t rotr4(uint8_t a) { return (uint8_t)(a >> 4 | a << 4); }

/* ---- tiles ------------------------------------------------------------------- */

/* $25A0 twice ($2585): a 2x2 block of tiles a..a+3 */
static void block(Game *g, uint16_t hl, uint8_t a)
{
    WR(hl, a);
    WR(hl + 1, a + 1);
    WR(hl + 0x20, a + 2);
    WR(hl + 0x21, a + 3);
}

/* $25A9: two tiles a and a+2, a column apart */
static void pair(Game *g, uint16_t hl, uint8_t a)
{
    WR(hl, a);
    WR(hl + 0x20, a + 2);
}

/* $2593: an icon (the ships left): a, a+1 then a-2, a-1 a row before;
   the next goes two rows up */
static uint16_t icon(Game *g, uint16_t hl, uint8_t a)
{
    WR(hl, a);
    WR(hl + 1, a + 1);
    WR(hl - 0x20, a - 2);
    WR(hl - 0x1F, a - 1);
    return (uint16_t)(hl - 0x40);
}

/* $20E1: where a swarm cell's alien is drawn; *odd: its row is odd (a
   half tile on: a block of four, else a pair) */
static uint16_t cell_tiles(uint8_t cell, int *odd)
{
    uint8_t col = cell & 0x0F, c = (uint8_t)(col >> 2 | col << 6);
    uint8_t row = (cell >> 4) & 7;
    *odd = row & 1;
    uint8_t a = (uint8_t)~((row >> 1) + row + (row & 1)) & 0x0F;
    return (uint16_t)(0x5000 + ((c & 3) << 8 | (uint8_t)((c & 0xC0) + a)));
}

/* $211D: the wing beat (0-3) for b (a cell or a column), c a frame back;
   the flagships ($70 on) have one picture */
static uint8_t beat(Game *g, uint8_t b, uint8_t c)
{
    if (b >= 0x70) return 0x80;
    return (uint8_t)(rotr4(RD(FRAME)) + b + c) & 3;
}

/* $2131: an alien's tiles at hl by its beat b */
static void alien(Game *g, uint16_t hl, uint8_t b, int odd)
{
    if (!odd) pair(g, hl, RD(0x2157 + b));
    else block(g, hl, b & 0x80 ? 0xA4 : RD(0x215B + b));
}

/* $205E: a cell emptied */
static void erase_cell(Game *g, uint8_t cell)
{
    int odd;
    uint16_t hl = cell_tiles(cell, &odd);
    if (odd) block(g, hl, 0x2C);
    else pair(g, hl, 0x2C);
}

/* $2055: a cell's alien drawn, beating with its column */
static void draw_cell(Game *g, uint8_t cell)
{
    int odd;
    uint16_t hl = cell_tiles(cell, &odd);
    uint8_t b = 0x80;
    if (cell < 0x70) {
        b = cell & 0x0F;
        b = beat(g, b, (RD(FRAME) & 0x0F) < b ? 0xFF : 0);
    }
    alien(g, hl, b, odd);
}

void screen_idle(Game *g)
{
    uint8_t f = RD(FRAME);
    if (f & 0x0F) {
        if (RD(0x4238) & 1) return;
        uint8_t l = (uint8_t)(0x20 + (f & 0x0F));
        for (int i = 0; i < 6; i++, l = (uint8_t)(l + 0x10)) {
            if (!(RD(0x4100 + l) & 1)) {
                erase_cell(g, l);
            } else {
                int odd;
                uint16_t hl = cell_tiles(l, &odd);
                alien(g, hl, beat(g, l, 0), odd);
            }
        }
        return;
    }
    /* the 1UP (2UP) blinks */
    if (RD(0x4006)) WR(0x40AB, RD(0x4006));
    else if (!RD(0x40AB)) return;
    uint8_t a = RD(0x400D);
    uint16_t hl = a ? 0x50E0 : 0x5340;
    if (f & 0x10) {
        WR(hl, 0x10);
        WR(hl - 0x20, 0x10);
        WR(hl - 0x40, 0x10);
        if (!RD(0x400E)) return;
        a ^= 1;
        hl = a ? 0x50E0 : 0x5340;
    }
    WR(hl, a + 1);
    WR(hl - 0x20, 0x25);
    WR(hl - 0x40, 0x20);
    if (f & 0x10) return;
    if (RD(0x4006)) return;
    WR(0x40AB, 0);
}

/* ---- scores -------------------------------------------------------------------- */

/* $2261: a score's 6 digits (its top byte at `at`) down the screen from ix,
   the first four blank while zero */
static void digits(Game *g, uint16_t at, uint16_t ix)
{
    int c = 4;
    for (int b = 0; b < 3; b++, at--) {
        for (int half = 0; half < 2; half++) {
            uint8_t a = (half ? RD(at) : rotr4(RD(at))) & 0x0F;
            if (a) c = 0;
            else if (c) a = 0x80, c--;
            WR(ix, a + 0x90);
            ix -= 0x20;
        }
    }
}

/* $2256: a player's score (n 0 or not) */
static void player_score(Game *g, uint16_t at, uint8_t n) { digits(g, at, n ? 0x5121 : 0x5381); }

void screen_ships(Game *g, uint8_t b)
{
    uint16_t hl = 0x539E;
    int c = 5;
    if (!RD(0x4200) || --b) /* $22C1: an icon for each ship to come */
        do {
            hl = icon(g, hl, 0x66);
            c--;
        } while (--b);
    while (--c >= 0) hl = icon(g, hl, 0x2E); /* the rest blank */
}

/* $229C: the bonus ship, once a game per player */
static void bonus(Game *g)
{
    uint16_t hl = (uint16_t)(0x40AD + RD(0x400D));
    if (RD(hl) & 1) return;
    WR(hl, 1);
    WR(0x41C7, 1);
    WR(0x421D, RD(0x421D) + 1);
    screen_ships(g, RD(0x421D));
}

/* $2290: the playing player's score */
static uint16_t score_at(Game *g) { return RD(0x400D) ? 0x40A5 : 0x40A2; }

/* $21A6: a score from $22D0's table added (not in the demo) */
static void add_score(Game *g, uint8_t n)
{
    if (RD(0x4007) & 1) return;
    uint16_t de = score_at(g), hl = (uint16_t)(0x22D0 + (uint8_t)(n * 3));
    int carry = 0;
    uint8_t a = 0;
    for (int b = 0; b < 3; b++, de++, hl++) {
        a = add_daa(RD(de), RD(hl), &carry);
        WR(de, a);
    }
    de--; /* the top byte */
    uint8_t thousands = (uint8_t)(a << 4 | RD(de - 1) >> 4);
    if (thousands >= RD(0x40AC)) bonus(g);
    player_score(g, de, RD(0x400D));
    uint16_t hi = 0x40AA;
    for (int b = 0; b < 3; b++, de--, hi--) {
        if (RD(de) < RD(hi)) return;
        if (RD(de) != RD(hi)) {
            game_copy(g, 0x40A8, score_at(g), 3);
            digits(g, 0x40AA, 0x5241);
            return;
        }
    }
}

/* $21FE: a score cleared, and the bonus flag with it */
static void clear_score(Game *g, uint8_t n)
{
    if (n >= 3) {
        for (uint8_t a = (uint8_t)(n - 1);; a--) {
            clear_score(g, a);
            if (!a) return;
        }
    }
    uint16_t hl = 0x40A8, de = 0x40A8;
    if (n == 0) hl = 0x40A2, de = 0x40AD;
    else if (n == 1) hl = 0x40A5, de = 0x40AE;
    WR(hl, 0);
    WR(hl + 1, 0);
    WR(hl + 2, 0);
    WR(de, 0);
    screen_score(g, n);
}

void screen_score(Game *g, uint8_t n)
{
    if (n >= 3) {
        for (uint8_t a = (uint8_t)(n - 1);; a--) {
            screen_score(g, a);
            if (!a) return;
        }
    }
    if (n == 0) player_score(g, 0x40A4, 0);
    else if (n == 2) digits(g, 0x40AA, 0x5241);
    else if (RD(0x400E)) player_score(g, 0x40A7, RD(0x400E));
}

/* ---- text ---------------------------------------------------------------------- */

void screen_text(Game *g, uint8_t n)
{
    uint16_t entry = game_rd16(g, (uint16_t)(0x235C + (uint8_t)(n * 2 & 0x3F)));
    uint16_t hl = game_rd16(g, entry), de = (uint16_t)(entry + 2);
    if (n & 0x80) { /* erased */
        for (; RD(de) != 0x3F; de++, hl -= 0x20) WR(hl, 0x40);
        return;
    }
    if (!(n & 0x40)) {
        for (uint8_t a; (a = (uint8_t)(RD(de) - 0x30)) != 0x0F; de++, hl -= 0x20) WR(hl, a);
        return;
    }
    /* typed out by the NMI ($18C0): its column scrolled in as it goes */
    game_wr16(g, 0x40B5, hl);
    game_wr16(g, 0x40B3, de);
    uint8_t e = (uint8_t)hl, d = (uint8_t)(hl >> 8), b = e & 0x1F;
    uint16_t counter = (uint16_t)(0x4000 | (uint8_t)(b * 2 + 0x20));
    game_wr16(g, 0x40B1, counter);
    e >>= 2;
    uint8_t c = (uint8_t)(((d & 3) << 6 | e) & 0xF8);
    uint16_t at = (uint16_t)(0x5000 + b);
    for (int i = 0; i < 32; i++, at += 0x20) WR(at, 0x10); /* the column cleared */
    WR(counter, c);
    WR(0x40B0, 1);
}

/* ---- the other commands ---------------------------------------------------------- */

/* $2187: the ship's place blanked */
static void ship_blank(Game *g)
{
    uint16_t hl = 0x51DA;
    for (int c = 0; c < 4; c++, hl += 0x1C)
        for (int b = 0; b < 4; b++) WR(hl++, 0x40);
}

/* $215F: the ship's explosion, frame n (0 the ship again, 1 gone) */
static void ship_frame(Game *g, uint8_t n)
{
    if (n == 0) {
        ship_blank(g);
        block(g, 0x51FC, 0x60);
        return;
    }
    if (n == 1) {
        ship_blank(g);
        return;
    }
    uint8_t a = (uint8_t)((uint8_t)~((uint8_t)(n - 2) << 4) & 0x30) + 0xC0;
    block(g, 0x51DA, a);
    block(g, 0x51DC, a + 4);
    block(g, 0x521A, a + 8);
    block(g, 0x521C, a + 12);
}

/* $2569: a byte as BCD (the high nibble's 16s added in decimal); *low is
   the low nibble's (register C, which the flags go on counting with) */
static uint8_t to_bcd(uint8_t a, uint8_t *low)
{
    int carry = 0;
    uint8_t c = add_daa(a & 0x0F, 0, &carry);
    if (low) *low = c;
    uint8_t r = 0;
    if (a & 0xF0)
        for (int b = a >> 4; b > 0; b--) {
            carry = 0;
            r = add_daa(r, 0x16, &carry);
        }
    carry = 0;
    return add_daa(r, c, &carry);
}

/* $2520: the waves' flags, bottom right: big ones for tens, small for ones */
static void flags(Game *g)
{
    if (RD(0x4007) & 1) return;
    if (RD(0x4220)) WR(0x41D0, 1);
    uint8_t a = (uint8_t)(RD(0x421C) + 1), c;
    if (a >= 0x30) a = 0x30;
    a = to_bcd(a, &c);
    uint16_t hl = 0x507E;
    if (a & 0xF0) {
        c = 0x10;
        for (int b = a >> 4; b > 0; b--) {
            block(g, hl, 0x68);
            hl += 0x40;
            c -= 2;
        }
    }
    for (int b = a & 0x0F; b > 0; b--) {
        WR(hl, 0x6C);
        WR(hl + 1, 0x6D);
        hl += 0x20;
        c--;
    }
    while ((int8_t)--c >= 0) {
        WR(hl, 0x2C);
        WR(hl + 1, 0x2D);
        hl += 0x1F + 1;
    }
}

/* $24EB: the credits (or free play), outside a game */
static void credits(Game *g)
{
    if (RD(0x4006) & 1) return;
    if ((RD(0x4011) & 0xC0) == 0xC0) {
        screen_text(g, 0x10);
        return;
    }
    screen_text(g, 5);
    uint8_t a = RD(0x4002);
    if (a >= 0x63) a = 0x63;
    a = to_bcd(a, NULL);
    if (a & 0xF0) WR(0x529F, a >> 4);
    WR(0x527F, a & 0x0F);
}

/* $24C8: the bonus line, if there is a bonus */
static void bonus_text(Game *g)
{
    uint8_t v = RD(0x40AC);
    if (v == 0xFF) return;
    screen_text(g, 6);
    WR(0x5138, v & 0x0F);
    uint8_t a = v & 0xF0;
    if (!a) a = 1;
    WR(0x5158, rotr4(a));
}

void screen_command(Game *g, uint8_t cmd, uint8_t arg)
{
    switch (cmd & 7) {
    case 0: draw_cell(g, arg); break;
    case 1: erase_cell(g, arg); break;
    case 2: ship_frame(g, arg); break;
    case 3: add_score(g, arg); break;
    case 4: clear_score(g, arg); break;
    case 5: screen_score(g, arg); break;
    case 6: screen_text(g, arg); break;
    default:
        switch (arg) {
        case 0: flags(g); break;
        case 1: credits(g); break;
        case 2: bonus_text(g); break;
        default:
            if (!(RD(0x4007) & 1)) screen_ships(g, RD(0x421D));
            break;
        }
        break;
    }
}

void screen_start(Game *g) { game_fill(g, 0x40A2, 0, 0x1E); }

void screen_run(Game *g)
{
    for (;;) {
        uint8_t l = RD(QUEUE_TAKE);
        uint8_t cmd = RD(0x4000 | l);
        if (cmd & 0x80) break; /* empty */
        WR(0x4000 | l, 0xFF);
        l++;
        uint8_t arg = RD(0x4000 | l);
        WR(0x4000 | l, 0xFF);
        l++;
        WR(QUEUE_TAKE, l < 0xC0 ? 0xC0 : l);
        screen_command(g, cmd, arg);
    }
    screen_idle(g);
}
