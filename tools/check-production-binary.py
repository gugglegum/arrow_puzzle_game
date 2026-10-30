#!/usr/bin/env python3
"""Reject developer capture hooks and built-in raylib hotkeys in a game binary."""

import argparse
from pathlib import Path
from build_features import check_production


def check(executable, portable=False):
    check_production(executable.read_bytes(), portable)
    print(f"Production binary has expected storage mode and no capture features: {executable}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--portable", action="store_true", help="Expect baked-in portable storage")
    args = parser.parse_args()
    check(args.executable, args.portable)
