# LANE VTFIX -- V9a, the assembled terrain colour sheet

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, **no commits**.
Read first: `CONSTITUTION.md`, `scratchpad/lane_rename_report.md` (with its
BUILD2 section), `scratchpad/lane_terrain_fix_report.md` (with its BUILD1
section), `tests/spells/lodgen_terrain_vt.sh`, and
`scratchpad/specs_20260906/spec_terrain_vt.md` §4.4 and V9a/V9b/V9c. Skills
loaded: `nifskope-ww-lodgen`, `ww-control-calibration` (§6).

All work is under `scratchpad/vtfix_20260909/`. Every bake was run from a COPY
of `release/` (`ns_run/`, `ns_b1/`) so lane OFFSCREEN2 could keep linking; no
build was attempted.

---

## 1. Attribution

### 1.1 What the two lanes actually changed in `src/lodgen.cpp`

**RENAME** (`scratchpad/rename_20260909/p04_lodgen.py`, read in full) makes
exactly seven substitutions and no others:

* two comment lines (`rung 3b: the terrain virtual texture (.lodv)` -> `.lodt`);
* the refusal message string `cannot be named in a .lodv` -> `.lodt`;
* the two container path strings `"%1/%2.VT.%3.lodv"` and
  `"Terrain%1%2.VT.%3.lodv"` -> `.lodt`;
* three `// 0x...` comments beside `LAND_VERTEX_DESC`, `OBJ_VERTEX_DESC` and
  `OBJ_VERTEX_DESC_COLORS`, with the script asserting each of the three
  DECIMAL values still occurs exactly once, before and after.

Nothing in that list is on the path that computes a texel. It renames a file and
corrects three comments.

**TERRAINFIX** rewrote the twelve lines of `lodgenBakeVtTile` that build the
terrain normal (`src/lodgen.cpp:6144-6152`): `int( ngx )` nearest became
`lodgenTerrainHeightAt` -- the same bilinear-through-quintic-ease reconstruction
the chunk baker uses -- and the encode became the shared
`lodgenTerrainMsnPixel` (up in green). It also replaced the five
`0xFFFF8080U` flat-normal fills with `LODGEN_MSN_FLAT = 0xFF80FF80U`.

**The normal is not only the `_msn`'s.** `nrm` is computed BEFORE the colour
composite on both paths and its Z is the operand of the ground-cover slope gate
(`:6230-6232` in the tile baker, `:5279-5287` in the chunk baker):

```
theta     = acos( nrm[2] ) * 180/pi
gate      = clamp( (sTex + 5 - theta) / 10, 0, 1 )
coverByte = 255 * gate * dTex / coverFull
tintW     = (coverByte/255) * tintStrength      // 0.350 in this bake
color     = color + (coverTint - color) * tintW // <-- the COLOUR sheet
```

So a change to the normal reaches the colour sheet, and it reaches it ONLY
through `--cover`. `dominantBase`, by contrast, reaches the colour composite
unconditionally, at `:6182` and `:6199`. **That asymmetry is the discriminator**,
and it needs no rebuild.

### 1.2 The measurement

Four bakes of the harness's own fixture region (`-24 24 -17 31`, `--dim 4`,
`--data-root` the unpacked corpus), `scratchpad/vtfix_20260909/bake4.sh`, on a
copy of the BUILD2 exe:

| bake | flags | `Commonwealth.4.-24.24.DDS` sha256[0:16] |
|---|---|---|
| `vt_cover` | `--vt --cover` | `1f39d6aa0c64e83c` |
| `dir_cover` | `--cover` | `aee0793ae2c1576b` |
| `vt_nc` | `--vt` | `2704ba0de639a664` |
| `dir_nc` | (neither) | `2704ba0de639a664` |

All four sheets are 174,888 bytes, DXT1, 512x512, 8 mips.

* **with `--cover`: DIFFERS** -- this is V9a's failure, reproduced.
* **without `--cover`: BYTE-IDENTICAL.**

