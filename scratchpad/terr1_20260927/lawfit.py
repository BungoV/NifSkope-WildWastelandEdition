"""TERR1 continuation: score sky laws against the physical cosine-weighted cast (physlaw.json), no re-cast.

Per direction the law's inputs (physlaw.py): terrain maxSlope `ms`, object lattice `wall`, ceiling opening `copen`
(`have` = a ceiling was seen). Laws (byte = floor(255 vis + 0.5)):
  shipped  vis = 1 - 1.6/8 * sum min(1, F(max(ms, wall)) + [have](1 - F(copen)))        F(t) = t/(1+t)
  cos      vis = visT - 1/8 * sum max(0, Pu - PT)                                       P(t) = t^2/(1+t^2) = sin^2 elev
           Pu = min(1, P(max(ms, wall)) + [have] 1/(1+copen^2)), PT = P(ms);  visT = the terrain march's own byte law
           (1 - 1.6/8 sum F(ms)); so ground with no object above its terrain horizon keeps the terrain byte exactly
  cosall   vis = 1 - 1/8 * sum Pu   (the cosine law for the terrain too: moves open ground -- reported, not a candidate)
usage: python lawfit.py physlaw.json [ref: phys1458|phys10k]
"""
import sys, json
import numpy as np


def F(t):
    return t / (1 + t)


def P(t):
    return t * t / (1 + t * t)


def law(r, name):
    ms, wall, co, hv = (np.array(r[k], float) for k in ('ms', 'wall', 'copen', 'have'))
    wu = np.maximum(ms, wall)
    visT = np.clip(1 - 1.6 / 8 * F(ms).sum(), 0, 1)
    if name == 'terrain':
        v = visT
    elif name == 'shipped':
        v = np.clip(1 - 1.6 / 8 * np.minimum(1, F(wu) + np.where(hv, 1 - F(co), 0)).sum(), 0, 1)
    elif name == 'cos':
        pu = np.minimum(1, P(wu) + np.where(hv, 1 / (1 + co * co), 0))
        v = np.clip(visT - np.maximum(0, pu - P(ms)).sum() / 8, 0, 1)
    elif name == 'cosall':
        pu = np.minimum(1, P(wu) + np.where(hv, 1 / (1 + co * co), 0))
        v = np.clip(1 - pu.sum() / 8, 0, 1)
    return np.floor(v * 255 + 0.5)


def main():
    R = json.load(open(sys.argv[1]))['rows']
    ref = sys.argv[2] if len(sys.argv) > 2 else 'phys1458'
    out = {}
    for name in ('terrain', 'shipped', 'cos', 'cosall'):
        row = {}
        for cls in ('canyon', 'open', 'near', 'deck', 'all'):
            s = [r for r in R if cls == 'all' or r['cls'] == cls]
            a = np.array([law(r, name) for r in s]); p = np.array([r[ref] for r in s])
            row[cls] = dict(n=len(s), mean=round(float(a.mean()), 1), ref=round(float(p.mean()), 1),
                            mae=round(float(np.abs(a - p).mean()), 1), bias=round(float((a - p).mean()), 1),
                            corr=round(float(np.corrcoef(a, p)[0, 1]), 3) if a.std() > 0 else None)
        out[name] = row
        print(name)
        for cls, v in row.items():
            print('   %-6s n %3d  law %6.1f  ref %6.1f  MAE %5.1f  bias %+6.1f  corr %s' % (
                cls, v['n'], v['mean'], v['ref'], v['mae'], v['bias'], v['corr']))
    # named streets, canyon means
    print('named canyon (law cos / shipped / baked mask B / ref)')
    for cell in sorted({r['cell'] for r in R if r['cls'] == 'canyon'}):
        s = [r for r in R if r['cell'] == cell]
        print('   %-6s %-28s cos %5.1f  shipped %5.1f  baked %5.1f  %s %5.1f' % (
            cell, s[0]['name'], np.mean([law(r, 'cos') for r in s]), np.mean([law(r, 'shipped') for r in s]),
            np.mean([r['maskB_after'] for r in s]), ref, np.mean([r[ref] for r in s])))
    # the shipped-law replay must reproduce the baked byte (self-check of the inputs)
    d = np.array([law(r, 'shipped') - r['maskB_after'] for r in R])
    print('self-check shipped replay vs baked mask B: |d| mean %.2f, max %.0f, share |d|<=2 %.3f' % (
        np.abs(d).mean(), np.abs(d).max(), (np.abs(d) <= 2).mean()))
    json.dump(out, open(sys.argv[1].replace('.json', '_fit_%s.json' % ref), 'w'), indent=1)


if __name__ == '__main__':
    main()
