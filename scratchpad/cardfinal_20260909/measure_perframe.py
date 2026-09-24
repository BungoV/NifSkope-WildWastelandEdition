#!/usr/bin/env python3
"""Fill, spacing, cross-frame bleed and the PER-FRAME gain on a card library.

Adapted from `scratchpad/cardpad_20260909/measure_gap.py` (lane CARDPAD), which
this repeats so the two reports compare, plus what lane CARDFINAL added:

  * THE MIP LAW IS AN ARGUMENT, not a guess.  Sheets baked before 2026-09-09
    evening carry `mips = 1 + log2(gap)` and sheets baked after carry
    `mips = log2(gap)`, and nothing in the sidecar distinguishes them -- so the
    caller states which library is under which law, and the column headings say
    so.  Passing `log2` for an OLD library is the isolation that shows the mip
    change ALONE removing the bleed, with no other difference.
  * FILL is reported two ways: against the UNION of the N^2 silhouette boxes
    (what a fixed-centre frame had to hold, and lane CARDPAD's own number) and
    against the WIDEST SINGLE view (what a per-frame-positioned frame holds).
    On a per-frame library the two are the same thing, because every frame is
    centred; the difference on an older library IS the waste this law removes.
  * THE `framefit` LINE, when the bake wrote one: the widest single view's half
    box beside the union half-extent, in units, straight from the bake.

Everything else -- the coverage floor of 16/255, the 2x2 box filter rounded
half-up, the gap metric across the two texels a border tap reads, the clear-run
in texels, the neighbour-alpha metric -- is CARDPAD's, unchanged, so the columns
mean what its report says they mean.

  measure_perframe.py <dir>:<law> [<dir>:<law> ...]     law = log2 | 1+log2
"""
import sys, os, glob, json
import numpy as np
from PIL import Image


def sidecar(p):
    d = {}
    off = {}
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
        elif t[0] == 'framefit' and len(t) >= 5:
            d.update(fit=[float(v) for v in t[1:5]])
        elif t[0] == 'frameclamped' and len(t) >= 2:
            d.update(clamped=int(t[1]))
        elif t[0] == 'frameoff' and len(t) >= 5:
            off[(int(t[1]), int(t[2]))] = (float(t[3]), float(t[4]))
    d['frameoff'] = off
    if d.get('law') == 'gap':
        d['padX'], d['padY'] = d['gapX'] // 2, d['gapY'] // 2
    elif d.get('law') == 'pad':
        d['gapX'], d['gapY'] = 2 * d['padX'], 2 * d['padY']
    # THE MIP UNIT IS THE GAP under every vintage; the LAW decides what to do with it
    if 'gapX' in d:
        d['mipUnit'] = min(d['gapX'], d['gapY'])
    return d


def boxdown(a):
    h, w = a.shape[:2]
    h2, w2 = h // 2, w // 2
    b = a[:h2 * 2, :w2 * 2].astype(np.uint32)
    acc = b[0::2, 0::2] + b[0::2, 1::2] + b[1::2, 0::2] + b[1::2, 1::2]
    return ((acc + 2) >> 2).astype(np.uint8)


def mips_of(unit, law):
    m, g = (1 if law == '1+log2' else 0), unit
    while g >= 2:
        g //= 2
        m += 1
    return max(1, m)


