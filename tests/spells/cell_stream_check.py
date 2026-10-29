"""lane CELLSTREAM1: the checker for tests/spells/cell_stream.sh. Reads only the files the runs wrote.

usage: python cell_stream_check.py <out folder>     exit 0 = every gate and every red as required
"""
import os
import sys

import numpy as np
from PIL import Image

FRAME_MS = 8.0          # a streaming frame's main-thread work: the 4 ms write budget plus one bucket's overrun
MEM_TOL = 0.05          # private bytes after crossing 20 vs the first full load
PIC_MEAN = 3.0          # streamed vs fixed picture: mean absolute difference, 0..255
PIC_NEAR = 0.97         # and the share of pixels within 8 / 255 on every channel


def steps(path):
    rows = []
    if not os.path.exists(path):
        return rows
    for line in open(path, encoding='utf-8', errors='replace'):
        if not line.startswith('step '):
            continue
        r = {}
        for part in line.strip().split(' | '):
            w = part.split()
            if not w:
                continue
            if w[0] in ('expected', 'loaded', 'error'):
                r[w[0]] = ' '.join(w[1:])
                continue
            for i in range(0, len(w) - 1, 2):
                r[w[i]] = w[i + 1]
            if w[0] == 'step':
                r['step'] = int(w[1])
        rows.append(r)
    return rows


def num(r, k, d=0.0):
    try:
        return float(r.get(k, d))
    except ValueError:
        return d


