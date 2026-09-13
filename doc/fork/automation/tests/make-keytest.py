"""Assemble keytest.asm into a plain 16kB cartridge image.

Usage:  py make-keytest.py <pasmo.exe> <output.rom>
"""
import os
import subprocess
import sys

ROM_SIZE = 0x4000

here = os.path.dirname(os.path.abspath(__file__))
pasmo, out = sys.argv[1], sys.argv[2]
tmp = out + ".raw"

subprocess.run([pasmo, "--bin", os.path.join(here, "keytest.asm"), tmp], check=True)

with open(tmp, "rb") as f:
    code = f.read()
os.remove(tmp)

if len(code) > ROM_SIZE:
    raise SystemExit("code overruns %04Xh (%d bytes)" % (ROM_SIZE, len(code)))

with open(out, "wb") as f:
    f.write(code + bytes(ROM_SIZE - len(code)))
print("wrote %s (%d bytes, %d used)" % (out, ROM_SIZE, len(code)))
