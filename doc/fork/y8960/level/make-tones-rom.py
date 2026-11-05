"""Turn a tone sequence of openMSX_Y8960 (doc/fork/y8960/level/*.txt) into a
16kB ROM for the Y8960 SCC cartridge slot, so blueMSX+ plays the same writes
the hardware and openMSX play.

Usage:  py make-tones-rom.py <sequence.txt> <output.rom> [--target Y8960]

The sequence format is the one tones-mga.ps1 and tones.tcl read: one command a
line, values in hex except wait (decimal ms), '#' starts a comment.
  mem ADDR VAL / io PORT VAL / mark LABEL / wait MS / only TARGET <command>

The code runs from the cartridge's init entry and never returns. Page 2 is
handed to the cartridge by writing A8h directly when the sequence writes to
8000h-BFFFh (the SCC); that holds only while the cartridge sits in a primary
slot that is not expanded, as in the test machines. Waits are busy loops with
interrupts off, so they do not depend on the VDP interrupt being enabled during
init; one loop is about 1ms at 3.58MHz with the M1 wait.
"""
import argparse
import sys

ROM_SIZE = 0x4000
CODE_LIMIT = 0x2000   # bank 0: 6000h-7FFFh may be switched away by the sequence
BASE = 0x4000
INIT = 0x4010

# inner loop: dec bc / ld a,b / or c / jr nz = 30 T with the M1 wait; 119 of them
# plus the outer loop come to about 3600 T = 1.0ms
INNER = 119


def parse(path, target):
    cmds = []
    for n, line in enumerate(open(path, encoding='utf-8'), 1):
        f = line.split('#', 1)[0].split()
        if not f:
            continue
        if f[0] == 'only':
            if len(f) < 3:
                sys.exit(f'{path}:{n}: only without a command')
            if f[1] != target:
                continue
            f = f[2:]
        op = f[0]
        if op in ('mem', 'io') and len(f) == 3:
            cmds.append((op, int(f[1], 16), int(f[2], 16)))
        elif op == 'wait' and len(f) == 2:
            cmds.append((op, int(f[1]), None))
        elif op == 'mark' and len(f) == 2:
            pass
        else:
            sys.exit(f'{path}:{n}: unknown command: {line.strip()}')
    return cmds


def assemble(cmds):
    code = bytearray()
    code += b'\xf3'                          # di
    if any(op == 'mem' and 0x8000 <= a < 0xC000 for op, a, _ in cmds):
        # page 2 takes the slot of page 1
        code += bytes([0xDB, 0xA8,           # in a,(A8h)
                       0x47,                 # ld b,a
                       0xE6, 0x0C,           # and 0Ch
                       0x87, 0x87,           # add a,a / add a,a
                       0x4F,                 # ld c,a
                       0x78,                 # ld a,b
                       0xE6, 0xCF,           # and CFh
                       0xB1,                 # or c
                       0xD3, 0xA8])          # out (A8h),a
    for op, a, v in cmds:
        if op == 'mem':
            if not 0x4000 <= a < 0xC000:
                sys.exit(f'mem {a:04X}: outside pages 1-2')
            code += bytes([0x3E, v, 0x32, a & 0xFF, a >> 8])
        elif op == 'io':
            code += bytes([0x3E, v, 0xD3, a])
        else:
            if not 0 < a <= 0xFFFF:
                sys.exit(f'wait {a}: out of range')
            code += bytes([0x11, a & 0xFF, a >> 8])           # ld de,ms
            outer = len(code)
            code += bytes([0x01, INNER & 0xFF, INNER >> 8])   # outer: ld bc,INNER
            code += bytes([0x0B, 0x78, 0xB1, 0x20, 0xFB])     # inner: dec bc/ld a,b/or c/jr nz,inner
            code += bytes([0x1B, 0x7A, 0xB3])                 # dec de/ld a,d/or e
            code += bytes([0x20, (outer - (len(code) + 2)) & 0xFF])   # jr nz,outer
    code += b'\x18\xfe'                      # jr $
    return code


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('seq')
    ap.add_argument('out')
    ap.add_argument('--target', default='Y8960')
    a = ap.parse_args()

    code = assemble(parse(a.seq, a.target))
    if (INIT - BASE) + len(code) > CODE_LIMIT:
        sys.exit(f'code overruns bank 0 ({len(code)} bytes)')
    image = bytearray(ROM_SIZE)
    image[0:2] = b'AB'
    image[2:4] = INIT.to_bytes(2, 'little')
    image[INIT - BASE:INIT - BASE + len(code)] = code
    with open(a.out, 'wb') as f:
        f.write(image)
    print(f'wrote {a.out} ({len(code)} bytes of code)')


if __name__ == '__main__':
    main()
