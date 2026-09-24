"""Work item 2: what the road meshes actually carry.

For every distinct Landscape\\Roads or Landscape\\Sidewalks model placed in the
Sanctuary window, per SHAPE: vertex alpha present and its range, the material's
alpha BLEND flag vs alpha TEST, bDecal, two-sided, and the NiAlphaProperty's
own flags / threshold.

Reads only Bethesda's shipped files plus ROADS1's placement dump.  Writes
scratchpad/roads2_20260911/meshflags.json and a table on stdout.
"""

import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'roads1_20260911'))

import roads2lib                                        # noqa: E402
from roads2lib import Nif, vertex_colors, alpha_property, \
    shape_vertex_data_offset, read_material_full         # noqa: E402
import matinfo                                           # noqa: E402
from rasterlib import MeshCache, local_to_model          # noqa: E402
import placements                                        # noqa: E402

DATA = r'E:\Tools\Fallout 4\DataUnpacked\Data'
REFS = os.path.join(HERE, '..', 'roads1_20260911', 'sanctuary_refs.json')
SEP = chr(92)


def is_roadish(sig, modl):
    c = placements.norm_components(modl)
    return (sig == 'STAT' and len(c) >= 3 and c[0] == 'landscape'
            and c[1] in ('roads', 'sidewalks'))


def model_path(modl):
    p = (modl or '').replace(SEP, '/').lstrip('/')
    if not p.lower().startswith('meshes/'):
        p = 'meshes/' + p
    mc = MeshCache(DATA)
    return mc.path_for(modl)


