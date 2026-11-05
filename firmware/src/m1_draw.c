/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular One on the SMK-37: the screen. A little black cat lives in it.
 *   the room     240 x 240: an 80 x 80 grid of 3 x 3 px cells. Old paper (a grain in it, the corners a little darker);
 *                the bottom of the screen is his floor: Modular Town's grass (its greens, a few small flowers), a
 *                few blades in front of his paws. Fader 1 brings the
 *                night (stars come out), fader 2 the snow (the FM winter).
 *   the cat      clips of frames (cat_sprites.h, tools/gen_cat.py, 80 x 80 cells, his feet on the bottom row): he
 *                sits and breathes, listens
 *                when you play (eyes ^^, notes), looks at a control you touch, and when you leave him alone he lives
 *                his life (griffes on the edge of the screen, toilette, roulade, la folie, chasse, mlem, ...). At night
 *                he falls asleep; playing wakes him.
 *   the words    at the top, big, in the pixel letters of Modular One (Silkscreen, m1_pixfont.h), only for a moment:
 *                the sound when a pad picks it, the control touched (its name, its value under it, bigger).
 * Only what changed goes to the LCD: the rows the cat moved in, the words when they change; the whole room
 * when the light changes or while snow falls. GLOBE (the settings) is a page of its own, in the same letters. */
#include "cat_sprites.h"                         /* (tools/gen_pets.py: the cat, the fox, the robot, the fish) */
#include "m1_pixfont.h"

#define M1_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
/* a colour between a and b (pct of the way to b), always in colour: not gfx.c's ux_mix, which turns everything grey
 * under Felucca's GREY display style (a setting the keyboard may keep: the paper and the grass came out grey) */
static uint16_t m1_mix(uint16_t a, uint16_t b, int32_t pct)
{
    static const uint8_t SH[3] = {11, 5, 0}, MK[3] = {31, 63, 31};
    uint32_t out = 0, k;
    for (k = 0; k < 3u; k++) {
        int32_t x = (a >> SH[k]) & MK[k], d = (((b >> SH[k]) & MK[k]) - x) * pct;
        out |= (uint32_t)(x + (d + (d >= 0 ? 50 : -50)) / 100) << SH[k];
    }
    return (uint16_t)out;
}
#define ROOM_G 80u                               /* the room's rows (the cat's: CAT_SCENE) */
#define CAP_GY 25u                               /* the words: grid rows 0 .. 24 (screen 0 .. 74) */


static uint8_t cat_buf[CAT_G * CAT_SCENE] __attribute__((section(".pool")));
static struct {
    uint8_t clip, step, behaving;
    uint32_t t_next, quiet_since, next_life;
    uint8_t box[4];                               /* the frame on screen: x y w h */
    int16_t night, winter;                        /* the light drawn (-1: never) */
    uint32_t snow_t, rnd;
    uint8_t full;                                 /* the whole room to draw */
    uint32_t cap_sig;
    uint32_t beat;                                /* DANSE: the beat its step was taken on */
    uint8_t away;                                 /* (the FM-1's tape plays: he is off the screen, but passing) */
    uint32_t next_pass;
} cat = {.night = -1, .winter = -1, .rnd = 0x2545F491u, .full = 1, .cap_sig = 0xFFFFFFFFu};

static struct { uint8_t x, y, dx; } snow[32];

static uint32_t cat_rand(uint32_t n)
{
    uint32_t x = cat.rnd;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    cat.rnd = x;
    return n ? x % n : x;
}

/* ------------------------------------------------------------ the pixel letters */
static int32_t pf_width_s(const char *s, int32_t sc)
{
    int32_t w = 0;
    for (; *s; s++)
        w += sc * PF_ADV[((uint8_t)*s - 32u) < 95u ? (uint8_t)*s - 32u : 0u];
    return w;
}
static int32_t pf_width(const char *s) { return pf_width_s(s, 2); }

/* s at sc x into a buffer of bw x bh (colour as the LCD takes it: swapped), its top-left at x, y (may be outside) */
static void pf_draw_s(uint16_t *buf, uint32_t bw, uint32_t bh, int32_t x, int32_t y, const char *s, uint16_t c,
                      int32_t sc)
{
    for (; *s; s++) {
        uint32_t g = (uint8_t)*s - 32u;
        int32_t r, k;
        if (g >= 95u)
            g = 0;
        for (r = 0; r < 5 * sc; r++) {
            int32_t yy = y + r;
            uint32_t bits = PF_ROWS[g][r / sc];
            if (yy < 0 || yy >= (int32_t)bh || !bits)
                continue;
            for (k = 0; k < 8 * sc; k++)
                if (bits & (0x80u >> (k / sc))) {
                    int32_t xx = x + k;
                    if (xx >= 0 && xx < (int32_t)bw)
                        buf[(uint32_t)yy * bw + (uint32_t)xx] = c;
                }
        }
        x += sc * PF_ADV[g];
    }
}
static void pf_draw(uint16_t *buf, uint32_t bw, uint32_t bh, int32_t x, int32_t y, const char *s, uint16_t c)
{
    pf_draw_s(buf, bw, bh, x, y, s, c, 2);
}
/* the biggest scale, at most hi, at which s fits in w */
static int32_t pf_fit(const char *s, int32_t w, int32_t hi)
{
    while (hi > 2 && pf_width_s(s, hi) > w)
        hi--;
    return hi;
}

static void pf_rect(uint16_t *buf, uint32_t bw, uint32_t bh, int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t i, j;
    for (j = y; j < y + h; j++)
        if (j >= 0 && j < (int32_t)bh)
            for (i = x; i < x + w; i++)
                if (i >= 0 && i < (int32_t)bw)
                    buf[(uint32_t)j * bw + (uint32_t)i] = c;
}

/* ------------------------------------------------------------ frames: a key, or a patch on a key */
/* the companion (GLOBE COMPANION): its pictures and clips; the life below is the same for all */
static const pet_t *PET = &PETS[0];
#define CAT_PAL (PET->pal)
#define CAT_DATA (PET->data)
#define CAT_FRAMES (PET->frames)
#define CAT_STEPS (PET->steps)
#define CAT_CLIPS (PET->clips)

static uint32_t pet_n(void) { return PET_N; }
static const char *pet_name(uint32_t v) { return PETS[v % PET_N].name; }

static void cat_decode(const cat_frame_t *f)
{
    const uint8_t *d = CAT_DATA + f->off;
    uint32_t n = f->len, p = 0;
    while (n--) {
        uint32_t b = *d++;
        if (b & 0x80u) {
            p += (b & 0x7Fu) + 1u;
        } else {
            uint32_t k = (b >> 4) + 1u, v = b & 15u;
            while (k-- && p < sizeof cat_buf)
                cat_buf[p++] = (uint8_t)v;
        }
    }
}

