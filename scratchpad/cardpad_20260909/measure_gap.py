#!/usr/bin/env python3
"""Fill, spacing and cross-frame bleed on a baked card library.

Reads a card directory (`<id>_oct_albedo.png` + `<id>.txt` sidecar) and reports,
per base and over the library:

  * the frame, the spacing (gap and per-side margin) and the shipped mip count,
    each read from the sidecar's own line so the numbers are the sheet's, not
    this script's;
  * FILL -- the union of the N^2 views' silhouette bounding boxes, against the
    frame and against the inner rect;
  * THE GAP METRIC -- at every shipped mip, across every INTERIOR frame border,
    the separation between the two silhouettes that meet on it, measured as
    transparent coverage over exactly the two texels a border tap reads:
        gap = (255-alphaA)/255 + (255-alphaB)/255      [texels]
    reported as the MINIMUM over borders and rows.  The outer border of the
    sheet is excluded: it has no neighbour and is sampled clamped.
  * THE ALPHA-BLEED METRIC used by lane CARDFIT3 -- the neighbour's alpha
    contribution to a bilinear tap taken exactly on a border, 0.5*alpha, as a
    max over borders per shipped mip.  Kept so the two laws are comparable on
    one number.

Mips are rebuilt with lodgen's own filter (2x2 box, rounded half-up).
"""
import sys, os, glob, json
import numpy as np
from PIL import Image


def sidecar(p):
    d = {}
    for ln in open(p, encoding='utf-8', errors='replace'):
        t = ln.split()
        if not t:
            continue
        if t[0] == 'oct' and len(t) >= 12:
            d.update(oct=int(t[1]), fw=int(t[2]), fh=int(t[3]), base=int(t[11]))
        elif t[0] == 'gap' and len(t) >= 3:
            d.update(gapX=int(t[1]), gapY=int(t[2]), law='gap')
        elif t[0] == 'pad' and len(t) >= 3:
            d.update(padX=int(t[1]), padY=int(t[2]), law='pad')
    if d.get('law') == 'gap':
        d['padX'], d['padY'] = d['gapX'] // 2, d['gapY'] // 2
        d['mipUnit'] = min(d['gapX'], d['gapY'])
    elif d.get('law') == 'pad':
        d['gapX'], d['gapY'] = 2 * d['padX'], 2 * d['padY']
        d['mipUnit'] = min(d['padX'], d['padY'])
    return d


def boxdown(a):
    h, w = a.shape[:2]
    h2, w2 = h // 2, w // 2
    b = a[:h2 * 2, :w2 * 2].astype(np.uint32)
    acc = b[0::2, 0::2] + b[0::2, 1::2] + b[1::2, 0::2] + b[1::2, 1::2]
    return ((acc + 2) >> 2).astype(np.uint8)


def mips_of(unit):
    m, g = 1, unit
    while g >= 2:
        g //= 2
        m += 1
    return m


