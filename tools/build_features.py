"""Check baked game features independently of filenames and package metadata."""

import re


def check_storage(data, portable):
    expected = b"ARROWPUZZLE_STORAGE=" + (b"portable" if portable else b"user")
    other = b"ARROWPUZZLE_STORAGE=" + (b"user" if portable else b"portable")
    if expected not in data or other in data:
        raise ValueError(f"Executable storage mode mismatch: expected {expected.decode('ascii')}")


def check_production(data, portable):
    check_storage(data, portable)
    forbidden = set(re.findall(
        rb"ARROWPUZZLE_(?:SCREENSHOT[A-Z_]*|SEED|DIFFICULTY|RESUME|SAVE_ON_EXIT)", data))
    for marker in (b"arrow-puzzle-screenshot.png", b"screenshot%03i.png", b"screenrec%03i.gif"):
        if marker in data:
            forbidden.add(marker)
    if forbidden:
        names = ", ".join(sorted(marker.decode("ascii") for marker in forbidden))
        raise ValueError(f"Capture functionality found in production binary: {names}")