def measure(d, law):
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
        M = mips_of(sc['mipUnit'], law)
        a = np.array(Image.open(png).convert('RGBA'))
        assert a.shape[1] == N * fw and a.shape[0] == N * fh, (ident, a.shape, N, fw, fh)

        # --- fill, two ways: the UNION of the boxes, and the WIDEST SINGLE view
        cov = a[:, :, 3] >= 16          # the spec's coverage floor, CARDPAD's threshold
        ux0, ux1, uy0, uy1 = fw, 0, fh, 0
        sw, sh = 0, 0                    # the widest single view's box
        centres = []
        for j in range(N):
            for i in range(N):
                c = cov[j * fh:(j + 1) * fh, i * fw:(i + 1) * fw]
                ys, xs = np.nonzero(c)
                if xs.size == 0:
                    continue
                x0, x1 = int(xs.min()), int(xs.max()) + 1
                y0, y1 = int(ys.min()), int(ys.max()) + 1
                ux0 = min(ux0, x0); ux1 = max(ux1, x1)
                uy0 = min(uy0, y0); uy1 = max(uy1, y1)
                sw = max(sw, x1 - x0); sh = max(sh, y1 - y0)
                centres.append((0.5 * (x0 + x1) - 0.5 * fw, 0.5 * (y0 + y1) - 0.5 * fh))
        uw, uh = max(0, ux1 - ux0), max(0, uy1 - uy0)
        iw, ih = fw - 2 * padX, fh - 2 * padY
        # how far the frames' own silhouettes sit off their frame centres: ~0 on a
        # per-frame library, and the waste itself on a fixed-centre one
        offw = max((abs(c[0]) for c in centres), default=0.0)
        offh = max((abs(c[1]) for c in centres), default=0.0)

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
            mingap, maxbleed, nborder = 2.0, 0, 0
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
            per_mip.append(dict(mip=k, frame=[fwk, fhk], min_gap=round(mingap, 4),
                                max_alpha_bleed=maxbleed,
                                min_clear_run=(min(runs) if runs else None),
                                run_samples=len(runs), samples=nborder))
        rows.append(dict(id=ident, frame=[fw, fh], base=sc.get('base'),
                         gap=[gapX, gapY], pad=[padX, padY], inner=[iw, ih], mips=M,
                         union=[uw, uh], single=[sw, sh],
                         fill_frame=[round(uw / fw, 4), round(uh / fh, 4)],
                         fill_single=[round(sw / fw, 4), round(sh / fh, 4)],
                         fill_inner=[round(uw / iw, 4), round(uh / ih, 4)],
                         inner_frac=[round(iw / fw, 4), round(ih / fh, 4)],
                         centre_off=[round(offw, 2), round(offh, 2)],
                         fit=sc.get('fit'), clamped=sc.get('clamped'),
                         nframeoff=len(sc.get('frameoff') or {}),
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
    print('  fill x / frame, UNION of the views     min %.3f  median %.3f  max %.3f' % col(lambda r: r['fill_frame'][0]))
    print('  fill y / frame, UNION of the views     min %.3f  median %.3f  max %.3f' % col(lambda r: r['fill_frame'][1]))
    print('  fill x / frame, WIDEST SINGLE view     min %.3f  median %.3f  max %.3f' % col(lambda r: r['fill_single'][0]))
    print('  fill y / frame, WIDEST SINGLE view     min %.3f  median %.3f  max %.3f' % col(lambda r: r['fill_single'][1]))
    print('  fill x / inner rect                    min %.3f  median %.3f  max %.3f' % col(lambda r: r['fill_inner'][0]))
    print('  fill y / inner rect                    min %.3f  median %.3f  max %.3f' % col(lambda r: r['fill_inner'][1]))
    print('  a frame\'s silhouette off its own centre, worst texels: x %.1f / %.1f / %.1f'
          % col(lambda r: r['centre_off'][0]))
    print('                                                          y %.1f / %.1f / %.1f'
          % col(lambda r: r['centre_off'][1]))
    print('  shipped mips                           min %d  median %.1f  max %d' % col(lambda r: r['mips']))
    print('  clear texels between two silhouettes at MIP 0:  min %d  median %.1f  max %d'
          % col(lambda r: r['clear_run_mip0'] if r['clear_run_mip0'] is not None else -1))
    print('  clear texels at the LAST shipped mip:           min %d  median %.1f  max %d'
          % col(lambda r: r['clear_run_last'] if r['clear_run_last'] is not None else -1))
    print('  narrowest gap at any shipped mip: min %.3f  median %.3f  max %.3f texels' % col(lambda r: r['min_gap']))
    print('  worst neighbour alpha at a border: min %d  median %d  max %d / 255'
          % tuple(int(v) for v in col(lambda r: r['max_alpha_bleed'])))
    print('  sheets under one texel of gap: %d of %d' % (sum(1 for r in rows if r['min_gap'] < 0.999), len(rows)))
    print('  SHEETS WITH ANY NEIGHBOUR ALPHA AT A BORDER: %d of %d'
          % (sum(1 for r in rows if r['max_alpha_bleed'] > 0), len(rows)))
    print('  total sheet area: %d texels; frame shapes: %d'
          % (sum(r['sheet_area'] for r in rows), len({tuple(r['frame']) for r in rows})))
    fits = [r for r in rows if r.get('fit')]
    if fits:
        gx = [1.0 - r['fit'][0] / r['fit'][2] for r in fits]
        gy = [1.0 - r['fit'][1] / r['fit'][3] for r in fits]
        print('  PER-FRAME GAIN from the bake\'s own framefit line, %d bases:' % len(fits))
        print('    the frame is narrower than a fixed centre would need: x %.1f%% / %.1f%% / %.1f%%'
              % (100 * min(gx), 100 * st.median(gx), 100 * max(gx)))
        print('                                                          y %.1f%% / %.1f%% / %.1f%%'
              % (100 * min(gy), 100 * st.median(gy), 100 * max(gy)))
        print('    frames whose crop had to be clamped: %d over the library'
              % sum(r.get('clamped') or 0 for r in rows))
        print('    frameoff lines per set: min %d max %d'
              % (min(r['nframeoff'] for r in rows), max(r['nframeoff'] for r in rows)))


def main():
    out = {}
    for spec in sys.argv[1:]:
        d, law = spec.rsplit(':', 1)
        assert law in ('log2', '1+log2'), law
        rows = measure(d, law)
        name = os.path.basename(d.rstrip('/\\')) + '  [mips = %s(gap)]' % law
        summarise(rows, name)
        print()
        out[name] = rows
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'measure_perframe.json'), 'w') as f:
        json.dump(out, f, indent=1)
    print('json: measure_perframe.json')


main()
