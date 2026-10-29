# Changelog

All notable changes to Arrow Puzzle are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Linux AppImage packaging after tar validation, with pinned tooling/runtime,
  project icon, license notices, hashes and FUSE-free startup checks in CI.

- Push/PR/manual development CI for Windows/MSVC and Linux GCC/Clang, with
  downloadable packages, hashes, metadata, Mesa render/text checks and isolated
  archive launch/save verification. Linux tar.gz packaging removes build RPATHs.

- Native Linux x86-64 build presets/helper, XDG settings and save directories,
  and window-manager-controlled X11/XWayland fullscreen. Windows retains portable
  files beside the EXE and its existing borderless fullscreen implementation.

- Touchpad-friendly board controls: left-button dragging, Ctrl + + / - (also
  Ctrl + = and keypad keys) for centred zoom, and Ctrl + 0 to fit the full board.
  Dragging more than 6 screen pixels cancels an arrow launch for the whole
  gesture, even after returning to the starting point. Updated in-game hints
  and both documentation editions.

### Fixed

- Enforce the existing Simple length/bend quality checks and Hard opening-move
  limits during candidate selection, including with libstdc++ random sequences.
  Reject candidates below the quality floor instead of returning a weak board.

### Changed

- The Board theme selector now has a light fill and contrasting dark label and
  chevrons when Light is chosen.
- Arrow loops, Flying arrow collisions, Crosshair guides, and Sounds now default
  to ON when settings.txt or the corresponding key is missing. Saved OFF
  preferences remain unchanged.
- Rounded arrowhead vertices and 90-degree body bends, with matching geometry
  during flight and rewind. Analytic pixel coverage preserves thin strokes;
  merged moving segments and integrated end caps reduce geometry storage.

## [0.4.0] - 2026-10-29

### Added

- A reproducible generator benchmark with separate generation timing, peak
  working set, border metrics, geometry hashes, and solution verification.
- A Very Hard screenshot showing the new generator, plus refreshed menu
  screenshots for 0.4.0.

### Changed

- Reworked Very Hard generation: construct solvable arrows
  from both ends of available maze fragments, with bounded retries and at most
  four workers instead of large interval/count tables. Border arrows now mostly
  continue into the interior, with enforced length diversity, 6..10 opening
  moves, and the same quality limits when arrow loops are disabled. Smaller
  presets retain their existing generator.
- Organized technical documentation into matching English and Russian editions
  under `docs/en` and `docs/ru`, with language switches, shared screenshots, and
  updated links from the main READMEs, agent roadmap, and release workflow.

### Fixed

- Hold the final collision on screen for 1.2 seconds before showing GAME OVER.
  Arrow movement stops at the fatal event, new launches are blocked, and the
  defeat sound follows the pause instead of covering the collision sound.

## [0.3.0] - 2026-10-29

### Added

- Optional **Crosshair guides**: slightly darker exposed grid dots mark the row
  and column nearest the pointer, helping trace an arrow's path on large boards.
- Embedded sounds for arrow launches, collisions, defeat, victory, and fireworks.
  **Sounds** is saved in settings and can also be toggled from the gameplay HUD.
- A back-arrow button in the upper-left corner of the gameplay HUD for returning
  to the menu with the mouse, saving the current game for **Resume game**.
- Large animated victory and **GAME OVER** screens, with colourful fireworks and
  firework reports on victory.
- Persistent **Board theme** selection: Dark by default, with Light available.
  The board and HUD follow the selected theme; menus and result screens retain
  their dark presentation.
- Release and development build identifiers in the lower-right corner of menus.
  An explicit local preview can display the upcoming version before its tag
  exists, for preparing documentation screenshots.
- A Windows x64 release workflow on GitHub Actions, with build metadata,
  SHA-256 checksums, and verifiable build-provenance attestations for the EXE
  and ZIP. It runs only for release tags or an explicit manual request.
- Six updated screenshots covering Simple and Middle gameplay, the main menu,
  difficulty selection, settings, and victory fireworks.
- Developer documentation for the architecture, generator, runtime, and
  release process. Release archives include this documentation and its images
  so README links also work offline.

### Changed

- Redesigned the main menu, difficulty selection, and settings to match the
  result screens, with a dark backdrop, illustrated buttons, and clear toggles.
- Mouse and keyboard now share one menu selection. Hovering selects a row;
  leaving it or crossing a gap keeps it selected. Arrow keys navigate from the
  current row while a stationary pointer no longer overrides the keyboard.
