# Working observations for agents

[AGENTS.md](../AGENTS.md) owns repository working rules. The
[project guide](../docs/PROJECT_GUIDE.md) owns build commands, test targets and
the validation matrix; the [runtime guide](../docs/RUNTIME.md) documents screenshot
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
| LNK1104 or access denial on an existing output | Check process paths, locks and permissions; an isolated output or the normal approval mechanism may solve it |
| Local GitHub CLI call has a transient TLS/permission error | Retry/check the actual error before changing credentials or declaring the service unavailable |

Do not infer that the user's game is running from an access-denied message or
kill a process by name. Own QA processes can be tracked by PID; user processes
need the appropriate task authorization. Prefer CMake targets over ad-hoc
manual linking, which has exposed raylib/Windows symbol-order conflicts.

## Agent-memory maintenance

Keep durable observations here, accepted choices/current work in
[status and decisions](status-and-decisions.md), and verified release evidence in
[release history](release-history.md). Put behavior/architecture changes in the
relevant technical guide under docs, then add only their navigation or decision
context here. Keep raw transcripts and temporary artifacts out of this folder.