/* frame fi, moved dx cells sideways (a walk: one picture moved; what goes past the edge is cut) */
static void cat_frame(uint32_t fi, int32_t dx)
{
    const cat_frame_t *f = &CAT_FRAMES[fi];
    uint32_t i;
    for (i = 0; i < sizeof cat_buf; i++)
        cat_buf[i] = 0;
    if (f->base != 0xFFFFu)
        cat_decode(&CAT_FRAMES[f->base]);
    cat_decode(f);
    if (dx) {
        uint32_t y;
        int32_t x;
        for (y = 0; y < CAT_SCENE; y++) {
            uint8_t *row = cat_buf + y * CAT_G;
            if (dx > 0) {
                for (x = CAT_G - 1; x >= 0; x--)
                    row[x] = x - dx >= 0 ? row[x - dx] : 0;
            } else {
                for (x = 0; x < (int32_t)CAT_G; x++)
                    row[x] = x - dx < (int32_t)CAT_G ? row[x - dx] : 0;
            }
        }
    }
}

/* ------------------------------------------------------------ the floor's words */
static const char *const M1_F_NAME[4] = {"DAY/NIGHT", "FM", "TIME", "ECHO"};
static const char *const M1_B_NAME[18] = {"WAVE 1", "WAVE 2", "", "", "OCTAVE", "OCTAVE", "ARP", "REPEAT", "SCALE",
                                          "CHORD", "SETTINGS", "", "", "", "VINTAGE", "", "", ""};
static struct {
    char lab[20], val[20], head[24];
    const char *snd;
    int16_t bar;                                  /* 0..127, -1 none */
    uint8_t show, page;                           /* page: the FM-1's page, its four knobs under its name */
} cap;

/* the opening (5 s, a note or a touch skips it): BUNGLE SOCIETY comes down from the top to the middle, as on the
 * FM-1's Modular One; the cat asleep on the grass below, his zzz; then he wakes up */
#define SPL_MS 5000u
#define SPL_STOP 2000                            /* (the descent, 2 s; then it stays 3 s in the middle) */
#define SPL_Y 104                                /* (just clear of his zzz) */
static struct { uint8_t on, started; uint32_t t0; int16_t y; } spl = {1, 0, 0, -20};
static int32_t spl_y(int32_t t) { return t >= SPL_STOP ? SPL_Y : -14 + (SPL_Y + 14) * t / SPL_STOP; }

static uint32_t m1_hash(uint32_t h, const char *t)
{
    while (*t)
        h = (h ^ (uint8_t)*t++) * 16777619u;
    return h * 16777619u;
}

/* what the floor says now; 1 = it changed */
static int m1_caption(void)
{
    uint32_t sig;
    cap.lab[0] = cap.val[0] = cap.head[0] = 0;
    cap.page = 0;
    cap.bar = -1;
    cap.snd = N_VILLE_MODE[M1T->p[P_E0] % VM_N];
    cap.show = m1.touched != M1T_NONE && fm1_ms - m1.t_ms < 2500u;
    if (cap.show) {
        switch (m1.touched) {
        case M1T_PAD:                                                        /* the sound picked, a moment */
            str_cpy(cap.lab, cap.snd, sizeof cap.lab);
            if (m1.value)
                str_cpy(cap.val, "SAVED", sizeof cap.val);                   /* (the FM-1: yours, SAVE) */
            break;
        case M1T_KNOB: str_cpy(cap.lab, m1_knob_name(m1.what), sizeof cap.lab); cap.bar = m1.value; break;
        case M1T_PAGE:                                                       /* (the FM-1: a page, its knobs) */
            str_cpy(cap.lab, m1_page_name(m1.what), sizeof cap.lab);
            cap.page = 1;
            break;
        case M1T_FADER:
            str_cpy(cap.lab, m1.what == 1u && VM_TOWN((uint32_t)M1T->p[P_E0]) ? "RISE" : M1_F_NAME[m1.what & 3u],
                    sizeof cap.lab);
            cap.bar = m1.value;
            break;
        case M1T_VOL: str_cpy(cap.lab, "VOLUME", sizeof cap.lab); cap.bar = m1.value; break;
        case M1T_HOLD:
            str_cpy(cap.lab, m1.hlab ? m1.hlab : "", sizeof cap.lab);
            str_cpy(cap.val, m1.hval, sizeof cap.val);
            break;
        case M1T_WAVE:
            str_cpy(cap.lab, m1.what == 1u ? "WAVE 1" : "WAVE 2", sizeof cap.lab);
            str_cpy(cap.val, m1.value < 22 ? "SAW" : m1.value < 54 ? "MIX" : m1.value < 96 ? "TRIANGLE" : "SQUARE",
                    sizeof cap.val);
            break;
        default:
            str_cpy(cap.lab, m1.what < 18u ? M1_B_NAME[m1.what] : "", sizeof cap.lab);
            if (m1.what == FM1_BTN_OCT_DOWN || m1.what == FM1_BTN_OCT_UP) {
                cap.val[0] = '+';
                fmt_int(cap.val + (m1.value > 0), m1.value);
            } else {
                str_cpy(cap.val, m1.value ? "ON" : "OFF", sizeof cap.val);
            }
            break;
        }
        if (!cap.lab[0])
            cap.show = 0;
    }
    if (cap.bar >= 0 && !cap.val[0])                                         /* the number, as Modular One shows it */
        fmt_int(cap.val, cap.bar);
    sig = m1_hash(m1_hash(m1_hash(m1_hash(2166136261u, cap.lab), cap.val), cap.head), cap.snd) ^
          (uint32_t)(cap.bar + 1) ^ ((uint32_t)m1.pad << 24) ^ (cap.show ? 0x40000000u : 0u) ^
          (cap.page ? 0x20000000u ^ (uint32_t)m1.what << 16 : 0u);
    if (sig == cat.cap_sig)
        return 0;
    cat.cap_sig = sig;
    return 1;
}

/* ------------------------------------------------------------ the room */
static const uint8_t STARS[18][2] = {
    {5, 6}, {14, 15}, {22, 4}, {31, 10}, {44, 3}, {52, 12}, {60, 5}, {70, 9}, {76, 20}, {9, 28},
    {66, 30}, {38, 22}, {3, 44}, {74, 40}, {27, 34}, {57, 24}, {48, 40}, {18, 46},
};
/* the paper (warm as the pads' white: the panel shows it whiter than it is); the grass (Modular Town's GREEN2 GREEN GREEN3, tools/gen_city.py) and its flowers */
enum { RC_WALL, RC_GRAIN, RC_TIP, RC_BLADE, RC_DEEP, RC_FPINK, RC_FYEL, RC_FWHITE, RC_N };
/* day, snow (the day a shade lighter: the snow is the flakes and the grass, the paper stays warm), night */
static const uint8_t ROOM_DAY[RC_N][3] = {{0xc8, 0x9a, 0x62}, {0xbc, 0x8e, 0x58}, {122, 150, 96}, {98, 128, 84},
                                          {78, 106, 70}, {246, 200, 206}, {236, 210, 120}, {230, 230, 220}};
