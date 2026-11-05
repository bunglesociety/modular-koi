/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular One on the SMK-37: the controls (what you see is what you get: one control, one job, no bank, no menu).
 *   16 pads      the sounds (VILLE, eng_ville.c). Top row: MODULAR JUNO SUPERSAW PWM TAPE FM SOUFFLE CREPUSC;
 *                bottom row: FLUTE BRASS STRINGS CHOIR RAIN DEEP PULSE MEMORY. A pad loads its sound.
 *   K1           the volume (smk_input.h: the FM-1's MASTER pot, master.c)
 *   K2 .. K8     CUTOFF RESO ATTACK DECAY SUSTAIN RELEASE DETUNE
 *   faders       1 day (up) / night (down), 2 FM (the winter; on SOUFFLE / CREPUSC the Montee), 3 echo TIME (free,
 *                10 ms .. 0.75 s), 4 ECHO (the send and its repeats)
 *   KNOB BANK / FADER BANK   WAVE 1 / WAVE 2: the next wave of the vending machines (lit where a sound has them)
 *   pitch wheel  bend (seq.c, as Felucca); mod wheel: the cutoff waves, faster as it goes up (the track LFO)
 *   OCTAVE- / +  octaves; ARP the arpeggio; SCALE the scale (snap); CHORD a chord on each key; NOTE REPEAT a
 *                (Modular Compagnon: ARP only: a tap turns it on / off; held, K2 its mode, K3 its speed, K4 the tempo;
 *                kept with the settings; a DAW's MIDI clock is followed by itself, seq.c. CHORD: an A 440 to tune
 *                to, on / off. SCALE, NOTE REPEAT and the tape: nothing)
 *                1/16 repeat while held; FX the vintage (fx.c); GLOBE the settings: one knob, one setting (below)
 *   PLAY, REC, < >  nothing (Modular Compagnon has no tape recorder)
 *   37 keys      their notes (seq.c: the 27 middle keys, the 10 outer ones) on part 1
 * The screen: m1_draw.c. Only part 1 plays; the other three are silent. */

/* the pads: hardware pad -> VILLE sound; their colours (RGB, dim and lit) */
static const uint8_t M1_PAD_SOUND[16] = {0, 1, 2, 3, 4, 5, 14, 15, 6, 7, 8, 9, 10, 11, 12, 13};
static const uint8_t M1_PAD_RGB[16][3] = {
    {120, 160, 70}, {200, 110, 40}, {220, 160, 50}, {150, 120, 200}, {170, 120, 80}, {150, 200, 255},
    {240, 200, 140}, {230, 130, 90},
    {180, 220, 160}, {255, 70, 160}, {80, 220, 255}, {200, 120, 255}, {80, 140, 255}, {40, 80, 200},
    {255, 90, 60}, {200, 160, 230},
};
enum { M1_K_CUT, M1_K_RES, M1_K_ATK, M1_K_DEC, M1_K_SUS, M1_K_REL, M1_K_DTN, M1_NK };
static const uint8_t M1_K_PARAM[M1_NK] = {P_E3, P_E4, P_ATK, P_DEC, P_SUS, P_REL, P_E5};
static const char *const M1_K_NAME[M1_NK] = {"CUTOFF", "RESO", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "DETUNE"};

/* what the screen shows: the last control touched (m1_draw.c) */
enum { M1T_NONE, M1T_KNOB, M1T_FADER, M1T_WAVE, M1T_BTN, M1T_PAD, M1T_VOL, M1T_HOLD, M1T_PAGE };
enum { M1_B_LEFT = 2, M1_B_RIGHT = 3, M1_B_PLAY = 16, M1_B_REC = 17 };
static struct {
    uint8_t pad;             /* the sound's pad (hardware number) */
    uint8_t touched, what;   /* M1T_*, which control */
    int16_t value;           /* its value, 0..127 (or a wave notch) */
    uint32_t t_ms;           /* when */
    uint8_t vintage, globe;  /* FX on; the GLOBE screen open */
    uint8_t fader[4];        /* the faders as applied (0xFF: not read yet) */
    uint32_t notes_ms;       /* the last note played (the cat listens) */
    uint8_t vel;             /* its velocity */
    uint8_t dirty;
    uint32_t active_ms;      /* the last hand on anything (the screen sleeps after MENU's time) */
    uint8_t grow;            /* GLOBE: the setting turned last (0xFF none) */
    uint32_t grow_ms;
    uint8_t hold, hold_used; /* ARP / SCALE / CHORD held (0xFF none); a knob turned meanwhile */
    const char *hlab;        /* M1T_HOLD: the words */
    char hval[16];
    uint32_t beat;           /* the ARP's steps (the companion dances on them) */
    uint8_t dancing;
    uint8_t calin;           /* (the FM-1's SEL: a cuddle asked; none here) */
} m1 = {.fader = {0xFF, 0xFF, 0xFF, 0xFF}, .grow = 0xFF, .hold = 0xFF};

/* GLOBE, the settings: K2 .. K7 one each. Kept with the settings, in bytes no engine uses: favorites.factory[13]
 * 24..31 (VILLE's favorites are bits of bytes 0..1; Modular One shows none). [0] 'M' once written. */
enum { G1_CH, G1_VEL, G1_TUNE, G1_PADS, G1_SCR, G1_CAT, G1_PET, G1_N };
static const char *const G1_NAME[G1_N] = {"MIDI CHANNEL", "VELOCITY", "TUNING", "PADS", "SLEEP", "MOOD", "COMPANION"};
static const char *const G1_VEL_N[4] = {"NORMAL", "SOFT", "HARD", "FIXED"};
static const char *const G1_PADS_N[4] = {"DIM", "SOFT", "BRIGHT", "VIVID"};
static const char *const G1_SCR_N[5] = {"NEVER", "5 MIN", "15 MIN", "30 MIN", "60 MIN"};
static const char *const G1_CAT_N[3] = {"LIVELY", "CALM", "QUIET"};
#define M1G (favorites.factory[13] + 24)
enum { M1G_MAGIC, M1G_VEL, M1G_PADS, M1G_CAT, M1G_PET = 7 };   /* ([4] [5]: TUNE, MIDI CH; [6] retired) */
/* the companion (m1_draw.c: their pictures) */
static uint32_t pet_n(void);
static const char *pet_name(uint32_t v);
static void pet_pick(uint32_t v);

static uint32_t g1_n(uint32_t r)
{
    return r == G1_CH ? menu_n(MI_TCH1) : r == G1_VEL ? 4u : r == G1_TUNE ? menu_n(MI_TUNE) : r == G1_PADS ? 4u :
           r == G1_SCR ? 5u : r == G1_PET ? pet_n() : 3u;
}
static uint32_t g1_get(uint32_t r)
{
    return r == G1_CH ? menu_get(MI_TCH1) : r == G1_VEL ? M1G[M1G_VEL] % 4u : r == G1_TUNE ? menu_get(MI_TUNE) :
           r == G1_PADS ? M1G[M1G_PADS] % 4u : r == G1_SCR ? scr_get() : r == G1_PET ? M1G[M1G_PET] % pet_n() : M1G[M1G_CAT] % 3u;
}
/* the value's words (b: 12 bytes) */
static const char *g1_vname(uint32_t r, char *b)
{
    uint32_t v = g1_get(r);
    switch (r) {
    case G1_CH: return menu_vname(MI_TCH1, v);
    case G1_VEL: return G1_VEL_N[v];
    case G1_TUNE: str_cpy(b, menu_vname(MI_TUNE, v), 8); str_cpy(b + str_len(b), " CT", 4); return b;
    case G1_PADS: return G1_PADS_N[v];
    case G1_SCR: return G1_SCR_N[v % 5u];
    case G1_PET: return pet_name(v);
    default: return G1_CAT_N[v];
    }
}
/* m1_draw.c: the line lit (the knob turned last, 3 s), what the left column says (its knob) */
static int g1_hot(uint32_t r) { return m1.grow == r && fm1_ms - m1.grow_ms < 3000u; }
static void g1_klab(uint32_t r, char *k) { k[0] = 'K'; k[1] = (char)('2' + r); k[2] = 0; }
#define G1_FOOT "GLOBE: BACK"

static void g1_put(uint32_t r, uint32_t v)
{
    switch (r) {
    case G1_CH: menu_put(MI_TCH1, v); break;
    case G1_VEL: M1G[M1G_VEL] = (uint8_t)v; smk_vel_curve = (uint8_t)v; break;
    case G1_TUNE: menu_put(MI_TUNE, v); break;
    case G1_PADS: M1G[M1G_PADS] = (uint8_t)v; break;
    case G1_SCR: scr_put(v); break;
    case G1_PET: M1G[M1G_PET] = (uint8_t)v; pet_pick(v); break;
    default: M1G[M1G_CAT] = (uint8_t)v; break;
    }
    M1G[M1G_MAGIC] = 'M';
    M1G[4] = (uint8_t)menu_get(MI_TUNE);                                     /* (the project's two: kept here too, */
    M1G[5] = (uint8_t)menu_get(MI_TCH1);                                     /*  Modular One has no project) */
    settings_save();                                                         /* (queued: settings_poll writes it) */
}
static void g1_turn(uint32_t r, int32_t s)
{
    int32_t n = (int32_t)g1_n(r), v = (int32_t)g1_get(r) + (s > 0 ? 1 : -1);
    if (r == G1_TUNE && (s > 1 || s < -1))
        v += s > 0 ? 4 : -4;                                                 /* (a fast turn: 5 cents) */
    g1_put(r, (uint32_t)clamp(v, 0, n - 1));
    m1.grow = (uint8_t)r;
    m1.grow_ms = fm1_ms;
    m1.dirty = 1;
}
static void g1_boot(void)                                                    /* the saved settings, at power-on */
{
    if (M1G[M1G_MAGIC] != 'M') {
        M1G[M1G_VEL] = 0;
        M1G[M1G_PADS] = 2;
        M1G[M1G_CAT] = 0;
        M1G[M1G_PET] = 0;
        return;
    }
    pet_pick(M1G[M1G_PET] % pet_n());                                       /* (the opening shows him asleep) */
    smk_vel_curve = (uint8_t)(M1G[M1G_VEL] % 4u);
    if (M1G[4] < menu_n(MI_TUNE))
        menu_put(MI_TUNE, M1G[4]);
    if (M1G[5] < menu_n(MI_TCH1))
        menu_put(MI_TCH1, M1G[5]);
}

#define M1T (&trk[0])

static const char *m1_knob_name(uint32_t k) { return M1_K_NAME[k % M1_NK]; }

static const char *m1_page_name(uint32_t pg) { (void)pg; return ""; }        /* (no pages on the SMK-37) */
static const char *m1_page_knob(uint32_t pg, uint32_t k) { (void)pg; (void)k; return ""; }

static void m1_touch(uint32_t kind, uint32_t what, int32_t v)
{
    m1.touched = (uint8_t)kind;
    m1.what = (uint8_t)what;
    m1.value = (int16_t)v;
    m1.t_ms = m1.active_ms = fm1_ms;
    m1.dirty = 1;
}

/* ARP: its choices (held, K2 .. K4), kept with the settings in favorites.factory[13][16..23] (bytes no engine uses;
 * [0] 'P' once written). They stay when the sound changes */
#define M1P (favorites.factory[13] + 16)
enum { MP_MAGIC, MP_ATYPE, MP_ARATE, MP_AOCT, MP_BPM };
enum { M1_B_ARP = 6, M1_B_CHORD = 9 };
#define MP_NATYPE 13u
static const uint8_t MP_ATYPE_V[MP_NATYPE] = {1, 2, 3, 7, 4, 5, 8, 9, 10, 11, 12, 13, 14};   /* params.c N_AMODE */
static const char *const MP_ATYPE_N[MP_NATYPE] = {"UP", "DOWN", "UP-DOWN", "DOWN-UP", "RANDOM",
                                                   "AS PLAYED", "UP +8", "CONVERGE", "DIVERGE", "PINKY",
                                                   "THUMB", "WALK", "CHORDS"};
#define MP_NRATE 7u
static const uint8_t MP_RATE_V[MP_NRATE] = {6, 0, 1, 4, 2, 5, 3};                              /* N_DIV, slow first */
static const char *const MP_RATE_N[MP_NRATE] = {"1/2", "1/4", "1/8", "1/8 T", "1/16", "1/16 T", "1/32"};

static void mp_boot(void)
{
    if (M1P[MP_MAGIC] != 'P' || M1P[MP_ATYPE] >= MP_NATYPE || M1P[MP_ARATE] >= MP_NRATE || M1P[MP_BPM] > 200u) {
        M1P[MP_ATYPE] = 0;                                                   /* up */
        M1P[MP_ARATE] = 4;                                                   /* 1/16 */
        M1P[MP_AOCT] = 0;                                                    /* one octave */
        M1P[MP_BPM] = 120 - 40;
    }
    song.g[G_CLOCK] = 0;                                                     /* (a DAW's clock: seq.c follows it) */
    song.g[G_BPM] = (int16_t)(40 + M1P[MP_BPM]);
}
/* the choices onto the sound (arp: on); no chord keys, no scale */
static void mp_apply(track_t *t, uint32_t arp)
{
    t->p[P_AMODE] = (int16_t)(arp ? MP_ATYPE_V[M1P[MP_ATYPE]] : 0);
    t->p[P_ARATE] = (int16_t)MP_RATE_V[M1P[MP_ARATE]];
    t->p[P_AOCT] = 1;
    t->p[P_CHRD] = CH_OFF;
    t->p[P_QUANT] = QN_OFF;
}

/* the waves the sound has (MODULAR TAPE FM: both; JUNO: WAVE 1, the saw / pulse mix); 0 none */
static uint32_t m1_waves(void)
{
    uint32_t m = (uint32_t)M1T->p[P_E0];
    return m == VM_MODULAR || m == VM_TAPE || m == VM_FM || VM_TOWN(m) ? 3u : m == VM_JUNO ? 1u : 0u;
}

/* the faders onto the sound (absolute: where a fader stands is what plays). Fader 1 up = day */
static void m1_faders_apply(int force)
{
    uint32_t i;
    track_t *t = M1T;
    for (i = 0; i < 4u; i++) {
        uint32_t v = fm1_in.fader[i];
        if (!force && v == m1.fader[i])
            continue;
        if (!force && m1.fader[i] != 0xFF)
            m1_touch(M1T_FADER, i, (int32_t)v);
        m1.fader[i] = (uint8_t)v;
        switch (i) {
        case 0: t->p[P_E6] = (int16_t)(127 - v); break;                     /* NUIT */
        case 1: t->p[P_E7] = (int16_t)v; break;                             /* HIVER (the Montee on TOWN) */
        case 2: m1_dly_target = 441u + v * 257u; break;                      /* echo TIME: 10 ms .. 0.75 s */
        default:
            t->p[P_DLY] = (int16_t)v;                                        /* ECHO: the send .. */
            song.g[G_DFDBK] = (int16_t)(25 + v * 70 / 127);                  /* .. and the repeats */
            break;
        }
    }
}

static void m1_sound(uint32_t pad)
{
    track_t *t = M1T;
    m1.pad = (uint8_t)(pad % 16u);
    fm1_irq_off();
    t->eng_req = ENGI_VILLE;
    fm1_irq_on();
    {
        uint32_t arp = t->p[P_AMODE] != 0;
        apply_preset_to(t, M1_PAD_SOUND[m1.pad]);
        mp_apply(t, arp);                                                    /* the ARP stays as it is */
    }
    t->p[P_REV] = 0;                                                         /* no reverb (the city's echo only) */
    t->p[P_LWAVE] = 0;                                                       /* the mod wheel's LFO: a sine */
    m1_faders_apply(1);                                                      /* the faders stay what they say */
    m1_touch(M1T_PAD, m1.pad, 0);
}

/* the mod wheel: the cutoff waves, slow and shallow at the bottom, fast and deep at the top */
static void m1_modwheel(void)
{
    static uint8_t last = 0xFF;
    uint32_t w = smk_modw;
    track_t *t = M1T;
    if (w == last)
        return;
    last = (uint8_t)w;
    t->p[P_LD_FLT] = (int16_t)(w * 48 / 127);
    t->p[P_LRATE] = (int16_t)(w ? 50 + w * 50 / 127 : 60);
}

static void m1_init(void)
{
    uint32_t k, i;
    /* the same on every SMK-37: nothing of a Felucca session (no autosave restored, main.c) nor of its settings
     * that are not Modular Compagnon's: the globals and every part's own values (those a preset keeps: level, pan,
     * mute, arp, scale, transpose, slicer, chord keys) at their defaults, no steps (a DAW's Start plays nothing) */
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0; i++)
            if (param_kept(i))
                trk[k].p[i] = TP[i].def;
        track_defaults_steps(&trk[k]);
    }
    song.octave = 0;
    fm1_led_dim_level(0);                                                    /* (the buttons' glow: Felucca's default) */
    for (k = 1; k < NTRK; k++)                                               /* parts 2..4 silent */
        trk[k].p[P_LEVEL] = 0;
    song.sel = 0;
    song.g[G_RTYPE] = 0;
    song.g[G_DTIME] = 1;
    song.g[G_DMIX] = 100;
    smk_pad_free = 1;                                                        /* pads are ours: not notes */
    g1_boot();
    mp_boot();
    m1_sound(0);
}

