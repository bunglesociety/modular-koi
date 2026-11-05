/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Koi on the FM-1: its controls (koi_ui.c), the thumbs and the rain (koi_play.c), the drum (eng_koi.c) and the
 * pond (koi_draw.c) on the host: a script of hands on the FM-1, the sound to OUTDIR/koi.wav, pictures to OUTDIR/NAME.ppm.
 * Exit 1 on a failure.
 *   cc -O1 -w -DMODULAR_ONE=1 -DFELUCCA_SLICE=0 -DMODULAR_KOI=1 -DKOI_HOST -Ibuild/gen -Ifirmware/src -Itests -o build/host/koi_fm1 tests/koi_fm1.c -lm
 *   build/host/koi_fm1 OUTDIR */
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
/* the LEDs, by hand (ui_input.c's helpers): one byte per id */
#define LED_BUF 64u
#define LED_KEY0 14u
static uint8_t host_led_l[64], host_led_d[64], host_led_b[64];
static void led_pos_init(void) {}
static void led_put(uint8_t *nl, uint32_t id, int on) { if (id < LED_BUF && on) nl[id] = 1; }
static int fm1_led_anim_on(void) { return 0; }
static void koi_host_leds(const uint8_t *l, const uint8_t *d, const uint8_t *b)
{
    memcpy(host_led_l, l, 64); memcpy(host_led_d, d, 64); memcpy(host_led_b, b, 64);
}
#include "../firmware/src/koi_ui.c"
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


/* ------------------------------------------------------------ the script */
static const char *dir;
static uint32_t fails;
#include "../firmware/src/m1_book.c"
#include "../firmware/src/koi_draw.c"

/* ------------------------------------------------------------ the script */
static const char *dir;
static uint32_t fails, clips, frames_w;
static FILE *wav;
#define FAIL(...) (printf("koi_fm1: FAIL " __VA_ARGS__), printf("\n"), fails++)
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
/* ms of the FM-1: the main loop every 15 ms, the audio between (mix_block: the sequencer, the keys, the drum) */
static uint32_t peak;
static void run(uint32_t ms)
{
    uint32_t end = fm1_ms + ms;
    while ((int32_t)(fm1_ms - end) < 0) {
        uint32_t b;
        m1_input();
        m1_service();
        m1_leds();
        m1_draw();
        for (b = 0; b < 15u * 44100u / 1000u / CTL + (fm1_ms % 3u == 0u); b++) {
            int32_t o[2 * CTL];
            uint32_t i;
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++) {
                int32_t l = o[2 * i] < 0 ? -o[2 * i] : o[2 * i];
                if (l > 32700)
                    clips++;
                if ((uint32_t)l > peak)
                    peak = (uint32_t)l;
                if (wav)
                    wav_put(wav, o[2 * i], o[2 * i + 1]), frames_w++;
            }
        }
        fm1_ms += 15u;
    }
}
static void press(uint32_t b) { host_pressed = 1u << b; run(60); host_released = 1u << b; run(60); }
static void hold(uint32_t b, uint32_t ms) { host_pressed = 1u << b; run(ms); host_released = 1u << b; run(30); }
static void turn(uint32_t e, int32_t s) { host_enc[e] = s; run(120); }
static void key(uint32_t k, uint32_t ms) { fm1_in.notes |= 1u << k; host_notes |= 1u << k; run(ms); fm1_in.notes &= ~(1u << k); run(20); }
static uint32_t slots_live(void)
{
    koi_part_t *pp = koi_part(&trk[0]);
    uint32_t i, n = 0;
    for (i = 0; i < KOI_SLOTS; i++)
        n += pp->s[i].kind == 1u;                                           /* (the tongues; the bowls ring long) */
    return n;
}


/* KOI_DEMO=DIR: a demo to hear and see (the firmware's own sound): a phrase on the thumbs, the rain answering it by day,
 * the night falling; DIR/koi_demo.wav and DIR/f_NNN.ppm (a picture every 120 ms, 2 .. 11 s, for a GIF) */
