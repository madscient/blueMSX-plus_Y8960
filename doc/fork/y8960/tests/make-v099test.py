"""Assemble v099test.asm into a plain 16kB cartridge image.

No mapper and no bank markers: the ROM sits at 4000h. Insert it as the whole
Y8960 Cartridge (/romtype1 Y8960), which carries the System Controller and
the enabler the test writes to.

Usage:  py make-v099test.py <pasmo.exe> <output.rom>
"""
import os
import subprocess
import sys

ROM_SIZE = 0x4000
# The memory mapped window covers 7FF0h-7FFFh, over the end of the image.
CODE_LIMIT = 0x3FF0

here = os.path.dirname(os.path.abspath(__file__))
pasmo, out = sys.argv[1], sys.argv[2]
tmp = out + ".raw"

subprocess.run([pasmo, "--bin", os.path.join(here, "v099test.asm"), tmp], check=True)

with open(tmp, "rb") as f:
    code = f.read()
os.remove(tmp)

if len(code) > CODE_LIMIT:
    raise SystemExit("code runs into the window at 7FF0h (%d bytes)" % len(code))

image = bytearray(ROM_SIZE)
image[:len(code)] = code

with open(out, "wb") as f:
    f.write(image)
print("wrote %s (%d bytes, %d used)" % (out, len(image), len(code)))