/* what the screen says for the ARP */
static void mp_show(const char *lab, const char *val)
{
    m1.hlab = lab;
    str_cpy(m1.hval, val, sizeof m1.hval);
    m1_touch(M1T_HOLD, 0, 0);
}
static void mp_tempo(char *b)
{
    if (song.g[G_CLOCK]) {
        str_cpy(b, "DAW", 4);
        return;
    }
    fmt_int(b, song.g[G_BPM]);
    str_cpy(b + str_len(b), " BPM", 5);
}
/* ARP held, knob K2 + k turned s */
static void mp_turn(uint32_t k, int32_t s)
{
    track_t *t = M1T;
    int32_t d = s > 0 ? 1 : -1;
    uint32_t arp = t->p[P_AMODE] != 0;
    char b[16];
    m1.hold_used = 1;
    switch (k) {
    case 0: M1P[MP_ATYPE] = (uint8_t)clamp(M1P[MP_ATYPE] + d, 0, MP_NATYPE - 1); arp = 1; break;
    case 1: M1P[MP_ARATE] = (uint8_t)clamp(M1P[MP_ARATE] + d, 0, MP_NRATE - 1); break;
    case 2:
        if (song.g[G_CLOCK])
            break;                                                           /* (the DAW's tempo) */
        M1P[MP_BPM] = (uint8_t)clamp(M1P[MP_BPM] + (s > 1 || s < -1 ? d * 5 : d), 0, 200);
        song.g[G_BPM] = (int16_t)(40 + M1P[MP_BPM]);
        break;
    default: return;
    }
    mp_apply(t, arp);
    M1P[MP_MAGIC] = 'P';
    settings_save();                                                         /* (queued: settings_poll writes it) */
    if (k == 0) {
        mp_show("ARP", MP_ATYPE_N[M1P[MP_ATYPE]]);
    } else if (k == 1) {
        mp_show("SPEED", MP_RATE_N[M1P[MP_ARATE]]);
    } else {
        mp_tempo(b);
        mp_show("TEMPO", b);
    }
}

