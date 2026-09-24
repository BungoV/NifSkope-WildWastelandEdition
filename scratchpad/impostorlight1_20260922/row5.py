# IMPOSTORLIGHT1 -- row 5 decomposed (see row5.sh).
#   python row5.py ROW5DIR
# For each shader folder (old, fix): mean over the 16 views of
#   IoU(rule)  = card and mesh silhouettes both by the harness rule
#                (a pixel within 12 of (43,45,49) on EVERY channel is background)
#                -- what row 5 prints;
#   IoU(true)  = the same, but the CARD silhouette taken from the s1 normals
#                grab of the same view, where a dark pixel cannot be read as bg;
#   dark%      = covered card pixels (s1) the rule drops in the lit grab.
# If IoU(true) >= 0.50 while IoU(rule) < 0.50 on the old shaders, the red row
# was dark card pixels read as background, not a silhouette defect.
import glob, os, sys
import numpy as np
from PIL import Image

BG = np.array([43, 45, 49])


def img(p):
    return np.asarray(Image.open(p).convert('RGB')).astype(int)


def rule(a):
    return ~(np.abs(a - BG) <= 12).all(-1)


def iou(a, b):
    u = (a | b).sum()
    return (a & b).sum() / u if u else float('nan')


d = sys.argv[1]
t = 'blast_n4'
ms = sorted(glob.glob('%s/s1/%s/*_mesh.png' % (d, t)))
print('views', len(ms))
for lab in ('old', 'fix'):
    ir, it, dk, cv = [], [], 0, 0
    for m1 in ms:
        b = os.path.basename(m1)
        pm, pc = '%s/%s/%s/%s' % (d, lab, t, b), '%s/%s/%s/%s' % (d, lab, t, b.replace('_mesh', '_card'))
        if not (os.path.exists(pm) and os.path.exists(pc)):
            print('missing', pm); continue
        mlit, clit = img(pm), img(pc)
        ctrue = rule(img(m1.replace('_mesh', '_card')))
        mrule, crule = rule(mlit), rule(clit)
        ir.append(iou(mrule, crule)); it.append(iou(mrule, ctrue))
        dk += int((ctrue & ~crule).sum()); cv += int(ctrue.sum())
    print('%-4s IoU(rule) %.4f   IoU(true card silhouette) %.4f   dark card px read as bg %.2f %% of %d' % (
        lab, np.mean(ir), np.mean(it), 100.0 * dk / max(1, cv), cv))
