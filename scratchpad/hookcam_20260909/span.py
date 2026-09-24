import sys, json
import numpy as np
from PIL import Image

def edges(prof, thr, bright):
    """Sub-pixel left and right crossings of thr in a 1-D profile."""
    m = (prof > thr) if bright else (prof < thr)
    idx = np.nonzero(m)[0]
    if idx.size == 0:
        return None
    # the widest contiguous run, so any stray marker cannot widen the answer
    runs, s = [], idx[0]
    for a, b in zip(idx[:-1], idx[1:]):
        if b != a + 1:
            runs.append((s, a)); s = b
    runs.append((s, idx[-1]))
    lo, hi = max(runs, key=lambda r: r[1] - r[0])

    def cross(i, j):
        # Where the straight line through prof[i] and prof[j] passes thr.
        # The (j - i) factor is not decoration: the right-hand edge is called
        # with j = i - 1, and without it the crossing lands one pixel OUTSIDE
        # the object and every span reads 1 px too wide. Caught on a hard
        # (unantialiased) edge, where the answer must come out exactly on the
        # pixel boundary: 637.5 and 868.5 for a 231.0 px cube.
        a, b = float(prof[i]), float(prof[j])
        if a == b:
            return float(j)
        return i + (thr - a) / (b - a) * (j - i)
    left = cross(lo - 1, lo) if lo > 0 else float(lo) - 0.5
    right = cross(hi + 1, hi) if hi + 1 < prof.size else float(hi) + 0.5
    return left, right

def measure(path):
    a = np.asarray(Image.open(path).convert("RGB")).astype(float)
    h, w, _ = a.shape
    lum = 0.299 * a[:, :, 0] + 0.587 * a[:, :, 1] + 0.114 * a[:, :, 2]
    bg = float(np.median([lum[0, 0], lum[0, w - 1], lum[h - 1, 0], lum[h - 1, w - 1]]))
    far = lum[np.abs(lum - bg) > 8.0]
    out = dict(w=w, h=h, bg=bg, n=int(far.size), spanx=0.0, spany=0.0, cx=0.0, cy=0.0, face=bg)
    if far.size < 64:                       # the floor: no object, no number
        return out
    vals, counts = np.unique(np.round(far).astype(int), return_counts=True)
    face = float(vals[int(np.argmax(counts))])
    out["face"] = face
    bright = face > bg
    thr = 0.5 * (bg + face)
    ex = edges(lum[h // 2], thr, bright)
    if ex:
        out["spanx"] = ex[1] - ex[0]
        out["cx"] = 0.5 * (ex[0] + ex[1])
    ey = edges(lum[:, int(round(out["cx"])) if ex else w // 2], thr, bright)
    if ey:
        out["spany"] = ey[1] - ey[0]
        out["cy"] = 0.5 * (ey[0] + ey[1])
    return out

print(json.dumps(measure(sys.argv[1])))
