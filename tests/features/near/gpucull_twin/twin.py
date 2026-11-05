"""lane FARTOGGLE2: a CPU twin of the GPU-driven step B occlusion cull (src/gl/cellgpucull.cpp) over a steady
camera's frames, on a synthetic far scene: does the cull make a far object toggle on and off?

What it mirrors (cellgpucull.cpp, origin/main f46b1c76):
  prepass   every occluder (here: the terrain and every object; the real list is every opaque shape learned from
            its own draw state, which the cull's result never feeds) drawn with glPolygonOffset(1, 2) (~965)
  Hi-Z      level 0 = the farthest sample of each pixel, each level the farthest of its 2x2, sizes (w + 1) / 2 (kHizN)
  cull      kCull's occlusion branch op for op: the sphere's bounding cube's 8 corners projected, the box one pixel
            wider, the level whose texel spans it, the nearest view depth c.z + r (red `hard`: c.z) at (0, 0, z)
            through the same projection, culled when that depth > Hi-Z max + 4e-6
  the frame TAA's sub-pixel jitter (Halton 2,3) in the projection, the same in the prepass, the cull and the frame
Per frame, per object: VISIBLE (some pixel of it nearer than everything else in the frame) and CULLED.
  unsafe    visible and culled (a conservative cull never does it): the object vanishes that frame
  flips     frames where an object's drawn-visible state (visible and not culled) differs from the frame before,
            beyond the flips of plain visibility (the jitter's own coverage changes, TAA's domain)
VERDICT PASS = unsafe 0 and the cull adds 0 flips; `--red hard` (WW_GPUDRIVEN_B_RED=hard, c.z) must FAIL.
`--feedback`: the occluder list is fed back frame to frame (an object occludes only if it was drawn the frame
before), the loop a review suspected; with a conservative test it adds nothing either.
python -I twin.py [--frames 24] [--size 320x180] [--objects 600] [--red hard] [--feedback] [--seed 7]
"""
import argparse, math, sys
import numpy as np

ap = argparse.ArgumentParser()
ap.add_argument('--frames', type=int, default=24)
ap.add_argument('--size', default='320x180')
ap.add_argument('--objects', type=int, default=600)
ap.add_argument('--red', default='', choices=('', 'hard', 'minhiz'))
ap.add_argument('--feedback', action='store_true')
ap.add_argument('--seed', type=int, default=7)
a = ap.parse_args()
W, H = [int(v) for v in a.size.split('x')]
NEAR, FAR = 16.0, 472098.0           # the cell view's planes with a far field (glview: near 16, far + reach)
FOV = math.radians(50.0)
P11 = 1.0 / math.tan(FOV / 2)
P00 = P11 * H / W
P22 = -(FAR + NEAR) / (FAR - NEAR)
P32 = -2.0 * FAR * NEAR / (FAR - NEAR)
EPS = 4e-6                            # kCull's depthEps
R24 = 1.0 / 16777216.0                # the 24-bit depth's step (the units of glPolygonOffset)
EYE = np.array([0.0, 0.0, 8370.0])     # 1500 u over the ground at the origin (a view from a hill over far land)


def ground(x, y):
    """Hills rising into the distance, like Concord's horizon (world units, z up)."""
    return (6000.0 + 420.0 * np.sin(x / 2900.0) * np.cos(y / 3700.0) + 900.0 * np.sin(y / 17000.0 + 1.3)
            + 0.012 * np.maximum(y, 0.0))


def halton(i, b):
    f, r = 1.0, 0.0
    while i > 0:
        f /= b
        r += f * (i % b)
        i //= b
    return r


# view: the camera looks north (+y world) = -z view; x right = +x world; y up = +z world
def to_view(p):
    d = p - EYE
    return np.stack([d[..., 0], d[..., 2], -d[..., 1]], -1)


def ndc_depth(zv):
    """window depth of view z (< 0) through the projection (x, y never reach z: standardProjection)."""
    return ((P22 * zv + P32) / (-zv)) * 0.5 + 0.5


