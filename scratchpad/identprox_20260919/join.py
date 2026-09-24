#!/usr/bin/env python3
"""IDENTPROX -- the PROXIMITY JOIN engine.

A simulation, on chunk 4.4.-12's shipped bake, of what the `.lodi` v7 group
table would hold under a proximity-join rule.  Nothing here writes a file the
generator reads; it re-derives the identity in Python and hands it to
HORIZON4's shadow-map simulator.

WHAT THE SHIPPED RULE IS (quoted, so the simulation can be checked against it)
-----------------------------------------------------------------------------
`src/nativeemit.cpp:1178`   archComponent = "architecture"
`src/nativeemit.cpp:1184`   touchTolerance = 16.0f          -- world units
`src/nativeemit.cpp:1191`   useBox = true                   -- the DRAWN MESH's AABB
`src/nativeemit.cpp:2476-2487`   (i) SCOL parts of one reference are one group
`src/nativeemit.cpp:2489-2527`   (ii) eligible placements get the mesh's eight
                                 local corners PLACED and RE-BOUNDED
                                 AXIS-ALIGNED in world
`src/nativeemit.cpp:2530-2553`   joined when the two world AABBs are within
                                 `touchTolerance` on EVERY axis
`src/nativeemit.cpp:2560-2563`   a placement that ended alone gets LODI_GROUP_ALONE

So TODAY'S MEASURE IS THE WORLD AABB GAP, not an oriented box and not the mesh.

THE THREE AXES OF THE SWEEP
---------------------------
(a) WHO MAY JOIN
    arch    today: only placements whose BASE model string carries an
            "architecture" path component (the eligibility test at
            `src/nativeemit.cpp:1990-2004`).
    all     every placement except trees.  (No placement is terrain -- the
            terrain surface is not in the instance table at all -- so the
            brief's "non-terrain" is a no-op on this chunk and is stated
            rather than silently dropped.)
    attach  every placement except trees, but a SMALL object may only join
            the NEAREST BIG one; smalls never join each other.  "Big" is a
            world-AABB diagonal >= BIG_DIAG; the sweep prints what that
            selects and a sensitivity row.

(b) GAP      16 / 32 / 64 / 128 / 256 world units.

(c) MEASURE
    aabb    the world AABB gap -- TODAY'S.
    obb     the oriented box gap, as the largest separation over the 15 SAT
            axes of two OBBs.  That is EXACT when the closest features are a
            face pair and a LOWER BOUND otherwise, so it errs toward joining;
            stated, not hidden.
    mesh    the minimum distance between the two placements' drawn-mesh
            sample sets.  Two sets are reported everywhere: `vert` = the
            level-0 vertices only, which is literally what the brief asked
            for and OVER-states the gap wherever a triangle is long; `dense`
            = the same vertices plus every edge midpoint and centroid, which
            is closer to the true surface distance.  `mesh` means `dense`;
            `meshv` means `vert`.

The SCOL clause of the shipped rule is kept in every cell: it is not what is
under test.
"""
import numpy as np
import os
import pickle
import struct
import sys

from scipy.spatial import cKDTree

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/identprox_20260919'
H4 = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon4_20260919'
SUNSIM = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/sunsim1_20260919'
SPELLS = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells'
for q in (LANE, SUNSIM, SPELLS):
    if q not in sys.path:
        sys.path.insert(0, q)

import lodgen_native_decode as ND                           # noqa: E402

BAKE = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
        '/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth')
SCAN = ('C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude'
        '/392777f8-9016-4913-858d-16d6eec4c01a/scratchpad/ident_scan.pkl')

BIG_DIAG = 1024.0          # world-AABB diagonal at or above which a thing is "big"
MAXGAP = 256.0             # the widest gap the sweep asks for
TREE_KEYS = ('tree', 'shrub', 'bush')
ATTACH_MUL = 4.0           # the attachment reach, as a multiple of the gap;
                           # candidate pairs stop at MAXGAP, so the reach is
                           # min(ATTACH_MUL x gap, MAXGAP) in practice
ROAD_KEYS = ('roads', 'highwayoverpass', 'highway', 'bridges')


def road_mask(P):
    """Today's `architecture` component test, widened to the ROAD/HIGHWAY kit.

    Measured by `hwy_why.py`: 0 of the 24 highway placements on this chunk
    carry an `architecture` path component -- they live under
    `Landscape\\Roads\\HighwayOverpass\\` -- so today's rule makes every one a
    singleton by ELIGIBILITY, whatever the geometry says.  This mask is the
    one-word change that lets the run join itself.
    """
    m = getattr(P, '_road', None)
    if m is None:
        low = [s.lower().replace('/', '\\').split('\\') for s in P.model]
        m = np.array([any(k in parts for k in ROAD_KEYS) for parts in low])
        P._road = m
    return m
