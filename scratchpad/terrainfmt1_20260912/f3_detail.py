"""Gate F3, the measurement, BEFORE any `--msn-detail` code exists.

The question: does our own `_msn` bake carry the texel-scale detail vanilla's
`_msn` carries? Four numbers for each sheet, each with what it is judged
against:

  rough1   mean |1-texel neighbour difference| over the east and north channels,
           in levels of 255 -- PIC-CHUNK's texel-scale roughness. This is the
           quantity `--msn-detail` would exist to raise.
  rough2   the same at a 2-texel lag. A sheet whose rough1 is high only because
           it is NOISY has rough1/rough2 near 1; a sheet with real structure at
           the texel scale has a ratio well below 1.
  twin     the same statistic on a PHASE-RANDOMISED twin of the same sheet:
           identical power spectrum, no structure. It is the control for
           "is this structure or is it noise", NOT a floor for energy -- a twin
           has by construction the same energy as its original.
  ceiling  the land textures' own `_n` maps, the thing `--msn-detail` proposes
           to blend in, measured at the footprint mip the blend would use.

Two tiles: Commonwealth.4.-20.24 (Sanctuary, mixed ground) and
Commonwealth.4.-16.24 (its eastern neighbour), both baked with
`--land-detail-source none` so the sheet is OUR bake and not a copy of
Bethesda's -- the shipped default copies vanilla's file byte for byte and would
have measured vanilla twice.
"""
import json
import os
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from dds_np import Dds                                            # noqa: E402

VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
TILES = [('Commonwealth.4.-20.24', os.path.join(HERE, 'bake', 'ourleg', 'tex')),
         ('Commonwealth.4.-16.24', os.path.join(HERE, 'bake', 'ourleg2', 'tex'))]


def rough(img, lag=1):
    """mean |neighbour difference| at `lag` texels, averaged over x and y."""
    a = img.astype(np.float64)
    dx = np.abs(a[:, lag:] - a[:, :-lag]).mean()
    dy = np.abs(a[lag:, :] - a[:-lag, :]).mean()
    return float(0.5 * (dx + dy))


def phase_twin(img, rng):
    """same power spectrum, random phase."""
    a = img.astype(np.float64)
    F = np.fft.rfft2(a)
    ph = rng.uniform(-np.pi, np.pi, F.shape)
    return np.fft.irfft2(np.abs(F) * np.exp(1j * ph), s=a.shape)


def stats(rgb, rng):
    """east = R, north = B (lodgenTerrainMsnPixel: R east, G up, B north)."""
    out = {}
    for name, ch in (('east', 0), ('north', 2)):
        img = rgb[:, :, ch]
        t = phase_twin(img, rng)
        out[name] = {'rough1': rough(img), 'rough2': rough(img, 2),
                     'twin_rough1': rough(t), 'twin_rough2': rough(t, 2)}
        out[name]['ratio12'] = out[name]['rough1'] / out[name]['rough2']
        out[name]['twin_ratio12'] = out[name]['twin_rough1'] / out[name]['twin_rough2']
    out['rough1_mean'] = 0.5 * (out['east']['rough1'] + out['north']['rough1'])
    return out


res = {}
rng = np.random.default_rng(20260912)
for ch, ourdir in TILES:
    res[ch] = {}
    for arm, path in (('vanilla', os.path.join(VAN, ch + '_msn.DDS')),
                      ('ours', os.path.join(ourdir, ch + '_msn.DDS'))):
        rgb = Dds(path).rgb(0)
        res[ch][arm] = stats(rgb, rng)
    v = res[ch]['vanilla']['rough1_mean']
    o = res[ch]['ours']['rough1_mean']
    res[ch]['ours_over_vanilla'] = o / v
    print('%s  vanilla rough1 %.3f   ours %.3f   ours/vanilla %.3f'
          % (ch, v, o, o / v))
    for arm in ('vanilla', 'ours'):
        for cn in ('east', 'north'):
            d = res[ch][arm][cn]
            print('   %-7s %-5s rough1 %6.3f  rough2 %6.3f  r1/r2 %.3f   '
                  'twin r1 %6.3f  twin r1/r2 %.3f'
                  % (arm, cn, d['rough1'], d['rough2'], d['ratio12'],
                     d['twin_rough1'], d['twin_ratio12']))

json.dump(res, open(os.path.join(HERE, 'f3_detail.json'), 'w'), indent=1)
print('wrote f3_detail.json')
