"""Assemble ssgstest.asm into a plain 16kB cartridge image.

No mapper and no bank markers: the ROM sits at 4000h and the machine's own
SSGS is what the test drives.

Usage:  py make-ssgstest.py <pasmo.exe> <output.rom>
"""
import os
import subprocess
import sys

ROM_SIZE = 0x4000

here = os.path.dirname(os.path.abspath(__file__))
pasmo, out = sys.argv[1], sys.argv[2]
tmp = out + ".raw"

subprocess.run([pasmo, "--bin", os.path.join(here, "ssgstest.asm"), tmp], check=True)

with open(tmp, "rb") as f:
    code = f.read()
os.remove(tmp)

if len(code) > ROM_SIZE:
    raise SystemExit("code overruns %04Xh (%d bytes)" % (ROM_SIZE, len(code)))

image = bytearray(ROM_SIZE)
image[:len(code)] = code

with open(out, "wb") as f:
    f.write(image)
print("wrote %s (%d bytes, %d used)" % (out, len(image), len(code)))
