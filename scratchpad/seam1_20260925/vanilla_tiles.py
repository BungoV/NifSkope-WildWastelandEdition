"""Bethesda's shipped terrain LOD diffuse, read in place (never copied): Textures/Terrain/<World>/<World>.<dim>.<x>.<y>.DDS
under the vanilla LOD root; <x>,<y> = the SW cell of a dim-cell chunk, the chunk covers cells x..x+dim-1, y..y+dim-1,
world rect [x*4096, (x+dim)*4096) x [y*4096, (y+dim)*4096); image row 0 = NORTH (checked below on the texels)."""
import struct, numpy as np, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vtread
ROOT = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/'
def read_dds(p):
    b = open(p, 'rb').read()
    h, w = struct.unpack_from('<II', b, 12); fourcc = b[84:88]; hdr = 128
    if fourcc == b'DX10': hdr += 20; fmt = struct.unpack_from('<I', b, 128)[0]
    else: fmt = {b'DXT1': 71, b'DXT5': 77}.get(fourcc)
    assert fmt in (71, 72, 77, 78), (p, fourcc, fmt)
    return vtread.decode(b, hdr, w, fmt)[:h, :w]
def tile(world, dim, x, y):
    p = '%s%s/%s.%d.%d.%d.DDS' % (ROOT, world, world, dim, x, y)
    return read_dds(p) if os.path.exists(p) else None
def mosaic(world, dim, x0, y0, x1, y1):
    """chunks with SW cell in x0..x1, y0..y1 (multiples of dim on the -96 grid); row 0 = north"""
    xs = list(range(x0, x1 + 1, dim)); ys = list(range(y1, y0 - 1, -dim))
    t0 = tile(world, dim, xs[0], ys[0]); n = t0.shape[0]
    out = np.zeros((len(ys) * n, len(xs) * n, 4), np.uint8)
    for j, y in enumerate(ys):
        for i, x in enumerate(xs):
            t = tile(world, dim, x, y)
            if t is not None: out[j * n:(j + 1) * n, i * n:(i + 1) * n] = t
    return out, n
if __name__ == '__main__':
    m, n = mosaic('Commonwealth', 4, -24, 16, -16, 24)
    print('vanilla dim-4 tile side', n, 'texels -> upt', 4 * 4096 / n)
    from PIL import Image; Image.fromarray(m[..., :3]).save('pics/vanilla_dim4_sanctuary.png')
    lum = m[..., :3].astype(np.float32) @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    cpc = n // 4
    cm = lum.reshape(lum.shape[0] // cpc, cpc, lum.shape[1] // cpc, cpc).mean((1, 3))
    print('vanilla cell mean lum, rows north->south from 27, cols from -24')
    for r in range(cm.shape[0]): print('%4d ' % (27 - r) + ' '.join('%5.0f' % v for v in cm[r]))
    # steps at the chunk borders x=-20,-16 over rows 20..23 ; y=24,20 over cols -20..-17
    r0 = (27 - 23) * cpc; r1 = (27 - 19) * cpc; c0 = 4 * cpc; c1 = 8 * cpc
    colp = lum[r0:r1].mean(0); rowp = lum[:, c0:c1].mean(1)
    for name, prof, bnds in (('x', colp, (4 * cpc, 8 * cpc)), ('y', rowp, (4 * cpc, 8 * cpc))):
        d = np.abs(np.diff(prof))
        print(name, 'step at chunk borders', [round(float(d[b - 1]), 2) for b in bnds], ' median |d| elsewhere', round(float(np.median(d)), 2), ' p99', round(float(np.percentile(d, 99)), 2))
