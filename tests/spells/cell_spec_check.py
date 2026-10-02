#!/usr/bin/env python3
"""The placed lights' specular check (lane POOL1, 2026-10-01; tests/spells/cell_spec.sh).

  cell_spec_check.py <Fallout4.esm> <interior EDID> <shot dir> [label: the shots' prefix, default the EDID]

Reads <label>.probe2/3/4/9/30.png, their .cam dumps and <label>.probe30.notes from the shot dir. The lights come
from tests/spells/cell_lit_check.py's own walk of the plugin (gated there). The specular is written out again
here from a reading of the game's shipped deferred point light shader (the variant with the specular bit;
the one without it writes zero to its specular target, so a light flagged Non Specular, LIGH 0x8000, adds none):
  n = 2^(10 gloss + 1), H = normalize(V + L)
  D = NdotH^n (n + 2) / (2 pi)
  m = min(NdotL, NdotV); G = VdotH >= 2 NdotH m ? 2 NdotH (NdotV == m ? 1 : NdotL / NdotV) / VdotH : 1 / NdotV
  f = 1 - VdotH; F = min((1 - f^5) 0.2 + f^5, 1)
  specular = min(D G F / 4, 15) pi x NdotL x the light's radial / cone weight x its colour   (x the gbuffer mask)
V runs from each pixel to the camera (the .cam dump), the gloss is probe 9's red. Probe 30 = the sum / 4,
before the mask. Judged over the clean pixels, and again over the pixels where either side shows a specular;
the tolerance is 3/255 + 8% + the spread of the expected value over half a step of the probes' gloss, normal
and position (the lobe is sharp; the lamps a Non Specular flag hides are not in that spread). The viewer's
total over the highlight pixels must also sit within 5% of the expected total.
The walkway floor (z -59.6, the two tiles at x 0..512, y -640..-384) gets its own measured line.
One verdict line; exit 0 on PASS.
"""
import os
import re
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cell_lit_check import lights_of  # noqa: E402


def weight(L, P, N):
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
    return np.where((d < L['r']) & (nl > 0), a, 0.0), Ld, nl


def spec_sum(lights, P, N, V, gloss, honour_flag=True):
    S = np.zeros_like(P)
    n = np.exp2(gloss * 10 + 1)
    nv = np.clip(np.einsum('ij,ij->i', N, V), 0, 1)
    for L in lights:
        if honour_flag and L['flags'] & 0x8000:
            continue
        a, Ld, nl = weight(L, P, N)
        nl = np.clip(nl, 0, 1)
        H = V + Ld
        H /= np.maximum(np.linalg.norm(H, axis=1), 1e-6)[:, None]
        nh = np.clip(np.einsum('ij,ij->i', N, H), 0, 1)
        vh = np.clip(np.einsum('ij,ij->i', V, H), 0, 1)
        D = nh ** n * (n + 2) / (2 * np.pi)
        m = np.minimum(nl, nv)
        G = np.where(vh >= 2 * nh * m,
                     2 * nh * np.where(nv == m, 1.0, nl / np.maximum(nv, 1e-4)) / np.maximum(vh, 1e-4),
                     1 / np.maximum(nv, 1e-4))
        f = 1 - vh
        F = np.minimum((1 - f ** 5) * 0.2 + f ** 5, 1)
        s = np.minimum(D * G * F * 0.25, 15) * np.pi * nl * a
        S += s[:, None] * L['c'][None, :]
    return S


