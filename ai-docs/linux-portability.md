# Linux portability assessment

Assessment date: 2026-10-29. Source baseline: `830bdf7`, plus the existing
uncommitted pointer-control changes inspected in the working tree. This is a
source-level feasibility assessment. On 2026-10-29 the user accepted the proposed
initial sequence: native Linux x86-64, X11/XWayland, a tar archive, then AppImage.
This agreement does not establish a working Linux build or verified support.
No game code or build configuration was changed for this assessment.

## Conclusion

A native Linux desktop build should be feasible with the existing C++20,
CMake, raylib 5.5, and OpenGL 3.3 design. Keep one source tree and platform
conditionals. A new engine, a separate Linux fork, and a raylib upgrade are not
prerequisites established by this review. A successful Linux build and runtime
have not yet been demonstrated.

Start with Linux x86-64, the existing GLFW/X11 backend, and testing in both X11
and Wayland sessions through XWayland. Produce a tar archive first, then an
AppImage if the desired user experience is downloading one executable file.
Native Wayland and Flatpak are useful follow-up targets, with separate checks.

## Evidence and required work

| Area | Existing implementation | Consequence / proposed action |
| --- | --- | --- |
| Core | `arrow_core` contains generation, motion, collisions, and persistence; no raylib dependency | Preserve and exercise the same core with GCC/Clang; no gameplay rewrite is indicated |
| Entry point | `src/main.cpp` selects `WinMain` on Windows and ordinary `main` elsewhere | A Linux entry point already exists |
| Build | CMake has non-MSVC warning branches and pins raylib to `c1ab645ca298a2801097931d1079b10ff7eb9df8` | Use the pinned dependency initially; add Linux configure/build/test presets and a reproducible helper |
| Thread dependency | `Generator.cpp` uses `std::async`, but `Threads::Threads` is linked directly only to the GUI target | Put the thread dependency on `arrow_core` with suitable propagation; verify core test/benchmark linkage, particularly on older libc/toolchains |
| Persistence | `Persistence::executableDirectory()` already reads `/proc/self/exe`; non-Windows replacement uses `std::filesystem::rename` | Path discovery and replacement have Linux paths; the selected data directory still needs adaptation |
| Text | `UiText.cpp` guards GDI with `_WIN32` and has a size-matched embedded Inter/raylib fallback | Compilation does not require GDI on Linux; assess fallback text appearance and metrics before choosing extra dependencies |
| Graphics | Board shaders explicitly use GLSL 330 | Retain desktop OpenGL 3.3; OpenGL ES, DRM-only devices, and WebGL require additional shader/backend work |
| Audio/assets | Inter and WAV effects are embedded; `GameAudio` uses raylib and handles failure to open a device | No loose runtime assets or game network service are required; verify Linux playback and operation without an audio device |
| Fullscreen | `Game.cpp` calls raylib's `ToggleBorderlessWindowed`; Windows-only overscan/topmost adjustment is guarded | Check focus, Alt-Tab, restoration, monitor choice, and topmost behavior on Linux; native Wayland needs special consideration below |
| Delivery | Helpers and the only build job in `.github/workflows/release.yml` build Windows x64 | Add a Linux build/test/package job, archives, checksums, and provenance; the Ubuntu job currently only assembles releases |

### User data is the main functional packaging blocker

`Game::run()` constructs `Persistence(Persistence::executableDirectory())`.
Both `settings.txt` and `savegame.txt` therefore go beside the executable.
This can work in a writable extracted directory, but not in a normal read-only
AppImage mount, Flatpak's `/app`, or a system installation such as `/usr/bin`.

Proposed Linux defaults:

- settings: `$XDG_CONFIG_HOME/arrow-puzzle/settings.txt`, with
  `$HOME/.config/arrow-puzzle/settings.txt` as the unset/empty fallback;
- saves: `$XDG_DATA_HOME/arrow-puzzle/savegame.txt`, with
  `$HOME/.local/share/arrow-puzzle/savegame.txt` as the unset/empty fallback.

