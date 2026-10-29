# Arrow Puzzle project guide

**English** | [Русский](../ru/PROJECT_GUIDE.md)

This guide was reviewed for version 0.4.0 on 2026-10-29. It introduces the
implementation and development workflow. User-facing information is in the
root `README.md` and `README_RU.md`.

In a source checkout, agent instructions live in `AGENTS.md`; the short English
roadmap and repository-local project memory start at `ai-docs/README.md`.

## 1. Project overview

Arrow Puzzle is a minimalist puzzle whose board is completely filled with
polyline arrows. Each arrow moves forward like a snake: the head enters the
next cell while the tail vacates a cell. Clear the board without losing all
three lives.

The current stack is:

- C++20;
- raylib 5.5;
- CMake and Ninja;
- MSVC and its static runtime on Windows;
- statically linked raylib;
- only the C++ standard library outside raylib;
- an Inter font embedded in the EXE.

Windows x64 is the only verified platform. Linux and Android are future targets,
so isolate platform-specific code and avoid unnecessary Windows-only dependencies.

The current Windows renderer requires an OpenGL 3.3 Core context and GLSL 330,
with a graphics driver that exposes them. A discrete GPU is not required;
integrated graphics meeting these requirements can run the same renderer.
Actual integrated-GPU frame rates have not been measured. Large-board
generation runs on the CPU, independently of the graphics API.

## 2. Product principles and required behavior

Preserve these user decisions when making changes:

1. The game is free, with no ads, accounts, telemetry, or runtime network access.
2. Keep a small standalone EXE, fast startup, few dependencies, and negligible
   resource use while idle.
3. The board defaults to Dark with a persistent Light option. Flight is blue,
   failure is red, and menus and result screens remain dark.
4. Levels should look like dense tangles of short, medium, and long arrows,
   rather than collections of simple S-shaped territories.
5. The generator must not replace a failed search with a boring row-by-row
   snake. A longer wait or explicit failure is preferable.
6. Generation must be constructively solvable, and both correctness and quality
   must remain covered by tests.
7. A dynamic self-pass is legal only when the head enters its own cell on the
   exact tick when the tail vacates it. Genuine self-collision is forbidden.
8. Multiple arrows can fly simultaneously. A moving-arrow contact episode costs
   one life, colors both arrows red, pauses forward flight for 0.5 seconds, and
   lets them continue without rewinding.
9. A static obstacle or genuine self-hit rewinds the launched arrow through its
   accepted history; it stays red afterward.
10. A press only arms an arrow. Release over the same arrow launches it if the
    pointer has not moved more than 6 screen pixels. Crossing that threshold
    pans the board and cancels the launch for the rest of the gesture, even
    when returning to the press position. Keep keyboard zoom and fit available.
11. Saves contain complete geometry and game state, not a seed. This protects
    them from generator changes and standard-library RNG differences.

## 3. Structure and dependencies

```text
include/arrow/             Public core interfaces
  Types.hpp                Cell coordinates and basic operations
  Board.hpp                Immutable level geometry and exit checks
  Generator.hpp            GeneratorOptions and generateBoard()
  MotionKinematics.hpp     Constant-acceleration formulas
  MotionResolver.hpp       Discrete cell-boundary collision resolution
  Persistence.hpp          Settings and saved-game snapshots
  Game.hpp                 Application entry point

src/
  Board.cpp                Geometry invariants, canExit(), solveGreedily()
  Generator.cpp            Constructive generation and candidate scoring
  FrontierGenerator.cpp    Bounded large-board construction in both directions
  MotionKinematics.cpp     Pure numerical motion functions
  MotionResolver.cpp       Pure collision classification
  Persistence.cpp          Versioned text formats and atomic replacement
  Game.cpp                 raylib, UI, input, camera, animation, main loop
  BoardPointerGesture.hpp  Click/drag threshold and launch cancellation
  StaticBoardGeometry.*    Geometry caching and analytic antialiasing
  ArrowStroke.hpp          Shared rounded strokes for idle/flight/rewind
  BoardPalette.hpp         Board and HUD palettes
  MenuScreen.*             Menu presentation
  MenuSelection.hpp        Shared mouse/keyboard selection
  EndScreen.*              Result screens and fireworks
  GameAudio.*              Optional embedded sounds
  AppIcon.*                Embedded window icons, decoded only at startup
  UiText.*                 Windows GDI smoothing and text-mask cache
  main.cpp                 Calls Game::run()
  UiFontData.hpp           Embedded font data interface

tests/
  GeneratorTests.cpp       Geometry, quality, motion and collision regressions
  PersistenceTests.cpp     Formats, malformed input and atomic saving
  MenuSelectionTests.cpp   Shared selection, gaps and stationary pointer
  BoardPointerGestureTests.cpp  Clicks, touchpad jitter, drags and cancellation
  EndScreenTests.cpp       Result states and finite animation timing

assets/fonts/              Inter Medium and OFL license
assets/sounds/             Embedded game and firework sounds
assets/icons/              SVG master, embedded PNG sizes, Windows ICO
cmake/EmbedUiFont.cmake    TTF-to-C++ generation
tools/build-windows.ps1    MSVC setup, build and tests
tools/package-windows.ps1  ZIP, documentation, licenses and SHA-256
.github/workflows/         Release build and provenance
docs/                      Technical documentation and shared screenshots
```

