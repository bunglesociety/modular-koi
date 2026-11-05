#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""The little black cat who lives in the Modular One SMK-37's screen: every pose drawn here as one silhouette on an
80 x 80 grid (x3 on the 240 x 240 screen), every animation a clip of frames. He sits on the bottom of the screen (his
floor), 1.2 x the size he was designed at: the poses are drawn in design units (feet on row 62) and VIEW scales the
shapes around his feet and sets them down on row 79; the small pixel patterns (eyes, mouth, whiskers, the little
things around him) are placed at the scaled points but drawn at their own size, so nothing comes apart.

Out: build/gen/cat_sprites.h (palette RGB565, run-length frames, clips) for firmware/src/m1_draw.c.
     --preview DIR: each clip as a GIF, decoded back from the header's data (what the firmware will draw).

Frame data: a bounding box, then bytes (n-1) << 4 | colour, colour 0 = see-through, rows left to right."""
import math, os, sys

G, SCENE = 80, 80
VIEW = {"k": 1.2, "ax": 40.0, "ay": 62.0, "dy": 17.0}   # scale around the feet, then down to the bottom row


def T(x, y):
    v = VIEW
    return (round(v["ax"] + (x - v["ax"]) * v["k"], 6), round(v["ay"] + (y - v["ay"]) * v["k"] + v["dy"], 6))   # (rounded: a pose moved by whole cells draws the same)


class anchor:
    """a pose against an edge scales around that edge (griffes on the right, territoire on the left)"""
    def __init__(self, ax): self.ax = ax
    def __enter__(self): self.old = VIEW["ax"]; VIEW["ax"] = self.ax
    def __exit__(self, *a): VIEW["ax"] = self.old

# ------------------------------------------------------------------ palette (0 = see-through)
PAL = [None, "#17141d", "#36323f", "#2b2733", "#57516a", "#a59ebd", "#f0a2b6", "#f4a3b6", "#f3cf5a",
       "#ffffff", "#ff8fa6", "#cfc8ff", "#e07a3a", "#f27c9a", "#ffd65a", "#fafaf0"]
KEY = dict(o=1, E=1, K=2, D=3, L=4, F=5, P=6, B=7, Y=8, H=9, M=10, z=11, n=12, h=13, b=14, w=15)
FUR = {"K", "D", "L"}


class C:
    """a canvas in final cells; ell / tri / stroke / put / line take design units (VIEW), pat an anchor in design units
    and offsets in cells"""
    def __init__(self): self.m = {}
    def raw(self, x, y, k): self.m[(int(round(x)), int(round(y)))] = k
    def put(self, x, y, k): self.raw(*T(x, y), k)
    def pat(self, x, y, offs, k, keep=False):
        X, Y = T(x, y); X, Y = int(round(X)), int(round(Y))
        for dx, dy in offs:
            if keep: self.m.setdefault((X + dx, Y + dy), k)
            else: self.m[(X + dx, Y + dy)] = k
    def line(self, x0, y0, x1, y1, k):
        (a, b), (c, d) = T(x0, y0), T(x1, y1)
        n = int(max(abs(c - a), abs(d - b))) + 1
        for i in range(n + 1): self.raw(a + (c - a) * i / n, b + (d - b) * i / n, k)
    def _ell(self, cx, cy, rx, ry, k):
        for y in range(int(cy - ry - 1), int(cy + ry + 2)):
            for x in range(int(cx - rx - 1), int(cx + rx + 2)):
                if ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 <= 1.0: self.m[(x, y)] = k
    def ell(self, cx, cy, rx, ry, k="K"):
        X, Y = T(cx, cy); self._ell(X, Y, rx * VIEW["k"], ry * VIEW["k"], k)
    def tri(self, a, b, c, k="K"):
        a, b, c = T(*a), T(*b), T(*c)
        def s(p, q, r): return (p[0] - r[0]) * (q[1] - r[1]) - (q[0] - r[0]) * (p[1] - r[1])
        for y in range(int(min(a[1], b[1], c[1])), int(max(a[1], b[1], c[1])) + 1):
            for x in range(int(min(a[0], b[0], c[0])), int(max(a[0], b[0], c[0])) + 1):
                p = (x + 0.5, y + 0.5)
                d1, d2, d3 = s(p, a, b), s(p, b, c), s(p, c, a)
                if not ((d1 < 0 or d2 < 0 or d3 < 0) and (d1 > 0 or d2 > 0 or d3 > 0)): self.m[(x, y)] = k
    def stroke(self, pts, r=1.0, k="K"):
        pts = [T(*p) for p in pts]; r *= VIEW["k"]
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            n = int(max(abs(x1 - x0), abs(y1 - y0)) * 3) + 1
            for i in range(n + 1):
                t = i / n; self._ell(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, r, r, k)
    def finish(self):
        """1 px outline round the whole silhouette, rim light top-left, a shade on the right."""
        body = set(self.m); out = {}; new = {}
        for (x, y) in body:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                if (x + dx, y + dy) not in body: out[(x + dx, y + dy)] = "o"
        for (x, y), k in self.m.items():
            if k in FUR:
                if (x - 1, y) not in body or (x, y - 1) not in body: new[(x, y)] = "L"
                elif (x + 2, y) not in body: new[(x, y)] = "D"
        self.m.update(new); self.m.update(out)


def bez(p0, p1, p2, n=16):
    return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0],
             (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]) for t in [i / n for i in range(n + 1)]]


def rot(px, py, cx, cy, a):
    ca, sa = math.cos(a), math.sin(a); dx, dy = px - cx, py - cy
    return (cx + ca * dx - sa * dy, cy + sa * dx + ca * dy)


class Head:
    """The head as geometry: its shape goes into the silhouette, its face is drawn after the outline;
    a tilt turns the ear and eye positions (never rotates pixels: nothing comes apart)."""
    def __init__(self, cx, cy, tilt=0.0, ear=0, flat=0):
        self.cx, self.cy, self.a, self.ear, self.flat = cx, cy, tilt, ear, flat
    def R(self, x, y): return rot(self.cx + x, self.cy + y, self.cx, self.cy, self.a)
    def shape(self, c):
        c.ell(self.cx, self.cy, 12, 8.5); f = self.flat
        c.tri(self.R(-11.5, -1), self.R(-10.5 - f, -11 + 2 * f), self.R(-3, -7))
        c.tri(self.R(11.5, -1), self.R(10.5 + self.ear + f, -11 + abs(self.ear) + 2 * f), self.R(3, -7))
    def ears(self, c):
        f = self.flat
        c.tri(self.R(-9.8, -3), self.R(-9.5 - f, -8.5 + 2 * f), self.R(-5.5, -6.5), "P")
        c.tri(self.R(9.8, -3), self.R(9.5 + self.ear + f, -8.5 + abs(self.ear) + 2 * f), self.R(5.5, -6.5), "P")
    def face(self, c, eyes="open", mouth="w", look=0, whisk=True):
        for sx in (-1, 1):
            ex, ey = self.R(sx * 5, 1)
            if eyes == "open":
                c.pat(ex, ey, [(-1 + dx + look, dy) for dx in (0, 1) for dy in (-1, 0, 1)], "Y")
                c.pat(ex, ey, [(-1 + look, -1)], "H")
            elif eyes == "wide":
                c.pat(ex, ey, [(dx + look, dy - 1) for dx in (-1, 0, 1) for dy in (-1, 0, 1, 2)], "Y")
                c.pat(ex, ey, [(-1 + look, -2)], "H"); c.pat(ex, ey, [(look, 0)], "E")
            elif eyes == "half":
                c.pat(ex, ey, [(dx, -1) for dx in (-2, -1, 0, 1, 2)], "F"); c.pat(ex, ey, [(-1, 0), (0, 0)], "Y")
            elif eyes == "happy":
                c.pat(ex, ey, [(-2, 1), (-1, 0), (0, -1), (1, 0), (2, 1)], "F")
            elif eyes == "content":
                c.pat(ex, ey, [(-2, 0), (-1, 1), (0, 1), (1, 1), (2, 0)], "F")
            elif eyes == "squeeze":
                c.pat(ex, ey, [(-sx, -1), (0, 0), (-sx, 1)], "F")
            c.pat(*self.R(sx * 8, 4), [(0, 0), (sx, 0)], "B")
        c.pat(*self.R(0, 3), [(0, 0)], "P")
        if mouth in ("w", "mlem", "tongue"):
            c.pat(*self.R(0, 4), [(-2, 0), (-1, 1), (0, 0), (1, 1), (2, 0)], "F")
            if mouth in ("mlem", "tongue"):
                c.pat(*self.R(0, 5.6), [(0, 0), (1, 0)] + ([(0, 1)] if mouth == "tongue" else []), "M")
        elif mouth == "o":
            c.pat(*self.R(0, 4), [(-1, 0), (0, 0), (1, 0), (-1, 1), (0, 1), (1, 1), (0, 2)], "M")
        if whisk:
            for sx in (-1, 1):
                c.pat(*self.R(sx * 12, 3), [(sx * i, 0) for i in range(3)], "L")
                c.pat(*self.R(sx * 12, 5), [(sx * i, int(i == 2)) for i in range(3)], "L")


# ------------------------------------------------------------------ poses
def sit(eyes="open", mouth="w", tail=0, look=0, tilt=0.0, ear=0, flat=0, nod=0, paws=(0, 0), cx=38, base=62, paw=None):
    c = C(); by = base; t = tail * 2
    c.stroke(bez((cx + 9, by - 2), (cx + 17 + t, by - 3), (cx + 15 + t, by - 11)), 1.4); c.ell(cx + 15 + t, by - 12, 1.6, 1.6)
    c.ell(cx, by - 8, 11.5, 8.5); c.ell(cx, by - 14, 9, 6)
    h = Head(cx + nod * 0.3, by - 22 + nod, tilt, ear, flat); h.shape(c)
    if paw is not None: c.stroke([(cx - 3, by - 11), paw], 1.6); c.ell(paw[0], paw[1], 2.3, 1.9)
    for k, lx in enumerate((cx - 3, cx + 3)):
        if paws[k]: c.ell(lx, by - 1 - paws[k], 2.2, 1.6)
    c.finish(); h.ears(c)
    if abs(tilt) < 0.05 and nod == 0:
        c.line(cx - 8, h.cy + 8, cx - 3, h.cy + 8, "D"); c.line(cx + 3, h.cy + 8, cx + 8, h.cy + 8, "D")
    for k, lx in enumerate((cx - 3, cx + 3)):
        if paw is not None and k == 0: continue
        c.line(lx, by - 11, lx, by - 2 - paws[k], "o")
    h.face(c, eyes, mouth, look)
    if paw is not None: c.pat(paw[0], paw[1] + 1, [(-1, 0), (0, 0), (1, 0)], "L")
    return c


def side(cx=40, base=62, phase=0, facing=1, eyes="open", tail_up=1.0, crouch=0, tail_to_head=False):
    c = C(); by = base - 3; f = facing; tb = cx - f * 11
    if tail_to_head: c.stroke(bez((tb, by - 9), (tb - f * 4, by - 22), (cx + f * 2, by - 24)), 1.3)
    else: c.stroke(bez((tb, by - 9), (tb - f * 6, by - 13 - tail_up * 3), (tb - f * 4, by - 20 - tail_up * 4)), 1.3)
    s = phase
    for lx, dx in ((cx + f * 7, f * (2 + 5 * s)), (cx + f * 4, f * (-1 + 3 * s)), (cx - f * 5, -f * (1 + 3 * s)), (cx - f * 8, -f * (2 + 5 * s))):
        c.stroke([(lx, by - 6 + crouch), (lx + dx, by + 2 - s * 1.5)], 1.2)
    c.ell(cx, by - 9 + crouch, 12, 6.5 - crouch * 0.5)
    h = Head(cx + f * 9, by - 15 + crouch); h.shape(c)
    c.finish(); h.ears(c); h.face(c, eyes, "w", look=f)
    return c


def cello(lick=0, eyes="content"):
    c = C(); cx, by = 34, 62
    c.stroke(bez((cx - 8, by - 1), (cx - 20, by - 1), (cx - 22, by - 6)), 1.3)
    c.ell(cx, by - 7, 12, 7.5)
    c.stroke([(cx - 6, by - 2), (cx - 13, by - 1)], 1.4); c.ell(cx - 14, by - 1, 2.2, 1.4)
    h = Head(cx, by - 16 + lick, 0.3); h.shape(c)
    c.finish(); h.ears(c); h.face(c, eyes, "tongue" if lick else "w")
    leg = C(); leg.stroke([(cx + 9, by - 6), (cx + 15, by - 33)], 1.8); leg.ell(cx + 15.5, by - 34.5, 2.6, 2.3); leg.finish()
    c.m.update(leg.m)
    c.pat(cx + 15.5, by - 36, [(-1, 0), (0, 0), (1, 0)], "P")
    return c


def loaf(eyes="content", breath=0, cx=40, base=62):
    c = C(); by = base
    c.ell(cx + 4, by - 6 - breath * 0.5, 16, 7 + breath * 0.5)
    c.stroke(bez((cx + 19, by - 3), (cx + 22, by + 0), (cx + 12, by - 0.5)), 1.3)
    hy = by - 13
    c.ell(cx - 6, hy, 11, 8)
    c.tri((cx - 16.5, hy - 1), (cx - 15.5, hy - 10), (cx - 9, hy - 6))
    c.tri((cx + 4.5, hy - 1), (cx + 3.5, hy - 10), (cx - 3, hy - 6))
    c.ell(cx - 9, by - 1.2, 3, 1.4); c.ell(cx - 2, by - 1.2, 3, 1.4)
    c.finish()
    c.tri((cx - 14.8, hy - 3), (cx - 14.5, hy - 7.5), (cx - 10.5, hy - 5.8), "P")
    c.tri((cx + 2.8, hy - 3), (cx + 2.5, hy - 7.5), (cx - 1.5, hy - 5.8), "P")
    c.line(cx - 11, by - 2, cx - 9, by - 2, "L"); c.line(cx - 4, by - 2, cx - 2, by - 2, "L")
    c.pat(cx - 9.5, by - 1, [(0, 0)], "o"); c.pat(cx - 2.5, by - 1, [(0, 0)], "o")
    ey = hy + 1
    for ex in (cx - 11, cx - 1):
        if eyes == "content": c.pat(ex, ey, [(-2, 0), (-1, 1), (0, 1), (1, 1), (2, 0)], "F")
        else: c.pat(ex, ey, [(-2, 1), (-1, 0), (0, -1), (1, 0), (2, 1)], "F")
        sx = -1 if ex < cx - 6 else 1
        c.pat(ex + 3 * sx, ey + 3, [(0, 0), (-sx, 0)], "B")
    c.pat(cx - 6, ey + 2, [(0, 0)], "P")
    c.pat(cx - 6, ey + 3, [(-2, 0), (-1, 1), (0, 0), (1, 1), (2, 0)], "F")
    return c


def back(cx=40, base=62, wiggle=0, eyes="happy"):
    """On his back, belly up, paws in the air."""
    c = C(); by = base
    c.stroke(bez((cx + 12, by - 4), (cx + 20, by - 3), (cx + 19, by - 10 + wiggle)), 1.3)
    c.ell(cx + 3, by - 6, 12, 6)
    for i, lx in enumerate((cx - 4, cx + 1, cx + 6, cx + 11)):
        w = wiggle * (1 if i % 2 else -1)
        c.stroke([(lx, by - 10), (lx + w, by - 16)], 1.2); c.ell(lx + w, by - 16.5, 1.7, 1.5)
    h = Head(cx - 12, by - 9, flat=1); h.shape(c)
    c.finish(); h.ears(c); h.face(c, eyes, "w")
    for i, lx in enumerate((cx - 4, cx + 1, cx + 6, cx + 11)):
        w = wiggle * (1 if i % 2 else -1); c.pat(lx + w, by - 17, [(0, 0)], "P")
    return c


def up(cx=70, base=62, reach=0, eyes="squeeze"):
    """Standing on the hind legs against the right edge, front paws on the edge (griffes)."""
    with anchor(78.0): return _up(cx, base, reach, eyes)

def _up(cx, base, reach, eyes):
    c = C(); by = base
    c.stroke(bez((cx - 6, by - 2), (cx - 16, by - 2), (cx - 16, by - 10)), 1.3)
    c.ell(cx, by - 11, 7.5, 11)
    c.ell(cx - 3, by - 1, 2.6, 1.6); c.ell(cx + 3, by - 1, 2.6, 1.6)
    for k, yy in enumerate((by - 24, by - 19)):
        r = reach if k == 0 else -reach
        c.stroke([(cx + 3, yy + 6), (78, yy + r)], 1.3); c.ell(78, yy + r, 1.6, 2)
    h = Head(cx - 2, by - 26); h.shape(c)
    c.finish(); h.ears(c); h.face(c, eyes, "w")
    return c


def rub(cx=16, base=62, push=0):
    """Rubbing his cheek on the left edge (territoire)."""
    with anchor(2.0): return _rub(cx, base, push)

def _rub(cx, base, push):
    c = C(); by = base
    c.stroke(bez((cx + 20, by - 9), (cx + 26, by - 16), (cx + 23, by - 25 + push)), 1.3)
    c.ell(cx + 10, by - 8, 12, 7)
    for lx in (cx + 2, cx + 6, cx + 14, cx + 18): c.stroke([(lx, by - 4), (lx, by)], 1.2)
    h = Head(cx - 2 - push, by - 16); h.shape(c)
    c.finish(); h.ears(c); h.face(c, "content", "w")
    return c


# ------------------------------------------------------------------ little things drawn with him
def bug(c, x, y, t):
    c.pat(x, y, [(0, 0), (1, 0)], "b", True); c.pat(x, y, [(-1 if t % 4 < 2 else 2, -1)], "w", True); return c

def zzz(c, n=3):
    z4 = [(0, 0), (1, 0), (2, 0), (3, 0), (2, 1), (1, 2), (0, 3), (1, 3), (2, 3), (3, 3)]
    z5 = [(0, 0), (1, 0), (2, 0), (3, 0), (4, 0), (3, 1), (2, 2), (1, 3), (0, 4), (1, 4), (2, 4), (3, 4), (4, 4)]
    for gx, gy, g in ((50, 38, z4), (55, 30, z4), (61, 20, z5))[:n]: c.pat(gx, gy, g, "z", True)
    return c

def notes(c, k=0):
    for gx, gy in ((55, 26 - k), (60, 21 + k)):
        c.pat(gx, gy, [(1, i) for i in range(4)] + [(0, 3), (0, 4), (1, 4), (2, 0), (3, 1)], "n", True)
    return c

def hearts(c, n):
    with anchor(2.0):
        for k in range(n):
            c.pat(10 + k * 6, 30 - k * 5, [(0, 0), (2, 0), (-1, 1), (0, 1), (1, 1), (2, 1), (3, 1), (0, 2), (1, 2), (2, 2), (1, 3)], "h", True)
    return c

def marks(c, n):
    for i in range(n):
        Y = int(round(T(78, 34 + i * 4)[1]))
        for k in range(4): c.m.setdefault((79 - (k % 2), Y + k), "L")
    return c

def dust(c, xs):
    for x in xs: c.pat(x, 61, [(0, 0), (1, 0), (0, -1), (-1, 0)], "F", True)
    return c

def ball(c, x, y, k="M", line="h"):
    """a small ball (yarn for the cat), its middle at cell x, y (props are drawn in cells)"""
    for dy in range(-3, 4):
        for dx in range(-3, 4):
            if dx * dx + dy * dy <= 10: c.m[(x + dx, y + dy)] = k
            elif dx * dx + dy * dy <= 17: c.m.setdefault((x + dx, y + dy), "o")
    for i in (-2, -1, 0, 1, 2): c.m[(x + i, y + (i * i) // 3 - 1)] = line
    return c

def flower(c, x, y, petal="M", heart="b"):
    """a little flower on the floor (cells), its heart at x, y"""
    for cx_, cy_ in ((0, -2), (-2, 0), (2, 0), (0, 2)):
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if abs(dx) + abs(dy) < 2: c.m[(x + cx_ + dx, y + cy_ + dy)] = petal
    c.m[(x, y)] = heart
    for dx in range(-4, 5):
        for dy in range(-4, 5):
            if (x + dx, y + dy) not in c.m and any((x + dx + a, y + dy + b) in c.m and c.m[(x + dx + a, y + dy + b)] in (petal, heart) for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                c.m[(x + dx, y + dy)] = "o"
    return c

def peek(c, dx):
    """only the part of a pose the screen's edge lets through (a head peeking in): the pose moved dx cells"""
    d = C(); d.m = {(x + dx, y): k for (x, y), k in c.m.items()}; return d

