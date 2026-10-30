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
renamed 59,728-byte Noto Sans SC subset; reproduction and pinned upstream
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
Linux production smoke explicitly exercises Left/Right, wraparound,
Enter/Space and persisted values. Windows Computer Use arrow-key injection
did not change the choice; physical Windows arrow keys remain a manual check.
CI results will be recorded after verification.
