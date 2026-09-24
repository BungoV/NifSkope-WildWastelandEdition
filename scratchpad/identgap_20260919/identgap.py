#!/usr/bin/env python3
"""IDENTGAP -- the engine.  Six ways of stopping a flat LOD wall shadowing itself.

The question the director put (2026-09-19): outside the cascade range, a low-poly
LOD wall shadows ITSELF and an artefact patch appears mid-wall.  Today's answer is
IDENTITY: a pixel ignores any occluder carrying its own `.lodi` GROUP id.  The
price is every genuine self-shadow inside one identity, and the just-ruled
proximity join (non-tree placements, mesh gap <= 64 u, 588 -> 167 groups) makes
identities BIGGER, so the price rises.

The candidate to be MEASURED, not assumed:

    IDENTITY-GATED DISTANCE.  A same-identity occluder is ignored ONLY when it
    lies within D units of the receiver ALONG THE SUN RAY -- the zone where
    self-occlusion is an artefact of the map's own resolution.  A same-identity
    occluder FARTHER than D still shadows.  A different-identity occluder always
    shadows, with the normal small bias.

The six groups, each on both identity tables and three cameras:

  G0  no identity at all; slope-scaled + normal-offset bias, tuned (`tune_bias`)
  G1  pure identity                    -- the baseline, must reproduce identres
  G2  the gate, D = 64/128/256/512/1024
  G3  back-face casting, no identity   -- only triangles facing AWAY from the sun
                                          write the depth map
  G4  the best G2 with the normal offset re-tuned on top

GEOMETRY OF THE GATE.  `h4map`'s light space puts `u` along the sun's horizontal
direction and `s = z - u*tan(el)` constant along a ray.  A blocker at depth `mu`
above a receiver at depth `u` sits

    L = (mu - u) / cos(el)

units from it along the ray.  So "within D along the ray" is `mu - u <= D*cos(el)`
-- ONE subtract, ONE compare, on a depth the shader has already fetched, beside
the identity it has already fetched.  That is the whole runtime cost.

THE EXACT (no-map) GATE.  `identres.IdentShear` stores, per 16 u cell and after a
suffix maximum along `u`, the best `s`, the identity that achieved it, and the
best `s` among entries of a DIFFERENT identity.  Its `lit()` is exactly

    blocked_G1 = (best-of-another-identity, over every column beyond the receiver) > s

Because a different-identity occluder shadows at any distance and a same-identity
one shadows beyond D, the gated answer decomposes with no new structure at all:

    blocked_G2 = blocked_G1  OR  ( best s over columns beyond u + D*cos(el) ) > s

and that second term is the SAME `bs` plane read at a shifted column.  D = 0 gives
the plain no-identity cast; D = infinity gives G1.  Both are asserted in `run.py`.
"""
import numpy as np
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
IP = ROOT + '/scratchpad/identprox_20260919'
IR_ = ROOT + '/scratchpad/identres_20260919'
SUNSIM = ROOT + '/scratchpad/sunsim1_20260919'
for q in (IR_, IP, SUNSIM, ROOT + '/tests/spells'):
    if q not in sys.path:
        sys.path.insert(0, q)

import h4map as MP                                            # noqa: E402
import identres as IR                                         # noqa: E402

TERR_KEY = 0xFFFFF
INF = float('inf')


