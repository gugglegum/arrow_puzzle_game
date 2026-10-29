# Rounded-arrow rendering

2026-10-29. The user requested softer arrowheads and 90-degree bends, with no
noticeable performance or memory regression. The user accepted the appearance,
speed improvement and small memory overhead, and explicitly authorized the
commit. The earlier commit/push hold is lifted; follow AGENTS.md's normal
publication workflow. Baseline: `1d1cc36`. No release was requested.
`TODO.txt` is unrelated, user-owned and remains untouched.

## Design and verification entry points

- `ArrowStroke.hpp` streams merged orthogonal runs from idle cells or existing
  flight/rewind points; it allocates no additional path. The bend radius is
  bounded by 8 world units and one quarter of each adjacent run, so trimming
  stays smooth. Each bend quad also contains a tangent straight stub of the
  same length; its raster boundary therefore lies away from the curve. This
  prevents weight changes even at the actual minimum camera zoom of 0.05.
- Each bend replaces an existing join quad. Integrating round caps into runs
  removes two old cap quads from every multi-cell idle arrow; streaming also
  stops emitting a line/disc pair for every collinear cell.
- Heads retain one quad, with a 1.6-world-unit corner radius. The coverage
  shader uses exact integrals of linear and circular vertical strips. An early
  polygon/circle-clipping prototype increased driver private memory and was
  replaced; do not restore its per-fragment arrays.
- Existing position/UV/color attributes carry the shapes; the unused fraction
  of the position Z shape tag carries the bend radius ratio. There is no new
  texture, per-arrow mesh, per-frame persistent upload, or vertex attribute.
- Gameplay cells, collision handling, click targets and idle invalidation are
  unchanged. Idle-cell lattice dots stay hidden, including at curved bends;
  launch restores the path dots as before.
- `arrow_render_tests` checks 5,412 primitive and 1,968 rounded-stroke coverage
  samples, 336 short-tail cached/streamed comparisons, geometry byte budgets,
  streaming-batch overflow and 90 guide recolouring samples in both themes.

## Local evaluation method

Compare separately compiled baseline/current Release code against the same
saved 160 x 160 board, without generation during measurements. Use the real
`cacheIdleBoardGeometry()` and `drawArrow()` paths, a 1100 x 900 render target,
five zooms (fit approximately 0.139434, 0.25, 0.5, 1, 3.5), and GPU completion
before/after timing. Warm up 40 frames, then measure 180 frames per case. Test
panning and a mixed board with 64 moving arrows near the viewport centre.
Repeat in fresh processes and reverse variant order. A second complete board
checks a different arrow/turn composition. Results are local samples, not
guarantees for other machines.

Separately instrument production Game.cpp in disposable copies: hide the
window, count BeginDrawing, exit on a timer and synthesize middle-button pan.
Remove the screenshot requirement only for automatic Resume in these QA
copies. Keep SCREENSHOT empty for idle tests; otherwise it forces continuous
redraws. Measure process CPU, working set and private memory after warmup.
Driver allocations and Windows process working sets vary substantially between
fresh graphics contexts; report repeat ranges and absolute differences.

All fixtures, executables, logs and screenshots live in ignored
`build/rounded-arrows-qa/`. Their scripts are local evidence, not maintained
tests or dependencies of this roadmap. Preview saves/settings are isolated
from the user's normal EXE. Rebuild current source before refreshing captures;
copies of the EXE do not update themselves.

## Evaluation results

Verified locally on Windows/MSVC Release, NVIDIA GeForce RTX 5070, 24 logical
processors. The primary fixture fills all 25,600 cells with 3,445 arrows;
the second fills the same dimensions with 2,844 arrows. The values below are
from the final tangent-stub/array-free implementation, not earlier prototypes.