- All interface text uses native Windows font smoothing, including the menus,
  HUD, counters, and controls legend.
- Board strokes, arrowheads, and grid dots use analytic antialiasing for more
  even line weights at small zoom levels.

### Fixed

- Zooming back out after zooming into a Very Hard board can once again fit the
  entire board in the default-sized window.
- Animated arrows are clipped below the HUD and no longer cover the title,
  arrow counter, or lives.
- The board draws over the optional controls legend instead of being obscured
  by it, without reserving an extra strip of gameplay space.
- Fullscreen uses a borderless window at the Windows desktop resolution,
  avoiding aspect-ratio changes and brief blackouts when gameplay resumes.
- The cursor remains visible when repeatedly switching fullscreen modes.

### Performance

- Cached static board geometry makes panning and zooming large boards much
  faster, without allocating a texture covering the whole enlarged board.
- Unchanged menus, boards, and completed result screens skip unnecessary GPU
  redraws; input continues to be polled.
- CPU-based frame pacing avoids driver-side VSync waits and reduces resource
  contention when multiple copies of the game are running.
- Finished games stop advancing gameplay and release arrow rewind histories;
  the finite victory celebration returns to idle after it ends.

### Compatibility

- Existing saved games and settings remain compatible.
- Crosshair guides and Sounds are off by default; both preferences are saved
  beside the executable along with the board theme.

## [0.2.0] - 2026-07-29

### Added

- Start menu with Simple (20 x 20), Middle (40 x 40), Hard (80 x 80), and
  experimental Very Hard (160 x 160) difficulty presets.
- Mouse and keyboard menu navigation.
- Automatic saving and resuming of unfinished games using complete level
  geometry rather than generator seeds.
- Persistent settings for dynamic arrow loops and flying-arrow collision
  penalties.
- Generation failure screen with explicit retry handling.
- Generator regression coverage for 80 x 80 and 160 x 160 boards.

### Changed

- Opening moves are seed-dependent and spatially distributed instead of being
  restricted to one fixed starting arrow.
- Every difficulty now generates eight candidates in parallel and selects the
  highest-scoring tangle.
- Generator scoring gives additional weight to complex dynamic self-passes
  with minimal head-to-tail clearance.
- Level composition uses a broader mixture of short, medium, long, and complex
  arrows on large boards.
- Life hearts use a fuller, rounded balloon-like shape.

### Fixed

- Improved generation reliability, including bounded retries for Hard levels.

### Performance

- Added cached interval and ray facts, exact range-based reachability for large
  boards, and reverse dependency updates in the generator.
- Optimized rendering and collision processing so moving arrows remain smooth
  on Very Hard boards.

## [0.1.0] - 2026-07-28

### Added

- First public Windows x64 release.
- Constructively solvable, completely filled generated levels with mixed arrow
  lengths and complex orthogonal shapes.
- Smooth pixel-level arrow motion with independent acceleration and no maximum
  speed cap.
- Event-driven collision checks that prevent tunnelling when an arrow crosses
  several cells during one rendered frame.
- Dynamic self-collision where the head may enter the cell vacated by its own
  tail on the same simulation tick.
- Simultaneous arrow launches and moving-arrow collision feedback.
- Three lives, collision pauses, animated return after failed launches, and
  persistent red failure tint.
- Grid dots, pointer-centred zoom, middle-button panning, and fullscreen mode.
- Embedded Inter UI font with no external runtime assets.
- Static raylib and MSVC runtime linking for a standalone executable.
- Responsive background generation with an animated progress overlay.
- Deterministic generator seeds and automated generator, motion, and collision
  tests.

### Changed

- Failed generation reports an error instead of substituting an uninteresting
  serpentine fallback board.

### Fixed

- Prevented repeated life loss while one moving arrow follows another in the
  same rear-end collision episode.
- Made newly launched arrows start with their own initial speed instead of
  inheriting simulation time from an older moving arrow.
- Allowed an armed mouse click to be cancelled by releasing away from the
  originally pressed arrow.

[Unreleased]: https://github.com/gugglegum/arrow_puzzle_game/compare/v0.4.0...HEAD
[0.4.0]: https://github.com/gugglegum/arrow_puzzle_game/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/gugglegum/arrow_puzzle_game/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/gugglegum/arrow_puzzle_game/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/gugglegum/arrow_puzzle_game/releases/tag/v0.1.0
