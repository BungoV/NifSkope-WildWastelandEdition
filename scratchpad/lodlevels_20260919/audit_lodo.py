#!/usr/bin/env python3
"""LODLEVELS1 section 0 -- is any level of OUR bake carrying geometry that is
NOT the authored MNAM-slot mesh?

Read-only.  For every library mesh in the newest default-recipe urban bake's
`.lodo`, per base and per rep slot 0..3:

    triangles / vertices / bound  IN THE .lodo
  vs the same numbers read straight from the authored MNAM-slot .nif
  vs the same numbers read from the base's NEAR MODL .nif

and a classification per (base, slot):

  A  identical to the authored MNAM mesh for that slot   (triangles equal)
  B  equals the near MODL                                (full geometry in a level)
  C  fewer triangles than the authored MNAM mesh         (decimated)
  D  more triangles than the authored MNAM mesh          (subdivided / other)
  E  no authored file found on disk

Nothing is written outside this lane's folder.
"""
import os
import pickle
import sys
import json

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ROOT + '/tests/spells')
import lodgen_native_decode as D            # noqa: E402
import gltf_nifread as NR                   # noqa: E402

DATA = 'E:/Tools/Fallout 4/DataUnpacked/Data'
BAKE = ROOT + '/scratchpad/horizon2_20260918/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth.lodo'

_nifcache = {}


def nif_measure(rel):
    """(triangles, vertices, (dx,dy,dz)) of a model path relative to Meshes\\,
    composed through its node chain.  None when the file is missing/unreadable."""
    if not rel:
        return None
    key = rel.lower().replace('\\', '/')
    if key in _nifcache:
        return _nifcache[key]
    path = os.path.join(DATA, 'Meshes', rel.replace('\\', '/'))
    out = None
    if os.path.exists(path):
        try:
            n = NR.Nif(path)
            tris = 0
            verts = 0
            lo = [1e30] * 3
            hi = [-1e30] * 3
            for sid, sh in n.shapes.items():
                tris += sh['numTris']
                verts += sh['numVerts']
                # compose the chain: shape local, then every parent node
                chain = []
                cur = sh
                guard = 0
                while cur is not None and guard < 64:
                    chain.append((cur['t'], cur['r'], cur['s']))
                    p = cur.get('parent')
                    cur = n.nodes.get(p) if p is not None else None
                    guard += 1
                for v in sh['verts']:
                    x, y, z = v
                    for (t, r, s) in chain:
                        x, y, z = (r[0] * x + r[1] * y + r[2] * z,
                                   r[3] * x + r[4] * y + r[5] * z,
                                   r[6] * x + r[7] * y + r[8] * z)
                        x = x * s + t[0]
                        y = y * s + t[1]
                        z = z * s + t[2]
                    for i, c in enumerate((x, y, z)):
                        if c < lo[i]:
                            lo[i] = c
                        if c > hi[i]:
                            hi[i] = c
            if verts:
                out = (tris, verts, tuple(hi[i] - lo[i] for i in range(3)))
            else:
                out = (tris, verts, (0.0, 0.0, 0.0))
        except Exception as e:                       # noqa: BLE001
            out = ('ERR', str(e)[:80], None)
    _nifcache[key] = out
    return out


