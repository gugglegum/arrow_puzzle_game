# Agent roadmap

Start here after reading the user's current request and [AGENTS.md](../AGENTS.md).
These notes explain where to look and which decisions to preserve; source code,
tests, Git history, and the current instructions settle discrepancies.

## First five minutes

1. Run `git status --short` and inspect the latest commits. Preserve unrelated
   edits, untracked user files, settings, and saved games.
2. Read [status and decisions](status-and-decisions.md). It distinguishes
   published behavior from changes already on the development branch.
3. Use the table below to open only the relevant technical guide and implementation.
4. Read [workflow notes](workflow-notes.md) and choose checks from the developer
   guide before changing behavior.
5. Complete the change, maintain relevant notes, then commit and push according
   to AGENTS.md. A routine push does not build or publish a release.

## Find the right entry point

| Task | Read | Main implementation |
| --- | --- | --- |
| Overall structure, models, build and tests | [Project guide](../docs/PROJECT_GUIDE.md) | `Game.cpp`, `include/arrow/`, CMake and `tests/` |
| Generation, solvability, arrow composition | [Generator guide](../docs/GENERATOR.md) | `Generator.cpp`, `Board.cpp` |
| Flight, collisions, rollback, saves | [Runtime guide](../docs/RUNTIME.md) | `Game.cpp`, `MotionResolver.cpp`, `Persistence.cpp` |
| Panning, antialiasing, idle load, fonts | [Runtime guide, section 9](../docs/RUNTIME.md) | `StaticBoardGeometry.cpp`, `UiText.cpp` |
| Menus, themes, sound, end screens | [Runtime guide](../docs/RUNTIME.md) | `MenuSelection.hpp`, `EndScreen.cpp`, `GameAudio.cpp` |
| Release procedure and verification | [Release guide](../docs/RELEASING.md) | `cmake/`, `tools/`, `.github/workflows/` |
| Previously verified release evidence | [Release history](release-history.md) | Tagged source and Actions run |

Technical facts and procedures live in [docs](../docs/README.md), mostly in Russian,
and serve both human developers and agents. This folder adds navigation, decisions,
status and working observations; do not maintain a second architecture manual here.
User-facing descriptions are [README.md](../README.md) and
[README_RU.md](../README_RU.md). Avoid copying whole guides into these notes.

## Maintaining this folder

- Keep accepted decisions, constraints, source entry points, useful commands,
  and reproducible findings. Update the topic affected by a completed change.
- Mark proposals, historical observations, and verification limits explicitly.
  Never turn an idea or an old benchmark into a claim about current behavior.
- Keep transient logs, binaries, sample saves, and screenshots under ignored
  `build/` or `out/`; do not make this roadmap depend on those files existing.
- Prefer stable file/function names over source line numbers or machine paths.
  Record machine-specific workarounds only in the workflow notes.
- Keep this index short. New topic pages should be reachable from here.

Code reviewed for this initial roadmap: `8ebae99`, 2026-10-29. Documentation
commits may follow that code baseline; inspect Git rather than assuming it is
still HEAD. This folder is repository-local project memory, not runtime data.
