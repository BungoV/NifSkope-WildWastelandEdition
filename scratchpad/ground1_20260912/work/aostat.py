"""Lane GROUND1: read the AO byte (mask sheet B) out of a .lodt pyramid level
and report its distribution, for a pair of bakes.

Imports the harness's own reader (tests/spells/lodgen_vt_check.py) unchanged --
no second copy of the container's arithmetic.
"""
import os, sys, struct
sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/scratchpad/ground1_20260912/work')
import lodgen_vt_check as V
import maskdec


def mask_rows(v, index, mip=0):
    """The mask sheet's decoded rows.  Routed through maskdec because a COVER
    tile's mask sheet is BC3, 16 bytes a block with the alpha half first, and
    `lodgen_vt_check.decode_bc1` walks 8-byte blocks -- which silently returns
    garbage for every cover tile.  This lane read tiles 4..15 that way once."""
    rows, fmt = maskdec.mask_rows(v, index, mip)
    return rows


def ao_of(path, level_name):
    v = V.Lodv(path)
    out = {}
    for i, e in enumerate(v.table):
        rows = mask_rows(v, i)
        if rows is None:
            continue
        out[i] = rows
    return v, out


def content_ao(v, rows):
    """AO bytes of the CONTENT area only (the border is shared with a neighbour)."""
    b = v.border
    n = v.stored
    vals = []
    for y in range(b, n - b):
        r = rows[y]
        for x in range(b, n - b):
            vals.append(r[x][2])
    return vals


def hist(vals, step=16):
    h = [0] * (256 // step)
    for x in vals:
        h[min(x // step, len(h) - 1)] += 1
    return h


if __name__ == '__main__':
    a, b = sys.argv[1], sys.argv[2]
    for lvl in sys.argv[3:]:
        pa = os.path.join(a, 'Terrain', 'Commonwealth.VT.%s.lodt' % lvl)
        pb = os.path.join(b, 'Terrain', 'Commonwealth.VT.%s.lodt' % lvl)
        if not (os.path.exists(pa) and os.path.exists(pb)):
            continue
        va, ra = ao_of(pa, lvl)
        vb, rb = ao_of(pb, lvl)
        A, B = [], []
        for i in sorted(set(ra) & set(rb)):
            A += content_ao(va, ra[i])
            B += content_ao(vb, rb[i])
        n = len(A)
        drop = [A[k] - B[k] for k in range(n)]
        moved = [d for d in drop if d > 0]
        print('level %s  tiles %d  content texels %d' % (lvl, len(ra), n))
        print('  off  mean %.2f  min %d  max %d' % (sum(A) / n, min(A), max(A)))
        print('  on   mean %.2f  min %d  max %d' % (sum(B) / n, min(B), max(B)))
        print('  darkened texels %d (%.1f%%)  mean drop over them %.2f  max drop %d'
              % (len(moved), 100.0 * len(moved) / n,
                 (sum(moved) / len(moved)) if moved else 0.0,
                 max(moved) if moved else 0))
        print('  brightened texels %d' % len([d for d in drop if d < 0]))
        print('  hist off (16-wide bins) %s' % hist(A))
        print('  hist on  (16-wide bins) %s' % hist(B))
