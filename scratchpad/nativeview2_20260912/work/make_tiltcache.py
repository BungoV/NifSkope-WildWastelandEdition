#!/usr/bin/env python
"""Gate (d)'s second known-answer input: a sheet cache whose normal tiles say
"tilted towards +X (east)" at every texel, built exactly the way the FLAT cache
was -- the bake's own .n.DDS with every BC1 block replaced by one constant
colour and zero indices, so nothing but the normal texels changes.

The colour is written in the sheet's own order (R east, G up, B north) and is
READ BACK after the 5/6/5 quantisation, because that read-back value -- not the
value asked for -- is what the arithmetic in the report must use.
"""
import os
import shutil
import struct
import sys

SRC = r"E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview1_20260912/work/sheetcache"
DST = r"E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview2_20260912/work/tiltcache"


def to565(r, g, b):
    return ((round(r / 255.0 * 31) << 11) | (round(g / 255.0 * 63) << 5)
            | round(b / 255.0 * 31))


def from565(c):
    r = (c >> 11) & 31
    g = (c >> 5) & 63
    bl = c & 31
    return ((r * 255 + 15) // 31, (g * 255 + 31) // 63, (bl * 255 + 15) // 31)


def main():
    # 30 degrees east of vertical, in the sheet's own channel order
    want = (191, 238, 128)          # R east, G up, B north
    c = to565(*want)
    got = from565(c)
    blk = struct.pack("<HHI", c, c, 0)

    if os.path.isdir(DST):
        shutil.rmtree(DST)
    shutil.copytree(SRC, DST)
    n = 0
    for root, _, files in os.walk(DST):
        for f in files:
            if not f.lower().endswith(".n.dds"):
                continue
            p = os.path.join(root, f)
            b = bytearray(open(p, "rb").read())
            h = struct.unpack_from("<7I", b, 4)
            w, hh, mips = h[3], h[2], max(1, h[6])
            off = 148 if bytes(b[84:88]) == b"DX10" else 128
            assert mips == 1, (p, mips)
            nb = ((w + 3) // 4) * ((hh + 3) // 4)
            assert off + nb * 8 == len(b), (p, off, nb, len(b))
            b[off:] = blk * nb
            open(p, "wb").write(bytes(b))
            n += 1
    print("asked  R,G,B = %d,%d,%d   (R east, G up, B north)" % want)
    print("stored 565 word 0x%04X" % c)
    print("READ BACK   R,G,B = %d,%d,%d" % got)
    e = got[0] / 255.0 * 2 - 1
    u = got[1] / 255.0 * 2 - 1
    nn = got[2] / 255.0 * 2 - 1
    L = (e * e + u * u + nn * nn) ** 0.5
    print("decoded unit normal  east %.4f  north %.4f  up %.4f"
          % (e / L, nn / L, u / L))
    import math
    print("tilt from vertical   %.2f deg, towards EAST" % math.degrees(math.acos(u / L)))
    print("%d normal tiles rewritten under %s" % (n, DST))


if __name__ == "__main__":
    sys.exit(main())
