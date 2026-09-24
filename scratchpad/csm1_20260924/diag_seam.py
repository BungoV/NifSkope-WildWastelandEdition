"""where do seam_p5's pixels differ from the judge? one verdict line + an overlay"""
import sys, os
import numpy as np
from PIL import Image
sys.path.insert(0, r"E:\Projects\NifskopeWildWastelandEdition\tests\spells")
import pbr_csm1_gates as J

OUT = r"E:\Projects\NifskopeWildWastelandEdition\scratchpad\csm1_20260924\g3"
e = J.parse_echo(os.path.join(OUT, "seam_p5.csm.txt"))
print("echo from", "csm.txt" if e else "none", e.get("cam") if e else "")
p5 = np.asarray(Image.open(os.path.join(OUT, "seam_p5.png")).convert("RGB")).astype(float) / 255
cam = e["camo"]
H, W = p5.shape[:2]
d, G, ground, cube, P, axis = J.scene_pixels(cam, W, H)
dv = (G - cam.C) @ cam.fwd
A, Bc, T = J.select(dv)
exp = np.stack([A * 0.5, Bc * 0.5, T], -1)
exp = np.where((dv > e["Dn"] + J.BLEND)[..., None], 1.0, exp)
err = np.abs(p5 - exp)
bad = ground & ((err[..., 0] > 2.5 / 255) | (err[..., 1] > 2.5 / 255) | (err[..., 2] > 3.5 / 255))
ys, xs = np.nonzero(bad)
print("size", W, H, "bad", bad.sum(), "rows %d..%d cols %d..%d" % (ys.min(), ys.max(), xs.min(), xs.max()))
print("dv of bad: %.0f..%.0f" % (dv[bad].min(), dv[bad].max()))
vals, cnt = np.unique((p5[bad] * 255).round().astype(int), axis=0, return_counts=True)
o = np.argsort(-cnt)[:4]
print("measured colours of bad:", [(tuple(vals[k]), cnt[k]) for k in o])
ve, ce = np.unique((exp[bad] * 255).round().astype(int), axis=0, return_counts=True)
o = np.argsort(-ce)[:4]
print("expected colours of bad:", [(tuple(ve[k]), ce[k]) for k in o])
ov = (p5 * 255).astype(np.uint8)
ov[bad] = (255, 0, 255)
Image.fromarray(ov).resize((W // 2, H // 2)).save(os.path.join(os.path.dirname(OUT), "diag_seam.png"))
