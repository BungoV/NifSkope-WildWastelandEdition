"""AUDIT1: section 5 -- the design gaps, as rows, against plan §5/§6, census §6
and plan §8.2--8.4, plus the one-line audit of PERF1 row C.

Every DONE row below was verified by reading the field off a real pair with the
tree's own independent decoder (scratchpad/audit1_20260916/step5_probe.py), not
by quoting the page that claims it. Two rows moved on that evidence.
"""
import io
import os
import sys
import tempfile

P = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/'
     'lane_audit1_report.md')
ANCHOR = '## 6. '

NEW = """## 5. Design gaps against the plan, the rulings and the census

Three verdicts only: **DONE** (verified by me on a file, with the number),
**PARKED-BY-RULING** (bungo has ruled and the words are quoted), **GAP**. A row
that the page calls DONE and whose field is zero on every bake is not DONE; it
is a field nobody has looked at, and two rows moved on exactly that test.

### 5.1 Plan §5, "What the generator still owes the runtime" -- all nineteen rows

| # | the row | verdict | my evidence |
|---|---|---|---|
| 1 | per-MNAM-slot instance totals in the `.lodi` header | **DONE as a field, GAP as an instrument** | the field is right: `slotInstances` sums to `instanceCount` on 4 of 4 pairs. But see 5.2 -- no file this CLI writes can have two non-zero slots |
| 2 | a per-base full-detail triangle count | **DONE** | `fullTriangles` non-zero on **2,974 of 2,974** bases, **1,120** distinct values, max **149,282**, identical on all four trees. The plan's own numbers, re-measured |
| 3 | a card count in the `.lodo` header | **DONE as a field, GAP as a value** | `cardCount` is present at 0xD0 and reads **0** on every bake including `bake/aggreal`; 0 of 2,974 bases carry a `cardLayer`. The field exists and nothing fills it -- which is row 11, not a second defect |
| 4 | the aggregate outermost-band cards, and the forested-cell count | **GAP** | owned by CARDS-AGG. `bake/aggreal` shows the aggregate PAYLOAD path works on a real card library (97 aggregates, 3,423 covered) -- what is missing is the band-3 bake, not the writer |
| 5 | a watertight bit in the `.lodo` mesh row | **DONE** | **654 of 5,567** meshes carry `LODO_MESH_WATERTIGHT`, strictly between 0 and the mesh count, on all four trees. The plan's number, re-measured |
| 6 | the decoder refuses the downtown-Boston pair, so R3's occluder gate has no fixture | **DONE -- this row is CLOSED, and closing it is a result of this audit** | region (c) is downtown Boston and its pair carries **280 occluders**; region (b) carries **61**. The independent decoder reads both at **6 checks / 0 failures**, and `lodgen_native_cut.py` adds 17 more each. R3's occluder gate now has two fixtures on disk |
| 7 | `.BTO` chunk files still written under the FO4CS target | **DONE** | BTOFREE1. Measured in section 2: the default FO4CS bake leaves no `.BTO`, `--keep-bto` is byte-identical to the rung, the stock target is untouched |
| 8 | the resource stack cannot see a `.pbrm` or a `.lodm` | **GAP** | not exercised by this audit; no `--resource` bake was made. Reported unmeasured rather than assumed |
| 9 | far-terrain sheets DXT1/8 mips against vanilla's DXT5/10 | **GAP** | TERRAINFMT1. Not re-measured here |
| 10 | splat grading vs vanilla, mean difference 19.96/255 | **GAP** | SPLAT1. Not re-measured here |
| 11 | no bake writes a card layer; `cardCorpusHash` is 0 | **GAP, and sharper than written** | `cardCorpusHash` reads `0000000000000000` on **all four** pairs **including the one baked against 23 real ortho cards**. In `src/` the only assignment outside the synthetic fixture is the copy `src/lodofile.cpp:1571`, so the value defaults (`src/lodofile.h:326`) and nothing ever sets it. `--impostors` + `--aggregate` feeds the aggregate payload without stamping the card corpus |
| 12 | the height sheet is opt-in, and the CLI table does not list the flag | **half DONE, half PARKED-BY-RULING** | the CLI half is stale: `--vt-height` **is** in the VT CLI table (`docs/LODGEN_TERRAIN_VT.md:2328`, inside `## 5. The CLI` at 2311), with its byte cost. The bake half is ruling (e), still open. I measured the cost myself: rows 4--6 of section 3 needed a `--vt-height` pyramid and the default one has three sheets, not four |
| 13 | the asymmetric-drop proof on the (-32,0) dim-32 chunk | **GAP** | not run by this lane either; a dim-32 chunk bake is outside the three fixture regions |
| 14 | no `.lodt` container and no VT `.lodm` has ever been written to disk | **DONE -- CLOSED by this audit** | `bake/everything/vt` holds `Commonwealth.VT.2.lodt`, `Commonwealth.VT.4.lodt` and `Commonwealth.VT.lodm`, and `bake/everything/vth` the same with a height sheet. All six containers decode at 0 violations (section 3 rows 4--6). **R2 has a sample** |
| 15 | no v2 manifest and no texture-array output exists on disk | **half DONE** | the manifest half is closed: every chunk sidecar begins `# lodgen manifest 2 ws Commonwealth dim 4 chunk ...`, 8 of them per region tree, kept by the BTOFREE1 teardown. The texture-array half was not separately measured |
| 16 | the shipped FO4CS `.lodl` parser pins `kVersion = 1u` | **GAP** | an FO4CS-side row; this lane does not touch that tree (standing order: FO4CS readers come last) |
| 17 | three census words this page needs | **GAP** | a census-page lane |
| 18 | the module still composes the OLD paths | **generator half DONE** | section 3.4: all four trees put every FO4CS output under `<mod>/FO4CSLOD/Commonwealth/` and nothing elsewhere. The reader half is FO4CS's |
| 19 | a stale pair cannot name the plugin that went stale | **DONE** | BAKEREC1. `lodgen_bakerec_gate.py` reads the record against the tree at **33 checks / 0 failures**, and section 3 row 13 gives it two refuters that both go red |

### 5.2 The one row that moved from DONE to half-DONE, and the two bakes that moved it

Census §6.3 row 1 says of `slotInstances`, in its own words:

> `tests/spells/lodgen_native_fields.py` §j4 checks both that the sum holds and
> that the four MOVE -- a slot distribution that is all in one bin would pass a
> sum check and mean nothing.

The sum holds. The four do not move, and they cannot:

| bake | command | `slotInstances` |
|---|---|---|
| `bake/sanctuary_fo4cs` | region (a), `--dim 4` | **[3526, 0, 0, 0]** |
| `bake/sanctuary_dim8` | the same region, `--dim 8` | **[0, 3219, 0, 0]** |
| `bake/merge2` | `--dim 8` run into a finished `--dim 4` tree | **[0, 3219, 0, 0]** |

The slot index IS the bake dim (`src/nativeemit.cpp:1711` takes the placement's
own MNAM slot; `src/lodifile.cpp:415` tallies it). `--dim` takes ONE integer and
defaults to 4 (`src/nifcli.cpp:7476`, `:7276`); there is no `--dims`. And the
third row is the one that settles it: a second `--native` run into the same tree
**replaces** the library rather than merging into it -- 3,526 instances became
3,219, not 6,745. So **no `.lodi` this CLI can write has two non-zero slots.**

The gate passes anyway, because `j4b`'s predicate is `any(slots) and
len(set(slots)) > 1` and `set([3526, 0, 0, 0])` has two members. It is satisfied
by one non-zero slot and three zeros -- precisely the distribution the census
page names as the thing it is there to catch.

**I am not tightening it, and the reason is a measurement rather than caution:**
`[3526, 0, 0, 0]` is a CORRECT file. A predicate demanding four non-zero slots
would refuse every legitimate single-dim bake, which is every bake this audit
made and every fixture in the gates. The instrument census §6.3 promises needs a
FIXTURE that mixes dims in one library, and nothing in the CLI can produce one
today. That is the row, and it belongs to the lane that owns the multi-ring
bake, not to a one-line predicate change here.

### 5.3 Plan §6, the open rulings -- what this audit's evidence says about each

Only the rulings this lane measured something against are listed; the rest are
untouched and stay open.

| ruling | status | what I measured |
|---|---|---|
| **(e)** "Must the full Commonwealth bake run with `--vt-height`?" | **PARKED-BY-RULING, still open** | the cost is real and now measured on our own files: the default pyramid carries three sheets (colour role 1, msn role 2, mask role 5) and `heights()` returns nothing; with the flag it carries four. Section 3.6 |
| **(k)** `.BTO` under the FO4CS target -- **RULED 2026-09-16** | **PARKED-BY-RULING, and implemented** | section 2's default/`--keep-bto`/stock legs all behave as the ruling describes |
| **(j)** where the ground cover lives -- the mask sheet's alpha (shipped) or the colour sheet's | **PARKED-BY-RULING** | confirmed on disk rather than assumed: exactly ONE sheet declares a `dxgiFormatCover` different from its `dxgiFormat`, and it is the **mask** (role 5, `dxgi 71` / `dxgiCover 77`). The shipped arm, not `--vt-cover-in-color` |
| **(l)** the CLI's `--candidates` default is `missing` while the panel's is `trees` | **GAP** | lane SHOWCASE1's library on disk records `candidates trees`, so the two halves still disagree by their own written record |

### 5.4 Plan §8.2--8.4 -- all three are PARKED-BY-RULING and none is this lane's

| | ruling, quoted | this lane |
|---|---|---|
| **8.2** per-asset LOD mesh generation | bungo: *"I think it should be done on individual asset level, but if a vanilla model features a lod mesh, it wins over ours."* then *"Except tree impostors"* | **PARKED-BY-RULING**, "after the lodgen queue". Nothing in this audit touches it |
| **8.3** preview an octahedral impostor as the game will react | bungo: *"Is there a way for me to preview a generated octahedral impostor for a tree in nifskope? I want to see it reacts as it does in game."* -- the page's own answer is "Today: NO" | **PARKED-BY-RULING (ASKED)**. The 23 ortho cards this audit decoded are the input such a preview would take |
| **8.4** quadtree seams | bungo, shown the three-panel diagram: *"Stitching looks good"* | **PARKED-BY-RULING**, hybrid LOD rung 3 |

### 5.5 The one-line audit of PERF1 row C, confirmed by test

PERF1 row C says a DEFAULT full bake can never reuse a library. **Confirmed, and
the exe says so itself in two different sentences, neither of which is a
warning.** Both are read out of my own bakes' censuses:

```
full bake, region (a):          native-library-build: rebuilt (not offered: this is not an incremental bake)
null incremental, region (a):   native-library-build: rebuilt (occluders are on and the per-model box is in
                                                      neither file)
```

So the reuse is unreachable from BOTH ends. A full bake is never offered it. A
null incremental with **0 of 9 chunks dirty and all 9 replayed** is offered it
and refused, because occluders are ON by default and the per-model occluder box
is stored in neither the `.lodo` nor the `.lodi` -- there is nothing on disk to
reload. That rebuild is `models 18.0 s` + `ladder 8.5 s` = **26.5 s of a 28.2 s
mesh stage**, on a run whose chunks cost nothing.

The one line: **under the shipped defaults the library is rebuilt from scratch
on every bake of any kind, and the only thing an incremental bake saves is the
chunks.** Measured, 45 s against 30 s on region (a), and the 15 s is the nine
chunks. This is a design gap and not a defect -- the census states the reason in
words every time rather than quietly reusing nothing -- and the way to close it
is a place to store the per-model occluder box, which is a format decision and
therefore not mine.

"""


def main():
    s = io.open(P, encoding='utf-8', newline='').read()
    if '## 5. Design gaps' in s:
        print('ABORT: section 5 is already in the report')
        return 1
    if ANCHOR in s:
        out = s.replace(ANCHOR, NEW + ANCHOR, 1)
    else:
        out = s.rstrip('\n') + '\n\n' + NEW
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