static int koi_demo(const char *d)
{
    static const uint8_t K[12] = {2, 4, 6, 24, 23, 21, 25, 0, 3, 26, 2, 8};
    static const uint16_t DT[12] = {420, 380, 640, 380, 380, 600, 900, 520, 480, 640, 1300, 0};
    char path[512];
    uint32_t k, f = 0, next = 0;
    dir = d;
    snprintf(path, sizeof path, "%s/koi_demo.wav", d);
    wav = fopen(path, "wb");
    wav_hdr(wav, 0);
    host_tracks_init();
    fm1_ms = 1000;
    song.master_q12 = 4096;
    m1_init();
    m1_boot();
    koi_n = 4;
    m1.k[KC_RES] = 64;                                                       /* (the demo: the drum's own length) */
    koi_world();
    run(1200);
    next = fm1_ms + 800;
    for (k = 0; k < 12u; k++) {
        fm1_in.notes |= 1u << K[k];
        host_notes |= 1u << K[k];
        run(110);
        fm1_in.notes &= ~(1u << K[k]);
        {
            uint32_t end = fm1_ms + DT[k];
            while ((int32_t)(fm1_ms - end) < 0) {
                run(15);
                if ((int32_t)(fm1_ms - next) >= 0 && f < 75u) {
                    char n[16];
                    snprintf(n, sizeof n, "f_%03u", f++);
                    shot(n);
                    next += 120u;
                }
            }
        }
    }
    host_pressed = 1u << B_ARP; run(60); host_released = 1u << B_ARP; run(60);
    while (f < 75u) {
        run(15);
        if ((int32_t)(fm1_ms - next) >= 0) {
            char n[16];
            snprintf(n, sizeof n, "f_%03u", f++);
            shot(n);
            next += 120u;
        }
    }
    run(36000);
    for (k = 0; k < 30u; k++) {                                               /* the night falls, slowly */
        host_enc[EN_SELECT] = -1;
        run(400);
    }
    run(30000);
    shot("night");
    fclose(wav);
    {
        FILE *fw = fopen(path, "r+b");
        wav_hdr(fw, frames_w);
        fclose(fw);
    }
    printf("koi_demo: %s, %u s, peak %u, clipped %u\n", path, frames_w / 44100u, peak, clips);
    return 0;
}

/* KOI_GESTES=DIR: the opening's pling, a phrase, ROULE held then tapped, PAUME laid on it, the hand resting (muffled
 * strikes), lifted again: DIR/koi_gestes.wav */
static void kk(uint32_t k, uint32_t hold, uint32_t after)
{
    fm1_in.notes |= 1u << k; host_notes |= 1u << k; run(hold); fm1_in.notes &= ~(1u << k); run(after);
}
static int koi_gestes(const char *d)
{
    static const uint8_t PH[6] = {2, 4, 6, 24, 23, 21};
    char path[512];
    uint32_t k;
    dir = d;
    snprintf(path, sizeof path, "%s/koi_gestes.wav", d);
    wav = fopen(path, "wb");
    wav_hdr(wav, 0);
    host_tracks_init();
    fm1_ms = 1000;
    song.master_q12 = 4096;
    m1_init();
    m1_boot();
    run(1000); shot("g_opening_1s");
    run(1100); shot("g_opening_2s");
    run(2500);                                                               /* the pling */
    for (k = 0; k < 6u; k++) kk(PH[k], 100, 380);
    run(2500);
    kk(6, 100, 200);                                                         /* sol: fast to slow (OP3), tapped */
    kk(5, 100, 2600);
    kk(24, 100, 200);                                                        /* re: slow to fast (MONO), tapped */
    kk(20, 100, 2600);
    kk(25, 100, 200);                                                        /* do: slow to fast held, a tremolo */
    kk(20, 3000, 1500);
    kk(4, 100, 200);                                                         /* mi: fast to slow held */
    kk(5, 3500, 1500);
    for (k = 0; k < 40u; k++) {                                              /* fast, every way */
        static const uint8_t RUN[11] = {0, 26, 2, 24, 4, 23, 6, 21, 1, 25, 3};
        kk(RUN[k % 11u], 45, 45);
    }
    run(2000);
    for (k = 0; k < 6u; k++) kk(PH[k], 100, 380);
    run(5000);
    fclose(wav);
    {
        FILE *fw = fopen(path, "r+b");
        wav_hdr(fw, frames_w);
        fclose(fw);
    }
    printf("koi_gestes: %s, %u s, peak %u\n", path, frames_w / 44100u, peak);
    return 0;
}