def rays(jx, jy):
    px = (np.arange(W) + 0.5) / W * 2 - 1 - 2 * jx / W
    py = (np.arange(H) + 0.5) / H * 2 - 1 - 2 * jy / H
    X, Y = np.meshgrid(px / P00, py / P11)
    d = np.stack([X, Y, -np.ones_like(X)], -1)          # view space, z = -1
    return d


def terrain_depth(d):
    """view z of the terrain along each pixel's ray (or None = sky), by marching and bisecting."""
    dw = np.stack([d[..., 0], -d[..., 2], d[..., 1]], -1)  # world direction for view z = -1
    t = np.full(d.shape[:2], NEAR)
    hit = np.zeros(d.shape[:2], bool)
    lo = t.copy()
    while True:
        live = ~hit & (t < FAR)
        if not live.any():
            break
        tn = np.where(live, t * 1.01 + 2.0, t)
        p = EYE + dw * tn[..., None]
        under = live & (p[..., 2] < ground(p[..., 0], p[..., 1]))
        lo = np.where(live & ~under, tn, lo)
        hit |= under
        t = np.where(live & ~under, tn, np.where(under, tn, t))
    hi = t.copy()
    for _ in range(30):
        mid = 0.5 * (lo + hi)
        p = EYE + dw * mid[..., None]
        under = p[..., 2] < ground(p[..., 0], p[..., 1])
        hi = np.where(hit & under, mid, hi)
        lo = np.where(hit & ~under, mid, lo)
    zv = np.where(hit, -hi, np.nan)                       # view z = -t for a ray with view z component -1
    return zv


def offset_depth(depth):
    """glPolygonOffset(1, 2): + max window-space slope + 2 depth steps (the far planes' slope from neighbours)."""
    dx = np.abs(np.diff(depth, axis=1, append=depth[:, -1:]))
    dy = np.abs(np.diff(depth, axis=0, append=depth[-1:, :]))
    s = np.nan_to_num(np.maximum(dx, dy), nan=0.0)
    return depth + 1.0 * np.minimum(s, 1e-3) + 2.0 * R24


rng = np.random.default_rng(a.seed)
# clusters on the ground, partly buried (as LOD objects stand), some on crests, some behind hills: a quarter near
# (300..3000 u: the depth test's tolerance is a fraction of a unit there, so a wrong test shows), a third mid
# (3000..60000 u, the loaded block and ring 0) and the rest far (60000..400000 u, the far LOD rings, 50..800 u big)
N = a.objects
band = rng.uniform(0.0, 1.0, N)
oy = np.where(band < 0.25, rng.uniform(300.0, 3000.0, N),
              np.where(band < 0.6, rng.uniform(3000.0, 60000.0, N), rng.uniform(60000.0, 400000.0, N)))
ox = rng.uniform(-0.5, 0.5, N) * oy * 1.2
orad = np.where(oy < 60000.0, rng.uniform(2.0, 60.0, N), rng.uniform(50.0, 800.0, N))
oz = ground(ox, oy) + orad * rng.uniform(-0.6, 0.8, N)
cen = np.stack([ox, oy, oz], -1)
cv = to_view(cen)


