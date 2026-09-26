"""ROADS1: the pavement (and road) bases placed in the Boston bake box, read independently.
Per base: model, count, shapes, material, diffuse, the diffuse's whole-texture mean (gamma space, 0..255 luma),
vertex-colour stream / SLSF2 Vertex_Colors flag / mean vertex colour, UV scale/offset (NIF and BGSM),
and how many placements carry a material swap. Pickles the placements for the next scripts."""
import collections
import os
import pickle
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import roadgeo as rg  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'out')
os.makedirs(OUT, exist_ok=True)


def luma(c):
    return 255.0 * (0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2])


def main():
    R = rg.Reader()
    print('plugins', len(R.plugins), 'refs', len(R.W.refs))
    pl = list(rg.placements(R, -10, -14, 5, 1))
    print('placements in box+2', len(pl))
    keep = []
    per = collections.defaultdict(lambda: {'n': 0, 'xmsp': 0, 'dec': collections.Counter()})
    for d in pl:
        bi = d['info']
        if not bi or bi['type'] != 'STAT' or not rg.is_road(bi['modl']):
            continue
        dec = rg.road_decision(d)
        e = per[bi['modl'].lower()]
        e['n'] += 1
        e['xmsp'] += 1 if d['xmsp'] else 0
        e['dec'][dec] += 1
        e['sidewalk'] = rg.is_sidewalk(bi['modl'])
        e['hasLod'] = bi['hasLod']
        keep.append({k: d[k] for k in ('ref', 'refFlags', 'plugin', 'origin', 'xmsp', 'cx', 'cy', 'part', 'base',
                                         'pos', 'rot', 'scale', 'refBase', 'refType')}
                    | {'modl': bi['modl'], 'hasLod': bi['hasLod'], 'decision': dec, 'mods': bi['mods']})
    with open(os.path.join(OUT, 'road_placements.pkl'), 'wb') as f:
        pickle.dump(keep, f)
    print('road/pavement placements', len(keep))
    lines = []
    sw_n = sum(e['n'] for e in per.values() if e['sidewalk'])
    for modl, e in sorted(per.items(), key=lambda kv: -kv[1]['n']):
        shapes = R.model(modl) or []
        for s in shapes:
            m = R.material(s['mat']) if s['mat'] else None
            tex = (m['tex0'] if m and m['tex0'] else s['tex0'])
            t = R.texture(tex)
            mean = luma(t.mean) if t else -1
            colm = (np.mean(s['col'][:, :3]) * 255 if s['col'] is not None else -1)
            lines.append('%s\t%d\t%s\t%d\t%s\tblk%d\t%s\t%s\t%.1f\tcolStream=%d vcFlag=%d colMean=%.1f\tnifUV=%s/%s\tbgsmUV=%s/%s\t%s\t%s' % (
                'SW' if e['sidewalk'] else 'RD', e['n'], dict(e['dec']), e['xmsp'], modl, s['block'], s['mat'], tex,
                mean, s['hascol'], s['vc'], colm,
                tuple(round(x, 3) for x in s['uvOff']), tuple(round(x, 3) for x in s['uvScale']),
                (tuple(round(x, 3) for x in m['uvOff']) if m else '-'), (tuple(round(x, 3) for x in m['uvScale']) if m else '-'),
                ('test%d/%d blend%d decal%d' % (m['alphaTest'], m['alphaRef'], m['blend'], m['decal'])) if m else 'nobgsm',
                'alphaProp%04x' % s['alphaFlags'] if s['hasAlpha'] else ''))
        if not shapes:
            lines.append('%s\t%d\t%s\t%d\t%s\t(no shapes: %s)' % ('SW' if e['sidewalk'] else 'RD', e['n'], dict(e['dec']),
                                                               e['xmsp'], modl, 'missing' if shapes is None else 'empty'))
    with open(os.path.join(OUT, 'pave_census.tsv'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print('sidewalk placements', sw_n, 'bases', sum(1 for e in per.values() if e['sidewalk']))
    print('xmsp on sidewalks', sum(e['xmsp'] for e in per.values() if e['sidewalk']),
          'on roads', sum(e['xmsp'] for e in per.values() if not e['sidewalk']))


if __name__ == '__main__':
    main()
