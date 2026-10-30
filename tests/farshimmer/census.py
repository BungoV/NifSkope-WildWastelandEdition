"""lane FARTOGGLE4: far objects that go on and off on a MOVING camera with the temporal AA OFF (bungo's settings).

What decides it, measured on the real far rings, frame by frame, no NifSkope window:
with TAA off nothing is carried between frames, so an object's weight in a frame is the number of the 4 MSAA sample
points (his saved Antialiasing 4x, the D3D/GL standard pattern) its footprint covers, over 4. There is no screen-size
cull, no per-frame LOD pick (LODPLACE1: 0 row changes on a slow walk) and no conservative raster in the cell view, so
this coverage rule is the whole rule. A footprint narrower than the pattern's gaps covers 0 samples at some sub-pixel
phases and 1+ at others: on a moving camera the phase changes every frame and the object blinks.

Inputs (on disk, read-only): the bake the c3 rows load (.lodo/.lodi), LODPLACE1's drawn far rows of Concord -15,17
(walk/still1/place.txt: the I rows the cell view drew + the far terrain vertices), and the c3 move row's camera log
(merge1030a hold run2 c3_rt_move_path.txt, 24 frames: cam= eye|fwd|tan, camrows= right|up).
Each drawn far row = its .lodo mesh box (rep0) placed by the .lodi (position, rotation, scale), projected with the
frame's camera at bungo's vertical FOV (60, Settings/Render/General/Camera) on a W x H view; its footprint = an
axis-aligned rectangle with the projected hull's area and the projected box's aspect. Rows behind the far terrain
(a 256 u height grid from the far terrain vertices, ray to the box top) are dropped.

Per frame and row: hits = the 4x samples inside the rectangle (exact lattice count); weight = hits / 4 px^2, true =
the rectangle's area. Over the frames: a row BLINKS between two frames when it goes from 0 hits to >0 or back.
Candidates printed beside today's rule: the footprint widened to >= s px each axis ("min footprint", FARTOGGLE3's
proposed option), with its weight gain (weight / true area: what the far field gains in boldness).

python -I census.py --bake DIR --place FILE --path FILE [--fov 60] [--w 1920 --h 1045] [--out TSV] [--grid 256]
"""
import argparse, math, os, sys, collections
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'spells'))
sys.path.insert(0, HERE)
import lodgen_native_decode as D   # noqa: E402
import farshimmer_raster as R       # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--bake', required=True)
ap.add_argument('--ws', default='Commonwealth')
ap.add_argument('--place', required=True)
ap.add_argument('--path', required=True)
ap.add_argument('--fov', type=float, default=60.0, help='vertical FOV in degrees; 0 = the path log\'s own')
ap.add_argument('--w', type=int, default=1920)
ap.add_argument('--h', type=int, default=1045)
ap.add_argument('--grid', type=float, default=256.0)
ap.add_argument('--out', default='')
ap.add_argument('--mins', default='0.5,0.6,0.7,0.8,1.0,0.8:1.25,0.8:1.5,1.0:1.5',
                help='candidate rules: s (min footprint s px) or s:g (and the widened area capped at g x the true one)')
ap.add_argument('--pan', type=float, default=0.0, help='> 0: frame 0 of the path turned by this many px a frame')
ap.add_argument('--frames', type=int, default=24, help='frames of the --pan camera')
a = ap.parse_args()


def read_place(path):
    rows, verts = [], []
    with open(path, encoding='utf-8') as f:
        for line in f:
            if line.startswith('I '):
                t = line.split()
                if t[-1] == '1':
                    rows.append((int(t[3], 16), float(t[5]), float(t[6])))
            elif line.startswith('V '):
                t = line.split()
                verts.append((float(t[-3]), float(t[-2]), float(t[-1])))
    return rows, np.array(verts)


def read_cams(path):
    cams = []
    for line in open(path, encoding='utf-8'):
        if not line.startswith('frame '):
            continue
        cam = [t for t in line.split() if t.startswith('cam=')][0][4:].split('|')
        rows = [t for t in line.split() if t.startswith('camrows=')][0][8:].split('|')
        v = lambda s: np.array([float(x) for x in s.split(',')])
        cams.append(dict(eye=v(cam[0]), fwd=v(cam[1]), tan=v(cam[2]), right=v(rows[0]), up=v(rows[1])))
    return cams


