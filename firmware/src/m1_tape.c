/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Compagnon on the FM-1: the tape (from the FM-1's Modular One 0.2, seq.c), on part 1, in the audio context.
 * PLAYER: the songs sent from the computer (the Carnet app), then the sketches kept (m1_book.c, flash), one after the
 *   other, then from the first again; demo_req from the UI (m1_ui_fm1.c): 1 play, 2 stop, 3 next, 4 restart,
 *   5 previous, 6 pl_target.
 * SKETCHBOOK: every note played (keys and MIDI in) goes into a ring in RAM; REC held keeps the last phrase.
 * Piece format: per note dt (from the previous start, 10 ms), note, vel, len (20 ms); dt / len: 1 byte < 128,
 * else 2 (high bit first). Included by seq.c (MODULAR_ONE on the FM-1). */
#define DEMO_HELD 32u
#define PL_MAX 40u                               /* (20 songs, 20 sectors of sketches) */
#define PL_NAME 16u
#define PL_MAXLEN (10u * 44100u)                 /* no note longer than 10 s */
#define PL_GAP (44100u * 3u / 2u)                /* 1.5 s of silence between two pieces */
typedef struct {
    const uint8_t *data;
    uint32_t n, dur10;                   /* notes, length (10 ms) */
    char name[PL_NAME];
} pl_item_t;
static pl_item_t pl_item[PL_MAX];
static uint32_t pl_count;                /* written by m1_book.c only while nothing plays */
static volatile uint8_t demo_req;        /* 1 play, 2 stop, 3 next, 4 restart, 5 previous, 6 pl_target (from the UI) */
static volatile uint8_t pl_target;       /* the piece chosen in the list */
static uint8_t pl_cursor;                /* the list's cursor (UI) */
static uint32_t pl_pick_ms;              /* KNOB 1 last turned (UI): the choice plays 0.8 s later */
static uint32_t pl_list_ms;
static uint8_t pl_songs;                 /* the songs come first in the list, the sketches after (m1_book.c) */
static uint8_t pl_start;                 /* where the player starts (0; after an erase, where it was) */
static uint8_t pl_del;                   /* UI: erase asked for piece pl_del - 1, REC held again confirms */
static uint32_t pl_del_ms;              /* KNOB 1 last turned (UI): the list stays up 5 s */
static volatile uint8_t demo_on;         /* playing (read by the UI) */
static volatile uint8_t pl_idx;          /* the piece playing (read by the UI) */
static volatile uint32_t pl_shown;       /* +1 at each piece start: the UI shows its name */
static const uint8_t *pl_data;
static uint32_t pl_n, pl_wait;
static uint32_t demo_pos, demo_i, demo_rd, demo_at;
static uint32_t nx_at, nx_note, nx_len, nx_vel;
static int nx_ok;
static uint8_t demo_t;
static int16_t demo_voice;
static struct { uint32_t off; uint8_t note; } demo_held[DEMO_HELD];

static void demo_release(void)
{
    track_t *t = &trk[demo_t];
    uint32_t k;
    for (k = 0; k < DEMO_HELD; k++)
        if (demo_held[k].note) {
            trk_note_off(t, demo_held[k].note);
            demo_held[k].note = 0;
        }
}

static void demo_end(void)
{
    demo_release();
    trk[demo_t].p[P_VOICE] = demo_voice;
    demo_on = 0;
}

static uint32_t al_var(void)
{
    uint32_t v = pl_data[demo_rd++];
    if (v & 0x80u)
        v = ((v & 0x7Fu) << 8) | pl_data[demo_rd++];
    return v;
}

/* the next note of the piece: 0 at the end; at, len in samples */
static int demo_next(uint32_t *at, uint32_t *note, uint32_t *len, uint32_t *vel)
{
    if (demo_i >= pl_n)
        return 0;
    demo_i++;
    demo_at += al_var() * 441u;                  /* 10 ms */
    *at = demo_at;
    *note = pl_data[demo_rd++] & 0x7Fu;
    *vel = pl_data[demo_rd++] & 0x7Fu;
    if (!*vel)
        *vel = 1;
    *len = al_var() * 882u;                      /* 20 ms */
    if (*len > PL_MAXLEN)
        *len = PL_MAXLEN;
    return 1;
}

static void demo_hold(uint32_t note, uint32_t off)
{
    uint32_t k, f = DEMO_HELD, old = DEMO_HELD;
    for (k = 0; k < DEMO_HELD; k++) {
        if (demo_held[k].note == note) {         /* the same key again: one note, the longer end */
            if (off > demo_held[k].off)
                demo_held[k].off = off;
            return;
        }
        if (!demo_held[k].note && f == DEMO_HELD)
            f = k;
        if (demo_held[k].note && (old == DEMO_HELD || demo_held[k].off < demo_held[old].off))
            old = k;
    }
    if (f == DEMO_HELD) {                        /* full: the one ending first lets go now */
        trk_note_off(&trk[demo_t], demo_held[old].note);
        f = old;
    }
    demo_held[f].note = (uint8_t)note;
    demo_held[f].off = off;
}

/* the length of a piece (10 ms): the end of its last note */
static uint32_t pl_measure(const uint8_t *d, uint32_t n)
{
    uint32_t i, rd = 0, t = 0, end = 0;
    for (i = 0; i < n; i++) {
        uint32_t v = d[rd++], len;
        if (v & 0x80u)
            v = ((v & 0x7Fu) << 8) | d[rd++];
        t += v;
        rd += 2u;
        len = d[rd++];
        if (len & 0x80u)
            len = ((len & 0x7Fu) << 8) | d[rd++];
        if (len * 2u > PL_MAXLEN / 441u)
            len = PL_MAXLEN / 882u;
        if (t + len * 2u > end)
            end = t + len * 2u;
    }
    return end;
}

/* start piece idx (past the last one: the first again, the playlist loops) */
static void pl_open(uint32_t idx)
{
    demo_release();
    pl_wait = 0;
    if (!pl_count) {
        demo_end();
        return;
    }
    if (idx >= pl_count)
        idx = 0;
    pl_idx = (uint8_t)idx;
    pl_data = pl_item[idx].data;
    pl_n = pl_item[idx].n;
    demo_pos = demo_i = demo_rd = demo_at = 0;
    nx_ok = demo_next(&nx_at, &nx_note, &nx_len, &nx_vel);
    pl_shown++;
}

static void demo_block(uint32_t n)
{
    track_t *t;
    uint32_t k, r = demo_req;
    demo_req = 0;
    if (r == 1u && !demo_on) {
        demo_t = song.sel;
        t = &trk[demo_t];
        trk_all_off(t);
        demo_voice = t->p[P_VOICE];
        t->p[P_VOICE] = V_POLY;                  /* chords: POLY while it plays, the part's mode after */
        demo_on = 1;
        pl_open(pl_start);
        pl_start = 0;
    } else if (demo_on) {
        if (r == 1u || r == 2u)
            demo_end();
        else if (r == 3u)
            pl_open(pl_idx + 1u);
        else if (r == 4u)
            pl_open(pl_idx);
        else if (r == 5u)
            pl_open(pl_idx ? pl_idx - 1u : pl_count - 1u);
        else if (r == 6u)
            pl_open(pl_target);
    }
    if (!demo_on)
        return;
    t = &trk[demo_t];
    if (pl_wait) {                               /* the silence between two pieces */
        if (pl_wait > n)
            pl_wait -= n;
        else
            pl_open(pl_idx + 1u);
        return;
    }
    for (k = 0; k < DEMO_HELD; k++)              /* the notes whose time is up */
        if (demo_held[k].note && demo_pos >= demo_held[k].off) {
            trk_note_off(t, demo_held[k].note);
            demo_held[k].note = 0;
        }
    while (nx_ok && nx_at <= demo_pos) {         /* the notes starting in this block */
        trk_note_on(t, nx_note, nx_vel);
        demo_hold(nx_note, nx_at + nx_len);
        nx_ok = demo_next(&nx_at, &nx_note, &nx_len, &nx_vel);
    }
    demo_pos += n;
    if (!nx_ok) {                                /* the piece is over once its last note ends */
        for (k = 0; k < DEMO_HELD && !demo_held[k].note; k++)
            ;
        if (k == DEMO_HELD)
            pl_wait = PL_GAP;
    }
}

/* SKETCHBOOK (Modular One 0.2): every note played (keys and MIDI in) goes into a ring, in RAM only.
 * REC held keeps the last phrase as a sketch (m1_book.c writes it to flash): a phrase starts at
 * the first note after at least SK_GAP of silence (no key held). Keeping again right away, with
 * nothing played in between, reaches one phrase further back each time. */
#define SK_EV 1280u                              /* (~640 notes; in the pool) */
#define SK_GAP (4u * 44100u)                     /* 4 s of silence between two phrases */
#define SK_WINDOW (120u * 44100u)                /* only the last 2 minutes */
#define SK_STARTS 64u
typedef struct {
    uint32_t t;                                  /* sk_clock at the event */
    uint8_t note, vel;                           /* vel 0 = note off */
    uint8_t rsv[2];
} sk_ev_t;
static sk_ev_t sk_ev[SK_EV] __attribute__((section(".pool")));   /* (the pool: RAM .bss is full) */
static volatile uint32_t sk_w;                   /* events written (the ring holds the last SK_EV) */
static volatile uint32_t sk_clock;               /* samples, since boot (wraps after 27 h: differences only) */
static volatile uint8_t m1_del_req;              /* list item + 1 to erase (ui_input.c, a sketch): m1_book.c */
static volatile uint8_t m1_keep_req;             /* REC held (ui_input.c): m1_book.c keeps a sketch */

static void sk_put(uint32_t note, uint32_t vel)
{
    sk_ev_t *e = &sk_ev[sk_w % SK_EV];
    e->t = sk_clock;
    e->note = (uint8_t)(note & 0x7Fu);
    e->vel = (uint8_t)(vel & 0x7Fu);
    sk_w++;
}

static uint32_t sk_put_var(uint8_t *o, uint32_t n, uint32_t max, uint32_t v)
{
    if (v > 0x7FFFu)
        v = 0x7FFFu;
    if (v < 128u) {
        if (n < max)
            o[n] = (uint8_t)v;
        return n + 1u;
    }
    if (n + 1u < max) {
        o[n] = (uint8_t)(0x80u | v >> 8);
        o[n + 1u] = (uint8_t)v;
    }
    return n + 2u;
}

/* The phrases of the ring, oldest first: starts[] gets the index (in sk_w counting) of the note
 * on that opens each one. Returns their count (the last SK_STARTS at most). */
static uint32_t sk_phrases(uint32_t w, uint32_t now, uint32_t *starts)
{
    static uint8_t held[128];
    uint32_t i, first = w > SK_EV ? w - SK_EV : 0u, ns = 0, nheld = 0, quiet = 0, any = 0;
    for (i = 0; i < 128u; i++)
        held[i] = 0;
    for (i = first; i < w; i++) {
        const sk_ev_t *e = &sk_ev[i % SK_EV];
        if (now - e->t > SK_WINDOW)              /* older than the window: only the keys it holds count */
            first = i + 1u;
        if (e->vel) {
            if (i >= first && !nheld && (!any || e->t - quiet >= SK_GAP)) {
                if (ns == SK_STARTS) {
                    uint32_t k;
                    for (k = 1; k < SK_STARTS; k++)
                        starts[k - 1u] = starts[k];
                    ns--;
                }
                starts[ns++] = i;
            }
            if (i >= first)
                any = 1;
            if (!held[e->note]) {
                held[e->note] = 1;
                nheld++;
            }
        } else if (held[e->note]) {
            held[e->note] = 0;
            if (!--nheld)
                quiet = e->t;
        }
    }
    return ns;
}

/* Pack the phrases [depth back .. now] into out (the piece format above). Returns the bytes
 * (0: nothing played); *notes, *dur10 (10 ms) and *got (phrases taken, <= depth) describe it. */
static uint32_t sk_extract(uint32_t depth, uint8_t *out, uint32_t max, uint32_t *notes, uint32_t *dur10, uint32_t *got)
{
    static uint32_t starts[SK_STARTS];
    uint32_t w = sk_w, now = sk_clock, ns = sk_phrases(w, now, starts), i, j, n = 0, cnt = 0, t0, prev, end = 0;
    *notes = *dur10 = *got = 0;
    if (!ns || !depth)
        return 0;
    if (depth > ns)
        depth = ns;
    *got = depth;
    i = starts[ns - depth];
    t0 = prev = sk_ev[i % SK_EV].t;
    for (; i < w; i++) {
        const sk_ev_t *e = &sk_ev[i % SK_EV];
        uint32_t off = now, len, m;
        if (!e->vel)
            continue;
        for (j = i + 1u; j < w; j++) {           /* its end: the next off, or the same key again */
            const sk_ev_t *f = &sk_ev[j % SK_EV];
            if (f->note == e->note) {
                off = f->t;
                break;
            }
        }
        len = (off - e->t + 441u) / 882u;
        if (!len)
            len = 1;
        m = n;
        n = sk_put_var(out, n, max, (e->t - prev + 220u) / 441u);
        if (n + 1u < max) {
            out[n] = e->note;
            out[n + 1u] = e->vel;
        }
        n = sk_put_var(out, n + 2u, max, len);
        if (n > max) {                           /* full: the sketch ends before this note */
            n = m;
            break;
        }
        prev = e->t;
        cnt++;
        if (off - t0 > end)
            end = off - t0;
    }
    *notes = cnt;
    *dur10 = end / 441u;
    return cnt ? n : 0u;
}

