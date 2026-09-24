# VT1 item 4: the V9c E/W seam is a WHITE BAND in the _msn sheet. Map it.
#
# White here means exactly (255,255,255), which is not a unit normal in the
# sheet's own R=east G=up B=north encoding -- it is a cleared texel.
import struct, sys, os, glob
import numpy as np

G = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vt1_20260912/gate'
VAN = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'


def bc1(path):
    b = open(path, 'rb').read()
    h, w = struct.unpack_from('<II', b, 12)
    off = 148 if b[84:88] == b'DX10' else 128
    out = np.zeros((h, w, 3), np.int16)
    bw, bh = w // 4, h // 4

    def rgb(c):
        return ((((c >> 11) & 31) * 255 + 15) // 31,
                (((c >> 5) & 63) * 255 + 31) // 63,
                ((c & 31) * 255 + 15) // 31)

    for by in range(bh):
        for bx in range(bw):
            o = off + (by * bw + bx) * 8
            c0, c1 = struct.unpack_from('<HH', b, o)
            bits = struct.unpack_from('<I', b, o + 4)[0]
            a, d = rgb(c0), rgb(c1)
            if c0 > c1:
                pal = [a, d,
                       tuple((2 * a[i] + d[i]) // 3 for i in range(3)),
                       tuple((a[i] + 2 * d[i]) // 3 for i in range(3))]
            else:
                pal = [a, d, tuple((a[i] + d[i]) // 2 for i in range(3)), (0, 0, 0)]
            for k in range(16):
                out[by * 4 + k // 4, bx * 4 + k % 4] = pal[(bits >> (2 * k)) & 3]
    return out


def runs(idx):
    """[3,4,5,9] -> '3-5,9'"""
    if len(idx) == 0:
        return '(none)'
    out = []
    s = p = idx[0]
    for v in idx[1:]:
        if v == p + 1:
            p = v
            continue
        out.append('%d-%d' % (s, p) if p > s else '%d' % s)
        s = p = v
    out.append('%d-%d' % (s, p) if p > s else '%d' % s)
    return ','.join(out)


def report(label, path):
    if not os.path.exists(path):
        print('%-46s MISSING' % label)
        return
    a = bc1(path)
    white = (a == 255).all(axis=2)
    colw = np.nonzero(white.all(axis=0))[0]
    roww = np.nonzero(white.all(axis=1))[0]
    print('%-46s white texels %7d of %d (%.2f%%)'
          % (label, int(white.sum()), white.size, 100.0 * white.sum() / white.size))
    print('%-46s   fully white COLUMNS: %s' % ('', runs(colw)))
    print('%-46s   fully white ROWS   : %s' % ('', runs(roww)))


def main():
    print('OUR bake, direct chunk path, bare ruled default:')
    for c in ('-24.24', '-20.24', '-24.28', '-20.28'):
        report('  Commonwealth.4.%s_msn' % c,
               '%s/new.d/tex/Commonwealth.4.%s_msn.DDS' % (G, c))
    print()
    print('OUR bake, assembled from the pyramid:')
    for c in ('-24.24', '-20.24'):
        report('  Commonwealth.4.%s_msn' % c,
               '%s/new.vt/tex/Commonwealth.4.%s_msn.DDS' % (G, c))
    print()
    print("VANILLA's own shipped sheets, for the same chunks:")
    for c in ('-24.24', '-20.24'):
        cand = glob.glob('%s/Commonwealth.4.%s*_msn.dds' % (VAN, c)) or \
               glob.glob('%s/Commonwealth.4.%s*_msn.DDS' % (VAN, c))
        if not cand:
            print('  Commonwealth.4.%s_msn                        no vanilla file at %s' % (c, VAN))
            continue
        report('  %s' % os.path.basename(cand[0]), cand[0])
    print()
    print('and the COLOUR sheet of the same chunk, as a control:')
    report('  Commonwealth.4.-20.24 (colour)',
           '%s/new.d/tex/Commonwealth.4.-20.24.DDS' % G)
    return 0


sys.exit(main())
