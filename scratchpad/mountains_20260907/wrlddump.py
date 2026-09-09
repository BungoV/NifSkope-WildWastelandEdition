"""Dump every subrecord of the Commonwealth WRLD record so nothing is guessed."""
import struct, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from esmland import Reader, subrecords, record_data

ESM = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm'
WANT = 0x0000003C
r = Reader(ESM)
pos = 24 + struct.unpack_from('<I', r.at(0, 24), 4)[0]


def scan(start, end):
    off = start
    while off + 24 <= end:
        tag = r.at(off, 24)
        if tag[:4] == b'GRUP':
            gs = struct.unpack_from('<I', tag, 4)[0]
            if gs < 24:
                return
            scan(off + 24, off + gs)
            off += gs
            continue
        typ = tag[:4]
        dsz, flags, formid = struct.unpack_from('<IiI', tag, 4)
        if typ == b'WRLD' and formid == WANT:
            d = record_data(r, off + 24, dsz, flags)
            print('WRLD %08X  dataSize=%d  compressed=%s  inflated=%d'
                  % (formid, dsz, bool(flags & 0x00040000), len(d)))
            for st, pl in subrecords(d):
                s = st.decode('latin1')
                extra = ''
                if len(pl) == 4:
                    u = struct.unpack('<I', pl)[0]
                    f = struct.unpack('<f', pl)[0]
                    extra = '  u32=%08X  f32=%g' % (u, f)
                elif len(pl) == 8:
                    a, b = struct.unpack('<II', pl)
                    fa, fb = struct.unpack('<ff', pl)
                    extra = '  u32=(%08X,%08X)  f32=(%g,%g)' % (a, b, fa, fb)
                elif st in (b'EDID', b'FULL', b'ICON', b'MNAM', b'NNAM', b'TNAM'):
                    try:
                        extra = '  str=%r' % pl.rstrip(b'\0').decode('latin1')
                    except Exception:
                        pass
                print('   %-5s len=%-6d %s%s' % (s, len(pl), pl[:24].hex(), extra))
            return True
        off += 24 + dsz


off = pos
while off + 24 <= r.size:
    tag = r.at(off, 24)
    if tag[:4] != b'GRUP':
        off += 24 + struct.unpack_from('<I', tag, 4)[0]
        continue
    gs, label = struct.unpack_from('<I4s', tag, 4)
    if label == b'WRLD':
        if scan(off + 24, off + gs):
            break
    off += gs
