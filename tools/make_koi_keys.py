#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular Koi: the picture of the FM-1's keys under the thumbs (web/koi-fm1/img/touches-fr.png, keys-en.png), in the
Game Boy's four greens, as koi_play.c lays them out (KOI_KEY). Font: Silkscreen (SIL OFL 1.1).
  tools/make_koi_keys.py FONT.ttf OUT_DIR"""
import sys
from PIL import Image, ImageDraw, ImageFont

G0, G1, G2, G3 = (15, 56, 15), (48, 98, 48), (139, 172, 15), (155, 188, 15)
PAPER = (201, 211, 166)
FR = ["SOL", "SI", "DO", "SI'", "MI", "OP3", "SOL'", "DO", "DO", "RE", "RE", "MI", "FA", "MI", "SOL", "SOL", "LA", "LA", "SI",
      "DO'", "MONO", "LA'", "POLY", "FA", "RE", "DO'", "LA"]
EN = ["G", "B", "C", "B'", "E", "OP3", "G'", "C", "C", "D", "D", "E", "F", "E", "G", "G", "A", "A", "B", "C'", "MONO", "A'",
      "POLY", "F", "D", "C'", "A"]
BLACK = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25]                  # Felucca's key order: F3 .. G5, the top row's
GEST = {5: ("VITE>LENT", "FAST>SLOW"), 20: ("LENT>VITE", "SLOW>FAST"), 22: ("HARMO", "HARMO")}
LEFT, RIGHT = {0, 1, 2, 3, 4, 6}, {21, 23, 24, 25, 26}             # the thumbs' tongues (the rest: the same, mid)


def draw(lang, font_path, out):
    W, H = 1200, 470
    im = Image.new("RGB", (W, H), PAPER)
    d = ImageDraw.Draw(im)
    f = ImageFont.truetype(font_path, 18)
    fs = ImageFont.truetype(font_path, 13)
    ft = ImageFont.truetype(font_path, 22)
    labels = FR if lang == "fr" else EN
    white = [k for k in range(27) if k not in BLACK]
    kw, gap, x0, yb, hb = 66, 7, 30, 250, 150                      # the bottom row: 16 keys
    pos = {}
    for i, k in enumerate(white):
        pos[k] = (x0 + i * (kw + gap), yb, kw, hb)
    for k in BLACK:                                                 # the top row, between its white neighbours
        a, b = pos[k - 1], pos[k + 1]
        cx = (a[0] + a[2] + b[0]) / 2
        pos[k] = (int(cx - kw / 2), 95, kw, 135)
    for k, (x, y, w, h) in pos.items():
        side = k in LEFT or k in RIGHT
        g = k in GEST
        fill = G0 if side else G1 if not g else G3
        ink = G3 if not g else G0
        d.rounded_rectangle((x, y, x + w, y + h), radius=22, fill=fill, outline=G0, width=4 if g else 0)
        lab = labels[k] if not g else GEST[k][0 if lang == "fr" else 1]
        if g:
            parts = lab.split(">")
            for j, part in enumerate(parts):
                part += ">" if j < len(parts) - 1 else ""
                tw = d.textlength(part, font=fs)
                d.text((x + (w - tw) / 2, y + h / 2 - 9 - 13 * (len(parts) - 1) + j * 26), part, font=fs, fill=ink)
        else:
            tw = d.textlength(lab, font=f)
            d.text((x + (w - tw) / 2, y + h / 2 - 9), lab, font=f, fill=ink)
    t = ("POUCE GAUCHE", "LE MILIEU : LES MEMES LAMES", "POUCE DROIT") if lang == "fr" else \
        ("LEFT THUMB", "THE MIDDLE: THE SAME TONGUES", "RIGHT THUMB")
    d.text((30, 30), t[0], font=ft, fill=G0)
    tw = d.textlength(t[2], font=ft)
    d.text((W - 30 - tw, 30), t[2], font=ft, fill=G0)
    tw = d.textlength(t[1], font=fs)
    d.text(((W - tw) / 2, 420), t[1], font=fs, fill=G1)
    leg = "' : LA LAME AIGUE" if lang == "fr" else "' : THE HIGHER TONGUE"
    d.text((30, 420), leg, font=fs, fill=G1)
    im.save(out, optimize=True)


if __name__ == "__main__":
    font, out = sys.argv[1], sys.argv[2]
    draw("fr", font, f"{out}/touches-fr.png")
    draw("en", font, f"{out}/keys-en.png")
    print("keys:", out)
