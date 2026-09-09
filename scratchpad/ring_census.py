"""How much geometry a far ring can actually hold, measured off Fallout4.esm.

For the chunk that covers Sanctuary at each ring, count the references whose
base FILLS the MNAM slot that ring reads (dim 4 -> 0, 8 -> 1, 16 -> 2,
32 -> 3), and the ones that would drop out there.

The slots are POSITIONAL: MNAM is four fixed 260-byte records and an empty one
means "nothing at this ring".  lod_emission_probe.py compacts them away because
it only asks whether a base has any LOD model at all; this reader keeps the
positions, which is the whole question here.  Self-check: every MNAM payload
must be a whole number of 260-byte slots and at most four.

SCOL parts are not walked, so placements are undercounted; REFR counts are exact.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools'))
from lod_emission_probe import (walk, subrecords, recordData, zstr,
                                COMMONWEALTH, DEFAULT_ESM, LOD_BEARING, REC_HDR)

LEVEL = {4: 0, 8: 1, 16: 2, 32: 3}


def read(esm):
    buf = open(esm, 'rb').read()
    cellWorld, cellXY, slotsOf = {}, {}, {}
    refs = []
    odd = 0
    oddTypes = {}
    for typ, form, flags, doff, dsize, stack in walk(buf):
        if typ == b'CELL':
            w = None
            for label, gtype in reversed(stack):
                if gtype == 1:
                    w = struct.unpack_from('<I', label, 0)[0]
                    break
            cellWorld[form] = w
            for st, payload in subrecords(recordData(buf, doff, dsize, flags)):
                if st == b'XCLC' and len(payload) >= 8:
                    cellXY[form] = struct.unpack_from('<ii', payload, 0)
            continue
        if typ == b'REFR':
            c = None
            for label, gtype in reversed(stack):
                if gtype in (6, 8, 9, 10):
                    c = struct.unpack_from('<I', label, 0)[0]
                    if gtype == 6:
                        break
            if c is None or cellWorld.get(c) != COMMONWEALTH:
                continue
            for st, payload in subrecords(recordData(buf, doff, dsize, flags)):
                if st == b'NAME' and len(payload) >= 4:
                    refs.append((struct.unpack_from('<I', payload, 0)[0], c))
                    break
            continue
        if typ in LOD_BEARING:
            slots = ['', '', '', '']
            for st, payload in subrecords(recordData(buf, doff, dsize, flags)):
                if st != b'MNAM':
                    continue
                if len(payload) % 260 or len(payload) > 1040:
                    odd += 1
                    oddTypes[typ] = oddTypes.get(typ, 0) + 1
                    continue
                for k in range(len(payload) // 260):
                    slots[k] = zstr(payload[k * 260:(k + 1) * 260])
            slotsOf[form] = slots
    print('MNAM payloads that are not 1..4 whole 260-byte slots: %d %s '
          '(the generator reads MNAM on STAT only, so STAT must be 0)'
          % (odd, {k.decode(): v for k, v in oddTypes.items()}))
    return cellXY, slotsOf, refs


def main():
    cellXY, slotsOf, refs = read(DEFAULT_ESM)
    filled = [0, 0, 0, 0]
    for s in slotsOf.values():
        for k in range(4):
            if s[k]:
                filled[k] += 1
    print('bases with a LOD-bearing type in Fallout4.esm: %d; slot fill 0:%d 1:%d 2:%d 3:%d'
          % (len(slotsOf), filled[0], filled[1], filled[2], filled[3]))
    for dim in (4, 8, 16, 32):
        x0 = (-20 // dim) * dim
        y0 = (24 // dim) * dim
        lvl = LEVEL[dim]
        have = drop = notBase = anySlot = 0
        models = {}
        fallbackModels = {}
        for base, cell in refs:
            xy = cellXY.get(cell)
            if xy is None or not (x0 <= xy[0] < x0 + dim and y0 <= xy[1] < y0 + dim):
                continue
            s = slotsOf.get(base)
            if s is None:
                notBase += 1
                continue
            if s[lvl]:
                have += 1
                models[s[lvl]] = models.get(s[lvl], 0) + 1
            else:
                drop += 1
            if any(s):
                anySlot += 1
                # what --slot-fallback would substitute: nearest filled slot,
                # searching inwards first then outwards, as lodgen.cpp does
                pick = ''
                for k in range(lvl, -1, -1):
                    if s[k]:
                        pick = s[k]
                        break
                if not pick:
                    for k in range(lvl, 4):
                        if s[k]:
                            pick = s[k]
                            break
                fallbackModels[pick] = fallbackModels.get(pick, 0) + 1
        print('dim %-3d chunk (%4d,%4d) slot %d: %6d refs with a model, %6d without, '
              '%5d not a LOD-bearing base; %d distinct models; '
              '%6d refs have SOME slot filled (%d distinct fallback models)'
              % (dim, x0, y0, lvl, have, drop, notBase, len(models), anySlot, len(fallbackModels)))
        for m, n in sorted(models.items(), key=lambda kv: -kv[1])[:4]:
            print('        %5d x %s' % (n, m))


if __name__ == '__main__':
    main()
