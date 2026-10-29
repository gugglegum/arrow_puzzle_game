# Native Linux development

**English** | [Русский](../ru/LINUX.md)

Windows remains the main development platform. Use separate Windows and Linux
clones and exchange completed changes through GitHub; do not share build trees.
The initial Linux target is x86-64 with X11 (or XWayland in a Wayland session)
and desktop OpenGL 3.3. Native Wayland, ARM64 and Android need separate work.

## Build

On Linux Mint 22.3 / Ubuntu 24.04, install build dependencies once:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build git python3 \
    libasound2-dev libx11-dev libxrandr-dev libxi-dev \
    libgl1-mesa-dev libxcursor-dev libxinerama-dev
```

CMake 3.24+ is required, including for presets (schema 5). Older distributions
may need a newer CMake than their default package. The first configuration
downloads the exact pinned raylib 5.5 revision; no system raylib is needed.

From the repository root:

```bash
./tools/build-linux.sh
./build/linux/arrow_puzzle
```

The helper configures Release, builds with two workers, and runs all five
headless CTest suites. It forwards additional CMake configuration arguments.
Use `--debug` for `build/linux-debug`; `CMAKE_BUILD_PARALLEL_LEVEL` controls
parallelism. Equivalent commands:

```bash
cmake --preset linux
cmake --build --preset linux --parallel 2
ctest --preset linux
```

All artwork, fonts, and sound effects are embedded. The executable does not
need runtime files from the repository and can start from another directory.
raylib is linked statically; system C/C++ libraries, X11, OpenGL drivers, and
an optional audio service are still runtime dependencies.

## Settings and saves

| File | Default Linux location | Absolute override |
| --- | --- | --- |
| Settings | `~/.config/arrow-puzzle/settings.txt` | `$XDG_CONFIG_HOME/arrow-puzzle/settings.txt` |
| Saved game | `~/.local/share/arrow-puzzle/savegame.txt` | `$XDG_DATA_HOME/arrow-puzzle/savegame.txt` |

Unset, empty or relative XDG variables use the defaults. An absent/invalid
`HOME` is resolved through the user's account. Directories are created when
writing, so the executable can live in a read-only directory. Windows keeps
both files beside its EXE. Existing version-1 text formats are unchanged;
to transfer progress, copy the two files to the corresponding OS locations
while the game is closed. Adjacent Linux files are not imported automatically.

Use temporary absolute XDG paths for QA so tests do not touch personal progress:

```bash
mkdir -p "$PWD/out/linux-smoke"
cd out/linux-smoke
XDG_CONFIG_HOME="$PWD/config" XDG_DATA_HOME="$PWD/data" \
    ARROWPUZZLE_SCREENSHOT=menu.png ARROWPUZZLE_SCREENSHOT_MENU=1 \
    ARROWPUZZLE_SCREENSHOT_FRAME=4 ../../build/linux/arrow_puzzle
```

raylib writes screenshots relative to the initial working directory; use a
filename and run in the desired output directory. Inspect the resulting image.

## Desktop validation

```bash
cmake --build build/linux --parallel 2 --target \
    arrow_render_tests arrow_ui_text_tests arrow_audio_tests
./build/linux/arrow_render_tests
./build/linux/arrow_ui_text_tests
./build/linux/arrow_audio_tests assets/sounds
```

Rendering/text checks need an X11 display and OpenGL 3.3. An Xvfb display with
Mesa software rendering can exercise them in CI; it does not measure GPU
performance. The audio test needs a playback device, observes the game's mixer,
and silences its output. It does not measure speaker or VMware audio latency.

Inspect Main/New game/Settings, both board themes, HUD and results. Exercise
click/drag, wheel and Ctrl +/- / Ctrl+0, save/resume, F11 and Alt+Enter, Escape,
Alt-Tab, window resizing and idle resource use. Linux fullscreen uses the X11
window manager's EWMH request: the manager restores the window and display
resolution remains unchanged. Windows retains its existing borderless path.

Mint/VMware testing does not establish bare-metal performance, another window
manager's behavior, XWayland behavior, fractional scaling or multi-monitor
support. The reported Wine sound delay has not been attributed to a cause.

Sources: [raylib Linux dependencies](https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux),
[XDG directories](https://specifications.freedesktop.org/basedir/latest/),
[EWMH fullscreen](https://specifications.freedesktop.org/wm/latest/ar01s05.html).
