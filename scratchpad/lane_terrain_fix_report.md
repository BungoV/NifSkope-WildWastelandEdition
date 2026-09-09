# LANE TERRAINFIX -- the terrain pyramid defects, Far Harbor, water fields

**BUILD PENDING.** The brief's gate was checked once at 16:45 and failed on its
first condition: `scratchpad/images_20260909/DONE` does not exist, so the
picture lane holds `release/NifSkope.exe` (`tasklist` also shows NifSkope pid
29964 live). `Fallout4.exe` was DOWN. Nothing was built, nothing under
`release/` touched. Resume: `scratchpad/terrainfix_20260909/PENDING.md`.

Everything below that carries a number was measured OFFLINE, against files
already on disk, by scripts that share no code with the writers they judge.

---

## 1. The VT pyramid's `_msn`: both 2026-09-07 defects, and what they cost

`lodgenBakeVtTile` sampled the height grid with `int( ngx )` -- NEAREST -- and
wrote north in green with up in blue. Both are the defects the per-chunk baker
lost on 2026-09-07; this was a COPY of those twelve lines and kept them.

**It was not a future consumer's problem.** With `--vt` on, the `.btr` chunk
sheets are ASSEMBLED from these tiles (`docs/LODGEN_TERRAIN_VT.md` 2.4), so a
`--vt` bake wrote the pre-2026-09-07 sheet into the same file name the stock
engine reads.

Fixed by making both paths call one function each:
`lodgenTerrainHeightAt` (bilinear through the quintic ease, four taps, hits
every VHGT sample) and `lodgenTerrainMsnPixel` (R east, G up, B north). The
pyramid's own re-encode after the box filter uses the same encoder.

**MEASURED** on the tile's own heights read out of a written `.lodt`, before
the block codec, against vanilla's shipped sheet for the same tile
(`scratchpad/terrainfix_20260909/vt_msn_sim.py`; the numbers are reproduced in
`sim_4.-60.36.txt` and `sim_4.-20.24.txt`):

| tile | statistic | before | after | vanilla |
|---|---|---|---|---|
| 4.-60.36 | grid-phase roughness | 2.001 | **0.209** | 0.065 |
| 4.-20.24 | grid-phase roughness | 2.000 | **0.150** | 0.031 |
| 4.-60.36 | left-diff by x mod 4 (%) | 98/0/0/0 | 36/98/99/98 | 100/63/64/63 |
| 4.-20.24 | left-diff by x mod 4 (%) | 93/0/0/0 | 40/93/95/93 | 100/67/68/67 |
| 4.-60.36 | mean UP as the shader reads it | 0.288 | **0.841** | 0.770 |
| 4.-20.24 | mean UP as the shader reads it | -0.151 | **0.943** | 0.894 |
| 4.-60.36 | mean Lambert light | 0.8047 | 0.8929 (+11.0%) | 0.8426 |
| 4.-20.24 | mean Lambert light | 0.5331 | 0.7004 (+31.4%) | 0.6267 |

**Controls** (`ww-control-calibration`, all printed by the script before any
verdict):

* KNOWN-ANSWER inputs for the roughness metric: a smooth analytic field reads
  **0.044**, the same field creased every fourth column reads **1.996** --
  separation **45.7x**, against the 5x gate lane MSN pre-registered. The
  pre-fix pyramid sheet reads 2.001, i.e. it sits AT the synthetic creased
  ceiling.
* The two sheets are produced from ONE height field on ONE sampling grid, so
  the only difference between the rows is the code under test.
* The roughness is read with each sheet's OWN channel convention (isolating the
  sampling defect) and the light with the CONSUMER's fixed convention -- up in
  green, which is what Fallout 4's terrain shader does -- isolating the channel
  defect. The mean-UP row is the sun-free form of the same statement: before
  the fix Sanctuary's ground described itself as facing slightly DOWNWARD.
* Floor/ceiling caveat, stated rather than buried: my implementation of lane
  LATTICE's statistic is a re-implementation (theirs is in `msn_curl.py`
  machinery), so my absolute numbers are not comparable with the 0.947/0.209
  pair in WW_CHANGES 2026-09-09 -- only the rows above, which share one
  implementation, are comparable with each other.

