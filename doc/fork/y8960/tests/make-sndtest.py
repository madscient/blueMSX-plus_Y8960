"""Assemble sndtest.asm into a 16kB image for the Y8960 SCC cartridge slot.

The code must fit in bank 0 (4000-5FFF): the test shows the SCC in BANK1,
which takes 6000-7FFF away from the ROM while it plays.

Usage:  py make-sndtest.py <pasmo.exe> <output.rom>
"""
import os
import subprocess
import sys

ROM_SIZE = 0x4000
BANK_SIZE = 0x2000

here = os.path.dirname(os.path.abspath(__file__))
pasmo, out = sys.argv[1], sys.argv[2]
tmp = out + ".raw"

subprocess.run([pasmo, "--bin", os.path.join(here, "sndtest.asm"), tmp], check=True)

with open(tmp, "rb") as f:
    code = f.read()
os.remove(tmp)

if len(code) > BANK_SIZE:
    raise SystemExit("code overruns bank 0 (%d bytes)" % len(code))

image = bytearray(ROM_SIZE)
image[:len(code)] = code

with open(out, "wb") as f:
    f.write(image)
print("wrote %s (%d bytes, %d used)" % (out, len(image), len(code)))
