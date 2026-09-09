# Lane FARRING1 — proxy meshes at rings 2 and 3, and a BC1 atlas for the stock target

Repo `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, no commits (bungo's "Not yet").
This session executes only under the repo, so the report lives at `scratchpad/lane_farring_report.md`
in the tree (untracked) — there is no writable scratchpad outside it.

---

## 1. Skills loaded

* `nifskope-ww-lodgen` — the LOD generator: build incantation, the CLI, byte-identity gates,
  the manifest, the merge/atlas/arrays passes, the editing traps.
* `nifskope-ww-build-verify` — the gated chain, `tools/ww_build.sh`, `make`'s own exit code,
  the exe under bungo's window renamed rather than killed, a number beside every verdict.
* `nifskope-ww-panel-style` — `wwHeading`/`wwMakeScrubField`/`wwMatchFieldStyle`/`wwGuardWheel`,
  one setting per row, the three bands, the self-test floors.

Read: `HANDOFF.md` top, `WW_CHANGES.md` 2026-09-06k/l, `docs/LODGEN_VERTEX_PACKING.md`,
`docs/LODGEN_IMPOSTOR_SPEC.md`, and in `src/lodgen.cpp` `lodgenBuildObjectChunk`,
`lodgenMergeChunkShapes`, `lodgenBuildAtlas`, `lodgenWriteDds`.

Game-up check at the start: `tasklist /FI "IMAGENAME eq Fallout4.exe"` → "No tasks are running"
(game down).

---

## 2. What vanilla actually ships, measured

All of section 2 is offline: no exe, no game. The readers are
`scratchpad/btocount.py` (a FO4 `.BTO`/`.BTR` shape reader), `scratchpad/vanilla_rings.py`
(the per-ring roll-up) and `scratchpad/ring_census.py` (MNAM slot fill off `Fallout4.esm`).
`btocount.py` self-checks every shape it reads: it recomputes
`Data Size = (VertexDesc & 0xF) * 4 * NumVertices + NumTriangles * 6` and aborts the file on a
mismatch, so a wrong field offset cannot pass itself off as a triangle count. All 465 vanilla
chunks passed that check.

### 2a. Vanilla's object-LOD triangle counts per ring (whole Commonwealth)

`E:\Tools\Fallout 4\DataUnpacked\Data\meshes\terrain\commonwealth\objects\`, all 465 `.BTO`:

| ring | dim | chunks | triangles | vertices | tris / chunk | **tris / cell** | shapes / chunk |
|---|---|---|---|---|---|---|---|
| 0 | 4  | 344 | 3,708,637 | 5,739,219 | 10,781 | **673.8** | 1.97 |
| 1 | 8  | 97  | 1,101,390 | 2,034,737 | 11,355 | **177.4** | 2.07 |
| 2 | 16 | 20  | 89,696    | 155,630   | 4,485  | **17.5**  | 1.85 |
| 3 | 32 | 4   | 10,594    | 17,127    | 2,649  | **2.6**   | 1.25 |

Per cell, vanilla's far rings are almost nothing: ring 1 is 26% of ring 0, ring 2 is 9.9% of
ring 1, ring 3 is 15% of ring 2. And the *coverage* collapses too — only 20 chunks in the whole
worldspace at ring 2 and 4 at ring 3, against 344 at ring 0.

The chunk that covers Sanctuary at each ring:

| ring | chunk | shapes | vertices | triangles |
|---|---|---|---|---|
| 0 (dim 4)  | (−20, 24) | 2 | 33,703 | 24,482 |
| 1 (dim 8)  | (−24, 24) | 2 | 14,566 | 7,140 |
| 2 (dim 16) | (−32, 16) | — | — | **no file at all** |
| 3 (dim 32) | (−32, 0)  | 1 | 762 | 416 |

### 2b. Why vanilla's ring 2 over Sanctuary is empty — and what that does to the harness

Vanilla fills a ring from the base's MNAM slot for that ring (dim 4 → slot 0 … dim 32 → slot 3),
and our generator does the same (`src/lodgen.cpp:2889`, `base.models[qMin(lodLevel,3)]`).
Measured over `Fallout4.esm` with positional MNAM slots (four fixed 260-byte records; the reader
asserts every STAT payload is a whole number of them — 0 violations on STAT, the 598 odd payloads
are all FURN, whose MNAM means something else and which `src/esmdata.cpp:449` also ignores):

* 28,932 LOD-bearing bases; slot fill **0: 3,081 · 1: 2,940 · 2: 456 · 3: 51**.
* Over the dim-16 chunk (−32,16) — 19,507 REFRs, SCOL parts not walked — **0 refs whose base
  fills slot 2**, 19,507 without.
* Over the dim-32 chunk (−32,0) — 148,362 REFRs — **0 refs whose base fills slot 3**.

So the far rings over Sanctuary hold **nothing** unless the generator is told to substitute
(`slotFallback`, the panel's "Use a nearer LOD slot when the ring's is empty") or to stand
placements on impostor cards. Cards are alpha-tested quads and are excluded from simplification
by design, so a card-filled chunk would make the gate vacuous. **The far-ring harness therefore
builds ring 2 and ring 3 with slot fallback on**, which is exactly the case the panel's own
tooltip warns about — "far chunks grow to many times vanilla's size" — and exactly what a proxy
pass is for. That needed one new CLI flag, `--slot-fallback` (the option and the panel toggle
already existed; only the CLI had no way to reach it).

### 2c. The atlas format claim — vanilla's is DXT1, not BC3

`src/lodgen.cpp:5161` said "BC3, like vanilla's sheet". Measured headers of
`E:\Tools\Fallout 4\DataUnpacked\Data\textures\terrain\commonwealth\objects\`
(`scratchpad/ddshdr.py`):

| file | size | mips | fourCC | bytes |
|---|---|---|---|---|
| `Commonwealth.Objects.DDS`   | 4096×2048 | 13 | **DXT1** | 5,592,552 |
| `Commonwealth.Objects_n.DDS` | 4096×2048 | 13 | **BC5U**  | 11,184,976 |
| `Commonwealth.Objects_s.DDS` | 4096×2048 | 13 | **BC5U**  | 11,184,976 |

So the diffuse sheet is BC1 and ours was BC3 — twice the memory per sheet, which is bungo's
point. A second thing falls out of the same measurement and is **not** in this lane's scope:
vanilla's *normal* sheet is BC5U too, and ours is BC3. That is another 2× on a second 11 MB
sheet. Recorded as a doubt in section 5, not done.

---

### 2d. Segments per ring — the regroup is future-proofing, not a fix

Vanilla's own far chunks carry ONE segment: across all 465 shipped files, every dim-8, dim-16
and dim-32 shape has exactly one (201, 37 and 5 shapes respectively), while at dim 4 they carry
up to sixteen (519 of 719 shapes have all sixteen, the rest fewer). Our generator agrees —
`segs = ( dim == 4 ) ? 16 : 1` at `src/lodgen.cpp:2662` — so at rings 1–3 the centroid regroup
this lane implements collapses to the single run it started with. It is correct code with no
work to do today, and the harness's "every centroid in its own segment's cell" check is a floor
for the day a far chunk grows a per-cell grid, not a measurement of one now. Said plainly in
`WW_CHANGES.md` and in the spec.

---

## 3. What changed, step by step

Every source edit was applied by a patch script written with the Write tool (never a heredoc),
each asserting its anchor matched exactly once and that the file's CR count did not move.
`src/` and `docs/` are LF-only and stayed at 0 CR; `WW_CHANGES.md` is mixed and its CR count is
19,020 before and after.

### 3a. `src/lodgen.h` — the contract (`scratchpad/fix_farring_1.py`)

* `lodgenBuildAtlas` takes `bool bc1` before its error out-parameter, with the measurement in
  its doc comment.
* New `struct LodgenSimplifyOptions` (`enabled`, `ratio8` = 1.00, `ratio16` = 0.35,
  `ratio32` = 0.20, `errorWorld` = 32 world units at ring 0, `minTris` = 8), plus
  `lodgenSimplifyRatio()` and `lodgenSimplifyFarRings()`.

### 3b. `src/lodgen.cpp` — the pass and the sheet (`fix_farring_2.py`, `fix_farring_10.py`)

* `lodgenWriteDds` gains `bool bc1Alpha = false`. The mip filter was
  `( bc3 ? ( acc[3] / 4 ) : 0xFFU ) << 24` — BC1 alpha forced opaque below mip 0. It now carries
  alpha when the caller asks; the default leaves every existing BC1 caller (the terrain bakes,
  the emissive sheets, all of them opaque) byte for byte as it was. The two-line replace was
  SCOPED to `lodgenWriteDds`'s body: the identical lines appear again in
  `lodgenEncodeArrayLayer`, and the anchor assertion caught that before anything was written.
* `lodgenBuildAtlas( ..., bool bc1, ... )` writes the diffuse as `!bc1` BC3 / `bc1` BC1, and the
  comment that claimed vanilla's sheet is BC3 now carries the measured header.
* **`lodgenSimplifyFarRings`**, ~300 lines, immediately after `lodgenMergeChunkShapes`:
  * the ring comes from the chunk's file name and is cross-checked against the shape's `Scale`;
    a disagreement leaves the shape alone and says so on stderr;
  * shapes with an alpha property are skipped whole; groups whose object index appears on a `C`
    manifest line are skipped; groups of `minTris` triangles or fewer are skipped;
  * triangles are grouped by `(identity index from colours R+G, layer from UV2.y)`, each group
    simplified alone with `meshopt_simplifyWithAttributes` under `meshopt_SimplifyErrorAbsolute`;
  * attributes: normal (weight 0.5 × bound), UV (1.0), sky visibility (0.25), AO (0.25), sway
    (0.25), ground contact (0.25) — the weights are fractions of the error bound so one knob
    scales the whole metric;
  * every group is asked for at least two triangles and restored whole if the simplifier returns
    nothing, which is what makes the identity set invariant;
  * segments regrouped by triangle centroid, vertices compacted to the survivors, bounding
    sphere and the node's multi-bound AABB recomputed;
  * `fix_farring_10.py` re-fetches the `Vertex Data` and `Segment` indices AFTER
    `setState( Processing )` and before `updateArraySize`, the way the merge does — an index held
    across a shrink is the kind of thing that works until it does not.

### 3c. `src/nifcli.cpp` (`fix_farring_3.py`, `fix_farring_3b.py`)

* `--no-simplify`, `--simplify8|16|32 R`, `--simplify-error UNITS`, `--atlas-bc1`,
  `--slot-fallback`, `--dump-geometry FILE`; `cmdLodgen` takes the three new settings; the pass
  runs right after the merge in region mode; usage text for all of them.
* `--slot-fallback` reaches BOTH ways into the chunk builder (the region sweep and the single
  `--objects` chunk), so the flag means the same thing whichever is used.
* **`--dump-geometry`** is the question the gates needed: one `G` line per shape with what it
  weighs and the raw counts its own invariants stand on — `segbad` (triangles whose centroid is
  in another segment's cell), `outofchunk`, `sphereout`, `aabbout` — plus an `i` line listing its
  object identity indices. Every number is a raw count and every one can be non-zero on a real
  file; the comparisons belong to the harness. Tolerance is one half-float ulp at 4096
  (4 miniature units), because positions are halves and the bounds containing them were computed
  from floats.

### 3d. `src/lodgenmanager.cpp` — the panel (`fix_farring_4.py`)

Under *Object LOD chunks*, after the slot-fallback row and before the atlas row: a
**Far-ring simplification** check box and four numbers on their own rows — *Ring 1 / Ring 2 /
Ring 3 triangles kept* and *Simplification error*. All four are `QDoubleSpinBox` +
`wwMakeScrubField` (which brings `wwGuardWheel` with it), one setting per row through the
section's `Form` grid, whole-word labels with the explanation in the tooltip, and they grey with
their own box on top of greying with the section. They are **not** added to `applyTarget`'s hide
list, so both targets offer them: a smaller mesh is a smaller mesh for the stock engine too.
Settings persist under `LodGeneration/simplify*`. The run calls the pass after the merge, and
the atlas call now passes `bc1 = !fo4cs()` with the format named in the summary sentence
("atlas written (BC1)" / "(BC3)").

### 3e. `src/nifskope_ui.cpp` — `WW_LODGEN_TEST` (`fix_farring_5.py`)

* the five new object names join the roll call;
* the number-field floor rises **8 → 12** with the four new numbers, or it stops being a floor;
* three new behaviour checks: the defaults fall away with distance
  (`ring2 < 1.00 && ring3 < ring2`), the ratios grey with their box **measured in both states**,
  and the rows survive the stock target **and** the FO4CS one.

### 3f. `tests/spells/lodgen_farring.sh` (new)

Rings 0, 1 and 2 built twice each over Sanctuary (−20,24)..(−19,25) with `--arrays --atlas
--no-ao`, `--no-simplify` against the pass, compared shape by shape through `--dump-geometry`:
triangles within 10 points of the ratio, vertices down with them, identity sets equal, segment
count unchanged with no stray or out-of-chunk centroid, every vertex inside its bounding sphere
and its node's AABB, manifest placement rows byte-identical, every `A` line naming a live block,
**ring 0 byte-identical chunk and manifest**. Then the atlas: DXT1 under `--atlas-bc1`, DXT5
without it, `_s` DX10 BC5 in both, the BC1 sheet 0.45–0.55× the BC3 one, vanilla's own header
re-measured as DXT1, and **punch-through blocks counted per mip** with mips 1 and 2 required to
carry them — the half of the check that fails on the old mip filter and passes on the new.
Floors: at least one shape cut and at least 200 triangles removed, or FAIL.

Ring 3 is opt-in (`FARRING_RING3=1`): the ring-3 chunk over Sanctuary is 21,498 references
before SCOL expansion, minutes rather than seconds.

### 3g. Docs (`fix_farring_6.py`, `_7.py`, `_8.py`, `_9.py`)

* `docs/LODGEN_IMPOSTOR_SPEC.md` — a **Far rings** section under *Chunk shapes*: the pass, the
  grouping, what is not cut, the error-bound arithmetic, the MNAM measurements, the atlas format
  and the gate.
* `docs/LODGEN_VERTEX_PACKING.md` — *What the far-ring simplifier owes this table*: why the
  identity set is invariant and why `(ref, part)` still resolves at rings 2 and 3 after a cut.
* `WW_CHANGES.md` — `## 2026-09-06n`, above `2026-09-06l`, with his words, every measured number
  and an explicit **Not done**. CR count unchanged at 19,020.
