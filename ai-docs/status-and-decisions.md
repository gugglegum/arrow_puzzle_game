# Status, accepted decisions, and open work

Snapshot: 2026-10-29, after publication and verification of 0.4.0. Check
`git status`, recent history, and [CHANGELOG.md](../CHANGELOG.md) before treating
this as current state.

## Application icon design direction (2026-10-30)

The user selected the symmetric 4-by-4 layout with two one-bend arrows, then
requested larger dots and rounded arrowheads. The selected vector master is
`assets/icons/arrow-puzzle.svg`, with a 1024-pixel transparent PNG alongside.
The user authorized integration and a rebuild. The PNG sizes are embedded
through `EmbedBinary.cmake`; `AppIcon.cpp` sets the window icons immediately
after `InitWindow`. Windows also embeds the seven-frame ICO as `GLFW_ICON`.
See [icon maintenance](../assets/icons/README.md) for the source/export workflow.
The user prefers bent pale arrows from gameplay over generic
colorful arrow logos.
The icon should resemble the actual dark game board and preserve its rules:
fixed orthogonal grid spacing, bends and head/tail cells on grid nodes,
separate nonintersecting paths with one head each, and gray dots at every
unoccupied node. Do not invent impossible arrow junctions or arbitrary bends.

Integration validation (2026-10-30): clean primary MSVC Release build; all
five default test suites passed. Persistence initially could not create its
temporary settings file in the restricted system temp location, then passed
using TEMP/TMP under the workspace. A standalone copy of only the EXE started
successfully; the title-bar icon was visually inspected. Native resource
inspection found seven PNG entries identical to the committed exports,
Windows loaded every size, and the live window exposed the intended 16/32 px
icons. Explorer's EXE icon and an ordinary new shortcut were also extracted
and inspected (including the normal shortcut-arrow overlay). Artifact
regeneration is byte-stable and pixel-identical to the approved artwork.
Local evidence is under ignored `out/icon-integration-2026-10-30`; no release
was requested. Rebuild after commit to refresh the Git-derived version label.

Artwork refinement: dot diameter equals arrow stroke width at every export
size, matching `kGridDotRadius = kArrowThickness / 2`. The master stroke is
52/1024, hence dot radius 26. All three corners of each triangular head use
true inset circular fillets of radius strokeWidth/4 (13 in the master),
matching the game's 1.6-to-6.4 radius/stroke relationship. SVG head paths use
circular arcs without the previous sharp polygon's extra outline. Exported
16/24 px strokes are strengthened to 64/58, with dots and fillets following
the same ratios; 32 px and larger use 52. All eight export sizes were checked
for six correctly sized dots and three rounded corners per head; the actual
16/32 px rasters were also inspected enlarged. The layout and palette remain
the selected version. No application code/build change accompanied this art.

Prior request: make the 4-by-4 pair symmetric with one bend each, and render
the user's 3-by-3 ASCII sketch. The new 4-by-4 pair is A
`(0,0),(2,0),(2,2)` left and B `(3,3),(1,3),(1,1)` right, exact 180-degree
counterparts. Two removed three-cell vertical arrows explain its six dots.
The literal ASCII is A `(0,2),(0,0),(1,0)` down and B
`(2,0),(2,1),(1,1),(1,2)` up, with one dot at `(2,2)`. It is solvable under
Board movement rules but cannot result from complete removals on a generated
full board: the generator enforces arrow length >= 2. It also requires even
board dimensions, so all 3-by-3 designs are miniature icon illustrations,
not actual generator output. The comparison preserves the literal sketch
and adds a nearby alternative with B shortened to `(2,0),(2,1),(1,1)`;
its two adjacent dots can be explained by one removed two-cell arrow.
Keep the literal-art exception explicit rather than silently changing the
user's sketch or claiming generator-faithful reachability.

Previous request: extend the tails of 4-by-4 variant 1 so they wrap around each
other and finish in different directions; also compare a compact 3-by-3 board.
The revised 4-by-4 head-first paths are A `(0,0),(2,0),(2,2),(3,2)` left,
B `(3,3),(1,3),(1,1),(0,1)` right: 12 occupied nodes and 4 dots left by two
removed two-cell vertical arrows. The compact 3-by-3 paths are A
`(0,0),(2,0),(2,1)` left and B `(2,2),(1,2),(1,1)` right: 7 occupied nodes
and 2 dots left by a removed vertical arrow at `(0,2),(0,1)` exiting down.
Both passed independent and renderer-based movement/coverage validation;
neither has been selected for application integration.

The preceding round requested five variants on a 4-by-4 grid, exactly two visible arrows
per icon, retaining the slimmer game-like strokes and open composition.
The comparison contains diagonal Ls, a short return plus L, two upper-corner
Ls, a long return plus short L, and a step-shaped arrow plus short L.
Variants 1/3 occupy 8 nodes; 2/4/5 occupy 9. Each has an explicit full initial
16-node board and a verified two-arrow removal prefix, leaving the displayed
dots. All five passed independent geometry checks and the deterministic
renderer movement simulation. Master stroke width is 52/1024 on a 240/1024
grid step, with optical strengthening at 16/24 px. This remains a design
comparison, not integration or a production runtime test.

The user rejected the 7-by-7 proposal: its arrows became thin visual noise
at small sizes, and arbitrary empty nodes did not plausibly result from play.
Initial game boards are completely occupied. Icon dots must represent nodes
vacated by actual arrows that can legally leave that fully occupied board.
The user explicitly requested a 5-by-5 revision with larger arrow silhouettes.