HAND_REF = ('DN135_GwinnettExt', 'AndrewStation', 'Theater47_Bld01')
LAYER_KEYS = ('bld', 'building', 'tower', 'bldg')


# ---------------------------------------------------------------------------
class Placements(object):
    """Everything the join rules need, derived ONCE."""

    def __init__(self, cache=LANE + '/placements.pkl', verbose=True):
        if os.path.exists(cache):
            d = pickle.load(open(cache, 'rb'))
            self.__dict__.update(d)
            if verbose:
                print('placements: cache %d' % self.n)
            return
        L = ND.read_lodo(BAKE + '.lodo')
        T = ND.read_lodi(BAKE + '.lodi')
        n = T['header']['instanceCount']
        self.n = n
        self.model = np.array([L['string_at'](L['bases'][r['baseId']]['modelStringOffset'])
                               for r in T['instances']])
        low = np.array([m.lower().replace('/', '\\') for m in self.model])
        self.arch = np.array([('architecture' in s.split('\\')) for s in low])
        self.tree = np.array([any(k in s for k in TREE_KEYS) for s in low])
        self.pos = np.array([[r['x'], r['y'], r['z']] for r in T['instances']], dtype=np.float64)
        # scolPart lives in the COLD table, not the hot instance row
        self.scol = np.array([T['cold'][i]['scolPart'] for i in range(n)], dtype=np.int64)
        self.refid = np.array([T['cold'][i]['refFormId'] for i in range(n)], dtype=np.int64)
        # ---- today's shipped group ids, made globally unique per chunk
        g = np.array(T['group'], dtype=np.int64)
        ch = np.zeros(n, dtype=np.int64)
        for ci, c in enumerate(T['chunks']):
            ch[c['instanceFirst']:c['instanceFirst'] + c['instanceCount']] = ci
        self.shipped = ch * 100000 + g + 1
        self.chunk = ch          # v7 group ids are dense PER CHUNK, so an
        # identity can never span two chunks whatever the geometry says
        # ---- world AABB, OBB and the mesh sample sets
        lo = np.full((n, 3), np.nan)
        hi = np.full((n, 3), np.nan)
        cen = np.zeros((n, 3))
        axs = np.zeros((n, 3, 3))
        hal = np.zeros((n, 3))
        vfirst = np.zeros(n + 1, dtype=np.int64)
        dfirst = np.zeros(n + 1, dtype=np.int64)
        VS, DS = [], []
        for i, r in enumerate(T['instances']):
            bse = L['bases'][r['baseId']]
            mid = bse['rep0']
            M = np.array(r['m'], dtype=np.float64).reshape(3, 3)
            sc = r['scaleF']
            P = np.array([r['x'], r['y'], r['z']])
            if mid == 0xFFFF or mid >= len(L['meshes']):
                lo[i] = hi[i] = P
                cen[i] = P
                axs[i] = np.eye(3)
                VS.append(P[None, :])
                DS.append(P[None, :])
                vfirst[i + 1] = vfirst[i] + 1
                dfirst[i + 1] = dfirst[i] + 1
                continue
            m = L['meshes'][mid]
            a = np.array(m['aabbMin'], dtype=np.float64)
            e = np.array(m['aabbExtent'], dtype=np.float64)
            c8 = np.array([[a[0] + e[0] * ((k >> 0) & 1),
                            a[1] + e[1] * ((k >> 1) & 1),
                            a[2] + e[2] * ((k >> 2) & 1)] for k in range(8)])
            w8 = (M @ c8.T).T * sc + P
            lo[i] = w8.min(0)
            hi[i] = w8.max(0)
            # the OBB: the LOCAL aabb rotated, so its axes are the base's own
            cen[i] = (M @ (a + e * 0.5)) * sc + P
            axs[i] = M.T                     # rows = the three world axis dirs
            hal[i] = np.abs(e) * 0.5 * sc
            v, d = self._samples(L, m, M, sc, P)
            VS.append(v)
            DS.append(d)
            vfirst[i + 1] = vfirst[i] + len(v)
            dfirst[i + 1] = dfirst[i] + len(d)
        self.lo, self.hi = lo, hi
        self.cen, self.axs, self.hal = cen, axs, hal
        self.V = np.concatenate(VS)
        self.D = np.concatenate(DS)
        self.vfirst, self.dfirst = vfirst, dfirst
        self.diag = np.linalg.norm(hi - lo, axis=1)
        self.big = self.diag >= BIG_DIAG
        # ---- the Creation Kit layer of every placement, for the SHATTER count
        S = pickle.load(open(SCAN, 'rb'))
        refsub, layr = S['refsub'], S['layr']
        lay = np.zeros(n, dtype=np.int64)
        for i in range(n):
            s = refsub.get(int(self.refid[i]), {})
            if b'XLYR' in s:
                lay[i] = struct.unpack_from('<I', s[b'XLYR'][0], 0)[0]
        self.layer = lay
        self.layer_name = {int(k): layr.get(int(k), ('', 0))[0] for k in np.unique(lay) if k}
        pickle.dump(self.__dict__, open(cache, 'wb'), 2)
        if verbose:
            print('placements: built %d (%d arch, %d tree, %d big)'
                  % (n, int(self.arch.sum()), int(self.tree.sum()), int(self.big.sum())))

    @staticmethod
    def _samples(L, m, M, sc, P):
        """Level-0 vertices (as asked) and a densified set (vertices + edge
        midpoints + centroids), both in WORLD units."""
        cs = L['clusters'][m['clusterFirst']:m['clusterFirst'] + m['clusterCount']]
        ls = L['clusterLods'][m['clusterFirst']:m['clusterFirst'] + m['clusterCount']]
        a = np.array(m['aabbMin'], dtype=np.float64)
        e = np.array(m['aabbExtent'], dtype=np.float64)
        if not cs:
            q = (M @ (a + e * 0.5)) * sc + P
            return q[None, :], q[None, :]
        vb = min(c['vertexBase'] for c in cs)
        ve = max(c['vertexBase'] + c['vertexCount'] for c in cs)
        q = np.array([(x['px'], x['py'], x['pz']) for x in L['vertices'][vb:ve]],
                     dtype=np.float64)
        pos = a + q / 65535.0 * e
        w = (M @ pos.T).T * sc + P
        tl = []
        for ci, (c, l) in enumerate(zip(cs, ls)):
            if l['level'] != 0:
                continue
            gi = m['clusterFirst'] + ci
            li = L['localIndices'][gi * 48:(gi + 1) * 48]
            off = c['vertexBase'] - vb
            for t in range(c['triangleCount']):
                tl.append((li[t * 3] + off, li[t * 3 + 1] + off, li[t * 3 + 2] + off))
        if not tl:
            return w, w
        tri = np.array(tl, dtype=np.int64)
        A, B, C = w[tri[:, 0]], w[tri[:, 1]], w[tri[:, 2]]
        dense = np.concatenate([w, 0.5 * (A + B), 0.5 * (B + C), 0.5 * (C + A),
                                (A + B + C) / 3.0])
        return w, dense