def quest(c):
    c.pat(22, 22, [(0, 0), (1, -1), (2, -1), (3, 0), (3, 1), (2, 2), (2, 3), (2, 5)], "n", True); return c


# ------------------------------------------------------------------ clips: (name, loop, [(canvas, ms)])
def walk(x0, x1, facing, step=2.5, ms=100):   # (2.5: 3 cells, so the steps are one picture moved)
    out, k, x = [], 0, x0
    while (x - x1) * facing < 0:
        out.append((side(cx=x, phase=k % 2, facing=facing), ms)); x += facing * step; k += 1
    return out

def back_home(x0):
    """From wherever the clip ended, walk back to the middle and sit."""
    f = 1 if x0 < 38 else -1
    return walk(x0, 38 + (2 if f < 0 else 0), f) + [(sit("open"), 300)]

CLIPS = []
def clip(name, frames, loop=0): CLIPS.append((name, loop, frames))

# IDLE: content, breathing tail, a blink, open eyes for a while, a happy look
fr = []
for i in range(64):
    tl = round(math.sin(i / 64 * math.tau * 2) * 2) / 2
    e = "content" if i < 24 else "open" if i < 52 else "happy"
    if i in (38, 39): e = "content"
    fr.append((sit(e, tail=tl), 140))
clip("IDLE", fr, 1)
clip("LOOK_L", [(sit("open", look=-1, tilt=-0.12, ear=1), 120), (sit("open", look=-1, tilt=-0.12), 600), (quest(sit("open", look=-1, tilt=-0.12)), 900)])
clip("LOOK_R", [(sit("open", look=1, tilt=0.12, ear=1), 120), (sit("open", look=1, tilt=0.12), 600), (sit("open", look=1, tilt=0.12), 900)])
clip("PLAY", [(notes(sit("happy", tail=(-1, -0.5, 0, 0.5, 1, 0.5, 0, -0.5)[i]), i // 4), 120) for i in range(8)], 1)
clip("SLEEP", [(zzz(loaf("content", breath=1 if (i // 6) % 2 else 0), 1 + (i // 4) % 3), 160) for i in range(24)], 1)
clip("WAKE", [(loaf("content"), 200), (sit("wide", flat=1), 160), (sit("wide"), 300), (sit("open"), 400)])

fr = [(sit("half" if i < 75 else "content", paws=(3, 0) if (i // 3) % 2 else (0, 3), tail=math.sin(i / 4) * 0.5), 110) for i in range(96)]
clip("PETRIR", fr)
fr = [(sit("open"), 110)] * 12 + [(sit("content" if i in (14, 15, 30, 31) else "open", "mlem", tail=math.sin(i / 5) * 0.4), 110) for i in range(72)] + [(sit("open"), 110)] * 6
clip("MLEM", fr)
fr = [(sit("half"), 140)] * 6 + [(sit("half", nod=n), 140) for n in (1, 2, 3, 4)] + [(sit("content", nod=n), 140) for n in (4, 5, 5, 5, 5, 5)]
fr += [(sit("wide"), 140)] * 2 + [(sit("half"), 140)] * 6 + [(sit("content", nod=n), 140) for n in (1, 2, 3, 4, 5, 6, 6)]
fr += [(zzz(sit("content", nod=6), 1 + i // 5), 140) for i in range(14)]
clip("ENDORT", fr)
fr = [(sit("open"), 100)] * 8 + [(sit("wide", look=1, tilt=0.12 if i > 10 else 0.0, ear=1 if i % 6 == 0 else 0), 100) for i in range(22)]
fr += [(sit("wide", look=1, flat=1), 100)] * 2
fr += [(side(cx=36 - 7.5 * k, phase=k % 2, facing=-1, tail_up=1.6), 100) for k in range(8)] + [(C(), 100)] * 10
fr += walk(-20, 38, 1) + [(sit("open"), 300)]
clip("VIDE", fr)
fr, f = [], 1
for s_ in [4, 4, 3, 3, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1]:
    fr += [(side(cx=40, facing=f, tail_to_head=True, phase=j % 2, eyes="wide"), 90) for j in range(s_)]; f = -f
fr += [(sit("squeeze", tilt=(0.15, 0, -0.15, 0)[i % 4]), 90) for i in range(10)] + [(sit("happy"), 90)] * 6
clip("QUEUE", fr)
fr = [(sit("open"), 110)] * 6 + [(cello(lick=1 if (i // 3) % 2 else 0), 110) for i in range(84)] + [(cello(0, "open"), 110)] * 10 + [(sit("content"), 110)] * 6
clip("CELLO", fr)
fr, path = [], [(62, 58), (63, 57), (65, 58), (64, 59), (66, 58)]
for i in range(16):
    bx, by_ = path[(i // 3) % len(path)]
    fr.append((bug(sit("open", look=1, tilt=(0, 0.22, 0.22, 0, -0.22, -0.22)[(i // 3) % 6]), bx, by_, i), 100))
for i in range(30):
    c = side(cx=34, crouch=4, tail_up=0.2); dx = (-1, 0, 1, 0)[i % 4]
    c.m = {((x + dx) if x < 26 else x, y): k for (x, y), k in c.m.items()}
    fr.append((bug(c, 64, 58, i), 100))
for k, (x, h) in enumerate(((40, 4), (47, 9), (54, 10), (60, 6), (64, 0))):
    c = side(cx=x, phase=1 if k < 4 else 0, crouch=0 if k < 4 else 3, tail_up=1.5)
    c.m = {(a, b - round(h * VIEW["k"])): v for (a, b), v in c.m.items()}
    fr.append((bug(c, 64, 58, k) if k < 2 else c, 100))
fr += [(side(cx=64, crouch=4, eyes="squeeze"), 100)] * 8 + [(bug(side(cx=64, facing=-1), 24, 40 - i, i), 100) for i in range(12)]
fr += walk(64, 40, -1) + [(sit("open"), 300)]
clip("CHASSE", fr)
fr = []
for i in range(30):
    bx = 22 if i < 20 else max(1, 22 - (i - 20)); by_ = 58 if i < 20 else 58 - (i - 20) // 2
    if i < 12: c = sit("open", look=-1, tilt=(0, 0.25, 0.25, 0.25, 0, -0.25, -0.25, -0.25, 0, 0.25, 0.25, 0.25)[i])
    elif i < 20: c = sit("open", look=-1, tilt=0.15, paw=(25, (46, 50, 54, 57, 57, 50, 54, 57)[i - 12]))
    else: c = sit("open", look=-1, tilt=-0.25)
    fr.append((bug(c, bx, by_, i), 110))
fr += [(sit("open"), 300)]
clip("TAPOTE", fr)
fr = []
for i in range(110):
    k = i % 10
    if i % 40 < 24: fr.append((sit("content", "tongue" if k in (2, 3, 7, 8) else "w", paw=(31, 44 - (1 if k < 5 else 0)), tilt=0.08), 110))
    else:
        a = (i % 40 - 24) / 16 * math.tau * 1.5
        fr.append((sit("content", paw=(29 + 2 * math.cos(a), 36 + 3 * math.sin(a)), tilt=0.08, ear=-1 if k < 5 else 0), 110))
clip("TOILETTE", fr)
fr = [(sit("open"), 110)] * 8 + [(side(cx=40, crouch=3, eyes="content"), 110)] * 4
fr += [(back(wiggle=(-1, 0, 1, 0)[i % 4]), 110) for i in range(48)] + [(side(cx=40, crouch=3, eyes="content"), 110)] * 4 + [(sit("happy"), 110)] * 8
clip("ROULADE", fr)
fr = [(side(cx=24 + i * 2.5, phase=i % 2, facing=1), 110) for i in range(15)]
fr += [(marks(up(reach=(-2, 0, 2, 0)[i % 4]), min(3, i // 16 + 1)), 110) for i in range(64)]
fr += walk(60, 40, -1) + [(sit("open"), 300)]
clip("GRIFFES", fr)
seq = [(-15 + 7.5 * k, 1) for k in range(16)] + [(100 - 7.5 * k, -1) for k in range(16)]
fr = [(dust(side(cx=x, phase=i % 2, facing=f, tail_up=1.5), [x - f * 14, x - f * 18]), 80) for i, (x, f) in enumerate(seq)]
fr += [(C(), 80)] * 8 + walk(-20, 38, 1, 5, 80) + [(sit("happy"), 80)] * 8
clip("FOLIE", fr)
fr = [(side(cx=38 - i * 2.5, phase=i % 2, facing=-1), 100) for i in range(8)]
fr += [(hearts(rub(push=(0, 1, 2, 1)[(i // 2) % 4]), min(3, i // 18 + 1)), 110) for i in range(64)]
fr += walk(20, 38, 1) + [(sit("happy"), 300)]
clip("TERRITOIRE", fr)
fr = [(sit("open"), 110)] * 10 + [(sit("squeeze", "o", flat=1, tilt=-0.08), 110)] * 14 + [(sit("content", tail=math.sin(i / 3)), 110) for i in range(16)]
clip("BAILLEMENT", fr)

# DANSE: while the ARP or the loop plays, a step of the dance on each beat (m1_draw.c steps it, not the clock): he sways
# from one side to the other, a paw up, then the other, a little bounce between, the notes around him
fr = []
for i in range(8):
    k = i % 4
    if k == 0: c = sit("happy", tail=-1, tilt=-0.25, cx=35, paw=(24, 42 + (i // 4) * 4))     # a paw up high, leaning
    elif k == 2: c = sit("happy", tail=1, tilt=0.25, cx=41, paw=(50, 44 - (i // 4) * 4))     # .. the other way
    else: c = sit("happy", tail=0, cx=38, base=58, ear=1)                                     # the bounce between
    fr.append((notes(c, i // 2) if k == 1 else c, 150))
clip("DANSE", fr, 1)

# CALIN (the FM-1's SEL: a cuddle): his eyes close, he leans into the hand, purrs (his tail curls), hearts rise
fr = [(sit("happy"), 200)]
for i in range(22):
    lean = (0, 0.12, 0.2, 0.12)[(i // 3) % 4]
    fr.append((hearts(sit("content", tail=math.sin(i / 3) * 0.6, tilt=lean, ear=1 if i % 6 < 3 else 0), 1 + min(i // 5, 3)), 130))
fr += [(sit("happy", mouth="w"), 600)]
clip("CALIN", fr)

# JOUER (SEL): a ball of yarn rolls in; he watches it, bats it up, it comes down, he bats it again, it rolls off
fr = [(ball(sit("wide", look=1, tilt=0.1), x, 73), 110) for x in (76, 70, 64, 60)]
fr += [(ball(sit("wide", look=1, tilt=0.15, ear=1), 60, 73), 300)]
for k, (bx, by_) in enumerate(((58, 66), (63, 56), (69, 52), (72, 58), (68, 68), (62, 73))):
    fr.append((ball(sit("happy" if 1 < k < 5 else "wide", look=1, paw=(47, 54) if k == 0 else None), bx, by_), 100))
for k, (bx, by_) in enumerate(((66, 64), (73, 58), (80, 62))):
    fr.append((ball(sit("wide", look=1, paw=(47, 54) if k == 0 else None), bx, by_), 100))
fr += [(sit("happy", "w", tail=0.5), 600)]
clip("JOUER", fr)

# CACHE (SEL): off he trots, left; a moment of nothing; his head peeks in at the edge, twice; then he dashes back in
# from the right and sits, pleased with himself
fr = [(sit("open", look=-1), 250)] + walk(38, -22, -1, step=5, ms=80) + [(C(), 120)] * 6
fr += [(peek(sit("open", look=1, tilt=0.2), -40), 200), (peek(sit("open", look=1, tilt=0.2), -37), 600),
       (peek(sit("content", look=1, tilt=0.2), -37), 150), (peek(sit("open", look=1, tilt=0.2), -37), 500), (C(), 500)]
fr += walk(100, 40, -1, step=5, ms=60)
fr += [(sit("happy", tail=1), 300), (hearts(sit("happy", "w", tail=0.5), 2), 700)]
clip("CACHE", fr)

# CADEAU (SEL): off he goes to the right and comes back with a flower, sets it down at your feet, and is very proud
fr = [(sit("open", look=1), 250)] + walk(38, 98, 1, step=5, ms=80) + [(C(), 120)] * 8
fr += walk(98, 50, -1, step=5, ms=90)
fr += [(flower(sit("happy", look=1), 62, 74), 500), (hearts(flower(sit("happy", "w", tail=0.6), 62, 74), 2), 600),
       (hearts(flower(sit("happy", "w", tail=-0.6), 62, 74), 3), 600), (flower(sit("open", look=1), 62, 74), 500)]
clip("CADEAU", fr)

# PASSE (the tape plays, its reels fill the screen): now and then he walks in, stops under the reels, looks up at them
# turning, wonders, and walks on out (poses he has already: no new picture)
fr = walk(-22, 38, 1, step=5, ms=110) + [(sit("open"), 300)]
fr += [(sit("open", look=-1, tilt=-0.12, ear=1), 120), (sit("open", look=-1, tilt=-0.12), 1600),
       (quest(sit("open", look=-1, tilt=-0.12)), 900), (sit("open", look=1, tilt=0.12), 1400), (sit("happy"), 600)]
fr += walk(38, 102, 1, step=5, ms=110)
clip("PASSE", fr)


# ------------------------------------------------------------------ encode
def grid(c):
    return tuple(KEY[c.m[(x, y)]] if (x, y) in c.m else 0 for y in range(SCENE) for x in range(G))

def bbox(g):
    xs = [i % G for i, v in enumerate(g) if v]; ys = [i // G for i, v in enumerate(g) if v]
    if not xs: return (0, 0, 0, 0)
    return (min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1)

def encode(cur, base):
    """bytes 1sssssss: skip s+1 cells (kept as the base has them); 0nnnvvvv: n+1 cells of colour v."""
    out, i, N = bytearray(), 0, len(cur)
    while i < N:
        if cur[i] == base[i]:
            n = 1
            while i + n < N and cur[i + n] == base[i + n] and n < 128: n += 1
            out.append(0x80 | (n - 1)); i += n
        else:
            v, n = cur[i], 1
            while i + n < N and cur[i + n] == v and cur[i + n] != base[i + n] and n < 8: n += 1
            out.append((n - 1) << 4 | v); i += n
    while out and out[-1] & 0x80: out.pop()
    return bytes(out)

def decode(data, base):
    g, p = list(base), 0
    for b in data:
        if b & 0x80: p += (b & 0x7F) + 1
        else:
            for _ in range((b >> 4) + 1): g[p] = b & 15; p += 1
    return g

def rgb565(h):
    r, g, b = (int(h[i:i + 2], 16) for i in (1, 3, 5)); return (r >> 3) << 11 | (g >> 2) << 5 | b >> 3

def main():
    out = "build/gen/cat_sprites.h"; prev = None
    a = sys.argv[1:]
    if "--preview" in a: prev = a[a.index("--preview") + 1]
    if "-o" in a: out = a[a.index("-o") + 1]
    grids, index, seqs, clips = [], {}, [], []
    for name, loop, fr in CLIPS:
        start = len(seqs)
        for c, ms in fr:
            g = grid(c)
            if g not in index: index[g] = len(grids); grids.append(g)
            if len(seqs) > start and seqs[-1][0] == index[g] and seqs[-1][1] + ms // 10 < 255:
                seqs[-1][1] += ms // 10                     # the same frame held: one longer step
            else: seqs.append([index[g], ms // 10])
        clips.append((name, start, len(seqs) - start, loop))
    zero = (0,) * (G * SCENE)
    frames, keys = [], []                                    # (data, base or 0xFFFF, bbox): a key, or a patch on a key
    for i, g in enumerate(grids):
        best, bb = encode(g, zero), 0xFFFF
        for k in keys[-48:]:
            d = encode(g, grids[k])
            if len(d) + 8 < len(best): best, bb = d, k
        if bb == 0xFFFF: keys.append(i)
        frames.append((best, bb, bbox(g)))
    data, offs = bytearray(), []
    for d, _, _ in frames: offs.append(len(data)); data += d
    for (d, bb, _), g in zip(frames, grids):
        assert decode(d, zero if bb == 0xFFFF else grids[bb]) == list(g)
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    with open(out, "w") as f:
        f.write("/* generated by tools/gen_cat.py: the Modular One cat (do not edit) */\n")
        f.write("#define CAT_G %d\n#define CAT_SCENE %d\n" % (G, SCENE))
        f.write("static const uint16_t CAT_PAL[16] = {%s};\n" % ", ".join("0x%04X" % (rgb565(p) if p else 0) for p in PAL))
        f.write("static const uint8_t CAT_DATA[%d] = {\n" % len(data))
        for i in range(0, len(data), 24): f.write("    " + ", ".join(str(b) for b in data[i:i + 24]) + ",\n")
        f.write("};\n")
        f.write("typedef struct { uint32_t off; uint16_t len, base; uint8_t x, y, w, h; } cat_frame_t;   /* base: 0xFFFF = a key */\n")
        f.write("static const cat_frame_t CAT_FRAMES[%d] = {\n" % len(frames))
        for o, (d, bb, (x, y, w, h)) in zip(offs, frames): f.write("    {%d, %d, %d, %d, %d, %d, %d},\n" % (o, len(d), bb, x, y, w, h))
        f.write("};\n")
        f.write("typedef struct { uint16_t f; uint8_t d10; } cat_step_t;\n")
        f.write("static const cat_step_t CAT_STEPS[%d] = {\n" % len(seqs))
        for i in range(0, len(seqs), 8): f.write("    " + " ".join("{%d, %d}," % (s[0], s[1]) for s in seqs[i:i + 8]) + "\n")
        f.write("};\n")
        f.write("enum { %s, CAT_NCLIPS };\n" % ", ".join("CAT_" + n for n, *_ in clips))
        f.write("typedef struct { uint16_t start, n; uint8_t loop; } cat_clip_t;\n")
        f.write("static const cat_clip_t CAT_CLIPS[CAT_NCLIPS] = {\n")
        for n, s, k, lp in clips: f.write("    {%d, %d, %d},   /* %s */\n" % (s, k, lp, n))
        f.write("};\n")
    print("cat: %d clips, %d steps, %d frames (%d keys), %d bytes of pixels" % (len(clips), len(seqs), len(frames), len(keys), len(data)))
    if prev:
        from PIL import Image
        os.makedirs(prev, exist_ok=True)
        cols = [(0, 0, 0)] + [tuple(int(p[i:i + 2], 16) for i in (1, 3, 5)) for p in PAL[1:]]
        for n, s, k, lp in clips:
            ims, durs = [], []
            for st in seqs[s:s + k]:
                g = grids[st[0]]; im = Image.new("RGB", (G * 3, SCENE * 3), (247, 236, 218)); px = im.load()
                for i, v in enumerate(g):
                    if v:
                        for a in range(3):
                            for b in range(3): px[(i % G) * 3 + a, (i // G) * 3 + b] = cols[v]
                ims.append(im); durs.append(st[1] * 10)
            ims[0].save(os.path.join(prev, n.lower() + ".gif"), save_all=True, append_images=ims[1:], duration=durs, loop=0)

if __name__ == "__main__":
    main()
