#!/usr/bin/env python3
"""IDENT lane -- one read-only walk of Fallout4.esm for the GROUPING subrecords.

esm.pkl (another lane's, read-only) already holds positions, bases and MODL
paths.  It does NOT hold the subrecords that could say "these REFRs are one
building", so this walk collects exactly those, and the records they point at:

  on the REFR   XLYR  XESP  XLKR  XLRT  XMBR  XRFG  XPRM  XPRD  XPPA
  records       LAYR  SCOL  PKIN  RFGP

Every layout is quoted from wbDefinitionsFO4.pas in ident_notes.md.
Output goes to the SESSION scratchpad, never into the lane folder.
"""
import os
import pickle
import struct
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
sys.path.insert(0, ROOT + '/tools')
import lod_emission_probe as P              # noqa: E402

ESM = 'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm'
OUT = os.environ.get('IDENT_OUT') or (
    'C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/'
    '392777f8-9016-4913-858d-16d6eec4c01a/scratchpad/ident_scan.pkl')

COMMONWEALTH = 0x0000003C
DELETED = 0x00000020
INITIALLY_DISABLED = 0x00000800

# the same window esm.pkl keeps, so the two tables join on refForm
WX0, WX1 = -4, 20
WY0, WY1 = -24, 4

# REFR subrecords that could carry an identity.  Signature -> how to decode.
#   XLYR  wbFormIDCk(XLYR, 'Layer', [LAYR])                        line 3246
#   XESP  wbStruct(XESP,'Enable Parent',[FormID, u8 flags, 3 pad]) line 4330
#   XLKR  wbStructSK(XLKR,[0],'Linked Reference',[Keyword/Ref, Ref]) line 3227
#   XLRT  wbArray(XLRT,'Location Ref Type', FormID)                line 3259
#   XMBR  wbFormIDCk(XMBR, 'MultiBound Reference', [REFR])         line 3257
#   XRFG  wbFormIDCk(XRFG, 'Reference Group', [RFGP])              line 3246+
#   XPRM  wbStruct(XPRM, 'Primitive', ...)                         line 11457
#   XPRD/XPPA  patrol, checked only to say plainly they are not pack-in marks
WANT = (b'XLYR', b'XESP', b'XLKR', b'XLRT', b'XMBR', b'XRFG', b'XPRM',
        b'XPRD', b'XPPA')


def main():
    buf = open(ESM, 'rb').read()
    sys.stderr.write('esm %d bytes\n' % len(buf))
    cellWorld = {}
    refsub = {}          # refForm -> {sig: payload}
    layr = {}            # form -> (edid, parentForm)
    scol = {}            # form -> dict(edid, obnd, parts=[(onam, nplace)], modl)
    pkin = {}            # form -> (edid, cellForm)
    rfgp = {}            # form -> (edid, name, refForm, pkinForm)
    nrefr = nkept = 0

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
            nrefr += 1
            pos = None
            got = {}
            for st, payload in P.subrecords(P.recordData(buf, doff, dsize, flags)):
                if st == b'DATA' and len(payload) >= 24:
                    pos = struct.unpack_from('<3f', payload, 0)
                elif st in WANT:
                    # XLKR and XLRT are arrays: keep every occurrence
                    got.setdefault(st, []).append(payload)
            if pos is None:
                continue
            cx = int(pos[0] // 4096.0)
            cy = int(pos[1] // 4096.0)
            if not (WX0 <= cx < WX1 and WY0 <= cy < WY1):
                continue
            nkept += 1
            if got:
                refsub[form] = got
            continue

        if typ == b'LAYR':
            edid, parent = '', 0
            for st, payload in P.subrecords(P.recordData(buf, doff, dsize, flags)):
                if st == b'EDID':
                    edid = P.zstr(payload)
                elif st == b'PNAM' and len(payload) >= 4:
                    parent = struct.unpack_from('<I', payload, 0)[0]
            layr[form] = (edid, parent)
            continue

        if typ == b'SCOL':
            edid, modl, obnd = '', '', None
            parts = []
            cur = None
            for st, payload in P.subrecords(P.recordData(buf, doff, dsize, flags)):
                if st == b'EDID':
                    edid = P.zstr(payload)
                elif st == b'MODL' and not modl and b'.' in payload:
                    modl = P.zstr(payload)
                elif st == b'OBND' and len(payload) >= 12:
                    obnd = struct.unpack_from('<6h', payload, 0)
                elif st == b'ONAM' and len(payload) >= 4:
                    cur = struct.unpack_from('<I', payload, 0)[0]
                    parts.append([cur, 0, len(payload)])
                elif st == b'DATA' and parts:
                    # wbStaticPartPlacements is NOT defined in wbDefinitionsFO4.pas
                    # (grep "Placements" finds only the use at line 12624).  The
                    # placement stride is therefore DERIVED: pos 3f + rot 3f +
                    # scale f = 28 bytes.  ident_notes.md records the check that
                    # every DATA in the corpus divides by 28.
                    parts[-1][1] = len(payload)
            scol[form] = dict(edid=edid, modl=modl, obnd=obnd,
                              parts=[(p[0], p[1]) for p in parts])
            continue

        if typ == b'PKIN':
            edid, cell = '', 0
            for st, payload in P.subrecords(P.recordData(buf, doff, dsize, flags)):
                if st == b'EDID':
                    edid = P.zstr(payload)
                elif st == b'CNAM' and len(payload) >= 4:
                    cell = struct.unpack_from('<I', payload, 0)[0]
            pkin[form] = (edid, cell)
            continue

        if typ == b'RFGP':
            edid, name, ref, pk = '', '', 0, 0
            for st, payload in P.subrecords(P.recordData(buf, doff, dsize, flags)):
                if st == b'EDID':
                    edid = P.zstr(payload)
                elif st == b'NNAM':
                    name = P.zstr(payload)
                elif st == b'RNAM' and len(payload) >= 4:
                    ref = struct.unpack_from('<I', payload, 0)[0]
                elif st == b'PNAM' and len(payload) >= 4:
                    pk = struct.unpack_from('<I', payload, 0)[0]
            rfgp[form] = (edid, name, ref, pk)
            continue

    sys.stderr.write('cw refrs %d  window %d  with-subrec %d\n'
                     % (nrefr, nkept, len(refsub)))
    sys.stderr.write('LAYR %d  SCOL %d  PKIN %d  RFGP %d\n'
                     % (len(layr), len(scol), len(pkin), len(rfgp)))
    with open(OUT, 'wb') as f:
        pickle.dump(dict(refsub=refsub, layr=layr, scol=scol, pkin=pkin,
                         rfgp=rfgp, nrefr=nrefr, nkept=nkept), f, 2)
    sys.stderr.write('wrote %s (%d B)\n' % (OUT, os.path.getsize(OUT)))


if __name__ == '__main__':
    main()
