"""IMPOSTORFIX2 instrument: the numpy reference card projected into the HARNESS's
own 512x768 pixel grid, so the truth can be the real MESH GRAB and all five
subjects fit in one table.

Registration is fitted against the harness's own CARD grab (card vs card), never
against the mesh, and is then checked by reproducing the harness's printed IoU.
"""
import os, sys, glob, json, math
import numpy as np
from PIL import Image

R1 = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix1_20260919'
R2 = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919'
sys.path.insert(0, R1)
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
from bcdec2 import load_dds   # NOT tests/spells/impostor_bc_decode.py: its alpha ramp is wrong (report 0c)

CLEAR = np.array([43, 45, 49], int)
AZ = list(range(0, 360, 30))
ELS = (15, 45)
VIEWS = [(az, el) for el in ELS for az in AZ]

def grabmask(tag, which, az, el, kind):
    p = '%s/control/%s_%s_b1/v_az%03d_el%02d_%s.png' % (R1, tag, which, az, el, kind)
    if not os.path.exists(p):
        return None
    a = np.asarray(Image.open(p).convert('RGB')).astype(int)
    bg = (np.abs(a - CLEAR) <= 12).all(-1)
    return ~bg

def dirOf(az, el):
    a, e = math.radians(az), math.radians(el)
    return np.array([math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e)])

# ---------------------------------------------------------------- the sheets
class Sheets:
    """One fixture's decoded sheets + .lodm metadata. `which` = 'cards' (repaired)
       or 'cards_before' (the shipped DDS)."""
    def __init__(self, tag, which='cards'):
        self.tag, self.which = tag, which
        d = '%s/fixture/%s/%s/' % (R1, tag, which)
        raw = open(glob.glob(d + '*_oct.lodm')[0], 'rb').read()
        j = json.loads(raw[raw.index(b'{'):].decode('utf-8'))['card']
        self.N = j['oct']; self.half = np.array(j['half'], float)
        self.span = float(j['depthSpan'])
        c = j.get('coverage', {})
        self.covFloor = c.get('floor', 0) / 255.0
        self.covBase = c.get('base', 0) / 255.0
        fo = j.get('frameOffset', None)
        self.foff = np.array(fo, float).reshape(self.N * self.N, 2) if fo else np.zeros((self.N * self.N, 2))
        self.alb = load_dds(glob.glob(d + '*_oct_d.DDS')[0])[0]
        self.nrm = load_dds(glob.glob(d + '*_oct_n.DDS')[0])[0]
        self.H, self.W = self.alb.shape[:2]
        self.fw, self.fh = self.W // self.N, self.H // self.N
    def clone(self):
        import copy as _c
        o = _c.copy(self); o.nrm = self.nrm.copy(); o.alb = self.alb.copy(); return o

def normalise(v):
    n = np.linalg.norm(v); return v / n

def frameDir(i, j, N):
    u = i / (N - 1) * 2 - 1; v = j / (N - 1) * 2 - 1
    d = np.array([(u + v) / 2, (u - v) / 2, 0.0]); d[2] = 1 - abs(d[0]) - abs(d[1])
    return normalise(d)

def frameBasis(d):
    d = normalise(np.asarray(d, float))
    elev = math.asin(max(-1, min(1, d[2]))); azim = math.atan2(d[1], d[0])
    rx = math.radians(-90 + math.degrees(elev)); rz = math.radians(270 - math.degrees(azim))
    sX, cX = math.sin(rx), math.cos(rx); sZ, cZ = math.sin(rz), math.cos(rz)
    right = np.array([cZ, -sZ, 0.0]); up = np.array([sZ * cX, cX * cZ, -sX]); fwd = np.array([sX * sZ, sX * cZ, cX])
    return normalise(right), normalise(up), normalise(fwd)

def dirToGrid(d, N):
    e = normalise(np.array(d, float))
    if e[2] < 0: e[2] = 0; e = normalise(e)
    L = abs(e[0]) + abs(e[1]) + e[2]; x, y = e[0] / L, e[1] / L
    u, v = x + y, x - y
    a = (u + 1) * 0.5 * (N - 1); b = (v + 1) * 0.5 * (N - 1)
    return min(max(a, 0), N - 1), min(max(b, 0), N - 1)

def pickFrames(d, N):
    fi, fj = dirToGrid(d, N)
    i0 = min(max(int(math.floor(fi)), 0), N - 2); j0 = min(max(int(math.floor(fj)), 0), N - 2)
    a = fi - i0; b = fj - j0
    if a + b <= 1.0: F = [(i0, j0, 1 - a - b), (i0 + 1, j0, a), (i0, j0 + 1, b)]
    else:            F = [(i0 + 1, j0 + 1, a + b - 1), (i0, j0 + 1, 1 - a), (i0 + 1, j0, 1 - b)]
    return [(i, j, max(w, 0.0)) for i, j, w in F]

