"""Refutation probe: xLODGen says an untextured outer region falls back to
"the default landscape texture". Does Fallout4.esm actually DEFINE one for
Commonwealth, and what colour is it?

WRLD carries DNAM = default land texture formid + default water texture formid.
LTEX carries TNAM = a TXST formid. TXST carries TX00 = the diffuse path.
If the default land texture exists, its own mean colour is directly comparable
with the mean of the LOD tiles Bethesda shipped for the outer region
(lum 71.0, meanSat 0.191, measured by landmap.py).
"""
import struct, zlib, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dds
from esmland import Reader, subrecords, record_data, COMPRESSED

ESM = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm'
WANT_WS = 0x0000003C

r = Reader(ESM)
hdr = r.at(0, 24)
pos = 24 + struct.unpack_from('<I', hdr, 4)[0]

wrld_dnam = None
ltex = {}      # formid -> TNAM txst formid
txst = {}      # formid -> TX00 path
edid = {}


def scan(start, end, depth=0):
    global wrld_dnam
    off = start
    while off + 24 <= end:
        tag = r.at(off, 24)
        if tag[:4] == b'GRUP':
            gsize = struct.unpack_from('<I', tag, 4)[0]
            if gsize < 24:
                return
            scan(off + 24, off + gsize, depth + 1)
            off += gsize
            continue
        typ = tag[:4]
        dsz, flags, formid = struct.unpack_from('<IiI', tag, 4)
        if typ in (b'WRLD', b'LTEX', b'TXST'):
            d = record_data(r, off + 24, dsz, flags)
            for st, pl in subrecords(d):
                if st == b'EDID':
                    edid[formid] = pl.rstrip(b'\0').decode('latin1')
                if typ == b'WRLD' and st == b'DNAM' and formid == WANT_WS:
                    wrld_dnam = struct.unpack_from('<II', pl, 0)
                if typ == b'LTEX' and st == b'TNAM' and len(pl) >= 4:
                    ltex[formid] = struct.unpack_from('<I', pl, 0)[0]
                if typ == b'TXST' and st == b'TX00':
                    txst[formid] = pl.rstrip(b'\0').decode('latin1')
        off += 24 + dsz


off = pos
while off + 24 <= r.size:
    tag = r.at(off, 24)
    if tag[:4] != b'GRUP':
        off += 24 + struct.unpack_from('<I', tag, 4)[0]
        continue
    gsize, label = struct.unpack_from('<I4s', tag, 4)
    if label in (b'WRLD', b'LTEX', b'TXST'):
        scan(off + 24, off + gsize)
    off += gsize

print('LTEX records with TNAM :', len(ltex))
print('TXST records with TX00 :', len(txst))
print()
print('Commonwealth (0000003C) WRLD DNAM:', wrld_dnam)
if wrld_dnam:
    land, water = wrld_dnam
    print('  default LAND  texture formid: %08X  edid=%s' % (land, edid.get(land)))
    print('  default WATER texture formid: %08X  edid=%s' % (water, edid.get(water)))
    for label, fid in (('default land', land),):
        if fid in (0, 0xFFFFFFFF):
            print('  -> %s is NULL: the worldspace defines NO default land texture' % label)
            continue
        ts = ltex.get(fid)
        print('  -> LTEX %08X TNAM -> TXST %s' % (fid, ('%08X' % ts) if ts else None))
        path = txst.get(ts) if ts else None
        print('  -> TX00 = %r' % path)
        if path:
            full = os.path.join(r'E:\Tools\Fallout 4\DataUnpacked\Data',
                                path.replace('\\', os.sep))
            print('  -> on disk:', os.path.exists(full), full)
            if os.path.exists(full):
                s = dds.stats(full, 2)
                print('     %dx%d %s  mean rgb %.1f %.1f %.1f  lum %.1f  meanSat %.3f  lumStd %.1f'
                      % (s['w'], s['h'], s['fmt'], s['mean'][0], s['mean'][1],
                         s['mean'][2], s['lum'], s['meanSat'], s['lumStd']))
