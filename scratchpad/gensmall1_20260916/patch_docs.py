#!/usr/bin/env python
"""GENSMALL1 doc edits (ww-contract-provenance: every number in a new line is
re-derived from an anchor that is quoted beside it, and the anchor text -- not a
line number -- is what the edit matches on).

  1. docs/LODGEN_TERRAIN_VT.md  s5   the missing `--vt-height` CLI row
  2. docs/LODGEN_NATIVE_LODO_LODI.md s3.4  the mesh report is version 4 with
     twenty-six columns, not version 2 with twenty-four (it was already v3/25 on
     disk before this lane; the page had never been moved)
  3. docs/LODGEN_CENSUS.md      s5.3 + s6.1 + s6.2  the three shadow census
     words, the new bake line, and the cross-check that binds them
"""
import sys

CHANGES = []


def edit(path, old, new, note):
    b = open(path, 'rb').read()
    n = b.count(old)
    if n != 1:
        sys.stderr.write('%s: anchor appears %d times, refusing: %r\n' % (path, n, old[:70]))
        sys.exit(1)
    before = (len(b), b.count(b'\r'), b.count(b'\n'))
    b = b.replace(old, new)
    open(path, 'wb').write(b)
    after = (len(b), b.count(b'\r'), b.count(b'\n'))
    print('%-38s %-52s bytes %d -> %d  CR %d -> %d  LF %d -> %d'
          % (path, note, before[0], after[0], before[1], after[1], before[2], after[2]))


# ---------------------------------------------------------------- 1. VT s5
VT = 'docs/LODGEN_TERRAIN_VT.md'
edit(VT,
     b'| `--vt-mips N` | 2 | stored mips per tile |\n',
     b'| `--vt-mips N` | 2 | stored mips per tile |\n'
     b'| `--vt-height` | **off** | \xc2\xa72.2 layer 3: a fourth R16_UNORM height sheet per tile, on the same tile grid and border as the other three, finest from the LAND records and coarser by the same box filter. **Off by default and it stays off** -- it is uncompressed where the other three are BC1, so at content 256 / border 8 / 2 mips a height sheet is **184,960 B** against a BC1 sheet\xe2\x80\x99s **46,240 B** (\xc2\xa73.6), and a tile goes from **138,720 B** of colour classes to **323,680 B** without cover, **369,920 B** with (\xc2\xa72.2, \xc2\xa73.6). Not passing it is byte-identical to the bake before the layer existed. The panel row is **Terrain \xe2\x86\x92 Carry a height layer** (`LodgenVtHeightCheck`), also unticked by default |\n',
     's5: the --vt-height row')

# ------------------------------------------------- 2. NATIVE s3.4, the report
NAT = 'docs/LODGEN_NATIVE_LODO_LODI.md'
edit(NAT,
     b'`--native-mesh-report <file>` writes one line per mesh (report version **2**,\n'
     b'twenty-four columns; `model` stays the line\xe2\x80\x99s remainder because it is the only\n'
     b'token that may hold a space):\n'
     b'`meshId triangles srcVerts emitVerts acmrBefore acmrAfter atvrBefore atvrAfter\n'
     b'boundarySrc boundaryEmitted levels clustersL0 clustersLadder maxError\n'
     b'groupsFormed refSmall refNoCut refFlat errExact errBounded weldedVerts\n'
     b'uvConflicts boundaryCoarsest model`.\n',
     b'`--native-mesh-report <file>` writes one line per mesh (report version **4**,\n'
     b'**twenty-six columns**; `model` stays the line\xe2\x80\x99s remainder because it is the only\n'
     b'token that may hold a space):\n'
     b'`meshId triangles srcVerts emitVerts acmrBefore acmrAfter atvrBefore atvrAfter\n'
     b'boundarySrc boundaryEmitted levels clustersL0 clustersLadder maxError\n'
     b'groupsFormed refSmall refNoCut refFlat refSilhouette errExact errBounded weldedVerts\n'
     b'uvConflicts boundaryCoarsest casterInstances model`.\n'
     b'\n'
     b'Two of those columns arrived after this paragraph was first written and the\n'
     b'paragraph did not move with them, so it is stated here: `refSilhouette` is v3\xe2\x80\x99s\n'
     b'(the header on disk already read `report 3` with twenty-five columns), and\n'
     b'**`casterInstances` is v4\xe2\x80\x99s** \xe2\x80\x94 the number of instances whose base names this\n'
     b'mesh, each instance counted once per DISTINCT mesh, which is the per-source\n'
     b'caster count at mesh granularity (CENSUS 5.3, bungo 2026-09-11 14:4x). Its\n'
     b'column sums to the `native-casters:` line\xe2\x80\x99s `mesh-slot casters`.\n',
     's3.4: report version 2/24 -> 4/26')

print('done')
