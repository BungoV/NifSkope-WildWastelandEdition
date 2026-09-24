"""DEFECT 1 -- FAT INK, split by cause, ONE STAGE AT A TIME.

Baseline is the card exactly as IMPOSTORFIX3 ships it (8-ring sheets, parallax
on, three frames, threshold = the set's coverage floor, mip 0). Each row below
changes ONE thing and nothing else, through the SAME frozen registration, over
the SAME 24 orbit views. `ink` is card-covered pixels / mesh-covered pixels,
meaned per view (1.00 is the mesh itself).

The stages, and what each one is:
  thr 0.20 / 0.50  the OWED ALPHA RULING. Reported, never applied. The set's
                   default threshold IS the 16/255 coverage floor, so today a
                   texel whose measured coverage is 6.27 per cent paints as
                   solidly as one that is covered whole.
  pre-BC3 alpha    the `_d` sheet's alpha taken from the pre-compression PNG
                   instead of the decoded DDS: the BC3 alpha round trip's own
                   share, with everything else identical.
  1 frame          the three-frame union removed (nearest frame, weight 1).
  no parallax      the height step removed.
  mip 1            one box downsample of both sheets. The drawer NEVER does
                   this -- every fetch in impostor_oct.frag is
                   textureLod(...,0.0) -- so this row is the counterfactual,
                   not a description of today.
"""
import os, sys, glob, json
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inst4 import *
from cal4 import alpha_ref, mask_at, place, load, TAGS, SHAPE

CAL = load()

def png_alpha_sheet(cs):
    """The `_d` sheet as it was BEFORE the DDS encode: RGB from the decoded DDS
       (so only ALPHA changes) and alpha from the pre-compression PNG."""
    g = glob.glob(cs.dir + '*_oct_albedo.png')
    if not g:
        return None
    p = np.asarray(Image.open(g[0]).convert('RGBA')).astype(np.float64) / 255.0
    if p.shape[:2] != cs.alb.shape[:2]:
        return None
    out = cs.alb.copy(); out[..., 3] = p[..., 3]
    return out

STAGES = [
    ('today (shipping)',      dict()),
    ('thr 0.20 (ruling A)',   dict(thresh=0.20)),
    ('thr 0.50 (spec crowns)',dict(thresh=0.50)),
    ('pre-BC3 alpha',         dict(_pngalpha=True)),
    ('1 frame (no union)',    dict(nframes=1)),
    ('no parallax',           dict(parallax=False)),
    ('mip 1 (counterfactual)',dict(mip=1)),
]

def run(tag):
    key = '%s|fixture|cards' % tag
    c = CAL[key]; s = c['scale']; offs = c['off']
    cs = Sheets(tag, 'cards', root=R3, sub='fixture')
    M = {v: grabmask(R3, tag, 'cards', *v, 'mesh') for v in VIEWS}
    pa = png_alpha_sheet(cs)
    out = []
    for label, kw in STAGES:
        kw = dict(kw)
        if kw.pop('_pngalpha', False):
            if pa is None:
                out.append((label, float('nan'), float('nan'), float('nan'))); continue
            kw['albOverride'] = pa
        thr = kw.pop('thresh', None)
        ious, inks = [], []
        for v in VIEWS:
            dy, dx = offs['%d_%d' % v]
            a = alpha_ref(cs, dirOf(*v), **kw)
            m = place(mask_at(a, cs, s, cs.covFloor if thr is None else thr), dy, dx)
            ious.append(iou(m, M[v]))
            mm = M[v].sum()
            inks.append(m.sum() / mm if mm else np.nan)
        out.append((label, float(np.mean(ious)), float(np.mean(inks)), float(np.min(ious))))
    return out

if __name__ == '__main__':
    res = {}
    hdr = '%-24s' % 'stage' + ''.join('%18s' % t for t in TAGS)
    for tag in TAGS:
        res[tag] = run(tag)
        print('%s done' % tag, flush=True)
        json.dump(res, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 's1_stages.json'), 'w'), indent=1)
    print()
    print('DEFECT 1 -- ink ratio (card ink / mesh ink) and IoU, 24 views, frozen registration')
    print(hdr)
    print('%-24s' % '' + ''.join('%18s' % 'ink / IoU' for t in TAGS))
    for i, (label, _, _, _) in enumerate(res[TAGS[0]]):
        line = '%-24s' % label
        for t in TAGS:
            _, io, ink, _ = res[t][i]
            line += '%10.3f /%6.4f' % (ink, io)
        print(line)
