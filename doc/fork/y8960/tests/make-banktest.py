"""Assemble banktest.asm and expand it into a 128kB Y8960 cartridge image.

Bank 0 holds the code; every bank carries its own number at offset 0C00h so
the test can tell which bank the BANK1 window is showing.

Usage:  py make-banktest.py <pasmo.exe> <output.rom>
"""
import os
import subprocess
import sys

BANK_SIZE = 0x2000
ROM_BANKS = 16
MARKER_OFFSET = 0x0C00

here = os.path.dirname(os.path.abspath(__file__))
pasmo, out = sys.argv[1], sys.argv[2]
tmp = out + ".bank0"

subprocess.run([pasmo, "--bin", os.path.join(here, "banktest.asm"), tmp], check=True)

with open(tmp, "rb") as f:
    code = f.read()
os.remove(tmp)

if len(code) > MARKER_OFFSET:
    raise SystemExit("code overruns the marker at %04Xh (%d bytes)" % (MARKER_OFFSET, len(code)))

image = bytearray(BANK_SIZE * ROM_BANKS)
image[:len(code)] = code
for bank in range(ROM_BANKS):
    image[bank * BANK_SIZE + MARKER_OFFSET] = bank

with open(out, "wb") as f:
    f.write(image)
print("wrote %s (%d bytes, %d banks)" % (out, len(image), ROM_BANKS))
