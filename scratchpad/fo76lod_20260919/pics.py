#!/usr/bin/env python3
"""Produce the picture evidence into images/.

  python pics.py probe     -- list candidate meshes with tri counts
  python pics.py make      -- render the two sheets
"""
import glob
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/lodlevels_20260919')

import meshstats
import nif76
import pics_nifread as nif4
import render

IMG = os.path.join(HERE, 'images')
BTO16 = os.path.join(HERE, 'bto', 'appalachia.16.-14.-29.bto')
BTO4 = os.path.join(HERE, 'bto', 'appalachia.4.-14.-29.bto')
F4 = 'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/LOD'


def merge(shapes):
    """Concatenate a list of shape dicts into one (verts, tris)."""
    V, T, off = [], [], 0
    for sh in shapes:
        v = np.asarray(sh['verts'], dtype=np.float64)
        t = np.asarray(sh['tris'], dtype=np.int64).reshape(-1, 3)
        V.append(v)
        T.append(t + off)
        off += len(v)
    return np.vstack(V), np.vstack(T)


def load76(path, block=None, namepart=None):
    n = nif76.Nif76(path)
    if block is not None:
        return merge([n.shapes[block]])
    sel = [n.shapes[i] for i in sorted(n.shapes)
           if namepart.lower() in (n.shapes[i]['name'] or '').lower()]
    if not sel:
        raise SystemExit('no shape matching %r in %s' % (namepart, path))
    return merge(sel)


def load4(path):
    m = nif4.Nif(path)
    return merge([m.shapes[i] for i in sorted(m.shapes) if m.shapes[i]['numTris'] > 0])


def cluster(V, T, radius, around=None):
    """Keep the triangles inside a ball of `radius` around the centroid of the
    biggest connected piece (or around an explicit point)."""
    import atlas
    comp, ncomp = atlas.components(V, T)
    if around is None:
        sz = np.bincount(comp, minlength=ncomp)
        big = int(np.argmax(sz))
        around = V[comp == big].mean(axis=0)
    d = np.linalg.norm(V[:, :2] - np.asarray(around)[:2], axis=1)
    keep = d <= radius
    tk = keep[T].all(axis=1)
    idx = np.unique(T[tk])
    remap = -np.ones(len(V), dtype=np.int64)
    remap[idx] = np.arange(len(idx))
    return V[idx], remap[T[tk]], around


def probe():
    n = nif76.Nif76(BTO4)
    print('== %s ==' % os.path.basename(BTO4))
    for i in sorted(n.shapes):
        sh = n.shapes[i]
        if sh['name'] and sh['name'].startswith('RemeshedShape'):
            print('  %3d %-72s nv=%-6d nt=%d' % (i, sh['name'][:72], sh['numVerts'], sh['numTris']))
    for pat in ('Buildings/Bld0*_LOD.nif', 'Neighborhoods/BackBay/BackBay0*_Bld0*LOD.nif',
                'Neighborhoods/*/*_Bld0*LOD.nif'):
        print('== %s ==' % pat)
        for f in sorted(glob.glob(os.path.join(F4, pat)))[:400]:
            try:
                V, T = load4(f)
            except Exception as e:
                continue
            d = V.max(axis=0) - V.min(axis=0)
            print('  %-62s nv=%-6d nt=%-5d bbox %.0f x %.0f x %.0f'
                  % (os.path.relpath(f, F4).replace('\\', '/')[:62], len(V), len(T), d[0], d[1], d[2]))


