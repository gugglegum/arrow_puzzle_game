#!/usr/bin/env python3
"""Check real Linux executables with isolated user or portable storage."""

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import signal
import struct
import subprocess
import tempfile
import time

from build_features import check_production, check_storage


def production_smoke(executable, appimage, work, env, settings_path):
    """Drive the real packaged binary through X11, without in-game QA hooks."""
    if shutil.which("xdotool") is None:
        raise RuntimeError("The production smoke check needs xdotool (run under X11 or Xvfb)")
    # These must be ignored: in particular the game must remain in the main menu
    # and must not capture a frame or exit before the explicit Escape below.
    env = env | {
        "ARROWPUZZLE_SCREENSHOT": "unexpected.png",
        "ARROWPUZZLE_SCREENSHOT_FRAME": "1",
        "ARROWPUZZLE_SCREENSHOT_MENU": "3",
        "ARROWPUZZLE_SEED": "747",
        "ARROWPUZZLE_DIFFICULTY": "1",
        "ARROWPUZZLE_RESUME": "1",
        "ARROWPUZZLE_SAVE_ON_EXIT": "1",
    }
    command = [str(executable)] + (["--appimage-extract-and-run"] if appimage else [])
    # Xvfb starts its pointer in the screen centre. On a page transition that
    # stationary pointer can select a setting beneath it, overriding the
    # keyboard fixture's assumed first row. Start outside all menu rows.
    subprocess.run(["xdotool", "mousemove", "0", "0"], check=True, timeout=5)
    with (work / "production.log").open("w") as log:
        process = subprocess.Popen(command, cwd=work, env=env, stdout=log,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        try:
            window = None
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline and process.poll() is None:
                found = subprocess.run(
                    ["xdotool", "search", "--onlyvisible", "--name", "^Arrow Puzzle$"],
                    text=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=3)
                for candidate in found.stdout.split():
                    owner = subprocess.run(["xdotool", "getwindowpid", candidate], text=True,
                                           stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                           timeout=3)
                    try:
                        if os.getpgid(int(owner.stdout.strip())) == process.pid:
                            window = candidate
                            break
                    except (ValueError, ProcessLookupError):
                        continue
                if window is not None:
                    break
                time.sleep(0.1)
            if window is None:
                raise RuntimeError(f"Production game did not open its window; see {work / 'production.log'}")
            subprocess.run(["xdotool", "windowfocus", "--sync", window], check=True, timeout=5)
            time.sleep(0.3)
            for key in ("F12", "ctrl+F12", "ctrl+F12"):
                subprocess.run(["xdotool", "key", "--window", window, "--delay", "80", key],
                               check=True, timeout=5)
                time.sleep(0.2)
            if process.poll() is not None:
                raise RuntimeError("Production game exited in response to developer capture variables")
            if list(work.rglob("*.png")) or list(work.rglob("*.gif")):
                raise RuntimeError("F12/Control+F12 or developer variables created a capture")
            def key(name):
                # Use XTest into the focused window, as a real keyboard does;
                # --window sends synthetic XSendEvent messages instead.
                subprocess.run(["xdotool", "key", "--clearmodifiers", "--delay", "80", name],
                               check=True, timeout=5)
                time.sleep(0.15)

            def expect_language(code, action="initial selection"):
                contents = settings_path.read_text()
                if f"language={code}\n" not in contents:
                    raise RuntimeError(f"Language selector did not save {code} after {action}: {contents!r}; "
                                       f"see {work / 'production.log'}")

            # Fresh menu: Down skips disabled Resume and selects Settings.
            # Up wraps from the first setting to Back, then to Language.
            for name in ("Down", "Return", "Up", "Up"):
                key(name)
            expect_language("auto")
            for name, language in (("Right", "en"), ("Left", "auto"),
                                   ("Left", "uk"), ("Right", "auto"),
                                   ("space", "en"), ("Return", "es")):
                key(name)
                expect_language(language, name)
            # Return to Auto for later smoke scenarios; the other preferences
            # and every save remain independent of the interface language.
            for name in ("Left", "Left"):
                key(name)
            expect_language("auto")
            key("Escape")
            print("Production language selector: Left/Right, wraparound, Enter/Space and saved values passed")
            subprocess.run(["xdotool", "keydown", "--window", window, "Escape"], check=True, timeout=5)
            if process.wait(timeout=15) != 0:
                raise RuntimeError("Production game did not exit cleanly")
            if not settings_path.is_file():
                raise RuntimeError("Production game did not initialize isolated settings")
            print("Production startup, ignored QA variables, F12/Control+F12, and clean exit passed")
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGTERM)
                process.wait(timeout=10)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--appimage", action="store_true", help="Use the FUSE-free AppImage extraction path")
    parser.add_argument("--debug-hooks", action="store_true", help="Run capture scenarios with a Debug build")
    parser.add_argument("--portable", action="store_true", help="Expect files beside the copied program")
    parser.add_argument("--output-dir", type=Path, default=Path("out/linux-smoke"))
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="run-", dir=args.output_dir.resolve()))
    program = work / "program"
    program.mkdir()
    executable = program / ("ArrowPuzzle.AppImage" if args.appimage else "ArrowPuzzle")
    shutil.copy2(args.executable.resolve(), executable)
    executable.chmod(0o555)
    program.chmod(0o755 if args.portable else 0o555)
    env = {key: value for key, value in os.environ.items() if not key.startswith("ARROWPUZZLE_")}
    env.update(XDG_CONFIG_HOME=str(work / "config"), XDG_DATA_HOME=str(work / "data"))
    settings_path = program / "settings.txt" if args.portable else work / "config/arrow-puzzle/settings.txt"
    save_path = program / "savegame.txt" if args.portable else work / "data/arrow-puzzle/savegame.txt"

    def check_writes(saved=False):
        expected = {executable}
        if args.portable:
            expected.add(settings_path)
            if saved:
                expected.add(save_path)
            if (work / "config").exists() or (work / "data").exists():
                raise RuntimeError("Portable game wrote into XDG directories")
        if set(program.iterdir()) != expected:
            raise RuntimeError("Unexpected files beside the executable")

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
        if not args.debug_hooks:
            if not args.appimage:
                check_production(executable.read_bytes(), args.portable)
            production_smoke(executable, args.appimage, work, env, settings_path)
            check_writes()
            print(f"{'Portable' if args.portable else 'User'} storage verified: {settings_path}")
            return
        if args.appimage or b"ARROWPUZZLE_SCREENSHOT" not in executable.read_bytes():
            raise RuntimeError("--debug-hooks requires a Debug executable with screenshot hooks")
        check_storage(executable.read_bytes(), args.portable)
        capture("menu", ARROWPUZZLE_SCREENSHOT_MENU="1")
        capture("new-game", ARROWPUZZLE_SCREENSHOT_MENU="2")
        capture("settings", ARROWPUZZLE_SCREENSHOT_MENU="3")
        capture("no-audio", ARROWPUZZLE_SCREENSHOT_MENU="1",
                PULSE_SERVER=f"unix:{work}/unavailable-pulse", ALSA_CONFIG_PATH="/dev/null")
        capture("board-dark", ARROWPUZZLE_SEED="747", ARROWPUZZLE_DIFFICULTY="1",
                ARROWPUZZLE_SCREENSHOT_REMOVE="3", ARROWPUZZLE_SAVE_ON_EXIT="1")
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
        check_writes(saved=True)
        print(f"Save SHA-256: {hashlib.sha256(saved).hexdigest()}")
        print(f"Smoke checks passed; inspect screenshots in {work}")
    finally:
        program.chmod(0o755)


if __name__ == "__main__":
    main()
