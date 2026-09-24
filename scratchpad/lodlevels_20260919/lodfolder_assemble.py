#!/usr/bin/env python3
"""LODFOLDER: splice the Q1/Q3 and Q2 fragments into lodfolder_notes.md."""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
TMP = os.environ.get('LODFOLDER_TMP') or HERE
BS = chr(92)

HEAD = """# Are the meshes in Meshes%sLOD the only LOD meshes?  (LODFOLDER, 2026-09-19)

**VERDICT: NO intermediate geometry exists.  Bethesda authored exactly two tiers --
the full near model, and the LOD tree.  There is nothing in between.**

Evidence, measured over every MNAM slot of every LOD-bearing record (STAT, SCOL,
MSTT, FURN, DOOR, ACTI, LIGH, TREE, FLOR, CONT, ALCH, MISC) in Fallout4.esm and all
six DLC masters -- 36,998 LOD-capable bases, 3,911 of them actually carrying LOD:

  1. **0 of 3,766** distinct MNAM mesh paths point anywhere outside an LOD folder.
     Not one LOD slot in the whole shipped game names a mesh from Architecture%s,
     Props%s, SetDressing%s or any other near-model folder.  The 403 paths that are
     not literally under `Meshes%sLOD%s` are all under `Meshes%sDLC03%sLOD%s` or
     `Meshes%sDLC04%sLOD%s` -- the DLCs' own LOD trees.
  2. The jump is a **cliff, not a ramp**: median triangles(LOD slot 0) /
     triangles(near MODL) = **0.038**, i.e. the very first LOD already throws away
     ~96%% of the geometry (~26x simpler) in one step.
  3. Multiple LOD *levels* do exist, but only INSIDE the LOD tree and only for
     **199 of 3,911** bases (5%%).  bungo is right that trees have them: 121 of
     those 199 are trees/landscape.  Architecture almost never does -- only 9
     building bases have different meshes across their slots; 2,512 repeat one
     mesh in every filled slot.

So: the meshes he is looking at in `Meshes%sLOD%sArchitecture` ARE the only LOD
meshes for those kits.  The finer `_lod_0 / _lod_1 / _lod_2` ladder he may have
seen belongs mostly to trees, and it lives in the same LOD folder -- it is not a
separate, higher-complexity intermediate tier sitting between the LOD and the
full model.

Sources: `X:%sPrograms%sSteam%ssteamapps%scommon%sFallout 4%sData%s*.esm` (7 masters),
meshes from `E:%sTools%sFallout 4%sDataUnpacked%sData%sMeshes%s`.  Scripts:
`lodfolder_collect.py`, `lodfolder_q1q3.py`, `lodfolder_q2.py`, `lodfolder_assemble.py`.
Triangle counts via `tests/spells/gltf_nifread.py`.  Read-only: nothing outside this
folder was written, no build, no NifSkope.

---

""" % tuple([BS] * 27)


def main():
    q1q3 = open(os.path.join(TMP, 'q1q3.md')).read()
    q2 = open(os.path.join(TMP, 'q2.md')).read()
    # Q1, then Q2, then Q3 -- q1q3.md carries Q1 then Q3; split on the Q3 heading
    marker = '### Q3 --'
    i = q1q3.index(marker)
    q1, q3 = q1q3[:i], q1q3[i:]
    txt = HEAD + q1 + '\n---\n\n' + q2 + '\n---\n\n' + q3
    out = os.path.join(HERE, 'lodfolder_notes.md')
    with open(out, 'w', newline='\n') as f:
        f.write(txt)
    print('wrote %s (%d bytes, %d lines)' % (out, len(txt), txt.count('\n') + 1))


if __name__ == '__main__':
    main()
