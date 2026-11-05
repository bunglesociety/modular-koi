#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Make the Modular Koi site (GitHub Pages: bunglesociety.github.io/modular-koi, or any https host):

  index.html                              the home: web/koi-fm1/block.html (the same block as the bunglesociety.com
                                          page) in a page of its own, the pictures and links made relative
  install/index.html                      web/koi-fm1/install.html with fm1pkg.js, fm1ota.js, fm1probe.js and the metadata
  img/                                    web/koi-fm1/img (GIFs: tests/koi_fm1.c KOI_GIFS; keys: tools/make_koi_keys.py)
  firmware/modular-koi-VER.fwsc          the package (+ LICENSE, LICENSING.md, LICENSES/: the package holds three
                                          JieLi SDK files under Apache-2.0)
  .nojekyll                               (GitHub Pages: serve the files as they are)

  tools/make_koi_site.py build/modular-koi-fm1-0.1.fwsc 0.1 OUT_DIR

Try it before publishing:  cd OUT_DIR && python3 -m http.server 8000  ->  http://localhost:8000
(Web MIDI needs https or localhost.)
"""
import re
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from make_m1_installer import ROOT, WEB, check, page   # noqa: E402

SITE = "https://bunglesociety.github.io/modular-koi/"


def home():
    block = (WEB / "koi-fm1" / "block.html").read_text(encoding="utf-8")
    style = block[block.index("<style>") + 7:block.index("</style>")]
    style = style[:style.index("html, body, #siteWrapper")] + style[style.index(".mkw {"):]   # (the Squarespace page rules)
    body = block[block.index("</style>") + 8:].replace(SITE, "")                            # relative links
    return f"""<!doctype html>
<!-- SPDX-License-Identifier: GPL-3.0-only. Modular Koi (Alexis Arakis, Bungle Society): tools/make_koi_site.py -->
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Modular Koi</title>
<meta name="description" content="Modular Koi: a free firmware that turns the M-VAVE FM-1 into a steel tongue drum played with the thumbs, over a pond of koi. By Bungle Society.">
<style>
html, body {{ margin: 0; background: #c9d3a6; }}
{style}
.mkw {{ max-width: 1120px; margin: 0 auto; min-height: 100vh; }}
</style>
</head>
<body>
{body.strip()}
</body>
</html>
"""


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = check(raw, pkg)
    name = f"modular-koi-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    inst, fw, img = out / "install", out / "firmware", out / "img"
    for d in (inst, fw, img):
        d.mkdir(parents=True, exist_ok=True)
    for old in fw.glob("*.fwsc"):                    # one package: the current one
        old.unlink()
    shutil.copy(pkg, fw / name)
    (inst / "index.html").write_text(page({"version": version, "product": product, "family": "koi", "name": "Modular Koi",
                                           "pkg": "../firmware/" + name},
                                          "../img/bassin.gif", None, fm1=True, koi=True),
                                     encoding="utf-8")
    for f in (WEB / "koi-fm1" / "img").iterdir():
        shutil.copy(f, img / f.name)
    (out / "index.html").write_text(home(), encoding="utf-8")
    (fw / "LICENSES").mkdir(exist_ok=True)
    for f in (ROOT / "LICENSES").glob("*.txt"):
        shutil.copy(f, fw / "LICENSES" / f.name)
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(ROOT / doc, fw / doc)
    (out / ".nojekyll").write_text("", encoding="utf-8")
    print(f"site: {out}: index.html, install/, img/ ({len(list(img.iterdir()))}), firmware/{name} ({len(raw)} B, {product})")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