def measure(d):
    rows = []
    for meta in sorted(glob.glob(os.path.join(d, '*.txt'))):
        ident = os.path.splitext(os.path.basename(meta))[0]
        if ident == 'library':
            continue
        png = os.path.join(d, ident + '_oct_albedo.png')
        if not os.path.exists(png):
            continue
        sc = sidecar(meta)
        if 'oct' not in sc or 'law' not in sc:
            continue
        N, fw, fh = sc['oct'], sc['fw'], sc['fh']
        padX, padY, gapX, gapY = sc['padX'], sc['padY'], sc['gapX'], sc['gapY']
        M = mips_of(sc['mipUnit'])
        a = np.array(Image.open(png).convert('RGBA'))
        assert a.shape[1] == N * fw and a.shape[0] == N * fh, (ident, a.shape, N, fw, fh)

        # --- fill: the union of the N^2 silhouette boxes ---------------------
        # the coverage floor of docs/LODGEN_IMPOSTOR_SPEC.md, 16/255, which is what
        # lane CARDFIT3's own measurement used -- so the two reports compare
        cov = a[:, :, 3] >= 16
        ux0, ux1, uy0, uy1 = fw, 0, fh, 0
        for j in range(N):
            for i in range(N):
                c = cov[j * fh:(j + 1) * fh, i * fw:(i + 1) * fw]
                ys, xs = np.nonzero(c)
                if xs.size == 0:
                    continue
                ux0 = min(ux0, int(xs.min())); ux1 = max(ux1, int(xs.max()) + 1)
                uy0 = min(uy0, int(ys.min())); uy1 = max(uy1, int(ys.max()) + 1)
        uw, uh = max(0, ux1 - ux0), max(0, uy1 - uy0)
        iw, ih = fw - 2 * padX, fh - 2 * padY

        # --- the gap and the alpha bleed, per shipped mip --------------------
        per_mip = []
        cur = a
        for k in range(M):
            if k:
                cur = boxdown(cur)
            fwk, fhk = fw >> k, fh >> k
            if fwk < 2 or fhk < 2:
                continue
            al = cur[:, :, 3].astype(np.float64)
            H, W = al.shape
            mingap = 2.0
            maxbleed = 0
            nborder = 0
            for i in range(1, N):
                x = i * fwk
                if x < 1 or x >= W:
                    continue
                left, right = al[:, x - 1], al[:, x]
                g = (255.0 - left) / 255.0 + (255.0 - right) / 255.0
                mingap = min(mingap, float(g.min()))
                maxbleed = max(maxbleed, int(left.max()) // 2, int(right.max()) // 2)
                nborder += g.size
            for j in range(1, N):
                y = j * fhk
                if y < 1 or y >= H:
                    continue
                top, bot = al[y - 1, :], al[y, :]
                g = (255.0 - top) / 255.0 + (255.0 - bot) / 255.0
                mingap = min(mingap, float(g.min()))
                maxbleed = max(maxbleed, int(top.max()) // 2, int(bot.max()) // 2)
                nborder += g.size
            # THE PHYSICAL GAP: the number of wholly clear texel columns (rows)
            # actually left between the two silhouettes that meet on a border,
            # taken at the row (column) where they come closest.  This is the
            # quantity bungo's number names -- "pixels of distance between two
            # rendered objects" -- and it is measured on the sheet, not derived.
            covk = cur[:, :, 3] >= 16
            runs = []
            for i in range(1, N):
                x = i * fwk
                for y in range(H):
                    lo = covk[y, x - fwk:x]
                    hi = covk[y, x:x + fwk]
                    if not lo.any() or not hi.any():
                        continue
                    runs.append(int(np.nonzero(hi)[0][0]) + int(fwk - 1 - np.nonzero(lo)[0][-1]))
            for j in range(1, N):
                y = j * fhk
                for x in range(W):
                    lo = covk[y - fhk:y, x]
                    hi = covk[y:y + fhk, x]
                    if not lo.any() or not hi.any():
                        continue
                    runs.append(int(np.nonzero(hi)[0][0]) + int(fhk - 1 - np.nonzero(lo)[0][-1]))
            per_mip.append(dict(mip=k, frame=[fwk, fhk], gap_texels=gapX / (1 << k),
                                min_gap=round(mingap, 4), max_alpha_bleed=maxbleed,
                                min_clear_run=(min(runs) if runs else None),
                                run_samples=len(runs), samples=nborder))
        rows.append(dict(id=ident, law=sc['law'], frame=[fw, fh], base=sc.get('base'),
                         gap=[gapX, gapY], pad=[padX, padY], inner=[iw, ih], mips=M,
                         union=[uw, uh],
                         fill_frame=[round(uw / fw, 4), round(uh / fh, 4)],
                         fill_inner=[round(uw / iw, 4), round(uh / ih, 4)],
                         inner_frac=[round(iw / fw, 4), round(ih / fh, 4)],
                         sheet_area=N * fw * N * fh,
                         per_mip=per_mip,
                         min_gap=round(min(m['min_gap'] for m in per_mip), 4),
                         clear_run_mip0=per_mip[0]['min_clear_run'],
                         clear_run_last=per_mip[-1]['min_clear_run'],
                         max_alpha_bleed=max(m['max_alpha_bleed'] for m in per_mip)))
    return rows


def summarise(rows, name):
    import statistics as st
    def col(f):
        v = sorted(f(r) for r in rows)
        return v[0], st.median(v), v[-1]
    print('=== %s : %d bases ===' % (name, len(rows)))
    print('  fill x / frame     min %.3f  median %.3f  max %.3f' % col(lambda r: r['fill_frame'][0]))
    print('  fill y / frame     min %.3f  median %.3f  max %.3f' % col(lambda r: r['fill_frame'][1]))
    print('  fill x / inner     min %.3f  median %.3f  max %.3f' % col(lambda r: r['fill_inner'][0]))
    print('  fill y / inner     min %.3f  median %.3f  max %.3f' % col(lambda r: r['fill_inner'][1]))
    print('  inner / frame  x   min %.4f  median %.4f  max %.4f' % col(lambda r: r['inner_frac'][0]))
    print('  inner / frame  y   min %.4f  median %.4f  max %.4f' % col(lambda r: r['inner_frac'][1]))
    print('  clear texels between two silhouettes at MIP 0:  min %d  median %.1f  max %d'
          % col(lambda r: r['clear_run_mip0'] if r['clear_run_mip0'] is not None else -1))
    print('  clear texels between two silhouettes at the LAST shipped mip: min %d  median %.1f  max %d'
          % col(lambda r: r['clear_run_last'] if r['clear_run_last'] is not None else -1))
    print('  narrowest gap at any shipped mip: min %.3f  median %.3f  max %.3f texels' % col(lambda r: r['min_gap']))
    print('  worst neighbour alpha at a border: min %d  median %d  max %d / 255'
          % tuple(int(v) for v in col(lambda r: r['max_alpha_bleed'])))
    print('  sheets under one texel of gap: %d of %d' % (sum(1 for r in rows if r['min_gap'] < 0.999), len(rows)))
    print('  sheets with any neighbour alpha at a border: %d of %d'
          % (sum(1 for r in rows if r['max_alpha_bleed'] > 0), len(rows)))
    print('  total sheet area: %d texels; frame shapes: %d'
          % (sum(r['sheet_area'] for r in rows), len({tuple(r['frame']) for r in rows})))


if __name__ == '__main__':
    out = {}
    for arg in sys.argv[1:]:
        name, d = arg.split('=', 1)
        rows = measure(d)
        out[name] = rows
        summarise(rows, name)
        json.dump(rows, open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                          'gap_%s.json' % name), 'w'), indent=1)
