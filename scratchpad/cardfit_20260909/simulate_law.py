#!/usr/bin/env python3
"""Predict the new frame law's fill from the measured union boxes.

TODAY  : G = max(4, tileLong/16); short side = tileLong * one of
         {1, .75, .5, .375, .25}, floored at max(32, 2G+4 rounded to 4).
         mips: chain stops while a frame's SHORTER side spans 8 texels.
NEW    : mips M = levels while the shorter side spans >= 16 texels;
         P = 2^(M-1)  (exactly the gutter the last shipped mip needs);
         short side = the union silhouette's aspect, quantised UP to a
         multiple of 16 texels, floor max(16, 2P+4 -> mult of 16), cap long.
"""
import json, os, math
import numpy as np

here = os.path.dirname(os.path.abspath(__file__))
rows = json.load(open(os.path.join(here, 'bbox_before.json')))

def mips_today(fw, fh):
    n, s = 1, min(fw, fh)
    while s > 8: s >>= 1; n += 1
    return n

def mips_new(fw, fh):
    n, s = 1, min(fw, fh)
    while (s >> 1) >= 16: s >>= 1; n += 1
    return n

def new_frame(tileLong, ratio):
    """ratio = union short extent / union long extent, in units"""
    # provisional: assume M from a square frame's short side, then settle
    for _ in range(4):
        pass
    # short side must satisfy: inner_short/inner_long == ratio
    # inner_long = tileLong - 2P ; short = inner_short + 2P
    best = None
    for shortq in range(16, tileLong + 1, 16):
        M = mips_new(shortq, tileLong)
        P = 1 << (M - 1)
        if shortq < 2 * P + 4:
            continue
        il, isq = tileLong - 2 * P, shortq - 2 * P
        if isq <= 0: continue
        # the frame must not CROP: inner aspect >= the silhouette's
        if isq / float(il) >= ratio - 1e-9:
            best = (shortq, P, M, il, isq)
            break
    if best is None:
        shortq = tileLong
        M = mips_new(shortq, tileLong); P = 1 << (M - 1)
        best = (shortq, P, M, tileLong - 2*P, shortq - 2*P)
    return best

print("%-9s %-9s %-6s %-9s %-6s  %-13s %-13s" % (
    "base", "old frame", "G/M", "new frame", "P/M", "old fill x,y", "new fill x,y"))
oldx, oldy, newx, newy = [], [], [], []
shapes_old, shapes_new = set(), set()
for r in rows:
    fw, fh = r['frame']; G = r['G']
    uw, uh = r['union_wh']            # texels of the present frame
    upx, upy = r['upx'], r['upy']
    Uw, Uh = uw * upx, uh * upy       # union box in UNITS
    tileLong = max(fw, fh)
    ratio = min(Uw, Uh) / max(Uw, Uh)
    shortq, P, M, il, isq = new_frame(tileLong, ratio)
    # the silhouette maps into the inner rect, keeping aspect, growing the loose axis
    if Uh >= Uw:
        nfw, nfh = shortq, tileLong
        fill_x = (Uw / Uh) * il / float(nfw)      # texels the silhouette spans / frame
        fill_y = il / float(nfh)
    else:
        nfw, nfh = tileLong, shortq
        fill_x = il / float(nfw)
        fill_y = (Uh / Uw) * il / float(nfh)
    # cap: the silhouette can never exceed the inner rect
    fill_x = min(fill_x, isq / float(nfw) if nfw == shortq else il / float(nfw))
    fill_y = min(fill_y, il / float(nfh) if nfh == tileLong else isq / float(nfh))
    ofx, ofy = uw / float(fw), uh / float(fh)
    print("%-9s %dx%-6d %d/%-4d %dx%-6d %d/%-4d  %.3f,%.3f   %.3f,%.3f" % (
        r['base'], fw, fh, G, mips_today(fw, fh), nfw, nfh, P, M, ofx, ofy, fill_x, fill_y))
    oldx.append(ofx); oldy.append(ofy); newx.append(fill_x); newy.append(fill_y)
    shapes_old.add((fw, fh)); shapes_new.add((nfw, nfh))
print()
for lbl, v in (("old fill x", oldx), ("new fill x", newx), ("old fill y", oldy), ("new fill y", newy)):
    a = np.array(v)
    print("%-11s min %.3f  median %.3f  max %.3f" % (lbl, a.min(), np.median(a), a.max()))
ax, an = np.array(oldx), np.array(newx)
ay, ay2 = np.array(oldy), np.array(newy)
print()
print("linear gain x: median %.2fx   area gain: median %.2fx" % (
    np.median(an/ax), np.median((an*ay2)/(ax*ay))))
print("distinct frame shapes: today %d %s   new %d %s" % (
    len(shapes_old), sorted(shapes_old), len(shapes_new), sorted(shapes_new)))
