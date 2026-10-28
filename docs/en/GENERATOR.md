# Constructive level generator

**English** | [Русский](../ru/GENERATOR.md)

The main implementation is `src/Generator.cpp`, the public interface is
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
`meetsDefaultQualityFloor()`. Other sizes choose the best valid candidate by
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
| Very Hard | 160 x 160 | 960 | 8 |

In Game.cpp, Hard can retry the entire `generateBoard()` up to three times with
a different seed after `runtime_error`. Other presets currently get one seed
attempt.

Very Hard may take tens of seconds: the result waits for the slowest of eight
candidates. Do not increase parallelism blindly; measure time, peak working
set, and memory bandwidth.

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
