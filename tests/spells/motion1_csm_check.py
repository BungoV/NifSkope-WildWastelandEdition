"""THE SUN CASCADES IN MOTION (lane MOTION1, 2026-10-04; src/gl/sunshadow.h, the CSM1 law).

Reads a camera path's sidecar (<out>_path.txt from WW_RENDER_PATH: per frame the camera, the temporal AA's
echo and wwSunShadowSummary()) and judges every frame with lane CSM1's own independent fit
(tests/spells/pbr_csm1_gates.py: fit(), model_uvd(), parse rules), then what only motion can show:

  F  per frame   the splits and blend zones (zn, zf of each slice), the texel, the viewport, the snapped light
                 origin and window (pb, l, b) = the spec's, and the UPLOADED matrices put the judge's world
                 points within 0.02 texel / 0.05 units of where the spec puts them
  S  snapping    a world point lands at the SAME sub-texel phase in every frame whose cascade kept its texel
                 and viewport: the shadow-map grid is nailed to the world while the camera moves, which is
                 what stops shadow edges crawling (worst phase change <= 0.02 texel; needs >= 2 frames per
                 cascade sharing a texel, else REFUSED)
  M  moving      the camera did move (else S proves nothing): the eye travelled >= 100 units

RED CONTROLS: WW_CSM_RED=nosnap must fail S; WW_CSM_RED=wrongsplit must fail F.

usage: python motion1_csm_check.py <path sidecar> <lookdev hour> [--expect-red nosnap|wrongsplit]
"""
import os, sys, tempfile

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pbr_csm1_gates as g  # noqa: E402


def main():
    args = sys.argv[1:]
    red = args[args.index('--expect-red') + 1] if '--expect-red' in args else None
    side, hour = args[0], float(args[1])
    frames = []
    for line in open(side, encoding='utf-8', errors='replace'):
        if not line.startswith('frame '):
            continue
        p = line.split()
        k = int(p[1])
        eye = np.array([float(x) for x in p[3:6]])
        i = line.find(' csm=')
        tmp = tempfile.NamedTemporaryFile('w', delete=False, suffix='.txt', encoding='utf-8')
        tmp.write(line[i:])
        tmp.close()
        e = g.parse_echo(tmp.name)
        os.unlink(tmp.name)
        frames.append((k, eye, e))
    if not frames:
        print('csm FAIL  no frames in %s' % side)
        return 1
    L = -g.sun_to(hour)
    right, up = g.basis(L)
    res = {}

    # ---- F ----
    badF = []
    Q = np.array([[x, y, z] for x in np.linspace(-1500, 1500, 7) for y in np.linspace(-1500, 1500, 7)
                  for z in (-256.0, 0.0, 256.0)])
    per = []  # (k, cascade, tex, vw, vh, phase[npoints, 2])
    for k, eye, e in frames:
        if e is None or e.get('csm') != 'on':
            badF.append('frame %d: no cascade pass (%s)' % (k, None if e is None else e.get('csm')))
            continue
        cam = e['camo']
        fits = g.fit(L, cam, e['Dn'], e['mapn'])
        if np.abs(e['Lv'] - L).max() > 2e-5:
            badF.append('frame %d: L' % k)
        R = np.stack([cam.rc, cam.uc, -cam.fwd])
        pv = cam.sc * (Q - cam.C) @ R.T
        for i in range(3):
            got, want = e['cs'][i], fits[i]
            for f in ('zn', 'zf', 'far'):
                if abs(got[f] - want[f]) > 1e-6 * max(1.0, abs(want[f])):
                    badF.append('frame %d c%d.%s %.6g != %.6g' % (k, i, f, got[f], want[f]))
            for f in ('tex', 'n', 'vw', 'vh'):
                if abs(got[f] - want[f]) > 1e-9:
                    badF.append('frame %d c%d.%s %s != %s' % (k, i, f, got[f], want[f]))
            for f in ('l', 'b'):
                if abs(got[f] - want[f]) > 1e-6:
                    badF.append('frame %d c%d.%s' % (k, i, f))
            if np.abs(got['pb'] - want['pb']).max() > 1e-6:
                badF.append('frame %d c%d.pb' % (k, i))
            M = e['m%dv' % i]
            uvd = (np.c_[pv, np.ones(len(pv))] @ M.T)[:, :3]
            wantu = g.model_uvd(Q, want, L, right, up)
            eu = np.abs(uvd[:, :2] - wantu[:, :2]).max() * want['mapsz']
            ed = np.abs(uvd[:, 2] - wantu[:, 2]).max() * (want['far'] - g.NEAR)
            if eu > 0.02 or ed > 0.05:
                badF.append('frame %d m%d off %.3f texel %.3f units' % (k, i, eu, ed))
            per.append((k, i, got['tex'], got['vw'], got['vh'], uvd[:, :2] * want['mapsz']))
    res['F'] = not badF
    print('F %s  %d frames x 3 cascades against the spec fit%s' % (
        'PASS' if res['F'] else 'FAIL', len(frames), ('  BAD: ' + '; '.join(badF[:6])) if badF else ''))

    # ---- S ----
    worst, groups = 0.0, 0
    for i in range(3):
        bykey = {}
        for k, c, tex, vw, vh, ph in per:
            if c == i:
                bykey.setdefault((tex, vw, vh), []).append(ph)
        for key, phs in bykey.items():
            if len(phs) < 2:
                continue
            groups += 1
            ref = phs[0]
            for ph in phs[1:]:
                dphi = (ph - ref) - np.round(ph - ref)  # the change of sub-texel phase, wrapped
                worst = max(worst, float(np.abs(dphi).max()))
    if groups == 0:
        res['S'] = False
        print('S FAIL  REFUSED: no cascade kept its texel over two frames')
    else:
        res['S'] = worst <= 0.02
        print('S %s  worst sub-texel phase change of a world point %.4f texel over %d (cascade, texel) groups' % (
            'PASS' if res['S'] else 'FAIL', worst, groups))

    # ---- M ----
    eyes = np.array([f[1] for f in frames])
    travel = float(np.sum(np.linalg.norm(np.diff(eyes, axis=0), axis=1))) if len(eyes) > 1 else 0.0
    res['M'] = travel >= 100.0
    print('M %s  the eye travelled %.1f units over %d frames' % ('PASS' if res['M'] else 'FAIL', travel, len(frames)))

    if red:
        must = {'nosnap': 'S', 'wrongsplit': 'F'}[red]
        print('csm RED %s: stage %s %s' % (red, must, 'FAILS as it must' if not res[must] else 'PASSED (the gate is blind)'))
        print('csm %s' % ('FAIL' if not res[must] else 'BLIND'))
        return 0 if not res[must] else 1
    ok = all(res.values())
    print('csm %s' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
