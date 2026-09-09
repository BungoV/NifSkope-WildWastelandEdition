"""Where is Sanctuary, measured rather than assumed? Scan Commonwealth exterior
CELL records for EDIDs naming known landmarks and print their grid coords."""
import struct, os, sys, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from esmland import Reader, subrecords, record_data

ESM = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm'
WANT_WS = 0x0000003C
PAT = re.compile(r'(sanctuary|concord|redrocket|vault111|redRocket)', re.I)

r = Reader(ESM)
pos = 24 + struct.unpack_from('<I', r.at(0, 24), 4)[0]
hits = []
named = 0
total = 0


def scan(start, end, ctx):
    global named, total
    off = start
    while off + 24 <= end:
        tag = r.at(off, 24)
        if tag[:4] == b'GRUP':
            gs, label, gt = struct.unpack_from('<I4si', tag, 4)
            if gs < 24:
                return
            c = dict(ctx)
            if gt == 1:
                c['ws'] = struct.unpack('<I', label)[0]
            if c.get('ws') == WANT_WS or gt in (0, 1):
                scan(off + 24, off + gs, c)
            off += gs
            continue
        typ = tag[:4]
        dsz, flags, formid = struct.unpack_from('<IiI', tag, 4)
        if typ == b'CELL' and ctx.get('ws') == WANT_WS:
            d = record_data(r, off + 24, dsz, flags)
            ed = None
            gx = gy = None
            for st, pl in subrecords(d):
                if st == b'EDID':
                    ed = pl.rstrip(b'\0').decode('latin1')
                elif st == b'XCLC' and len(pl) >= 8:
                    gx, gy = struct.unpack_from('<ii', pl, 0)
            total += 1
            if ed:
                named += 1
                if PAT.search(ed):
                    hits.append((ed, gx, gy, formid))
        off += 24 + dsz


off = pos
while off + 24 <= r.size:
    tag = r.at(off, 24)
    if tag[:4] != b'GRUP':
        off += 24 + struct.unpack_from('<I', tag, 4)[0]
        continue
    gs, label = struct.unpack_from('<I4s', tag, 4)
    if label == b'WRLD':
        scan(off + 24, off + gs, {})
    off += gs

print('Commonwealth exterior CELLs: %d (with an EDID: %d)' % (total, named))
print('matches:')
for ed, gx, gy, f in sorted(hits, key=lambda h: (h[1] or 0, h[2] or 0)):
    print('   %-44s cell (%4s,%4s)  %08X' % (ed, gx, gy, f))
