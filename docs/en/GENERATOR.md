# Constructive level generator

**English** | [Русский](../ru/GENERATOR.md)

The implementations are `src/Generator.cpp` and `src/FrontierGenerator.cpp`; the public interface is
`include/arrow/Generator.hpp`, and statistical/regression checks are in
`tests/GeneratorTests.cpp`.

## 1. generateBoard() contract

```cpp
struct GeneratorOptions {
    int columns{20};
    int rows{20};
    int minimumLength{2};
    int maximumLength{30};
    std::uint32_t seed{0};
    int candidateAttempts{8};
    bool allowArrowLoops{true};
};
```

A successful result must:

- have positive even dimensions no smaller than 3 x 2;
- cover every cell with exactly one arrow;
- contain connected, non-self-overlapping bodies without repeated cells;
- respect the length range after safe clamping;
- have a constructive order that removes all arrows;
- handle dynamic self-passes correctly;
- be deterministic for identical options within the same implementation.

If no suitable candidate is found, generation throws `std::runtime_error`.
There is deliberately no simplified fallback level.

## 2. Overall pipeline

The path-cutting pipeline below remains in use for Simple, Middle, Hard, and
custom length constraints. Development toward 0.4.0 uses frontier construction
for boards with both sides at least 128 cells, minimum length 2, and maximum
length at least 30. Narrow custom boards retain the path cutter. See section
11. Saved games retain their geometry across this change.

```text
random spanning tree over a grid of 2x2 blocks
                ↓
one Hamiltonian cycle through all cells
                ↓
open the cycle at the boundary → directed path
                ↓
analyze exit rays and valid intervals
                ↓
DP/range-based reachability of arrow counts
                ↓
cut the path into arrows with valid dependencies
                ↓
measure quality and verify the solution order
                ↓
best of N parallel candidates
```

## 3. Constructing the Hamiltonian cycle

`makeLabyrinthCycle()` divides the board into 2 x 2 macro-cells. Each begins as
an independent four-cell cycle. A randomized Kruskal algorithm with
`DisjointSet` constructs a spanning tree over the macro-cell graph.

Each tree edge cuts one edge in each neighboring cycle and reconnects them
crosswise. Because the macro-graph is a tree, the result is one cycle visiting
every cell exactly once.

This imposes a strict restriction: both board dimensions must be even.

## 4. Opening the cycle

`findOpenings()` searches for boundary cells from which one of the two cycle
traversals points outward. `openCycle()` breaks the cycle and reverses it if
needed. The resulting `path` defines a total order of cells that is then split
into consecutive intervals.

Each interval becomes an arrow:

- the first cell is its head;
- the last cell is its tail;
- the first two cells determine direction;
- interval order is also the proposed solution order.

## 5. Rays, intervals, and dependencies

`analyzeRays()` examines the head's exit path for each potential arrow in advance.
`buildIntervalFacts()` caches interval validity accounting for:

- minimum and maximum length;
- the head's exit along its direction;
- blocking cells belonging to other future arrows;
- genuine self-collisions;
- permitted dynamic self-passes;
- the `allowArrowLoops` setting.

A cut is valid only when dependencies on earlier arrows preserve the
constructive removal order.

Small boards use bitset DP in `reachableArrowCounts()`. Large boards use the
exact range-based `reachableArrowCountsByRanges()` to avoid a dense
area-by-arrow-count table. `findIntervalsForCount()` reconstructs a particular
cut; reverse dependencies update only affected choices as availability changes.

## 6. Lengths and arrow counts

`makeLengthProfile()` and `preferredArrowCounts()` create a broad mixture of
short, medium, long, and very long arrows. The generator does not simply
maximize the number of segments, which would produce many trivial arrows.

The target number of opening moves depends on board size and is chosen
randomly for each seed:

| Maximum dimension | Opening-move target |
| --- | --- |
| Up to 20 | 2..4 |
| Up to 40 | 2..5 |
| Up to 80 | 3..7 |
| Above 80 | Computed range; 6..10 for 160 x 160 |

For large boards the bounds use `clamp(maxDimension / 40 + 2, 4, 8)` and
`clamp(maxDimension / 16, 6, 12)`. This is a search target, not a separate
postcondition, so the actual count may differ slightly. It creates multiple
independent untangling fronts without giving the player a fixed opening hint.

## 7. Quality metrics

`measureTangle()` builds a cell-owner map and measures, among other things:

- short, medium, long, and very long arrow counts;
- turn counts;
- diversity across the four directions;
- horizontal and vertical boundaries between different arrows;
- repeated appearances of one arrow on a scanline;
- bounding-box fill of long arrows;
- opening and subsequent move choices;
- the number and complexity of exact dynamic self-passes;
- large rectangular S-shaped territories;
- validity of the proposed solution order.

`scoreCandidate()` rewards dense mixing, direction balance, length variety,
and complex self-passes with minimal clearance. Long arrows whose heads follow
their tails into newly vacated cells are especially valuable.

Large rectangular S-shaped regions receive a severe penalty: they are solvable
but look like separate territories rather than a tangle of threads.

Standard 20 x 20 boards also have a strict quality floor,
`meetsDefaultQualityFloor()`. Frontier boards have the additional hard limits
described in section 11. The other sizes choose the best valid candidate by
score and have separate test thresholds.

## 8. Parallel candidates and determinism

