#!/usr/bin/env python3
"""CELLVIEW4 -- A SIMULATION. NOT THE VIEWER, NOT A GATE.

Builds two top-down pictures of one cell's painted ground from the plugin's own
LAND record and the loose diffuse textures:

  * `*_mosaic.png`  -- TODAY's rule, reproduced exactly: every 128-unit quad
                       takes ONE texture, chosen at its own SW corner by
                       src/cellground.cpp's layer rule (with CELLVIEW3's two
                       repairs).  Flat per quad, hard edges.
  * `*_blended.png` -- the ENGINE's rule: the quadrant's BTXT, then every ATXT
                       layer composited over it in paint order with its own
                       per-vertex opacity bilinearly interpolated.  This is the
                       TARGET the build lane must match.
  * `*_diff.png`    -- |blended - mosaic| per pixel, and the per-quad mean
                       colour difference table the gate row is pinned on.

Nothing here runs in NifSkope and nothing here is evidence about the viewer.
It is a known-answer: the C++ splat builder must reproduce the BLENDED image.

Usage:
  python splat_sim.py <Fallout4.esm> <texture root> <cx> <cy> <outdir> [px]
"""
import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from cell_census import Esm, Rec, fields, decompress          # noqa: E402
from impostor_bc_decode import load_dds                        # noqa: E402

QUAD_GRID = 17
LAND_GRID = 33
CELL_UNITS = 4096.0
TILING = 341.3333          # fLandTextureTilingMult, src/cellground.h
LAYER_MIN = 0.5            # CELL_GROUND_LAYER_MIN


# --------------------------------------------------------------------------
# the plugin
# --------------------------------------------------------------------------
def it_of(esm, rec):
    if rec.flags & 0x00040000:
        data = decompress(esm.buf, rec)
        return fields(data, Rec(rec.type, len(data), 0, rec.form, 0))
    return fields(esm.buf, rec)


