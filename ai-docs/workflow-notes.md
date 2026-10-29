# Working observations for agents

[AGENTS.md](../AGENTS.md) owns repository working rules. The
[project guide](../docs/en/PROJECT_GUIDE.md) owns build commands, test targets and
the validation matrix; the [runtime guide](../docs/en/RUNTIME.md) documents screenshot
hooks. Use those references instead of maintaining duplicate procedures here.

## Useful habits from previous work

- Read the relevant code before trusting a snapshot. Search by module/function
  names; Game.cpp is the main controller and remains a large file.
- Identify the actual running EXE and adjacent settings/save. Multiple copies
  exist under build/out and do not share persistence automatically.
- A commit does not rebuild an EXE. Documentation commits also increase the
  development version's commit count when the next build regenerates its label.
- For focused builds, `--parallel 2` has kept local resource use manageable.
  Do not run the full generator stress suite for a documentation-only change.
- Keep UI fixtures in a fresh ignored directory with its own EXE and working
  directory. Never replace the user's normal settings or saved game for QA.
- Start screenshot smoke checks with MENU=1 and FRAME=4 to avoid unintended
  board generation. Captures must be viewed. Clear environment hooks afterward
  when reusing a shell; SCREENSHOT intentionally forces redraws, so leave it
  empty when measuring idle behavior.
- Deterministic controller harnesses used real rendering and substituted input
  queries around production Game.cpp. Existing harnesses/logs under `build/`
  are disposable, may be stale, and are not maintained CMake tests.
- Check actual output and state: a command returning or a screenshot file
  existing does not establish correct pixels, sound playback, or public access.
- In a release, tag/publish according to the user's requested scope. A request
  to build may end with the workflow's draft; public publication is a separate
  action. Continue already authorized steps without asking for them again.

## Environment-specific findings

These were observed in the maintainer's Windows agent environment. They are
diagnostic leads, not mandatory project configuration; rediscover paths on a
different machine.

| Observation | Useful response |
| --- | --- |
| CMake/Ninja absent from PATH | The build helper also checks the standard CLion installation |
| Fresh compiler detection fails in Debug with a PDB-service error | `-DCMAKE_TRY_COMPILE_CONFIGURATION=Release` worked for an isolated Release configure |
| Persistence tests cannot atomically replace files in default TEMP | Use a fresh workspace `build/` directory for child-process TEMP/TMP |
| LNK1104 or access denial on an existing output | Match the running EXE to the output path; AGENTS.md authorizes stopping that process and rebuilding in place. Check permissions if no matching process exists |
| Local GitHub CLI call has a transient TLS/permission error | Retry/check the actual error before changing credentials or declaring the service unavailable |

Do not infer that the user's game is running from an access-denied message or
stop unrelated copies by process name. Identify the exact EXE/PID first and use
the standing permission in AGENTS.md when it blocks that build output. Prefer
CMake targets over ad-hoc manual linking, which has exposed raylib/Windows
symbol-order conflicts.

## Local build artifacts

- Reuse `build/windows-msvc` for normal Release builds. The Debug preset uses
  `build/windows-msvc-debug`; CLion may instead use `cmake-build-debug`. Inspect
  the active IDE profile before keeping or deleting duplicate Debug trees.
- Use a separate compiler tree only for an actual configuration or instrumented
  source difference. A settings/screenshot fixture usually needs just a copy of
  the primary EXE and its own working directory, not another full build.
- An ignored folder is not necessarily disposable. Inspect settings, saved games
  and user-named backups; local QA harness sources/scripts and audio experiments
  are not all tracked. Preserve useful data before an authorized cleanup.
- `out/audio-previews` contains the original procedural generators and rejected
  sound variants. Selected sounds are tracked under `assets/sounds`, but the
  remaining experiments need their own archive if they are to be retained.
- Several local build caches refer to `build/windows-msvc/_deps/raylib-src`.
  Keep that shared source cache while those trees are used. Separate Debug
  trees can contain their own large, otherwise reproducible dependency clones;
  recreating deleted clones requires downloading the pinned revision again.
- A recently delivered QA EXE can be newer than the primary EXE. Rebuild and
  verify the primary before removing the only checked copy of a recent change.
- Keep machine-specific inventories and proposed cleanup lists in ignored
  `ai-docs/local`. A request for an inventory does not authorize deletion.

## Agent-memory maintenance

Keep durable observations here, accepted choices/current work in
[status and decisions](status-and-decisions.md), and verified release evidence in
[release history](release-history.md). Put behavior/architecture changes in the
relevant technical guide under docs, then add only their navigation or decision
context here. Keep matching docs/en and docs/ru pages aligned when facts change.
Personal scratch notes belong in [local/](local/), whose contents
are ignored except for its .gitignore. Keep generated artifacts in build/out.
