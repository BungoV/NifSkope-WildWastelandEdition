"""Two label spaces, and the reason there are two.

LTEX identity is what the .esp must write.  But the stated end product is that a
landscape TEXTURE REPLACER can rebake far LOD from its new textures -- and a
replacer replaces a *texture file*, not an LTEX record.  Several Commonwealth
LTEX records point at the SAME diffuse:

    LDriedGrass01 / LDriedGrass01NoGrass / LDriedGrass02Weeds / LDriedGrass03
        -> Landscape\Ground\DriedGrass01_D.dds
    LOceanFloor01 / LOceanFloor01ShortKelp / LOceanFloor01TallKelp
        -> Landscape\Ground\OceanFloor01_d.DDS

Confusing two LTEX inside such a family costs the replacer user nothing: the
same pixels get sampled either way.  Confusing across families is the error that
actually hurts.  So accuracy is reported BOTH ways and the difference is stated,
rather than quoting whichever number flatters the result.

LTEX with no resolvable TXST path keep their own form id as their group, since
we cannot prove they share anything.
"""
import numpy as np

from rec_common import NULL, ltex_names


def build():
    names = ltex_names()
    groups = {}
    gname = {}
    for fid, (ed, tx) in names.items():
        k = tx.lower() if tx else ('#%08x' % fid)
        groups[fid] = k
        gname.setdefault(k, []).append(ed)
    keys = sorted(set(groups.values()))
    gid = {k: i for i, k in enumerate(keys)}
    return groups, gid, keys, gname


def group_of(ltex_array):
    """map an array of form ids to integer group ids."""
    groups, gid, keys, gname = build()
    return np.array([gid[groups.get(int(f), '#%08x' % int(f))] for f in ltex_array]), keys, gname


if __name__ == '__main__':
    groups, gid, keys, gname = build()
    multi = {k: v for k, v in gname.items() if len(v) > 1 and not k.startswith('#')}
    print('%d LTEX -> %d distinct diffuse groups' % (len(groups), len(keys)))
    print('%d groups hold more than one LTEX:' % len(multi))
    for k in sorted(multi, key=lambda k: -len(multi[k])):
        print('  %-52s %s' % (k, ', '.join(sorted(multi[k]))))