Each attempt gets its own `std::seed_seq`, including the seed, dimensions,
length bounds, and attempt number. Attempts run with
`std::async(std::launch::async, ...)`; results are read in attempt order and
compared deterministically.

All four presets currently use eight candidates:

| Preset | Board | max length | candidates |
| --- | ---: | ---: | ---: |
| Simple | 20 x 20 | 30 | 8 |
| Middle | 40 x 40 | 60 | 8 |
| Hard | 80 x 80 | 120 | 8 |
| Very Hard | 160 x 160 | 960 (frontier profile caps bodies at 120) | 8 |

In Game.cpp, Hard can retry the entire `generateBoard()` up to three times with
a different seed after `runtime_error`. Other presets currently get one seed
attempt.

Frontier candidates use at most four workers. Each candidate tries at most 32
cycles, stopping at its first complete board that passes every quality limit.
Attempts have independent RNGs; results are still compared in attempt order,
so scheduling does not choose the board. Exhaustion is an explicit generation
error, never a relaxed-quality fallback. Smaller presets keep their existing
eight parallel attempts and generation behavior.

The 0.3.0 Very Hard path cutter could wait tens of seconds for its slowest
candidate and allocate hundreds of MiB in dense interval-score tables. Do not
reintroduce that search as a fallback or increase concurrency without measuring.

## 9. Solvability and Board::canExit()

The cut constructs a removal order, which quality measurement verifies again.
`Board::canExit()` independently simulates one arrow's motion. It excludes the
tail cell from self-collision on the current tick because the tail vacates it
at the same time.

`Board::solveGreedily()` repeatedly removes all available arrows and must return
the complete list. Tests also verify geometry, complete coverage, length
distribution, and statistical quality across many seeds.

## 10. Changing the generator safely

State the metric to improve before editing. After a change:

1. Compare several fixed seeds before and after.
2. Run the full `arrow_tests`.
3. Compare arrow counts, opening moves, and length mix.
4. Visually inspect at least 20 x 20, 40 x 40, and the affected larger preset.
5. Measure timing on multiple seeds, not just one.
6. Ensure `allowArrowLoops=false` still works.
7. Do not weaken thresholds just to pass without explicit product justification.

Particularly risky changes include:

- changing path orientation or the head-first convention;
- replacing dynamic self-collision with static checks;
- saving only a seed instead of geometry;
- sharing one RNG across parallel attempts;
- falling back to a simple serpentine board;
- exhaustive area-by-arrow-count work on 160 x 160 and larger boards.

## 11. Large-board frontier construction (development toward 0.4.0)

The random macro-tree still supplies a maze-like Hamiltonian cycle, but the
generator no longer has to follow one global orientation or solve a giant
arrow-count DP. `partitionFromFrontiers()` removes an escapable contiguous
fragment from either direction, records that arrow, and updates affected
fronts. Repeating this until the board is empty constructs the solution order.
Only a complete partition can become a candidate.

Each cell has two possible heads. For each head, compact arrays store the
available body length, self-collision bound and currently required length.
If an exit ray hits body offset `k` at tick `d`, a legal self-pass requires
`length <= k + d`; with loops disabled it requires `length <= k`. Every still
occupied ray cell must belong to the selected body; all other ray cells must
have been removed earlier. This is a dynamic tail-vacating check, not a static
ban on crossing the body's original cells.

Cuts cannot leave a one-cell fragment on either side. Both odd and even lengths
remain possible. Cached contiguous lengths use a `lengthLimit + 2` sentinel so
a cut that would leave one cell can be distinguished from a longer remainder.
After a cut, only heads along affected rows, columns and nearby cycle fragments
are refreshed. Indexed sets contain each eligible head once; there is no growing
queue of obsolete entries or stored reverse-ray graph. Frontier construction
uses O(board area) working storage; the existing final quality scorer separately
builds the dependency graph.

Each selection samples 48 fronts and scores a small range of lengths. A broad
short/medium/long profile, a penalty for short border arrows, and a preference
for bodies connecting the edge to the interior prevent the repetitive border
chain. Opening arrows favor longer bodies that expose useful interior paths.
Long, nearly filled rectangular bodies are rejected. The local profile caps
length at `min(requested maximum, 120)`; the requested maximum remains an upper
bound, not a promised length.

Every completed frontier candidate must have all four directions, a valid
removal order, no long rectangular territories, 30..60% short arrows, at least
30% medium arrows and 9% long arrows. At most 20% of perimeter cells may belong
to short arrows (length <= 5); at least 75% must belong to arrows that also
enter the interior. Opening counts must lie in the size-dependent range from
section 6 (6..10 for Very Hard). These limits also apply with loops disabled.

### Reproducing measurements

Build the explicit `arrow_generator_benchmark` target in Release, then run:

```powershell
.\tools\benchmark-generator.ps1
.\tools\benchmark-generator.ps1 -NoLoops -OutputCsv build\generator-no-loops.csv
```

Each seed runs in a new process. CSV records generation time, Windows peak
working set in MiB, arrow and border counts, a geometry hash and independent
solution verification. The timed region excludes that verification; memory
is captured before it. `-1` means the peak-memory metric is unavailable.
This measures the core process, not rendering/GPU allocations in the game.
Use `-Executable` to compare another build on the same machine. The fixed
regressions include multiple Very Hard seeds in both loop modes, rectangular
boards, the length cap, odd lengths and repeatability under parallel execution.
