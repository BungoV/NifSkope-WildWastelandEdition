"""MERGE1: every labelled map picture -- title bar height, body not blank. Prints one line per picture and a verdict.
A body is BLANK when fewer than 0.5% of its pixels differ from its most common colour (the empty background).
Red control: a synthetic all-grey picture must read BLANK."""
import sys, glob, os
import numpy as np
from PIL import Image

D = sys.argv[1]


def body_stats(a):
    flat = a.reshape(-1, a.shape[-1])
    vals, cnt = np.unique(flat, axis=0, return_counts=True)
    top = vals[cnt.argmax()]
    other = float((np.abs(flat.astype(int) - top.astype(int)).max(axis=1) > 6).mean())
    return other, tuple(int(x) for x in top)


grey = np.full((200, 200, 3), 128, np.uint8)
assert body_stats(grey)[0] < 0.005, 'red control did not fire'
bad = []
for p in sorted(glob.glob(os.path.join(D, '*.png'))):
    im = np.asarray(Image.open(p).convert('RGB'))
    h, w = im.shape[:2]
    other, top = body_stats(im[60:])
    tb = im[:60]
    ok = other >= 0.005
    print('%-60s %5dx%-5d body %6.2f%% not background %s %s' % (os.path.basename(p)[:60], w, h, 100 * other, top, '' if ok else 'BLANK'))
    if not ok:
        bad.append(os.path.basename(p))
print('pictures %d, blank %d %s' % (len(glob.glob(os.path.join(D, '*.png'))), len(bad), bad))
print('red control (all-grey picture reads blank): fired')
