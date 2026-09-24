#!/usr/bin/env python3
"""HORIZON4 (A) -- the identity screenshots bungo asked for.

"send me screenshots of the chunks with our identity" -- chunk 4.4.-12, shaded
by the SHADOW IDENTITY the `.lodi` v8 group table carries.

Each picture is a PAIR over the same G-buffer and the same N.L shading:

  LEFT   one flat colour per GROUP      -- our shadow identity
  RIGHT  one flat colour per PLACEMENT  -- what the file would look like if
                                          every reference were its own thing

Where the two panels look the SAME, the grouping did nothing.  Where the LEFT
panel is one colour over several buildings, the grouping has told the shadow
map that they are one thing and none of them may shadow another.  Where the
LEFT panel is many colours over ONE building, the opposite mistake.

Trees get one colour each in both panels and are drawn desaturated, so they
cannot be mistaken for architecture.

Terrain is a flat grey in both panels: it is its own identity by rule.
"""
import numpy as np
import os
import pickle
import struct
import sys
import time

import h4core as H
import h4map as MP
import render as RD

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import lodgen_native_decode as ND                          # noqa: E402

LANE = H.LANE
IMG = LANE + '/images'
BAKE = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
        '/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth')
SCAN = ('C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude'
        '/392777f8-9016-4913-858d-16d6eec4c01a/scratchpad/ident_scan.pkl')
NAMES = ('bld', 'building', 'tower')
SUN = (120.0, 35.0)                 # a plain high sun: this is a shape picture


def palette(ids, seed=12345):
    """A stable, well-separated colour per id.  Neighbouring ids must NOT get
    neighbouring colours or a shattered group reads as a gradient."""
    u, inv = np.unique(ids, return_inverse=True)
    r = np.random.RandomState(seed)
    h = r.permutation(len(u)) / max(1, len(u))
    s = 0.45 + 0.4 * r.rand(len(u))
    v = 0.60 + 0.35 * r.rand(len(u))
    i = np.floor(h * 6.0).astype(int) % 6
    f = h * 6.0 - np.floor(h * 6.0)
    p_, q_, t_ = v * (1 - s), v * (1 - s * f), v * (1 - s * (1 - f))
    R = np.choose(i, [v, q_, p_, p_, t_, v])
    G = np.choose(i, [t_, v, v, q_, p_, p_])
    B = np.choose(i, [p_, p_, t_, v, v, q_])
    return np.stack([R, G, B], axis=1)[inv], len(u)


def shade(gb, idpix, istree, sun, grey=(0.62, 0.60, 0.56)):
    a, e = np.radians(sun[0]), np.radians(sun[1])
    sd = np.array([np.sin(a) * np.cos(e), np.cos(a) * np.cos(e), np.sin(e)])
    n = len(gb.kind)
    col = np.zeros((n, 3))
    col[:] = 0.10
    ter = gb.terfirst
    col[ter] = grey
    obj = gb.objfirst
    c, _ = palette(idpix[obj])
    col[obj] = c
    t = istree[obj]
    if t.any():
        g = col[obj][:, :3].mean(axis=1, keepdims=True)
        cc = col[obj].copy()
        cc[t] = 0.35 * cc[t] + 0.65 * np.concatenate([g[t] * 0.9, g[t] * 1.05, g[t] * 0.8], axis=1)
        col[obj] = cc
    ndl = np.clip(gb.nrm @ sd, 0.0, 1.0)
    lam = (0.42 + 0.58 * ndl)[:, None]
    out = col * lam
    out[gb.kind == 0] = 0.06
    return np.clip(out, 0.0, 1.0)


def pair(gb, left, right, ltitle, rtitle, lsub, rsub, caption, name, istree):
    import shade as SH
    w, h = gb.cam.w, gb.cam.h
    a = SH.to8(shade(gb, left, istree, SUN), w, h)
    b = SH.to8(shade(gb, right, istree, SUN), w, h)
    p = IMG + '/' + name + '.png'
    SH.pair(a, b, w, h, ltitle, rtitle, caption, p, sub_l=lsub, sub_r=rsub)
    return p


