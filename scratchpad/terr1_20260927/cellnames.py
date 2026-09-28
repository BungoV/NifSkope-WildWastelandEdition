"""TERR1: exterior CELL EditorIDs of the Commonwealth (0x3C) in a cell box, read straight from Fallout4.esm
(record walk; compressed records inflated). Used only to NAME the canyon samples.

usage: python cellnames.py <x0> <y0> <x1> <y1> <out.json>
"""
import sys, struct, zlib, json

ESM = 'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm'


def subrecords(data):
    o = 0
    big = None
    while o + 6 <= len(data):
        t = data[o:o + 4]
        sz = struct.unpack_from('<H', data, o + 4)[0]
        o += 6
        if t == b'XXXX':
            big = struct.unpack_from('<I', data, o)[0]
            o += sz
            continue
        if big is not None:
            sz, big = big, None
        yield t, data[o:o + sz]
        o += sz


def main():
    x0, y0, x1, y1 = map(int, sys.argv[1:5])
    b = open(ESM, 'rb').read()
    out = {}
    o = 0
    n = len(b)
    # find the WRLD top group, then the world-children group of 0x3C
    while o < n:
        t = b[o:o + 4]
        size = struct.unpack_from('<I', b, o + 4)[0]
        if t == b'GRUP':
            label = b[o + 8:o + 12]
            gtype = struct.unpack_from('<i', b, o + 12)[0]
            if gtype == 0 and label != b'WRLD':
                o += size
                continue
            if gtype == 1 and struct.unpack_from('<I', b, o + 8)[0] != 0x3C:
                o += size
                continue
            if gtype in (6, 8, 9, 10, 7):      # cell / topic children: skip
                o += size
                continue
            o += 24                         # descend
            continue
        # a record
        flags = struct.unpack_from('<I', b, o + 8)[0]
        data = b[o + 24:o + 24 + size]
        if t == b'CELL':
            if flags & 0x40000:
                data = zlib.decompress(data[4:])
            edid, grid = None, None
            for st, sd in subrecords(data):
                if st == b'EDID':
                    edid = sd.rstrip(b'\0').decode('latin1')
                elif st == b'XCLC':
                    grid = struct.unpack_from('<ii', sd, 0)
            if grid and x0 <= grid[0] <= x1 and y0 <= grid[1] <= y1 and edid:
                out['%d,%d' % grid] = edid
        o += 24 + size
    json.dump(out, open(sys.argv[5], 'w'), indent=0, sort_keys=True)
    print(len(out), 'named cells')


if __name__ == '__main__':
    main()
