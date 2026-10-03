#!/usr/bin/env python3
"""The far map's trees, read a second time (lane TREE1, 2026-10-02; docs/PRTP_PLAN.md).

Imported by probe_far.py. Shares no code with src/probefar.cpp: the placements and the tree models come
through tests/spells/lodgen_native_decode.py, the texture layers through this file's own BC3 decoder, the
layer of a material from the LOD folder's own text files (Objects/<world>.LodgenArrays.txt, then the chunk
manifests' `M` + `A` lines).

  Trees(lodi)            the library beside the .lodi, its tree bases, the placements
  .placed(x0,y0,x1,y1)   the tree placements whose origin stands in those cells
  .model(tree)           the authored triangles of the slot the far map takes (the coarsest), in the world
  .reference(placed, k)  the first-slot models with every alpha-tested triangle cut into up to k x k pieces,
                         a piece kept when its middle texel passes the material's own threshold
  check(...)             every tree in the soup exactly once, the leaf area, the canopy rule
"""
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lodgen_native_decode as D  # noqa: E402

CELL = 4096.0
NO = 0xFFFF
TOL = 0.5          # units: a soup triangle's middle against the authored triangle's
AREA_TOL = 0.05    # the soup's tree area against this file's own cut, set 2026-10-02 before the first run
BS = chr(92)


def read_psp(path):
    b = open(path, 'rb').read()
    magic, n, nd = struct.unpack_from('<III', b, 0)
    assert magic == 0x31505350, 'not a PSP1 soup'
    tris = np.frombuffer(b, '<f4', n * 9, 12).reshape(n, 3, 3).astype(np.float64)
    off = 12 + n * 36 + nd * 28
    alb = np.full((n, 3), 128, np.uint8)
    if len(b) >= off + 8 + n * 3 and struct.unpack_from('<II', b, off) == (0x31424C41, n):
        alb = np.frombuffer(b, np.uint8, n * 3, off + 8).reshape(n, 3).copy()
    return tris, alb


def write_psp(path, tris, alb):
    t = np.asarray(tris, '<f4').reshape(-1, 9)
    a = np.asarray(alb, np.uint8).reshape(-1, 3)
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', 0x31505350, len(t), 0))
        f.write(t.tobytes())
        f.write(struct.pack('<II', 0x31424C41, len(t)))
        f.write(a.tobytes())


