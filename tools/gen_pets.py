#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular Compagnon's companions: the cat (tools/gen_cat.py), the fox, the robot and the fish. Each one fills the
same clip slots (CAT_IDLE, CAT_LOOK_L, ... the cat's names: the firmware's life of the companion, m1_draw.c, is the
same for all); a slot a companion has no scene of its own for plays another of its scenes (ALIAS). Drawn as the cat:
an 80 x 80 grid, design units scaled 1.2 x around the feet (gen_cat.VIEW), outline and light by gen_cat.C.finish.

Out: build/gen/cat_sprites.h: every companion's palette, frames and clips, and PETS[] (the firmware picks one).
     --preview DIR: each companion's clips as GIFs.   --only NAME: that companion alone (with --preview)."""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_cat as gc
from gen_cat import C, T, VIEW, bez, rot, anchor

G, SCENE = gc.G, gc.SCENE
SLOTS = [n for n, _, _ in gc.CLIPS]                     # the cat's clip names, in the firmware's order


def within(c, shape, k):
    """recolour with k the cells of c's silhouette (not its outline) that shape (a canvas) covers"""
    for p in shape.m:
        if p in c.m and c.m[p] != "o": c.m[p] = k


def over(c, d):
    """d (finished: its own outline) drawn over c"""
    c.m.update(d.m); return c


def moved(c, dx):
    d = C(); d.m = {(x + dx, y): k for (x, y), k in c.m.items()}; return d


def shift(c, dx=0, dy=0):
    c.m = {(x + dx, y + dy): k for (x, y), k in c.m.items()}; return c


def eyes_pat(c, x, y, kind, look=0, ink="N", eye="E"):
    """a face's eye at design point x, y (patterns in cells, as the cat's)"""
    if kind == "open":
        c.pat(x, y, [(-1 + dx + look, dy) for dx in (0, 1) for dy in (-1, 0, 1)], eye); c.pat(x, y, [(-1 + look, -1)], "H")
    elif kind == "wide":
        c.pat(x, y, [(dx + look, dy - 1) for dx in (-1, 0, 1) for dy in (-1, 0, 1, 2)], eye); c.pat(x, y, [(-1 + look, -2)], "H")
    elif kind == "half":
        c.pat(x, y, [(dx, -1) for dx in (-2, -1, 0, 1, 2)], ink); c.pat(x, y, [(-1, 0), (0, 0)], eye)
    elif kind == "happy":
        c.pat(x, y, [(-2, 1), (-1, 0), (0, -1), (1, 0), (2, 1)], ink)
    elif kind == "content":
        c.pat(x, y, [(-2, 0), (-1, 1), (0, 1), (1, 1), (2, 0)], ink)
    elif kind == "squeeze":
        s = 1 if x > 40 else -1
        c.pat(x, y, [(-s, -1), (0, 0), (-s, 1)], ink)


# ================================================================== the fox
FOX_PAL = [None, "#2a1a14", "#e2782c", "#b5561c", "#f6a45a", "#f7f0e4", "#d9cdb8", "#3b2a22", "#1b1410", "#ffffff",
           "#ff8fa6", "#cfc8ff", "#2c6e6a", "#f27c9a", "#ffd65a", "#f4a3b6"]
FOX_KEY = dict(o=1, K=2, D=3, L=4, W=5, S=6, N=7, E=8, H=9, M=10, z=11, n=12, h=13, b=14, B=15, w=9, F=6, P=15, Y=14)


class FoxHead:
    def __init__(self, cx, cy, tilt=0.0, ear=0):
        self.cx, self.cy, self.a, self.ear = cx, cy, tilt, ear
    def R(self, x, y): return rot(self.cx + x, self.cy + y, self.cx, self.cy, self.a)
    def shape(self, c):
        c.ell(self.cx, self.cy, 10, 7.5)
        c.tri(self.R(-9.5, -2), self.R(-13, 5), self.R(-4, 5)); c.tri(self.R(9.5, -2), self.R(13, 5), self.R(4, 5))
        c.tri(self.R(-9, -2), self.R(-9 - self.ear * 0.5, -16 + abs(self.ear)), self.R(-2.5, -6))
        c.tri(self.R(9, -2), self.R(9 + self.ear, -16 + abs(self.ear)), self.R(2.5, -6))
        c.ell(*self.R(0, 3.5), 4.5, 3.4)
    def detail(self, c, eyes="open", mouth="", look=0):
        e = abs(self.ear)
        m = C(); m.tri(self.R(-7.5, -4), self.R(-7.8 - self.ear * 0.5, -12.5 + e), self.R(-4, -6.5)); m.tri(self.R(7.5, -4), self.R(7.8 + self.ear, -12.5 + e), self.R(4, -6.5)); within(c, m, "W")
        m = C(); m.tri(self.R(-10, -12), self.R(-9 - self.ear * 0.5, -16 + e), self.R(-6.5, -13)); m.tri(self.R(10, -12), self.R(9 + self.ear, -16 + e), self.R(6.5, -13)); within(c, m, "N")
        m = C(); m.ell(*self.R(-6, 3.6), 4.6, 2.6); m.ell(*self.R(6, 3.6), 4.6, 2.6); m.ell(*self.R(0, 4.8), 4, 2.6); within(c, m, "W")
        for sx in (-1, 1): eyes_pat(c, *self.R(sx * 4.6, -0.5), eyes, look)
        c.pat(*self.R(0, 2.6), [(-1, 0), (0, 0), (1, 0), (0, 1)], "N")
        if mouth == "o": c.pat(*self.R(0, 5.4), [(-1, 0), (0, 0), (1, 0), (-1, 1), (0, 1), (1, 1)], "M")
        elif mouth == "lick": c.pat(*self.R(0, 5.2), [(0, 0), (1, 0), (1, -1)], "M")
        else: c.pat(*self.R(0, 5), [(-1, 0), (1, 0)], "N")
        for sx in (-1, 1): c.pat(*self.R(sx * 7.5, 2.5), [(0, 0), (sx, 0)], "B")


def fox_tail_front(cx, by, t=0.0):
    """the brush wrapped round the front of the feet, its white tip on the left"""
    d = C()
    d.stroke(bez((cx + 8, by - 5), (cx + 22, by - 3), (cx + 13, by + 0)), 3.0)
    d.stroke(bez((cx + 13, by + 0), (cx + 2, by + 1.2), (cx - 9 + t, by - 1.5 - abs(t) * 0.3)), 2.7)
    d.ell(cx - 10 + t, by - 1.8, 3.0, 2.4)
    d.finish()
    m = C(); m.ell(cx - 10 + t, by - 1.8, 4.2, 3.2); within(d, m, "W")
    return d


def fox_sit(eyes="open", mouth="", tail=0.0, look=0, tilt=0.0, ear=0, nod=0, cx=38, base=62, leg=None):
    c = C(); by = base
    c.ell(cx, by - 9, 10, 8.5); c.ell(cx, by - 15, 7.5, 6.5)
    for lx in (cx - 3.5, cx + 3.5): c.ell(lx, by - 1, 2.4, 1.6)
    h = FoxHead(cx + nod * 0.3, by - 25 + nod, tilt, ear); h.shape(c)
    c.finish()
    m = C(); m.ell(cx, by - 14, 4.6, 6.5); within(c, m, "W")
    for lx in (cx - 3.5, cx + 3.5):
        m = C(); m.stroke([(lx, by - 7), (lx, by - 1)], 1.1); m.ell(lx, by - 1, 2.2, 1.4); within(c, m, "N")
    if leg is not None:                                          # a hind leg up to the ear (the scratch)
        l = C(); l.stroke([(cx + 8, by - 4), (cx + 12, by - 10), leg], 1.8); l.ell(leg[0], leg[1], 2.2, 1.8); l.finish()
        m = C(); m.ell(leg[0], leg[1], 2.6, 2.2); within(l, m, "N"); over(c, l)
    over(c, fox_tail_front(cx, by, tail))
    h.detail(c, eyes, mouth, look)
    return c