The first, now superseded 5-by-5 proposal used these head-first vertices (expand
each straight segment into unit cells):

- A: `(0,0), (3,0), (3,3), (4,3)`, direction left, 8 cells.
- B: `(1,1), (1,3), (2,3), (2,1)`, direction up, 6 cells.
- C: `(4,4), (0,4), (0,1)`, direction right, 8 cells.
- Already removed R: `(4,0), (4,2)`, direction up, 3 cells.

Read-only simulation of `Board::canExit` semantics confirmed all 25 initial
nodes are covered exactly once, R can exit immediately, and R-A-B-C solves
the board. B depends on A; C exits independently. The three dots at `(4,0)`,
`(4,1)`, `(4,2)` are exactly R's former cells. This remains a proposal, not
a selected production asset or a runtime test. Strokes are substantially
thicker than the rejected draft; 16-24 px versions strengthen them further.
Surface and arrow colors use the dark `BoardPalette.hpp` palette; dots are
slightly brighter for small-icon visibility. The preview includes a filled
initial-board diagram showing which arrow vacated the three nodes.
The user accepted that draft's solvability but rejected its visual density,
heavy strokes, and M-like silhouette. The following round explored five
5-by-5 arrangements with more empty space and slimmer lines closer to gameplay.
The resulting comparison contains: (1) two opposite L arrows, (2) three short
L arrows pointing left/right/up, (3) two arrows forming an open frame,
(4) three L arrows around an open center, (5) a short return and a long L.
Options 1/2/5 occupy 11 nodes, options 3/4 occupy 14; all other nodes have dots.
Each was independently checked and validated by a deterministic renderer's
movement simulation against an explicit fully filled initial board, with a
legal removal prefix and complete solve order. This is geometry validation,
not a game runtime test. Their master stroke width is 44/1024 versus the dense
draft's 96/1024; smaller versions use modest optical corrections. No option
has yet been selected or integrated into the application.

`cacheIdleBoardGeometry` defines free-node dot placement. Arrow head cell
centers align to the grid; triangle tips extend beyond those centers.

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

Post-0.4.0 settings defaults are ON for all four boolean options, including
Sounds and Crosshair guides. Missing files/keys use those defaults; existing
explicit OFF values are preserved. Persistence checks cover all 16 combinations
of missing keys and explicit OFF, upgrades, malformed values and failed writes.
A standalone fresh-start Settings capture confirmed all four ON switches.
The Board theme selector also previews Light with the light board surface and
dark label/chevrons; Dark retains its gold accents. Both standalone Settings
captures were inspected; no additional texture or animation cache was added.

The user authorized local build cleanup on 2026-10-29. The primary
`build/windows-msvc/arrow_puzzle.exe` was rebuilt with these menu changes;
all five default CTest suites and a direct startup/Settings capture passed.
Fifty-four temporary directories, including the duplicate preset Debug tree,
were removed after preserving settings/saves and local QA sources/results in
ignored `ai-docs/local/cleanup-2026-10-29`. Keep the primary Release tree,
`cmake-build-debug`, `out/audio-previews`, and the official local 0.3.0/0.4.0
release folders. Historical QA paths on these pages no longer imply existing
files. The local backup is machine-specific and is not part of Git or a release.

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
| Themes | Dark board/HUD default, Light optional; selector reflects Light with a light fill; menus/results always dark | Switching all screens to Light was considered but not selected |
| Menu selection | One persistent current item shared by mouse/keys | Restoring a separate keyboard bookmark on mouse exit caused gap flicker |
| Fonts | Embedded Inter with native Windows smoothing throughout UI | Scaled/blurred text and mixed unsmoothed labels |
| Fullscreen | Desktop-resolution borderless window, restored cursor | Exclusive mode caused resolution/aspect problems and brief blackouts |
| HUD/legend | Protected HUD, back arrow at left, board above legend | Gameplay over HUD or reserving another bottom gameplay strip |
| Board navigation | Wheel/MMB plus LMB drag from arrows or empty space; Ctrl +/- centred zoom and Ctrl+0 fit | A drag must never launch an arrow, even after returning to its origin; tolerate up to 6 screen pixels of click jitter |
| Settings defaults | Arrow loops, flying collisions, guides and sounds default to ON for missing files/keys; explicit OFF is retained; HUD shares sound preference | Overwriting existing preferences or replaying effects on redraw/unmute |
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
- Native Linux/Android builds remain unverified; even dimensions are a generator
  restriction. Linux development should use the user's Mint 22.3 VMware VM;
  WSL2 is excluded to preserve the VMware setup. The user reports adequate Wine
  operation with approximately 0.5-second audio delay. Keep this observation
  separate from native support; see the [Linux workflow and handoff](linux-portability.md#current-development-environment-and-handoff).
- A local, user-owned, untracked `TODO.txt` mentions softer visual rounding of
  arrowheads/body corners. The accepted renderer implements that live request;
  preserve the file unless asked otherwise.

## Continuity discipline

Keep behavior and the explanation of why together. Distinguish completed work,
ideas, and historical evidence. After a relevant change, revise this topic or
its technical page rather than appending a raw conversation transcript. Use
English and commit/push completed task changes as AGENTS.md requires. Source
and current user instructions win over a stale snapshot.