Respect absolute XDG overrides; ignore invalid relative overrides. The current
single-directory persistence interface would need to accept separate locations
for this split, or an explicitly chosen single user-data directory as a simpler
first step. Keep Windows portable behavior. An optional Linux portable mode and
one-time import of existing adjacent files are product choices, not requirements
already accepted by the user. Save serialization stores geometry rather than
only a seed, so format changes are not inherently needed for the port; verify
cross-platform file transfer rather than asserting it from inspection alone.

### X11, XWayland, and native Wayland

The exact fetched raylib 5.5 `CMakeOptions.txt` defaults to
`GLFW_BUILD_X11=ON`, `GLFW_BUILD_WAYLAND=OFF`, and embedded GLFW. An X11 client
can run within a Wayland session when XWayland is available. This is still a
native Linux binary; it does not require Wine.

The bundled GLFW supports compiling both backends with
`GLFW_BUILD_X11=ON` and `GLFW_BUILD_WAYLAND=ON`. Native Wayland additionally
needs Wayland/xkbcommon development files, pkg-config, and wayland-scanner.
Enabling compilation alone is insufficient acceptance evidence.

In the pinned backend, raylib's borderless implementation uses
`glfwSetWindowPos`, floating/topmost attributes, and manual monitor-sized
resizing. GLFW's Wayland implementation explicitly rejects application-set
window positions and floating windows. Thus borderless/fullscreen behavior may
need a compositor-compatible path on Wayland. Do not promise native Wayland
support by changing one CMake option or claim that all of Wayland is unsupported.
Check GNOME and KDE, fractional scaling, framebuffer/input coordinate mapping,
and multi-monitor transitions on actual desktops.

### Font options

1. Initially use the existing raylib fallback and inspect menus/HUD/results at
   their actual sizes. No new font dependency is necessary for a first build.
2. If visual quality is insufficient, add a Linux FreeType-backed rasterizer
   inside `UiText`, using the same embedded Inter font and bounded caches.
   Include its license and measure output, layout, and resource use. Matching
   Windows ClearType pixel-for-pixel is not a sensible cross-platform criterion.

The native GDI pixel/composition assertions in `UiTextTests.cpp` are skipped
outside Windows; the remaining cache checks are not sufficient evidence of Linux
font quality. Add platform-neutral nonempty-glyph/layout checks if implementing
the port, together with real screenshots.

## Build and distribution choices

Build requirements are a C++20-capable GCC or Clang, CMake, Ninja or Make, Git,
and development packages for OpenGL/Mesa, X11 and its required extensions, and
Linux audio. Native Wayland adds the packages above. The project fetches raylib
from Git during initial configuration, unless its sources are supplied locally;
raylib does not need to be installed as a separate runtime package.

The root CMake file requires 3.24, but `CMakePresets.json` uses schema version 6,
which requires CMake 3.25. Either document 3.25+ for presets or revise the schema
if retaining 3.24 compatibility. Direct `cmake -S . -B ...` avoids this preset
schema requirement, but still needs CMake 3.24+.

For a first probe after installing dependencies, use a fresh `build/linux` tree
with `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Release`, followed
by `cmake --build build/linux --parallel 2` and
`ctest --test-dir build/linux --output-on-failure`. These commands are proposed,
not commands successfully executed by this assessment.

| Delivery option | Benefit | Cost / limit |
| --- | --- | --- |
| Native binary in `.tar.gz` | Small, simple initial artifact; no package manager required | Declare runtime libraries and minimum libc/libstdc++ versions; test clean systems |
| AppImage | Download one file; no system-wide installation | Select a compatible build baseline and bundle appropriate libraries; usual runtimes need FUSE or an extraction fallback; writable data must be outside the mounted image |
| Flatpak / possible Flathub publication | Consistent runtime, desktop integration, and update distribution | Manifest, application ID, icon, desktop file, metadata, runtime maintenance, and deliberate display/audio/GPU permissions; publication is a separate action |
| `.deb` / `.rpm` | Native package-manager integration | Separate distribution/version dependency policies and maintenance; not necessary for the first release |
| Build from source | Low packaging effort for technical users | Requires toolchain/development dependencies; does not give ordinary players a ready application |
| Existing Windows EXE under Wine | Quick compatibility experiment without porting | Requires Wine; this game's rendering, GDI text, audio, and save behavior remain unverified there; not native Linux support |

