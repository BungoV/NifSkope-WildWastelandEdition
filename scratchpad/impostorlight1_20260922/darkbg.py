# IMPOSTORLIGHT1 -- how many COVERED pixels the harness silhouette rule
# (src/impostorpreviewtest.cpp:384-405: within 12 of the clear colour on every
# channel = background) throws away because they are merely DARK.
# Covered = the stage-1 normals grab of the same view (a normal colour is never
# the clear colour). Prints, per subject, the share of covered mesh and card
# pixels that the rule reads as background in the LIT grab of each root.
#   python darkbg.py MASKROOT LITROOT[=label] ...
import sys, glob, os
import numpy as np
from PIL import Image

BG = np.array([43, 45, 49])
SUBJ = ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4')


def img(p):
    return np.asarray(Image.open(p).convert('RGB')).astype(int)


def isbg(a):
    return (np.abs(a - BG) <= 12).all(-1)


maskroot = sys.argv[1]
for arg in sys.argv[2:]:
    root, _, lab = arg.partition('=')
    for t in SUBJ:
        n = {'mesh': [0, 0], 'card': [0, 0]}
        for m1 in sorted(glob.glob('%s/%s/*_mesh.png' % (maskroot, t))):
            for kind in ('mesh', 'card'):
                p1 = m1.replace('_mesh.png', '_%s.png' % kind)
                p = p1.replace(maskroot, root)
                if not (os.path.exists(p1) and os.path.exists(p)):
                    continue
                cov = ~isbg(img(p1))
                n[kind][0] += int(cov.sum()); n[kind][1] += int((cov & isbg(img(p))).sum())
        if n['mesh'][0]:
            print('%-10s %-9s dark-read-as-background: mesh %6.2f%% of %8d   card %6.2f%% of %8d' % (
                lab, t, 100.0 * n['mesh'][1] / n['mesh'][0], n['mesh'][0], 100.0 * n['card'][1] / max(1, n['card'][0]), n['card'][0]))
