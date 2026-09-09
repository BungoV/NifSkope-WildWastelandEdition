#!/usr/bin/env python3
"""Cross-frame MIP BLEED on a baked card sheet, measured, with a control.

The sheet's mip chain is built with lodgen's own filter (2x2 box, rounded
half-up).  Frames never mix during CONSTRUCTION (a frame side stays even), so
the bleed is at SAMPLE time: a reader sampling anywhere inside a frame's UV
rect bilinearly reaches half a texel past the rect at the rect's own border,
and that tap lands in the NEIGHBOURING frame.

Bleed is measured on ALPHA (the coverage, i.e. the silhouette) because that is
what a reader cuts on.  For every frame border at every shipped mip:

    bleed = the neighbour frame's alpha contribution to a bilinear sample
            taken exactly on the border  ( = 0.5 * neighbour edge texel alpha )

CONTROL (must FAIL): the same sheet with the gutter removed -- each frame's
inner rect re-tiled with no padding at all.  If the metric cannot see that, it
is not a metric.
"""
import sys, os, glob, json
import numpy as np
from PIL import Image

def sidecar(p):
    d = {}
    for ln in open(p, encoding='utf-8', errors='replace'):
        t = ln.split()
        if t and t[0] == 'oct' and len(t) >= 12:
            d.update(oct=int(t[1]), fw=int(t[2]), fh=int(t[3]))
        elif t and t[0] == 'pad' and len(t) >= 3:
            d.update(padX=int(t[1]), padY=int(t[2]))
    return d

def boxdown(a):
    """lodgen's mip filter: 2x2 box, (acc+2)>>2, on uint8 RGBA."""
    h, w = a.shape[:2]
    h2, w2 = h // 2, w // 2
    b = a[:h2*2, :w2*2].astype(np.uint32)
    acc = b[0::2, 0::2] + b[0::2, 1::2] + b[1::2, 0::2] + b[1::2, 1::2]
    return ((acc + 2) >> 2).astype(np.uint8)

def bleed_of(sheet, N, fw, fh, mips):
    """per shipped mip: the max neighbour alpha contribution over every border"""
    out = []
    a = sheet
    for k in range(mips):
        if k:
            a = boxdown(a)
        fwk, fhk = fw >> k, fh >> k
        if fwk < 1 or fhk < 1:
            out.append(dict(mip=k, frame=[fwk, fhk], gutter_texels=None,
                            max_bleed=None, borders_bleeding=None, note="frame gone"))
            continue
        al = a[:, :, 3].astype(np.int32)
        worst = 0
        nbad = 0
        ntot = 0
        # vertical borders: between frame i and i+1, column x = (i+1)*fwk
        for i in range(N - 1):
            x = (i + 1) * fwk
            left = al[:, x - 1]       # last texel of frame i
            right = al[:, x]          # first texel of frame i+1
            # a sample on the border in frame i gets 0.5*right from the neighbour
            for src, nbr in ((left, right), (right, left)):
                ntot += 1
                c = int(nbr.max()) // 2
                worst = max(worst, c)
                if c > 0:
                    nbad += 1
        # horizontal borders
        for j in range(N - 1):
            y = (j + 1) * fhk
            top = al[y - 1, :]
            bot = al[y, :]
            for src, nbr in ((top, bot), (bot, top)):
                ntot += 1
                c = int(nbr.max()) // 2
                worst = max(worst, c)
                if c > 0:
                    nbad += 1
        out.append(dict(mip=k, frame=[fwk, fhk], max_bleed=worst,
                        borders_bleeding=nbad, borders=ntot))
    return out

def strip_gutter(sheet, N, fw, fh, px, py):
    """the zero-padding CONTROL: each frame's inner rect re-tiled edge to edge"""
    iw, ih = fw - 2*px, fh - 2*py
    out = np.zeros((N*ih, N*iw, 4), np.uint8)
    for j in range(N):
        for i in range(N):
            out[j*ih:(j+1)*ih, i*iw:(i+1)*iw] = sheet[j*fh+py:j*fh+fh-py,
                                                      i*fw+px:i*fw+fw-px]
    return out, iw, ih

def main(dirpath, mips_by_frame):
    rows = []
    for p in sorted(glob.glob(os.path.join(dirpath, '*.txt'))):
        bid = os.path.basename(p)[:-4]
        d = sidecar(p)
        if not d: continue
        alb = os.path.join(dirpath, bid + '_oct_albedo.png')
        if not os.path.exists(alb): continue
        sheet = np.asarray(Image.open(alb).convert('RGBA'))
        N, fw, fh = d['oct'], d['fw'], d['fh']
        # the padding as the bake recorded it; a bake from before the `pad`
        # line carries the old max(4, longSide/16) on both axes
        px = d.get('padX', max(4, max(fw, fh) // 16))
        py = d.get('padY', max(4, max(fw, fh) // 16))
        G = min(px, py)
        OPT.clear()
        if 'padX' in d:
            OPT['padX'] = d['padX']
        mips = mips_by_frame(fw, fh, px, py)
        r = bleed_of(sheet, N, fw, fh, mips)
        ctl_sheet, iw, ih = strip_gutter(sheet, N, fw, fh, px, py)
        rc = bleed_of(ctl_sheet, N, iw, ih, mips)
        rows.append(dict(base=bid, frame=[fw, fh], G=G, mips=mips,
                         shipped=r, control=rc))
        print("%-9s frame %dx%-4d G=%-2d mips=%d" % (bid, fw, fh, G, mips))
        for m in r:
            print("    mip %d  frame %dx%-4d  gutter %.2f tx  max alpha bleed %s  borders %s/%s"
                  % (m['mip'], m['frame'][0], m['frame'][1], G / float(1 << m['mip']),
                     m['max_bleed'], m['borders_bleeding'], m['borders']))
        print("    CONTROL (gutter stripped): mip0 max bleed %s / %s borders" %
              (rc[0]['max_bleed'], rc[0]['borders_bleeding']))
    here = os.path.dirname(os.path.abspath(__file__))
    json.dump(rows, open(os.path.join(here, 'bleed_%s.json' % sys.argv[2]), 'w'), indent=1)

# THE SHIPPED CAP. Before 2026-09-09: the chain stopped while a frame's SHORTER
# side spanned 8 texels, a rule with no relation to the gutter. Now: one whole
# texel of padding at the deepest level, on both axes.
def mips_shipped(fw, fh, px, py):
    if 'padX' in OPT:           # a bake that carries `pad` -> the padding decides
        n, g = 1, min(px, py)
        while g >= 2:
            g >>= 1; n += 1
        return n
    n, s = 1, min(fw, fh)
    while s > 8:
        s >>= 1; n += 1
    return n

OPT = {}
main(sys.argv[1], mips_shipped)
