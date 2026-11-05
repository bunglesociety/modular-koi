/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Koi on the FM-1: the screen. A pond seen from above, in the four greens of the Game Boy, and nothing drawn
 * around it: the FM-1's own bezel is its rim.
 *   the pond     240 x 240: an 80 x 80 grid of 3 x 3 px cells, four shades (0 the darkest .. 3 the lightest); the water
 *                is shade 2, a few lighter flecks; three lily pads at the edges
 *   the drops    every note is a drop where its tongue lies (koi_drop_pos: the drum's ring, the octave on an inner ring,
 *                the bowls in the middle): a dark crest, a light trough behind it, growing at KOI_RIPPLE_CPS; at the
 *                edge it comes back (each edge a mirror: the same ring from the drop's image behind it), and then the
 *                drum answers softly (K3, koi_play.c). They fade as the tongue does (K2). A bowl: a slow, wide wave.
 *   the koi      GLO KOI: 0 .. 5 carp, white with dark patches, all black, all white, black with white; they wander,
 *                keep off the edges, come to a drop (they think it is food), and when SEL feeds them they rush to the
 *                pellets and eat them. Their tails beat as they swim.
 *   the light    SELECT: the day, the night (darker greens, the moon on the water, a few stars that tremble); GLO
 *                WINTER: snow melting on the water
 *   the words    at the top, in Modular One's pixel letters (m1_pixfont.h), for a moment: the control touched
 *   the tape     while it plays (and while SEQ holds its list), Modular One's deck on the water, the koi swimming
 *                in front of it; the list pops up over its bottom half, as in Modular One
 * GLO is a page of its own, dark green. ~10 pictures a second while anything moves. */
#include "m1_pixfont.h"

#define M1_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
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
#define PG 80u                                   /* the pond's grid */
#define CAP_GY 25u                               /* the words: grid rows 0 .. 24 */

/* ------------------------------------------------------------ the pixel letters */
static int32_t pf_width_s(const char *s, int32_t sc)
{
    int32_t w = 0;
    for (; *s; s++)
        w += sc * PF_ADV[((uint8_t)*s - 32u) < 95u ? (uint8_t)*s - 32u : 0u];
    return w;
}
static int32_t pf_width(const char *s) { return pf_width_s(s, 2); }
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
static int32_t pf_fit(const char *s, int32_t w, int32_t hi)
{
    while (hi > 2 && pf_width_s(s, hi) > w)
        hi--;
    return hi;
}
/* the words with a shadow (they lie on moving water) */
static void pf_say(uint16_t *buf, uint32_t bh, int32_t x, int32_t y, const char *s, uint16_t c, uint16_t sh, int32_t sc)
{
    pf_draw_s(buf, 240u, bh, x + sc / 2 + 1, y + sc / 2 + 1, s, sh, sc);
    pf_draw_s(buf, 240u, bh, x, y, s, c, sc);
}

/* ------------------------------------------------------------ small sums */
static uint32_t k_isqrt(uint32_t x)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > x)
        b >>= 2;
    while (b) {
        if (x >= r + b) {
            x -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return r;
}
/* the angle of (dx, dy) as a fraction of a turn (2^32), y down */
static uint32_t k_atan2(int32_t dy, int32_t dx)
{
    uint32_t ax = (uint32_t)(dx < 0 ? -dx : dx), ay = (uint32_t)(dy < 0 ? -dy : dy), a, r;
    if (!ax && !ay)
        return 0;
    r = ax >= ay ? (ay << 15) / ax : (ax << 15) / ay;                        /* Q15, 0 .. 1 */
    a = (r << 14) + (r * (32768u - r) >> 15) * 5695u;                        /* atan, in turns (an octant: 2^29) */
    if (ay > ax)
        a = 0x40000000u - a;
    if (dx < 0)
        a = 0x80000000u - a;
    if (dy < 0)
        a = 0u - a;
    return a;
}
static int32_t k_cos(uint32_t a) { return sine_i(a + 0x40000000u); }
static int32_t k_sin(uint32_t a) { return sine_i(a); }

static struct {
    uint32_t rnd;
    uint8_t full;
    uint32_t cap_sig, frame_ms;
    uint32_t ev_r;                               /* koi_ev read */
    int16_t day;
} pd = {.rnd = 0x2545F491u, .full = 1, .cap_sig = 0xFFFFFFFFu, .day = -1};
static uint32_t pd_rand(uint32_t n)
{
    uint32_t x = pd.rnd;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    pd.rnd = x;
    return n ? x % n : x;
}

static uint8_t pond[PG * PG] __attribute__((section(".pool")));   /* shades 0..3 of the picture (bit 2: a koi) */
static int16_t wave[PG * PG] __attribute__((section(".pool")));   /* the ripples' sum */

/* ------------------------------------------------------------ the drops */
#define ND 16u
static struct { uint8_t on, kind, x, y, vel; uint32_t t0; } drop[ND];
static void drop_add(uint32_t note, uint32_t vel, uint32_t ms)
{
    uint32_t i, best = 0, x, y, k;
    for (i = 0; i < ND; i++) {                                               /* a free one, else the oldest */
        if (!drop[i].on) {
            best = i;
            break;
        }
        if ((int32_t)(drop[i].t0 - drop[best].t0) < 0)
            best = i;
    }
    k = koi_drop_pos(note, &x, &y);
    drop[best].on = 1;
    drop[best].kind = (uint8_t)k;
    drop[best].x = (uint8_t)x;
    drop[best].y = (uint8_t)y;
    drop[best].vel = (uint8_t)vel;
    drop[best].t0 = ms;
}
static void drops_take(void)
{
    uint32_t w = koi_ev_w;
    if (w - pd.ev_r > KOI_NEV)
        pd.ev_r = w - KOI_NEV;
    for (; pd.ev_r != w; pd.ev_r++) {
        const koi_ev_t *e = &koi_ev[pd.ev_r % KOI_NEV];
        drop_add(e->note, e->vel, e->ms);
    }
}
/* + a to the cells of row y between radii ri and ro (1/16 cells) around (cx, cy) */
static void ring_row(int32_t cx, int32_t cy, int32_t y, int32_t ri, int32_t ro, int32_t a)
{
    int32_t dy = (y - cy) * 16, xo, xi, x;
    int16_t *w = wave + (uint32_t)y * PG;
    if (ro <= 0 || dy * dy >= ro * ro)
        return;
    xo = (int32_t)k_isqrt((uint32_t)(ro * ro - dy * dy)) / 16;
    xi = ri > 0 && dy * dy < ri * ri ? (int32_t)k_isqrt((uint32_t)(ri * ri - dy * dy)) / 16 : -1;
    for (x = xi + 1; x <= xo; x++) {
        if (cx + x >= 0 && cx + x < (int32_t)PG)
            w[cx + x] = (int16_t)(w[cx + x] + a);
        if (x && cx - x >= 0 && cx - x < (int32_t)PG)
            w[cx - x] = (int16_t)(w[cx - x] + a);
    }
}
static void ring(int32_t cx, int32_t cy, int32_t r, int32_t th, int32_t a)
{
    int32_t y, y0 = cy - (r + th) / 16 - 1, y1 = cy + (r + th) / 16 + 1;
    if (y0 < 0)
        y0 = 0;
    if (y1 > (int32_t)PG - 1)
        y1 = PG - 1;
    for (y = y0; y <= y1; y++) {
        ring_row(cx, cy, y, r - th, r + th, a);                             /* the crest */
        ring_row(cx, cy, y, r - 3 * th, r - th, -a / 2);                    /* the trough behind it */
    }
}
static void drops_wave(uint32_t now)
{
    uint32_t i, life0 = 1500u * KOI_RES_Q8[koi_res & 127u] / 256u;
    for (i = 0; i < PG * PG; i++)
        wave[i] = 0;
    for (i = 0; i < ND; i++) {
        uint32_t t = now - drop[i].t0, life = drop[i].kind == 2u ? 7000u : drop[i].vel < 24u ? 900u : life0 + 600u;
        int32_t cps = drop[i].kind == 2u ? 11 : (int32_t)KOI_RIPPLE_CPS, r, th, a, cx = drop[i].x, cy = drop[i].y, k;
        if (!drop[i].on)
            continue;
        if ((int32_t)t < 0)
            t = 0;
        if (t >= life) {
            drop[i].on = 0;
            continue;
        }
        r = (int32_t)(t * (uint32_t)cps * 16u / 1000u);
        th = drop[i].kind == 2u ? 24 : drop[i].vel < 24u ? 8 : 12;
        a = (int32_t)drop[i].vel * (int32_t)(life - t) / (int32_t)life;
        a = a * (int32_t)(life - t) / (int32_t)life;                         /* (fading as the tongue) */
        if (drop[i].kind == 2u)
            a = a * 3 / 4;
        if (t < 120u)                                                         /* the splash: a dark dot first */
            ring(cx, cy, 10, 10, (int32_t)drop[i].vel);
        if (!koi_ripple)                                                      /* GLO RIPPLES OFF: the dot only */
            continue;
        ring(cx, cy, r, th, a);
        if (drop[i].kind == 2u && r > 70)
            ring(cx, cy, r - 70, th, a / 2);                                  /* (a bowl: a second wave) */
        for (k = 0; k < 4; k++) {                                             /* the edges send it back */
            int32_t mx = k == 0 ? -1 - cx : k == 1 ? 2 * (int32_t)PG - 1 - cx : cx;
            int32_t my = k == 2 ? -1 - cy : k == 3 ? 2 * (int32_t)PG - 1 - cy : cy;
            int32_t d = k == 0 ? cx : k == 1 ? (int32_t)PG - 1 - cx : k == 2 ? cy : (int32_t)PG - 1 - cy;
            if (r > d * 16)
                ring(mx, my, r, th, a * 2 / 3);
        }
    }
}

/* ------------------------------------------------------------ the koi */
#define NK 5u
static struct {
    int32_t x, y;                                /* 1/256 cells */
    uint32_t ang, ph;                            /* heading (a turn: 2^32), the tail's beat */
    int32_t sp;                                  /* speed, 1/16 cells a second */
    uint8_t style;
    uint32_t aim_ms;                             /* going for a drop or a pellet until then */
    int32_t ax, ay;
} koi[NK];
#define NFOOD 10u
static struct { uint8_t on, x, y; } food[NFOOD];
static uint32_t feed_seen;

static void koi_place(uint32_t i)
{
    koi[i].x = (int32_t)(12u + pd_rand(56u)) * 256;
    koi[i].y = (int32_t)(14u + pd_rand(52u)) * 256;
    koi[i].ang = pd_rand(0);
    koi[i].sp = 50;
    koi[i].style = (uint8_t)(i % 5u);
}
static void koi_swim(uint32_t dt, uint32_t now)
{
    uint32_t i, j;
    if (m1.feed_ms && m1.feed_ms != feed_seen) {                             /* SEL: pellets on the water */
        uint32_t cx = 22u + pd_rand(36u), cy = 24u + pd_rand(32u);
        feed_seen = m1.feed_ms;
        for (j = 0; j < NFOOD; j++) {
            food[j].on = 1;
            food[j].x = (uint8_t)(cx + pd_rand(13u) - 6u);
            food[j].y = (uint8_t)(cy + pd_rand(13u) - 6u);
        }
    }
    for (i = 0; i < koi_n && i < NK; i++) {
        int32_t tx = 0, ty = 0, aim = 0, best = 0x7FFFFFFF, want, turn, spd = 45, kx, ky;
        uint32_t ta;
        if (!koi[i].sp)
            koi_place(i);
        kx = koi[i].x >> 4;                                                  /* (1/16 cells) */
        ky = koi[i].y >> 4;
        for (j = 0; j < NFOOD; j++)                                          /* the nearest pellet */
            if (food[j].on) {
                int32_t dx = food[j].x * 16 + 8 - kx, dy = food[j].y * 16 + 8 - ky, d = dx * dx + dy * dy;
                if (d < best) {
                    best = d;
                    tx = food[j].x * 16 + 8;
                    ty = food[j].y * 16 + 8;
                    aim = 2;
                }
                if (d < 40 * 40)
                    food[j].on = 0;                                          /* eaten */
            }
        if (!aim && koi[i].aim_ms && (int32_t)(koi[i].aim_ms - now) > 0) {   /* a drop fell near: food? */
            tx = koi[i].ax;
            ty = koi[i].ay;
            aim = 1;
        }
        if (aim) {
            ta = k_atan2(ty - ky, tx - kx);
            want = (int32_t)(ta - koi[i].ang);
            spd = aim == 2 ? 110 : 75;
        } else {
            int32_t m = 10 * 16;                                             /* keep off the edges */
            want = (int32_t)(pd_rand(0) >> 3) - 0x10000000;                  /* (a wander) */
            if (kx < m || kx > (int32_t)PG * 16 - m || ky < m || ky > (int32_t)PG * 16 - m) {
                ta = k_atan2(40 * 16 - ky, 40 * 16 - kx);
                want = (int32_t)(ta - koi[i].ang);
            }
        }
        turn = (int32_t)((1u << 25) / 100u * dt);                           /* (~ a sixth of a turn a second) */
        if (aim == 2)
            turn *= 2;
        koi[i].ang += (uint32_t)(want > turn ? turn : want < -turn ? -turn : want);
        koi[i].sp += (spd - koi[i].sp) / 6;
        koi[i].x += ((k_cos(koi[i].ang) * koi[i].sp) >> 6) * (int32_t)dt / 32000;
        koi[i].y += ((k_sin(koi[i].ang) * koi[i].sp) >> 6) * (int32_t)dt / 32000;
        koi[i].x = clamp(koi[i].x, 2 * 256, ((int32_t)PG - 3) * 256);
        koi[i].y = clamp(koi[i].y, 2 * 256, ((int32_t)PG - 3) * 256);
        koi[i].ph += (uint32_t)koi[i].sp * dt * 2400u;
    }
}
/* a drop fell: the koi near it come and look */
static void koi_notice(uint32_t x, uint32_t y, uint32_t now)
{
    uint32_t i;
    for (i = 0; i < koi_n && i < NK; i++) {
        int32_t dx = (int32_t)x * 16 - (koi[i].x >> 4), dy = (int32_t)y * 16 - (koi[i].y >> 4);
        if (dx * dx + dy * dy < (26 * 16) * (26 * 16) && !pd_rand(2)) {
            koi[i].ax = (int32_t)x * 16;
            koi[i].ay = (int32_t)y * 16;
            koi[i].aim_ms = (now + 1600u) | 1u;
        }
    }
}
static void disc(int32_t cx, int32_t cy, int32_t r, uint8_t sh)             /* (1/16 cells) */
{
    int32_t x, y, x0 = (cx - r) >> 4, x1 = (cx + r) >> 4, y0 = (cy - r) >> 4, y1 = (cy + r) >> 4;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            int32_t dx = x * 16 + 8 - cx, dy = y * 16 + 8 - cy;
            if (x >= 0 && y >= 0 && x < (int32_t)PG && y < (int32_t)PG && dx * dx + dy * dy <= r * r)
                pond[(uint32_t)y * PG + (uint32_t)x] = sh;
        }
}
/* a carp: a spine of ten round segments (~15 cells long), the tail beating, a dark outline around a light one (the Game
 * Boy's sprites); its patches as its style says. Shades | 4: the koi's cells (the tape's deck stays under them) */
