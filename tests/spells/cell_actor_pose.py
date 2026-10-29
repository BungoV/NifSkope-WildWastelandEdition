"""Lane ACTORPOSE1 -- the pose a placed actor stands (or lies) in, rebuilt without NifSkope's code.

src/cellactor.cpp (header: THE POSE) poses the skeleton before it skins the parts. This file is the checker's own
reading of the same rule, on the game's files: the GNRL archive read with the standard library, skeleton.hkx and the
clips decoded by tests/spells/hkxanim_decode.py, skeleton.nif by tests/spells/gltf_nifread.py.

  RAGDOLL  a dead-on-start actor with XRGD: one 28-byte entry per body of skeleton.hkx's ragdoll skeleton, in a
           depth-first walk (children in index order). Each body is the animation bone at its bind position (the
           nearest within half a unit, a tie keeps the first). The entry's rotation, Rx(-x) Ry(-y) Rz(-z) (the
           reference rotation's convention), replaces the bone's local rotation; its offset is the bone's own times
           the actor's height, the self-check: at least 4 in 5 offsets fit one scale (5% + 0.5 unit), else refused.
           Entry 0 sets the root outright.
  IDLE     anything else: frame 0 of the first clip that exists of CANDIDATES under the skeleton's actor folder;
           tracks named through <folder>\\characterassets\\skeleton.hkx.
  BIND     neither: the skeleton's bind pose.
A bone only a part has rides its nearest ancestor the skeleton names, by that ancestor's move from the part's own
node (posed only; the bind pose keeps the part's own node).
Measured on Fallout4.esm (notes/actorpose1): the hkx reference pose equals skeleton.nif's bind to 0.0016 unit and
0.033 deg; the XRGD order and convention from 7 Concord corpses (knees and elbows bend about one local axis).
"""

import math
import os
import struct
import sys
import zlib

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hkxanim_decode as hd  # noqa: E402

CANDIDATES = ('animations\\mt\\neutral\\posea_idle1.hkx', 'animations\\idle.hkx', 'animations\\idle_mt.hkx',
              'animations\\mt\\idle.hkx', 'animations\\{leaf}_idle.hkx')
DEATH_CANDIDATES = ('animations\\deathpose1.hkx',)
XRGD_ENTRY = 28
XRGD_UNSETTLED = 4   # bodies within a quarter degree of their bind rotation: a ragdoll never settled


# ---- the archive -------------------------------------------------------------------------------------------

class Archive:
    def __init__(self, path):
        self.path = path
        with open(path, 'rb') as f:
            _m, _v, _k, num, nto = struct.unpack('<4sIIIQ', f.read(24))
            self.recs = [struct.unpack('<I4sIIQIII', f.read(36)) for _ in range(num)]
            f.seek(nto)
            names = []
            for _ in range(num):
                n = struct.unpack('<H', f.read(2))[0]
                names.append(f.read(n).decode('latin-1'))
        self.index = {n.lower().replace('/', '\\'): i for i, n in enumerate(names)}
        self._hkx = {}

    def has(self, name):
        return name.lower().replace('/', '\\') in self.index

    def get(self, name):
        i = self.index.get(name.lower().replace('/', '\\'))
        if i is None:
            return None
        off, packed, size = self.recs[i][4], self.recs[i][5], self.recs[i][6]
        with open(self.path, 'rb') as f:
            f.seek(off)
            data = f.read(packed if packed else size)
        return zlib.decompress(data) if packed else data

    def hkx(self, name):
        """hkxanim_decode's parse of an archived .hkx (it reads a path: a scratch copy beside this process)."""
        if name not in self._hkx:
            blob = self.get(name)
            if blob is None:
                self._hkx[name] = None
            else:
                tmp = os.environ.get('TMPDIR') or os.environ.get('TEMP') or '.'
                p = os.path.join(tmp, 'cap_%d_%s' % (os.getpid(), name.replace('\\', '_')))
                with open(p, 'wb') as fh:
                    fh.write(blob)
                try:
                    self._hkx[name] = hd.parse_hkx(p)
                finally:
                    os.remove(p)
        return self._hkx[name]


def actor_folder(skeleton):
    s = skeleton.lower().replace('/', '\\')
    if s.startswith('meshes\\'):
        s = s[7:]
    k = s.find('\\characterassets\\')
    return 'meshes\\' + s[:k] if k > 0 else None


# ---- transforms as (R, t, s) ---------------------------------------------------------------------------------

def qmat(q):
    """Havok (x, y, z, w) -> 3x3 (column vectors); the NiNode rotation as it stands."""
    x, y, z, w = q
    n = math.sqrt(x * x + y * y + z * z + w * w) or 1.0
    x, y, z, w = x / n, y / n, z / n, w / n
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def euler_xyz(x, y, z):
    """NifSkope's Matrix::fromEuler: Rx(x) Ry(y) Rz(z)."""
    sx, cx, sy, cy, sz, cz = math.sin(x), math.cos(x), math.sin(y), math.cos(y), math.sin(z), math.cos(z)
    return np.array([[cy * cz, -cy * sz, sy],
                     [sx * sy * cz + sz * cx, cx * cz - sx * sy * sz, -sx * cy],
                     [sx * sz - cx * sy * cz, cx * sy * sz + sx * cz, cx * cy]])


