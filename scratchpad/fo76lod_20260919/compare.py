#!/usr/bin/env python3
"""Measure FO76 .bto shapes, FO76 per-object _lod.nif, FO4 kit _lod.nif and
FO4 *_BldNNLOD.nif with the same statistics, and write compare.json.

Usage: compare.py <mode>   where mode is one of bto | f76lod | f4lod | f4bld | all
"""
import glob
import json
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/lodlevels_20260919')

import meshstats
import nif76
import pics_nifread as nif4

HERE = os.path.dirname(os.path.abspath(__file__))
F76LOD = 'E:/Projects/F76/Data/meshes/lod'
F4LOD = 'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/LOD'


def bto_shapes(limit_files=None):
    rows = []
    for f in sorted(glob.glob(os.path.join(HERE, 'bto', '*.bto'))):
        n = nif76.Nif76(f)
        base = os.path.basename(f)
        assert not n.problems, (base, n.problems[:3])
        for i in sorted(n.shapes):
            sh = n.shapes[i]
            if sh['numTris'] < 8:
                continue
            st = meshstats.stats(sh['verts'], sh['tris'], sh['uvs'] or None,
                                 name='%s#%d %s' % (base, i, sh['name']))
            st['file'] = base
            st['shape'] = sh['name']
            st['kind'] = ('remesh' if sh['name'].startswith('RemeshedShape')
                          else 'atlas' if sh['name'].startswith('GlobalAtlasShape')
                          else 'other')
            st['segs'] = len(sh['segs']) if sh['segs'] else 0
            rows.append(st)
            print('  %-58s nv=%-6d nt=%-6d AR90=%.2f axisN=%.1f%%'
                  % (st['name'][:58], st['nv'], st['nt'],
                     st.get('AR_p90', 0), st.get('axis_normal_pct', 0)))
    return rows


def sample_nifs(root, pattern, n, seed, reader, tag):
    files = sorted(glob.glob(os.path.join(root, '**', pattern), recursive=True))
    random.Random(seed).shuffle(files)
    rows = []
    used = 0
    for f in files:
        if used >= n:
            break
        try:
            m = reader(f)
        except Exception as e:
            continue
        shapes = m.shapes
        if not shapes:
            continue
        for i in sorted(shapes):
            sh = shapes[i]
            if sh['numTris'] < 8:
                continue
            st = meshstats.stats(sh['verts'], sh['tris'], sh['uvs'] or None,
                                 name='%s#%d' % (os.path.relpath(f, root).replace('\\', '/'), i))
            st['file'] = os.path.relpath(f, root).replace('\\', '/')
            st['kind'] = tag
            st['stride'] = sh['stride']
            rows.append(st)
        used += 1
    print('  %s: %d files sampled of %d, %d shapes' % (tag, used, len(files), len(rows)))
    return rows


def agg(rows, keys):
    import numpy as np
    out = {}
    for k in keys:
        v = [r[k] for r in rows if k in r and r[k] is not None]
        if v:
            out[k] = dict(n=len(v), median=float(np.median(v)),
                          p10=float(np.percentile(v, 10)),
                          p90=float(np.percentile(v, 90)))
    out['_shapes'] = len(rows)
    out['_tris'] = int(sum(r['nt'] for r in rows))
    return out


KEYS = ('AR_median', 'AR_p90', 'AR_p99', 'sliver_pct_AR4', 'sliver_pct_AR10',
        'degenerate_pct', 'axis_normal_pct', 'axis_normal_areapct',
        'axis_edge_pct', 'edge_p90_over_p10', 'valence_mean', 'valence_std',
        'valence_pct_le4', 'valence_pct_5to7', 'valence_entropy',
        'snap_pct_1', 'snap_pct_4', 'snap_pct_16',
        'uv_islands', 'texdens_p90_over_p10', 'components_3d')


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else 'all'
    out = {}
    if mode in ('bto', 'all'):
        print('== FO76 .bto shapes ==')
        out['bto'] = bto_shapes()
    if mode in ('f76lod', 'all'):
        print('== FO76 per-object Meshes/LOD ==')
        out['f76lod'] = sample_nifs(F76LOD, '*_lod.nif', 160, 11,
                                    lambda p: nif76.Nif76(p), 'f76_objlod')
    if mode in ('f4lod', 'all'):
        print('== FO4 kit _lod.nif ==')
        out['f4lod'] = sample_nifs(F4LOD, '*_LOD.nif', 160, 11,
                                   lambda p: nif4.Nif(p), 'f4_kitlod')
    if mode in ('f4bto', 'all'):
        print('== FO4 .bto (the merge-of-authored-LOD control) ==')
        rows = []
        for f in ['Commonwealth.4.-16.-16.BTO', 'Commonwealth.8.-16.-16.BTO',
                  'Commonwealth.16.-16.-16.BTO', 'Commonwealth.32.-32.-32.BTO',
                  'Commonwealth.4.0.0.BTO']:
            p = os.path.join('E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Terrain/Commonwealth/Objects', f)
            if not os.path.exists(p):
                print('   missing %s' % f)
                continue
            m = nif4.Nif(p)
            for i in sorted(m.shapes):
                sh = m.shapes[i]
                if sh['numTris'] < 8:
                    continue
                st = meshstats.stats(sh['verts'], sh['tris'], sh['uvs'] or None,
                                     name='%s#%d %s' % (f, i, sh['name']))
                st['file'] = f
                st['shape'] = sh['name']
                st['kind'] = 'f4_bto'
                st['tex'] = m.diffuse_for(sh)
                rows.append(st)
                print('  %-52s nv=%-6d nt=%-6d yawbox=%.1f%% tex=%s'
                      % (st['name'][:52], st['nv'], st['nt'],
                         st.get('yawinv_box_pct', 0), st['tex']))
        out['f4bto'] = rows
    if mode in ('f4bld', 'all'):
        print('== FO4 *BldNNLOD.nif ==')
        out['f4bld'] = sample_nifs(F4LOD, 'Bld*LOD.nif', 160, 11,
                                   lambda p: nif4.Nif(p), 'f4_bld')
    with open(os.path.join(HERE, 'compare_%s.json' % mode), 'w') as fh:
        json.dump(out, fh, indent=1)
    print()
    for k, rows in out.items():
        if k == 'bto':
            for sub in ('remesh', 'atlas'):
                sel = [r for r in rows if r['kind'] == sub]
                if sel:
                    print('--- bto/%s' % sub)
                    print(json.dumps(agg(sel, KEYS), indent=1))
        else:
            print('--- %s' % k)
            print(json.dumps(agg(rows, KEYS), indent=1))


if __name__ == '__main__':
    main()