def scan(esm, world_edid, want):
    """-> (lands, ltex_to_txst, txst_to_tx00)

    `lands[(cx,cy)] = (base[4], layers[4])` where a layer is
    (ltex, layer_index, opacity 17x17).  The LAYER INDEX is the 16-bit field at
    ATXT offset 6 that src/esmdata.cpp does NOT read -- it is the engine's PAINT
    ORDER, and a blend needs it.
    """
    buf = esm.buf
    target = world_edid.encode('cp1252')
    wform = [None]
    ltex, txst = {}, {}

    def pass1(rec, path):
        if rec is None:
            return
        if rec.type == b'WRLD':
            for t, p in fields(buf, rec):
                if t == b'EDID' and p.split(b'\0')[0] == target:
                    wform[0] = rec.form
        elif rec.type == b'LTEX':
            for t, p in it_of(esm, rec):
                if t == b'TNAM' and len(p) >= 4:
                    ltex[rec.form] = struct.unpack_from('<I', p, 0)[0]
        elif rec.type == b'TXST':
            for t, p in it_of(esm, rec):
                if t == b'TX00':
                    txst[rec.form] = p.split(b'\0')[0].decode('cp1252')

    esm.walk(24 + struct.unpack_from('<I', buf, 4)[0], len(buf), pass1)

    cellxy, lands = {}, {}

    def pass2(rec, path):
        under, parent = False, None
        for label, gtype, goff in path:
            if gtype == 1 and struct.unpack_from('<I', label, 0)[0] == wform[0]:
                under = True
            if gtype in (8, 9, 10):
                parent = struct.unpack_from('<I', label, 0)[0]
        if not under or rec is None:
            return
        if rec.type == b'CELL':
            for t, p in it_of(esm, rec):
                if t == b'XCLC' and len(p) >= 8:
                    cellxy[rec.form] = struct.unpack_from('<ii', p, 0)
        elif rec.type == b'LAND' and parent is not None:
            base = [0, 0, 0, 0]
            layers = [[], [], [], []]
            heights = np.zeros((33, 33))
            pending = -1
            for t, p in it_of(esm, rec):
                if t == b'BTXT' and len(p) >= 8:
                    lt, q = struct.unpack_from('<IB', p, 0)
                    if 0 <= q < 4:
                        base[q] = lt
                elif t == b'ATXT' and len(p) >= 8:
                    lt, q, _unk, lidx = struct.unpack_from('<IBBh', p, 0)
                    if 0 <= q < 4:
                        layers[q].append([lt, lidx, np.zeros((17, 17))])
                        pending = q
                    else:
                        pending = -1
                elif t == b'VTXT' and pending >= 0 and layers[pending]:
                    grid = layers[pending][-1][2]
                    for e in range(len(p) // 8):
                        posn, _u, op = struct.unpack_from('<HHf', p, 8 * e)
                        if posn <= 288:
                            grid[posn // 17][posn % 17] = op
                    pending = -1
                elif t == b'VHGT' and len(p) >= 4 + 33 * 33:
                    b0 = struct.unpack_from('<f', p, 0)[0]
                    d = np.frombuffer(p[4:4 + 33 * 33], np.int8).reshape(33, 33)
                    rs = b0
                    for row in range(33):
                        rs += float(d[row][0])
                        acc = rs
                        heights[row][0] = acc * 8.0
                        for col in range(1, 33):
                            acc += float(d[row][col])
                            heights[row][col] = acc * 8.0
            lands[parent] = (base, layers, heights)

    esm.walk(24 + struct.unpack_from('<I', buf, 4)[0], len(buf), pass2)
    out = {}
    for f, xy in cellxy.items():
        if (want is None or xy in want) and f in lands:
            out[xy] = lands[f]
    return out, ltex, txst


def diffuse_of(form, ltex, txst):
    t = ltex.get(form)
    if not t:
        return None
    return txst.get(t)


# --------------------------------------------------------------------------
# textures
# --------------------------------------------------------------------------
_cache = {}


def texture(root, rel):
    if rel in _cache:
        return _cache[rel]
    img = None
    if rel:
        p = os.path.join(root, rel.replace('\\', os.sep))
        if not os.path.exists(p):
            # the corpus is case-mangled in places
            d, n = os.path.split(p)
            if os.path.isdir(d):
                for f in os.listdir(d):
                    if f.lower() == n.lower():
                        p = os.path.join(d, f)
                        break
        if os.path.exists(p):
            try:
                img = load_dds(p)[0][..., :3].astype(np.float32)
            except Exception as e:                       # noqa: BLE001
                print('  ! %s: %s' % (rel, e))
    _cache[rel] = img
    return img


def sample(img, u, v):
    """Nearest-texel wrap sample.  u,v in texture repeats."""
    if img is None:
        return None
    h, w = img.shape[:2]
    xi = np.mod((u * w).astype(np.int64), w)
    yi = np.mod((v * h).astype(np.int64), h)
    return img[yi, xi]


# --------------------------------------------------------------------------
# the two rules
# --------------------------------------------------------------------------
def quadrant_of(row, col):
    top = 1 if row >= QUAD_GRID - 1 else 0
    right = 1 if col >= QUAD_GRID - 1 else 0
    q = top * 2 + right
    lrow = min(max(row - top * (QUAD_GRID - 1), 0), QUAD_GRID - 1)
    lcol = min(max(col - right * (QUAD_GRID - 1), 0), QUAD_GRID - 1)
    return q, lrow, lcol


def bilinear(grid, fy, fx):
    """grid 17x17, fy/fx float arrays in 0..16."""
    y0 = np.clip(np.floor(fy).astype(np.int64), 0, 16)
    x0 = np.clip(np.floor(fx).astype(np.int64), 0, 16)
    y1 = np.clip(y0 + 1, 0, 16)
    x1 = np.clip(x0 + 1, 0, 16)
    ty = fy - y0
    tx = fx - x0
    a = grid[y0, x0] * (1 - tx) + grid[y0, x1] * tx
    b = grid[y1, x0] * (1 - tx) + grid[y1, x1] * tx
    return a * (1 - ty) + b * ty


def render(base, layers, root, ltex, txst, px, blended):
    """px x px RGB of one cell, +Y up in the image (row 0 = north)."""
    out = np.zeros((px, px, 3), np.float32)
    per = px // 2                       # pixels across one quadrant
    # world coords of every pixel centre, cell-local
    for q in range(4):
        qy, qx = q // 2, q % 2
        y0p, x0p = qy * per, qx * per
        jj, ii = np.meshgrid(np.arange(per), np.arange(per), indexing='ij')
        # cell-local world position of the pixel centre
        wx = (x0p + ii + 0.5) * (CELL_UNITS / px)
        wy = (y0p + jj + 0.5) * (CELL_UNITS / px)
        u = wx / TILING
        v = -wy / TILING
        # local 0..16 grid coords inside the quadrant
        fx = (wx - qx * CELL_UNITS / 2.0) / (CELL_UNITS / 32.0)
        fy = (wy - qy * CELL_UNITS / 2.0) / (CELL_UNITS / 32.0)

        acc = np.zeros((per, per, 3), np.float32)
        have = np.zeros((per, per), bool)
        bimg = texture(root, diffuse_of(base[q], ltex, txst)) if base[q] else None
        if bimg is not None:
            acc[:] = sample(bimg, u, v)
            have[:] = True

        ordered = sorted(layers[q], key=lambda L: L[1])
        if blended:
            for lt, _li, grid in ordered:
                img = texture(root, diffuse_of(lt, ltex, txst)) if lt else None
                if img is None:
                    continue
                a = np.clip(bilinear(grid, fy, fx), 0.0, 1.0)[..., None]
                col = sample(img, u, v)
                # where nothing is under it yet, the layer IS the ground
                first = ~have & (a[..., 0] > 0.0)
                acc = np.where(first[..., None], col, acc * (1 - a) + col * a)
                have |= a[..., 0] > 0.0
        else:
            # TODAY: one texture per 128-unit quad, chosen at its SW corner
            for lr in range(16):
                for lc in range(16):
                    chosen = base[q]
                    best = LAYER_MIN if base[q] else 0.0
                    for lt, _li, grid in ordered:
                        o = grid[lr][lc]
                        if lt and o >= best and o > 0.0:
                            best, chosen = o, lt
                    img = texture(root, diffuse_of(chosen, ltex, txst)) if chosen else None
                    sy = slice(lr * (per // 16), (lr + 1) * (per // 16))
                    sx = slice(lc * (per // 16), (lc + 1) * (per // 16))
                    if img is None:
                        acc[sy, sx] = 0.0
                    else:
                        acc[sy, sx] = sample(img, u[sy, sx], v[sy, sx])
                        have[sy, sx] = True
        out[y0p:y0p + per, x0p:x0p + per] = acc
    return out


def png(path, arr):
    import zlib
    a = np.clip(arr * 255.0 + 0.5, 0, 255).astype(np.uint8)
    h, w = a.shape[:2]
    raw = b''.join(b'\0' + a[y].tobytes() for y in range(h - 1, -1, -1))  # flip: +Y up

    def chunk(t, d):
        c = t + d
        return struct.pack('>I', len(d)) + c + struct.pack('>I', zlib.crc32(c))
    hdr = struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', hdr)
                           + chunk(b'IDAT', zlib.compress(raw, 9))
                           + chunk(b'IEND', b''))


def main():
    esmp, root, cx, cy, outdir = (sys.argv[1], sys.argv[2], int(sys.argv[3]),
                                  int(sys.argv[4]), sys.argv[5])
    px = int(sys.argv[6]) if len(sys.argv) > 6 else 512
    os.makedirs(outdir, exist_ok=True)
    esm = Esm(esmp)
    lands, ltex, txst = scan(esm, 'Commonwealth', {(cx, cy)})
    if (cx, cy) not in lands:
        print('cell %d,%d has no LAND' % (cx, cy))
        return 2
    base, layers, _h = lands[(cx, cy)]

    print('=== cell %d,%d: the record ===' % (cx, cy))
    for q in range(4):
        d = diffuse_of(base[q], ltex, txst)
        print('  quadrant %d  BTXT %s  %s' % (
            q, ('%08X' % base[q]) if base[q] else 'NONE',
            d or '(no texture)'))
        for lt, li, g in sorted(layers[q], key=lambda L: L[1]):
            nz = int((g > 0).sum())
            print('     layer %2d  LTEX %08X  %-46s  nonzero %3d/289  max %.3f'
                  % (li, lt, (diffuse_of(lt, ltex, txst) or '(no texture)'), nz,
                     float(g.max())))

    mos = render(base, layers, root, ltex, txst, px, blended=False)
    bld = render(base, layers, root, ltex, txst, px, blended=True)
    tag = '%s%d_%d' % ('m' if cx < 0 else '', abs(cx), cy)
    png(os.path.join(outdir, 'sim_%s_mosaic.png' % tag), mos)
    png(os.path.join(outdir, 'sim_%s_blended.png' % tag), bld)
    d = np.abs(bld - mos)
    png(os.path.join(outdir, 'sim_%s_diff.png' % tag), d)

    # the gate's known answer: mean |blended - mosaic| per 128-unit quad
    n = px // 32
    pq = d.reshape(32, n, 32, n, 3).mean(axis=(1, 3, 4))
    print('\n=== SIMULATION: mean colour difference, blended vs mosaic ===')
    print('  over the whole cell            %.4f' % float(d.mean()))
    print('  worst 128-unit quad            %.4f' % float(pq.max()))
    print('  quads differing by > 0.02      %d of 1024' % int((pq > 0.02).sum()))
    print('  quads differing by > 0.10      %d of 1024' % int((pq > 0.10).sum()))
    print('  quads identical (< 0.001)      %d of 1024' % int((pq < 0.001).sum()))
    np.save(os.path.join(outdir, 'sim_%s_perquad.npy' % tag), pq)
    return 0


if __name__ == '__main__':
    sys.exit(main())
