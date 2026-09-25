# LAND extent from a lodgen --dump-land file: count cells with LAND, their true bounding box.
import struct, sys
for fn in sys.argv[1:]:
    b = open(fn, 'rb').read()
    mnx, mny, cw, ch = struct.unpack_from('<4i', b, 0)
    flags = b[16:16 + cw * ch]
    cells = [(mnx + i % cw, mny + i // cw) for i, f in enumerate(flags) if f]
    xs = [c[0] for c in cells]; ys = [c[1] for c in cells]
    print('%s: LAND cells %d, extent x %d..%d y %d..%d (file box %dx%d from %d,%d)' % (
        fn.split('/')[-1], len(cells), min(xs), max(xs), min(ys), max(ys), cw, ch, mnx, mny))
