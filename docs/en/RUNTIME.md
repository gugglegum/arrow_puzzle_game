# Game loop, motion, collisions, and persistence

**English** | [Русский](../ru/RUNTIME.md)

The main controller is `src/Game.cpp`. Pure mathematical and collision logic
lives in `MotionKinematics.cpp` and `MotionResolver.cpp` so it can be tested
without raylib.

## 1. Static and live geometry

`Board` holds the original arrows and does not change during a game.
`RuntimeArrow` in Game.cpp holds live state:

- current `cells`;
- `targetCells` for the next cell transition;
- `history` for rewinding after a failure;
- `direction`;
- `Motion`: `Idle`, `Forward`, `CollisionPause`, `Returning`;
- fractional `progress` and independent `speed`;
- `launchDelay`, separating a new launch from accumulated simulation debt;
- `present`, `failed`, `movingCollisionTint`.

Logical simulation uses grid cells while the displayed body is interpolated at
pixel resolution. This combines exact collisions with smooth animation.

## 2. Application states

`Screen` contains:

- `MainMenu`;
- `NewGameMenu`;
- `SettingsMenu`;
- `Playing`.

Background generation is not another Screen. It is represented by a
`std::future<Board>`, `generationInProgress`, and an optional error message.
Only the main thread uses raylib; the worker produces a value-type Board that
is accepted at a frame boundary.

## 3. Smooth accelerated motion

Parameters in Game.cpp:

```text
initial speed       16 cells/s
acceleration        88 cells/s²
maximum speed       none
```

`acceleratedDistance()` computes distance over time; `timeToCoverDistance()`
computes time to the next cell boundary. The main loop finds the earliest event
across all flying arrows, advances every active arrow to that event, and only
then resolves discrete transitions.

An arrow can cover several cells in one displayed frame, but no cell boundary
is skipped. This prevents high-speed tunnelling.

An event budget (`kMaxForwardEventsPerFrame`) bounds work per frame. Unprocessed
time is carried as `forwardTimeDebt`, not discarded. A new arrow receives
`launchDelay == forwardTimeDebt`, so it starts at its own initial speed rather
than aging retroactively with earlier arrows.

## 4. Preparing and completing transitions

For an active arrow, `advancedCells()` moves the head by `direction` and gives
each other cell its predecessor's position. This becomes its proposal or
`targetCells`.

A cell event has two stages:

1. Prepare proposals and determine whether each transition may begin.
2. At the boundary, revalidate simultaneous completions and only then commit
   accepted transitions.

Revalidation is necessary because multiple arrows may reach a boundary at the
same instant.

## 5. MotionResolver

`resolveMotionStep()` receives a `MotionBodyView` for each arrow and is
independent of Game/raylib. Roles are:

- `Static`: a stationary obstacle;
- `Moving`: in flight but not completing a transition in this event;
- `Advancing`: currently attempting to enter its proposal.

The result separates:

- `hardFailures`: self-hit, empty proposal, static obstacle, or chained
  blockage by an unsuccessful advancing arrow;
- `movingContacts`: the matrix of ongoing moving-arrow contact episodes;
- `hasNewMovingContact`: a new episode that costs a life.

When board bounds are known, static cells are collected into a dense occupancy
grid. This is much faster than pairwise searches on large boards.

## 6. Collision semantics

### Static arrow or own body

- One life is lost.
- The arrow turns red.
- It briefly stays in `CollisionPause`.
- It rewinds through recorded `history`.
- After recovery it remains `failed=true`, and therefore red while idle.

The tail cell is not an obstacle if the same transition vacates it.

### Two moving arrows

- Both continue through each other.
- Both keep red `movingCollisionTint` for the rest of their flight.
- A life is lost only at the start of a new contact episode.
- All forward flight pauses for `kMovingCollisionPause == 0.5` seconds.

`movingContacts` tracks pairs between events. This matters when one arrow
follows another: continuing rear-end contact must not repeatedly consume lives.
MotionResolver also detects a head swap, when two heads exchange cells in one
tick.

`flyingArrowCollisions=false` disables moving-arrow penalties, not static
obstacles or genuine self-collision.

## 7. Rewinding after failure

Each accepted forward transition records a `HistoryState` with its logical tick
and cells. Recovery uses a separate clock, debt, and event budget to restore
the original body without jumps or freezing a frame on a long, fast arrow.

Do not merge forward debt and recovery debt: their time scales and rules differ.

