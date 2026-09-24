"""LAND1 Part B -- make a REAL edited plugin, so the byte-identity gate can
exercise the dependency map's `cells` and `refs` rows and not only the rows a
loose-file override can reach.

An incremental bake is only worth trusting if the ledger notices the edits a
person actually makes, and the two commonest are "I raised that hill" and "I
moved that building".  Neither can be simulated with a resource override, so
this makes a COPY of Fallout4.esm and patches bytes in it -- the record layout
is unchanged, so nothing downstream can tell it from a plugin a person edited,
which is the point.

    python b_esmedit.py list   <esm> <cx0> <cy0> <cx1> <cy1>
    python b_esmedit.py height <esm> <out> <cx> <cy> <delta>
    python b_esmedit.py move   <esm> <out> <cx> <cy> <dx>
    python b_esmedit.py moveid <esm> <out> <cx> <cy> <dx> <formid-hex>

`height` adds `delta` to ONE height-gradient byte of the cell's LAND VHGT (row
1, column 1 -- an interior vertex, so no chunk seam is involved and the change
is inside one cell); `move` adds `dx` world units to the X of the first
uncompressed REFR in the cell.

COMPRESSED RECORDS.  37,019 of Fallout4.esm's 37,020 LAND records carry flag
0x00040000: u32 decompressed size, then a zlib stream.  Rewriting one changes
its size, and the first version of this refused for that reason.  That refusal
was wrong, and the reason it was wrong is worth writing down: an ESM has NO
global offset table.  The only length fields in the file are each record's own
dataSize and each enclosing GRUP's groupSize, and both are reachable from the
walk that found the record.  So a size change is legal as long as the record's
dataSize and every ancestor GRUP's groupSize move by the same delta -- which is
what `splice()` below does.  Nothing downstream reads a byte offset that this
invalidates.

Record header, Fallout 4: type[4] dataSize:u32 flags:u32 formID:u32
timestamp:u16 vcs:u16 version:u16 unknown:u16 -- 24 bytes, dataSize EXCLUDING
it.  GRUP: 'GRUP' groupSize:u32 (INCLUDING the 24) label:u32 groupType:i32 ...
"""
import os
import struct
import sys
import zlib

HDR = 24
COMPRESSED = 0x00040000


def walk(buf, start, end, cb, cell=None, stack=(), ws=0):
    """cb(type, recStart, dataStart, dataSize, flags, cell, grupStack, ws).

    grupStack is the tuple of enclosing GRUP record starts, outermost first --
    exactly the groupSize fields a size change has to fix up.

    `ws` is the worldspace form id of the enclosing world-children GRUP
    (groupType 1, label = the WRLD's form id).  It is NOT optional bookkeeping:
    cell (-20,20) exists in Commonwealth AND in a second worldspace of
    Fallout4.esm, so a walker that ignored it listed two LAND records for one
    coordinate and would have edited whichever came first.
    """
    p = start
    while p + HDR <= end:
        typ = bytes(buf[p:p + 4])
        size = struct.unpack_from('<I', buf, p + 4)[0]
        if typ == b'GRUP':
            label, gtype = struct.unpack_from('<Ii', buf, p + 8)
            walk(buf, p + HDR, p + size, cb, cell, stack + (p,),
                 label if gtype == 1 else ws)
            p += size
            continue
        flags = struct.unpack_from('<I', buf, p + 8)[0]
        if typ == b'CELL':
            c = cell_coords(payload(buf, p, size, flags), 0, -1)
            if c is not None:
                cell = c
        cb(typ, p, p + HDR, size, flags, cell, stack, ws)
        p += HDR + size
    return cell


def payload(buf, recStart, size, flags):
    """The record's DATA, decompressed if it has to be."""
    raw = bytes(buf[recStart + HDR:recStart + HDR + size])
    if flags & COMPRESSED:
        try:
            return zlib.decompress(raw[4:])
        except zlib.error:
            return b''
    return raw


def subrecords(buf, dataStart, dataSize):
    p = dataStart
    end = len(buf) if dataSize < 0 else dataStart + dataSize
    while p + 6 <= end:
        sig = bytes(buf[p:p + 4])
        ln = struct.unpack_from('<H', buf, p + 4)[0]
        yield sig, p + 6, ln
        p += 6 + ln


def cell_coords(buf, dataStart, dataSize):
    for sig, off, ln in subrecords(buf, dataStart, dataSize):
        if sig == b'XCLC' and ln >= 8:
            return struct.unpack_from('<ii', buf, off)
    return None


def splice(buf, recStart, oldSize, newData, flags, stack):
    """Replace a record's data and fix every length field the change moves.

    Returns the new bytearray.  newData is the UNCOMPRESSED payload; it is
    re-compressed here if the record was compressed, so the record stays the
    shape the engine expects.
    """
    if flags & COMPRESSED:
        body = struct.pack('<I', len(newData)) + zlib.compress(newData, 9)
    else:
        body = newData
    delta = len(body) - oldSize
    out = bytearray(buf)
    out[recStart + HDR:recStart + HDR + oldSize] = body
    if delta:
        struct.pack_into('<I', out, recStart + 4, oldSize + delta)
        for g in stack:
            gs = struct.unpack_from('<I', out, g + 4)[0]
            struct.pack_into('<I', out, g + 4, gs + delta)
    return out, delta


