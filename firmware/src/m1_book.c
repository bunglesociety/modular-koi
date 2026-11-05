/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Compagnon on the FM-1: the songs and the sketchbook in flash, for the tape (m1_tape.c), as Modular One 0.2
 * keeps them (the same places, the same formats: what Modular One kept plays here, and the Carnet app reads both).
 * Two of Felucca's three user sample slots (eng_sample.c SMP_USER_HAS: the sample editor keeps slot 1 only):
 *   sketches  0xB4000..0xC7FFF (slot 2): 20 sectors. A sketch = a 32-byte header at the start of a sector, then its
 *             name (12 bytes) and its notes (the piece format, m1_tape.c); it takes as many sectors as it needs; the
 *             sketches follow each other from the first sector, the first sector without a valid header is the free
 *             space.
 *   songs     0xC8000..0xDBFFF (slot 3): written from the computer (the Carnet app, tools/fm1_songs.py; editor
 *             commands 40..49 below): a 512-byte header (count, per song offset, notes, name), then the data at +512.
 * Everything here runs in the main loop (a sector erase stops the audio for its time). */
#define SK_BASE (SMP_USER_BASE + SMP_USER_SIZE)
#define SK_NSECT (SMP_USER_SIZE / 0x1000u)
#define SK_MAGIC 0x314B5346u                       /* "FSK1" */
#define SONG_BASE (SMP_USER_BASE + 2u * SMP_USER_SIZE)
#define SONG_SIZE SMP_USER_SIZE
#define SONG_DATA 512u
#define SONG_MAGIC 0x314E5346u                     /* "FSN1" */
#define SONG_MAX 20u
#define SK_OUT (SK_EV * 6u)
_Static_assert(SONG_BASE + SONG_SIZE <= 0xDC000u, "the songs past the user sample slots (the user presets follow)");

typedef struct {
    uint32_t magic;
    uint16_t nsect, num;                           /* sectors taken; its number when kept */
    uint32_t notes, len, dur10, crc;               /* len: bytes after the header; CRC-32 of them */
    uint32_t flags, rsv;                           /* flags SK_NAMED: the body starts with the name */
} sk_hdr_t;                                        /* 32 B */
#define SK_NAMED 0xFFFFFFFEu                       /* (0xFFFFFFFF: no name, the sketches of 0.2 before names) */
#define SK_NAME 12u                                /* the name: 12 bytes, 0-padded (all 0: "SKETCH n") */
typedef struct {
    uint32_t off, notes;                           /* off: from the data start (+512) */
    char name[12];
} song_ent_t;                                      /* 20 B */
typedef struct {
    uint32_t magic;
    uint16_t version, count;
    uint32_t data_len, crc, rsv;
    song_ent_t e[SONG_MAX];
} song_hdr_t;                                      /* 20 + 400 = 420 B */
typedef char m1_size_check[sizeof(song_hdr_t) == 420u && sizeof(sk_hdr_t) == 32u ? 1 : -1];   /* fm1_songs.py's layout */

static struct {
    uint8_t scan;                                  /* the list is to be rebuilt (when nothing plays) */
    uint8_t writing;                               /* songs: SONG_BEGIN done, SONG_END not yet */
    uint8_t nsk, free_sect, songs;
    uint8_t last_sect, last_ok;                    /* the last kept sketch: overwritten by a deeper keep */
    uint16_t next_num;
    uint32_t last_w, last_ms, depth;
} m1b;
/* the sketch being written: in the screen's canvas (gfx.c cv_px, 59 KB), free while the main loop is here (the
 * screen draws after; lcd_sync first: the last blit may still read it) */
#define sk_out ((uint8_t *)cv_px)
typedef char m1_skout_fits[SK_OUT + 256u <= sizeof cv_px ? 1 : -1];

static const uint8_t *m1_xip(uint32_t off) { return fm1_xip_ptr(off); }
static void m1_inval(uint32_t off, uint32_t n)
{
    fm1_irq_off();
    fl_inval(off, n);
    fm1_irq_on();
}