# ---------------------------------------------------------------------------
def aabb_gap(P, i, j):
    return float(np.maximum(P.lo[i] - P.hi[j], P.lo[j] - P.hi[i]).max())


def obb_gap(P, i, j):
    """Largest separation over the 15 SAT axes.  Exact for a face-face closest
    pair, a LOWER bound otherwise (so it joins at least as readily as the truth)."""
    Ca, Cb = P.cen[i], P.cen[j]
    Aa, Ab = P.axs[i], P.axs[j]
    Ha, Hb = P.hal[i], P.hal[j]
    d = Cb - Ca
    best = -1e30
    axes = [Aa[0], Aa[1], Aa[2], Ab[0], Ab[1], Ab[2]]
    for u in range(3):
        for v in range(3):
            c = np.cross(Aa[u], Ab[v])
            nn = np.linalg.norm(c)
            if nn > 1e-9:
                axes.append(c / nn)
    for L in axes:
        ra = float(np.abs(Aa @ L) @ Ha)
        rb = float(np.abs(Ab @ L) @ Hb)
        s = abs(float(d @ L)) - ra - rb
        if s > best:
            best = s
    return best


def mesh_gap(P, i, j, dense=True):
    if dense:
        a = P.D[P.dfirst[i]:P.dfirst[i + 1]]
        b = P.D[P.dfirst[j]:P.dfirst[j + 1]]
    else:
        a = P.V[P.vfirst[i]:P.vfirst[i + 1]]
        b = P.V[P.vfirst[j]:P.vfirst[j + 1]]
    if len(a) == 0 or len(b) == 0:
        return 1e30
    t = cKDTree(b)
    return float(t.query(a, k=1)[0].min())