`dominantBase` is therefore **not** what moved: a scoping difference would paint
NULL-LTEX and `baseTex == 0` texels a different colour with cover off as well,
and with cover off the two sheets are the same file. (Reading agrees: the tile
baker scopes it to `lodgenVtFloorTo( cellX0, 4 )` at `:6018`, which for a dim-2
tile inside a dim-4 chunk is exactly the 16-cell set the chunk baker counts over
at `:4966-4976`; both take the first strict maximum over a key-sorted `QMap`, so
ties break the same way too.)

### 1.3 Where the differing texels are, per texel

`scratchpad/vtfix_20260909/ddsdiff.py` (a BC1/BC3 decoder re-typed from the
format, sharing no code with `lodgenWriteDds`) and `analyze.py`. Mip 0,
512x512, chunk `Commonwealth.4.-24.24`:

| comparison | differing | of | max per channel | mean abs |
|---|---|---|---|---|
| FLOOR: assembled vs direct, `--no-cover` | **0** | 262,144 | 0/0/0 | 0.000 |
| CEILING: direct `--cover` vs `--no-cover` | 207,945 (79.3%) | 262,144 | 36/35/36 | 12.355 |
| UNDER TEST: assembled vs direct, `--cover` | **43** (0.016%) | 262,144 | 17/17/14 | 5.186 |
| the `_msn` sheets, the gate's operand | 5,524 (2.1%) | 262,144 | 44/29/36 | 8.528 |

Locality of the 43 texels, against the two rival explanations:

| statistic | value |
|---|---|
| distance to the **chunk's outer boundary** | min 0, median 1, p90 3, **max 3** |
| within 4 texels of the outer boundary | **43 of 43 (100%)**, and that band is 3.1% of the sheet |
| distance to the nearest **interior dim-2 tile seam** | **min 37**, median 42, max 48 |

The `_msn`'s 5,524 differing texels are localised identically: 100% within 4
texels of the chunk's outer boundary, max distance 3.

Four texels is 128 world units at this density -- **exactly one heightfield
sample**, which is the bound `scratchpad/specs_20260906/spec_terrain_vt.md` §4.4
predicted in words before any of this was written: *"the msn within one
heightfield sample of a chunk boundary"*. The colour's 43 texels are a strict
subset of the msn's 5,524.

### 1.4 RENAME exonerated by measurement, not only by reading

`release/NifSkope.before.exe` is the BUILD1 link (17:22:05): TERRAINFIX in,
RENAME out. The same four bakes were run on it (`bake_b1/`). All **twelve**
sheets -- colour, `_msn` and `_data` for each of the four bakes -- are
**byte-identical** to the BUILD2 exe's.

**Verdict: TERRAINFIX moved the colour sheet's bytes, through the ground-cover
slope gate. RENAME moved nothing.**

---

## 2. Right or wrong -- RIGHT, with the controls

### 2.1 The controls (`ww-control-calibration`)

| part | what it is here | reading |
|---|---|---|
| known-answer, must read 0 | the same metric on the `--no-cover` pair, which the code says must be identical | **0** differing texels on all four chunks |
| floor, carrying the signal's own amplitude through the SAME lossy pipeline | that pair is the same content through the same BC1 encoder and the same 8-mip chain; only the tint is removed | **0** |
| ceiling, the same data with the property removed | the direct bake with `--cover` against the direct bake without it -- the most colour the mechanism under test can move on this ground | 207,945 texels (79.3%), max 36/255, mean 12.4 |
| the subject, between them | the assembled sheet against the direct one, both `--cover` | **43** texels, max 17/255 -- **0.021% of the ceiling's count** |
| independent replication | the same three readings on all four dim-4 chunks of the fixture, four different paint sets | below |

| chunk | FLOOR | CEILING | UNDER TEST | max | inside the 4-texel boundary band |
|---|---|---|---|---|---|
| `4.-24.24` | 0 | 207,945 | 43 | 17 | 43 of 43 (100%) |
| `4.-20.24` | 0 | 191,588 | 84 | 12 | 84 of 84 (100%) |
| `4.-24.28` | 0 | 6,979 | **0** | 0 | -- |
| `4.-20.28` | 0 | 21,760 | **0** | 0 | -- |