def main():
    out = sys.argv[1]
    fails = []
    oks = []

    def gate(name, ok, detail):
        print(('  PASS  ' if ok else '  FAIL  ') + name + ': ' + detail)
        (oks if ok else fails).append(name)

    walk = steps(os.path.join(out, 'walk.stream.txt'))
    crossings = [r for r in walk if r['step'] >= 1 and r['step'] <= 20]
    # (a)
    if len(walk) < 7:
        gate('(a) loaded set', False, '%d steps in the walk dump (need 7+)' % len(walk))
    else:
        bad = [r['step'] for r in walk if r.get('match') != '1']
        gate('(a) loaded set == expected 5x5', not bad,
             '%d steps, %d crossings, mismatched steps %s' % (len(walk), len(crossings), bad or 'none'))
        # the crossings are what streams; step 0 is the first fill from nothing, reported beside it
        cr = crossings or walk
        tick = max(num(r, 'tickMaxMs') for r in cr)
        slc = max(num(r, 'sliceMaxMs') for r in cr)
        unl = max(num(r, 'unloadMaxMs') for r in cr)
        reb = max(num(r, 'rebuildMaxMs') for r in cr)
        bur = max(num(r, 'buryMaxMs') for r in cr)
        bld = max(num(r, 'buildMaxMs') for r in walk)
        first = num(walk[0], 'tickMaxMs')
        # lane FRAMESPEED2 (D2): the bar holds the stream's own frame work, i.e. its dearest frame ON the processor
        # (cpuMaxMs, as streamwalk's CPU-VERDICT). The wall figure also counts the machine's pre-emption and paging:
        # SHOTPASS1's 14.21 ms was step 15's frame at 0.05 ms on the processor, 14.17 ms of it before any stream
        # work. A dump without cpuMaxMs (before FRAMESPEED2) is judged on the wall as before.
        cpu = [num(r, 'cpuMaxMs', -1.0) for r in cr]
        if min(cpu) >= 0:
            cw = max(cpu)
            ws = max(cr, key=lambda r: num(r, 'tickMaxMs'))
            stall = num(ws, 'wtMs') - num(ws, 'wtCpu')
            gate('(a) frame max <= %.0f ms on every crossing (on the processor)' % FRAME_MS, cw <= FRAME_MS,
                 'worst crossing cpu frame %.2f ms; wall worst %.2f ms at step %d (%.2f ms of it off the processor) '
                 '(write slice %.2f, unload %.2f, node frees %.2f, tables/lights %.2f); first fill %.2f ms; worker build '
                 'max %.0f ms' % (cw, tick, ws['step'], stall, slc, unl, bur, reb, first, bld))
        else:
            gate('(a) frame max <= %.0f ms on every crossing' % FRAME_MS, tick <= FRAME_MS,
                 'worst crossing frame %.2f ms (write slice %.2f, unload %.2f, node frees %.2f, tables/lights %.2f); '
                 'first fill %.2f ms; worker build max %.0f ms' % (tick, slc, unl, bur, reb, first, bld))
    # (c)
    if walk:
        cov = [r for r in walk if num(r, 'samples') > 0]
        holes = sum(int(num(r, 'holes')) for r in walk)
        dbl = sum(int(num(r, 'doubles')) for r in walk)
        smp = sum(int(num(r, 'samples')) for r in walk)
        gate('(c) far LOD: no hole, no double', bool(cov) and holes == 0 and dbl == 0,
             '%d samples over %d steps: %d holes, %d doubles' % (smp, len(cov), holes, dbl))
    # (d)
    s0 = [r for r in walk if r['step'] == 0]
    s20 = [r for r in walk if r['step'] == 20]
    if s0 and s20:
        p0, p20 = num(s0[0], 'private'), num(s20[0], 'private')
        ratio = p20 / p0 if p0 else 0.0
        peak = max(num(r, 'private') for r in walk)
        gate('(d) memory flat (within %d%%)' % round(MEM_TOL * 100), p0 > 0 and abs(ratio - 1.0) <= MEM_TOL,
             'first load %.0f MB, after crossing 20 %.0f MB (x%.3f), peak %.0f MB; blocks %s -> %s'
             % (p0 / 2**20, p20 / 2**20, ratio, peak / 2**20, s0[0].get('blocks'), s20[0].get('blocks')))
    else:
        gate('(d) memory flat', False, 'the walk dump has no step 0 or step 20')

    # (e) lane STREAMTEX1: the texture pool's census, one per crossing (deduped by texCrossing). A loaded cell's
    # texture is never freed (texKeepFreed 0), and a crossing reloads (freed and loaded again within 2 crossings) no
    # more than the 2 crossings before it released past the budget: the same bar as tests/streamtex (row c)
    cen = {}
    for r in walk:
        if 'texCrossing' in r:
            cen[int(num(r, 'texCrossing'))] = r
    if cen:
        red = any(num(r, 'texRed') > 0 for r in cen.values())
        keys = sorted(cen)
        over = []
        for k in keys:
            rel = sum(num(cen[j], 'texReleasedMb') for j in (k - 1, k - 2) if j in cen)
            room = max(0.0, rel - num(cen[k], 'texBudgetMb'))
            if num(cen[k], 'texReloadedMb') > room + 0.05:
                over.append('%d: %.1f MB > %.1f' % (k, num(cen[k], 'texReloadedMb'), room))
        kf = sum(int(num(r, 'texKeepFreed')) for r in cen.values())
        rl = sum(num(r, 'texReloadedMb') for r in cen.values())
        kept = max(num(r, 'texKeptMb') for r in cen.values())
        gate('(e) texture pool: no loaded cell\'s texture freed, no reload past the budget%s' % (' (RED off)' if red else ''),
             red or (kf == 0 and not over),
             '%d crossings, keep-freed %d, reloaded %.1f MB, pool peak %.1f MB of %.0f, crossings past the bar %s'
             % (len(keys), kf, rl, kept, num(cen[keys[0]], 'texBudgetMb'), over or 'none'))
    else:
        print('  SKIP  (e) texture pool: the dump has no texCrossing (an exe before STREAMTEX1)')

    # reds
    nu = steps(os.path.join(out, 'red_nounload.stream.txt'))
    if len(nu) >= 2:
        p0, pl = num(nu[0], 'private'), num(nu[-1], 'private')
        ratio = pl / p0 if p0 else 0.0
        gate('red nounload fails (d)', abs(ratio - 1.0) > MEM_TOL,
             'first load %.0f MB, after crossing %d %.0f MB (x%.3f)' % (p0 / 2**20, nu[-1]['step'], pl / 2**20, ratio))
    else:
        gate('red nounload fails (d)', False, 'no red_nounload dump')

    # (b)
    a, b = os.path.join(out, 'streamed.png'), os.path.join(out, 'fixed.png')
    if os.path.exists(a) and os.path.exists(b):
        A = np.asarray(Image.open(a).convert('RGB')).astype(np.int16)
        B = np.asarray(Image.open(b).convert('RGB')).astype(np.int16)
        if A.shape != B.shape:
            gate('(b) picture == fixed open', False, 'sizes differ %s vs %s' % (A.shape, B.shape))
        else:
            d = np.abs(A - B)
            mean = float(d.mean())
            near = float((d.max(axis=2) <= 8).mean())
            gate('(b) picture == fixed open', mean <= PIC_MEAN and near >= PIC_NEAR,
                 'mean |diff| %.2f / 255 (<= %.1f), %.2f%% of pixels within 8 (>= %.0f%%)'
                 % (mean, PIC_MEAN, near * 100, PIC_NEAR * 100))
        for p in (a, b):
            P = np.asarray(Image.open(p).convert('RGB')).astype(np.int16)
            mag = float(((P[:, :, 0] > 200) & (P[:, :, 2] > 200) & (P[:, :, 1] < 60)).mean())
            gate('no magenta in %s' % os.path.basename(p), mag < 1e-4, '%.4f%% magenta pixels' % (mag * 100))
    else:
        gate('(b) picture == fixed open', False, 'missing %s' % (a if not os.path.exists(a) else b))

    print('  %d pass, %d fail' % (len(oks), len(fails)))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
