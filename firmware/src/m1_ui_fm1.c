/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Compagnon on the FM-1: the controls. Modular Compagnon's sounds, echo, vintage, ARP and companions, played
 * with Modular One's pages: the four knobs do what the page says, its button chooses it.
 *   SELECT       day (right) / night (left), whatever the page; ALGORITHM the winter (the FM; the Montee on SOUFFLE /
 *                CREPUSC): the SMK-37's faders 1 and 2
 *   PRESETS      the 16 sounds (VILLE, eng_ville.c), in the SMK-37's pad order (a sound SAVEd: yours)
 *   HOME         K1 CUTOFF   K2 RESO     K3 DETUNE     K4 ECHO      (what one plays with most)
 *   ENV          K1 ATTACK   K2 DECAY    K3 SUSTAIN    K4 RELEASE
 *   EDIT         K1 WAVE 1   K2 WAVE 2   K3 GLIDE      K4 VOICE (POLY / MONO: one note at a time, legato)
 *   LFO          K1 MOD (the cutoff waves: the SMK's mod wheel)  K2 VIBRATO  K3 TREMOLO  K4 SPEED (one LFO, all three)
 *   FX           K1 ECHO TIME (free, it glides)  K2 ECHO  K3 TAPE (wow, flutter)  K4 WARMTH (saturation, the top)
 *   ARP          a tap: on / off; held: K1 its mode, K2 its speed, K3 the tempo (a DAW's MIDI clock: seq.c follows it)
 *   PLAY         the tape (Modular One's): the songs sent from the Carnet app, then your sketches, one after the
 *                other; again: stop (or OCT- and OCT+ held together 1 s, as in Modular One). While it plays OCT+ the
 *                next one, OCT- back to the start (twice: the one before)
 *   REC          held: the sketchbook keeps the last phrase you played (held again at once: one phrase further back)
 *   SEQ          held: the tape's list; K1 chooses, let go: it plays. REC in the list: erase that sketch (REC again)
 *   SAVE         the sound as it is, kept for its place in PRESETS (user preset 1..16, flash); held 2 s: the factory one
 *   GLO          the settings (K1 or SELECT: the line, K2 or PRESETS: its value; GLO or HOME: back)
 *   SEL          time with him (m1_draw.c): a few cuddles (eyes closed, hearts), then he plays (a ball, a stick, a
 *                bubble), hides off the screen, peeks in and comes running back; now and then a present (a flower,
 *                a stick, a pearl). Asleep at night: he sleeps on
 *   OCT- / OCT+  octaves (the tape playing: next, start, previous). The MASTER pot: the volume (master.c)
 * The screen: m1_draw.c (the same room, the same companions; they dance while the ARP or the tape plays; the tape's
 * reels turn at the top). The tape and the sketchbook: m1_tape.c (seq.c), m1_book.c. Only part 1 plays. */
#define M1_TAPE 1                /* (m1_draw.c: the tape's reels and list) */

/* the sounds, in the SMK-37's pad order (PRESETS steps through them); their colours (the words' accent) */
static const uint8_t M1_PAD_SOUND[16] = {0, 1, 2, 3, 4, 5, 14, 15, 6, 7, 8, 9, 10, 11, 12, 13};
static const uint8_t M1_PAD_RGB[16][3] = {
    {120, 160, 70}, {200, 110, 40}, {220, 160, 50}, {150, 120, 200}, {170, 120, 80}, {150, 200, 255},
    {240, 200, 140}, {230, 130, 90},
    {180, 220, 160}, {255, 70, 160}, {80, 220, 255}, {200, 120, 255}, {80, 140, 255}, {40, 80, 200},
    {255, 90, 60}, {200, 160, 230},
};
static const char *const M1_SND_NAME[16] = {"MODULAR", "JUNO", "SUPERSAW", "PWM", "TAPE", "FM", "SOUFFLE", "CREPUSC",
                                            "FLUTE", "BRASS", "STRINGS", "CHOIR", "RAIN", "DEEP", "PULSE", "MEMORY"};

/* the knobs' jobs, every page (and the two big encoders') */
enum { C_ATK, C_DEC, C_SUS, C_REL, C_CUT, C_RES, C_W1, C_W2, C_MOD, C_SPD, C_DTN, C_GLD, C_TIME, C_ECHO, C_TAPE,
       C_WARM, C_DAY, C_WIN, C_VIB, C_TREM, C_VOICE, M1_NK };
static const char *const M1_K_NAME[M1_NK] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE", "CUTOFF", "RESO", "WAVE 1",
                                             "WAVE 2", "MOD", "SPEED", "DETUNE", "GLIDE", "ECHO TIME", "ECHO", "TAPE",
                                             "WARMTH", "DAY/NIGHT", "WINTER", "VIBRATO", "TREMOLO", "VOICE"};
enum { PG_HOME, PG_ENV, PG_EDIT, PG_LFO, PG_FX, PG_N };
static const char *const PG_NAME[PG_N] = {"HOME", "ENV", "EDIT", "LFO", "FX"};
static const uint8_t PG_KNOB[PG_N][4] = {
    {C_CUT, C_RES, C_DTN, C_ECHO},
    {C_ATK, C_DEC, C_SUS, C_REL},
    {C_W1, C_W2, C_GLD, C_VOICE},
    {C_MOD, C_VIB, C_TREM, C_SPD},
    {C_TIME, C_ECHO, C_TAPE, C_WARM},
};

/* what the screen shows: the last control touched (m1_draw.c) */
enum { M1T_NONE, M1T_KNOB, M1T_FADER, M1T_WAVE, M1T_BTN, M1T_PAD, M1T_VOL, M1T_HOLD, M1T_PAGE };
static struct {
    uint8_t pad;             /* the sound (its place in PRESETS, as the SMK's pad) */
    uint8_t touched, what;   /* M1T_*, which control */
    int16_t value;           /* its value, 0..127 (or a wave notch) */
    uint32_t t_ms;           /* when */
    uint8_t vintage, globe;  /* the vintage on (TAPE or WARMTH up); the GLO screen open */
    uint8_t fader[4];        /* the SMK's faders, here the encoders and knobs: [0] day (127) .. night (0), [1] winter,
                              * [2] echo time, [3] echo (m1_draw.c reads [0] [1]) */
    uint32_t notes_ms;       /* the last note played (the companion listens) */
    uint8_t vel;             /* its velocity */
    uint8_t dirty;
    uint32_t active_ms;      /* the last hand on anything (the screen sleeps after GLO SLEEP's time) */
    uint8_t grow;            /* GLO: the line chosen */
    uint32_t grow_ms;
    uint8_t hold, hold_used; /* ARP or SEQ held (a B_* label, 0xFF none); a knob turned meanwhile */
    uint32_t hold_ms;        /* since when */
    const char *hlab;        /* M1T_HOLD: the words */
    char hval[16], hbuf[20];
    uint8_t list;            /* SEQ held: the tape's list on the screen */
    uint8_t rec_down;        /* REC held (1), the sketch asked (2) */
    uint32_t rec_ms, octdn_ms;
    uint8_t oct_held, oct_both; /* OCT- (1) OCT+ (2) down; both held: 1, the tape started or stopped: 2 */
    int8_t oct_before;          /* the octave before the first of the two (both held: no octave change) */
    uint32_t both_ms;
    uint8_t page;            /* PG_* */
    uint8_t mod, speed;      /* LFO MOD and SPEED 0..127 (kept across the sounds, as the SMK's mod wheel) */
    uint8_t vib, trem;       /* LFO VIBRATO, TREMOLO 0..127 (the same) */
    uint8_t tape, warm;      /* FX TAPE, WARMTH 0..127 */
    uint8_t save_down;       /* SAVE held (its press time: save_ms); 2: the long press done */
    uint32_t save_ms;
    uint32_t beat;           /* the ARP's steps, the tape's notes (the companion dances on them) */
    uint8_t dancing;
    uint8_t calin;           /* SEL: a cuddle asked (m1_draw.c cat_think plays it) */
    uint32_t calin_ms;       /* (its LED lit a moment) */
} m1 = {.fader = {127, 0, 64, 40}, .grow = 0, .hold = 0xFF, .speed = 60};

/* GLO, the settings: a line each, chosen with K1, changed with K2. Kept with the settings, in bytes no engine uses:
 * favorites.factory[13] 24..31 (as on the SMK-37). [0] 'M' once written */
enum { G1_PET, G1_CAT, G1_CH, G1_TUNE, G1_EQ, G1_SCR, G1_N };
static const char *const G1_NAME[G1_N] = {"COMPANION", "MOOD", "MIDI CHANNEL", "TUNING", "SPEAKER", "SLEEP"};
static const char *const G1_SCR_N[5] = {"NEVER", "5 MIN", "15 MIN", "30 MIN", "60 MIN"};
static const char *const G1_CAT_N[3] = {"LIVELY", "CALM", "QUIET"};
#define M1G (favorites.factory[13] + 24)
enum { M1G_MAGIC, M1G_CAT = 3, M1G_PET = 7 };                                /* ([4] [5]: TUNE, MIDI CH) */
#define G1_FOOT "K1 LINE  K2 VALUE"
static uint32_t pet_n(void);
static const char *pet_name(uint32_t v);
static void pet_pick(uint32_t v);

static uint32_t g1_n(uint32_t r)
{
    return r == G1_CH ? menu_n(MI_TCH1) : r == G1_TUNE ? menu_n(MI_TUNE) : r == G1_EQ ? menu_n(MI_LOWCUT) :
           r == G1_SCR ? 5u : r == G1_PET ? pet_n() : 3u;
}
static uint32_t g1_get(uint32_t r)
{
    return r == G1_CH ? menu_get(MI_TCH1) : r == G1_TUNE ? menu_get(MI_TUNE) : r == G1_EQ ? menu_get(MI_LOWCUT) :
           r == G1_SCR ? scr_get() : r == G1_PET ? M1G[M1G_PET] % pet_n() : M1G[M1G_CAT] % 3u;
}
static const char *g1_vname(uint32_t r, char *b)                             /* (b: 12 bytes) */
{
    uint32_t v = g1_get(r);
    switch (r) {
    case G1_CH: return menu_vname(MI_TCH1, v);
    case G1_TUNE: str_cpy(b, menu_vname(MI_TUNE, v), 8); str_cpy(b + str_len(b), " CT", 4); return b;
    case G1_EQ: return menu_vname(MI_LOWCUT, v);
    case G1_SCR: return G1_SCR_N[v % 5u];
    case G1_PET: return pet_name(v);
    default: return G1_CAT_N[v];
    }
}
/* m1_draw.c: the line lit, and what the left column says */
static int g1_hot(uint32_t r) { return m1.grow == r; }
static void g1_klab(uint32_t r, char *k) { k[0] = (char)(m1.grow == r ? '>' : ' '); k[1] = 0; }

static void g1_put(uint32_t r, uint32_t v)
{
    switch (r) {
    case G1_CH: menu_put(MI_TCH1, v); break;
    case G1_TUNE: menu_put(MI_TUNE, v); break;
    case G1_EQ: menu_put(MI_LOWCUT, v); break;
    case G1_SCR: scr_put(v); break;
    case G1_PET: M1G[M1G_PET] = (uint8_t)v; pet_pick(v); break;
    default: M1G[M1G_CAT] = (uint8_t)v; break;
    }
    M1G[M1G_MAGIC] = 'M';
    M1G[4] = (uint8_t)menu_get(MI_TUNE);
    M1G[5] = (uint8_t)menu_get(MI_TCH1);
    settings_save();                                                         /* (queued: settings_poll writes it) */
}
static void g1_line(int32_t s)
{
    m1.grow = (uint8_t)clamp((int32_t)m1.grow + (s > 0 ? 1 : -1), 0, G1_N - 1);
    m1.grow_ms = m1.active_ms = fm1_ms;
    m1.dirty = 1;
}
static void g1_turn(int32_t s)
{
    uint32_t r = m1.grow % G1_N;
    int32_t n = (int32_t)g1_n(r), v = (int32_t)g1_get(r) + (s > 0 ? 1 : -1);
    if (r == G1_TUNE && (s > 1 || s < -1))
        v += s > 0 ? 4 : -4;                                                 /* (a fast turn: 5 cents) */
    g1_put(r, (uint32_t)clamp(v, 0, n - 1));
    m1.grow_ms = m1.active_ms = fm1_ms;
    m1.dirty = 1;
}
static void g1_boot(void)                                                    /* the saved settings, at power-on */
{
    if (M1G[M1G_MAGIC] != 'M') {
        M1G[M1G_CAT] = 0;
        M1G[M1G_PET] = 0;
        return;
    }
    pet_pick(M1G[M1G_PET] % pet_n());                                       /* (the opening shows him asleep) */
    if (M1G[4] < menu_n(MI_TUNE))
        menu_put(MI_TUNE, M1G[4]);
    if (M1G[5] < menu_n(MI_TCH1))
        menu_put(MI_TCH1, M1G[5]);
}

#define M1T (&trk[0])

static void m1_touch(uint32_t kind, uint32_t what, int32_t v)
{
    m1.touched = (uint8_t)kind;
    m1.what = (uint8_t)what;
    m1.value = (int16_t)v;
    m1.t_ms = m1.active_ms = fm1_ms;
    m1.dirty = 1;
}
/* m1_draw.c: a knob's name (WINTER is the Montee on SOUFFLE / CREPUSC) */
static const char *m1_knob_name(uint32_t c)
{
    return c == C_WIN && VM_TOWN((uint32_t)M1T->p[P_E0]) ? "RISE" : M1_K_NAME[c % M1_NK];
}
/* m1_draw.c: a page's name and its knobs (k 0..3) */
static const char *m1_page_name(uint32_t pg) { return PG_NAME[pg % PG_N]; }
static const char *m1_page_knob(uint32_t pg, uint32_t k) { return m1_knob_name(PG_KNOB[pg % PG_N][k & 3u]); }

/* what the screen says (the name, the value under it) */
static void mp_show(const char *lab, const char *val)
{
    m1.hlab = lab;
    str_cpy(m1.hval, val, sizeof m1.hval);
    m1_touch(M1T_HOLD, 0, 0);
}

/* ARP: its choices (held, K1 .. K3), kept with the settings in favorites.factory[13][16..23] ([0] 'P' once written) */
#define M1P (favorites.factory[13] + 16)
enum { MP_MAGIC, MP_ATYPE, MP_ARATE, MP_AOCT, MP_BPM };
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
        M1P[MP_AOCT] = 0;
        M1P[MP_BPM] = 120 - 40;
    }
    song.g[G_CLOCK] = 0;                                                     /* (a DAW's clock: seq.c follows it) */
    song.g[G_BPM] = (int16_t)(40 + M1P[MP_BPM]);
}
static void mp_apply(track_t *t, uint32_t arp)
{
    t->p[P_AMODE] = (int16_t)(arp ? MP_ATYPE_V[M1P[MP_ATYPE]] : 0);
    t->p[P_ARATE] = (int16_t)MP_RATE_V[M1P[MP_ARATE]];
    t->p[P_AOCT] = 1;
    t->p[P_CHRD] = CH_OFF;
    t->p[P_QUANT] = QN_OFF;
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
static void mp_tempo_turn(int32_t s)
{
    char b[16];
    int32_t d = s > 0 ? 1 : -1;
    if (!song.g[G_CLOCK]) {                                                  /* (the DAW's tempo: shown only) */
        M1P[MP_BPM] = (uint8_t)clamp(M1P[MP_BPM] + (s > 1 || s < -1 ? d * 5 : d), 0, 200);
        song.g[G_BPM] = (int16_t)(40 + M1P[MP_BPM]);
        M1P[MP_MAGIC] = 'P';
        settings_save();
    }
    mp_tempo(b);
    mp_show("TEMPO", b);
}
/* ARP held, knob K1 + k turned s */
static void mp_turn(uint32_t k, int32_t s)
{
    track_t *t = M1T;
    int32_t d = s > 0 ? 1 : -1;
    uint32_t arp = t->p[P_AMODE] != 0;
    m1.hold_used = 1;
    switch (k) {
    case 0: M1P[MP_ATYPE] = (uint8_t)clamp(M1P[MP_ATYPE] + d, 0, MP_NATYPE - 1); arp = 1; break;
    case 1: M1P[MP_ARATE] = (uint8_t)clamp(M1P[MP_ARATE] + d, 0, MP_NRATE - 1); break;
    case 2: mp_tempo_turn(s); return;
    default: return;
    }
    mp_apply(t, arp);
    M1P[MP_MAGIC] = 'P';
    settings_save();
    if (k == 0)
        mp_show("ARP", MP_ATYPE_N[M1P[MP_ATYPE]]);
    else
        mp_show("SPEED", MP_RATE_N[M1P[MP_ARATE]]);
}

/* the tape's words: copied (the sketchbook's names are its own buffers) */
static void m1_say(const char *lab, const char *val)
{
    str_cpy(m1.hbuf, lab, sizeof m1.hbuf);
    mp_show(m1.hbuf, val);
}

/* ------------------------------------------------------------ the sound */
/* the waves the sound has (MODULAR TAPE FM: both; JUNO: WAVE 1); 0 none */
static uint32_t m1_waves(void)
{
    uint32_t m = (uint32_t)M1T->p[P_E0];
    return m == VM_MODULAR || m == VM_TAPE || m == VM_FM || VM_TOWN(m) ? 3u : m == VM_JUNO ? 1u : 0u;
}

/* the world (the light, the winter, the echo, the LFO, the vintage) onto the sound: it stays what the controls say
 * whatever sound plays */
static void m1_world(void)
{
    track_t *t = M1T;
    t->p[P_E6] = (int16_t)(127 - m1.fader[0]);                               /* NUIT */
    t->p[P_E7] = (int16_t)m1.fader[1];                                       /* HIVER (the Montee on TOWN) */
    m1_dly_target = 441u + m1.fader[2] * 257u;                               /* echo TIME: 10 ms .. 0.75 s */
    t->p[P_DLY] = (int16_t)m1.fader[3];                                      /* ECHO: the send .. */
    song.g[G_DFDBK] = (int16_t)(25 + m1.fader[3] * 70 / 127);               /* .. and the repeats */
    t->p[P_LD_FLT] = (int16_t)(m1.mod * 48 / 127);                           /* MOD: the cutoff waves */
    t->p[P_LD_PIT] = (int16_t)(m1.vib * 24 / 127);                          /* VIBRATO: up to a few tenths of a tone */
    t->p[P_LD_AMP] = (int16_t)(m1.trem * 90 / 127);                          /* TREMOLO */
    t->p[P_LRATE] = (int16_t)m1.speed;
    t->p[P_LWAVE] = 0;                                                       /* (a sine) */
    t->p[P_REV] = 0;                                                         /* no reverb (the city's echo only) */
    m1_vtape = (uint8_t)(m1.tape + (m1.tape >> 6));                          /* 0..128 */
    m1_vwarm = (uint8_t)(m1.warm + (m1.warm >> 6));
    m1.vintage = m1.tape || m1.warm;
    m1_vintage = m1.vintage;
}

/* SAVE: sound s kept in user preset s (upreset.c, flash), named "MC " + its name; only such a one is ours (a Felucca
 * user preset left in that slot is not) */
static int m1_saved(uint32_t s)
{
    char b[16];
    if (!up_used(s) || up_rec(s)->engine != ENGI_VILLE)
        return 0;
    up_name(s, b);
    return b[0] == 'M' && b[1] == 'C' && b[2] == ' ';
}

static void m1_sound(uint32_t s)
{
    track_t *t = M1T;
    uint32_t arp = t->p[P_AMODE] != 0;
    m1.pad = (uint8_t)(s % 16u);
    fm1_irq_off();
    t->eng_req = ENGI_VILLE;
    fm1_irq_on();
    apply_preset_to(t, M1_PAD_SOUND[m1.pad]);
    if (m1_saved(m1.pad))
        up_load(m1.pad);                                                     /* yours */
    mp_apply(t, arp);                                                        /* the ARP stays as it is */
    m1_world();
}

static void m1_save(void)
{
    char name[16] = "MC ";
    int rc;
    if (transport_busy()) {
        mp_show("SAVE", "STOP FIRST");
        return;
    }
    str_cpy(name + 3, M1_SND_NAME[m1.pad], 10);
    song.sel = 0;
    rc = up_store(m1.pad, name);
    mp_show(M1_SND_NAME[m1.pad], rc == 0 || rc == 3 ? "SAVED" : "NOT SAVED");
}
static void m1_unsave(void)                                                  /* SAVE held: the factory sound back */
{
    if (transport_busy()) {
        mp_show("SAVE", "STOP FIRST");
        return;
    }
    if (m1_saved(m1.pad))
        up_put(m1.pad, 0);
    m1_sound(m1.pad);
    mp_show(M1_SND_NAME[m1.pad], "FACTORY");
}

static void m1_init(void)
{
    uint32_t k, i;
    /* the same on every FM-1: nothing of a Felucca session nor of its settings that are not Modular Compagnon's */
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0; i++)
            if (param_kept(i))
                trk[k].p[i] = TP[i].def;
        track_defaults_steps(&trk[k]);
    }
    song.octave = 0;
    song.rec = 0;
    fm1_led_dim_level(0);
    for (k = 1; k < NTRK; k++)                                               /* parts 2..4 silent */
        trk[k].p[P_LEVEL] = 0;
    song.sel = 0;
    song.g[G_RTYPE] = 0;
    song.g[G_DTIME] = 1;
    song.g[G_DMIX] = 100;
    g1_boot();
    mp_boot();
    m1_sound(0);
    m1_touch(M1T_NONE, 0, 0);
}

