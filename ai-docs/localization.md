# Interface localization (2026-10-30)

Requested behavior: a sideways language selector like Board theme, Auto by
default, supported system language or English fallback, translations embedded
in the executable, shared Windows/Linux implementation.

Implemented choices, in order: Auto, English, Español, Français, Deutsch,
Português, 简体中文, Русский, Українська. Chinese is Simplified Chinese;
all regional Chinese locales select this translation. Native names remain
readable when the rest of the UI changes. Settings persist stable ASCII codes
`auto/en/es/fr/de/pt/zh/ru/uk`, retaining version 1 and old valid preferences.

The maintained contract is in the [runtime guide](../docs/en/RUNTIME.md#interface-localization).
Source entry points: `include/arrow/Localization.hpp`, `src/Localization.cpp`,
`Game.cpp`, `MenuScreen.cpp`, `EndScreen.cpp`, `UiText.cpp`, persistence tests.
The catalog statically rejects missing/empty translations or misordered IDs.

Inter already contains Latin/Cyrillic including Ukrainian. Chinese uses a
renamed Noto Sans SC subset; reproduction and pinned upstream
SHA-256 are in [fonts](../assets/fonts/README.md). Both platform package helpers
include the new OFL notice. Updating Chinese strings requires regenerating the
checked-in subset; the glyph coverage test detects omissions. Normal builds
need no font tools, catalog files, installed Chinese fonts or runtime network.

Validation: all six Windows Release CTest suites passed, including new pure
locale/cycling tests and settings migration, all choices and failed-write
tests. The explicit UI test passed embedded glyph coverage and native Windows
composition/cache checks, rendering all eight languages at 720x600 and 1100x900.
Screenshots were inspected locally in ignored `out/localization/windows-ui`.
The screenshot harness flushes and captures before buffer swap to avoid
capturing the previous page. Linux CI runs this same test through raylib/Mesa
and uploads its localized screen captures. A separate renamed portable Release
copy opened in Russian from Auto, switched to Ukrainian with the left mouse
chevron, retained Ukrainian after restart, returned to Russian through Auto,
and switched with Space/Enter to English/Spanish. Its settings were checked
after each action; all other settings stayed unchanged. Menu input now consumes
queued press events as well as held-state transitions, retaining short taps.
An additional isolated Windows controller probe includes production `Game.cpp`,
substitutes queued key events with all held-state queries false, and checks the
file on the following frame. It passed Left/Right, wraparound, Enter/Space,
navigation and preservation of all other settings. Windows Computer Use arrow
injection produced no queued arrow events; the deterministic probe covers the
controller path, while physical Windows arrow keys remain a manual check.

Linux production smoke explicitly exercises Left/Right, wraparound,
Enter/Space, both mouse chevrons and persisted values. It targets the Language
row explicitly in the standard 1100x900 fixture. Do not assume the first
setting remains selected across page transitions: shared pointer selection
can choose a row underneath a stationary/cached cursor. Initial keyboard-only
smoke failures were captured on the Flying arrow collisions row. XTest input
goes to the focused window; optional `xwd` captures the actual window and focus
on failure, and development CI uploads that evidence. This capture is external
and adds no release-binary screenshot hooks.

Font fitting jumps near the required integer size instead of loading an atlas
for every intermediate pixel size on Linux. It retains actual-width checks
and the existing native Windows texture cache budget. No new steady-state
memory or latency benchmark was performed.

On implementation head `8348520`, [development CI run 40997859109](https://github.com/gugglegum/arrow_puzzle_game/actions/runs/40997859109)
passed all three jobs: Windows/MSVC, Ubuntu 22.04/GCC and Ubuntu 24.04/Clang.
Each ran six default suites. Both Linux jobs also passed translated glyph/layout
rendering, Debug save/resume scenarios and ordinary/portable tar launch checks.
GCC additionally passed both AppImage variants through extraction-and-run.
Real X11 Left/Right, wraparound, Enter/Space, both mouse chevrons and on-disk
values passed in every packaged Linux variant. Release binaries retained their
expected storage modes and excluded developer screenshot features. Windows ZIP
packages and all four Linux packages were produced; no release/tag was created.
This is CI/Xvfb validation, not a new physical Linux desktop session. Existing
desktop-port validation remains documented separately in linux-portability.md.
