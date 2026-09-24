"""DEFECT 3 -- why the FULL CROWN is the worst subject when the spec says it is
the easy case.

THE SHOT THAT DOES NOT EXIST, named and not substituted. The brief asks for a
single-frame known-answer control: the leafy maple rendered from ONE of its own
sixteen bake directions, scored against the MESH from that same direction. The
mesh grabs at a subject's own bake directions exist for ONE subject only --
`scratchpad/impostorfix1_20260919/control/blast_n4_bake/` (blast_n4, 16
directions). There is no such folder for maple_n4. The control cannot be run
offline. A BUILD LANE MUST SHOOT IT:
    maple_n4 (0004a074), mesh AND card, at the 16 bake directions of N=4,
    same camera and canvas as impostorfix1's blast_n4_bake, written to
    scratchpad/<lane>/control/maple_n4_bake/.
Nothing below is offered as a replacement for it.

WHAT CAN BE MEASURED WITHOUT IT, and what each part decides:

A. THE SHEET'S OWN INK INFLATION -- no mesh needed at all, and it is the
   cleanest number in this report. A texel's coverage is a FRACTION. The
   honest area of a frame is the SUM of those fractions. What the card paints
   is the COUNT of texels over the threshold. count/sum is the ink the
   threshold invents, before any blend, any parallax, any view. If the maple's
   inflation is large here, the defect is in the sheet-plus-threshold contract
   and no drawing change can reach it.

B. IoU AGAINST ANGULAR DISTANCE TO THE NEAREST BAKE DIRECTION, single frame
   and three frames. A PROXY for the missing control, labelled as one: none of
   the 24 orbit views sits exactly on a bake direction, so this reads the trend
   towards one instead of the value at one. If single-frame IoU is still low at
   the CLOSEST views, the sheet is wrong for alpha-tested leaves; if it climbs
   steeply as the distance falls, the blend carries the loss. blast_n4 is run
   beside it because its value AT a bake direction is known (0.8785), so the
   proxy's own trend can be checked against a measured endpoint.

C. THE BC3 ALPHA RAMP'S SHARE for the maple specifically: the same render with
   `_d` alpha taken from the pre-compression PNG instead of the decoded DDS.
"""
import os, sys, glob, json
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inst4 import *
from cal4 import alpha_ref, mask_at, place, load, SHAPE

HERE = os.path.dirname(os.path.abspath(__file__))
CAL = load()
TAGS = ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4')


def png_alpha(cs):
    g = glob.glob(cs.dir + '*_oct_albedo.png')
    if not g:
        return None
    p = np.asarray(Image.open(g[0]).convert('RGBA')).astype(np.float64) / 255.0
    if p.shape[:2] != cs.alb.shape[:2]:
        return None
    out = cs.alb.copy()
    out[..., 3] = p[..., 3]
    return out


# ------------------------------------------------------------------ part A
print('A. SHEET INK INFLATION -- texels painted / honest covered area, no mesh')
print('%-10s %10s %12s | %8s %8s %8s' %
      ('tag', 'cov sum', 'texels>=floor', 'x@floor', 'x@0.20', 'x@0.50'))
A = []
for tag in TAGS:
    cs = Sheets(tag, 'cards', root=R3, sub='fixture')
    a = cs.alb[..., 3]
    cov = coverageOf(a, cs)
    tot = cov.sum()
    row = dict(tag=tag, covsum=float(tot))
    for nm, thr in (('floor', cs.covFloor), ('t20', 0.20), ('t50', 0.50)):
        n = int((cov >= thr).sum())
        row[nm] = n
        row[nm + '_x'] = float(n / tot) if tot else float('nan')
    A.append(row)
    print('%-10s %10.0f %12d | %8.3f %8.3f %8.3f' %
          (tag, row['covsum'], row['floor'], row['floor_x'], row['t20_x'], row['t50_x']))

# ------------------------------------------------------------------ part B/C
print()
print('B/C running...', flush=True)
B = {}
for tag in ('blast_n4', 'maple_n4'):
    c = CAL['%s|fixture|cards' % tag]
    s = c['scale']
    offs = c['off']
    cs = Sheets(tag, 'cards', root=R3, sub='fixture')
    pa = png_alpha(cs)
    bake = [frameDir(i, j, cs.N) for j in range(cs.N) for i in range(cs.N)]
    rows = []
    for v in VIEWS:
        d = dirOf(*v)
        ang = min(np.degrees(np.arccos(np.clip(float(np.dot(d, b)), -1, 1))) for b in bake)
        dy, dx = offs['%d_%d' % v]
        M = grabmask(R3, tag, 'cards', *v, 'mesh')
        out = dict(view='%d_%d' % v, ang=float(ang))
        for nm, kw in (('f3', {}), ('f1', dict(nframes=1)),
                       ('f1png', dict(nframes=1, albOverride=pa) if pa is not None else None),
                       ('f3png', dict(albOverride=pa) if pa is not None else None)):
            if kw is None:
                out[nm] = float('nan')
                continue
            a = alpha_ref(cs, d, **kw)
            m = place(mask_at(a, cs, s, cs.covFloor), dy, dx)
            out[nm] = float(iou(m, M))
            out[nm + '_ink'] = float(m.sum() / M.sum()) if M.sum() else float('nan')
        rows.append(out)
    B[tag] = rows
    print('  %s done' % tag, flush=True)

print()
print('B. IoU vs ANGULAR DISTANCE TO THE NEAREST BAKE DIRECTION (PROXY, 24 views)')
for tag in B:
    rows = sorted(B[tag], key=lambda r: r['ang'])
    print('  %s  (blast_n4 AT a bake direction, measured: 1 frame = 0.8785)' % tag)
    print('    %8s %8s %8s %8s %8s' % ('deg', '3 frame', '1 frame', '1f preBC3', '1f ink'))
    for r in rows:
        print('    %8.2f %8.4f %8.4f %8.4f %8.3f' %
              (r['ang'], r['f3'], r['f1'], r['f1png'], r.get('f1_ink', float('nan'))))
    q = np.array([r['ang'] for r in rows])
    for lo, hi in ((0, 10), (10, 20), (20, 40), (40, 90)):
        m = (q >= lo) & (q < hi)
        if m.any():
            print('    bin %2d-%2d deg  n=%2d  3f %.4f  1f %.4f' %
                  (lo, hi, int(m.sum()),
                   float(np.mean([rows[i]['f3'] for i in np.where(m)[0]])),
                   float(np.mean([rows[i]['f1'] for i in np.where(m)[0]]))))

print()
print('C. THE BC3 ALPHA RAMP SHARE (3 frames, 24 views, mean)')
print('%-10s %10s %10s %8s' % ('tag', 'DDS alpha', 'PNG alpha', 'delta'))
for tag in B:
    a3 = float(np.mean([r['f3'] for r in B[tag]]))
    p3 = float(np.mean([r['f3png'] for r in B[tag]]))
    print('%-10s %10.4f %10.4f %+8.4f' % (tag, a3, p3, p3 - a3))
json.dump(dict(A=A, B=B), open(os.path.join(HERE, 's3.json'), 'w'), indent=1)
