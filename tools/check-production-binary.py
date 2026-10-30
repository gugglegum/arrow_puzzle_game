#!/usr/bin/env python3
"""Reject developer capture hooks and built-in raylib hotkeys in a game binary."""

import argparse
from pathlib import Path
import re


def check(executable):
    data = executable.read_bytes()
    forbidden = set(re.findall(
        rb"ARROWPUZZLE_(?:SCREENSHOT[A-Z_]*|SEED|DIFFICULTY|RESUME|SAVE_ON_EXIT)", data))
    for marker in (b"arrow-puzzle-screenshot.png", b"screenshot%03i.png", b"screenrec%03i.gif"):
        if marker in data:
            forbidden.add(marker)
    if forbidden:
        names = ", ".join(sorted(marker.decode("ascii") for marker in forbidden))
        raise RuntimeError(f"Capture functionality found in production binary: {names}")
    print(f"Production binary has no screenshot hooks or capture hotkeys: {executable}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    check(parser.parse_args().executable)
