"""CARDFIX1 step 6 (IMPOSTORWIND1 job 3, sway A): the measurements behind tests/spells/impostor_wind.sh.
Every sub-command prints ONE verdict line of numbers; the shell script owns the bars.

  g1   <cards> <fid>                  mask-0 texels and the crown, over every frame of the set
  proj <cards> <fid> <nif> <v>        reprojection: the bake's _n.A in ring frame v against W x h, W
                                      rasterised from the NIF's own vertex alpha (impostor_wind_nif.py,
                                      shares no code with NifSkope), h from the bake's own coverage rows
  g4   <cards> <fid> <n.dds>          BC7 error of the sway channel: the compressed _n against the
                                      bake's own PNG, covered texels; and the same against the NEXT
                                      frame's PNG (the red: a metric that cannot see a wrong picture)
  same <dirA> <dirB> <fid>            byte identity of every sheet + the .lodm; the sidecar with its
                                      `sway` line removed
  lodm <file>                         the payload's version, kind and sway keys
"""
import os, sys, json, struct, hashlib
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


def rgba(p):
    return np.asarray(Image.open(p).convert('RGBA')).astype(np.int64)


def layout(cards, fid):
    """(cols, rows, tw, th) from the sidecar's `ring V tw th` or `oct N tw th` line."""
    for l in open(os.path.join(cards, fid + '.txt')).read().split('\n'):
        t = l.split()
        if len(t) >= 4 and t[0] in ('ring', 'oct'):
            n, tw, th = int(t[1]), int(t[2]), int(t[3])
            return (n, 1, tw, th) if t[0] == 'ring' else (n, n, tw, th)
    raise SystemExit('no layout line')


def sheets(cards, fid):
    nrm = rgba(os.path.join(cards, fid + '_oct_normal.png'))
    alb = rgba(os.path.join(cards, fid + '_oct_albedo.png'))
    for s in ('gsaos', 'rmaos'):
        p = os.path.join(cards, fid + '_oct_%s.png' % s)
        if os.path.exists(p):
            return alb, nrm, rgba(p)
    raise SystemExit('no mask sheet')


def g1(cards, fid):
    alb, nrm, msk = sheets(cards, fid)
    cov = alb[..., 3] >= 128
    A = nrm[..., 3]
    m0 = cov & (msk[..., 3] < 128)
    m1 = cov & (msk[..., 3] >= 128)
    z = (A[m0] == 0).mean() if m0.any() else -1
    print('mask0 %d texels, share A=0 %.4f, max A %d | crown %d texels, distinct %d, share 255 %.4f, mean A %.1f'
          % (m0.sum(), z, A[m0].max() if m0.any() else -1, m1.sum(), len(np.unique(A[m1])),
             (A[m1] == 255).mean() if m1.any() else -1, A[m1].mean() if m1.any() else -1))


def proj(cards, fid, nif, v):
    import impostor_wind_nif as wn
    cols, rows, tw, th = layout(cards, fid)
    assert rows == 1, 'proj reads a RING set'
    alb, nrm, msk = sheets(cards, fid)
    sl = (slice(0, th), slice(v * tw, (v + 1) * tw))
    a, A, M = alb[sl][..., 3], nrm[sl][..., 3].astype(np.float64), msk[sl][..., 3]
    ys, xs = np.nonzero(a > 0)
    top, bottom, left, right = ys.min(), ys.max(), xs.min(), xs.max()
    h = np.clip((bottom - np.arange(th, dtype=np.float64)) / max(1.0, bottom - top), 0, None)[:, None] * np.ones((1, tw))
    R = 1024
    W, C = wn.raster(wn.load(nif), 2 * np.pi * v / cols, R)
    py, px = np.nonzero(C)
    Wc = (W * C)[py.min():py.max() + 1, px.min():px.max() + 1]
    Cc = C[py.min():py.max() + 1, px.min():px.max() + 1].astype(np.float64)
    bh = bottom - top + 1
    sc = bh / Cc.shape[0]
    bw = max(1, int(round(Cc.shape[1] * sc)))
    Wr = np.asarray(Image.fromarray(Wc.astype(np.float32)).resize((bw, bh), Image.BOX), np.float64)
    Cr = np.asarray(Image.fromarray(Cc.astype(np.float32)).resize((bw, bh), Image.BOX), np.float64)
    x0 = int(round(0.5 * (left + right) - 0.5 * (bw - 1)))
    Wp = np.zeros((th, tw)); Cp = np.zeros((th, tw))
    for j in range(bw):
        x = x0 + j
        if 0 <= x < tw:
            Wp[top:top + bh, x] = Wr[:, j]; Cp[top:top + bh, x] = Cr[:, j]
    Wn = np.where(Cp > 1e-6, Wp / np.maximum(Cp, 1e-6), 0.0)
    sel = (a >= 128) & (M >= 128) & (Cp >= 0.5)
    ref = Wn[sel] * h[sel]
    got = A[sel]
    ca = 255.0 * h[sel]                               # what a bake from C.a (forced 1) would write

    def score(x):
        e = np.abs(x - ref).mean()
        r = np.corrcoef(x, ref)[0, 1] if x.std() > 0 and ref.std() > 0 else 0.0
        return e, r
    e, r = score(got)
    ec, rc = score(ca)
    print('frame %d: %d crown texels compared (bbox width bake %d, reprojection %d): bake err %.2f r %.4f | '
          'C.a law err %.2f r %.4f | W mean %.1f' % (v, sel.sum(), right - left + 1, bw, e, r, ec, rc, Wn[sel].mean()))


