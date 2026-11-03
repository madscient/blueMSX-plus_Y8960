"""List, or copy out, the files in the root directory of a FAT12 disk image.

For taking MSXDOS.SYS and COMMAND.COM off an MSX-DOS 1 boot disk, so that a
folder with them in it boots (run-dos.ps1). The image is only read.

Usage:  py dsk-extract.py <image.dsk> [<outdir>]
"""
import os
import struct
import sys

img = open(sys.argv[1], 'rb').read()
out = sys.argv[2] if len(sys.argv) > 2 else None

bps = struct.unpack_from('<H', img, 11)[0]
spc = img[13]
reserved = struct.unpack_from('<H', img, 14)[0]
nfats = img[16]
rootents = struct.unpack_from('<H', img, 17)[0]
spf = struct.unpack_from('<H', img, 22)[0]

fat_off = reserved * bps
root_off = (reserved + nfats * spf) * bps
data_off = root_off + rootents * 32
csize = spc * bps


def fat_next(cluster):
    word = struct.unpack_from('<H', img, fat_off + (cluster * 3) // 2)[0]
    return (word >> 4) if (cluster & 1) else (word & 0xFFF)


for i in range(rootents):
    e = img[root_off + i * 32: root_off + (i + 1) * 32]
    if e[0] == 0:
        break
    if e[0] == 0xE5 or e[11] & 0x08:        # deleted, or the volume label
        continue
    name = e[0:8].decode('ascii', 'replace').rstrip()
    ext = e[8:11].decode('ascii', 'replace').rstrip()
    full = name + ('.' + ext if ext else '')
    start = struct.unpack_from('<H', e, 26)[0]
    size = struct.unpack_from('<I', e, 28)[0]
    print('%-12s %7d' % (full, size))
    if out and not (e[11] & 0x10):
        data = bytearray()
        c = start
        while 2 <= c < 0xFF8 and len(data) < size:
            o = data_off + (c - 2) * csize
            data += img[o:o + csize]
            c = fat_next(c)
        with open(os.path.join(out, full), 'wb') as f:
            f.write(bytes(data[:size]))
