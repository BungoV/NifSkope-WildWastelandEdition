"""IMPOSTORFIX4 instrument.

IMPOSTORFIX2's `inst.py` with ONE thing changed: the fixture root and the
control-grab root are parameters instead of constants, so the same instrument
can read

  * impostorfix3_20260919/fixture/      the 8-ring bake that ships today
  * impostorfix3_20260919/fixture_r3/   the R3 control
  * impostorfix1_20260919/fixture/      the sheets IMPOSTORFIX1 measured 0.8823 on

and the matching `control/<tag>_<which>_b1/` grab folders.

Nothing about the RENDER changed. `render()` below is byte-for-byte
IMPOSTORFIX2's, plus the extra ablation switches this lane needs (covFloorOn,
dilateOff, mip, unionOff) which all DEFAULT to the shipping behaviour, so a
call with no switches is the old instrument exactly.
"""
import os, sys, glob, json, math
import numpy as np
from PIL import Image

SCR = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad'
R1  = SCR + '/impostorfix1_20260919'
R3  = SCR + '/impostorfix3_20260919'
sys.path.insert(0, R1)
sys.path.insert(0, SCR + '/impostorfix2_20260919')
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
from bcdec2 import load_dds   # IMPOSTORFIX2's corrected decoder

CLEAR = np.array([43, 45, 49], int)
AZ = list(range(0, 360, 30))
ELS = (15, 45)
VIEWS = [(az, el) for el in ELS for az in AZ]

def grabmask(root, tag, which, az, el, kind):
    p = '%s/control/%s_%s_b1/v_az%03d_el%02d_%s.png' % (root, tag, which, az, el, kind)
    if not os.path.exists(p):
        return None
    a = np.asarray(Image.open(p).convert('RGB')).astype(int)
    bg = (np.abs(a - CLEAR) <= 12).all(-1)
    return ~bg

def grabmask_named(folder, az, el, kind):
    p = '%s/v_az%03d_el%02d_%s.png' % (folder, az, el, kind)
    if not os.path.exists(p):
        return None
    a = np.asarray(Image.open(p).convert('RGB')).astype(int)
    bg = (np.abs(a - CLEAR) <= 12).all(-1)
    return ~bg

def dirOf(az, el):
    a, e = math.radians(az), math.radians(el)
    return np.array([math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e)])

class Sheets:
    """One fixture's decoded sheets + .lodm metadata.
       root  -- a folder holding fixture/<tag>/<which>/
       which -- 'cards' | 'cards_before' | 'cards_r3'
       sub   -- the fixture subfolder name ('fixture' or 'fixture_r3')"""
    def __init__(self, tag, which='cards', root=R3, sub='fixture'):
        self.tag, self.which, self.root, self.sub = tag, which, root, sub
        d = '%s/%s/%s/%s/' % (root, sub, tag, which)
        self.dir = d
        raw = open(glob.glob(d + '*_oct.lodm')[0], 'rb').read()
        j = json.loads(raw[raw.index(b'{'):].decode('utf-8'))['card']
        self.meta = j
        self.N = j['oct']; self.half = np.array(j['half'], float)
        self.span = float(j['depthSpan'])
        self.center = np.array(j.get('center', [0, 0, 0]), float)
        c = j.get('coverage', {})
        self.covFloorI = c.get('floor', 0); self.covBaseI = c.get('base', 0)
        self.covFloor = self.covFloorI / 255.0
        self.covBase = self.covBaseI / 255.0
        fo = j.get('frameOffset', None)
        self.foff = np.array(fo, float).reshape(self.N * self.N, 2) if fo else np.zeros((self.N * self.N, 2))
        tex = '%s/%s/%s/textures/data/fo4cslod/cards/' % (root, sub, tag)
        def pick(pat):
            g = glob.glob(d + pat) + glob.glob(d + pat.upper()) + glob.glob(tex + pat) + glob.glob(tex + pat.upper())
            g = [x for x in g if os.path.exists(x)]
            if not g:
                raise IOError('no sheet %s under %s or %s' % (pat, d, tex))
            return g[0]
        self.albPath = pick('*_oct_d.dds'); self.nrmPath = pick('*_oct_n.dds')
        self.alb = load_dds(self.albPath)[0]
        self.nrm = load_dds(self.nrmPath)[0]
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

def coverageOf(a, cs, floorOn=True):
    """The shader's `coverageOf`. floorOn=False is the ABLATION: the same linear
       remap WITHOUT clamping up to covFloor, i.e. a texel whose measured
       coverage is 1/255 decodes to 1/255 instead of to 16/255."""
    if cs.covBase <= 0:
        return np.where(a < 16 / 255.0, 0.0, a)
    lo = cs.covFloor if floorOn else 0.0
    out = np.clip(cs.covFloor + (a - cs.covBase) * (1 - cs.covFloor) / (1 - cs.covBase), lo, 1.0)
    return np.where(a < cs.covBase, 0.0, out)

def render(cs, d, res, *, parallax=True, nframes=3, useOffset=True, thresh=None,
           wpow=1.0, nearestOnly=False, reject=None, ortho=True,
           covFloorOn=True, mip=0, albOverride=None, nrmOverride=None,
           returnParts=False):
    """Alpha over the card quad, x right, y up, res=(W,H) PIXELS of the quad.

       The defaults are the SHIPPING shader. The extras:
         covFloorOn -- False ablates the 16/255 coverage floor (defect 1 stage)
         mip        -- integer box-downsample of BOTH sheets before sampling,
                       the offline stand-in for the drawer's mip choice
         albOverride/nrmOverride -- substitute decoded sheets (stage ablations)
         returnParts -- also return per-frame coverage stack (for the union test)
    """
    W, H = res
    alb = cs.alb if albOverride is None else albOverride
    nrm = cs.nrm if nrmOverride is None else nrmOverride
    for _ in range(mip):
        alb = 0.25 * (alb[0::2, 0::2] + alb[1::2, 0::2] + alb[0::2, 1::2] + alb[1::2, 1::2])
        nrm = 0.25 * (nrm[0::2, 0::2] + nrm[1::2, 0::2] + nrm[0::2, 1::2] + nrm[1::2, 1::2])
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
    parts = []
    thr = cs.covFloor if thresh is None else thresh
    for k, (i, j, _w0) in enumerate(frames):
        w = ws[k]
        if w <= 0:
            parts.append(np.zeros((H, W))); continue
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
            h = bilinear(nrm, uv[..., 0], uv[..., 1])[..., 2]
            want = -(h - 0.5) * cs.span
            denom = float(np.dot(ray, fk))
            if abs(denom) > 0.15:
                t = (want - dd) / denom
                Q = P + ray[None, None, :] * t[..., None]
                uv, dd = uvof(Q)
        a = bilinear(alb, uv[..., 0], uv[..., 1])[..., 3]
        cov = coverageOf(a, cs, floorOn=covFloorOn)
        hs = bilinear(nrm, uv[..., 0], uv[..., 1])[..., 2]
        if k == 0:
            hdom = hs
        elif reject is not None:
            bad = np.abs(hs - hdom) * 255.0 > reject
            cov = np.where(bad, 0.0, cov)
        parts.append(cov * w)
        alpha += cov * w
    if returnParts:
        return alpha >= thr, alpha, parts
    return alpha >= thr, alpha

def iou(a, b):
    u = (a | b).sum(); return (a & b).sum() / u if u else 0.0

def inkratio(card, mesh):
    m = mesh.sum()
    return card.sum() / m if m else float('nan')
