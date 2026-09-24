# VT1 step 3b: the containment gate.
#
# FLOOR    every mip-0 texel that actually differs between the assembled sheet
#          and the direct bake lies inside the predicted set (the texels whose
#          macro slope differs because the two ring grids disagree).
# REFUTER  the predicted set must not be the whole sheet, or containment is
#          vacuous; and a deliberately SHIFTED prediction must fail the floor.
import sys, struct, numpy as np

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
V9A = ROOT + '/scratchpad/defaults1_20260912/v9a'
W = ROOT + '/scratchpad/vt1_20260912'

def bc1_mip0(path):
    b = open(path, 'rb').read()
    h, w = struct.unpack_from('<II', b, 12)
    out = np.zeros((h, w, 3), np.int32)
    bw, bh = w // 4, h // 4
    for by in range(bh):
        for bx in range(bw):
            o = 128 + (by * bw + bx) * 8
            c0, c1 = struct.unpack_from('<HH', b, o)
            bits = struct.unpack_from('<I', b, o + 4)[0]
            def rgb(c):
                return ((((c >> 11) & 31) * 255 + 15) // 31,
                        (((c >> 5) & 63) * 255 + 31) // 63,
                        ((c & 31) * 255 + 15) // 31)
            a, d = rgb(c0), rgb(c1)
            if c0 > c1:
                pal = [a, d, tuple((2 * a[i] + d[i]) // 3 for i in range(3)),
                       tuple((a[i] + 2 * d[i]) // 3 for i in range(3))]
            else:
                pal = [a, d, tuple((a[i] + d[i]) // 2 for i in range(3)), (0, 0, 0)]
            for k in range(16):
                out[by * 4 + k // 4, bx * 4 + k % 4] = pal[(bits >> (2 * k)) & 3]
    return out

def main():
    fails = 0; checks = 0
    for (cx, cy) in [(-24, 28), (-20, 28), (-24, 24), (-20, 24)]:
        tag = 'Commonwealth.4.%d.%d' % (cx, cy)
        pv = bc1_mip0('%s/default.vt/tex/%s.DDS' % (V9A, tag))
        pd = bc1_mip0('%s/default.d/tex/%s.DDS' % (V9A, tag))
        actual = (pv != pd).any(axis=2)
        try:
            pred = np.load('%s/predict_%d_%d.npy' % (W, cx, cy))
        except IOError:
            print('%s: no prediction on disk, skipped' % tag); continue
        na, npd = int(actual.sum()), int(pred.sum())
        outside = int((actual & ~pred).sum())
        checks += 1
        print('%-24s actual differing texels %4d   predicted set %5d   outside prediction %d'
              % (tag, na, npd, outside))
        if outside:
            fails += 1
            ys, xs = np.nonzero(actual & ~pred)
            print('    FAIL first outside: %s' % list(zip(xs[:8].tolist(), ys[:8].tolist())))
        # refuter 1: the prediction must be a small part of the sheet
        checks += 1
        if npd >= actual.size // 4:
            fails += 1
            print('    FAIL the prediction covers %.1f%% of the sheet - containment is vacuous'
                  % (100.0 * npd / actual.size))
        # refuter 2: the SAME prediction shifted 8 texels east must stop containing
        if na:
            checks += 1
            shifted = np.roll(pred, 8, axis=1)
            if int((actual & ~shifted).sum()) == 0:
                fails += 1
                print('    FAIL a prediction shifted by 8 texels still contains it - the bar cannot fail')
            else:
                print('    refuter ok: shifted 8 texels east, %d texels fall outside'
                      % int((actual & ~shifted).sum()))
    print('\n%d checks, %d failures' % (checks, fails))
    return 1 if fails else 0

sys.exit(main())