CMake targets:

- `arrow_core`: Board, generation, kinematics, collisions, and persistence,
  independent of raylib;
- `arrow_puzzle`: GUI application linking the core and raylib;
- `arrow_tests`: generator, level quality, kinematics, and collisions;
- `arrow_persistence_tests`: settings and saved games;
- `arrow_menu_selection_tests`: window-free menu-selection logic;
- `arrow_board_pointer_tests`: window-free click/drag and launch-cancellation checks;
- `arrow_end_screen_tests`: window-free result-state/timing checks.

Build `arrow_render_tests`, `arrow_ui_text_tests`, and `arrow_audio_tests`
explicitly. They are not part of default CTest because they need a GPU context
or a playback device.

The intended dependency direction is:

```text
Types
  ├─ Board ─ Generator
  ├─ MotionKinematics
  ├─ MotionResolver
  └─ Persistence
          \       /
             Game + raylib
```

Keep the core testable without a window or GPU.

## 4. Core data models

### Cell and Arrow

`Cell` is an integer grid coordinate. `Arrow::cells` is always head-first;
`Arrow::direction` is a cardinal vector. For an arrow longer than one cell:

```text
cells[0] - cells[1] == direction
```

Adjacent body cells differ by exactly one horizontal or vertical step. An
arrow's body cannot contain a cell twice.

### Board

`Board` stores the original immutable geometry. Live game state is not written
back into it; that state belongs to `RuntimeArrow` in `Game.cpp`.

Important checks:

- `hasValidGeometry()`: direction, continuity, and unique body cells;
- `isCompletelyFilled()`: every board cell belongs to exactly one arrow;
- `canExit()`: dynamic exit simulation accounting for the vacating tail;
- `solveGreedily()`: a general check that all arrows can be removed. The
  generator also retains a constructive order implicitly.

See [GENERATOR.md](GENERATOR.md) for generation and [RUNTIME.md](RUNTIME.md)
for live game state.

## 5. Building on Windows

Requirements: Visual Studio 2022 with MSVC x64 and Windows SDK, CMake 3.24+,
Ninja, and Git. FetchContent downloads raylib during the first configuration.

Application icons are embedded in the EXE, including the Windows resource
used by Explorer and shortcuts. No PNG/ICO files need to accompany the game.
Normal builds use committed icon assets; optional artwork regeneration is
documented in [assets/icons/README.md](../../assets/icons/README.md).

From a regular PowerShell in the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
    -File .\tools\build-windows.ps1 `
    -Configuration Release
```

The script configures, builds, and runs the full test suite. Output:

```text
build\windows-msvc\arrow_puzzle.exe
```

Debug build:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
    -File .\tools\build-windows.ps1 `
    -Configuration Debug
```

From a Developer PowerShell, use presets directly:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

If a running game locks the output EXE, use another build directory. Stopping a
user's instance requires authorization; own test processes can be stopped by
their recorded PID.

## 6. Testing

Full CTest runs five test suites:

```powershell
ctest --test-dir build\windows-msvc --output-on-failure
```

The default generator test checks 1,333 boards and special motion regressions.
It is deliberately substantial and may take tens of seconds.

For a quick reproducible range:

```powershell
.\build\windows-msvc\arrow_tests.exe 25 700
```