static int m1_sk_valid(uint32_t s, const sk_hdr_t **hp)
{
    const sk_hdr_t *h = (const sk_hdr_t *)m1_xip(SK_BASE + s * 0x1000u);
    *hp = h;
    if (h->magic != SK_MAGIC || !h->nsect || s + h->nsect > SK_NSECT || !h->notes ||
        h->len > h->nsect * 0x1000u - sizeof(sk_hdr_t) || (h->flags == SK_NAMED && h->len < SK_NAME))
        return 0;
    return st_crc32((const uint8_t *)h + sizeof(sk_hdr_t), h->len) == h->crc;
}

static void m1_name(char *d, const char *s, uint32_t n)   /* printable ASCII, upper case */
{
    uint32_t i, k = 0;
    for (i = 0; i < n && s[i] && k < PL_NAME - 1u; i++) {
        char c = s[i];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 32);
        d[k++] = c >= 32 && c < 127 ? c : ' ';
    }
    d[k] = 0;
}

/* a sketch's notes; its name (if one was given on the FM-1's Carnet app), else "CROQUIS n" */
static const uint8_t *m1_sk_notes(const sk_hdr_t *h)
{
    return (const uint8_t *)h + sizeof(sk_hdr_t) + (h->flags == SK_NAMED ? SK_NAME : 0u);
}
static void m1_sk_name(const sk_hdr_t *h, char *b)
{
    uint32_t k = 7, v = h->num;
    if (h->flags == SK_NAMED) {
        m1_name(b, (const char *)h + sizeof(sk_hdr_t), SK_NAME);
        if (b[0] && !(b[0] == ' ' && !b[1]))
            return;
    }
    str_cpy(b, "SKETCH ", PL_NAME);
    if (v >= 10u)
        b[k++] = (char)('0' + v / 10u % 10u);
    b[k++] = (char)('0' + v % 10u);
    b[k] = 0;
}

/* the songs sent from the computer (0: none, or not whole) */
static const song_hdr_t *m1_songs(void)
{
    const song_hdr_t *h = (const song_hdr_t *)m1_xip(SONG_BASE);
    uint32_t i;
    if (h->magic != SONG_MAGIC || h->version != 1 || !h->count || h->count > SONG_MAX ||
        h->data_len > SONG_SIZE - SONG_DATA)
        return 0;
    for (i = 0; i < h->count; i++)
        if (h->e[i].off >= h->data_len || !h->e[i].notes)
            return 0;
    if (st_crc32(m1_xip(SONG_BASE + SONG_DATA), h->data_len) != h->crc)
        return 0;
    return h;
}

/* rebuild the player's list: the songs, then the sketches */
static void m1_rescan(void)
{
    const song_hdr_t *sh = flash_ok && !m1b.writing ? m1_songs() : 0;
    uint32_t n = 0, s, i;
    m1b.songs = 0;
    if (sh) {
        for (i = 0; i < sh->count && n < PL_MAX; i++, n++) {
            pl_item[n].data = m1_xip(SONG_BASE + SONG_DATA + sh->e[i].off);
            pl_item[n].n = sh->e[i].notes;
            m1_name(pl_item[n].name, sh->e[i].name, 12);
            if (!pl_item[n].name[0])
                str_cpy(pl_item[n].name, "SONG", PL_NAME);
            pl_item[n].dur10 = pl_measure(pl_item[n].data, pl_item[n].n);
        }
        m1b.songs = (uint8_t)n;
    }
    pl_songs = m1b.songs;
    m1b.nsk = 0;
    m1b.next_num = 1;
    for (s = 0; flash_ok && s < SK_NSECT;) {
        const sk_hdr_t *h;
        if (!m1_sk_valid(s, &h))
            break;
        if (n < PL_MAX) {
            pl_item[n].data = m1_sk_notes(h);
            pl_item[n].n = h->notes;
            pl_item[n].dur10 = h->dur10;
            m1_sk_name(h, pl_item[n].name);
            n++;
        }
        if (h->num >= m1b.next_num)
            m1b.next_num = (uint16_t)(h->num + 1u);
        m1b.nsk++;
        s += h->nsect;
    }
    m1b.free_sect = (uint8_t)s;
    pl_count = n;
}