/* a knob of the page (or an encoder) turned s */
static void m1_knob(uint32_t c, int32_t s)
{
    track_t *t = M1T;
    static const uint8_t PARAM[M1_NK] = {[C_ATK] = P_ATK, [C_DEC] = P_DEC, [C_SUS] = P_SUS, [C_REL] = P_REL,
                                         [C_CUT] = P_E3, [C_RES] = P_E4, [C_DTN] = P_E5, [C_GLD] = P_GLIDE};
    uint8_t *w = c == C_DAY ? &m1.fader[0] : c == C_WIN ? &m1.fader[1] : c == C_TIME ? &m1.fader[2] :
                 c == C_ECHO ? &m1.fader[3] : c == C_MOD ? &m1.mod : c == C_SPD ? &m1.speed : c == C_VIB ? &m1.vib : c == C_TREM ? &m1.trem : c == C_TAPE ? &m1.tape :
                 c == C_WARM ? &m1.warm : 0;
    int32_t v;
    if (s > 1 || s < -1)
        s *= 2;                                                              /* a fast turn goes further */
    if (w) {                                                                 /* the world */
        v = clamp((int32_t)*w + s, 0, 127);
        *w = (uint8_t)v;
        m1_world();
        m1_touch(M1T_KNOB, c, v);
        return;
    }
    if (c == C_VOICE) {                                                      /* POLY / MONO (legato: the glide joins) */
        t->p[P_VOICE] = (int16_t)(s > 0 ? V_LEGATO : V_POLY);
        mp_show("VOICE", t->p[P_VOICE] ? "MONO" : "POLY");
        return;
    }
    if (c == C_W1 || c == C_W2) {                                            /* the next / previous wave */
        uint32_t wb = c == C_W1 ? 1u : 2u, p = c == C_W1 ? P_E1 : P_E2, k;
        static const int16_t WV[4] = {0, 43, 64, 127};                       /* saw, between, triangle, square */
        if (!(m1_waves() & wb)) {
            mp_show(M1_K_NAME[c], "NONE");
            return;
        }
        for (k = 0; k < 3u && WV[k + 1u] <= t->p[p]; k++)
            ;
        k = (uint32_t)clamp((int32_t)k + (s > 0 ? 1 : -1), 0, 3);
        t->p[p] = WV[k];
        m1_touch(M1T_WAVE, wb, t->p[p]);
        return;
    }
    v = clamp(t->p[PARAM[c]] + s, 0, 127);
    t->p[PARAM[c]] = (int16_t)v;
    m1_touch(M1T_KNOB, c, v);
}

