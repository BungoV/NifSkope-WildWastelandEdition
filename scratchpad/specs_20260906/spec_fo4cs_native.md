> **CORRECTION, before you read the card figures.** This document counts
> octahedral frames as `(N+1)^2`, treating N as the number of grid CELLS. The
> implementation counts them as `N^2`, treating N as the number of frames per
> side: `src/nifskope_ui.cpp` maps `u = i / (octN - 1) * 2 - 1` over
> `i` in `0..octN-1` and writes a sheet `octN` frames wide, and
> `tests/spells/lodgen_octahedral.sh` asserts exactly that - `WW_IMPOSTOR_OCT=4`
> produces 16 frames, not 25. So this document's "N=8, 81 frames" is the
> implementation's **9x9**, and its "N=4, 25 frames" is **5x5**. Every card cost
> here is therefore one grid step too large. Corrected, at the derived 0.0125 MiB
> a frame and 30 far-field tree bases:
>
> | grid | frames | MiB a base | 30 bases |
> |---|---|---|---|
> | 4x4 | 16 | 0.200 | 6.0 MiB |
> | 6x6 | 36 | 0.450 | 13.5 MiB |
> | **8x8** | **64** | **0.800** | **24.0 MiB** |
> | 9x9 | 81 | 1.013 | 30.4 MiB |
>
> bungo chose **8x8, 64 frames, 24.0 MiB**, which is also what
> `tools/bake_impostor_cards.sh` already defaults to (`OCT=8`). Read every other
> card number in this document with the same one-step shift.

# FO4CS-native far-field format (`.lodg` + `.lodi`) and the staged migration

**What this changes.** Object LOD stops being 481 stitched `.BTO` pictures of the world and becomes two files per worldspace — one deduplicated geometry library and one flat table of quantised instance records — which takes the Commonwealth's object far field from **538.3 MiB on disk to 60.2 MiB** (29.8 MiB if the new impostor cards are not baked), from **125.9–370.0 MiB resident to 63.8 MiB**, and from **343–1,795 engine draws to 8–14** in the main view. It also deletes, by construction rather than by fix, the silent 6.17% geometry drop the current writer commits at `lodgen.cpp:3144`. **The first lane to run is Lane 0 (BASELINE)** — a checked-in `stock_baseline.sha256` from one named build — because every other lane's standing gate is a byte-identical stock bake and there is currently nothing to be identical *to*.

**Status: specification, read-only.** Nothing was written to either repo, nothing was built, no GUI was launched.
Numbers are attributed: **[inv]** inventory, **[par]** parity, **[con]** consumer, **[cei]** ceiling, **[pri]** priorart, **[nat]** measured by me this session, **[arith]** derived arithmetic.

**Units.** Every byte figure in this document is **binary**: KiB = 1,024 B, MiB = 1,048,576 B, GiB = 1,073,741,824 B. Where a source measured in decimal MB the raw byte count is given and the MiB figure derived. Every ratio is MiB over MiB. The previous draft mixed the two; every headline ratio is recomputed below and none of them survived unchanged.

---

## 0. Corrections to the brief, and to the previous draft

Three of the five investigators independently caught the same error in the brief; the hostile reading of the first draft caught seven more in the draft itself. All are settled here before any arithmetic is spent.

### 0.1 Corrections to the brief

| the brief says | measured truth | decision |
|---|---|---|
| the ring-2 chunk is 55,293 tris / 73,416 verts | those are the **non-alpha** subtotals. The chunk is **87,443 tris / 145,569 verts**, 29 shapes, 5,193,450–5,200,284 B = **681–682 B a placement** over 7,626 placements [par][pri][cei] | use 87,443 / 145,569 / **682 B**. The alpha-tested half (32,150 tris, 72,153 verts) is exactly the half the far-ring simplifier refuses to touch, so "decimation is topology-bound at 15%" sits on top of a 37% exclusion |
| a 12×12 grid of ring-2 chunks is 0.7 GB | the Commonwealth is not a dense grid [cei] | the whole four-ring baseline is recomputed in §0.2 from the 682 B/placement anchor |
| — | library size: 3,355 distinct MNAM model paths / 289,003 v / 156,818 t / 9,648,037 B [pri] vs 2,986 models / 263,876 v / 142,138 t / 9,296,860 B [cei] | not a contradiction — **[pri] is every LOD-bearing base in the plugin, [cei] is the subset a Commonwealth placement actually reaches.** Ship the subset; **size the format for [pri]'s superset** |
| — | worldspace placements: 162,816 [cei] vs ~341k [pri] | **162,816.** [cei] validated its census placement-for-placement against three real bakes (678/678, 2,211/2,211, 7,627/7,626) and reproduced vanilla's own file counts exactly. [pri]'s 3.03 SCOL multiplier came from one rural chunk and [pri] flagged it |
| smallest-three quaternion at 10 bits/component = 0.0024 rad [cei] | over 10^6 random rotations: **mean 0.00294 rad, worst 0.00822 rad** [nat] | [cei] quoted the mean. Moot below — v1 spends 6 bytes, not 4 |

Command behind [nat]: `python <scratch>/nat/spec/quat6.py` and `quant.py` (1,000,000 samples, seed 20260906).

### 0.2 Corrections to the previous draft, each recomputed

**Chunk counts, labelled.** Vanilla ships **465** `.BTO` files: 344 dim-4 + 97 dim-8 + 20 dim-16 + 4 dim-32. The fork's fully-filled bake writes **481**: 344 + 97 + **29** + **11**, because slot fallback fills every ring for every placement where vanilla left the coarse rings nearly empty. Every count in this document is now labelled *vanilla's* or *the fork's*. §0.1's ring-2 figure (29 chunks) is the fork's dim-16 ring; vanilla's is 20. Both were true in the last draft and neither was labelled.

**The 615.5 MiB baseline was three copies of one unverified number.** The draft reported rings 1, 2 and 3 as 131.6 / 131.6 / 131.6 MiB at three different chunk dims — that is not a measurement, that is one number written three times. Recomputed from the one figure that *is* measured (682 B a placement) and the ruling that all four rings stand on the same 162,816 placements:

```
per ring    682 B x 162,816   = 111,040,512 B = 105.9 MiB
four rings  111,040,512 x 4   = 444,162,048 B = 423.6 MiB   [arith]
```

Cross-check against the fork's ring-2: 162,816 placements over 29 chunks = 5,614 a chunk × 682 B = 3.83 MB a chunk × 29 = 111.0 MB = 105.9 MiB. Closes. A **measured** full-bake total is still owed and is scheduled once, in Lane 0 (§9).

**Manifests were MB-labelled MiB.** 78.65 B a placement (mid of the measured 78.3–79.0) × 162,816 × 4 = 51,222,914 B = **48.85 MiB**. The draft's "48.9 MB" was the MiB figure wearing the wrong unit.

**The array set was 11% unexplained and is now derived.** 66 × 87,376 (256² BC3, full chain) + 20 × 21,840 (128² BC3, full chain) = **6,203,616 B a sheet** = 5.92 MiB; three sheets (diffuse / normal / gsaos, emissive dropped by §4) = **18,610,848 B = 17.75 MiB**. The draft's 20.7 MB matches no split of the stated layer counts.

**86 layers and 177 diffuses are two different corpora, and the atlas does not overflow on the Commonwealth.** 86 is the count of distinct LOD materials a Commonwealth placement reaches [cei]; 177 is the count of distinct `*_d.dds` under `Data/textures/LOD` the base game ships across all worldspaces [par]. Against the atlas's 128 cells the Commonwealth has **1.49× headroom, not an overflow**. The corrected capacity argument is in §4 and it is still decisive — it just is not the sentence the draft wrote.

**The card table priced N² frames with no mips; §5 asks for (N+1)² frames with mips.** Recomputed in §5. Per base at N=8 / 44×64 / 3 × BC3 + 1 × BC1 with full chains: **1,062,186 B = 1.013 MiB**, 1.47× the draft's figure.

**Cards for 98.4% of bases was a self-contradiction and is withdrawn.** §3.4 said slot fallback fills `rep[]` inward *and* that 98.4% of slots stay empty. Both cannot be true, and the second reading priced the format at 3.4 GiB against a claimed 31.3 MiB. Ruling: **slot fallback fills every `rep[]` for any base with at least one authored LOD mesh, so those bases need no card.** The v1 card candidate set is the tree list only (§5), and it is an explicit input the owner can grow at a stated per-base price.

**Vertex duplication from clusterization was not counted.** A cluster addresses vertices with a u8 local index over its own ≤48, so clusters cannot share a vertex and every cluster-boundary vertex is duplicated. At 1.843 verts a triangle and ~18 vertices a 16-triangle cluster the expected factor is **1.15–1.35×**; this document plans at **1.25×** and gates at ≤1.35×. The draft's quoted 1.63× padding ratio covers the *index* blob only (16,008 × 48 = 768,384 allocated against 156,818 × 3 = 470,454 real) and is unrelated.

**"~29× on resident memory" and "worst streaming burst 33.2 MB → 0" compared a disk figure to a runtime one.** Both are withdrawn and recomputed in §8.4.

**`indexCrc32` covered "the five tables"; six tables and a string blob are defined.** Fixed in §8.
**`lodgIdentity` XORed a u32 into a u64,** leaving the top half unmixed — a 32-bit check wearing a 64-bit type. It is now an FNV-1a over three fields.
**"zero pad bytes" forbade the alignment padding the same rule requires.** It now reads **zero-filled pad**, so the determinism harness and the alignment harness stop contradicting each other.

---

## 1. What the native target is

The FO4CS-native target stops writing a *picture of the world* and starts writing *the world*: one deduplicated geometry library for the whole worldspace, and one flat table of quantised instance records that point into it. Nothing is stitched, nothing is merged, nothing is atlased, and the ring stops being a property of a file — it becomes a decision the consumer makes per instance per frame, from a bound radius the generator already computes. Every channel the current `.bto` smuggles through a vertex (object identity, AO, sky visibility, ground blend, sway) moves to whichever of the two files it belongs to: the ones that vary per object move to the instance record, the ones that vary per vertex stay in the library where they are shared by every copy.

**Three targets, not two.** The GUI's `LodgenTargetBox` already has "FO4 Community Shaders" (index 0, the box's default) and "Stock engine" (index 1), and today index 0 means *stock `.BTO` objects **plus** `.lodt` / heightmap / identity* (`applyTarget` at `:1153-1178` sets `objectsCheck` true). Redefining that saved value would silently stop bungo's existing profile from producing object LOD, with no dialog, no refusal and no gate — Lanes 1–5 are additive, so nobody would notice the wiring until Lane 6. So this work adds a **third** entry with its **own settings key**:

| target | meaning | settings key |
|---|---|---|
| `stock` | today's stock behaviour, verbatim | — |
| `fo4cs` | today's FO4CS behaviour, verbatim (index 0, still the box's default) | `LodGeneration/target`, meaning unchanged |
| `fo4cs-native` | this document | `LodGeneration/objectsNative`, **default 0** |

Every pre-existing QSettings profile keeps writing `.BTO`. The CLI gains `--target` with the default `stock`, and Lane 1 carries a gate that a CLI run with **no** `--target` emits a byte-identical file set to a pre-lane run, plus a gate that the GUI's argument builder emits a string-identical argument vector from a seeded pre-lane `.ini`.

**The single command:**

```
release/NifSkope.exe -no-gui lodgen "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" \
  --worldspace 3C --target fo4cs-native \
  --out-dir "E:/Bake/Data" \
  --data-root "E:/Tools/Fallout 4/DataUnpacked/Data"
```

`--target fo4cs-native` implies, and needs no other flag: `.lodg` + `.lodi` (objects), `--arrays`, and the emissive sheet only where something emits. It **refuses** `--atlas`, `--merge`, `--simplify*`, `--geomorph` and `--water-subdiv` by name rather than ignoring them, and it refuses an `--out-dir` that already holds another target's files unless `--overwrite` is given.

**`--impostors` is not in that command, deliberately.** Card baking needs the GUI render hook; the previous draft's headless command passed `--impostors` under `-no-gui`, in the same document that admitted no card had ever been baked. Card baking is a **separate invocation** (§5), and an offscreen card path is Lane 4's stated prerequisite rather than an assumption.

**Terrain is not in that command either.** See §6: the terrain drop moved behind its own flag and is out of every headline number in this document.

**One invocation, one target.** The texture pass writes the array layer into UV2.y of every vertex and a line into the manifest (`src/lodgen.cpp:4237`), so forcing `--arrays` inside a combined bake would mutate stock bytes. Two targets in one out-dir is a **named refusal**, not a merge.

### 1.1 Which code is shared and which forks

The fork already exists in the GUI: `LodgenTargetBox` at `src/lodgenmanager.cpp:621-630`, `fo4cs()` at `:1516`, `applyTarget` at `:1153-1178`. The CLI has no twin — adding `--target` is the first thing Lane 1 does. Every line number here is anchored by quoted text; re-grep the anchor rather than trusting the number (`src/lodgen.cpp` and `src/lodgenmanager.cpp` were being edited by another lane throughout).

| shared, unchanged by this work | forks |
|---|---|
| the ESM walk, SCOL expansion, disabled/deleted filtering, the placement census (`src/esmdata.cpp`) | **the emitter, and only the emitter.** Today `lodgenWriteChunk` stitches placements into a NIF; the native target calls `lodgWriteLibrary` + `lodiWriteInstances` instead |
| the slot pick and slot fallback (`src/lodgen.cpp:2915`) | the stitcher, the merge pass, the atlas pass, the far-ring simplifier, the u16 bucketing, and the BSMultiBound / segment-table / BSLightingShaderProperty writers — **not written for the native target, untouched for the other two** |
| the per-placement bound radius (`~2981`) | |
| AO, sky visibility, sway weight, ground-contact blend — same maths, different destination | |
| the LOD source model loader, `.bgsm`/`.lodm` material resolution (`src/io/lodmfile.h`) | |
| the texture-array pass, the card bake, `lodgenWriteDds` | |
| `.lodt`, the height sheet, the `.lodv` pyramid (TERRAIN1) | |

---

## 2. The drop list

Applies **only** under `--target fo4cs-native`. Under `stock` and `fo4cs` every one of these is still written, so bungo's 2026-09-06 ruling ("keep both the pyramid and the `.btr` sheets, ~4.0 GB") is untouched. On his install the native files are therefore **additive** until he makes a separate shipping decision — the disk saving below is what a native-only bake produces, not what appears on his drive the day Lane 6 lands. That is stated here because the previous draft booked it as a saving in both places at once.

