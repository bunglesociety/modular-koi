/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* KOI: the steel tongue drum and the singing bowls of Modular Koi (the FM-1 held like a Game Boy, played with the
 * thumbs). Engine 13 of the MODULAR_KOI build (in VILLE's place).
 *
 * The drum is a pool of KOI_SLOTS resonating tongues in the part's engine memory, not one tongue per voice: a tongue
 * rings ~7 s, and a real drum keeps every tongue that was struck ringing. A note starts (or strikes again, its phases
 * kept) a tongue of the pool at note_on; one voice, the carrier, renders the whole pool; the others end at their first
 * tick (done), so the voice budget stays free and nothing is cut while it rings.
 *
 * A tongue (prototype: Modular Koi's tongue3.py, the sound the composer chose):
 *   the note and its twin 0.8..2 Hz above (a real tongue's two close modes beating slowly), a weak octave that fades
 *   sooner, two inharmonic tongue modes gone fast (2.87 x, 4.6 x), a low 'tock' of the finger; a rounded attack (~12 ms,
 *   no click). Decay: the low G ~1.15 s (-47 dB at 6.35 s, as the composer's drum), higher tongues a little shorter.
 *   Sympathy: the tongues the hand does not hold answer what was struck, those whose note meets one of its partials
 *   (an octave above or below, a twelfth, two octaves, a fifth, a fourth below); they swell slowly (0.25 s) and ring.
 *   Every strike differs a little (tuning, beat, octave, length, its modes).
 * A bowl (note >= KOI_BOWL: the note 40 below; no key plays one now, a tape or MIDI IN may): four inharmonic partials (1, 2.71, 5.15, 8.1), each a slowly beating
 *   pair, 9 / 6 / 3 / 1.4 s, a 35 ms attack.
 * Velocity odd: the harmonic (HARMO: the tongue touched in its middle, the octave sings, the note almost gone).
 * Set from the controls (koi_ui.c, koi_play.c): koi_tongue[] (the scale), koi_mat (the material), koi_strike (K1),
 * koi_res (K2), koi_night, koi_hand (PAUME latched: the hand rests on the drum, short notes, no sympathy), koi_mute
 * (PAUME held: everything dies quickly). Each strike also goes to the screen (koi_ev, the drops). */
#define KOI_SLOTS 10u
#define KOI_NOSC 8u
#define KOI_BOWL 100u                           /* notes from here: a bowl, its pitch 40 semitones below */
#define KOI_BOWL_DN 40
#define KOI_BPS 1378                            /* control blocks a second (CTL 32 at 44.1 kHz) */
#define KOI_HZ_INC(hz) ((uint32_t)((hz) * 4294967296.0 / FS))

/* ------------------------------------------------------------ the drum's scales (PRESETS), 11 tongues low to high */
#define KOI_NSCALE 6u
static const char *const KOI_SCALE_N[KOI_NSCALE] = {"MAJOR", "PENTATONIC", "MINOR", "AKEBONO", "DORIAN", "HIJAZ"};
static const char *const KOI_SCALE_S[KOI_NSCALE] = {"MAJOR", "PENTA", "MINOR", "AKEBONO", "DORIAN", "HIJAZ"};   /* (the tape) */
/* the key (GLO KEY): every scale moved up 0 .. 11 semitones from C */
static const char *const KOI_KEY_N[12] = {"C", "C#", "D", "EB", "E", "F", "F#", "G", "AB", "A", "BB", "B"};
static const uint8_t KOI_SCALES[KOI_NSCALE][11] = {
    {55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72},            /* the composer's drum, in C: 5 6 7 under the octave, 1 on top */
    {55, 57, 60, 62, 64, 67, 69, 72, 74, 76, 79},
    {55, 56, 58, 60, 62, 63, 65, 67, 68, 70, 72},
    {55, 56, 60, 62, 63, 67, 68, 72, 74, 75, 79},
    {55, 57, 58, 60, 62, 63, 65, 67, 69, 70, 72},
    {55, 56, 58, 60, 61, 64, 65, 67, 68, 70, 72},
};
static volatile uint8_t koi_tongue[11] = {55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72};

/* ------------------------------------------------------------ the materials (ALGORITHM) */
#define KOI_NMAT 4u
static const char *const KOI_MAT_N[KOI_NMAT] = {"STEEL", "LOW DRUM", "GLASS", "WOOD"};
typedef struct {
    int8_t tr;                                  /* semitones */
    uint16_t tau_q8;                            /* the note's decay x */
    uint16_t twin, b_lo, b_hi;                  /* twin level Q15, its beat (Hz x 100) */
    uint16_t oct, oct_tau_q8;                   /* the octave's level Q15, its decay (of the note's, Q8) */
    uint16_t r2_q12, a2, tau2_ms;               /* tongue mode 2: ratio, level, decay */
    uint16_t r3_q12, a3, tau3_ms;
    uint16_t tock, tock_k;                      /* the finger: level (x 1/4), its low-pass (Q15) */
    uint16_t att_ms;
} koi_mat_t;
static const koi_mat_t KOI_MAT[KOI_NMAT] = {
    {0, 256, 11469, 80, 200, 3277, 90, 11756, 1147, 180, 18842, 655, 90, 4, 921, 12},     /* STEEL (tongue3.py) */
    {-12, 410, 9830, 50, 120, 5243, 100, 11305, 1638, 250, 17613, 983, 120, 6, 692, 16},  /* LOW DRUM */
    {0, 333, 4915, 150, 300, 8192, 154, 12247, 2621, 500, 22118, 1311, 250, 2, 1815, 8},  /* GLASS */
    {0, 77, 3277, 0, 0, 1638, 128, 16097, 3932, 50, 25805, 1966, 25, 10, 3942, 4},        /* WOOD */
};

static volatile uint8_t koi_mat, koi_strike = 64, koi_res = 64, koi_night, koi_hand, koi_mute;

/* K2: the decay x 2^((v - 64) / 40), Q8 */
static const uint16_t KOI_RES_Q8[128] = {
    84, 86, 87, 89, 91, 92, 94, 95, 97, 99, 100, 102, 104, 106, 108, 110, 111, 113, 115, 117, 119, 122, 124, 126, 128,
    130, 133, 135, 137, 140, 142, 145, 147, 150, 152, 155, 158, 160, 163, 166, 169, 172, 175, 178, 181, 184, 187, 191,
    194, 197, 201, 204, 208, 212, 215, 219, 223, 227, 231, 235, 239, 243, 247, 252, 256, 260, 265, 270, 274, 279, 284,
    289, 294, 299, 304, 310, 315, 321, 326, 332, 338, 344, 350, 356, 362, 368, 375, 381, 388, 395, 402, 409, 416, 423,
    431, 438, 446, 454, 461, 470, 478, 486, 495, 503, 512, 521, 530, 539, 549, 558, 568, 578, 588, 598, 609, 620, 630,
    641, 653, 664, 676, 687, 699, 712, 724, 737, 750, 763};
/* the attack's shape, (i / 32)^1.5, Q15 */
static const uint16_t KOI_ATT[33] = {0, 181, 512, 941, 1448, 2024, 2660, 3352, 4096, 4887, 5724, 6604, 7525, 8485,
                                     9482, 10516, 11585, 12688, 13824, 14991, 16190, 17420, 18679, 19967, 21283, 22627,
                                     23998, 25396, 26819, 28269, 29744, 31243, 32767};

/* ------------------------------------------------------------ the drops on the screen (koi_draw.c reads them) */
#define KOI_NEV 16u
typedef struct { uint8_t note, vel; uint32_t ms; } koi_ev_t;
static koi_ev_t koi_ev[KOI_NEV];
static volatile uint32_t koi_ev_w;              /* events written */
static volatile uint32_t koi_ms_now;            /* fm1_ms, kept by the UI (the audio context has no clock of its own) */

/* where each tongue's drops fall in the basin (koi_draw.c: an 80 x 80 grid): the drum's eleven around a ring, low notes
 * at the bottom, the left thumb's on the left; the octave above (the middle keys) on an inner ring; the bowls in the
 * middle. A drop's ripple reaches the nearest edge after koi_edge_ms: then the drum answers softly (koi_play.c) */
static const uint8_t KOI_POS_T[11][2] = {{24, 64}, {56, 64}, {16, 56}, {12, 46}, {64, 56}, {13, 35}, {68, 46}, {17, 25},
                                         {67, 35}, {24, 18}, {63, 25}};
static const uint8_t KOI_POS_O[8][2] = {{28, 45}, {27, 38}, {30, 32}, {36, 28}, {44, 28}, {50, 32}, {53, 38}, {52, 45}};
#define KOI_RIPPLE_CPS 24u                      /* a ripple's speed, cells a second */
/* the place of a note's drop: 0 a tongue, 1 the octave above, 2 a bowl (the middle) */
static uint32_t koi_drop_pos(uint32_t note, uint32_t *x, uint32_t *y)
{
    uint32_t i, o;
    if (note >= KOI_BOWL) {
        *x = 40;
        *y = 41;
        return 2;
    }
    for (o = 0; o < 4u; o++)                                    /* (the octave buttons: the same tongue) */
        for (i = 0; i < 11u; i++) {
            int32_t d = (int32_t)note - (int32_t)koi_tongue[i];
            if (d == 12 * (int32_t)o || d == -12 * (int32_t)o) {
                if (o == 1u && d > 0 && i >= 3u) {
                    *x = KOI_POS_O[i - 3u][0];
                    *y = KOI_POS_O[i - 3u][1];
                    return 1;
                }
                *x = KOI_POS_T[i][0];
                *y = KOI_POS_T[i][1];
                return 0;
            }
        }
    *x = 40;
    *y = 41;
    return 1;
}
/* ms for a drop's ripple to reach the nearest edge */
static uint32_t koi_edge_ms(uint32_t x, uint32_t y)
{
    uint32_t d = x < 79u - x ? x : 79u - x, e = y < 79u - y ? y : 79u - y;
    return (d < e ? d : e) * 1000u / KOI_RIPPLE_CPS;
}

/* ------------------------------------------------------------ the pool */
typedef struct {
    uint8_t note, kind, live, dip;              /* kind 0 free, 1 tongue, 2 bowl; live: oscillators sounding (bits);
                                                 * dip: struck again, the finger touches it first */
    uint16_t att_t, att_len;                    /* the attack: samples since the strike, its length */
    int16_t r0[KOI_NOSC];                       /* where each oscillator's rise starts (Q15: what it rang with, of its
                                                 * new level; 0 a fresh strike): nothing ever jumps */
    int32_t lvl[KOI_NOSC];                      /* what each sounded at, the last block (amp >> 11 scale) */
    uint32_t inc[KOI_NOSC], ph[KOI_NOSC];
    int32_t amp[KOI_NOSC], dec[KOI_NOSC];       /* Q30 level, Q30 decay a block */
    int32_t sw[KOI_NOSC], swk[KOI_NOSC];        /* the swell (sympathy): Q15 0 .. 32767, its step (Q15 of the rest) */
    int32_t tock, tock_lvl, tock_k, lp1, lp2;   /* the finger: env Q30, its level, low-pass */
} koi_slot_t;
typedef struct {
    koi_slot_t s[KOI_SLOTS];
    koi_slot_t fade;                            /* a tongue that gave its place (the pool full): it fades in 8 ms */
    int32_t fade_g;                             /* its gain, Q15 (0: none) */
    uint8_t carrier;                            /* the voice that renders the pool (0xFF none) */
    int32_t nz, rnd;
} koi_part_t;
ENG_MEM_FITS("KOI", sizeof(koi_part_t));
static inline koi_part_t *koi_part(track_t *t) { return (koi_part_t *)ENG_MEM((uint32_t)(t - trk)); }

static uint32_t koi_rand(koi_part_t *pp, uint32_t n)
{
    uint32_t r = noise32(&pp->rnd);
    return n ? (r >> 8) % n : r;
}
/* a factor around 1 (Q12): 4096 +- spread */
static int32_t koi_var(koi_part_t *pp, int32_t spread)
{
    return 4096 + (int32_t)koi_rand(pp, (uint32_t)(2 * spread + 1)) - spread;
}

/* the phase increment of pitch16 (1/16 semitone) with the global TUNE (cents) */
static uint32_t koi_inc(int32_t pitch16)
{
    int32_t g = song.g[G_TUNE], t16 = g >= 0 ? g * 16 / 100 : -((-g * 16 + 99) / 100);
    int32_t fine = (g * 16 - t16 * 100) * 2367 / 16000;
    uint32_t inc = pitch_inc(clamp(pitch16 + t16, 0, 2047));
    return inc + (uint32_t)((int32_t)(inc >> 12) * fine);
}
static uint32_t koi_ratio(uint32_t inc, uint32_t r_q12)
{
    uint64_t x = ((uint64_t)inc * r_q12) >> 12;
    return x > 0x70000000u ? 0u : (uint32_t)x;           /* (above ~19 kHz: none) */
}
/* Q30 decay a control block for a time constant of ms */
static int32_t koi_dec(uint32_t ms)
{
    uint32_t d;
    if (ms < 2u)
        ms = 2u;
    d = 779207000u / ms;                                  /* 2^30 / (1378 blocks a second x ms / 1000) */
    return (int32_t)(0x40000000u - (d > 0x3FFFFFFFu ? 0x3FFFFFFFu : d));
}
/* a tongue's decay, ms: 1.15 s on the drum's lowest tongue, 20 ms less a semitone up (as tongue3.py), x the
 * material, K2 */
static uint32_t koi_tau(uint32_t note, const koi_mat_t *mt)
{
    int32_t ms = 1150 - ((int32_t)note - (int32_t)koi_tongue[0]) * 20;
    ms = clamp(ms, 380, 1600);
    return (uint32_t)ms * mt->tau_q8 / 256u * KOI_RES_Q8[koi_res & 127u] / 256u;
}
static void koi_osc(koi_slot_t *s, uint32_t k, uint32_t inc, int32_t lvl, uint32_t ms, int32_t swell)
{
    if (!inc || lvl <= 0) {
        s->amp[k] = 0;
        return;
    }
    s->inc[k] = inc;
    s->amp[k] = clamp(lvl, 0, 32767) << 15;
    s->dec[k] = koi_dec(ms);
    s->sw[k] = swell ? 0 : 32767;
    s->swk[k] = swell;
    s->live |= (uint8_t)(1u << k);
}

/* the drum's sympathy for a tongue struck: the intervals that ring along (semitones), how much */
static const int8_t KOI_SY_IV[6] = {12, 19, -12, 24, 7, -5};
static const uint16_t KOI_SY_AMT[6] = {32767, 19661, 16384, 11469, 8192, 4915};

/* after the oscillators are set (koi_osc): where each one rises from. A fresh strike from nothing; a tongue struck
 * again from what it rings with now (its phases kept), a little dip first (the finger touches it), then up to the new
 * level, or on from where it is when that is louder: every level moves by a ramp, never a step (no click) */
static void koi_rise(koi_slot_t *s, int again)
{
    uint32_t k;
    s->att_t = 0;
    s->dip = (uint8_t)again;
    for (k = 0; k < KOI_NOSC; k++) {
        int32_t cur = again ? s->lvl[k] : 0, a = s->amp[k] >> 11;
        if (!((s->live >> k) & 1u)) {
            s->lvl[k] = 0;
            continue;
        }
        if (!again) {
            s->r0[k] = (int16_t)(s->sw[k] < 32767 ? 32767 : 0);               /* (the sympathy: its swell only) */
            continue;
        }
        s->sw[k] = 32767;
        if (cur >= a) {
            s->amp[k] = cur << 11;
            s->r0[k] = 32767;
        } else {
            s->r0[k] = (int16_t)(((int64_t)cur << 15) / (a + 1));
        }
    }
}
/* the rise at t samples, shared by a tongue's oscillators: *u (Q15) how far into the dip (t inside it: return 1) or
 * into the attack's shape (return 0); each oscillator then goes from its own r0 (koi_gk) */
#define KOI_DIP 264u                                                               /* 6 ms */
static int koi_shape(const koi_slot_t *s, uint32_t t, int32_t *u)
{
    uint32_t d = s->dip ? KOI_DIP : 0u, x, l = s->att_len, i;
    if (t < d) {
        *u = (int32_t)(t * 32767u / d);
        return 1;
    }
    x = (t - d) * 32u;
    if (x >= l * 32u) {
        *u = 32767;
        return 0;
    }
    i = x / l;
    *u = KOI_ATT[i] + (int32_t)((KOI_ATT[i + 1u] - KOI_ATT[i]) * (x % l) / l);
    return 0;
}
static inline int32_t koi_gk(const koi_slot_t *s, uint32_t k, int dip, int32_t u)
{
    int32_t r0 = s->r0[k], rd = s->dip ? r0 * 2 / 5 : r0;
    return dip ? r0 + mulq15(rd - r0, u) : rd + mulq15(32767 - rd, u);
}

static void koi_tongue_on(koi_part_t *pp, koi_slot_t *s, uint32_t note, uint32_t vel, int again)
{
    const koi_mat_t *mt = &KOI_MAT[koi_mat % KOI_NMAT];
    int32_t p16 = ((int32_t)note + mt->tr) * 16, a = (int32_t)vel * 125;           /* 127: ~15900 */
    int32_t hz = (int32_t)(vel & 1u), st = (int32_t)koi_strike - 64, nt = koi_night;
    int32_t up = 4096 + st * 48 - nt * 12, oct = mt->oct, f = hz ? 4000 : 32767, b;
    uint32_t inc = koi_inc(p16), tau = koi_tau(note, mt), oms, k, i, j;
    a = a * koi_var(pp, 250) >> 12;                                                  /* (each strike: +-6 %) */
    inc = (uint32_t)(((uint64_t)inc * (uint32_t)(4096 + (int32_t)koi_rand(pp, 7) - 3)) >> 12);   /* (+-1 ct) */
    tau = tau * (uint32_t)koi_var(pp, 330) >> 12;                                  /* (+-8 %) */
    if (koi_hand)                                                                  /* the hand on the drum: a muffled */
        tau = tau * 150u / (tau + 150u);                                           /* strike, ~0.15 s, all knock */
    if (!again) {
        for (k = 0; k < KOI_NOSC; k++)
            s->ph[k] = (uint32_t)k * 0x2F1A0000u;
        s->lp1 = s->lp2 = 0;
    }
    s->live = 0;
    s->kind = 1;
    s->note = (uint8_t)note;
    /* the note and its twin, a slow beat (the harmonic: almost gone) */
    b = (int32_t)mt->b_lo + (int32_t)koi_rand(pp, (uint32_t)(mt->b_hi - mt->b_lo + 1u));
    koi_osc(s, 0, inc, mulq15(a, f), tau, 0);
    koi_osc(s, 1, mt->twin && !koi_hand ? inc + KOI_HZ_INC(1.0) / 100u * (uint32_t)b : 0u, mulq15(mulq15(a, f), mt->twin), tau, 0);
    /* the octave (the harmonic: it sings), the tongue's own modes */
    oct = oct * koi_var(pp, 820) >> 12;                                            /* (+-20 %) */
    oms = tau * mt->oct_tau_q8 / 256u;
    if (koi_hand)
        oct >>= 1;
    if (hz) {
        oct = 18000;
        oms = tau * 180u / 256u;
    }
    koi_osc(s, 2, koi_ratio(inc, 8192), mulq15(a, clamp(oct * up >> 12, 0, 32767)), oms, 0);
    koi_osc(s, 3, koi_ratio(inc, mt->r2_q12),
            mulq15(a, clamp(((int32_t)mt->a2 * up >> 12) * koi_var(pp, 1200) >> 12, 0, 32767)),
            koi_hand ? mt->tau2_ms / 2u : mt->tau2_ms, 0);
    koi_osc(s, 4, koi_ratio(inc, mt->r3_q12), mulq15(a, clamp((int32_t)mt->a3 * up >> 12, 0, 32767)), mt->tau3_ms, 0);
    /* the sympathy: the three tongues that answer most (none with the hand on the drum) */
    s->amp[5] = s->amp[6] = s->amp[7] = 0;
    if (!koi_hand) {
        int32_t amt[11];
        for (i = 0; i < 11u; i++) {
            int32_t iv = (int32_t)koi_tongue[i] - (int32_t)note;
            amt[i] = 0;
            for (j = 0; j < 6u && iv; j++)
                if (iv == KOI_SY_IV[j] && KOI_SY_AMT[j] > amt[i])
                    amt[i] = KOI_SY_AMT[j];
        }
        for (k = 0; k < 2u; k++) {                                                 /* (the two that answer most) */
            uint32_t bi = 11;
            int32_t lv;
            for (i = 0; i < 11u; i++)
                if (amt[i] && (bi == 11u || amt[i] > amt[bi]))
                    bi = i;
            if (bi == 11u)
                break;
            lv = mulq15(mulq15(a, 1475), amt[bi]);                                 /* 0.045 x its amount */
            if (hz)
                lv >>= 1;
            koi_osc(s, 5u + k, koi_inc(((int32_t)koi_tongue[bi] + mt->tr) * 16), lv, koi_tau(koi_tongue[bi], mt), 95);
            amt[bi] = 0;
        }
    }
    /* the finger */
    s->tock_lvl = ((int32_t)mt->tock * clamp(4096 + st * 96, 0, 16384) >> 12) * (int32_t)vel >> 7;
    if (koi_hand)
        s->tock_lvl *= 4;                                                          /* (the flesh on the steel) */
    s->tock_k = mt->tock_k;
    s->tock = 1 << 30;
    s->att_len = (uint16_t)(clamp((int32_t)mt->att_ms * (4096 - st * 40) >> 12, 2, 40) * 441 / 10);
    koi_rise(s, again);
}

static const uint16_t KOI_BW_R[4] = {4096, 11100, 21094, 33178};                  /* 1, 2.71, 5.15, 8.1 (Q12) */
static const uint16_t KOI_BW_A[4] = {32767, 14746, 5898, 1966};
static const uint16_t KOI_BW_MS[4] = {9000, 6000, 3000, 1400};
static const uint16_t KOI_BW_B[4] = {45, 105, 165, 220};                           /* the pair's beat, Hz x 100 */

static void koi_bowl_on(koi_part_t *pp, koi_slot_t *s, uint32_t note, uint32_t vel, int again)
{
    int32_t a = (int32_t)vel * 30 * koi_var(pp, 200) >> 12;                        /* (0.2 of a tongue's) */
    uint32_t inc = koi_inc(((int32_t)note - KOI_BOWL_DN) * 16), k;
    if (!again)
        for (k = 0; k < KOI_NOSC; k++)
            s->ph[k] = (uint32_t)k * 0x2F1A0000u;
    s->live = 0;
    s->kind = 2;
    s->note = (uint8_t)note;
    for (k = 0; k < 4u; k++) {
        uint32_t f = koi_ratio(inc, KOI_BW_R[k]), ms = KOI_BW_MS[k];
        int32_t lv = mulq15(a, KOI_BW_A[k]);
        if (koi_hand)
            ms = ms * 1500u / (ms + 1500u);
        koi_osc(s, 2u * k, f, lv, ms, 0);
        koi_osc(s, 2u * k + 1u, f ? f + KOI_HZ_INC(1.0) / 100u * KOI_BW_B[k] : 0u, mulq15(lv, 9830), ms, 0);
    }
    s->tock = 0;
    s->att_len = 1544;                                                             /* 35 ms */
    koi_rise(s, again);
}

static void koi_note_on(track_t *t, voice_t *v)
{
    koi_part_t *pp = koi_part(t);
    uint32_t note = v->note, vel = v->vel, kind = note >= KOI_BOWL ? 2u : 1u, i, best = 0, w;
    int32_t lo = 0x7FFFFFFF;
    koi_slot_t *s = 0;
    if (!pp->rnd) {
        pp->rnd = 0x2545F491;
        pp->nz = 0x1234567;
    }
    if (pp->carrier >= NVOICE || !t->v[pp->carrier].active)
        pp->carrier = (uint8_t)(v - t->v);
    v->env = 1 << 24;                                                              /* (a key let go before the first tick:
                                                                                    * the release must not end it at 0) */
    for (i = 0; i < KOI_SLOTS && !s; i++)                                          /* this tongue rings already */
        if (pp->s[i].kind == kind && pp->s[i].note == note)
            s = &pp->s[i];
    for (i = 0; i < KOI_SLOTS && !s; i++)                                          /* a free one */
        if (!pp->s[i].kind)
            s = &pp->s[i];
    if (!s) {                                                                      /* the quietest */
        for (i = 0; i < KOI_SLOTS; i++) {
            int32_t l = (pp->s[i].amp[0] >> 15) + (pp->s[i].amp[2] >> 15) + (pp->s[i].amp[5] >> 15);
            if (l < lo) {
                lo = l;
                best = i;
            }
        }
        s = &pp->s[best];
        pp->fade = *s;                                                             /* (it fades, not cut) */
        pp->fade_g = 32767;
        s->kind = 0;
    }
    if (kind == 2u)
        koi_bowl_on(pp, s, note, vel, s->kind == 2u);
    else
        koi_tongue_on(pp, s, note, vel, s->kind == 1u);
    w = koi_ev_w;                                                                  /* a drop on the screen */
    koi_ev[w % KOI_NEV].note = (uint8_t)note;
    koi_ev[w % KOI_NEV].vel = (uint8_t)vel;
    koi_ev[w % KOI_NEV].ms = koi_ms_now;
    koi_ev_w = w + 1u;
}

/* one tongue's block into acc[] (cleared by the caller); 0 = it is quiet now */
static int koi_slot_run(koi_part_t *pp, koi_slot_t *s, int32_t *acc, uint32_t n)
{
    uint32_t k, i, t0 = s->att_t, t1 = s->att_t + n, rising = t0 < (uint32_t)s->att_len + (s->dip ? KOI_DIP : 0u);
    int any = 0, d0 = 0, d1 = 0;
    int32_t u0 = 32767, u1 = 32767;
    if (rising) {
        d0 = koi_shape(s, t0, &u0);
        d1 = koi_shape(s, t1, &u1);
    }
    for (k = 0; k < KOI_NOSC; k++) {
        int32_t a0, a1, da, a, g0 = 32767, g1 = 32767;
        uint32_t ph, inc;
        if (!((s->live >> k) & 1u))
            continue;
        if (rising) {
            g0 = koi_gk(s, k, d0, u0);
            g1 = koi_gk(s, k, d1, u1);
        }
        a0 = (int32_t)(((int64_t)(s->amp[k] >> 11) * mulq15(s->sw[k], g0)) >> 15);   /* (4 bits more: quiet tails clean) */
        s->amp[k] = (int32_t)(((int64_t)s->amp[k] * s->dec[k]) >> 30);
        if (koi_mute)
            s->amp[k] = (int32_t)(((int64_t)s->amp[k] * 1060800000) >> 30);       /* (~60 ms) */
        if (s->sw[k] < 32767) {                                                    /* (the swell, to the top) */
            s->sw[k] += (((32767 - s->sw[k]) * s->swk[k]) >> 15) + 1;
            if (s->sw[k] > 32767)
                s->sw[k] = 32767;
        }
        a1 = (int32_t)(((int64_t)(s->amp[k] >> 11) * mulq15(s->sw[k], g1)) >> 15);
        if (s->amp[k] < (24 << 15) && s->sw[k] > 16384 && !rising) {             /* (~-56 dB under a strike: gone) */
            s->live &= (uint8_t)~(1u << k);
            s->amp[k] = 0;
            a1 = 0;
        }
        s->lvl[k] = a1;
        any = 1;
        da = a1 - a0;
        ph = s->ph[k];
        inc = s->inc[k];
        for (i = 0; i < n; i++) {
            a = a0 + ((da * (int32_t)i) >> CTL_LOG2);
            acc[i] += ((sine_i(ph) >> 3) * a) >> 12;                               /* (x 16) */
            ph += inc;
        }
        s->ph[k] = ph;
    }
    if (rising)
        s->att_t = (uint16_t)(t1 > 60000u ? 60000u : t1);
    if (s->tock) {                                                                 /* the finger's 'tock' */
        int32_t e0 = s->tock >> 15, e1, lvl = s->tock_lvl;
        s->tock = (int32_t)(((int64_t)s->tock * 1035400000) >> 30);               /* (20 ms) */
        if (koi_mute || s->tock < (1 << 20))
            s->tock = 0;
        e1 = s->tock >> 15;
        for (i = 0; i < n; i++) {
            int32_t x = (int32_t)(noise32(&pp->nz) >> 16) - 32768, e = e0 + (((e1 - e0) * (int32_t)i) >> CTL_LOG2);
            s->lp1 += ((x - s->lp1) * s->tock_k) >> 15;
            s->lp2 += ((s->lp1 - s->lp2) * s->tock_k) >> 15;
            acc[i] += (mulq15(s->lp2, e) * lvl) >> 2;                             /* (x 16) */
        }
        any = 1;
    }
    return any || s->live;
}
static void koi_out(int32_t *out, const int32_t *acc, uint32_t n, int32_t g0, int32_t g1)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t g = g0 + (((g1 - g0) * (int32_t)i) >> CTL_LOG2);
        out[i] += mulq15(mulq15(acc[i] >> 4, g), VOICE_FS) * 2 + ((mulq15(acc[i] & 15, g) * VOICE_FS) >> 18);
    }
}

/* the carrier renders the whole pool (n <= CTL), and the tongue fading out of it */
static void koi_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    koi_part_t *pp = koi_part(t);
    uint32_t si, i;
    int32_t acc[CTL];
    (void)m;
    if ((uint32_t)(v - t->v) != pp->carrier)
        return;
    for (si = 0; si < KOI_SLOTS; si++) {
        koi_slot_t *s = &pp->s[si];
        if (!s->kind)
            continue;
        for (i = 0; i < n; i++)
            acc[i] = 0;
        if (!koi_slot_run(pp, s, acc, n))
            s->kind = 0;                                                           /* (this tongue is quiet) */
        koi_out(out, acc, n, 32767, 32767);
    }
    if (pp->fade_g > 0) {
        int32_t g0 = pp->fade_g;
        pp->fade_g -= 2979;                                                        /* (11 blocks: 8 ms) */
        for (i = 0; i < n; i++)
            acc[i] = 0;
        koi_slot_run(pp, &pp->fade, acc, n);
        koi_out(out, acc, n, g0, pp->fade_g > 0 ? pp->fade_g : 0);
    }
}

/* the carrier sounds while anything rings; every other voice is done at once (its strike is in the pool) */
static int koi_done(track_t *t, voice_t *v)
{
    koi_part_t *pp = koi_part(t);
    uint32_t i;
    if ((uint32_t)(v - t->v) != pp->carrier)
        return 1;
    for (i = 0; i < KOI_SLOTS; i++)
        if (pp->s[i].kind)
            return 0;
    if (pp->fade_g > 0)
        return 0;
    pp->carrier = 0xFF;
    return 1;
}

static const preset_t KOI_PRESETS[] = {
    {"TONGUE DRUM", {0, 0, 64, 64, 40, 50, 0, 0}, {0, 0, 127, 127}, 0, 0, FX(0, 0, 0, 0)},   /* (REL long: the ADSR never ends the carrier) */
};

static const engine_t ENG_KOI = {
    .name = "KOI",
    .page_title = {"KOI", "KOI"},
    .edit = {
        {"SCALE", F_INT, 0, 127, 0, 0, 0},
        {"MATERIAL", F_INT, 0, 127, 0, 0, 0},
        {"STRIKE", F_PCT, 0, 127, 64, 0, 0},
        {"RESONANCE", F_PCT, 0, 127, 64, 0, 0},
        {"BOUNCE", F_PCT, 0, 127, 40, 0, 0},
        {"SPACE", F_PCT, 0, 127, 50, 0, 0},
        {"-", F_INT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 127, 0, 0, 0},
    },
    .presets = KOI_PRESETS,
    .npresets = (uint8_t)NELEM(KOI_PRESETS),
    .knob = {2, 3, 4, 5},
    .note_on = koi_note_on,
    .render = koi_render,
    .ownenv = 1,
    .done = koi_done,
    .mem = sizeof(koi_part_t),
};
