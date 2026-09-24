# VT1 item 4: V9c with the sheet decoded as what it IS.
#
# V9c decodes the _msn sheet with a DXT1 reader (8 bytes a block). The _msn
# sheet is DXT5 (16 bytes a block: 8 alpha then 8 colour), and vanilla's own
# _msn is DXT5 too. The DXT1 reader therefore takes every ALPHA block for a
# colour block, which is where the 4-white-columns-in-8 pattern comes from and
# where V9c's numbers come from.
#
# This runs V9c's statistic twice on the same files: once with its own reader
# and once with a DXT5 reader, so the two can be compared directly.
import struct, sys
import numpy as np


def _pal(c0, c1, three):
    def rgb(c):
        return ((((c >> 11) & 31) * 255 + 15) // 31,
                (((c >> 5) & 63) * 255 + 31) // 63,
                ((c & 31) * 255 + 15) // 31)
    a, d = rgb(c0), rgb(c1)
    if three and c0 <= c1:
        return [a, d, tuple((a[i] + d[i]) // 2 for i in range(3)), (0, 0, 0)]
    return [a, d,
            tuple((2 * a[i] + d[i]) // 3 for i in range(3)),
            tuple((a[i] + 2 * d[i]) // 3 for i in range(3))]


def decode(path, stride):
    """stride 8 = read as DXT1 (V9c's own reader); 16 = read as DXT5."""
    b = open(path, 'rb').read()
    h, w = struct.unpack_from('<II', b, 12)
    off = 148 if b[84:88] == b'DX10' else 128
    out = np.zeros((h, w, 3), np.int16)
    bw, bh = w // 4, h // 4
    cof = 8 if stride == 16 else 0
    for by in range(bh):
        for bx in range(bw):
            o = off + (by * bw + bx) * stride + cof
            c0, c1 = struct.unpack_from('<HH', b, o)
            bits = struct.unpack_from('<I', b, o + 4)[0]
            pal = _pal(c0, c1, stride == 8)
            for k in range(16):
                out[by * 4 + k // 4, bx * 4 + k % 4] = pal[(bits >> (2 * k)) & 3]
    return out


def mad(a, b):
    return float(np.abs(a.astype(np.int32) - b.astype(np.int32)).max(axis=-1).mean())


def run(d, stride, label):
    W = decode(d + '/Commonwealth.4.-24.24_msn.DDS', stride)
    E = decode(d + '/Commonwealth.4.-20.24_msn.DDS', stride)
    S = W
    N = decode(d + '/Commonwealth.4.-24.28_msn.DDS', stride)
    w = W.shape[1]
    seam_x = mad(W[:, w - 1], E[:, 0])
    ctl_x = sum(mad(t[:, x], t[:, x + 1]) for t in (W, E) for x in (100, 200, 300, 400)) / 8.0
    edge_x = (mad(W[:, w - 2], W[:, w - 1]) + mad(E[:, 0], E[:, 1])) / 2.0
    seam_y = mad(S[0], N[w - 1])
    ctl_y = sum(mad(t[y], t[y + 1]) for t in (S, N) for y in (100, 200, 300, 400)) / 8.0
    edge_y = (mad(S[0], S[1]) + mad(N[w - 2], N[w - 1])) / 2.0
    print('%-30s E/W seam %8.3f interior %7.3f ratio %6.2f  (edge %7.3f)'
          % (label, seam_x, ctl_x, seam_x / ctl_x, edge_x))
    print('%-30s N/S seam %8.3f interior %7.3f ratio %6.2f  (edge %7.3f)'
          % ('', seam_y, ctl_y, seam_y / ctl_y, edge_y))
    bad = []
    if not (1.20 <= ctl_x <= 2.20) or not (1.20 <= ctl_y <= 2.20):
        bad.append('interior control outside 1.20..2.20')
    if seam_x / ctl_x > 3.20:
        bad.append('E/W ratio over 3.20')
    if seam_y / ctl_y > 3.30:
        bad.append('N/S ratio over 3.30')
    if max(edge_x, edge_y) > 2.60:
        bad.append('edge step over 2.60')
    print('%-30s verdict: %s' % ('', 'PASS' if not bad else 'FAIL - ' + '; '.join(bad)))
    return not bad


def main():
    G = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vt1_20260912/gate'
    VAN = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
    print("V9c's own statistic, on the SAME files, under two readers")
    print()
    run(G + '/new.d/tex', 8, 'ours, read as DXT1 (V9c)')
    run(G + '/new.d/tex', 16, 'ours, read as DXT5 (right)')
    print()
    print('the same two readers on VANILLA\'s own shipped _msn sheets:')
    run(VAN, 8, 'vanilla, read as DXT1')
    run(VAN, 16, 'vanilla, read as DXT5')
    print()
    print('CONTROL: the bars were pinned 2026-09-09 at interior 1.803/1.797 E/W and')
    print('1.603/1.606 N/S. A reader that is right about the format should land in')
    print('that neighbourhood; a reader that is wrong should not.')
    return 0


sys.exit(main())