static void m1_btn(uint32_t id, int down)
{
    track_t *t = M1T;
    if (!down) {
        if (id == m1.hold) {                                                 /* ARP let go */
            if (!m1.hold_used) {                                             /* a tap: on / off */
                mp_apply(M1T, !M1T->p[P_AMODE]);
                mp_show("ARP", M1T->p[P_AMODE] ? MP_ATYPE_N[M1P[MP_ATYPE]] : "OFF");
            }
            m1.hold = 0xFF;
        }
        return;
    }
    switch (id) {
    case SMK_BTN_KNOB_BANK: case SMK_BTN_FADER_BANK: {                       /* WAVE 1 / WAVE 2: the next notch */
        uint32_t w = id == SMK_BTN_KNOB_BANK ? 1u : 2u, p = id == SMK_BTN_KNOB_BANK ? P_E1 : P_E2;
        static const int16_t WV[4] = {0, 43, 64, 127};                       /* saw, between, triangle, square */
        uint32_t k;
        if (!(m1_waves() & w))
            break;
        for (k = 0; k < 4u && WV[k] <= t->p[p]; k++)
            ;
        t->p[p] = WV[k % 4u];
        m1_touch(M1T_WAVE, w, t->p[p]);
        break;
    }
    case FM1_BTN_OCT_DOWN: case FM1_BTN_OCT_UP:
        song.octave = (int8_t)clamp(song.octave + (id == FM1_BTN_OCT_UP ? 1 : -1), -3, 3);
        m1_touch(M1T_BTN, id, song.octave);
        break;
    case M1_B_ARP:                                                           /* (on / off when let go) */
        m1.hold = (uint8_t)id;
        m1.hold_used = 0;
        break;
    case M1_B_CHORD:                                                         /* the A 440 */
        m1_la ^= 1u;
        mp_show("A 440", m1_la ? "ON" : "OFF");
        break;
    case 10:                                                                 /* GLOBE: the settings */
        m1.globe ^= 1u;
        m1.grow = 0xFF;
        m1.active_ms = fm1_ms;
        m1.dirty = 1;
        break;
    case 14:                                                                 /* FX: the vintage */
        m1.vintage ^= 1u;
        m1_vintage = m1.vintage;
        m1_touch(M1T_BTN, id, m1.vintage);
        break;
    default:
        break;
    }
}

