# Frame check for a whole-map render: size read back, and the drawn content's bounding box against the image edges.
# Background = the colour of the four image corner pixels (must agree); content = any pixel differing from it by > 6.
# The terrain rectangle's four corners are extreme points of the content, so "bbox >= 20 px inside every edge" means
# every corner is inside the frame by at least that much.
import sys
import numpy as np
from PIL import Image
p = sys.argv[1]
im = np.asarray(Image.open(p).convert('RGB')).astype(int)
h, w, _ = im.shape
corners = [im[0, 0], im[0, -1], im[-1, 0], im[-1, -1]]
bg = corners[0]
agree = all(np.abs(c - bg).max() <= 6 for c in corners)
diff = np.abs(im - bg).max(-1) > 6
ys, xs = np.nonzero(diff)
if not len(xs):
    print('%s %dx%d: EMPTY (no content against background %s)' % (p, w, h, tuple(bg))); sys.exit(1)
l, r, t, b = xs.min(), w - 1 - xs.max(), ys.min(), h - 1 - ys.max()
m = min(l, r, t, b)
print('%s %dx%d bg %s (corners agree %s): content bbox x %d..%d y %d..%d; margins L %d R %d T %d B %d -> %s; content %.1f%%'
      % (p.replace('\\', '/').split('/')[-1], w, h, tuple(bg), agree, xs.min(), xs.max(), ys.min(), ys.max(), l, r, t, b,
         'INSIDE >=20px' if (m >= 20 and agree) else 'NOT INSIDE', 100.0 * diff.mean()))