def compose(a, b):
    return a[0] @ b[0], a[0] @ b[1] * a[2] + a[1], a[2] * b[2]


def inverse(a):
    R = a[0].T
    return R, -(R @ a[1]) / a[2], 1.0 / a[2]


def nif_nodes(nif):
    """[(name, parent index or -1, (R, t, s) local)] for every node of a gltf_nifread.Nif, block order."""
    ids = sorted(nif.nodes)
    at = {b: k for k, b in enumerate(ids)}
    out = []
    for b in ids:
        nd = nif.nodes[b]
        out.append((nd['name'], at.get(nd['parent'], -1) if nd['parent'] is not None else -1,
                    (np.array(nd['r'], dtype=np.float64).reshape(3, 3), np.array(nd['t'], dtype=np.float64),
                     float(nd['s']))))
    return out


def posed(nodes, local, root_name=None, root_world=None):
    """name -> (R, t, s): node transforms with locals replaced (lower-case name -> local), first name wins."""
    w = [None] * len(nodes)

    def at(i, depth=0):
        if w[i] is None:
            name, par, loc = nodes[i]
            low = name.lower()
            if root_name is not None and low == root_name:
                w[i] = root_world
            else:
                loc = local.get(low, loc)
                w[i] = compose(at(par, depth + 1), loc) if par >= 0 and depth < 256 else loc
        return w[i]
    out = {}
    for i, (name, _, _) in enumerate(nodes):
        at(i)
        if name and name not in out:
            out[name] = w[i]
    return out


def hk_bind_positions(sk):
    w = []
    for i in range(len(sk['boneNames'])):
        t, q, s = sk['referencePose'][i]
        loc = (qmat(q), np.array(t[:3], dtype=np.float64), 1.0)
        p = sk['parents'][i]
        w.append(compose(w[p], loc) if 0 <= p < i else loc)
    return [x[1] for x in w]


# ---- the two poses -----------------------------------------------------------------------------------------

def idle_pose(arc, skeleton, nodes):
    """(name -> world, clip) or (None, why)."""
    return clip_pose(arc, skeleton, nodes, CANDIDATES, 'no idle clip')


def death_pose(arc, skeleton, nodes):
    return clip_pose(arc, skeleton, nodes, DEATH_CANDIDATES, 'no death pose')


def clip_pose(arc, skeleton, nodes, candidates, none):
    """frame 0 of the first clip of `candidates` that exists: (name -> world, clip) or (None, why)."""
    folder = actor_folder(skeleton)
    if folder is None:
        return None, 'skeleton outside a CharacterAssets folder'
    bones = arc.hkx(folder + '\\characterassets\\skeleton.hkx')
    if not bones or not bones['skeletons']:
        return None, 'no skeleton.hkx'
    sk = bones['skeletons'][0]
    leaf = folder.rsplit('\\', 1)[1]
    for c in candidates:
        rel = folder + '\\' + c.format(leaf=leaf)
        if not arc.has(rel):
            continue
        try:
            r = arc.hkx(rel)
            frames, _ = hd.decode(r['animations'][0])
        except Exception:  # noqa: BLE001  (a refusal of the decoder is the C++'s "does not load")
            return None, 'clip does not load'
        bind = r['bindings'][0]['transformTrackToBoneIndices'] if r['bindings'] else []
        local = {}
        for tr, (t, q, s, _) in enumerate(frames[0]):
            bone = bind[tr] if tr < len(bind) else tr
            if 0 <= bone < len(sk['boneNames']):
                local[sk['boneNames'][bone].lower()] = (qmat(q), np.array(t[:3], dtype=np.float64), float(s[0]))
        return posed(nodes, local), rel[7:]
    return None, none