### 2.2 Which of the two boundary normals is right

The ground is continuous, so two chunk sheets that meet at a chunk seam hold
texels 32 world units apart -- the same spacing as any two adjacent columns
inside a sheet. `scratchpad/vtfix_20260909/seam.py` reads the step ACROSS the
seam and, as its control, the typical adjacent-column step well inside the
sheet (x = 100|101, 200|201, 300|301, 400|401 on both sides, averaged), because
the clamped bake's own edge column is *inside* the defect and using it would put
the defect in the control.

| bake | E/W seam step | interior step | ratio | its own edge step |
|---|---|---|---|---|
| assembled (ringed) `_msn` | 4.955 | 1.803 | **2.75** | 1.961 |
| direct (clamped) `_msn` | 7.312 | 1.797 | **4.07** | 3.310 |

North/south: ringed 4.600 / 1.603 = 2.87, clamped 6.068 / 1.606 = 3.78.

The interior control is the same to three digits on both bakes (1.803 vs 1.797),
which is the check that the two bakes agree everywhere except the band. Across
the seam the ringed normals are **32% more continuous** east/west and 24%
north/south, and the step onto a sheet's own last column is **41% smaller**
(1.961 vs 3.310) -- i.e. the clamped bake's edge column is the anomalous one.
Neither reaches 1.00, and I am not claiming it should: a chunk seam is also a
cell seam, where VHGT's shared samples take the maximum, and the BC1 block grid
lands exactly there.

