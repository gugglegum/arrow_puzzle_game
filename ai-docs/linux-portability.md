# Linux portability assessment

## Implemented native baseline (2026-10-30)

The native build and desktop port are implemented on the shared source tree.
Use [the maintained Linux guide](../docs/en/LINUX.md), `tools/build-linux.sh`
and the `linux` / `linux-debug` presets. Windows remains the main platform;
Linux changes are transferred through GitHub, with separate local build trees.
The initial port work created no release or tag. Development tar/AppImage packaging and
pre-release CI are implemented. [Final implementation run 40952766100](https://github.com/gugglegum/arrow_puzzle_game/actions/runs/40952766100)
passed Windows/MSVC, Ubuntu 22.04/GCC and Ubuntu 24.04/Clang on `54845a2`,
including Linux rendering/text, tar and AppImage smoke tests. The downloaded
Ubuntu 22.04 Actions artifact was independently SHA-256-verified and passed all
seven AppImage scenarios again on Mint/VMware. It requires GLIBC 2.35 and
GLIBCXX 3.4.29, versus GLIBC 2.38 / GLIBCXX 3.4.31 for this VM's GCC 13 build.
These are measured symbol requirements, not a promise for every desktop.
Earlier CI exposed an MSVC
single-attempt test assumption and STRING-vs-FILEPATH compiler metadata parsing;
both were fixed. A bounded one-candidate failure is valid; aggregate comparisons
use successful paired seeds while all eight-candidate boards retain their floors.

- `package-linux.py` installs/strips a copy with no build RPATH, bundles docs
  and licenses, and records ELF symbol requirements/hashes/source metadata. The
  extracted archive passed all seven smoke scenarios locally. `build.yml`
  checks Windows/MSVC, Ubuntu 22.04/GCC and Ubuntu 24.04/Clang on push/PR/manual
  runs, with Mesa/Xvfb and downloadable packages/logs. No publication permissions.
- `package-appimage.py` wraps the validated tar using SHA-256-pinned
  appimagetool 1.9.1 and its embedded `caf24f9` runtime (explicit runtime-file,
  no moving runtime download). It includes icon/desktop/AppRun, runtime notices,
  source/build metadata and SHA-256. It bundles no game shared libraries, so
  the input binary's system-library baseline still applies. All seven local
  AppImage scenarios passed through extraction-and-run. CI builds/tests the
  AppImage on Ubuntu 22.04 after tar validation. `/dev/fuse` is absent in this
  VM: mounting is not verified and no host/kernel/FUSE settings were changed.
- Native X11 input checks passed: restored geometry/progress, Ctrl zoom and
  pixel-identical Ctrl+0 fit, a head drag returning to its origin without launch,
  Alt+Enter/Escape and focus switching. One quiet 20x20 board sample used
  97,056 KiB RSS and accumulated no CPU clock ticks over two seconds; this is
  a small VM observation, not a performance guarantee or GPU measurement.
- Mint 22.3 / Ubuntu noble, x86-64, GCC 13.3, CMake 3.28.3, Ninja 1.11.1.
  Native Release and all five default suites passed (full generator sample).
- X11/Cinnamon in VMware: Mesa 25.2.8 SVGA3D exposes OpenGL 4.3. Explicit
  rendering and platform-neutral glyph/layout checks passed. Main/Settings
  and Dark/Light boards were captured and visually inspected.
- Linux uses separate XDG config/data directories, with absolute overrides,
  HOME/account fallback and unchanged version-1 formats. Explicit directories
  remain available for tests. Windows portable paths are unchanged.
- `tools/smoke-linux.py` runs the real executable outside its build tree in a
  read-only program directory, checks menus and both themes, and verifies a
  CRLF save import plus byte-identical save/resume. QA uses isolated XDG paths.
- All embedded audio/mixer tests passed via PipeWire/PulseAudio; a menu launch
  with unavailable PulseAudio and ALSA also completed with the expected warning.
  End-to-end sound latency is not measured. See the user desktop follow-up below
  for the later laptop/mouse comparison; the VM delay cause remains unknown.
- raylib's manual borderless resize on Cinnamon became topmost and failed to
  restore the original window geometry. Linux now uses EWMH fullscreen through
  bundled GLFW's X11 handle. F11/Escape restored 1100x900 at the previous position
  from a 2560x1440 desktop, without ABOVE state. Windows keeps its existing path.
- libstdc++ random sequences exposed real quality gaps: Hard seed 747 had only
  one opening move; Simple seed 229 had a median length of five. Selection now
  enforces existing tested Hard opening and Simple shape/length thresholds;
  no eligible candidate means explicit failure. Tail-pass candidate comparison
  uses the aggregate fixed-seed sample, retaining all per-board floors.

Local evidence is ignored under `out/linux-qa` and `build/linux`; the downloaded
Actions package is in `out/github-linux-54845a2`. The final native implementation
and local packages were rebuilt from clean source. This evidence is not
required for a fresh checkout. System sudo needs a password in this VM;
official Ubuntu development packages were downloaded and extracted under
`build/linux-deps` instead of installed system-wide. The local CMake cache
records that prefix and Ninja path. Do not commit these machine paths or copy
the cache to Windows. Normal package installation is in the Linux guide.

Still unverified: native Wayland, XWayland session, other window managers,
multi-monitor/fractional scaling, older distributions and measured bare-metal performance.
The remainder below is the historical source assessment, superseded by this
implementation record wherever it describes work as unperformed.

## User desktop and sound follow-up (2026-10-30)

The user reports that native Linux in the VMware guest has a sound delay similar
to the Windows executable under Wine. On a separate laptop running Linux as its
main OS, a small initial apparent delay disappeared when using a mouse instead
of the touchpad. This provides user-reported native desktop/audio validation;
it is not an instrumented latency measurement or an independent agent test.
The laptop distribution, display session, exact build/package and input/audio
configuration were not specified. Delayed touchpad event handling is the user's
working hypothesis, not a diagnosed driver or game defect. Do not claim that
Wine alone causes the VM delay, or change audio buffering based only on this
observation.

Development CI is already configured for Linux and Windows. The later
[run 40953718746](https://github.com/gugglegum/arrow_puzzle_game/actions/runs/40953718746)
on `9900a0b` also passed all three jobs. `build.yml` creates development tar.gz,
AppImage and Windows ZIP artifacts on branch push, PR and manual dispatch.
`release.yml` now prepares Windows ZIP and Linux tar.gz/AppImage together,
with separate platform bundles/metadata, a common attested checksum manifest,
and a draft-only publication gate. A branch manual run leaves that gate off
while exercising the complete assembly/provenance path. See the release guide.
The user authorized this pipeline change, not publication or a new version tag.

The expanded pipeline passed a real manual branch run on implementation commit
`69b1e82`: [Release run 40963804710](https://github.com/gugglegum/arrow_puzzle_game/actions/runs/40963804710).
Windows and Linux builds, tests, signatures and common assembly all passed;
the draft job was skipped (`create_release=false`). The combined artifact
`release-69b1e821e5708cf99bdc7c36910cc660373c1dbe` contains ten release files.
After downloading it, its GitHub ZIP digest and all five checksum subjects
(three packages, two unpacked executables) were verified locally. Independent
`gh attestation verify --bundle` checks passed for both executables, all three
packages and SHA256SUMS.txt, requiring the exact source commit, release.yml
identity, repository and hosted runners. The four existing releases and their
asset identities/digests remained unchanged. Development CI also passed:
[run 40963734517](https://github.com/gugglegum/arrow_puzzle_game/actions/runs/40963734517).

Ignored evidence and the downloaded common files are under
`out/release-validation-69b1e82`. In the restricted agent environment, local
GitHub CLI verification needed both GH_CONFIG_DIR and XDG_CACHE_HOME redirected
there; the default read-only cache prevented Sigstore verifier initialization.
These are verification-process overrides, not game settings or system changes.

The user subsequently authorized a common versioned draft. The tagged 0.5.0
Release run `40965820527` passed both platforms and created draft `428459388`
with ten Windows/Linux/checksum/metadata/provenance files. Their attachment
digests, five manifest subjects and six signed provenance subjects were
verified; the user's downloaded AppImage also matches exactly. Publication
remains unauthorized and the four existing public releases are unchanged.
See [release history](release-history.md#050-draft-2026-10-30) for the complete
evidence and current draft URL. This supersedes the earlier no-new-tag scope.

## Historical pre-implementation assessment

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

## Current development environment and handoff

Updated 2026-10-30 from the user's follow-up. This supersedes the earlier
recommendation to use WSL2 for regular Linux development.

- The user already actively uses **Linux Mint 22.3 in VMware**. Use that VM as
  the primary environment for native Linux compilation, debugging, and desktop
  smoke checks. Windows remains the Windows development/validation environment.
- VMware availability and performance take priority. WSL2 is not part of this
  workflow. Do not enable Hyper-V/Virtual Machine Platform, install WSL2, or
  change VMware/host virtualization settings as a port prerequisite.
- The user reports that the Windows game runs adequately through Wine inside
  the VM, except for approximately 0.5 seconds of audio delay. This is a user
  observation, not an independently reproduced test suite or native-build
  validation. The tested commit/Wine version and latency cause are unknown.
  Compare the native Linux build in the same VM before attributing the delay to
  the game, Wine, guest audio, or VMware's audio path.
- GitHub Actions should provide build/test checks **before release**: on branch
  pushes, pull requests, and manual runs, with downloadable development
  artifacts. Keep publishing separate and gated by the release procedure.
  The current `release.yml` builds only Windows and triggers on version tags or
  manual dispatch; general development checks are not configured yet.
- Use separate repository clones in Windows and Linux, and transfer source
  changes through GitHub. Before switching, commit/push the completed scoped
  changes; in the destination checkout, inspect status and use
  `git pull --ff-only` on the intended branch when it is safe to do so. Preserve
  dirty files and resolve divergence deliberately. Keep CMake caches, binaries,
  dependencies, and build directories local to each OS.
- Continue practical port work in a chat whose working directory is the Linux
  checkout, so compiler/test output and the actual runtime environment are
  available there. A fresh chat should read `AGENTS.md`, `ai-docs/README.md`,
  this assessment, and relevant developer guides. Git synchronizes the tracked
  files and notes; do not assume it transfers this conversation's transcript.

First Linux-chat task: inspect the current checkout and Mint's compiler/CMake,
display session, and OpenGL capabilities; produce and test a native build using
the existing raylib/core; fix demonstrated portability issues; add repeatable
Linux build instructions/presets and pre-release CI. Retain the agreed X11/
XWayland-first and tar-then-AppImage scope. Use the existing persistence/font/
fullscreen findings below, and preserve Windows behavior. Neither a new release
nor a successful native Linux run has been produced by this handoff. Mint/VMware
testing alone also does not establish bare-metal performance or compatibility
with older distributions; choose a release-library baseline separately.

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
| Existing Windows EXE under Wine | User reports adequate operation in Mint/VMware on 2026-10-30 | Approximately 0.5-second audio delay reported; no comprehensive regression evidence or native Linux validation |

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

The existing Mint/VMware VM is the chosen local Linux build/debug environment.
Linux CI provides repeatable development artifacts and automated checks before
release. VM graphics/audio tests are useful but do not establish bare-metal
latency or performance. Containers can be build environments rather than an
end-user launch requirement. WSL2 is excluded from the current workflow above.

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
