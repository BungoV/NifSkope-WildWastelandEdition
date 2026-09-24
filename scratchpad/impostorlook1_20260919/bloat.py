# IMPOSTORLOOK1 section 3 -- WHERE THE FAT AND THE LUMPS COME FROM.
#
# period.py could not tell a texel-shaped edge from a pixel-shaped one, and says
# so: in those grabs one sheet texel is 0.78-1.24 SCREEN PIXELS, so the two
# grids coincide and no phase test can separate them. What it did settle is that
# the edge is not on a 4-texel grid either (a BC block), and a bake direction
# rules out parallax by construction. So the lumps are not a block artefact.
#
# This script measures the thing that is left, INSIDE THE SHEET, where no
# viewer, no camera and no screen resolution are involved:
#
#   the sheet's coverage does not go from 0 to 1 at the silhouette, it RAMPS,
#   because the bake dilates/floods colour outwards so filtering does not drag
#   background in. The drawer keeps every texel above the sheet's own floor
#   (0.0627). So the card's outline is the 0.0627 iso-contour of a ramp whose
#   half-way line is the real edge -- and the gap between those two contours is
#   exactly how much fatter the trunk is, in texels, per side.
#
# Reported per subject, on the trunk band of one bake frame:
#   width at cov>=floor   what the card draws
#   width at cov>=0.5     where the silhouette really is
#   bloat per side        half the difference, in TEXELS
#   ramp rows             how many texels the ramp itself spans
# OFFLINE.
import glob, json
import sys
import numpy as np
REPO = 'E:/Projects/NifskopeWildWastelandEdition'
sys.path.insert(0, REPO + '/tests/spells')
import impostor_bc_decode as bc
SC = REPO + '/scratchpad'

def frame(tag, i=0, j=0):
    d = '%s/impostorfix5_20260919/fixture/%s/cards' % (SC, tag)
    raw = open(glob.glob(d+'/*_oct.lodm')[0], 'rb').read()
    m = json.loads(raw[raw.index(b'{'):].decode('utf-8'))['card']
    N = int(m['oct']); fl = m['coverage']['floor']/255.; ba = m['coverage']['base']/255.
    a, _, _ = bc.load_dds(glob.glob(d+'/*_oct_d.DDS')[0])
    H, W = a.shape[:2]; fw, fh = W//N, H//N
    al = a[j*fh:(j+1)*fh, i*fw:(i+1)*fw, 3]
    cov = np.where(al < ba, 0.0, np.clip(fl+(al-ba)*(1-fl)/(1-ba), fl, 1.0))
    return cov, fl, (fw, fh)

print('%-9s %6s %9s %9s %9s %9s %8s' % ('subject', 'frame', 'w@floor', 'w@0.5', 'bloat/side', 'ramp', 'partial%'))
for tag in ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4'):
    cov, fl, (fw, fh) = frame(tag)
    ink = cov >= fl
    ys, xs = np.nonzero(ink)
    y0, y1 = ys.min(), ys.max()
    lo = int(y0+(y1-y0)*0.66); hi = int(y0+(y1-y0)*0.97)
    wa, wb, ramp = [], [], []
    for y in range(lo, hi+1):
        r1 = np.nonzero(cov[y] >= fl)[0]
        r2 = np.nonzero(cov[y] >= 0.5)[0]
        if len(r1) < 2 or len(r2) < 1: continue
        wa.append(r1.max()-r1.min()+1); wb.append(r2.max()-r2.min()+1)
        ramp.append(((cov[y] > fl) & (cov[y] < 0.9)).sum())
    if not wa: print('%-9s  (no trunk band)' % tag); continue
    wa = float(np.median(wa)); wb = float(np.median(wb))
    part = float(((cov >= fl) & (cov < 0.9)).sum())/max(1, int(ink.sum()))
    print('%-9s %3dx%-3d %9.1f %9.1f %10.2f %9.1f %7.0f%%'
          % (tag, fw, fh, wa, wb, (wa-wb)/2.0, float(np.median(ramp)), 100*part))
