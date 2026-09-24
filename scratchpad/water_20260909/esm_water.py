#!/usr/bin/env python3
"""esm_water.py -- read-only water census of a worldspace in Fallout4.esm.

Lane WATER1.  Reuses the VALIDATED reader
scratchpad/mountains_20260907/fo4esm.py (lane ESPWRITE; its group walk asserts
that every GRUP's children consume exactly its declared size, and its
compressed-record wrapper round-trips on this master's exterior CELLs).
Nothing here re-implements the record layout.

Collected, per exterior cell of the worldspace:
    XCLC x, y            docs/LODGEN_ESM_LAYOUTS.md CELL
    DATA u16 flags       bit1 Has Water, bit3 No LOD Water
    XCLW float           local water height, sentinels 0xFF7FFFFF / 0x7F7FFFFF
                         / 0x4F7FFFC9 = "no local water"
    XCWT u32             WATR form id
    LAND                 whether the cell's temporary children carry a LAND
                         record at all (the census needs the GENERATED extent,
                         not the 192x192 filler rectangle)
And for the worldspace itself: DNAM (default land / default water height),
NAM3 (LOD water type), NAM4 (LOD water height), MNAM (usable cell extent).
And every WATR record in the file: form id, EDID, and the raw DNAM bytes.

Output: a pickle beside this file, so the census script does not re-walk the
330 MB master on every run.
"""

import os
import pickle
import struct
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(os.path.join(_HERE, '..', 'mountains_20260907')))

import fo4esm  # noqa: E402

ESM = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm'
NO_WATER_SENTINELS = (0xFF7FFFFF, 0x7F7FFFFF, 0x4F7FFFC9)


def collect(path, want_ws):
    buf = fo4esm.load(path)
    cells = {}
    watr = {}
    wrld = {}
    state = {'ws': None, 'cell': None}

    def sub(payload, tag):
        for t, c in fo4esm.subrecords(payload):
            if t == tag:
                return c
        return None

    def on_record(off, sig, dsize, flags, formid, tail, stack):
        # which worldspace are we inside?  GRUP type 1 label = worldspace formid
        ws = None
        for node in stack:
            if node.gtype == 1:
                ws = struct.unpack('<I', node.label)[0]
        if sig == b'WATR':
            p = fo4esm.record_payload(buf, off, dsize, flags)
            e = sub(p, b'EDID')
            watr[formid] = {
                'edid': e.split(b'\0')[0].decode('latin1') if e else '',
                'subs': [(t.decode('latin1'), len(c)) for t, c in fo4esm.subrecords(p)],
                'DNAM': sub(p, b'DNAM'),
                'NAM0': sub(p, b'NAM0'),
                'NAM1': sub(p, b'NAM1'),
                'GNAM': sub(p, b'GNAM'),
            }
            return
        if sig == b'WRLD':
            state['ws'] = formid
            if formid == want_ws:
                p = fo4esm.record_payload(buf, off, dsize, flags)
                for t, c in fo4esm.subrecords(p):
                    k = t.decode('latin1')
                    if k in ('EDID', 'DNAM', 'NAM0', 'NAM9', 'NAM3', 'NAM4',
                             'MNAM', 'DATA', 'WCTR'):
                        wrld.setdefault(k, c)
            return
        if ws != want_ws:
            return
        if sig == b'CELL':
            p = fo4esm.record_payload(buf, off, dsize, flags)
            xy = None
            rec = {'data': None, 'xclw': None, 'xcwt': None, 'land': 0}
            for t, c in fo4esm.subrecords(p):
                if t == b'XCLC' and len(c) >= 8:
                    xy = struct.unpack_from('<ii', c, 0)
                elif t == b'DATA':
                    rec['data'] = struct.unpack_from('<H', c, 0)[0] if len(c) >= 2 else c[0]
                elif t == b'XCLW' and len(c) >= 4:
                    rec['xclw'] = struct.unpack_from('<I', c, 0)[0]   # raw bits
                elif t == b'XCWT' and len(c) >= 4:
                    rec['xcwt'] = struct.unpack_from('<I', c, 0)[0]
            state['cell'] = xy
            if xy is not None:
                cells[xy] = rec
        elif sig == b'LAND':
            xy = state['cell']
            if xy is not None and xy in cells:
                cells[xy]['land'] += 1

    for node in fo4esm.top_level_groups(buf):
        if node.label not in (b'WRLD', b'WATR'):
            continue
        fo4esm.walk(buf, node.offset + 24, node.offset + node.gsize, [node], on_record)
    return cells, watr, wrld


def main():
    ws = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0x3C
    cells, watr, wrld = collect(ESM, ws)
    out = os.path.join(_HERE, 'esm_water_%08X.pkl' % ws)
    with open(out, 'wb') as f:
        pickle.dump({'cells': cells, 'watr': watr, 'wrld': wrld}, f)
    print('cells %d  WATR records %d  wrld keys %s' % (
        len(cells), len(watr), sorted(wrld)))
    print('wrote', out)


if __name__ == '__main__':
    main()
