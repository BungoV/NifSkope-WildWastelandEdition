"""Refusal controls that get PAST the CRCs: flip one byte of a .lodo/.lodi and
re-sign every CRC that covers it, so the readers' ROW RULES (sort law, reserved
bits, NOLIB/identity pairing) are seen to fire by name -- a plain flip is caught
by headerCrc32 / indexCrc32 / the chunk crc32 first and proves only the CRC.

  python resign.py <in> <out> <offset> <xor>
CRC arithmetic from the contract page only (zlib polynomial); no C++ shared."""
import struct
import sys
import zlib


def crc(b, seed=0):
    return zlib.crc32(b, seed) & 0xFFFFFFFF


def resign_lodo(b):
    (bases, meshes, clusters, mats, verts, _mx, _mt, _vs, sb) = struct.unpack_from('<IIIIIIHHI', b, 0x50)
    offs = struct.unpack_from('<QQQQQQQ', b, 0x70)
    sizes = (bases * 32, meshes * 56, clusters * 16, mats * 16, clusters * 48, verts * 16, sb)
    c = 0
    for o, n in zip(offs, sizes):
        c = crc(b[o:o + n], c)
    struct.pack_into('<I', b, 0xA8, c)
    struct.pack_into('<I', b, 0x0C, crc(b[0x10:0x100]))


def resign_lodi(b):
    chunks, inst, present = struct.unpack_from('<III', b, 0x54)
    offC, offR, offI, offK = struct.unpack_from('<QQQQ', b, 0x68)
    for ci in range(chunks):
        first, count = struct.unpack_from('<II', b, offC + ci * 32)
        if count == 0:
            continue
        c = crc(b[offI + first * 24:offI + (first + count) * 24])
        c = crc(b[offK + first * 8:offK + (first + count) * 8], c)
        struct.pack_into('<I', b, offC + ci * 32 + 24, c)
    struct.pack_into('<I', b, 0x64, crc(b[offC:offC + chunks * 32] + b[offR:offR + present * 16 * 8]))
    struct.pack_into('<I', b, 0x0C, crc(b[0x10:0x100]))


def main():
    src, dst, off, x = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4], 0)
    b = bytearray(open(src, 'rb').read())
    b[off] ^= x
    if b[:4] == b'LODO':
        resign_lodo(b)
    elif b[:4] == b'LODI':
        resign_lodi(b)
    else:
        sys.exit('not a LODO/LODI file')
    open(dst, 'wb').write(b)
    print('%s: byte %d ^ 0x%02x, CRCs re-signed -> %s' % (src, off, x, dst))


if __name__ == '__main__':
    main()