static void koi_draw(uint32_t i)
{
    static const uint8_t RAD[10] = {30, 40, 44, 44, 40, 34, 27, 20, 14, 10};
    static const uint8_t BODY[5] = {3, 0, 3, 3, 1}, LINE[5] = {0, 0, 1, 0, 0};
    static const uint8_t PAT[5][10] = {                                      /* (whole patches: at 3 px a cell a */
        {3, 1, 1, 1, 3, 3, 3, 3, 3, 3},                                      /* scatter of spots reads as debris) */
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                                      /* karasu: black */
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3},                                      /* ogon: all light */
        {1, 3, 3, 3, 3, 3, 3, 3, 3, 3},                                      /* tancho: the dark mark on its head */
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1},                                      /* chagoi: all mid green */
    };
    int32_t c = k_cos(koi[i].ang), s = k_sin(koi[i].ang), k, pass, px[10], py[10];
    uint32_t st = koi[i].style % 5u;
    for (k = 0; k < 10; k++) {
        int32_t back = k * 26, wig = (k_sin(koi[i].ph - (uint32_t)k * 0x16000000u) * k * 3) >> 15;   /* (1/16) */
        px[k] = (koi[i].x >> 4) - (c * back >> 15) - (s * wig >> 15);
        py[k] = (koi[i].y >> 4) - (s * back >> 15) + (c * wig >> 15);
    }
    for (pass = 0; pass < 2; pass++) {                                       /* the outline, then the body */
        int32_t grow = pass ? 0 : 12;
        uint8_t sh = (uint8_t)((pass ? 0u : LINE[st]) | 4u);
        if (!pass && BODY[st] <= 1u)
            continue;                                                        /* (a dark koi needs none) */
        {   /* the tail's fan, the fins */
            int32_t ox = -s * 16 >> 15, oy = c * 16 >> 15, tx = px[9] - (c * 26 >> 15), ty = py[9] - (s * 26 >> 15);
            uint8_t f = pass ? (uint8_t)(PAT[st][9] | 4u) : sh;
            disc(tx + ox, ty + oy, 14 + grow, f);
            disc(tx - ox, ty - oy, 14 + grow, f);
            ox = -s * 52 >> 15;
            oy = c * 52 >> 15;
            f = pass ? (uint8_t)((BODY[st] ? 3u : 1u) | 4u) : sh;                /* (the fins: the body's) */
            disc(px[2] + ox, py[2] + oy, 14 + grow, f);
            disc(px[2] - ox, py[2] - oy, 14 + grow, f);
        }
        for (k = 9; k >= 0; k--)
            disc(px[k], py[k], RAD[k] + grow, pass ? (uint8_t)(PAT[st][k] | 4u) : sh);
    }
}

