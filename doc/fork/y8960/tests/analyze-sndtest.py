"""Judge a WAV recorded while sndtest.rom ran.

Splits the recording into bursts of sound separated by silence, estimates the
pitch, the level and the balance of each burst, and compares the list with
the steps of sndtest.asm that must sound: the same pitch at full, half, full,
half and quarter level in the centre, the two muted steps leaving no burst,
then six steps that move the SCC's pan and one write that must not move it,
then the OPM at its own pitch, at full and half level and on the left side
alone. Anything else that sounds shows up as an extra burst.

Usage:  py analyze-sndtest.py <recording.wav>
Exit status 0 when the pitches match, 1 otherwise.
"""
import array
import sys
import wave

# A, B, D, E, F, H, I, J, K, L, M, N, then O, P, Q; C and G are muted
EXPECTED = [990] * 12 + [440] * 3
LEVELS = [1.0, 0.5, 1.0, 0.5, 0.25]    # A-F, relative to the first burst
OPM_LEVELS = {13: (12, 0.5)}           # burst: (against burst, level)
LEVEL_TOLERANCE = 0.1

# Each burst's sides. "centre": the right as loud as the left. "left" and
# "right": the other side silent. A number: the right over the left, for pan
# 40h cos/sin of (40h - 1) / 254 of a right angle. Step H and I also give the
# loud side against the first burst's: a channel at the centre is 1/sqrt(2)
# on each side, so moved to one end it is sqrt(2) as loud there. The OPM's
# RL bits switch a side off without moving any level to the other, so step Q
# is as loud on its side as step O.
SIDES = (["centre"] * 5 + ["left", "right", 0.411, "right", "right", "left", "left"]
         + ["centre", "centre", "left"])
SIDE_TOLERANCE = 0.05
END_GAIN = {5: (0, 0, 2 ** 0.5), 6: (1, 0, 2 ** 0.5),
            14: (0, 12, 1.0)}                   # burst: (side, against, gain)
STEP_NAMES = {990: "SCC", 440: "OPM"}
TOLERANCE = 0.03

FRAME_MS = 20
MIN_BURST_FRAMES = 5                # shorter than 0.1 s is a click, not a step
MAX_GAP_FRAMES = 2


def read_wav(path):
    """The mono mix and the two sides."""
    with wave.open(path, "rb") as w:
        channels = w.getnchannels()
        width = w.getsampwidth()
        rate = w.getframerate()
        raw = w.readframes(w.getnframes())
    if width != 2:
        raise SystemExit("expected 16-bit samples, got %d bytes" % width)
    if channels != 2:
        raise SystemExit("expected stereo, got %d channels" % channels)
    samples = array.array("h", raw)
    if sys.byteorder != "little":
        samples.byteswap()
    left = samples[0::2]
    right = samples[1::2]
    mono = [(a + b) / 2 for a, b in zip(left, right)]
    return mono, left, right, rate


def rms(segment):
    mean = sum(segment) / len(segment)
    return (sum((v - mean) ** 2 for v in segment) / len(segment)) ** 0.5


def bursts(mono, rate):
    step = rate * FRAME_MS // 1000
    levels = []
    for i in range(0, len(mono) - step, step):
        frame = mono[i:i + step]
        mean = sum(frame) / step
        levels.append((sum((s - mean) ** 2 for s in frame) / step) ** 0.5)
    if not levels or max(levels) == 0:
        return [], step
    threshold = max(max(levels) * 0.1, 50)

    found = []
    start = None
    quiet = 0
    for n, level in enumerate(levels):
        if level >= threshold:
            if start is None:
                start = n
            quiet = 0
        elif start is not None:
            quiet += 1
            if quiet > MAX_GAP_FRAMES:
                end = n - quiet + 1
                if end - start >= MIN_BURST_FRAMES:
                    found.append((start, end))
                start = None
                quiet = 0
    if start is not None and len(levels) - start >= MIN_BURST_FRAMES:
        found.append((start, len(levels)))
    return found, step


def pitch(segment, rate):
    """Zero crossings of the mean-removed signal, with a little hysteresis so
    the interpolation ripple on a square wave is not counted."""
    mean = sum(segment) / len(segment)
    peak = max(abs(s - mean) for s in segment)
    band = peak * 0.2
    state = 0
    rises = 0
    for s in segment:
        v = s - mean
        if state <= 0 and v > band:
            if state < 0:
                rises += 1
            state = 1
        elif state >= 0 and v < -band:
            state = -1
    return rises * rate / len(segment)


def side_ok(n, left, right, sides):
    want = SIDES[n]
    left = max(left, 1e-9)     # a silent side is a ratio, not a crash
    right = max(right, 1e-9)
    if want == "centre":
        ok = abs(right / left - 1) <= SIDE_TOLERANCE
    elif want == "left":
        ok = right / left <= SIDE_TOLERANCE
    elif want == "right":
        ok = left / right <= SIDE_TOLERANCE
    else:
        ok = abs(right / left - want) <= SIDE_TOLERANCE
    if ok and n in END_GAIN:
        side, against, gain = END_GAIN[n]
        loud = (left, right)[side] / sides[against][side]
        print("  burst %d: %.2f of burst %d on its side, want %.2f"
              % (n + 1, loud, against + 1, gain))
        ok = abs(loud - gain) <= LEVEL_TOLERANCE
    return ok


def main():
    mono, left, right, rate = read_wav(sys.argv[1])
    found, step = bursts(mono, rate)

    heard = []
    levels = []
    sides = []
    for start, end in found:
        length = end - start
        # the middle of the burst, away from the attack and the release
        a = (start + length // 5) * step
        b = (end - length // 5) * step
        f = pitch(mono[a:b], rate)
        level = rms(mono[a:b])
        lr = (rms(left[a:b]), rms(right[a:b]))
        heard.append(f)
        levels.append(level)
        sides.append(lr)
        name = next((v for k, v in STEP_NAMES.items()
                     if abs(f - k) <= k * TOLERANCE), "?")
        print("%6.2f s  %5.2f s  %7.1f Hz  rms %7.1f  L %7.1f  R %7.1f  %s"
              % (start * FRAME_MS / 1000, length * FRAME_MS / 1000, f, level,
                 lr[0], lr[1], name))

    ok = (len(heard) == len(EXPECTED) and
          all(abs(f - e) <= e * TOLERANCE for f, e in zip(heard, EXPECTED)))
    if ok:
        ratios = [lv / levels[0] for lv in levels[:len(LEVELS)]]
        print("levels relative to the first: %s" % ", ".join("%.2f" % r for r in ratios))
        ok = all(abs(r - e) <= LEVEL_TOLERANCE for r, e in zip(ratios, LEVELS))
        for n, (against, want) in OPM_LEVELS.items():
            r = levels[n] / levels[against]
            print("burst %d against burst %d: %.2f, want %.2f" % (n + 1, against + 1, r, want))
            ok = ok and abs(r - want) <= LEVEL_TOLERANCE
    if ok:
        print("right over left: %s" % ", ".join(
            "%.3f" % (r / l) if l >= 1 else "R only" for l, r in sides))
        bad = [n + 1 for n, (l, r) in enumerate(sides)
               if not side_ok(n, l, r, sides)]
        if bad:
            print("wrong sides in burst %s, want %s" % (bad, SIDES))
            ok = False
    print("PASS" if ok else "FAIL: expected %s Hz at levels %s" % (EXPECTED, LEVELS))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
