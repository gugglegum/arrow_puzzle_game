#!/usr/bin/env python3
"""Run the real Linux executable with isolated data and a read-only program dir."""

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--appimage", action="store_true", help="Use the FUSE-free AppImage extraction path")
    parser.add_argument("--output-dir", type=Path, default=Path("out/linux-smoke"))
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="run-", dir=args.output_dir.resolve()))
    program = work / "program"
    program.mkdir()
    executable = program / ("ArrowPuzzle.AppImage" if args.appimage else "ArrowPuzzle")
    shutil.copy2(args.executable.resolve(), executable)
    executable.chmod(0o555)
    program.chmod(0o555)
    env = {key: value for key, value in os.environ.items() if not key.startswith("ARROWPUZZLE_")}
    env.update(XDG_CONFIG_HOME=str(work / "config"), XDG_DATA_HOME=str(work / "data"))

    def capture(name, **overrides):
        image = work / (name + ".png")
        case_env = env | {
            "ARROWPUZZLE_SCREENSHOT": image.name,
            "ARROWPUZZLE_SCREENSHOT_FRAME": "4",
            **overrides,
        }
        with (work / (name + ".log")).open("w") as log:
            command = [str(executable)] + (["--appimage-extract-and-run"] if args.appimage else [])
            subprocess.run(command, cwd=work, env=case_env, stdout=log,
                           stderr=subprocess.STDOUT, timeout=90, check=True)
        header = image.read_bytes()[:24]
        if header[:8] != b"\x89PNG\r\n\x1a\n" or struct.unpack(">II", header[16:24]) != (1100, 900):
            raise RuntimeError(f"Invalid game screenshot: {image}")
        print(f"Passed: {name}", flush=True)

    try:
        capture("menu", ARROWPUZZLE_SCREENSHOT_MENU="1")
        capture("new-game", ARROWPUZZLE_SCREENSHOT_MENU="2")
        capture("settings", ARROWPUZZLE_SCREENSHOT_MENU="3")
        capture("no-audio", ARROWPUZZLE_SCREENSHOT_MENU="1",
                PULSE_SERVER=f"unix:{work}/unavailable-pulse", ALSA_CONFIG_PATH="/dev/null")
        capture("board-dark", ARROWPUZZLE_SEED="747", ARROWPUZZLE_DIFFICULTY="1",
                ARROWPUZZLE_SCREENSHOT_REMOVE="3", ARROWPUZZLE_SAVE_ON_EXIT="1")
        save_path = work / "data/arrow-puzzle/savegame.txt"
        settings_path = work / "config/arrow-puzzle/settings.txt"
        saved = save_path.read_bytes()
        if not saved.startswith(b"format=arrow-puzzle-save\nversion=1\n"):
            raise RuntimeError("The game did not create a version-1 save")
        # Windows text-file transfer must not alter restored geometry/state.
        save_path.write_bytes(saved.replace(b"\n", b"\r\n"))
        capture("resume-dark", ARROWPUZZLE_RESUME="1", ARROWPUZZLE_SAVE_ON_EXIT="1")
        if save_path.read_bytes() != saved:
            raise RuntimeError("Save/resume changed geometry or progress")
        settings = settings_path.read_text()
        if "board_theme=dark" not in settings:
            raise RuntimeError("Fresh settings did not select Dark")
        settings_path.write_text(settings.replace("board_theme=dark", "board_theme=light"))
        capture("resume-light", ARROWPUZZLE_RESUME="1", ARROWPUZZLE_SAVE_ON_EXIT="1")
        if save_path.read_bytes() != saved:
            raise RuntimeError("Changing the theme affected progress")
        if set(program.iterdir()) != {executable}:
            raise RuntimeError("The program wrote beside its executable")
        print(f"Save SHA-256: {hashlib.sha256(saved).hexdigest()}")
        print(f"Smoke checks passed; inspect screenshots in {work}")
    finally:
        program.chmod(0o755)


if __name__ == "__main__":
    main()
