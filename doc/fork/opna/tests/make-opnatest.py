"""Assemble opnatest.asm, or another source beside it, into a 16 KB plain ROM.

Usage:  py make-opnatest.py <pasmo.exe> <output.rom> [source.asm]
"""
import os
import subprocess
import sys

ROM_SIZE = 0x4000

here = os.path.dirname(os.path.abspath(__file__))
pasmo, out = sys.argv[1], sys.argv[2]
source = sys.argv[3] if len(sys.argv) > 3 else "opnatest.asm"
tmp = out + ".raw"

subprocess.run([pasmo, "--bin", os.path.join(here, source), tmp], check=True)

with open(tmp, "rb") as f:
    code = f.read()
os.remove(tmp)

if len(code) > ROM_SIZE:
    raise SystemExit("code overruns 16 KB (%d bytes)" % len(code))

image = bytearray(ROM_SIZE)
image[:len(code)] = code

with open(out, "wb") as f:
    f.write(image)
# The source is named in the output, because the third argument picks it and
# a wrong one still produces a ROM under the name given.
print("wrote %s from %s (%d bytes, %d used)" % (out, source, len(image), len(code)))