def main():
    pl = placements.load(REFS)
    roads = [p for p in pl if is_roadish(p['sig'], p['modl'])]
    models = {}
    for p in roads:
        models.setdefault(p['modl'].lower(), p['modl'])
    print('road/sidewalk placements in the window: %d over %d distinct models'
          % (len(roads), len(models)))

    rows = []
    for key in sorted(models):
        modl = models[key]
        full = model_path(modl)
        if full is None or not os.path.isfile(full):
            rows.append(dict(model=modl, shape=None, error='model not found'))
            continue
        nif = Nif(full)
        for idx, sh in nif.shapes.items():
            if not sh['verts'] or not sh['tris']:
                continue
            info = matinfo.shader_info(nif, sh)
            matname = info[0] if info else ''
            f1 = info[1] if info else 0
            mpath = matinfo.find_material(DATA, matname)
            mat = read_material_full(mpath) if mpath else None
            _, apref = shape_vertex_data_offset(nif, sh)
            ap = alpha_property(nif, apref)
            col = vertex_colors(nif, sh)
            r = dict(model=modl, shape=sh['name'], block=idx,
                     verts=sh['numVerts'], tris=sh['numTris'],
                     matname=matname, f1=f1, f1_decal=bool(f1 & (1 << 26)),
                     f1_dyndecal=bool(f1 & (1 << 27)),
                     f1_vertexalpha=bool(f1 & (1 << 3)),
                     f1_landscape=bool(f1 & (1 << 14)),
                     has_alphaprop=ap is not None,
                     ap_flags=(ap[0] if ap else None),
                     ap_thr=(ap[1] if ap else None),
                     ap_blend=(bool(ap[0] & 0x0001) if ap else False),
                     ap_test=(bool(ap[0] & 0x0200) if ap else False),
                     mat_found=mat is not None)
            if mat:
                r.update(mat_alpha=mat['alpha'], mat_blend=mat['alphaBlend'],
                         mat_test=mat['alphaTest'], mat_thr=mat['alphaTestRef'],
                         mat_decal=mat['decal'], mat_twoSided=mat['twoSided'],
                         mat_kind=mat['kind'])
            if col is None:
                r.update(vcol=False)
            else:
                a = col[:, 3]
                r.update(vcol=True, a_min=float(a.min()), a_max=float(a.max()),
                         a_mean=float(a.mean()),
                         a_frac_below_0_9=float((a < 0.9).mean()),
                         a_frac_below_0_1=float((a < 0.1).mean()),
                         rgb_min=float(col[:, :3].min()),
                         rgb_max=float(col[:, :3].max()))
                # does the alpha fall towards the shape's own XY boundary?
                R, t, s = local_to_model(nif, sh)
                v = (np.array(sh['verts'], dtype=np.float64) * s).dot(R.T) + t
                xy = v[:, :2]
                lo, hi = xy.min(axis=0), xy.max(axis=0)
                span = np.maximum(hi - lo, 1e-6)
                # normalised distance to the nearest of the four bbox sides
                d = np.minimum((xy - lo) / span, (hi - xy) / span).min(axis=1)
                if a.std() > 1e-6 and d.std() > 1e-6:
                    r.update(alpha_vs_edge_corr=float(np.corrcoef(a, d)[0, 1]))
                else:
                    r.update(alpha_vs_edge_corr=None)
            rows.append(r)

    out = os.path.join(HERE, 'meshflags.json')
    json.dump(rows, open(out, 'w'), indent=1)
    print('wrote %s, %d shape rows' % (out, len(rows)))

    # ---- the summary the report quotes -------------------------------
    sh = [r for r in rows if r.get('shape') is not None]
    print('')
    print('shapes: %d ; with vertex colours: %d ; material read: %d'
          % (len(sh), sum(1 for r in sh if r.get('vcol')),
             sum(1 for r in sh if r.get('mat_found'))))
    print('material bAlphaBlend true: %d ; bAlphaTest true: %d ; bDecal true: %d'
          % (sum(1 for r in sh if r.get('mat_blend')),
             sum(1 for r in sh if r.get('mat_test')),
             sum(1 for r in sh if r.get('mat_decal'))))
    print('NiAlphaProperty present: %d (blend bit %d, test bit %d)'
          % (sum(1 for r in sh if r.get('has_alphaprop')),
             sum(1 for r in sh if r.get('ap_blend')),
             sum(1 for r in sh if r.get('ap_test'))))
    vc = [r for r in sh if r.get('vcol')]
    ramp = [r for r in vc if r.get('a_min', 1.0) < 0.9]
    print('vertex-alpha shapes whose alpha reaches below 0.9: %d of %d'
          % (len(ramp), len(vc)))
    if ramp:
        print('  their a_min range %.3f..%.3f, a_mean range %.3f..%.3f'
              % (min(r['a_min'] for r in ramp), max(r['a_min'] for r in ramp),
                 min(r['a_mean'] for r in ramp), max(r['a_mean'] for r in ramp)))
        cc = [r['alpha_vs_edge_corr'] for r in ramp
              if r.get('alpha_vs_edge_corr') is not None]
        if cc:
            print('  alpha-vs-distance-to-bbox-edge correlation: median %.3f '
                  'over %d shapes (positive = alpha falls at the edge)'
                  % (float(np.median(cc)), len(cc)))
    print('shader flag Vertex_Alpha (F4SF1 bit 3) set: %d of %d shapes; '
          'of the %d vertex-alpha-ramped ones: %d'
          % (sum(1 for r in sh if r.get('f1_vertexalpha')), len(sh),
             len(ramp), sum(1 for r in ramp if r.get('f1_vertexalpha'))))
    print('shader flag Decal (bit 26) set: %d ; Landscape (bit 14) set: %d'
          % (sum(1 for r in sh if r.get('f1_decal')),
             sum(1 for r in sh if r.get('f1_landscape'))))
    print('ramped shapes by material alpha-test state: test %d, no test %d'
          % (sum(1 for r in ramp if r.get('mat_test') or r.get('ap_test')),
             sum(1 for r in ramp if not (r.get('mat_test') or r.get('ap_test')))))
    print('')
    print('the 25 most strongly ramped shapes (lowest a_mean):')
    for r in sorted(ramp, key=lambda x: x['a_mean'])[:25]:
        print('  %-52s %-24s a %.2f..%.2f mean %.2f  vaflag %s test %s decal %s'
              % (r['model'].replace(SEP, '/')[-52:], (r['shape'] or '')[:24],
                 r['a_min'], r['a_max'], r['a_mean'],
                 'Y' if r.get('f1_vertexalpha') else 'n',
                 'Y' if (r.get('mat_test') or r.get('ap_test')) else 'n',
                 'Y' if (r.get('mat_decal') or r.get('f1_decal')) else 'n'))
    print('')
    print('the shapes that are BOTH blended and vertex-alpha-ramped:')
    n = 0
    for r in sh:
        blended = r.get('mat_blend') or r.get('ap_blend')
        if blended and r.get('vcol') and r.get('a_min', 1.0) < 0.9:
            n += 1
            if n <= 25:
                print('  %-58s %-26s a %.2f..%.2f mean %.2f'
                      % (r['model'].replace(SEP, '/')[-58:], r['shape'][:26],
                         r['a_min'], r['a_max'], r['a_mean']))
    print('  total %d' % n)


if __name__ == '__main__':
    main()