/* KOI_GIFS=DIR: the pictures of the web pages, each a run of frames every 120 ms (DIR/SCENE_NNN.ppm), and a few stills */
static uint32_t gif_n;
static void gif_run(const char *scene, uint32_t ms)
{
    uint32_t end = fm1_ms + ms, next = fm1_ms;
    while ((int32_t)(fm1_ms - end) < 0) {
        run(15);
        if ((int32_t)(fm1_ms - next) >= 0) {
            char n[48];
            snprintf(n, sizeof n, "%s_%03u", scene, gif_n++);
            shot(n);
            next += 120u;
        }
    }
}
static void gk(uint32_t k, uint32_t hold) { fm1_in.notes |= 1u << k; host_notes |= 1u << k; run(hold); fm1_in.notes &= ~(1u << k); run(15); }
static int koi_gifs(const char *d)
{
    static const uint8_t PH[8] = {2, 4, 6, 24, 23, 21, 25, 0};
    uint32_t k;
    dir = d;
    host_tracks_init();
    fm1_ms = 1000;
    song.master_q12 = 4096;
    m1_init();
    m1_boot();
    koi_n = 4;
    gif_n = 0; gif_run("demarrage", 3800);                                   /* the opening */
    run(400);
    gif_n = 0;                                                               /* the pond, a phrase */
    {
        uint32_t end = fm1_ms + 6000, next = fm1_ms, i = 0, at = fm1_ms + 200;
        while ((int32_t)(fm1_ms - end) < 0) {
            if ((int32_t)(fm1_ms - at) >= 0 && i < 8u) { fm1_in.notes |= 1u << PH[i]; host_notes |= 1u << PH[i]; }
            run(15);
            if ((int32_t)(fm1_ms - at - 90u) >= 0 && i < 8u) { fm1_in.notes &= ~(1u << PH[i]); i++; at += 520u + (i % 3u) * 160u; }
            if ((int32_t)(fm1_ms - next) >= 0) { char n[48]; snprintf(n, sizeof n, "bassin_%03u", gif_n++); shot(n); next += 120u; }
        }
    }
    run(3000);
    gk(6, 80); run(150);                                                     /* a roll, fast to slow */
    gif_n = 0;
    fm1_in.notes |= 1u << 5; host_notes |= 1u << 5;
    gif_run("roulement", 300);
    fm1_in.notes &= ~(1u << 5);
    gif_run("roulement", 3300);
    run(2000);
    host_pressed = 1u << B_ARP; run(60); host_released = 1u << B_ARP; run(60);   /* the rain, by day */
    run(1400);
    gif_n = 0; gif_run("pluie", 7200);
    for (k = 0; k < 30u; k++) { host_enc[EN_SELECT] = -1; run(60); }        /* the night */
    run(2000);
    gif_n = 0; gif_run("nuit", 7200);
    host_pressed = 1u << B_ARP; run(60); host_released = 1u << B_ARP; run(60);
    for (k = 0; k < 30u; k++) { host_enc[EN_SELECT] = 1; run(60); }
    run(6000);
    host_pressed = 1u << B_SCL; run(60); host_released = 1u << B_SCL; run(60);   /* dinner */
    gif_n = 0; gif_run("repas", 4800);
    run(4000);
    for (k = 0; k < 7u; k++) gk(PH[k], 110), run(260);                        /* a sketch, then the tape */
    run(500);
    host_pressed = 1u << B_REC; run(800); host_released = 1u << B_REC; run(100);
    for (k = 0; k < 300u && !pl_count; k++) run(15);
    host_pressed = 1u << B_PLAY; run(60); host_released = 1u << B_PLAY; run(60);
    run(900);
    gif_n = 0; gif_run("magneto", 4800);
    host_pressed = 1u << B_SEQ; run(400);
    shot("liste");
    host_released = 1u << B_SEQ; run(100);
    host_pressed = 1u << B_PLAY; run(60); host_released = 1u << B_PLAY; run(500);
    host_pressed = 1u << B_GLO; run(60); host_released = 1u << B_GLO; run(200);
    shot("reglages");
    host_pressed = 1u << B_GLO; run(60); host_released = 1u << B_GLO; run(200);
    printf("koi_gifs: into %s\n", d);
    return 0;
}