# ---------------------------------------------------------------------------
# G0 / G1 / G2 / G4 through a shadow MAP
# ---------------------------------------------------------------------------
def query_gate(mp, pos, nrm, ident, D=INF, normal_bias=1.0, depth_bias=0.5,
               slope=0.0, slope_clamp=8.0, taps=1):
    """`identres.query_pcf` with the identity rule replaced by the GATE.

    `D` is the gate distance along the sun ray, in world units.
      D = 0         a same-identity occluder is never ignored  -> G0, no identity
      D = infinity  a same-identity occluder is always ignored -> G1, pure identity
      0 < D < inf   the gate                                   -> G2
    Returns (dark_fraction, any_nearer, no_data)."""
    p = pos + nrm * (normal_bias * mp.texel)
    u = p[:, 0] * mp.sx + p[:, 1] * mp.sy
    v = -p[:, 0] * mp.sy + p[:, 1] * mp.sx
    s = p[:, 2] - u * mp.tan
    iv0 = np.floor((v - mp.v0) / mp.texel).astype(np.int64)
    is0 = np.floor((s - mp.s0) / mp.texel).astype(np.int64)
    b = depth_bias
    if slope > 0.0:
        nu = nrm[:, 0] * mp.sx + nrm[:, 1] * mp.sy + nrm[:, 2] * mp.tan
        nv = -nrm[:, 0] * mp.sy + nrm[:, 1] * mp.sx
        ns_ = nrm[:, 2]
        den = np.where(np.abs(nu) < 1e-6, 1e-6, np.abs(nu))
        g = np.maximum(np.abs(nv) / den, np.abs(ns_) / den)
        b = depth_bias + slope * np.minimum(slope_clamp, g)
    # the gate's reach, expressed in the map's own depth `u`
    cosel = np.cos(np.radians(mp.el))
    reach = D * cosel if np.isfinite(D) else INF
    r = taps // 2
    acc = np.zeros(len(u))
    anyn = np.zeros(len(u), dtype=bool)
    nod = np.zeros(len(u), dtype=bool)
    n = 0
    for dv in range(-r, r + 1):
        for ds in range(-r, r + 1):
            iv = iv0 + dv
            isv = is0 + ds
            inside = (iv >= 0) & (iv < mp.nv) & (isv >= 0) & (isv < mp.ns)
            pix = np.where(inside, iv * mp.ns + np.clip(isv, 0, mp.ns - 1), 0)
            k = mp.KEY[pix]
            mu, mi = mp._unkey(np.maximum(k, 0))
            has = inside & (k >= 0)
            nearer = has & (mu > u + b * mp.texel)
            if D <= 0.0:
                dark = nearer
            else:
                same = (mi == ident) & (ident != MP.TERRAIN_ID)
                if np.isfinite(D):
                    # the ONE extra compare: is this same-identity blocker inside
                    # the self-occlusion zone?
                    close = (mu - u) <= reach
                    dark = nearer & ~(same & close)
                else:
                    dark = nearer & ~same
            acc += dark
            anyn |= nearer
            if dv == 0 and ds == 0:
                nod = ~has
            n += 1
    return acc / n, anyn, nod


# ---------------------------------------------------------------------------
# G2 / G1 / G0 exactly, with no map anywhere in the answer
# ---------------------------------------------------------------------------
class GateShear(IR.IdentShear):
    """`identres.IdentShear` with the gate, exact.

    `lit_gate(pos, ident, D)`:
        D = 0    -> the plain sun cast (must equal the LEFT panel)
        D = inf  -> `IdentShear.lit` (must equal it array-for-array)
        else     -> blocked_G1 OR (a blocker beyond u + D*cos(el))
    """

    def lit_gate(self, pos, ident, D=INF):
        x, y, z = pos[:, 0], pos[:, 1], pos[:, 2]
        u = x * self.sx + y * self.sy
        v = -x * self.sy + y * self.sx
        s = z - u * self.tan + self.RISE
        blocked = np.zeros(len(x), dtype=bool)
        own = np.zeros(len(x), dtype=bool)
        iy = np.floor((v - self.nv0) / self.NEAR_CELL).astype(np.int64)
        ix = np.floor((u - self.nu0) / self.NEAR_CELL).astype(np.int64) + 1
        okv = (iy >= 0) & (iy < self.nh)
        ok = okv & (ix >= 0) & (ix < self.nw)
        if ok.any():
            bs = self.bs[iy[ok], ix[ok]]
            bi = self.bi[iy[ok], ix[ok]]
            sc = self.ss[iy[ok], ix[ok]]
            g = ident[ok]
            mine = (g != MP.TERRAIN_ID) & (bi == g)
            if D <= 0.0:
                b = bs > s[ok]
            else:
                b = np.where(mine, sc > s[ok], bs > s[ok])
            blocked[ok] |= b
            own[ok] = mine & (bs > s[ok])
        if np.isfinite(D) and D > 0.0:
            cosel = np.cos(np.radians(self.el))
            fx = np.floor((u + D * cosel - self.nu0) / self.NEAR_CELL).astype(np.int64) + 1
            # `ok`, not `okv`: a receiver the near grid cannot answer for at all
            # must not suddenly acquire a blocker just because D moved the read
            # column into range.  Without this the rule is not monotone in D,
            # and `run.py` asserts that it is.
            okf = ok & (fx >= 0) & (fx < self.nw)
            if okf.any():
                blocked[okf] |= self.bs[iy[okf], fx[okf]] > s[okf]
        # the far terrain grid, the truth panel's own; terrain is never the
        # receiver's identity, so it blocks under every rule here
        fx = np.floor((u - self.fu0) / self.FAR_CELL).astype(np.int64) + 1
        fy = np.floor((v - self.fv0) / self.FAR_CELL).astype(np.int64)
        okF = (fx >= 0) & (fx < self.fw) & (fy >= 0) & (fy < self.fh)
        fb = np.zeros(len(x), dtype=bool)
        if okF.any():
            fb[okF] = self.far[fy[okF], fx[okF]] > s[okF]
        blocked |= fb
        return ~blocked, own & ~fb


