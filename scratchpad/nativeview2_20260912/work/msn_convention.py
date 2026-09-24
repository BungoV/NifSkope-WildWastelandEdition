#!/usr/bin/env python
"""Section 0 -- what channel order does a VANILLA FO4 terrain `_msn` sheet use?

Two independent producers are put side by side:

  * Bethesda's shipped `Data/Textures/Terrain/Commonwealth/Commonwealth.16.X.Y_msn.DDS`
    (512x512 DXT5, 16 cells, so 32 texels a cell = one texel per 128-unit LAND sample);
  * the heights of the SAME cells, read out of the native shadow heightmap
    `Commonwealth_fine.HeightMap.-96.-96.95.95.-8320.44872.dds` (R16_UNORM,
    6144x6144, pixel = height/8 + 32767, row 0 = NORTH) -- a transcription of
    vanilla's own LAND records by a different piece of code than Bethesda's
    LOD baker.

A heightfield's unit normal is  n ~ ( -dh/dx, -dh/dy, 1 ) normalised, with +x
EAST and +y NORTH.  So if a channel carries "east" its decoded value must
correlate positively with -dh/dx and near zero with -dh/dy.  Every one of the
six (channel, axis, sign) pairings is printed, so the wrong ones are the
refuters of the right one -- nothing is assumed, including which way the
texel rows run.

usage: python msn_convention.py [ntiles]
"""
import glob
import os
import struct
import sys

import numpy as np

VAN = r"E:/Tools/Fallout 4/DataUnpacked/Data/textures/terrain/Commonwealth"
HM = (r"E:/Projects/Fallout 4 Mods/mods/FO4CS/Textures/Terrain/Commonwealth/"
      r"Commonwealth_fine.HeightMap.-96.-96.95.95.-8320.44872.dds")
CELL0 = -96            # the heightmap's south-west cell, both axes
CELLS = 192            # cells a side
SPC = 32               # samples a cell
QUANT = 8.0            # world units a stored step
STEP = 128.0           # world units between samples


# ---------------------------------------------------------------- DDS decode
def dds_header(b):
    h = struct.unpack_from("<7I", b, 4)
    return h[3], h[2], b[84:88], max(1, h[6])   # w, h, fourcc, mips