static void m1_input(void)
{
    uint32_t released = 0, pressed = fm1_input_edges(&released), k;
    int32_t e;
    master_poll();
    midi_take();
    {   /* the volume (K1): only to show it */
        static int32_t last = -1;
        int32_t v = master_knob / 16;
        if (last >= 0 && (v - last > 6 || last - v > 6))
            m1_touch(M1T_VOL, 0, v * 127 / 1023);
        if (last < 0 || v - last > 6 || last - v > 6)
            last = v;
    }
    while ((e = smk_padev_take()) >= 0)                                      /* pads: a sound */
        if (e & 0x7F)
            m1_sound((uint32_t)e >> 8);
    for (k = 0; k < M1_NK; k++) {                                            /* K2 .. K8 */
        int32_t s = fm1_enc_take(k + 1u);
        if (s && m1.hold != 0xFF && !m1.globe) {                            /* ARP held: choose */
            mp_turn(k, s);
        } else if (s && m1.globe) {                                                 /* GLOBE: K2 .. K7 the settings */
            m1.active_ms = fm1_ms;
            if (k < G1_N)
                g1_turn(k, s);
        } else if (s) {
            uint32_t id = M1_K_PARAM[k];
            int32_t lo = 0, hi = 127, v;
            if (s > 1 || s < -1)
                s *= 2;                                                      /* a fast turn goes further */
            v = clamp(M1T->p[id] + s, lo, hi);
            M1T->p[id] = (int16_t)v;
            m1_touch(M1T_KNOB, k, v);
        }
    }
    for (k = 0; k < FM1_NBTN; k++) {
        if ((pressed >> k) & 1u)
            m1_btn(k, 1);
        if ((released >> k) & 1u)
            m1_btn(k, 0);
    }
    if (fm1_in.frames > 200u)
        m1_faders_apply(m1.fader[0] == 0xFF);
    m1_modwheel();
    {   /* the ARP's steps: the companion dances on them */
        static uint32_t last_arp, beat_ms;
        if (M1T->p[P_AMODE] && (M1T->arp_note || M1T->arp_phys) && M1T->arp_idx != last_arp) {
            m1.beat++;
            beat_ms = fm1_ms;
        }
        last_arp = M1T->arp_idx;
        m1.dancing = (uint8_t)(beat_ms && fm1_ms - beat_ms < 700u);   /* (the gaps between the steps too) */
    }
    {   /* the cat listens: the last key */
        uint32_t n = fm1_input_note_edges();
        if (n) {
            uint32_t i;
            m1.notes_ms = m1.active_ms = fm1_ms;
            for (i = 0; i < 27u; i++)
                if ((n >> i) & 1u)
                    m1.vel = fm1_in.key_vel[SMK_KEY_F3 + i];
        }
    }
}