static const uint8_t ROOM_SNOW[RC_N][3] = {{0xce, 0xa4, 0x70}, {0xc4, 0x98, 0x66}, {226, 228, 220}, {150, 168, 136},
                                           {112, 132, 100}, {240, 214, 218}, {236, 222, 170}, {240, 240, 234}};
static const uint8_t ROOM_NIGHT[RC_N][3] = {{0x26, 0x22, 0x40}, {0x22, 0x1e, 0x3a}, {0x2c, 0x3a, 0x3c}, {0x22, 0x30, 0x32},
                                            {0x1a, 0x26, 0x2a}, {0x5a, 0x44, 0x5c}, {0x56, 0x50, 0x48}, {0x50, 0x50, 0x5c}};

/* the grass, column by column: its height (5 .. 8 cells, a blade now and then 11), a flower on top of some */
static inline uint32_t grass_h(uint32_t x)
{
    uint32_t h = (x * 2654435761u) >> 27;                                    /* 0 .. 31 */
    return h < 3u ? 11u : 5u + h % 4u;
}
static inline uint32_t grass_kind(uint32_t x, uint32_t y)                   /* y: grid row; RC_WALL = no grass */
{
    uint32_t h = grass_h(x), top = ROOM_G - h, r = (x * 73856093u ^ y * 19349663u) * 0x5bd1e995u >> 28;
    if (y < top)
        return RC_WALL;
    if (y == top) {
        uint32_t f = ((x + 7u) * 2654435761u >> 20) % 23u;
        return f == 0u ? RC_FPINK : f == 7u ? RC_FYEL : f == 15u ? RC_FWHITE : RC_TIP;
    }
    return y >= ROOM_G - 1u || r < 4u ? RC_DEEP : RC_BLADE;
}
static inline int grass_front(uint32_t x) { return ((x * 2246822519u) >> 29) < 3u; }   /* ~3 in 8 columns */

/* the aquarium (a companion of the water, the fish): the screen is its glass. Water (light glints in it), the sand and
 * its pebbles and shells, the weed; the same kinds as the room's (wall, grain, tip, blade, deep, and the three
 * flowers: a pebble, a shell, the weed) */
static const uint8_t AQ_DAY[RC_N][3] = {{0x7e, 0xc3, 0xd8}, {0x94, 0xd0, 0xe2}, {0xe3, 0xc9, 0x9a}, {0xd2, 0xb4, 0x83},
                                        {0xb8, 0x98, 0x6a}, {0x8e, 0x86, 0x7e}, {0xf2, 0xe4, 0xcc}, {0x4e, 0x9a, 0x58}};
static const uint8_t AQ_SNOW[RC_N][3] = {{0x9c, 0xd2, 0xe2}, {0xb0, 0xdc, 0xea}, {0xec, 0xdc, 0xc0}, {0xdc, 0xc6, 0x9e},
                                         {0xc4, 0xaa, 0x80}, {0x9c, 0x96, 0x90}, {0xf4, 0xec, 0xdc}, {0x6a, 0xa8, 0x72}};
static const uint8_t AQ_NIGHT[RC_N][3] = {{0x14, 0x24, 0x40}, {0x1a, 0x2c, 0x4a}, {0x4a, 0x44, 0x40}, {0x3c, 0x38, 0x36},
                                          {0x30, 0x2c, 0x2c}, {0x2c, 0x2a, 0x30}, {0x56, 0x52, 0x50}, {0x1c, 0x3a, 0x2a}};
static const uint8_t AQ_WEED[7][2] = {{6, 18}, {9, 24}, {13, 14}, {60, 16}, {66, 26}, {69, 20}, {74, 12}};   /* column, height */
static inline uint32_t sand_kind(uint32_t x, uint32_t y)
{
    uint32_t k, top = ROOM_G - 4u - ((x * 2654435761u) >> 31), r = (x * 73856093u ^ y * 19349663u) * 0x5bd1e995u >> 28;
    for (k = 0; k < 7u; k++) {                                               /* the weed, waving up from the sand */
        uint32_t h = AQ_WEED[k][1], b = AQ_WEED[k][0] + ((y / 4u) & 1u);
        if (y + h >= ROOM_G && y < top + 1u && (x == b || (x == b + 1u && y % 4u == 1u)))
            return RC_FWHITE;
    }
    if (y < top)
        return RC_WALL;
    if (y == top)
        return r == 3u ? RC_FPINK : r == 9u ? RC_FYEL : RC_TIP;
    return r < 3u ? RC_DEEP : RC_BLADE;
}

static uint16_t room_col(uint32_t k, int32_t w, int32_t n)
{
    int aq = PET->water;
    const uint8_t *dc = aq ? AQ_DAY[k] : ROOM_DAY[k], *sc = aq ? AQ_SNOW[k] : ROOM_SNOW[k], *nc = aq ? AQ_NIGHT[k] : ROOM_NIGHT[k];
    uint16_t d = M1_RGB(dc[0], dc[1], dc[2]);
    uint16_t s = M1_RGB(sc[0], sc[1], sc[2]);
    uint16_t nn = M1_RGB(nc[0], nc[1], nc[2]);
    return m1_mix(m1_mix(d, s, w * 100 / 127), nn, n * 100 / 127);
}

/* the ink of the words: sepia by day, old paper by night; the sound's colour, warmed towards the ink */
static void room_ink(uint16_t *ink, uint16_t *acc, uint16_t *dim)
{
    int32_t n = cat.night < 0 ? 0 : cat.night;
    const uint8_t *rgb = M1_PAD_RGB[m1.pad];
    uint16_t day = M1_RGB(0x2a, 0x1a, 0x10), ngt = M1_RGB(0xe6, 0xd6, 0xb2), pad = M1_RGB(rgb[0], rgb[1], rgb[2]);
    *ink = n > 64 ? ngt : day;
    *acc = n > 64 ? m1_mix(pad, *ink, 70) : M1_RGB(0x5c, 0x1e, 0x08);   /* the value: a burnt orange by day */
    *dim = m1_mix(room_col(RC_WALL, cat.winter < 0 ? 0 : cat.winter, n), *ink, 30);
}

/* the paper's grain: a few cells a shade darker, always the same ones */
static inline int room_grain(uint32_t x, uint32_t y)
{
    uint32_t h = (x * 73856093u) ^ (y * 19349663u);
    h ^= h >> 13;
    return (h * 0x5bd1e995u >> 28) < 3u;                                     /* ~ 3 in 16 */
}

