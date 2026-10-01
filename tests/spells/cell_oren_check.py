#!/usr/bin/env python3
"""The game's diffuse check (lane ON1, 2026-10-01; tests/spells/cell_oren.sh).

  cell_oren_check.py <Fallout4.esm> <interior EDID> <shot dir> [label: the shots' prefix, default the EDID]

Reads <cell>.probe2/3/4/8/9/10.png, their .cam dumps and <cell>.probe8.notes from the shot dir. The lights come
from tests/spells/cell_lit_check.py's own walk of the plugin (gated there); this check adds the game's
legacy-branch diffuse, written out again from its reading of the shipped point / spot light shaders:
  sigma = 1 - gloss, s2 = sigma^2, A = 1 - 0.5 s2 / (s2 + 0.57), B = 0.45 s2 / (s2 + 0.09)
  cosPhi = dot(V - N NdotV, L - N NdotL)            (NOT normalised: the asm's own quirk)
  geom = sqrt(sat((1 - NdotL^2)(1 - NdotV^2))) / max(NdotL, NdotV)
  rim = sat(dot(V, -L)) (1 - NdotV)^0.01 (1 - gloss)          (lane RIM1: the back-light term)
  diffuse = (max(cosPhi, 0) B geom + A + rim) x NdotL x the light's radial / cone weight x its colour
  a light flagged No Rim Lighting (LIGH 0x80000) drops the rim; one flagged Ignore Roughness (0x40000) drops
  the rim and takes 1 for the Oren-Nayar factor (the shipped shader variants with those bits, lane RIM1)
V runs from each pixel to the camera (the .cam dump), the gloss is probe 9's red. Probe 8 = that sum / 4.
Probe 10 = the rim part alone x 4: in probe 8 the flagged lights' rim sits under the 8-bit tolerance almost
everywhere (6 of 18,029 lit Institute pixels move), sixteen times larger it moves 12,178 of Vault view 1's.
It gets its own verdict over the pixels where either side shows a rim.
One verdict line; exit 0 on PASS.
"""
import os
import re
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cell_lit_check import lights_of  # noqa: E402


def oren_sum(lights, P, N, V, gloss, normalised=False, flags=True, rim_only=False):
    E = np.zeros_like(P)
    s2 = (1.0 - gloss) ** 2
    A = 1.0 - 0.5 * s2 / (s2 + 0.57)
    B = 0.45 * s2 / (s2 + 0.09)
    nv = np.einsum('ij,ij->i', N, V)
    for L in lights:
        v = L['pos'] - P
        d = np.linalg.norm(v, axis=1)
        Ld = v / np.maximum(d, 1e-3)[:, None]
        nl = np.einsum('ij,ij->i', N, Ld)
        x = np.clip(d / L['r'], 0, 1)
        bias, scale, ex = L['bse']
        xe = x ** ex if ex > 0 else np.ones_like(x)
        a = (1 - np.clip(scale * xe + bias, 0, 1)) ** 2.2
        if L['spot']:
            base = np.clip(1 - (1 - (-Ld @ L['aim'])) / max(1 - L['cos'], 1e-4), 0, 1)
            a = a * np.minimum(base ** max(L['cone'], 1e-3), 1)
        tv = V - N * nv[:, None]
        tl = Ld - N * nl[:, None]
        cphi = np.einsum('ij,ij->i', tv, tl)
        if normalised:      # the textbook form: only to find the pixels where the game's quirk shows
            cphi = cphi / np.maximum(np.linalg.norm(tv, axis=1) * np.linalg.norm(tl, axis=1), 1e-6)
        geom = np.sqrt(np.clip((1 - nl * nl) * (1 - nv * nv), 0, 1)) / np.maximum(np.maximum(nl, nv), 1e-4)
        oren = np.maximum(cphi, 0) * B * geom + A
        if flags and L['rough']:
            oren = np.ones_like(oren)
        if rim_only:        # probe 10
            oren = np.zeros_like(oren)
        # lane RIM1: the back-light term every light adds to the legacy diffuse, unless the light opts out
        if not (flags and (L['norim'] or L['rough'])):
            oren = oren + np.clip(-np.einsum('ij,ij->i', V, Ld), 0, 1) * np.clip(1 - nv, 0, 1) ** 0.01 * (1 - gloss)
        w = np.where((d < L['r']) & (nl > 0), a * nl * oren, 0.0)
        E += w[:, None] * L['c'][None, :]
    return E