## 8. Input

An arrow launch has two input events:

1. `MOUSE_BUTTON_LEFT` pressed performs a hit test and stores `pressedArrow`.
2. Release repeats the hit test and launches only if the indices match.

Moving the pointer away before release cancels an accidental click. Launch
input is blocked during rewind, collision pause, moving hit-stop, generation,
and after the last life is lost.

Camera controls:

- Wheel zooms around the pointer.
- Middle-button drag pans the board.
- Zoom is bounded to 0.05..3.5 so Very Hard can fit the window again after zooming in.
- F11 or Left Alt+Enter switches a borderless fullscreen window at the desktop
  resolution, without entering an exclusive display mode.

Menus support mouse, Up/Down, Enter, and Space for settings including theme.
`MenuSelection` stores one current item. Hover selects it; moving into a gap or
outside the menu keeps it selected. Up/Down moves from that item even with the
pointer resting over another row. New pointer movement over an item or a click
selects by mouse again. Disabled items are skipped. Initial selection is the
first enabled item unless the pointer is over another row. `MenuScreen` only
draws the supplied selection.

The back arrow left of the title uses the same save/menu path as Escape; the
speaker button changes the shared persisted sound preference.

## 9. Rendering and performance

Static arrows and grid dots use two cache levels:

- `StaticBoardGeometry` (`src/StaticBoardGeometry.cpp`) stores prepared meshes
  in board coordinates. They rebuild for a new/restored board, an arrow launch,
  or recovery to idle, not for camera movement.
- `RenderTexture2D` stores a viewport-sized image. Pan, zoom, and window resize
  update this layer from the already uploaded geometry. Idle frames reuse it.

Lines, circles, and arrowheads share an embedded coverage shader
(`src/BoardCoverageShader.hpp`, GLSL 330 for the current desktop backend). It
computes how much of each pixel a shape covers. For example, a 1.6-pixel stroke
does not alternate between one and two fully black pixels during panning.
There is no extra high-resolution raster copy. Zoom is passed through
`StaticBoardGeometry::draw()`; the GPU expands the half-pixel primitive edge
without rebuilding meshes. Lines filter their side edges; round joints cover
internal segment ends, which are not filtered again, avoiding pale seams.

`ArrowStroke.hpp` merges straight runs for idle, flying and rewinding arrows
without allocating another path. Its 90-degree bends use quarter-circle strokes
with an 8-world-unit centreline radius (1.25 times the stroke thickness), reduced
continuously for short head/tail runs. The same quad includes tangent straight
stubs, keeping its raster boundaries away from the curve even at zoom 0.05.
Each bend replaces one join quad; round end caps are integrated into the
first/last run instead of overlapping discs.
Arrowheads round all three vertices with a 1.6-world-unit radius, still using
one quad. Their exact coverage is integrated as straight-sided and circular
strips, with no per-fragment polygon arrays or new vertex buffers/textures.
Simulation cells, collisions and hit targets retain their original geometry.

Grid dots belonging to idle arrows are omitted from the mesh, including at
bends. Launching an arrow restores its dots to the static cache so they appear
as its body moves away. Flying arrows use the same
shader through `beginDynamic()` / `endDynamic()` and raylib's streamed batch;
they do not get new persistent meshes every frame. HUD and overlays draw
separately. All gameplay, including flight, is clipped below the HUD separator.
The controls legend draws behind the board and does not cover arrows.

`BoardPalette` supplies Dark/Light board and HUD colors. Crosshair guides draw
only exposed dots in the chosen row and column: subtractive color in Light,
coverage-weighted additive color in Dark, preserving destination alpha. Neither
theme nor guides require another full-size texture.

On Windows, `UiText` obtains masks for embedded Inter through GDI using system
font-smoothing settings, including ClearType. RGB masks are composed over the
background. Its bounded cache is released before the graphics context closes.
Menus, HUD, legend, and result screens share this text backend.

CPU vertices are freed after upload. RAM retains small index arrays required
by raylib; the temporary upload batch is bounded to 16,384 vertices. The cache
does not contain a raster of the entire board at world scale. Release meshes
and shaders before `CloseWindow()`.

Do not reintroduce full traversal/tessellation of all static arrows every frame,
especially at 160 x 160. Holding the middle button without pointer movement
must not invalidate the image layer.

