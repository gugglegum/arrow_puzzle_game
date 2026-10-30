#!/usr/bin/env python3
"""Regenerate the embedded CJK font. Normal builds use the checked-in subset.

Requires fonttools 4.61.1. The input must be the pinned Google Fonts original;
see assets/fonts/README.md for its source and digest.
"""
import argparse
import hashlib
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SHA256 = "a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path)
    args = parser.parse_args()
    if hashlib.sha256(args.original.read_bytes()).hexdigest() != SOURCE_SHA256:
        parser.error("The input is not the pinned Noto Sans SC source font")
    # Include every non-Latin/Cyrillic UI character and basic Latin for mixed
    # Chinese strings (key names, numbers, and the untranslated product name).
    source = (ROOT / "src/Localization.cpp").read_text(encoding="utf-8")
    codepoints = set(range(32, 127)) | {ord(c) for c in source if ord(c) >= 0x2E80}
    font = TTFont(args.original, recalcTimestamp=False)
    missing = codepoints - set(font.getBestCmap())
    if missing:
        raise ValueError(f"Source lacks required glyphs: {sorted(missing)}")
    instantiateVariableFont(font, {"wght": 500}, inplace=True)
    options = subset.Options()
    options.recalc_timestamp = False
    options.name_IDs = [0, 1, 2, 3, 4, 5, 6, 13, 14, 16, 17]
    subsetter = subset.Subsetter(options=options)
    subsetter.populate(unicodes=codepoints)
    subsetter.subset(font)
    names = {1: "Arrow Puzzle CJK", 2: "Medium", 3: "ArrowPuzzleCJK-Medium-1",
             4: "Arrow Puzzle CJK Medium", 6: "ArrowPuzzleCJK-Medium",
             16: "Arrow Puzzle CJK", 17: "Medium"}
    for record in font["name"].names:
        if record.nameID in names:
            record.string = names[record.nameID].encode(record.getEncoding())
    output = ROOT / "assets/fonts/ArrowPuzzleCJK-Medium.ttf"
    font.save(output)
    print(f"Embedded CJK subset: {len(codepoints)} characters, {output.stat().st_size} bytes")


if __name__ == "__main__":
    main()