static void m1_page(uint32_t pg)
{
    m1.page = (uint8_t)pg;
    m1.globe = 0;
    m1_touch(M1T_PAGE, pg, 0);
}

static void m1_btn(uint32_t b, int down)
{
    if (!down) {
        if (b == m1.hold) {                                                  /* ARP / SEQ let go */
            if (!m1.hold_used && b == B_ARP) {                               /* a tap: on / off */
                mp_apply(M1T, !M1T->p[P_AMODE]);
                mp_show("ARP", M1T->p[P_AMODE] ? MP_ATYPE_N[M1P[MP_ATYPE]] : "OFF");
            } else if (b == B_SEQ && m1.hold_used == 1u && pl_count) {      /* the list, a piece chosen: it plays */
                if (demo_on) {
                    pl_target = pl_cursor;
                    demo_req = 6;
                } else {
                    pl_start = pl_cursor;
                    demo_req = 1;
                }
            }
            if (b == B_SEQ) {
                m1.list = 0;
                pl_del = 0;
                m1.dirty = 1;
            }
            m1.hold = 0xFF;
        }
        if (b == B_REC)
            m1.rec_down = 0;
        if (b == B_OCTDN || b == B_OCTUP) {                                  /* the tape: OCT- / OCT+ act when let go */
            if (demo_on && !m1.oct_both) {
                if (b == B_OCTUP)
                    demo_req = 3;
                else
                    demo_req = m1.octdn_ms && fm1_ms - m1.octdn_ms < 1000u ? 5u : 4u;
                m1.octdn_ms = b == B_OCTDN && demo_req == 4u ? (fm1_ms | 1u) : 0u;
                m1.active_ms = fm1_ms;
            }
            m1.oct_held &= (uint8_t)~(b == B_OCTDN ? 1u : 2u);
            if (!m1.oct_held)
                m1.oct_both = 0;
        }
        if (b == B_SAVE && m1.save_down) {
            if (m1.save_down == 1u)
                m1_save();                                                   /* (a long press did its job already) */
            m1.save_down = 0;
        }
        return;
    }
    switch (b) {
    case B_OCTDN: case B_OCTUP: {
        char o[6] = "+";
        if (m1.oct_held) {                                                   /* the other one is down: both held */
            m1.oct_held |= b == B_OCTDN ? 1u : 2u;
            m1.oct_both = 1;
            m1.both_ms = fm1_ms;
            if (!demo_on)
                song.octave = m1.oct_before;                                 /* (no octave change for the gesture) */
            break;
        }
        m1.oct_held = b == B_OCTDN ? 1u : 2u;
        m1.oct_before = song.octave;
        if (demo_on)                                                         /* the tape: next, start, the one before */
            break;                                                           /* (when let go: m1_btn up) */
        song.octave = (int8_t)clamp(song.octave + (b == B_OCTUP ? 1 : -1), -3, 3);
        fmt_int(o + (song.octave > 0), song.octave);
        mp_show("OCTAVE", o);
        break;
    }
    case B_ARP: case B_SEQ:                                                  /* (what they do: when let go, or held) */
        m1.hold = (uint8_t)b;
        m1.hold_used = 0;
        m1.hold_ms = fm1_ms;
        if (b == B_SEQ) {                                                    /* the tape's list */
            m1.list = 1;
            pl_cursor = (uint8_t)(demo_on ? pl_idx : pl_start);
            pl_del = 0;
            m1.globe = 0;
            m1.active_ms = fm1_ms;
            m1.dirty = 1;
        }
        break;
    case B_PLAY:
        if (!pl_count && !demo_on) {
            m1_say("TAPE", "EMPTY");
            break;
        }
        demo_req = demo_on ? 2u : 1u;                                        /* (the reels say it) */
        m1.touched = M1T_NONE;
        m1.active_ms = fm1_ms;
        m1.dirty = 1;
        break;
    case B_REC:
        if (m1.list) {                                                       /* in the list: erase this sketch? */
            if (pl_cursor < pl_songs) {
                m1_say(pl_item[pl_cursor].name, "A SONG");                   /* (the songs: from the Carnet app) */
            } else if (pl_cursor < pl_count && pl_del == pl_cursor + 1u) {
                m1_del_req = (uint8_t)(pl_cursor + 1u);                      /* REC again: erased (m1_book.c) */
                pl_del = 0;
            } else if (pl_cursor < pl_count) {
                pl_del = (uint8_t)(pl_cursor + 1u);
            }
            m1.hold_used = 2;                                                /* (letting SEQ go plays nothing) */
            m1.dirty = 1;
            break;
        }
        m1.rec_down = 1;
        m1.rec_ms = fm1_ms;
        break;
    case B_SAVE: m1.save_down = 1; m1.save_ms = fm1_ms; break;
    case B_SCL:                                                              /* SEL (printed so; SCL in panel.c): a cuddle */
        m1.calin = 1;
        m1.calin_ms = m1.active_ms = fm1_ms;
        if (127 - m1.fader[0] > 80)
            mp_show("SHHH", "ASLEEP");                                       /* (at night he sleeps on) */
        break;
    case B_GLO:
        m1.globe ^= 1u;
        m1.active_ms = fm1_ms;
        m1.dirty = 1;
        break;
    case B_HOME: m1_page(PG_HOME); break;
    case B_ENV: m1_page(PG_ENV); break;
    case B_EDIT: m1_page(PG_EDIT); break;
    case B_LFO: m1_page(PG_LFO); break;
    case B_FX: m1_page(PG_FX); break;
    default: break;
    }
}

