#!/usr/bin/env python3
"""VOLAIR1 hue gate (bungo 10-15: "our fog is what vanilla has as the color"): the air adds no hue of its own.

  volair1_hue_check.py <load order> <dir> <on> <nol> <vac> <game> <depth> [lamp refs, comma-separated hex]

Frames of one camera, bloom / glare / imagespace bloom off, linear (WW_CELL_HDR_DUMP: int32 W, H, RGBA float32,
bottom row first), <dir>/<tag>.hdr; <on>.bin.txt is the air dump of the on frame (camera, lights, phase weights):
  on    the air, lamps on            nol   the air with WW_CELL_AIR_RED=nolights (the haze alone)
  vac   the fog row off (no air)     game  the air off (WW_CELL_AIR=0): the game's own distance fog
  depth <depth>.ao, WW_CELL_AO_DUMP of a frame of the same camera (the scene depth each ray stops at)

Stage H (the haze is the records' colour): on the pixels where the haze outweighs the scene (vac under a quarter of
  nol and of game in luminance, haze and game fog both above 1e-4), the haze nol - vac and the game's fog game - vac
  are compared as chromaticity (r = R / G, b = B / G of the pixel sums). PASS when both ratios (air / game) lie in
  0.9..1.1. The game's fog is the reference: it is the records' colour as vanilla draws it.
Stage P (a lamp keeps its colour in the air): for each named lamp (default: every placed light in view within 4000
  units), the ring 20..80 px round its projection, halo = on - nol (only the air's lamp term differs), its
  chromaticity against the single-scatter reference along the ring's rays (to the scene depth) through every light
  ball the ray crosses (10-15: the ring holds every lamp on the way, not only its own), built from the
  records' colours, the radial law and NEUTRAL scatter (each lobe at its luminance: the reference never takes the
  dump's weights, so a coloured medium cannot move it). PASS when r and b each lie within 0.85..1.18 of the
  reference for every judged lamp (halo luminance at least 1e-3 and 5% of the reference).
Reds: WW_CELL_AIR_RED=bluescatter (the records' coloured weights) must FAIL stage P; physcol (the haze in the
  physical, not the records', colour) must FAIL stage H.
"""
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import volair1_lamp_check as LC  # noqa: E402

LUM = LC.LUM


def hdr(p):
    with open(p, 'rb') as f:
        w, h = struct.unpack('<ii', f.read(8))
        a = np.frombuffer(f.read(w * h * 16), np.float32).reshape(h, w, 4)
    return a[::-1, :, :3].astype(float)


def depth(p):
    """the AO dump's opaque pass (WW_CELL_AO_DUMP: int32 W H hw hh, W x H RGBA float, bottom row first): its alpha
    is the linear view depth in game units (1e6 = nothing drawn), the buffer the air ends its rays at"""
    with open(p, 'rb') as f:
        w, h, _, _ = struct.unpack('<4i', f.read(16))
        a = np.frombuffer(f.read(w * h * 16), np.float32).reshape(h, w, 4)
    return a[::-1, :, 3].astype(float)


def chroma(v):
    return v[0] / max(v[1], 1e-12), v[2] / max(v[1], 1e-12)


def stage_h(ON, NOL, VAC, GAME):
    haze, fog = NOL - VAC, GAME - VAC
    lv, ln, lg = VAC @ LUM, NOL @ LUM, GAME @ LUM
    m = (lv < 0.25 * ln) & (lv < 0.25 * lg) & ((haze @ LUM) > 1e-4) & ((fog @ LUM) > 1e-4)
    if m.sum() < 1000:
        return 'FAIL', 'stage H: only %d haze pixels' % m.sum()
    ra, ba = chroma(haze[m].sum(0))
    rg, bg = chroma(fog[m].sum(0))
    v = 'PASS' if 0.9 <= ra / rg <= 1.1 and 0.9 <= ba / bg <= 1.1 else 'FAIL'
    return v, 'stage H %s: %d px; haze r %.3f b %.3f, game fog r %.3f b %.3f -> ratio r %.3f b %.3f (bar 0.9..1.1)' % (
        v, m.sum(), ra, ba, rg, bg, ra / rg, ba / bg)