def ragdoll_pose(arc, skeleton, nodes, xrgd):
    """(name -> world, 'ragdoll N bodies, F fit') or (None, why)."""
    folder = actor_folder(skeleton)
    bones = arc.hkx(folder + '\\characterassets\\skeleton.hkx') if folder else None
    if not bones or len(bones['skeletons']) < 2:
        return None, 'no ragdoll skeleton'
    anim, rag = bones['skeletons'][0], bones['skeletons'][1]
    anim_at, rag_at = hk_bind_positions(anim), hk_bind_positions(rag)
    order = []

    def walk(i, depth):
        order.append(i)
        for c in range(len(rag['parents'])):
            if c != i and rag['parents'][c] == i and depth < 64:
                walk(c, depth + 1)
    for i, p in enumerate(rag['parents']):
        if p < 0:
            walk(i, 0)
    n = len(order)
    if n < 2 or len(xrgd) < XRGD_ENTRY * n:
        return None, 'ragdoll data has %d entries for %d bodies' % (len(xrgd) // XRGD_ENTRY, n)
    node_of = {}
    for i, (name, _, _) in enumerate(nodes):
        node_of.setdefault(name.lower(), i)
    node = []
    for k in range(n):
        best, bd = -1, 0.5
        for j in range(len(anim_at)):
            dd = float(np.linalg.norm(anim_at[j] - rag_at[order[k]]))
            if dd < bd - 1e-4:
                best, bd = j, dd
        m = node_of.get(anim['boneNames'][best].lower(), -1) if best >= 0 else -1
        if m < 0:
            return None, 'a ragdoll body has no bone'
        node.append(m)

    def entry(k):
        e = struct.unpack_from('<3f3f', xrgd, XRGD_ENTRY * k + 4)
        return np.array(e[:3], dtype=np.float64), e[3:]
    own = [float(np.linalg.norm(nodes[node[k]][2][1])) for k in range(n)]
    ratio = [float(np.linalg.norm(entry(k)[0])) / own[k] for k in range(1, n) if own[k] > 1.0]
    if not ratio:
        return None, 'ragdoll data does not fit the skeleton'
    s = sorted(ratio)[len(ratio) // 2]
    fit = sum(abs(float(np.linalg.norm(entry(k)[0])) - s * own[k]) < 0.05 * s * own[k] + 0.5 for k in range(1, n))
    if fit * 5 < (n - 1) * 4 or not 0.25 <= s <= 4.0:
        return None, 'ragdoll data does not fit the skeleton: %d of %d bone lengths' % (fit, n - 1)
    local, at_rest = {}, 0
    for k in range(1, n):
        R0, t0, s0 = nodes[node[k]][2]
        R = euler_xyz(*(-v for v in entry(k)[1]))
        at_rest += float(np.trace(R0.T @ R)) > 1.0 + 2.0 * math.cos(math.radians(0.25))
        local[nodes[node[k]][0].lower()] = (R, t0, s0)
    if at_rest >= XRGD_UNSETTLED:
        return None, 'ragdoll data never settled: %d of %d bodies at rest' % (at_rest, n - 1)
    pos, rot = entry(0)
    root = (euler_xyz(*(-v for v in rot)), pos / s, 1.0)
    return posed(nodes, local, nodes[node[0]][0].lower(), root), 'ragdoll %d bodies, %d fit' % (n, fit)


def pose_for(arc, skeleton, nodes, dead, xrgd, red='', rot=(0.0, 0.0, 0.0)):
    """(kind 'idle' / 'ragdoll' / 'death' / 'bind', why, name -> world or None for the bind pose)."""
    if red == 'bind':
        return 'bind', 'red control', None
    rag_why = ''
    if dead and xrgd:
        w, why = ragdoll_pose(arc, skeleton, nodes, xrgd)
        if w is not None:
            return 'ragdoll', why, w
        rag_why = why
    if dead:
        w, why = death_pose(arc, skeleton, nodes)
        if w is not None:
            if rot[0] != 0.0 or rot[1] != 0.0:
                # the placement turns by the whole reference rotation; take its tilt back out
                up = (euler_xyz(-rot[0], -rot[1], -rot[2]).T @ euler_xyz(0.0, 0.0, -rot[2]), np.zeros(3), 1.0)
                w = {k: compose(up, v) for k, v in w.items()}
            return 'death', (rag_why + ', then ' if rag_why else '') + why, w
        rag_why = rag_why + ', then ' + why if rag_why else why
    w, why = idle_pose(arc, skeleton, nodes)
    if rag_why:
        why = rag_why + ', then ' + why
    return ('idle', why, w) if w is not None else ('bind', why, None)


def part_only_bone(part, block, rest, moved, node_world):
    """A bone only the part has: its own node, or (posed) ridden by its nearest ancestor the skeleton names."""
    own = node_world(part, block)
    if not moved:
        return own
    i = part.nodes[block]['parent']
    hop = 0
    while i is not None and hop < 256:
        nm = part.nodes[i]['name']
        if nm in rest:
            return compose(compose(rest[nm], inverse(node_world(part, i))), own)
        i = part.nodes[i]['parent']
        hop += 1
    # no named ancestor (a face mesh hangs every bone off its root): the nearest node the skeleton names
    best, bd, bw = None, 0.0, None
    for b in sorted(part.nodes):
        if b == block or part.nodes[b]['name'] not in rest:
            continue
        w = node_world(part, b)
        dd = float(np.linalg.norm(np.asarray(w[1]) - np.asarray(own[1])))
        if best is None or dd < bd:
            best, bd, bw = b, dd, w
    if best is not None:
        return compose(compose(rest[part.nodes[best]['name']], inverse(bw)), own)
    return own
