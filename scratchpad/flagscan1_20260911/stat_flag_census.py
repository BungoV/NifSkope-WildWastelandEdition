#!/usr/bin/env python
"""Whole-worldspace STAT flag census for lane FLAGSCAN1.

Walks EVERY exterior cell of the Commonwealth (worldspace 0x0000003C) in
Fallout4.esm and, for every STAT base placed at least once, records:

  * EDID, model path, the base record's 32 header flag bits, MNAM presence
    (the Distant LOD mesh list) and MNAM's level-0 string,
  * placement counts: direct REFR placements, and placements reached by
    expanding a placed SCOL one level (ROADS1 measured that most Sanctuary
    road pieces arrive that way),
  * the REFR header-flag histogram over that base's direct placements
    (bit 8 LOD Respects Enable State, bit 15 Visible When Distant,
    bit 16 Is Full LOD, bit 11 Initially Disabled),
  * how many of its placements sit in a cell whose DATA flag bit 12
    ("Distant LOD only") is set.

Derived from scratchpad/roads1_20260911/esm_refs.py (same GRUP walk, same
field reader from tests/spells/lodgen_cover_model.py); widened from a cell
window to the whole worldspace and extended to carry record flags.

  python stat_flag_census.py <esm> <out.json>
"""

import json
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', '..', 'tests', 'spells'))
from lodgen_cover_model import read_fields, record_data, GRUP, REC_HDR  # noqa: E402

WORLD = 0x3C

# Every signature that can own a model and be placed.  We keep flags for all of
# them so the STAT-vs-other question can be answered later, but the tables the
# brief asks for are STAT only.
MODELLED = (b'STAT', b'SCOL', b'MSTT', b'FURN', b'DOOR', b'ACTI', b'CONT',
            b'TREE', b'FLOR', b'LIGH', b'ALCH', b'MISC', b'TERM', b'PWAT')


def walk(path):
    with open(path, 'rb') as f:
        buf = f.read()
    n = len(buf)

    bases = {}      # formid -> record dict
    counts = {}     # formid -> [direct, viaSCOL, inDistantLODOnlyCell]
    refrflags = {}  # formid -> [32 ints]  over DIRECT placements
    stats = {'cells': 0, 'refr': 0, 'refr_world': 0, 'scol_placed': 0,
             'scol_parts': 0, 'lodonly_cells': 0}

    def bump(fid, idx, k=1):
        c = counts.get(fid)
        if c is None:
            c = counts[fid] = [0, 0, 0]
        c[idx] += k

    def handle_base(sig, formid, flags, body):
        rec = {'sig': sig.decode('latin-1'), 'edid': '', 'modl': '',
               'flags': flags, 'mnam': 0, 'mnam0': '', 'parts': []}
        cur = None
        for ft, fd in read_fields(body):
            if ft == b'EDID':
                rec['edid'] = fd.rstrip(b'\0').decode('latin-1', 'replace')
            elif ft == b'MODL' and not rec['modl'] and len(fd) > 1 and fd[0:1] != b'\0':
                rec['modl'] = fd.rstrip(b'\0').decode('latin-1', 'replace')
            elif ft == b'MNAM':
                rec['mnam'] = len(fd)
                z = fd.split(b'\0')[0]
                rec['mnam0'] = z.decode('latin-1', 'replace')
            elif sig == b'SCOL' and ft == b'ONAM' and len(fd) >= 4:
                cur = {'base': struct.unpack_from('<I', fd, 0)[0], 'n': 0}
                rec['parts'].append(cur)
            elif sig == b'SCOL' and ft == b'DATA' and cur is not None:
                cur['n'] += len(fd) // 28
        bases[formid] = rec

    def walkRange(start, end, world, cellcoord, celllod):
        i = start
        while i + REC_HDR <= end:
            t = buf[i:i + 4]
            size = struct.unpack_from('<I', buf, i + 4)[0]
            if t == GRUP:
                label = struct.unpack_from('<I', buf, i + 8)[0]
                gtype = struct.unpack_from('<I', buf, i + 12)[0]
                sub = (label if gtype == 1 else world)
                cellcoord, celllod = walkRange(i + REC_HDR, i + size, sub,
                                               cellcoord, celllod)
                i += size
                continue
            flags = struct.unpack_from('<I', buf, i + 8)[0]
            formid = struct.unpack_from('<I', buf, i + 12)[0]
            if t == b'CELL':
                body = record_data(buf, i + REC_HDR, size, flags)
                cellcoord, celllod = None, 0
                for ft, fd in read_fields(body):
                    if ft == b'XCLC' and len(fd) >= 8:
                        cellcoord = struct.unpack_from('<ii', fd, 0)
                    elif ft == b'DATA' and len(fd) >= 2:
                        d = struct.unpack_from('<H', fd, 0)[0]
                        celllod = 1 if (d & (1 << 12)) else 0
                    elif ft == b'DATA' and len(fd) == 1:
                        celllod = 0
                if cellcoord is not None:
                    stats['cells'] += 1
                    stats['lodonly_cells'] += celllod
            elif t in MODELLED:
                body = record_data(buf, i + REC_HDR, size, flags)
                handle_base(t, formid, flags, body)
            elif t == b'REFR':
                stats['refr'] += 1
                if world == WORLD and cellcoord is not None:
                    stats['refr_world'] += 1
                    body = record_data(buf, i + REC_HDR, size, flags)
                    base = 0
                    for ft, fd in read_fields(body):
                        if ft == b'NAME' and len(fd) >= 4:
                            base = struct.unpack_from('<I', fd, 0)[0]
                            break
                    if base:
                        bump(base, 0)
                        if celllod:
                            bump(base, 2)
                        h = refrflags.get(base)
                        if h is None:
                            h = refrflags[base] = [0] * 32
                        for b in range(32):
                            if flags & (1 << b):
                                h[b] += 1
            i += REC_HDR + size
        return cellcoord, celllod

    hsize = struct.unpack_from('<I', buf, 4)[0]
    walkRange(REC_HDR + hsize, n, 0, None, 0)

    # Expand placed SCOLs one level into their part bases.
    for fid, c in list(counts.items()):
        rec = bases.get(fid)
        if rec is None or rec['sig'] != 'SCOL':
            continue
        stats['scol_placed'] += c[0]
        for p in rec['parts']:
            stats['scol_parts'] += c[0] * p['n']
            bump(p['base'], 1, c[0] * p['n'])
            if c[2]:
                bump(p['base'], 2, c[2] * p['n'])
    return bases, counts, refrflags, stats


def main(argv):
    esm, out = argv[0], argv[1]
    bases, counts, refrflags, stats = walk(esm)
    rows = []
    for fid, c in counts.items():
        rec = bases.get(fid)
        if rec is None:
            continue
        rows.append({
            'formid': '%08X' % fid,
            'sig': rec['sig'],
            'edid': rec['edid'],
            'modl': rec['modl'],
            'flags': rec['flags'],
            'mnam': rec['mnam'],
            'mnam0': rec['mnam0'],
            'direct': c[0], 'viascol': c[1], 'lodonlycell': c[2],
            'refrflags': refrflags.get(fid, [0] * 32),
        })
    with open(out, 'w') as f:
        json.dump({'stats': stats, 'rows': rows}, f)
    print('cells %(cells)d (lod-only %(lodonly_cells)d)  refr %(refr)d '
          '(worldspace %(refr_world)d)  scol placed %(scol_placed)d -> '
          'parts %(scol_parts)d' % stats)
    print('placed bases with a record: %d' % len(rows))


if __name__ == '__main__':
    main(sys.argv[1:])
