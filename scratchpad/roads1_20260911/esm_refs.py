#!/usr/bin/env python
"""An INDEPENDENT walk of Fallout4.esm's PLACED REFERENCES in a cell window,
with each reference's base record and that base's model path.

Written for lane ROADS1's section 1 (measure vanilla FIRST).  It shares nothing
with src/lodgen.cpp: the GRUP walk, the zlib decompression and the field reader
come from tests/spells/lodgen_cover_model.py (which parses the plugin itself),
and the base-record handling below is typed from the record layout, not from the
generator's EsmData.

  python esm_refs.py <esm> <cx0> <cy0> <cx1> <cy1> <out.json>

Emits, for every REFR whose owning exterior CELL lies in the inclusive cell
window of worldspace 0x3C:  formid, base formid, position, rotation, scale,
base signature, base EDID, base model path.

SCOL (static collections) are expanded one level: the collection's ONAM parts
are emitted as pseudo-references with their own model paths and transforms,
because 471 of Sanctuary chunk (-20,24)'s 678 objects are SCOL parts and a walk
that stops at the collection sees none of the road pieces inside one.
"""

import json
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', '..', 'tests', 'spells'))
from lodgen_cover_model import read_fields, record_data, GRUP, REC_HDR  # noqa: E402

WORLD = 0x3C
CELL_UNITS = 4096.0

# base signatures worth carrying a model for
MODELLED = (b'STAT', b'SCOL', b'MSTT', b'FURN', b'DOOR', b'ACTI', b'CONT',
            b'TREE', b'FLOR', b'LIGH', b'ALCH', b'MISC', b'TERM', b'PWAT')


def walk(path, cx0, cy0, cx1, cy1):
    with open(path, 'rb') as f:
        buf = f.read()
    n = len(buf)
    refs = []
    bases = {}           # formid -> {'sig','edid','modl','parts'}

    def handle_base(sig, formid, body):
        rec = {'sig': sig.decode('latin-1'), 'edid': '', 'modl': '', 'parts': []}
        cur = None
        for ft, fd in read_fields(body):
            if ft == b'EDID':
                rec['edid'] = fd.rstrip(b'\0').decode('latin-1')
            elif ft == b'MODL' and not rec['modl'] and len(fd) > 1 and fd[0:1] != b'\0':
                try:
                    rec['modl'] = fd.rstrip(b'\0').decode('latin-1')
                except Exception:
                    pass
            elif sig == b'SCOL' and ft == b'ONAM' and len(fd) >= 4:
                cur = {'base': struct.unpack_from('<I', fd, 0)[0], 'xforms': []}
                rec['parts'].append(cur)
            elif sig == b'SCOL' and ft == b'DATA' and cur is not None:
                # DATA is an array of 7 floats: pos(3) rot(3) scale(1)
                for e in range(len(fd) // 28):
                    v = struct.unpack_from('<7f', fd, e * 28)
                    cur['xforms'].append(list(v))
        bases[formid] = rec

    def walkRange(start, end, world, cell, cellcoord):
        i = start
        while i + REC_HDR <= end:
            t = buf[i:i + 4]
            size = struct.unpack_from('<I', buf, i + 4)[0]
            if t == GRUP:
                label = struct.unpack_from('<I', buf, i + 8)[0]
                gtype = struct.unpack_from('<I', buf, i + 12)[0]
                sub = (label if gtype == 1 else world)
                cell, cellcoord = walkRange(i + REC_HDR, i + size, sub, cell, cellcoord)
                i += size
                continue
            flags = struct.unpack_from('<I', buf, i + 8)[0]
            formid = struct.unpack_from('<I', buf, i + 12)[0]
            body = record_data(buf, i + REC_HDR, size, flags)
            if t == b'CELL':
                cell, cellcoord = formid, None
                for ft, fd in read_fields(body):
                    if ft == b'XCLC' and len(fd) >= 8:
                        cellcoord = struct.unpack_from('<ii', fd, 0)
            elif t in MODELLED:
                handle_base(t, formid, body)
            elif t == b'REFR' and world == WORLD and cellcoord is not None:
                cx, cy = cellcoord
                if cx0 <= cx <= cx1 and cy0 <= cy <= cy1:
                    base, pos, rot, scale = 0, None, (0.0, 0.0, 0.0), 1.0
                    for ft, fd in read_fields(body):
                        if ft == b'NAME' and len(fd) >= 4:
                            base = struct.unpack_from('<I', fd, 0)[0]
                        elif ft == b'DATA' and len(fd) >= 24:
                            v = struct.unpack_from('<6f', fd, 0)
                            pos, rot = v[0:3], v[3:6]
                        elif ft == b'XSCL' and len(fd) >= 4:
                            scale = struct.unpack_from('<f', fd, 0)[0]
                    if pos is not None:
                        refs.append({'formid': formid, 'base': base, 'cell': [cx, cy],
                                     'pos': list(pos), 'rot': list(rot), 'scale': scale})
            i += REC_HDR + size
        return cell, cellcoord

    size = struct.unpack_from('<I', buf, 4)[0]
    walkRange(REC_HDR + size, n, 0, None, None)
    return refs, bases


def main(argv):
    esm, cx0, cy0, cx1, cy1, out = argv[0], int(argv[1]), int(argv[2]), \
        int(argv[3]), int(argv[4]), argv[5]
    refs, bases = walk(esm, cx0, cy0, cx1, cy1)
    used = set(r['base'] for r in refs)
    # SCOL parts pull in their own bases
    for f in list(used):
        for p in bases.get(f, {}).get('parts', []):
            used.add(p['base'])
    slim = dict((('%08X' % k), bases[k]) for k in used if k in bases)
    with open(out, 'w') as f:
        json.dump({'refs': refs, 'bases': slim}, f)
    print('refs %d  bases %d  (of %d modelled records seen)'
          % (len(refs), len(slim), len(bases)))


if __name__ == '__main__':
    main(sys.argv[1:])