/* ------------------------------------------------------------ the light */
static const uint16_t GB_DAY[4] = {M1_RGB(0x0f, 0x38, 0x0f), M1_RGB(0x30, 0x62, 0x30), M1_RGB(0x8b, 0xac, 0x0f),
                                   M1_RGB(0x9b, 0xbc, 0x0f)};
static const uint16_t GB_NIGHT[4] = {M1_RGB(0x06, 0x14, 0x0e), M1_RGB(0x14, 0x30, 0x24), M1_RGB(0x24, 0x48, 0x34),
                                     M1_RGB(0x8a, 0xa8, 0x6a)};
static const uint8_t LILY[3][3] = {{8, 71, 6}, {71, 70, 7}, {74, 33, 5}};
static struct { uint8_t x, y, t; } snow[24];
static const uint8_t STARS[10][2] = {{8, 30}, {20, 72}, {33, 54}, {47, 8}, {55, 60}, {72, 30}, {64, 44}, {15, 14},
                                     {40, 66}, {30, 22}};

static void pond_paint(uint32_t now)
{
    uint32_t x, y, i, night = 127u - m1.day;
    for (y = 0; y < PG; y++)                                                 /* the water, its flecks */
        for (x = 0; x < PG; x++) {
            uint32_t h = (x * 73856093u) ^ (y * 19349663u) ^ ((now / 700u) * 83492791u * ((x + y) % 3u == 0u));
            h ^= h >> 13;
            pond[y * PG + x] = (uint8_t)(night > 70u ? (h % 211u == 0u ? 3u : 2u) : (h * 0x5bd1e995u >> 27) < 1u ? 3u : 2u);
        }
    if (night > 70u) {                                                       /* the moon on the water, the stars */
        int32_t r = 6 * 16;
        disc(60 * 16, 18 * 16, r, 3);
        for (i = 0; i < 10u; i++)
            if (((now / 400u + i * 7u) % 9u) < 6u)
                pond[STARS[i][1] * PG + STARS[i][0]] = 3;
    }
    for (i = 0; i < NFOOD; i++)                                              /* the pellets */
        if (food[i].on)
            pond[food[i].y * PG + food[i].x] = 1;
    for (i = 0; i < koi_n && i < NK; i++)
        koi_draw(i);
    for (i = 0; i < PG * PG; i++) {                                          /* the ripples on top */
        int32_t w = wave[i];
        uint32_t sh = pond[i] & 3u, f = pond[i] & 4u;
        if (w > 40)
            sh = 0;
        else if (w > 14)
            sh = sh > 1u ? 1u : 0u;
        else if (w < -14 && sh == 2u)
            sh = 3;
        pond[i] = (uint8_t)(sh | f);
    }
    for (i = 0; i < 3u && !(demo_on || m1.list); i++) {                     /* the lily pads (the koi go under; */
                                                                             /* not under the tape's words) */
        int32_t lx = LILY[i][0], ly = LILY[i][1], r = LILY[i][2], xx, yy;
        uint32_t notch = 0x9000000u * i + 0x20000000u;                       /* (each its own way) */
        for (yy = ly - r; yy <= ly + r; yy++)
            for (xx = lx - r; xx <= lx + r; xx++) {
                int32_t dx = xx - lx, dy = yy - ly;
                uint32_t a;
                if (xx < 0 || yy < 0 || xx >= (int32_t)PG || yy >= (int32_t)PG || dx * dx + dy * dy > r * r + r)
                    continue;
                a = k_atan2(dy, dx) - notch;                                 /* the notch: a wedge to the middle */
                if ((dx || dy) && (a < 0x0E000000u || a > 0xF2000000u))
                    continue;
                pond[(uint32_t)yy * PG + (uint32_t)xx] = dx * dx + dy * dy > (r - 1) * (r - 1) ? 0 : 1;
            }
        if (i == 1u)
            disc((lx - 2) * 16, (ly + 1) * 16, 26, 3);                       /* (a flower on one) */
    }
    if (koi_winter)                                                          /* the snow, melting on the water */
        for (i = 0; i < 24u; i++) {
            if (!snow[i].t) {
                snow[i].x = (uint8_t)pd_rand(PG);
                snow[i].y = (uint8_t)pd_rand(PG);
                snow[i].t = (uint8_t)(4u + pd_rand(12u));
            }
            snow[i].t--;
            if (!pd_rand(3))
                snow[i].y = (uint8_t)((snow[i].y + 1u) % PG);
            pond[snow[i].y * PG + snow[i].x] = 3;
        }
}