def main(esm, cell, shots, label=None):
    label = label or cell
    tags = (2, 3, 4, 9, 30)
    img = {p: np.asarray(Image.open(os.path.join(shots, '%s.probe%d.png' % (label, p))).convert('RGB'), float)
           for p in tags}
    notes = open(os.path.join(shots, label + '.probe30.notes'), encoding='utf-8', errors='replace').read()
    m = re.search(r'cell lighting: .*center=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', notes)
    if not m:
        return 'spec FAIL %s: no "cell lighting ... center=" line in the notes' % label
    center = np.array([float(v) for v in m.groups()])
    cams = set()
    for p in tags:
        f = os.path.join(shots, '%s.probe%d.cam' % (label, p))
        c = re.search(r'cam=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', open(f).read()) if os.path.exists(f) else None
        if not c:
            return 'spec FAIL %s: no camera dump for probe %d' % (label, p)
        cams.add(c.groups())
    if len(cams) != 1:
        return 'spec FAIL %s: the probes saw %d different cameras' % (label, len(cams))
    cam = np.array([float(v) for v in cams.pop()])
    q = np.round(img[2]) * 256 + np.round(img[3])
    P = q + 0.5 - 32768.0 + center
    N = img[4] / 255.0 * 2 - 1
    nlen = np.linalg.norm(N, axis=2)
    ok = np.abs(nlen - 1) < 0.06
    for k in (2, 3, 4):
        ok &= np.any(img[k] != img[k][0, 0], axis=2)
    ok &= (img[9][..., 1] == 0) & (img[9][..., 2] == 0)     # the legacy program only
    for dy, dx in ((0, 1), (1, 0), (0, -1), (-1, 0)):
        ok &= np.linalg.norm(P - np.roll(P, (dy, dx), (0, 1)), axis=2) < 40
        ok &= np.linalg.norm(N - np.roll(N, (dy, dx), (0, 1)), axis=2) < 0.06
        # the specular lobe is sharp in gloss: a gloss step between neighbours makes the 8-bit gloss a guess
        ok &= np.abs(img[9][..., 0] - np.roll(img[9][..., 0], (dy, dx), (0, 1))) <= 6
    ok[0, :] = ok[-1, :] = ok[:, 0] = ok[:, -1] = False
    ys, xs = np.nonzero(ok)
    if len(ys) < 2000:
        return 'spec SKIP %s: %d clean legacy cell-lit pixels, under 2000' % (label, len(ys))
    rng = np.random.default_rng(1)
    pick = rng.choice(len(ys), size=min(30000, len(ys)), replace=False)
    ys, xs = ys[pick], xs[pick]
    Pp, Np = P[ys, xs], N[ys, xs] / nlen[ys, xs][:, None]
    V = cam[None, :] - Pp
    V /= np.maximum(np.linalg.norm(V, axis=1), 1e-3)[:, None]
    gloss = img[9][ys, xs, 0] / 255.0
    lights = lights_of(esm, cell)
    exp = np.clip(spec_sum(lights, Pp, Np, V, gloss) / 4.0, 0, 1)
    got = img[30][ys, xs] / 255.0
    # the lobe is sharp, and the probes carry the gloss and the normal in 8 bits and the position in whole units:
    # the tolerance takes the spread of the expected value over half a step of each (measured 2026-10-02: with
    # the gloss alone 51 of 871 highlight pixels missed, all beside a lamp; with all three none did, and the
    # red control still missed 150 of 1025)
    def at(P_, N_, g_):
        return np.clip(spec_sum(lights, P_, N_, V, g_) / 4.0, 0, 1)
    spread = np.abs(at(Pp, Np, np.clip(gloss + 0.5 / 255, 0, 1)) - at(Pp, Np, np.clip(gloss - 0.5 / 255, 0, 1)))
    nspread = np.zeros_like(exp)
    pspread = np.zeros_like(exp)
    for ax in range(3):
        for s in (-1, 1):
            N2 = N[ys, xs].copy()
            N2[:, ax] += s * 0.5 / 127.5
            N2 /= np.linalg.norm(N2, axis=1)[:, None]
            nspread = np.maximum(nspread, np.abs(at(Pp, N2, gloss) - exp))
            P2 = Pp.copy()
            P2[:, ax] += s * 0.5
            pspread = np.maximum(pspread, np.abs(at(P2, Np, gloss) - exp))
    tol = 3.0 / 255 + 0.08 * exp + spread + nspread + pspread
    good = np.all(np.abs(got - exp) <= tol, axis=1)
    shows = (exp.max(axis=1) > 0.02) | (got.max(axis=1) > 0.02)
    share = good.mean()
    show_share = good[shows].mean() if shows.any() else 0.0
    if shows.sum() < 300:
        return 'spec SKIP %s: %d pixels show a specular, under 300' % (label, shows.sum())
    # what the Non Specular lights would add here if drawn (the red's lever), and the walkway floor alone
    extra = np.clip(spec_sum(lights, Pp, Np, V, gloss, honour_flag=False) / 4.0, 0, 1) - exp
    floor = (np.abs(Pp[:, 2] + 59.6) < 2) & (Pp[:, 0] > 0) & (Pp[:, 0] < 512) & (Pp[:, 1] > -640) & (Pp[:, 1] < -384)
    fl = ('floor %d px: specular/4 mean %.4f max %.3f (got mean %.4f), the Non Specular lamps would add mean %.4f'
          % (floor.sum(), exp[floor].max(axis=1).mean(), exp[floor].max(), got[floor].max(axis=1).mean(),
             extra[floor].max(axis=1).mean())) if floor.sum() >= 50 else 'floor too few px'
    # the per-pixel tolerance is wide where the highlight is dim, so the total over the highlight pixels is held
    # too: a wrong scale passes pixel by pixel long before it passes here
    ratio = got[shows].sum() / max(exp[shows].sum(), 1e-6)
    verdict = share >= 0.97 and show_share >= 0.95 and 0.95 <= ratio <= 1.05
    nonspec = sum(1 for L in lights if L['flags'] & 0x8000)
    return ('spec %s %s: %d lights (%d Non Specular); %d clean pixels sampled, %d show a specular; agree %.1f%% '
            '(where shown %.1f%%); viewer/expected %.3f; mean |err| %.4f, p99 %.4f; %s'
            % ('PASS' if verdict else 'FAIL', label, len(lights), nonspec, len(ys), shows.sum(), 100 * share,
               100 * show_share, ratio, np.abs(got - exp).mean(), np.percentile(np.abs(got - exp), 99), fl))


if __name__ == '__main__':
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(2)
    line = main(*sys.argv[1:5])
    print(line)
    sys.exit(0 if ' PASS ' in line else 1)