# ---------------------------------------------------------------------------
def candidates(P, maxgap=MAXGAP, cache=LANE + '/cand.pkl', verbose=True):
    """Every placement pair whose world AABBs are within `maxgap`.

    The AABB gap is <= the OBB gap <= the mesh gap for the same pair (each box
    contains the next), so this set is a SUPERSET of the candidates of every
    measure in the sweep.  That is the whole reason it is legal to compute it
    once."""
    if os.path.exists(cache):
        d = pickle.load(open(cache, 'rb'))
        if d['maxgap'] >= maxgap:
            if verbose:
                print('candidates: cache %d pairs' % len(d['pairs']))
            return d['pairs'], d['g']
    lo, hi = P.lo, P.hi
    cell = 1024.0
    grid = {}
    for i in range(P.n):
        if not np.isfinite(lo[i][0]):
            continue
        for gy in range(int(np.floor((lo[i][1] - maxgap) / cell)),
                        int(np.floor((hi[i][1] + maxgap) / cell)) + 1):
            for gx in range(int(np.floor((lo[i][0] - maxgap) / cell)),
                            int(np.floor((hi[i][0] + maxgap) / cell)) + 1):
                grid.setdefault((gx, gy), []).append(i)
    seen = set()
    for v in grid.values():
        for a in range(len(v)):
            for b in range(a + 1, len(v)):
                seen.add((min(v[a], v[b]), max(v[a], v[b])))
    pairs, gaps = [], []
    for (i, j) in seen:
        g = aabb_gap(P, i, j)
        if g <= maxgap:
            pairs.append((i, j))
            gaps.append(g)
    pairs = np.array(pairs, dtype=np.int64)
    g = dict(aabb=np.array(gaps))
    if verbose:
        print('candidates: %d pairs within %.0f u (AABB)' % (len(pairs), maxgap))
    # the other two measures on the SAME pairs
    g['obb'] = np.array([obb_gap(P, int(i), int(j)) for i, j in pairs])
    g['mesh'] = np.array([mesh_gap(P, int(i), int(j), True) for i, j in pairs])
    g['meshv'] = np.array([mesh_gap(P, int(i), int(j), False) for i, j in pairs])
    pickle.dump(dict(pairs=pairs, g=g, maxgap=maxgap), open(cache, 'wb'), 2)
    return pairs, g


# ---------------------------------------------------------------------------
class UF(object):
    def __init__(self, n):
        self.p = np.arange(n)

    def find(self, a):
        p = self.p
        while p[a] != a:
            p[a] = p[p[a]]
            a = p[a]
        return a

    def join(self, a, b):
        a, b = self.find(a), self.find(b)
        if a != b:
            self.p[max(a, b)] = min(a, b)


