"""Do the road DECAL shapes name a diffuse the bake can open?

  python road_decal_probe.py <refs.json> <dataRoot> <cx0> <cy0> <cx1> <cy1>
"""

import collections
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tools'))
from gltf_nifread import Nif                                  # noqa: E402
from lod_emission_probe import readBgsm                        # noqa: E402
from matinfo import find_material, read_material, shader_info   # noqa: E402
from placements import load                                   # noqa: E402
from rasterlib import MeshCache                               # noqa: E402

SEP = chr(92)


def tex_exists(dataRoot, rel):
    if not rel:
        return False
    p = rel.replace(SEP, '/').lower().lstrip('/')
    if not p.startswith('textures/'):
        p = 'textures/' + p
    cur = dataRoot
    for part in p.split('/'):
        try:
            names = os.listdir(cur)
        except Exception:
            return False
        hit = next((n for n in names if n.lower() == part), None)
        if hit is None:
            return False
        cur = os.path.join(cur, hit)
    return os.path.isfile(cur)


def main(argv):
    pl = load(argv[0])
    dataRoot = argv[1]
    cx0, cy0, cx1, cy1 = (int(argv[2]), int(argv[3]), int(argv[4]), int(argv[5]))
    mc = MeshCache(dataRoot)
    used = collections.Counter()
    for p in pl:
        if p['family'] not in ('road', 'landscape/sidewalks'):
            continue
        cx, cy = p['pos'][0] / 4096.0, p['pos'][1] / 4096.0
        if cx0 - 2 <= cx <= cx1 + 3 and cy0 - 2 <= cy <= cy1 + 3:
            used[p['modl']] += 1
    rows = collections.Counter()
    examples = {}
    for m, n in used.items():
        path = mc.path_for(m)
        if path is None:
            continue
        nif = Nif(path)
        for sh in nif.shapes.values():
            info = shader_info(nif, sh)
            if info is None:
                continue
            name, f1, f2 = info
            mat = find_material(dataRoot, name) if name else None
            dec = False
            t0 = nif.diffuse_for(sh) or ''
            if mat:
                r = read_material(mat)
                dec = bool(r and r['decal'])
                b = readBgsm(mat)
                if b and b['textures'] and b['textures'][0]:
                    t0 = b['textures'][0]
            key = (dec, bool(t0), tex_exists(dataRoot, t0))
            rows[key] += n
            examples.setdefault(key, (m, os.path.basename(mat or '(none)'), t0))
    print('key = (material says decal, names a diffuse, that diffuse exists)'
          '  -> placements')
    for k, v in rows.most_common():
        print('%-24s %6d   e.g. %s | %s | %s' % (k, v, examples[k][0],
                                                 examples[k][1], examples[k][2]))


if __name__ == '__main__':
    main(sys.argv[1:])
