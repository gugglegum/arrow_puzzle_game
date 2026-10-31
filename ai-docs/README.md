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
   to AGENTS.md. Branch pushes run development build/test CI; release preparation
   uses the separate tag/manual workflow, and publication is a separate action.

## Find the right entry point

| Task | Read | Main implementation |
| --- | --- | --- |
| Overall structure, models, build and tests | [Project guide](../docs/en/PROJECT_GUIDE.md) | `Game.cpp`, `include/arrow/`, CMake and `tests/` |
| Generation, solvability, arrow composition | [Generator guide](../docs/en/GENERATOR.md) | `Generator.cpp`, `Board.cpp` |
| Very Hard 0.4.0 work, benchmark evidence | [Generator work](generator-0.4.md) | `FrontierGenerator.cpp`, `GeneratorBenchmark.cpp` |
| Flight, collisions, rollback, saves | [Runtime guide](../docs/en/RUNTIME.md) | `Game.cpp`, `MotionResolver.cpp`, `Persistence.cpp` |
| Manual named snapshots, catalog, load/rename/delete | [Runtime guide, manual snapshots](../docs/en/RUNTIME.md#manual-snapshots) | `Persistence.cpp`, `Game.cpp`, `MenuScreen.cpp`, persistence tests |
| Touchpad, click/drag safety, keyboard zoom/fit | [Runtime guide, section 8](../docs/en/RUNTIME.md) | `BoardPointerGesture.hpp`, `BoardPointerGestureTests.cpp`, `Game.cpp` |
| Panning, antialiasing, idle load, fonts | [Runtime guide, section 9](../docs/en/RUNTIME.md) | `StaticBoardGeometry.cpp`, `UiText.cpp` |
| Debug-only screenshot hooks; no capture hotkeys | [Project guide, section 7](../docs/en/PROJECT_GUIDE.md), [capture policy](portable-distribution.md#accepted-screenshot-policy) | `Game.cpp`, CMake, `smoke-linux.py` |
| Accepted rounded arrows, resource comparisons | [Rounding evaluation](rounded-arrows.md) | `ArrowStroke.hpp`, `BoardCoverageShader.hpp`, `RenderingTests.cpp` |
| Linux build, Mint/VMware verification, packaging | [Linux progress](linux-portability.md), [developer guide](../docs/en/LINUX.md) | Linux presets/helpers, `Persistence.cpp`, `LinuxWindow.cpp`, workflows |
| Ordinary/portable builds and AppImage data paths | [Accepted storage contract](portable-distribution.md) | `Persistence.cpp`, `Game.cpp`, packaging/CI |
| Menus, themes, sound, end screens | [Runtime guide](../docs/en/RUNTIME.md) | `MenuSelection.hpp`, `EndScreen.cpp`, `GameAudio.cpp` |
| Timed hints, safe random choices, yellow highlight/chime | [Runtime guide, section 8](../docs/en/RUNTIME.md) | `Hints.cpp`, `HintState`, `Game.cpp`, `HintTests.cpp` |
| Interface languages, Auto locale, embedded CJK font | [Localization notes](localization.md), [runtime guide](../docs/en/RUNTIME.md#interface-localization) | `Localization.cpp`, `UiText.cpp`, `MenuScreen.cpp`, `Persistence.cpp` |
| Release procedure and verification | [Release guide](../docs/en/RELEASING.md) | `cmake/`, `tools/`, `.github/workflows/` |
| Previously verified release evidence | [Release history](release-history.md) | Tagged source and Actions run |

Technical facts and procedures live in [English docs](../docs/en/README.md), with
a matching [Russian edition](../docs/ru/README.md), and serve both developers and
agents. Keep both editions aligned. This folder adds navigation, decisions,
status and working observations; do not maintain a second architecture manual here.
User-facing descriptions are [README.md](../README.md) and
[README_RU.md](../README_RU.md). Avoid copying whole guides into these notes.

## Maintaining this folder

- Keep accepted decisions, constraints, source entry points, useful commands,
  and reproducible findings. Update the topic affected by a completed change.
- Shared project notes and AGENTS.md are tracked in Git. Put personal or
  temporary notes in [local/](local/); only its .gitignore is tracked.
- Mark proposals, historical observations, and verification limits explicitly.
  Never turn an idea or an old benchmark into a claim about current behavior.
- Keep transient logs, binaries, sample saves, and screenshots under ignored
  `build/` or `out/`; do not make this roadmap depend on those files existing.
- Prefer stable file/function names over source line numbers or machine paths.
  Record machine-specific workarounds only in the workflow notes.
- Keep this index short. New topic pages should be reachable from here.

Documentation reviewed for the complete 0.5.0 release on 2026-10-30, including
ordinary/portable packages, localization, optional left dragging and caption
alignment. See release history for verified tagged-source identities; inspect
Git rather than assuming a recorded source is still HEAD. This folder is
repository-local project memory, not runtime data.
