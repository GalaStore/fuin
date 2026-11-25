#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Builds assets/fonts/FuinIcons.ttf: the Material Symbols Rounded glyphs that Fuin refers to.

Every lowercase token found in a string literal of qml/ and src/ that is the name of a Material Symbols icon is kept.
The font is pinned to opsz 24 and wght 400; FILL and GRAD (-25..0) stay variable because qml/Icon.qml sets them.
"""
import argparse
import re
import sys
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

FAMILY = "Fuin Icons"
LITERAL = re.compile(r'"((?:[^"\\\n]|\\.)*)"|\'((?:[^\'\\\n]|\\.)*)\'')
TOKEN = re.compile(r"[a-z][a-z0-9_]{2,}")


def ligature_tables(font):
    for lookup in font["GSUB"].table.LookupList.Lookup:
        for sub in lookup.SubTable:
            sub = getattr(sub, "ExtSubTable", sub)
            if hasattr(sub, "ligatures"):
                yield sub


def icon_names(font):
    chars = {glyph: chr(code) for code, glyph in font.getBestCmap().items()}
    names = {}
    for sub in ligature_tables(font):
        for first, ligs in sub.ligatures.items():
            for lig in ligs:
                parts = [first, *lig.Component]
                if all(p in chars for p in parts):
                    names["".join(chars[p] for p in parts)] = lig.LigGlyph
    return names


def used_tokens(root):
    tokens = set()
    files = [*root.glob("qml/*.qml"), *root.glob("qml/*.js"), *root.glob("src/**/*.cpp")]
    for path in files:
        for m in LITERAL.finditer(path.read_text(encoding="utf-8")):
            tokens.update(TOKEN.findall(m.group(1) or m.group(2) or ""))
    return tokens


def keep_only(font, wanted):
    chars = {glyph: chr(code) for code, glyph in font.getBestCmap().items()}
    for sub in ligature_tables(font):
        for first in list(sub.ligatures):
            kept = [l for l in sub.ligatures[first] if "".join(chars.get(p, "?") for p in [first, *l.Component]) in wanted]
            if kept:
                sub.ligatures[first] = kept
            else:
                del sub.ligatures[first]


def rename(font):
    for rec in font["name"].names:
        if rec.nameID in (1, 16):
            rec.string = FAMILY
        elif rec.nameID in (3, 4, 6):
            rec.string = FAMILY.replace(" ", "") + ("-Regular" if rec.nameID == 6 else "")
        elif rec.nameID in (17,):
            rec.string = "Regular"


def main():
    root = Path(__file__).resolve().parent.parent
    ap = argparse.ArgumentParser()
    ap.add_argument("font", help="MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf")
    ap.add_argument("--out", default=str(root / "assets/fonts/FuinIcons.ttf"))
    ap.add_argument("--list", default=str(root / "assets/fonts/icons.txt"))
    args = ap.parse_args()

    font = TTFont(args.font, recalcTimestamp=False)
    available = icon_names(font)
    wanted = sorted(used_tokens(root) & available.keys())
    if not wanted:
        sys.exit("no icon names found")
    keep_only(font, set(wanted))
    opts = subset.Options()
    opts.layout_features = ["rlig", "rclt"]
    opts.hinting = False
    opts.notdef_outline = True
    opts.name_IDs = [1, 2, 3, 4, 5, 6, 13, 14, 16, 17]
    opts.recalc_bounds = True
    sub = subset.Subsetter(opts)
    sub.populate(unicodes=[ord(c) for c in "abcdefghijklmnopqrstuvwxyz0123456789_"])
    sub.subset(font)
    font = instancer.instantiateVariableFont(font, {"opsz": 24, "wght": 400, "GRAD": (-25, 0)})
    rename(font)
    font.save(args.out)
    Path(args.list).write_text("\n".join(wanted) + "\n", encoding="utf-8")
    print(f"{len(wanted)} icons, {Path(args.out).stat().st_size} bytes")


if __name__ == "__main__":
    main()