#ifdef M1_TAPE
/* ------------------------------------------------------------ the tape: Modular One's deck (ui_draw.c pl_deck there,
 * the same geometry), drawn in pixels over the pond; the koi swim in front of it */
#define PL_RY 74                                                    /* the reels' centres */
#define PL_RL 64
#define PL_RR 176
#define PL_R 50                                                     /* their flanges */
#define PL_GY 148                                                   /* the rollers */
#define PL_GL 20
#define PL_GR 220
static uint16_t kd_ink, kd_dim, kd_bg, kd_panel;                    /* (raw colours: cv_pset swaps) */
static void kd_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t dy;
    for (dy = -r; dy <= r; dy++) {
        int32_t w = (int32_t)k_isqrt((uint32_t)(r * r - dy * dy));
        cv_rect(cx - w, cy + dy, 2 * w + 1, 1, c);
    }
}
static void kd_ring(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t dy;
    for (dy = -r; dy <= r; dy++) {
        int32_t wo = (int32_t)k_isqrt((uint32_t)(r * r - dy * dy));
        int32_t wi = dy * dy < (r - 1) * (r - 1) ? (int32_t)k_isqrt((uint32_t)((r - 1) * (r - 1) - dy * dy)) : -1;
        cv_rect(cx - wo, cy + dy, wo - wi, 1, c);
        cv_rect(cx + wi + 1, cy + dy, wo - wi, 1, c);
    }
}
static void kd_reel(int32_t cx, int32_t cy, int32_t tape, uint32_t ang)
{
    uint32_t k;
    kd_disc(cx, cy, PL_R, kd_ink);                                  /* the flange */
    kd_disc(cx, cy, PL_R - 2, kd_bg);
    kd_disc(cx, cy, tape, kd_dim);                                  /* the tape wound on it */
    for (k = 0; k < 3u; k++) {                                      /* its three spokes, turning */
        uint32_t a = ang + k * 0x55555555u;
        int32_t c0 = sine_i(a + 0x40000000u), s0 = sine_i(a);
        int32_t x0 = cx + c0 * 11 / 32768, y0 = cy + s0 * 11 / 32768;
        int32_t x1 = cx + c0 * (PL_R - 3) / 32768, y1 = cy + s0 * (PL_R - 3) / 32768;
        cv_line(x0, y0, x1, y1, kd_ink);
        cv_line(x0 + 1, y0, x1 + 1, y1, kd_ink);
    }
    kd_disc(cx, cy, 11, kd_panel);                                  /* the hub */
    kd_ring(cx, cy, 11, kd_ink);
    kd_disc(cx, cy, 2, kd_ink);
}
static void kd_deck(uint32_t p1000, uint32_t ang)
{
    int32_t lt = 16 + (int32_t)((1000u - p1000) * 30u / 1000u), rt = 16 + (int32_t)(p1000 * 30u / 1000u);
    cv_line(PL_RL - lt, PL_RY, PL_GL - 6, PL_GY, kd_ink);           /* the tape: down to the left roller, */
    cv_line(PL_GL, PL_GY + 6, PL_GR, PL_GY + 6, kd_ink);            /* across under the head, */
    cv_line(PL_RR + rt, PL_RY, PL_GR + 6, PL_GY, kd_ink);           /* up to the right reel */
    kd_disc(PL_GL, PL_GY, 6, kd_panel);                             /* the rollers */
    kd_ring(PL_GL, PL_GY, 6, kd_ink);
    kd_disc(PL_GL, PL_GY, 1, kd_ink);
    kd_disc(PL_GR, PL_GY, 6, kd_panel);
    kd_ring(PL_GR, PL_GY, 6, kd_ink);
    kd_disc(PL_GR, PL_GY, 1, kd_ink);
    cv_rect(108, PL_GY - 2, 24, 8, kd_ink);                         /* the head, its two guides */
    cv_rect(86, PL_GY + 1, 6, 5, kd_dim);
    cv_rect(148, PL_GY + 1, 6, 5, kd_dim);
    kd_reel(PL_RL, PL_RY, lt, ang);
    kd_reel(PL_RR, PL_RY, rt, ang + 0x2AAAAAAAu);
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
#endif

/* ------------------------------------------------------------ the words */
static struct { char lab[20], val[20]; uint8_t show, page; } cap;
static uint32_t m1_hash(uint32_t h, const char *t)
{
    while (*t)
        h = (h ^ (uint8_t)*t++) * 16777619u;
    return h * 16777619u;
}
static void m1_caption(void)
{
    cap.lab[0] = cap.val[0] = 0;
    cap.page = 0;
    cap.show = m1.touched != M1T_NONE && fm1_ms - m1.t_ms < 2500u;
    if (!cap.show)
        return;
    switch (m1.touched) {
    case M1T_SCALE:
        koi_scale_name(cap.lab, sizeof cap.lab, m1.what, 0);
        if (m1.value)
            str_cpy(cap.val, "YOURS", sizeof cap.val);
        break;
    case M1T_MAT: str_cpy(cap.lab, KOI_MAT_N[m1.what % KOI_NMAT], sizeof cap.lab); break;
    case M1T_KNOB: str_cpy(cap.lab, KC_NAME[m1.what % KC_N], sizeof cap.lab); fmt_int(cap.val, m1.value); break;
    case M1T_LIGHT:
        str_cpy(cap.lab, m1.value > 96 ? "DAY" : m1.value > 56 ? "EVENING" : m1.value > 20 ? "DUSK" : "NIGHT",
                sizeof cap.lab);
        fmt_int(cap.val, m1.value);
        break;
    case M1T_VOL: str_cpy(cap.lab, "VOLUME", sizeof cap.lab); fmt_int(cap.val, m1.value); break;
    case M1T_HOLD:
        str_cpy(cap.lab, m1.hlab ? m1.hlab : "", sizeof cap.lab);
        str_cpy(cap.val, m1.hval, sizeof cap.val);
        break;
    case M1T_PAGE: cap.page = 1; koi_scale_name(cap.lab, sizeof cap.lab, m1.scale, 0); break;
    default: cap.show = 0; break;
    }
}

/* ------------------------------------------------------------ the opening, as the composer's Game Boy one (Modular
 * One's): BUNGLE SOCIETY comes down from the top to the middle on the plain lightest green, stops, and the drum gives
 * the 'pling' (koi_play.c: B5 then E6, struck); a koi turns on itself at the bottom; then the pond. A note or a touch
 * skips it */
#define SPL_MS 3600u
#define SPL_STOP 2000
#define SPL_Y 112
static struct { uint8_t on, started, pinged; uint32_t t0; int16_t y; } spl = {1, 0, 0, 0, -100};
/* the koi of the opening: turning on itself at the bottom, ~a turn in 2.6 s, its tail beating (grid rows 56 .. 79) */
static void splash_koi(uint32_t now)
{
    static uint32_t last;
    uint32_t ang = now * 1651910u, i, r, gx, k, save_n = koi_n;   /* (2^32 / 2600 ms) */
    uint16_t pal[4];
    int32_t c, sn;
    if (last && now - last < 70u)
        return;
    last = now;
    for (i = 56u * PG; i < PG * PG; i++)
        pond[i] = 3;
    {
        typeof(koi[0]) keep = koi[0];
        c = k_cos(ang);
        sn = k_sin(ang);
        koi[0].ang = ang;
        koi[0].x = 40 * 256 + (c * 117 >> 11);                              /* (the head 7.3 cells off the middle) */
        koi[0].y = 68 * 256 + (sn * 117 >> 11);
        koi[0].ph = now * 900000u;
        koi[0].style = 0;
        koi_n = 1;
        koi_draw(0);
        koi_n = (uint8_t)save_n;
        koi[0] = keep;
    }
    for (i = 0; i < 4u; i++)
        pal[i] = swap16(GB_DAY[i]);
    lcd_sync();
    for (r = 0; r < 24u; r++) {
        uint16_t *row = cv_px + r * 3u * 240u;
        const uint8_t *cl = pond + (56u + r) * PG;
        for (gx = 0; gx < PG; gx++)
            row[gx * 3u] = row[gx * 3u + 1u] = row[gx * 3u + 2u] = pal[cl[gx] & 3u];
        for (k = 0; k < 240u; k++)
            row[240u + k] = row[480u + k] = row[k];
    }
    lcd_blit(0, 168, 240u, 72u, cv_px);
}
static void splash_draw(uint32_t now)
{
    int32_t t = (int32_t)(now - spl.t0), y = t >= SPL_STOP ? SPL_Y : -14 + (SPL_Y + 14) * t / SPL_STOP;
    const char *w = "BUNGLE SOCIETY";
    if (spl.y == -100)
        lcd_fill(0, 0, 240, 240, GB_DAY[3]);
    if (y != spl.y) {                                                       /* one strip: where it was and where it */
        int32_t lo = spl.y > -14 && spl.y < y ? spl.y : y, hi = (spl.y > y ? spl.y : y) + 12;   /* is: no flicker */
        if (lo < 0)
            lo = 0;
        if (hi > 166)
            hi = 166;
        spl.y = (int16_t)y;
        if (hi > lo) {
            cv_begin(240u, (uint32_t)(hi - lo), GB_DAY[3]);
            pf_draw(cv_px, 240u, (uint32_t)(hi - lo), 120 - pf_width(w) / 2, y + 1 - lo, w, swap16(GB_DAY[0]));
            cv_blit(0, (uint32_t)lo);
        }
    }
    splash_koi(now);
    if (t >= SPL_STOP && !spl.pinged) {
        spl.pinged = 1;
        koi_ping = 1;                                                        /* the 'pling' */
    }
}

/* the tape's words under the deck (Modular One's: the piece, the scale, the time, where it is, the buttons), or its
 * list (SEQ held: K1 chooses, REC asks to erase, REC again erases), in the strip being drawn (oy, bh) */
#ifdef M1_TAPE
static void kd_words(uint32_t bh, int32_t oy, const uint16_t *pal, uint16_t ink, uint16_t sh, uint16_t acc, uint32_t idx,
                     uint32_t el, uint32_t dur, uint32_t p1000)
{
    char b[28], c[16];
    if (!m1.list) {
        const char *nm = pl_count ? pl_item[idx % PL_MAX].name : "TAPE EMPTY";
        pf_say(cv_px, bh, 8, 162 + oy, nm, ink, sh, pf_fit(nm, 224, 3));
        pl_time(b, el);
        str_cpy(b + str_len(b), " / ", 4);
        pl_time(c, dur);
        str_cpy(b + str_len(b), c, 8);
        pf_say(cv_px, bh, 8, 194 + oy, b, ink, sh, 2);
        koi_scale_name(c, sizeof c, m1.scale, 1);
        pf_say(cv_px, bh, 232 - pf_width(c), 194 + oy, c, acc, sh, 2);
        {
            uint32_t y, x, w = 224u * p1000 / 1000u;
            for (y = 212; y < 214; y++)
                if ((int32_t)y + oy >= 0 && (int32_t)y + oy < (int32_t)bh)
                    for (x = 8; x < 232; x++)
                        cv_px[(uint32_t)((int32_t)y + oy) * 240u + x] = x - 8u < w ? ink : pal[1];
        }
        pf_say(cv_px, bh, 120 - pf_width_s("SEQ LIST  OCT+ NEXT  OCT- START", 1) / 2, 224 + oy,
               "SEQ LIST  OCT+ NEXT  OCT- START", ink, sh, 1);
    } else {                                                        /* the list, popped up */
        int32_t first = clamp((int32_t)pl_cursor - 2, 0, pl_count > 5u ? (int32_t)pl_count - 5 : 0), row;
        uint32_t y, x;
        for (y = 128; y < 222; y++)                                 /* its paper, its frame */
            if ((int32_t)y + oy >= 0 && (int32_t)y + oy < (int32_t)bh)
                for (x = 4; x < 236; x++)
                    cv_px[(uint32_t)((int32_t)y + oy) * 240u + x] =
                        y == 128u || y == 221u || x == 4u || x == 235u ? pal[0] : pal[3];
        if (!pl_count)
            pf_draw(cv_px, 240u, bh, 12, 136 + oy, "NO SONG, NO SKETCH", pal[0]);
        for (row = 0; row < 5 && first + row < (int32_t)pl_count; row++) {
            uint32_t k = (uint32_t)(first + row);
            int32_t yy = 134 + row * 17;
            int sel = k == pl_cursor, playing = demo_on && k == pl_idx;
            if (pl_del == k + 1u || sel) {
                for (y = (uint32_t)(yy - 3); y < (uint32_t)(yy + 13); y++)
                    if ((int32_t)y + oy >= 0 && (int32_t)y + oy < (int32_t)bh)
                        for (x = 5; x < 235; x++)
                            cv_px[(uint32_t)((int32_t)y + oy) * 240u + x] = pl_del == k + 1u ? pal[0] : pal[2];
            }
            if (pl_del == k + 1u) {                                 /* "erase?" */
                str_cpy(b, "ERASE ", 8);
                str_cpy(b + 6, pl_item[k].name, sizeof b - 6u);
                pf_draw_s(cv_px, 240u, bh, 12, yy + oy, b, pal[3], pf_fit(b, 170, 2));
                pf_draw(cv_px, 240u, bh, 228 - pf_width("REC"), yy + oy, "REC", pal[3]);
                continue;
            }
            fmt_int(b, (int32_t)k + 1);
            pf_draw(cv_px, 240u, bh, 10, yy + oy, playing ? ">" : k < pl_songs ? "*" : " ", pal[0]);
            pf_draw(cv_px, 240u, bh, 22, yy + oy, b, sel || playing ? pal[0] : pal[1]);
            pf_draw_s(cv_px, 240u, bh, 48, yy + oy, pl_item[k].name, sel || playing ? pal[0] : pal[1],
                      pf_fit(pl_item[k].name, 130, 2));
            pl_time(c, pl_item[k].dur10);
            pf_draw(cv_px, 240u, bh, 230 - pf_width(c), yy + oy, c, pal[1]);
        }
        pf_say(cv_px, bh, 120 - pf_width_s("K1 CHOOSE  REC ERASE  LET GO: PLAY", 1) / 2, 226 + oy,
               "K1 CHOOSE  REC ERASE  LET GO: PLAY", ink, sh, 1);
    }
}
#endif

/* grid rows gy0 .. gy1 - 1 to the LCD, through the canvas, 41 at a time */
static void pond_rows(uint32_t gy0, uint32_t gy1)
{
    uint16_t pal[4], ink, sh, acc;
    uint32_t i, night = 127u - m1.day;
    int32_t pn = (int32_t)(night * 100u / 127u);
#ifdef M1_TAPE
    int deck = (demo_on || m1.list) && !spl.on;
    uint32_t idx = demo_on && !m1.list ? pl_idx : pl_cursor, el = demo_on && idx == pl_idx ? demo_pos / 441u : 0u;
    uint32_t dur = pl_count ? pl_item[idx % PL_MAX].dur10 : 0u, p1000 = dur ? (el >= dur ? 1000u : el * 1000u / dur) : 0u;
    uint32_t ang = demo_on ? demo_pos * 1715u : 0u;
#else
    const int deck = 0;
#endif
    for (i = 0; i < 4u; i++)
        pal[i] = swap16(m1_mix(GB_DAY[i], GB_NIGHT[i], pn));
    ink = pn > 50 ? pal[3] : pal[0];
    sh = pn > 50 ? pal[0] : pal[3];
    acc = pn > 50 ? pal[3] : pal[1];
#ifdef M1_TAPE
    kd_ink = swap16(pal[0]);
    kd_dim = swap16(pal[1]);
    kd_panel = swap16(pal[2]);
    kd_bg = swap16(pal[3]);
#endif
    while (gy0 < gy1) {
        uint32_t nr = gy1 - gy0, r, sy0 = gy0 * 3u;
        if (nr > 41u)
            nr = 41u;
        lcd_sync();
        for (r = 0; r < nr; r++) {
            uint32_t gy = gy0 + r, gx;
            uint16_t *row = cv_px + r * 3u * 240u;
            const uint8_t *c = pond + gy * PG;
            for (gx = 0; gx < PG; gx++)
                row[gx * 3u] = row[gx * 3u + 1u] = row[gx * 3u + 2u] = pal[c[gx] & 3u];
            for (gx = 0; gx < 240u; gx++)
                row[240u + gx] = row[480u + gx] = row[gx];
        }
        {
            int32_t oy = -(int32_t)sy0;
            uint32_t bh = nr * 3u;
#ifdef M1_TAPE
            if (deck) {                                                       /* the deck, the koi in front of it */
                cv_w = 240u;
                cv_h = bh;
                cv_oy = oy;
                kd_deck(p1000, ang);
                cv_oy = 0;
                for (r = 0; r < nr; r++) {
                    uint32_t gy = gy0 + r, gx, k;
                    const uint8_t *c = pond + gy * PG;
                    for (gx = 0; gx < PG; gx++)
                        if (c[gx] & 4u)
                            for (k = 0; k < 3u; k++)
                                cv_px[(r * 3u + k) * 240u + gx * 3u] = cv_px[(r * 3u + k) * 240u + gx * 3u + 1u] =
                                    cv_px[(r * 3u + k) * 240u + gx * 3u + 2u] = pal[c[gx] & 3u];
                }
                kd_words(bh, oy, pal, ink, sh, acc, idx, el, dur, p1000);
                {
                    char b[16], c[8];
                    fmt_int(b, (int32_t)(pl_count ? idx + 1u : 0u));
                    str_cpy(b + str_len(b), " / ", 4);
                    fmt_int(c, (int32_t)pl_count);
                    str_cpy(b + str_len(b), c, 8);
                    if (!(cap.show && gy0 < CAP_GY))
                        pf_say(cv_px, bh, 234 - pf_width(b), 4 + oy, b, acc, sh, 2);
                }
            }
#endif
            if (gy0 < CAP_GY && cap.show && cap.page) {               /* the drum and its knobs */
                uint32_t k;
                pf_say(cv_px, bh, 12, 8 + oy, cap.lab, ink, sh, pf_fit(cap.lab, 216, 4));
                for (k = 0; k < KC_N; k++) {
                    char b[20] = {(char)('1' + k), ' ', 0};
                    str_cpy(b + 2, KC_NAME[k], 12);
                    str_cpy(b + str_len(b), " ", 2);
                    fmt_int(b + str_len(b), m1.k[k]);
                    pf_say(cv_px, bh, 12 + (int32_t)(k & 1u) * 114, 42 + (int32_t)(k >> 1) * 16 + oy, b, acc, sh,
                           pf_fit(b, 108, 2));
                }
            } else if (gy0 < CAP_GY && cap.show && cap.val[0]) {
                pf_say(cv_px, bh, 12, 14 + oy, cap.lab, ink, sh, pf_fit(cap.lab, 216, 4));
                pf_say(cv_px, bh, 12, 42 + oy, cap.val, acc, sh, pf_fit(cap.val, 216, 6));
            } else if (gy0 < CAP_GY && cap.show) {
                pf_say(cv_px, bh, 12, 26 + oy, cap.lab, ink, sh, pf_fit(cap.lab, 216, 6));
            }
        }
        lcd_blit(0, sy0, 240u, nr * 3u, cv_px);
        gy0 += nr;
    }
}

/* ------------------------------------------------------------ GLO and the tape's list: dark green pages */
#define G1_BG GB_DAY[0]
#define G1_HOT GB_DAY[1]
#define G1_INK GB_DAY[3]
#define G1_DIM GB_DAY[2]
static void g1_strip(uint32_t h, uint16_t bg) { cv_begin(240u, h, bg); }
static void m1_globe_row(uint32_t r)
{
    char b[12];
    int hot = g1_hot(r);
    const char *v = g1_vname(r, b);
    g1_strip(21u, hot ? G1_HOT : G1_BG);                                     /* (8 lines: 21 px each) */
    pf_draw(cv_px, 240u, 21u, 10, 5, hot ? ">" : " ", swap16(G1_INK));
    pf_draw(cv_px, 240u, 21u, 42, 5, G1_NAME[r], swap16(hot ? G1_INK : G1_DIM));
    pf_draw(cv_px, 240u, 21u, 230 - pf_width(v), 5, v, swap16(G1_INK));
    cv_blit(0, 38u + r * 21u);
}
static void m1_globe_draw(int all)
{
    static uint32_t sig[G1_N];
    uint32_t r;
    if (all) {
        lcd_fill(0, 0, 240, 240, G1_BG);
        g1_strip(36u, G1_BG);
        pf_draw(cv_px, 240u, 36u, 10, 14, "SETTINGS", swap16(G1_INK));
        cv_blit(0, 2);
        g1_strip(30u, G1_BG);
        pf_draw(cv_px, 240u, 30u, 10, 10, G1_FOOT, swap16(G1_DIM));
        cv_blit(0, 208);
    }
    for (r = 0; r < G1_N; r++) {
        uint32_t v = g1_get(r) | (g1_hot(r) ? 0x100u : 0u);
        if (all || v != sig[r]) {
            sig[r] = v;
            m1_globe_row(r);
        }
    }
}
/* ------------------------------------------------------------ the screen asleep (GLO SLEEP) */
static struct { uint8_t st; uint32_t t; } m1_scr;
static int m1_screen_awake(void)
{
    uint32_t lim = SCR_MIN[scr_get() % 5u] * 60000u, now = fm1_ms;
    int active = (int32_t)(now - m1.active_ms) < 1000;
    switch (m1_scr.st) {
    case 0:
        if (!lim || (int32_t)(now - m1.active_ms) < (int32_t)lim)
            return 1;
        lcd_power(0);
        m1_scr.st = 1;
        m1_scr.t = now;
        return 0;
    case 1:
        if (active && now - m1_scr.t >= 120u) {
            lcd_power(1);
            m1_scr.st = 2;
            m1_scr.t = now;
        }
        return 0;
    default:
        if (now - m1_scr.t < 120u)
            return 0;
        m1_scr.st = 0;
        pd.full = 1;
        return 2;
    }
}

static void m1_draw(void)
{
    static uint8_t page;
    static uint32_t last_ev, idle_drop;
    uint32_t now = fm1_ms, dt;
    int wake = m1_screen_awake();
    if (!wake)
        return;
    if (m1.globe) {
        m1_globe_draw(page != 1u || wake == 2);
        page = 1;
        if (wake == 2)
            lcd_power(2);
        return;
    }
    if (page) {
        page = 0;
        pd.full = 1;
    }
    if (!spl.started) {
        spl.started = 1;
        spl.t0 = now;
    }
    if (spl.on && (now - spl.t0 >= SPL_MS || (m1.notes_ms && (int32_t)(m1.notes_ms - spl.t0) > 0) ||
                   (m1.touched != M1T_NONE && (int32_t)(m1.t_ms - spl.t0) > 300))) {
        spl.on = 0;
        pd.full = 1;
    }
    if (spl.on) {
        splash_draw(now);
        if (wake == 2)
            lcd_power(2);
        return;
    }
    dt = now - pd.frame_ms;
    if (m1.dirty)
        pd.full = 1;                                                         /* (the tape's list moved) */
    if (!pd.full && dt < 115u)                                               /* ~8 pictures a second (as the tape's deck) */
        return;
    if (dt > 300u)
        dt = 300u;
    pd.frame_ms = now;
    pd.full = 0;
    drops_take();
    if (koi_ev_w != last_ev) {                                               /* a drop: the koi near it look */
        const koi_ev_t *e = &koi_ev[(koi_ev_w - 1u) % KOI_NEV];
        uint32_t x, y;
        koi_drop_pos(e->note, &x, &y);
        koi_notice(x, y, now);
        last_ev = koi_ev_w;
    }
    if (!koi_rain_on && !demo_on && now - m1.notes_ms > 20000u && now - idle_drop > 9000u + pd_rand(9000u)) {
        idle_drop = now;                                                     /* the bamboo drips, at rest (silent) */
        drop_add(koi_tongue[pd_rand(11u)], 30, now);
    }
    koi_swim(dt, now);
    drops_wave(now);
    pond_paint(now);
    m1_caption();
    pond_rows(0, PG);
    if (wake == 2)
        lcd_power(2);
    m1.dirty = 0;
}