def make():
    os.makedirs(IMG, exist_ok=True)
    out = []

    # ---- sheet 1: the level-16 merged chunk shell, two angles ------------
    V, T = load76(BTO16, block=30)
    Vc, Tc, ctr = cluster(V, T, 620.0, around=(3150.0, 4000.0, 0.0))
    sc = meshstats.stats(Vc, Tc)
    pans = []
    for az, el, tag in ((40.0, 22.0, 'azimuth 40 deg, elevation 22 deg'),
                        (220.0, 14.0, 'azimuth 220 deg, elevation 14 deg')):
        im = render.trim(render.render(Vc, Tc, W=1100, H=1100, azim=az, elev=el, fov=26.0))
        pans.append((im, ['FO76  appalachia.16.-14.-29.bto  RemeshedShape_NotAlphaTested_Merged',
                          'one building group cut out of the chunk sheet: %d of its %d triangles, valence %.2f'
                          % (len(Tc), len(T), sc['valence_mean']),
                          tag]))
    out.append(render.sheet(pans, os.path.join(IMG, '01_fo76_chunk16_remesh_two_angles.png'),
                            title='FO76 level-16 object LOD, one machine-remeshed sheet per chunk (wireframe over flat grey)',
                            panel_w=860, panel_h=860, cap_h=92))
    im = render.trim(render.render(V, T, W=1400, H=1400, azim=40, elev=38, fov=26.0))
    out.append(render.sheet([(im, ['FO76  appalachia.16.-14.-29.bto  RemeshedShape_NotAlphaTested_Merged, whole chunk',
                                   '%d triangles, %d vertices, %d separate pieces in ONE shape - every structure in a'
                                   % (len(T), len(V), 141),
                                   '16-cell square, remeshed and packed into one 512x256 baked atlas'])],
                            os.path.join(IMG, '01b_fo76_chunk16_whole.png'),
                            title='The same shape, whole chunk (the cluster above sits in the middle)',
                            panel_w=1180, panel_h=1180, cap_h=92))

    # ---- sheet 2: one FO76 building vs FO4 authored LOD ------------------
    V1, T1 = load76(BTO4, namepart='HouseShotgun_TwoStory_03')
    f2 = os.path.join(F4, 'Buildings', 'Bld02FrontBrickCom01_LOD.nif')
    f3 = os.path.join(F4, 'Neighborhoods', 'Cambridge', 'Cambridge46_Bld01LOD.nif')
    V2, T2 = load4(f2)
    V3, T3 = load4(f3)

    def line(V, T):
        st = meshstats.stats(V, T)
        return ('%d tris | %d piece(s) | slivers AR>10 %.1f%% | valence %.2f | boxy %.0f%% of area'
                % (len(T), st['components_3d'], st['sliver_pct_AR10'],
                   st['valence_mean'], st['yawinv_box_areapct']))
    pans = [
        (render.trim(render.render(V1, T1, W=1000, H=1000, azim=35, elev=22, fov=26)),
         ['FO76 GENERATED  RemeshedShape_BLD_A_Siding_HouseShotgun_TwoStory_03',
          '        ..._LOD_0_sg_processed_scene   (from appalachia.4.-14.-29.bto)',
          line(V1, T1)]),
        (render.trim(render.render(V2, T2, W=1000, H=1000, azim=35, elev=22, fov=26)),
         ['FO4 HAND-AUTHORED kit piece',
          '        LOD/Buildings/Bld02FrontBrickCom01_LOD.nif',
          line(V2, T2)]),
        (render.trim(render.render(V3, T3, W=1000, H=1000, azim=35, elev=22, fov=26)),
         ['FO4 HAND-AUTHORED whole building',
          '        LOD/Neighborhoods/Cambridge/Cambridge46_Bld01LOD.nif',
          line(V3, T3)]),
    ]
    out.append(render.sheet(pans, os.path.join(IMG, '02_fo76_remesh_vs_fo4_authored.png'),
                            title='Same on-screen size: a FO76 generated building shell beside FO4 hand-authored LOD',
                            panel_w=760, panel_h=760, cap_h=92))

    # ---- sheet 3 (bonus): the GlobalAtlasShape merge for contrast --------
    n = nif76.Nif76(BTO16)
    V4, T4 = merge([n.shapes[8]])
    V4c, T4c, _ = cluster(V4, T4, 900.0)
    im = render.trim(render.render(V4c, T4c, W=1100, H=1100, azim=35, elev=22, fov=26))
    out.append(render.sheet([(im, ['FO76 appalachia.16.-14.-29.bto  GlobalAtlasShape_NotAlphaTested (block 8),',
                                   'a cut-out: the whole shape is %d tris / %d verts in 197 separate pieces.' % (len(T4), len(V4)),
                                   'A MERGE of per-object LOD, NOT a remesh; drawn against 256x256 texture-array slices'])],
                            os.path.join(IMG, '03_fo76_globalatlas_merge.png'),
                            title='The other FO76 path in the same file',
                            panel_w=980, panel_h=980, cap_h=92))
    for p in out:
        print('wrote %s' % p)


if __name__ == '__main__':
    (probe if (len(sys.argv) > 1 and sys.argv[1] == 'probe') else make)()
