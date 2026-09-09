"""Shared loading for the RECOVER lane: layers.txt, cells.npz, LTEX names.

Quadrant order is the ESM's: 0 BL, 1 BR, 2 TL, 3 TR.

Composite weights.  The engine lays the BTXT base down, then composites each
ATXT layer over it in order with that layer's alpha.  For one layer the mean of
the composite is exactly (1-mean_a)*base + mean_a*layer, so mean opacity is the
right thing to use; for several layers the exact answer needs E[a_i a_j], which
layers.txt does not carry, so we assume independence:

    w_base = prod_i (1 - a_i)
    w_k    = a_k * prod_{j>k} (1 - a_j)

These sum to 1 by construction.  When a quadrant has NO BTXT the engine has
nothing underneath (measured: Commonwealth's WRLD DNAM default land texture is
NULL), so w_base is kept and attributed to a pseudo-material with form id
00000000 -- whose colour the regression then estimates rather than us guessing.
"""
import json
import os

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MINX = MINY = -96
N = 192
NULL = 0            # pseudo-material: "no BTXT under these layers"


def parse_layers(path=None):
    """-> (painted, blends) where blends[(cy-MINY, cx-MINX)][q] = [(ltex, w), ...]

    painted is a (192,192) bool array: the cell has any BTXT or ATXT anywhere.
    """
    path = path or os.path.join(HERE, 'layers.txt')
    painted = np.zeros((N, N), dtype=bool)
    blends = {}
    for line in open(path):
        if not line.startswith('L '):
            continue
        f = line.split()
        cx, cy = int(f[1]), int(f[2])
        blend = None
        for tok in f[3:]:
            if tok.startswith('blend='):
                blend = tok[6:]
        if blend is None or blend == '-|-|-|-':
            continue
        r, c = cy - MINY, cx - MINX
        painted[r, c] = True
        quads = []
        for qs in blend.split('|'):
            if qs == '-':
                quads.append([])
                continue
            ents = []
            for e in qs.split(';'):
                fid, op = e.split(':')
                ents.append((int(fid, 16), float(op)))
            # first entry at 1.000 is the BTXT base; otherwise there is none
            if ents and abs(ents[0][1] - 1.0) < 1e-9:
                base, layers = ents[0][0], ents[1:]
            else:
                base, layers = NULL, ents
            rest = 1.0
            w = []
            for fid, a in reversed(layers):
                w.append((fid, a * rest))
                rest *= (1.0 - a)
            w.append((base, rest))
            w.reverse()
            # merge duplicates (the same ltex can appear as base and as a layer)
            agg = {}
            for fid, ww in w:
                agg[fid] = agg.get(fid, 0.0) + ww
            quads.append(sorted(agg.items(), key=lambda kv: -kv[1]))
        blends[(r, c)] = quads
    return painted, blends


def load_cells():
    z = np.load(os.path.join(HERE, 'cells.npz'))
    return z['pix'], z['have'], z['vclr']


# quadrant -> (row slice, col slice) inside a cell's 32x32 north-up block
QSLICE = {0: (slice(16, 32), slice(0, 16)),     # BL: south half, west half
          1: (slice(16, 32), slice(16, 32)),    # BR
          2: (slice(0, 16), slice(0, 16)),      # TL: north half, west half
          3: (slice(0, 16), slice(16, 32))}     # TR


def quad_stats(pix, vclr):
    """-> (mean, std) each (192,192,4,3) float32, VCLR divided out."""
    lin = pix.astype(np.float32) / np.maximum(vclr, 1.0 / 255.0)
    mean = np.empty((N, N, 4, 3), dtype=np.float32)
    std = np.empty((N, N, 4, 3), dtype=np.float32)
    for q, (rs, cs) in QSLICE.items():
        sub = lin[:, :, rs, cs, :]
        mean[:, :, q] = sub.mean(axis=(2, 3))
        std[:, :, q] = sub.std(axis=(2, 3))
    return mean, std


def ltex_names():
    """form id -> (EDID, diffuse path or None)."""
    j = json.load(open(os.path.join(HERE, 'lens2', 'layers3C.json')))
    out = {}
    for k, (edid, tn) in j['ltex'].items():
        tx = None
        if tn is not None and str(tn) in j['txst']:
            tx = j['txst'][str(tn)][1].get('TX00')
        out[int(k)] = (edid, tx)
    out[NULL] = ('<no BTXT under layers>', None)
    return out
