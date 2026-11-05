#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular Compagnon: the installer as ONE file to open from the disk in Chrome (file://, nothing sent anywhere): the
site's install page (web/compagnon/install.html) with the package and the cat's picture inside.

  tools/make_m1_installer.py PACKAGE.fwsc OUT.html [VERSION]
  tools/make_m1_installer.py --koi PACKAGE.fwsc OUT.html [VERSION]     Modular Koi (web/koi-fm1, its pond as the picture)
The FM-1's (an FM-1 package, BOARD=fm1c: web/compagnon-fm1/install.html, no controls picture) is chosen by the package.
"""
import base64
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WEB = ROOT / "web"


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    return "".join(chr((m - i - 1) & 0xFF) for i in range(20) if (m := raw[i * 0x30 + 0x2F]) != 0x7D)


def page(meta, hero_src, map_src, fm1=False, koi=False):
    html = (WEB / ("koi-fm1" if koi else "compagnon-fm1" if fm1 else "compagnon") / "install.html").read_text(encoding="utf-8")
    lib = "\n".join(strip_module((WEB / f).read_text(encoding="utf-8")) for f in ("fm1pkg.js", "fm1ota.js", "fm1probe.js"))
    for mark, val in (("/*LIB*/", lib), ("/*META*/", json.dumps(meta)), ("/*HERO*/", hero_src), ("/*MAP*/", json.dumps(map_src))):
        if fm1 and mark == "/*MAP*/":
            continue
        if html.count(mark) != 1:
            raise SystemExit(f"install.html must contain {mark} once")
        html = html.replace(mark, val)
    return html


def check(raw, path):
    product = product_of(raw)
    if not re.fullmatch(r"(SMK-37 Elite|FM-1)_9\d\d", product) or b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{path}: not a Modular Compagnon SMK-37 Elite or FM-1 package (identity {product!r})")
    return product


def main(pkg, out, version="beta", koi=False):
    raw = Path(pkg).read_bytes()
    product = check(raw, pkg)
    gif = WEB / "koi-fm1" / "img" / "bassin.gif" if koi else WEB / "compagnon" / "img" / "joue.gif"
    hero = "data:image/gif;base64," + base64.b64encode(gif.read_bytes()).decode()
    img = WEB / "compagnon" / "img"
    maps = {l: "data:image/png;base64," + base64.b64encode((img / f).read_bytes()).decode()
            for l, f in (("fr", "commandes-fr.png"), ("en", "controls-en.png"))}
    fam, nm = ("koi", "Modular Koi") if koi else ("compagnon", "Modular Compagnon")
    html = page({"version": version, "product": product, "family": fam, "name": nm, "local": 1,
                 "b64": base64.b64encode(raw).decode()}, hero, maps, product.startswith("FM-1"), koi)
    Path(out).write_text(html, encoding="utf-8")
    print(f"installer: {out} ({len(html)} B, {product})")


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--koi"]
    if len(args) not in (2, 3):
        sys.exit(__doc__)
    main(*args, koi="--koi" in sys.argv[1:])