Static raylib does not mean a fully static Linux executable. Declare the
supported libc/C++ runtime baseline and inspect the resulting ELF dependencies;
host graphics drivers and desktop services still matter. AppImage does not make
a binary built on a newer glibc universally compatible with older distributions.
Build in a pinned environment suitable for the oldest promised target and test
the actual packaged result elsewhere. Avoid using a moving `ubuntu-latest` image
as the implicit compatibility contract.

For a future Flatpak build, provide the pinned raylib source through the
manifest/cache, rather than relying on configure-time network fetching. Allow
GPU, display, and optional audio access, with no game network permission needed.
An X11-only build needs the X11 socket; Wayland plus fallback-X11 permission is
appropriate only after the app actually supports native Wayland. Installed
games can remain offline; downloading dependencies/packages/updates is separate.

Linux CI, a local Linux machine, or a VM can produce the binary. WSL2 can be
useful for compile/test iteration on Windows, but WSLg/VM graphics are not
substitutes for final desktop graphics/audio/fullscreen testing. Containers are
useful build environments, rather than an end-user launch requirement.

## Proposed milestones and acceptance evidence

1. **Build probe:** select Linux x86-64 and an explicit minimum system/toolchain;
   configure and build the pinned sources, fix demonstrated compiler/link issues,
   and run the full default CTest suite. Preserve unrelated pointer-control work.
2. **Desktop behavior:** choose writable user-data paths; verify settings and
   resume across launches, including read-only program directories. Check core
   regressions, corrupt input, audio enabled/disabled/missing-device, text, board
   themes, pointer gestures, zoom/fit, fullscreen/focus, and idle CPU/GPU load.
   Run the existing explicit render/text/audio tests where their hardware exists.
3. **First release artifact:** make a tar archive and/or AppImage with licenses,
   documentation, executable permission, checksums, build metadata/provenance,
   and startup independent of the source/build tree. Add repeatable Linux CI.
   Validate the minimum target and a second Linux desktop/distribution.
4. **Additional coverage:** assess native Wayland, then Flatpak if needed.
   ARM64 is a separate build/test target with compatible desktop OpenGL support,
   not a promise that every ARM board will run the current renderer.

Peak-memory output from `GeneratorBenchmark.cpp` currently uses Windows PSAPI
and returns unavailable elsewhere. A Linux benchmark can use external tools or
a scoped `getrusage` implementation; this is tooling work, not a gameplay blocker.

## Verification limits and upstream references

This assessment inspected source, CMake, tests, documentation, the Windows-only
release workflow, and the exact locally fetched raylib/GLFW source. It did not
compile or run a Linux binary. A local WSL inventory request returned
`Wsl/E_ACCESSDENIED`; this does not establish whether a usable distro is installed.
No environment installation, workflow run, Linux implementation, or release was
performed. No timing, memory, or distribution-compatibility claims are measured.

- [raylib GNU/Linux build guide](https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux)
- [Pinned raylib 5.5 build options](https://github.com/raysan5/raylib/blob/c1ab645ca298a2801097931d1079b10ff7eb9df8/CMakeOptions.txt)
- [GLFW window guide and Wayland position limits](https://www.glfw.org/docs/latest/window_guide.html)
- [XDG base-directory specification](https://specifications.freedesktop.org/basedir/latest/)
- [CMake preset schema versions](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html#schema)
- [AppImage compatibility practices](https://docs.appimage.org/reference/best-practices.html)
- [AppImage FUSE and extraction fallback](https://docs.appimage.org/user-guide/troubleshooting/fuse.html)
- [Flatpak runtime model](https://docs.flatpak.org/en/latest/introduction.html)
- [Flatpak display/audio/GPU and user-data permissions](https://docs.flatpak.org/en/latest/sandbox-permissions.html)
- [FreeType overview](https://freetype.org/freetype2/docs/index.html)
- [Wine overview](https://www.winehq.org/about)
