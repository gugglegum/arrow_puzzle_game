# Status, accepted decisions, and open work

Snapshot: 2026-10-29, after publication and verification of 0.4.0. Check
`git status`, recent history, and [CHANGELOG.md](../CHANGELOG.md) before treating
this as current state.

## Released versus development

0.4.0 was published from `b785f23` using GitHub Actions and marked Latest; see
[release evidence](release-history.md) for the run, hashes, and verification.
It includes the accepted Very Hard frontier generator, delayed Game Over,
bilingual documentation, and updated screenshots. All four default headless
test suites passed locally and in Actions. The downloaded release EXE and ZIP
passed checksum/provenance verification and local startup/rendering checks.

The 0.3.0 release from `52182f1` introduced the rendering/idle optimizations,
antialiasing, borderless fullscreen, Windows font smoothing, version labels,
audio, HUD buttons, redesigned menus/results, board themes, and screenshots.

The user accepted rounded arrowheads and curved 90-degree bends after reviewing
their appearance and local performance/memory measurements, then authorized
commit/push. This is post-0.4.0 development; see
[rounding evidence](rounded-arrows.md). No new release was requested.

Touchpad controls are accepted post-0.4.0 work: LMB click/drag separation,
centred Ctrl +/- zoom (also Ctrl+=, keypad and key repeat), and Ctrl+0 fit.
`BoardPointerGesture` latches launch cancellation beyond 6 screen pixels.
Use raylib's gesture drag vector to recover the real press origin when press
and movement share a native input poll; sampling only the frame's pointer
position allowed a fast drag to launch its endpoint arrow. Preserve queued
key-down events as well, so short Ctrl chords survive release between frames.
See [runtime input](../docs/en/RUNTIME.md#8-input) for the maintained contract.

Validation on Windows: a clean Release build and all five default headless
suites passed. A local controller harness around production Game.cpp passed
36 scenarios: click/jitter, drag from arrows/empty space, return to origin,
release-frame movement, focus/HUD handling, simultaneous middle/left buttons,
keyboard/keypad/repeat/queued chords, zoom bounds/anchors and resized-window
fit. These controller checks use real rendering with scripted input queries
and are local QA, not default CTest. Native-input checks of a separate
standalone production EXE also confirmed fast LMB panning without changing
arrow/life counts, centred Ctrl+= / Ctrl- and Ctrl+0 after panning. Dark/Light
720x600 and normal 1100x900 hints were inspected. No release was requested.

Commit `8ebae99`, included in 0.4.0, adds a 1.2-second frozen fatal-collision view before
Game Over. Collision audio is immediate; defeat audio follows the pause. New
launches stop immediately, in-flight interpolation is preserved, and pending or
finished results do not cause continuous GPU work. Timing tests at multiple
frame rates and eight controller scenarios passed, including light/dark, self
and moving collisions, an unrelated flying arrow, victory and idle behavior.
These controller integration runs were local; Actions runs the headless timing
tests. The fix is in the published 0.4.0 binary, not 0.3.0.

The released Very Hard generator constructs arrows from multiple
fronts in both directions, with bounded search and explicit perimeter-quality
limits. Local Release tests and benchmarks passed; see
[generator evidence](generator-0.4.md). The user accepted the resulting gameplay
and requested the completed 0.4.0 release. Benchmark figures are local samples,
not timing guarantees for every seed or machine.

Use the published ZIP for the attested release binary. Local build directories
are not guaranteed on another checkout. Rebuild to deliver later commits;
ordinary source pushes leave release artifacts unchanged.

## Accepted product decisions

| Area | Keep | Rejected approach / reason |
| --- | --- | --- |
| Distribution | Free standalone offline game; no ads, accounts, telemetry | Runtime service or asset dependencies |
| Generator | Dense, varied, constructively solvable tangles; explicit failure | Boring serpentine/S-shaped fallback or relaxed quality just to succeed |
| Performance | Bounded memory, cached geometry, sleeping/polling while idle | Full geometry every camera frame, huge supersampled textures, driver VSync waits |
| Arrow appearance | Rounded head vertices and curved 90-degree bends; retain cached analytic coverage | Extra per-frame static traversal or a large raster cache |
| Themes | Dark board/HUD default, Light optional; menus/results always dark | Switching all screens to Light was considered but not selected |
| Menu selection | One persistent current item shared by mouse/keys | Restoring a separate keyboard bookmark on mouse exit caused gap flicker |
| Fonts | Embedded Inter with native Windows smoothing throughout UI | Scaled/blurred text and mixed unsmoothed labels |
| Fullscreen | Desktop-resolution borderless window, restored cursor | Exclusive mode caused resolution/aspect problems and brief blackouts |
| HUD/legend | Protected HUD, back arrow at left, board above legend | Gameplay over HUD or reserving another bottom gameplay strip |
| Board navigation | Wheel/MMB plus LMB drag from arrows or empty space; Ctrl +/- centred zoom and Ctrl+0 fit | A drag must never launch an arrow, even after returning to its origin; tolerate up to 6 screen pixels of click jitter |
| Sounds/guides | Optional, off by default, persisted; HUD shares sound preference | Surprise sound at startup or replay on redraw/unmute |
| Results | Finite effects; preserve the fatal pose before delayed defeat | Immediate Game Over hiding the mistake; endless finished-game work |
| Release trust | Build in Actions, bot-created release, hashes and provenance | Claiming a local EXE is the attested Actions artifact |
| Documentation | Mirrored docs/en and docs/ru with shared screenshots; English-first AI navigation | Diverging technical manuals in ai-docs or duplicated images |

User feedback confirmed that large-board panning, antialiasing, and multi-instance
responsiveness improved. Historical resource-pressure freezes are a reason to
preserve the performance design, not a current benchmark or a diagnosed driver bug.

## Open work and limitations

- Rounded rendering is accepted and its earlier commit/push hold is lifted.
  Integrated-GPU performance remains unmeasured: OpenGL 3.3 Core is required;
  modern iGPUs are expected to cope, but RTX 5070 timings are not iGPU timings.
- Game.cpp remains large; refactor only in scoped steps that preserve event
  ordering, rendering invalidation, and testability.
- Very Hard now uses bounded frontier construction for 0.4.0;
  see [generator work](generator-0.4.md) for the method, measurements and limits.
  This is separate from the earlier rendering work and is not in released 0.3.0.
- Linux/Android are unverified; even dimensions are a generator restriction.
- A local, user-owned, untracked `TODO.txt` mentions softer visual rounding of
  arrowheads/body corners. The accepted renderer implements that live request;
  preserve the file unless asked otherwise.

## Continuity discipline

Keep behavior and the explanation of why together. Distinguish completed work,
ideas, and historical evidence. After a relevant change, revise this topic or
its technical page rather than appending a raw conversation transcript. Use
English and commit/push completed task changes as AGENTS.md requires. Source
and current user instructions win over a stale snapshot.