def stage_p(esm, t, lights, ON, NOL, only, DEP):
    H, W = ON.shape[:2]
    dh, dwd = DEP.shape
    p00, p11 = t['proj']
    R = np.stack([t['cellRow%d' % i] for i in range(3)])
    M, cam = R[:, :3], R[:, 3]
    Minv = np.linalg.inv(M)
    s0 = float(t['airMed'][0])
    zoff = float(t['fogView'][3]) - float(t['camZw'][0])
    PL = LC.plugin_lights(esm, {L['ref'] for L in lights if L['ref']})
    tn = dict(t)	# the neutral air: each lobe's weights at their luminance on every channel
    for k in ('phase0', 'phase1', 'phase2'):
        w = np.array(t[k], float)
        w[:3] = w[:3] @ LUM
        tn[k] = w
    yy, xx = np.mgrid[0:H, 0:W]
    lines, judged, fails = [], 0, 0
    for L in lights:
        v = Minv @ (L['pos'] - cam)
        dist = float(np.linalg.norm(v))
        if v[2] >= 0 or dist > 4000 or (only and L['ref'] not in only):
            continue
        px, py = (0.5 + 0.5 * p00 * v[0] / -v[2]) * W, (0.5 - 0.5 * p11 * v[1] / -v[2]) * H
        if not (0 <= px < W and 0 <= py < H):
            continue
        rr = np.hypot(xx - px, yy - py)
        ring = (rr >= 20) & (rr <= 80)
        if ring.sum() < 50:
            continue
        halo = (ON - NOL)[ring].mean(0)
        idx = np.flatnonzero(ring.ravel())
        idx = idx[np.linspace(0, len(idx) - 1, 48).astype(int)]
        ref = np.zeros(3)
        for q in idx:
            X, Y = q % W, q // W
            d = np.array([((X + 0.5) / W * 2 - 1) / p00, (1 - (Y + 0.5) / H * 2) / p11, -1.0])
            dw = M @ (d / np.linalg.norm(d))
            a = DEP[min(int(Y * dh / H), dh - 1), min(int(X * dwd / W), dwd - 1)]
            end = a * np.linalg.norm(d / -d[2]) / np.linalg.norm(M[0]) if a < 5.0e5 else dist + L['r']
            ts = np.linspace(1.0, max(end, 2.0), 600)	# the ray ends at the drawn scene, as the air's does
            dt = ts[1] - ts[0]
            Wp = cam + np.outer(ts, dw)
            sig = s0 * LC.rho_law(t, Wp[:, 2] + zoff)
            Tr = np.exp(-np.cumsum(sig) * dt)
            for P in PL.values():
                if P['off'] or P['lum'] <= 0:
                    continue
                tc = np.clip((P['pos'] - cam) @ dw, ts[0], ts[-1])	# every light whose ball the ray crosses
                if np.linalg.norm(cam + tc * dw - P['pos']) >= P['r']:
                    continue
                # the lamp check's own single-scatter source (radial law, spot cone, hemisphere, the light-side
                # optical depth) under the neutral phase: the scalar source times the record's colour
                j = LC.ref_J(P, Wp, np.tile(dw, (len(ts), 1)), tn, s0, zoff) / P['lum']
                ref += np.outer(sig * Tr * j, P['c']).sum(0) * dt / len(idx)
        hl, fl = halo @ LUM, ref @ LUM
        hr, hb = chroma(halo)
        fr, fb = chroma(ref)
        if hl < 1e-3 or hl < 0.05 * fl or fl <= 0:
            st = 'SKIP(dim)'
        else:
            judged += 1
            ok = 0.85 <= hr / fr <= 1.18 and 0.85 <= hb / fb <= 1.18
            st = 'ok' if ok else 'FAIL'
            fails += not ok
        lines.append('  %08x px (%4.0f,%4.0f) d %5.0f r %5.0f | halo lum %.4f r %.3f b %.3f | ref lum %.4f r %.3f b %.3f | '
                     'ratio r %.3f b %.3f %s' % (L['ref'], px, py, dist, L['r'], hl, hr, hb, fl, fr, fb,
                                                 hr / fr if fr else float('nan'), hb / fb if fb else float('nan'), st))
    v = 'PASS' if judged >= 1 and fails == 0 else 'FAIL'
    return v, 'stage P %s: %d lamps judged, %d fail (bar 0.85..1.18 on r and b)' % (v, judged, fails), lines


def main(a):
    esm, d, on, nol, vac, game, dep = a[:7]
    only = {int(x, 16) for x in a[7].split(',')} if len(a) > 7 and a[7] else None
    t, lights = LC.read_txt(os.path.join(d, on + '.bin.txt'))
    ON, NOL, VAC, GAME = (hdr(os.path.join(d, x + '.hdr')) for x in (on, nol, vac, game))
    vh, lh = stage_h(ON, NOL, VAC, GAME)
    vp, lp, lines = stage_p(esm, t, lights, ON, NOL, only, depth(os.path.join(d, dep + '.ao')))
    print('VOLAIR1 HUE %s: phase0 %s (red %d)' % (on, ' '.join('%.3f' % x for x in t['phase0'][:3]), int(t['red'][0])))
    print(lh)
    print(lp)
    print('\n'.join(lines))
    v = 'PASS' if vh == 'PASS' and vp == 'PASS' else 'FAIL'
    print('VOLAIR1 HUE VERDICT %s (H %s, P %s)' % (v, vh, vp))
    return v


if __name__ == '__main__':
    sys.exit(0 if main(sys.argv[1:]) == 'PASS' else 1)
