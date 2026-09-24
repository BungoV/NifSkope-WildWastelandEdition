#!/usr/bin/env python
"""GENSMALL1 doc edits, part 2 (ww-contract-provenance, ww-census-contract).

Punctuation follows each page's own measured convention: ASCII apostrophes
(CENSUS 56/0 curly, NATIVE 252/0) and real em dashes (CENSUS 63, NATIVE 165).

  0. docs/LODGEN_TERRAIN_VT.md  the ONE curly apostrophe part 1 introduced, back
     to the ASCII one the other 451 apostrophes on that page use
  1. docs/LODGEN_NATIVE_LODO_LODI.md 3.4  report version 2/24 -> 4/26
  2. docs/LODGEN_CENSUS.md  5.3 three rows, 6.1 the new bake line, 6.2 the
     cross-check that binds the runtime rows to the bake number
"""
import sys

S = b'\xc2\xa7'          # section sign
M = b'\xe2\x80\x94'      # em dash


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
    print('%-38s %-34s bytes %d -> %d  CR %d -> %d  LF %d -> %d'
          % (path, note, before[0], after[0], before[1], after[1], before[2], after[2]))


# ------------------------------------------------------- 0. the apostrophe
edit('docs/LODGEN_TERRAIN_VT.md',
     b'against a BC1 sheet\xe2\x80\x99s **46,240 B**',
     b"against a BC1 sheet's **46,240 B**",
     'the ascii apostrophe back')

# ------------------------------------------------- 1. NATIVE 3.4, the report
edit('docs/LODGEN_NATIVE_LODO_LODI.md',
     b'`--native-mesh-report <file>` writes one line per mesh (report version **2**,\n'
     b"twenty-four columns; `model` stays the line's remainder because it is the only\n"
     b'token that may hold a space):\n'
     b'`meshId triangles srcVerts emitVerts acmrBefore acmrAfter atvrBefore atvrAfter\n'
     b'boundarySrc boundaryEmitted levels clustersL0 clustersLadder maxError\n'
     b'groupsFormed refSmall refNoCut refFlat errExact errBounded weldedVerts\n'
     b'uvConflicts boundaryCoarsest model`.\n',
     b'`--native-mesh-report <file>` writes one line per mesh (report version **4**,\n'
     b"**twenty-six columns**; `model` stays the line's remainder because it is the only\n"
     b'token that may hold a space):\n'
     b'`meshId triangles srcVerts emitVerts acmrBefore acmrAfter atvrBefore atvrAfter\n'
     b'boundarySrc boundaryEmitted levels clustersL0 clustersLadder maxError\n'
     b'groupsFormed refSmall refNoCut refFlat refSilhouette errExact errBounded weldedVerts\n'
     b'uvConflicts boundaryCoarsest casterInstances model`.\n'
     b'\n'
     b'**Two of those columns arrived after this paragraph, and it did not move with\n'
     b'them.** `refSilhouette` is v3' + M + b's: the header on disk already wrote `report 3`\n'
     b'with twenty-five columns while this page still said 2 and twenty-four, so the\n'
     b'page was wrong rather than merely old (lane GENSMALL1, 2026-09-16).\n'
     b'**`casterInstances` is v4' + M + b's**: the number of instances whose base names this\n'
     b'mesh, each instance counted once per DISTINCT mesh its base names. That is the\n'
     b'per-source caster count at mesh granularity (CENSUS ' + S + b'5.3, bungo 2026-09-11\n'
     b"14:4x), and the column sums to the `native-casters:` line's `mesh-slot casters`.\n",
     '3.4: report 2/24 -> 4/26')

