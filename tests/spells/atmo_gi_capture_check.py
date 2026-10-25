"""SKY3 (SKYGI2, offline, no NifSkope window): the probes' sky term from the physical sky.

The 6-face capture (atmo_sky.frag at exposure 1, linear, no sun disc, 90-degree faces, the tree's real .prog files) is
projected as src/probesky.cpp probeSkyPictureQ does (exact texel solid angle by the atan formula, the cosine at the
texel centre) onto the +Z axis, and compared with atmo_model's sky irradiance on level ground (the MS Psi, the cosine-
weighted upper hemisphere). Both in the sky pass's scene units (pi / T45 x L), the units of the probes' sun term.
The capture stands at the probes' height (src/probesky.cpp captureZNow; 20 m here).

Reds (each must FAIL):
  --red half  the texel solid angle as the flat (2/N)^2 (no projection of the face onto the sphere)
  --red camz  the capture taken from a raised view camera (1.5 km) instead of the probes' height: the SKY3 defect
Run: python -I tests/spells/atmo_gi_capture_check.py [--red half|camz]"""
import math, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import atmo_skyrender as R
A = R.A
red = sys.argv[sys.argv.index('--red') + 1] if '--red' in sys.argv else ''
sky = R.Sky(R.TREE + '/res/shaders')
P = sky.P
N = 96
PROBE_KM, VIEW_KM = 0.02, 1.5
# the model's Psi on the GPU table's texel centres (atmo_lut_check.py's psi_sub, 32 x 32)
mw = mh = 32
sub = lambda u, n: (u - 0.5 / n) / (1 - 1.0 / n)
T = np.zeros((mh, mw, 3))
for y in range(mh):
    for x in range(mw):
        T[y, x] = A.ms_psi(P, A.RB + max(0.01, sub((y + 0.5) / mh, mh) * (A.RT - A.RB)), sub((x + 0.5) / mw, mw) * 2 - 1, 8, 20)


def psi(rr, mus):
    x = np.clip(((mus * 0.5 + 0.5) * (1 - 1.0 / mw) + 0.5 / mw) * mw - 0.5, 0, mw - 1.001)
    y = np.clip((np.clip((rr - A.RB) / (A.RT - A.RB), 0, 1) * (1 - 1.0 / mh) + 0.5 / mh) * mh - 0.5, 0, mh - 1.001)
    x0, y0 = x.astype(int), y.astype(int)
    fx, fy = (x - x0)[:, None], (y - y0)[:, None]
    return (T[y0, x0] * (1 - fx) * (1 - fy) + T[y0, x0 + 1] * fx * (1 - fy) + T[y0 + 1, x0] * (1 - fx) * fy
            + T[y0 + 1, x0 + 1] * fx * fy)


# probeSkyPictureQ's texel solid angle: A(x0,y0) - A(x0,y1) - A(x1,y0) + A(x1,y1), A = atan2(x y, sqrt(x^2 + y^2 + 1))
edges = np.arange(N + 1) / N * 2 - 1
At = lambda x, y: np.arctan2(x * y, np.sqrt(x * x + y * y + 1))
x0, x1 = np.meshgrid(edges[:-1], edges[:-1])[0], np.meshgrid(edges[1:], edges[1:])[0]
y0, y1 = np.meshgrid(edges[:-1], edges[:-1])[1], np.meshgrid(edges[1:], edges[1:])[1]
om = At(x0, y0) - At(x0, y1) - At(x1, y0) + At(x1, y1)
if red == 'half':
    om = np.full_like(om, (2.0 / N) ** 2)
assert red != '' or abs(om.sum() - 4 * math.pi / 6) < 1e-9, 'a face holds 4 pi / 6'

tl = A.transmittance(P, np.array([A.RB]), np.array([0.70710678]), 400)
K = math.pi / float(np.dot(tl[0], [0.2126, 0.7152, 0.0722]))
capKm = VIEW_KM if red == 'camz' else PROBE_KM
fails = 0
for sun_el in (45.0, 15.0, 4.0):
    sun = (30.0, sun_el)
    E = np.zeros(3)
    ndc = (np.arange(N) + 0.5) / N * 2 - 1
    xx, yy = np.meshgrid(ndc, -ndc)   # rows top-down like the read-back
    for yaw, pit in ((0, 0), (90, 0), (180, 0), (270, 0), (0, 89.999), (0, -89.999)):
        img, _ = sky.render(N, N, (yaw, pit), sun, camZkm=capKm, vfov=90.0, disc=0.0)
        y_, p_ = math.radians(yaw), math.radians(pit)
        fwd = np.array([math.cos(p_) * math.cos(y_), math.cos(p_) * math.sin(y_), math.sin(p_)])
        rt = np.cross(fwd, [0, 0, 1.0]); rt /= np.linalg.norm(rt)
        up = np.cross(rt, fwd)
        d = xx[..., None] * rt + yy[..., None] * up + fwd
        cz = np.maximum(d[..., 2] / np.linalg.norm(d, axis=2), 0)
        E += (img * (cz * om)[..., None]).reshape(-1, 3).sum(0)
    # the model at the probes' height: 24 elevation x 48 azimuth bands, cos(el) sin(el) d(el) d(az)
    ne, na = 24, 48
    els = (np.arange(ne) + 0.5) / ne * math.pi / 2
    azs = (np.arange(na) + 0.5) / na * 2 * math.pi
    ee, aa = np.meshgrid(els, azs, indexing='ij')
    sd = R.sundir(*sun)
    v = np.stack([np.cos(ee) * np.cos(aa), np.cos(ee) * np.sin(aa), np.sin(ee)], -1).reshape(-1, 3)
    L = A.integrate(P, A.RB + PROBE_KM, v, np.array(sd), 200, psi=psi)[0]
    w = (np.cos(ee) * np.sin(ee)).reshape(-1) * (math.pi / 2 / ne) * (2 * math.pi / na)
    Em = K * (L * w[:, None]).sum(0)
    rel = np.abs(E / Em - 1).max()
    ok = rel < 0.03
    fails += not ok
    print('%s sun %4.1f deg: capture E %s  model %s  worst rel %.4f (< 0.03)' % ('PASS' if ok else 'FAIL', sun_el,
          np.round(E, 4), np.round(Em, 4), rel))
print('%s %d (red %s, capture at %.2f km)' % ('FAIL' if fails else 'PASS', fails, red or 'none', capKm))
sys.exit(1 if fails else 0)
