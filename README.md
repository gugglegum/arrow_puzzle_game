# Arrow Puzzle

[Русская версия](README_RU.md)

Arrow Puzzle is a minimalist desktop puzzle game about untangling a board of
arrows. It is written in C++20 with raylib and currently targets Windows.

The project is an early but fully playable prototype. It starts directly on a
generated 40 x 40 board; menus, difficulty selection, saving, and loading are
planned but not implemented yet.

## Gameplay

Every arrow is a polyline made of grid cells. Its head defines the direction
in which the whole body moves, one cell at a time, like the snake in the
classic Snake game.

Click an arrow to launch it. A launched arrow is highlighted in blue, starts
slowly, and accelerates smoothly like a small rocket without a maximum-speed
cap. To remove the arrow, it must leave the board completely. Hitting a
stationary arrow or its own body costs one of the three lives: the failed
arrow turns red, rewinds to its original position, and remains red while idle.
The goal is to clear the board.

Self-collision is evaluated dynamically rather than from a static snapshot.
The head may enter a cell vacated by its own tail on the same movement tick,
but it may not pass through any other part of its body.

Several arrows can move simultaneously. If two moving arrows touch, the batch
costs one life, but both arrows pass through each other and continue forward
instead of rewinding. Both turn red before a half-second hit-stop and remain
red for the rest of their flight, making the contact and lost life clearly
visible before and after movement resumes. Timing is therefore part of the
puzzle.

Every arrow has an independent speed and movement clock. A frame is resolved
chronologically at every cell boundary crossed by any arrow, so even an arrow
that covers several cells in one rendered frame cannot tunnel through an
obstacle. Rendering remains continuous at pixel resolution. Corners stay
orthogonal: the line grows at the head and retracts at the tail without
diagonal warping. A per-frame event budget keeps very large or unusually fast
boards responsive; unfinished simulation time is carried forward, never
replaced by skipped collision checks. A newly clicked arrow is timestamped
after any older queued simulation time, so backlog can never make it inherit
another arrow's speed.

## Current features

- Completely filled 40 x 40 generated boards
- Usually about 180 arrows on the default board
- A deliberate mix of short, medium, and long arrows from 2 to 60 cells
- Constructive generation with a stored valid solution order
- Dynamic self-collision and simultaneous moving-arrow collisions
- Three lives with animated collision recovery
- Persistent light-grey grid dots revealed as arrows leave
- Independent, continuously accelerating motion without a speed cap
- Event-driven cell collision checks that prevent high-speed tunnelling
- Windowed and fullscreen modes
- Pointer-centred zoom and middle-button panning
- Deterministic seeds and extensive generator tests
- No ads, telemetry, accounts, or runtime network access

## Controls

| Input | Action |
| --- | --- |
| Left mouse button | Launch an idle arrow |
| Mouse wheel | Zoom at the pointer |
| Middle mouse button + drag | Pan the board |
| F11 or Left Alt+Enter | Toggle fullscreen |
| Escape | Leave fullscreen, or quit in windowed mode |
| R | Generate a new board when no arrow is moving |
| F12 | Save `arrow-puzzle-screenshot.png` in the working directory |

New arrows may be launched while other arrows are moving forward. Input is
temporarily locked while collided arrows are paused or rewinding.

## Building on Windows

### Requirements

- A Visual Studio installation with the MSVC x64 C++ tools and Windows SDK
  (tested with Visual Studio 2022)
- CMake 3.24 or newer
- Ninja
- Git and an internet connection for the first configuration

The build helper looks for CMake and Ninja in `PATH`. If they are not found,
it tries the standard bundled-tool locations used by CLion. raylib does not
need to be installed manually: CMake downloads the pinned raylib 5.5 source
during the first configuration and builds it statically.

### One-command Release build

Run this from the repository root in a regular PowerShell window:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
    -File .\tools\build-windows.ps1 `
    -Configuration Release
```

The helper configures the project, builds it, and runs the complete test suite.
The generator stress test creates 1,300 boards and can take several minutes.

The resulting executable is:

```text
build\windows-msvc\arrow_puzzle.exe
```

Launch it with:

```powershell
& .\build\windows-msvc\arrow_puzzle.exe
```

For a Debug build:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
    -File .\tools\build-windows.ps1 `
    -Configuration Debug
```

### CMake presets

From a Developer PowerShell with CMake and Ninja in `PATH`:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

### Focused generator test

The helper can reproduce a smaller deterministic seed range:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
    -File .\tools\build-windows.ps1 `
    -Configuration Release `
    -SeedCount 25 `
    -FirstSeed 700
```

This example checks standard seeds 700 through 724, plus a smaller set of
alternate board configurations.

## Development hooks

The initial board can be reproduced without changing the source:

```powershell
$env:ARROWPUZZLE_SEED = "747"
& .\build\windows-msvc\arrow_puzzle.exe
```

For automated visual checks, set `ARROWPUZZLE_SCREENSHOT` to an output path.
The game captures a frame and exits automatically:

```powershell
$env:ARROWPUZZLE_SEED = "747"
$env:ARROWPUZZLE_SCREENSHOT = "$PWD\preview.png"
& .\build\windows-msvc\arrow_puzzle.exe
```

Additional screenshot-only hooks are available:

- `ARROWPUZZLE_SCREENSHOT_FRAME` selects the captured frame (default: `30`)
- `ARROWPUZZLE_SCREENSHOT_REMOVE` hides the first N arrows before capture
- `ARROWPUZZLE_SCREENSHOT_LAUNCH` launches one arrow by zero-based index
- `ARROWPUZZLE_SCREENSHOT_LAUNCH_2` launches a second arrow later
- `ARROWPUZZLE_SCREENSHOT_LAUNCH_2_FRAME` selects that launch frame

## Project structure

```text
include/arrow/       Public C++ headers
src/Board.cpp        Board state, movement rules, and solution checks
src/Generator.cpp    Constructive level generator and candidate scoring
src/MotionKinematics.cpp  Independent accelerated movement calculations
src/MotionResolver.cpp  Moving/static collision classification
src/Game.cpp         raylib rendering, input, and animation
tests/               Board and generator tests
tools/               Windows build helper
```

`arrow_core` contains the board and generator without a raylib dependency.
`arrow_puzzle` links that core with statically built raylib. `arrow_tests`
links only `arrow_core`.

## How generation works

The generator builds a maze-like Hamiltonian cycle that visits every board
cell, then uses dynamic programming to cut it into arrows with a broad length
distribution. A cut is accepted only when its exit dependencies preserve a
constructive solution order.

Multiple candidates are scored for inter-arrow contact, spatial mixing,
direction diversity, turns, dependency structure, and length distribution.
Large rectangular S-shaped regions are penalized so the result resembles a
tangle of separate threads rather than a few territorial snakes.

The automated suite verifies complete coverage, geometry, requested length
limits, constructive solvability, dynamic self-collision, and statistical
quality across many deterministic seeds.

## Portability

Windows is the only tested target at the moment. The core is standard C++20
and raylib is cross-platform, so Linux and Android are intended future targets,
but they are not yet supported or covered by the current build scripts.

## License

Arrow Puzzle is released under the [MIT License](LICENSE), copyright
2026 Paul Melekhov.

raylib 5.5 is distributed under the unmodified zlib/libpng license and is
compatible with static linking in this MIT-licensed project. See
[Third-Party Notices](THIRD_PARTY_NOTICES.md) for its copyright and license
text.