def bilinear(img, u, v):
    H, W = img.shape[:2]
    x = np.clip(u * W - 0.5, 0, W - 1); y = np.clip(v * H - 0.5, 0, H - 1)
    x0 = np.floor(x).astype(int); y0 = np.floor(y).astype(int)
    x1 = np.minimum(x0 + 1, W - 1); y1 = np.minimum(y0 + 1, H - 1)
    fx = (x - x0)[..., None]; fy = (y - y0)[..., None]
    return (img[y0, x0] * (1 - fx) * (1 - fy) + img[y0, x1] * fx * (1 - fy)
            + img[y1, x0] * (1 - fx) * fy + img[y1, x1] * fx * fy)

def coverageOf(a, cs):
    if cs.covBase <= 0: return np.where(a < 16 / 255.0, 0.0, a)
    out = np.clip(cs.covFloor + (a - cs.covBase) * (1 - cs.covFloor) / (1 - cs.covBase), cs.covFloor, 1.0)
    return np.where(a < cs.covBase, 0.0, out)

def render(cs, d, res, *, parallax=True, nframes=3, useOffset=True, thresh=None,
           wpow=1.0, nearestOnly=False, reject=None, ortho=True):
    """Alpha over the card quad, x right, y up, res=(W,H) PIXELS of the quad.
       wpow      -- blend weights raised to this power then renormalised (1 = spec)
       nearestOnly -- one frame, weight 1 (equivalent to nframes=1)
       reject    -- height-consistency rejection: drop a frame's sample where its
                    reprojected height disagrees with the DOMINANT frame's by more
                    than `reject` * (depthSpan / 255) ... expressed in LEVELS."""
    W, H = res
    xs = (np.arange(W) + 0.5) / W * 2 - 1; ys = 1 - (np.arange(H) + 0.5) / H * 2
    X, Y = np.meshgrid(xs * cs.half[0], ys * cs.half[1])
    rC, uC, fC = frameBasis(d)
    P = X[..., None] * rC + Y[..., None] * uC
    ray = -fC
    frames = sorted(pickFrames(d, cs.N), key=lambda t: -t[2])[:nframes]
    if nearestOnly or nframes == 1:
        frames = [(frames[0][0], frames[0][1], 1.0)]
    ws = np.array([f[2] for f in frames], float)
    if wpow != 1.0:
        ws = ws ** wpow
    s = ws.sum()
    ws = ws / s if s > 0 else ws
    alpha = np.zeros((H, W)); hdom = None
    thr = cs.covFloor if thresh is None else thresh
    for k, (i, j, _w0) in enumerate(frames):
        w = ws[k]
        if w <= 0: continue
        rk, uk, fk = frameBasis(frameDir(i, j, cs.N))
        off = cs.foff[j * cs.N + i] if useOffset else np.zeros(2)
        def uvof(Q):
            st = np.stack([Q @ rk - off[0], Q @ uk - off[1]], -1)
            dd = Q @ fk
            uv = np.stack([st[..., 0] / (2 * cs.half[0]) + 0.5, 0.5 - st[..., 1] / (2 * cs.half[1])], -1)
            uv = np.clip(uv, 0, 1)
            return (np.stack([(i + uv[..., 0]) / cs.N, (j + uv[..., 1]) / cs.N], -1), dd)
        uv, dd = uvof(P)
        if parallax:
            h = bilinear(cs.nrm, uv[..., 0], uv[..., 1])[..., 2]
            want = -(h - 0.5) * cs.span
            denom = float(np.dot(ray, fk))
            if abs(denom) > 0.15:
                t = (want - dd) / denom
                Q = P + ray[None, None, :] * t[..., None]
                uv, dd = uvof(Q)
        a = bilinear(cs.alb, uv[..., 0], uv[..., 1])[..., 3]
        cov = coverageOf(a, cs)
        hs = bilinear(cs.nrm, uv[..., 0], uv[..., 1])[..., 2]
        if k == 0:
            hdom = hs
        elif reject is not None:
            bad = np.abs(hs - hdom) * 255.0 > reject
            cov = np.where(bad, 0.0, cov)
        alpha += cov * w
    return alpha >= thr, alpha

def iou(a, b):
    u = (a | b).sum(); return (a & b).sum() / u if u else 0.0
