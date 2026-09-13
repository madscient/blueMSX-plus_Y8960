"""Judge a WAV recorded while sndtest.rom ran.

Splits the recording into bursts of sound separated by silence, estimates the
pitch of each burst, and compares the list with the steps of sndtest.asm that
must sound. Steps that must stay silent have pitches of their own, so a gate
that leaks shows up as an extra burst.

Usage:  py analyze-sndtest.py <recording.wav>
Exit status 0 when the pitches match, 1 otherwise.
"""
import array
import sys
import wave

EXPECTED = [440, 660, 880, 990]     # B, D, E, F
STEP_NAMES = {330: "A (3EH SHUT)", 440: "B", 554: "C (3FH SHUT)",
              660: "D", 880: "E", 990: "F"}
TOLERANCE = 0.03

FRAME_MS = 20
MIN_BURST_FRAMES = 5                # shorter than 0.1 s is a click, not a step
MAX_GAP_FRAMES = 2


def read_mono(path):
    with wave.open(path, "rb") as w:
        channels = w.getnchannels()
        width = w.getsampwidth()
        rate = w.getframerate()
        raw = w.readframes(w.getnframes())
    if width != 2:
        raise SystemExit("expected 16-bit samples, got %d bytes" % width)
    samples = array.array("h", raw)
    if sys.byteorder != "little":
        samples.byteswap()
    mono = [sum(samples[i:i + channels]) / channels
            for i in range(0, len(samples), channels)]
    return mono, rate


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


def main():
    mono, rate = read_mono(sys.argv[1])
    found, step = bursts(mono, rate)

    heard = []
    for start, end in found:
        length = end - start
        # the middle of the burst, away from the attack and the release
        a = (start + length // 5) * step
        b = (end - length // 5) * step
        f = pitch(mono[a:b], rate)
        heard.append(f)
        name = next((v for k, v in STEP_NAMES.items()
                     if abs(f - k) <= k * TOLERANCE), "?")
        print("%6.2f s  %5.2f s  %7.1f Hz  %s"
              % (start * FRAME_MS / 1000, length * FRAME_MS / 1000, f, name))

    ok = (len(heard) == len(EXPECTED) and
          all(abs(f - e) <= e * TOLERANCE for f, e in zip(heard, EXPECTED)))
    print("PASS" if ok else "FAIL: expected %s Hz" % EXPECTED)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