def render_objects(d, depth_scene):
    """each object's pixels nearer than the scene: depth image of all objects and per object a visible flag."""
    obj_depth = np.full((H, W), np.inf)
    owner = np.full((H, W), -1)
    rects = []
    for i in range(N):
        c, r = cv[i], orad[i]
        if c[2] > -NEAR:
            rects.append(None)
            continue
        # screen box of the sphere (generous): centre +- r / -z through P
        sx = c[0] / -c[2] * P00
        sy = c[1] / -c[2] * P11
        ex = (r / -c[2]) * P00 * 1.5 + 4.0 / W
        ey = (r / -c[2]) * P11 * 1.5 + 4.0 / H
        x0 = int(max(0, math.floor(((sx - ex) * 0.5 + 0.5) * W) - 2))
        x1 = int(min(W - 1, math.ceil(((sx + ex) * 0.5 + 0.5) * W) + 2))
        y0 = int(max(0, math.floor(((sy - ey) * 0.5 + 0.5) * H) - 2))
        y1 = int(min(H - 1, math.ceil(((sy + ey) * 0.5 + 0.5) * H) + 2))
        if x0 > x1 or y0 > y1:
            rects.append(None)
            continue
        dd = d[y0:y1 + 1, x0:x1 + 1]
        # ray (t * dd) against the sphere: |t dd - c|^2 = r^2
        A = (dd * dd).sum(-1)
        B = -2.0 * (dd * c).sum(-1)
        C = float((c * c).sum() - r * r)
        disc = B * B - 4 * A * C
        ok = disc >= 0
        t = (-B - np.sqrt(np.where(ok, disc, 0.0))) / (2 * A)
        ok &= t > 0
        zv = -t
        dep = np.where(ok, ndc_depth(np.where(ok, zv, -1.0)), np.inf)
        sub = obj_depth[y0:y1 + 1, x0:x1 + 1]
        nearer = dep < sub
        sub[nearer] = dep[nearer]
        owner[y0:y1 + 1, x0:x1 + 1][nearer] = i
        rects.append((y0, y1, x0, x1, dep))
    return obj_depth, owner, rects


def hiz_levels(level0):
    lv = [level0]
    while lv[-1].shape != (1, 1):
        s = lv[-1]
        h, w = s.shape
        nh, nw = (h + 1) // 2, (w + 1) // 2
        ay = np.minimum(np.arange(nh) * 2, h - 1)
        by = np.minimum(np.arange(nh) * 2 + 1, h - 1)
        ax = np.minimum(np.arange(nw) * 2, w - 1)
        bx = np.minimum(np.arange(nw) * 2 + 1, w - 1)
        # the twin's own red `minhiz`: a NEAREST pyramid (the bug class that culls what pokes out in front)
        red = np.minimum if a.red == 'minhiz' else np.maximum
        m = red(red(s[ay][:, ax], s[ay][:, bx]), red(s[by][:, ax], s[by][:, bx]))
        lv.append(m)
        if len(lv) >= 16:
            break
    return lv


def cull(lv, jx, jy, hard):
    """kCull's occlusion branch, op for op, for every object (r scale 1, a similar transform)."""
    jxn, jyn = 2 * jx / W, 2 * jy / H
    out = np.zeros(N, bool)
    for i in range(N):
        c, r = cv[i], orad[i]
        lo = np.array([1e30, 1e30])
        hi = -lo
        front = True
        for k in range(8):
            o = np.array([r if k & 1 else -r, r if k & 2 else -r, r if k & 4 else -r])
            p = c + o
            qw = -p[2]
            qz = P22 * p[2] + P32
            if qw <= 0.0 or qz < -qw:
                front = False
                break
            qx = P00 * p[0] + jxn * (-p[2])        # the jitter rides in column 2 (x, y), times -z
            qy = P11 * p[1] + jyn * (-p[2])
            v = np.array([qx / qw, qy / qw])
            lo = np.minimum(lo, v)
            hi = np.maximum(hi, v)
        if not front:
            continue
        p0 = (lo * 0.5 + 0.5) * np.array([W, H]) - 1.0
        p1 = (hi * 0.5 + 0.5) * np.array([W, H]) + 1.0
        if not (p1[0] >= 0 and p1[1] >= 0 and p0[0] < W and p0[1] < H):
            continue
        ax_, ay_ = int(min(max(math.floor(p0[0]), 0), W - 1)), int(min(max(math.floor(p0[1]), 0), H - 1))
        bx_, by_ = int(min(max(math.floor(p1[0]), 0), W - 1)), int(min(max(math.floor(p1[1]), 0), H - 1))
        span = max(bx_ - ax_, by_ - ay_) + 1
        L = 0
        while L < len(lv) - 1 and (1 << L) < span:
            L += 1
        h = lv[L][ay_ >> L:(by_ >> L) + 1, ax_ >> L:(bx_ >> L) + 1].max()
        z = c[2] if hard else c[2] + r
        if ndc_depth(z) > h + EPS:
            out[i] = True
    return out


