# Changelog

All notable changes to Arrow Puzzle are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Gameplay, main-menu, and difficulty-selection screenshots to the project
  documentation.

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

[Unreleased]: https://github.com/gugglegum/arrow_puzzle_game/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/gugglegum/arrow_puzzle_game/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/gugglegum/arrow_puzzle_game/releases/tag/v0.1.0