def g4(cards, fid, dds):
    import impostor_bc_decode as bc
    img, (w, hgt), four = bc.load_dds(dds)
    Ad = np.rint(img[..., 3] * 255.0)
    alb, nrm, msk = sheets(cards, fid)
    A = nrm[..., 3].astype(np.float64)
    cols, rows, tw, th = layout(cards, fid)
    if A.shape != Ad.shape:
        raise SystemExit('size %s vs %s (auxDiv?)' % (A.shape, Ad.shape))
    cov = alb[..., 3] >= 128
    e = np.abs(Ad - A)[cov]
    Ash = np.roll(A, tw, axis=1)                      # the NEXT frame's picture in this frame's place
    es = np.abs(Ad - Ash)[cov]
    Rd = np.rint(img[..., 0] * 255.0); Gd = np.rint(img[..., 1] * 255.0)
    erg = np.concatenate([np.abs(Rd - nrm[..., 0])[cov], np.abs(Gd - nrm[..., 1])[cov]])
    Aq = np.floor(Ad / 16.0) * 16.0 + 8.0              # RED: the sway channel corrupted to 4 bits
    eq = np.abs(Aq - A)[cov]
    print('%s %dx%d: %d covered texels, sway error mean %.3f p95 %.1f max %d | against the next frame: mean %.3f p95 %.1f'
          % (four.decode().strip(), w, hgt, cov.sum(), e.mean(), np.percentile(e, 95), e.max(), es.mean(), np.percentile(es, 95))
          + ' | normal R/G error mean %.3f p95 %.1f' % (erg.mean(), np.percentile(erg, 95))
          + ' | 4-bit sway: mean %.3f p95 %.1f' % (eq.mean(), np.percentile(eq, 95)))


def same(a, b, fid):
    diff, n = [], 0
    for f in sorted(os.listdir(a)):
        if not f.startswith(fid) or f.endswith('.txt'):
            continue
        n += 1
        pb = os.path.join(b, f)
        if not os.path.exists(pb) or open(os.path.join(a, f), 'rb').read() != open(pb, 'rb').read():
            diff.append(f)

    def meta(d):
        # 'specular none' is the legacy card's line from CARDFIX1 step 7 (1f0368d, the .pbrm card bake); the
        # previous exe predates it. Only that exact line is dropped: '_s' on a legacy model still DIFFERS.
        return [l for l in open(os.path.join(d, fid + '.txt')).read().split('\n')
                if not l.startswith('sway ') and l != 'specular none']
    side = meta(a) == meta(b)
    print('%d files compared, %d differ%s; sidecar without its sway line %s' % (
        n, len(diff), (' (' + ' '.join(diff[:4]) + ')') if diff else '', 'identical' if side else 'DIFFERS'))


def lodm(p):
    b = open(p, 'rb').read()
    j = json.loads(b[12:])
    arr = j.get('array', {})
    print('lodm %s kind %s sway %s leafAmplitude %s leafFrequency %s | array sway %s' % (
        j.get('lodm'), j.get('kind'), j.get('sway'), j.get('leafAmplitude'), j.get('leafFrequency'),
        arr.get('sway')))


if __name__ == '__main__':
    c = sys.argv[1]
    if c == 'g1':
        g1(sys.argv[2], sys.argv[3])
    elif c == 'proj':
        proj(sys.argv[2], sys.argv[3], sys.argv[4], int(sys.argv[5]))
    elif c == 'g4':
        g4(sys.argv[2], sys.argv[3], sys.argv[4])
    elif c == 'same':
        same(sys.argv[2], sys.argv[3], sys.argv[4])
    elif c == 'lodm':
        lodm(sys.argv[2])
