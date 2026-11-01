"""lane FARLOOK1101 (FARSHIMMER2 = FARSPLAT1 + FARTHIN1): the blended screen-space splat, modelled the way the
shader draws it (res/shaders/fo4_default.vert, farSplat).

Per far row and frame the vertex stage measures the row's projected box (bw x bh px, the same box census.py
projects) and widens the row about its projected centre by kx = max(1, S / bw) across and ky = max(1, S / bh) up;
the fragment stage draws it at alpha 1 / (kx * ky). The mesh's own outline (here: census.py's rectangle w x h of the
hull's area and the box's aspect) grows to (w kx) x (h ky), and with the TAA off a frame shows

    weight = alpha * hits(widened outline) / 4 px^2

The expectation over sub-pixel phases of hits / 4 is the widened area, so the expected weight is w * h: ink-true by
construction, whatever S. S only decides blinking (no sample hit) and the frame-to-frame spread of the weight.
"""
import numpy as np
import farshimmer_raster as R


def splat_footprint(f, idx, S, alpha=True):
    bw = np.maximum(f['bw'][idx], 1e-6)
    bh = np.maximum(f['bh'][idx], 1e-6)
    ar = np.minimum(f['area'][idx], bw * bh)
    w = np.sqrt(ar * bw / bh)
    h = np.sqrt(ar * bh / bw)
    kx = np.maximum(1.0, S / bw) if S > 0 else np.ones_like(bw)
    ky = np.maximum(1.0, S / bh) if S > 0 else np.ones_like(bh)
    w2, h2 = w * kx, h * ky
    cx, cy = f['cx'][idx], f['cy'][idx]
    return cx - w2 / 2, cx + w2 / 2, cy - h2 / 2, cy + h2 / 2, w, h, (1.0 / (kx * ky)) if alpha else np.ones_like(kx)


def splat_census(frames, idx, sizes, alpha=True):
    """per S (index 0 = S 0 = today's opaque rule): blinks per row, frames off, mean weight, the per-row spread of the
    weight over the frames (std / mean), switches per frame"""
    out = dict(sizes=[0.0] + [float(s) for s in sizes], blinks=[], offs=[], weight=[], spread=[], switch=[],
               w=None, h=None, area=None)
    for S in out['sizes']:
        wts, ws_, hh_ = [], [], []
        for f in frames:
            x0, x1, y0, y1, w, h, al = splat_footprint(f, idx, S, alpha)
            wts.append(al * R.hits(x0, x1, y0, y1) / 4.0)
            ws_.append(w); hh_.append(h)
        wts = np.array(wts)
        on = wts > 0
        sw = on[1:] != on[:-1]
        out['blinks'].append(sw.sum(0))
        out['offs'].append((~on).sum(0))
        out['switch'].append(sw.sum(1))
        m = wts.mean(0)
        out['weight'].append(m)
        out['spread'].append(wts.std(0) / np.maximum(m, 1e-12))
        if out['w'] is None:
            out['w'] = np.mean(ws_, 0); out['h'] = np.mean(hh_, 0)
            out['area'] = np.mean(np.array(ws_) * np.array(hh_), 0)
    return out


def splat_report(res, bar_ink=0.10):
    """lines + the pick: the smallest S with 0 blinkers, total ink within 1 +- bar_ink over the sub-pixel rows and the
    lowest median spread among those that pass (ties: the smaller S, the less widened)"""
    w, h, area = res['w'], res['h'], res['area']
    sub = np.minimum(w, h) < 1.0
    L, rows = [], []
    for k, S in enumerate(res['sizes']):
        b = res['blinks'][k]; wt = res['weight'][k]; sp = res['spread'][k]; sw = res['switch'][k]
        ink = float(wt[sub].sum() / max(area[sub].sum(), 1e-9)) if sub.any() else 1.0
        g = wt[sub] / np.maximum(area[sub], 1e-9)
        med_sp = float(np.median(sp[sub])) if sub.any() else 0.0
        p90_sp = float(np.percentile(sp[sub], 90)) if sub.any() else 0.0
        name = "today's rule (opaque, 4x, TAA off)" if k == 0 else 'splat S %.2f px' % S
        L.append('%-36s rows that blink %5d; switches a frame mean %.1f max %d; ink over sub-pixel rows %.3f '
                 '(row gain median %.2f p10 %.2f p90 %.2f); weight spread over frames median %.2f p90 %.2f'
                 % (name, int((b > 0).sum()), float(sw.mean()) if len(sw) else 0.0, int(sw.max()) if len(sw) else 0,
                    ink, float(np.median(g)) if sub.any() else 1.0, float(np.percentile(g, 10)) if sub.any() else 1.0,
                    float(np.percentile(g, 90)) if sub.any() else 1.0, med_sp, p90_sp))
        rows.append((S, int((b > 0).sum()), ink, med_sp, p90_sp))
    ok = [r for r in rows[1:] if r[1] == 0 and abs(r[2] - 1.0) <= bar_ink]
    pick = min(ok, key=lambda r: (round(r[3], 2), r[0])) if ok else None
    return L, rows, pick