def identity(P, pairs, gaps, who='arch', measure='aabb', gap=16.0,
             scol=True, big_diag=BIG_DIAG):
    """Return an identity id per placement under one cell of the sweep."""
    uf = UF(P.n)
    CH = P.chunk
    if scol:
        first = {}
        for i in range(P.n):
            if P.scol[i] < 0:
                continue
            # keyed by (reference, CHUNK): the shipped union-find joins SCOL
            # parts region-wide, but the writer emits a group id that is dense
            # PER CHUNK, so a SCOL straddling a chunk edge comes out of the FILE
            # as two identities however the union-find ran.  One does on this
            # chunk: ParkPierStr01, chunks 1 and 2, AABB gap -0.03 u.
            k = (int(P.refid[i]), int(CH[i]))
            if k in first:
                uf.join(first[k], i)
            else:
                first[k] = i
    d = gaps[measure]
    ok = d <= gap
    big = P.diag >= big_diag
    if who in ('arch', 'kit'):
        # `kit` = today's rule with ONE word added to the eligible list: the
        # road/highway kit.  Nothing else about the rule changes.
        elig = (P.arch | road_mask(P)) if who == 'kit' else P.arch
        for k in np.nonzero(ok)[0]:
            i, j = int(pairs[k][0]), int(pairs[k][1])
            if elig[i] and elig[j] and CH[i] == CH[j]:
                uf.join(i, j)
    elif who == 'all':
        elig = ~P.tree
        for k in np.nonzero(ok)[0]:
            i, j = int(pairs[k][0]), int(pairs[k][1])
            if elig[i] and elig[j] and CH[i] == CH[j]:
                uf.join(i, j)
    elif who == 'attach':
        # `all` FIRST -- every non-tree placement joins what it touches at the
        # gap -- and THEN one attachment pass: a SMALL placement that is still
        # alone takes the identity of the NEAREST BIG placement within
        # ATTACH_MUL x gap.  That is what "roof clutter, pipes, signs and AC
        # units join their building" has to mean: the clutter does not TOUCH
        # the roof, so no touching rule can ever reach it, and a rule that only
        # attached smalls to bigs (and never a wall to a wall) shatters every
        # building whose pieces are all small -- measured, 1,846 identities at
        # gap 16, three times what ships.
        elig = ~P.tree
        for k in np.nonzero(ok)[0]:
            i, j = int(pairs[k][0]), int(pairs[k][1])
            if elig[i] and elig[j] and CH[i] == CH[j]:
                uf.join(i, j)
        alone = np.bincount(np.array([uf.find(i) for i in range(P.n)]),
                            minlength=P.n)
        R = gap * ATTACH_MUL
        best = {}
        for k in range(len(pairs)):
            if d[k] > R:
                continue
            i, j = int(pairs[k][0]), int(pairs[k][1])
            if not (elig[i] and elig[j]) or CH[i] != CH[j]:
                continue
            for s, b in ((i, j), (j, i)):
                if big[s] or not big[b] or alone[uf.find(s)] != 1:
                    continue
                if s not in best or d[k] < best[s][0]:
                    best[s] = (d[k], b)
        for s, (_, b) in best.items():
            uf.join(s, b)
    else:
        raise ValueError(who)
    return np.array([uf.find(i) for i in range(P.n)], dtype=np.int64)


# ---------------------------------------------------------------------------
def reference_layers(P):
    """The layers this lane treats as "one building": the Bld/Building/Tower/
    Bldg-named Creation Kit layers (ident_notes.md s: 97% of them are one
    building) plus the hand list bungo named."""
    out = {}
    for f, nm in P.layer_name.items():
        if not nm:
            continue
        lownm = nm.lower()
        if any(k in lownm for k in LAYER_KEYS) or nm in HAND_REF:
            sel = np.nonzero(P.layer == f)[0]
            if len(sel) >= 2:
                out[nm] = sel
    for nm in HAND_REF:
        if nm in out:
            continue
        for f, n2 in P.layer_name.items():
            if n2 == nm:
                sel = np.nonzero(P.layer == f)[0]
                if len(sel) >= 2:
                    out[nm] = sel
    return out


def score(P, ident, refs, over_extent):
    """The five numbers every cell of the sweep reports."""
    u, inv = np.unique(ident, return_inverse=True)
    sizes = np.bincount(inv)
    # extent per identity
    ext = np.zeros(len(u))
    npl = np.zeros(len(u), dtype=np.int64)
    for k in range(len(u)):
        sel = np.nonzero(inv == k)[0]
        npl[k] = len(sel)
        lo = np.nanmin(P.lo[sel], axis=0)
        hi = np.nanmax(P.hi[sel], axis=0)
        ext[k] = float(max(hi[0] - lo[0], hi[1] - lo[1]))
    big = int(np.argmax(sizes))
    shatter = 0
    for nm, sel in refs.items():
        if len(set(int(x) for x in ident[sel])) > 1:
            shatter += 1
    # over-merge: an identity holding placements of TWO different reference
    # layers, and the broader signal -- an identity spanning two different CK
    # layers of ANY kind (two city blocks share no wall; a street runs between)
    pair_over = 0
    cross_layer = 0
    for k in range(len(u)):
        sel = np.nonzero(inv == k)[0]
        got = set()
        for nm, s2 in refs.items():
            if len(np.intersect1d(sel, s2)):
                got.add(nm)
        if len(got) >= 2:
            pair_over += 1
        ls = set(int(x) for x in P.layer[sel] if x)
        if len(ls) >= 2:
            cross_layer += 1
    return dict(groups=int(len(u)), singletons=int((sizes == 1).sum()),
                largest=int(sizes.max()), largest_extent=float(ext[big]),
                over_extent=int((ext > over_extent).sum()),
                over_two_refs=int(pair_over), cross_layer=int(cross_layer),
                shatter=int(shatter), nrefs=int(len(refs)),
                ext_p50=float(np.percentile(ext, 50)),
                ext_p99=float(np.percentile(ext, 99)),
                ext_max=float(ext.max()))
