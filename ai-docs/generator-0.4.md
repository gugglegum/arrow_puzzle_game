# Very Hard generator work for 0.4.0

Status: implemented in development, 2026-10-29; not published as 0.4.0.
The previous generator source was unchanged between released `52182f1` and
the starting code snapshot `955af41`. Preserve the existing unrelated fatal
collision delay and bilingual documentation changes when publishing this work.

## Decision and implementation

- Preserve Simple, Middle and Hard generation, saved geometry, movement rules,
  and the no-serpentine-fallback policy.
- Large boards (both sides >= 128, minimum length 2, maximum >= 30) use
  `FrontierGenerator.cpp`: remove escapable cycle fragments in either direction,
  retaining their removal order as a constructive solution. This replaces the
  one-direction path/count search for Very Hard. Narrow custom boards retain
  the generic path cutter because they cannot meet the interior-linked profile.
- Use compact per-head arrays and indexed sets. Update affected rows/columns
  after each removal; do not retain dense length-score tables or a reverse-ray
  graph. Self-passes obey the exact tail-vacating tick; loopless mode disallows
  every own-body intersection with an exit ray.
- Sample 48 heads per decision; try at most 32 cycles per candidate; use at most
  four workers for eight candidates. RNG and final comparison order are
  deterministic. Failure stays explicit; there is no hidden old-generator
  fallback and no time-based RNG/scheduling decision.
- Keep 6..10 initial moves on 160x160, no long rectangular territories,
  30..60% short / at least 30% medium / at least 9% long arrows. Short arrows
  occupy <=20% of perimeter cells; >=75% of perimeter cells belong to arrows
  entering the interior. These are acceptance limits, not just score bonuses.
- Bodies use a local maximum of min(requested maximum, 120), with both odd and
  even lengths. The preset's 960 option remains a legal upper bound, not a
  promised generated length. Typical new boards contain more arrows than
  0.3.0: roughly 3.3k..3.7k rather than the old fixed 2,844. More medium and long
  threads replace much of the short-arrow population; assess play feel with
  the user before a release rather than claiming subjective fun is proven.

Algorithm details and benchmark commands live in the mirrored
[English](../docs/en/GENERATOR.md) and [Russian](../docs/ru/GENERATOR.md) guides.
`tools/benchmark-generator.ps1` launches a fresh core-only process per seed;
`arrow_generator_benchmark` is an explicit CMake target. Measurement excludes
the independent solution simulation; the tool reports that verification too.

## Before/after evidence

Local Windows x64 Release/MSVC, AMD Ryzen 9 5900X. These are observations on
this machine, not a universal latency or memory promise. Peak is Windows peak
working set in binary MiB, captured before benchmark verification allocations.

| Seed | Old seconds | New seconds | Old peak MiB | New peak MiB | Old short border cells | New short border cells |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 2.445 | 0.841 | 938.3 | 14.4 | 278 | 38 |
| 1 | 39.702 | 0.989 | 764.4 | 14.3 | 207 | 32 |
| 2 | 33.385 | 0.695 | 803.6 | 13.0 | 376 | 45 |
| 64 | 37.260 | 0.834 | 1041.3 | 14.0 | 356 | 41 |
| 747 | 30.399 | 0.693 | 857.2 | 14.0 | 368 | 36 |
| 23000 | 1.765 | 0.498 | 944.2 | 14.4 | 338 | 44 |

All six runs used eight candidates and loops enabled. Each board has 636
perimeter cells. Seed 747 changed from 1,934/613/297 short/medium/long arrows to
1,785/1,276/384; exact tight self-passes rose from 24 to 85, including 29 long
complex versions instead of 5. All six old and new boards were independently
solved; this comparison does not claim that every old seed met the new quality
requirements.

Expanded fresh-process run, seeds 0..99 in each mode (200 boards, all verified):

| Mode | Seconds min / median / p95 / max | Maximum peak MiB | Arrows | Opening moves | Short perimeter cells |
| --- | --- | ---: | --- | --- | --- |
| Loops on | 0.384 / 0.859 / 1.293 / 1.467 | 14.90 | 3334..3594 | 6..10 | 15..74 |
| Loops off | 0.571 / 1.075 / 1.468 / 1.531 | 15.08 | 3420..3661 | 6..10 | 16..68 |

At least 519/636 perimeter cells connected to the interior with loops, and at
least 524/636 without. No loopless board crossed its own body on an exit ray.
Raw local CSV/logs are disposable `build/generator-v04/` artifacts; the durable
facts and procedure above must remain usable without them.

Whole-game screenshot runs used isolated EXEs/settings/saves, seed 747 and the
same 1100x900 window. The local 0.3.0 EXE peaked at 928.1 MiB and took 32.53 s
from process launch through screenshot/exit; the new EXE peaked at 91.1 MiB
and took 1.31 s (90.8 MiB / 1.91 s with loops off). Those times include startup,
rendering, screenshot encoding and shutdown, unlike core-generation times.
Last sampled working sets were about 92.5, 81.2 and 81.9 MiB respectively;
these are not a dedicated steady-state idle-memory benchmark.

## Validation and limits

- A fresh Release build and full CTest passed: generator/motion, persistence, menu selection and
  end-screen suites. The generator suite now exercises 1,333 boards, including
  new frontier regressions in both loop modes, rectangular fields, max length
  30, one candidate, odd lengths, and exact geometry repeatability.
- Visually opened game screenshots for Simple, Middle and Very Hard, with and
  without loops on Very Hard. The short border chain is absent in the new
  captures. User settings and saved games were not used as fixtures.
- The 200-board run is a measured sample, not a proof that every seed succeeds.
  Work budgets and quality limits remain fixed; an exhausted seed returns the
  existing explicit error/retry UI. Linux/Android remain unverified.
- A source push is not a release. CMake/release tags remain 0.3.0 until the user
  authorizes preparation/publication of the next release.
