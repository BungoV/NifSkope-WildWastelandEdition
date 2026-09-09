"""Stitch a whole worldspace of terrain LOD diffuse tiles into one low-res PNG.

The point is a single question: are the cells outside the playable area darker,
or flatter, than the ones inside? So the image is deliberately small — a few
pixels per cell — and it is drawn in WORLD orientation: +x east to the right,
+y north UP, which means the tile rows are flipped against file order.

Tiles are decoded from a mip near the target size rather than mip 0 and then
resized, so a 48x48 grid of 512x512 sources costs almost nothing to read.

    python mosaic.py <dir> <level> <pxPerTile> <out.png> [label]

Missing tiles are drawn as magenta so a hole in coverage cannot be mistaken for
dark terrain.
"""
import os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dds import DDS  # the lane's verified decoder, self-checked on file size


def tile_rgb(path, want):
    """Decode the smallest mip still >= want on a side, box-reduce to want."""
    d = DDS(path)
    pick = 0
    for i in range(d.mips):
        if max(1, d.width >> i) >= want and max(1, d.height >> i) >= want:
            pick = i
        else:
            break
    mw, mh, rgba = d.decode(pick)
    px = [(rgba[i * 4] << 16) | (rgba[i * 4 + 1] << 8) | rgba[i * 4 + 2]
          for i in range(mw * mh)]
    # box-reduce mw x mh down to want x want
    out = bytearray(want * want * 3)
    for y in range(want):
        y0, y1 = y * mh // want, max(y * mh // want + 1, (y + 1) * mh // want)
        for x in range(want):
            x0, x1 = x * mw // want, max(x * mw // want + 1, (x + 1) * mw // want)
            r = g = b = n = 0
            for yy in range(y0, y1):
                base = yy * mw
                for xx in range(x0, x1):
                    p = px[base + xx]
                    r += (p >> 16) & 0xFF
                    g += (p >> 8) & 0xFF
                    b += p & 0xFF
                    n += 1
            o = (y * want + x) * 3
            out[o] = r // n
            out[o + 1] = g // n
            out[o + 2] = b // n
    return out


def write_png(path, w, h, rgb):
    import zlib
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += rgb[y * w * 3:(y + 1) * w * 3]
    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        return c + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(bytes(raw), 6))
    png += chunk(b'IEND', b'')
    open(path, 'wb').write(png)


def main():
    d, level, per, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    pat = re.compile(r'^Commonwealth\.%d\.(-?\d+)\.(-?\d+)\.DDS$' % level, re.I)
    tiles = {}
    for n in os.listdir(d):
        m = pat.match(n)
        if m:
            tiles[(int(m.group(1)), int(m.group(2)))] = os.path.join(d, n)
    if not tiles:
        print('no level-%d tiles in %s' % (level, d))
        return 1
    xs = sorted({k[0] for k in tiles})
    ys = sorted({k[1] for k in tiles})
    W, H = len(xs) * per, len(ys) * per
    img = bytearray(b'\xff\x00\xff' * (W * H))
    miss = 0
    for j, ty in enumerate(ys):
        for i, tx in enumerate(xs):
            p = tiles.get((tx, ty))
            if not p:
                miss += 1
                continue
            t = tile_rgb(p, per)
            # +y north drawn UP: last tile row lands at the top
            oy = (len(ys) - 1 - j) * per
            ox = i * per
            for y in range(per):
                src = y * per * 3
                # each tile's own rows also flip, since DDS row 0 is the tile's north edge
                dst = ((oy + y) * W + ox) * 3
                img[dst:dst + per * 3] = t[src:src + per * 3]
    write_png(out, W, H, img)
    print('%s  %dx%d px  %d tiles (%s..%s x, %s..%s y)  %d missing'
          % (out, W, H, len(tiles), xs[0], xs[-1], ys[0], ys[-1], miss))
    return 0


if __name__ == '__main__':
    sys.exit(main())