/* the player's list may only change while nothing plays */
static int m1_quiet(void) { return !demo_on && !demo_req; }

/* write a sketch at sector s (its sectors are erased first; the header goes last) */
static int m1_sk_write(uint32_t s, uint32_t num, uint32_t flags, const uint8_t *data, uint32_t len, uint32_t notes,
                       uint32_t dur10)
{
    static sk_hdr_t h;
    static uint8_t page[256] __attribute__((aligned(4)));
    uint32_t ns = (sizeof h + len + 0xFFFu) / 0x1000u, i, off, took;
    if (s + ns > SK_NSECT)
        return 1;
    for (i = 0; i < ns; i++) {
        audio_silence();
        if (fl_erase4k(SK_BASE + (s + i) * 0x1000u, &took))
            return 2;
        fm1_wdt_feed();
    }
    for (off = 0; off < sizeof h + len; off += 256u) {   /* the header's bytes stay 0xFF for now */
        uint32_t k;
        for (k = 0; k < 256u; k++) {
            uint32_t p = off + k;
            page[k] = p < sizeof h || p >= sizeof h + len ? 0xFFu : data[p - sizeof h];
        }
        if (fl_write(SK_BASE + s * 0x1000u + off, page, 256u))
            return 3;
    }
    h.magic = SK_MAGIC;
    h.nsect = (uint16_t)ns;
    h.num = (uint16_t)num;
    h.notes = notes;
    h.len = len;
    h.dur10 = dur10;
    h.crc = st_crc32(data, len);
    h.flags = flags;
    h.rsv = 0xFFFFFFFFu;
    if (fl_write(SK_BASE + s * 0x1000u, (const uint8_t *)&h, sizeof h))
        return 3;
    m1_inval(SK_BASE + s * 0x1000u, ns * 0x1000u);
    return 0;
}

static void m1_say_kept(uint32_t num, uint32_t dur10)
{
    static char lab[16] = "SKETCH ";
    char b[16];
    uint32_t k = 7, sec = dur10 / 100u;
    if (num >= 10u)
        lab[k++] = (char)('0' + num / 10u % 10u);
    lab[k++] = (char)('0' + num % 10u);
    lab[k] = 0;
    k = 0;
    if (sec >= 600u)
        b[k++] = (char)('0' + sec / 600u % 10u);
    b[k++] = (char)('0' + sec / 60u % 10u);
    b[k++] = ':';
    b[k++] = (char)('0' + sec % 60u / 10u);
    b[k++] = (char)('0' + sec % 10u);
    str_cpy(b + k, " KEPT", 6);
    m1_say(lab, b);
}

/* REC held: keep the last phrase (again right away: one phrase further back) */
static void m1_keep(void)
{
    lcd_sync();
    uint32_t w = sk_w, deeper, depth, len, notes, dur10, got, s, num, rc;
    if (!flash_ok) {
        m1_say("SKETCHBOOK", "NO FLASH");
        return;
    }
    deeper = m1b.last_ok && w == m1b.last_w && fm1_ms - m1b.last_ms < 20000u;
    depth = deeper ? m1b.depth + 1u : 1u;
    len = sk_extract(depth, sk_out + SK_NAME, SK_OUT, &notes, &dur10, &got);   /* after an empty name */
    if (!len) {
        m1_say("SKETCHBOOK", "NOTHING YET");
        return;
    }
    if (deeper && got < depth) {                     /* no older phrase: that one stays as it is */
        m1_say("SKETCHBOOK", "NO EARLIER");
        m1b.last_ms = fm1_ms;
        return;
    }
    if (demo_on || demo_req) {                       /* the list changes under the player */
        demo_req = 2;
        while (demo_on)
            fm1_wdt_feed();
    }
    if (deeper) {
        s = m1b.last_sect;
        num = m1b.next_num ? m1b.next_num - 1u : 1u;
    } else {
        s = m1b.free_sect;
        num = m1b.next_num;
    }
    memset(sk_out, 0, SK_NAME);
    rc = m1_sk_write(s, num, SK_NAMED, sk_out, SK_NAME + len, notes, dur10);
    m1_rescan();
    if (rc == 1u) {
        m1_say("SKETCHBOOK", "FULL");
        m1b.last_ok = 0;
        return;
    }
    if (rc) {
        m1_say("SKETCHBOOK", "FLASH ERROR");
        m1b.last_ok = 0;
        return;
    }
    m1b.last_ok = 1;
    m1b.last_sect = (uint8_t)s;
    m1b.last_w = w;
    m1b.last_ms = fm1_ms;
    m1b.depth = got;
    m1_say_kept(num, dur10);
}

