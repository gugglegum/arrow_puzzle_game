# Status, accepted decisions, and open work

Snapshot: 2026-10-29. Code baseline: `8ebae99`. Check `git status`, recent history,
and [CHANGELOG.md](../CHANGELOG.md) before treating this as current state.

## Released versus development

0.3.0 was published from `52182f1` using GitHub Actions; see
[release evidence](release-history.md). It includes the rendering/idle optimizations,
antialiasing, borderless fullscreen, Windows font smoothing, version labels,
guides, audio, HUD buttons, redesigned menus/results, board themes, and six new
screenshots.

Development commit `8ebae99` adds a 1.2-second frozen fatal-collision view before
Game Over. Collision audio is immediate; defeat audio follows the pause. New
launches stop immediately, in-flight interpolation is preserved, and pending or
finished results do not cause continuous GPU work. Timing tests at multiple
frame rates and eight controller scenarios passed, including light/dark, self
and moving collisions, an unrelated flying arrow, victory and idle behavior.
These integration runs were local; do not describe the fix as already in the
published 0.3.0 binary or already tested by a new Actions release run.

The last supplied development EXE was in `build/windows-msvc/arrow_puzzle.exe`.
Its existence/version is not guaranteed on another checkout. Rebuild to deliver
later commits; ordinary source pushes leave release artifacts unchanged.

## Accepted product decisions

| Area | Keep | Rejected approach / reason |
| --- | --- | --- |
| Distribution | Free standalone offline game; no ads, accounts, telemetry | Runtime service or asset dependencies |
| Generator | Dense, varied, constructively solvable tangles; explicit failure | Boring serpentine/S-shaped fallback or relaxed quality just to succeed |
| Performance | Bounded memory, cached geometry, sleeping/polling while idle | Full geometry every camera frame, huge supersampled textures, driver VSync waits |
| Themes | Dark board/HUD default, Light optional; menus/results always dark | Switching all screens to Light was considered but not selected |
| Menu selection | One persistent current item shared by mouse/keys | Restoring a separate keyboard bookmark on mouse exit caused gap flicker |
| Fonts | Embedded Inter with native Windows smoothing throughout UI | Scaled/blurred text and mixed unsmoothed labels |
| Fullscreen | Desktop-resolution borderless window, restored cursor | Exclusive mode caused resolution/aspect problems and brief blackouts |
| HUD/legend | Protected HUD, back arrow at left, board above legend | Gameplay over HUD or reserving another bottom gameplay strip |
| Sounds/guides | Optional, off by default, persisted; HUD shares sound preference | Surprise sound at startup or replay on redraw/unmute |
| Results | Finite effects; preserve the fatal pose before delayed defeat | Immediate Game Over hiding the mistake; endless finished-game work |
| Release trust | Build in Actions, bot-created release, hashes and provenance | Claiming a local EXE is the attested Actions artifact |
| Documentation | Mirrored docs/en and docs/ru with shared screenshots; English-first AI navigation | Diverging technical manuals in ai-docs or duplicated images |

User feedback confirmed that large-board panning, antialiasing, and multi-instance
responsiveness improved. Historical resource-pressure freezes are a reason to
preserve the performance design, not a current benchmark or a diagnosed driver bug.

## Open work and limitations

- No active implementation task is assigned by this roadmap. Work on the live
  user's request; do not silently implement backlog items.
- The Game Over delay is an unreleased change relative to 0.3.0. A future release
  must deliberately choose its version and include the new changelog entry.
- Game.cpp remains large; refactor only in scoped steps that preserve event
  ordering, rendering invalidation, and testability.
- Very Hard compares eight parallel candidates and can have substantial
  generation latency/transient memory use. No new generation-memory reduction
  was established by the rendering work. Do not claim it was fixed.
- Linux/Android are unverified; even dimensions are a generator restriction.
- A local, user-owned, untracked `TODO.txt` mentions softer visual rounding of
  arrowheads/body corners. It is an idea, not an implemented feature or an
  instruction to change geometry now. Preserve the file unless asked otherwise.

## Continuity discipline

Keep behavior and the explanation of why together. Distinguish completed work,
ideas, and historical evidence. After a relevant change, revise this topic or
its technical page rather than appending a raw conversation transcript. Use
English and commit/push completed task changes as AGENTS.md requires. Source
and current user instructions win over a stale snapshot.