static void m1_leds(void)
{
    uint32_t p, w = m1_waves();
    track_t *t = M1T;
    static const uint8_t LIT[4] = {12, 20, 30, 42}, DIM[4] = {4, 7, 10, 14};   /* GLOBE LUMIERE PADS, % */
    uint32_t b = M1G[M1G_PADS] % 4u;
    for (p = 0; p < 16u; p++) {                                              /* warm white; the sound's pad bright */
        uint32_t pc = p == m1.pad ? LIT[b] : DIM[b];
        padled_set(p, (uint8_t)(255u * pc / 100u), (uint8_t)(150u * pc / 100u), (uint8_t)(62u * pc / 100u));
    }
    fm1_led_key(SMK_BTN_KNOB_BANK, (w & 1u) != 0);
    fm1_led_key(SMK_BTN_FADER_BANK, (w & 2u) != 0);
    fm1_led_key(FM1_BTN_OCT_DOWN, song.octave < 0);
    fm1_led_key(FM1_BTN_OCT_UP, song.octave > 0);
    fm1_led_key(6, t->p[P_AMODE] != 0);
    fm1_led_key(7, 0);
    fm1_led_key(8, 0);
    fm1_led_key(9, m1_la);
    fm1_led_key(10, m1.globe);
    fm1_led_key(14, m1.vintage);
    fm1_led_key(M1_B_PLAY, 0);
    fm1_led_key(M1_B_REC, 0);
}
