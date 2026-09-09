#!/usr/bin/env python
"""What the VT tile baker's two defects cost, measured OFFLINE.

The heights come from a written .lodt (the FO4 path stores VHGT exactly, one
word a sample at quantum 8), so this needs no build and no ESM parse. Three
sheets are then produced on the SAME grid the chunk baker uses:

  OLD   the pyramid path as it stood: `int( ngx )` nearest, north in green;
  NEW   the shared eased-bilinear reconstruction, up in green;
  VAN   vanilla's own shipped _msn for the same tile, decoded.

and three numbers are reported per sheet:

  * the grid-phase roughness of lane LATTICE -- on the slope field minus its
    own 9x9 box mean, average |second difference| over the columns of each
    residue class of x mod 4, report (max - min) / mean. It reads ~0 on a
    smooth field and high on one creased at a fixed period;
  * the left-neighbour difference fractions by x mod 4 (2026-09-07's
    block-flatness discriminator);
  * mean Lambert light and its standard deviation under one sun, which is what
    the channel order costs.

KNOWN-ANSWER CONTROLS, printed first: the roughness of a smooth analytic field
(must read ~0) and of the same field creased every fourth column (must read
high). A metric that cannot separate those is not measuring the artefact.

usage: vt_msn_sim.py <Commonwealth.lodt> <cellX> <cellY> [vanilla_msn.dds]
"""
import struct, sys
import numpy as np

sys.path.insert(0, __file__.rsplit('\\', 1)[0].rsplit('/', 1)[0])
from lodt_vs_heightmap import read_lodt, height_grid  # noqa: E402

RES = 512
DIM = 4


def ease(t):
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0)


def height_at_eased(h, hn, gx, gy):
    """The shipped lodgenTerrainHeightAt, vectorised."""
    cx = np.clip(gx, 0.0, hn - 1 - 0.001)
    cy = np.clip(gy, 0.0, hn - 1 - 0.001)
    ix = cx.astype(np.int32)
    iy = cy.astype(np.int32)
    tx = ease(cx - ix)
    ty = ease(cy - iy)
    h00 = h[iy, ix]
    h10 = h[iy, ix + 1]
    h01 = h[iy + 1, ix]
    h11 = h[iy + 1, ix + 1]
    return (h00 * (1 - tx) + h10 * tx) * (1 - ty) + (h01 * (1 - tx) + h11 * tx) * ty


def normals_new(h, hn, ngx, ngy, spacing):
    dzdx = (height_at_eased(h, hn, ngx + 1.0, ngy)
            - height_at_eased(h, hn, ngx - 1.0, ngy)) / (2.0 * spacing)
    dzdy = (height_at_eased(h, hn, ngx, ngy + 1.0)
            - height_at_eased(h, hn, ngx, ngy - 1.0)) / (2.0 * spacing)
    return stack_unit(-dzdx, -dzdy)


def normals_old(h, hn, ngx, ngy, spacing):
    """`int( ngx )` -- NEAREST, exactly as lodgenBakeVtTile sampled."""
    hx = np.clip(ngx.astype(np.int32), 1, hn - 2)
    hy = np.clip(ngy.astype(np.int32), 1, hn - 2)
    dzdx = (h[hy, hx + 1] - h[hy, hx - 1]) / (2.0 * spacing)
    dzdy = (h[hy + 1, hx] - h[hy - 1, hx]) / (2.0 * spacing)
    return stack_unit(-dzdx, -dzdy)


def stack_unit(nx, ny):
    n = np.stack([nx, ny, np.ones_like(nx)], -1)
    return n / np.linalg.norm(n, axis=-1, keepdims=True)


def encode(n, up_in_green):
    """0.5+0.5 to bytes. up_in_green True = R east, G up, B north (Fallout 4);
    False = the conventional order the VT path wrote."""
    b = np.clip((n * 0.5 + 0.5) * 255.0 + 0.5, 0, 255).astype(np.uint8)
    if up_in_green:
        return np.stack([b[..., 0], b[..., 2], b[..., 1]], -1)
    return b                                     # R east, G north, B up


def decode(rgb, up_in_green):
    v = rgb.astype(np.float64) / 255.0 * 2.0 - 1.0
    if up_in_green:
        return np.stack([v[..., 0], v[..., 2], v[..., 1]], -1)   # -> east, north, up
    return v


def box_mean(a, k=9):
    p = k // 2
    b = np.pad(a, p, mode='edge')
    c = np.cumsum(np.cumsum(b, 0), 1)
    c = np.pad(c, ((1, 0), (1, 0)))
    h, w = a.shape
    return (c[k:k + h, k:k + w] - c[0:h, k:k + w] - c[k:k + h, 0:w] + c[0:h, 0:w]) / (k * k)


def grid_phase_roughness(slope):
    """Lane LATTICE's phase-conditional statistic, x axis."""
    r = slope - box_mean(slope)
    d2 = np.abs(r[:, :-2] - 2.0 * r[:, 1:-1] + r[:, 2:])     # centred on x = 1..w-2
    xs = np.arange(1, slope.shape[1] - 1)
    m = np.array([d2[:, xs % 4 == c].mean() for c in range(4)])
    return (m.max() - m.min()) / m.mean(), m


def left_diff(rgb):
    d = np.any(rgb[:, 1:] != rgb[:, :-1], axis=-1)
    xs = np.arange(1, rgb.shape[1])
    return [100.0 * d[:, xs % 4 == c].mean() for c in range(4)]