* `HANDOFF.md` — the FARRING1 ledger line closed with BUILD PENDING and what landed, plus a
  narrative block above the newest one.
* `docs/MISTAKES.md` — two entries: *A comment that asserted someone else's file format, and was
  the argument for a decision*, and *A mip chain that dropped the channel its top mip carried*.
* **The skill paragraph could not be written**: `nifskope-ww-lodgen` lives at
  `C:\Users\bungo\.claude-b\skills\`, outside this session's working directory, and the write is
  refused. The paragraph the overseer should add to `nifskope-ww-lodgen` is:

  > FAR RINGS (2026-09-06n): `lodgenSimplifyFarRings` runs LAST, after the merge, and cuts rings
  > 2 and 3 to `ratio16` 0.35 / `ratio32` 0.20 (`--no-simplify`, `--simplify8|16|32 R`,
  > `--simplify-error UNITS`, panel row *Far-ring simplification*, both targets). Ring 0 is never
  > touched — the byte-identity gates depend on it. The cut is per (identity index, UV2.y layer)
  > group so no collapse crosses an object or a layer, meshoptimizer creates no vertices so no
  > channel is interpolated, and every group is asked for ≥2 triangles and restored whole on a
  > null result: **the identity set of a chunk is invariant under the pass**. Alpha-tested shapes
  > and `C`-line cards are never cut. MEASURED: 0 of 19,507 refs in the ring-2 chunk (−32,16) and
  > 0 of 148,362 in the ring-3 chunk (−32,0) fill their ring's MNAM slot (456 and 51 bases in the
  > whole plugin do), so a far ring is EMPTY without `--slot-fallback` (new on the CLI) or
  > `--impostors`; vanilla ships 20 dim-16 chunks and 4 dim-32 for the whole Commonwealth against
  > 344 at dim 4, at 17.5 and 2.6 triangles a cell against 673.8. Vanilla's atlas diffuse is
  > **DXT1** (4096x2048, 13 mips, 5,592,552 bytes), its `_n` and `_s` both BC5U — `--atlas-bc1`
  > (the stock target's default) matches the diffuse; `lodgenWriteDds` needed `bc1Alpha` because
  > its mip filter forced BC1 alpha opaque. New question: `lodgen --dump-geometry FILE.BTO`
  > (per-shape weight + `segbad`/`outofchunk`/`sphereout`/`aabbout` + an `i` identity line).
  > Gate `tests/spells/lodgen_farring.sh`; ring 3 opt-in with `FARRING_RING3=1`.

---

## 4. Verdicts

**BUILD PENDING.** `bash tools/ww_build.sh` is refused to this session ("This command requires
approval"), and so is every `release/NifSkope.exe` invocation — account B allowlists no NifSkope
command, as it did for MERGE2/MERGE3/CARDS1/MO2LOAD. **Nothing in this lane was compiled and no
harness was run.** The overseer owns the build and the gates.

What WAS verified, and how:

| what | how | result |
|---|---|---|
| game down before any work | `tasklist /FI "IMAGENAME eq Fallout4.exe"` | "No tasks are running" |
| every anchor unique | each patch script asserts `count == 1` before replacing | 38 anchors, all 1; one (`C2`) matched **2** and the assert stopped the run before anything was written — it was scoped to `lodgenWriteDds`'s body and re-run |
| `src/`, `docs/` stay LF-only | Python byte counts before and after | CR 0 → 0 on all five sources and three docs |
| `WW_CHANGES.md` CR count | Python byte count, asserted in the script | 19,020 → **19,020**, LF 21,950 → 22,081 |
| the C++ is not structurally broken | `scratchpad/brace_balance.py` (comment- and literal-aware) over all five edited files | braces/parens/brackets **+0 +0 +0** on each |
| the harness's embedded Python parses | `scratchpad/check_harness.py`, `compile()` on both heredoc blocks | 2 blocks, 128 and 75 lines, both compile; heredoc markers balanced |
| the vanilla readers are self-checking | `Data Size` recomputed per shape from the descriptor | 465 of 465 chunks agreed; 0 STAT MNAM payloads of a non-slot length |

Numbers owed on the overseer's build, all of them of OUR output (every number in section 2 is of
vanilla's files or of `Fallout4.esm`, offline):

* triangles and vertices per ring, before → after, per chunk;
* the achieved ratio against 0.35 / 0.20;
* identity sets equal at each ring;
* ring 0 byte-identical, chunk and manifest;
* atlas fourCC per target, sheet sizes, punch-through blocks per mip;
* `WW_LODGEN_TEST` counts with the exe timestamp beside them.

Chain the overseer should run, once built:
`lodgen_farring.sh`, then `lodgen_merge.sh`, `lodgen_texture_arrays.sh`, `lodgen_card_arrays.sh`,
`lodgen_identity.sh`, `lodgen_impostor_cards.sh`, `lod_generation.sh`.

---

## 5. Not done, and doubts

1. **Nothing was built or run.** Every claim about our own output is unmeasured. That is the
   whole of doubt 1 and it dominates the rest.
2. **The ratio gate can legitimately miss.** meshoptimizer stops short of a target on topology,
   and a far chunk of many small disconnected LOD shells — which is exactly what
   `--slot-fallback` produces — is where that happens. The harness prints the achieved ratio
   beside the asked one, so a miss is diagnosable rather than mysterious, but it will read as a
   FAIL. If it does, the honest fixes are a looser tolerance or a per-group report of which
   shells refused, not a quieter check.
3. **The chunk builder silently drops geometry past 65,535 vertices a bucket**
   (`src/lodgen.cpp:3141`, `continue; // bucket full`). Pre-existing, not this lane's, but the
   ring-2 build with slot fallback is the most likely thing yet to hit it, so our ring-2 "before"
   number may already be a truncated chunk. Both builds truncate identically, so the comparison
   stays valid; the absolute number does not.
