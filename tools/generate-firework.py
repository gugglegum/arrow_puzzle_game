"""Create the original firework SFX; optional developer tool requiring NumPy.

The WAV is committed and embedded at build time; Python is not a build/runtime
dependency. No recordings or external samples are used.
"""

from pathlib import Path
import wave

import numpy as np


RATE = 48_000
rng = np.random.default_rng(2026102804)
t = np.arange(round(RATE * 0.96)) / RATE


def noise(low, high):
    size = 1 << 17
    f = np.fft.rfftfreq(size, 1 / RATE)
    weight = 1 / np.sqrt(1 + (low / np.maximum(1, f)) ** 6)
    weight /= np.sqrt(1 + (f / high) ** 10)
    x = np.fft.irfft(np.fft.rfft(rng.standard_normal(size)) * weight)
    x = x[10000:10000 + len(t)].copy()
    return x / np.sqrt(np.mean(x * x))


crack = noise(700, 9000) * np.exp(-t / 0.008)
body = 0.43 * noise(65, 850) * np.exp(-t / 0.065)
rumble = 0.15 * np.sin(2 * np.pi * (53 * t + 2.7 * (1 - np.exp(-t / 0.025)))) * np.exp(-t / 0.095)
tail = 0.09 * noise(300, 4400) * np.exp(-t / 0.13)
sparks = np.zeros_like(t)
spark_noise = noise(1300, 7300)
for position in np.sort(rng.uniform(0.055, 0.43, 15)):
    local = np.maximum(t - position, 0)
    envelope = (t >= position) * (1 - np.exp(-local / 0.00025)) * np.exp(-local / 0.0025)
    sparks += rng.uniform(0.025, 0.075) * spark_noise * envelope
dry = crack + body + rumble + tail + sparks
x = dry.copy()
for delay, gain in [(0.024, 0.19), (0.059, 0.11), (0.112, 0.07), (0.176, 0.04)]:
    n = round(delay * RATE)
    x[n:] += gain * dry[:-n]
window = np.ones_like(t)
attack, release = 20, round(0.08 * RATE)
window[:attack] = np.sin(np.linspace(0, np.pi / 2, attack)) ** 2
window[-release:] = np.cos(np.linspace(0, np.pi / 2, release)) ** 2
x *= window
x -= np.mean(x) * window / np.mean(window)
x *= 10 ** (-4 / 20) / np.max(np.abs(x))
x[0] = x[-1] = 0
pcm = np.rint(x * 32767).astype('<i2')
output = Path(__file__).resolve().parents[1] / 'assets/sounds/firework-burst.wav'
with wave.open(str(output), 'wb') as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(RATE)
    w.writeframes(pcm.tobytes())
assert pcm[0] == pcm[-1] == 0 and np.max(np.abs(pcm.astype(int))) < 32767
print(f'{output.name}: {len(pcm)/RATE:.2f}s, mono PCM16, {output.stat().st_size} bytes, no clipping')