def _c565(c):
    r = (c >> 11) & 31
    g = (c >> 5) & 63
    bl = c & 31
    return ((r * 255 + 15) // 31, (g * 255 + 31) // 63, (bl * 255 + 15) // 31)


def decode_bc(b, off, w, h, bc3):
    """mip 0 as uint8 [h][w][4] (RGBA). Re-typed from the format, shares no
    code with the writer under test."""
    out = np.zeros((h, w, 4), np.uint8)
    out[..., 3] = 255
    bw, bh = (w + 3) // 4, (h + 3) // 4
    stride = 16 if bc3 else 8
    p = off
    for by in range(bh):
        for bx in range(bw):
            blk = b[p:p + stride]
            p += stride
            if bc3:
                a0, a1 = blk[0], blk[1]
                abits = int.from_bytes(blk[2:8], "little")
                if a0 > a1:
                    at = [a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
                else:
                    at = [a0, a1] + [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
                cblk = blk[8:16]
            else:
                at = None
                cblk = blk
            c0, c1 = struct.unpack_from("<HH", cblk, 0)
            idx = struct.unpack_from("<I", cblk, 4)[0]
            p0, p1 = _c565(c0), _c565(c1)
            if c0 > c1 or bc3:
                pal = [p0, p1,
                       tuple((2 * p0[k] + p1[k]) // 3 for k in range(3)),
                       tuple((p0[k] + 2 * p1[k]) // 3 for k in range(3))]
            else:
                pal = [p0, p1,
                       tuple((p0[k] + p1[k]) // 2 for k in range(3)),
                       (0, 0, 0)]
            for j in range(4):
                y = by * 4 + j
                if y >= h:
                    continue
                for i in range(4):
                    x = bx * 4 + i
                    if x >= w:
                        continue
                    sel = (idx >> (2 * (j * 4 + i))) & 3
                    out[y, x, 0:3] = pal[sel]
                    if bc3:
                        out[y, x, 3] = at[(abits >> (3 * (j * 4 + i))) & 7]
    return out


def load_msn(path):
    b = open(path, "rb").read()
    w, h, cc, mips = dds_header(b)
    off = 148 if cc == b"DX10" else 128
    assert cc in (b"DXT1", b"DXT5"), cc
    return decode_bc(b, off, w, h, cc == b"DXT5"), w, h, cc.decode("latin1"), mips


# ------------------------------------------------------------------ heights
def load_heights():
    b = open(HM, "rb").read()
    w, h, cc, mips = dds_header(b)
    off = 148 if cc == b"DX10" else 128
    a = np.frombuffer(b, np.uint16, count=w * h, offset=off).reshape(h, w)
    return a, w, h


def tile_heights(H, cx, cy, n=512):
    """World heights for the n x n samples of the dim-16 tile whose south-west
    cell is (cx, cy), returned as [row][col] with row 0 = SOUTH, col 0 = WEST
    (i.e. +y up the array, +x across it)."""
    c0 = (cx - CELL0) * SPC
    # heightmap row 0 = north = world y of cell CELL0+CELLS
    rNorth = (CELL0 + CELLS - (cy + 16)) * SPC      # row of the tile's north edge
    sub = H[rNorth:rNorth + n, c0:c0 + n].astype(np.float64)
    sub = (sub - 32767.0) * QUANT
    return sub[::-1, :]                              # flip so row 0 = south


def corr(a, b):
    a = a.ravel() - a.mean()
    b = b.ravel() - b.mean()
    d = np.sqrt((a * a).sum() * (b * b).sum())
    return float((a * b).sum() / d) if d > 0 else 0.0


def main():
    ntiles = int(sys.argv[1]) if len(sys.argv) > 1 else 6
    H, hw, hh = load_heights()
    print("heightmap %dx%d  cells %d..%d  quantum %g  step %g"
          % (hw, hh, CELL0, CELL0 + CELLS - 1, QUANT, STEP))

    names = sorted(glob.glob(os.path.join(VAN, "Commonwealth.16.*_msn.DDS")))
    print("vanilla _msn tiles on disk: %d" % len(names))

    # ---- part 1: per-channel means over a spread of tiles
    picked, rows = [], []
    for p in names:
        base = os.path.basename(p)[:-len("_msn.DDS")]
        _, dim, sx, sy = base.split(".", 3)
        cx, cy = int(sx), int(sy)
        if not (-96 <= cx <= 79 and -96 <= cy <= 79):
            continue
        picked.append((p, cx, cy))
    step = max(1, len(picked) // ntiles)
    picked = picked[::step][:ntiles]

    print("\n--- per-channel means, vanilla sheets (0..255) ---")
    print("%-28s %7s %7s %7s %7s" % ("tile", "R", "G", "B", "A"))
    allm = []
    tiles = []
    for p, cx, cy in picked:
        img, w, h, cc, mips = load_msn(p)
        m = img.reshape(-1, 4).mean(axis=0)
        allm.append(m)
        tiles.append((p, cx, cy, img))
        print("%-28s %7.1f %7.1f %7.1f %7.1f   %s %dx%d mips %d"
              % (os.path.basename(p), m[0], m[1], m[2], m[3], cc, w, h, mips))
    am = np.array(allm).mean(axis=0)
    print("%-28s %7.1f %7.1f %7.1f %7.1f" % ("MEAN OF THE ABOVE", am[0], am[1], am[2], am[3]))

    # ---- part 2: the known answer -- which channel tracks which height gradient
    print("\n--- correlation against the heightfield of the SAME cells ---")
    print("n ~ ( -dh/dx, -dh/dy, 1 ), x EAST, y NORTH; per tile, 512x512 samples")
    hdr = ("%-22s %8s %8s %8s %8s %8s %8s"
           % ("tile", "R:-dh/dx", "R:-dh/dy", "B:-dh/dx", "B:-dh/dy", "G:nz", "A:nz"))
    print(hdr)
    tot = np.zeros(6)
    rowflip_tot = np.zeros(2)
    for p, cx, cy, img in tiles:
        Z = tile_heights(H, cx, cy)
        # central differences, world units per world unit
        dzdx = np.gradient(Z, STEP, axis=1)
        dzdy = np.gradient(Z, STEP, axis=0)
        nx = -dzdx
        ny = -dzdy
        nz = np.ones_like(Z)
        L = np.sqrt(nx * nx + ny * ny + nz * nz)
        nx, ny, nz = nx / L, ny / L, nz / L
        # texel rows: the sheet is stored row 0 = ? -- flip it so row 0 = south
        # to match Z, and print the unflipped control below.
        T = img[::-1, :, :].astype(np.float64) / 255.0 * 2.0 - 1.0
        r = corr(T[..., 0], nx), corr(T[..., 0], ny)
        bl = corr(T[..., 2], nx), corr(T[..., 2], ny)
        g = corr(T[..., 1], nz)
        al = corr(T[..., 3], nz)
        vals = np.array([r[0], r[1], bl[0], bl[1], g, al])
        tot += vals
        print("%-22s %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f"
              % ("%s %d,%d" % ("16", cx, cy), *vals))
        # row-orientation control: the SAME statistic with the sheet unflipped
        U = img.astype(np.float64) / 255.0 * 2.0 - 1.0
        rowflip_tot += np.array([corr(U[..., 0], nx), corr(U[..., 2], ny)])
    n = len(tiles)
    print("%-22s %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f"
          % ("MEAN", *(tot / n)))
    print("\nROW-ORDER CONTROL (sheet NOT flipped; must be markedly worse):")
    print("  R:-dh/dx %.3f   B:-dh/dy %.3f" % tuple(rowflip_tot / n))


if __name__ == "__main__":
    main()
