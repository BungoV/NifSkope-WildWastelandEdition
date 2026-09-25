"""DG1 (pre-registered 14:16, after the broken-path exe b57a/471a was found and BEFORE the fixed exe was measured):
the engine-default ground must paint CommonwealthDefault01_d's own colour, not the missing-texture colour.
On a control bake (controls.sh a, --grass-tint 0) over the Sanctuary block + ring, the share of GREY texels
(chroma max-min < 12 AND luminance > 100) must be no more than 2x the OLD exe's share on the same control, plus
0.5 percentage points.
RE-REGISTERED 14:21: the first form (luminance > 150) read GREEN on the broken exe too (share 0.0000 on both) --
the missing-texture colour sits at luminance ~100-130, under that bar, so it could not fail. Measured on the
second form before it was adopted: old 0.0064, broken 0.1635, fixed 0.0064.
The broken exe (unknown "\\G" escape, path named no file) must read RED.
usage: default_gate.py <old control dir> <new control dir>   e.g. ctl/old_t0/a ctl/new_t0/a"""
import sys, os, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import vtread
V = '/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt'
def pale(d):
    m, _, _ = vtread.Vt(d + V).mosaic(-24, 16, -13, 27, 1)
    lum = m[..., :3].astype(np.float32) @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    c = m[..., :3].astype(np.float32); chroma = c.max(-1) - c.min(-1)
    return float(((chroma < 12) & (lum > 100)).mean()), m[..., :3].reshape(-1, 3).mean(0)
po, mo = pale(sys.argv[1]); pn, mn = pale(sys.argv[2])
ok = pn <= 2 * po + 0.005
print('DG1 grey share old %.4f new %.4f (bar %.4f) | mean old (%.0f %.0f %.0f) new (%.0f %.0f %.0f) -> %s' % (
    po, pn, 2 * po + 0.005, *mo, *mn, 'GREEN' if ok else 'RED'))
