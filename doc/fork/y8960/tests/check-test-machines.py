"""Compare the Y8960 test machine configurations here with the copies a build
output runs from, and say which differ.

Usage:  py check-test-machines.py <Machines directory of the build output>

The copies go stale whenever a RomType number or a test changes here, and a
stale one still loads: it plugs whatever device now holds the old number.
Run this before the tests; copy the ones it names from here.

Exit status is non-zero if any copy differs or is missing.

A ROM sitting beside a copy (the banktest machine loads its banktest.rom from
there) is built, not kept here, so it cannot be compared. It is listed so
that it gets rebuilt rather than trusted.
"""
import filecmp
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))
out = sys.argv[1]

stale = 0
for name in sorted(os.listdir(here)):
    mine = os.path.join(here, name, "config.ini")
    if not os.path.isfile(mine):
        continue
    theirs = os.path.join(out, name, "config.ini")
    if not os.path.isfile(theirs):
        state = "missing"
    elif filecmp.cmp(mine, theirs, shallow=False):
        state = "same"
    else:
        state = "DIFFERENT"
    if state != "same":
        stale += 1
    print("%-45s %s" % (name, state))
    if os.path.isdir(os.path.join(out, name)):
        for f in sorted(os.listdir(os.path.join(out, name))):
            if f.lower().endswith(".rom"):
                print("    %s beside it is not compared: rebuild it" % f)

sys.exit(1 if stale else 0)