# ---------------------------------------------------------------------------
# G3 -- back-face casting
# ---------------------------------------------------------------------------
def sun_vec(az, el):
    a, e = np.radians(az), np.radians(el)
    return np.array([np.sin(a) * np.cos(e), np.cos(a) * np.cos(e), np.sin(e)])


def face_normals(ob):
    """Per-triangle outward normal, oriented by the AUTHORED vertex normals.

    The winding of a `.lodo` cluster is not guaranteed consistent, and a triangle
    whose geometric normal is flipped would be culled backwards.  So the sign is
    taken from the mean of the three authored vertex normals, and the number of
    triangles where the two disagree is reported."""
    t = ob.tri
    a, b, c = ob.v[t[:, 0]], ob.v[t[:, 1]], ob.v[t[:, 2]]
    fn = np.cross(b - a, c - a)
    L = np.linalg.norm(fn, axis=1)
    fn = fn / np.where(L[:, None] < 1e-12, 1.0, L[:, None])
    vn = (ob.n[t[:, 0]] + ob.n[t[:, 1]] + ob.n[t[:, 2]]) / 3.0
    Lv = np.linalg.norm(vn, axis=1)
    vn = vn / np.where(Lv[:, None] < 1e-12, 1.0, Lv[:, None])
    d = np.einsum('ij,ij->i', fn, vn)
    flip = d < 0.0
    fn[flip] *= -1.0
    return fn, flip, (L < 1e-12)


def open_shell_census(ob, weld=0.25):
    """Per placement: is its level-0 shell CLOSED (every edge used twice)?

    The test is on WELDED POSITIONS, not on vertex indices.  A `.lodo` mesh
    splits a vertex at every UV/normal seam, so an index-keyed edge test calls
    even a watertight box open; positions rounded to `weld` world units put the
    split copies back together.  Both counts are reported by the caller.

    A card (one quad, two triangles, four edges used once) and any open shell
    LEAK under back-face casting: nothing lies behind the sun-facing side, so
    once that side is culled the object writes no depth and stops casting."""
    t = ob.tri
    inst = ob.inst[t[:, 0]]
    order = np.argsort(inst, kind='stable')
    ti = inst[order]
    bnd = np.nonzero(np.diff(ti))[0] + 1
    seg = np.split(order, bnd)
    closed, closed_idx, ntri = {}, {}, {}
    for s in seg:
        i = int(inst[s[0]])
        tt = t[s]
        for key, out in ((tt, closed_idx), (None, closed)):
            if key is None:
                q = np.rint(ob.v[tt.ravel()] / weld).astype(np.int64)
                _, w = np.unique(q, axis=0, return_inverse=True)
                e3 = w.reshape(-1, 3)
            else:
                e3 = key
            e = np.concatenate([e3[:, [0, 1]], e3[:, [1, 2]], e3[:, [2, 0]]], axis=0)
            e = np.sort(e, axis=1)
            _, cnt = np.unique(e, axis=0, return_counts=True)
            out[i] = bool((cnt == 2).all())
        ntri[i] = len(s)
    return closed, ntri, closed_idx


def backface_mask(ob, az, el):
    """True for the triangles that WRITE the depth map under G3: the ones whose
    outward normal faces away from the sun."""
    fn, flip, degen = face_normals(ob)
    d = fn @ sun_vec(az, el)
    return (d < 0.0) & ~degen, flip, degen


class TriSubset(object):
    """An `Objects`-shaped view holding only the triangles of a mask."""

    def __init__(self, ob, mask):
        self.v = ob.v
        self.n = ob.n
        self.tri = ob.tri[mask]
        self.inst = ob.inst
        self.mask = mask
