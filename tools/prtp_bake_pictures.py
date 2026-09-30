# prtp_bake_pictures.py -- lane PRTPBAKE: four top-down views of a baked .tbk folder, from its bytes only
#   python tools/prtp_bake_pictures.py <bakedir> <out.png> [title] [zmax: a floor cut for interiors]
import glob, math, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tests', 'spells'))
import probe_bake as pb

PANEL = 760


def srgb(lin):
    lin = np.clip(lin, 0, 1)
    return np.where(lin <= 0.0031308, lin * 12.92, 1.055 * np.power(lin, 1 / 2.4) - 0.055)


def load(d):
    S, P, links = [], [], []
    for f in sorted(glob.glob(os.path.join(d, 'sector_*.tbk'))):
        t = pb.read_tbk(f)
        cell = t['cell']
        s = t['surfels']
        keys = {tuple(pb.floordiv(p[a], cell) for a in range(3)): i + len(S) for i, p in enumerate(s['pos'])}
        base = len(S)
        S.extend(s)
        for pr in t['probes']:
            pk = tuple(pb.floordiv(pr['pos'][a], cell) for a in range(3))
            L = t['links'][pr['off']:pr['off'] + pr['cnt']]
            ids = [keys.get(tuple(pk[a] + int(l['delta'][a]) for a in range(3)), -1) for l in L]
            w = L['w'].astype(np.float64) * float(pr['scale'])
            P.append(pr)
            links.append((np.array(ids), w))
    return np.array(S, dtype=pb.SURFEL), np.array(P, dtype=pb.PROBE), links


def main(d, out, title, zmax=None):
    S, P, links = load(d)
    if zmax is not None:
        # a floor cut: surfels and probes below zmax only (links re-pointed to the kept surfels)
        keepS = S['pos'][:, 2] < zmax
        remap = np.cumsum(keepS) - 1
        S = S[keepS]
        keepP = P['pos'][:, 2] < zmax
        P = P[keepP]
        links = [(np.where((ids >= 0) & keepS[np.maximum(ids, 0)], remap[np.maximum(ids, 0)], -1), w)
                 for (ids, w), k in zip(links, keepP) if k]
    pos = S['pos'].astype(np.float64)
    lo = np.minimum(pos[:, :2].min(0), P['pos'][:, :2].min(0)) - 100
    hi = np.maximum(pos[:, :2].max(0), P['pos'][:, :2].max(0)) + 100
    scale = (PANEL - 20) / float(max(hi - lo))
    W = int((hi[0] - lo[0]) * scale) + 20
    H = int((hi[1] - lo[1]) * scale) + 20

    def px(x, y):
        return 10 + (x - lo[0]) * scale, H - 10 - (y - lo[1]) * scale

    cellpx = max(1.0, 70 * scale)
    alb = srgb(S['alb'].astype(np.float64) / 255.0)
    order = np.argsort(pos[:, 2])

    def base(dim=1.0):
        im = Image.new('RGB', (W, H), (18, 18, 20))
        dr = ImageDraw.Draw(im)
        for i in order:
            x, y = px(pos[i, 0], pos[i, 1])
            c = tuple(int(v * 255 * dim) for v in alb[i])
            dr.rectangle([x - cellpx / 2, y - cellpx / 2, x + cellpx / 2, y + cellpx / 2], fill=c)
        return im, dr

    font = ImageFont.load_default()
    panels = []
    # 1. what the rays hit, colored by the surface's albedo
    im, dr = base()
    panels.append((im, 'Surfels: %d, top view, albedo' % len(S)))
    # 2. sky visibility per probe
    im, dr = base(0.35)
    sky = P['sky'].mean(1)
    r = max(3, int(cellpx * 0.35))
    for p, s in zip(P, sky):
        x, y = px(p['pos'][0], p['pos'][1])
        c = (int(40 + 200 * s), int(60 + 180 * s), int(90 + 165 * s)) if s > 0 else (200, 40, 40)
        dr.ellipse([x - r, y - r, x + r, y + r], fill=c, outline=(0, 0, 0))
    panels.append((im, 'Probes: %d, sky seen (red = none, white = open sky)' % len(P)))
    # 3. bounce tint: each probe's link-weighted surfel albedo (bigger dots, darker ground)
    im, dr = base(0.15)
    rb = int(r * 1.6)
    for p, (ids, w) in zip(P, links):
        ok = ids >= 0
        if not ok.any() or w[ok].sum() <= 0:
            continue
        a = (S['alb'][ids[ok]].astype(np.float64) / 255.0 * w[ok, None]).sum(0) / w[ok].sum()
        c = tuple(int(v * 255) for v in srgb(a))
        x, y = px(p['pos'][0], p['pos'][1])
        dr.ellipse([x - rb, y - rb, x + rb, y + rb], fill=c, outline=(0, 0, 0))
    panels.append((im, 'Bounce color each probe gathers (links x albedo)'))
    # 4. one probe's links: the probe with the most links under a roof (least sky), else the most links
    covered = [i for i in range(len(P)) if sky[i] < 0.05 and len(links[i][0])]
    pick = max(covered or range(len(P)), key=lambda i: len(links[i][0]))
    im, dr = base(0.3)
    ids, w = links[pick]
    wm = w.max() if len(w) else 1
    pp = px(P['pos'][pick][0], P['pos'][pick][1])
    for j in np.argsort(w):
        if ids[j] < 0:
            continue
        x, y = px(pos[ids[j], 0], pos[ids[j], 1])
        k = w[j] / wm
        dr.line([pp, (x, y)], fill=(int(80 + 175 * k), int(160 + 60 * k), 255), width=1)
        c = tuple(int(v * 255) for v in alb[ids[j]])
        dr.rectangle([x - cellpx / 2, y - cellpx / 2, x + cellpx / 2, y + cellpx / 2], fill=c)
    dr.ellipse([pp[0] - r - 2, pp[1] - r - 2, pp[0] + r + 2, pp[1] + r + 2], fill=(255, 220, 40), outline=(0, 0, 0))
    panels.append((im, 'One probe (yellow): its %d links, sky %.2f, unlinked %.2f' % (
        len(ids), sky[pick], float(P['unl'][pick]))))

    cols = 2
    rows = (len(panels) + 1) // 2
    out_im = Image.new('RGB', (cols * (W + 10) + 10, rows * (H + 30) + 40), (10, 10, 12))
    od = ImageDraw.Draw(out_im)
    od.text((10, 10), title, fill=(230, 230, 230), font=font)
    for n, (im, cap) in enumerate(panels):
        x0 = 10 + (n % cols) * (W + 10)
        y0 = 40 + (n // cols) * (H + 30)
        out_im.paste(im, (x0, y0))
        od.text((x0, y0 + H + 6), cap, fill=(220, 220, 220), font=font)
    out_im.save(out)
    print('%s: %d surfels, %d probes, %dx%d' % (out, len(S), len(P), out_im.size[0], out_im.size[1]))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else os.path.basename(sys.argv[1]),
         float(sys.argv[4]) if len(sys.argv) > 4 else None)
