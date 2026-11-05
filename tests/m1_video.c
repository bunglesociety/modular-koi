/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Compagnon: the presentation video's screen, drawn by the firmware's own screen code (tests/m1_screen.c's
 * harness) under a script of hands. Each scene to OUT/<n>_<name>.rgb (240 x 240 RGB24 frames, 30 a second) and
 * OUT/<n>_<name>.txt (what tools/make_video.py writes beside it: "frame cap TEXT|SUB", "frame hl CONTROL").
 *   cc -O1 -w -DMODULAR_ONE=1 -DFELUCCA_SLICE=0 -Ibuild/gen -Ifirmware/src -Itests -o build/host/m1_video tests/m1_video.c -lm
 *   build/host/m1_video OUT */
#define M1_SCREEN_NO_MAIN
#include "m1_screen.c"
#include <sys/stat.h>

static FILE *vf, *vt;
static uint32_t vframes;
static double vnext;
static const char *vroot;
static uint32_t vscene;

static void vframe(void)
{
    uint32_t i;
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t v = (uint16_t)((host_screen[i] >> 8) | (host_screen[i] << 8));
        uint8_t rgb[3] = {(uint8_t)((v >> 11) << 3 | (v >> 13)), (uint8_t)(((v >> 5) & 63u) << 2 | ((v >> 9) & 3u)),
                          (uint8_t)((v & 31u) << 3 | ((v >> 2) & 7u))};
        fwrite(rgb, 1, 3, vf);
    }
    vframes++;
}
static void scene(const char *name)
{
    char p[512];
    if (vf) { fclose(vf); fclose(vt); }
    snprintf(p, sizeof p, "%s/%02u_%s.rgb", vroot, ++vscene, name);
    vf = fopen(p, "wb");
    snprintf(p, sizeof p, "%s/%02u_%s.txt", vroot, vscene, name);
    vt = fopen(p, "w");
    vframes = 0;
    vnext = fm1_ms;
}
static void vcap(const char *t, const char *sub) { fprintf(vt, "%u cap %s|%s\n", vframes, t, sub ? sub : ""); }
static void vhl(const char *c) { fprintf(vt, "%u hl %s\n", vframes, c); }

/* run ms of simulated time (the UI every 15 ms, as the firmware), a picture every 1/30 s */
static void vrun(uint32_t ms)
{
    uint32_t end = fm1_ms + ms;
    while ((int32_t)(fm1_ms - end) < 0) {
        fm1_in.buttons = host_btn;
        m1_input();
        m1_draw();
        {
            uint32_t b;
            for (b = 0; b < 15u * 44100u / 1000u / CTL; b++)
                events_block(CTL);
        }
        fm1_ms += 15u;
        while (vf && fm1_ms >= vnext) {
            vframe();
            vnext += 1000.0 / 30.0;
        }
    }
}
static void notes(uint32_t mask, uint32_t ms)              /* keys played (the companion listens) */
{
    uint32_t t, i;
    for (t = 0; t < ms; t += 300u) {
        host_notes |= mask;
        for (i = 0; i < 27u; i++)
            if ((mask >> i) & 1u)
                fm1_in.key_vel[SMK_KEY_F3 + i] = 90;
        vrun(300);
    }
}
static void turn(uint32_t enc, int32_t s, uint32_t times, uint32_t gap)
{
    uint32_t k;
    for (k = 0; k < times; k++) { host_enc[enc] = s; vrun(gap); }
}
static void press(uint32_t b) { host_pressed = 1u << b; vrun(30); host_released = 1u << b; vrun(30); }
static void fader(uint32_t f, uint32_t from, uint32_t to, uint32_t ms)
{
    uint32_t k, n = ms / 45u;
    for (k = 0; k <= n; k++) { fm1_in.fader[f] = (uint8_t)(from + ((int32_t)to - (int32_t)from) * (int32_t)k / (int32_t)n); vrun(45); }
}
static void globe_pet(uint32_t steps)                        /* GLOBE open, K8 to the next companion, closed */
{
    press(10); vrun(500);
    turn(7, 1, steps, 400);
    vrun(700);
    press(10);
}