#ifdef M1_TAPE
/* ------------------------------------------------------------ the tape: while it plays, the deck fills the screen,
 * drawn in the room's cells (as the companion: the same grain, the same ink); now and then he passes in front */
#define DK_LX 22                                 /* the reels' centres (cells), their flanges' radius */
#define DK_RX 58
#define DK_Y 41
#define DK_R 15
enum { DK_BG, DK_INK, DK_FACE, DK_TAPE, DK_N };
static struct { int32_t t[2]; int32_t cx[2][3], sy[2][3]; } dk;
static int dk_on(void) { return demo_on && !spl.on; }

/* the reels' spokes and the tape wound on each, for the picture being drawn */
static void dk_setup(void)
{
    const pl_item_t *it = &pl_item[pl_idx % PL_MAX];
    uint32_t el = demo_pos / 441u, dur = it->dur10, p = dur ? (el >= dur ? 1000u : el * 1000u / dur) : 0u, r, k;
    uint32_t ang = demo_pos * 1715u;                                         /* (a turn every ~2.5 s) */
    dk.t[0] = 5 + (int32_t)((1000u - p) * 8u / 1000u);                     /* the tape goes from the left reel .. */
    dk.t[1] = 5 + (int32_t)(p * 8u / 1000u);                                /* .. to the right one */
    for (r = 0; r < 2u; r++)
        for (k = 0; k < 3u; k++) {
            uint32_t a = ang + r * 0x2AAAAAAAu + k * 0x55555555u;
            dk.cx[r][k] = sine_i(a + 0x40000000u) >> 7;                     /* (Q8) */
            dk.sy[r][k] = sine_i(a) >> 7;
        }
}
/* x of the line (x0, y0) .. (x1, y1) at row y (y0 < y1) */
static int dk_on_line(int32_t x, int32_t y, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    int32_t xl;
    if (y < y0 || y > y1)
        return 0;
    xl = x0 + ((x1 - x0) * (y - y0) * 2 + (y1 - y0)) / (2 * (y1 - y0));
    return x == xl || (y == y1 && (x - xl) * (x - xl) <= 1);
}
static uint32_t dk_cell(int32_t x, int32_t y)
{
    uint32_t r;
    for (r = 0; r < 2u; r++) {                                               /* the reels */
        int32_t dx = x - (r ? DK_RX : DK_LX), dy = y - DK_Y, d2 = dx * dx + dy * dy, k;
        if (d2 > (DK_R + 1) * (DK_R + 1))
            continue;
        if (d2 > DK_R * DK_R - 4)
            return DK_INK;                                                   /* the flange's rim */
        if (d2 <= 4)
            return DK_FACE;                                                  /* the hub */
        if (d2 <= 9)
            return DK_INK;
        for (k = 0; k < 3; k++) {                                            /* its three spokes, turning */
            int32_t al = dx * dk.cx[r][k] + dy * dk.sy[r][k], pe = dx * dk.sy[r][k] - dy * dk.cx[r][k];
            if (al > 0 && pe < 150 && pe > -150 && d2 < (DK_R - 2) * (DK_R - 2))
                return DK_INK;
        }
        return d2 <= dk.t[r] * dk.t[r] ? DK_TAPE : DK_FACE;
    }
    if (dk_on_line(x, y, DK_LX - dk.t[0], DK_Y, 7, 64) || dk_on_line(x, y, DK_RX + dk.t[1], DK_Y, 73, 64))
        return DK_INK;                                                       /* the tape: down to the rollers, */
    if (y == 66 && x >= 7 && x <= 73)
        return DK_INK;                                                       /* .. across under the head */
    {
        int32_t a = (x - 7) * (x - 7) + (y - 65) * (y - 65), b = (x - 73) * (x - 73) + (y - 65) * (y - 65);
        if (a <= 5 || b <= 5)
            return a <= 1 || b <= 1 ? DK_FACE : DK_INK;                     /* the rollers */
    }
    if (y >= 62 && y <= 65 && x >= 35 && x <= 45)
        return y == 62 || x == 35 || x == 45 ? DK_INK : DK_FACE;            /* the head */
    return DK_BG;
}
static void pl_time(char *b, uint32_t d10)
{
    uint32_t sec = d10 / 100u, k = 0;
    if (sec >= 600u)
        b[k++] = (char)('0' + sec / 600u % 10u);
    b[k++] = (char)('0' + sec / 60u % 10u);
    b[k++] = ':';
    b[k++] = (char)('0' + sec % 60u / 10u);
    b[k++] = (char)('0' + sec % 10u);
    b[k] = 0;
}
/* the piece's name, its time, its place, at the top */
static void dk_words(uint16_t *buf, uint32_t bh, int32_t oy, uint16_t ink, uint16_t acc)
{
    const pl_item_t *it = &pl_item[pl_idx % PL_MAX];
    char b[24], c[12];
    pf_draw_s(buf, 240u, bh, 12, 10 + oy, it->name, ink, pf_fit(it->name, 216, 4));
    pl_time(b, demo_pos / 441u);
    str_cpy(b + str_len(b), " / ", 4);
    pl_time(c, it->dur10);
    str_cpy(b + str_len(b), c, 8);
    pf_draw_s(buf, 240u, bh, 12, 44 + oy, b, acc, 2);
    fmt_int(b, (int32_t)pl_idx + 1);
    str_cpy(b + str_len(b), " OF ", 5);
    fmt_int(c, (int32_t)pl_count);
    str_cpy(b + str_len(b), c, 8);
    pf_draw_s(buf, 240u, bh, 228 - pf_width(b), 44 + oy, b, ink, 2);
}
#endif

