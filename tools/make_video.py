#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular Compagnon: the presentation video (1920 x 1080, 30 fps). The screen from tests/m1_video.c's scenes (the
firmware's own drawing), large on the left in its bezel; on the right the words in the screen's pixel letters and the
keyboard (web/compagnon/img/commandes-fr.png) with the control in use circled. One file per scene, joined.

  tools/make_video.py SCENES_DIR OUT.mp4 [--still N]   (--still: one frame of each scene as PNG, no video)"""
import math, os, subprocess, sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FONT = ROOT / "assets" / "fonts" / "ui" / "silkscreen-latin-400-normal.woff"
MAP = ROOT / "web" / "compagnon" / "img" / "commandes-fr.png"
W, H, FPS = 1920, 1080, 30
PAPER, GRAIN, INK, DIM, ORANGE = (220, 200, 160), (210, 189, 148), (52, 36, 24), (120, 98, 74), (204, 98, 54)
SX, SY, SS = 150, 170, 720                                    # the screen (3 x)
MX, MY, MW = 1010, 600, 820                                   # the keyboard map
CROP = (80, 100, 1540, 705)
MS = MW / (CROP[2] - CROP[0])
CTRL = {**{f"k{i + 1}": (530 + i * 82, 241) for i in range(8)},
        **{f"pad{i}": (530 + (i % 8) * 82, 315 + (i // 8) * 82) for i in range(16)},
        **{f"f{i + 1}": (1232 + i * 80, 266) for i in range(4)},
        "arp": (1218, 420), "chord": (1386, 420), "globe": (1442, 420), "fx": (1330, 468), "screen": (365, 265)}


def font(px): return ImageFont.truetype(str(FONT), px)


F_T, F_S, F_L = font(64), font(32), font(24)


def wrap(d, text, f, width):
    words, lines, cur = text.split(), [], ""
    for w in words:
        t = (cur + " " + w).strip()
        if d.textlength(t, font=f) <= width or not cur: cur = t
        else: lines.append(cur); cur = w
    if cur: lines.append(cur)
    return lines


def background():
    im = Image.new("RGB", (W, H), PAPER)
    d = ImageDraw.Draw(im)
    for y in range(0, H, 8):
        for x in range((y // 8) % 2 * 4, W, 8):
            if (x * 7 + y * 13) % 5 == 0: d.rectangle([x, y, x + 1, y + 1], fill=GRAIN)
    d.rounded_rectangle([SX - 34 + 10, SY - 34 + 12, SX + SS + 34 + 10, SY + SS + 34 + 12], 40, fill=(190, 168, 128))
    d.rounded_rectangle([SX - 34, SY - 34, SX + SS + 34, SY + SS + 34], 40, fill=(26, 24, 22))
    d.text((SX - 34, SY + SS + 62), "M-VAVE SMK-37 ELITE  ·  MODULAR COMPAGNON 1.0", font=F_L, fill=DIM)
    m = Image.open(MAP).convert("RGB").crop(CROP)
    m = m.resize((MW, int(m.height * MS)), Image.LANCZOS)
    im.paste(m, (MX, MY))
    return im


def events(txt):
    caps, hls = [], []
    for line in Path(txt).read_text(encoding="utf-8").splitlines():
        n, kind, rest = (line.split(" ", 2) + [""])[:3]
        if kind == "cap": t, s = rest.split("|", 1); caps.append((int(n), t, s))
        elif kind == "hl": hls.append((int(n), rest.strip()))
    return caps, hls


def at(evs, i):
    cur = None
    for e in evs:
        if e[0] <= i: cur = e
    return cur


def frame(bg, scr, cap, cap_age, hl, hl_age, fade):
    im = bg.copy()
    im.paste(scr.resize((SS, SS), Image.NEAREST), (SX, SY))
    k = int(round(150 * MS))                                  # the keyboard drawing's own screen: the same picture
    im.paste(scr.resize((k, k), Image.LANCZOS), (int(MX + (290 - CROP[0]) * MS), int(MY + (190 - CROP[1]) * MS)))
    d = ImageDraw.Draw(im, "RGBA")
    if cap and cap[1]:
        a = min(1.0, cap_age / 8)
        y = 190
        for ln in wrap(d, cap[1], F_T, 800):
            d.text((MX, y + int((1 - a) * 12)), ln, font=F_T, fill=INK + (int(255 * a),)); y += 80
        y += 10
        for ln in wrap(d, cap[2], F_S, 800):
            d.text((MX, y + int((1 - a) * 12)), ln, font=F_S, fill=DIM + (int(255 * a),)); y += 46
    if hl and hl[1] in CTRL:
        cx, cy = CTRL[hl[1]]
        x, y = MX + (cx - CROP[0]) * MS, MY + (cy - CROP[1]) * MS
        r = 30 + 4 * math.sin(hl_age / 6)
        d.ellipse([x - r, y - r, x + r, y + r], outline=INK + (230,), width=5)
    if fade < 1:
        d.rectangle([0, 0, W, H], fill=PAPER + (int(255 * (1 - fade)),))
    return im


def scene_frames(rgb):
    data = Path(rgb).read_bytes(); n = len(data) // (240 * 240 * 3)
    return n, (lambda i: Image.frombytes("RGB", (240, 240), data[i * 172800:(i + 1) * 172800]))


def main():
    a = sys.argv[1:]
    src, out = Path(a[0]), Path(a[1])
    still = int(a[a.index("--still") + 1]) if "--still" in a else None
    if "--en" in a:                                            # (English: the keyboard drawing in English)
        global MAP
        MAP = MAP.with_name("controls-en.png")
    bg = background()
    scenes = sorted(src.glob("*.rgb"))
    parts = []
    tmp = out.parent / (out.stem + "_parts"); tmp.mkdir(parents=True, exist_ok=True)
    for si, rgb in enumerate(scenes):
        n, get = scene_frames(rgb)
        caps, hls = events(rgb.with_suffix(".txt"))
        last = si == len(scenes) - 1
        def make(i):
            c, h = at(caps, i), at(hls, i)
            fade = min(1.0, i / 20) if si == 0 else min(1.0, (n - 1 - i) / 30) if last else 1.0
            return frame(bg, get(i), c, i - c[0] if c else 0, h, i - h[0] if h else 0, fade)
        if still is not None:
            make(min(still, n - 1)).save(tmp / f"{rgb.stem}.png"); continue
        part = tmp / f"{rgb.stem}.mp4"; part_tmp = tmp / f"{rgb.stem}.tmp.mp4"
        p = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}",
                              "-r", str(FPS), "-i", "-", "-c:v", "libx264", "-preset", "medium", "-crf", "17",
                              "-pix_fmt", "yuv420p", "-r", str(FPS), str(part_tmp)], stdin=subprocess.PIPE)
        for i in range(n): p.stdin.write(make(i).tobytes())
        p.stdin.close()
        if p.wait() != 0: raise SystemExit(f"ffmpeg failed on {rgb.name}")
        part_tmp.rename(part); parts.append(part)
        print(f"video: {rgb.stem} {n} frames")
    if still is not None:
        print(f"stills in {tmp}"); return
    lst = tmp / "list.txt"
    lst.write_text("".join(f"file '{p.resolve()}'\n" for p in parts))
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", str(lst), "-c", "copy",
                    "-movflags", "+faststart", str(out)], check=True)
    print(f"video: {out}")


if __name__ == "__main__":
    main()
