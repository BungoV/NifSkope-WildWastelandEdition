"""THE WATER IN THE LINEAR FRAME, REBUILT (WATERHDR1, 2026-10-06; tests/spells/cell_water_hdr.sh).

The game draws its water into the HDR target with everything else and tone-maps the sum once. Here: the water's
own linear light (probes 25/26) through the game's chain (cell_is_check.chain: Shaders011's tonemap PS and LUT PS,
transcribed independently) with the weather imagespace the notes print (hdr, cine, tint, the two LUT keys and
their blend), the adapted luminance and the bloom from the measure dump (cell_is_check.bloom_of).

  R  the rebuild is the frame's tone map: the opaque cell-lit pixels within 3/255 of the rebuild's 3x3
     neighbourhood range, >= 97% (the same rule as cell_is_check P; proves the exterior chain is read right)
  W  the water's pixels (probe 27, eroded one pixel), the picture within 3/255 of the chain over the water's
     linear light + the bloom (3x3 range), >= 97% of >= 2000 pixels

usage  cell_water_hdr_check.py <Fallout4.esm> <run dir>   (on.png on.hdr on.hdr.txt p25.png p26.png p27.png p25.notes)
"""
import os
import re
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cell_is_check import LUMA, bloom_of, chain, lut_of  # noqa: E402

TOL = 3.0 / 255.0


def img(p):
    return np.asarray(Image.open(p).convert('RGB'), np.float64)


def weather_is(notes, data_dir):
    m = re.search(r'cell imagespace: exterior=(\S+) hdr=(\S+) lut=(\S+?)(?: \(LUT NOT FOUND\))? cine=(\S+) tint=(\S+) '
                  r'lutT=(\S+)', notes)
    if not m:
        return None
    im = dict(edid=m.group(1), hdr=[float(v) for v in m.group(2).split(',')],
              cine=[float(v) for v in m.group(4).split(',')], tint=[float(v) for v in m.group(5).split(',')])
    t = float(m.group(6))
    lut = None
    if m.group(3) != 'none':
        keys = m.group(3).split('->')
        luts = [lut_of(data_dir, k) for k in keys]
        if any(x is None for x in luts):
            return None
        lut = luts[0]
        if len(luts) == 2:   # the renderer blends the two strips' bytes, rounded (celllights.cpp isLighting)
            a, b = np.round(luts[0] * 255.0), np.round(luts[1] * 255.0)
            lut = np.floor(a + (b - a) * t + 0.5) / 255.0
    im['lutname'] = m.group(3)
    return im, lut


def within(want, pic, sel):
    h, w = want.shape[:2]
    pad = np.pad(want, ((1, 1), (1, 1), (0, 0)), mode='edge')
    nb = [pad[1 + dy:1 + dy + h, 1 + dx:1 + dx + w] for dy in (-1, 0, 1) for dx in (-1, 0, 1)]
    lo, hi = np.min(nb, 0), np.max(nb, 0)
    d = np.maximum(np.maximum(lo - pic, pic - hi), 0.0).max(-1)[sel]
    return float((d <= TOL).mean()) if d.size else 0.0, int(d.size)


def main(esm, run):
    notes = open(os.path.join(run, 'p25.notes'), errors='replace').read()
    got = weather_is(notes, os.path.dirname(esm))
    if got is None:
        print('water hdr FAIL: no exterior imagespace line (or its LUT) in p25.notes')
        return 1
    im, lut = got
    print('imgs %s hdr %s cine %s tint %s lut %s' % (im['edid'], ','.join('%g' % v for v in im['hdr']),
          ','.join('%g' % v for v in im['cine']), ','.join('%g' % v for v in im['tint']), im['lutname']))
    raw = open(os.path.join(run, 'on.hdr'), 'rb').read()
    w, h = struct.unpack_from('<2i', raw)
    px = np.frombuffer(raw, np.float32, w * h * 4, 8).reshape(h, w, 4).astype(np.float64)   # bottom row first
    up = np.where(np.isfinite(px[..., :3]), px[..., :3], 0.0)
    cover = np.frombuffer(raw, np.uint8, w * h, 8 + w * h * 16).reshape(h, w)[::-1]
    rgb = up[::-1]
    echo = open(os.path.join(run, 'on.hdr.txt')).read()
    adapted = float(re.search(r'adapted=(\S+)', echo).group(1))
    _, bup, _ = bloom_of(up, im['hdr'])
    bloom = bup[::-1]
    pic = img(os.path.join(run, 'on.png')) / 255.0
    if pic.shape[:2] != (h, w):
        print('water hdr FAIL: the picture is %dx%d, the dump %dx%d' % (pic.shape[1], pic.shape[0], w, h))
        return 1

    # R: the ground, the frame's own tone map against ours
    geo = cover == 1
    fr, nr = within(chain(rgb + bloom, im, adapted, lut), pic, geo)
    okR = fr >= 0.97 and nr >= 5000
    print('R %s  %.2f%% of %d opaque cell-lit pixels within 3/255 of the rebuilt chain (adapted %.6g)' % (
        'PASS' if okR else 'FAIL', 100 * fr, nr, adapted))

    # W: the water
    p25, p26, p27 = (img(os.path.join(run, 'p%d.png' % k)) for k in (25, 26, 27))
    water = (p27[..., 0] == 255) & (p27[..., 1] == 0) & (p27[..., 2] == 255)
    core = water.copy()   # eroded one pixel: the edges are antialiased against the bank
    core[1:, :] &= water[:-1, :]
    core[:-1, :] &= water[1:, :]
    core[:, 1:] &= water[:, :-1]
    core[:, :-1] &= water[:, 1:]
    lin = (np.round(p25) * 256.0 + np.round(p26)) / 65535.0 * 4.0
    sel = core & (p25.max(-1) < 255)   # lin / 4 clamped at 1: a glint past 4 is not read back
    want = chain(lin + bloom, im, adapted, lut)
    fw, nw = within(want, pic, sel)
    okW = fw >= 0.97 and nw >= 2000
    mean = lambda a: ', '.join('%.1f' % v for v in 255.0 * a[sel].mean(0)) if nw else '-'
    print('W %s  %.2f%% of %d water pixels within 3/255 of the chain over the water\'s linear light '
          '(picture mean %s; chain %s; the linear light raw %s; mean luma %.4g)' % (
              'PASS' if okW else 'FAIL', 100 * fw, nw, mean(pic), mean(want), mean(np.minimum(lin, 1.0)),
              float((lin[sel] @ LUMA).mean()) if nw else 0.0))
    print('water hdr %s (R %s, W %s)' % ('PASS' if okR and okW else 'FAIL', 'PASS' if okR else 'FAIL',
                                         'PASS' if okW else 'FAIL'))
    return 0 if okR and okW else 1


if __name__ == '__main__':
    sys.exit(main(*sys.argv[1:3]))
