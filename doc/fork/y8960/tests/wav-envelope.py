"""The RMS level of each side of a 16 bit WAV, one row per step.

Shows where a recording sounds and where it is silent, which is enough to
tell the sections of a piece apart when one block of the card is muted.

Usage:  py wav-envelope.py <file.wav> [<step in seconds, default 1>]
"""
import math
import struct
import sys
import wave

path = sys.argv[1]
step = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0

w = wave.open(path, 'rb')
rate, ch, width, n = w.getframerate(), w.getnchannels(), w.getsampwidth(), w.getnframes()
if width != 2:
    raise SystemExit('%s: not 16 bit' % path)
data = w.readframes(n)
w.close()

samples = struct.unpack('<%dh' % (len(data) // 2), data)
frames = len(samples) // ch
per = max(1, int(rate * step))
print('%s: %d Hz, %d ch, %.1f s' % (path, rate, ch, frames / rate))
for start in range(0, frames, per):
    end = min(frames, start + per)
    acc = [0.0] * ch
    for i in range(start, end):
        for c in range(ch):
            v = samples[i * ch + c]
            acc[c] += v * v
    rms = [math.sqrt(a / (end - start)) for a in acc]
    bar = '#' * int(min(60, max(rms) / 200))
    print('%6.1f s  ' % (start / rate) + '  '.join('%7.1f' % r for r in rms) + '  ' + bar)
