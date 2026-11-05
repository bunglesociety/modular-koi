/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Compagnon: the screen alone, filmed (the firmware's own drawing): the companions' lives, GLOBE, controls
 * turned. Scenes to OUT/<n>_<name>.rgb (240 x 240 RGB24, 30 a second); tools/make_film.sh makes the video.
 *   cc -O1 -w -DMODULAR_ONE=1 -DFELUCCA_SLICE=0 -Ibuild/gen -Ifirmware/src -Itests -o build/host/m1_film tests/m1_film.c -lm */
#define M1_VIDEO_NO_MAIN
#include "m1_video.c"

static void act(uint32_t clip, uint32_t ms) { cat_play(clip); cat.quiet_since = fm1_ms; vrun(ms); }
static void night(uint32_t sleep_ms)                          /* dusk, asleep, and the day again */
{
    fader(0, 127, 0, 2400);
    cat.next_life = fm1_ms;
    vrun(sleep_ms);
    fader(0, 0, 127, 1800);
    vrun(2200);
}
static void next_pet(void)                                     /* GLOBE, K8: the next companion */
{
    press(10); vrun(700);
    turn(7, 1, 1, 1100);
    press(10);
}

int main(int argc, char **argv)
{
    vroot = argc > 1 ? argv[1] : "build/film";
    mkdir(vroot, 0755);
    host_tracks_init();
    fm1_ms = 1000;
    no_shots = 1;
    m1_init();

    scene("chat");
    vrun(8000);                                                    /* the opening, he wakes, sits */
    notes(1u << 4 | 1u << 8 | 1u << 11, 3000);                    /* he listens */
    host_pad = 1; vrun(1500);                                     /* JUNO */
    turn(1, -3, 7, 150); vrun(900);                               /* CUTOFF */
    act(CAT_CELLO, 9000);                                         /* the leg-up wash */
    act(CAT_GRIFFES, 9500);
    act(CAT_FOLIE, 6500);
    notes(1u << 6 | 1u << 10, 2100);
    night(9000);
    fader(1, 0, 120, 1800); vrun(4500); fader(1, 120, 0, 1200);   /* snow */

    scene("globe");
    press(10); vrun(1000);
    turn(1, 1, 1, 700); turn(1, -1, 1, 700);
    turn(2, 1, 1, 700); turn(2, -1, 1, 700);
    turn(4, 1, 1, 700); turn(4, -1, 1, 700);
    turn(6, 1, 1, 700); turn(6, -1, 1, 700);
    turn(7, 1, 1, 1300);
    press(10);

    scene("renard");
    vrun(4000);
    act(CAT_CHASSE, 7000);
    notes(1u << 4 | 1u << 8, 2400);
    act(CAT_CELLO, 5000);
    act(CAT_GRIFFES, 7500);
    act(CAT_QUEUE, 4500);
    night(8000);

    scene("robot");
    next_pet(); vrun(2500);
    act(CAT_QUEUE, 3500);
    act(CAT_CELLO, 4500);
    act(CAT_GRIFFES, 4200);
    notes(1u << 4 | 1u << 8 | 1u << 11, 3000);
    act(CAT_MLEM, 4000);
    act(CAT_FOLIE, 4500);
    night(9000);

    scene("poisson");
    next_pet(); vrun(5000);
    act(CAT_MLEM, 4500);
    act(CAT_CHASSE, 3500);
    act(CAT_ROULADE, 3000);
    notes(1u << 4 | 1u << 8, 2400);
    act(CAT_FOLIE, 3600);
    act(CAT_TERRITOIRE, 4300);
    night(7000);

    scene("retour");
    press(10); vrun(500); turn(7, -1, 3, 350); vrun(500); press(10);
    vrun(4000);
    notes(1u << 4 | 1u << 8 | 1u << 11, 2400);
    vrun(2500);
    fclose(vf); fclose(vt);
    printf("m1_film: %u scenes in %s\n", vscene, vroot);
    return 0;
}