/* grid rows gy0 .. gy1-1 to the LCD (3 screen rows each), through the canvas, 41 grid rows at a time */
static void scene_rows(uint32_t gy0, uint32_t gy1)
{
    static const uint8_t none[CAT_G];
    uint16_t pal[16], rc[RC_N][4], star = 0, flake, ink, acc, dim;
    uint32_t i, j, nflakes = cat.winter > 16 ? (uint32_t)cat.winter / 4u : 0u;
    int32_t n = cat.night < 0 ? 0 : cat.night, w = cat.winter < 0 ? 0 : cat.winter;
    for (i = 0; i < RC_N; i++) {                                             /* each colour, 4 steps darker to the edge */
        uint16_t c = room_col(i, w, n);
        for (j = 0; j < 4u; j++)
            rc[i][j] = swap16(m1_mix(c, M1_RGB(0x20, 0x16, 0x10), (int32_t)j * 7));
    }
    if (n > 64)
        star = swap16(m1_mix(room_col(RC_WALL, w, n), M1_RGB(0xf0, 0xe2, 0xc0), (n - 64) * 100 / 63));
    flake = swap16(M1_RGB(0xf4, 0xee, 0xe0));
    room_ink(&ink, &acc, &dim);
    ink = swap16(ink);
    acc = swap16(acc);
    (void)dim;
    if (nflakes > 32u)
        nflakes = 32u;
    for (i = 0; i < 16u; i++)
        pal[i] = swap16(CAT_PAL[i]);
#ifdef M1_TAPE
    uint16_t dkc[DK_N];
    int deck = dk_on();
    if (deck) {                                                              /* the deck's colours: the room's */
        uint16_t wall = room_col(RC_WALL, w, n);
        dk_setup();
        dkc[DK_BG] = rc[RC_WALL][0];
        dkc[DK_INK] = ink;
        dkc[DK_FACE] = swap16(m1_mix(wall, M1_RGB(0xff, 0xf6, 0xe4), 30));
        dkc[DK_TAPE] = swap16(n > 64 ? M1_RGB(0x8a, 0x6a, 0x4a) : M1_RGB(0x4a, 0x30, 0x20));
        nflakes = 0;
        star = 0;
    }
#else
    const int deck = 0;
    (void)deck;
#endif
    while (gy0 < gy1) {
        uint32_t nr = gy1 - gy0, r, sy0 = gy0 * 3u;
        if (nr > 41u)
            nr = 41u;                                                        /* 123 px: the canvas holds 124 rows */
        lcd_sync();                                                          /* (the last blit may still read it) */
        for (r = 0; r < nr; r++) {
            uint32_t gy = gy0 + r, gx, k, ey = gy < ROOM_G - 1u - gy ? gy : ROOM_G - 1u - gy;
            uint16_t *row = cv_px + r * 3u * 240u;
            const uint8_t *c = gy < CAT_SCENE ? cat_buf + gy * CAT_G : none;
            for (gx = 0; gx < CAT_G; gx++) {
                uint32_t e = gx < CAT_G - 1u - gx ? gx : CAT_G - 1u - gx, kind, lv;
                uint16_t v;
                e = e < ey ? e : ey;
                lv = 0;                                                      /* (no frame: the screen's edge is his) */
                (void)e;
#ifdef M1_TAPE
                if (deck) {                                                  /* the tape's deck, he in front of it */
                    uint32_t d = dk_cell((int32_t)gx, (int32_t)gy);
                    v = d == DK_BG ? rc[room_grain(gx, gy) ? RC_GRAIN : RC_WALL][0] : dkc[d];
                    if (c[gx])
                        v = pal[c[gx]];
                    row[gx * 3u] = row[gx * 3u + 1u] = row[gx * 3u + 2u] = v;
                    continue;
                }
#endif
                kind = PET->water ? (gy >= ROOM_G - 26u ? sand_kind(gx, gy) : RC_WALL)
                                  : gy >= ROOM_G - 11u ? grass_kind(gx, gy) : RC_WALL;
                if (kind == RC_WALL)
                    kind = room_grain(gx, gy) ? RC_GRAIN : RC_WALL;
                v = rc[kind][lv];
                if (c[gx] && (PET->water || !(gy >= ROOM_G - 2u && grass_front(gx))))     /* (a few blades in front of his paws) */
                    v = pal[c[gx]];
                row[gx * 3u] = row[gx * 3u + 1u] = row[gx * 3u + 2u] = v;
            }
            for (k = 0; k < nflakes; k++)
                if (snow[k].y == gy && snow[k].x < CAT_G && !c[snow[k].x]) {
                    uint32_t x = snow[k].x * 3u;
                    row[x] = row[x + 1u] = row[x + 2u] = flake;
                }
            for (k = 0; k < 240u; k++)
                row[240u + k] = row[480u + k] = row[k];
            if (star && gy >= CAP_GY && gy < 50u)                            /* a star: one pixel in the middle */
                for (k = 0; k < 18u; k++)
                    if (STARS[k][1] == gy && !c[STARS[k][0]])
                        row[240u + STARS[k][0] * 3u + 1u] = star;
        }
        if (spl.on) {                                                        /* the opening's words */
            int32_t oy = -(int32_t)sy0;
            uint32_t bh = nr * 3u;
            pf_draw(cv_px, 240u, bh, 120 - pf_width("BUNGLE SOCIETY") / 2, spl.y + oy, "BUNGLE SOCIETY", ink);
        } else if (gy0 < CAP_GY) {                                           /* the words, at the top */
            int32_t oy = -(int32_t)sy0;
            uint32_t bh = nr * 3u;
            if (cap.show && cap.page) {                                      /* a page: its name, its knobs under it, */
                uint32_t k;                                                  /* two by two, as on the panel */
                pf_draw_s(cv_px, 240u, bh, 12, 8 + oy, cap.lab, ink, pf_fit(cap.lab, 216, 4));
                for (k = 0; k < 4u; k++) {
                    const char *n = m1_page_knob(m1.what, k);
                    char b[16] = {(char)('1' + k), ' ', 0};
                    if (!n[0])
                        continue;
                    str_cpy(b + 2, n, sizeof b - 2);
                    pf_draw_s(cv_px, 240u, bh, 12 + (int32_t)(k & 1u) * 114, 42 + (int32_t)(k >> 1) * 16 + oy, b, acc,
                              pf_fit(b, 108, 2));
                }
            } else if (cap.show && cap.val[0]) {                                   /* the name, the value under it, bigger */
                int32_t s1 = pf_fit(cap.lab, 216, 4), s2 = pf_fit(cap.val, 216, 6);
                pf_draw_s(cv_px, 240u, bh, 12, 14 + oy, cap.lab, ink, s1);
                pf_draw_s(cv_px, 240u, bh, 12, 42 + oy, cap.val, acc, s2);
            } else if (cap.show) {                                           /* a word alone (the sound): big */
                int32_t s1 = pf_fit(cap.lab, 216, 6);
                pf_draw_s(cv_px, 240u, bh, 12, 26 + oy, cap.lab, ink, s1);
            }
#ifdef M1_TAPE
            else if (deck) {                                                 /* the tape plays: its piece, its time */
                dk_words(cv_px, bh, oy, ink, acc);
            }
#endif
        }
        lcd_blit(0, sy0, 240u, nr * 3u, cv_px);
        gy0 += nr;
    }
}

/* ------------------------------------------------------------ his life */
static void cat_play(uint32_t clip)
{
    cat.clip = (uint8_t)clip;
    cat.step = 0;
    cat.t_next = 0;                                                          /* its first frame: now */
    cat.behaving = clip >= CAT_PETRIR;
}

