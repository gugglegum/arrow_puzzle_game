These original procedural sounds were created for Arrow Puzzle and are
distributed under the project's MIT license.

All files are 48,000 Hz, mono, 16-bit PCM:
- b2-light-pistol.wav: arrow launch, 190 ms.
- c3-metal-clack.wav: collision / life lost, 190 ms.
- d4-retro-fail.wav: all lives lost, 940 ms.
- v3-glass-chord.wav: board cleared, 1,120 ms.
- firework-burst.wav: firework report with rumble, crackles and reflections,
  960 ms. Reproduce with tools/generate-firework.py (optional NumPy tool).

CMake embeds these sounds in the executable. No separate audio files are needed
when running the game.
