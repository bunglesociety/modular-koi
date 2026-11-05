/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Koi: the KOI engine alone on the host (tongue3.py's phrase, hand on then free; a bowl; a re-strike), to
 * OUTDIR/koi_engine.wav, and its cost. cc -O1 -w -DMODULAR_ONE=1 -DFELUCCA_SLICE=0 -DMODULAR_KOI=1 ... */
#define main hostsim_main
#include "hostsim.c"
#undef main
static FILE *w;
static uint32_t fr, clipn;
static void run_s(double sec)
{
    uint32_t end = fr + (uint32_t)(sec * FS);
    for (; fr < end; fr += CTL) {
        int32_t o[2 * CTL];
        uint32_t i;
        koi_ms_now = fr / 44u;
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            if (o[2 * i] > 32700 || o[2 * i] < -32700)
                clipn++;
            wav_put(w, o[2 * i], o[2 * i + 1]);
        }
    }
}
int main(int argc, char **argv)
{
    static const double AT[12] = {0, 0.6, 1.2, 1.8, 2.7, 3.3, 4.2, 4.8, 5.7, 6.3, 7.3, 7.3};
    static const uint8_t NT[12] = {60, 64, 67, 72, 71, 67, 69, 64, 62, 60, 55, 67};
    static const uint8_t VL[12] = {102, 76, 88, 70, 64, 76, 82, 64, 70, 96, 88, 44};
    char path[512];
    uint32_t pass, k, maxv = 0;
    track_t *t = &trk[0];
    host_tracks_init();
    host_preset(t, ENGI_VILLE, 0);   /* (13: KOI in this build) */
    t->p[P_VOICE] = V_POLY;
    t->p[P_DLY] = 60; t->p[P_REV] = 0;
    song.g[G_DTIME] = 1; song.g[G_DFDBK] = 30;
    for (k = 1; k < NTRK; k++) trk[k].p[P_LEVEL] = 0;
    snprintf(path, sizeof path, "%s/koi_engine.wav", argc > 1 ? argv[1] : ".");
    w = fopen(path, "wb");
    wav_hdr(w, (uint32_t)(44.0 * FS) / CTL * CTL);
    song.master_q12 = 4096;
    run_s(0.3);
    for (pass = 0; pass < 2; pass++) {               /* 1: the hand on the drum; 2: free */
        double t0 = 0;
        koi_hand = pass == 0;
        for (k = 0; k < 12; k++) {
            run_s(AT[k] - t0);
            t0 = AT[k];
            trk_note_on(t, NT[k], VL[k] & ~1u);
            trk_note_off(t, NT[k]);
        }
        run_s(12.5 - t0);
        for (k = 0; k < NVOICE; k++) if (t->v[k].active) maxv++;
    }
    koi_hand = 0;
    trk_note_on(t, 100, 100); trk_note_off(t, 100);          /* a bowl, C4 */
    run_s(1.5);
    trk_note_on(t, 64, 97); trk_note_off(t, 64);             /* a harmonic */
    run_s(1.0);
    for (k = 0; k < 10; k++) { trk_note_on(t, 67, 60 + (k & 1) * 10); trk_note_off(t, 67); run_s(0.08); }   /* a roll */
    run_s(44.0 - fr / (double)FS - 0.01);
    fclose(w);
    printf("koi_engine: %s, clipped samples %u, voices left %u\n", path, clipn, maxv);
    return 0;
}