# --------------------------------------------------- 2a. CENSUS 5.3, the rows
edit('docs/LODGEN_CENSUS.md',
     b'| `shadowIdentityUnique` | 0/1 | whether every far caster drawn this frame carried a unique identity index.'
     b' The far-shadow pass keys on identity and excludes self-shadowing (bungo 2026-09-11 08:4x), so a collision'
     b' is a wrong shadow, not a slow one | NATIVE 4.1c | doctor a duplicate identity into the `.lodi`: it must'
     b' read 0 | `unchecked` | `unchecked` |\n',
     b'| `shadowIdentityUnique` | 0/1 | whether every far caster drawn this frame carried a unique identity index.'
     b' The far-shadow pass keys on identity and excludes self-shadowing (bungo 2026-09-11 08:4x), so a collision'
     b' is a wrong shadow, not a slow one | NATIVE 4.1c | doctor a duplicate identity into the `.lodi`: it must'
     b' read 0 | `unchecked` | `unchecked` |\n'
     b'| `shadowCasters[src]` | count | far-shadow casters this frame **per SOURCE** ' + M + b' `tree`, `card`,'
     b' `mesh`, `terrain` (bungo 2026-09-11 14:4x, *"the census counts casters per source"*). The four are a'
     b' PARTITION, because his ruling is that every placement has exactly ONE shadow representation at a time;'
     b' the bake-side denominator for the first three is the `native-casters:` line (' + S + b'6.1, ' + S + b'6.2)'
     b' | NATIVE 3.4, the `native-casters:` line | walk into a forest: `tree` rises and the others do not. On a'
     b' region with no trees `tree` reads **0**, and that 0 is a measurement, not a default | `no_cut`,'
     b' `no_library` | `uncounted` |\n'
     b'| `shadowMarchMs` | ms | the terrain shadow **march** alone, not the whole shadow pass. bungo 15:0x made'
     b' the hybrid far shadow the arm and both of its halves census fields, so neither half may hide inside the'
     b" other's number. The plan's 0.3" + M + b'0.8 ms is an ESTIMATE and stays labelled one until a capture round'
     b' measures it | runtime ' + M + b' FO4CS times it; no bake number exists | lower the sun: the march is the'
     b' half whose cost rises | `off` when the far-shadow arm is off, `n/a` on a frame with no terrain in the far'
     b' field | `unmeasured` |\n'
     b'| `shadowMapMs` | ms | the far shadow **map** alone ' + M + b' the cluster cut plus the geometry rendered'
     b" into it, the other half of the same hybrid (bungo 15:0x). The plan's 1" + M + b'3 ms is an ESTIMATE and'
     b' stays labelled one | runtime ' + M + b' FO4CS times it; no bake number exists | raise'
     b' `shadowTolerancePx`: fewer triangles, less time | `off` when the far-shadow arm is off, `n/a` on a frame'
     b' that drew no far caster | `unmeasured` |\n',
     '5.3: the three shadow rows')

# --------------------------------------------------- 2b. CENSUS 6.1, the line
edit('docs/LODGEN_CENSUS.md',
     b'| `vt:` |',
     b'| `native-casters:` | the per-SOURCE caster counts and the law that makes them a partition: instances'
     b' total; `tree`, `card`, `mesh`, `none`; their sum against the instance count, printed as **AGREE** or'
     b' **DISAGREE** so a reader never has to add four numbers up; `mesh-slot casters` over the mesh count'
     b' (each instance once per DISTINCT mesh its base names); and the note that the terrain march is a runtime'
     b' cost FO4CS measures, not a bake number |\n'
     b'| `vt:` |',
     '6.1: the native-casters: line')

# --------------------------------------------- 2c. CENSUS 6.2, the cross-check
edit('docs/LODGEN_CENSUS.md',
     b'| `suppressedDraws` | the cold blob',
     b'| `shadowCasters[tree]`, `[card]`, `[mesh]` | the `native-casters:` line' + M + b's four bins | no runtime'
     b' source may exceed its own bake bin, and the three together may not exceed `instanceCount` minus the bake'
     b" line's `none`. `none` is a FAULT count: a placement with no representation of any kind casts nothing |\n"
     b'| `shadowCasters[terrain]` | ' + M + b' | **no bake number exists**, and that is the honest answer rather'
     b' than a missing row: the march runs over the resident height source at runtime, so the field reads from'
     b' the frame, never from a file |\n'
     b'| `suppressedDraws` | the cold blob',
     '6.2: the caster cross-check')

print('done')
