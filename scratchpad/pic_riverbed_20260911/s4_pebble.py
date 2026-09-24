#!/usr/bin/env python
"""PIC-RIVERBED step 4: how big is a pebble in the window's dominant texture?

Two independent instruments, each shown working on a KNOWN ANSWER first
(CONSTITUTION 4: an invariant that fails on broken input, and it is shown
failing / hitting the planted value):

  A. autocorrelation half-width of the mean-removed luminance -> the
     characteristic blob RADIUS, doubled for a diameter.
  B. connected-component blobs of the bright grey spots -> the median
     equivalent diameter 2*sqrt(area/pi).

Known answer for both: a synthetic field of discs of radius 30 texels.

Then the conversion the caption needs: one texture texel is repeat/2048 world
units, and one far-sheet texel is 32 world units.
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'scratchpad', 'splat1_20260911'))
sys.path.insert(0, os.path.join(REPO, 'tests', 'spells'))

import splatlib as S                                          # noqa: E402

UPT = 32.0          # world units per far-sheet texel, dim 4 at 512
ENGINE = 341.3333   # world units per repeat, SPLAT1 section 2c
BAKED = 2048.0      # world units per repeat, src/lodgen.cpp TILE


def window_composition():
    """The window's LTEX layers grouped by the DIFFUSE they resolve to, so
    'the dominant texture' is a texture and not an EDID -- two LTEXs here share
    one TXST and one .dds, and separately neither of them is the largest."""
    w = json.load(open(os.path.join(HERE, 'window.json')))
    by, bx = w['y'], w['x']
    z = np.load(os.path.join(HERE, 'riverbed.npz'))
    chain = {r['edid']: r for r in json.load(open(os.path.join(HERE, 'chain.json')))}
    per = {}
    for k in z.files:
        if not k.startswith('w_'):
            continue
        nm = k[2:]
        v = float(z[k][by:by + 128, bx:bx + 128].mean())
        if v <= 0.001:
            continue
        pth = (chain.get(nm) or {}).get('path')
        e = per.setdefault(pth, dict(share=0.0, edids=[], rel=(chain.get(nm) or {}).get('rel')))
        e['share'] += v
        e['edids'].append((nm, v))
    rows = sorted(per.items(), key=lambda kv: -kv[1]['share'])
    return w, rows


TEX = window_composition()[1][0][0]


def autocorr_halfwidth(a):
    """Radius in texels at which the normalised radial autocorrelation of the
    mean-removed field first falls to 0.5."""
    f = a - a.mean()
    F = np.fft.rfft2(f)
    ac = np.fft.irfft2(F * np.conj(F), s=f.shape).real
    ac = np.fft.fftshift(ac)
    ac = ac / ac.max()
    h, w = ac.shape
    cy, cx = h // 2, w // 2
    yy, xx = np.mgrid[0:h, 0:w]
    r = np.sqrt((yy - cy) ** 2.0 + (xx - cx) ** 2.0)
    rr = r.astype(np.int64)
    n = np.bincount(rr.ravel(), minlength=int(r.max()) + 2)
    s = np.bincount(rr.ravel(), weights=ac.ravel(), minlength=n.size)
    prof = s / np.maximum(n, 1)
    for i in range(1, prof.size):
        if prof[i] < 0.5:
            # linear interpolation between i-1 and i
            p0, p1 = prof[i - 1], prof[i]
            return (i - 1) + (p0 - 0.5) / max(p0 - p1, 1e-9), prof
    return float(prof.size), prof


def blobs(mask, maxiter=400):
    """Connected components by iterative label propagation (no scipy).
    Returns the list of component areas in texels."""
    h, w = mask.shape
    lab = np.where(mask, np.arange(h * w, dtype=np.int64).reshape(h, w), -1)
    for _ in range(maxiter):
        prev = lab
        m = lab.copy()
        m[1:, :] = np.maximum(m[1:, :], np.where(mask[1:, :] & mask[:-1, :], lab[:-1, :], -1))
        m[:-1, :] = np.maximum(m[:-1, :], np.where(mask[:-1, :] & mask[1:, :], lab[1:, :], -1))
        m[:, 1:] = np.maximum(m[:, 1:], np.where(mask[:, 1:] & mask[:, :-1], lab[:, :-1], -1))
        m[:, :-1] = np.maximum(m[:, :-1], np.where(mask[:, :-1] & mask[:, 1:], lab[:, 1:], -1))
        m = np.where(mask, m, -1)
        if np.array_equal(m, prev):
            break
        lab = m
    v = lab[mask]
    if v.size == 0:
        return np.array([])
    u, c = np.unique(v, return_counts=True)
    return c


def eqdiam(areas, minarea=20):
    a = areas[areas >= minarea]
    if a.size == 0:
        return 0.0, 0.0, 0
    d = 2.0 * np.sqrt(a / np.pi)
    return float(np.median(d)), float(d.mean()), int(a.size)


def disc_field(n=512, radius=30.0, seed=7, count=40):
    """Known answer: bright discs of a known radius on a dark ground."""
    rng = np.random.RandomState(seed)
    a = np.full((n, n), 40.0)
    yy, xx = np.mgrid[0:n, 0:n]
    for _ in range(count):
        cy, cx = rng.randint(0, n, 2)
        a[((yy - cy) ** 2 + (xx - cx) ** 2) <= radius ** 2] = 180.0
    return a


def main():
    log = []

    def p(s):
        print(s)
        log.append(s)

    # ---- the controls, first ----
    p('CONTROLS: discs of a KNOWN diameter, to calibrate instrument A and to')
    p('show it tracking the planted size over a 3x range.')
    cal = []
    for rad in (15.0, 30.0, 45.0):
        ctl = disc_field(radius=rad, count=int(40 * (30.0 / rad) ** 2))
        hw, _ = autocorr_halfwidth(ctl)
        cal.append(2.0 * rad / hw)
        p('  planted diameter %3.0f  ->  half-width %5.1f  ->  factor %.2f'
          % (2 * rad, hw, 2 * rad / hw))
    K = float(np.mean(cal))
    p('  CALIBRATION  diameter = %.2f x half-width   (spread %.2f .. %.2f)'
      % (K, min(cal), max(cal)))
    assert max(cal) / min(cal) < 1.35, 'the half-width factor is not stable over the range'
    ctl = disc_field()
    mk = ctl > (ctl.mean() + ctl.std())
    med, mean, nb = eqdiam(blobs(mk))
    p('  B blob equivalent diameter (r=30) median %.1f  mean %.1f  over %d blobs'
      % (med, mean, nb))
    p('    (B over-reads because overlapping discs merge into one blob; it is')
    p('     used only for the SPOTS on the far sheets, which do not overlap.)')
    # a floor: a field with NO structure must report a half-width near 1 texel
    rng = np.random.RandomState(3)
    noise = rng.normal(90.0, 20.0, (512, 512))
    hwn, _ = autocorr_halfwidth(noise)
    p('  FLOOR white noise, no structure   half-width %.2f texels (must be < 1.5)' % hwn)
    assert hwn < 1.5, 'the autocorrelation instrument does not fall on structureless input'
    mkn = noise > (noise.mean() + noise.std())
    medn, meann, nbn = eqdiam(blobs(mkn), minarea=20)
    p('  FLOOR white noise blob diameter   median %.1f over %d blobs (must be ~0)' % (medn, nbn))
    p('')

    # ---- the texture ----
    d = S.Dds(TEX)
    img = d.level(0)[:, :, :3]
    p('TEXTURE %s' % TEX)
    p('  %dx%d, %d mips, %d bytes, fourCC %s, mean RGB %.1f,%.1f,%.1f'
      % (d.width, d.height, d.maxMip + 1, os.path.getsize(TEX), d.fourcc.decode(),
         img[:, :, 0].mean(), img[:, :, 1].mean(), img[:, :, 2].mean()))
    L = S.lum(img)
    p('  luminance mean %.1f  sd %.1f  min %.0f  max %.0f'
      % (L.mean(), L.std(), L.min(), L.max()))

    # crop for the blob pass (2048^2 labelling is not needed to size a pebble)
    crop = L[768:1280, 768:1280]
    hw_t, prof = autocorr_halfwidth(crop)
    thr = crop.mean() + crop.std()
    mask = crop > thr
    areas = blobs(mask)
    med_t, mean_t, nb_t = eqdiam(areas)
    p('')
    p('  A autocorrelation half-width      %.1f texture texels' % hw_t)
    p('  A CALIBRATED PEBBLE DIAMETER      %.1f texture texels  (%.2f x %.1f)'
      % (K * hw_t, K, hw_t))
    p('  B bright spots: threshold lum > mean+1sd = %.1f, %.1f%% of the crop'
      % (thr, 100.0 * mask.mean()))
    p('    blob equivalent diameter        median %.1f  mean %.1f  over %d blobs >= 20 texels'
      % (med_t, mean_t, nb_t))
    p('    p25 / p75 of the diameter       %.1f / %.1f'
      % tuple(np.percentile(2.0 * np.sqrt(areas[areas >= 20] / np.pi), [25, 75])))

    # the PALE GREY rocks specifically, whole texture, the same definition the
    # far sheets are measured with below
    sat_t = img.max(2) - img.min(2)
    mg = (L > L.mean() + 2.0 * L.std()) & (sat_t < sat_t.mean())
    ag = blobs(mg, maxiter=600)
    ag = ag[ag >= 12]
    dg = 2.0 * np.sqrt(ag / np.pi)
    p('')
    p('  PALE GREY rocks (lum > mean+2sd AND saturation < mean), whole 2048x2048:')
    p('    %.2f%% of the texture, %d rocks >= 12 texels' % (100.0 * mg.mean(), ag.size))
    p('    diameter  median %.1f  mean %.1f  p90 %.1f  max %.1f texture texels'
      % (float(np.median(dg)), float(dg.mean()), float(np.percentile(dg, 90)), float(dg.max())))
    grey_med, grey_p90, grey_max = float(np.median(dg)), float(np.percentile(dg, 90)), float(dg.max())

    # ---- the conversion the caption needs ----
    p('')
    p('SIZE ON THE GROUND (one texture texel = repeat / %d world units)' % d.width)
    for name, rep in (('engine  341.333', ENGINE), ('baked   2048.000', BAKED)):
        upx = rep / d.width
        p('  %s  1 texture texel = %.4f world units;  one repeat = %.2f far-sheet texels'
          % (name, upx, rep / UPT))
        p('      A PEBBLE (%.1f texture texels) = %.1f world units = %.2f far-sheet texels'
          % (K * hw_t, K * hw_t * upx, K * hw_t * upx / UPT))
        p('      the biggest pale rock (%.1f texels) = %.1f world units = %.2f far-sheet texels'
          % (grey_max, grey_max * upx, grey_max * upx / UPT))
    p('')
    p('  ratio baked/engine = %.4f' % (BAKED / ENGINE))

    # ---- and the grey spots as they appear on the FAR SHEETS themselves ----
    # "grey spot" = brighter than the riverbed region's mean by 1 sd AND less
    # saturated than that region's mean. Same definition on both sheets.
    REPO2 = REPO
    o_sheet = S.Dds(REPO2 + '/scratchpad/roads1_20260911/out/after/tex/'
                    'Commonwealth.4.-20.20.DDS').level(0)[:, :, :3]
    v_sheet = S.Dds('E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/'
                    'Commonwealth/Commonwealth.4.-20.20.DDS').level(0)[:, :, :3]
    reg = np.load(os.path.join(HERE, 'riverbed.npz'))['riverbed'] > 0.85
    p('')
    p('GREY SPOTS ON THE FAR SHEETS, over the riverbed-dominant region (%d texels)'
      % int(reg.sum()))
    spots = {}
    for nm, a in (('ours', o_sheet), ('vanilla', v_sheet)):
        Ls, sats = S.lum(a), a.max(2) - a.min(2)
        m = (Ls > Ls[reg].mean() + Ls[reg].std()) & (sats < sats[reg].mean()) & reg
        ar = blobs(m, maxiter=200)
        ar = ar[ar >= 3]
        dd = 2.0 * np.sqrt(ar / np.pi)
        spots[nm] = dict(area_pct=100.0 * m.sum() / reg.sum(), n=int(ar.size),
                         median=float(np.median(dd)), mean=float(dd.mean()),
                         p90=float(np.percentile(dd, 90)))
        p('  %-8s grey area %5.2f%% of the region, %3d spots >= 3 texels, '
          'median diameter %.2f far-sheet texels = %.0f world units (p90 %.2f)'
          % (nm, spots[nm]['area_pct'], spots[nm]['n'], spots[nm]['median'],
             spots[nm]['median'] * UPT, spots[nm]['p90']))
    p('  a spot of %.2f texels at the ENGINE repeat would be 6x smaller: %.2f texels = %.0f world units'
      % (spots['ours']['median'], spots['ours']['median'] / 6.0,
         spots['ours']['median'] * UPT / 6.0))
    p('  that spot spans %.0f texels of %s at the baked repeat'
      % (spots['ours']['median'] * UPT / (BAKED / d.width), os.path.basename(TEX)))

    json.dump(dict(tex=TEX, width=d.width, mips=d.maxMip + 1,
                   spots=spots,
                   spot_ours_texels=spots['ours']['median'],
                   spot_van_texels=spots['vanilla']['median'],
                   bytes=os.path.getsize(TEX),
                   mean_rgb=[float(img[:, :, i].mean()) for i in range(3)],
                   halfwidth=hw_t, blob_median=med_t, blob_mean=mean_t,
                   blob_n=nb_t, K=K, pebble_texels=K * hw_t,
                   pebble_engine_units=K * hw_t * ENGINE / d.width,
                   pebble_engine_far=K * hw_t * ENGINE / d.width / UPT,
                   pebble_baked_units=K * hw_t * BAKED / d.width,
                   pebble_baked_far=K * hw_t * BAKED / d.width / UPT,
                   grey_rock=dict(median=grey_med, p90=grey_p90, max=grey_max,
                                  n=int(ag.size), pct=float(100.0 * mg.mean())),
                   engine=dict(repeat=ENGINE, upx=ENGINE / d.width,
                               spot_units=med_t * ENGINE / d.width,
                               spot_texels=med_t * ENGINE / d.width / UPT,
                               repeat_texels=ENGINE / UPT),
                   baked=dict(repeat=BAKED, upx=BAKED / d.width,
                              spot_units=med_t * BAKED / d.width,
                              spot_texels=med_t * BAKED / d.width / UPT,
                              repeat_texels=BAKED / UPT)),
              open(os.path.join(HERE, 'pebble.json'), 'w'), indent=1)
    with open(os.path.join(HERE, 'logs', 's4.log'), 'w') as f:
        f.write('\n'.join(log) + '\n')


if __name__ == '__main__':
    main()
