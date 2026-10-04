"""Say what bootmark.rom did in each recording.

Usage:  py analyze-bootmark.py <recording.wav>...

bootmark.rom plays 999 Hz as soon as the BIOS calls the cartridge and 1998 Hz
after 60 interrupts. A recording with neither means the machine never reached
the cartridge. Each line gives the verdict, the length of the recording and
where each stretch starts, in seconds.
"""
import sys
import wave

import numpy as np

WINDOW = 0.25
SILENT = 30.0       # RMS, out of 32768


def timeline(path):
    with wave.open(path, "rb") as w:
        rate, channels, width = w.getframerate(), w.getnchannels(), w.getsampwidth()
        raw = w.readframes(w.getnframes())
    if width != 2:
        raise SystemExit("%s: %d-byte samples" % (path, width))
    data = np.frombuffer(raw, dtype="<i2").astype(np.float64)
    data = data.reshape(-1, channels).mean(axis=1)
    step = int(rate * WINDOW)
    out = []
    for i in range(0, len(data) - step + 1, step):
        chunk = data[i:i + step]
        chunk = chunk - chunk.mean()
        rms = float(np.sqrt((chunk ** 2).mean()))
        if rms < SILENT:
            out.append(None)
            continue
        spectrum = np.abs(np.fft.rfft(chunk * np.hanning(len(chunk))))
        out.append(int(round(np.argmax(spectrum) * rate / len(chunk))))
    return out, len(data) / rate


def near(freq, target):
    return freq is not None and abs(freq - target) <= 12


for path in sys.argv[1:]:
    marks, seconds = timeline(path)
    segments = []
    for i, m in enumerate(marks):
        name = "silent" if m is None else ("999" if near(m, 999) else "1998" if near(m, 1998) else "%dHz" % m)
        if not segments or segments[-1][0] != name:
            segments.append([name, i * WINDOW])
    low = any(near(m, 999) for m in marks)
    high = any(near(m, 1998) for m in marks)
    verdict = "BOOTS" if high else ("ROM CALLED, NO FRAMES" if low else "NEVER REACHES THE ROM")
    print("%-22s %5.1fs  %s" % (verdict, seconds, "  ".join("%s@%.2f" % (n, t) for n, t in segments)))