/* GLOBE COMPANION: another one moves in (seen when GLOBE closes: everything is drawn again) */
static void pet_pick(uint32_t v)
{
    if (PET == &PETS[v % PET_N])
        return;
    PET = &PETS[v % PET_N];
    cat.box[3] = 0;
    cat.full = 1;
    cat_play(CAT_WAKE);
}

/* (the leg-up wash, his toilette: the ones he does most) */
static const uint8_t CAT_DAY_LIFE[] = {CAT_PETRIR, CAT_MLEM, CAT_VIDE, CAT_QUEUE, CAT_CELLO, CAT_CHASSE, CAT_TAPOTE,
                                       CAT_TOILETTE, CAT_ROULADE, CAT_GRIFFES, CAT_FOLIE, CAT_TERRITOIRE, CAT_BAILLEMENT,
                                       CAT_CELLO, CAT_CELLO, CAT_CELLO, CAT_TOILETTE, CAT_TOILETTE};
static const uint8_t CAT_NIGHT_LIFE[] = {CAT_ENDORT, CAT_PETRIR, CAT_BAILLEMENT, CAT_ENDORT};

static int cat_night(void) { return cat.night > 80; }   /* (fader 1 in its lowest third) */

static int cat_sel(uint32_t c) { return c == CAT_CALIN || c == CAT_JOUER || c == CAT_CACHE || c == CAT_CADEAU; }

static void cat_think(void)
{
    uint32_t now = fm1_ms, c = cat.clip;
#ifdef M1_TAPE
    int playing = (m1.notes_ms && now - m1.notes_ms < 1200u) || demo_on;   /* (the tape too: he listens) */
#else
    int playing = m1.notes_ms && now - m1.notes_ms < 1200u;
#endif
    int touched = m1.touched != M1T_NONE && now - m1.t_ms < 1500u;
    if (playing || touched)
        cat.quiet_since = now;
    if (m1.calin) {                                                          /* SEL (the FM-1): you, with him, if he is awake */
        m1.calin = 0;
        if (!(c == CAT_SLEEP || c == CAT_ENDORT || c == CAT_WAKE || (c == CAT_PETRIR && cat_night())) && !cat_sel(c)) {
            /* a few cuddles, then he wants to play, then he hides and comes running back; now and then a present */
            static const uint8_t SEL_NEXT[8] = {CAT_CALIN, CAT_CALIN, CAT_JOUER, CAT_CALIN, CAT_CACHE, CAT_CALIN, CAT_JOUER,
                                                CAT_CADEAU};
            static uint8_t k;
            uint32_t want = SEL_NEXT[k++ % 8u];
            if (cat_rand(4) == 0)                                            /* (not quite always the same order) */
                want = SEL_NEXT[cat_rand(8)];
            cat_play(want);
            cat.quiet_since = now;
            cat.away = 0;
            return;
        }
    }
#ifdef M1_TAPE
    if (dk_on()) {                                                           /* the tape: the deck fills the screen; */
        if (c == CAT_PASSE || cat_sel(c))                                    /* now and then he passes in front */
            return;
        if (!cat.away) {
            cat.away = 1;
            cat.box[3] = 0;
            memset(cat_buf, 0, sizeof cat_buf);
            cat.full = 1;
            cat.next_pass = now + 9000u + cat_rand(8000u);
            return;
        }
        if ((int32_t)(now - cat.next_pass) >= 0) {
            cat.next_pass = now + 25000u + cat_rand(20000u);
            if (!cat_night() && M1G[M1G_CAT] % 3u != 2u) {                   /* (QUIET: he lets you listen) */
                cat.away = 0;
                cat_play(CAT_PASSE);
            }
        }
        return;
    }
    if (cat.away) {                                                          /* the tape stopped: he is back */
        cat.away = 0;
        cat.full = 1;
        cat_play(cat_night() ? CAT_SLEEP : CAT_IDLE);
        return;
    }
#endif
    if (c == CAT_SLEEP || c == CAT_ENDORT || (c == CAT_PETRIR && cat_night())) {   /* asleep: only the day wakes him */
        if (cat.night < 48) {
            cat_play(CAT_WAKE);
            cat.next_life = now + 10000u;
        }
        return;
    }
    if (c == CAT_WAKE)
        return;                                                              /* (he gets up first) */
    if (m1.dancing && !cat_night() && !cat_sel(c)) {                      /* the ARP or the loop plays: he dances */
        if (c != CAT_DANSE)
            cat_play(CAT_DANSE);
        return;
    }
    if (c == CAT_DANSE) {                                                    /* the music stopped */
        cat_play(playing ? CAT_PLAY : CAT_IDLE);
        return;
    }
    if (cat_night()) {                                                       /* night: he falls asleep, music or not */
        if (!cat.behaving || c == CAT_IDLE)
            cat_play(cat_rand(4) ? CAT_ENDORT : CAT_PETRIR);                 /* (PETRIR ends asleep at night) */
        return;
    }
    if (playing && !cat_sel(c)) {
        if (c != CAT_PLAY)
            cat_play(CAT_PLAY);                                              /* the music stops his business too */
        return;
    }
    if (touched && (c == CAT_IDLE || c == CAT_PLAY || c == CAT_LOOK_L || c == CAT_LOOK_R)) {
        uint32_t want = m1.touched == M1T_PAD || m1.touched == M1T_BTN || m1.touched == M1T_HOLD ? CAT_LOOK_L : CAT_LOOK_R;
        if (c != want)
            cat_play(want);
        return;
    }
    if (cat.behaving)
        return;                                                              /* (his business: until the clip ends) */
    if (c != CAT_IDLE) {                                                     /* the music or the touch is over */
        cat_play(CAT_IDLE);
        return;
    }
    if (!cat.next_life)
        cat.next_life = now + 6000u;
    if ((int32_t)(now - cat.next_life) < 0)
        return;
    if (M1G[M1G_CAT] % 3u == 2u && !cat_night()) {                          /* SAGE: he only sits (and sleeps at night) */
        cat.next_life = now + 60000u;
        return;
    }
    if (cat_night())                                                         /* night: kneading, a yawn, then dodo */
        cat_play(CAT_NIGHT_LIFE[cat_rand(sizeof CAT_NIGHT_LIFE)]);
    else if (now - cat.quiet_since > 180000u)                                /* three minutes alone: sleepy */
        cat_play(cat_rand(2) ? CAT_BAILLEMENT : CAT_ENDORT);
    else
        cat_play(CAT_DAY_LIFE[cat_rand(sizeof CAT_DAY_LIFE)]);
    cat.next_life = M1G[M1G_CAT] % 3u == 1u ? now + 20000u + cat_rand(30000u)   /* CALME: every 20 .. 50 s */
                                            : now + 5000u + cat_rand(10000u);   /* VIVANT: 5 .. 15 s */
}

