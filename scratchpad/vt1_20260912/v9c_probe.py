# VT1 item 4: WHAT the V9c E/W seam is, measured.
#
# V9c says the last column of chunk (-24,24)'s _msn and the first column of
# (-20,24)'s differ by 188 counts where neighbouring columns inside a sheet
# differ by 13. This asks the obvious follow-up questions:
#   1. which CHANNEL moves (R = east, G = up, B = north)?
#   2. is it an OFF-BY-ONE -- does some other column of the east sheet match?
#   3. is it the whole column or a band?
#   4. is either sheet vanilla's own file, copied?
import struct, sys, os
import numpy as np

G = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vt1_20260912/gate/new.d/tex'


def bc1(path):
    b = open(path, 'rb').read()
    h, w = struct.unpack_from('<II', b, 12)
    out = np.zeros((h, w, 3), np.int16)
    bw, bh = w // 4, h // 4

    def rgb(c):
        return ((((c >> 11) & 31) * 255 + 15) // 31,
                (((c >> 5) & 63) * 255 + 31) // 63,
                ((c & 31) * 255 + 15) // 31)

    for by in range(bh):
        for bx in range(bw):
            o = 128 + (by * bw + bx) * 8
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


def mad(a, b):
    return float(np.abs(a - b).max(axis=-1).mean())


def main():
    W = bc1(G + '/Commonwealth.4.-24.24_msn.DDS')
    E = bc1(G + '/Commonwealth.4.-20.24_msn.DDS')
    S = W
    N = bc1(G + '/Commonwealth.4.-24.28_msn.DDS')
    w = W.shape[1]

    print('1. the E/W seam per channel  (R = east, G = up, B = north)')
    a, b = W[:, w - 1], E[:, 0]
    for i, ch in enumerate('RGB'):
        print('   %s mean |diff| %7.3f   W mean %6.1f   E mean %6.1f'
              % (ch, float(np.abs(a[:, i] - b[:, i]).mean()),
                 float(a[:, i].mean()), float(b[:, i].mean())))
    print('   whole-sheet channel means   W %s   E %s'
          % (np.round(W.reshape(-1, 3).mean(axis=0), 1),
             np.round(E.reshape(-1, 3).mean(axis=0), 1)))

    print('2. is it an off-by-one?  W column %d against E columns 0..5' % (w - 1))
    for k in range(6):
        print('   E col %d : %7.3f' % (k, mad(W[:, w - 1], E[:, k])))
    print('   ...and E column 0 against W columns %d..%d' % (w - 6, w - 1))
    for k in range(w - 6, w):
        print('   W col %d : %7.3f' % (k, mad(W[:, k], E[:, 0])))

    print('3. is it the whole column or a band?  seam |diff| by 64-row band')
    d = np.abs(W[:, w - 1] - E[:, 0]).max(axis=-1)
    for y0 in range(0, W.shape[0], 64):
        print('   rows %3d-%3d : mean %7.3f  max %3d'
              % (y0, y0 + 63, float(d[y0:y0 + 64].mean()), int(d[y0:y0 + 64].max())))

    print('4. the N/S seam, for contrast (it passes V9c): per channel')
    a, b = S[0], N[w - 1]
    for i, ch in enumerate('RGB'):
        print('   %s mean |diff| %7.3f' % (ch, float(np.abs(a[:, i] - b[:, i]).mean())))

    print('5. is either sheet a copy of a shipped vanilla file?')
    for tag in ('-24.24', '-20.24'):
        p = G + '/Commonwealth.4.%s_msn.DDS' % tag
        print('   %s : %d bytes, mip0 %dx%d' % (tag, os.path.getsize(p), W.shape[1], W.shape[0]))
    return 0


sys.exit(main())