static void m1_input(void)
{
    uint32_t released = 0, pressed = fm1_input_edges(&released), k;
    int32_t s;
    master_poll();
    midi_take();
    {   /* the volume (the MASTER pot): only to show it */
        static int32_t last = -1;
        int32_t v = master_knob / 16;
        if (last >= 0 && (v - last > 6 || last - v > 6))
            m1_touch(M1T_VOL, 0, v * 127 / 1023);
        if (last < 0 || v - last > 6 || last - v > 6)
            last = v;
    }
    s = panel_enc(EN_PRESET);
    if (s && m1.globe) {
        g1_turn(s);
    } else if (s) {                                                          /* a sound */
        m1_sound((uint32_t)clamp((int32_t)m1.pad + (s > 0 ? 1 : -1), 0, 15));
        m1_touch(M1T_PAD, m1.pad, m1_saved(m1.pad));
    }
    s = panel_enc(EN_SELECT);
    if (s && m1.globe)
        g1_line(s);
    else if (s)
        m1_knob(C_DAY, s * 4);                                               /* (32 clicks from night to day) */
    s = panel_enc(EN_ALGO);
    if (s && !m1.globe)
        m1_knob(C_WIN, s * 4);
    for (k = 0; k < 4u; k++) {                                               /* K1 .. K4 */
        s = panel_enc(EN_K1 + k);
        if (!s)
            continue;
        if (m1.globe) {
            m1.active_ms = fm1_ms;
            if (k == 0)
                g1_line(s);
            else if (k == 1)
                g1_turn(s);
        } else if (m1.hold == B_ARP) {                                       /* ARP held: choose */
            mp_turn(k, s);
        } else if (m1.hold == B_SEQ) {                                       /* SEQ held: the tape's list */
            if (k == 0 && pl_count) {
                pl_cursor = (uint8_t)clamp((int32_t)pl_cursor + (s > 0 ? 1 : -1), 0, (int32_t)pl_count - 1);
                pl_del = 0;
                if (m1.hold_used != 2u)
                    m1.hold_used = 1;
                m1.dirty = 1;
            }
        } else {
            m1_knob(PG_KNOB[m1.page % PG_N][k], s);
        }
    }
    for (k = 0; k < FM1_NBTN; k++) {
        if ((pressed >> k) & 1u)
            m1_btn(panel_btn_of(k), 1);
        if ((released >> k) & 1u)
            m1_btn(panel_btn_of(k), 0);
    }
    if (m1.oct_both == 1u && m1.oct_held == 3u && fm1_ms - m1.both_ms >= 1000u) {   /* OCT- OCT+ held 1 s: the tape */
        m1.oct_both = 2;
        if (pl_count || demo_on)
            demo_req = demo_on ? 2u : 1u;
        else
            m1_say("TAPE", "EMPTY");
        m1.touched = M1T_NONE;
        m1.active_ms = fm1_ms;
        m1.dirty = 1;
    }
    if (m1.rec_down == 1u && fm1_ms - m1.rec_ms >= 600u) {                   /* REC held: keep the last phrase */
        m1.rec_down = 2;
        m1_keep_req = 1;
        m1.active_ms = fm1_ms;
    }
    if (m1.save_down == 1u && fm1_ms - m1.save_ms >= 2000u) {                /* SAVE held 2 s: the factory sound */
        m1.save_down = 2;
        m1_unsave();
    }
    if (fm1_input_note_edges())                                              /* the companion listens */
        m1.notes_ms = m1.active_ms = fm1_ms, m1.vel = (uint8_t)kb_vel;
    {   /* the ARP's steps, the tape's notes: the companion dances on them */
        static uint32_t last_arp, last_tape, beat_ms;
        track_t *t = M1T;
        uint32_t a = t->arp_idx, q = demo_i;
        int arp = t->p[P_AMODE] && (t->arp_note || t->arp_phys), tape = demo_on && !pl_wait;
        if ((arp && a != last_arp) || (tape && q != last_tape && fm1_ms - beat_ms > 90u)) {
            m1.beat++;                                                       /* (the tape: at most a step in 90 ms) */
            beat_ms = fm1_ms;
        }
        last_arp = a;
        last_tape = q;
        m1.dancing = (uint8_t)(beat_ms && fm1_ms - beat_ms < 700u);         /* (the gaps between the steps too) */
        if (m1.dancing)
            m1.active_ms = fm1_ms;
    }
}

static void panel_led(uint32_t label, int on) { fm1_led_key(panel.btn[label], on); }

static void m1_leds(void)
{
    static const uint8_t PB[PG_N] = {B_HOME, B_ENV, B_EDIT, B_LFO, B_FX};
    uint32_t p;
    for (p = 0; p < PG_N; p++)
        panel_led(PB[p], !m1.globe && m1.page == p);
    panel_led(B_GLO, m1.globe);
    panel_led(B_ARP, M1T->p[P_AMODE] != 0);
    panel_led(B_OCTDN, song.octave < 0);
    panel_led(B_OCTUP, song.octave > 0);
    panel_led(B_PLAY, demo_on != 0);
    panel_led(B_REC, m1.rec_down != 0 || (m1.list && pl_del));
    panel_led(B_SEQ, m1.list);
    panel_led(B_SAVE, m1_saved(m1.pad));
    panel_led(B_SCL, m1.calin_ms && fm1_ms - m1.calin_ms < 2500u);
}
