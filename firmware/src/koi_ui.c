/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Koi on the FM-1: the controls. The FM-1 is a steel tongue drum held like a Game Boy, played with the thumbs
 * (the keys: koi_play.c), over a pond of koi seen from above (koi_draw.c).
 *   PRESETS      the drum's scale (C MAJOR, the composer's drum; PENTATONIC MINOR AKEBONO DORIAN HIJAZ)
 *   ALGORITHM    its material: STEEL, LOW DRUM (an octave down, longer), GLASS, WOOD
 *   SELECT       the day (right) and the night (left): the pond, the rain's mood, the drum a little darker
 *   K1 STRIKE    soft (a felt finger) .. hard (the tongue's own modes, the finger's tock); hard at power-on
 *   K2 RESONANCE how long it rings (0 at power-on: short; the middle: the composer's drum, ~6 s)
 *   K3 BOUNCE    the ripples answering at the pond's edge (0, at power-on: none)
 *   K4           nothing: no echo, no hall (the drum dry, as it is)
 *   ARP          the rain: the drum plays alone (koi_play.c); a tap: on / off
 *   PLAY, OCT- + OCT+ held 1 s, OCT+ / OCT- while it plays, REC held, SEQ held (the list, REC: erase): the tape and the
 *                sketchbook, exactly as in Modular One (m1_tape.c, m1_book.c)
 *   OCT- / OCT+  the drum an octave down / up
 *   SAVE         keeps your drum (scale, material, the four knobs, the light); held 2 s: the factory drum
 *   SEL          feeds the koi (they come)
 *   HOME ENV EDIT LFO  the knobs, their values (FX: nothing)
 *   GLO          the settings: KEY (C .. B: every scale moved up from C), KOI (how many carp, 0 .. 5), RIPPLES (the rings on the water, on / off), WINTER (snow on the pond), MIDI CHANNEL, TUNING, SPEAKER,
 *                SLEEP (K1 or SELECT the line, K2 or PRESETS its value; GLO or HOME: back)
 *   MASTER       the volume (master.c). Only part 1 plays. */
#define M1_TAPE 1

enum { KC_STRIKE, KC_RES, KC_BOUNCE, KC_N };
static const char *const KC_NAME[KC_N] = {"STRIKE", "RESONANCE", "BOUNCE"};
static const uint8_t KC_DEF[KC_N] = {127, 0, 0};

/* what the screen shows: the last control touched (koi_draw.c) */
enum { M1T_NONE, M1T_KNOB, M1T_SCALE, M1T_MAT, M1T_LIGHT, M1T_VOL, M1T_HOLD, M1T_PAGE };
static struct {
    uint8_t touched, what;
    int16_t value;
    uint32_t t_ms;
    uint32_t active_ms;      /* the last hand on anything (the screen sleeps after SLEEP's time) */
    uint8_t globe, grow, dirty;
    const char *hlab;
    char hval[16], hbuf[20];
    uint8_t k[KC_N];         /* K1 .. K3 */
    uint8_t scale, mat, day; /* day: 127 day .. 0 night */
    uint8_t fx;              /* FX: the old tape */
    uint8_t hold, hold_used; /* SEQ held (B_SEQ, 0xFF none) */
    uint32_t hold_ms;
    uint8_t list;
    uint8_t rec_down;
    uint32_t rec_ms, octdn_ms;
    uint8_t oct_held, oct_both;
    int8_t oct_before;
    uint32_t both_ms;
    uint8_t save_down;
    uint32_t save_ms;
    uint32_t feed_ms;        /* SEL: food on the water (koi_draw.c) */
    uint32_t notes_ms;
} m1 = {.k = {127, 0, 0}, .day = 127, .hold = 0xFF};

/* kept with the settings, in bytes no engine uses (favorites.factory[13] 16..31, as Modular Compagnon's): the drum
 * SAVEd ([0] 'M': test 4's defaults), the settings ([8] 'G') */
#define KS (favorites.factory[13] + 16)
enum { KS_MAGIC, KS_SCALE, KS_MAT, KS_K1, KS_DAY = KS_K1 + KC_N, KS_G = 8, KS_KOI, KS_WINTER, KS_TUNE, KS_CH, KS_RIPPLE, KS_KEY };

/* ------------------------------------------------------------ GLO, the settings */
enum { G1_KEY, G1_KOI, G1_RIPPLE, G1_WINTER, G1_CH, G1_TUNE, G1_EQ, G1_SCR, G1_N };
static const char *const G1_NAME[G1_N] = {"KEY", "KOI", "RIPPLES", "WINTER", "MIDI CHANNEL", "TUNING", "SPEAKER", "SLEEP"};
static const char *const G1_SCR_N[5] = {"NEVER", "5 MIN", "15 MIN", "30 MIN", "60 MIN"};
#define G1_FOOT "K1 LINE  K2 VALUE"
static uint8_t koi_n = 3, koi_winter, koi_ripple = 1;                       /* (koi_draw.c) */
static uint8_t koi_tonic;                                                      /* GLO KEY: 0 C .. 11 B */
static void koi_world(void);
/* the scale's name with its key: "D MAJOR"; short: the tape's ("F# PENTA") */
static const char *koi_scale_name(char *b, uint32_t n, uint32_t scale, int brief)
{
    str_cpy(b, KOI_KEY_N[koi_tonic % 12u], n);
    str_cpy(b + str_len(b), " ", n - str_len(b));
    str_cpy(b + str_len(b), (brief ? KOI_SCALE_S : KOI_SCALE_N)[scale % KOI_NSCALE], n - str_len(b));
    return b;
}

static uint32_t g1_n(uint32_t r)
{
    return r == G1_CH ? menu_n(MI_TCH1) : r == G1_TUNE ? menu_n(MI_TUNE) : r == G1_EQ ? menu_n(MI_LOWCUT) :
           r == G1_SCR ? 5u : r == G1_KOI ? 6u : r == G1_KEY ? 12u : 2u;
}
static uint32_t g1_get(uint32_t r)
{
    return r == G1_CH ? menu_get(MI_TCH1) : r == G1_TUNE ? menu_get(MI_TUNE) : r == G1_EQ ? menu_get(MI_LOWCUT) :
           r == G1_SCR ? scr_get() : r == G1_KOI ? koi_n : r == G1_KEY ? koi_tonic : r == G1_RIPPLE ? koi_ripple : koi_winter;
}
static const char *g1_vname(uint32_t r, char *b)                             /* (b: 12 bytes) */
{
    uint32_t v = g1_get(r);
    switch (r) {
    case G1_CH: return menu_vname(MI_TCH1, v);
    case G1_TUNE: str_cpy(b, menu_vname(MI_TUNE, v), 8); str_cpy(b + str_len(b), " CT", 4); return b;
    case G1_EQ: return menu_vname(MI_LOWCUT, v);
    case G1_SCR: return G1_SCR_N[v % 5u];
    case G1_KEY: return KOI_KEY_N[v % 12u];
    case G1_KOI: fmt_int(b, (int32_t)v); return v ? b : "NONE";
    default: return v ? "ON" : "OFF";
    }
}
static int g1_hot(uint32_t r) { return m1.grow == r; }
static void g1_put(uint32_t r, uint32_t v)
{
    switch (r) {
    case G1_CH: menu_put(MI_TCH1, v); break;
    case G1_TUNE: menu_put(MI_TUNE, v); break;
    case G1_EQ: menu_put(MI_LOWCUT, v); break;
    case G1_SCR: scr_put(v); break;
    case G1_KEY: koi_tonic = (uint8_t)v; koi_world(); break;
    case G1_KOI: koi_n = (uint8_t)v; break;
    case G1_RIPPLE: koi_ripple = (uint8_t)v; break;
    default: koi_winter = (uint8_t)v; break;
    }
    KS[KS_G] = 'G';
    KS[KS_KOI] = koi_n;
    KS[KS_WINTER] = koi_winter;
    KS[KS_RIPPLE] = (uint8_t)(koi_ripple ? 2u : 1u);                         /* (0: never written: on) */
    KS[KS_KEY] = koi_tonic;
    KS[KS_TUNE] = (uint8_t)menu_get(MI_TUNE);
    KS[KS_CH] = (uint8_t)menu_get(MI_TCH1);
    settings_save();
}
static void g1_line(int32_t s)
{
    m1.grow = (uint8_t)clamp((int32_t)m1.grow + (s > 0 ? 1 : -1), 0, G1_N - 1);
    m1.active_ms = fm1_ms;
    m1.dirty = 1;
}
static void g1_turn(int32_t s)
{
    uint32_t r = m1.grow % G1_N;
    int32_t n = (int32_t)g1_n(r), v = (int32_t)g1_get(r) + (s > 0 ? 1 : -1);
    if (r == G1_TUNE && (s > 1 || s < -1))
        v += s > 0 ? 4 : -4;
    g1_put(r, (uint32_t)clamp(v, 0, n - 1));
    m1.active_ms = fm1_ms;
    m1.dirty = 1;
}

/* ------------------------------------------------------------ the words on the screen */
static void m1_touch(uint32_t kind, uint32_t what, int32_t v)
{
    m1.touched = (uint8_t)kind;
    m1.what = (uint8_t)what;
    m1.value = (int16_t)v;
    m1.t_ms = m1.active_ms = fm1_ms;
    m1.dirty = 1;
}
static void mp_show(const char *lab, const char *val)
{
    m1.hlab = lab;
    str_cpy(m1.hval, val, sizeof m1.hval);
    m1_touch(M1T_HOLD, 0, 0);
}
static void m1_say(const char *lab, const char *val)                         /* (the tape's words: copied) */
{
    str_cpy(m1.hbuf, lab, sizeof m1.hbuf);
    mp_show(m1.hbuf, val);
}

/* ------------------------------------------------------------ the drum */
#define M1T (&trk[0])
static void koi_world(void)
{
    track_t *t = M1T;
    uint32_t i;
    for (i = 0; i < 11u; i++)
        koi_tongue[i] = (uint8_t)(KOI_SCALES[m1.scale % KOI_NSCALE][i] + koi_tonic % 12u);
    koi_mat = (uint8_t)(m1.mat % KOI_NMAT);
    koi_strike = m1.k[KC_STRIKE];
    koi_res = m1.k[KC_RES];
    koi_bounce = m1.k[KC_BOUNCE];
    koi_night = (uint8_t)(127 - m1.day);
    t->p[P_DLY] = 0;                                                         /* dry: no echo, no hall */
    t->p[P_REV] = 0;
    t->p[P_CHOR] = 0;
    t->p[P_DIST] = 0;
    t->p[P_VOICE] = V_POLY;
    t->p[P_AMODE] = 0;                                                       /* (the rain is Koi's, not the ARP's) */
    t->p[P_CHRD] = CH_OFF;
    t->p[P_QUANT] = QN_OFF;
    m1_vintage = 0;
}
static int koi_saved(void)
{
    uint32_t i;
    if (KS[KS_MAGIC] != 'M' || KS[KS_SCALE] != m1.scale || KS[KS_MAT] != m1.mat || KS[KS_DAY] != m1.day)
        return 0;
    for (i = 0; i < KC_N; i++)
        if (KS[KS_K1 + i] != m1.k[i])
            return 0;
    return 1;
}
static void koi_save(void)
{
    uint32_t i;
    KS[KS_MAGIC] = 'M';
    KS[KS_SCALE] = m1.scale;
    KS[KS_MAT] = m1.mat;
    KS[KS_DAY] = m1.day;
    for (i = 0; i < KC_N; i++)
        KS[KS_K1 + i] = m1.k[i];
    settings_save();
    mp_show("YOUR DRUM", "SAVED");
}
static void koi_factory(int say)
{
    uint32_t i;
    m1.scale = 0;
    m1.mat = 0;
    m1.day = 127;
    for (i = 0; i < KC_N; i++)
        m1.k[i] = KC_DEF[i];
    koi_world();
    if (say)
        mp_show("THE DRUM", "FACTORY");
}

static void m1_init(void)
{
    uint32_t k, i;
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
    song.sel = 0;
    fm1_led_dim_level(0);
    for (k = 1; k < NTRK; k++)                                               /* parts 2..4 silent */
        trk[k].p[P_LEVEL] = 0;
    song.g[G_RTYPE] = 2;                                                     /* (a hall) */
    song.g[G_RSIZE] = 90;
    song.g[G_DTIME] = 1;
    song.g[G_DMIX] = 100;
    song.g[G_DCOLOR] = 30;                                                   /* (a darker echo) */
    song.g[G_CLOCK] = 0;
    fm1_irq_off();
    M1T->eng_req = ENGI_VILLE;                                               /* (13: KOI in this build) */
    fm1_irq_on();
    apply_preset_to(M1T, 0);
    koi_factory(0);
    if (KS[KS_MAGIC] == 'M') {                                               /* your drum */
        m1.scale = (uint8_t)(KS[KS_SCALE] % KOI_NSCALE);
        m1.mat = (uint8_t)(KS[KS_MAT] % KOI_NMAT);
        m1.day = (uint8_t)(KS[KS_DAY] > 127u ? 127u : KS[KS_DAY]);
        for (i = 0; i < KC_N; i++)
            m1.k[i] = (uint8_t)(KS[KS_K1 + i] > 127u ? KC_DEF[i] : KS[KS_K1 + i]);
    }
    if (KS[KS_G] == 'G') {
        koi_n = (uint8_t)(KS[KS_KOI] % 6u);
        koi_winter = (uint8_t)(KS[KS_WINTER] & 1u);
        koi_ripple = KS[KS_RIPPLE] != 1u;
        koi_tonic = (uint8_t)(KS[KS_KEY] % 12u);
        if (KS[KS_TUNE] < menu_n(MI_TUNE))
            menu_put(MI_TUNE, KS[KS_TUNE]);
        if (KS[KS_CH] < menu_n(MI_TCH1))
            menu_put(MI_TCH1, KS[KS_CH]);
    }
    koi_world();
    m1_touch(M1T_NONE, 0, 0);
}

/* a knob turned s */
static void koi_knob(uint32_t c, int32_t s)
{
    if (s > 1 || s < -1)
        s *= 2;                                                              /* (a fast turn goes further) */
    m1.k[c] = (uint8_t)clamp((int32_t)m1.k[c] + s, 0, 127);
    koi_world();
    m1_touch(M1T_KNOB, c, m1.k[c]);
}

static void m1_btn(uint32_t b, int down)
{
    if (!down) {
        if (b == m1.hold) {                                                  /* SEQ let go */
            if (m1.hold_used == 1u && pl_count) {                            /* a piece chosen in the list: it plays */
                if (demo_on) {
                    pl_target = pl_cursor;
                    demo_req = 6;
                } else {
                    pl_start = pl_cursor;
                    demo_req = 1;
                }
            }
            m1.list = 0;
            pl_del = 0;
            m1.dirty = 1;
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
                koi_save();
            m1.save_down = 0;
        }
        return;
    }
    switch (b) {
    case B_OCTDN: case B_OCTUP: {
        char o[6] = "+";
        if (m1.oct_held) {                                                   /* both held: the tape's gesture */
            m1.oct_held |= b == B_OCTDN ? 1u : 2u;
            m1.oct_both = 1;
            m1.both_ms = fm1_ms;
            if (!demo_on)
                song.octave = m1.oct_before;
            break;
        }
        m1.oct_held = b == B_OCTDN ? 1u : 2u;
        m1.oct_before = song.octave;
        if (demo_on)
            break;
        song.octave = (int8_t)clamp(song.octave + (b == B_OCTUP ? 1 : -1), -1, 1);
        fmt_int(o + (song.octave > 0), song.octave);
        mp_show("OCTAVE", o);
        break;
    }
    case B_SEQ:                                                              /* the tape's list */
        m1.hold = (uint8_t)b;
        m1.hold_used = 0;
        m1.hold_ms = fm1_ms;
        m1.list = 1;
        pl_cursor = (uint8_t)(demo_on ? pl_idx : pl_start);
        pl_del = 0;
        m1.globe = 0;
        m1.active_ms = fm1_ms;
        m1.dirty = 1;
        break;
    case B_ARP:                                                              /* the rain */
        koi_rain_on ^= 1u;
        mp_show("RAIN", koi_rain_on ? "ON" : "OFF");
        break;
    case B_PLAY:
        if (!pl_count && !demo_on) {
            m1_say("TAPE", "EMPTY");
            break;
        }
        demo_req = demo_on ? 2u : 1u;
        m1.touched = M1T_NONE;
        m1.active_ms = fm1_ms;
        m1.dirty = 1;
        break;
    case B_REC:
        if (m1.list) {                                                       /* in the list: erase this sketch? */
            if (pl_cursor < pl_songs) {
                m1_say(pl_item[pl_cursor].name, "A SONG");
            } else if (pl_cursor < pl_count && pl_del == pl_cursor + 1u) {
                m1_del_req = (uint8_t)(pl_cursor + 1u);
                pl_del = 0;
            } else if (pl_cursor < pl_count) {
                pl_del = (uint8_t)(pl_cursor + 1u);
            }
            m1.hold_used = 2;
            m1.dirty = 1;
            break;
        }
        m1.rec_down = 1;
        m1.rec_ms = fm1_ms;
        break;
    case B_SAVE: m1.save_down = 1; m1.save_ms = fm1_ms; break;
    case B_SCL:                                                              /* SEL: food for the koi */
        m1.feed_ms = m1.active_ms = fm1_ms | 1u;
        mp_show("KOI", koi_n ? "DINNER" : "NO KOI");
        break;

    case B_GLO:
        m1.globe ^= 1u;
        m1.active_ms = fm1_ms;
        m1.dirty = 1;
        break;
    case B_HOME: case B_ENV: case B_EDIT: case B_LFO:
        if (m1.globe && b == B_HOME) {
            m1.globe = 0;
            m1.dirty = 1;
        }
        m1_touch(M1T_PAGE, 0, 0);
        break;
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
    } else if (s) {                                                          /* the scale */
        m1.scale = (uint8_t)clamp((int32_t)m1.scale + (s > 0 ? 1 : -1), 0, KOI_NSCALE - 1);
        koi_world();
        m1_touch(M1T_SCALE, m1.scale, koi_saved());
    }
    s = panel_enc(EN_ALGO);
    if (s && !m1.globe) {                                                    /* the material */
        m1.mat = (uint8_t)clamp((int32_t)m1.mat + (s > 0 ? 1 : -1), 0, KOI_NMAT - 1);
        koi_world();
        m1_touch(M1T_MAT, m1.mat, 0);
    }
    s = panel_enc(EN_SELECT);
    if (s && m1.globe) {
        g1_line(s);
    } else if (s) {                                                          /* the day, the night */
        m1.day = (uint8_t)clamp((int32_t)m1.day + s * 4, 0, 127);
        koi_world();
        m1_touch(M1T_LIGHT, 0, m1.day);
    }
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
        } else if (m1.hold == B_SEQ) {                                       /* the tape's list */
            if (k == 0 && pl_count) {
                pl_cursor = (uint8_t)clamp((int32_t)pl_cursor + (s > 0 ? 1 : -1), 0, (int32_t)pl_count - 1);
                pl_del = 0;
                if (m1.hold_used != 2u)
                    m1.hold_used = 1;
                m1.dirty = 1;
            }
        } else {
            if (k < KC_N)
                koi_knob(k, s);
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
    if (m1.save_down == 1u && fm1_ms - m1.save_ms >= 2000u) {                /* SAVE held 2 s: the factory drum */
        m1.save_down = 2;
        koi_factory(1);
    }
    if (fm1_input_note_edges())
        m1.notes_ms = m1.active_ms = fm1_ms;
    if (koi_rain_on || demo_on)                                              /* (the pond lives: the screen stays) */
        m1.active_ms = fm1_ms;
}

/* ------------------------------------------------------------ the LEDs: the tongues glow, a key
 * struck lights while its tongue rings; the gestures lit while held or latched */
static void m1_leds(void)
{
    uint8_t nl[LED_BUF] = {0}, nd[LED_BUF] = {0}, nb[LED_BUF] = {0};
    static const uint8_t BTN_ALL[10] = {B_SCL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY};
    static uint32_t lit_ms[27];
    static uint8_t ready;
    uint32_t k, c, held = fm1_in.notes;
    if (fm1_led_anim_on())
        return;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    for (k = 0; k < 27u; k++) {
        uint32_t code = KOI_KEY[k], id = LED_KEY0 + k;
        if ((held >> k) & 1u)
            lit_ms[k] = fm1_ms | 1u;
        if (code >= KK_SLOW) {
            uint32_t g = code - KK_SLOW, on = (koi_g_held >> g) & 1u || g < 2u,  /* (the rolls: always lit, */
                     latch = g == 2u && koi_harmo_latch;                     /* to find them) */
            led_put(on ? nl : nb, id, (int)(on || latch));
            continue;
        }
        if (lit_ms[k] && fm1_ms - lit_ms[k] < 450u)
            led_put(nl, id, 1);                                              /* struck: lit a moment */
        else
            led_put(nd, id, 1);                                              /* the tongues glow */
    }
    for (k = 0; k < 10u; k++)
        led_put(nd, panel.btn[BTN_ALL[k]], 1);
    led_put(nl, panel.btn[B_GLO], m1.globe);
    led_put(nl, panel.btn[B_ARP], koi_rain_on);
    led_put(nl, panel.btn[B_OCTDN], song.octave < 0);
    led_put(nl, panel.btn[B_OCTUP], song.octave > 0);
    led_put(nl, panel.btn[B_PLAY], demo_on != 0);
    led_put(nl, panel.btn[B_REC], m1.rec_down != 0 || (m1.list && pl_del));
    led_put(nl, panel.btn[B_SEQ], m1.list);
    led_put(nl, panel.btn[B_SAVE], koi_saved());
    led_put(nl, panel.btn[B_SCL], m1.feed_ms && fm1_ms - m1.feed_ms < 2500u);
    for (c = 0; c < LED_BUF; c++) {
        nd[c] &= (uint8_t)~(nl[c] | nb[c]);
        nb[c] &= (uint8_t)~nl[c];
    }
#ifndef KOI_HOST
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = nd[c];
        fm1_led_breath[c] = nb[c];
        fm1_led_mid[c] = 0;
    }
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = nl[c];
#else
    koi_host_leds(nl, nd, nb);
#endif
}