frames = []
drawn_prev = np.ones(N, bool)
stats = dict(unsafe=0, unsafe_objs=set(), vis_flips=0, drawn_flips=0, culled=0)
prev_vis = prev_drawn = None
seen_frames = np.zeros(N, int)
for f in range(a.frames):
    jx, jy = halton(f + 1, 2) - 0.5, halton(f + 1, 3) - 0.5
    d = rays(jx, jy)
    tz = terrain_depth(d)
    tdep = np.where(np.isnan(tz), 1.0, ndc_depth(np.where(np.isnan(tz), -1.0, tz)))
    obj_depth, owner, rects = render_objects(d, tdep)
    # the frame (main pass: every opaque surface with the same offset, so the plain depths decide)
    frame = np.minimum(tdep, obj_depth)
    vis = np.zeros(N, bool)
    m = (obj_depth < tdep) & (owner >= 0)
    vis[np.unique(owner[m])] = True
    # the prepass: terrain + the objects that occlude (all, or with --feedback only those drawn the frame before)
    if a.feedback:
        occ_dep = np.full((H, W), np.inf)
        for i, rc in enumerate(rects):
            if rc is None or not drawn_prev[i]:
                continue
            y0, y1, x0, x1, dep = rc
            sub = occ_dep[y0:y1 + 1, x0:x1 + 1]
            np.minimum(sub, dep, out=sub)
        pre = np.minimum(tdep, occ_dep)
    else:
        pre = frame
    pre = offset_depth(np.where(np.isinf(pre), 1.0, pre))
    lv = hiz_levels(np.minimum(pre, 1.0))
    cu = cull(lv, jx, jy, a.red == 'hard')
    drawn = vis & ~cu
    stats['culled'] += int(cu.sum())
    bad = vis & cu
    stats['unsafe'] += int(bad.sum())
    stats['unsafe_objs'] |= set(np.nonzero(bad)[0].tolist())
    if prev_vis is not None:
        stats['vis_flips'] += int((vis != prev_vis).sum())
        stats['drawn_flips'] += int((drawn != prev_drawn).sum())
    prev_vis, prev_drawn = vis, drawn
    seen_frames += vis
    drawn_prev = ~cu
    print('frame %2d jitter %+.3f %+.3f: visible %d culled %d unsafe %d' % (f, jx, jy, int(vis.sum()), int(cu.sum()),
                                                                             int(bad.sum())))
added = stats['drawn_flips'] - stats['vis_flips']
ok = stats['unsafe'] == 0 and added <= 0
# the jitter's own blink (no cull involved): objects seen in some frames of a steady camera and not in others, by
# distance band; one pixel-centre sample a pixel and no history, so this is the raw signal TAA must hold together
blink = (seen_frames > 0) & (seen_frames < a.frames)
for name, lo_, hi_ in (('near <3000 u', 0, 3000), ('mid 3000..60000 u', 3000, 60000), ('far >60000 u', 60000, 1e9)):
    sel = (oy >= lo_) & (oy < hi_)
    px = 2.0 * orad[sel] / np.maximum(oy[sel], 1.0) * P11 * H / 2.0
    print('jitter blink %-18s: seen in every frame %3d, in some frames only %3d (median size %.2f px)'
          % (name, int(((seen_frames == a.frames) & sel).sum()), int((blink & sel).sum()), float(np.median(px))))
print('gpucull twin%s%s: %d frames %dx%d, %d far objects: culled %d, unsafe (visible and culled) %d in %d objects, '
      'visibility flips %d, drawn flips %d, flips the cull adds %d VERDICT %s'
      % (' red ' + a.red if a.red else '', ' feedback' if a.feedback else '', a.frames, W, H, N, stats['culled'],
         stats['unsafe'], len(stats['unsafe_objs']), stats['vis_flips'], stats['drawn_flips'], added,
         'PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
