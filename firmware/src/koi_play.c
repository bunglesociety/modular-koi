/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Koi: the hands, the rain and the ripples, in the audio context (included by seq.c; keyboard_block hands
 * every key to koi_key, events_block calls koi_block after the tape's player).
 *
 * The keys, held like a Game Boy (the FM-1's 27, in Felucca's order k = 0 F3 .. 26 G5; the white keys the bottom row,
 * the black ones the top row). One octave, as the composer's drum: 5 6 7 under it, 1 on top (G3 .. C5 in C)
 *   left thumb   the bottom-left four: sol do mi sol; above them OP1 OP2: si si
 *   right thumb  the bottom-right four: la fa re la (the lowest outside as on the left); top right, the big key: the
 *                high do
 *   the middle   bottom eight: do re mi fa sol la si do, the same tongues; top five (OP4 OP5 OP6 PIT GLO): do re mi sol
 *                la, the same again
 *   gestures     OP3: a roll that starts fast and slows down (a finger bouncing); MONO: one that starts slow and
 *                speeds up; on the last tongue struck. A tap: the whole gesture; held: it goes on at its last speed.
 *                HARMO (POLY): held, the tongues give their harmonic; a tap: they stay so.
 * The notes follow the scale (koi_tongue[], PRESETS) and the octave buttons; the sketchbook hears everything played
 * (input_on), the rain too.
 * The rebounds (K3): a drop's ripple reaching the basin's edge makes the drum answer softly there, its octave (or its
 * fifth) a little later, as far as the drop fell from the edge (koi_edge_ms: the screen draws the same ripple).
 * The rain (ARP): the drum playing alone, slowly. Phrases that rise and fall, a motif that comes back changed, rests
 * on stable notes, a third under the last note now and then, the low G as a drone, long breaths. When you play it
 * listens: it waits, then answers with your phrase, changed. At night it is slower, softer, it breathes longer. */
enum { KK_SLOW = 48, KK_FAST, KK_HARMO };                    /* (the gestures: fast to slow, slow to fast) */
static const uint8_t KOI_KEY[27] = {0, 2, 3, 9, 5, KK_SLOW, 7, 3, 3, 4, 4, 5, 6, 5, 7, 7, 8, 8, 9, 10,
                                    KK_FAST, 8, KK_HARMO, 6, 4, 10, 1};
#define KOI_SR(ms) ((uint32_t)(ms) * 441u / 10u)               /* ms -> samples */
#define KOI_TAP KOI_SR(350)                                   /* shorter: a tap (latch), longer: held (momentary) */

static uint32_t koi_clk;                                      /* samples since boot */
static uint8_t koi_knote[27];                                 /* key k: its note + 1 sounding, 0 none */
static uint32_t koi_kdown;                                    /* the keys koi_key saw go down */
static uint32_t koi_g_at[3];                                  /* PAUME ROULE HARMO: when pressed */
static volatile uint8_t koi_g_held;                           /* bits: the slowing roll 1, the quickening 2, HARMO 4 */
static volatile uint8_t koi_harmo_latch;                     /* HARMO tapped */
static volatile uint8_t koi_ping;                            /* the opening's 'pling' asked (koi_draw.c) */
static uint32_t koi_ping_at;
static uint8_t koi_roll;                                     /* the roll: 0 none, 1 fast to slow, 2 slow to fast */
static uint32_t koi_roll_gap;                                /* samples to the next tap */
static volatile uint8_t koi_rain_on, koi_bounce = 40;         /* ARP; K3 */
static volatile uint32_t koi_play_ms;                         /* the last note the hands played (fm1_ms) */
static uint8_t koi_last_note;                                 /* the roll's tongue */
static uint32_t koi_roll_at, koi_roll_n;
static int32_t koi_rs = 0x1F2E3D4C;

static uint32_t koi_rnd(uint32_t n) { return n ? (noise32(&koi_rs) >> 8) % n : noise32(&koi_rs); }

/* ------------------------------------------------------------ notes: on now, off a moment later */
#define KOI_NOFF 16u
static struct { uint8_t note; uint32_t at; } koi_off[KOI_NOFF];
static void koi_note(uint32_t note, uint32_t vel, uint32_t len)
{
    uint32_t i;
    input_on(&trk[0], note, vel);
    for (i = 0; i < KOI_NOFF; i++)
        if (!koi_off[i].note) {
            koi_off[i].note = (uint8_t)note;
            koi_off[i].at = koi_clk + len;
            return;
        }
    input_off(&trk[0], note);                                 /* (full: let go at once; the tongue rings anyway) */
}

/* ------------------------------------------------------------ the rebounds: a ripple reaches the edge */
#define KOI_NBOUNCE 8u
static struct { uint8_t note, vel; uint32_t at; } koi_bq[KOI_NBOUNCE];
static void koi_bounce_add(uint32_t note, uint32_t vel)
{
    uint32_t x, y, i, j, bn = 0, v = vel * koi_bounce * 58u / (127u * 128u);   /* (K3 full: ~0.45 of the strike) */
    if (note >= KOI_BOWL || v < 6u)
        return;
    koi_drop_pos(note, &x, &y);
    for (j = 0; j < 3u && !bn; j++)                           /* its octave, else its fifth, else the octave under */
        for (i = 0; i < 11u; i++) {
            int32_t d = (int32_t)koi_tongue[i] - (int32_t)note;
            if ((j == 0u && d == 12) || (j == 1u && d == 7) || (j == 2u && d == -12)) {
                bn = koi_tongue[i];
                break;
            }
        }
    if (!bn)
        return;
    for (i = 0; i < KOI_NBOUNCE; i++)
        if (!koi_bq[i].note) {
            koi_bq[i].note = (uint8_t)bn;
            koi_bq[i].vel = (uint8_t)(v & ~1u);
            koi_bq[i].at = koi_clk + KOI_SR(koi_edge_ms(x, y));
            return;
        }
}

/* a strike: src 0 the hands, 1 the rain, 2 the roll, 3 a rebound */
static void koi_hit(uint32_t note, uint32_t vel, uint32_t src)
{
    if (note > 127u)
        return;
    vel = (vel & ~1u) | (note < KOI_BOWL && (koi_harmo_latch || (koi_g_held & 4u)) && src != 3u);   /* odd: HARMO */
    koi_note(note, vel, KOI_SR(src ? 150 : 250));
    if (src <= 1u)
        koi_bounce_add(note, vel);
}

/* ------------------------------------------------------------ the rain */
#define KOI_NRAIN 16u
static struct { uint32_t at; uint8_t note, vel; } koi_rain[KOI_NRAIN];   /* the phrase to come (absolute times) */
static uint32_t koi_rain_n, koi_rain_i, koi_rain_next;       /* its events, the next one; the next phrase's start */
static uint8_t koi_motif[8], koi_motif_n, koi_phr;
static uint8_t koi_heard[8], koi_heard_n;                    /* the player's last phrase (tongue indices) */
static uint32_t koi_heard_at;                                /* when the player last played (samples) */
static const uint8_t KOI_STABLE[4] = {3, 5, 7, 10};
static const int16_t KOI_ARC[9][8] = {                       /* sin(pi (k + 0.5) / n), Q12, n = 1 .. 8 */
    {4096}, {2896, 2896}, {2048, 4096, 2048}, {1567, 3784, 3784, 1567}, {1266, 3314, 4096, 3314, 1266},
    {1060, 2896, 3956, 3956, 2896, 1060}, {911, 2560, 3690, 4096, 3690, 2560, 911},
    {799, 2276, 3406, 4017, 4017, 3406, 2276, 799}, {0}};

static uint32_t koi_tidx(uint32_t note)                      /* a note's tongue (its octave folded), 11 none */
{
    uint32_t i, o;
    for (o = 0; o < 4u; o++)
        for (i = 0; i < 11u; i++)
            if (note == koi_tongue[i] + 12u * o || note + 12u * o == koi_tongue[i])
                return i;
    return 11;
}
static void koi_walk(uint32_t start, uint32_t n)
{
    static const int8_t STEP[8] = {-2, -1, -1, 1, 1, 2, 3, -3};
    uint32_t k;
    int32_t x = (int32_t)start;
    koi_motif_n = (uint8_t)n;
    for (k = 0; k < n; k++) {
        koi_motif[k] = (uint8_t)x;
        x = clamp(x + STEP[koi_rnd(8)], 0, 10);
    }
}
static void koi_vary(void)
{
    uint32_t r = koi_rnd(100), k, n = koi_motif_n;
    if (r < 35u) {                                           /* moved up or down the drum */
        static const int8_t SH[3] = {-1, 1, 2};
        int32_t d = SH[koi_rnd(3)];
        for (k = 0; k < n; k++)
            koi_motif[k] = (uint8_t)clamp((int32_t)koi_motif[k] + d, 0, 10);
    } else if (r < 60u) {                                    /* mirrored around its first note */
        for (k = 1; k < n; k++)
            koi_motif[k] = (uint8_t)clamp(2 * (int32_t)koi_motif[0] - (int32_t)koi_motif[k], 0, 10);
    } else if (r < 80u) {                                    /* a note less, or one more */
        if (n > 3u)
            koi_motif_n--;
        else if (n < 8u) {
            koi_motif[n] = (uint8_t)clamp((int32_t)koi_motif[n - 1u] + (int32_t)koi_rnd(4) - 1, 0, 10);
            koi_motif_n++;
        }
    } else {                                                 /* the same notes, another order */
        for (k = n - 1u; k > 1u; k--) {
            uint32_t j = 1u + koi_rnd(k), x = koi_motif[k];
            koi_motif[k] = koi_motif[j];
            koi_motif[j] = (uint8_t)x;
        }
    }
}
static void koi_rain_put(uint32_t at, uint32_t note, uint32_t vel)
{
    if (koi_rain_n < KOI_NRAIN) {
        koi_rain[koi_rain_n].at = at;
        koi_rain[koi_rain_n].note = (uint8_t)note;
        koi_rain[koi_rain_n].vel = (uint8_t)clamp((int32_t)vel, 2, 126);
        koi_rain_n++;
    }
}
/* the next phrase, from t (samples): its notes into koi_rain[], sorted; the start of the one after */
static void koi_phrase(uint32_t t)
{
    uint32_t n, k, end, i, j, night = koi_night;
    int32_t gap = (int32_t)KOI_SR(500 + koi_rnd(400)) * (int32_t)(128u + night * 77u / 127u) >> 7;
    koi_rain_n = koi_rain_i = 0;
    if (koi_heard_n >= 2u && koi_heard_at) {                 /* it answers: your phrase, changed */
        for (k = 0; k < koi_heard_n; k++)
            koi_motif[k] = koi_heard[k];
        koi_motif_n = koi_heard_n;
        koi_heard_n = 0;
        koi_vary();
    } else if (!koi_motif_n) {
        koi_walk(3, 5);
    }
    n = koi_motif_n;
    for (k = 0; k < n; k++) {
        int32_t v = 54 + (38 * KOI_ARC[n - 1u][k] >> 12) + (int32_t)koi_rnd(17) - 8;   /* (0.42 + 0.3 arc, +-0.06) */
        v = v * (int32_t)(127u - night / 4u) / 127;
        koi_rain_put(t, koi_tongue[koi_motif[k]], (uint32_t)v);
        t += (uint32_t)(gap * (k == n - 1u ? 1536 : 3482 + (int32_t)koi_rnd(1638)) >> 12);   /* (x 1.5, x 0.85 .. 1.25) */
        gap = gap * (3891 + (int32_t)koi_rnd(533)) >> 12;                                  /* (x 0.95 .. 1.08) */
    }
    end = koi_motif[n - 1u];
    for (k = 0; k < 4u && KOI_STABLE[k] != end; k++)
        ;
    if (k == 4u) {                                           /* it comes to rest on a stable note */
        uint32_t best = 3, bd = 99;
        for (k = 0; k < 4u; k++) {
            uint32_t d = KOI_STABLE[k] > end ? KOI_STABLE[k] - end : end - KOI_STABLE[k];
            if (d < bd) {
                bd = d;
                best = KOI_STABLE[k];
            }
        }
        end = best;
        koi_rain_put(t, koi_tongue[end], 57);
        t += KOI_SR(900);
    }
    if (koi_rnd(100) < 35u)                                  /* sometimes a third under it */
        koi_rain_put(t - KOI_SR(50), koi_tongue[end >= 2u ? end - 2u : 0u], 32);
    koi_phr++;
    if (koi_phr % 3u == 0u)                                  /* the low G, now and then */
        koi_rain_put(t + KOI_SR(300), koi_tongue[0], 38);
    for (i = 1; i < koi_rain_n; i++)                         /* (in time order) */
        for (j = i; j > 0u && koi_rain[j].at < koi_rain[j - 1u].at; j--) {
            uint32_t a = koi_rain[j].at;
            uint8_t x = koi_rain[j].note, y = koi_rain[j].vel;
            koi_rain[j] = koi_rain[j - 1u];
            koi_rain[j - 1u].at = a;
            koi_rain[j - 1u].note = x;
            koi_rain[j - 1u].vel = y;
        }
    koi_rain_next = t + (uint32_t)((int32_t)KOI_SR(3000 + koi_rnd(3500)) * (int32_t)(128u + night * 51u / 127u) >> 7);
    if (koi_rnd(100) < 70u)                                  /* the motif comes back changed, or a new one */
        koi_vary();
    else
        koi_walk(KOI_STABLE[koi_rnd(4)], 3u + koi_rnd(4));
}

static void koi_rain_block(void)
{
    uint32_t now = koi_clk;
    if (!koi_rain_on || demo_on) {
        koi_rain_n = koi_rain_i = 0;
        koi_rain_next = now + KOI_SR(1500);                  /* (switched on: the first drop in 1.5 s) */
        return;
    }
    if (koi_heard_at && now - koi_heard_at < KOI_SR(3000)) { /* you play: it listens, it waits */
        koi_rain_n = koi_rain_i = 0;
        koi_rain_next = koi_heard_at + KOI_SR(3000);
        return;
    }
    while (koi_rain_i < koi_rain_n && (int32_t)(now - koi_rain[koi_rain_i].at) >= 0) {
        koi_hit(koi_rain[koi_rain_i].note, koi_rain[koi_rain_i].vel, 1);
        koi_rain_i++;
    }
    if (koi_rain_i >= koi_rain_n && (int32_t)(now - koi_rain_next) >= 0)
        koi_phrase(now);
}

/* ------------------------------------------------------------ the keys */
static void koi_gesture(uint32_t g, int down)
{
    uint32_t b = 1u << g;
    if (down) {
        koi_g_at[g] = koi_clk;
        koi_g_held |= (uint8_t)b;
        if (g < 2u) {                                        /* a roll: its first tap now */
            koi_roll = (uint8_t)(g + 1u);
            koi_roll_n = 0;
            koi_roll_gap = KOI_SR(g ? 340 : 100);
            koi_roll_at = koi_clk;
        }
        return;
    }
    koi_g_held &= (uint8_t)~b;
    if (g == 2u && koi_clk - koi_g_at[g] < KOI_TAP)
        koi_harmo_latch ^= 1u;
}

/* key k down / up (keyboard_block, after the wake press). 1 = it is Koi's (every key is) */
static int koi_key(uint32_t k, int down)
{
    uint32_t c = KOI_KEY[k % 27u], note, vel;
    if (!down) {
        if (!((koi_kdown >> k) & 1u))
            return 1;
        koi_kdown &= ~(1u << k);
        if (c >= KK_SLOW)
            koi_gesture(c - KK_SLOW, 0);
        else if (koi_knote[k]) {
            input_off(&trk[0], koi_knote[k] - 1u);
            koi_knote[k] = 0;
        }
        return 1;
    }
    koi_kdown |= 1u << k;
    if (c >= KK_SLOW) {
        koi_gesture(c - KK_SLOW, 1);
        return 1;
    }
    {
        int32_t n = (int32_t)koi_tongue[c % 16u] + 12 * song.octave;
        note = (uint32_t)clamp(n, 24, KOI_BOWL - 4);
        vel = 92;
        koi_last_note = (uint8_t)note;
        koi_roll_n = 0;
        if (koi_clk - koi_heard_at >= KOI_SR(2500))
            koi_heard_n = 0;                                 /* (a new phrase of yours) */
        if (koi_heard_n < 8u)
            koi_heard[koi_heard_n++] = (uint8_t)(c % 16u);   /* the rain listens */
        koi_heard_at = koi_clk | 1u;
    }
    if (koi_knote[k])
        input_off(&trk[0], koi_knote[k] - 1u);
    vel = (vel & ~1u) | (note < KOI_BOWL && (koi_harmo_latch || (koi_g_held & 4u)));
    input_on(&trk[0], note, vel);
    koi_knote[k] = (uint8_t)(note + 1u);
    if (note < KOI_BOWL)
        koi_bounce_add(note, vel);
    koi_play_ms = fm1_ms;
    return 1;
}

/* ------------------------------------------------------------ every block */
static void koi_block(uint32_t n)
{
    uint32_t i;
    koi_clk += n;
    koi_ms_now = fm1_ms;
    for (i = 0; i < KOI_NOFF; i++)                           /* the notes let go */
        if (koi_off[i].note && (int32_t)(koi_clk - koi_off[i].at) >= 0) {
            input_off(&trk[0], koi_off[i].note);
            koi_off[i].note = 0;
        }
    for (i = 0; i < KOI_NBOUNCE; i++)                        /* the ripples at the edge */
        if (koi_bq[i].note && (int32_t)(koi_clk - koi_bq[i].at) >= 0) {
            if (!koi_mute)
                koi_hit(koi_bq[i].note, koi_bq[i].vel, 3);
            koi_bq[i].note = 0;
        }
    if (koi_roll && koi_last_note && (int32_t)(koi_clk - koi_roll_at) >= 0) {   /* the roll: two fingers */
        uint32_t held = (koi_g_held >> (koi_roll - 1u)) & 1u, v, gap;
        int32_t k = (int32_t)koi_roll_n;
        if (koi_roll == 1u)                                  /* fast to slow: it bounces, softer and softer */
            v = (uint32_t)clamp(74 - k * 5, held ? 34 : 24, 74);
        else                                                 /* slow to fast: it grows */
            v = (uint32_t)clamp(30 + k * 5, 30, 72);
        koi_hit(koi_last_note, v + (koi_roll_n & 1u ? 0u : 6u), 2);                 /* (one finger stronger) */
        koi_roll_n++;
        gap = koi_roll == 1u ? koi_roll_gap * 23u / 20u : koi_roll_gap * 17u / 20u;
        if (koi_roll == 1u && gap > KOI_SR(380)) {           /* slow enough: the end (held: on at that pace) */
            gap = KOI_SR(380);
            if (!held)
                koi_roll = 0;
        }
        if (koi_roll == 2u && gap < KOI_SR(100)) {           /* fast enough, as fingers go (held: a tremolo) */
            gap = KOI_SR(100);
            if (!held)
                koi_roll = 0;
        }
        koi_roll_gap = gap;
        koi_roll_at = koi_clk + gap + koi_rnd(KOI_SR(12));
    }
    if (koi_ping == 1u) {                                    /* the opening: the 'pling', on the drum */
        trk_note_on(&trk[0], 83, 64);
        trk_note_off(&trk[0], 83);
        koi_ping = 2;
        koi_ping_at = koi_clk + KOI_SR(80);
    } else if (koi_ping == 2u && (int32_t)(koi_clk - koi_ping_at) >= 0) {
        trk_note_on(&trk[0], 88, 96);
        trk_note_off(&trk[0], 88);
        koi_ping = 3;
    }
    koi_rain_block();
}