def main():
    os.makedirs(IMG, exist_ok=True)
    log = open(LANE + '/ident_pics.log', 'w')

    def p(*a):
        s = ' '.join(str(x) for x in a)
        print(s)
        log.write(s + '\n')
        log.flush()

    T0 = time.time()
    ter, ob, sh = H.load_scene()
    T = ND.read_lodi(BAKE + '.lodi')
    L = ND.read_lodo(BAKE + '.lodo')
    gvert = MP.group_of_vertex(ob, T)                 # group id per .lodo vertex
    inst = ob.inst                                     # placement index per vertex

    mdl = []
    for r in T['instances']:
        b = L['bases'][r['baseId']]
        mdl.append(L['string_at'](b['modelStringOffset']).lower())
    tree_inst = np.array([('tree' in m or 'shrub' in m or '\\trees\\' in m) for m in mdl])
    p('placements %d, of which tree-like models %d' % (len(mdl), int(tree_inst.sum())))

    g = np.array(T['group'], dtype=np.int64)
    n = T['header']['instanceCount']
    chunkOf = np.zeros(n, dtype=np.int64)
    for ci, c in enumerate(T['chunks']):
        chunkOf[c['instanceFirst']:c['instanceFirst'] + c['instanceCount']] = ci
    gid = chunkOf * 100000 + g + 1
    sizes = np.bincount(np.unique(gid, return_inverse=True)[1])
    CAP = ('%s groups  |  %s placements  |  %s singletons  |  largest group %d placements'
           % ('{:,}'.format(len(sizes)), '{:,}'.format(n),
              '{:,}'.format(int((sizes == 1).sum())), int(sizes.max())))
    p(CAP)

    # the Bld-named XLYR merge, for the proposal panel
    S = pickle.load(open(SCAN, 'rb'))
    refsub, layr = S['refsub'], S['layr']
    lay = np.zeros(n, dtype=np.int64)
    lname = {}
    for i in range(n):
        s = refsub.get(T['cold'][i]['refFormId'], {})
        if b'XLYR' in s:
            f = struct.unpack_from('<I', s[b'XLYR'][0], 0)[0]
            lay[i] = f
            lname[f] = layr.get(f, ('', 0))[0]
    par = {}

    def find(a):
        while par.get(a, a) != a:
            par[a] = par.get(par[a], par[a])
            a = par[a]
        return a
    byl = {}
    for i in range(n):
        f = int(lay[i])
        if f and any(k in lname.get(f, '').lower() for k in NAMES):
            byl.setdefault(f, []).append(int(gid[i]))
    for f, gs in byl.items():
        u = sorted(set(gs))
        for x in u[1:]:
            par[find(x)] = find(u[0])
    mid = np.array([find(int(x)) for x in gid], dtype=np.int64)
    p('Bld-named XLYR merge: %d identities -> %d' % (len(np.unique(gid)), len(np.unique(mid))))

    gpix_src = gvert                       # per .lodo vertex
    ipix_src = inst.astype(np.int64)
    mpix_src = mid[inst]
    istree_v = tree_inst[inst]

    import cams as CAMS
    cs = CAMS.build(ter, 1600, 900)
    cs['wide'] = CAMS.topdown(1400, 1400, crop=False)
    cs['wide'].name = 'wide'

    # the close-up cameras: aimed at the layers ident_notes names
    def look(cx, cy, half, bear=215.0, pitch=26.0, nm='cu'):
        b, q = np.radians(bear), np.radians(pitch)
        d = half * 3.0
        ex = cx + np.sin(b) * d
        ey = cy + np.cos(b) * d
        tz = float(ter.atf(np.array([cx]), np.array([cy]))[0])
        return RD.Camera((ex, ey, tz + d * np.tan(q)), (cx, cy, tz + 300.0),
                         42.0, 1400, 800, name=nm)

    cen = {}
    for i in range(n):
        f = lname.get(int(lay[i]), '')
        if f:
            cen.setdefault(f, []).append((T['instances'][i]['x'], T['instances'][i]['y']))
    # the named cases first, then the chunk's own worst groups-vs-layer cases
    want = ['DN135_GwinnettExt', 'AndrewStation', 'Theater47_Bld01']
    shatter = sorted(((len(set(int(gid[i]) for i in range(n)
                               if lname.get(int(lay[i]), '') == f)), f)
                      for f in cen if f), reverse=True)
    p('worst groups-vs-layer cases on this chunk (v7 groups inside ONE layer):')
    for k, f in shatter[:5]:
        p('    %-24s %4d placements shatter into %4d groups' % (f, len(cen[f]), k))
        if f not in want:
            want.append(f)
    CU = {}
    for nm in want[:6]:
        if nm not in cen:
            p('close-up %-24s NOT on this chunk, skipped' % nm)
            continue
        a = np.array(cen[nm], dtype=np.float64)
        cx, cy = a[:, 0].mean(), a[:, 1].mean()
        half = max(600.0, 0.6 * max(a[:, 0].max() - a[:, 0].min(),
                                    a[:, 1].max() - a[:, 1].min()))
        gs = len(set(int(gid[i]) for i in range(n) if lname.get(int(lay[i]), '') == nm))
        CU[nm] = (len(a), gs)
        p('close-up %-24s %d placements in %d v7 groups, centre (%.0f, %.0f), half %.0f'
          % (nm, len(a), gs, cx, cy, half))
        cs['cu_' + nm] = look(cx, cy, half, nm='cu_' + nm)

    out = []
    for nm, cam in cs.items():
        gp = LANE + '/gb_%s.npz' % nm
        if os.path.exists(gp):
            z = np.load(gp)
            gb = H._GB(cam, z)
        else:
            gb = RD.GBuffer(cam, ter, ob, verbose=False)
            np.savez_compressed(gp, kind=gb.kind, t=gb.t, pos=gb.pos.astype(np.float32),
                                dir=gb.dir.astype(np.float32), nrm=gb.nrm.astype(np.float32),
                                tri=gb.tri.astype(np.int32), bary=gb.bary.astype(np.float32),
                                dropped=gb.dropped)
        v0 = ob.tri[gb.tri, 0]
        gpix = gpix_src[v0]
        ipix = ipix_src[v0]
        mpix = mpix_src[v0]
        tpix = istree_v[v0]
        seen = int(len(np.unique(gpix[gb.objfirst])))
        seenp = int(len(np.unique(ipix[gb.objfirst])))
        extra = ''
        if nm.startswith('cu_') and nm[3:] in CU:
            k, gs = CU[nm[3:]]
            extra = ('\nCLOSE-UP on CK layer %s: %d placements in ONE layer, shattered '
                     'into %d shadow identities.' % (nm[3:], k, gs))
        cap = ('chunk 4.4.-12, the .lodi v8 group table   |   camera "%s" %dx%d   |   '
               'sun az %.0f el %.0f, flat identity colour x N.L (no shadows drawn)\n'
               '%s\nin frame: %s groups, %s placements.  Terrain is flat grey (its own '
               'identity by rule); trees are desaturated, one colour each.%s'
               % (nm, cam.w, cam.h, SUN[0], SUN[1], CAP,
                  '{:,}'.format(seen), '{:,}'.format(seenp), extra))
        f = pair(gb, gpix, ipix,
                 'LEFT  one colour per GROUP = our shadow identity',
                 'RIGHT  one colour per PLACEMENT = every reference its own thing',
                 'two placements the same colour may not shadow each other',
                 'what the file would look like with no grouping at all',
                 cap, 'ident_%s' % nm, tpix)
        out.append(f)
        p('wrote %s' % f)
        if nm in ('east', 'full'):
            cap2 = ('chunk 4.4.-12   |   camera "%s" %dx%d   |   PROPOSAL, NOT RULED.\n'
                    'Merge rule under test: two groups become one identity when their '
                    'placements share a Creation Kit XLYR layer whose editor id contains '
                    '"bld", "building" or "tower".\n'
                    'On this chunk that fires on 3 of 30 layers: %d identities become %d.  '
                    'South Boston is layered by CITY BLOCK, not by building, so the '
                    'proposal is nearly a no-op HERE and would bite elsewhere.'
                    % (nm, cam.w, cam.h, len(np.unique(gid)), len(np.unique(mid))))
            f = pair(gb, mpix, gpix,
                     'LEFT  PROPOSAL  groups merged by Bld-named CK layer',
                     'RIGHT  the GROUPS we ship today',
                     'a proposal, not a ruling: the layer is an authoring label',
                     CAP,
                     cap2, 'ident_%s_layermerge' % nm, tpix)
            out.append(f)
            p('wrote %s' % f)
    p('TOTAL %.1f min, %d pictures' % ((time.time() - T0) / 60.0, len(out)))
    log.close()


if __name__ == '__main__':
    main()
