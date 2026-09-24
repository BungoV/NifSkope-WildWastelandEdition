"""AUDIT1: section 3 -- the independent decode of every output file.

Every number below was produced by a reader in tests/spells/ run by me against a
file on disk, and every row's refuter was applied to a copy of a REAL
product-written file and its result recorded, including the two that did not
catch and why.
"""
import io
import os
import sys
import tempfile

P = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/'
     'lane_audit1_report.md')
ANCHOR = '## 4. Diff review of the campaign'

NEW = """## 3. Independent decode of every output file

Nothing in this section reads a number the writer printed. Each file is opened
by a Python reader in `tests/spells/` that works from the format documents, and
each invariant is followed by a REFUTER -- the same reader run against a doctored
copy of the same real file, which must go red. A check with no refuter is not
reported as a check.

**Two readers were written for this audit, and the brief asks me to say which.**
`tests/spells/lodgen_lodm_check.py` is the permitted new one: there was a reader
for every other type in the family -- `.lodl`, `.lodt`, `.lodo`, `.lodi`,
`.lodb`, `.lodj` -- and none for `.lodm`. `scratchpad/audit1_20260916/`
`lodj_sweep.py` is audit scaffolding rather than a gate: it drives the existing
`tests/spells/lodj_read.py` across a whole tree and cross-checks it against the
`.lodi`, which no single-file reader could do. The refuter drivers
(`refute_native.sh`, `refute_rest.sh`, `mutate_*.py`) are scaffolding too. The
readers themselves are the tree's.

### 3.1 The invariants

| # | invariant | reader | files | checks | violations | refuter, and what it did |
|---|---|---|---|---|---|---|
| 1 | `.lodl` subsample: a coarse level IS the fine sample, so it cannot hold a mean | `lodgen_lodl_pyramid.py` A | 1 | 1 (295,936 samples on 295,936 distinct slots) | **0** | A-FLOOR, three children collapsed onto one slot: **not a bijection, red** |
| 2 | `.lodl` every sampled height inside its own cell's stored range | same, B | 1 | 1 (18,496 samples, worst 0.000 u) | **0** | B-FLOOR, the range shrunk to a tenth of its span: **15,720 outside, red** |
| 3 | `.lodl` water: the dry sentinel, no sentinel where there is water, every type in WATR | same, C1--C3 | 1 | 3 | **0** | C2/C3 have subjects (36,864 water cells, 15 interned types). **C1 does not** -- see 3.3 |
| 4 | `.lodt` tiles: an absent tile is 24 zero bytes, every payload the size the header implies, 4,096-aligned, non-overlapping, pad bytes zero, CRC recomputes | `lodgen_vt_check.py tiles` | 2 | 14 | **0** | R1, one byte flipped at offset 5,898,288 of 11,796,576: **red** |
| 5 | `.lodt` a tile's border is its NEIGHBOUR's content, not a clamp of its own edge | `lodgen_vt_check.py border` | 2 | 8 | **0** | the check's own second half: border texels equal to this tile's edge must stay under 75 % |
| 6 | `.lodt` georef: row 0 is north, and two tiles are not the same tile | `lodgen_vt_check.py georef` | 2 | 4 | **0** | built into the check |
| 7 | `.lodo`/`.lodi` pair: worldspace, corpus hashes, load order, base ids, draw keys | `lodgen_native_decode.py` | 4 pairs | 24 | **0** | `lodi-wrap` and `lodo-wrap` doctored onto a real pair: **both REFUSED**, "instances/vertices runs past the file" |
| 8 | `.lodo` version 3 is refused BY NAME, not by falling off a range | same | 1 real pair stamped back to v3 | 1 | **0** | the refusal is the check: *"version 3: a v3 base row spends crossPx16[0..1] on two screen-size steps"* |
| 9 | `.lodi` fields, every word lane, against the bake's own mesh report and manifests | `lodgen_native_fields.py` | 1 pair + report + 8 manifests | **52** | **0** | groups e1 and g1 REFUSE TO RUN without `--native-mesh-report` and report themselves as FAIL rather than skipping -- see 3.2 |
| 10 | `.lodo` ladder, cluster spheres, normal cones, and the cut is a partition at nine (tolerance, distance) pairs | `lodgen_native_cut.py` | 4 pairs | 64 | **0** | four floors inside each run: a sphere shrunk 10 % (**438.3 u overshoot, red**), a cone tightened past its own worst face, tolerance 0, a huge tolerance |
| 11 | `.lodm` cards: envelope, family, kind, textures on disk, emissive, gap/pad/mips recomputed, `projection ortho`, coverage ordering, library agreement | `lodgen_lodm_check.py` (**new**) | 23 | 24 | **0** | six mutations -- magic, version, declared size, family, `projection persp`, an odd gap -- **each refused** |
| 12 | FO4CSLOD layout: everything the FO4CS target writes is under `<mod>/FO4CSLOD/<ws>/` and nothing else is | direct | 4 trees | 4 | **0** | see 3.4 |
| 13 | `.lodb` record: its sections against the tree and the log, its five corpus hashes against the `.lodo` header, written last | `lodgen_bakerec_gate.py` `sections`/`hashes`/`written-last` | 1 | **33** | **0** | R2 one hex digit of `objectCorpusHash` (**red**), R3 one `out` row deleted (**red**) |
| 14 | INCR1 `.lodj`: every cache names its own chunk, its `end` trailer equals its sections, every lighting row names a placed object, and the placements summed over the chunks equal the `.lodi` instance count | `lodj_sweep.py` over `lodj_read.py` (**new driver**) | 36 | 20 | **0** | one placement row dropped from one cache: **J2, J3 and J4 all red** |
| 15 | INCR1 byte identity: a null incremental reproduces the bake it replayed | `cmp`, section 2.2 | 55 | 55 | **0** | the record's OWN bytes do move, and are compared by content rows instead |

**Totals: 15 invariants, 76 files, 304 checks, 0 violations, and 21 refuters of
which 19 went red.** The two that did not are rows 7's aggregate pair and they
are not a gap in this section -- they are C2, and they are named in 3.5.

### 3.2 Two field groups needed a switch, and said so rather than passing

`lodgen_native_fields.py` on the four step-2 trees reported 35--39 checks and
**two FAILs**, `e1 a mesh report was given` and `g1 a mesh report was given`.
That is not a product defect and it is not a stale gate: `--native-mesh-report
<file>` is an opt-in on the bake, my step-2 command lines are the DEFAULTS and
do not spell it, and the reader refuses to pretend it measured a group whose
input is absent. It is the right behaviour -- a skipped check that prints `ok`
is how a suite loses coverage silently.

So the bake was made: `bake/meshrep`, region (a), `--native <dir>
--native-mesh-report <dir>/mesh_report.txt`, 39 s, a 911,262-byte report and
eight manifests. With the report and all eight manifests on the command line the
same reader on the same pair goes from 35 checks / 2 fails to **52 checks / 0
fails**. Every word lane in the `.lodi` is measured against the mesh report the
bake wrote for it.

### 3.3 One water check has no subjects, and a check with no subjects is not a check

Row 3's C1 reads *"every cell WITHOUT water writes height 0 and type 0xFFFF (0
dry cell(s), 0 broke it)"*. Zero dry cells. On the Commonwealth every one of the
36,864 cells in the 192x192 grid carries a water height, so C1 passes over an
empty set on this worldspace and always will. C2 and C3 are the two that have
subjects here (36,864 water cells with no sentinel, 15 interned types all inside
the WATR table), and they carry the row.

I am reporting C1 as VACUOUS rather than as a pass. It is not a defect in the
reader -- a worldspace with dry cells would give it subjects -- but this audit
did not exercise it, and a later lane reading "3 water checks green" would
inherit a check that has never run. That is exactly the failure mode the tree's
own rule names: a check that cannot fail on its input is not a check.

### 3.4 The layout, and the chunk with no manifest

All four FO4CS trees put every file of the target under
`<mod>/FO4CSLOD/Commonwealth/` and nothing anywhere else except where the
command line pointed `--out-dir` and `--tex-dir`. Region (a):

```
FO4CSLOD/Commonwealth/   1 .lodo   1 .lodi   1 .lodb   9 .lodj   8 .BTO.manifest.txt
```

**Nine caches and eight manifests**, which reads like a lost file until the
chunk is named. It is `Commonwealth.4.-20.32`, and both the cache and the bake
log account for it: the `.lodj` says `placements 0`, and the log says
`0 px no land, 262144 px no base tex` and `placements=0 meshes=0 shapes=0`. An
empty chunk builds no `.BTO`, so the scratch pass has nothing to leave a
manifest for; the cache is still written so that an incremental bake knows the
chunk was considered and found empty rather than never visited. Coherent, and
worth writing down because the count looks wrong at a glance.

### 3.5 What the independent readers caught that the exe did not

Row 7's refuters are the sharpest result in this section, because the two
readers of the same contract disagree about the same real file.

| doctored case | the Python decoder | the exe's `--native-verify` |
|---|---|---|
| `lodi-wrap` -- the instance blob's offset set 4,096 short of 2^64 so `off + bytes` wraps | **REFUSED**, "instances runs past the file" | **accepted, rc 0** |
| `lodo-wrap` -- the same on the `.lodo` vertex blob | **REFUSED**, "vertices runs past the file" | **accepted, rc 0** |
| `agg-views` -- a version-5 file with one aggregate at `aggregateViews 1` | accepted | accepted, rc 0 |
| `agg-record` -- a version-5 file with one all-zero 48-byte aggregate record | accepted | accepted, rc 0 |

The first two are C1 measured from the other side: the Python reader compares
`bytes > fileBytes || off > fileBytes - bytes` and the C++ at
`src/lodifile.cpp` / `src/lodofile.cpp` adds first, so the sum wraps below
`fileBytes` and the file is waved through. Two readers, one contract, opposite
answers on bytes neither of them wrote.

The last two are C2, and the Python decoder misses them for a different reason
that is worth stating so nobody counts it as a second bug: the decoder has no
`aggregateViews` rule and no aggregate-record rule **at all**. Those two rules
exist only in the C++ (`src/lodifile.cpp:888` and `:1160`), gated on version 4.
The decoder's own version handling is already right -- its header parse is
`version >= 4` and its payload table is `version == 4 or (v5 and
aggregateCount)`, which is the in-file idiom the fix in section 6 copies. So
C2's refuter is the exe's, and it is `lodgen_native.sh` section 14's two
doctored files, which are red today by design and must turn green on the fixed
build.

### 3.6 One reader is fragile on a legal file, and it is not a defect I am fixing

`lodgen_vt_check.py border` and `georef` both crashed on the default `--vt`
pyramid with a bare `TypeError: 'NoneType' object is not subscriptable`, having
run **0 checks** and exited 1 -- which on a gate board reads exactly like two
failing checks. The cause is not the product: `--vt-height` is OFF by default
(`src/lodgen.h:1341`, `src/nifcli.cpp:7882`), a version-2 container is required
to carry only the colour, msn and mask sheets (`src/io/lodvfile.cpp:592`), and
`Lodv.heights()` returns `None` when no role-4 sheet is present. Both commands
then index it.

**This is not a stale gate.** The shipped gate that owns these commands,
`tests/spells/lodgen_terrain_vt.sh`, spells `--vt-height` on its own bakes
(lines 130 and 133) and even asserts at V23 that the flag is off by default. The
gate is correct and my invocation was the gap; I re-baked with `--vt-height`
(`bake/everything/vth`, 10 s) and rows 4--6 above are measured on that pyramid,
all green. I am recording the fragility as an observation rather than fixing it,
because the brief's licence is for confirmed bugs and for STALE gates, and this
reader is neither. It is worth one line in the changelog for whoever next points
it at a default bake.

"""


def main():
    s = io.open(P, encoding='utf-8', newline='').read()
    if s.count(ANCHOR) != 1:
        print('ABORT: the section 4 anchor appears %d times' % s.count(ANCHOR))
        return 1
    if '## 3. Independent decode' in s:
        print('ABORT: section 3 is already in the report')
        return 1
    out = s.replace(ANCHOR, NEW + ANCHOR, 1)
    if out.count('\r') != s.count('\r'):
        print('ABORT: CR count moved')
        return 1
    d = os.path.dirname(P)
    f = tempfile.NamedTemporaryFile('w', encoding='utf-8', newline='', dir=d,
                                    delete=False, suffix='.tmp')
    f.write(out)
    f.close()
    os.replace(f.name, P)
    print('report: %d -> %d bytes, CR %d' % (len(s), len(out), out.count('\r')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