def main():
    st = os.stat(BAKE)
    lodi = BAKE[:-5] + '.lodi'
    print('bake   %s' % BAKE)
    print('       %d bytes, mtime %s' % (st.st_size, __import__('time').strftime(
        '%Y-%m-%d %H:%M:%S', __import__('time').localtime(st.st_mtime))))
    sl = os.stat(lodi)
    print('       %s %d bytes, mtime %s' % (os.path.basename(lodi), sl.st_size,
          __import__('time').strftime('%Y-%m-%d %H:%M:%S', __import__('time').localtime(sl.st_mtime))))

    L = D.read_lodo(BAKE)
    h = L['header']
    sa = L['string_at']
    print('lodo   v%d  bases %d meshes %d clusters %d levelMax %d ladderGroup %d flags 0x%x'
          % (h['version'], h['baseCount'], h['meshCount'], h['clusterCount'],
             h['levelMax'], h['ladderGroup'], h['flags']))

    # ---- per-mesh numbers in the library, level-0 clusters only
    mesh = []
    for i, m in enumerate(L['meshes']):
        t = v = 0
        for c in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount']):
            if L['clusterLods'][c]['level'] != 0:
                continue
            t += L['clusters'][c]['triangleCount']
            v += L['clusters'][c]['vertexCount']
        mesh.append(dict(path=sa(m['modelStringOffset']), tris=t, verts=v,
                         ext=tuple(m['aabbExtent']), clusters=m['clusterCount'],
                         levels=m['levelCount']))
    print('meshes level-0 triangles total %d, vertices(split) total %d'
          % (sum(m['tris'] for m in mesh), sum(m['verts'] for m in mesh)))

    esm = pickle.load(open(os.path.join(HERE, 'esm.pkl'), 'rb'))
    bases = esm['bases']

    counts = {}
    rows = []
    nonA = []
    pathmismatch = []
    for b in L['bases']:
        f = b['formId']
        eb = bases.get(f)
        slots = eb['slots'] if eb else ['', '', '', '']
        modl = eb['modl'] if eb else ''
        near = nif_measure(modl)
        for k in range(4):
            mid = b['rep%d' % k]
            if mid == 0xFFFF:
                continue
            M = mesh[mid]
            authored = slots[k]
            am = nif_measure(authored)
            samepath = (M['path'].lower().replace('\\', '/') ==
                        (authored or '').lower().replace('\\', '/'))
            nearpath = (M['path'].lower().replace('\\', '/') ==
                        (modl or '').lower().replace('\\', '/'))
            if am is None or (isinstance(am, tuple) and am[0] == 'ERR'):
                cls = 'E'
            elif nearpath and not samepath:
                cls = 'B'
            elif M['tris'] == am[0]:
                cls = 'A'
            elif M['tris'] < am[0]:
                cls = 'C'
            else:
                cls = 'D'
            counts.setdefault(k, {}).setdefault(cls, 0)
            counts[k][cls] += 1
            if not samepath:
                pathmismatch.append((f, k, M['path'], authored, modl))
            if cls != 'A':
                nonA.append(dict(form='%08X' % f, edid=eb['edid'] if eb else '?',
                                 slot=k, cls=cls, lodoPath=M['path'],
                                 authored=authored, modl=modl,
                                 lodoTris=M['tris'], lodoVerts=M['verts'],
                                 lodoExt=[round(x, 1) for x in M['ext']],
                                 authTris=(am[0] if isinstance(am, tuple) else None),
                                 authVerts=(am[1] if isinstance(am, tuple) else None),
                                 authExt=([round(x, 1) for x in am[2]]
                                          if isinstance(am, tuple) and am[2] else None),
                                 nearTris=(near[0] if isinstance(near, tuple) else None)))
            rows.append((f, k, cls, M['tris'], am[0] if isinstance(am, tuple) else -1))

    print('\nCLASS COUNTS per rep slot (A identical / B = near MODL / C fewer / D more / E no file)')
    print('slot |     A |     B |     C |     D |     E | total')
    tot = {}
    for k in range(4):
        c = counts.get(k, {})
        n = sum(c.values())
        for cc in 'ABCDE':
            tot[cc] = tot.get(cc, 0) + c.get(cc, 0)
        print(' %d   | %5d | %5d | %5d | %5d | %5d | %5d'
              % (k, c.get('A', 0), c.get('B', 0), c.get('C', 0), c.get('D', 0), c.get('E', 0), n))
    print('ALL  | %5d | %5d | %5d | %5d | %5d | %5d'
          % (tot.get('A', 0), tot.get('B', 0), tot.get('C', 0), tot.get('D', 0),
             tot.get('E', 0), sum(tot.values())))
    print('\n.lodo mesh path != the base\'s own MNAM slot path: %d of %d (base,slot) pairs'
          % (len(pathmismatch), len(rows)))
    for r in pathmismatch[:10]:
        print('  %08X slot %d  lodo=%s  mnam=%s  modl=%s' % r)

    print('\nEVERY NON-A (base, slot): %d' % len(nonA))
    for r in nonA[:80]:
        print('  %s %-40s slot %d  %s  lodoTris %s authTris %s nearTris %s'
              % (r['form'], r['edid'][:40], r['slot'], r['cls'],
                 r['lodoTris'], r['authTris'], r['nearTris']))
        print('      lodo=%s' % r['lodoPath'])
        print('      mnam=%s' % r['authored'])
    json.dump(dict(counts=counts, nonA=nonA, pathmismatch=len(pathmismatch),
                   rows=len(rows)), open(os.path.join(HERE, 'audit_lodo.json'), 'w'), indent=1)
    print('\nwrote audit_lodo.json')


if __name__ == '__main__':
    main()
