"""Lane VKFAR2 (2026-11-14, the froxel item): the independent rebuild of every volume fogview.exe dumped. From each arm's
dumped inject alone (float32, layer / row / column / RGBA), numpy rebuilds the integrate in float64: per column of a grid,
L += T * rgb, T *= a over its layers from its base, the froxel holding (L, T) after its own layer. The near grid is
layers 0 .. near-1 over near cols x rows; the far grid follows at base = the near slices over its own cols x rows.
RGBA16F volumes: the rebuild is stored as the format stores it (store16: round toward zero, measured), then judged.
Bars (set before the first run, never widened; skill ww-froxel-volume-gate):
  rgb: of the lit values (rebuild > 1e-3) >= 99 % within 1 % relative; the summed light of the grids within 1 %
  alpha: within rebuild +- 1e-4 (RGBA16F: both ends stored as the format stores them; set first as + half a half-float
         spacing on round-to-nearest, before the store was measured as toward zero)
  outside the grids: the sentinel exactly; no NaN / Inf anywhere
Both GL's volume (gl.bin) and Vulkan's (vk.bin) are judged; each red dump (vk_red<r>.bin) must FAIL.

    python -I tests/vkframe/fogview_check.py <fogview out dir>

Last line `FOGVIEW CHECK OK` or `FOGVIEW CHECK FAILED`.
"""
import glob
import os
import sys

import numpy as np

OUT = os.path.abspath(sys.argv[1])


def meta(path):
    m = {}
    with open(path) as f:
        for line in f:
            p = line.split()
            if p:
                m[p[0]] = p[1:]
    return m


def rebuild(inj, near, far, D, H, W):
    ref = np.zeros((D, H, W, 4))
    inside = np.zeros((D, H, W), bool)
    for cols, rows, base, layers in ((near[0], near[1], 0, near[2]), (far[0], far[1], near[2], far[2])):
        if layers <= 0:
            continue
        s = inj[base:base + layers, :rows, :cols].astype(np.float64)
        L = np.zeros((rows, cols, 3))
        T = np.ones((rows, cols))
        for k in range(layers):
            L = L + T[..., None] * s[k, ..., :3]
            T = T * s[k, ..., 3]
            ref[base + k, :rows, :cols, :3] = L
            ref[base + k, :rows, :cols, 3] = T
        inside[base:base + layers, :rows, :cols] = True
    return ref, inside


def store16(x):
    """the RGBA16F store as both devices do it here: round toward zero (half_round_probe, 11-14: of the values where
    nearest-even and toward-zero differ, every alpha and all but 3 of ~265k rgb hold toward-zero, on GL and Vulkan alike)"""
    h = x.astype(np.float16)
    return np.where(np.abs(h.astype(np.float64)) > np.abs(x), np.nextafter(h, np.float16(0)), h).astype(np.float64)


def judge(x, ref, inside, half, sentinel):
    raw = ref[inside][:, 3]
    if half:
        ref = store16(ref)
    v = {}
    v['nonfinite'] = int((~np.isfinite(x)).sum())
    v['outside'] = int((x[~inside] != sentinel).any(axis=-1).sum())
    xi, ri = x[inside], ref[inside]
    lit = ri[:, :3] > 1e-3
    rel = np.abs(xi[:, :3] - ri[:, :3]) / np.maximum(ri[:, :3], 1e-3)
    v['lit'] = int(lit.sum())
    v['lit_ok'] = float((rel[lit] <= 0.01).mean()) if lit.any() else 0.0
    sx, sr = float(np.nansum(xi[:, :3])), float(ri[:, :3].sum())
    v['sum_rel'] = abs(sx - sr) / max(sr, 1e-30)
    # the 1e-4 is the arithmetic's (float32 vs float64); RGBA16F stores both ends of it as the format does
    lo, hi = (store16(raw - 1e-4), store16(raw + 1e-4)) if half else (raw - 1e-4, raw + 1e-4)
    da = np.abs(xi[:, 3] - ri[:, 3])
    v['alpha_bad'] = int((~((xi[:, 3] >= lo) & (xi[:, 3] <= hi))).sum())
    v['alpha_max'] = float(np.nanmax(da)) if da.size else 0.0
    v['ok'] = (v['nonfinite'] == 0 and v['outside'] == 0 and v['lit'] > 0 and v['lit_ok'] >= 0.99
               and v['sum_rel'] <= 0.01 and v['alpha_bad'] == 0)
    return v


def say(v):
    return ('lit %d, within 1 %%: %.4f %%; summed light off %.3g %%; alpha past bar %d (max %.3g); outside written %d; '
            'non-finite %d' % (v['lit'], 100 * v['lit_ok'], 100 * v['sum_rel'], v['alpha_bad'], v['alpha_max'],
                                v['outside'], v['nonfinite']))


ok = True
arms = sorted(d for d in glob.glob(os.path.join(OUT, 'a*')) if os.path.isfile(os.path.join(d, 'meta.txt')))
reds = 0
for d in arms:
    m = meta(os.path.join(d, 'meta.txt'))
    W, H, D = (int(t) for t in m['tex'])
    near = [int(t) for t in m['near']]
    far = [int(t) for t in m['far']]
    half = m['f32'][0] == '0'
    sentinel = float(m['sentinel'][0])
    shape = (D, H, W, 4)
    inj = np.fromfile(os.path.join(d, 'inject.bin'), np.float32).reshape(shape)
    ref, inside = rebuild(inj, near, far, D, H, W)
    for side in ('gl', 'vk'):
        v = judge(np.fromfile(os.path.join(d, side + '.bin'), np.float32).reshape(shape), ref, inside, half, sentinel)
        ok = ok and v['ok']
        print('FOGVIEW CHECK %s %s (%s, %s): %s -> %s' % (os.path.basename(d), side, 'RGBA16F' if half else 'RGBA32F',
                                                         m['set'][0], say(v), 'ok' if v['ok'] else 'FAIL'))
    for rp in sorted(glob.glob(os.path.join(d, 'vk_red*.bin'))):
        v = judge(np.fromfile(rp, np.float32).reshape(shape), ref, inside, half, sentinel)
        reds += 1
        ok = ok and not v['ok']
        print('%s %s must fail (%s): %s -> %s' % ('BAD' if v['ok'] else 'ok', os.path.basename(rp)[:-4],
                                                  os.path.basename(d), say(v), 'PASSED (gate defect)' if v['ok'] else 'failed'))
ok = ok and len(arms) == 5 and reds == 4
print('FOGVIEW CHECK: %d arms, %d red dumps' % (len(arms), reds))
print('FOGVIEW CHECK OK' if ok else 'FOGVIEW CHECK FAILED')
