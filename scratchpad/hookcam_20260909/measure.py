import sys, numpy as np
from PIL import Image
def span(path):
    im = Image.open(path).convert("RGB")
    a = np.asarray(im).astype(int)
    h, w, _ = a.shape
    # background = the modal corner colour
    corners = [tuple(a[0,0]), tuple(a[0,w-1]), tuple(a[h-1,0]), tuple(a[h-1,w-1])]
    bg = max(set(corners), key=corners.count)
    d = np.abs(a - np.array(bg)).sum(axis=2)
    m = d > 12
    if not m.any():
        return (w, h, 0, 0, None, None, bg)
    ys, xs = np.nonzero(m)
    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    return (w, h, x1-x0+1, y1-y0+1, (x0+x1+1)/2.0, (y0+y1+1)/2.0, bg)
for p in sys.argv[1:]:
    w,h,sw,sh,cx,cy,bg = span(p)
    print("%-46s %4dx%-4d  spanX=%-6s spanY=%-6s cx=%-8s cy=%-8s bg=%s" % (
        p.split('/')[-1], w, h, sw, sh,
        ("%.1f"%cx) if cx is not None else "-", ("%.1f"%cy) if cy is not None else "-", bg))