Arguments are the ordinary seed count and first seed. Special regressions still
run. Persistence tests can be run separately:

```powershell
.\build\windows-msvc\arrow_persistence_tests.exe
```

Minimum validation by change type:

| Change | Required check |
| --- | --- |
| Generator, metrics, or lengths | Full `arrow_tests`; visual checks of fixed seeds |
| Kinematics or collisions | Motion regressions in `arrow_tests` |
| Settings/save format | `arrow_persistence_tests`; format version |
| UI, input, or camera | Build, automated screenshot, and manual launch |
| CMake, font embedding, or release | Clean Release build and startup outside the build tree |

## 7. Automated visual checks

Game.cpp supports environment variables for testing UI without manual clicks:

```powershell
$env:ARROWPUZZLE_SEED = "747"
$env:ARROWPUZZLE_SCREENSHOT = "$PWD\preview.png"
$env:ARROWPUZZLE_DIFFICULTY = "2"
& .\build\windows-msvc\arrow_puzzle.exe
```

Available hooks:

- `ARROWPUZZLE_SEED`: reproducible seed;
- `ARROWPUZZLE_DIFFICULTY`: difficulty 1..4;
- `ARROWPUZZLE_SCREENSHOT`: output PNG and automatic exit;
- `ARROWPUZZLE_SCREENSHOT_FRAME`: frame number for capture;
- `ARROWPUZZLE_SCREENSHOT_MENU`: Main/New game/Settings as 1..3;
- `ARROWPUZZLE_SCREENSHOT_REMOVE`: hide the first N arrows;
- `ARROWPUZZLE_SCREENSHOT_LAUNCH`: launch an arrow by index;
- `ARROWPUZZLE_SCREENSHOT_LAUNCH_2`: launch a second arrow;
- `ARROWPUZZLE_SCREENSHOT_LAUNCH_2_FRAME`: its launch frame;
- `ARROWPUZZLE_RESUME`: open a saved game in an automated scenario;
- `ARROWPUZZLE_SAVE_ON_EXIT`: check saving on exit.

Create the image and inspect its pixels visually; file existence is not enough.

## 8. Development workflow

Before editing:

1. Read this guide and the relevant topic guide.
2. Run `git status --short`; preserve unrelated user changes.
3. Locate implementations/tests with `rg`, not just the README.
4. For generator work, state the invariants and metrics to preserve first.
5. For motion, distinguish continuous rendering from discrete cell simulation.

After editing:

1. Build Release or Debug as appropriate.
2. Run checks proportional to the change.
3. Create and view a screenshot for visual changes.
4. Check `git diff --check` and `git status --short`.
5. Agent commit/push rules are defined in `AGENTS.md`.
   The release process is in [RELEASING.md](RELEASING.md).

## 9. Limitations and technical debt

- Game.cpp is a large controller. Extract UI, rendering, or simulation modules
  in small testable steps rather than an unrelated rewrite.
- Very Hard uses bounded frontier construction with up to four workers since
  0.4.0. The 0.3.0 generator had substantial time/memory
  spikes. See the generator guide for current limits and the benchmark tool.
- The generator needs even board dimensions.
- Linux and Android are not built or verified in CI.
- The root may contain the user's TODO.txt. Do not delete or rewrite this
  user-owned file without a request.

## 10. Licenses and releases

- Project code: MIT, `LICENSE`.
- raylib 5.5: zlib/libpng, `THIRD_PARTY_NOTICES.md`.
- Inter 4.1: SIL OFL 1.1, `assets/fonts/Inter-LICENSE.txt`.

The CMake project version is 0.4.0; check GitHub Releases for published versions.
A release ZIP contains standalone ArrowPuzzle.exe, both READMEs, changelog,
documentation/screenshots, and licenses. Publication requires a clean Release
build, full CTest, successful unpacked startup, and SHA-256 checks.
A `vX.Y.Z` tag starts Actions, which creates verifiable attestations and a draft
as `github-actions[bot]`. A normal branch push does not trigger a build.

The displayed version comes from Git tags; intermediate builds add the commit
count and short hash. An isolated local build with
`ARROWPUZZLE_RELEASE_PREVIEW=ON` displays the future CMake version before a tag
exists. Official builds leave it OFF. See [RELEASING.md](RELEASING.md) for the
release and verification process.
