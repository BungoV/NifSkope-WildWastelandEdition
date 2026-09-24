"""IMPOSTORFIX3 section 2 -- the rock's ~10 px placement error, named with
numbers and then re-measured against THIS lane's bake.

IMPOSTORFIX2 s3c ran four rows on IMPOSTORFIX1's fixture (shipped sheets and
R3-repaired sheets, parallax ray on and off) with the registration FROZEN at
the calibrated value, so the only thing that moves between rows is the sheet
content. This script re-runs the same four rows and adds a fifth: the sheets
exe (this lane's build) bakes with the 8-ring dilation, read out of THIS
lane's fixture.

It also prints the placement arithmetic itself -- half extents, centre,
frameOffset, depthSpan, the pixel scale -- so the ~10 px can be attributed to a
term instead of asserted.

Nothing under the repo's own tree is written.
"""
import sys, os, json, glob, math
import numpy as np

R1 = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix1_20260919'
R2 = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919'
R3 = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix3_20260919'
sys.path.insert(0, R2)
from inst import *                      # noqa: E402  (Sheets, VIEWS, iou, grabmask, dirOf)
from calib import alpha_ref, mask_at, place, best_offset   # noqa: E402

TAG = 'rock_n4'
C = json.load(open(R2 + '/calib.json'))[TAG]
S = C['scale']; OFF = C['off']


class Sheets3(Sheets):
    """The same reader pointed at THIS lane's fixture."""
    def __init__(self, tag, which='cards', root=R3 + '/fixture'):
        self.tag, self.which = tag, which
        d = '%s/%s/%s/' % (root, tag, which)
        raw = open(glob.glob(d + '*_oct.lodm')[0], 'rb').read()
        j = json.loads(raw[raw.index(b'{'):].decode('utf-8'))['card']
        self.j = j
        self.N = j['oct']; self.half = np.array(j['half'], float)
        self.span = float(j['depthSpan'])
        c = j.get('coverage', {})
        self.covFloor = c.get('floor', 0) / 255.0
        self.covBase = c.get('base', 0) / 255.0
        fo = j.get('frameOffset', None)
        self.foff = (np.array(fo, float).reshape(self.N * self.N, 2)
                     if fo else np.zeros((self.N * self.N, 2)))
        self.alb = load_dds(glob.glob(d + '*_oct_d.DDS')[0])[0]
        self.nrm = load_dds(glob.glob(d + '*_oct_n.DDS')[0])[0]
        self.H, self.W = self.alb.shape[:2]
        self.fw, self.fh = self.W // self.N, self.H // self.N


def resid(m, g):
    inter, dy, dx = best_offset(m, g)
    mm = np.roll(np.roll(m, dy, 0), dx, 1)
    return dy, dx, iou(mm, g)


def run(cs, name, par):
    io = []; ish = []; d15 = []; d45 = []
    for v in VIEWS:
        dy, dx = OFF['%d_%d' % v]
        a = alpha_ref(cs, dirOf(*v), parallax=par)
        m = place(mask_at(a, cs, S, cs.covFloor), dy, dx)
        g = grabmask(TAG, 'after', *v, 'mesh')
        io.append(iou(m, g))
        sy, sx, i2 = resid(m, g); ish.append(i2)
        (d15 if v[1] == 15 else d45).append(sy)
    print('%-34s %8.4f %10.4f %9.2f %9.2f'
          % (name, np.mean(io), np.mean(ish), np.mean(d15), np.mean(d45)), flush=True)
    return np.mean(io), np.mean(ish), np.mean(d15), np.mean(d45)


# ------------------------------------------------------------- the geometry
c3 = Sheets3(TAG)
j = c3.j
print('THE ROCK CARD, as its own .lodm records it', flush=True)
print('  oct N            %d   frame %dx%d   sheet %dx%d'
      % (c3.N, c3.fw, c3.fh, c3.W, c3.H), flush=True)
print('  half extents     %s' % (list(np.round(c3.half, 3)),), flush=True)
print('  centre           %s' % (j.get('center'),), flush=True)
print('  depthSpan        %.2f world units  (1 height level = %.3f units)'
      % (c3.span, c3.span / 255.0), flush=True)
fo = np.asarray(c3.foff)
print('  frameOffset      %d pairs, max |dx| %.4f max |dy| %.4f  (fraction of the frame)'
      % (len(fo), np.abs(fo[:, 0]).max(), np.abs(fo[:, 1]).max()), flush=True)
print('  pixel scale      1 px = %.3f world units at the harness 512x768 grab'
      % (1.0 / S), flush=True)
print('  a 1-level height error therefore moves the silhouette by '
      '%.4f px at elevation 15 (x sin 15 deg)'
      % (c3.span / 255.0 * S * math.sin(math.radians(15))), flush=True)

# ---------------------------------------------- the rows, registration frozen
print(flush=True)
print('%-34s %8s %10s %9s %9s'
      % ('variant', 'IoU', 'IoU@shift', 'dy el15', 'dy el45'), flush=True)
rows = {}
rows['old_on'] = run(Sheets(TAG, 'cards_before'), 'shipped heights, ray on', True)
rows['old_off'] = run(Sheets(TAG, 'cards_before'), 'shipped heights, ray off', False)
rows['r3_on'] = run(Sheets(TAG, 'cards'), 'R3 plane-outside, ray on', True)
rows['new_on'] = run(c3, '8-ring dilation (this bake), ray on', True)
rows['new_off'] = run(Sheets3(TAG), '8-ring dilation, ray off', False)

print(flush=True)
print('R3 -> 8-ring : IoU %+.4f, shape@shift %+.4f, el15 dy %+.2f px, el45 dy %+.2f px'
      % (rows['new_on'][0] - rows['r3_on'][0], rows['new_on'][1] - rows['r3_on'][1],
         rows['new_on'][2] - rows['r3_on'][2], rows['new_on'][3] - rows['r3_on'][3]), flush=True)
print('shipped -> 8-ring: IoU %+.4f, el15 dy %+.2f px'
      % (rows['new_on'][0] - rows['old_on'][0],
         rows['new_on'][2] - rows['old_on'][2]), flush=True)
print('the ray itself on this bake: IoU %+.4f, el15 dy %+.2f px'
      % (rows['new_on'][0] - rows['new_off'][0],
         rows['new_on'][2] - rows['new_off'][2]), flush=True)
