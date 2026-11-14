"""Assemble voicetest.asm into a 16kB image for the Y8960 cartridge.

Usage:  py make-voicetest.py <pasmo.exe> <output.rom>
"""
import os
import subprocess
import sys

ROM_SIZE = 0x4000

here = os.path.dirname(os.path.abspath(__file__))
pasmo, out = sys.argv[1], sys.argv[2]
tmp = out + ".raw"

subprocess.run([pasmo, "--bin", os.path.join(here, "voicetest.asm"), tmp], check=True)

with open(tmp, "rb") as f:
    code = f.read()
os.remove(tmp)

image = bytearray(ROM_SIZE)
image[:len(code)] = code

with open(out, "wb") as f:
    f.write(image)
print("wrote %s (%d bytes, %d used)" % (out, len(image), len(code)))
