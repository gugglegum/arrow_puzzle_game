"""Generate the original two-note hint chime using only the Python standard library.

The committed WAV is embedded; Python is not a build/runtime dependency.
"""

from array import array
from math import exp, pi, sin
from pathlib import Path
import sys
import wave

RATE = 48_000
samples = []
for frame in range(23_040):
    time = frame / RATE
    value = 0.0
    for start, frequency in [(0.0, 659.255), (0.11, 987.767)]:
        local = time - start
        if local >= 0:
            envelope = (1 - exp(-local / 0.003)) * exp(-local / 0.085)
            value += envelope * (sin(2 * pi * frequency * local) + 0.18 * sin(4 * pi * frequency * local))
    value *= min(1.0, (0.48 - time) / 0.04)
    samples.append(value)
scale = 0.45 * 32767 / max(abs(value) for value in samples)
pcm = array("h", (round(value * scale) for value in samples))
if sys.byteorder != "little":
    pcm.byteswap()
output = Path(__file__).resolve().parents[1] / "assets/sounds/hint-chime.wav"
with wave.open(str(output), "wb") as wav:
    wav.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
    wav.writeframes(pcm.tobytes())
print(output)
