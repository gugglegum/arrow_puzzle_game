# Embedded UI fonts

Inter Medium 4.1 is unchanged and covers Latin and Cyrillic UI text, including
Russian and Ukrainian. Its license is in `Inter-LICENSE.txt`.

`ArrowPuzzleCJK-Medium.ttf` is a modified, static weight-500 subset of Noto Sans
SC, renamed **Arrow Puzzle CJK**. It contains basic Latin and all Chinese UI
characters used in `src/Localization.cpp` (264 characters, 59,728 bytes initially).
It is embedded on Windows and Linux; no installed CJK font is required.
The complete upstream license is in `NotoSansSC-LICENSE.txt` and accompanies
all binary packages.

Pinned source:

- [Google Fonts revision a85815a42757630ce188fdad368c2dfc444d4773](https://github.com/google/fonts/tree/a85815a42757630ce188fdad368c2dfc444d4773/ofl/notosanssc)
- Original file: `NotoSansSC[wght].ttf`
- Original SHA-256: `a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da`
- Subset generator: `tools/subset-ui-font.py`, using fonttools 4.61.1.

After changing Chinese translations, download that original into an ignored
directory and run `python tools/subset-ui-font.py path/to/original.ttf`.
Commit the resulting subset. Normal CMake builds need neither Python nor
fonttools or network access for fonts. `arrow_ui_text_tests` verifies the
actual rasterized glyph coverage for every translated character.
