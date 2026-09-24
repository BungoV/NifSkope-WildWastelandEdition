"""AUDIT1 step 3: stamp a real .lodo back to version 3 and re-sign the header
CRC, so that the version is the ONLY thing a reader can object to.

A v3 base row spends crossPx16[0..1] on two screen-size steps where v4 spends
them on the ladder; a reader that accepts the version and then reads the row
reads two different numbers as the same field. The refusal is the invariant.

usage: python mutate_lodo_v3.py <file.lodo>
"""
import struct
import sys
import zlib

p = sys.argv[1]
b = bytearray(open(p, 'rb').read())
was = struct.unpack_from('<I', b, 4)[0]
struct.pack_into('<I', b, 4, 3)
struct.pack_into('<I', b, 0x0C, zlib.crc32(bytes(b[0x10:0x100])) & 0xFFFFFFFF)
open(p, 'wb').write(bytes(b))
print('  version %d -> 3, header CRC re-signed' % was)
