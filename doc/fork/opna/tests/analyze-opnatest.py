"""Judge a WAV recorded while opnatest.rom played on a Makoto.

Splits the recording at silences of 300 ms or more and expects the six
sections of opnatest.asm, in order. For each it prints the length and the
pitch (rising crossings of the mean, with a hysteresis band, so the SSG's
square wave that only swings above zero counts like a sine).

Sections 1, 2 and 4 last 60 VDP frames, so their length depends on the
machine and is shown but not judged. Sections 5 and 6 are timed by the
OPNA's timer A (500 x 1.8 ms), so their length is judged.

Usage:  py analyze-opnatest.py <recording.wav>
Exit code: the number of failed checks.
"""
import math
import struct
import sys
import wave

WINDOW_MS = 10
SILENCE_MS = 300
PITCH_TOL = 0.01
LENGTH_TOL = 0.04

EXPECTED = [
    ("fm ch1 (lower ports)",  261.94, None),
    ("ssg tone a",            440.14, None),
    ("rhythm",                None,   None),
    ("fm ch4 (upper ports)",  523.88, None),
    ("fm, timer a polled",    327.16, 0.9),
    ("ssg, timer a irq",      586.85, 0.9),
]


def read(path):
    w = wave.open(path, "rb")
    ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    if width != 2:
        raise SystemExit("expected 16-bit samples")
    raw = w.readframes(n)
    samples = struct.unpack("<%dh" % (n * ch), raw)
    return [samples[i] for i in range(0, len(samples), ch)], rate


def segments(left, rate):
    win = rate * WINDOW_MS // 1000
    levels = []
    for i in range(0, len(left) - win, win):
        chunk = left[i:i + win]
        mean = sum(chunk) / win
        levels.append(math.sqrt(sum((s - mean) ** 2 for s in chunk) / win))
    peak = max(levels) if levels else 0
    threshold = max(peak * 0.02, 30)
    loud = [lv > threshold for lv in levels]
    gap = SILENCE_MS // WINDOW_MS
    found, start, quiet = [], None, 0
    for i, on in enumerate(loud):
        if on:
            if start is None:
                start = i
            quiet = 0
            end = i
        elif start is not None:
            quiet += 1
            if quiet >= gap:
                found.append((start * win, (end + 1) * win))
                start = None
    if start is not None:
        found.append((start * win, (end + 1) * win))
    return found


def pitch(s, rate):
    mean = sum(s) / len(s)
    peak = max(abs(v - mean) for v in s)
    band = peak * 0.3
    state, crossings, first, last = 0, 0, 0, 0
    for i, v in enumerate(s):
        v -= mean
        if state <= 0 and v > band:
            if state < 0:
                if crossings == 0:
                    first = i
                last = i
                crossings += 1
            state = 1
        elif state >= 0 and v < -band:
            state = -1
    if crossings < 2:
        return 0.0
    return (crossings - 1) * rate / (last - first)


def main():
    left, rate = read(sys.argv[1])
    segs = segments(left, rate)
    failed = 0
    print("%d sections found (expected %d)" % (len(segs), len(EXPECTED)))
    if len(segs) != len(EXPECTED):
        failed += 1
    for k, (a, b) in enumerate(segs):
        length = (b - a) / rate
        name, f_exp, len_exp = EXPECTED[k] if k < len(EXPECTED) else ("(unexpected)", None, None)
        # measure pitch away from the attack and release
        margin = int(0.05 * rate)
        f = pitch(left[a + margin:b - margin], rate) if b - a > 4 * margin else 0.0
        verdicts = []
        if f_exp is not None:
            ok = abs(f - f_exp) / f_exp < PITCH_TOL
            verdicts.append("%s pitch %.2f Hz (expected %.2f)" % ("OK" if ok else "NG", f, f_exp))
            failed += 0 if ok else 1
        if len_exp is not None:
            ok = abs(length - len_exp) / len_exp < LENGTH_TOL
            verdicts.append("%s length %.3f s (expected %.3f)" % ("OK" if ok else "NG", length, len_exp))
            failed += 0 if ok else 1
        else:
            verdicts.append("length %.3f s" % length)
        print("%d  %-22s at %6.2f s  %s" % (k + 1, name, a / rate, ";  ".join(verdicts)))
    print("%d failed" % failed)
    return failed


if __name__ == "__main__":
    sys.exit(main())
