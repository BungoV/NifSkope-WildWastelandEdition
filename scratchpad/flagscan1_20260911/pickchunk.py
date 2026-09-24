#!/usr/bin/env python
"""Pick section 3's second tile: a dim-4 chunk where the bit-15 NON-road bases
are heavily placed and roads are not, and whose vanilla sheet exists on disk.

Walks Fallout4.esm once more, this time keeping the cell coordinate with every
placement, and bins into dim-4 chunks (chunk origin = floor(cell/4)*4).
SCOL parts are expanded one level and attributed to the placing REFR's cell.

  python pickchunk.py <esm> <census.json> <vanillaDir>
"""

import collections
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from lodgen_cover_model import read_fields, record_data, GRUP, REC_HDR  # noqa: E402
from flagtable import is_road  # noqa: E402

WORLD = 0x3C
DIM = 4


def main(argv):
    esm, censusPath, vanDir = argv[0], argv[1], argv[2]
    cen = json.load(open(censusPath))
    byfid = dict((r['formid'], r) for r in cen['rows'])
    flagged = set()   # non-road STAT with bit 15
    roads = set()
    for r in cen['rows']:
        if r['sig'] != 'STAT':
            continue
        if is_road(r):
            roads.add(int(r['formid'], 16))
        elif r['flags'] & (1 << 15):
            flagged.add(int(r['formid'], 16))

    with open(esm, 'rb') as f:
        buf = f.read()
    n = len(buf)
    scolparts = {}
    for r in cen['rows']:
        pass

    # SCOL part membership: re-read SCOL records for their ONAM lists
    chunkF = collections.Counter()
    chunkR = collections.Counter()

    scol = {}

    def walkRange(start, end, world, cellcoord):
        i = start
        while i + REC_HDR <= end:
            t = buf[i:i + 4]
            size = struct.unpack_from('<I', buf, i + 4)[0]
            if t == GRUP:
                label = struct.unpack_from('<I', buf, i + 8)[0]
                gtype = struct.unpack_from('<I', buf, i + 12)[0]
                sub = (label if gtype == 1 else world)
                cellcoord = walkRange(i + REC_HDR, i + size, sub, cellcoord)
                i += size
                continue
            flags = struct.unpack_from('<I', buf, i + 8)[0]
            formid = struct.unpack_from('<I', buf, i + 12)[0]
            if t == b'CELL':
                body = record_data(buf, i + REC_HDR, size, flags)
                cellcoord = None
                for ft, fd in read_fields(body):
                    if ft == b'XCLC' and len(fd) >= 8:
                        cellcoord = struct.unpack_from('<ii', fd, 0)
            elif t == b'SCOL':
                body = record_data(buf, i + REC_HDR, size, flags)
                parts, cur = [], None
                for ft, fd in read_fields(body):
                    if ft == b'ONAM' and len(fd) >= 4:
                        cur = [struct.unpack_from('<I', fd, 0)[0], 0]
                        parts.append(cur)
                    elif ft == b'DATA' and cur is not None:
                        cur[1] += len(fd) // 28
                scol[formid] = parts
            elif t == b'REFR' and world == WORLD and cellcoord is not None:
                body = record_data(buf, i + REC_HDR, size, flags)
                base = 0
                for ft, fd in read_fields(body):
                    if ft == b'NAME' and len(fd) >= 4:
                        base = struct.unpack_from('<I', fd, 0)[0]
                        break
                if base:
                    key = ((cellcoord[0] // DIM) * DIM, (cellcoord[1] // DIM) * DIM)
                    if base in flagged:
                        chunkF[key] += 1
                    elif base in roads:
                        chunkR[key] += 1
                    elif base in scol:
                        for pb, cnt in scol[base]:
                            if pb in flagged:
                                chunkF[key] += cnt
                            elif pb in roads:
                                chunkR[key] += cnt
            i += REC_HDR + size
        return cellcoord

    hsize = struct.unpack_from('<I', buf, 4)[0]
    walkRange(REC_HDR + hsize, n, 0, None)

    rows = []
    for k, v in chunkF.items():
        p = os.path.join(vanDir, 'Commonwealth.4.%d.%d.DDS' % (k[0], k[1]))
        if not os.path.isfile(p):
            continue
        rows.append((v, chunkR.get(k, 0), k))
    rows.sort(reverse=True)
    print('%-14s %10s %10s' % ('chunk', 'flagged', 'road'))
    for v, r, k in rows[:25]:
        print('%-14s %10d %10d' % ('%d,%d' % k, v, r))
    print()
    print('best with road == 0 or tiny:')
    for v, r, k in rows[:200]:
        if r <= 2:
            print('   %-12s flagged %6d  road %3d' % ('%d,%d' % k, v, r))


if __name__ == '__main__':
    main(sys.argv[1:])