def main(esm, cell, shots, label=None):
    label = label or cell   # the shots' file prefix (a cell seen from two cameras has two)
    tags = (2, 3, 4, 8, 9, 10)
    img = {p: np.asarray(Image.open(os.path.join(shots, '%s.probe%d.png' % (label, p))).convert('RGB'), float)
           for p in tags}
    notes = open(os.path.join(shots, label + '.probe8.notes'), encoding='utf-8', errors='replace').read()
    m = re.search(r'cell lighting: .*center=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', notes)
    if not m:
        return 'oren FAIL %s: no "cell lighting ... center=" line in the notes' % cell
    center = np.array([float(v) for v in m.groups()])
    cams = set()
    for p in tags:
        f = os.path.join(shots, '%s.probe%d.cam' % (label, p))
        if not os.path.exists(f):
            return 'oren FAIL %s: no camera dump for probe %d' % (cell, p)
        c = re.search(r'cam=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', open(f).read())
        if not c:
            return 'oren FAIL %s: an empty camera dump for probe %d' % (cell, p)
        cams.add(c.groups())
    if len(cams) != 1:
        return 'oren FAIL %s: the probes saw %d different cameras' % (cell, len(cams))
    cam = np.array([float(v) for v in cams.pop()])
    q = np.round(img[2]) * 256 + np.round(img[3])
    P = q + 0.5 - 32768.0 + center
    N = img[4] / 255.0 * 2 - 1
    nlen = np.linalg.norm(N, axis=2)
    ok = np.abs(nlen - 1) < 0.06
    for k in (2, 3, 4):
        ok &= np.any(img[k] != img[k][0, 0], axis=2)
    ok &= np.any(img[2] != img[3], axis=2) | np.any(img[3] != img[4], axis=2)
    # probe 9 is (gloss, 0, 0) on the legacy program only: anything else drew it (PBR, effects, glass)
    ok &= (img[9][..., 1] == 0) & (img[9][..., 2] == 0)
    for dy, dx in ((0, 1), (1, 0), (0, -1), (-1, 0)):
        ok &= np.linalg.norm(P - np.roll(P, (dy, dx), (0, 1)), axis=2) < 40
        # a smooth normal and gloss too: an 8-bit normal on a crease is not the fragment's normal. The gloss
        # step is 12/255, not 2: a gloss map varies texel to texel, and at 2 only flat-gloss surfaces were left
        # (Vault view 2 kept 443 lit pixels once the steam stopped covering the probes; lane EFX1). Measured on
        # those shots: with no gloss limit at all the pixels still agree 99.9%; 12 still drops material seams.
        ok &= np.linalg.norm(N - np.roll(N, (dy, dx), (0, 1)), axis=2) < 0.06
        ok &= np.abs(img[9][..., 0] - np.roll(img[9][..., 0], (dy, dx), (0, 1))) <= 12
    ok[0, :] = ok[-1, :] = ok[:, 0] = ok[:, -1] = False
    ys, xs = np.nonzero(ok)
    if len(ys) < 2000:     # too little to judge: SKIP, so a red cannot "fail" on a data shortage
        return 'oren SKIP %s: %d clean legacy cell-lit pixels, under 2000' % (label, len(ys))
    rng = np.random.default_rng(1)
    pick = rng.choice(len(ys), size=min(20000, len(ys)), replace=False)
    ys, xs = ys[pick], xs[pick]
    Pp, Np = P[ys, xs], N[ys, xs] / nlen[ys, xs][:, None]
    V = cam[None, :] - Pp
    V /= np.maximum(np.linalg.norm(V, axis=1), 1e-3)[:, None]
    gloss = img[9][ys, xs, 0] / 255.0
    lights = lights_of(esm, cell)
    exp = np.clip(oren_sum(lights, Pp, Np, V, gloss) / 4.0, 0, 1)
    got = img[8][ys, xs] / 255.0
    err = np.abs(got - exp)
    good = np.all(err <= 3.0 / 255 + 0.05 * exp, axis=1)
    lit = exp.max(axis=1) > 0.02
    share = good.mean()
    lit_share = good[lit].mean() if lit.any() else 0.0
    # how far the game's diffuse sits from Lambert here (the red's lever): the A term alone
    s2 = (1 - gloss[lit]) ** 2
    a_mean = float(np.mean(1 - 0.5 * s2 / (s2 + 0.57))) if lit.any() else 1.0
    if lit.sum() < 500:
        return 'oren SKIP %s: %d lit pixels of %d, under 500' % (label, lit.sum(), len(ys))
    # the pixels where the game's unnormalised cosPhi and the textbook one part by over twice the tolerance:
    # the whole-frame share hides them, so they get their own verdict
    tol = 3.0 / 255 + 0.05 * exp
    expn = np.clip(oren_sum(lights, Pp, Np, V, gloss, normalised=True) / 4.0, 0, 1)
    quirk = lit & np.any(np.abs(exp - expn) > 2 * tol, axis=1)
    quirk_share = good[quirk].mean() if quirk.any() else 1.0
    quirk_ok = quirk.sum() < 200 or quirk_share >= 0.95
    # lane RIM1: the rim alone (probe 10), judged where either side shows one, so a rim the flags should have
    # dropped counts against it
    exp_r = np.clip(oren_sum(lights, Pp, Np, V, gloss, rim_only=True) * 4.0, 0, 1)
    got_r = img[10][ys, xs] / 255.0
    rim = (exp_r.max(axis=1) > 0.02) | (got_r.max(axis=1) > 0.02)
    good_r = np.all(np.abs(got_r - exp_r) <= 3.0 / 255 + 0.05 * exp_r, axis=1)
    rim_share = good_r[rim].mean() if rim.any() else 1.0
    rim_ok = rim.sum() < 200 or rim_share >= 0.95
    verdict = share >= 0.97 and lit_share >= 0.95 and quirk_ok and rim_ok
    return ('oren %s %s: %d lights; %d clean pixels sampled, %d lit; agree %.1f%% (lit %.1f%%; quirk %d px, %s; '
            'rim %d px, %s); gloss mean %.2f, A mean %.3f; mean |err| %.4f, p99 %.4f'
            % ('PASS' if verdict else 'FAIL', label, len(lights), len(ys), lit.sum(), 100 * share, 100 * lit_share,
               quirk.sum(), ('%.1f%%' % (100 * quirk_share)) if quirk.sum() >= 200 else 'too few to judge',
               rim.sum(), ('%.1f%%' % (100 * rim_share)) if rim.sum() >= 200 else 'too few to judge',
               gloss.mean(), a_mean, err.mean(), np.percentile(err, 99)))


if __name__ == '__main__':
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(2)
    line = main(*sys.argv[1:5])
    print(line)
    sys.exit(0 if ' PASS ' in line else 1)
