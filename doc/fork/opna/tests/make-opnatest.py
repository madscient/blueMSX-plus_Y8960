"""Assemble opnatest.asm into a 16 KB plain ROM.

Usage:  py make-opnatest.py <pasmo.exe> <output.rom>
"""
import os
import subprocess
import sys

ROM_SIZE = 0x4000

here = os.path.dirname(os.path.abspath(__file__))
pasmo, out = sys.argv[1], sys.argv[2]
tmp = out + ".raw"

subprocess.run([pasmo, "--bin", os.path.join(here, "opnatest.asm"), tmp], check=True)

with open(tmp, "rb") as f:
    code = f.read()
os.remove(tmp)

if len(code) > ROM_SIZE:
    raise SystemExit("code overruns 16 KB (%d bytes)" % len(code))

image = bytearray(ROM_SIZE)
image[:len(code)] = code

with open(out, "wb") as f:
    f.write(image)
print("wrote %s (%d bytes, %d used)" % (out, len(image), len(code)))
