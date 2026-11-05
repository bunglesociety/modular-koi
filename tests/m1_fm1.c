/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Compagnon on the FM-1: its controls (m1_ui_fm1.c) and the screen (m1_draw.c) on the host, a script of hands
 * on the FM-1's knobs and buttons. Checks each page's knobs, the sounds on PRESETS, the ARP, GLO's lines, the
 * companions, the night. Pictures of the moments that matter to OUTDIR/NAME.ppm. Exit 1 on a failure.
 *   cc -O1 -w -DMODULAR_ONE=1 -DFELUCCA_SLICE=0 -Ibuild/gen -Ifirmware/src -Itests -o build/host/m1_fm1 tests/m1_fm1.c -lm
 *   build/host/m1_fm1 OUTDIR */
#define main hostsim_main
#include "hostsim.c"
#undef main

/* ------------------------------------------------------------ LCD */
static uint16_t host_screen[240 * 240];
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_screen[(y + j) * 240u + x + i] = (uint16_t)((c >> 8) | (c << 8));
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_screen[(y + j) * 240u + x + i] = p[j * w + i];
}
#include "../firmware/src/gfx.c"

/* ------------------------------------------------------------ the FM-1's panel (panel.c), by hand */
#define FM1_NBTN 14u
enum { FM1_BTN_OCT_DOWN = 0, FM1_BTN_OCT_UP = 1 };   /* (fm1_input.h) */
enum { B_FX, B_SCL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC, B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };
static struct { uint8_t btn[NB]; } panel = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}};   /* (label = matrix id) */
static uint32_t panel_btn_of(uint32_t id) { return id < NB ? id : NB; }
static uint32_t host_pressed, host_released, host_notes, host_leds;
static int32_t host_enc[NE];
static uint32_t fm1_input_edges(uint32_t *rel) { uint32_t p = host_pressed; *rel = host_released; host_pressed = host_released = 0; return p; }
static uint32_t fm1_input_note_edges(void) { uint32_t n = host_notes; host_notes = 0; return n; }
static int32_t panel_enc(uint32_t e) { int32_t s = host_enc[e]; host_enc[e] = 0; return s; }
static int32_t master_knob = 8000;
static void master_poll(void) {}
static void midi_take(void) {}
static void fm1_led_key(uint32_t k, int on) { host_leds = on ? host_leds | 1u << k : host_leds & ~(1u << k); }
static void apply_preset_to(track_t *t, uint32_t pi) { host_preset(t, ENGI_VILLE, pi); }
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
/* MENU's rows GLO uses (menu_items.c, ui.c), as plain values */
enum { MI_TUNE, MI_TCH1, MI_LOWCUT };
static uint32_t host_menu[3] = {50, 0, 0}, host_scr, host_saves, host_lcd_on = 1;
static uint32_t menu_n(uint32_t r) { return r == MI_TUNE ? 101u : r == MI_LOWCUT ? 3u : 18u; }
static uint32_t menu_get(uint32_t r) { return host_menu[r % 3u]; }
static void menu_put(uint32_t r, uint32_t v) { host_menu[r % 3u] = v; }
static const char *menu_vname(uint32_t r, uint32_t v)
{
    static char b[8];
    static const char *const EQ[3] = {"OFF", "LOW CUT", "BASS+"};
    if (r == MI_LOWCUT) return EQ[v % 3u];
    if (r == MI_TCH1 && v >= 16u) return v == 16u ? "OMNI" : "OFF";
    b[0] = '+';
    fmt_int(b + (r == MI_TUNE && (int32_t)v > 50), r == MI_TUNE ? (int32_t)v - 50 : (int32_t)v + 1);
    return b;
}
static const uint8_t SCR_MIN[5] = {0, 5, 15, 30, 60};
static uint32_t scr_get(void) { return host_scr; }
static void scr_put(uint32_t v) { host_scr = v; }
static void settings_save(void) { host_saves++; }
static void lcd_power(uint32_t s) { host_lcd_on = s != 0u; }
#ifndef HOST_HAS_FAVORITES
static struct { uint8_t factory[16][32]; uint32_t user, filter; } favorites;
#endif
static int param_kept(uint32_t i)
{
    return i == P_LEVEL || i == P_PAN || i == P_MUTE || (i >= P_AMODE && i <= P_SGATE) ||
           (i >= P_SLCR && i <= P_SLDEPTH) || i == P_CHRD || i == P_VOIC || i == P_SQNT;
}
static void track_defaults_steps(track_t *t) { memset(t->step, 0, sizeof t->step); }   /* (ui.c: step_clear each) */
static void fm1_led_dim_level(uint32_t lo) { (void)lo; }
static int step_on(const step_t *st) { return st->time == ST_NOTE && (st->n || st->hit); }   /* (ui.c) */
static int transport_busy(void) { return song.playing != 0; }
/* the user presets (upreset.c), in RAM */
typedef struct { uint8_t used, engine; char name[13]; int16_t p[P_COUNT]; } host_up_t;
static host_up_t host_up[32];
static uint32_t host_flash_writes;
static int up_used(uint32_t k) { return k < 32u && host_up[k].used; }
static const host_up_t *up_rec(uint32_t k) { return &host_up[k % 32u]; }
static void up_name(uint32_t k, char *b) { str_cpy(b, host_up[k % 32u].name, 13); }
static int up_store(uint32_t k, const char *name)
{
    host_up_t *u = &host_up[k % 32u];
    u->used = 1; u->engine = (uint8_t)trk[0].eng_req;
    str_cpy(u->name, name, 13);
    memcpy(u->p, trk[0].p, sizeof u->p);
    host_flash_writes++;
    return 0;
}
static int up_load(uint32_t k)
{
    uint32_t i;
    if (!up_used(k)) return 1;
    for (i = 0; i < P_COUNT; i++)
        if (!param_kept(i)) trk[0].p[i] = host_up[k].p[i];
    return 0;
}
static int up_put(uint32_t k, const void *r) { if (!r) host_up[k % 32u].used = 0; host_flash_writes++; return 0; }
#include "../firmware/src/m1_ui_fm1.c"
/* the flash of the songs and sketches (0xA0000 .. 0xDBFFF), in RAM; the editor's helpers the Carnet commands use */
#define HOST_FL0 0xA0000u
static uint8_t host_fl[0xDC000u - HOST_FL0];
static uint8_t flash_ok = 1;
static const uint8_t *host_xip(uint32_t off) { return host_fl + (off - HOST_FL0); }
#undef fm1_xip_ptr
#define fm1_xip_ptr(off) host_xip(off)
static int fl_erase4k(uint32_t off, uint32_t *took) { memset(host_fl + (off - HOST_FL0), 0xFF, 0x1000u); *took = 1; return 0; }
static int fl_write(uint32_t off, const void *p, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) host_fl[off - HOST_FL0 + i] &= ((const uint8_t *)p)[i];   /* (NOR: bits only go to 0) */
    return 0;
}
static void fl_inval(uint32_t off, uint32_t n) { (void)off; (void)n; }
static uint32_t st_crc32(const void *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu, i, k;
    for (i = 0; i < n; i++) { c ^= ((const uint8_t *)p)[i]; for (k = 0; k < 8u; k++) c = c >> 1 ^ (0xEDB88320u & -(c & 1u)); }
    return ~c;
}
static void audio_silence(void) {}
static void fm1_wdt_feed(void) { events_block(CTL); }                       /* (the audio runs while the main loop waits) */
static uint8_t host_ed[8192];
static uint32_t host_edn;
static void ed_b(uint32_t v) { if (host_edn < sizeof host_ed) host_ed[host_edn++] = (uint8_t)(v & 127u); }
static void ed_str(const char *s, uint32_t max) { uint32_t i; for (i = 0; i < max && s[i]; i++) ed_b((uint8_t)s[i]); ed_b(0); }
static uint8_t ed_smp_buf[512];
static uint32_t ed_unpack7(const uint8_t *a, uint32_t na, uint8_t *out, uint32_t max)
{
    uint32_t n = 0;
    while (na && n < max) {
        uint32_t m = *a++, j, k;
        na--;
        k = na > 7u ? 7u : na;
        if (!k || n + k > max || (m >> k)) return 0;
        for (j = 0; j < k; j++, na--) out[n++] = (uint8_t)(*a++ | ((m >> j) & 1u) << 7);
    }
    return na ? 0u : n;
}
#include "../firmware/src/m1_book.c"
#include "../firmware/src/m1_draw.c"

