#!/usr/bin/env python
"""GENSMALL1 refuter for the decoder's new cell rule (item 1).

Moves ONE instance's stored position inside its chunk box while the cell RANGE
table keeps saying what it said, then RE-SIGNS the file completely (every chunk
crc32, indexCrc32, headerCrc32) with `lodgen_native_mutate.resign_lodi`, so the
only thing left wrong is the thing under test.  The decoder must refuse by name.

  cellrefute.py <in.lodi> <out.lodi> <instance> <axis x|y> <delta, in cells>
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)) + '/../../tests/spells')
from lodgen_native_mutate import resign_lodi  # noqa: E402

CHUNK = 16384.0
CELL = 4096.0
src, dst, inst, axis, d_cells = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4], float(sys.argv[5])
b = bytearray(open(src, 'rb').read())
offChunks, offCellRanges, offInstances = struct.unpack_from('<QQQ', b, 0x68)
chunkCount = struct.unpack_from('<I', b, 0x54)[0]
ci_hit = None
for ci in range(chunkCount):
    first, cnt = struct.unpack_from('<II', b, offChunks + ci * 32)
    if cnt and first <= inst < first + cnt:
        ci_hit = ci
        break
assert ci_hit is not None, 'instance %d in no chunk' % inst
o = offInstances + inst * 24 + (0 if axis == 'x' else 2)
old = struct.unpack_from('<H', b, o)[0]
new = int(round(old + d_cells * CELL / CHUNK * 65535.0))
assert 0 <= new <= 65535, 'moved off the chunk box'
struct.pack_into('<H', b, o, new)
resign_lodi(b)
open(dst, 'wb').write(bytes(b))
print('mutated instance %d in chunk %d, axis %s: p %d -> %d (%.6f -> %.6f cells from the chunk origin); '
      'file re-signed whole'
      % (inst, ci_hit, axis, old, new, old / 65535.0 * 4.0, new / 65535.0 * 4.0))