Unchanged menus and idle boards also skip new GPU frames. Input and worker state
continue polling at up to 60 Hz through `PollInputEvents()`. `FramePacer` uses
`steady_clock` and `sleep_until()`; forced VSync and raylib's own limiter are
disabled. Independent clocks prevent idle skips of `EndDrawing()` from adding
accumulated time to the next launch. Flight, generation, and automated
screenshot scenarios still draw frames. A long window-event stall forces a
refresh.

The GPU antialiasing check is not part of default `ctest`, keeping core tests
usable without a display. Build and run it from Developer PowerShell:

```powershell
cmake --build build/windows-msvc --target arrow_render_tests
.\build\windows-msvc\arrow_render_tests.exe
```

It checks 5,412 subpixel primitive placements and 1,968 complete rounded-stroke
placements, with analytical area references and zooms down to 0.05. Another
336 comparisons cover cached/streamed short tails and merged straight cells,
including vanishing
inner bend radii. Geometry byte budgets, streaming-batch overflow, and 90 guide
recolouring placements across both themes are checked too. The tests inspect
pixels read back from the GPU; they do not establish performance on other GPUs.

`forwardPolyline()` and `trimPolylineTail()` preserve orthogonal corners:
animation extends the head and trims the tail without diagonal warping.

## 10. Background generation

`startGeneration()`:

1. Switches UI to Playing with an overlay.
2. Calls `generateBoard()` through `std::async`.
3. Keeps processing the window and animating the indicator.
4. Accepts a completed Board only on the main thread.
5. Offers retry on failure instead of a simple fallback.

Hard can change seed and retry up to three times. Never call raylib in a worker.

On exit, an existing future waits for completion: a standard `std::future`
cannot simply be abandoned while its worker still uses captured data.

## 11. Settings and saved games

Persistence stores these files beside the EXE, not in the working directory:

- `settings.txt`;
- `savegame.txt`.

Both are text formats with `format=` and `version=1`. Writes use a temporary
`.tmp` followed by replacement of the destination. Public persistence operations
do not throw on filesystem failures.

### settings.txt

Contents:

```text
format=arrow-puzzle-settings
version=1
arrow_loops=on|off
flying_arrow_collisions=on|off
crosshair_guides=on|off
sounds=on|off
board_theme=dark|light
```

A missing or noncanonical file is repaired with safe values. Menu changes are
written immediately and update in-memory Settings only after a successful write.
Loops and flying collisions default to on, guides and sounds to off, and theme
to Dark. Missing new keys receive these defaults while valid old preferences
are retained; the format version remains 1.

### savegame.txt

The save contains dimensions, lives, each arrow's complete original geometry,
and `present`/`failed` flags. It does not store a seed.

Transient animation state (mid-flight cells, speed, progress, history, and pause)
is not saved. Resume restores remaining arrows at their original idle positions;
removed arrows stay absent, and failure tint and lives are retained.

A game is saved when returning to the menu or exiting if:

- there is an active level;
- generation is finished;
- arrows remain on the board;
- at least one life remains.

Victory or loss of all lives removes the save and disables Resume. Corrupt or
incompatible saves are ignored.

`EndScreen` displays YOU WON! or GAME OVER; Game stops gameplay updates and
releases rewind history. Fireworks and their reports are finite; after the
animation, unchanged result frames are also skipped. Sounds do not replay on
redraw or Resume. `GameAudio` opens the device only when Sounds is enabled and
uses embedded WAV data, so no external sound directory is needed.

After the last life, the outcome becomes active immediately, but Game Over
appears after 1.2 seconds. `EndScreen::active()` blocks further simulation while
`visible()` stays false during the delay, retaining the fatal collision frame.
Interpolation targets remain until reveal so flying arrows do not snap backward.
Collision sound plays at impact; defeat sound plays on reveal. The timer itself
requires no GPU frames; rendering resumes for the result animation when it ends.

## 12. Runtime regression checks

Required regressions include:

- independent initial speeds for two arrows;
- no tunnelling when several cells are crossed in one frame;
- exact dynamic head-following-tail passes;
- genuine self-hit;
- head-on and head-swap moving contacts;
- rear-end contact without repeated life loss;
- chained blockage of advancing arrows;
- event budgets preserving debt;
- mouse cancel-on-release;
- Resume after partially clearing a level;
- corrupt settings/save input.

After a visual change, create and inspect an automated PNG. After collision
changes, run the full `arrow_tests`, not just a manual playthrough.
