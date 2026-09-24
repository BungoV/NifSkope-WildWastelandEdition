"""F1 probe: lodgen_octahedral.sh's F1 instrument (span of the half-coverage mask
vs the predicted span) on a bake, beside the same instrument on the AREA truth
(SS=12) and on the IDEAL 2x2 ESTIMATOR (SS=2 point samples = the ruled design),
both at that bake's own frame extents.   python f1probe.py <bakedir> [half]"""
import sys, math
import numpy as np
from PIL import Image

d = sys.argv[1]; half = float(sys.argv[2]) if len(sys.argv) > 2 else 256.0
m = {}
for line in open(d + '/cube512.txt'):
    f = line.split()
    if f: m.setdefault(f[0], []).append(f[1:])
o = m['oct'][0]; n, tw, th = int(o[0]), int(o[1]), int(o[2]); fw, fh = float(o[3]), float(o[4])
cf, ct, cb = (int(v) for v in m['coverage'][0][:3])

def axes(i, j, n):
    u = i / float(n - 1) * 2 - 1; v = j / float(n - 1) * 2 - 1
    dx, dy = (u + v) * .5, (u - v) * .5; dz = 1 - abs(dx) - abs(dy)
    L = math.sqrt(dx*dx + dy*dy + dz*dz); dx, dy, dz = dx/L, dy/L, dz/L
    el = math.asin(max(-1, min(1, dz))); az = math.atan2(dy, dx)
    return (math.sin(az), -math.cos(az), 0.0), (math.sin(el)*math.cos(az), math.sin(el)*math.sin(az), math.cos(el))

def hull(pts):
    pts = sorted(set(pts))
    def h(ps):
        st = []
        for p in ps:
            while len(st) >= 2 and (st[-1][0]-st[-2][0])*(p[1]-st[-2][1]) - (st[-1][1]-st[-2][1])*(p[0]-st[-2][0]) <= 0:
                st.pop()
            st.append(p)
        return st
    return h(pts)[:-1] + h(pts[::-1])[:-1]

a8 = np.asarray(Image.open(d + '/cube512_oct_albedo.png').convert('RGBA'))[..., 3].astype(float)
dec = np.where(a8 < cb, 0, cf + np.round((a8 - cb) * (255. - cf) / (255. - cb)))

def span(mk):
    c = np.flatnonzero(mk.any(0)); r = np.flatnonzero(mk.any(1))
    return (c[-1]-c[0]+1 if c.size else 0), (r[-1]-r[0]+1 if r.size else 0)

def cover(i, j, SS):
    r, up = axes(i, j, n)
    P = hull([(sx*r[0]+sy*r[1]+sz*r[2], sx*up[0]+sy*up[1]+sz*up[2]) for sx in (-half, half) for sy in (-half, half) for sz in (-half, half)])
    tx, ty = 2*fw/tw, 2*fh/th; sub = (np.arange(SS)+.5)/SS
    X = (-fw + (np.arange(tw)[:, None] + sub[None, :])*tx).ravel()[None, :]
    Y = (-fh + (np.arange(th)[:, None] + sub[None, :])*ty).ravel()[:, None]
    ins = np.ones((th*SS, tw*SS), bool)
    for k in range(len(P)):
        ax, ay = P[k]; bx, by = P[(k+1) % len(P)]
        ins &= ((bx-ax)*(Y-ay) - (by-ay)*(X-ax)) >= 0
    return ins.reshape(th, SS, tw, SS).mean((1, 3))

W = {'bake': 0., 'area@.5': 0., 'est2x2 tie in': 0., 'est2x2 tie out': 0., 'est3x3': 0., 'est4x4': 0.}
for j in range(n):
    for i in range(n):
        r, up = axes(i, j, n)
        px = 2*half*(abs(r[0])+abs(r[1])+abs(r[2]))*tw/(2*fw); py = 2*half*(abs(up[0])+abs(up[1])+abs(up[2]))*th/(2*fh)
        mk = dec[j*th:(j+1)*th, i*tw:(i+1)*tw] >= 128
        # the instrument measures in the IMAGE frame (y down); the span is the same
        for key, m2 in (('bake', mk), ('area@.5', cover(i, j, 12) >= .5),
                        ('est2x2 tie in', cover(i, j, 2) >= .5), ('est2x2 tie out', cover(i, j, 2) > .5), ('est3x3', cover(i, j, 3) >= .5), ('est4x4', cover(i, j, 4) >= .5)):
            mx, my = span(m2)
            W[key] = max(W[key], abs(mx-px), abs(my-py))
print('n %d tile %dx%d fw %.3f  ' % (n, tw, th, fw) + '  '.join('%s %.2f' % kv for kv in W.items()))