| Primary board, fit zoom | Baseline | Rounded preview |
| --- | ---: | ---: |
| Pan frame median, average of 3 process medians | 0.17223 ms | 0.14157 ms |
| Pan frame mean, average of 3 runs | 0.22605 ms | 0.18185 ms |
| 64 moving arrows, frame median | 0.34780 ms | 0.21643 ms |
| 64 moving arrows, frame mean | 0.44021 ms | 0.27717 ms |
| Static mesh bytes on GPU | 3,122,712 | 2,378,592 |
| Warm mixed-board geometry rebuild | 4.86376 ms | 4.19581 ms |

Static mesh storage is 23.8% smaller (2.98 to 2.27 MiB). The retained CPU index
arrays shrink by the same proportion; this is not a 23.8% reduction in total
process/driver memory. All five measured zooms improved in median render time,
and the second fixture also improved. The first cache-build average includes
shader creation: 8 rebuilds averaged 9.26 ms baseline / 17.11 ms preview;
that initial shader setup is separate from steady rendering.

Three production-controller pan runs per variant, with two seconds warmup and
about ten seconds sampled, submitted 1,024 / 1,026 frames in about 30 seconds.
The hidden-window pacing limit applies to both; these are not foreground FPS
claims. CPU time was 765.625 / 890.625 ms, or 0.106% / 0.124% of the machine's
24 logical processors. The absolute difference is about 0.017 percentage
points (4.1 ms CPU per second), rather than a claim of literally identical CPU
cost. Driver/context variation prevents a general cross-machine guarantee.

Paired steady pan working sets differed by +0.97 to +2.47 MiB (mean +1.66);
private bytes by +0.84 to +1.19 MiB (mean +0.99). In the idle board run, working
set was 76.67 / 76.28 MiB and private memory 137.26 / 137.06 MiB. Each variant
used 15.625 ms CPU and drew **zero** new frames during the ten-second idle
sample. The initial menu also drew no new frames after its first image.

Startup peak working-set differences in those pan processes ranged from +1.1
to +14.1 MiB, including graphics-driver/font initialization. Fresh graphics
contexts also varied by about 60 MiB in their overall allocation baseline.
There is a small steady memory overhead and a variable startup peak; do not
describe this preview as a proven zero-memory-growth change.

Validation: Release build, all 4 default CTest suites (38.17 seconds), and the
expanded GPU tests passed. The GPU tests include the actual minimum zoom 0.05,
not just fitted-board zooms. Inspected current screenshots cover Dark/Light,
20/40/160-cell boards, flight, and an enlarged before/after comparison. Geometry
and colours look consistent in those captures; the user accepted the visual
result and measured resource tradeoff. No gameplay/simulation behavior was
intentionally changed.

The isolated trial EXE is `build/rounded-arrows-qa/preview/ArrowPuzzle.exe`, with
sound disabled and a sample 20 x 20 save accessible through Resume. Its SHA-256
matches the Release EXE used for the evaluation. Source publication does not
replace the existing 0.4.0 release artifacts. Inspect Git history for the feature
commit; the performance comparison above remains against baseline `1d1cc36`.

## Integrated graphics

The Windows build requests OpenGL 3.3 Core and the board shaders use GLSL 330;
a discrete GPU is not required. The [Intel API support table](https://www.intel.com/content/www/us/en/support/articles/000005524/graphics.html)
lists modern UHD/Iris Xe above this minimum, but HD 2000/3000 only at OpenGL
3.1. The [Khronos conformance register](https://www.khronos.org/conformance/adopters/conformant-products/opengl)
also lists Ryzen APUs such as 5600G/5700G with OpenGL 4.6 on Windows. These are
API compatibility facts, not Arrow Puzzle frame-rate measurements.

Smooth 1080p play on modern integrated graphics is an expectation based on the
small cached renderer, not a verified iGPU benchmark. Check 160 x 160 boards,
4K panning, many moving arrows and result effects on actual target hardware
before promising 60 FPS. Generation itself is CPU work. All measured rendering
results on this page came from the RTX 5070.