| dropped | measured/derived bytes, whole Commonwealth | why nothing loses it |
|---|---|---|
| **481 `.BTO` object chunks** (the fork's count) | **423.6 MiB** = 682 B × 162,816 × 4 rings [arith on a [par]-measured anchor] | replaced by `.lodg` + `.lodi`. Rings 0–3 are *byte-for-byte the same geometry in bigger boxes* — slot fallback searches inward, so all four rings stand on the same 162,816 placements [cei] |
| **481 `.BTO.manifest.txt`** | **48.85 MiB** (78.65 B a placement × 162,816 × 4) [cei] | the instance record *is* the manifest, in binary, with rotation added |
| **the object atlas ×3** | **32.00 MiB** (33,554,804 B measured, fixed per worldspace regardless of content; 26.7 MiB with `--atlas-bc1`) [inv] | arrays win outright — see §4 |
| **loose LOD texture copies** | **≤ 16.0 MiB, 253 files** [cei] — byte-identical to files the game already ships (`cmp`-verified) [inv] | they exist because the atlas cannot hold tiling UVs. An array layer can |
| **the emissive `_g`/`_e` array sheets, when every layer's `emissiveScale` is 0** | 1,147,104 B on the far chunk's own set [inv]; **100% of the vanilla corpus today** — 3,430 of 3,430 LOD shader blocks own-emit with a BLACK colour, 0 of 121 LOD materials emit (`tools/lod_emission_probe.py`) | the sheet measured as **one distinct BC1 block, 0x0000000000000000, 141,986 times** [inv] (141,986 × 8 B = 1,135,888 B of the 1,147,104) |
| the crossed-quad impostors (`_fs`) | 280 B a carded placement (8 v × 32 + 4 t × 6) [par] | a carded base emits **no geometry at all** — see §5 |
| the ten-slot `BSShaderTextureSet` (7 empty strings/set), the `BSMultiBoundNode`/`BSMultiBound`/`BSMultiBoundAABB` trio (47.9% of all blocks, 0.035% of bytes), the dim×dim segment table, the NIF header and size table | 0.03–0.07% each [par] | listed for completeness: the cost was never bytes, it was that a bound is per *material bucket per chunk* and there is no per-object or per-cluster bound anywhere in the file |

**Terrain is not on this list.** See §6.

### 2.1 The corrected headline

| | today | native | ratio |
|---|---|---|---|
| geometry + instances/manifests | 472.5 MiB | **12.07 MiB** | **39.1×** |
| object textures (atlas 32.00 + loose 16.0 + arrays 17.75) | 65.75 MiB | 17.75 MiB (arrays only) | 3.7× |
| **object LOD, no cards** | **538.3 MiB** | **29.8 MiB** | **18.1×** |
| impostor cards (new capability, §5) | 0 | 30.39 MiB | — |
| **object LOD, with cards at the recommended N=8** | **538.3 MiB** | **60.2 MiB** | **8.9×** |
| vanilla's own shipped object LOD | 180.5 MiB | — | native-with-cards is **3.0× smaller than vanilla**, native-without-cards **6.1× smaller** |

The draft's 62.5× and 23.4× are withdrawn. They came from a 615.5 MiB baseline that did not follow from the draft's own 682 B a placement, a 10.6 MiB native figure that omitted cluster vertex duplication, and a card budget that priced 30 of a claimed 3,346 bases. Every term above is recomputed in §0.2, §3.3, §4 and §5. **None of the totals in this table is measured end to end** — they are [arith] over measured per-unit anchors, and Lane 0 schedules the one full bake that turns them into a measurement (§9).
---

## 3. The geometry story: a library and an instance table

### 3.1 Why this and not decimation, culling or cards

Measured, all four levers, ranked:

| lever | measured worth |
|---|---|
| mesh decimation | **15%**, topology-bound: 7,626 identity groups over 55,293 opaque triangles = 3.5–43 tris a group, almost all open border. And 36.8% of the chunk's triangles are alpha-tested and exempt [par] |
| screen-size culling at 8 px | **3%** — the far field is trees and trees are big |
| impostor cards | **1.22×** on the far field, and **nothing at all once instanced**: the 51 tree models in the whole library are 59,268 B of 9,648,037 [cei][pri]. Cards **cost** 30.39 MiB and buy shading, overdraw and coverage, not bytes |
| **instancing** | **39.1×** on disk (geometry + instances), **2.0–5.8×** on resident memory, draw calls 343–1,795 → **8–14** in the main view |

The reason is one number: the 34 distinct LOD models behind 97.5% of that chunk's 7,626 placements are **1,088 triangles** [pri]. One hundred and thirteen triangles of maple generate 69% of a five-megabyte file. We are not a geometry-density problem, we are a repetition problem, and today we write the repetition out 75 times.

### 3.2 The per-instance record — 24 bytes

Little-endian, 8-byte aligned, fixed stride. Position is quantised into the **chunk box**: X and Y span 16,384 world units (a 4-cell chunk — see §8), Z spans the per-chunk range stated in the chunk directory.

| off | size | field | encoding | precision it buys | at a 65,536-unit chunk |
|---|---|---|---|---|---|
| 0x00 | 6 | `position` | 3 × u16 into the chunk box | step **0.250 u**, worst **0.125 u** = **0.017 px** at D=10,240 | step 1.000 u, worst 0.500 u = **0.067 px** |
| 0x06 | 6 | `rotation` | 2-bit selector + 3 × 15-bit smallest-three quaternion (48 bits, LSB-first over the three u16) | worst **0.0146°** = 2.54e-4 rad → **0.51 u** on a 2,000-unit crown = **0.07 px** at D=10,240 | identical (ring-independent) |
| 0x0C | 2 | `scale` | u16, `scale = v / 8192`, range 0 … 7.99988 | step 1.221e-4 → **0.244 u** on a 2,000-unit object = 0.03 px. Measured corpus range 0.010 … 4.970, only 5.4–21% at exactly 1.0 [pri] | identical |
| 0x0E | 2 | `baseId` | u16 into the `.lodg` base table | exact to 65,535 bases (corpus: 3,400) | exact |
| 0x10 | 1 | `ao` | u8 | 1/255; the bake resolves **9 distinct values** today [par] | |
| 0x11 | 1 | `sky` | u8 | 1/255; the bake resolves **10 levels** (nine ray directions) [par] | |
| 0x12 | 1 | `ground` | u8 ground-contact blend | 1/255 over the 256-unit ramp = **1.0 u** | |
| 0x13 | 1 | `seed` | u8, the generator's own position-derived hash (§3.2.2) | sway phase, tree yaw, UV mirror, light flicker, colour jitter | |
| 0x14 | 2 | `flags` | u16 | bit0 mirrored, bit1 force-card, bit2 alpha-tested, bit3 emits, bit4 SCOL part, bit5 buried-cull candidate, **bits 6–15 reserved, must be 0 — a set reserved bit is a refusal** | |
| 0x16 | 2 | — | **reserved, must be 0** | the declared v2 growth slot (per-instance tint, or a light-record index), kept so the stride stays 24 B and 8-byte aligned | |

**24 B × 162,816 = 3,907,584 B = 3.73 MiB for every object in the Commonwealth, once, for all four rings.**
Against the far chunk's own 5,193,450 B: 7,626 × 24 = 183,024 B = **28.4×**.

Decisions and their justifications:

- **Six bytes of rotation, not four.** 74.0% of Commonwealth refs whose base fills an MNAM slot are yaw-only and 7.1% are unrotated, but **26.0% are genuinely tilted** [pri], so a yaw-only record is wrong for a quarter of the world, and a variable-stride record destroys `SV_InstanceID` addressing. Four bytes (2+3×10) measures worst 0.471° = 2.20 px on a 2,000-unit crown at D=10,240 [nat] — visible during a near cross-fade against the real object. Six bytes (2+3×15) measures worst 0.0146° = **0.07 px** [nat] and costs 326 KiB worldwide. Take the precision.
- **No identity field.** The instance's own index *is* its identity, in the u32 domain. The `(ref, part)` key moves to a parallel **cold table** (§8) the consumer need not load to draw. This deletes the 16-bit identity smuggled through vertex colour R+G, whose hard ceiling is 65,536 placements a chunk and whose measured headroom on the two densest dim-32 chunks was only 1.54× (42,560 and 42,641) [par] — it wraps silently.
- **`boundRadius` is gone from the record.** The previous draft spent 2 B an instance on a u16 world-unit radius. `base.boundRadius` is an f32 at scale 1 and the instance already carries `scale`, so `base.boundRadius × scale` is *more* accurate than a 1-unit u16 and costs nothing. 326 KiB deleted, and §7.7 no longer books it as a cost of goal two. The two bytes stay as the reserved v2 slot rather than shrinking the stride to 22 and breaking 8-byte alignment.
- **A radius, not two baked distances.** The consumer computes screen size from `base.boundRadius × scale / distance` against its own INI. Baking a distance freezes the policy in the file; baking a radius does not. Per-*base* crossover hints live in the base table where they cost nothing per instance.

#### 3.2.1 The two silent u16 ceilings are refusals, not clamps

`scale` maxes at 7.99988 and `baseId` at 65,535. The measured corpus reaches 4.970 and 3,400, but a modded load order is the only environment this ever ships into, and the standard set in §3.5 is *a refusal, never a drop*. So: the writer computes the maxima over the whole census before it writes a byte, and **refuses above range naming the offending ref formId or base editor ID**. The harness synthesises an out-of-range fixture for each (§10) — without the fixture the floor is decoration, because no Commonwealth chunk contains the case.

#### 3.2.2 `seed` is the generator's existing position hash, not a new one

`src/lodgen.cpp:2955-2964` derives a tree's yaw *and* its UV mirror from position: `treeHash = qRound(r.pos[0]) * 2654435761U + …`. Two ways to get this wrong, and the stock-to-stock gate observes neither: bake that yaw into the quaternion, and the ESM cross-check of §9 cannot run for trees (94.8% of the far field); replace it with a `(refFormId, part)` hash, and every tree in the Commonwealth rotates differently from the stock bake with nothing to notice.

Ruling: **the quaternion carries the ESM rotation only. `seed` is the existing position-derived hash, computed from the pre-quantisation float position, and its formula is copied verbatim into the format document.** The consumer applies the yaw and the mirror from `seed`. Hashing the *quantised* position would flip the hash for some trees and swing their yaw by up to 360°, so the order of operations is part of the format: hash first, quantise second.

### 3.3 Where the per-base meshes live — the library

`.lodg` holds, once per worldspace: a **base table**, a **mesh table** (one row per distinct model path), a **cluster table**, a **material table**, a fixed-stride **local-index blob**, a **vertex blob**, and a **string blob**.

**Library vertex — 16 bytes** (today: 32, of which 8 are provably redundant [par]):

| off | size | field | encoding |
|---|---|---|---|
| 0x00 | 6 | position | 3 × u16 into the **mesh's own AABB** (in the mesh table). At a 4,096-unit model box: step 0.0625 u |
| 0x06 | 4 | UV | 2 × u16 unorm into the mesh's own UV rect (`uvMin`, `uvExtent` in the mesh table) — so **tiling UVs are exact**, including the measured `[-1.299, 2.438]` cases [par] |
| 0x0A | 3 | normal | octahedral **12:12**, `n = b0 \| b1<<8 \| b2<<16`, `octX = n & 0xFFF`, `octY = n >> 12` |
| 0x0D | 1 | tangent | bits 0–6 roll angle around the normal (2.83° step), bit 7 handedness |
| 0x0E | 1 | `sway` | per-vertex sway weight, 0 = rigid (this one really is per-vertex: `h²·(0.35+0.65r)`) |
| 0x0F | 1 | `selfAO` | the **model's own** self-occlusion — the answer to [cei]'s doubt that instancing loses within-object AO variation. It does not: the variation inside an object is the same for every copy, so it belongs here |

Measured normal accuracy [nat], 10^6 random unit vectors:

| encoding | bytes | worst | mean |
|---|---|---|---|
| vanilla `.bto` `ByteVector3` | 3 (+1 bitangent byte) | 0.384° | 0.170° |
| octahedral 8:8 | 2 | 0.946° | 0.335° |
| **octahedral 12:12** | **3** | **0.0591°** | **0.0209°** |

The native vertex spends **4 bytes on the whole tangent frame** against today's ~10, is **6.5× more accurate on the normal than vanilla**, and drops a bitangent that is *exactly* `cross(normal, tangent)` — verified over 949,477 real vertices, max component deviation 0.0134, zero vertices above 0.02 [par]. And position error stops depending on where you are: today more than half of every chunk's XY components sit on a lattice of `dim × 2` world units (52.27% on 32 u at dim 16, 51.18% on 64 u at dim 32 [par]) because the miniature is a `HalfVector3`. In the library the box is the model, not the ring.

**Mesh entry — 48 B:** `f32 aabbMin[3]`, `f32 aabbExtent[3]`, `f32 uvMin[2]`, `f32 uvExtent[2]`, `u32 clusterFirst`, `u16 clusterCount`, `u16 flags` (bit0 anyAlphaTested, bit1 anySway).

**Cluster entry — 16 B:** `u32 vertexBase`, `u8 vertexCount` (≤48), `u8 triangleCount` (≤16), `u16 materialId`, `u8 boundCentre[3]` (u8 into the mesh AABB), `u8 boundRadius` (u8/255 of the mesh radius), `u16 meshId`, `u16 flags` — **bits 0–1 are the draw size class** (0 = ≤4 tris, 1 = ≤8, 2 = ≤16; see §8.3), bits 2–15 reserved. This is the per-cluster bound the NIF had nowhere to put: today the only box a consumer gets for "every maple branch in this 1,024-cell chunk" is a box the size of the chunk [par].

**Local-index blob:** a fixed **48 bytes per cluster** at `clusterIndex * 48`, three u8 local indices per triangle, slots past `triangleCount` filled with 0xFF (a degenerate marker).

**Material entry — 16 B:** `u8 arrayClass` (0 = 256², 1 = 128²), `u8 arraySet` (which `Texture2DArray` within the class — see §4.1), `u16 layer` (**< 2048**, the D3D11 array-axis limit), `u8 family` (0 legacy / 1 pbr), `u8 alphaThreshold` (0 = opaque; vanilla writes 128), `u8 flags` (twoSided / emits / tree), `u8 reserved`, `f32 emissiveScale` (copied from the `.lodm` and pinned against it by a gate), `u32 lodmStringOffset`.

**Base entry — 32 B:** `u32 formId`, `u32 modelStringOffset`, `u16 rep[4]` (per MNAM slot 0–3: a mesh index, or 0xFFFF), `u16 cardLayer` (**low 11 bits the layer, high 5 bits the card array set**; 0xFFFF = no card), `u16 flags`, `f32 boundRadius` (at scale 1, **never 0** — §5 states where a carded base's radius comes from), `u16 crossPx16[4]`.

**Derived library size** (upper bound, over [pri]'s full 3,355-model superset, at the planning duplication factor 1.25×):

| | bytes |
|---|---|
| vertices 289,003 × **1.25** × 16 | 5,780,064 |
| clusters ~16,008 × 16 | 256,128 |
| local indices 16,008 × 48 | 768,384 |
| bases 3,400 × 32 | 108,800 |
| meshes 3,355 × 48 | 161,040 |
| materials 177 × 16 | 2,832 |
| strings (model + `.lodm` paths) | ~250,000 |
| header + ≤7 payload alignments | ~28,921 |
| **`.lodg`** | **7,356,169 B = 7.02 MiB** |

Sensitivity: **5.91 MiB** at no duplication (the draft's implicit assumption), **7.46 MiB** at the 1.35× gate ceiling. The planning figure is 7.02 MiB and the harness measures the real one.

Cluster count is **[arith]** from [pri]'s measured model-size histogram (1,540 models ≤8 tris, 944 at 9–32, 586 at 33–128, 241 at 129–512, 45 at 513+), `sum ceil(t/16)` + 10% for multi-material models. The 16-triangle size was swept [nat]: 8 tris → 29,284 clusters / 1.12 MiB index blob / 1.49× padding; **16 → 16,008 / 0.98 MiB / 1.63×**; 32 → 9,198 / 0.98 MiB / 1.88×. Sixteen wins on cull entries at equal bytes.

**The 16-triangle cap and the 48-vertex cap can fight each other** at 1.843 verts a triangle: a 16-triangle patch of an open strip can need more than 48 vertices. The partitioner's rule is therefore **whichever cap binds first closes the cluster**, `triangleCount` may end below 16, and the padding-ratio gate (≤2.0×) is what keeps a pathological mesh from shredding into 3-triangle clusters.

### 3.4 How a base's mesh is chosen per ring

`rep[0..3]` are MNAM's own four positional slots, filled by the existing slot-fallback pick (`src/lodgen.cpp:2915`, searching inward first). **Slot fallback fills all four for any base with at least one authored LOD mesh** — which is the same fact §2 leans on when it says all four rings stand on the same 162,816 placements. A base with *no* authored LOD mesh in any slot has `rep[0..3]` all 0xFFFF and must have a `cardLayer`; that set, not "98.4% of bases", is what §5 prices.

**Consumer guidance (FO4CS).** Do not select by ring. Compute `screenPxRadius = base.boundRadius × scale / (distance × 7.294e-4)` and walk `crossPx16` down the ladder. `crossPx16` is a **radius in pixels**, in 1/16 px units — stated because the draft left a factor-of-two ambiguity running through the whole ladder. The constant is a **reference constant, not a format constant**: 960/tan(35°) = 1371.0, 1/1371.0 = 7.2939e-4, verified [nat] at 7.47 u a pixel at D=10,240, 80.2 at 110,000, 182.4 at 250,000 — it assumes 1920 wide and `fDefaultWorldFOV = 70` and the consumer recomputes it from the live projection. The measured 172× spread of bound heights inside one chunk (47.8 … 8,224.1 u, median 1,086.4) [pri] is why a per-chunk distance is wrong by two orders of magnitude for most of its contents.

### 3.5 How the 16-bit index cap and the dropped-geometry bug stop existing

They stop existing because **nothing is stitched.**

Today the cap fires in four places [par]: a terrain chunk above dim 4 is refused outright (`lodgen.cpp:862` — 257² = 66,049 verts, 514 over the ceiling, so decimation at dim 8 is not a quality choice but the only legal state); the water quadtree walks its subdivision down until it fits (`:1255`); the merge pass stops short (`:7200`); and — the one that matters — `:3144` **silently drops object geometry**: `continue; // bucket full`. Measured: the ring-3 chunk at (−32,0) has four shapes sitting at 65,535 / 65,534 / 65,529 / 65,424 vertices and **2,628 of 42,560 placements (6.17%) have no geometry in the file at all**; downtown (0,−32) loses 882 of 42,641 (2.07%). The loss is order-dependent, not importance-dependent: the first missing identity index is 38,695, so **68% of everything processed after that point is discarded** — what survives is whatever the ESM happened to list first. Both figures are lower bounds (a placement that lost *some* of its shapes still shows an identity index).

In the native format:

- there is no per-material bucket, so there is nothing to fill;
- the only index domain is a cluster's **u8 local index** over ≤48 vertices, plus a **u32 `vertexBase`** over ~361k library vertices;
- the largest thing that could overflow anything is a single source model, and the heaviest LOD model in the Commonwealth is `airporttower01_lod.nif` at 2,244 triangles [pri] — three orders below any ceiling;
- if a model ever did exceed a cluster's addressing, the writer **refuses and names the model**. A refusal, never a drop.

**Asserting this is not testing it.** The draft asserted the bug away by construction and then ran every harness chunk on Sanctuary dim-4, which never reaches the cap — the classic count that omitted the broken case. Lane 2's reconstruction gate therefore runs on **(−32,0) dim 32** and asserts an *asymmetric* difference: the set of placements with ≥1 triangle in `.lodg`+`.lodi` minus the set with ≥1 triangle in the `.BTO` must be **non-empty (≥2,000 members, each verified 0-in-BTO)**, and the reverse difference must be **empty**. That is the only check in the plan that proves the drop is gone rather than asserting it, and it doubles as proof the reconstruction is faithful.

**And the census gate has to mean something.** "Instances written == census placements" is vacuous if *census* means our own `placed` counter, because `placed++` already increments for a placement whose every shape was dropped at `:3144`, and the manifest row is appended at `:3027` *before* the shape loop. Both sides of that equality agree today with 6.17% of the geometry missing. So the gate is split in two (§10): **(i)** `instanceCount` equals the count from an **independent ESM census** (`tools/esm_refcensus.py`, §9), exactly; **(ii)** every instance whose base has a non-0xFFFF `rep` resolves to ≥1 cluster with ≥1 triangle, and the count of instances with neither geometry nor a `cardLayer` is **0**.
---

## 4. The material story: texture arrays, and the atlas is dropped

**Arrays are the only path on the native target.** The atlas is not written. Four measured reasons; the first two are restated because the draft's versions did not survive checking.

| | measured |
|---|---|
| **bytes** | arrays for the whole Commonwealth: **86 layers in 2 size classes (66 × 256², 20 × 128²)**, three sheets after the emissive drop = 66 × 87,376 + 20 × 21,840 = 6,203,616 B a sheet × 3 = **18,610,848 B = 17.75 MiB** [cei][arith]. Sized for the shipped superset of 177 layers at 256²: 3 × 177 × 87,376 = **44.25 MiB** worst case. The atlas is a **fixed 32.00 MiB** per worldspace regardless of content (measured identical in a 4-cell and a 256-cell build) [inv], 26.7 MiB with `--atlas-bc1`. Arrays win on this corpus and stay within a worldspace's budget on the superset |
| **capacity** | the sheet is 4096×2048 with 256² cells = **128 cells**. The Commonwealth reaches 86, so the atlas has **1.49× headroom here — it does not overflow on this worldspace, and the draft's claim that it does was two corpora spliced together.** What is true: the base game ships **177 distinct `*_d.dds` LOD diffuses** under `Data/textures/LOD` [par], so the atlas cannot hold the shipped set across worldspaces, and a load order that adds 43 distinct LOD diffuses to the Commonwealth overflows it. Overflowed shapes fall back to direct textures and also stop merging. An array is capped at 2048 layers, 16× the sheet, and §4.1 states what happens at that cap |
| **tiling** | **9 of 29 shapes** in the ring-2 chunk have UVs outside [0,1] (measured to `[-1.299, 2.438]`) and can *never* be atlased at any sheet size [par]. An array layer is a whole texture — a wrap sampler and the exact `uvMin`/`uvExtent` of §3.3 handle them. **This is what deletes the 253 loose texture copies (≤16.0 MiB)** |
| **mips** | `lodgenWriteDds` stops the chain at `while (mw > 4 && mh > 4)`, so the sheet gets **9 mips** against vanilla's 13; a cell's guard is `2 / 2^k` texels, so **from mip 2 onward neighbouring cells bleed into one another** — and mip 2 and deeper is precisely what a LOD chunk samples. Worse, the coarsest measured UV quantum on the atlased chunk is **2.00 texels of the 4096 sheet — exactly the size of the whole 2-texel guard band** [par]. Arrays have real per-layer mip chains and no neighbours |

### 4.1 The array-axis cap is a rule, not a hope

`D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION` is **2048**. The draft advertised `u16 layer` (65,535 layers) and `u16 cardLayer` with no cap and no overflow behaviour — strictly less robust at its capacity boundary than the atlas it replaces, which at least degrades to direct textures. The rule:

- a material's texture set is addressed by `(arrayClass, arraySet, layer)` with **`layer` < 2048**;
- when a size class exceeds 2048 layers the writer opens **`arraySet` + 1** and keeps going; the consumer binds one `Texture2DArray` per `(class, set)` and the bucket key gains `arraySet`, so the draw count grows by one per extra set and nothing silently fails;
- `arraySet` is a u8, so the hard ceiling is 256 sets × 2048 = 524,288 layers a class, and **above that the writer refuses, naming the material**;
- `cardLayer` packs the same way: low 11 bits the layer, high 5 bits the card array set (32 sets = 65,536 card layers), refusal above.

On the measured corpus every count is one set and this rule costs nothing. It exists so a load order finds a refusal or an extra draw instead of a wrong texture.

**`.lodm` kinds involved** (all already implemented, `src/io/lodmfile.h`):

- `kind: "array"` — `array.class [w,h]`, `array.set`, `array.layers[]` (the source per layer), `array.emissiveScale[]` (one per layer — two layers of one array are two materials and do not share it).
- `kind: "cardArray"` — `class`, `set`, `oct`, `frame`, `mips`, `emissiveScale[]`, and `layers[] {id, half, center, depthSpan, source}`.
- `kind: "source"` — read as an override beside a LOD material, unchanged.

Two families, one array set each: **legacy** (`_d`, `_n`, `_gsaos`, `_g`) and **pbr** (`_bc`, `_n`, `_rmaos`, `_e`). The `.lodg` material table carries a **binary copy** of family, class, set, layer, threshold and `emissiveScale` so the consumer never parses JSON on the render thread; the `.lodm` stays the authority and a harness pins the copy field for field.

Decisions:

- **Keep two size classes.** Folding the 20 × 128² layers into 256² costs 20 × (87,376 − 21,840) × 3 = **3,932,160 B = 3.75 MiB** and buys two fewer draws. Not worth it. Revisit only if a worldspace ends with many classes.
- **Write the emissive sheet only when some layer's `emissiveScale > 0`.** On the vanilla corpus that means never: 3,430 of 3,430 LOD shader blocks own-emit with a black colour, 0 of 121 LOD materials emit, and the shipped `_g` array measured as one distinct BC1 block (all zero) 141,986 times = 1,135,888 B of the 1,147,104 measured [inv]. A modded or PBR source that does emit still gets its sheet. **The gate has a positive leg** (§10): a fixture `.lodm` with `emissiveScale = 2.0` on one layer must produce exactly one sheet with exactly the referenced layer count, in the same run that produces zero on vanilla — otherwise "0 sheets" cannot tell *correctly suppressed* from *the writer never ran*.
- **The mesh's own descriptor problem disappears.** No shape names the arrays today — a raw byte search of both `.BTO` files finds **0 occurrences** of `LodgenArrays`, `.lodm` and `_gsaos` against 9 and 18 of `LodgenObjects` [inv]; the arrays are reachable only through the manifest's `A` lines, which nothing reads. In `.lodg` the layer is a field in the material table that the cluster names directly.

---

## 5. The impostor story: octahedral cards, and nothing replaces the crossed quads

### 5.1 Who gets a card

The draft said two contradictory things in one section — that slot fallback fills `rep[]` inward, and that 98.4% of slots stay empty and get a card. Read the second way, the format costs 3.4 GiB. Ruling:

**A card goes to a base if and only if it is in the explicit candidate list**, and the v1 list is:

1. **Trees** — 51 tree models in the whole library, of which **30** are reached by the far field; 7,230 of 7,626 placements in the measured far chunk (94.8%) and 71.7% of far mesh bytes [inv][pri][cei].
2. **Bases with no authored LOD mesh in any MNAM slot** (`rep[0..3]` all 0xFFFF) — the set that today emits `_fs` crossed quads or nothing. This set is **not measured** and is an open item: the 98.4% figure the draft used counts bases that do not fill slots 2 and 3, which slot fallback fills for them.
3. Anything the owner adds by name, priced at the per-base cost below.

The lane-4 gate is the invariant, not the percentage: **zero bases with neither a mesh in any `rep` slot nor a `cardLayer`.**

### 5.2 What replaces the crossed quads: nothing

The `_fs` crossed quads (`lodgen.cpp:2476`, `:2499`, `:2608`) exist only so the stock engine sees something, and cost **280 B a carded placement** (8 verts × 32 + 4 tris × 6) [par]. On the native target a carded base has `cardLayer` set, `rep[slot] = 0xFFFF`, and **emits no geometry whatsoever** — the quad is generated in the vertex shader from `SV_VertexID`, so a carded instance costs exactly its 24-byte record and zero bytes of mesh.

Everything the blend needs is already baked (`docs/LODGEN_IMPOSTOR_SPEC.md`): frames on the **vertices** of a hemi-octahedral N×N grid so every direction falls inside a triangle of three frames; coverage in alpha (a fraction, not a cut-out, with a 16/255 floor); the geometric normal in the **view's** space; and a **depth channel** (`_oct_ds` R, `units = (R/255 − 0.5) × depthSpan`, `depthSpan = 3 × max(boundRadius, 1024)`) so the card writes real depth and the silhouette is not a flat plane.

**A carded base's `boundRadius` comes from the card, not from a mesh.** `localMaxDist` is computed by iterating `shapes` (`src/lodgen.cpp:~2986`) and a base with no shapes contributes none — so a carded base would land at radius 0 and every screen-size test would collapse. Rule: for a carded base, `base.boundRadius = max(octHalfW, octHalfH)` read back from the `cardArray` `.lodm`; **`boundRadius == 0` is a refusal naming the base.**

### 5.3 Card sheet cost, recomputed

The draft's table priced **N² frames, 4 B a texel, no mips** — while the prose two paragraphs above it asked for frames on the **vertices** of the grid ((N+1)², so 81 at N=8, not 64) and for **three BC3 sheets plus one BC1, with mips** (3.5 B a texel × ≈1.33). Corrected, per base:

```
N=8, 44x64 tile:  81 frames x 2,816 texels x 3.5 B x 1.3305 (mip chain) = 1,062,186 B = 1.013 MiB
N=4, 44x64 tile:  25 frames x 2,816 texels x 3.5 B x 1.3305             =   327,836 B = 0.313 MiB
N=8, 88x128 tile: 81 frames x 11,264 texels x 3.5 B x 1.3305            = 4,248,745 B = 4.052 MiB
```

That is **1.47×** the draft's per-base figure, and §4's own costing (which does include full mip chains, and reproduces 66 × 87,376 exactly) was already using the honest method — the two texture sections were costing differently and only one of them could be right.

| N | tile class | bytes a base | **30 far-field tree bases** | all 51 tree bases |
|---|---|---|---|---|
| 4 | 44×64 | 327,836 | **9.38 MiB** | 15.95 MiB |
| **8** | **44×64** | **1,062,186** | **30.39 MiB** | 51.66 MiB |
| 8 | 88×128 | 4,248,745 | 121.6 MiB | 206.7 MiB |

**Default: N = 8 at the fitted 44×64 class, candidates from the far-field census only — 30 bases, 30.39 MiB.** Justification: N=4 gives 25 frames and large blend triangles, which is fine at ring 3 and not fine when a card stands in for a real mesh at ring 0–1. The 88×128 tile is out of budget at any candidate count. The lane-4 gate is **≤ 32 MiB**, set from this policy rather than from a target; the draft's ≤ 24 MiB gate failed its own default. This is an owner decision — see the open questions.

**State plainly what a card is and is not worth.** Once instanced, cards save **nothing on disk** — the 51 tree models in the library are 59,268 B of 9,648,037. Cards are a **shading and overdraw** feature (one quad instead of six crossed alpha-tested leaf cards) and a **coverage** feature (a base with no authored LOD mesh has nothing else to draw). They *cost* 30.39 MiB of the 60.2 MiB total. That is the honest trade.

### 5.4 Two changes for the native target

**Bake the card from the mesh that hands over to it.** Today `--list-impostor-candidates` returns the base's own near MODL by design, and bungo's reason for that stands for the stock target ("the base is more detailed"). But RDR2 and UE both bake the impostor from the mesh it replaces, and the reason is the crossover: two agreeing silhouettes dissolve, two disagreeing silhouettes pop. On the native target the source is **the last mesh in the base's ladder** (`rep[3]`, falling inward). No format change, a re-bake of the card library, and it is the single largest contributor to a clean mesh→card handover.

**The shadow pass picks the frame nearest the *light*, not the camera.** §8.5 requires the far shadow map to be re-rendered from our own buffers, and with cards covering 94.8% of far-field placements a camera-facing billboard's silhouette in the light's view is not the object's silhouette. The frames already exist on a hemi-octahedral grid; the format supports this and the draft never said so. Rule: **in a shadow or light-space pass, the card's frame selection uses the light direction and the quad is oriented to face the light**, not the eye.

### 5.5 The card bake is a real cost with a real clock

At the recommended policy the card bake is **30 bases × 81 frames = 2,430 offscreen renders** (51 bases = 4,131). That is tractable; the draft's withdrawn 98.4% policy would have been 271,026 renders and 3.36 GiB of output, which is the kind of number that has to appear in a plan before it appears in a night. The bake still needs the GUI render hook, so **an offscreen card path is Lane 4's stated prerequisite** and no card byte in this document is measured — every one is scaled from an earlier lane's measured 44×64 frame class.

**Card arrays are inside the identity chain.** `cardLayer` is a bare index into a `cardArray` `.lodm`; re-bake the card library with one more or one fewer base and every `cardLayer` in the `.lodg` silently reindexes. So `.lodg` carries a **`cardCorpusHash`** over the card candidate list, N, tile class and every source model, and a mismatch is a refusal (§8.6 states which kind).

---

## 6. The terrain story: meeting TERRAIN1's pyramid, and not booking its savings

Terrain is **not** this format's business, and the one rule is: **duplicate nothing TERRAIN1 already writes.**

**The terrain drop has left this document's headline.** The draft booked ~1.63 GB of dropped `.BTR` and per-chunk sheets as a saving against a replacement it never sized: `.lodt` is stated (34.3–36.0 MB), **`.lodv` is not sized anywhere**. A virtual texture carrying COLOR / MSN / DATA / HEIGHT at the resolution that replaces 3,060 × 524,664 B = 1,605,471,840 B (= 1.495 GiB, which the draft labelled "1.50 GB") is the same order of texels minus per-chunk borders — most of that is a **transfer**, not a saving. And bungo's 2026-09-06 ruling keeps both the pyramid and the `.btr` sheets (~4.0 GB) on his install, so on his disk none of it is saved in any case. Both facts were already in the draft and neither reached its totals.

Rulings:

- **No terrain file is dropped by `--target fo4cs-native`.** Dropping terrain requires its own explicit flag, **`--drop-terrain`**, which refuses unless TERRAIN1 has published a sized `.lodv` for the worldspace and a consumer version recorded in the header says it can read it.
- Without that flag, flipping the GUI's target cannot produce a world with no distant terrain. §11's "nothing here can break anyone's game by existing on disk" is true of the files and was false of the *bake selection*; this is the fix.
- No terrain byte appears in §2.1.

What exists, in flight right now:

- **`.lodt`** (`src/lodtfile.h`, shipped) — one file per worldspace: heights, LTEX blend alphas, water height *and* type, terrain colour, ground cover, coarse AO, behind a **progressive** pyramid whose coarse levels are exact subsamples (a coarse vertex *is* a fine vertex — what geomorphing and a clipmap both need). Commonwealth 34.3–36.0 MB. FO4CS reconstructed it against both shipped 1:1 R16 heightmaps at **37,748,736 of 37,748,736 texels exact, worst |error| 0** [con].
- **`.lodv`** (`src/io/lodvfile.h`, TERRAIN1 implementing) — one file per **level** of a terrain virtual texture: a fixed grid of square tiles, fixed 24-byte table stride, sheets `COLOR` / `MSN` / `DATA` / **`HEIGHT`**, CLI `--vt --vt-finest --vt-content --vt-border --vt-mips --vt-compress --vt-btr --vt-estimate`, verifier `--lodv-check`. **Total size unstated; this document does not assume one.**
- **the shadow height sheet** — `Commonwealth.HeightMap.-96.-96.95.95.-8320.44872.dds`, DX10 R16_UNORM 6144², 75,497,620 B, the map FO4CS already reads.

**The geometry clipmap hook is already in TERRAIN1's requirements** (`scratchpad/reqs_terrain_clipmap.md`): a `HEIGHT` sheet at **every** pyramid level, origin-aligned, span stated per level, nothing camera-relative baked, encoded `height/8 + 32767` as R16_UNORM. A geometry clipmap consumes that directly and needs nothing from the object format.

**Where objects and terrain meet, and the only three places they do:**

1. **The ground-contact blend.** Today it is a float32 per vertex (Eye Data, 1,001 distinct values measured [par]) — 3 of its 4 bytes wasted. It becomes **one byte per instance** (`ground`, 1.0 u over the 256-unit ramp). The object file does **not** carry terrain height; a consumer that wants exactness samples the `.lodv` `HEIGHT` sheet.
2. **Buried culling.** `flags` bit5 marks a buried-cull candidate; the decision is the consumer's, against the height sheet.
3. **Staleness.** Both object files carry the corpus hashes of §8.6 and `.lodi` additionally carries the `.lodg`'s identity.

**The object format adopts `.lodv`'s conventions verbatim** — a house rule, not a preference, and one of them has already cost a consumer a Y mirror:

- little-endian throughout, **absolute 64-bit offsets** (an int32 relative offset dies inside one worldspace);
- fixed table strides, payloads at **4,096-aligned offsets in table-index order, every pad byte zero-filled** — that is what makes two runs of the same bake byte-identical;
- an **absent entry is all-zero**, never a sentinel;
- **CRC-32, zlib polynomial 0xEDB88320**, per payload and over the index;
- **NORTH-UP row order**, declared in a flag whose clear state is a *refusal* — `.lodt` is row-0-SOUTH and `.lodv` is north-up, and mixing them is reader rule 19;
- the worldspace editor ID at 32 bytes, **refused, never truncated**;
- a magic deliberately neither `DDS `, `LODT` nor `LODV`, because a wrong-but-plausible parse is worse than a refusal.
---

## 7. The pop story — goal two

Vanilla's unit of visibility is **a file**. That is the whole bug: "whole chunks loading at once" is not a runtime defect, it is the granularity of the `.BTO`. Both halves of the fix are the bake's; the third half, which the draft did not have, is a shading contract (§7.8).

### 7.1 The identity handshake — already exists, and gets better

| what exists today | measured | what it becomes |
|---|---|---|
| per-placement index in vertex colour R+G, exact, unique in the chunk, **invariant under the far-ring simplifier** | 16-bit, ceiling 65,536 a chunk, measured headroom only 1.54× on the densest dim-32 chunks [par] | the **instance's own index**, u32 domain, cannot wrap |
| the `(ref, part)` key, measured invariant across rings — **406 objects shared** between the Sanctuary dim-4 and dim-8 chunks [pri] | in ASCII, in a 48.85 MiB manifest nothing reads | the **cold table**: `u32 refFormId`, `i16 scolPart`, `u16 flags`, 8 B, parallel to the instance array, **not needed to draw** |
| cross-ring correspondence | must be joined at load across two loaded chunks | **free.** There is one instance list; the representation is a per-frame decision |

**Cost: 8 B an instance, 1.24 MiB worldwide, and the consumer may skip loading it entirely.**

### 7.2 The ring overlap band — does not exist today, and costs frame time, not bytes

Today ring N and ring N+1 are **different files** with different geometry, and vanilla's far ring is nearly empty (vanilla ships 344 dim-4 chunks against 20 dim-16 and 4 dim-32; 673.8 / 17.5 / 2.6 triangles a cell) [cei] — so a band is impossible for two reasons at once: you cannot afford both rings, and the coarser one often has nothing in it. Filling every ring with slot fallback is what costs 423.6 MiB.

In the native format the band is a property of the *draw*: the cull pass emits an instance **twice** while its screen size is inside a crossover band, once with `rep[k]` at dither weight *w* and once with `rep[k+1]` at *1−w*, into the same indirect draw.

**Cost, stated honestly:** **zero bytes on disk**, and at a 15% band the crossover shell is **24% of the disc's area at that crossover** [arith], so up to ~24% of the instances in that shell append twice. The append buffers must be sized for it (§8.4), and the extra shading and overdraw are real. The draft booked this at 0 in the goal-two cost table; that is the same category error as booking resident memory as a disk figure, and it is corrected in §7.7.

### 7.3 Per-instance fade parameters

| carried | where | bytes | measured input |
|---|---|---|---|
| `boundRadius` | **base table, f32 at scale 1** | 0 per instance | already computed at `lodgen.cpp:~2981`; spread 47.8 … 8,224.1 u in one chunk. The consumer multiplies by the instance's `scale` |
| `crossPx16[4]` | base table | 8 per **base** (3,400 bases = 26.6 KiB) | the ladder's four screen-size **radius** steps in 1/16 px, derived from the bake's own per-slot triangle counts |
| `alphaThreshold` | material table | 1 per **material** (177) | vanilla writes 128 (`NiAlphaProperty` flags 4844 = 0x12EC, GREATER, threshold 128) |
| `seed` | instance, 0x13 | 1 | the generator's existing position-derived hash (§3.2.2) — the same seed every bake, so a fade never re-randomises between bakes |

**The alpha-test double-dip, and the one bit that fixes it.** Dithering *on top of* an alpha test eats the canopy from the inside, and **36.8% of the measured chunk's triangles are alpha-tested** [par]. Today, after the merge, a tree card and an alpha-tested fence are one shape and the file cannot tell them apart — the same blind spot that makes the simplifier refuse any shape with an alpha property. In the native format `alphaThreshold` is per material and `flags` bit2 is per instance, so the consumer can fold the dissolve weight into the threshold instead of stacking two binary tests. **Cost: 1 byte per material.**

**Why not alpha-to-coverage.** The draft offered it as the escape hatch. FO4 is not MSAA; at one sample alpha-to-coverage degenerates to a binary test and buys nothing. The sentence is deleted rather than left for someone to budget a lane against, and the surviving mechanism is threshold-folding plus §7.8's blue noise.

### 7.4 Impostor-to-mesh blending

Three things, of which two already ship:

- **shipped:** the hemi-octahedral frame grid (three-frame barycentric blending, no seams), the depth channel for pixel-depth-offset and depth-correct dissolve, per-frame gutters and full-channel dilation so filtering never pulls black into an edge.
- **§5.4's change:** bake the card from the last mesh in the ladder, not the near MODL, so the two silhouettes agree at the crossover. No format change.
- **the base table's `crossPx16[3]`** is the mesh→card step; the card's `depthSpan` and `half` extents are already in the `cardArray` `.lodm`.

### 7.5 The near-field pop — the one nobody has named

The LOD instance must vanish **exactly** as the engine draws the real reference at the loaded-grid edge (`uGridsToLoad = 5`, ~10,240 units, where 1 px = 7.47 u). Two mechanisms, both free:

- **by cell:** instances are sorted by cell within a chunk and the chunk directory carries a **`chunkCells²`-entry cell-range table** (8 B a cell; 16 entries a chunk at the default `chunkCells = 4`, 44,032 B worldwide). Suppressing a loaded cell is one range test. **The blob is `chunkCells²` per chunk, not a hardcoded 16** — at the elastic 8-cell setting it is 64 a chunk, and a reader written to the printed 16 is wrong the day the knob moves.
- **by reference:** the cold table's `refFormId` correlates an instance with the engine's own loaded ref, for the objects the player can disable, scrap or destroy.

### 7.6 What is *not* solved: object geomorphing

**Ruled out for v1, deliberately.** Terrain geomorphing already ships (`.btr` Eye Data carries the world-unit delta to the parent ring, `--geomorph`, dim < 32). Objects cannot have it, and the reason is not the simplifier — it is that in the native format the finer and coarser representations are **Bethesda's own MNAM slot meshes, authored independently, with no vertex correspondence at all**. There is no collapse to record because we did not do the collapse. [pri] proposed a per-removed-vertex collapse map out of meshoptimizer; that only applies to proxies *we* generate, which v1 does not generate. See §11 — it is the elastic v2 item, and the format reserves for it.

The dissolve (7.2), the per-object crossover (7.3), the agreeing-silhouette card (7.4) and the shading contract (7.8) carry goal two without it.

### 7.7 What goal two costs, recomputed

| | bytes worldwide |
|---|---|
| identity (cold table, optional to load) | 1,302,528 B = 1.24 MiB |
| bound radius per instance | **0** — derived from `base.boundRadius × scale`, and 326 KiB deleted from the draft |
| crossovers per base | 27,200 B = 26.6 KiB |
| alpha threshold per material | 177 B |
| seed per instance | 162,816 B = 159 KiB |
| cell ranges | 44,032 B = 43 KiB |
| the overlap band, on disk | **0** |
| **total on disk** | **~1.47 MiB on a 60.2 MiB bake** |
| the overlap band, at frame time | up to **~24% extra appended instances** inside a 15% crossover shell — sized into the append buffers, not free |

### 7.8 The shading contract — new, and more visible than the geometry pop

The record carries AO, sky visibility, ground blend and sway, and the draft never said what lights the result. A two-pixel silhouette wobble is invisible; a fog band across the whole horizon is not. Three rules, all consumer-side, all charter text rather than format:

1. **Fog, sun and ambient come from the engine's own constants, not from ours.** The consumer reads the same directional-light and fog constants the engine's deferred pass reads, so the far field cannot drift from the near field across weather, exposure and image space. The exact constant-buffer slot is recorded during the C0 spike (§9) and written into the format document, not guessed here.
2. **Beyond the froxel volume there is one stated rule.** Volumetric Air is a froxel medium whose range does not reach 250,000 units, so the far field is outside it by construction. The rule: **outside the froxel range the consumer applies the analytic fog term the froxel medium converges to at its far plane**, so the transition at the volume boundary is continuous by construction rather than by tuning.
3. **The far field writes motion vectors.** The G-buffer has a `motion` target (`GBufferBridge::Targets12::motion`) and this project has three competing jitter owners (`src/Base/Temporal/TemporalAccumulator.cpp`: Engine / Upscaler / TemporalAA) plus DLSS. Geometry drawn without motion vectors ghosts and smears. For a static far field this is cheap and exact: **prev = the same world position under the previous frame's view-projection.**
4. **The dissolve is blue noise correlated with the jitter sequence, not a `seed`-hashed white-noise dither.** A dither uncorrelated with TAA's jitter is the single artefact TAA is worst at, and it would crawl across 100% of the screen area this format exists to serve. `seed` stays what §3.2.2 says it is — sway phase, yaw, mirror, colour jitter — and the dissolve threshold comes from a blue-noise texture indexed by pixel and by the frame's jitter index.
---

## 8. The container

**Two new files per worldspace**, against 481 `.BTO` + 481 manifests today (the fork's counts).

```
Data\Terrain\<WorldspaceEditorID>\Objects\<WS>.lodg      the geometry library     ~7.0 MiB
Data\Terrain\<WorldspaceEditorID>\Objects\<WS>.lodi      the instance tables      ~5.1 MiB
```

Same folder convention as `.lodt` (`Data\Terrain\`, which FO4 does not use, so nothing collides) and the same editor-ID stem, so a child worldspace with `kUseLandData` inherits its parent's files exactly as the heightmap loader already resolves them.

**Rules, all inherited from `.lodv`:** little-endian; absolute 64-bit offsets; fixed table strides; payloads in table-index order at **4,096-aligned** offsets with **zero-filled** pad; absent = all-zero; CRC-32 zlib poly 0xEDB88320; NORTH-UP row order (bit clear = refusal); 32-byte editor ID refused not truncated; every reserved field zero, and a nonzero reserved field is a refusal.

**Table ordering is part of the format, not an accident.** Today's stability is incidental — `QMap<QString, ObjBucket> buckets` at `src/lodgen.cpp:2881` happens to be key-sorted. A two-bake byte-identity check cannot see a bake that is perfectly self-consistent and differently ordered from the last one, so the sort keys are written into the format document and **sortedness is gated in the file** (§10):

| table | sort key |
|---|---|
| base | `formId` ascending, over the **full worldspace ESM census**, in every bake including a one-chunk one |
| mesh | model path ascending, then MNAM slot |
| cluster | `meshId`, then `materialId`, then first triangle index |
| material | `family`, `arrayClass`, `arraySet`, `layer` |
| instance | chunk index (north-up row-major), then cell index, then `refFormId`, then `scolPart` |

The base ordering being over the **full** census — not over the bases a region bake happens to touch — is what makes `baseId` worldspace-stable, and it is cheap: the ESM walk is fast, only the geometry bake is slow.

### 8.1 `.lodg` header — 256 bytes

| off | type | field |
|---|---|---|
| 0x00 | char[4] | magic `LODG` |
| 0x04 | u32 | version = 1 |
| 0x08 | u32 | flags — bit0 must be 1 (vertex layout v1, stride 16), bit1 PARTIAL (rows present for a subset, absent rows all-zero, **indices worldspace-stable**); others reserved 0 |
| 0x0C | u32 | headerCrc32 (over 0x10…0xFF) |
| 0x10 | u64 | `pluginCorpusHash` — the terrain writers' hash, carried for the object/terrain/plugin triple |
| 0x18 | u64 | `objectCorpusHash` — **new**, see §8.6 |
| 0x20 | u64 | `modelCorpusHash` — over every source model read (path, size, content) |
| 0x28 | u64 | `cardCorpusHash` — over the card candidate list, N, tile class and every card source model |
| 0x30 | char[32] | worldspace editor ID, NUL-padded |
| 0x50 | u32 | baseCount |
| 0x54 | u32 | meshCount |
| 0x58 | u32 | clusterCount |
| 0x5C | u32 | materialCount |
| 0x60 | u32 | vertexCount |
| 0x64 | u32 | `maxClustersPerMesh` — **new**, so the consumer can size an append buffer statically (§8.4) |
| 0x68 | u16 | clusterMaxTris = 16 |
| 0x6A | u16 | vertexStride = 16 |
| 0x6C | u32 | stringBytes |
| 0x70 | u64 | offset: base table (32 B stride) |
| 0x78 | u64 | offset: mesh table (48 B) |
| 0x80 | u64 | offset: cluster table (16 B) |
| 0x88 | u64 | offset: material table (16 B) |
| 0x90 | u64 | offset: local-index blob (48 B per cluster) |
| 0x98 | u64 | offset: vertex blob (16 B per vertex) |
| 0xA0 | u64 | offset: string blob (NUL-terminated UTF-8) |
| 0xA8 | u32 | `indexCrc32` — over the **six tables and the string blob**, in file order |
| 0xAC | u32 | reserved 0 |
| 0xB0 | u64 | fileBytes |
| 0xB8 | — | reserved, zero, to 0xFF |

### 8.2 `.lodi` header — 256 bytes

| off | type | field |
|---|---|---|
| 0x00 | char[4] | magic `LODI` |
| 0x04 | u32 | version = 1 |
| 0x08 | u32 | flags — bit0 ROW_ORDER_NORTH_UP (clear = refusal), bit1 PARTIAL, bit2 NOLIB (written before a `.lodg` exists; `lodgIdentity` must then be 0, and a zero identity without this bit is a refusal) |
| 0x0C | u32 | headerCrc32 |
| 0x10 | u64 | `pluginCorpusHash` — must equal the `.lodg`'s |
| 0x18 | u64 | `objectCorpusHash` — must equal the `.lodg`'s |
| 0x20 | u64 | `lodgIdentity` — **FNV-1a 64 over the `.lodg`'s `headerCrc32`, `modelCorpusHash` and `objectCorpusHash`**, not an XOR (the draft's XOR left the top 32 bits unmixed) |
| 0x28 | char[32] | worldspace editor ID |
| 0x48 | i16 ×4 | chunkWest, chunkSouth, chunkEast, chunkNorth — **inclusive, in chunk units** (`cellX >> 2`) |
| 0x50 | u16 | chunkCells = 4 |
| 0x52 | u16 | instanceStride = 24 |
| 0x54 | u32 | chunkCount = (east−west+1)·(north−south+1), the dense table length — **capped at 65,536; above that the writer refuses naming the extreme chunk** |
| 0x58 | u32 | instanceCount |
| 0x5C | u32 | presentChunks |
| 0x60 | u32 | `maxInstancesPerChunk` — **new**, so the consumer can size an append buffer statically |
| 0x64 | u32 | indexCrc32 |
| 0x68 | u64 | offset: chunk table (32 B stride, dense, north-up row-major) |
| 0x70 | u64 | offset: cell-range blob (8 B per cell, **`chunkCells²` per present chunk**, table order) |
| 0x78 | u64 | offset: instance blob (24 B) |
| 0x80 | u64 | offset: cold blob (8 B, parallel) |
| 0x88 | u64 | fileBytes |
| 0x90 | — | reserved, zero, to 0xFF |

**Chunk table entry — 32 B:** `u32 instanceFirst`, `u32 instanceCount`, `f32 zMin`, `f32 zExtent`, `f32 maxBoundRadius`, `u32 cellRangeOffset`, `u32 crc32` (over this chunk's instance + cold records), `u32 reserved`. X and Y of the chunk box are **implied by the table index** (`chunkX·16384 … +16384`), so only Z is stated. Absent chunk = 32 zero bytes.

`maxBoundRadius` is new and it is load-bearing: the chunk box is built from instance **origins**, so an 8,224-unit tree's bound leaves the box and a per-chunk cull against `zMin`/`zExtent` alone would pop it. The consumer expands the chunk box by this value. The gate is `z + boundRadius ≤ zMin + zExtent + maxBoundRadius` for every instance.

**Cell-range entry — 8 B:** `u32 instanceFirst`, `u32 instanceCount`.
**Cold record — 8 B:** `u32 refFormId`, `i16 scolPart` (−1 when not a SCOL part), `u16 flags`.

**Position decode:**
`x = chunkX·16384 + px/65535·16384`, `y = chunkY·16384 + py/65535·16384`, `z = zMin + pz/65535·zExtent`.

**Where the chunk index comes from at draw time.** The record carries no chunk id, and the cull is one dispatch over all instances, so the decode above needs one more input. It is **derived at load, not stored**: the loader walks the chunk table once and writes a parallel `u32 chunkIndex[instanceCount]` buffer. That is 651,264 B = **636 KiB of VRAM, zero bytes of disk**, and it beats the three alternatives the draft left unresolved (a per-thread binary search over 1,024 entries; one dispatch per chunk, which kills the one-pass claim and gives wildly unbalanced groups at 473 … 42,641 instances a chunk; or a `u16 chunkId` in the record, which takes the stride to 32 B and the file up 33%).

### 8.3 How this is drawn in D3D11 — what one developer writes, and where

The layout is chosen so that **FO4CS never has to create a vertex or index buffer** — the one primitive the tree has never used (`grep -rn "BIND_VERTEX_BUFFER" src/` returns nothing outside `extern/` and CommonLib [con]).

| what | where it is written | what it is |
|---|---|---|
| graphics-stage save/restore scope | `src/GraphicsStateSaveScope.h` (new), modelled line for line on the existing `src/ComputeStateSaveScope.h` | RAII capture and restore of OM (RTVs, DSV, blend, depth-stencil + stencil ref), RS (viewport, scissor, rasteriser state) and IA. **Written first, as its own lane, with a recording-fake host test** — see §9 C0 |
| file readers | `src/FarField/LodgFile.{h,cpp}`, `src/FarField/LodiFile.{h,cpp}` (new) | header validation, CRC checks, the refusal policy of §8.6, the derived `chunkIndex` buffer |
| GPU buffers | `src/FarField/FarFieldBuffers.{h,cpp}` (new), on the existing `src/Buffer.h:84-110` `StructuredBufferDesc` + SRV/UAV | two `StructuredBuffer`s (library, instances) plus append/args buffers sized from `maxInstancesPerChunk` and `maxClustersPerMesh` |
| cull + append | `src/FarField/FarFieldCull.{hlsl,cpp}` (new), one `Dispatch` (157 sites already exist) | frustum test using `base.boundRadius × scale`, ladder step from `crossPx16`, appends `(clusterId, instanceId)` pairs per bucket plus one `DrawInstancedIndirect` args row. Needs an append/counter UAV, `CopyStructureCount` and `D3D11_RESOURCE_MISC_DRAWINDIRECT` — **none of which exist in the tree today** (`grep -rn "DrawInstancedIndirect\|CopyStructureCount\|DRAWINDIRECT\|BUFFER_UAV_FLAG_APPEND" src/` returns zero hits), all of which are stock D3D11 |
| draw | `src/FarField/FarFieldDraw.{hlsl,cpp}` (new), seated at `DeferredPrePass_Post` (`src/RenderAnchors.h:15-31`, `RenderAnchors.cpp:223`) | `DrawInstancedIndirect` per bucket. `SV_VertexID` is the local index slot, `SV_InstanceID` picks the pair, the VS pulls the vertex from the StructuredBuffer. **No IA, no input layout, no index buffer.** A 0xFF local index emits a zero-area triangle |
| shadow pass | `src/FarField/FarFieldShadow.{hlsl,cpp}` (new), into the existing `farMapDsv` | see §8.5 conflict 2 — a second complete consumer, not a sentence |
| census | `src/FarField/FarFieldCensus.h` (new), replacing `FarFieldLodBtoChannels.h` + `LodShadowMapCensus.h` | see §8.5 conflict 3 |

**Three size classes, not one fixed 48.** `DrawInstancedIndirect(vertexCountPerInstance = 48)` launches 48 VS invocations for a 3-triangle cluster, and 1,540 of 3,355 models are ≤8 triangles [pri]. At an average 18.05 real vertices a cluster that is **2.66× the vertex shading of an indexed draw**, with no post-transform reuse by construction. No format change fixes it: the cluster's `flags` bits 0–1 carry a **draw size class** written at bake time, the cull appends into three buckets, and the draw issues `vertexCountPerInstance` of **12 / 24 / 48**. The cost is a few more draws.

**Bucket key** = (draw size class × family × arrayClass × arraySet × alpha state) for clusters, and (family × card class × card set) for cards. Measured today: 3 size classes, ~1 live family, 2 array classes, 1 set, 2 alpha states → **8–14 mesh draws plus 1–2 card draws** for the whole worldspace, against **343–1,795 resident engine draws today** (Glowing Sea / Sanctuary 998 / downtown 1,795) [cei]. Cards, which are 94.8% of far-field placements, stay **one draw per class**.

**The draw count is main-view only.** The far shadow map (§8.5) runs the same cull and the same bucket set once per slice it renders. Quote both numbers together or the comparison against 343–1,795 is not a comparison.

**What the C0 spike must record before any of this is written**, because the spec cannot honestly assert it: which RTVs the engine's deferred prepass has bound (FO4's MRT set is albedo / normal / specular / motion + main depth), the depth-stencil state and stencil reference the deferred lighting pass expects to read back, the viewport, the rasteriser state including LOD's own cull mode and depth bias, and **whether the depth buffer is reversed-Z**. That last one decides whether the card pixel shader uses `SV_DepthGreaterEqual` or `SV_DepthLessEqual` — and it must use one of them, never plain `SV_Depth`, because a plain depth write disables early-Z and hi-Z and would make the single all-cards draw the most overdraw-exposed draw in the frame. This tree already carries the scar for guessing at state: `src/ComputeStateSaveScope.h` exists because a clear-on-exit scope nulled another pass's bindings, and D3D11 then returned zeros from the unbound slots with no debug-layer error and no device removal.

### 8.4 Residency — read once, and what "once" actually costs

**Streamable? Yes, and deliberately not streamed.** The honest resident set for one worldspace, at the recommended card policy:

| | MiB |
|---|---|
| `.lodg` + `.lodi` in StructuredBuffers | 12.07 |
| derived `chunkIndex` buffer | 0.62 |
| mesh texture arrays (86 layers × 3 sheets) | 17.75 |
| impostor cards (30 tree bases, N=8) | 30.39 |
| append, args and cull scratch (sized from `maxInstancesPerChunk`, `maxClustersPerMesh`, and the 24% overlap-shell allowance of §7.2) | ~3.0 |
| **total** | **63.8** |

Against **125.9–370.0 MiB** of resident object geometry today [cei] that is **2.0×–5.8×**, not the draft's 29× — which compared a disk figure to a runtime one. Without cards the resident set is 33.4 MiB and the ratio is 3.8×–11.1×. **Geometry is the smallest term in the resident set; textures are 76% of it.** The decision not to build a streamer therefore rests on the *total*, not on the buffers alone, and the total is what §11 must keep bounded.

**And the burst does not go to zero.** It becomes **one 63.8 MiB upload at worldspace load**, recurring on every worldspace change. That is a loading screen the engine already shows, not a hitch mid-traversal, and it deletes the capability gap the consumer investigation named ("continuous streaming is NOT proven and is the biggest gap… most likely to produce hitching that only shows up on bungo's machine" [con]).

**One worldspace resident at a time.** Everything above is sized per worldspace, and Fallout 4 ships Commonwealth, Far Harbor and Nuka-World plus mod worldspaces. The rule: **the active worldspace's set is resident and the previous one is released on worldspace change.** Card frames are cached by `(base formId, source model hash, N, tile class)` during the bake, so a maple shared by three worldspaces is rendered once and *written* three times — the duplication is on disk, stated, and never in VRAM.

The container is nonetheless chunk-indexed with per-chunk CRCs and 4,096-aligned payloads, so a later consumer *can* page it, and so an incremental region re-bake is possible.

### 8.5 Conflicts, named, because there are four and all four are real

1. **Suppressing the engine's far field, and the order it happens in.** LOD reaches the renderer purely by scene-graph parentage under `spLODRoot` / `spLODObjectRoot`; an exhaustive data-xref scan found no renderer-side function touching those globals, and `Script::ToggleLODLandFunction` already xrefs them [con]. Hiding `spLODObjectRoot` removes the engine's object LOD with **no code patch**. But that scan establishes *possibility, not scope* — it does not say what else stops appearing (the engine's own distant caster list, water reflections, the local map). **One measurement before OBJD: hide the node, capture the frame, list every pass whose draw count changed.** Half a day, and the difference between one named conflict and three unnamed ones. And the order of operations is a format rule, not a preference: **validate → build buffers → read back a nonzero drawn-primitive count for one frame → only then hide `spLODObjectRoot`.** Nothing in the draft forbade hiding the engine's far field before ours was known to work.
2. **The far shadow map goes dark if you do — and it is a second complete consumer.** `FirstPersonShadowRuntime.cpp:19646` binds `farMapDsv` with no RTV, `:19677` routes draws to it, and the file's own comment at `:18969` states the pass runs "with the ENGINE's BSUtilityShader. There is no pixel shader of ours". Rendering our buffers there means authoring our own VS+PS for a depth-only pass and matching the engine's depth format, constant and slope bias, and cascade view-projection — **once per slice, not once**. With cards covering 94.8% of far-field placements, §5.4's light-space frame selection is what keeps this from being a quality regression in a feature bungo has already flown. Its gate is an **altered-capture A/B** of the far map against the engine's (dark% over the far-shadow region, native vs stock, against a measured noise floor from a control pair), never a flight report and never a screenshot.
3. **The `.bto` channel census stops observing.** `src/FarField/FarFieldLodBtoChannels.h` + `LodShadowMapCensus.h` count draws as packed / colours-only / vanilla. With no `.bto` they report "vanilla" forever — a live instrument lying in bungo's logs, which is how a wave gets blamed for a regression that never happened. Per the census rule, a field that cannot observe must accuse its own plumbing: the replacement row is **`FarField: native drawn=<n> culled=<n> overflow=<n> | not observing: no .bto in scene`**, and it lands in the **same wave as the drop**, not deferred into guidance.
4. **The consumer reads loose files only, and a shipped far-field mod is a BA2.** The bake writes `Data\Terrain\<WS>\Objects\*.lodg` plus loose array DDS, and FO4CS's DDS loading appears in exactly two files (`src/Effects/ScreenSpaceGIRuntime.cpp`, `src/Materials/TruePBRShimRuntime.cpp`), both filesystem paths; nothing in the tree reads a BA2. **v1 is loose-files-only by design and that is a shipping constraint, stated here rather than discovered at packaging time.** A `BSResourceNiBinaryStream` path is a named v2 item.

### 8.6 Refusal is for the generator; the consumer degrades

The draft applied one refusal policy to files the shipped game reads at runtime, and one of the refusal keys was a hash of the user's load order. `pluginCorpusHash` changes the first time a user installs or removes any mod after baking — the normal state of a Fallout 4 load order — and by then conflict 1 has already hidden `spLODObjectRoot`, so the failure mode is an empty horizon with a log line. Vanilla `.bto` in the same situation keeps drawing slightly-stale LOD, which is the correct failure. Split the policy in the format document, not in the consumer's head:

| class | keys | generator | consumer |
|---|---|---|---|
| **hard** | magic, version, `vertexStride`, `instanceStride`, `clusterMaxTris`, a set reserved bit, ROW_ORDER_NORTH_UP clear, `chunkCount` over cap, a zero `lodgIdentity` without NOLIB, a CRC mismatch | refuse, name the field | **refuse to load, and never hide the engine tree** |
| **soft** | `pluginCorpusHash`, `objectCorpusHash`, `modelCorpusHash`, `cardCorpusHash` mismatch | refuse, name the field | **load anyway, log it, raise a `stale=1` census row, keep rendering** |

**`objectCorpusHash` is new because the terrain hash is provably blind to us.** The terrain writers' hash is `EsmWorld::vhgtCorpusHash` (`src/esmdata.cpp:210`): it FNV-hashes the **first VHGT field of each LAND record** in coord-bearing CELLs and nothing else. Move a REFR, rescale it, add a placement, repoint a base's MNAM — the hash does not change, and a stale instance table sails through. `objectCorpusHash` is FNV-1a over exactly what the object walk reads, in the sort order of §8: `(REFR formId, base formId, DATA position + rotation, XSCL, record flags)` and `(STAT/SCOL MNAM slot paths, SCOL part transforms)`. Its gate has four legs and one of them must **not** refuse (§10) — a hash that refuses on every unrelated mod is a hash the user disables.

### 8.7 Region bakes, and the mod-authoring claim

A region bake sets `PARTIAL` in **both** files — the draft gave the flag to `.lodi` only, while `.lodg`'s flags said "all others reserved 0", so a partial library was unrepresentable. Absent rows are all-zero and **indices stay worldspace-stable** because §8's base ordering is over the full census. A consumer merges every `*.lodi` in the folder by chunk key, last wins.

**The "a mod ships instances for its own cells" claim is withdrawn for v1**, because three things in this document forbid it: `lodgIdentity` cannot be computed by a mod that has not re-baked the whole library; `baseId` is a u16 *index* into that library, not a formId, so a mod's records are meaningless against a different `.lodg`; and §7.1 makes the instance's own index its identity, which a merge reindexes. What v1 supports is **incremental region re-bakes of one install's own library**, which is what `PARTIAL` is for. Cross-mod instance authoring needs a base namespace keyed by formId rather than by index, and that is a named v2 item.
---

## 9. The staged migration

Every lane is shippable alone, gateable alone, and carries the same **standing gate**: *the stock bake is byte-identical to the checked-in baseline, file for file, by hash.* Each lane below also states, explicitly, **which baseline it moves** — because a lane that legitimately changes stock or native bytes and does not say so is indistinguishable from a regression.

**The baseline is a file, not a fresh bake.** The draft's standing gate compared a bake from a tree containing ten other live lanes' edits against the same tree plus one more, and called it a regression test. Lane 0 checks in `tests/fixtures/stock_baseline.sha256`: the sha256 of **every file** of a fixed region set, generated once from a **named build** (the exe's sha256 and `git describe` recorded in the file's header). Every later lane diffs against that file.

The region set must reach the paths a dim-4 bake never touches:

| region | why it is in the set |
|---|---|
| (−20,24) dim 4 | the ordinary case, and the identity harness's existing chunk |
| (−24,24) dim 8 | the first fallback ring |
| (−32,16) dim 16 with `--slot-fallback` | the fork's filled ring, where vanilla ships nothing |
| (−32,0) dim 32 | **the bucket-cap chunk** — 2,628 of 42,560 placements dropped today |
| plus the atlas, the arrays, and every manifest for the above | the texture and manifest writers are inside the identity |

**Prove the gate fails.** Flip one constant in the stock vertex writer, run it, show the diff names the file, and record that in the spell's header the way `lodgen_farring.sh` already records its floors.

### 9.1 The lanes

| # | lane | what one developer writes, and where | gate | baseline it moves |
|---|---|---|---|---|
| **0** | **BASELINE** *(first)* | `tests/fixtures/stock_baseline.sha256`; `tests/spells/lodgen_native.sh` skeleton; **`--target stock` added explicitly to all seventeen existing lodgen spells** (`lod_channel_preview`, `lod_generation`, `lodgen_card_arrays`, `lodgen_farring`, `lodgen_ground_cover`, `lodgen_identity`, `lodgen_impostor_cards`, `lodgen_merge`, `lodgen_octahedral`, `lodgen_resources`, `lodgen_terrain`, `lodgen_terrain_vt`, `lodgen_texture_arrays`, `lodgen_tree_sway`, `lodgen_water_subdiv`, `lodt_btd`, `lodt_write`) so a later default flip cannot silently retarget them | the baseline file exists, its named build is recorded, and the mutation test above fails the gate | **establishes** the stock baseline |
| **C0** | **graphics-stage save/restore** *(FO4CS, before any draw)* | `src/GraphicsStateSaveScope.h` + a recording-fake host test, modelled on `ComputeStateSaveScope` | the fake records every OM/RS/IA setter and the test asserts full restoration, including stencil ref and viewport | none |
| **C0b** | **indirect-draw spike** *(FO4CS, prerequisite to OBJG landing)* | a hardcoded one-instance, one-cluster `DrawInstancedIndirect` at `DeferredPrePass_Post`, plus the C0 scope; records the prepass RTV set, depth-stencil state, viewport, rasteriser state and **whether depth is reversed-Z**, into `docs/RE/far-field-draw-state.md` | **one triangle visible in a named rdc capture**, and the recorded state written into the format document | none |
| **1** | **OBJI — the instance table** | `--target` on the CLI + the third GUI entry and its own settings key; `lodiWriteInstances`; **`tools/esm_refcensus.py`**, an independent REFR/SCOL walker on the cached `wbDefinitionsFO4.pas` layouts, linking none of our C++, itself validated by diffing 50 refs against an xEdit CSV export. Writes `.lodi` **beside** today's `.BTO` | §10 rows 1.x, all against the **python census**, never against our builder | **none for stock** (purely additive). Establishes the **native** baseline |
| **2** | **OBJG — the geometry library** | `lodgWriteLibrary`; a **separate-invocation** verifier `--lodg-reconstruct` that reads only the two files plus the source `_lod.nif` set (through `nifskope-cli` or the python NIF parser) and never touches in-process arrays | §10 rows 2.x, including the (−32,0) asymmetric drop proof | none for stock; **moves the native baseline** (`.lodi` gains a real `lodgIdentity`, NOLIB clears) |
| **3** | **OBJM — arrays only** | array forcing and atlas refusal on the native target; the binary material table; the emissive positive-leg fixture | §10 rows 3.x | none for stock; moves the native baseline (material table) |
| **4** | **OBJC — cards** | **prerequisite: an offscreen card path** (`--impostors` currently needs the GUI render hook); `cardLayer` on the §5.1 candidate list; card source = the last mesh in the ladder on the native target only; `cardCorpusHash` | §10 rows 4.x | none for stock (it still bakes from the near MODL and still writes `_fs`); moves the native baseline |
| **5** | **OBJP — the pop payload** | `crossPx16`, `alphaThreshold`, `seed`, cell ranges, `maxBoundRadius` | §10 rows 5.x | none for stock; moves the native baseline |
| **6** | **OBJD — the object drop** *(hard-ordered behind a live-confirmed C1)* | on the native target only: stop writing `.BTO`, `.manifest.txt`, the atlas, loose copies. **Terrain is not dropped here** — that is `--drop-terrain`, §6 | §10 rows 6.x, including the per-placement budget | none for stock; moves the native baseline (files disappear) |
| **7** | **OBJT — the hash handshake** | `objectCorpusHash`, `cardCorpusHash`, `--lodg-check` / `--lodi-check` beside the existing `--lodv-check` | §10 rows 7.x, four mutations of which one must **not** refuse | none |
| **C1** | **consumer, FO4CS side** | the six files of §8.3 minus C0's; the §8.6 refusal split; the §8.5 order of operations | §10 rows C.x, including a rendered-frame set | ships behind `[FarFieldObjects] bEnabled = 0` |
| **C2** | **far shadow map from our buffers** | `src/FarField/FarFieldShadow.{hlsl,cpp}` into `farMapDsv`; light-space card frame selection | altered-capture A/B against the engine's far map | none |
| **C3** | **census retarget** | `src/FarField/FarFieldCensus.h`, retiring `FarFieldLodBtoChannels.h` + `LodShadowMapCensus.h` | the row reports `not observing` when no `.bto` is in scene | **lands in the same wave as OBJD** |
| **8** | *(elastic)* **OBJX — our own proxy ladder** | decimate slot 0 into our own coarser representations with a recorded collapse map, enabling object geomorph | geomorph delta magnitude; ladder triangle ratios | v1 files must still validate at version 1 |

### 9.2 Why the order is this order

**Lane 0 first** because the standing gate of every other lane is meaningless without it, and because it costs one bake and one file.

**C0 and C0b before OBJG lands.** The consumer was one row of a table in the draft, marked "GUIDANCE ONLY", and its first primitive does not exist anywhere in this codebase: `DrawInstancedIndirect`, `CopyStructureCount`, `D3D11_RESOURCE_MISC_DRAWINDIRECT` and append/counter UAVs all return zero hits across `src/`. The only world-geometry draw in the tree is `src/Effects/CloudCoverageRuntime.cpp:1274` into its own target, and `src/GBufferBridge.h` shows the direction of travel — FO4CS *copies out* depth/normal/motion/albedo and has never written into the engine's G-buffer. Sizing eight generator lanes against a consumer that is realistically **6–12 lanes**, whose first lane is unprecedented here, is how a plan ships 500 MiB of savings on disk and a black horizon in the game. C0b is a spike with a picture as its gate; it is cheap and it either works or it changes the whole plan.

**OBJI before OBJG** on risk: additive first, and the lane whose failure mode is *a file nobody reads* before the lane whose failure mode is *geometry is wrong*. OBJI is also where the format's identity ordering is frozen (below), which OBJG then has to match rather than invent.

**OBJD hard-ordered behind a live-confirmed C1.** Not a preference — the drop is the only irreversible lane, and until a frame has been drawn from these buffers on bungo's machine there is nothing to be irreversible in favour of.

### 9.3 Three things Lane 1 must get right or Lane 2 inherits them

**The base ordering is frozen in Lane 1, over the full census.** Lane 1 writes a `u16 baseId` into a table that does not exist yet. If the ordering is the natural one — order of first appearance — then two region bakes produce libraries with different orderings and `.lodi` files whose indices address whichever `.lodg` was written last: instances silently attached to the wrong models, no CRC violated, no refusal. So the ordering is **ascending formId over the full worldspace ESM census, in every bake including a one-chunk one** (§8), Lane 1 writes `lodgIdentity = 0` with the `NOLIB` flag set, and the gate is the cheapest in the plan: bake region A, region B, and A∪B, and assert every ref common to two bakes has an identical `baseId` in all three.

**The manifest is not an independent reference, and it is not 90% of the record.** The manifest row (`src/lodgen.cpp:3027`, header at `:3481`) is `index base type x y z scale class height ref part` — **no rotation, no ao, no sky, no ground, no seed, no flags**. Twelve of 24 bytes have any reference at all, and the six rotation bytes, the field with the most novel packing, have none. It is also built from the same `r` struct, in the same loop, in the same process that feeds the encoder, and it only exists when `opts.identity` is on (`src/lodgen.h:206`, gated at `:3024`), so the check silently disappears under `--no-identity`. **Every Lane 1 floor is therefore against `tools/esm_refcensus.py`**, and the manifest comparison is kept only as a second leg.

**The manifest's ninth column is misnamed.** It is labelled `height` and the value written is `localMaxDist * r.scale` — a **radius** (`src/lodgen.cpp:3033`). The draft repeated the wrong name three times. The format document renames it, and every check names the column *index* and the *expression*, not the label.

### 9.4 Lane 2's reconstruction diff, made independent

The draft's version re-stitched a chunk and compared it against the `.BTO` **the same run wrote** — both artefacts products of one in-memory pass from the same `shapes`/`xf` arrays. A mis-transformed SCOL part, a wrong slot pick or a wrong LOD model resolution appears identically in both and the diff reads 0.0. It catches encoder bugs, which is real value, but it is not what `lodtfile.h`'s own rule means by *nothing verifies a format writer except an independent reader*.

Corrected: the verifier is a **separate invocation** that reads only `.lodg` + `.lodi` + the source `_lod.nif` set, and compares against the source model's own geometry transformed by the **python-ESM** placement. The `.BTO` comparison survives as a second leg. Tolerances unchanged (0.5 u position, 0.06° normal, 0.13 texel UV) plus one new exact floor: per placement, the reconstructed triangle count equals the source model's triangle count for the picked slot.

### 9.5 What the bake itself costs, which the draft never computed

| peak | figure |
|---|---|
| **migration bake output, Lanes 1–5** | today's full stock set **plus** the native files — the native bake does not get smaller until Lane 6, so plan for ~600 MiB of output, not 60 |
| **Lane 2's reconstruction diff** | holds the stitched `.BTO` chunk, the reconstruction, the whole ~7.0 MiB library and the whole ~5.1 MiB instance table simultaneously, per chunk. Peak ~40 MiB; run it per chunk, not per worldspace |
| **the card bake** | 30 bases × 81 frames = **2,430 offscreen renders** at the recommended policy (51 bases = 4,131). Wall-clock unmeasured because no card has ever been baked; Lane 4's first deliverable is that number |
| **the full-worldspace bake** | 481 chunks with AO. **Wall-clock unstated.** It is scheduled **once**, in Lane 0, and its per-file hashes become the baseline and its totals the only measured byte figures in this document |

Until that one bake runs, **every total in §2.1 is labelled [arith]**, and Lane 6's gate is not a total at all but a **per-placement budget** measured on three region bakes: `(lodg + lodi bytes) / placements ≤ 68 B` and `.BTO bytes / placements ≥ 3,000 B` on the same region, with the placement count from the python census. That measures the ratio per placement instead of extrapolating from one run's total, which is the ledger's "floor calibrated to the first run".
---

## 10. The harness

New spell `tests/spells/lodgen_native.sh`, beside `lodgen_farring.sh` and `lodgen_resources.sh`, plus the FO4CS-side checks in the consumer lanes' own suites. Three rules govern every row:

1. **Every floor must be shown to fail.** Mutation-test the writer, one field at a time, and record the mutation in the spell's header the way `lodgen_farring.sh` already records its floors. A floor that has never failed is decoration.
2. **Every floor must run on a chunk that exercises the case.** The draft ran everything on Sanctuary dim-4, which never reaches the bucket cap — the classic count that omitted the broken case. Where a row names a chunk, that chunk is the point of the row.
3. **An absence check needs a presence twin in the same run.** "0 crossed quads" passes when the card pass never ran; "0 emissive sheets" passes when the emissive writer never ran. Both get a positive leg.

**Fixtures the suite has to synthesise**, because the Commonwealth does not contain the case: a placement at `scale = 8.0` on the largest base; a base at index 65,536; a `.lodm` with `emissiveScale = 2.0` on one layer; 200 distinct LOD diffuses (the atlas caps at 128 cells) plus 3 shapes with UVs outside [0,1]; a mod worldspace with one far-flung chunk at +32767; a bake at `chunkCells = 8`.

**Rendered-frame checks are not optional.** The draft had forty gates and not one measured a picture, while the question this format exists to answer *is* a picture. The consumer rows below use instruments the project already owns: the `[Perf] frameMs` A/B regime, the altered-capture protocol, and the census.

### The gate table

| stage | check | floor | the failure it catches |
|---|---|---|---|
| **all** | stock bake vs `tests/fixtures/stock_baseline.sha256` | **every file, byte for byte** | any stock file changed by a native lane |
| **all** | a CLI run with **no** `--target` | file set and hashes identical to the pre-lane run | the CLI default drifting off `stock` |
| **all** | GUI argument vector from a seeded pre-lane `.ini` | **string-identical** to the pre-lane vector | the third target entry silently redefining `LodGeneration/target` |
| **all** | reserved fields | **all zero**; a set reserved bit is a named refusal | a v2 writer's field read as v1 data |
| **all** | payload alignment | every payload 4,096-aligned, table order, **every pad byte zero-filled** | two runs of the same bake not byte-identical |
| **all** | determinism | two bakes of the same inputs byte-identical | non-deterministic ordering |
| **all** | **sortedness in the file** (base/mesh/cluster/material/instance, §8) | **0 out-of-order rows** | a bake that is perfectly self-consistent and differently ordered from the last — invisible to the two-bake diff |
| **0** | the baseline's own mutation test | flip one constant in the stock vertex writer → the diff **names the file** | a baseline gate that cannot fail |
| **C0** | `GraphicsStateSaveScope` recording fake | **every** OM/RS/IA setter restored, stencil ref and viewport included | the `ComputeStateSaveScope` scar repeating on the graphics stages |
| **C0b** | one hardcoded indirect draw | **one triangle visible in a named rdc capture**, and the prepass state (RTVs, DS state, viewport, RS, **reversed-Z yes/no**) written into `docs/RE/far-field-draw-state.md` | eight generator lanes sized against a draw path nobody has proven |
| **1** | `instanceCount` vs `tools/esm_refcensus.py` | **exact**, per chunk | our own `placed` counter agreeing with itself while 6.17% of geometry is missing |
| **1** | cold-table `(refFormId, scolPart)` set vs the python census | **0 in A\B, 0 in B\A**, on (−20,24) dim4, (−32,16) dim16 and (−32,0) dim32 | a re-bin, a double-count, a dropped SCOL part |
| **1** | decoded position vs the python census's DATA | **≤ 0.125 u** (dim-4 chunk), max over all rows | wrong AABB, wrong chunk index, Y-mirror |
| **1** | decoded scale vs XSCL | **≤ 1.3e-4** | wrong divisor |
| **1** | decoded quaternion vs the python-composed matrix from DATA's Euler triple | **≤ 0.02°** worst over all rows | wrong selector, wrong bit packing, LSB order |
| **1** | tree `flags` bit0 vs `(treeHash>>8)&1` recomputed in python; tree yaw recovered from the stock `.BTO` vertices vs the seed-derived yaw | **0 mismatches**; yaw **≤ 0.02°** over 100 trees | the generator's position-derived yaw baked into the quaternion, or replaced by a new seed |
| **1** | `scale` and `baseId` out-of-range fixtures | a **refusal naming the ref or base**, never a clamp | the two silent u16 ceilings |
| **1** | region A, region B, A∪B | every ref common to two bakes has an **identical `baseId`** in all three | order-of-first-appearance indexing, which corrupts region bakes on day one |
| **1** | manifest-parser reconciliation (native twin of the identity harness) | rows parsed + rows skipped **== total lines − 1**, skipped counted by `C/I/M/A` prefix, not by parse failure | a parser counting only the lines it could parse |
| **1** | row order | NORTH_UP set; row 0 is NORTH | the `.lodt`/`.lodv` mirror trap |
| **2** | **independent** reconstruction (separate invocation, source `_lod.nif` + python ESM placement): triangles per placement | **exact match to the source model's picked slot** | a lost cluster, a wrong slot pick, a mis-transformed SCOL part |
| **2** | reconstruction: vertex position / normal / UV | **≤ 0.5 u / ≤ 0.06° / ≤ 0.13 texels** worst | wrong mesh AABB, oct pack/unpack asymmetry, tiling clipped |
| **2** | **(−32,0) dim 32 asymmetric drop proof** | forward difference (native has geometry, `.BTO` does not) **≥ 2,000, every member 0-in-BTO**; reverse difference **== 0** | the 6.17% silent drop asserted away instead of tested |
| **2** | geometry-bearing instances | every instance with a non-0xFFFF `rep` resolves to ≥1 cluster with ≥1 triangle; instances with neither geometry nor a card **== 0** | a writer that emits a record and no clusters |
| **2** | cluster index-blob padding ratio | **≤ 2.0×** (measured 1.63 at 16 tris) | a bad partition |
| **2** | **vertex duplication factor** | **≤ 1.35×** over the mesh-unique 289,003 | a cluster partition that shreds shared vertices, and the library size estimate with it |
| **2** | no cluster crosses a material or a mesh | **0 violations** | a bucket that cannot be drawn |
| **2** | `vertexCount ≤ 48`, `triangleCount ≤ 16`, local index < `vertexCount` | **0 violations**; a violation is a refusal naming the model | the u16 cap's replacement, made explicit |
| **2** | dedup, as exact invariants | `meshCount ==` distinct (modelPath, slot) from the python census, exactly; **0** meshes with byte-identical vertex+index ranges; **0** meshes referenced by no base; **0** bases whose `rep` points at another model path's mesh | dedup off for trees — 94.8% of placements but 0.6% of library bytes, which a "≥20×" floor would never notice |
| **3** | materials resolving to a live array layer | **100%**, on the 200-diffuse overflow fixture | an overflowed atlas's fallback, resurrected |
| **3** | shapes falling back to a direct texture | **0** on the array path **and ≥ 72 on the stock atlas path**, same fixture, same run | a fixture that proves nothing about the array path's advantage |
| **3** | array-axis cap | a class past 2,048 layers opens `arraySet + 1`; past 256 sets is a **refusal naming the material** | a `u16 layer` advertising 32× what D3D11 can bind |
| **3** | binary material table vs its `.lodm` | **field for field**: family / class / set / layer / threshold / `emissiveScale` | a stale copy on the render thread |
| **3** | emissive sheet, both legs in one run | **1 sheet** on the `emissiveScale = 2.0` fixture with exactly the referenced layer count; **0** on vanilla | "0 sheets" that cannot tell suppression from a writer that never ran |
| **4** | bases with no mesh in any `rep` and no card | **0** | an invisible object |
| **4** | card presence twin | **≥ 1** instance with `cardLayer != 0xFFFF`, and **≥ 7,000** on the far chunk | "0 crossed quads" passing because the card pass never ran |
| **4** | no `_fs` crossed quads on the native target | **0 bytes** | 280 B a placement of dead geometry |
| **4** | card array bytes | **≤ 32 MiB** at the default N=8 / 30 bases (measured target 30.39) | a candidate-set or frame-count explosion — the draft's ≤24 MiB failed its own default |
| **4** | card vs source-mesh silhouette IoU | **worst view ≥ 0.85** over a fixed set: the 8×8 grid's own frame directions **plus 9 deliberately off-frame directions**, rasterised at 256², alpha threshold 16/255 | a card that is right from one angle — "best view" picked the weakest reading |
| **4** | carded base radius | every `base.boundRadius ≥ 1`; for a carded base within 5% of `max(octHalfW, octHalfH)` read back from the `.lodm` | radius 0 by construction, collapsing every screen-size test |
| **4** | `cardCorpusHash` | re-bake the card library with one base added → **refusal** | every `cardLayer` silently reindexing |
| **5** | `boundRadius` vs the manifest's **column 9** (`localMaxDist * scale`, named by index and expression) | **≤ 1 u**, and **0 bases at radius 0** | the fade band collapsing, and the misnamed `height` column |
| **5** | `crossPx16` monotone down the ladder | **0 violations** | a ladder that steps backwards |
| **5** | seed reproducible across two bakes | **100%** | the fade re-randomising every bake |
| **5** | cell ranges partition the chunk | **sum == instanceCount**, no overlap, **`chunkCells²` entries a chunk** — verified at `chunkCells = 8` too | near-field suppression missing objects; a reader written to a hardcoded 16 |
| **5** | `maxBoundRadius` | for every instance, `z + boundRadius ≤ zMin + zExtent + maxBoundRadius` | the 8,224-unit tree popping out of a chunk box built from origins |
| **6** | native bake directory census | **exactly** the named file set, nothing else; **no terrain file removed without `--drop-terrain`** | a leftover writer; a GUI switch producing a world with no distant terrain |
| **6** | per-placement budget on three region bakes | `(lodg+lodi)/placements` **≤ 68 B**; `.BTO/placements` **≥ 3,000 B**, placements from the python census | a headline extrapolated from one run's total |
| **6** | `chunkCount` cap | a mod worldspace with a chunk at +32767 → **refusal**, not a 103 GB dense table | an i16 extent with no cap |
| **7** | four `objectCorpusHash` mutations, each a separate run | (a) shift one REFR DATA X by 1.0 → refusal naming the field; (b) repoint one base's MNAM slot 1 → refusal; (c) flip one byte in a source `_lod.nif` → refusal naming `modelCorpusHash`; (d) **edit an unrelated DIAL record → must NOT refuse.** Floor: **3 refusals + 1 acceptance** | a hash blind to every object mutation (the terrain VHGT hash is), and a hash that refuses on every unrelated mod, which the user then disables |
| **7** | `.lodg`/`.lodi` pairing | mismatched `lodgIdentity` → **refusal**; zero identity without NOLIB → **refusal** | a half-updated install |
| **7** | refusal-class split | a `pluginCorpusHash` mismatch **loads, logs, sets `stale=1`, keeps rendering**; a version mismatch **refuses and does not hide `spLODObjectRoot`** | an empty horizon the first time the user installs any mod |
| **C1** | order of operations | the engine tree is hidden **only after** a nonzero drawn-primitive readback for one frame | hiding the engine's far field before ours is known to work |
| **C1** | `spLODObjectRoot` scope measurement | hide the node, capture, **list every pass whose draw count changed** | three unnamed conflicts behind one named one |
| **C1** | draws resident | **8–14** main view, plus the same bucket set per far-shadow slice, both reported | a draw-call headline that counted only the main view |
| **C1** | **cull append overflow counter** | **0 per frame** in downtown Boston, and the counter is a census row | `CopyStructureCount` clamping and discarding surplus clusters silently, per frame, in dispatch order — `lodgen.cpp:3144` moved to the GPU where no disk gate can see it |
| **C1** | `[Perf] frameMs` A/B at `[FarFieldObjects] bEnabled` 0/1, downtown Boston, same scene | within bungo's **0.1 ms** bar | the 2.66×-vertex-shading cost of the fixed-48 path, and the 24% overlap-shell append |
| **C1** | motion vectors | the far field writes motion; a camera pan shows **no ghosting** in an altered capture against a static control | TAA smearing across the whole horizon |
| **C2** | far shadow map, altered-capture A/B | dark% over the far-shadow region, native vs stock, against a measured noise floor from a control pair | the far map going dark when the engine tree is hidden; camera-facing card silhouettes in the light's view |
| **C3** | the census row | reports `not observing: no .bto in scene`, never a silent "vanilla" | a live instrument lying in bungo's logs, and a wave blamed for a regression that never happened |
---

## 11. What is deliberately not done, and what stays elastic

### 11.1 Not done, with the reason

- **Object geomorphing.** The finer and coarser representations are Bethesda's own MNAM slot meshes with no vertex correspondence — there is no collapse to record. Terrain geomorph already ships. §7.6.
- **Virtual texturing for objects.** A page table, an indirection texture and a feedback pass is 5–10 lanes with a long artefact tail [con], and the charter already struck it. Our whole object texture set is 17.75 MiB of arrays. TERRAIN1's `.lodv` is terrain's business and is not extended to objects.
- **Nanite-style cluster DAGs.** SM6 wave intrinsics, 64-bit image atomics and a compute rasteriser — D3D11 has none of them. And it would be absurd: the Commonwealth's *entire* unique far-field geometry (156,818 triangles) is smaller than one Nanite hero asset. Our fixed 16-triangle clusters take the useful half (few draws, a real per-cluster bound) and none of the impossible half.
- **More SLOD tiers.** RAGE has four merged proxy tiers because a GTA block is thousands of distinct facades. We have 3,355 distinct models totalling 9.6 MB.
- **Merged/atlased proxies as the default.** That is exactly today's format, and it is the 423.6 MiB. Its virtue in Unreal is collapsing thousands of distinct meshes into one draw; our far chunk has 123 distinct bases.
- **Procedural placement** (Frostbite/Decima style). It would destroy the `(ref, part)` key — a *measured* invariant, 406 objects shared between two rings over Sanctuary — in exchange for a procedural one. Never trade a measured identity for a derived one.
- **Streaming.** 63.8 MiB resident, one upload at worldspace load. Building a streamer would be 1–2 lanes of the exact code most likely to hitch on one machine.
- **The terrain drop.** Moved behind `--drop-terrain` and out of every headline; `.lodv` is unsized, and most of the 1.63 GB is a transfer, not a saving. §6.
- **BA2 reading.** v1 is loose-files-only, stated as a shipping constraint. A `BSResourceNiBinaryStream` path is v2. §8.5 conflict 4.
- **Cross-mod instance authoring.** Withdrawn from v1: `lodgIdentity`, the u16 `baseId` and the index-as-identity rule all forbid it. Needs a formId-keyed base namespace. §8.7.
- **Distant lights.** Parked by bungo 2026-09-06 17:4x. The format reserves for it anyway: `flags` bit3 (emits), the `seed` byte, the `emissiveScale` in the material table, and the instance record's 2 reserved bytes. `scratchpad/priorart_distant_lights.md` keeps the thinking.
- **Changing anything on the `stock` or `fo4cs` targets.** The crossed quads, the atlas, `--atlas-bc1`, the merge, the far-ring simplifier, the `.btr` and its sheets, the 32-byte `.bto` descriptor and its still-unrun in-engine gate: all exactly as they are.

### 11.2 Why not X — decisions kept against a reading that pushed the other way

- **Why not shrink the instance record to 22 bytes** now that `boundRadius` is derived? Because 22 is neither 8- nor 4-aligned, a `StructuredBuffer` of 22-byte elements costs the shader an unaligned gather, and the two bytes are worth more as a **declared, zero-checked v2 growth slot** (per-instance tint, or a light-record index) than as 326 KiB. The stride stays 24 and the reserved field is gated at zero.
- **Why not an index buffer**, when the fixed-invocation draw does 2.66× the vertex shading of an indexed one? Because the tree has never created a vertex or index buffer for world geometry, and adding an IA path means telling the engine's input assembler about a 16-byte vertex inside a shared, shadow-cached device context. The three size classes (12/24/48) take most of the waste back at the cost of a few draws, and the remaining cost is measured against the 0.1 ms bar in C1 rather than assumed away. If that A/B fails, an R32_UINT index buffer is 1.88 MiB and the fallback is one lane.
- **Why not keep the atlas**, now that the Commonwealth has 1.49× headroom rather than an overflow? Because the byte argument was never the decisive one: **9 of 29 shapes in the measured chunk have UVs outside [0,1] and can never be atlased at any sheet size**, and the sheet's coarsest measured UV quantum (2.00 texels) is exactly the size of its whole guard band, so mip 2 and deeper — precisely what a LOD chunk samples — bleeds between cells. Either of those is sufficient on its own, and neither depends on the layer count.
- **Why not put the cold table in its own file** in v1? Because `PARTIAL` merging is by chunk key and a third file triples the ways a region bake can half-update. It stays parallel and optional-to-load; splitting it is listed as elastic.
- **Why not fold the 20 × 128² layers into the 256² class**? It costs 3.75 MiB and saves two draws out of eight-to-fourteen. Revisit only if a worldspace ends with many classes.
- **Why not keep the `.BTO` leg of the reconstruction diff**, given it is not independent? It *is* kept — as a second leg. It catches encoder bugs cheaply and it is the only thing that can compare against the stitched output at all. What changed is that it is no longer called the load-bearing gate.

### 11.3 Elastic, and where the knob is

| elastic | today's default | why it can move |
|---|---|---|
| cluster size | 16 triangles | swept [nat]: 8 → 1.49× index padding / 29,284 clusters; 32 → 1.88× / 9,198. One header field (`clusterMaxTris`) |
| draw size classes | 3 (12 / 24 / 48) | two bits of `cluster.flags`; fewer classes = fewer draws and more wasted invocations |
| card grid N, tile class and candidate set | N=8, 44×64, 30 far-field tree bases → 30.39 MiB | the only number here that is a taste judgement; §5.3 prices all three grids and both candidate counts |
| array size classes | 2 (66 × 256², 20 × 128²) | folding costs 3.75 MiB, saves 2 draws |
| array set size | 2,048 layers a set | the D3D11 cap; `arraySet` grows the address space to 524,288 a class |
| the cold table | shipped, parallel | a consumer that never needs `(ref, part)` skips loading 1.24 MiB; it can also be split into its own file in v2 |
| chunk granularity | 4 cells (16,384 u) | one header field (`chunkCells`); at 65,536 u the position step is 1.0 u, still 0.067 px, and the cell-range blob becomes 256 entries a chunk |
| instance stride | 24 B, of which 2 reserved | one header field; a v2 that needs a per-instance tint or a light record spends the reserved slot, and the reserved-bit gate says so |
| whether FO4CS ever reads any of it | it does not yet | **if the consumer is never written, the native target's files are inert and the other two targets are what ships.** Nothing here can break anyone's game by existing on disk — and, since §6 moved the terrain drop behind its own flag, nothing here can break a game by the *bake selection* either |

**The one thing that is not elastic:** the `stock` and `fo4cs` targets' output. Every lane's standing gate is a byte-identical stock bake against Lane 0's checked-in baseline, and if that gate cannot be made to pass, the lane does not land.

---

### Paths

- Spec scratch and measurements: `C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/a53dc563-42e1-49bb-a9c5-cc9b709272e7/scratchpad/nat/spec/quant.py`, `quat6.py`, `totals.py`, `totals2.py`
- Format precedents read: `E:/Projects/NifskopeWildWastelandEdition/src/lodtfile.h`, `src/io/lodvfile.h`, `src/io/lodmfile.h`, `docs/LODGEN_BTD_FORMAT.md`, `docs/LODGEN_IMPOSTOR_SPEC.md`, `docs/LODGEN_VERTEX_PACKING.md`
- The existing target fork: `E:/Projects/NifskopeWildWastelandEdition/src/lodgenmanager.cpp:621-630`, `:1153-1178`, `:1516`
- Generator anchors quoted here: `src/lodgen.cpp:862`, `:1255`, `:2476`, `:2499`, `:2608`, `:2881`, `:2915`, `:~2955-2964`, `:~2981`, `:~2986`, `:3024`, `:3027`, `:3033`, `:3144`, `:3208`, `:3481`, `:4237`, `:7200`; `src/lodgen.h:206`; `src/esmdata.cpp:210`
- CLI flag table: `E:/Projects/NifskopeWildWastelandEdition/src/nifcli.cpp:5070-5170`
- Terrain lane in flight: `scratchpad/spec_terrain_vt.md`, `scratchpad/reqs_terrain_clipmap.md`
- Consumer side: `E:/Projects/Fo4CommunityShaders/wt-fixfirst/src/FarField/`, `src/Buffer.h:84-110`, `src/RenderAnchors.h:15-31`, `src/RenderAnchors.cpp:223`, `src/ComputeStateSaveScope.h`, `src/GBufferBridge.h`, `src/Base/Temporal/TemporalAccumulator.cpp`, `src/Effects/FirstPersonShadowRuntime.cpp:18969`, `:19450`, `:19646`, `:19677`, `docs/plans/lod-revamp-charter.md`, `docs/RE/far-field-terrain-lod.md`

### Uncertainties this spec did not close

- **No total in §2.1 is measured end to end.** Every one is [arith] over a measured per-unit anchor. Lane 0 schedules the single full-worldspace bake that turns them into measurements, and its wall-clock is itself unknown.
- **Cluster count (~16,008) is arithmetic**, from [pri]'s measured model-size histogram, not a partition of the real library. **The 1.25× vertex duplication factor is an estimate** in a 1.15–1.35 range. Both are gated, not assumed.
- **String-blob size (~250 KiB) is an estimate:** 3,355 model paths + ~177 `.lodm` paths at ~70 chars.
- **The card candidate set's second class — bases with no authored LOD mesh in any slot — is unmeasured.** The draft's 98.4% figure counts something else (bases that do not fill slots 2 and 3, which slot fallback fills for them). Measuring it is Lane 4's first task, and the budget moves if it is large.
- **No impostor card was baked this session** — `--impostors` needs the GUI render hook, which is forbidden here, and an offscreen path does not exist. Every card byte in §5 is scaled from an earlier lane's measured 44×64 frame class.
- **Whether FO4's depth is reversed-Z is not stated here** because it is not measured here; C0b records it before any pixel shader is written.
- **No frame was rendered and nothing was run in the game.** Every claim about *bytes* is measured or derived from a measurement; no claim about how any of it *looks* is.
- **The 26%-tilted rotation census reads REFR `DATA` only** [pri] — SCOL parts (67% of the measured chunk's placements) carry their own transforms, and the generator adds its own hash-derived yaw and UV mirror to trees. The 6-byte rotation is sized for the worst case either way.
- **`src/lodgen.cpp` and `src/lodgenmanager.cpp` were being edited by another lane throughout.** Every line number quoted is anchored by quoted text; re-grep the anchor rather than trusting the number.

---

## OPEN QUESTIONS FOR THE OWNER

1. **Impostor cards cost 30.4 MiB at N=8 (81 frames a tree) or 9.4 MiB at N=4 (25 frames), over the 30 far-field tree bases — which do you want?** *Recommendation: N=8, because N=4's blend triangles are large enough to see when a card stands in for a real mesh at ring 0–1, and 21 MiB is cheap against the 478 MiB the format saves elsewhere.*
2. **Does the native bake ever replace the stock set on your install, or stay additive alongside your ~4.0 GB pyramid-plus-`.btr` ruling?** *Recommendation: additive until C1 has drawn a frame on your machine and you have accepted the picture; the disk saving is real but it is not worth an irreversible drop before that.*
3. **What should the third entry in the LOD Generation target box say — you will see this string every bake?** *Recommendation: "FO4 Community Shaders — native objects (experimental)", with the existing "FO4 Community Shaders" entry untouched so your saved profile keeps behaving exactly as it does today.*
4. **Order of work: two consumer spikes first (the graphics-state scope and one hardcoded indirect draw, ~2 lanes, zero disk saving), or generator Lanes 1–2 first (the full 39× on paper, nothing visible in game)?** *Recommendation: the spikes first, because the drop lane is hard-ordered behind a live-confirmed consumer and the indirect-draw primitive has never been used in this tree.*
5. **Should the terrain drop stay behind its own `--drop-terrain` flag, gated on TERRAIN1 publishing a sized `.lodv`?** *Recommendation: yes — most of the claimed 1.63 GB is a transfer to a file nobody has sized, and without the flag a GUI switch can produce a world with no distant terrain.*
