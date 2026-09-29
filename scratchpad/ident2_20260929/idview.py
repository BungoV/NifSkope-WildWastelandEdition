"""IDENT2: offline identity picture of a box of the map, from the FILE (colour = the viewer's hash of the group id),
orthographic, elevation 35 deg (or 90 = top) at a given azimuth; also writes a pick map (instance index per pixel).
usage: python idview.py <base> <dump> x0 y0 x1 y1 az el px out.png"""
import sys, os, math, numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tests', 'spells'))
import lodgen_native_decode as ND, lodi_occluder_building as OB, groups, fp_hull as FH
from PIL import Image
base, dump = sys.argv[1], sys.argv[2]
x0, y0, x1, y1, az, el, PX = [float(v) for v in sys.argv[3:10]]; out = sys.argv[10]
def col(idx):
    seeds = np.array([12.9898, 78.233, 45.164], np.float32)
    s = np.sin((np.float32(idx) + np.float32(1)) * seeds).astype(np.float32) * np.float32(43758.5453)
    return ((s - np.floor(s)) * np.float32(0.8) + np.float32(0.2)) * 255
h, P, C = groups.load(dump); pk = {(p['ref'], p['part']): p for p in P}
L = ND.read_lodo(base + '.lodo'); T = ND.read_lodi(base + '.lodi')
tris, owner = [], []
for ii, c in enumerate(T['cold']):
    p = pk.get((c['refFormId'], c['scolPart']))
    if not p or p['tree'] or FH.model(p['name']).startswith(FH.VEG) or not p['tris']: continue
    if p['hi'][0] < x0 or p['lo'][0] > x1 or p['hi'][1] < y0 or p['lo'][1] > y1: continue
    me = OB.drawn_mesh(L, T['instances'][ii], (p['tris'], np.array(p['lo']), np.array(p['hi'])))
    if me == OB.NO_MESH: continue
    t = OB.placed_tris(L, T['instances'][ii], me)
    if len(t): tris.append(t); owner.append(np.full(len(t), ii))
tris = np.concatenate(tris); owner = np.concatenate(owner)
a, e = math.radians(az), math.radians(el)
d = np.array([-math.cos(e) * math.cos(a), -math.cos(e) * math.sin(a), -math.sin(e)])
up = np.array([0, 0, 1.0]) if el < 89.9 else np.array([0, 1.0, 0])
u = np.cross(d, up); u /= np.linalg.norm(u); v = np.cross(u, d)
A, B, Cc = tris[:, 0], tris[:, 1], tris[:, 2]
rng = np.random.default_rng(1)
pa = np.stack([A @ u, A @ v], 1); pb = np.stack([B @ u, B @ v], 1); pc = np.stack([Cc @ u, Cc @ v], 1)
area = 0.5 * np.abs((pb[:, 0] - pa[:, 0]) * (pc[:, 1] - pa[:, 1]) - (pb[:, 1] - pa[:, 1]) * (pc[:, 0] - pa[:, 0])) / PX / PX
n = np.clip(np.ceil(area * 4).astype(np.int64), 3, 40000); idx = np.repeat(np.arange(len(tris)), n)
r1 = rng.random(len(idx)); r2 = rng.random(len(idx)); s1 = np.sqrt(r1)
pts = A[idx] * (1 - s1)[:, None] + B[idx] * (s1 * (1 - r2))[:, None] + Cc[idx] * (s1 * r2)[:, None]
pu, pv, pd = pts @ u, pts @ v, pts @ d
iu = np.floor((pu - pu.min()) / PX).astype(np.int64); iv = np.floor((pv - pv.min()) / PX).astype(np.int64)
W = iu.max() + 1; Hh = iv.max() + 1; pix = iv * W + iu
order = np.lexsort((pd, pix)); ps = pix[order]; first = np.ones(len(order), bool); first[1:] = ps[1:] != ps[:-1]
win = order[first]
img = np.full((Hh * W, 3), 255, np.uint8); pick = np.full(Hh * W, -1, np.int64)
wo = owner[idx[win]]
img[pix[win]] = np.array([col(T['group'][i] + 1) for i in wo]).astype(np.uint8)
pick[pix[win]] = wo
Image.fromarray(img.reshape(Hh, W, 3)[::-1]).save(out)
np.save(out.replace('.png', '_pick.npy'), pick.reshape(Hh, W)[::-1])
print(out, W, Hh)