def scan(path, want=None, types=(b'LAND', b'REFR'), worldspace=0x3C):
    """-> (buf, [(cell, type, recStart, dataSize, flags, grupStack)])."""
    buf = bytearray(open(path, 'rb').read())
    found = []

    def cb(typ, recStart, dataStart, dataSize, flags, cell, stack, ws):
        if typ in types and cell is not None and (worldspace is None or ws == worldspace):
            if want is None or cell in want:
                found.append((cell, typ.decode(), recStart, dataSize, flags, stack))

    walk(buf, 0, len(buf), cb)
    return buf, found


def main(argv):
    mode = argv[0]
    if mode == 'list':
        esm = argv[1]
        x0, y0, x1, y1 = (int(v) for v in argv[2:6])
        want = {(x, y) for x in range(x0, x1 + 1) for y in range(y0, y1 + 1)}
        _buf, found = scan(esm, want)
        by = {}
        for cell, typ, _rs, _sz, fl, _st in found:
            by.setdefault(cell, {}).setdefault(typ, [0, 0])
            by[cell][typ][0] += 1
            if fl & COMPRESSED:
                by[cell][typ][1] += 1
        for cell in sorted(by):
            print('%-12s %s' % (str(cell), ' '.join(
                '%s %d (%d compressed)' % (t, n[0], n[1]) for t, n in sorted(by[cell].items()))))
        return 0

    esm, out = argv[1], argv[2]
    cx, cy = int(argv[3]), int(argv[4])
    amount = float(argv[5])
    buf, found = scan(esm, {(cx, cy)})
    hits = [f for f in found if f[0] == (cx, cy)]
    if not hits:
        print('REFUSED: no LAND or REFR indexed in cell (%d,%d)' % (cx, cy))
        return 2

    if mode == 'height':
        lands = [f for f in hits if f[1] == 'LAND']
        if not lands:
            print('REFUSED: cell (%d,%d) has no LAND record to edit' % (cx, cy))
            return 2
        _c, _t, rs, sz, fl, stack = lands[0]
        data = payload(buf, rs, sz, fl)
        if not data:
            print('REFUSED: that LAND record would not decompress')
            return 2
        pos = None
        for sig, off, ln in subrecords(data, 0, len(data)):
            if sig == b'VHGT':
                # VHGT = float offset, then 33*33 signed gradient bytes, then pad.
                # Byte 4 + 34 is row 1, column 1: an interior vertex.
                pos = off + 4 + 34
                break
        if pos is None:
            print('REFUSED: that LAND has no VHGT subrecord')
            return 2
        d = bytearray(data)
        old = d[pos]
        d[pos] = (old + int(amount)) & 0xFF
        print('VHGT gradient byte at data+%d: %d -> %d (cell %d,%d, %s)'
              % (pos, old, d[pos], cx, cy,
                 'compressed' if fl & COMPRESSED else 'uncompressed'))
        buf, delta = splice(buf, rs, sz, bytes(d), fl, stack)
        print('record data %d -> %d bytes, %d GRUP size(s) fixed up'
              % (sz, sz + delta, len(stack) if delta else 0))

    elif mode == 'moveid':
        # Move ONE NAMED reference -- the caller picked it out of the base
        # bake's manifest, so it is a ref the bake demonstrably draws. See the
        # module docstring for why `move` was not good enough.
        target = int(argv[6], 16)
        refrs = [f for f in hits if f[1] == 'REFR' and not (f[4] & COMPRESSED)]
        done = False
        for _c, _t, rs, sz, _fl, _st in refrs:
            form = struct.unpack_from('<I', buf, rs + 12)[0]
            if form != target:
                continue
            for sig, off, ln in subrecords(buf, rs + HDR, sz):
                if sig == b'DATA' and ln >= 24:
                    x = struct.unpack_from('<f', buf, off)[0]
                    struct.pack_into('<f', buf, off, x + amount)
                    print('REFR 0x%08X DATA x: %.3f -> %.3f (cell %d,%d)'
                          % (form, x, x + amount, cx, cy))
                    done = True
                    break
            if done:
                break
        if not done:
            print('REFUSED: 0x%08X is not an uncompressed REFR of cell (%d,%d)'
                  % (target, cx, cy))
            return 2

    elif mode == 'move':
        refrs = [f for f in hits if f[1] == 'REFR' and not (f[4] & COMPRESSED)]
        if not refrs:
            print('REFUSED: cell (%d,%d) has no uncompressed REFR to move' % (cx, cy))
            return 2
        done = False
        for _c, _t, rs, sz, _fl, _st in refrs:
            for sig, off, ln in subrecords(buf, rs + HDR, sz):
                if sig == b'DATA' and ln >= 24:
                    x = struct.unpack_from('<f', buf, off)[0]
                    struct.pack_into('<f', buf, off, x + amount)
                    print('REFR 0x%08X DATA x: %.3f -> %.3f (cell %d,%d)'
                          % (struct.unpack_from('<I', buf, rs + 12)[0],
                             x, x + amount, cx, cy))
                    done = True
                    break
            if done:
                break
        if not done:
            print('REFUSED: no REFR DATA subrecord found')
            return 2
    else:
        print(__doc__)
        return 2

    with open(out, 'wb') as f:
        f.write(buf)
    print('wrote %s (%d bytes)' % (out, os.path.getsize(out)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