def fox_side(cx=40, base=62, phase=0, facing=1, eyes="open", crouch=0, tail_up=1.0, lift=0, ear=0):
    c = C(); by = base - 3; f = facing
    tb = cx - f * 11
    t = C(); t.stroke(bez((tb + f * 2, by - 10 + crouch), (tb - f * 8, by - 12 - tail_up * 4), (tb - f * 13, by - 8 - tail_up * 7)), 2.8)
    t.ell(tb - f * 14, by - 8 - tail_up * 7.5, 3, 2.6); t.finish()
    m = C(); m.ell(tb - f * 14.5, by - 8 - tail_up * 7.5, 3.6, 3.2); within(t, m, "W")
    s = phase
    for lx, dx in ((cx + f * 8, f * (2 + 4 * s)), (cx + f * 5, f * (-1 + 3 * s)), (cx - f * 6, -f * (1 + 3 * s)), (cx - f * 9, -f * (2 + 4 * s))):
        c.stroke([(lx, by - 7 + crouch), (lx + dx, by + 2 - s * 1.5)], 1.1)
    c.ell(cx, by - 10 + crouch, 12, 5.8 - crouch * 0.4)
    hx, hy = cx + f * 11, by - 17 + crouch
    c.ell(hx, hy, 6, 5); c.tri((hx + f * 3, hy - 2), (hx + f * 12, hy + 2.5), (hx + f * 2, hy + 4))
    c.tri((hx - f * 3, hy - 3), (hx - f * 1 - f * ear, hy - 13), (hx + f * 3, hy - 3))
    c.finish(); over(c, t); c = shift(c, 0, -lift)
    m = C(); m.stroke([(hx + f * 4, hy + 2.5), (hx + f * 10, hy + 2.5)], 1.0); m.ell(cx + f * 6, by - 8 + crouch, 4, 2.6)
    shift(m, 0, -lift); within(c, m, "W")
    for lx, dx in ((cx + f * 8, f * (2 + 4 * s)), (cx + f * 5, f * (-1 + 3 * s)), (cx - f * 6, -f * (1 + 3 * s)), (cx - f * 9, -f * (2 + 4 * s))):
        m = C(); m.stroke([(lx + dx * 0.6, by - 1 + crouch * 0.3), (lx + dx, by + 2 - s * 1.5)], 1.1); shift(m, 0, -lift); within(c, m, "N")
    m = C(); m.tri((hx - f * 2, hy - 7), (hx - f * 1 - f * ear, hy - 13), (hx + f * 1.5, hy - 7)); shift(m, 0, -lift); within(c, m, "N")
    X, Y = T(hx + f * 1.5, hy - 1); c.raw(X, Y - lift, "E" if eyes != "content" else "N")
    if eyes == "open": c.raw(X - f, Y - 1 - lift, "H")
    X, Y = T(hx + f * 12, hy + 2.5); c.raw(X, Y - lift, "N")
    return c


def fox_dive(cx=40, base=62, wig=0, depth=0):
    """nose down in the grass after a mouse: the body upright, back legs kicking, the brush straight up"""
    c = C(); by = base + depth
    c.ell(cx, by - 9, 6, 10)
    for k, (lx, dx) in enumerate(((cx - 4, -4 - wig), (cx + 4, 4 + wig))):
        c.stroke([(lx, by - 14), (lx + dx, by - 19)], 1.1)
    t = C(); t.stroke(bez((cx, by - 17), (cx + wig * 2, by - 25), (cx - wig, by - 31)), 2.8); t.ell(cx - wig, by - 32, 3, 2.6); t.finish()
    m = C(); m.ell(cx - wig, by - 32.5, 3.6, 3.2); within(t, m, "W")
    c.finish(); over(c, t)
    for k, (lx, dx) in enumerate(((cx - 4, -4 - wig), (cx + 4, 4 + wig))):
        m = C(); m.ell(lx + dx, by - 19, 1.6, 1.6); within(c, m, "N")
    for k in range(5):                                           # the grass pushed aside
        c.pat(cx - 7 + k * 3.5, by - 0.5, [(0, 0), (0, -1)] if k % 2 else [(0, 0)], "D", True)
    return c


def fox_curl(eyes="content", breath=0, cx=40, base=62, tail=0):
    """asleep, a round ball, the brush over his nose"""
    c = C(); by = base
    c.ell(cx + 3, by - 7 - breath * 0.4, 15, 7.5 + breath * 0.4)
    hx, hy = cx - 7, by - 9
    c.ell(hx, hy, 7, 5.5)
    c.tri((hx - 6, hy - 1), (hx - 7, hy - 11), (hx - 1, hy - 4)); c.tri((hx + 2, hy - 4), (hx + 5, hy - 12), (hx + 6, hy - 2))
    c.finish()
    m = C(); m.tri((hx - 7.2, hy - 8), (hx - 7, hy - 11), (hx - 4.8, hy - 8.5)); m.tri((hx + 3.5, hy - 9), (hx + 5, hy - 12), (hx + 5.7, hy - 9)); within(c, m, "N")
    t = C(); t.stroke(bez((cx + 17, by - 4), (cx + 8, by + 2), (hx + 3 + tail, by - 2)), 3.0); t.ell(hx + 2 + tail, by - 2.5, 3.2, 2.8); t.finish()
    m = C(); m.ell(hx + 1.5 + tail, by - 2.5, 4, 3.4); within(t, m, "W"); over(c, t)
    eyes_pat(c, hx - 2.5, hy - 1, eyes)
    return c


def fox_back(cx=40, base=62, wiggle=0):
    c = C(); by = base
    c.ell(cx + 3, by - 6, 12, 6)
    for i, lx in enumerate((cx - 4, cx + 1, cx + 6, cx + 11)):
        w = wiggle * (1 if i % 2 else -1)
        c.stroke([(lx, by - 10), (lx + w, by - 16)], 1.1); c.ell(lx + w, by - 16.5, 1.7, 1.5)
    h = FoxHead(cx - 13, by - 9, -1.2); h.shape(c)
    t = C(); t.stroke(bez((cx + 14, by - 4), (cx + 22, by - 6), (cx + 21, by - 14 + wiggle)), 2.8); t.ell(cx + 21, by - 15 + wiggle, 3, 2.6); t.finish()
    m = C(); m.ell(cx + 21, by - 15.5 + wiggle, 3.6, 3.2); within(t, m, "W")
    c.finish(); over(c, t)
    m = C(); m.ell(cx + 3, by - 8, 8, 3); within(c, m, "W")
    for i, lx in enumerate((cx - 4, cx + 1, cx + 6, cx + 11)):
        w = wiggle * (1 if i % 2 else -1); m = C(); m.ell(lx + w, by - 16.5, 1.7, 1.5); within(c, m, "N")
    h.detail(c, "happy")
    return c


def dirt(c, x, y, t):
    for k in range(3):
        a = (t * 3 + k * 5) % 9
        c.pat(x - a, y - (a * (8 - a)) // 4, [(0, 0), (1, 0)], "D", True)
    return c


def stick(c, x, y, k="N"):
    """a little stick on the floor (cells), its middle at x, y"""
    pts = [(x + i, y - i // 4) for i in range(-6, 7)] + [(x + 2, y - 2), (x + 3, y - 3)]
    for p in pts: c.m[p] = k
    for (px, py) in pts:
        for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0)): c.m.setdefault((px + dx, py + dy), "o")
    return c


def pearl(c, x, y):
    """a pearl on the sand (cells)"""
    for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)): c.m[(x + dx, y + dy)] = "H"
    for dx, dy in ((-1, 0), (-1, 1), (2, 0), (2, 1), (0, -1), (1, -1), (0, 2), (1, 2)): c.m.setdefault((x + dx, y + dy), "o")
    c.m[(x, y)] = "H"; c.m[(x + 1, y + 1)] = "W"
    return c


