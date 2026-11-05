"""Turn a tone sequence of openMSX_Y8960 (doc/fork/y8960/level/*.txt, or the
one adpcm-tones.py writes) into a ROM for the Y8960 SCC cartridge slot, so
blueMSX+ plays the same writes the hardware and openMSX play.

Usage:  py make-tones-rom.py <sequence.txt> <output.rom> [--target Y8960]

The sequence format is the one tones-mga.ps1 and tones.tcl read: one command a
line, values in hex except wait (decimal ms), '#' starts a comment.
  mem ADDR VAL / io PORT VAL / mark LABEL / wait MS / only TARGET <command>

Bank 0 holds a player; the commands follow as a table from bank 1 on, read
through BANK3 (A000h-BFFFh, selected at B000h in compatibility mode), so a
sequence may run to 15 banks. Consecutive writes to one I/O port go out as one
OTIR, which keeps the ADPCM sequence (some 25000 writes to C1h) to a few banks.

Page 2 is handed to the cartridge by writing A8h directly; that holds only
while the cartridge sits in a primary slot that is not expanded, as in the
test machines. Waits are busy loops with interrupts off, so they do not depend
on the VDP interrupt being enabled during init; one loop is about 1ms at
3.58MHz with the M1 wait. The bank in use is kept at C000h.
"""
import argparse
import sys

BANK = 0x2000
MAX_BANKS = 16          # ROM banks #0-#15
INIT = 0x4010
TABLE = 0xA000
BANK_VAR = 0xC000

# inner loop: dec bc / ld a,b / or c / jr nz = 30 T with the M1 wait; 119 of them
# plus the outer loop come to about 3600 T = 1.0ms
INNER = 119

OP_END, OP_MEM, OP_IO, OP_WAIT, OP_NEXT = range(5)


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
            a, v = int(f[1], 16), int(f[2], 16)
            if op == 'mem' and not (0x4000 <= a < 0xA000):
                sys.exit(f'{path}:{n}: mem {a:04X} is outside 4000h-9FFFh (A000h-BFFFh holds the table)')
            cmds.append((op, a, v))
        elif op == 'wait' and len(f) == 2:
            ms = int(f[1])
            if not 0 < ms <= 0xFFFF:
                sys.exit(f'{path}:{n}: wait {ms} out of range')
            cmds.append((op, ms, None))
        elif op == 'mark' and len(f) == 2:
            pass
        else:
            sys.exit(f'{path}:{n}: unknown command: {line.strip()}')
    return cmds


def entries(cmds):
    i = 0
    while i < len(cmds):
        op, a, v = cmds[i]
        if op == 'mem':
            yield bytes([OP_MEM, a & 0xFF, a >> 8, v])
            i += 1
        elif op == 'wait':
            yield bytes([OP_WAIT, a & 0xFF, a >> 8])
            i += 1
        else:
            run = []
            while i < len(cmds) and cmds[i][0] == 'io' and cmds[i][1] == a and len(run) < 255:
                run.append(cmds[i][2])
                i += 1
            yield bytes([OP_IO, a, len(run)]) + bytes(run)


def table(cmds):
    banks, cur = [], bytearray()
    for e in list(entries(cmds)) + [bytes([OP_END])]:
        if len(cur) + len(e) + 1 > BANK:
            cur.append(OP_NEXT)
            banks.append(cur)
            cur = bytearray()
        cur += e
    banks.append(cur)
    return banks


def player():
    """The player at INIT, assembled by hand with two passes for the jumps."""
    code, labels, fixups = bytearray(), {}, []

    def emit(*b):
        code.extend(b)

    def label(name):
        labels[name] = len(code)

    def jr(opcode, target):
        emit(opcode, 0)
        fixups.append((len(code) - 1, target))

    emit(0xF3)                                  # di
    emit(0xDB, 0xA8, 0x47, 0xE6, 0x0C, 0x87, 0x87, 0x4F,
         0x78, 0xE6, 0xCF, 0xB1, 0xD3, 0xA8)    # page 2 takes the slot of page 1
    emit(0x3E, 0x01)                            # ld a,1
    emit(0x32, BANK_VAR & 0xFF, BANK_VAR >> 8)  # ld (bank),a
    emit(0x32, 0x00, 0xB0)                      # ld (B000h),a
    emit(0x21, TABLE & 0xFF, TABLE >> 8)        # ld hl,table
    label('loop')
    emit(0x7E, 0x23)                            # ld a,(hl) / inc hl
    emit(0xB7); jr(0x28, 'end')                 # or a / jr z,end
    emit(0x3D); jr(0x28, 'mem')                 # dec a / jr z,mem
    emit(0x3D); jr(0x28, 'io')                  # dec a / jr z,io
    emit(0x3D); jr(0x28, 'wait')                # dec a / jr z,wait
    emit(0x3A, BANK_VAR & 0xFF, BANK_VAR >> 8)  # next: ld a,(bank)
    emit(0x3C)                                  # inc a
    emit(0x32, BANK_VAR & 0xFF, BANK_VAR >> 8)  # ld (bank),a
    emit(0x32, 0x00, 0xB0)                      # ld (B000h),a
    emit(0x21, TABLE & 0xFF, TABLE >> 8)        # ld hl,table
    jr(0x18, 'loop')
    label('mem')
    emit(0x5E, 0x23, 0x56, 0x23, 0x7E, 0x23)    # ld e,(hl) / ld d,(hl) / ld a,(hl)
    emit(0x12)                                  # ld (de),a
    jr(0x18, 'loop')
    label('io')
    emit(0x4E, 0x23, 0x46, 0x23)                # ld c,(hl) / ld b,(hl)
    emit(0xED, 0xB3)                            # otir
    jr(0x18, 'loop')
    label('wait')
    emit(0x5E, 0x23, 0x56, 0x23)                # ld e,(hl) / ld d,(hl)
    label('outer')
    emit(0x01, INNER & 0xFF, INNER >> 8)        # ld bc,INNER
    label('inner')
    emit(0x0B, 0x78, 0xB1); jr(0x20, 'inner')   # dec bc / ld a,b / or c / jr nz
    emit(0x1B, 0x7A, 0xB3); jr(0x20, 'outer')   # dec de / ld a,d / or e / jr nz
    jr(0x18, 'loop')
    label('end')
    jr(0x18, 'end')

    for at, name in fixups:
        rel = labels[name] - (at + 1)
        assert -128 <= rel <= 127
        code[at] = rel & 0xFF
    return code


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('seq')
    ap.add_argument('out')
    ap.add_argument('--target', default='Y8960')
    a = ap.parse_args()

    banks = table(parse(a.seq, a.target))
    if 1 + len(banks) > MAX_BANKS:
        sys.exit(f'the table needs {len(banks)} banks; at most {MAX_BANKS - 1} fit')
    image = bytearray(BANK * max(2, 1 + len(banks)))
    image[0:2] = b'AB'
    image[2:4] = INIT.to_bytes(2, 'little')
    code = player()
    image[INIT - 0x4000:INIT - 0x4000 + len(code)] = code
    for i, b in enumerate(banks, 1):
        image[i * BANK:i * BANK + len(b)] = b
    with open(a.out, 'wb') as f:
        f.write(image)
    print(f'wrote {a.out} ({len(image) // 1024}kB, table in {len(banks)} banks)')


if __name__ == '__main__':
    main()