4. **Ring 3 is not measured by default.** 21,498 references before SCOL expansion in the chunk
   over Sanctuary. `FARRING_RING3=1` runs it.
5. **`--ao-grey` breaks the grouping.** It writes AO into R and G, which is where the identity
   index lives, so under that debug flag the pass groups by AO level and will barely cut
   anything. It is a debug flag, it is not defended against, and it is not detectable from the
   written file.
6. **Vanilla's normal atlas is BC5U and ours is BC3** — measured on the same shipped files, 13
   mips, 11,184,976 bytes. Another 2× on a second 11 MB sheet, and BC5's two channels are what a
   tangent-space normal actually needs. Out of this lane's scope (the brief asked for the diffuse
   only) and left alone deliberately, because changing the normal sheet's format changes what
   every consumer's shader must reconstruct Z from.
7. **The alpha rule is an over-approximation.** The brief says "alpha-tested tree cards are NOT
   simplified"; the implementation refuses to simplify ANY shape with an alpha property, because
   after the merge a card quad and an alpha-tested fence are one shape and cannot be told apart
   from the file. That is the safe direction, but it means an alpha-tested mesh with real surface
   area keeps every triangle. If Sanctuary's far rings turn out to be mostly alpha-tested, the
   measured saving will be small and the rule is the first thing to revisit — a per-GROUP
   alpha rule would need the alpha state carried per group, which the file does not record.
8. **The centroid regroup does no work today** (section 2d): vanilla and we both write one
   segment at rings 1–3. The code is right and the check is a floor, not a measurement.
9. **`--slot-fallback` on the CLI is new and beyond the literal brief.** It was necessary: the
   harness has nothing to measure without it (section 2b). It changes no default.
10. **The skill file could not be updated** (section 3g); the paragraph is quoted there for the
    overseer.
11. **`scratchpad/` is a new untracked directory in the repo.** This session cannot write outside
    the working directory, so the report and the patch scripts live there, as earlier lanes' did.
    Nothing in it is referenced by the build.