/* erase the sketch j (0 = the first one): the ones after it move down, the sectors freed at the end are
 * erased (after the last sketch the flash stays blank: the scan stops there) */
static int m1_sk_erase(uint32_t j)
{
    lcd_sync();
    const sk_hdr_t *h;
    uint32_t s = 0, i, dst, took;
    for (i = 0; i < j; i++) {
        if (!m1_sk_valid(s, &h))
            return 1;
        s += h->nsect;
    }
    if (!m1_sk_valid(s, &h))
        return 1;
    dst = s;
    s += h->nsect;
    while (s < SK_NSECT && m1_sk_valid(s, &h)) {
        uint32_t ns = h->nsect, num = h->num, notes = h->notes, len = h->len, dur10 = h->dur10, fl = h->flags;
        if (len > SK_OUT + 256u)
            return 2;
        memcpy(sk_out, (const uint8_t *)h + sizeof(sk_hdr_t), len);   /* (the copy may land on itself) */
        if (m1_sk_write(dst, num, fl, sk_out, len, notes, dur10))
            return 3;
        dst += (sizeof(sk_hdr_t) + len + 0xFFFu) / 0x1000u;
        s += ns;
    }
    for (i = dst; i < s; i++) {
        audio_silence();
        if (fl_erase4k(SK_BASE + i * 0x1000u, &took))
            return 3;
        fm1_wdt_feed();
    }
    m1_inval(SK_BASE + dst * 0x1000u, (s - dst) * 0x1000u);
    m1b.last_ok = 0;
    return 0;
}

/* the list's item k erased (a sketch; ui_input.c, REC held twice in the list): the player stops,
 * the list is rebuilt, the player goes on from the same place */
static void m1_del(uint32_t k)
{
    char b[PL_NAME + 8];
    int was = demo_on;
    if (!flash_ok || k < pl_songs || k >= pl_count)
        return;
    str_cpy(b, pl_item[k].name, PL_NAME);
    if (demo_on || demo_req) {
        demo_req = 2;
        while (demo_on)
            fm1_wdt_feed();
    }
    if (m1_sk_erase(k - pl_songs)) {
        m1_rescan();
        m1_say("SKETCHBOOK", "FLASH ERROR");
        return;
    }
    m1_rescan();
    m1_say(b, "ERASED");
    if (was && pl_count) {
        pl_start = (uint8_t)(k < pl_count ? k : 0u);
        pl_cursor = pl_start;
        demo_req = 1;
    }
}

/* rename the sketch j (0 = the first): the Carnet app */
static int m1_sk_rename(uint32_t j, const char *name)
{
    const sk_hdr_t *h;
    uint32_t s = 0, i, nl, num, notes, dur10, rc;
    lcd_sync();
    for (i = 0; i < j; i++) {
        if (!m1_sk_valid(s, &h))
            return 1;
        s += h->nsect;
    }
    if (!m1_sk_valid(s, &h))
        return 1;
    nl = h->len - (h->flags == SK_NAMED ? SK_NAME : 0u);
    if (sizeof(sk_hdr_t) + SK_NAME + nl > h->nsect * 0x1000u || SK_NAME + nl > SK_OUT + 256u)
        return 4;                                    /* no room for the name */
    memset(sk_out, 0, SK_NAME);
    for (i = 0; i < SK_NAME && name[i]; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 32);
        sk_out[i] = (uint8_t)(c >= 32 && c < 127 ? c : ' ');
    }
    memcpy(sk_out + SK_NAME, m1_sk_notes(h), nl);
    num = h->num;
    notes = h->notes;
    dur10 = h->dur10;
    rc = (uint32_t)m1_sk_write(s, num, SK_NAMED, sk_out, SK_NAME + nl, notes, dur10);
    if (m1b.last_ok && m1b.last_sect == s)
        m1b.last_ok = 0;                             /* a deeper keep would overwrite the renamed one */
    return (int)rc;
}