def light(n):
    """One sun, 45 degrees up from the north-east, plus the ambient the
    2026-09-07 measurement used."""
    s = np.array([0.5, 0.5, 0.7071])
    s = s / np.linalg.norm(s)
    lam = np.clip(n @ s, 0, None)
    return 0.25 + 0.75 * lam


def dds_rgb(path):
    b = open(path, 'rb').read()
    h, w = struct.unpack_from('<II', b, 12)
    fcc = b[84:88]
    bs = 8 if fcc == b'DXT1' else 16
    col = 0 if bs == 8 else 8
    off = 148 if fcc == b'DX10' else 128
    bw, bh = (w + 3) // 4, (h + 3) // 4
    out = np.zeros((h, w, 3), np.uint8)
    for by in range(bh):
        for bx in range(bw):
            o = off + (by * bw + bx) * bs + col
            c0, c1 = struct.unpack_from('<HH', b, o)
            bits = struct.unpack_from('<I', b, o + 4)[0]

            def rgb(c):
                return (((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63,
                        (c & 31) * 255 // 31)
            e0, e1 = rgb(c0), rgb(c1)
            if c0 > c1 or bs == 16:
                t = [e0, e1, tuple((2 * e0[k] + e1[k]) // 3 for k in range(3)),
                     tuple((e0[k] + 2 * e1[k]) // 3 for k in range(3))]
            else:
                t = [e0, e1, tuple((e0[k] + e1[k]) // 2 for k in range(3)), (0, 0, 0)]
            for k in range(16):
                y, x = by * 4 + k // 4, bx * 4 + k % 4
                if y < h and x < w:
                    out[y, x] = t[(bits >> (2 * k)) & 3]
    return out


def controls():
    print('KNOWN-ANSWER CONTROLS')
    y, x = np.mgrid[0:256, 0:256]
    smooth = np.sin(x / 37.0) * np.cos(y / 41.0)
    r, m = grid_phase_roughness(smooth)
    print('  smooth analytic field          roughness %.4f   (must be ~0)' % r)
    creased = smooth.copy()
    creased[:, ::4] += 0.02
    r2, _ = grid_phase_roughness(creased)
    print('  the same field creased every 4 roughness %.4f   (must be high)' % r2)
    print('  separation %.1fx' % (r2 / max(r, 1e-9)))
    print('')


def main():
    lodt, cx, cy = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    van = sys.argv[4] if len(sys.argv) > 4 else None
    controls()
    f = read_lodt(lodt)
    H = height_grid(f)
    hn = DIM * 32 + 1
    x0 = (cx - f['minX']) * 32
    y0 = (cy - f['minY']) * 32
    h = (H[y0:y0 + hn, x0:x0 + hn].astype(np.float64) - 32767.0) * f['quant']
    print('tile %s.%d.%d.%d  heights %.0f..%.0f from %s'
          % ('Commonwealth', DIM, cx, cy, h.min(), h.max(), lodt))

    span = DIM * 4096.0
    spacing = span / (hn - 1)
    px = np.arange(RES) + 0.5
    ngx = np.tile(px / RES * (hn - 1), (RES, 1))
    ngy = np.tile(((1.0 - (np.arange(RES) + 0.5) / RES) * (hn - 1))[:, None], (1, RES))

    rows = []
    for name, n, green in (('OLD  nearest, up in blue', normals_old(h, hn, ngx, ngy, spacing), False),
                           ('NEW  eased bilinear, up in green', normals_new(h, hn, ngx, ngy, spacing), True)):
        rgb = encode(n, green)
        # the ROUGHNESS is read with the writer's own convention, so it
        # isolates the SAMPLING defect from the channel one
        slope = decode(rgb, green)
        slope = slope[..., 0] / np.maximum(slope[..., 2], 1e-6)
        r, m = grid_phase_roughness(slope)
        # the LIGHT is read the way the CONSUMER reads it, which is fixed:
        # Fallout 4's terrain shader takes up from GREEN. That is what a sheet
        # written in the conventional order costs, and it is the whole point.
        asRead = decode(rgb, True)
        L = light(asRead)
        rows.append((name, r, m, left_diff(rgb), L.mean(), L.std(),
                     asRead[..., 2].mean()))
    if van:
        rgb = dds_rgb(van)
        d = decode(rgb, True)
        slope = d[..., 0] / np.maximum(d[..., 2], 1e-6)
        r, m = grid_phase_roughness(slope)
        L = light(d)
        rows.append(('VAN  vanilla shipped (BC3)', r, m, left_diff(rgb), L.mean(),
                     L.std(), d[..., 2].mean()))

    print('%-34s %9s  %-24s %7s %7s %7s'
          % ('', 'roughness', 'left-diff by x mod 4 (%)', 'light', 'lightSD', 'meanUp'))
    for name, r, m, ld, lm, ls, up in rows:
        print('%-34s %9.3f  %-24s %7.4f %7.4f %7.4f'
              % (name, r, '/'.join('%.0f' % v for v in ld), lm, ls, up))
    print('')
    print('per-residue-class mean |second difference|:')
    for name, r, m, ld, lm, ls, up in rows:
        print('  %-32s %s' % (name, ' '.join('%.5f' % v for v in m)))
    o, n = rows[0], rows[1]
    print('')
    print('the fix, on this tile: roughness %.3f -> %.3f (%.0f%%), '
          'light %.4f -> %.4f (%+.1f%%), mean UP as the shader reads it '
          '%.4f -> %.4f' % (o[1], n[1], 100.0 * (n[1] - o[1]) / o[1],
                            o[4], n[4], 100.0 * (n[4] - o[4]) / o[4], o[6], n[6]))


main()
