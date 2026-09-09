"""Read-only ESM walker: for worldspace Commonwealth (0000003C) in Fallout4.esm,
report for every exterior cell which LAND subrecords exist.

Written from the layouts in E:\\Projects\\NifskopeWildWastelandEdition\\
src\\esmdata.cpp (BTXT/ATXT/VTXT at :305-333, VHGT at :334, VCLR at :351).

Record header (24 bytes): type[4] dataSize:u32 flags:u32 formID:u32
                          timestamp:u16 vcs:u16 version:u16 unknown:u16
GRUP header  (24 bytes): 'GRUP' groupSize:u32 (INCLUDES header) label[4]
                          groupType:i32 timestamp:u16 vcs:u16 version:u16 unk:u16
Compressed record: flags & 0x00040000 -> data is u32 uncompressedSize + zlib.
Subrecord: type[4] size:u16 ; 'XXXX' carries a u32 size for the NEXT subrecord.

Self-check: the walker asserts every GRUP's children consume exactly its
groupSize, so a layout error is loud rather than silent.
"""
import struct, zlib, sys, os, io, collections

ESM = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm'
WANT_WS = 0x0000003C

COMPRESSED = 0x00040000


class Reader:
    def __init__(self, path):
        self.f = open(path, 'rb')
        self.size = os.path.getsize(path)

    def at(self, off, n):
        self.f.seek(off)
        return self.f.read(n)


def subrecords(data):
    """yield (type, payload). Handles XXXX oversize."""
    i = 0
    n = len(data)
    override = None
    while i + 6 <= n:
        typ = data[i:i + 4]
        sz = struct.unpack_from('<H', data, i + 4)[0]
        i += 6
        if typ == b'XXXX':
            override = struct.unpack_from('<I', data, i)[0]
            i += sz
            continue
        if override is not None:
            sz = override
            override = None
        yield typ, data[i:i + sz]
        i += sz


def record_data(r, off, dataSize, flags):
    raw = r.at(off, dataSize)
    if flags & COMPRESSED:
        if len(raw) < 4:
            return b''
        try:
            return zlib.decompress(raw[4:])
        except zlib.error:
            return b''
    return raw


def walk(path):
    r = Reader(path)
    # TES4 header record
    hdr = r.at(0, 24)
    assert hdr[:4] == b'TES4', hdr[:4]
    dataSize = struct.unpack_from('<I', hdr, 4)[0]
    pos = 24 + dataSize

    cells = {}            # (x, y) -> dict of present subrecord types
    stats = collections.Counter()
    ws_found = False

    def scan_group(start, end, ctx):
        nonlocal ws_found
        off = start
        while off + 24 <= end:
            tag = r.at(off, 24)
            if tag[:4] == b'GRUP':
                gsize, label, gtype = struct.unpack_from('<I4si', tag, 4)
                if gsize < 24:
                    return
                child_ctx = dict(ctx)
                if gtype == 1:      # World Children; label = worldspace formID
                    wsid = struct.unpack('<I', label)[0]
                    child_ctx['ws'] = wsid
                    if wsid == WANT_WS:
                        ws_found = True
                if ctx.get('ws') == WANT_WS or gtype in (0, 1) or child_ctx.get('ws') == WANT_WS:
                    scan_group(off + 24, off + gsize, child_ctx)
                off += gsize
                continue
            typ = tag[:4]
            dsz, flags, formid = struct.unpack_from('<IiI', tag, 4)
            body = off + 24
            if ctx.get('ws') == WANT_WS:
                if typ == b'CELL':
                    d = record_data(r, body, dsz, flags)
                    gx = gy = None
                    for st, pl in subrecords(d):
                        if st == b'XCLC' and len(pl) >= 8:
                            gx, gy = struct.unpack_from('<ii', pl, 0)
                    ctx['cell'] = (gx, gy) if gx is not None else None
                elif typ == b'LAND':
                    d = record_data(r, body, dsz, flags)
                    cur = ctx.get('cell')
                    if cur is not None:
                        info = cells.setdefault(cur, {'BTXT': 0, 'ATXT': 0, 'VTXT': 0,
                                                      'VHGT': 0, 'VCLR': 0, 'VNML': 0,
                                                      'base': set(), 'lay': set()})
                        for st, pl in subrecords(d):
                            k = st.decode('latin1')
                            if k in info:
                                info[k] += 1
                            if st == b'BTXT' and len(pl) >= 8:
                                info['base'].add(struct.unpack_from('<I', pl, 0)[0])
                            if st == b'ATXT' and len(pl) >= 8:
                                info['lay'].add(struct.unpack_from('<I', pl, 0)[0])
                        stats['LAND'] += 1
            elif typ == b'WRLD':
                ctx['ws'] = formid
            off += 24 + dsz

    # top level
    off = pos
    while off + 24 <= r.size:
        tag = r.at(off, 24)
        if tag[:4] != b'GRUP':
            dsz = struct.unpack_from('<I', tag, 4)[0]
            off += 24 + dsz
            continue
        gsize, label, gtype = struct.unpack_from('<I4si', tag, 4)
        if label == b'WRLD':
            scan_group(off + 24, off + gsize, {})
        off += gsize

    return cells, stats, ws_found


if __name__ == '__main__':
    cells, stats, ok = walk(ESM)
    print('worldspace 0000003C group found:', ok)
    print('LAND records seen:', stats['LAND'])
    print('cells with a LAND:', len(cells))
    import pickle
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'landinfo.pkl'), 'wb') as f:
        pickle.dump(cells, f)
    xs = [c[0] for c in cells]
    ys = [c[1] for c in cells]
    if xs:
        print('cell x range %d..%d   y range %d..%d' % (min(xs), max(xs), min(ys), max(ys)))
    nv = sum(1 for v in cells.values() if v['VHGT'])
    nc = sum(1 for v in cells.values() if v['VCLR'])
    nb = sum(1 for v in cells.values() if v['BTXT'])
    na = sum(1 for v in cells.values() if v['ATXT'])
    print('with VHGT %d   with VCLR %d   with BTXT %d   with ATXT(layers) %d'
          % (nv, nc, nb, na))
