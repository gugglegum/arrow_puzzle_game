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
sudo apt-get install build-essential cmake ninja-build git python3 curl \
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

The helper configures Release, builds with two workers, and runs all six
headless CTest suites. It forwards additional CMake configuration arguments.
It builds `arrow_puzzle` (user storage) and `arrow_puzzle_portable` (adjacent
storage) from the same sources.
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

The table applies to ordinary tar.gz and AppImage builds:

| File | Default Linux location | Absolute override |
| --- | --- | --- |
| Settings | `~/.config/arrow-puzzle/settings.txt` | `$XDG_CONFIG_HOME/arrow-puzzle/settings.txt` |
| Saved game | `~/.local/share/arrow-puzzle/savegame.txt` | `$XDG_DATA_HOME/arrow-puzzle/savegame.txt` |

Unset, empty or relative XDG variables use the defaults. An absent/invalid
`HOME` is resolved through the user's account. Directories are created when
writing, so the executable can live in a read-only directory. Ordinary Windows
builds use `%LOCALAPPDATA%\ArrowPuzzle\`. Existing version-1 text formats are unchanged;
to transfer progress, copy the two files to the corresponding OS locations
while the game is closed. Adjacent Linux files are not imported automatically.

Packages ending in `-portable.tar.gz` or `-portable.AppImage` instead store both
files beside the executable or outer AppImage. The mode is compiled into
`arrow_puzzle_portable`; neither a marker file nor a particular filename is
required. XDG variables are ignored and an unwritable portable directory does
not cause a fallback to user storage. Keep the portable directory writable.
AppImage managers that move only the image do not move its adjacent data.

Screenshot hooks exist only in Debug; Release and RelWithDebInfo exclude them.
All configurations disable capture hotkeys.
Use temporary absolute XDG paths for QA so tests do not touch personal progress:

```bash
cmake --preset linux-debug
cmake --build build/linux-debug --parallel 2 --target arrow_puzzle
mkdir -p "$PWD/out/linux-smoke"
cd out/linux-smoke
XDG_CONFIG_HOME="$PWD/config" XDG_DATA_HOME="$PWD/data" \
    ARROWPUZZLE_SCREENSHOT=menu.png ARROWPUZZLE_SCREENSHOT_MENU=1 \
    ARROWPUZZLE_SCREENSHOT_FRAME=4 ../../build/linux-debug/arrow_puzzle
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

## Development archive and CI

```bash
python3 tools/package-linux.py --version dev-local
cd out/linux-package
sha256sum --check --ignore-missing SHA256SUMS.txt
tar -xzf ArrowPuzzle-dev-local-linux-x64.tar.gz
cd ArrowPuzzle-dev-local-linux-x64
sha256sum --check --ignore-missing ../SHA256SUMS.txt
./ArrowPuzzle
```

The helper requires Release, rebuilds the executable, installs/strips a copy
(removing build RPATHs), and archives it with documentation, licenses and
`build-info.json`. The metadata records compiler, raylib commit, source state,
Actions run when applicable, ELF dependencies and required symbol versions.
It refuses to overwrite package/metadata files; choose `--output-dir` for the
next package. To check a downloaded binary:

```bash
python3 tools/smoke-linux.py /absolute/path/to/ArrowPuzzle
```

This production check requires `xdotool` and an X11 display (or Xvfb). It uses
external input to verify startup, ignored QA variables, no files on F12 or
Control+F12, and clean Escape shutdown. It does not rely on in-game hooks.
For the separate deterministic QA scenarios, use:

```bash
python3 tools/smoke-linux.py build/linux-debug/arrow_puzzle --debug-hooks
python3 tools/check-production-binary.py build/linux/arrow_puzzle
```

**Build and test** (`.github/workflows/build.yml`) checks branch pushes, PRs and
manual runs on Windows/MSVC, Ubuntu 22.04/GCC and Ubuntu 24.04/Clang. Linux
includes Mesa/Xvfb rendering, glyphs and extracted-package smoke checks. Download
packages, logs and captures from the run's artifacts (14-day retention). It
never creates a tag or release; the attested Windows/Linux **Release** workflow remains separate; see [RELEASING.md](RELEASING.md).

Ubuntu 22.04/GCC is the intended oldest CI build baseline, not a promise about
all older systems. Check the artifact's `required_symbol_versions`: a build made
on Mint 22.3/GCC 13 needs newer glibc/libstdc++ than one built on Ubuntu 22.04.
X11/OpenGL drivers and system libraries must still be installed. Neither format
establishes native Wayland support or performance on another GPU.

## AppImage after tar validation

```bash
python3 tools/package-appimage.py out/linux-package/ArrowPuzzle-dev-local-linux-x64.tar.gz
chmod +x out/linux-package/ArrowPuzzle-dev-local-linux-x64.AppImage
./out/linux-package/ArrowPuzzle-dev-local-linux-x64.AppImage
```

The archive must come from the current source commit. The helper uses `curl`
for the first download of checksum-pinned appimagetool 1.9.1, then caches it
under `build/appimage-tools`. It reuses that tool's exact embedded runtime
with `--runtime-file`; there is no download of a moving runtime. AppRun, a
desktop entry, the project icon, runtime license/source notices and build
metadata are included. The game binary is identical to the tar archive's.
The helper appends the AppImage hash to `SHA256SUMS.txt` and refuses overwrites.

System game libraries and graphics drivers are not bundled: the AppImage has
the same glibc/libstdc++ requirements as its input tar archive. It is a single
file container, not universal compatibility with older Linux distributions.
Ordinary settings/saves remain in XDG directories outside the image. Portable
data lives beside the outer AppImage, including with `--appimage-extract-and-run`,
rather than in the temporary mount/extraction.

When FUSE is unavailable (including the current VMware guest), run without a
mount or system changes:

```bash
./out/linux-package/ArrowPuzzle-dev-local-linux-x64.AppImage --appimage-extract-and-run
python3 tools/smoke-linux.py out/linux-package/ArrowPuzzle-dev-local-linux-x64.AppImage --appimage
```

CI creates the AppImage on Ubuntu 22.04/GCC only, after the extracted tar passes,
and checks production startup/input through extraction-and-run. The seven
capture/save/resume scenarios run on a separate Debug build;
release packaging accepts only Release. Both platforms also scan the Release
binary for capture hook/hotkey signatures before release packaging.
Normal FUSE mounting is not verified in the current VM; extraction-and-run is.
Local checks also exercised real X11 Ctrl zoom/fit, a returning head drag that
must not launch, unchanged saved progress, Alt+Enter/Escape and focus switching.

## Both package variants

Use separate output directories for the metadata and checksum manifests. The
AppImage helper verifies the baked storage mode from the input archive:

```bash
python3 tools/package-linux.py --version dev-local --output-dir out/packages/user
python3 tools/package-linux.py --version dev-local --portable --output-dir out/packages/portable
python3 tools/package-appimage.py out/packages/user/ArrowPuzzle-dev-local-linux-x64.tar.gz
python3 tools/package-appimage.py out/packages/portable/ArrowPuzzle-dev-local-linux-x64-portable.tar.gz
```

CI uses `bash tools/check-linux-desktop.sh` for rendering and both Debug capture
scenarios, then `bash tools/check-linux-packages.sh dev-local --appimage` for
both tar archives and both AppImages. Add `--portable` to `smoke-linux.py` and
`check-production-binary.py` when checking the portable target. The smoke helper
copies/renames the executable alone, verifies adjacent files with no XDG writes
in portable mode, and checks read-only program directories in ordinary mode.