/* ------------------------------------------------------------ the script */
static const char *dir;
static uint32_t fails;
#define FAIL(...) (printf("m1_fm1: FAIL " __VA_ARGS__), printf("\n"), fails++)
static void shot(const char *name)
{
    char p[512];
    FILE *f;
    uint32_t i;
    snprintf(p, sizeof p, "%s/%s.ppm", dir, name);
    if (!(f = fopen(p, "wb")))
        return;
    fprintf(f, "P6 240 240 255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t v = (uint16_t)((host_screen[i] >> 8) | (host_screen[i] << 8));
        uint8_t rgb[3] = {(uint8_t)((v >> 11) << 3), (uint8_t)(((v >> 5) & 63u) << 2), (uint8_t)((v & 31u) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
static void run(uint32_t ms)
{
    uint32_t end = fm1_ms + ms;
    while ((int32_t)(fm1_ms - end) < 0) {
        m1_input();
        m1_service();
        m1_leds();
        m1_draw();
        {
            uint32_t b;
            for (b = 0; b < 15u * 44100u / 1000u / CTL; b++)
                events_block(CTL);
        }
        if (cat.box[2] && (cat.box[0] + cat.box[2] > CAT_G || cat.box[1] + cat.box[3] > CAT_SCENE))
            FAIL("frame box off the grid (clip %u)", cat.clip);
        fm1_ms += 15u;
    }
}
static void press(uint32_t b) { host_pressed = 1u << b; run(60); host_released = 1u << b; run(60); }
static void turn(uint32_t e, int32_t s) { host_enc[e] = s; run(60); }

int main(int argc, char **argv)
{
    memset(host_fl, 0xFF, sizeof host_fl);
    uint32_t k;
    dir = argc > 1 ? argv[1] : "build/m1_fm1";
    host_tracks_init();
    fm1_ms = 1000;
    host_up[1].used = 1; host_up[1].engine = 0; str_cpy(host_up[1].name, "BASS 1", 13);   /* a Felucca user preset */
    m1_init();
    m1_boot();                                                              /* (main.c: the songs and sketches kept) */
    run(6500);
    shot("01_home");
    if (spl.on) FAIL("the opening never ends");

    turn(EN_PRESET, 1); run(300);                                           /* JUNO (slot 1: Felucca's, not ours) */
    shot("02_sound");
    if (m1.pad != 1 || M1T->p[P_E0] != VM_JUNO || m1_saved(1)) FAIL("PRESETS: sound %u mode %d", m1.pad, M1T->p[P_E0]);
    for (k = 0; k < 20u; k++) turn(EN_PRESET, 1);
    if (m1.pad != 15) FAIL("PRESETS past the end: %u", m1.pad);
    for (k = 0; k < 15u; k++) turn(EN_PRESET, -1);                          /* back to MODULAR */
    run(2600);

    {   /* HOME: CUTOFF RESO DETUNE ECHO */
        int16_t c = M1T->p[P_E3], d = M1T->p[P_E5];
        turn(EN_K1, -3); turn(EN_K3, 1); turn(EN_K4, 2);
        if (M1T->p[P_E3] != c - 6 || M1T->p[P_E5] != d + 1 || m1.fader[3] != 44 || M1T->p[P_DLY] != 44)
            FAIL("HOME: cutoff %d detune %d echo %u", M1T->p[P_E3], M1T->p[P_E5], m1.fader[3]);
        shot("03_knob");
        turn(EN_K4, -2);
    }
    press(B_ENV); run(200);                                                 /* ENV: its own page */
    shot("04_page_env");
    if (m1.page != PG_ENV || !(host_leds & 1u << B_ENV) || (host_leds & 1u << B_HOME)) FAIL("ENV page / LEDs");
    {
        int16_t a = M1T->p[P_ATK], r = M1T->p[P_REL];
        turn(EN_K1, 2); turn(EN_K4, -1);
        if (M1T->p[P_ATK] != a + 4 || M1T->p[P_REL] != r - 1) FAIL("ENV K1 K4: %d %d", M1T->p[P_ATK], M1T->p[P_REL]);
    }
    press(B_EDIT); run(200);
    shot("05_page_edit");
    {   /* EDIT: WAVE 1 WAVE 2 GLIDE VOICE */
        int16_t w = M1T->p[P_E1];
        turn(EN_K1, 1); run(200);
        shot("06_wave");
        turn(EN_K3, 5); turn(EN_K4, 1);
        if (M1T->p[P_E1] <= w || M1T->p[P_GLIDE] != 10 || M1T->p[P_VOICE] != V_LEGATO)
            FAIL("EDIT: wave %d glide %d voice %d", M1T->p[P_E1], M1T->p[P_GLIDE], M1T->p[P_VOICE]);
        turn(EN_K4, -1);
        if (M1T->p[P_VOICE] != V_POLY) FAIL("EDIT K4: not POLY again");
    }
    press(B_LFO); run(200);
    shot("07_page_lfo");
    turn(EN_K1, 3); turn(EN_K2, 4); turn(EN_K3, 5); turn(EN_K4, 1);         /* MOD, VIBRATO, TREMOLO, SPEED */
    if (!M1T->p[P_LD_FLT] || !M1T->p[P_LD_PIT] || !M1T->p[P_LD_AMP] || M1T->p[P_LRATE] != 61) FAIL("LFO: mod %d vib %d trem %d speed %d",
        M1T->p[P_LD_FLT], M1T->p[P_LD_PIT], M1T->p[P_LD_AMP], M1T->p[P_LRATE]);
    press(B_FX); run(200);
    turn(EN_K3, 3); turn(EN_K4, 2); run(200);                               /* TAPE, WARMTH */
    shot("08_fx");
    if (!m1_vintage || m1_vtape != 6 || m1_vwarm != 4) FAIL("FX: vintage %u tape %u warm %u", m1_vintage, m1_vtape, m1_vwarm);
    turn(EN_K3, -3); turn(EN_K4, -2);
    if (m1_vintage) FAIL("FX: vintage still on at 0");
    {
        uint32_t t0 = m1_dly_target;
        turn(EN_K1, 3);
        if (m1_dly_target <= t0) FAIL("FX K1: ECHO TIME");
    }
    /* SELECT: day / night, ALGORITHM: the winter, on any page */
    turn(EN_SELECT, -2); turn(EN_ALGO, 3); run(200);
    shot("09_select");
    if (m1.fader[0] != 127 - 16 || m1.fader[1] != 24 || M1T->p[P_E6] != 16 || M1T->p[P_E7] != 24)
        FAIL("SELECT / ALGORITHM: day %u winter %u", m1.fader[0], m1.fader[1]);
    turn(EN_SELECT, 2); turn(EN_ALGO, -3); run(2600);

    /* ARP: a tap on / off; held: K1 mode, K2 speed, K3 tempo; the companion dances */
    press(B_ARP);
    if (M1T->p[P_AMODE] != 1) FAIL("ARP tap: %d", M1T->p[P_AMODE]);
    {
        int16_t bpm = song.g[G_BPM];
        host_pressed = 1u << B_ARP; run(100);
        turn(EN_K1, 1); turn(EN_K2, -1); turn(EN_K3, 1); run(200);
        shot("10_arp");
        host_released = 1u << B_ARP; run(100);
        if (M1T->p[P_AMODE] != 2 || M1T->p[P_ARATE] != 4 || song.g[G_BPM] != bpm + 1)
            FAIL("ARP held: mode %d rate %d bpm %d", M1T->p[P_AMODE], M1T->p[P_ARATE], song.g[G_BPM]);
    }
    fm1_in.notes = 1u << 12 | 1u << 16; run(2500);                          /* chord held: the arp plays, he dances */
    shot("11_dance");
    if (cat.clip != CAT_DANSE || m1.beat < 4u) FAIL("ARP: no dance (clip %u, %u beats)", cat.clip, m1.beat);
    {
        uint32_t b = m1.beat;
        run(1000);
        if (m1.beat - b < 3u) FAIL("ARP: the dance does not follow the steps (%u)", m1.beat - b);
    }
    fm1_in.notes = 0; run(1500);
    if (cat.clip == CAT_DANSE) FAIL("ARP: still dancing after the keys");
    press(B_ARP);
    if (M1T->p[P_AMODE]) FAIL("ARP tap off");

    /* the tape and the sketchbook: nothing yet; two phrases played, 5 s apart; REC held keeps the last one, held again
     * the two; PLAY plays it, he dances; the songs (as the Carnet app sends them) come first; SEQ held: the list, K1
     * chooses, let go: it plays; REC in the list twice: the sketch erased (a song: not) */
    press(B_PLAY); run(200);
    if (demo_on || strcmp(m1.hval, "EMPTY")) FAIL("PLAY with nothing: %s", m1.hval);   /* (the only words PLAY says) */
    run(5000);                                                              /* (the keys of before: an older phrase) */
    {
        uint32_t k2;
        for (k2 = 0; k2 < 4u; k2++) { fm1_in.notes = 1u << (8 + k2 * 2); run(220); fm1_in.notes = 0; run(120); }
        run(5000);
        for (k2 = 0; k2 < 6u; k2++) { fm1_in.notes = 1u << (12 + (k2 % 3) * 2); run(200); fm1_in.notes = 0; run(100); }
        run(800);
    }
    host_pressed = 1u << B_REC; run(800); host_released = 1u << B_REC; run(300);
    shot("12_kept");
    if (m1b.nsk != 1u || pl_count != 1u || pl_item[0].n != 6u) FAIL("REC held: %u sketches, %u pieces, %u notes", m1b.nsk, pl_count, pl_item[0].n);
    host_pressed = 1u << B_REC; run(800); host_released = 1u << B_REC; run(300);
    if (m1b.nsk != 1u || pl_item[0].n != 10u) FAIL("REC held again: %u sketches, %u notes (the two phrases: 10)", m1b.nsk, pl_item[0].n);
    press(B_PLAY); run(1000);
    shot("13_tape");
    if (!demo_on || !cat.away) FAIL("PLAY: on %u, the companion away %u", demo_on, cat.away);   /* (the deck fills the screen) */
    {
        uint32_t g = 0;
        while (demo_on && !pl_wait && g++ < 2000u) run(15);                 /* to its end, then the gap */
        if (!pl_wait) FAIL("the tape never ends its piece");
    }
    press(B_PLAY); run(300);
    if (demo_on) FAIL("PLAY again: still playing");
    {   /* two songs, as ED_SONG_BEGIN / WRITE / END leave them */
        static const uint8_t SONG[] = {0, 60, 90, 20, 50, 64, 90, 20, 50, 67, 90, 20, 50, 72, 90, 20,   /* dt note vel */
                                       50, 67, 90, 20, 50, 64, 90, 20, 50, 62, 90, 20, 50, 60, 90, 60};  /* len, x8 */
        song_hdr_t h;
        uint32_t took;
        memset(&h, 0, sizeof h);
        fl_erase4k(SONG_BASE, &took);
        h.magic = SONG_MAGIC; h.version = 1; h.count = 2; h.data_len = 2u * sizeof SONG;
        h.e[0].off = 0; h.e[0].notes = 8; memcpy(h.e[0].name, "GYMNOPEDIE", 10);
        h.e[1].off = sizeof SONG; h.e[1].notes = 8; memcpy(h.e[1].name, "NOCTURNE", 8);
        fl_write(SONG_BASE + SONG_DATA, SONG, sizeof SONG);
        fl_write(SONG_BASE + SONG_DATA + sizeof SONG, SONG, sizeof SONG);
        h.crc = st_crc32(host_xip(SONG_BASE + SONG_DATA), h.data_len);
        fl_write(SONG_BASE, &h, sizeof h);
        m1b.scan = 1; run(100);
        if (pl_count != 3u || pl_songs != 2u || strcmp(pl_item[0].name, "GYMNOPEDIE") || strcmp(pl_item[2].name, "SKETCH 1"))
            FAIL("songs: %u pieces, %u songs, %s / %s", pl_count, pl_songs, pl_item[0].name, pl_item[2].name);
        host_edn = 0;
        ed_m1_handle(ED_SONG_INFO, 0, 0);
        if (host_ed[0] != 2u || host_ed[1] != 8u) FAIL("ED_SONG_INFO: %u songs, %u notes", host_ed[0], host_ed[1]);
        host_edn = 0;
        ed_m1_handle(ED_SK_INFO, 0, 0);
        if (host_ed[0] != 1u) FAIL("ED_SK_INFO: %u sketches", host_ed[0]);
    }
    host_pressed = 1u << B_SEQ; run(200);                                   /* the list: K1 down twice: the sketch */
    turn(EN_K1, 1); turn(EN_K1, 1); run(200);
    shot("14_list");
    if (!m1.list || pl_cursor != 2u) FAIL("SEQ held: list %u cursor %u", m1.list, pl_cursor);
    turn(EN_K1, -1); run(100);                                              /* .. back up: NOCTURNE */
    host_released = 1u << B_SEQ; run(300);
    if (!demo_on || pl_idx != 1u || m1.list) FAIL("the list, let go: on %u playing %u", demo_on, pl_idx);
    run(1500);
    shot("14b_tape_song");
    cat.next_pass = fm1_ms; run(2500);                                      /* he passes in front of the reels */
    shot("14c_pass");
    if (cat.away || cat.clip != CAT_PASSE) FAIL("the tape: no pass (away %u clip %u)", cat.away, cat.clip);
    {
        uint32_t g = 0;
        while (cat.clip == CAT_PASSE && g++ < 2000u) run(15);
        run(100);
        if (!cat.away) FAIL("after his pass: not away");
    }
    pl_target = 1; demo_req = 6; run(300);                                  /* (NOCTURNE again, for OCT+ / OCT-) */
    press(B_OCTUP); run(300);
    if (pl_idx != 2u) FAIL("OCT+ while it plays: %u", pl_idx);
    if (song.octave) FAIL("OCT+ while it plays changed the octave");
    press(B_OCTDN); run(150); press(B_OCTDN); run(300);
    if (pl_idx != 1u) FAIL("OCT- twice: %u", pl_idx);
    {   /* OCT- and OCT+ held together 1 s (Modular One's gesture): the tape stops, again: it plays; no octave change */
        int8_t oc = song.octave;
        host_pressed = 1u << B_OCTDN; run(100); host_pressed = 1u << B_OCTUP; run(1200);
        host_released = 1u << B_OCTDN | 1u << B_OCTUP; run(300);
        if (demo_on || song.octave != oc) FAIL("OCT- OCT+ held: playing %u octave %d", demo_on, song.octave);
        host_pressed = 1u << B_OCTUP; run(100); host_pressed = 1u << B_OCTDN; run(1200);
        host_released = 1u << B_OCTDN | 1u << B_OCTUP; run(300);
        if (!demo_on || song.octave != oc) FAIL("OCT+ OCT- held again: playing %u octave %d", demo_on, song.octave);
    }
    press(B_PLAY); run(300);
    host_pressed = 1u << B_SEQ; run(200);
    turn(EN_K1, -3); run(100);
    press(B_REC); run(200);
    if (pl_del) FAIL("REC on a song: erase asked");
    turn(EN_K1, 1); turn(EN_K1, 1); turn(EN_K1, 1); run(100);                 /* (one piece a click: the last, the sketch) */
    press(B_REC); run(200);
    shot("15_erase");
    if (pl_del != 3u) FAIL("REC in the list: no ERASE? (%u)", pl_del);
    press(B_REC); run(300);
    host_released = 1u << B_SEQ; run(300);
    if (m1b.nsk || pl_count != 2u || demo_on) FAIL("REC twice: %u sketches, %u pieces, playing %u", m1b.nsk, pl_count, demo_on);
    run(1500);

    /* SAVE: this sound kept; back to it later: yours; SAVE held: the factory one */
    {
        int16_t atk = M1T->p[P_ATK];
        press(B_SAVE); run(300);
        shot("14_saved");
        if (!m1_saved(0) || host_up[0].p[P_ATK] != atk) FAIL("SAVE: not kept");
        turn(EN_PRESET, 1); run(200); turn(EN_PRESET, -1); run(200);
        if (M1T->p[P_ATK] != atk) FAIL("SAVE: not loaded back (%d / %d)", M1T->p[P_ATK], atk);
        host_pressed = 1u << B_SAVE; run(2300); host_released = 1u << B_SAVE; run(200);
        if (m1_saved(0) || M1T->p[P_ATK] == atk) FAIL("SAVE held: not the factory sound");
    }

    /* SEL: a cuddle (not while asleep) */
    run(3000);
    press(B_SCL); run(400);
    shot("15a_calin");
    if (!cat_sel(cat.clip) || !(host_leds & 1u << B_SCL)) FAIL("SEL: no cuddle (clip %u)", cat.clip);
    {   /* again and again: cuddles, play, hiding, a present; each one ends */
        uint32_t seen = 0, n;
        for (n = 0; n < 24u && seen != 15u; n++) {
            uint32_t g = 0;
            seen |= 1u << (cat.clip == CAT_CALIN ? 0 : cat.clip == CAT_JOUER ? 1 : cat.clip == CAT_CACHE ? 2 : cat.clip == CAT_CADEAU ? 3 : 4);
            while (cat_sel(cat.clip) && g++ < 1000u) run(15);
            if (g >= 1000u) FAIL("SEL: clip %u never ends", cat.clip);
            if (n == 3u) shot("15b_sel");
            press(B_SCL); run(300);
        }
        if ((seen & 15u) != 15u) FAIL("SEL: not all four (seen %x)", seen);
        while (cat_sel(cat.clip)) run(15);
    }

    /* GLO: K1 the line, K2 its value; the companion */
    press(B_GLO); run(400);
    shot("15_glo");
    turn(EN_K2, 1); run(400);                                               /* COMPANION: FOX */
    if (PET != &PETS[1] || M1G[M1G_PET] != 1) FAIL("GLO COMPANION: %s", PET->name);
    turn(EN_K1, 1); turn(EN_K2, 1);                                         /* MOOD: CALM */
    turn(EN_SELECT, 1); turn(EN_PRESET, 2);                                 /* MIDI CHANNEL 1 -> 2 */
    turn(EN_K1, 1); turn(EN_K1, 1); turn(EN_K2, 1);                         /* SPEAKER: LOW CUT */
    turn(EN_K1, 5); turn(EN_K2, 1); run(300);                               /* SLEEP: 5 MIN */
    if (M1G[M1G_CAT] != 1 || host_menu[MI_TCH1] != 1 || host_menu[MI_LOWCUT] != 1 || host_scr != 1 || m1.grow != G1_SCR)
        FAIL("GLO: mood %u ch %u eq %u scr %u line %u", M1G[M1G_CAT], host_menu[MI_TCH1], host_menu[MI_LOWCUT], host_scr, m1.grow);
    if (m1.pad != 0 || m1.fader[0] != 127) FAIL("GLO turned the sound or the light");
    press(B_GLO); run(3000);
    shot("16_fox");
    press(B_ARP); fm1_in.notes = 1u << 12; run(2000);
    shot("17_fox_dance");
    if (cat.clip != CAT_DANSE) FAIL("the fox does not dance");
    fm1_in.notes = 0; press(B_ARP); run(1500);

    /* the night: SELECT left */
    for (k = 0; k < 40u; k++) turn(EN_SELECT, -1);
    cat.next_life = fm1_ms;
    run(30000);
    shot("18_night");
    if (cat.clip != CAT_SLEEP) FAIL("not asleep at night (clip %u)", cat.clip);
    press(B_SCL); run(600);
    if (cat.clip != CAT_SLEEP) FAIL("SEL at night woke him (clip %u)", cat.clip);
    for (k = 0; k < 40u; k++) turn(EN_SELECT, 1);
    run(3000);
    if (cat.clip == CAT_SLEEP || cat.clip == CAT_ENDORT) FAIL("still asleep in the day");

    /* the robot and the fish dance too */
    press(B_GLO); m1.grow = G1_PET; turn(EN_K2, 1); press(B_GLO); run(2500);
    press(B_ARP); fm1_in.notes = 1u << 12; run(2000);
    shot("19_robot_dance");
    fm1_in.notes = 0; run(1500);
    press(B_GLO); m1.grow = G1_PET; turn(EN_K2, 1); press(B_GLO); run(2500);
    fm1_in.notes = 1u << 12; run(2000);
    shot("20_fish_dance");
    if (!PET->water || cat.clip != CAT_DANSE) FAIL("the fish: %s clip %u", PET->name, cat.clip);
    fm1_in.notes = 0; press(B_ARP); run(1500);
    press(B_OCTUP); run(200);
    if (song.octave != 1 || !(host_leds & 1u << B_OCTUP)) FAIL("OCT+");

    printf("m1_fm1: %s (%u failures)\n", fails ? "FAIL" : "ok", fails);
    return fails ? 1 : 0;
}