int main(int argc, char **argv)
{
    static const uint8_t PHR[10] = {2, 4, 6, 26, 24, 23, 21, 25, 0, 3};     /* left, right thumbs: a phrase */
    uint32_t k, w0, n0;
    char path[512];
    memset(host_fl, 0xFF, sizeof host_fl);
    if (getenv("KOI_GIFS"))
        return koi_gifs(getenv("KOI_GIFS"));
    if (getenv("KOI_GESTES"))
        return koi_gestes(getenv("KOI_GESTES"));
    if (getenv("KOI_DEMO"))
        return koi_demo(getenv("KOI_DEMO"));
    dir = argc > 1 ? argv[1] : "build/koi";
    host_tracks_init();
    fm1_ms = 1000;
    snprintf(path, sizeof path, "%s/koi.wav", dir);
    wav = fopen(path, "wb");
    wav_hdr(wav, 0);
    song.master_q12 = 4096;
    m1_init();
    m1_boot();
    run(300);
    shot("00_opening");
    if (trk[0].eng_req != 13u)
        FAIL("the drum is not engine 13 (%u)", trk[0].eng_req);
    if (!host_led_l[LED_KEY0 + 5u] || !host_led_l[LED_KEY0 + 20u])
        FAIL("the rolls' keys (OP3, MONO) not lit");
    if (koi_res != 0 || koi_bounce != 0 || koi_strike != 127 || koi_tongue[0] != 55)
        FAIL("power-on: RESONANCE %u BOUNCE %u STRIKE %u, the low tongue %u", koi_res, koi_bounce, koi_strike, koi_tongue[0]);
    run(3600);
    /* 1. the thumbs: every tongue key gives its note of the scale */
    {
        static const uint8_t WANT[27] = {55, 59, 60, 71, 64, 0, 67, 60, 60, 62, 62, 64, 65, 64, 67, 67, 69, 69, 71, 72,
                                         0, 69, 0, 65, 62, 72, 57};
        for (k = 0; k < 27u; k++) {
            if (!WANT[k])
                continue;
            w0 = koi_ev_w;
            key(k, 80);
            {
                uint32_t e, got = 0;
                for (e = w0; e != koi_ev_w; e++)                             /* (a rebound may come in between) */
                    got |= koi_ev[e % KOI_NEV].note == WANT[k];
                if (!got)
                    FAIL("key %u: no note %u", k, WANT[k]);
            }
            run(220);
        }
    }
    shot("01_drops");
    run(4000);
    /* 2. a phrase; the drum rings on after the keys are let go; the edges answer (K3) */
    for (k = 0; k < 10u; k++) {
        key(PHR[k], 140);
        run(300 + (k % 3u) * 120u);
    }
    shot("02_phrase");
    n0 = slots_live();
    if (!n0)
        FAIL("nothing rings after the phrase");
    run(11000);
    if (slots_live())
    {
        koi_part_t *pp = koi_part(&trk[0]);
        uint32_t i, j;
        FAIL("%u tongues still ringing 11 s after (the drum should be quiet)", slots_live());
        for (i = 0; i < KOI_SLOTS; i++)
            if (pp->s[i].kind) {
                printf("  slot %u note %u live %x tock %d:", i, pp->s[i].note, pp->s[i].live, pp->s[i].tock);
                for (j = 0; j < 8u; j++)
                    printf(" %d/%d", pp->s[i].amp[j] >> 15, pp->s[i].sw[j]);
                printf("  carrier %u active %u\n", pp->carrier, pp->carrier < 8u ? trk[0].v[pp->carrier].active : 9u);
            }
    }
    /* 3. the rolls: OP3 fast to slow, MONO slow to fast (a tap: the whole gesture, then it stops) */
    {
        uint32_t g, e, gaps[16], ng;
        for (g = 0; g < 2u; g++) {
            key(6, 60);
            run(300);
            w0 = koi_ev_w;
            key(g ? 20 : 5, 80);
            run(3500);
            ng = 0;
            for (e = w0 + 1u; e != koi_ev_w && ng < 16u; e++)
                if (koi_ev[e % KOI_NEV].note == 67 && koi_ev[(e - 1u) % KOI_NEV].note == 67)
                    gaps[ng++] = koi_ev[e % KOI_NEV].ms - koi_ev[(e - 1u) % KOI_NEV].ms;
            if (koi_ev_w - w0 < 6u || koi_ev_w - w0 > 16u)
                FAIL("roll %u: %u taps", g, koi_ev_w - w0);
            else if (ng >= 4u && (g ? gaps[ng - 1u] >= gaps[0] : gaps[ng - 1u] <= gaps[0]))
                FAIL("roll %u: the taps do not %s (%u ms .. %u ms)", g, g ? "quicken" : "slow down", gaps[0], gaps[ng - 1u]);
            w0 = koi_ev_w;
            run(1500);
            if (koi_ev_w - w0 > 2u)
                FAIL("roll %u tapped: it goes on (%u taps more)", g, koi_ev_w - w0);
        }
        w0 = koi_ev_w;                                                       /* held: on at its last pace */
        fm1_in.notes |= 1u << 20; host_notes |= 1u << 20;
        run(3000);
        fm1_in.notes &= ~(1u << 20);
        run(600);
        if (koi_ev_w - w0 < 18u)
            FAIL("MONO held 3 s: %u taps (a tremolo wanted at the end)", koi_ev_w - w0);
        if (koi_roll_gap < KOI_SR(100))
            FAIL("the roll's fastest: %u samples (fingers: 100 ms at most)", koi_roll_gap);
    }
    run(4000);
    /* 4. fast playing: no click (a step between two samples, far over what the tones make) */
    {
        uint32_t f0 = frames_w;
        long pos;
        (void)f0;
        fflush(wav);
        pos = ftell(wav);
        for (k = 0; k < 60u; k++) {
            static const uint8_t RUN[11] = {0, 26, 2, 24, 4, 23, 6, 21, 1, 25, 3};
            key(k < 30u ? RUN[k % 11u] : k < 45u ? 2u : 26u, 30 + (k % 3u) * 15u);   /* (then one key, fast: do, then la) */
            run(30 + (k % 4u) * 10u);
        }
        run(1000);
        fflush(wav);
        {
            FILE *f = fopen(path, "rb");
            long n = (ftell(wav) - pos) / 4, i;
            int16_t *x = malloc((size_t)n * 4u);
            double d2 = 0, pk = 1;
            fseek(f, pos, SEEK_SET);
            if (fread(x, 4, (size_t)n, f) != (size_t)n) n = 0;
            fclose(f);
            for (i = 2; i < n; i++) {
                double a = x[2 * i], b = x[2 * i - 2], c = x[2 * i - 4], q = fabs(a - 2 * b + c);
                if (fabs(a) > pk) pk = fabs(a);
                if (q > d2) d2 = q;
            }
            free(x);
            printf("koi_fm1: fast run, peak %.0f, largest second difference %.0f (%.3f of the peak)\n", pk, d2, d2 / pk);
            if (d2 / pk > 0.25)
                FAIL("fast run: a click (second difference %.0f of a peak %.0f)", d2, pk);
        }
    }
    /* 5. HARMO tapped: harmonics (odd velocities); ROULE held: a roll */
    key(22, 100);
    w0 = koi_ev_w;
    key(6, 80);
    if (!(koi_ev[(koi_ev_w - 1u) % KOI_NEV].vel & 1u))
        FAIL("HARMO latched: no harmonic");
    key(22, 100);
    run(1500);
    key(4, 60);
    key(5, 2000);
    shot("03_roll");
    run(5000);
    /* 6. the knobs, the scale, the material, the night */
    turn(EN_K1, -3);
    if (koi_strike != 121) FAIL("K1: STRIKE %u", koi_strike);  /* (a fast turn: x 2) */
    shot("04_knob");
    turn(EN_PRESET, 1);
    if (koi_tongue[2] != 60) FAIL("PRESETS: PENTATONIC's third tongue %u", koi_tongue[2]);
    shot("05_scale");
    turn(EN_PRESET, -1);
    turn(EN_ALGO, 1); turn(EN_ALGO, 1); turn(EN_ALGO, 1);
    if (koi_mat != 3) FAIL("ALGORITHM: WOOD %u", koi_mat);
    for (k = 0; k < 5u; k++) { key(PHR[k], 100); run(200); }
    turn(EN_ALGO, -1); turn(EN_ALGO, -1); turn(EN_ALGO, -1);
    run(1500);
    for (k = 0; k < 8u; k++) turn(EN_SELECT, -4);
    if (koi_night < 120) FAIL("SELECT left: the night %u", koi_night);
    run(2600);
    /* 7. the rain at night, listening */
    press(B_ARP);
    if (!koi_rain_on) FAIL("ARP: no rain");
    w0 = koi_ev_w;
    run(20000);
    if (koi_ev_w - w0 < 8u) FAIL("the rain: %u drops in 20 s", koi_ev_w - w0);
    shot("06_night_rain");
    for (k = 0; k < 4u; k++) { key(PHR[k], 120); run(250); }               /* you play: it listens, then answers */
    w0 = koi_ev_w;
    run(2000);
    if (koi_ev_w - w0 > 3u) FAIL("the rain does not wait while you play (%u drops)", koi_ev_w - w0);
    run(14000);
    press(B_ARP);
    for (k = 0; k < 8u; k++) turn(EN_SELECT, 4);
    /* 8. the koi: 5 of them, SEL feeds them; winter */
    press(B_GLO);
    run(100);
    shot("07_settings");
    turn(EN_K2, 1); turn(EN_K2, 1);                                          /* KEY: C -> D, every tongue up 2 */
    if (koi_tongue[0] != 57u || koi_tongue[3] != 62u) FAIL("GLO KEY D: tongues %u %u", koi_tongue[0], koi_tongue[3]);
    {
        char nm[20];
        if (strcmp(koi_scale_name(nm, sizeof nm, 0, 0), "D MAJOR")) FAIL("GLO KEY D: the scale is \"%s\"", nm);
    }
    shot("07_settings_key");
    turn(EN_K2, -1); turn(EN_K2, -1);                                        /* (back to C) */
    if (koi_tongue[3] != 60u) FAIL("GLO KEY C: %u", koi_tongue[3]);
    turn(EN_K1, 1);                                                          /* the KOI line */
    turn(EN_K2, 1); turn(EN_K2, 1);                                          /* (KOI 3 -> 5) */
    if (koi_n != 5) FAIL("GLO KOI: %u", koi_n);
    turn(EN_K1, 1);
    turn(EN_K1, 1);
    turn(EN_K2, 1);
    if (!koi_winter) FAIL("GLO WINTER: off");
    press(B_GLO);
    press(B_SCL);
    run(2500);
    shot("08_feeding_winter");
    press(B_GLO); turn(EN_K2, -1);                                           /* (winter off again) */
    turn(EN_K1, -1); turn(EN_K2, -1);                                        /* RIPPLES OFF */
    if (koi_ripple) FAIL("GLO RIPPLES: still on");
    press(B_GLO);
    for (k = 0; k < 4u; k++) { key(PHR[k], 100); run(150); }
    {
        uint32_t i, rings = 0;
        for (i = 0; i < PG * PG; i++)
            rings += wave[i] > 14;
        if (rings > 40u) FAIL("RIPPLES OFF: %u cells of rings", rings);
    }
    shot("10_no_ripples");
    press(B_GLO); turn(EN_K2, 1); press(B_GLO);                              /* (on again) */
    if (!koi_ripple) FAIL("GLO RIPPLES: not on again");
    /* 9. the sketchbook: REC held keeps the last phrase; PLAY plays it (the deck) */
    run(5000);
    for (k = 0; k < 6u; k++) { key(PHR[k], 120); run(220); }
    run(500);
    hold(B_REC, 800);
    for (k = 0; k < 400u && !pl_count; k++) run(15);
    if (!pl_count) FAIL("REC held: no sketch kept");
    press(B_PLAY);
    run(1500);
    if (!demo_on) FAIL("PLAY: the tape does not play");
    shot("09_tape");
    host_pressed = 1u << B_SEQ; run(300);                                    /* SEQ held: the list over the deck */
    shot("09b_list");
    host_pressed = 1u << B_REC; run(60); host_released = 1u << B_REC; run(200);
    if (pl_del != pl_cursor + 1u) FAIL("SEQ held, REC: no 'erase?' (%u)", pl_del);
    shot("09c_erase");
    host_released = 1u << B_SEQ; run(200);
    if (pl_del) FAIL("SEQ let go: 'erase?' stays");
    run(3000);
    press(B_PLAY);
    /* 10. SAVE */
    turn(EN_K4, 5);
    press(B_SAVE);
    if (!koi_saved()) FAIL("SAVE: not saved");
    run(6000);
    fclose(wav);
    {   /* the WAV's header, now that its length is known */
        FILE *f = fopen(path, "r+b");
        wav_hdr(f, frames_w);
        fclose(f);
    }
    printf("koi_fm1: %s, %u s, peak %u, clipped %u, %u failures\n", path, frames_w / 44100u, peak, clips, fails);
    return fails ? 1 : 0;
}