/* the next step of the clip when its time comes: decoded, and its rows drawn */
static void cat_tick(void)
{
    if (cat.away)
        return;
    const cat_clip_t *cl = &CAT_CLIPS[cat.clip];
    const cat_step_t *s;
    const cat_frame_t *f;
    uint32_t now = fm1_ms;
    if (cat.t_next) {                                                        /* (0: the clip's first frame, now) */
        if (cat.clip == CAT_DANSE) {                                         /* DANSE: a step a beat, not a clock */
            if (cat.beat == m1.beat)
                return;
            cat.beat = m1.beat;
        } else if ((int32_t)(now - cat.t_next) < 0) {
            return;
        }
        if (cat.step + 1u < cl->n) {
            cat.step++;
        } else if (cl->loop) {
            cat.step = 0;
        } else {                                                             /* the clip is over */
            uint32_t c = cat.clip;
            if (c == CAT_LOOK_L || c == CAT_LOOK_R) {
                cat.t_next = now + 100u;                                     /* he keeps looking */
                return;
            }
            cat_play(c == CAT_ENDORT || (c == CAT_PETRIR && cat_night()) ? CAT_SLEEP : CAT_IDLE);
            cl = &CAT_CLIPS[cat.clip];
        }
    }
    s = &CAT_STEPS[cl->start + cat.step];
    cat.t_next = now + s->d10 * 10u;
    if (!cat.t_next)
        cat.t_next = 1;
    cat_frame(s->f, s->dx);
    f = &CAT_FRAMES[s->f];
    {
        uint32_t oy0 = cat.box[1], oy1 = cat.box[1] + cat.box[3], ny0 = f->y, ny1 = f->y + f->h, y0, y1;
        y0 = !cat.box[3] ? ny0 : !f->h ? oy0 : oy0 < ny0 ? oy0 : ny0;
        y1 = oy1 > ny1 ? oy1 : ny1;
        cat.box[0] = f->x; cat.box[1] = f->y; cat.box[2] = f->w; cat.box[3] = f->h;
        if (!cat.full && y1 > y0)
            scene_rows(y0, y1 > CAT_SCENE ? CAT_SCENE : y1);
    }
}

/* ------------------------------------------------------------ GLOBE: the settings, one knob each */
#define G1_BG M1_RGB(0x2a, 0x22, 0x1c)            /* an old wood */
#define G1_HOT M1_RGB(0x3c, 0x31, 0x27)
#define G1_INK M1_RGB(0xe6, 0xd6, 0xb2)
#define G1_DIM M1_RGB(0x8a, 0x7a, 0x62)
static void g1_strip(uint32_t y, uint32_t h, uint16_t bg)
{
    cv_begin(240u, h, bg);
    (void)y;
}

static void m1_globe_row(uint32_t r)
{
    char b[12], k[4];
    const uint8_t *rgb = M1_PAD_RGB[m1.pad];
    uint16_t acc = m1_mix(M1_RGB(rgb[0], rgb[1], rgb[2]), G1_INK, 25);
    int hot = g1_hot(r);
    const char *v = g1_vname(r, b);
    g1_klab(r, k);
    g1_strip(38u + r * 24u, 24u, hot ? G1_HOT : G1_BG);
    pf_draw(cv_px, 240u, 24u, 10, 7, k, swap16(hot ? acc : G1_DIM));
    pf_draw(cv_px, 240u, 24u, 42, 7, G1_NAME[r], swap16(hot ? G1_INK : m1_mix(G1_INK, G1_BG, 20)));
    pf_draw(cv_px, 240u, 24u, 230 - pf_width(v), 7, v, swap16(hot ? acc : G1_INK));
    cv_blit(0, 38u + r * 24u);
}

/* the whole page once, then a row when it changes (and the highlight going out after 3 s) */
static void m1_globe_draw(int all)
{
    static uint32_t sig[G1_N];
    uint32_t r;
    if (all) {
        lcd_fill(0, 0, 240, 240, G1_BG);
        g1_strip(2u, 36u, G1_BG);
        pf_draw(cv_px, 240u, 36u, 10, 14, "SETTINGS", swap16(G1_INK));
        cv_blit(0, 2);
        g1_strip(208u, 30u, G1_BG);
        pf_draw(cv_px, 240u, 30u, 10, 10, G1_FOOT, swap16(G1_DIM));
        cv_blit(0, 208);
    }
    for (r = 0; r < G1_N; r++) {
        uint32_t v = g1_get(r) | (g1_hot(r) ? 0x100u : 0u) | (uint32_t)m1.pad << 12;
        if (all || v != sig[r]) {
            sig[r] = v;
            m1_globe_row(r);
        }
    }
}

#ifdef M1_TAPE
/* ------------------------------------------------------------ the tape's list (SEQ held): the same wood as GLO */
static void m1_list_row(uint32_t row, uint32_t k)
{
    const uint8_t *rgb = M1_PAD_RGB[m1.pad];
    uint16_t acc = m1_mix(M1_RGB(rgb[0], rgb[1], rgb[2]), G1_INK, 25);
    int hot = k == pl_cursor, del = pl_del == k + 1u, playing = demo_on && k == pl_idx;
    char b[12];
    g1_strip(38u + row * 24u, 24u, hot ? G1_HOT : G1_BG);
    if (k < pl_count) {
        pf_draw(cv_px, 240u, 24u, 10, 7, playing ? ">" : k < pl_songs ? "*" : " ", swap16(hot ? acc : G1_DIM));
        pf_draw(cv_px, 240u, 24u, 26, 7, del ? "ERASE? REC" : pl_item[k].name,
                swap16(del ? acc : hot ? G1_INK : m1_mix(G1_INK, G1_BG, 20)));
        if (!del) {
            pl_time(b, pl_item[k].dur10);
            pf_draw(cv_px, 240u, 24u, 230 - pf_width(b), 7, b, swap16(hot ? acc : G1_DIM));
        }
    }
    cv_blit(0, 38u + row * 24u);
}
static void m1_list_draw(int all)
{
    static uint32_t sig;
    uint32_t first = pl_cursor > 3u ? pl_cursor - 3u : 0u, row, v;
    char b[16], c[8];
    if (pl_count > 7u && first + 7u > pl_count)
        first = pl_count - 7u;
    v = first | (uint32_t)pl_cursor << 8 | (uint32_t)pl_del << 16 | ((uint32_t)pl_count << 24 ^ (demo_on ? (uint32_t)pl_idx << 4 : 0u));
    if (!all && v == sig)
        return;
    sig = v;
    if (all) {
        lcd_fill(0, 0, 240, 240, G1_BG);
        g1_strip(208u, 30u, G1_BG);
        pf_draw(cv_px, 240u, 30u, 10, 10, "K1 CHOOSE  REC ERASE", swap16(G1_DIM));
        cv_blit(0, 208);
    }
    g1_strip(2u, 36u, G1_BG);
    pf_draw(cv_px, 240u, 36u, 10, 14, "TAPE", swap16(G1_INK));
    fmt_int(b, (int32_t)pl_count);
    str_cpy(b + str_len(b), pl_count == 1u ? " PIECE" : " PIECES", 8);
    (void)c;
    pf_draw(cv_px, 240u, 36u, 230 - pf_width(b), 14, pl_count ? b : "EMPTY", swap16(G1_DIM));
    cv_blit(0, 2);
    for (row = 0; row < 7u; row++)
        m1_list_row(row, first + row);
}
#endif