**So the assembled sheet's 43 texels are the BETTER ones**, and the direct
bake's are the artefact. That is the same conclusion `spec_terrain_vt.md` §4.4
reached in words for the msn and the AO ("today's per-chunk bake clamps
`heightAt` at the chunk edge ... so both its AO and its outer-ring normals are
wrong within the clamp distance of every chunk boundary"); what nobody noticed
is that with `--cover` the COLOUR is a third consumer of that same normal.

### 2.3 Vanilla cannot arbitrate these 43 texels, and I am saying so

Against Bethesda's own shipped `Commonwealth.4.*.DDS` for the same four chunks:

| chunk | assembled vs vanilla | direct vs vanilla |
|---|---|---|
| `4.-24.24` | 99.6% differ, max 110, mean 18.649 | 99.6%, max 110, mean **18.649** |
| `4.-20.24` | 99.6%, max 123, mean 22.399 | 99.6%, max 123, mean **22.399** |
| `4.-24.28` | 99.8%, max 123, mean 23.733 | identical figures |
| `4.-20.28` | 99.8%, max 131, mean 21.723 | identical figures |

Our colour sheet differs from vanilla's on 99.6-99.8% of texels whichever
variant is chosen, and the two variants' distance to vanilla agrees to three
decimals. Vanilla's sheets carry no ground-cover tint and were composited from
finer source data, so the vanilla comparison has no power at 43 texels: it is
reported as a NEGATIVE result, not as support.

### 2.4 What is NOT measured

The magnitude of the V9a difference BEFORE lane TERRAINFIX. No exe older than
`release/NifSkope.before.exe` (BUILD1, 17:22:05, TERRAINFIX already in) exists on
disk, and building one would take the build slot lane OFFSCREEN2 holds. The
mechanism says it was larger and NOT boundary-confined -- the tile path did not
share `lodgenTerrainHeightAt` then, so the reconstruction did not cancel between
the two paths and the gate's operand differed wherever the ground is not flat,
not only inside the band. The band confinement measured today is itself evidence
for that (a difference in the shared reconstruction cannot confine itself to the
one region where the two paths' INPUT grids differ), but the number is owed and
unmeasured.

---

## 3. Re-pin, not fix -- and what the re-pin costs

**No source file was changed.** `src/lodgen.cpp`, `src/lodtfile.cpp` and
`src/lodtfile.h` are exactly as lanes TERRAINFIX, RENAME and BUILD2 left them.
The code is right; V9a's bar was not.

`tests/spells/lodgen_terrain_vt.sh` (21,440 bytes, LF-only, CR 0 before and
after; patch script `scratchpad/vtfix_20260909/p01_harness_v9.py`, original kept
beside it as `lodgen_terrain_vt.sh.orig`):

1. **Two more bakes**, `--vt` and direct, both WITHOUT `--cover` (about 11 s
   together).
2. **V9a-1, the `dominantBase` gate, exact.** With the tint off, the assembled
   colour sheet must be byte-identical to the direct bake **on all four dim-4
   chunks** of the fixture, not one. This is what the spec actually wanted, now
   asked on the pair where byte identity can honestly hold.
3. **V9a-2, with the tint on**, through a BC1 decoder re-typed from the format
   inside the harness (a check that decoded through `lodgenWriteDds` could not
   fail on `lodgenWriteDds`). Four bars and a floor:
   * every differing texel within **4 texels** of the chunk's OUTER boundary
     (that band is 3.1% of a sheet, so this is discriminating, not permissive);
   * no differing texel within **8 texels** of an interior dim-2 tile seam
     (measured nearest: 29) -- the `dominantBase`-rescope refuter;
   * max channel difference **<= 24/255** (measured 17);
   * differing texels **<= 0.05%** of a sheet (measured 0.016% and 0.032%);
   * **at least one chunk must differ** -- the floor. Without it, a change that
     silently disabled the cover pass, the tint or the decode would make all
     four bars pass on nothing.

**The check was run against three inputs before it was believed** (CONSTITUTION
rule 4 -- show the invariant failing):

| input | result |
|---|---|
| the real pair (`--vt --cover` vs `--cover`) | 127 differing texels over four chunks, **0 bars failed**, rc 0 |
| a sheet-wide difference (`--cover` vs `--no-cover`, the ceiling) | 428,272 texels, **16 bars failed** across the four chunks, rc 1 |
| two identical sheets (the no-cover pair) | 0 texels, **the floor fired**, rc 1 |

**Every number pinned in the harness is quoted with its measurement in the
comment beside it**, and the justification is this report plus the
`WW_CHANGES.md` entry. No bar was moved to fit a reading without one.

## 4. Gates

Run on a COPY of the BUILD2 exe (`scratchpad/vtfix_20260909/ns_run/`,
`release/NifSkope.exe` of 18:43:13) so lane OFFSCREEN2 kept the link slot; the
process table was checked once and showed neither `Fallout4.exe` nor
`NifSkope.exe`. **No build was needed or attempted** -- the change is a harness
and two ledgers, and the exe is already newer than every source the harness's
own preflight names (it passes that check in the log).

| gate | result |
|---|---|
| `tests/spells/lodgen_terrain_vt.sh` | **32 checks, 0 failures, RESULT PASS**, rc 0 (was 31 checks / 1 failure; V9a became two checks) |
| `tests/spells/lodgen_terrain.sh` | **26 checks, 0 failures, PASS**, rc 0 |
| `tests/spells/lodgen_identity.sh` | **RESULT PASS**, rc 0 |
| the pyramid `_msn` numbers from lane TERRAINFIX | **unchanged**: assembled `UP=G D0=76 D1=32 D2=51 D3=32`, direct `UP=G 76/32/51/32`, vanilla's own sheet `UP=G 99/67/67/67` as the control -- statistic for statistic what BUILD1 reported |

Logs: `scratchpad/vtfix_20260909/logs/`. Files changed: `WW_CHANGES.md`
(+81 lines, 0 removed, mixed file, **CR unchanged at 19,020**), `MISTAKES.md`
(+70, LF-only, CR 0), `tests/spells/lodgen_terrain_vt.sh` (342 -> 442 lines,
LF-only, CR 0), and `scratchpad/vtfix_20260909/`. **No commits**
(CONSTITUTION 8).

**Owed, and named so it is not lost:** the real defect is the DIRECT chunk
bake's edge clamp, not the assembled sheet. Giving the chunk baker the same
one-cell ring the tile baker has would make both paths correct AND
byte-identical, and V9a could go back to a plain `cmp`. It moves the edge bytes
of every terrain chunk sheet in every worldspace and re-baselines the
byte-identity gates, so it is bungo's call, not a lane's.

## 5. Mistakes

Four entries, written into `MISTAKES.md` at the root the moment they were
recognised:

1. **A byte-identity bar written for a sheet with three consumers, and only two
   counted.** V9a pinned the colour to byte identity while the spec had already
   exempted the normal at a chunk boundary; the colour reads that same normal
   through the ground-cover slope gate. Rule: before pinning a channel to byte
   identity, list every consumer of every operand it reads -- an exemption
   granted to an operand propagates to every channel that reads it.
2. **A comparison script reported DIFFERS for a file that did not exist.** My
   own `bake4.sh` printed `cover: DIFFERS / nocover: DIFFERS` from a run that
   had failed rc=127 and written nothing, because `cmp` returns 2 for a missing
   operand and the `||` branch cannot tell 2 from 1. Caught by the `MISSING`
   lines printed above it. Fixed (`p02_bake4_missing.py`) and the guard
   demonstrated firing.
3. **A control drawn one texel from the boundary sat inside the defect.** The
   first seam statistic used a sheet's last two columns as its control and made
   the CLAMPED bake look better (2.53 against 2.21); with the control taken from
   the sheet's interior the reading reverses (2.75 against 4.07). Found because
   the two bakes' controls disagreed by 84% where a control must agree.
4. **Patches typed into a heredoc**, which `nifskope-ww-lodgen` names as a trap
   by name -- first a Python patch whose anchor then matched zero times, and
   later a bash heredoc holding this very report section, which died on an
   unmatched quote. Both were caught (an assertion, and a parse error), so
   nothing was damaged; recording the repeat is itself the entry.

## 6. Finished-work skill review (CONSTITUTION 1a)

**Loaded and used.** `nifskope-ww-lodgen` -- the CLI table (`--vt`, `--cover`,
`--tex-dir`, `--terrain-region`, the worldspace IDs), the rule about copying
`release/` to a scratch directory for a long CLI run that must not block builds,
which is the whole reason this lane could measure anything while OFFSCREEN2 held
the link, and the editing traps (patch scripts with anchor counts, Python byte
counts for line endings) -- see mistake 4 for the two times I did not.
`ww-control-calibration` -- its five parts map onto section 2.1 one for one: the
known-answer input, the floor carrying the signal's own amplitude through the
same lossy pipeline, the ceiling from the same data with the property removed,
the independent replication, and the instruction to report the negative result
(vanilla) rather than quietly drop it. `nifskope-ww-build-verify` and
`nifskope-ww-resume-pending` were named in the brief and **not** loaded: no
build was needed or attempted, so neither procedure applied. Saying so rather
than pretending.

**The skill that SHOULD exist, and I am recommending it rather than writing it
into a tree other lanes hold: `ww-sheet-diff`** -- decode two of our DDS sheets
and say WHERE they differ. Three pieces of this lane were re-derived from first
principles and will be needed the next time a bake is questioned: a BC1/BC3
decoder that shares no code with `lodgenWriteDds` (written twice today, once as
`scratchpad/vtfix_20260909/ddsdiff.py` and once inside the harness, because a
spell may not import from `scratchpad/`); the locality statistic that separates
a chunk-boundary defect from a tile-seam defect (distance to the outer boundary
AND to the interior seam, with the band's share of the sheet printed beside it,
or 100% within 4 texels means nothing); and the seam-continuity test with its
interior control, which is the only instrument here that says which of two
variants is RIGHT rather than merely which is different. The working code is
`ddsdiff.py`, `analyze.py`, `analyze2.py` and `seam.py` under
`scratchpad/vtfix_20260909/`, and the trap it must carry is mistake 3. **The
director should place it in both skill trees**, since they drift.

**Declined, with reasons.** A skill for mistake 1's lesson: it is one paragraph
and it belongs beside the thing it guards, which is where it now is -- a comment
at both halves of V9a and an entry in `MISTAKES.md`. A skill nobody would think
to load is a skill nobody loads. And a skill for running three lodgen harnesses
on a scratch copy of the exe: four lines of shell, already the second half of a
`nifskope-ww-lodgen` bullet.
