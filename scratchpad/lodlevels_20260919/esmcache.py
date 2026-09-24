#!/usr/bin/env python3
"""LODLEVELS1 -- one read-only walk of Fallout4.esm, cached.

Produces, in a pickle:
  bases[form]  = dict(type, edid, modl, obnd, slots[4])   slots INDEX-PRESERVED
                 (MNAM is four fixed 260-byte slots; an empty slot stays '')
  cwcount[form]= how many Commonwealth REFRs name that base (whole worldspace)
  refs         = [(refform, baseform, x, y, z, rx, ry, rz, scale)] for the
                 window of cells around downtown Boston only
Nothing is written outside this lane's folder.
"""
import os
import pickle
import struct
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
sys.path.insert(0, ROOT + '/tools')
import lod_emission_probe as P              # noqa: E402  (walk/subrecords/recordData)

ESM = 'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm'
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'esm.pkl')

COMMONWEALTH = 0x0000003C
DELETED = 0x00000020
INITIALLY_DISABLED = 0x00000800

# the window of cells kept in full detail (downtown Boston plus room for the
# wider cameras).  chunk 4.4.-12 == cells x 4..7, y -12..-9.
WX0, WX1 = -4, 20
WY0, WY1 = -24, 4

LOD_BEARING = (b'STAT', b'SCOL', b'MSTT', b'FURN', b'DOOR', b'ACTI', b'LIGH',
               b'TREE', b'FLOR', b'CONT', b'ALCH', b'MISC')


def main():
    buf = open(ESM, 'rb').read()
    print('esm %d bytes' % len(buf))
    bases = {}
    cwcount = {}
    refs = []
    cellWorld = {}
    nrefr = 0
    for typ, form, flags, doff, dsize, stack in P.walk(buf):
        if typ == b'CELL':
            w = None
            for label, gtype in reversed(stack):
                if gtype == 1:
                    w = struct.unpack_from('<I', label, 0)[0]
                    break
            cellWorld[form] = w
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
            if flags & (DELETED | INITIALLY_DISABLED):
                continue
            base = None
            pos = None
            scale = 1.0
            for st, payload in P.subrecords(P.recordData(buf, doff, dsize, flags)):
                if st == b'NAME' and len(payload) >= 4:
                    base = struct.unpack_from('<I', payload, 0)[0]
                elif st == b'DATA' and len(payload) >= 24:
                    pos = struct.unpack_from('<6f', payload, 0)
                elif st == b'XSCL' and len(payload) >= 4:
                    scale = struct.unpack_from('<f', payload, 0)[0]
            if base is None:
                continue
            nrefr += 1
            cwcount[base] = cwcount.get(base, 0) + 1
            if pos is None:
                continue
            cx = int(pos[0] // 4096.0)
            cy = int(pos[1] // 4096.0)
            if WX0 <= cx < WX1 and WY0 <= cy < WY1:
                refs.append((form, base, pos[0], pos[1], pos[2],
                             pos[3], pos[4], pos[5], scale))
            continue
        if typ in LOD_BEARING:
            edid, modl, obnd = '', '', None
            slots = ['', '', '', '']
            for st, payload in P.subrecords(P.recordData(buf, doff, dsize, flags)):
                if st == b'EDID':
                    edid = P.zstr(payload)
                elif st == b'MODL' and not modl and b'.' in payload:
                    modl = P.zstr(payload)
                elif st == b'OBND' and len(payload) >= 12:
                    obnd = struct.unpack_from('<6h', payload, 0)
                elif st == b'MNAM':
                    for k in range(4):
                        if (k + 1) * 260 <= len(payload):
                            slots[k] = P.zstr(payload[k * 260:(k + 1) * 260])
            bases[form] = dict(type=typ.decode(), edid=edid, modl=modl,
                               obnd=obnd, slots=slots)
    print('bases %d  commonwealth refrs %d  window refs %d'
          % (len(bases), nrefr, len(refs)))
    with open(OUT, 'wb') as f:
        pickle.dump(dict(bases=bases, cwcount=cwcount, refs=refs,
                         window=(WX0, WY0, WX1, WY1), esm=ESM), f, 2)
    print('wrote %s (%d B)' % (OUT, os.path.getsize(OUT)))


if __name__ == '__main__':
    main()
