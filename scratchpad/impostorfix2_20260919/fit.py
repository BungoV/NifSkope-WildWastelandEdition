"""Fit the quad->pixel registration against the harness's own CARD grab."""
import sys, numpy as np, math
sys.path.insert(0,'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919')
from inst import *
from scipy.signal import fftconvolve

def best_offset(card_px, target):
    """card_px: small bool array; target: 768x512 bool. Return (inter, dy, dx)."""
    a = target.astype(np.float32)
    b = card_px.astype(np.float32)[::-1, ::-1]
    c = fftconvolve(a, b, mode='full')
    # c[y,x] = sum over overlap when card's top-left sits at (y-(bh-1), x-(bw-1))
    idx = np.unravel_index(np.argmax(c), c.shape)
    bh, bw = card_px.shape
    return c[idx], idx[0]-(bh-1), idx[1]-(bw-1)

def render_px(cs, d, s, **kw):
    W = max(4, int(round(2*cs.half[0]*s))); H = max(4, int(round(2*cs.half[1]*s)))
    m,_ = render(cs, d, (W,H), **kw)
    return m

def fit_scale(tag, which='after', shts=None, srange=None):
    cs = shts or Sheets(tag, 'cards' if which=='after' else 'cards_before')
    best=None
    for s in (srange if srange is not None else np.arange(0.90,1.25,0.01)):
        tot=0.0; n=0
        for (az,el) in VIEWS[:6]:
            g = grabmask(tag, which, az, el, 'card')
            if g is None: continue
            m = render_px(cs, dirOf(az,el), s)
            if m.shape[0]>g.shape[0] or m.shape[1]>g.shape[1]: continue
            inter,dy,dx = best_offset(m, g)
            uni = m.sum()+g.sum()-inter
            tot += inter/uni if uni else 0; n+=1
        if n and (best is None or tot/n>best[1]):
            best=(s, tot/n)
    return cs, best

if __name__=='__main__':
    for tag in ('blast_n4',):
        cs,b = fit_scale(tag)
        print(tag,'best scale %.3f  card-vs-card IoU %.4f'%b)