The 67.7% light / 92.0% shading-variation figures of 2026-09-07 are NOT claimed
for this path: those were measured on vanilla's high-detail sheets with a
different sun. What is claimed is the table above, with the sun stated in the
script (45 degrees, north-east).

**A third defect found on the way.** All five `0xFFFF8080U` flat-normal fills
in `lodgen.cpp` were the RENDERER's constant for a flat tangent-space normal,
where a `quint32` is RGBA bytes; these buffers are ARGB, so the value decoded
as east +1, up 0 -- sideways, wrong under both channel orders. Now
`LODGEN_MSN_FLAT = 0xFF80FF80U`. Reachable only where a tile or a mosaic row is
missing, which is why it never showed.

**Gate** (written, not yet run): `tests/spells/lodgen_terrain.sh` rung 4 bakes
the same chunk WITH `--vt` and asserts the assembled sheet's up channel is
green and its left-difference classes 1..3 clear 20% -- nearest sampling gives
0 -- with vanilla's own sheet read beside it as the known-answer control. The
statistic was run today on three real sheets and reads UP=G with
99/67/67/67 (vanilla), 82/44/48/43 and 76/39/61/39 (our two post-2026-09-07
chunk bakes from lane LATTICE's folder), so the 20% threshold separates the
defect from every bilinear sheet we have.

---

## 2. Far Harbor's 62 texels -- and DiamondCity's 167,936

The brief asked which texels and why. Answer, measured
(`scratchpad/terrainfix_20260909/lodt_vs_heightmap.py`, an independent decoder
of the progressive pyramid; the heightmaps are the authority because the
Commonwealth one is byte-identical to Bethesda's `Commonwealth_fine`):

| worldspace | texels | differing | landless cells | cause |
|---|---|---|---|---|
| Commonwealth | 37,748,736 | **0** | 0 of 36,864 | -- |
| NukaWorld | 4,326,400 | **0** | 0 | -- |
| DLC03FarHarbor | 20,207,616 | **62** | cell (14,-6) | the inherited seam |
| NukaWorldAmphitheater | 114,688 | **97** | 110 of 112 | the inherited seam |
| DiamondCity | 172,032 | **167,936** | 164 of 168 | that AND the default height |

The row flip is a control, not an assumption: read south-up instead of
north-up, the same comparison reads 2,246,992 / 19,053,132 differing.

**Far Harbor's 62 are ONE cell.** (14,-6) has no `LAND` record and is ringed by
eight cells that do. VHGT's row 32 and column 32 ARE the next cell's row 0 and
column 0, so this cell's row 0 belongs to the cell south of it and its column 0
to the cell west of it. The heightmap writes those samples; the `.lodt` refused
the plane outright and the caller substituted a bare 32767. Differing: row 0
cols 0..31 (32) plus col 0 rows 1..31 (31), less one -- col 0 row 19, where the
real terrain happens to be exactly 0 and the sentinel was accidentally right.
32 + 31 - 1 = **62**. The values are real terrain, -96 to -264 units.

**It is a writer defect, not a DDS defect.** Two independent reasons: the
heightmap reproduces Bethesda's own reference byte for byte, and FO4CS reads
the two files as ONE surface -- terrain from the `.lodt`, far shadows from the
DDS -- so a sample they disagree on is the ridge-that-casts-a-shadow-without-
being-drawn of 2026-09-05c, one row in from a hole in the landscape.

**The larger half the four-worldspace check did not report.** A landless cell's
whole plane was height ZERO, where the heightmap writes the worldspace's
default land height. DiamondCity's default is -2048, so 164 cells were wrong
across their full 32x32, not just at their seams: **97.6% of that worldspace's
texels**. The Commonwealth has no landless cell at all, which is why four days
of byte-identity gates said nothing.

**Fixed** in `src/lodtfile.cpp`: a landless cell's plane is the default land
height, raised on row 0 / column 0 by the south / west / south-west neighbours
under the same maximum rule as any other shared sample; the default does NOT
take part in that maximum (Far Harbor's default is 0 and its inherited row is
around -250, so a max against the default would have kept all 62 wrong); the
per-cell min/max covers the inherited row so a renderer culling on it cannot
cull what the file draws; and the plane fallback in `lodtWriteSource` now
encodes the default height instead of a bare 32767. One encoding helper,
`lodtHeightWord`, is now the only place a height becomes a word.

**Commonwealth stays byte-identical** in the sense the gate can check: it has
no landless cell, so not one byte of its blocks moves. Its FILE is 8 bytes
longer because of the version-2 header (see 3), and `WW_LODT_VERSION=1`
reproduces the old bytes exactly.

**Gate** (written, run RED today): `tests/spells/lodt_write.sh` bakes
NukaWorldAmphitheater's `.lodt` AND its shadow heightmap and requires all
114,688 texels to agree, after first asserting the worldspace HAS landless
cells so the check can fail. Run against the shipped 2026-09-05 files it reports
`110 of 112 cells carry no LAND record`, `first difference: cell (1,-5) col 0
row 0 lodt 32767 heightmap 32832`, `FAIL ... (97 differ)`, exit 1 -- two
independent implementations (numpy and pure python) agreeing on 97.

---

## 3. The water planes

**What was already right.** `EsmWorld::cellWater` resolves both fields: the
height is the cell's `XCLW` unless it is one of the three no-water sentinels,
otherwise the worldspace's `DNAM` default; the type is the cell's `XCWT`,
otherwise the worldspace's `NAM2`. The writer stores the resolved height, sets
the has-water flag from `CELL DATA` bit 1, and interns an explicit type into
the WATR table.

**The gap.** A cell whose type equals the worldspace default stores `0xFFFF`,
and the default's WATR form is deliberately NOT interned -- that is what keeps
"inherited" distinguishable from "explicitly this type". So the form appeared
in no section of the file and `0xFFFF` was a promise the file could not keep.
On the Commonwealth that is the COMMON case, not an edge one.

**Filled by header version 2**: `float defaultWaterHeight` at 0x98 and
`uint32 defaultWaterType` at 0x9C, appended AFTER the ten section offsets, so
every offset a version 1 reader uses is at the byte it was and only the first
section moved (0x98 -> 0xA0). The reader accepts 1 and 2 and exposes
`hasDefaultWater()` / `defaultWaterHeight()` / `defaultWaterType()`. The census
line gained `v<N> ... water <n> (worldspace default height <h> type <form>)`.

**The fallback**, because a behavioural change carries a way back that is exact
at its off value: `LodtOptions::headerVersion` and the environment override
`WW_LODT_VERSION=1` write the version 1 bytes. The CLI flag belongs in
`src/nifcli.cpp`, which this lane does not own -- hence the environment
variable, which needs no rebuild.

**THE CONSUMER REFUSES VERSION 2 TODAY, verified in its own source, not taken
from a document**: `E:\Projects\Fo4CommunityShaders\wt-fixfirst\src\FarField\
FarFieldLodtFormat.h:66` has `inline constexpr std::uint32_t kVersion = 1u;`
and line 378 `if (h.version != kVersion)` refuses. So a fresh bake deployed to
his mod folder without `WW_LODT_VERSION=1` will be rejected by the shipped
FO4CS reader. **That is a decision for bungo, not for this lane**: ship v1
bytes until FO4CS learns v2, or teach FO4CS first.

**Gate** (written, not yet run): `lodt_write.sh` reads the two fields back out
of the FILE and compares them with what the writer PRINTED (telemetry echoes
truth, never intent); asserts the first section is at 0xA0; asserts cells that
inherit AND cells that name their own type both exist and that every explicit
index is inside the WATR table; and proves the v1 fallback exact -- every
section offset is the v2 one minus 8 and everything past the header is
byte-identical.

Plane semantics are now written down in `docs/LODGEN_BTD_FORMAT.md` (the
per-cell table section and the new landless-cell paragraph).

---

## 4. Gates run, gates skipped, and the clocks

| artefact | mtime |
|---|---|
| `release/NifSkope.exe` | 2026-09-09 **15:30:27** |
| `src/lodgen.cpp` | 2026-09-09 16:27:07 |
| `src/lodtfile.cpp` | 2026-09-09 16:31:40 |
| `src/lodtfile.h` | 2026-09-09 16:31:24 |
| `tests/spells/lodgen_terrain.sh` | 2026-09-09 16:34:30 |
| `tests/spells/lodt_write.sh` | 2026-09-09 16:39:40 |

**The exe is older than every source this lane changed.** No harness result
from it would describe this code, so none was run.

| gate | state |
|---|---|
| `tests/spells/lodgen_terrain.sh` (incl. the PENDING ease gate + new rung 4) | NOT RUN -- needs a build |
| `tests/spells/lodt_write.sh` (incl. the new v2 + landless cases) | NOT RUN -- needs a build |
| the four/five-worldspace `.lodt` vs heightmap diff | RUN, on the SHIPPED files: 0 / 0 / 62 / 97 / 167,936 |
| the landless gate against the OLD state | RUN, goes RED (97 differ, exit 1) |
| the VT `_msn` simulation with its controls | RUN, offline, table in 1 |
| the `_msn` statistic on three real sheets | RUN (vanilla UP=G 99/67/67/67) |
| `g++ -fsyntax-only` on both changed sources | RUN, **RC=0** each |

The syntax pass proves they compile. It proves nothing about linking or
behaviour, and both harnesses are still owed.

**A collision to report.** `docs/LODGEN_BTD_FORMAT.md` was rewritten by ANOTHER
lane at 16:41:03 while this one held it -- it had already read this lane's
in-progress `lodtfile.cpp` and documented version 2, `WW_LODT_VERSION` and the
FO4CS `kVersion` consequence, with line numbers. `docs/LODGEN_TERRAIN_VT.md`
grew from 38,591 to ~44,000 bytes in the same window. My edits are additive and
anchored and were re-verified present at 16:44 (`landless` x2, `The water
fields, exactly` x1, `lodgenTerrainHeightAt` x1 in the VT doc), but if that
lane writes again from a stale copy they will be lost. The entry texts for
`WW_CHANGES.md` and `MISTAKES.md` are kept verbatim in
`scratchpad/terrainfix_20260909/fix07_ledgers.py` so the director can
re-splice. At 16:45-16:47 that lane wrote all four documents AGAIN, including its own
landless-cell paragraph in the `.lodt` contract; mine was then a duplicate of
it, so **I deleted mine and left theirs standing** (theirs is better placed,
right after Shared edges, and covers the no-neighbour case too). Re-verified at
16:50: `The water fields, exactly` x1, the reader invariants say "version is 1
or 2", the VT doc note x1, my WW_CHANGES entry and my four MISTAKES entries all
present, no duplication. One lane per file was not held here, and it was not
this lane's choice.

**Files changed** (7 + 2 ledgers, all LF-only, CR counts unchanged and
asserted by every patch script): `src/lodgen.cpp`, `src/lodtfile.cpp`,
`src/lodtfile.h`, `tests/spells/lodgen_terrain.sh`, `tests/spells/lodt_write.sh`,
`docs/LODGEN_BTD_FORMAT.md`, `docs/LODGEN_TERRAIN_VT.md`, `WW_CHANGES.md`
(mixed, 19,020 CR unchanged), `MISTAKES.md`. No commits.

---

## 5. Mistakes

Four entries written into `MISTAKES.md` the moment they were recognised:

1. **A fixed defect left standing in the second writer of the same file.** The
   2026-09-07 round fixed the chunk `_msn` and left `lodgenBakeVtTile`'s copy of
   the same twelve lines; the harness baked without `--vt`, so it never saw the
   other writer. Rule: a defect is a property of an OUTPUT -- grep for every
   producer of that file before closing it, and give both one function to call.
2. **A constant copied across a byte-order boundary.** `0xFFFF8080U` is the
   renderer's flat tangent normal in RGBA bytes; in lodgen's ARGB buffers it is
   a sideways normal. Rule: re-derive, never copy, and name the constant.
3. **A four-worldspace check that could not fail on three of them.** The
   Commonwealth has no landless cell and no non-default water case worth the
   name; the gate ran there. Rule: run the gate on the input that HAS the case,
   and make the harness assert that the input has it.
4. **A harness that printed its verdict and returned success.**
   `lodt_write.sh`'s python block printed `RESULT PASS`/`FAIL` and never called
   `sys.exit`, and it was the script's last command. Every run was a pass. It
   exits on its verdict now.

Not a mistake but worth the director's eye: the brief's item 2 named 62 texels
on Far Harbor. The same check on the other worldspaces on disk found 167,936 on
DiamondCity. Whatever produced the "four-worldspace" number was not looking at
DiamondCity, or was not looking at landless cells.

---

## 6. Finished-work skill review

**Loaded and used**: `nifskope-ww-lodgen` (the CLI table, the worldspace IDs,
the editing traps -- the heredoc/backslash warning and the "measure line
endings with Python byte counts" rule were both applied literally),
`nifskope-ww-build-verify` (the patch-with-a-script rule, and its "when you
CANNOT build" section, which is the only reason a syntax pass was run at all),
`ww-control-calibration` (the known-answer inputs and the floor/ceiling
separation that the roughness table quotes). `ww-artefact-localise` was named
in the brief and NOT loaded: the artefact was already localised by lane
LATTICE and this lane's job was the second writer, not the localisation. Saying
so rather than pretending.

**The skill that should exist, and does not.** Three separate times today the
same procedure was re-derived from first principles: *read a written `.lodt`
back into a full-rate height grid in Python, offline, and compare it with
something else.* It was written once in numpy for the four-worldspace diff,
once again in pure python for the harness (no numpy dependency in a spell), and
a third time to feed the `_msn` simulation. It is the procedure that found both
`.lodt` defects without a build, and the next `.lodt` question will need it
again. **Recommended: `ww-lodt-offline-read`** -- the progressive pyramid's
inverse (levels coarsest-first, `be*be` at the coarsest and the right/below/
below-right triple at every finer level), the per-cell table's dtype, the
north-up/south-up flip against the HeightMap DDS with the wrong flip as its
control, and the standing fact that the heightmap is the authority because the
Commonwealth one is byte-identical to Bethesda's own. I have not written it:
this lane owns no file under `.claude/skills`, and the two skill trees drift
(CONSTITUTION 1a), so the director should place it. The working code for it is
`scratchpad/terrainfix_20260909/lodt_vs_heightmap.py` (numpy) and the PYEOF3
block of `tests/spells/lodt_write.sh` (dependency-free).

A second, smaller candidate: **the offline "what did this bake defect cost"
pattern** -- reconstruct both the old and the new sampling from data already on
disk, encode both, and run the metric with its known-answer controls, instead
of waiting for a build slot. `vt_msn_sim.py` is the worked example. It may be a
section of the skill above rather than one of its own.

---

## Build (BUILD1) -- 2026-09-09

**The clocks, in one table** (CONSTITUTION 4). The exe is newer than every
source and every harness the two lanes touched, and than the two harness fixes
this lane had to make.

| artefact | mtime |
|---|---|
| `src/lodgen.cpp` | 16:27:07 |
| `src/lodtfile.h` | 16:31:24 |
| `src/lodtfile.cpp` | 16:31:40 |
| `src/nifskope.h` | 16:55:58 |
| `src/nifskope.cpp` | 16:57:58 |
| `tests/spells/render_shot.sh` | 17:12:18 (BUILD1's fixture fix) |
| `tests/spells/lodgen_terrain.sh` | 17:15:02 (BUILD1's rung-4 region) |
| `tests/spells/lodt_write.sh` | 17:17:21 (BUILD1's fallback check) |
| `Makefile.Release` | 17:21:52 (BUILD1's dependency stopgap) |
| **`release/NifSkope.exe`** | **17:22:05**, 17,796,608 bytes |
| `release/style.qss` | 17:22:05, identical to `res/style.qss` |

`make -j2` exited **0** (its own exit code gated the chain, not a grep), on a
game-down and NifSkope-free machine checked before the build and before every
run. The FIRST link, 17:09:31, was thrown away: it carried a stale
`btdterrain.o` and crashed -- see Mistakes.

### The gate table, all on the 17:22:05 exe

| gate | result |
|---|---|
| `tests/spells/render_shot.sh` | **15 checks, 0 failures, PASS**, exit 0 |
| `tests/spells/lodgen_terrain.sh` | **26 checks, 0 failures, PASS**, exit 0 |
| `tests/spells/lodt_write.sh` | **PASS**, exit 0, all three sections |
| four/five-worldspace `.lodt` vs heightmap | **0 differing texels** on all five |
| `tests/spells/lodt_open.sh` (version 1 file) | **23 checks, 0 failures, PASS**, exit 0 |
| `tests/spells/lodt_open.sh` (version 2 file) | **23 checks, 0 failures, PASS**, exit 0 |
| `tests/spells/btd_terrain.sh` | **13 checks, 0 failures, PASS**, exit 0 |
| `tests/spells/lodgen_identity.sh` | **RESULT PASS**, exit 0 |

Nothing in the list was skipped. Harnesses NOT run, and why: everything the
two lanes did not reach -- the impostor-card, atlas, merge, far-ring, resource
stack, texture-array, octahedral, collision, panel and workspace spells. The
one that comes closest is `lodt_btd.sh` (the FO76 `.btd` -> `.lodt` conversion,
~25 minutes): `btd_terrain.sh` covers the same reader against the same
Appalachia file in a fraction of the time, and no lane touched the conversion
path.

### The terrain numbers, measured on the built exe

**The `_msn` pyramid, rung 4 of `lodgen_terrain.sh`.** The sheet the pyramid
assembles now reads `UP=G  D0=76 D1=32 D2=51 D3=32` -- identical, statistic for
statistic, to the direct bake of the same chunk (`UP=G 76/32/51/32`), with
vanilla's own shipped sheet beside it as the known-answer control
(`UP=G 99/67/67/67`). Nearest sampling gives 0 on classes 1..3; the gate wants
above 20.

**The grid-phase roughness table reproduced**, offline, on a `.lodt` written by
THIS exe (`vt_msn_sim.py`, controls printed first: smooth analytic field
0.0437, the same field creased every fourth column 1.9955, separation 45.7x):

| tile | roughness before | after | vanilla | mean UP before | after |
|---|---|---|---|---|---|
| 4.-60.36 | 2.001 | **0.209** | 0.065 | 0.288 | **0.841** |
| 4.-20.24 | 2.000 | **0.150** | 0.031 | -0.151 | **0.943** |

Light: 0.8047 -> 0.8929 (+11.0%) and 0.5331 -> 0.7004 (+31.4%). These are the
lane's own numbers, recomputed here from the new file rather than quoted.

**The landless cell.** `lodt_write.sh`'s NukaWorldAmphitheater section reads
`110 of 112 cells carry no LAND record` (the assertion that the input HAS the
case) and then **0 of 114,688 texels differ**, where the shipped 2026-09-05
file read 97 and exited 1. Freshly baked and diffed against their own shadow
heightmaps, every worldspace agrees:

| worldspace | texels | differing now | before |
|---|---|---|---|
| Commonwealth | 37,748,736 | **0** | 0 |
| NukaWorld | 4,326,400 | **0** | 0 |
| DLC03FarHarbor | 20,207,616 | **0** | 62 |
| DiamondCity | 172,032 | **0** | 167,936 |
| NukaWorldAmphitheater | 114,688 | **0** | 97 |

The row-flip control is printed beside each: 19,053,132 / 4,316,218 /
2,247,054 / 18 / 1,726 differing read south-up. On DiamondCity that control is
weak (18 of 172,032), because 164 of its 168 cells are landless and sit at one
constant default height -- so for that worldspace the floor is the pre-fix
number, 167,936, not the flip.

**Version 2 and the water fields.** The writer prints `worldspace default
height 450 type 00000018` and the FILE's bytes at 0x98/0x9C say the same; the
first section is at 0xA0; 36,357 cells inherit the worldspace type and 507 name
their own, and every explicit index is inside the 15-entry WATR table.

**The version 1 fallback is exact**, but not in the way the harness asserted --
see Mistakes. Measured: 48,960 directory entries, **0** whose payload offset is
not exactly v1 + 8, **0** whose sizes changed, and the 9,437,644 bytes before
the directory and the 25,732,130 after it byte-identical.

### The `.lodt` set bungo has installed is now version 2

Backed up first as `<name>.lodt.bak-20260909` beside each, then written into a
scratch directory, `--verify-only`'d there, moved over, and `--verify-only`'d
again in place. NOT renamed to `.lodl` -- a later lane owns that.

| file | before | after | verify-only |
|---|---|---|---|
| Commonwealth.lodt | 35,953,286 (v1) | **35,953,294 (v2)** | rc 0, 36,864 samples, 0 mismatched |
| DLC03FarHarbor.lodt | 9,195,806 (v1) | **9,195,933 (v2)** | rc 0, 5,568 samples, 0 mismatched |
| DiamondCity.lodt | 53,220 (v1) | **53,148 (v2)** | rc 0, 256 samples, 0 mismatched |
| NukaWorld.lodt | 7,182,348 (v1) | **7,182,356 (v2)** | rc 0, 69,696 samples, 0 mismatched |
| NukaWorldAmphitheater.lodt | 38,104 (v1) | **38,303 (v2)** | rc 0, 128 samples, 0 mismatched |

Commonwealth and NukaWorld grew by exactly the eight header bytes: neither has
a landless cell. Far Harbor grew 127 and the Amphitheater 199 -- the eight plus
the blocks that now carry real terrain where they carried a flat sentinel.
DiamondCity SHRANK by 72 despite the eight: its 164 landless cells now hold one
constant default height, which the pyramid compresses better than the sentinel
plane it replaced.

**FO4CS WILL REFUSE THESE FILES TODAY.** `FarFieldLodtFormat.h` pins
`kVersion = 1u` (lane LODT1, wave 71) and rejects anything else. Either FO4CS
learns version 2 or the set is rewritten with `WW_LODT_VERSION=1`, which needs
no rebuild on our side and reproduces the version 1 bytes exactly. That is
bungo's call; the `.bak-20260909` files are the immediate way back.

### Finished-work skill review (BUILD1)

**Loaded and used**: `nifskope-ww-build-verify` (the gated chain, make's own
exit code, the exe renamed aside, the link-time stylesheet copy, the
exe-newer-than-sources test, and the patch-with-a-script-file rule -- every fix
here is a `fixNN.py` under `scratchpad/build1_20260909/` with an anchor-count
assertion and a CR-count assertion), `nifskope-ww-lodgen` (the CLI table, the
worldspace IDs, the byte-identity gates, the editing traps),
`nifskope-ww-render-shot` (the switch table and the new "the run hangs and
writes nothing" section lane NOPROMPT added -- it named `rc=124`-with-output as
the signature, which is what made the rc-0 result legible).

**The skill that should exist and does not**, and it cost this build an hour:
*prove a build is CONSISTENT, not merely successful*. Nothing in
`nifskope-ww-build-verify` catches a translation unit that make had no reason
to rebuild, and that is exactly what happened -- the chain's own
`test exe -nt source` passed while one object was two hours stale against a
header that had grown three members. The procedure is short and mechanical:
after any change to a header, list every `.cpp` that includes it, check each
object's mtime against the header's, and re-run qmake when an include is NEW
(qmake's dependency lists are frozen at generation time). **WRITTEN**, as a
section of `nifskope-ww-build-verify` rather than a skill of its own, since it
belongs to the same chain: "A successful build is not a consistent one
(2026-09-09)", in the LIVE tree
`E:\Projects\Claude\.claude\skills
ifskope-ww-build-verify\SKILL.md`, with the
grep-and-mtime check, the delete-the-object-and-relink fix, the qmake caveat
and the symptom to expect. The repo tree `<repo>/.claude/skills` holds only
`ww-control-calibration`, so there is no second copy to keep in step (checked,
CONSTITUTION 1a).

**A second candidate, declined with a reason**: "regenerate bungo's installed
`.lodt` set" is now a written script
(`scratchpad/build1_20260909/regen_lodt.sh`) with the backup-first,
write-to-scratch, verify, then install order. It will recur -- the `.lodl`
rename is already owed -- but it is fifteen lines of shell that read better as
the script than as prose, and the script is in the repo. If a third lane needs
it with different worldspaces, it becomes a skill.

**Declined outright**: the header-string-table rename used to build the dirty
fixture. It is four lines of Python, it exists in `render_shot.sh` now, and the
CLI limitation it works around is the thing that should be fixed instead
(`-no-gui set -f Name` could assign through `NifModel::set<QString>`), which is
a code change for a lane that owns `src/nifcli.cpp`.
