"""Span of the CUBE only: the flat grey face, not the 3D-cursor gizmo.

The gizmo is white/red/black; the lit cube face is a single flat grey, so the
measurement takes the modal non-background colour and its bounding box.  A
floor is built in: fewer than 64 pixels of that colour is reported as NONE, so
a picture with no cube in it cannot quietly return a number.
"""
import sys, numpy as np
from PIL import Image

def cube_box(path):
    a = np.asarray(Image.open(path).convert("RGB")).astype(int)
    h, w, _ = a.shape
    corners = [tuple(a[0,0]), tuple(a[0,w-1]), tuple(a[h-1,0]), tuple(a[h-1,w-1])]
    bg = max(set(corners), key=corners.count)
    d = np.abs(a - np.array(bg)).sum(axis=2)
    m = d > 12
    if m.sum() < 64:
        return None
    px = a[m].reshape(-1, 3)
    vals, counts = np.unique(px, axis=0, return_counts=True)
    face = vals[np.argmax(counts)]
    n = counts.max()
    if n < 64:
        return None
    sel = (np.abs(a - face).sum(axis=2) <= 6)
    ys, xs = np.nonzero(sel)
    return dict(w=w, h=h, x0=int(xs.min()), x1=int(xs.max()), y0=int(ys.min()),
                y1=int(ys.max()), sx=int(xs.max()-xs.min()+1), sy=int(ys.max()-ys.min()+1),
                cx=(xs.min()+xs.max()+1)/2.0, cy=(ys.min()+ys.max()+1)/2.0,
                face=tuple(int(v) for v in face), n=int(n))

if __name__ == "__main__":
    for p in sys.argv[1:]:
        r = cube_box(p)
        if r is None:
            print("%-30s NONE" % p.split('/')[-1]); continue
        print("%-30s %4dx%-4d spanX=%-5d spanY=%-5d cx=%-8.1f cy=%-8.1f face=%s n=%d"
              % (p.split('/')[-1], r['w'], r['h'], r['sx'], r['sy'], r['cx'], r['cy'], r['face'], r['n']))