def fox_clips():
    S, side = fox_sit, fox_side
    cl = {}
    fr = []
    for i in range(48):
        tl = (0, 0.5, 1, 1, 0.5, 0, -0.5, -1, -1, -0.5)[i % 10] if i % 24 < 10 else 0
        e = "content" if i < 16 else "open" if i < 40 else "happy"
        if i in (28, 29): e = "content"
        fr.append((S(e, tail=tl), 150))
    cl["IDLE"] = (fr, 1)
    cl["LOOK_L"] = ([(S("open", look=-1, tilt=-0.12, ear=1), 120), (S("open", look=-1, tilt=-0.12), 600), (gc.quest(S("open", look=-1, tilt=-0.12)), 900)], 0)
    cl["LOOK_R"] = ([(S("open", look=1, tilt=0.12, ear=1), 120), (S("open", look=1, tilt=0.12), 1500)], 0)
    cl["PLAY"] = ([(gc.notes(S("happy", tail=(-1, -0.5, 0.5, 1, 0.5, -0.5)[i % 6], nod=i % 2), i // 3), 120) for i in range(6)], 1)
    cl["SLEEP"] = ([(gc.zzz(fox_curl("content", breath=1 if (i // 5) % 2 else 0), 1 + (i // 4) % 3), 170) for i in range(20)], 1)
    cl["WAKE"] = ([(fox_curl("content"), 200), (fox_curl("open"), 200), (S("wide", ear=1), 200), (S("open"), 400)], 0)
    fr = [(S("half"), 150)] * 4 + [(S("half", nod=n), 150) for n in (1, 2, 3)] + [(S("content", nod=n), 150) for n in (3, 4, 4)]
    fr += [(side(40, facing=f, phase=k % 2, eyes="content", tail_up=0.3), 130) for k, f in enumerate((1, 1, -1, -1, 1, 1, -1, -1))]
    fr += [(gc.zzz(fox_curl("content"), 1 + i // 4), 150) for i in range(10)]
    cl["ENDORT"] = (fr, 0)
    cl["PETRIR"] = ([(S("half", tail=(0, 0.5, 1, 0.5)[i % 4]), 150) for i in range(12)] + [(side(40, facing=(1, -1)[k // 2 % 2], phase=k % 2, eyes="content", tail_up=0.3), 130) for k in range(12)] + [(fox_curl("content"), 300)], 0)
    # the mouse: he hears it, ears forward, crouches, leaps high and dives nose first into the grass
    fr = [(S("open", look=1, tilt=0.15 * (1 if (i // 3) % 2 else -1), ear=1), 120) for i in range(12)]
    fr += [(side(40, crouch=k, ear=1), 110) for k in (1, 2, 3, 4, 4, 4, 3)]
    for k, (x, h) in enumerate(((44, 6), (48, 13), (52, 18), (56, 19), (59, 15))):
        fr.append((side(x, phase=1, lift=h, tail_up=1.6, ear=1), 90))
    fr += [(fox_dive(60, wig=(-1, 1)[i % 2], depth=0 if i else -4), 110) for i in range(18)]
    fr += [(side(60, facing=-1, eyes="content", crouch=2), 300)]
    fr += [(side(60 - 2.5 * k, facing=-1, phase=k % 2), 100) for k in range(8)] + [(S("happy", mouth="lick"), 500), (S("open"), 300)]
    cl["CHASSE"] = (fr, 0)
    # the scratch: a hind leg up to the ear
    fr = [(S("open"), 120)] * 4
    for i in range(40):
        fr.append((S("squeeze", tilt=0.18, leg=(46 + (i % 2), 38 - (i % 2) * 2)), 90))
    fr += [(S("content", tilt=0.1), 200), (S("happy"), 400)]
    cl["CELLO"] = (fr, 0)
    fr = [(S("open"), 120)] * 4 + [(S("content", "lick" if (i // 3) % 2 else "", tilt=0.1 * ((i // 6) % 2)), 110) for i in range(36)] + [(S("happy"), 300)]
    cl["MLEM"] = (fr, 0)
    fr = []
    for s_ in [3, 3, 2, 2, 2, 1, 1, 1, 1, 1, 1]:
        fr += [(side(40, facing=f_, phase=j % 2, eyes="wide", tail_up=1.8), 90) for j in range(s_) for f_ in (1,)]
        fr += [(side(40, facing=-1, phase=j % 2, eyes="wide", tail_up=1.8), 90) for j in range(s_)]
    fr += [(S("squeeze", tilt=(0.15, 0, -0.15, 0)[i % 4]), 90) for i in range(8)] + [(S("happy"), 400)]
    cl["QUEUE"] = (fr, 0)
    # digging a hole in the grass, the earth flying
    fr = [(side(40 + 2.5 * k, phase=k % 2), 100) for k in range(8)]
    fr += [(dirt(side(57.5, crouch=4, phase=i % 2, tail_up=1.4), 46, 58, i), 90) for i in range(40)]
    fr += [(side(57.5, facing=-1, eyes="happy"), 400)] + [(side(57.5 - 2.5 * k, facing=-1, phase=k % 2), 100) for k in range(8)] + [(S("open"), 300)]
    cl["GRIFFES"] = (fr, 0)
    # a trot off the screen and back
    fr = [(S("open", look=-1), 300)] + [(side(37.5 - 5 * k, phase=k % 2, facing=-1), 100) for k in range(13)] + [(C(), 100)] * 14
    fr += [(side(-25 + 5 * k, phase=k % 2, facing=1), 100) for k in range(13)] + [(S("happy"), 400)]
    cl["VIDE"] = (fr, 0)
    seq = [(-15 + 7.5 * k, 1) for k in range(16)] + [(100 - 7.5 * k, -1) for k in range(16)]
    fr = [(gc.dust(side(x, phase=i % 2, facing=f, tail_up=1.6), [x - f * 15, x - f * 19]), 80) for i, (x, f) in enumerate(seq)]
    fr += [(C(), 80)] * 6 + [(side(-20 + 5 * k, phase=k % 2), 80) for k in range(12)] + [(S("happy"), 400)]
    cl["FOLIE"] = (fr, 0)
    fr = [(S("open"), 110)] * 6 + [(S("squeeze", "o", tilt=-0.08, ear=1), 110)] * 12 + [(S("content", tail=math.sin(i / 3)), 110) for i in range(12)]
    cl["BAILLEMENT"] = (fr, 0)
    # DANSE (a step a beat, m1_draw.c): he hops from one side to the other, his tail swinging, his ears up
    fr = []
    for i in range(8):
        k = i % 4
        lean = (-1, 0, 1, 0)[k]
        c = S("happy", tail=lean * 1.2, tilt=0.22 * lean, ear=1 if k % 2 else 0, cx=38 + 3 * lean, base=62 if k % 2 == 0 else 57)
        fr.append((gc.notes(c, i // 2) if k == 1 else c, 150))
    cl["DANSE"] = (fr, 1)
    # CALIN (SEL): ears back, eyes closed, he leans into the hand, the brush sweeps, hearts rise
    fr = [(S("happy"), 200)]
    fr += [(gc.hearts(S("content", tail=math.sin(i / 3), tilt=(0, 0.12, 0.2, 0.12)[(i // 3) % 4], ear=1), 1 + min(i // 5, 3)), 130) for i in range(22)]
    cl["CALIN"] = (fr + [(S("happy"), 600)], 0)
    # JOUER (SEL): a ball bounces in; he follows it with his head, pounces on it, sends it off with his nose
    fr = [(gc.ball(S("wide", look=1, tilt=0.12, ear=1), x, y, "M", "h"), 110) for x, y in ((78, 60), (74, 68), (70, 72), (66, 66), (63, 72))]
    fr += [(gc.ball(side(44, crouch=k, ear=1), 66, 73), 110) for k in (2, 4, 4)]
    fr += [(gc.ball(side(52, phase=1, lift=h, tail_up=1.6, ear=1), 68, 73 - h // 2), 90) for h in (8, 12, 6)]
    fr += [(gc.ball(side(56, crouch=3), x, y), 100) for x, y in ((70, 66), (74, 58), (78, 62), (83, 70))]
    fr += [(side(56, eyes="happy"), 400)] + [(side(56 - 5 * k, facing=-1, phase=k % 2), 90) for k in range(4)] + [(S("happy"), 500)]
    cl["JOUER"] = (fr, 0)
    # CACHE (SEL): off he trots, left; his ears and eyes peek in at the edge; then he bounds back in from the right
    fr = [(S("open", look=-1), 250)] + [(side(37.5 - 5 * k, phase=k % 2, facing=-1), 80) for k in range(13)] + [(C(), 120)] * 6
    fr += [(gc.peek(S("open", look=1, tilt=0.2, ear=1), -40), 200), (gc.peek(S("open", look=1, tilt=0.2, ear=1), -37), 600),
           (gc.peek(S("content", look=1, tilt=0.2), -37), 150), (gc.peek(S("open", look=1, tilt=0.2, ear=1), -37), 500), (C(), 500)]
    fr += [(side(100 - 5 * k, phase=k % 2, facing=-1, tail_up=1.6), 60) for k in range(13)]
    fr += [(S("happy", tail=1), 300), (gc.hearts(S("happy", tail=0.5), 2), 700)]
    cl["CACHE"] = (fr, 0)
    # CADEAU (SEL): off he trots, right, and comes back to lay a stick at your feet; very proud, his brush sweeping
    fr = [(S("open", look=1), 250)] + [(side(37.5 + 5 * k, phase=k % 2), 80) for k in range(13)] + [(C(), 120)] * 8
    fr += [(side(100 - 5 * k, phase=k % 2, facing=-1), 90) for k in range(10)]
    fr += [(stick(S("happy", look=1), 64, 75), 500), (gc.hearts(stick(S("happy", tail=1), 64, 75), 2), 600),
           (gc.hearts(stick(S("happy", tail=-1), 64, 75), 3), 600), (stick(S("open", look=1), 64, 75), 500)]
    cl["CADEAU"] = (fr, 0)
    # PASSE (the tape plays): he trots in, stops, looks up at the reels, wonders, trots on out (poses he has already)
    fr = [(side(-25 + 5 * k, phase=k % 2), 110) for k in range(13)] + [(S("open"), 300)]
    fr += [(S("open", look=-1, tilt=-0.12, ear=1), 120), (S("open", look=-1, tilt=-0.12), 1600),
           (gc.quest(S("open", look=-1, tilt=-0.12)), 900), (S("open", look=1, tilt=0.12), 1400), (S("happy"), 600)]
    fr += [(side(37.5 + 5 * k, phase=k % 2), 110) for k in range(13)]
    cl["PASSE"] = (fr, 0)
    alias = {"TAPOTE": "CHASSE", "TOILETTE": "MLEM", "TERRITOIRE": "MLEM", "ROULADE": "QUEUE"}
    return cl, alias


# ================================================================== the robot (a little one from Modular Town)
BOT_PAL = [None, "#1e2228", "#b8c3cb", "#8592a0", "#e6eef2", "#1d2b33", "#6ff3e3", "#ff5a4a", "#ffd65a", "#e07a3a",
           "#5b656e", "#ffffff", "#cfc8ff", "#2c6e6a", "#f27c9a", "#3a434b"]
BOT_KEY = dict(o=1, K=2, D=3, L=4, S=5, C=6, R=7, Y=8, O=9, G=10, H=11, z=12, n=13, h=14, A=15,
               b=8, w=11, F=3, P=14, M=7, E=5, N=10)


def box(c, x0, y0, x1, y1, k="K", cut=True):
    """a rectangle in design units (its corners cut: a rounded box)"""
    X0, Y0 = T(x0, y0); X1, Y1 = T(x1, y1)
    X0, Y0, X1, Y1 = int(round(X0)), int(round(Y0)), int(round(X1)), int(round(Y1))
    for y in range(Y0, Y1):
        for x in range(X0, X1):
            if cut and (x in (X0, X1 - 1)) and (y in (Y0, Y1 - 1)): continue
            c.m[(x, y)] = k


def bot(eyes="open", look=0, lamp="Y", arms=(0, 0), bob=0, ant=0, spin=0, wheel=0, cx=40, base=62, hand=None, head_w=10):
    """front view: a wheel, a box body with its lights, two arms, a box head with a screen face, an antenna"""
    c = C(); by = base - bob
    c.ell(cx, base - 4.5, 4.5, 4.5)                                  # the wheel (on the floor, it does not bob)
    box(c, cx - 3, by - 9, cx + 3, by - 6)                          # the fork
    box(c, cx - 8, by - 21, cx + 8, by - 8)                         # the body
    for sx, a in ((-1, arms[0]), (1, arms[1])):                     # the arms: a = 0 down, 1 out, 2 up
        sh = (cx + sx * 8, by - 18)
        hd = [(cx + sx * 12, by - 10), (cx + sx * 15, by - 16), (cx + sx * 13, by - 26)][a] if not (hand and sx == 1) else hand
        c.stroke([sh, hd], 1.1); c.ell(hd[0], hd[1], 1.9, 1.9)
    box(c, cx - 2, by - 23, cx + 2, by - 20)                        # the neck
    hw = head_w
    box(c, cx - hw, by - 37, cx + hw, by - 22)                      # the head
    if hw >= 8:
        c.ell(cx - hw - 0.5, by - 30, 1.4, 1.8); c.ell(cx + hw + 0.5, by - 30, 1.4, 1.8)   # the bolts
    ax = cx + ant
    c.stroke([(cx, by - 37), (ax, by - 42)], 0.6); c.ell(ax, by - 43, 1.7, 1.7)
    c.finish()
    m = C(); m.ell(cx, base - 4.5, 4.5, 4.5); within(c, m, "A")
    m = C(); m.ell(cx, base - 4.5, 1.5, 1.5); within(c, m, "G")
    X, Y = T(cx, base - 4.5)
    for k_, (dx, dy) in enumerate(((0, -3), (0, 3), (-3, 0), (3, 0)) if wheel % 2 == 0 else ((-2, -2), (2, 2), (-2, 2), (2, -2))):
        c.raw(X + dx, Y + dy, "D")
    m = C(); box(m, cx - 6, by - 18, cx + 6, by - 11); within(c, m, "D")
    for k_, (lx, col) in enumerate(((-3.5, "R"), (0, "Y"), (3.5, "C"))):
        c.pat(cx + lx, by - 16, [(0, 0), (1, 0)], col if (bob + k_) % 3 else "G")
    c.pat(cx - 3, by - 13, [(i, 0) for i in range(8)], "G")
    m = C(); box(m, cx - 2, by - 23, cx + 2, by - 20, cut=False); within(c, m, "G")
    if hw >= 6:                                                     # the screen and its face
        m = C(); box(m, cx - hw + 3, by - 34, cx + hw - 3, by - 24); within(c, m, "S")
        if hw >= 9:
            ey = by - 29.5
            for sx in (-1, 1):
                x = cx + sx * 3.5
                if eyes == "open": c.pat(x, ey, [(dx + look, dy) for dx in (-1, 0) for dy in (-1, 0, 1)], "C")
                elif eyes == "wide": c.pat(x, ey, [(dx + look, dy) for dx in (-1, 0, 1) for dy in (-2, -1, 0, 1)], "C")
                elif eyes == "happy": c.pat(x, ey, [(-2, 1), (-1, 0), (0, -1), (1, 0), (2, 1)], "C")
                elif eyes == "half": c.pat(x, ey, [(-1, 0), (0, 0), (-1, 1), (0, 1)], "C")
                elif eyes == "off": c.pat(x, ey, [(-1, 1), (0, 1), (1, 1)], "G")
                elif eyes == "heart": c.pat(x, ey, [(-1, -1), (1, -1), (-2, 0), (-1, 0), (0, 0), (1, 0), (2, 0), (-1, 1), (0, 1), (1, 1), (0, 2)], "h")
                elif eyes == "spiral": c.pat(x, ey, [(-1, -1), (0, -1), (1, -1), (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0)][:(look % 8) + 1], "C")
                elif eyes == "dots": c.pat(x, ey, [(0, 0)], "C" if (look + (sx > 0)) % 2 else "G")
            if eyes in ("open", "wide", "happy", "heart"):
                c.pat(cx, by - 26, [(-1, 0), (0, 0), (1, 0)], "C")
        else:
            c.pat(cx + (2 if spin > 0 else -2), by - 29.5, [(0, -1), (0, 0), (0, 1)], "C")
    X, Y = T(ax, by - 43); m = C(); m.ell(ax, by - 43, 1.7, 1.7); within(c, m, lamp)
    return c


def battery(c, n, x=56, y=20):
    """a little battery icon: n bars of 3"""
    c.pat(x, y, [(i, 0) for i in range(8)] + [(i, 4) for i in range(8)] + [(0, j) for j in range(5)] + [(7, j) for j in range(5)] + [(8, 1), (8, 2), (8, 3)], "o", True)
    for b_ in range(n): c.pat(x, y, [(1 + b_ * 2 + dx, j) for dx in (0, 1) for j in (1, 2, 3)], "C" if n > 1 else "R", True)
    return c


def waves(c, n, x=40, y=14):
    for k in range(n):
        r = 2 + k * 2
        c.pat(x, y, [(-r, -k), (-r - 1, -k - 1), (r, -k), (r + 1, -k - 1)], "C", True)
    return c


def oilcan(c, k, x=52, y=28.5):
    """the can in his raised hand, its spout to his head, a drop now and then"""
    c.pat(x, y, [(dx, dy) for dx in (0, 1, 2, 3) for dy in (0, 1, 2, 3)], "O")
    c.pat(x, y, [(-1, 0), (-2, -1), (-3, -1), (1, -1), (2, -1)], "G")
    c.pat(x, y, [(4, 1), (4, 2)], "G")
    if k % 4 == 1: c.pat(x, y, [(-4, 0)], "Y", True)
    if k % 4 == 2: c.pat(x, y, [(-4, 2)], "Y", True)
    if k % 4 == 3: c.pat(x, y, [(-4, 4)], "Y", True)
    return c


def bot_clips():
    B = bot
    cl = {}
    fr = []
    for i in range(48):
        lamp = "Y" if (i // 4) % 2 else "O"
        e = "open"; lk = 0
        if i in (12, 13, 36): e = "off"
        if 20 <= i < 26: lk = -1
        if 26 <= i < 32: lk = 1
        fr.append((B(e, look=lk, lamp=lamp, bob=0), 140))
    cl["IDLE"] = (fr, 1)
    cl["LOOK_L"] = ([(B("wide", look=-1, ant=-2), 150), (B("open", look=-1, ant=-1), 600), (gc.quest(B("open", look=-1, ant=-1)), 900)], 0)
    cl["LOOK_R"] = ([(B("wide", look=1, ant=2), 150), (B("open", look=1, ant=1), 1500)], 0)
    cl["PLAY"] = ([(gc.notes(B("happy", arms=((2, 0), (1, 1), (0, 2), (1, 1))[i % 4], bob=i % 2, ant=(-1, 0, 1, 0)[i % 4], lamp="Y"), i // 4), 120) for i in range(8)], 1)
    cl["SLEEP"] = ([(gc.zzz(B("off", lamp="R" if i % 10 < 2 else "G", ant=0), 1 + (i // 4) % 3), 170) for i in range(20)], 1)
    cl["WAKE"] = ([(B("off", lamp="G"), 200), (B("dots", look=0, lamp="O"), 150), (B("off", lamp="G"), 100), (B("wide", lamp="Y"), 250), (B("open", lamp="Y"), 400)], 0)
    fr = [(B("half", lamp="O"), 200)] * 3 + [(battery(B("half", lamp="O"), 1), 300), (battery(B("half", lamp="R"), 1), 300)] * 3
    fr += [(B("dots", look=i, lamp="R"), 200) for i in range(8)] + [(gc.zzz(B("off", lamp="G"), 1 + i // 4), 160) for i in range(10)]
    cl["ENDORT"] = (fr, 0)
    fr = [(battery(B("off", lamp="G"), n), 500) for n in (1, 2, 3, 3, 2, 3)] + [(B("off", lamp="G"), 300)]
    cl["PETRIR"] = (fr, 0)
    fr = [(B("open", lamp="Y"), 200)] + [(battery(B("half", lamp="R" if i % 2 else "O"), 1), 250) for i in range(10)] + [(B("open", lamp="Y"), 300)]
    cl["BAILLEMENT"] = (fr, 0)
    # the head spins round and round
    fr = []
    seq = [10, 7, 3, 1, 3, 7] * 4 + [10]
    for k, w in enumerate(seq):
        fr.append((B("spiral" if w >= 9 else "open", look=k, head_w=max(w, 1), spin=(1 if k % 6 < 3 else -1), lamp="Y"), 80))
    fr += [(B("spiral", look=i, lamp="O"), 90) for i in range(8)] + [(B("happy", lamp="Y"), 500)]
    cl["QUEUE"] = (fr, 0)
    # a drop of oil on the shoulder
    fr = [(B("open"), 200)] + [(oilcan(B("happy", hand=(52, 33), arms=(0, 0)), i), 160) for i in range(24)] + [(B("happy"), 400)]
    cl["CELLO"] = (fr, 0)
    fr = [(B("open"), 200)] + [(gc.hearts(B("heart", lamp="Y", bob=i % 2), 1 + i // 8), 150) for i in range(24)] + [(B("happy"), 400)]
    cl["MLEM"] = (fr, 0)
    # the radio: its antenna picks up a tune
    fr = [(waves(B("open", look=0, ant=(0, 1, 0, -1)[i % 4], lamp="Y"), 1 + i % 3), 150) for i in range(24)] + [(gc.notes(B("happy", lamp="Y"), 0), 600)]
    cl["GRIFFES"] = (fr, 0)
    # a bug: he follows it with his eyes, his arm stretches out and catches it
    fr, path = [], [(60, 30), (63, 27), (66, 30), (63, 33), (58, 31), (55, 26)]
    for i in range(18):
        bx, by_ = path[(i // 3) % len(path)]
        fr.append((gc.bug(B("open", look=1 if bx > 58 else 0, ant=1), bx, by_, i), 110))
    for k, h in enumerate(((50, 30), (54, 28), (58, 27), (60, 27))):
        fr.append((gc.bug(B("wide", look=1, hand=h, ant=1), 60, 27, k), 100))
    fr += [(B("happy", hand=(50, 30)), 300), (B("happy", arms=(0, 0)), 500)]
    cl["CHASSE"] = (fr, 0)
    # off he rolls, and back
    fr = [(B("open", look=-1), 300)] + [(B("open", look=-1, wheel=k, ant=1), 100) for k in range(1)]
    fr += [(moved(B("open", look=-1, wheel=k % 2, ant=1), -5 * k * 6 // 5 * 1), 100) for k in range(14)] + [(C(), 100)] * 12
    fr += [(moved(B("open", look=1, wheel=k % 2, ant=-1), -84 + 6 * k), 100) for k in range(15)] + [(B("happy"), 400)]
    cl["VIDE"] = (fr, 0)
    fr = [(gc.dust(moved(B("wide", look=1, wheel=k % 2, ant=-2), -60 + 9 * k), [34 - 60 * 5 // 6 + 7.5 * k - 12]), 70) for k in range(17)]
    fr += [(gc.dust(moved(B("wide", look=-1, wheel=k % 2, ant=2), 90 - 9 * k), [46 + 75 - 7.5 * k + 12]), 70) for k in range(17)]
    fr += [(C(), 80)] * 6 + [(moved(B("happy", wheel=k % 2), -60 + 6 * k), 80) for k in range(11)] + [(B("happy"), 400)]
    cl["FOLIE"] = (fr, 0)
    # DANSE (a step a beat, m1_draw.c): an arm up, then the other, both; he bobs, his antenna sways, his lamp blinks
    fr = []
    for i in range(8):
        arms = ((2, 0), (1, 1), (0, 2), (2, 2))[i % 4]
        c = B("happy", arms=arms, bob=i % 2, ant=(-2, 0, 2, 0)[i % 4], lamp=("Y", "O", "R", "Y")[i % 4])
        fr.append((gc.notes(c, i // 2) if i % 4 == 3 else c, 150))
    cl["DANSE"] = (fr, 1)
    # JOUER (SEL): he juggles a ball over his head, hand to hand, then heads it away
    fr = []
    for k, (bx, by_) in enumerate(((52, 30), (49, 20), (44, 15), (39, 20), (36, 30), (39, 20), (44, 15), (49, 20), (52, 30))):
        arms = (2, 0) if bx >= 49 else (0, 2) if bx <= 39 else (1, 1)
        fr.append((gc.ball(B("happy" if k % 4 else "wide", look=1 if bx > 44 else -1 if bx < 44 else 0, arms=arms, bob=k % 2, lamp="Y"), bx, by_, "O", "Y"), 110))
    fr += [(gc.ball(B("wide", arms=(1, 1), bob=1), x, y, "O", "Y"), 100) for x, y in ((44, 18), (52, 10), (62, 6), (72, 8), (82, 14))]
    fr += [(B("happy", lamp="Y"), 500)]
    cl["JOUER"] = (fr, 0)
    # CACHE (SEL): off he rolls, left; his head and antenna peek in at the edge; then he races back in from the right
    fr = [(B("open", look=-1), 250)] + [(moved(B("open", look=-1, wheel=k % 2, ant=1), -6 * k), 80) for k in range(12)] + [(C(), 120)] * 6
    fr += [(gc.peek(B("open", look=1, ant=-1), -40), 200), (gc.peek(B("open", look=1, ant=-1), -36), 600),
           (gc.peek(B("off", look=1), -36), 150), (gc.peek(B("open", look=1, ant=-1), -36), 500), (C(), 500)]
    fr += [(moved(B("wide", look=-1, wheel=k % 2, ant=2), 60 - 6 * k), 60) for k in range(11)]
    fr += [(B("happy", lamp="Y"), 300), (gc.hearts(B("heart", lamp="Y"), 2), 700)]
    cl["CACHE"] = (fr, 0)
    # CADEAU (SEL): off he rolls, right, and comes back with a flower for you, held out in his hand
    fr = [(B("open", look=1), 250)] + [(moved(B("open", look=1, wheel=k % 2, ant=-1), 6 * k), 80) for k in range(12)] + [(C(), 120)] * 8
    fr += [(moved(B("open", look=-1, wheel=k % 2, ant=1), 60 - 6 * k), 90) for k in range(11)]
    hf = lambda e, n=0: gc.hearts(gc.flower(B(e, hand=(52, 33), lamp="Y"), int(T(53, 31)[0]), int(T(53, 31)[1]), "P", "Y"), n) if n else \
                        gc.flower(B(e, hand=(52, 33), lamp="Y"), int(T(53, 31)[0]), int(T(53, 31)[1]), "P", "Y")
    fr += [(hf("happy"), 600), (hf("heart", 2), 600), (hf("heart", 3), 600), (hf("happy"), 400)]
    cl["CADEAU"] = (fr, 0)
    # PASSE (the tape plays): he rolls in, stops, looks up at the reels, wonders, rolls on out
    fr = [(moved(B("open", look=1, wheel=k % 2, ant=-1), -66 + 6 * k), 110) for k in range(11)] + [(B("open"), 300)]
    fr += [(B("wide", look=-1, ant=-2), 150), (B("open", look=-1, ant=-1), 1600), (gc.quest(B("open", look=-1, ant=-1)), 900),
           (B("wide", look=1, ant=2), 150), (B("open", look=1, ant=1), 1400), (B("happy", lamp="Y"), 600)]
    fr += [(moved(B("open", look=1, wheel=k % 2, ant=-1), 6 * k), 110) for k in range(12)]
    cl["PASSE"] = (fr, 0)
    alias = {"TAPOTE": "CHASSE", "TOILETTE": "CELLO", "TERRITOIRE": "MLEM", "ROULADE": "QUEUE", "CALIN": "MLEM"}
    return cl, alias


# ================================================================== the fish (the screen is his aquarium: m1_draw.c draws the
# water, the sand and the weed behind him when the companion says water)
FISH_PAL = [None, "#7a3412", "#f47a24", "#c85814", "#ffb15e", "#ffd08a", "#1b1410", "#ffffff", "#9fd3e6", "#7fbcd6",
            "#4f7389", "#c9a27a", "#9b7650", "#4e9a58", "#f27c9a", "#f4a3b6"]
FISH_KEY = dict(o=1, K=2, D=3, L=4, F=5, E=6, H=7, W=8, A=9, g=10, R=11, r=12, P=13, h=14, B=15,
                z=7, n=10, b=5, w=7, M=14, N=1, Y=5)
FS = 1.5                                                         # his size (cells: he is drawn in cells)


def fish_side(c, x, y, f=1, ph=0, eyes="open", mouth=""):
    """a goldfish seen from the side, its middle at cell x, y, f 1 to the right"""
    d = C(); k = FS
    d._ell(x, y, 5.5 * k, 3.7 * k, "K")
    tx = x - f * int(5.5 * k); w = (0, 1, 0, -1)[ph % 4]
    for i in range(int(6 * k)):
        for j in range(-2 - int(i / 1.6) + w, 3 + int(i / 1.6) + w):
            d.m[(tx - f * i, y + j)] = "F"
    for i in range(int(5 * k)): d.m[(x - f * (2 - i), y - int(3.7 * k) - (1 if 1 <= i <= 4 else 0))] = "F"
    for i in range(int(4 * k)): d.m[(x - f * (i - 1), y + int(3.7 * k))] = "F"
    d.finish()
    ex, ey = x + f * int(3 * k), y - 2
    if eyes == "open":
        for dx in (0, f): d.m[(ex + dx, ey)] = "E"; d.m[(ex + dx, ey + 1)] = "E"
        d.m[(ex + f, ey)] = "H"
    elif eyes in ("content", "half"):
        d.m[(ex - 1, ey)] = "o"; d.m[(ex, ey + 1)] = "o"; d.m[(ex + 1, ey)] = "o"
        if eyes == "half": d.m[(ex, ey)] = "E"
    mx = x + f * int(5.5 * k)
    if mouth == "o":
        for dy in (0, 1): d.m[(mx, y + 1 + dy)] = "h"; d.m[(mx + f, y + 1 + dy)] = "h"
    else: d.m[(mx - f, y + 2)] = "o"
    d.m[(ex - f, y + 2)] = "B"
    return over(c, d)


def fish_front(c, x, y, eyes="open", mouth="", fin=0, k=FS):
    d = C()
    d._ell(x, y, 4.4 * k, 4.6 * k, "K")
    for sx in (-1, 1):
        for j in range(int(2 * k)): d.m[(x + sx * (int(4.4 * k) + 1 + fin), y + j)] = "F"; d.m[(x + sx * (int(4.4 * k) + 2 + fin), y + j)] = "F"
    for j in range(int(3 * k)): d.m[(x - 1 + j - int(k / 2), y - int(4.6 * k) - 1)] = "F"
    d.finish()
    for sx in (-1, 1):
        ex, ey = x + sx * int(2.2 * k), y - 1
        if eyes == "open":
            for dx in (0, 1):
                for dy in (0, 1, 2): d.m[(ex + dx - (sx < 0), ey + dy)] = "E"
            d.m[(ex - (sx < 0), ey)] = "H"
        elif eyes == "happy": d.m[(ex - 1, ey + 1)] = "o"; d.m[(ex, ey)] = "o"; d.m[(ex + 1, ey + 1)] = "o"
        elif eyes == "squeeze": d.m[(ex, ey)] = "o"; d.m[(ex - sx, ey + 1)] = "o"; d.m[(ex, ey + 2)] = "o"
        elif eyes == "content": d.m[(ex - 1, ey + 1)] = "o"; d.m[(ex, ey + 2)] = "o"; d.m[(ex + 1, ey + 1)] = "o"
        d.m[(x + sx * int(3.4 * k), y + 3)] = "B"; d.m[(x + sx * int(3.4 * k) - (sx < 0), y + 3)] = "B"
    my = y + int(2.6 * k)
    if mouth == "o":
        for dx in (-1, 0, 1):
            for dy in (0, 1): d.m[(x + dx, my + dy)] = "h"
    elif mouth == "kiss":
        d.m[(x, my)] = "h"; d.m[(x - 1, my)] = "h"; d.m[(x, my + 1)] = "h"; d.m[(x - 1, my + 1)] = "h"
    else: d.m[(x - 1, my)] = "o"; d.m[(x, my)] = "o"
    return over(c, d)


def bubbles(c, pts):
    for x, y in pts:
        for dx, dy in ((0, -1), (1, 0), (0, 1), (-1, 0)): c.m.setdefault((x + dx, y + dy), "H")
    return c


def fish_clips():
    cl = {}
    W = lambda: C()
    fr = []                                                      # a slow swim about the screen
    for i in range(40):
        a = i / 40 * math.tau
        x, y = 40 + int(round(math.sin(a) * 18)), 52 + int(round(math.sin(2 * a) * 6))
        f = 1 if math.cos(a) > 0 else -1
        c = fish_side(W(), x, y, f, i // 2)
        if i % 20 in (5, 6, 7, 8): bubbles(c, [(x + f * 11, y - 4 - (i % 20 - 5) * 4)])
        fr.append((c, 200))
    cl["IDLE"] = (fr, 1)
    cl["LOOK_L"] = ([(fish_side(W(), 40 - k * 3, 52, -1, k), 120) for k in range(5)] + [(fish_front(W(), 26, 52, "open"), 600), (gc.quest(fish_front(W(), 26, 52, "open")), 900)], 0)
    cl["LOOK_R"] = ([(fish_side(W(), 40 + k * 3, 52, 1, k), 120) for k in range(5)] + [(fish_side(W(), 54, 52, 1, 0), 1500)], 0)
    cl["PLAY"] = ([(gc.notes(fish_front(W(), 40, 51 + (i % 2), "happy", fin=i % 2), i // 3), 120) for i in range(6)], 1)
    cl["SLEEP"] = ([(gc.zzz(fish_side(W(), 44, 66 + ((i // 6) % 2), 1, 0, "content"), 1 + (i // 4) % 3), 180) for i in range(24)], 1)
    cl["WAKE"] = ([(fish_side(W(), 44, 66, 1, 0, "content"), 250), (fish_side(W(), 44, 64, 1, 1, "half"), 200), (fish_side(W(), 44, 58, 1, 2), 150), (fish_front(W(), 40, 52, "open"), 400)], 0)
    fr = [(fish_side(W(), 40 + (k % 3) - 1, 52 + k, 1, k // 2, "open" if k < 8 else "half"), 200) for k in range(15)]
    fr += [(gc.zzz(fish_side(W(), 44, 66, 1, 0, "content"), 1 + i // 4), 180) for i in range(10)]
    cl["ENDORT"] = (fr, 0)
    cl["PETRIR"] = ([(fish_side(W(), 30 + k * 2, 56 + k, 1, k // 2, "half"), 260) for k in range(8)] + [(fish_side(W(), 44, 66, 1, 0, "content"), 400)], 0)
    cl["BAILLEMENT"] = ([(fish_front(W(), 40, 52, "open"), 300), (fish_front(W(), 40, 52, "squeeze", "o"), 900), (fish_front(W(), 40, 52, "content"), 600)], 0)
    fr = []                                                      # bubbles, one after the other, up and away
    for i in range(30):
        pts = [(40, 44 - ((i - k * 5) % 15) * 2) for k in range(3) if i >= k * 5]
        fr.append((bubbles(fish_front(W(), 40, 52, "open", "o" if i % 5 < 2 else ""), [p for p in pts if p[1] < 42]), 140))
    cl["MLEM"] = (fr + [(fish_front(W(), 40, 52, "happy"), 500)], 0)
    fr = []                                                      # a flake of food sinks; he darts and gulps it
    for i in range(16):
        c = fish_side(W(), 26 + min(i, 10), 58 - min(i, 10) // 2, 1, i)
        fx, fy = 52 - (i // 2) % 2, 14 + i * 2
        c.m[(fx, fy)] = "F"; c.m[(fx + 1, fy)] = "F"; c.m[(fx, fy + 1)] = "F"
        fr.append((c, 120))
    fr += [(fish_side(W(), 44, 50, 1, 0, "open", "o"), 250), (fish_front(W(), 46, 50, "happy"), 700), (fish_front(W(), 40, 52, "happy"), 300)]
    cl["CHASSE"] = (fr, 0)
    fr = []                                                      # la folie: a zigzag dash, right across and back
    for k in range(12): fr.append((fish_side(W(), -10 + k * 9, 44 + (k % 2) * 8, 1, k), 70))
    fr += [(C(), 80)] * 4
    for k in range(12): fr.append((fish_side(W(), 92 - k * 9, 56 - (k % 2) * 8, -1, k), 70))
    fr += [(C(), 80)] * 4 + [(fish_side(W(), -10 + k * 5, 52, 1, k), 90) for k in range(10)] + [(fish_front(W(), 40, 52, "happy"), 500)]
    cl["FOLIE"] = (fr, 0)
    fr = []                                                      # a loop
    for i in range(24):
        a = i / 24 * math.tau
        x, y = 40 + int(round(math.cos(a) * 12)), 50 + int(round(math.sin(a) * 10))
        fr.append((fish_side(W(), x, y, -1 if math.sin(a) > 0 else 1, i), 100))
    cl["ROULADE"] = (fr + [(fish_front(W(), 40, 52, "happy"), 400)], 0)
    fr = [(fish_side(W(), 40 - k * 6, 52, -1, k), 110) for k in range(10)] + [(C(), 100)] * 16   # off he swims, and back
    fr += [(fish_side(W(), -14 + k * 6, 50, 1, k), 110) for k in range(10)] + [(fish_front(W(), 46, 50, "happy"), 400)]
    cl["VIDE"] = (fr, 0)
    fr = [(fish_front(W(), 40, 52, "open"), 300)]                # close to the glass (the screen): a kiss
    fr += [(fish_front(W(), 40, 48, "open", k=FS * 1.3), 200), (fish_front(W(), 40, 46, "content", "kiss", k=FS * 1.6), 300)]
    fr += [(gc.hearts(fish_front(W(), 40, 46, "content", "kiss", k=FS * 1.6), 1 + i // 6), 150) for i in range(18)] + [(fish_front(W(), 40, 52, "happy"), 400)]
    cl["TERRITOIRE"] = (fr, 0)
    fr = []                                                      # DANSE (a step a beat): little rounds, a bubble each
    for i in range(8):
        a = i / 8 * math.tau
        x, y = 40 + int(round(math.cos(a) * 10)), 50 + int(round(math.sin(a) * 7))
        f = -1 if math.sin(a) > 0 else 1
        c = fish_side(W(), x, y, f, i, "happy")
        fr.append((bubbles(c, [(x + f * 11, y - 5), (x + f * 13, y - 11)]) if i % 2 else c, 150))
    cl["DANSE"] = (fr, 1)
    fr = []                                                      # JOUER (SEL): a ball of air; he bumps it up with his nose, again
    for k, (fy, by_) in enumerate(((58, 40), (54, 36), (50, 40), (47, 37), (47, 30), (50, 26), (54, 30), (50, 36), (47, 33), (47, 26), (52, 20), (56, 14))):
        fr.append((gc.ball(fish_front(W(), 40, fy, "happy" if k % 3 == 2 else "open"), 40, by_, "W", "H"), 110))
    fr += [(fish_front(W(), 40, 52, "happy"), 500)]
    cl["JOUER"] = (fr, 0)
    fr = [(fish_side(W(), 40 - k * 6, 52, -1, k), 90) for k in range(10)] + [(C(), 120)] * 6   # CACHE (SEL): off he swims,
    fr += [(fish_side(W(), -6, 50, 1, 0), 200), (fish_side(W(), -3, 50, 1, 1), 600), (fish_side(W(), -3, 50, 1, 2, "content"), 150),
           (fish_side(W(), -3, 50, 1, 3), 500), (C(), 500)]                                          # .. peeks in, ..
    fr += [(fish_side(W(), 94 - k * 6, 52, -1, k), 60) for k in range(10)]                         # .. dashes back
    fr += [(fish_front(W(), 40, 52, "happy"), 300), (gc.hearts(fish_front(W(), 40, 52, "happy"), 2), 700)]
    cl["CACHE"] = (fr, 0)
    fr = [(fish_side(W(), 40 + k * 2, 52 + k * 2, 1, k), 120) for k in range(8)]                  # CADEAU (SEL): he dives to
    fr += [(fish_side(W(), 56, 68, 1, k % 4, "content"), 140) for k in range(6)]                    # the sand, digs, and comes
    fr += [(pearl(fish_side(W(), 56 - k * 2, 66 - k * 2, -1, k), 60, 74), 120) for k in range(6)]  # up: a pearl, for you
    fr += [(pearl(fish_front(W(), 42, 52, "happy"), 60, 74), 500), (gc.hearts(pearl(fish_front(W(), 42, 52, "happy"), 60, 74), 2), 600),
           (gc.hearts(pearl(fish_front(W(), 42, 52, "happy", fin=1), 60, 74), 3), 600)]
    cl["CADEAU"] = (fr, 0)
    fr = [(fish_side(W(), -14 + k * 6, 50, 1, k), 110) for k in range(10)]                        # PASSE (the tape plays):
    fr += [(fish_front(W(), 46, 50, "happy"), 300), (fish_front(W(), 40, 52, "open"), 1600),        # he swims in, stops,
           (gc.quest(fish_front(W(), 26, 52, "open")), 900), (fish_side(W(), 54, 52, 1, 0), 1400)]  # looks at the reels,
    fr += [(fish_side(W(), 54 + k * 6, 52, 1, k), 110) for k in range(7)]                          # swims on out
    cl["PASSE"] = (fr, 0)
    alias = {"TAPOTE": "CHASSE", "TOILETTE": "MLEM", "CELLO": "ROULADE", "QUEUE": "ROULADE", "GRIFFES": "VIDE", "CALIN": "TERRITOIRE"}
    return cl, alias


# ================================================================== registry
PETS = [                                                         # name, palette, keys, clips, water (the screen is his aquarium)
    ("FOX", FOX_PAL, FOX_KEY, fox_clips, 0),
    ("ROBOT", BOT_PAL, BOT_KEY, bot_clips, 0),
    ("FISH", FISH_PAL, FISH_KEY, fish_clips, 1),
]


def grid(c, key):
    return tuple(key[c.m[(x, y)]] if (x, y) in c.m and 0 <= x < G and 0 <= y < SCENE else 0 for y in range(SCENE) for x in range(G))


def canon(c, key):
    """the pose without its horizontal place (all of it, even off the screen) and its first column"""
    cells = [(x, y, key[k]) for (x, y), k in c.m.items() if 0 <= y < SCENE]
    if not cells: return (), 0, 0
    x0 = min(p[0] for p in cells); x1 = max(p[0] for p in cells)
    return frozenset((x - x0, y, k) for x, y, k in cells), x0, x1 - x0 + 1


def build(name, pal, key, clips_fn):
    """-> (grids, seqs [frame, d10, dx], clips by slot): as gen_cat.main; a picture that is another one moved sideways
    is that one with a dx (the walks, the runs: two poses, not thirty)"""
    cl, alias = clips_fn()
    grids, index, seqs, made = [], {}, [], {}
    for n in SLOTS:
        if n not in cl: continue
        fr, loop = cl[n]; start = len(seqs)
        for c, ms in fr:
            k, x0, w = canon(c, key)
            if w > G:                                   # (wider than the screen: as it is, no reuse)
                k = (grid(c, key),)
            if k not in index:
                gx = x0 if w > G else min(max(x0, 0), G - w)   # kept whole: moved onto the screen if it has to be
                index[k] = (len(grids), gx); grids.append(grid(c if gx == x0 else moved(c, gx - x0), key))
            gi, gx = index[k]; dx = x0 - gx
            if len(seqs) > start and seqs[-1][0] == gi and seqs[-1][2] == dx and seqs[-1][1] + ms // 10 < 255: seqs[-1][1] += ms // 10
            else: seqs.append([gi, ms // 10, dx])
        made[n] = (start, len(seqs) - start, loop)
    for n in SLOTS:
        if n not in made:
            made[n] = made[alias.get(n, "IDLE")]
    return grids, seqs, made


def pack(grids):
    zero = (0,) * (G * SCENE); frames, keys = [], []
    for i, g in enumerate(grids):
        best, bb = gc.encode(g, zero), 0xFFFF
        for k in keys[-48:]:
            d = gc.encode(g, grids[k])
            if len(d) + 8 < len(best): best, bb = d, k
        if bb == 0xFFFF: keys.append(i)
        frames.append((best, bb, gc.bbox(g)))
    data, offs = bytearray(), []
    for d, _, _ in frames: offs.append(len(data)); data += d
    for (d, bb, _), g in zip(frames, grids): assert gc.decode(d, zero if bb == 0xFFFF else grids[bb]) == list(g)
    return frames, offs, data, keys


def preview(dirn, name, pal, grids, seqs, made, water=0):
    from PIL import Image
    os.makedirs(dirn, exist_ok=True)
    cols = [(0, 0, 0)] + [tuple(int(p[i:i + 2], 16) for i in (1, 3, 5)) for p in pal[1:]]
    done = set()
    for n in SLOTS:
        s, k, lp = made[n]
        if (s, k) in done: continue
        done.add((s, k)); ims, durs = [], []
        for st in seqs[s:s + k]:
            g = grids[st[0]]; dx = st[2]
            g = [g[i - dx] if 0 <= (i % G) - dx < G else 0 for i in range(G * SCENE)]
            im = Image.new("RGB", (G * 3, SCENE * 3), (0x7e, 0xc3, 0xd8) if water else (0xb0, 0x7a, 0x42)); px = im.load()
            for i, v in enumerate(g):
                if v:
                    for a in range(3):
                        for b in range(3): px[(i % G) * 3 + a, (i // G) * 3 + b] = cols[v]
            ims.append(im); durs.append(st[1] * 10)
        ims[0].save(os.path.join(dirn, "%s_%s.gif" % (name.lower(), n.lower())), save_all=True, append_images=ims[1:], duration=durs, loop=0)


def emit(f, P, pal, frames, offs, data, seqs, made):
    f.write("static const uint16_t %s_PAL[16] = {%s};\n" % (P, ", ".join("0x%04X" % (gc.rgb565(p) if p else 0) for p in pal)))
    f.write("static const uint8_t %s_DATA[%d] = {\n" % (P, len(data)))
    for i in range(0, len(data), 24): f.write("    " + ", ".join(str(b) for b in data[i:i + 24]) + ",\n")
    f.write("};\nstatic const cat_frame_t %s_FRAMES[%d] = {\n" % (P, len(frames)))
    for o, (d, bb, (x, y, w, h)) in zip(offs, frames): f.write("    {%d, %d, %d, %d, %d, %d, %d},\n" % (o, len(d), bb, x, y, w, h))
    f.write("};\nstatic const cat_step_t %s_STEPS[%d] = {\n" % (P, len(seqs)))
    for i in range(0, len(seqs), 8): f.write("    " + " ".join("{%d, %d, %d}," % (s[0], s[1], s[2]) for s in seqs[i:i + 8]) + "\n")
    f.write("};\nstatic const cat_clip_t %s_CLIPS[CAT_NCLIPS] = {\n" % P)
    for n in SLOTS:
        s, k, lp = made[n]; f.write("    {%d, %d, %d},   /* %s */\n" % (s, k, lp, n))
    f.write("};\n")


def cat_clips():
    return {n: (fr, lp) for n, lp, fr in gc.CLIPS}, {}


ALL = [("CAT", gc.PAL, gc.KEY, cat_clips, 0)] + PETS


def main():
    a = sys.argv[1:]
    out = a[a.index("-o") + 1] if "-o" in a else "build/gen/cat_sprites.h"
    prev = a[a.index("--preview") + 1] if "--preview" in a else None
    only = a[a.index("--only") + 1].upper() if "--only" in a else None
    pets = [p for p in ALL if not only or p[0] == only]
    built = []
    total = 0
    for name, pal, key, fn, water in pets:
        grids, seqs, made = build(name, pal, key, fn)
        frames, offs, data, keys = pack(grids)
        n = len(data) + len(frames) * 12 + len(seqs) * 4
        total += n
        print("%s: %d steps, %d frames (%d keys), %d bytes of pixels, ~%d bytes in all" % (name.lower(), len(seqs), len(frames), len(keys), len(data), n))
        if prev: preview(prev, name, pal, grids, seqs, made, water)
        built.append((name, pal, frames, offs, data, seqs, made, water))
    print("companions: ~%d bytes" % total)
    if only: return
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    with open(out, "w") as f:
        f.write("/* generated by tools/gen_pets.py: Modular Compagnon's companions (do not edit) */\n")
        f.write("#define CAT_G %d\n#define CAT_SCENE %d\n" % (G, SCENE))
        f.write("typedef struct { uint32_t off; uint16_t len, base; uint8_t x, y, w, h; } cat_frame_t;   /* base: 0xFFFF = a key */\n")
        f.write("typedef struct { uint16_t f; uint8_t d10; int8_t dx; } cat_step_t;   /* dx: the frame moved sideways */\n")
        f.write("enum { %s, CAT_NCLIPS };\n" % ", ".join("CAT_" + n for n in SLOTS))
        f.write("typedef struct { uint16_t start, n; uint8_t loop; } cat_clip_t;\n")
        for name, pal, frames, offs, data, seqs, made, water in built:
            emit(f, "PET_" + name, pal, frames, offs, data, seqs, made)
        f.write("typedef struct { const char *name; const uint16_t *pal; const uint8_t *data; const cat_frame_t *frames;\n"
                "                 const cat_step_t *steps; const cat_clip_t *clips; uint8_t water; } pet_t;\n")
        f.write("#define PET_N %d\nstatic const pet_t PETS[PET_N] = {\n" % len(built))
        for name, *_, water in built:
            P = "PET_" + name
            f.write("    {\"%s\", %s_PAL, %s_DATA, %s_FRAMES, %s_STEPS, %s_CLIPS, %d},\n" % (name, P, P, P, P, P, water))
        f.write("};\n")


if __name__ == "__main__":
    main()
