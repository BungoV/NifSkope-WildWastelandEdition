# VT1 step 3: from the height-grid disagreement, PREDICT the texels whose
# colour can differ, and check the texels that actually differ are inside it.
#
# The land lookup's warp amplitude under the ruled default (FLATWARP, strength 1)
# is amp * (1 - min(1, |macro slope| / slopeRef)), and the macro slope is a Sobel
# over the RING HEIGHT GRID at +-512 world units.  A texel can only differ
# between the two paths where that Sobel reads a sample the two grids disagree on.
import struct, sys, numpy as np

LAND = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/build4_20260910/land.bin'
RC = 1
GUIDE_SCALE = 1024.0

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vt1_20260912')
from ringfix_model import Land, fill_ring, parent_inner_box, floor_to

def ease(t):
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0)

def height_at(g, hn, gx, gy):
    cx = min(max(gx, 0.0), hn - 1 - 0.001)
    cy = min(max(gy, 0.0), hn - 1 - 0.001)
    ix, iy = int(cx), int(cy)
    tx, ty = ease(cx - ix), ease(cy - iy)
    h00 = g[iy, ix]; h10 = g[iy, ix + 1]; h01 = g[iy + 1, ix]; h11 = g[iy + 1, ix + 1]
    return (h00 * (1 - tx) + h10 * tx) * (1 - ty) + (h01 * (1 - tx) + h11 * tx) * ty

def macro_grad(g, hn, offx, offy, wx, wy):
    s = GUIDE_SCALE * 0.5
    gs = s / 128.0
    cx = wx / 128.0 + offx
    cy = wy / 128.0 + offy
    h = [[height_at(g, hn, cx + i * gs, cy + j * gs) for i in (-1, 0, 1)] for j in (-1, 0, 1)]
    gx = (h[0][2] + 2 * h[1][2] + h[2][2]) - (h[0][0] + 2 * h[1][0] + h[2][0])
    gy = (h[2][0] + 2 * h[2][1] + h[2][2]) - (h[0][0] + 2 * h[0][1] + h[0][2])
    return gx / (8 * s), gy / (8 * s)

def main():
    cx0, cy0, D = int(sys.argv[1]), int(sys.argv[2]), 4
    L = Land(LAND)
    RES = 512
    d = D // 2
    content = RES // 2
    upt = d * 4096.0 / content                # 32 world units a texel

    gC, hnC, rxC, ryC = fill_ring(L, cx0, cy0, D)
    offCx = -(rxC * 4096.0) / 128.0
    offCy = -(ryC * 4096.0) / 128.0

    diff = np.zeros((RES, RES), bool)
    for by in range(2):
        for bx in range(2):
            tx0, ty0 = cx0 + bx * d, cy0 + by * d
            rdimT = d + 2 * RC; hnT = rdimT * 32 + 1
            gT, _, rxT, ryT = fill_ring(L, tx0, ty0, d)
            offTx = -(rxT * 4096.0) / 128.0
            offTy = -(ryT * 4096.0) / 128.0
            tileW = tx0 * 4096.0
            tileN = (ty0 + d) * 4096.0
            for j in range(content):
                wy = tileN - (j + 0.5) * upt
                for i in range(content):
                    wx = tileW + (i + 0.5) * upt
                    a = macro_grad(gC, hnC, offCx, offCy, wx, wy)
                    b = macro_grad(gT, hnT, offTx, offTy, wx, wy)
                    if a != b:
                        # assembled sheet row: by == 0 is the NORTH (rTop) block
                        Y = (1 - by) * content + j
                        X = bx * content + i
                        diff[Y, X] = True
    n = int(diff.sum())
    ys, xs = np.nonzero(diff)
    print('chunk Commonwealth.%d.%d.%d' % (D, cx0, cy0))
    print('  texels whose macro slope differs between the two paths: %d of %d' % (n, RES * RES))
    if n:
        print('  bounding box  x %d..%d   y %d..%d' % (xs.min(), xs.max(), ys.min(), ys.max()))
    np.save('E:/Projects/NifskopeWildWastelandEdition/scratchpad/vt1_20260912/predict_%d_%d.npy'
            % (cx0, cy0), diff)

main()