#ifndef M1_VIDEO_NO_MAIN
int main(int argc, char **argv)
{
    vroot = argc > 1 ? argv[1] : "build/video";
    mkdir(vroot, 0755);
    host_tracks_init();
    fm1_ms = 1000;
    no_shots = 1;
    m1_init();

    scene("ouverture");                                                     /* the opening */
    vcap("", "");
    vrun(1200);
    vcap("MODULAR COMPAGNON", "UN FIRMWARE GRATUIT POUR LE M-VAVE SMK-37");
    vrun(4300);
    vcap("UN PETIT COMPAGNON", "VIT DANS L'ECRAN DU CLAVIER");
    vrun(2500);

    scene("sons");                                                          /* sounds and controls */
    vcap("SEIZE SONS", "DE MODULAR TOWN, UN PAR PAD"); vhl("pad1");
    host_pad = 1; vrun(1200);
    notes(1u << 4 | 1u << 8 | 1u << 11, 2400);
    vhl("pad5"); host_pad = 5; vrun(1200);
    notes(1u << 6 | 1u << 10, 1800);
    vcap("UNE COMMANDE, UN ROLE", "LE REGLAGE S'ECRIT EN GRAND"); vhl("k2");
    turn(1, -3, 6, 160);
    vrun(600);
    vhl("k3"); turn(2, 4, 5, 160);
    vrun(1200);

    scene("arp");                                                           /* the arpeggio */
    vcap("L'ARPEGE", "MAINTIENS ARP : K2 LE MODE, K3 LA VITESSE"); vhl("arp");
    host_pad = 0; vrun(400);
    host_btn = 1u << 6; host_pressed = host_btn; vrun(500);
    vhl("k2"); turn(1, 1, 4, 650);
    vhl("k3"); turn(2, -1, 2, 650);
    host_released = host_btn; host_btn = 0; vrun(400);
    vcap("IL SUIT TON DAW", "ET LUI ENVOIE SES NOTES"); vhl("");
    notes(1u << 4 | 1u << 8 | 1u << 11, 3000);

    scene("nuit");                                                          /* night */
    vcap("LE FADER 1", "LE JOUR, LA NUIT"); vhl("f1");
    fader(0, 127, 0, 2200);
    vcap("LA NUIT, IL S'ENDORT", "MEME SI TU JOUES");
    cat.next_life = fm1_ms;
    vrun(7000);
    notes(1u << 6, 1500);
    vcap("IL DORT JUSQU'AU JOUR", ""); vhl("f1");
    fader(0, 0, 127, 1800);
    vrun(1800);

    scene("hiver");                                                         /* winter */
    vcap("LE FADER 2", "L'HIVER FM : LA NEIGE TOMBE"); vhl("f2");
    fader(1, 0, 120, 1800);
    vhl("");
    vrun(3500);
    fader(1, 120, 0, 900);

    scene("globe");                                                         /* the settings */
    vcap("GLOBE", "LES REGLAGES, UN PAR POTARD"); vhl("globe");
    press(10); vrun(900);
    vhl("k5"); turn(4, 1, 1, 700); turn(4, -1, 1, 700);
    vhl("k7"); turn(6, 1, 1, 700); turn(6, -1, 1, 700);
    vcap("K8 : LE COMPAGNON", "LE CHAT, LE RENARD, LE ROBOT, LE POISSON"); vhl("k8");
    turn(7, 1, 1, 1200);
    press(10); vhl("");

    scene("renard");                                                        /* the fox */
    vcap("LE RENARD", "IL SE REVEILLE");
    vrun(1800);
    vcap("UNE SOURIS DANS L'HERBE", "");
    cat_play(CAT_CHASSE); cat.quiet_since = fm1_ms; vrun(6200);
    vcap("IL ECOUTE QUAND TU JOUES", "");
    notes(1u << 4 | 1u << 8, 2400);

    scene("robot");                                                         /* the robot */
    vcap("LE ROBOT", "UN HABITANT DE MODULAR TOWN"); vhl("k8");
    globe_pet(1); vhl("");
    vrun(1500);
    cat_play(CAT_QUEUE); cat.quiet_since = fm1_ms; vrun(3000);
    vcap("IL DANSE QUAND TU JOUES", "");
    notes(1u << 4 | 1u << 8 | 1u << 11, 2700);

    scene("poisson");                                                       /* the fish */
    vcap("LE POISSON", "TOUT L'ECRAN DEVIENT SON AQUARIUM"); vhl("k8");
    globe_pet(1); vhl("");
    vrun(3200);
    vcap("UN BISOU CONTRE LA VITRE", "");
    cat_play(CAT_TERRITOIRE); cat.quiet_since = fm1_ms; vrun(4200);

    scene("fin");                                                           /* back to the cat, the end */
    press(10); vrun(400); turn(7, -1, 3, 300); press(10);
    vcap("MODULAR COMPAGNON", "GRATUIT ET LIBRE · BUNGLESOCIETY.COM");
    vrun(2000);
    notes(1u << 4 | 1u << 8 | 1u << 11, 2400);
    vrun(1800);
    fclose(vf); fclose(vt);
    printf("m1_video: %u scenes in %s\n", vscene, vroot);
    return 0;
}
#endif