def main():
    B = os.path.join(a.bake, a.ws + '.')
    L = D.read_lodo(B + 'lodo')
    T = D.read_lodi(B + 'lodi')
    drawn, verts = read_place(a.place)
    cams = read_cams(a.path)
    if a.pan > 0:
        # a slow pan: frame 0's camera turned about world z by a.pan px a frame at the view centre
        c0 = cams[0]
        tanY = math.tan(math.radians(a.fov) / 2) if a.fov > 0 else c0['tan'][1]
        step = math.atan(a.pan * tanY / (a.h / 2.0))
        cams = []
        for k in range(a.frames):
            t = step * k
            rz = np.array([[math.cos(t), -math.sin(t), 0], [math.sin(t), math.cos(t), 0], [0, 0, 1]])
            cams.append(dict(eye=c0['eye'], fwd=rz @ c0['fwd'], tan=c0['tan'], right=rz @ c0['right'], up=rz @ c0['up']))
    print('farshimmer census: lodi v%d %d instances, %d drawn far rows, %d far terrain verts, %d frames'
          % (T['header']['version'], len(T['instances']), len(drawn), len(verts), len(cams)))
    # join the drawn rows to the .lodi by (ref low 24, position)
    byref = collections.defaultdict(list)
    for r, c in zip(T['instances'], T['cold']):
        byref[c['refFormId'] & 0xFFFFFF].append(r)
    C = []    # 8 world corners per row
    NAMES = []
    miss = 0
    for ref, x, y in drawn:
        best = None
        for r in byref.get(ref & 0xFFFFFF, ()):
            d = abs(r['x'] - x) + abs(r['y'] - y)
            if best is None or d < best[0]:
                best = (d, r)
        if best is None or best[0] > 4.0:
            miss += 1
            continue
        r = best[1]
        base = L['bases'][r['baseId']]
        mesh = None
        for k in ('rep0', 'rep1', 'rep2', 'rep3'):
            if base[k] != 0xFFFF and base[k] < len(L['meshes']):
                mesh = L['meshes'][base[k]]
                break
        if mesh is None:
            miss += 1
            continue
        NAMES.append(L['string_at'](mesh['modelStringOffset']))
        M = np.array(r['m']).reshape(3, 3)
        mn, ex = np.array(mesh['aabbMin']), np.array(mesh['aabbExtent'])
        corners = np.array([mn + ex * np.array([kx, ky, kz]) for kx in (0, 1) for ky in (0, 1) for kz in (0, 1)])
        C.append(np.array([r['x'], r['y'], r['z']]) + r['scaleF'] * corners @ M.T)
    C = np.array(C)   # (n, 8, 3)
    print('joined %d rows (%d not found in the .lodi or without a mesh)' % (len(C), miss))
    # the far terrain height grid (mean per cell of the grid)
    g = a.grid
    gx = np.floor(verts[:, 0] / g).astype(np.int64)
    gy = np.floor(verts[:, 1] / g).astype(np.int64)
    x0, y0 = gx.min(), gy.min()
    HG = np.full((gx.max() - x0 + 1, gy.max() - y0 + 1), np.nan)
    acc = np.zeros(HG.shape); cnt = np.zeros(HG.shape)
    np.add.at(acc, (gx - x0, gy - y0), verts[:, 2]); np.add.at(cnt, (gx - x0, gy - y0), 1)
    HG[cnt > 0] = acc[cnt > 0] / cnt[cnt > 0]

    def occluded(eye, tops):
        """rows whose box top is under the far terrain somewhere on the ray from the eye (sampled 96 times)"""
        occ = np.zeros(len(tops), bool)
        for t in np.linspace(0.04, 0.96, 96):
            p = eye + (tops - eye) * t
            ix = np.floor(p[:, 0] / g).astype(np.int64) - x0
            iy = np.floor(p[:, 1] / g).astype(np.int64) - y0
            ok = (ix >= 0) & (iy >= 0) & (ix < HG.shape[0]) & (iy < HG.shape[1])
            h = np.full(len(tops), -np.inf)
            h[ok] = HG[ix[ok], iy[ok]]
            h = np.nan_to_num(h, nan=-np.inf)
            occ |= h > p[:, 2] + 1.0
        return occ

    W, H = a.w, a.h
    mins = [(float(s.split(':')[0]), float(s.split(':')[1]) if ':' in s else 0.0) for s in a.mins.split(',')]
    centre = C.mean(1)
    tops = centre.copy(); tops[:, 2] = C[:, :, 2].max(1)
    frames = []
    for fi, cam in enumerate(cams):
        tanY = math.tan(math.radians(a.fov) / 2) if a.fov > 0 else cam['tan'][1]
        tanX = tanY * W / H if a.fov > 0 else cam['tan'][0]
        d = C - cam['eye']
        zv = d @ cam['fwd']
        xv = d @ cam['right']
        yv = d @ cam['up']
        front = (zv > 16.0).all(1)
        zs = np.where(zv > 16.0, zv, 16.0)
        px = W / 2 + (xv / zs) / tanX * W / 2
        py = H / 2 - (yv / zs) / tanY * H / 2
        bx0, bx1, by0, by1 = px.min(1), px.max(1), py.min(1), py.max(1)
        onscr = front & (bx1 > 0) & (bx0 < W) & (by1 > 0) & (by0 < H)
        occ = np.zeros(len(C), bool)
        occ[onscr] = occluded(cam['eye'], tops[onscr])
        area = R.hull_area(px, py)
        frames.append(dict(cx=(bx0 + bx1) / 2, cy=(by0 + by1) / 2, bw=bx1 - bx0, bh=by1 - by0, area=area,
                           vis=onscr & ~occ, onscr=onscr, occ=occ))
        print('frame %2d: on screen %d, behind the far terrain %d, visible %d' % (fi, onscr.sum(), (onscr & occ).sum(),
                                                                             (onscr & ~occ).sum()))
    # rows visible in every frame: the census population
    vis = np.all([f['vis'] for f in frames], 0)
    idx = np.nonzero(vis)[0]
    print('visible in all %d frames: %d rows' % (len(frames), len(idx)))
    # footprint per frame: a rectangle of the hull's area with the box's aspect, centred on the box
    res = R.blink_census(frames, idx, mins)
    for line in R.report(res, len(frames)):
        print(line)
    # the verdict the run.sh reads: today's rule blinks on the real rings, and no tried footprint rule is both
    # blink-free and within 1.25x ink over the sub-pixel rows
    sub = np.minimum(res['w'], res['h']) < 1.0
    today = int((res['blinks'][0] > 0).sum())
    fits = []
    for k, (s, gc) in enumerate(res['rules'][1:], 1):
        b = int((res['blinks'][k] > 0).sum())
        ink = float(res['weight'][k][sub].sum() / max(res['area'][sub].sum(), 1e-9))
        fits.append((s, gc, b, ink))
    holds = today > 0 and all(not (b == 0 and i <= 1.25) for _, _, b, i in fits)
    print('CENSUS today blinks %d; %s; refusal %s' % (today, '; '.join(
        '%.2f%s: blinks %d ink %.2f' % (s, '/%.2f' % gc if gc else '', b, i) for s, gc, b, i in fits),
        'HOLDS' if holds else 'BROKEN'))
    # which models blink (today's rule), by the .lodo model path
    tally = collections.Counter()
    for k, i in enumerate(idx):
        if res['blinks'][0][k] > 0:
            tally[NAMES[i].lower().replace('\\', '/').split('/')[-1]] += 1
    print('blinking rows by model (top 15 of %d models): %s' % (len(tally), ', '.join(
        '%s %d' % kv for kv in tally.most_common(15))))
    if a.out:
        with open(a.out, 'w', encoding='utf-8') as f:
            f.write('row\tmodel\tmean_w_px\tmean_h_px\tmean_area_px2\tblinks\tframes_off\n')
            for k, i in enumerate(idx):
                f.write('%d\t%s\t%.3f\t%.3f\t%.4f\t%d\t%d\n' % (i, NAMES[i], res['w'][k], res['h'][k], res['area'][k],
                                                                 res['blinks'][0][k], res['offs'][0][k]))


main()