def srgb_lin(c):
    c = c / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def bc3_layer(path, layer):
    """one layer of a DX10 BC3 texture array, mip 0 -> RGBA uint8 [h, w, 4]."""
    b = open(path, 'rb').read()
    h, w, _, _, mips = struct.unpack_from('<5I', b, 12)
    assert b[84:88] == b'DX10' and struct.unpack_from('<I', b, 128)[0] in (77, 78), 'not a BC3 array'
    per = sum(((max(1, w >> k) + 3) // 4) * ((max(1, h >> k) + 3) // 4) * 16 for k in range(max(1, mips)))
    bw, bh = (w + 3) // 4, (h + 3) // 4
    blk = np.frombuffer(b, np.uint8, bw * bh * 16, 148 + layer * per).reshape(bh, bw, 16)
    out = np.zeros((bh * 4, bw * 4, 4), np.uint8)
    a0, a1 = blk[..., 0].astype(np.int32), blk[..., 1].astype(np.int32)
    bits = np.zeros((bh, bw), np.uint64)
    for k in range(6):
        bits |= blk[..., 2 + k].astype(np.uint64) << np.uint64(8 * k)
    pal = np.zeros((bh, bw, 8), np.int32)
    pal[..., 0], pal[..., 1] = a0, a1
    gt = a0 > a1
    for k in range(1, 7):
        pal[..., k + 1] = np.where(gt, ((7 - k) * a0 + k * a1) // 7, 0)
    for k in range(1, 5):
        pal[..., k + 1] = np.where(gt, pal[..., k + 1], ((5 - k) * a0 + k * a1) // 5)
    pal[..., 6] = np.where(gt, pal[..., 6], 0)
    pal[..., 7] = np.where(gt, pal[..., 7], 255)
    c0 = blk[..., 8].astype(np.int32) | (blk[..., 9].astype(np.int32) << 8)
    c1 = blk[..., 10].astype(np.int32) | (blk[..., 11].astype(np.int32) << 8)

    def rgb(c):
        r, g, bl = (c >> 11) & 31, (c >> 5) & 63, c & 31
        return np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (bl << 3) | (bl >> 2)], -1)
    p0, p1 = rgb(c0), rgb(c1)
    cp = np.stack([p0, p1, (2 * p0 + p1) // 3, (p0 + 2 * p1) // 3], -2)
    idx = blk[..., 12:16].astype(np.uint32)
    cbits = idx[..., 0] | (idx[..., 1] << 8) | (idx[..., 2] << 16) | (idx[..., 3] << 24)
    ii, jj = np.meshgrid(np.arange(bh), np.arange(bw), indexing='ij')
    for py in range(4):
        for px in range(4):
            t = py * 4 + px
            ai = ((bits >> np.uint64(3 * t)) & np.uint64(7)).astype(np.int64)
            ci = ((cbits >> (2 * t)) & 3).astype(np.int64)
            out[py::4, px::4, 3] = np.take_along_axis(pal, ai[..., None], -1)[..., 0]
            out[py::4, px::4, :3] = cp[ii, jj, ci]
    return out[:h, :w]


def _source(s):
    s = s.lower().replace('/', BS)
    k = s.rfind('materials' + BS)
    return s[k:] if k >= 0 else s


class Trees(object):
    def __init__(self, lodi):
        self.dir = os.path.dirname(os.path.abspath(lodi))
        self.ws = os.path.splitext(os.path.basename(lodi))[0]
        self.L = D.read_lodo(os.path.join(self.dir, self.ws + '.lodo'))
        self.T = D.read_lodi(lodi)
        self.bases = {i for i, b in enumerate(self.L['bases']) if b['flags'] & 1}
        self._mesh, self._img, self._cut, self._cov = {}, {}, {}, {}
        self._layers()

    # --- the texture layer of a material, from the LOD folder's own text files
    def _layers(self):
        L = self.L
        side = {}
        p = os.path.join(self.dir, 'Objects', self.ws + '.LodgenArrays.txt')
        if os.path.exists(p):
            for ln in open(p, encoding='utf-8', errors='replace'):
                c = ln.rstrip('\n').split(' ')
                if not ln.startswith('#') and len(c) >= 10:
                    side.setdefault(_source(c[-2]), (c[1], int(c[2])))
        mats = set()
        for i in self.bases:
            for r in self.slots(i):
                mats |= set(self.mesh(r)[2].tolist())
        self.lay, want = {}, {}
        for m in sorted(mats):
            s = _source(L['string_at'](L['materials'][m]['lodmStringOffset']))
            if s in side:
                self.lay[m] = side[s]
            else:
                want.setdefault(s, []).append(m)
        names = sorted(f for f in os.listdir(self.dir) if f.endswith('.BTO.manifest.txt')) if want else []
        for f in names:
            if not want:
                break
            M, A = {}, {}
            for ln in open(os.path.join(self.dir, f), encoding='utf-8', errors='replace'):
                if ln.startswith('M '):
                    c = ln.rstrip('\n').split(' ', 2)
                    M[c[1]] = _source(c[2])
                elif ln.startswith('A '):
                    c = ln.rstrip('\n').split(' ', 3)
                    A[c[1]] = (int(c[2]), c[3])
            for blk, name in M.items():
                if name in want and blk in A and A[blk][0] >= 0:
                    for m in want.pop(name):
                        self.lay[m] = (A[blk][1].rsplit('.', 2)[-2], A[blk][0])
        self.materials = len(mats)
        self.no_layer = sorted(m for ms in want.values() for m in ms)

    def slots(self, base):
        b = self.L['bases'][base]
        return [b['rep%d' % k] for k in range(4) if b['rep%d' % k] != NO]

    def mesh(self, mi):
        """the full-detail triangles of mesh mi: pos [n,3,3] model space, uv [n,3,2], material [n]."""
        if mi not in self._mesh:
            L = self.L
            m = L['meshes'][mi]
            amin, aext, umin, uext = (np.array(m[k], np.float64) for k in ('aabbMin', 'aabbExtent', 'uvMin', 'uvExtent'))
            P, U, M = [], [], []
            for ci in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount']):
                if L['clusterLods'][ci]['level'] != 0:
                    continue
                c = L['clusters'][ci]
                li = L['localIndices'][ci * 48:ci * 48 + c['triangleCount'] * 3]
                vs = L['vertices'][c['vertexBase']:c['vertexBase'] + c['vertexCount']]
                vp = np.array([(v['px'], v['py'], v['pz']) for v in vs], np.float64) / 65535.0 * aext + amin
                vu = np.array([(v['u'], v['v']) for v in vs], np.float64) / 65535.0 * uext + umin
                for t in range(c['triangleCount']):
                    i = list(li[t * 3:t * 3 + 3])
                    P.append(vp[i])
                    U.append(vu[i])
                    M.append(c['materialId'])
            self._mesh[mi] = (np.array(P).reshape(-1, 3, 3), np.array(U).reshape(-1, 3, 2), np.array(M, np.int64))
        return self._mesh[mi]

    def image(self, mat):
        if mat not in self._img:
            self._img[mat] = None
            if mat in self.lay:
                cls, lay = self.lay[mat]
                im = bc3_layer(os.path.join(self.dir, 'Objects', '%s.LodgenArrays.%s_d.DDS' % (self.ws, cls)), lay)
                self._img[mat] = (srgb_lin(im[..., :3].astype(np.float64)), im[..., 3])
        return self._img[mat]

    def sample(self, mat, uv):
        """uv [n,2] -> (linear rgb [n,3], passes [n])."""
        img = self.image(mat)
        thr = self.L['materials'][mat]['alphaThreshold']
        if img is None:
            return np.tile(np.array([0.07, 0.05, 0.03]), (len(uv), 1)), np.ones(len(uv), bool)
        rgb, a = img
        h, w = a.shape
        x = np.floor((uv[:, 0] % 1.0) * w).astype(int) % w
        y = np.floor((uv[:, 1] % 1.0) * h).astype(int) % h
        return rgb[y, x], (a[y, x] >= thr) if thr else np.ones(len(uv), bool)

    @staticmethod
    def mirror(U, M):
        """the repetition breaker's mirror: about the middle of each material's own U range."""
        U = U.copy()
        for m in set(M.tolist()):
            s = M == m
            U[s, :, 0] = (U[s, :, 0].min() + U[s, :, 0].max()) - U[s, :, 0]
        return U

    def cut(self, mi, mirrored, kmax):
        """mesh mi with its alpha-tested triangles cut into pieces, a piece kept when its middle passes:
        (pos [n,3,3], linear rgb [n,3], the source triangle of each piece [n])."""
        key = (mi, mirrored, kmax)
        if key in self._cut:
            return self._cut[key]
        P, U, M = self.mesh(mi)
        U = self.mirror(U, M) if mirrored else U
        oP, oC, oT = [np.zeros((0, 3, 3))], [np.zeros((0, 3))], [np.zeros(0, np.int64)]
        for t in range(len(P)):
            m = int(M[t])
            img = self.image(m)
            k = 1
            if self.L['materials'][m]['alphaThreshold'] and img is not None:
                h, w = img[1].shape
                e = max(np.abs((U[t, a] - U[t, b]) * (w, h)).max() for a, b in ((0, 1), (1, 2), (2, 0)))
                k = int(np.clip(np.ceil(e / 3.0), 1, kmax))
            sub = []
            for i in range(k):
                for j in range(k - i):
                    sub.append(((i, j), (i + 1, j), (i, j + 1)))
                    if j < k - i - 1:
                        sub.append(((i + 1, j), (i + 1, j + 1), (i, j + 1)))
            sb = np.array(sub, np.float64) / k
            sp = P[t, 0] + sb[..., :1] * (P[t, 1] - P[t, 0]) + sb[..., 1:] * (P[t, 2] - P[t, 0])
            su = U[t, 0] + sb[..., :1] * (U[t, 1] - U[t, 0]) + sb[..., 1:] * (U[t, 2] - U[t, 0])
            rgb, ok = self.sample(m, su.mean(1))
            oP.append(sp[ok])
            oC.append(rgb[ok])
            oT.append(np.full(int(ok.sum()), t, np.int64))
        self._cut[key] = (np.concatenate(oP), np.concatenate(oC), np.concatenate(oT))
        return self._cut[key]

    def kept(self, mi, mirrored, kmax=8):
        """per authored triangle of mesh mi: the share of its area the cut keeps."""
        key = (mi, mirrored, kmax)
        if key not in self._cov:
            P = self.mesh(mi)[0]
            cp, _, ct = self.cut(mi, mirrored, kmax)
            a = area(cp)
            full = np.maximum(area(P), 1e-12)
            self._cov[key] = np.bincount(ct, a, len(P)) / full
        return self._cov[key]

    def placed(self, x0, y0, x1, y1):
        out = []
        for r in self.T['instances']:
            if r['baseId'] in self.bases and self.slots(r['baseId']):
                cx, cy = int(np.floor(r['x'] / CELL)), int(np.floor(r['y'] / CELL))
                if x0 <= cx <= x1 and y0 <= cy <= y1:
                    out.append(r)
        return out

    def world(self, r, P):
        m = np.array(r['m']).reshape(3, 3)
        return np.array([r['x'], r['y'], r['z']]) + (P * r['scaleF']) @ m.T

    def far_slot(self, r):
        return self.slots(r['baseId'])[-1]     # the coarsest authored slot: what the far map takes

    def reference(self, placed, kmax):
        oP, oC = [], []
        for r in placed:
            P, C, _ = self.cut(self.slots(r['baseId'])[0], bool(r['flags'] & 1), kmax)
            oP.append(self.world(r, P))
            oC.append(C)
        return np.concatenate(oP), np.clip(np.rint(np.concatenate(oC) * 255.0), 0, 255).astype(np.uint8)


def area(P):
    return np.linalg.norm(np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]), axis=1) / 2.0


def check(T, placed, tree_tris, probes, hoist, fails):
    """the soup's tree triangles against the LOD data: (text, canopy tops by cell)."""
    from scipy.spatial import cKDTree
    eC, eTree, eArea, tops = [], [], 0.0, {}
    for n, r in enumerate(placed):
        mi = T.far_slot(r)
        W = T.world(r, T.mesh(mi)[0])
        eC.append(W.mean(1))
        eTree.append(np.full(len(W), n, np.int64))
        eArea += float((area(W) * T.kept(mi, bool(r['flags'] & 1))).sum())
        # the canopy: every authored vertex of EVERY slot (the coarsest is in places taller than the first)
        v = np.concatenate([T.world(r, T.mesh(s)[0]).reshape(-1, 3) for s in sorted(set(T.slots(r['baseId'])))])
        for (cx, cy), z in zip(map(tuple, np.floor(v[:, :2] / CELL).astype(int)), v[:, 2]):
            if z > tops.get((cx, cy), -1e30):
                tops[(cx, cy)] = z
    eC = np.concatenate(eC) if eC else np.zeros((0, 3))
    eTree = np.concatenate(eTree) if eTree else np.zeros(0, np.int64)
    sC = tree_tris.mean(1) if len(tree_tris) else np.zeros((0, 3))
    found, stray, twice = 0, len(sC), 0
    if len(eC) and len(sC):
        ek, sk = cKDTree(eC), cKDTree(sC)
        stray = int((ek.query_ball_point(sC, TOL, return_length=True) == 0).sum())
        n_soup = sk.query_ball_point(eC, TOL, return_length=True)
        n_exp = ek.query_ball_point(eC, TOL, return_length=True)
        twice = int((n_soup > n_exp).sum())
        found = len(set(eTree[n_soup > 0].tolist()))
    sArea = float(area(tree_tris).sum()) if len(tree_tris) else 0.0
    if found != len(placed):
        fails.append('trees: %d of the %d in the LOD data are in the soup' % (found, len(placed)))
    if stray:
        fails.append('trees: %d soup triangles stand on no authored triangle of a placed tree' % stray)
    if twice:
        fails.append('trees: %d authored triangles are in the soup more than once' % twice)
    ratio = sArea / eArea if eArea > 0 else (1.0 if sArea == 0 else float('inf'))
    if abs(ratio - 1.0) > AREA_TOL:
        fails.append('trees: soup area %.3f of what the textures keep (want within %.2f of 1)' % (ratio, AREA_TOL))
    pc = np.floor(probes[:, :2] / CELL).astype(int)
    gaps = np.array([probes[i, 2] - tops[tuple(pc[i])] for i in range(len(probes)) if tuple(pc[i]) in tops])
    worst = float(gaps.min()) if len(gaps) else float('inf')
    if worst < hoist - 1.0:
        fails.append('canopy: a probe stands %.0f over its cell\'s tallest tree (want >= %.0f)' % (worst, hoist))
    return ('trees %d of %d in the soup, each once (stray %d, twice %d), %d triangles, area %.3f of the cut; '
            'canopy worst %.0f over %d wooded cells' % (found, len(placed), stray, twice, len(tree_tris), ratio,
                                                         worst, len(gaps)))
