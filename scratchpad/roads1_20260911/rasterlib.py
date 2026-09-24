"""Top-down projection of placed Fallout 4 references onto a terrain-LOD texel
grid, written for lane ROADS1's MEASUREMENT of vanilla (report section 1).

Nothing here is shared with src/lodgen.cpp.  The NIF reading comes from
tests/spells/gltf_nifread.py (the glTF gates' independent reader), the ESM walk
from scratchpad/roads1_20260911/esm_refs.py, and the placement rotation is
Matrix::fromEuler( -rx, -ry, -rz ) re-typed from src/data/niftypes.cpp:215 with
the negation docs/LODGEN_PARITY.md proved per object.  Matrix * Vector3 in this
tree is m[row][col] on a COLUMN vector (niftypes.h:1005), so a numpy array of
ROW vectors V transforms as V.dot(M.T).
"""

import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from gltf_nifread import Nif                      # noqa: E402

SEP = chr(92)
CELL = 4096.0


def euler_matrix(rx, ry, rz):
    """Matrix::fromEuler( x, y, z ), src/data/niftypes.cpp:215, as m[row][col]."""
    sx, cx = math.sin(rx), math.cos(rx)
    sy, cy = math.sin(ry), math.cos(ry)
    sz, cz = math.sin(rz), math.cos(rz)
    return np.array([
        [cy * cz, -cy * sz, sy],
        [sx * sy * cz + sz * cx, cx * cz - sx * sy * sz, -sx * cy],
        [sx * sz - cx * sy * cz, cx * sy * sz + sx * cz, cx * cy],
    ], dtype=np.float64)


def ref_matrix(rot):
    return euler_matrix(-rot[0], -rot[1], -rot[2])


def compose(Rp, tp, sp, Rl, tl, sl):
    """parent o local, for transforms of the form v -> R*(s*v) + t."""
    return Rp.dot(Rl), Rp.dot(np.asarray(tl) * sp) + tp, sp * sl


class MeshCache(object):
    def __init__(self, dataRoot):
        self.root = dataRoot
        self.cache = {}
        self.missing = {}

    def path_for(self, modl):
        if not modl:
            return None
        p = modl.replace(SEP, '/').lstrip('/')
        if not p.lower().startswith('meshes/'):
            p = 'meshes/' + p
        full = os.path.join(self.root, p.replace('/', os.sep))
        if os.path.isfile(full):
            return full
        cur = self.root
        for part in p.split('/'):
            if not part:
                continue
            try:
                names = os.listdir(cur)
            except Exception:
                return None
            hit = None
            low = part.lower()
            for nm in names:
                if nm.lower() == low:
                    hit = nm
                    break
            if hit is None:
                return None
            cur = os.path.join(cur, hit)
        return cur if os.path.isfile(cur) else None

    def get(self, modl):
        """Shapes of one model, each already in MODEL space (the NiNode chain
        folded in): dict(v=(N,3), t=(M,3), uv=(N,2)|None, tex=str, name=str)."""
        key = (modl or '').lower()
        if key in self.cache:
            return self.cache[key]
        full = self.path_for(modl)
        if full is None:
            self.missing[key] = 'model file not found'
            self.cache[key] = []
            return []
        try:
            nif = Nif(full)
        except Exception as e:
            self.missing[key] = 'unreadable: %s' % e
            self.cache[key] = []
            return []
        out = []
        for idx, sh in nif.shapes.items():
            if not sh['verts'] or not sh['tris']:
                continue
            R, t, s = local_to_model(nif, sh)
            v = np.array(sh['verts'], dtype=np.float64)
            v = (v * s).dot(R.T) + t
            tri = np.array(sh['tris'], dtype=np.int32).reshape(-1, 3)
            uv = np.array(sh['uvs'], dtype=np.float64) if sh['uvs'] else None
            out.append({'v': v, 't': tri, 'uv': uv,
                        'tex': nif.diffuse_for(sh) or '', 'name': sh['name'],
                        'block': idx})
        self.cache[key] = out
        return out


def local_to_model(nif, shape):
    chain = []
    b = shape
    while True:
        chain.append(b)
        p = b.get('parent')
        if p is None or p not in nif.nodes:
            break
        b = nif.nodes[p]
    R, t, s = np.eye(3), np.zeros(3), 1.0
    for b in reversed(chain):           # root first
        Rl = np.array(b['r'], dtype=np.float64).reshape(3, 3)
        tl = np.array(b['t'], dtype=np.float64)
        R, t, s = compose(R, t, s, Rl, tl, float(b['s']))
    return R, t, s


class Grid(object):
    """A terrain-LOD texel grid over a world rectangle, row 0 NORTH."""

    def __init__(self, wx0, wy0, wx1, wy1, n):
        self.wx0, self.wy0, self.wx1, self.wy1, self.n = wx0, wy0, wx1, wy1, n
        self.upt = (wx1 - wx0) / float(n)

    def to_texel(self, xy):
        i = (xy[:, 0] - self.wx0) / self.upt
        j = (self.wy1 - xy[:, 1]) / self.upt
        return np.stack([i, j], axis=1)


def rasterise(grid, p, z, zbuf, idbuf, ident, attrs=None, abuf=None,
              alpha=None, alphaCut=None):
    """Top-down scan conversion with a MAXIMUM-z buffer: the topmost triangle
    covering a texel wins, which is what "seen from above" means.

    p:      (T,3,2) texel coordinates; z: (T,3) world z.
    attrs:  (T,3,K) per-vertex attributes, interpolated into abuf (list of K
            arrays).  alpha: (T,3) per-vertex alpha, texels below alphaCut are
            not written (an alpha-tested decal honours its own cut-out).
    """
    n = grid.n
    count = 0
    for k in range(p.shape[0]):
        q = p[k]
        i0 = max(int(math.floor(q[:, 0].min())), 0)
        i1 = min(int(math.ceil(q[:, 0].max())), n - 1)
        j0 = max(int(math.floor(q[:, 1].min())), 0)
        j1 = min(int(math.ceil(q[:, 1].max())), n - 1)
        if i1 < i0 or j1 < j0:
            continue
        X, Y = np.meshgrid(np.arange(i0, i1 + 1) + 0.5,
                           np.arange(j0, j1 + 1) + 0.5)
        ax, ay = q[0]
        bx, by = q[1]
        cx, cy = q[2]
        d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(d) < 1e-12:
            continue
        w0 = ((by - cy) * (X - cx) + (cx - bx) * (Y - cy)) / d
        w1 = ((cy - ay) * (X - cx) + (ax - cx) * (Y - cy)) / d
        w2 = 1.0 - w0 - w1
        inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if not inside.any():
            continue
        zz = w0 * z[k][0] + w1 * z[k][1] + w2 * z[k][2]
        sub = zbuf[j0:j1 + 1, i0:i1 + 1]
        take = inside & (zz > sub)
        if alpha is not None and alphaCut is not None:
            aa = w0 * alpha[k][0] + w1 * alpha[k][1] + w2 * alpha[k][2]
            take = take & (aa >= alphaCut)
        if not take.any():
            continue
        sub[take] = zz[take]
        idbuf[j0:j1 + 1, i0:i1 + 1][take] = ident
        if attrs is not None and abuf is not None:
            a = attrs[k]
            for c in range(a.shape[1]):
                vv = w0 * a[0, c] + w1 * a[1, c] + w2 * a[2, c]
                abuf[c][j0:j1 + 1, i0:i1 + 1][take] = vv[take]
        count += int(take.sum())
    return count
