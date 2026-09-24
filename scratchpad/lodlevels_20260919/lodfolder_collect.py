#!/usr/bin/env python3
"""LODFOLDER (read-only): collect every LOD-bearing base from Fallout4.esm
(via this lane's esm.pkl) plus the five DLC masters, walked here.

Writes scratchpad/lodlevels_20260919/lodfolder_bases.pkl:
    allbases[(plugin, form)] = dict(type, edid, modl, slots[4])
Nothing else is touched.
"""
import os
import pickle
import struct
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
sys.path.insert(0, ROOT + '/tools')
import lod_emission_probe as P              # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
# intermediates go to the session scratchpad, not this lane's folder
TMP = os.environ.get('LODFOLDER_TMP') or HERE
DATA = 'X:/Programs/Steam/steamapps/common/Fallout 4/Data/'
DLCS = ['DLCRobot.esm', 'DLCworkshop01.esm', 'DLCworkshop02.esm',
        'DLCworkshop03.esm', 'DLCCoast.esm', 'DLCNukaWorld.esm']

LOD_BEARING = (b'STAT', b'SCOL', b'MSTT', b'FURN', b'DOOR', b'ACTI', b'LIGH',
               b'TREE', b'FLOR', b'CONT', b'ALCH', b'MISC')


def walkPlugin(path):
    buf = open(path, 'rb').read()
    out = {}
    for typ, form, flags, doff, dsize, stack in P.walk(buf):
        if typ not in LOD_BEARING:
            continue
        edid, modl = '', ''
        slots = ['', '', '', '']
        for st, payload in P.subrecords(P.recordData(buf, doff, dsize, flags)):
            if st == b'EDID':
                edid = P.zstr(payload)
            elif st == b'MODL' and not modl and b'.' in payload:
                modl = P.zstr(payload)
            elif st == b'MNAM':
                for k in range(4):
                    if (k + 1) * 260 <= len(payload):
                        slots[k] = P.zstr(payload[k * 260:(k + 1) * 260])
        out[form] = dict(type=typ.decode(), edid=edid, modl=modl, slots=slots)
    return out


def main():
    allbases = {}
    d = pickle.load(open(os.path.join(HERE, 'esm.pkl'), 'rb'))
    for form, b in d['bases'].items():
        allbases[('Fallout4.esm', form)] = dict(type=b['type'], edid=b['edid'],
                                                modl=b['modl'], slots=list(b['slots']))
    print('Fallout4.esm  bases %d' % len(d['bases']))
    for name in DLCS:
        p = DATA + name
        if not os.path.exists(p):
            print('%-22s MISSING' % name)
            continue
        got = walkPlugin(p)
        for form, b in got.items():
            allbases[(name, form)] = b
        print('%-22s bases %d' % (name, len(got)))
    with open(os.path.join(TMP, 'lodfolder_bases.pkl'), 'wb') as f:
        pickle.dump(allbases, f, 2)
    print('total bases %d' % len(allbases))


if __name__ == '__main__':
    main()