/* erase every sketch (the Carnet app) */
static int m1_sk_clear(void)
{
    uint32_t i, took;
    int rc = 0;
    for (i = 0; i < SK_NSECT && !rc; i++) {
        audio_silence();
        rc = fl_erase4k(SK_BASE + i * 0x1000u, &took);
        fm1_wdt_feed();
    }
    m1_inval(SK_BASE, SK_NSECT * 0x1000u);
    m1b.last_ok = 0;
    m1b.scan = 1;
    return rc;
}

static void m1_stop_tape(void)                       /* (the list changes: nothing may play from it meanwhile) */
{
    demo_req = 2;
    while (demo_on)
        fm1_wdt_feed();
}

/* the Carnet app (web/carnet.html, tools/fm1_songs.py), the editor's SysEx: Modular One 0.2's commands 40..49 */
enum { ED_SONG_BEGIN = 40, ED_SONG_WRITE, ED_SONG_END, ED_SONG_INFO, ED_SK_INFO, ED_SK_READ, ED_SK_CLEAR,
       ED_SONG_READ, ED_SK_ERASE, ED_SK_RENAME };
static int ed_m1_handle(uint32_t cmd, const uint8_t *a, uint32_t na)
{
    uint32_t i;
    if (cmd < ED_SONG_BEGIN || cmd > ED_SK_RENAME)
        return 0;
    if (!flash_ok)
        return 1;
    switch (cmd) {
    case ED_SONG_BEGIN: {                                  /* -> rc. Stops the player; the bank is empty from now on */
        uint32_t took;
        m1_stop_tape();
        m1b.scan = 1;
        m1b.writing = 1;
        audio_silence();
        ed_b(fl_erase4k(SONG_BASE, &took) ? 1u : 0u);
        m1_inval(SONG_BASE, 0x1000u);
        break;
    }
    case ED_SONG_WRITE: {                                  /* off (3 x 7 bit), pack7 data -> off, rc (as SMP_WRITE) */
        uint32_t off, len, rc = 0, took;
        if (na < 4u)
            return 1;
        off = (uint32_t)a[0] | (uint32_t)a[1] << 7 | (uint32_t)a[2] << 14;
        len = ed_unpack7(a + 3, na - 3u, ed_smp_buf, 256u);
        if (off < SONG_DATA || (off & 0xFFu) || !len || off + len > SONG_SIZE)
            rc = 1;
        else if (!m1b.writing)
            rc = 4;                                        /* SONG_BEGIN first */
        else {
            if (!(off & 0xFFFu)) {
                audio_silence();
                rc = fl_erase4k(SONG_BASE + off, &took) ? 2u : 0u;
            }
            if (!rc && fl_write(SONG_BASE + off, ed_smp_buf, len))
                rc = 3;
        }
        ed_b(off);
        ed_b(off >> 7);
        ed_b(off >> 14);
        ed_b(rc);
        break;
    }
    case ED_SONG_END: {                                    /* pack7 header (420 B) -> rc: 0 ok, 1 size, 2 header, 3 CRC, 4 flash */
        uint32_t rc = 0;
        if (ed_unpack7(a, na, ed_smp_buf, sizeof(song_hdr_t)) != sizeof(song_hdr_t))
            rc = 1;
        else {
            const song_hdr_t *h = (const song_hdr_t *)ed_smp_buf;
            m1_inval(SONG_BASE, SONG_SIZE);
            if (h->magic != SONG_MAGIC || h->version != 1 || !h->count || h->count > SONG_MAX ||
                h->data_len > SONG_SIZE - SONG_DATA)
                rc = 2;
            else if (st_crc32(m1_xip(SONG_BASE + SONG_DATA), h->data_len) != h->crc)
                rc = 3;
            else if (fl_write(SONG_BASE, ed_smp_buf, sizeof(song_hdr_t)))
                rc = 4;
            m1_inval(SONG_BASE, 0x1000u);
        }
        m1b.writing = 0;
        m1b.scan = 1;
        ed_b(rc);
        break;
    }
    case ED_SONG_INFO: {                                   /* -> count, then per song: notes (3 x 7 bit), name */
        const song_hdr_t *h = m1b.writing ? 0 : m1_songs();
        uint32_t c = h ? h->count : 0u;
        ed_b(c);
        for (i = 0; i < c; i++) {
            char nm[13] = {0};
            uint32_t j;
            for (j = 0; j < 12u; j++)
                nm[j] = h->e[i].name[j] >= 32 && h->e[i].name[j] < 127 ? h->e[i].name[j] : 0;
            ed_b(h->e[i].notes);
            ed_b(h->e[i].notes >> 7);
            ed_b(h->e[i].notes >> 14);
            ed_str(nm, 12);
        }
        break;
    }
    case ED_SK_INFO:                                       /* -> sketches, sectors used, sectors, KiB per sector */
        ed_b(m1b.nsk);
        ed_b(m1b.free_sect);
        ed_b(SK_NSECT);
        ed_b(4);
        break;
    case ED_SK_READ:                                       /* off (3 x 7 bit) -> off, pack7 256 bytes of the sketch area */
    case ED_SONG_READ: {                                   /* off (3 x 7 bit) -> off, pack7 256 bytes of the song bank */
        uint32_t off, k;
        const uint8_t *p;
        if (na < 3u)
            return 1;
        off = (uint32_t)a[0] | (uint32_t)a[1] << 7 | (uint32_t)a[2] << 14;
        if (off + 256u > (cmd == ED_SK_READ ? SK_NSECT * 0x1000u : SONG_SIZE))
            return 1;
        p = m1_xip((cmd == ED_SK_READ ? SK_BASE : SONG_BASE) + off);
        ed_b(off);
        ed_b(off >> 7);
        ed_b(off >> 14);
        for (k = 0; k < 256u; k += 7u) {                   /* pack7: the top bits, then 7 bytes */
            uint32_t j, m = 0;
            for (j = 0; j < 7u && k + j < 256u; j++)
                m |= (uint32_t)(p[k + j] >> 7) << j;
            ed_b(m);
            for (j = 0; j < 7u && k + j < 256u; j++)
                ed_b(p[k + j]);
        }
        break;
    }
    case ED_SK_CLEAR:                                      /* -> rc. Stops the player */
        m1_stop_tape();
        ed_b(m1_sk_clear() ? 1u : 0u);
        break;
    case ED_SK_ERASE: {                                    /* the sketch j (0 = the first) -> rc. Stops the player */
        uint32_t rc;
        if (na < 1u)
            return 1;
        m1_stop_tape();
        rc = a[0] < m1b.nsk ? (uint32_t)m1_sk_erase(a[0]) : 1u;
        m1_rescan();
        ed_b(rc);
        break;
    }
    case ED_SK_RENAME: {                                   /* the sketch j, name (<= 12 chars, 0-ended) -> rc. Stops the player */
        char nm[13] = {0};
        uint32_t j, rc;
        if (na < 1u)
            return 1;
        m1_stop_tape();
        for (j = 0; j < 12u && 1u + j < na && a[1u + j]; j++)
            nm[j] = (char)a[1u + j];
        rc = a[0] < m1b.nsk ? (uint32_t)m1_sk_rename(a[0], nm) : 1u;
        m1_rescan();
        ed_b(rc);
        break;
    }
    }
    return 1;
}

static void m1_boot(void)
{
    m1_rescan();
}

/* main loop */
static void m1_service(void)
{
    if (m1_del_req) {
        uint32_t k = m1_del_req - 1u;
        m1_del_req = 0;
        m1_del(k);
    }
    if (m1_keep_req) {
        m1_keep_req = 0;
        m1_keep();
    }
    if (m1b.scan && m1_quiet()) {
        m1b.scan = 0;
        m1_rescan();
    }
}