/* ------------------------------------------------------------ the screen asleep (GLOBE VEILLE ECRAN) */
static struct { uint8_t st; uint32_t t; } m1_scr;                            /* 0 on, 1 off, 2 waking (SLPOUT sent) */

/* 1 = draw; 0 = the panel is dark or waking; 2 = awake again (draw everything, then DISPON) */
static int m1_screen_awake(void)
{
    uint32_t lim = SCR_MIN[scr_get() % 5u] * 60000u, now = fm1_ms;
    int active = (int32_t)(now - m1.active_ms) < 1000;
    switch (m1_scr.st) {
    case 0:
        if (!lim || (int32_t)(now - m1.active_ms) < (int32_t)lim)
            return 1;
        lcd_power(0);                                                        /* DISPOFF, SLPIN */
        m1_scr.st = 1;
        m1_scr.t = now;
        return 0;
    case 1:
        if (active && now - m1_scr.t >= 120u) {
            lcd_power(1);                                                    /* SLPOUT */
            m1_scr.st = 2;
            m1_scr.t = now;
        }
        return 0;
    default:
        if (now - m1_scr.t < 120u)
            return 0;
        m1_scr.st = 0;
        cat.full = 1;                                                        /* everything again, then the panel on */
        cat.cap_sig = 0xFFFFFFFFu;
        return 2;
    }
}

static void m1_draw(void)
{
    static uint8_t globe;
    uint32_t now = fm1_ms;
    int32_t night = m1.fader[0] == 0xFF ? 0 : 127 - m1.fader[0], winter = m1.fader[1] == 0xFF ? 0 : m1.fader[1];
    int wake = m1_screen_awake();
    if (!wake)
        return;
#ifdef M1_TAPE
    if (m1.list) {                                                           /* (the same as GLO: a page of its own) */
        m1_list_draw(globe != 2u || wake == 2);
        globe = 2;
        if (wake == 2)
            lcd_power(2);
        return;
    }
#endif
    if (m1.globe) {
        m1_globe_draw(globe != 1u || wake == 2);
        globe = 1;
        if (wake == 2)
            lcd_power(2);
        return;
    }
    if (globe) {                                                             /* back from GLOBE: everything again */
        globe = 0;
        cat.full = 1;
        cat.cap_sig = 0xFFFFFFFFu;
    }
    if (night - cat.night > 2 || cat.night - night > 2 || (night != cat.night && (night == 0 || night == 127))) {
        cat.night = (int16_t)night;
        cat.full = 1;
    }
    if (winter - cat.winter > 2 || cat.winter - winter > 2 || (winter != cat.winter && (winter == 0 || winter == 127))) {
        cat.winter = (int16_t)winter;
        cat.full = 1;
    }
    if (cat.winter > 16 && now - cat.snow_t >= 110u) {                       /* the snow falls */
        uint32_t k;
        cat.snow_t = now;
        for (k = 0; k < 32u; k++) {
            if (!snow[k].y || snow[k].y >= CAT_SCENE - 2u) {
                snow[k].x = (uint8_t)cat_rand(CAT_G);
                snow[k].y = (uint8_t)(snow[k].y ? 1u : 1u + cat_rand(60u));
                snow[k].dx = (uint8_t)cat_rand(3);
            }
            snow[k].y++;
            if (!cat_rand(3))
                snow[k].x = (uint8_t)((snow[k].x + CAT_G + snow[k].dx - 1u) % CAT_G);
        }
        cat.full = 1;
    }
    if (spl.on) {                                                            /* the opening */
        int32_t t, y;
        if (!spl.started) {
            spl.started = 1;
            spl.t0 = now;
            cat_play(CAT_SLEEP);
            cat.full = 1;
        }
        t = (int32_t)(now - spl.t0);
        if (t < (int32_t)SPL_MS && !(m1.notes_ms && (int32_t)(m1.notes_ms - spl.t0) > 0) &&
            !(m1.touched != M1T_NONE && (int32_t)(m1.t_ms - spl.t0) > 300)) {
            y = spl_y(t);
            cat_tick();
            if (cat.full) {
                cat.full = 0;
                spl.y = (int16_t)y;
                scene_rows(0, ROOM_G);
            } else {
                if (y != spl.y) {                                            /* only the rows the name crosses */
                    int32_t lo = (spl.y < y ? spl.y : y), hi = (spl.y > y ? spl.y : y) + 10;
                    spl.y = (int16_t)y;
                    lo = lo < 0 ? 0 : lo / 3;
                    hi = hi < 0 ? 0 : hi / 3 + 1;
                    if (hi > lo)
                        scene_rows((uint32_t)lo, (uint32_t)(hi > (int32_t)ROOM_G ? ROOM_G : hi));
                }
            }
            if (wake == 2)
                lcd_power(2);
            return;
        }
        spl.on = 0;                                                          /* the end: the room as it is, he wakes */
        cat.full = 1;
        cat.cap_sig = 0xFFFFFFFFu;
        cat_play(cat_night() ? CAT_SLEEP : CAT_WAKE);
    }
    cat_think();
#ifdef M1_TAPE
    {
        static uint8_t dk_was;
        if (dk_was != demo_on) {                                             /* the tape starts or stops: all again */
            dk_was = demo_on;
            cat.cap_sig = 0xFFFFFFFFu;
            cat.full = 1;
        }
    }
#endif
    {
        int words = m1_caption();
        cat_tick();
        if (cat.full) {
            cat.full = 0;
            scene_rows(0, ROOM_G);
        } else if (words) {
            scene_rows(0, CAP_GY);
        }
#ifdef M1_TAPE
        else if (dk_on()) {                                                  /* the reels turn: ~8 pictures a second */
            static uint32_t dk_ms;
            if (now - dk_ms >= 120u) {
                dk_ms = now;
                scene_rows(0, ROOM_G);
            }
        }
#endif
    }
    if (wake == 2)
        lcd_power(2);                                                        /* DISPON: the picture is there */
    m1.dirty = 0;
}
