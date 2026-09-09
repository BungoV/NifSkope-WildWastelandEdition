# TERRAIN1 — Ground cover in the far terrain, and an optional terrain virtual texture

**What this lane delivers.** Two opt-in, off-by-default additions to the terrain half of the LOD generator: a **ground-cover plane** (the LTEX→GNAM→GRAS chain read for the first time, composited against the cell's splat paint, slope-gated, written to the alpha of `<ws>.<dim>.<x>.<y>_data.DDS`, with a matching grass tint mixed into the far albedo so the stock engine stops rendering meadows as bare dirt), and a **terrain virtual texture** (the same bake restructured into a five-level pyramid of 256-texel tiles with an 8-texel border, one `.lodv` container per level under `Data\Terrain\`, indexed by a `terrainVT` `.lodm`). Neither feature changes a single output byte when its toggle is off, and that is a gated, measured claim (checks C1, C2, V17), not an assertion.

**The exact command a reviewer runs to see it work** (game down; the build skill is `nifskope-ww-build-verify`):

```sh
cd /e/Projects/NifskopeWildWastelandEdition && \
  ESM='E:/Tools/Fallout 4/Data/Fallout4.esm' \
  DATA='E:/Tools/Fallout 4/DataUnpacked/Data' \
  tests/spells/lodgen_ground_cover.sh && \
  ESM='E:/Tools/Fallout 4/Data/Fallout4.esm' \
  DATA='E:/Tools/Fallout 4/DataUnpacked/Data' \
  tests/spells/lodgen_terrain_vt.sh && \
  tests/spells/lodgen_terrain.sh && tests/spells/lodgen_identity.sh
```

Each of the four prints `RESULT PASS` and exits 0. The first two are new in this lane; the last two must be unchanged, because a terrain-only change must move neither.

**Target repo:** `E:/Projects/NifskopeWildWastelandEdition` (branch `main`, working tree dirty — 60 entries at HEAD `2dd8444`, do not clean it).
**Written against:** `release/NifSkope.exe` of 2026-09-06 17:17, which is newer than `src/lodgen.cpp` (17:09) and `src/lodgenmanager.cpp` (16:21). Every number below either came from a command run against that exe / the shipped corpora, or is arithmetic over source the implementer can read; each is labelled.

---

## 1. What is being added and why

**(a) Ground cover.** The LTEX→GNAM→GRAS chain that the Creation Kit uses to grow grass is read for the first time, composited per texel against the cell's splat paint, gated by terrain slope, and written into the one free channel the terrain data sheet has — the alpha of `_data.DDS` — with a matching grass tint mixed into the far albedo so the *stock* engine, which reads no data sheet at all, still stops rendering meadows as bare dirt.

**(b) A terrain virtual texture.** The same bake, restructured into a five-level pyramid of 256-texel tiles with an 8-texel border, one binary container per level under `Data\Terrain\`, indexed by a new `.lodm` of kind `terrainVT`, so a consumer can stream terrain colour at a fixed memory budget instead of loading 3,060 whole chunk sheets. The pyramid's default finest level is 2 cells per tile (32 world units per texel — *exactly* vanilla's finest ring density, measured over 6,120 shipped files) with 1 cell per tile as the "full" option at 16 units per texel.

FO4CS has no consumer for either yet — that is deliberate; the owner asked for elasticity, so this spec fixes the *file formats* and leaves every runtime policy (page tables, feedback, residency) out of the bake.

**What the owner actually pays in disk.** The headline pyramid figure is not the delivered cost, because §4.4 assembles the `.btr` chunk sheets *from* the pyramid rather than replacing them — both live on disk. The delivered totals are in §4.2 and the panel computes them live; the short version is 3.081 GiB (VT on, cover off) against today's 1.495 GiB.

---

## 2. The ground-cover law

### 2.1 What is read from the plugin, and where it lives

Nothing in the repo reads GRAS today. `grep -n "GNAM\|GRAS" src/esmdata.cpp src/esmdata.h` returns **zero hits**; `EsmWorld::ltexTextures` (`src/esmdata.cpp:497-536`) walks `LTEX → TNAM → TXST` and stops. Both pieces are added beside it.

**Measured corpus (all three plugins parsed with exact byte accounting, 0 records left bytes over):** Fallout4.esm 105 LTEX / 82 GRAS; DLCCoast 8/10; DLCNukaWorld 15/15; DLCRobot and the three Workshop ESMs 0/0. Totals **128 LTEX / 107 GRAS**. 63 of the 128 LTEXes carry 1–4 `GNAM` links (never more; distribution in Fallout4.esm: 0×53, 1×6, 2×26, 3×16, 4×4), 153 links in all.

**GRAS payload is `DATA`, 32 bytes, in all 107 records — there is no `DNAM`.** Subrecord inventory over all 107: `EDID, OBND, MODL, MODT, DATA`, one of each, no exceptions. Layout, confirmed three independent ways (xEdit `wbRecord(GRAS)`; the byte-variation footprint of the corpus, where offsets 3, 6–7 and 29–31 co-vary in exactly three fixed groups — the signature of stale 4-byte slots now written 3/2/1 bytes deep; and every float's exponent byte falling in the range its CK default implies):

| off | type | field | measured range over 107 |
|---|---|---|---|
| 0 | u8 | Density | 1..96, 25 distinct; mode 2 (×17), 10 (×15) |
| 1 | u8 | Min Slope | **0 in all 107** |
| 2 | u8 | Max Slope | 14..70 deg, mode 40 and 50 (×13 each), never 90 |
| 3 | — | stale | {0,63,127,233} |
| 4 | u16 | Units From Water | {0×64, 1, 32×4, 64×29, 96×2, 128×4, 300×3} |
| 6..7 | — | stale | {0000, 803e, 703f} |
| 8 | u32 | Units-From-Water Type | {0×65, 2×7, 6×1, 7×34} |
| 12 | f32 | Position Range | 6..76, mode 32.0 |
| 16 | f32 | Height Range | 0.1..0.75, mode 0.2 |
| 20 | f32 | Colour Range | 0..0.3, mode 0.15 |
| 24 | f32 | Wave Period | 8..200, mode 10.0 |
| 28 | u8 | Flags | 0x01 Vertex Lighting (all 107), 0x02 Uniform Scaling (49), 0x04 Fit to Slope (104) |
| 29..31 | — | stale | {000000, 90903d} |

The bake reads **Density (0), Max Slope (2) and MODL** and nothing else. Min Slope is 0 everywhere, so a lower gate would be dead code.

### 2.2 The per-LTEX constants

For an LTEX form `L` with grasses `g ∈ GNAM(L)`:

```
D(L) = Σ_g  density(g)                                     (u16 sum; 0 when L has no GNAM)
S(L) = ( Σ_g density(g) · maxSlope(g) ) / D(L)   degrees    (0 when D(L) == 0)
T(L) = ( Σ_g density(g) · avg(g) ) / Σ_g density(g)         (RGB, see §3; grasses with no
                                                             resolvable texture drop out of
                                                             this sum only, not out of D)
```

Measured: per-LTEX `D` spans **2..226** (median 14); the extremes are `LPreWarGreenLawn01` and `LPreWarWildGrass02` at 226 and `LDebrisRubbleHightechChunk01` at 2.

`D`, `S` and `T` are cached per LTEX form exactly the way `ltexCache` (`src/esmdata.h:175`) caches the texture pair, and are computed lazily on first use, so a worldspace whose paint names no grass-bearing LTEX never opens a GRAS record.

### 2.3 The per-texel formula

Evaluated inside the existing paint loop (`src/lodgen.cpp:4609-4718`), using operands the loop already has in hand: the quadrant `q` picked at `:4625`, `baseTex` at `:4654`, and the bilinear opacity `a` of each layer computed at `:4661-4667`.

```
per quadrant (NOT per texel — see "resolve once per quadrant" below):
  for each layer i in land.layers[q]:  resolve D(ltex_i), S(ltex_i), T(ltex_i) ONCE
per texel:
  for each layer i in land.layers[q]:  a_i = the SAME bilinear opacity the diffuse loop computes
  A     = Σ_i a_i
  if A > 1:  a_i ← a_i / A ;  A ← 1                    (renormalise: measured on 62 of the
                                                        3,059,576 texels that carry at least
                                                        one alpha layer, 0.002%, worst A = 1.906)
  wBase = 1 − A
  Dtex  = wBase·D(base) + Σ_i a_i·D(ltex_i)
  Stex  = [ wBase·D(base)·S(base) + Σ_i a_i·D(ltex_i)·S(ltex_i) ] / Dtex      (0 if Dtex == 0)
  θ     = degrees( acos( clamp(nrm.z, 0, 1) ) )        from the SAME unit normal the msn
                                                        write builds at src/lodgen.cpp:4711
  gate  = clamp( (Stex + 5 − θ) / 10 , 0, 1 )
  cover = clamp( round( 255 · gate · Dtex / COVER_FULL ), 0, 255 )
  COVER_FULL = 96.0 by default; --cover-full overrides it
```

**The renormalised `a_i` must not reach the colour composite.** Renormalisation is a cover-side correction only; the diffuse loop keeps the opacities it computes today, or `--no-cover` and `--cover` produce different albedos on those 62 texels and C1 fails for a reason that is not a cover bug. The implementation keeps a separate `aCover[]` array. Mutation M7 in §8.3 proves the separation.

**Resolve once per quadrant.** `D`, `S` and `T` are per-FORM scalars and the layer set is constant across a quadrant, so all three are resolved into a small fixed array when the quadrant is entered — ≤ 8 hash lookups per quadrant, not ≤ 8 per texel. Doing it per texel would add ~1.4 billion avoidable `QHash<quint32,…>` lookups over the finest pyramid level, on top of the per-texel `lodgenLoadTexture` lookup `sampleLtex` already performs at `:4638`. Check C15 gates this by counting, not by wall clock.

**Two painted-texel denominators, and which is which.** The corpus carries two totals that do not divide into one another: **4,445,398** is the count of painted grid points (15,382 painted quadrants × 289 VTXT points, which reconciles exactly), and **3,059,576** is the count of those points that carry at least one *alpha* layer — the only population renormalisation can act on. The bake's census line (§2.7) prints both with their denominators so the pair reconciles on every run rather than being carried in prose.

Layer resolution rules, all three chosen to match what the *colour* in the same texel does, so cover and albedo never disagree about what is growing there:

- **`layer.ltex == 0` (NULL LTEX).** The diffuse loop paints `dominantBase` (`src/lodgen.cpp:4672-4673`). Cover therefore uses `D(dominantBase)`, `S(dominantBase)`. Measured: 3,227 of 55,679 Commonwealth alpha layers (5.8%), including two in Sanctuary cell (−20,24). **`dominantBase` is computed per bake unit over that unit's cells** (`:4596-4607`), so it is scope-dependent; §4.4 makes that scope load-bearing and V9 gates it.
- **`layer.ltex` names a form that is not a record.** Contributes `D = 0`, `S = 0` and increments the `danglingLtex` **error** counter. Measured: exactly one such layer in the Commonwealth (`000464C5`). A dangling reference is a data error, not paint intent, so it does *not* fall back to the dominant base.
- **`land.baseTex[q] == 0`.** Already falls back to `dominantBase` at `:4654`; cover follows.

**Why `COVER_FULL = 96.0`, and why it is now a flag.** 96 is the largest `Density` byte in the entire vanilla corpus (`PreWarLawnGrassObj02`); "one grass at the densest an artist ever authored" is full cover. Measured over all 4,445,398 painted Commonwealth grid points the largest composited `Dtex` is **80.0**, so vanilla's *painted* texels never clip, and the mean over the 2,059,913 nonzero texels (10.35) lands on byte **27** — the field uses the low third of the range in ordinary terrain and the top of it on lawns, which is the shape you want.

That measurement does **not** cover the 132,307 unpainted quadrants (89.7%), where `wBase = 1` and `Dtex = D(base)` outright, and `D(L)` is a *sum* that reaches 226 (`LPreWarGreenLawn01`) — 226/96 × 255 = 600, which clips. Only 162 of 147,456 quadrants have a grass-bearing base layer at all, so the exposure is small, but it is not zero and it is not measured. Two consequences, both shipped: `--cover-full <n>` exists, and the §2.7 census prints the **observed** maximum `Dtex` and the clip count separately for painted and unpainted quadrants, so the first vanilla run reports the truth instead of the harness discovering it. Under the owner's own load order it matters more: True Grass raises one LTEX to 14 links, i.e. `D` up to ~1,344, which saturates cover to 255 across whole regions — carrying no information while forcing BC3 everywhere. That is what the flag is for.

Normalising against a per-run maximum was rejected: it makes two chunks baked in different runs incomparable and destroys byte-determinism. Comparability across runs and across mod setups is the whole game for a streaming format.

**Why a ±5° slope gate.** Max Slope is 14..70 deg over the corpus with 20 records capping at ≤30, and 104 of 107 set Fit-to-Slope; a hard step at `Stex` would alias hard against a heightfield sampled every 128 units. 10 degrees wide is chosen, not measured — it is listed as elastic in §9.

**Measured non-degeneracy (Commonwealth, 36,864 LAND records, 147,456 quadrants):** 15,382 quadrants carry paint; of those 13,850 (90.0%) contain some grass; 2,059,913 of 4,445,398 painted texels (46.3%) get a nonzero cover from opacity × density alone, before the slope gate. Only 162 of 147,456 quadrants (0.1%) have a grass-bearing *base* layer — grass in the Commonwealth is 98.9% an alpha-layer phenomenon (2,037,530 texels vs 22,383), which is why the base term cannot be dropped but also cannot carry the field on its own.

**Resolution is not the limit for the paint; it is for the gate.** The paint is 33×33 shared-edge samples per cell = **128 units per sample**; the bake's texel at dim 4 is `16384/512` = **32 units**. The cover plane is 4× finer than anything authored, so no paint detail is lost and none is invented. **The slope gate does not have that headroom.** `θ` is built from `nrm`, whose central differences read the same 128-unit heightfield, so the gate resolves slope four times coarser than the texel it gates and will systematically over-report cover on ground that is steep at sub-128-unit scale. The ±5° half-width is a smoothing constant against a 128-unit operand, not a 32-unit precision claim; read it that way.

**Do not recompute cover from the stored `.lodt` alpha plane.** `src/lodtfile.cpp:863-885` keeps only the five strongest layers per quadrant and `:918-925` quantises each opacity to 3 bits. Measured: 1,074 Commonwealth quadrants (0.73%) exceed five layers — which reproduces the "99.27% fit in five" comment already at `src/lodtfile.cpp:869`. Cover is baked from the ESM's full ATXT set at 8-bit opacity (opacity is exactly n/255 in all 4,102,951 Commonwealth VTXT entries, 0 non-integral).

**The cover value is ordinal in scale and linear in composition.** It is `255 · gate · Dtex / COVER_FULL`, so it is *not* a coverage fraction — nothing states what `Density` counts per unit area, and a consumer must not read 128 as "half the ground is grass". But it *is* linear in `Dtex` everywhere except the clamp, which is why averaging four cover bytes is the correct filter for a coarser tile (§4.3) and why the `.lodm` says `"ordinal": true, "linearInComposition": true` rather than the bare `ordinal` that would forbid the filter it needs.

### 2.4 What it does not model, and why that is acceptable

| not modelled | measured scale | why acceptable |
|---|---|---|
| **Water constraints.** 43 of 107 GRAS carry Units From Water 1..300 across 8 type values; the three TallKelp grasses want ≥300 units *below* water. | 40% of records | Whether "Units From Water" is a vertical height difference or a horizontal distance to shore is **not established by anything measured**, and the two readings need completely different machinery (a plane compare vs a distance transform). The data sheet's **B channel already carries shore proximity** at the same 512² resolution (`src/lodgen.cpp:4831`), so a consumer that wants to suppress cover near water can do it from a channel we already ship, without the bake guessing. Guessing here would be the `MISTAKES.md` "wrote it in a spec without checking a pixel against its texel" shape. |
| **Per-GRAS multiplicity.** An LTEX with 4 GNAMs spawns four types with different densities, slopes, water rules and meshes. | max 4 links, 26 LTEXes at 2 | Collapsed to a density-weighted scalar `D` plus a density-weighted `S`. The alternative is 4 channels the sheet has no room for and the consumer has no use for at 32 units per texel. |
| **Placement jitter.** Position Range runs to 76 units against a 32-unit texel at dim 4. | 2.4 texels | The field is a probability of cover, not a footprint. At LOD range the smear is sub-pixel. |
| **What Density counts.** The byte tracks intent (PreWarLawn 91–96; rubble 1–3; DriedGrassObj01 18 vs its Sparse variant 6) but nothing states instances per what area. | — | Declared **ordinal in scale, linear in composition, normalised against a fixed constant**, in the `.lodm` and in the doc. A consumer fades on it; it is not a coverage fraction and must not be labelled one. |
| **Object occlusion, precombines, navmesh.** | unmeasured | Not in the ESM records read. A paint-derived cover map will show grass under a building whose footprint was never painted out. Vanilla LTEX artists already encoded "same texture, deliberately no grass" as a separate record: **19 EDIDs end in `NoGrass`, 18 have a same-named twin, and all 19 carry zero GNAM** — so the paint carries most of the intent, and the residue is an artefact at 32 units per texel. |
| **Engine grass settings** (`iMinGrassSize`, `fGrassStartFadeDistance`). | outside the ESM | Runtime policy. The bake describes the ground; the consumer decides where grass stops being drawn. His live profile has `iMinGrassSize=20`, `fGrassStartFadeDistance=7000`. |

### 2.5 The format change to the data sheet

Today `_data.DDS` is written by `lodgenWriteDds(..., bc3=false)` at `src/lodgen.cpp:4868`: BC1, 512², 8 mips (the chain stops while `mw > 4 && mh > 4` at `:3645`), **174,888 bytes** (21,845 blocks × 8 = 174,760 payload + 128 header — the arithmetic reproduces the source's own "175 KB a chunk instead of 350" comment at `:4861`). The alpha is a hard `0xFF` at `:4864` and BC1 drops it (`:3661` forces 0xFF through the mip chain).

**The change:** when the chunk's cover plane is not everywhere zero, `_data.DDS` is written **BC3** — `lodgenWriteDds(..., bc3=true)` — with `A = cover`. Everything else about the file is unchanged: same 512², same mip rule, same north-up row order (`aoTex[(RES-1-j)*RES + i]`, `:4864`), same R/G/B.

> **Why not "the RGB bytes must change anyway across a BC1→BC3 switch".** They do not, in *this* encoder. `lodgenEncodeBC1Block` takes an `allowPunch` argument and the call site passes `!bc3` (`src/lodgen.cpp:3729`); punch-through (the three-colour `c0 <= c1` mode) is entered only when some texel's alpha is `< 128`. Today's BC1 terrain sheets are alpha-255 everywhere, so `punch` is already always false, and the BC3 path forces `allowPunch = false`. Both paths therefore run the identical four-colour endpoint search over identical RGB and emit identical colour bytes. The claim is checked, not assumed: C3b `cmp`s the decoded RGB planes across the switch.

**The alpha the BC1 fallback writes is `0xFF`, not the cover byte.** The cover plane is accumulated into its own `std::vector<quint8> coverPlane` during the paint loop; only after the loop, and only when `coverMax > 0`, are those bytes OR-ed into `aoTex`'s alpha. If `coverMax == 0` the alpha stays `0xFF` and the BC1 path is byte-identical to today. Writing cover-0 as alpha 0 into a BC1 sheet would set `punch` on every block, select the three-colour mode, and turn the sheet's bytes — and its transparent index-3 texels — into something new for no reason. This is the single sharpest implementation trap in the lane.

- BC3 512² with the same chain: 21,845 blocks × 16 = 349,520 + 128 = **349,648 bytes**, +174,760 per chunk (+99.9%).
- Worst case across the 3,060 dim-4..32 chunks of the Commonwealth: **+510.0 MiB** over today's 1.495 GiB of terrain sheets.
- BC3's alpha block is a 2-endpoint, 3-bit, 8-step interpolated palette *per 4×4 block* — strictly higher fidelity for a scalar than the 5:6:5 colour endpoints the RGB channels get. Cover is the best-carried channel in the sheet, not the worst.

**A chunk with no cover stays exactly as cheap as today.** The bake tracks `coverMax` while filling the plane. If `coverMax == 0` — no grass-bearing LTEX anywhere in the chunk, or every texel gated off by slope — the writer falls back to `bc3=false`, and the file is **byte-identical to today's**, 174,888 bytes, DXT1. The consumer distinguishes the two by the fourCC it already has to read: `DXT1` = no cover plane, `DXT5` = alpha *may* be cover, qualified by the provenance stamp below.

**The BC3 worst case is the expected case, and the spec says so.** 13,850 of 147,456 quadrants carry grass = 9.393%. A dim-4 `.btr` chunk holds 64 quadrants, so P(any grass) = 1 − (1 − 0.09393)^64 = **99.82%**; a dim-2 tile holds 16, P = **79.36%**; a dim-1 tile holds 4, P = **32.60%**. So "+510.0 MiB" is what the owner actually pays on the `.btr` sheets, not a corner. The reassurance "no size penalty on ocean, rubble, downtown" holds only where a bake unit touches *zero* grass, which clustering makes rarer than a per-quadrant rate suggests. The bake already tracks `coverMax`; the first full Commonwealth run reports the **measured** per-chunk and per-tile BC3 fraction into `WW_CHANGES.md`, replacing these binomial estimates. Measured relevance of the cheap path: 132,307 of 147,456 Commonwealth quadrants (89.7%) carry no paint whatsoever, and 65 of 128 LTEXes carry no GNAM.

**Provenance stamp — which law wrote this alpha.** A `_data.DDS` that is DXT5 for some other reason (an xLODGen sheet, a mod's sheet, or a build of this repo made from the stale "A = SLOPE" comment) would otherwise read as "alpha is cover", and a constant-255 alpha would decode as *full cover on every texel* — grass on rubble and ocean floor, the worst possible failure, and unrepairable later because the bytes contain nothing to repair. Two rules, both free:

1. The writer stamps the DDS header's unused `dwReserved1[11]` (file offsets **32..75**, i.e. `hdr[8]..hdr[18]` in `lodgenWriteDds`, all zero today): `hdr[8] = 0x5643_5757` (`'WWCV'`) and `hdr[9] = (coverLawVersion << 24) | round(COVER_FULL)`, with `coverLawVersion = 1`. Every other reader in this tree ignores those dwords.
2. **Reader rule:** a DXT5 `_data.DDS` whose `hdr[8] != 'WWCV'`, or whose decoded alpha is constant 255, carries **no cover** and must be treated as DXT1-equivalent.

### 2.6 Against the four rejected alpha candidates

The comment at `src/lodgen.cpp:4850-4861` rejects four candidates and states the two tests, restated in `docs/LODGEN_VERTEX_PACKING.md:272-273`: **is it derivable from what is already shipped**, and **does it have operands at its own resolution (512²)**. Ground cover is a fifth candidate and must be argued against all four, not appended to them.

| rejected candidate | its reason for failing | why cover does not fail the same way |
|---|---|---|
| **sky visibility** — "IS AO on a heightfield (r = 0.969)" | Redundant with R. `lodgenTerrainChannels` literally writes `skyVis[i] = vis` and `ao[i] = vis*255` from one horizon measure. | Cover is **not derivable from any channel shipped**. It needs `GNAM` and the GRAS `DATA` block — records nothing in this codebase has ever read (`grep GNAM src/esmdata.cpp` = 0 hits). It is orthogonal to R by construction: R comes from the heightfield, cover from the splat. The harness measures `|r|` between the cover plane and each of R/G/B on **two** chunks of different terrain character and requires **< 0.5** (C7) — the direct refuter of this exact failure mode. |
| **slope** — "recoverable as `acos(n.z)` from `_msn`" | Redundant with a sheet already shipped. | Cover *consumes* slope rather than storing it: `θ = acos(nrm.z)` from the same normal at `:4711` is an **input** to the gate. The redundancy argument that killed slope is exactly what makes the gate free — the operand is already computed in the same iteration of the same loop. But "consumes" is not "is not recoverable from", so C17 gates it directly: `|r|` between cover and both the `_msn` B channel and `θ` must be < 0.5, and a least-squares fit of cover against `θ` alone must leave **> 50%** of the variance unexplained. A field that is 90% explained by slope has not earned the slot. |
| **water depth** — "the water mesh already carries it, and land wants shore proximity, not depth" | Wrong consumer, and duplicated. | Cover has no other carrier. `LodtFile::groundCover(gx,gy)` exists (`src/lodtfile.h:142`, `src/lodtfile.cpp:1457-1462`) behind `SECT_GROUNDCOVER` (`:42`) — but `src.quadGcvr` is assigned **only on the Fallout 76 `.btd` import path** (`:1093`); the FO4 ESM writer says `g.clear(); // FO4 has no ground cover` (`:900`). Confirmed by measurement: Fallout4.esm contains **0 GCVR records**. FO4's ground cover is the LTEX→GNAM→GRAS chain and nothing has ever carried it. |
| **material blend** — "the diffuse ALREADY composites the layers, and the class ids it would weight are per-VERTEX, so a per-texel weight has no operands at its own resolution" | No operands at 512². | This is the one cover must answer hardest, because cover *is* a layer weight. The difference is measurable: the class ids are per-vertex (~1,180 vertices in a dim-4 chunk, ~480 units apart) whereas **`D(ltex)` is a per-FORM scalar, not a per-vertex one**, and the opacity it multiplies is the 17×17 grid the diffuse loop already bilinearly samples **per texel** at `:4661-4667`. Every operand of the cover formula exists at 512² inside the existing loop. And cover is a **smooth scalar field**, so it tolerates BC interpolation inside a 4×4 block — which is the precise reason the class *ids* were kept per-vertex (`src/lodgen.cpp:4776-4782`: "BC block compression interpolates inside each 4×4 block — which would synthesise class ids that do not exist"). Cover as a *type id* would fail that test; cover as a *fraction* does not. |

And the BC1-vs-BC3 half of that comment — "BC1 rather than BC3 until something actually needs the slot: 175 KB a chunk instead of 350" — is answered on its own terms: something now needs the slot, the price is exactly the 350 the comment predicted, and it is paid **only by chunks that have cover** (§2.5).

**Mandatory cleanup.** `src/lodgen.cpp:4833-4849` is a full paragraph beginning "A = SLOPE, as the angle from horizontal over 0..90 degrees" that argues *for* slope in alpha. It contradicts `:4850-4861` and it contradicts the code (`:4864` writes `0xFF`). It is stale and must be **deleted**, not appended to; the new comment states the cover law, its normalisation constant, the BC1/BC3 switch, the `'WWCV'` stamp and the four-candidate argument above in compressed form. Leaving two contradictory paragraphs stacked is how `docs/MISTAKES.md` describes half its entries starting.

Also stale and to be fixed in the same pass: `docs/LODGEN_VERTEX_PACKING.md` line 242 says the data map is "512^2 per chunk, BC3" and lines 265-267 argue "**BC3, not BC1.** Alpha is real payload here… ~350 KB a chunk against BC1's 175 KB", while line 250 says "A | — unused; the map is BC1" and line 281 says "So the map is **BC1 at 175 KB a chunk, not BC3 at 350**". The file on disk today settles it (DXT1, 174,888 bytes) — 242 and 265-267 were the stale pair. After this lane the honest text is: **BC1 with A unused when the chunk has no cover, BC3 with A = cover and a `'WWCV'` stamp when it has.**

### 2.7 New diagnostics

The bake prints a self-accusing line to stderr, extending the one at `src/lodgen.cpp:4721-4728`. Three changes to how it is printed, each closing a way the line could fail to accuse:

1. **The print condition is extended.** Today it is `if (statNoLand || statNoBase || statNoTex)` — the three OLD counters. Under `--cover` the line prints **unconditionally**, so a bake with 9,000 dangling GNAMs cannot go silent.
2. **One physical line, every counter a `key=value` token, no comma inside any value.** Form-id lists are space-separated inside `[]` and the whole token is `danglingLtexIds=[464C5 …]`. The spec's own rule (`MISTAKES.md:34-38`, live instance `lodgen_water_subdiv.sh:96-98` reading `$8/$10/$12`) is that report lines are parsed by keyword; a comma-separated line with a variable-width bracketed list inside it is the trap that rule exists for.
3. **Error counters and informational counters are separate**, because 65 of 128 LTEXes carry no GNAM *by design* and the C1 fixture itself names `LDriedGrass01NoGrass`, one of the 19 deliberate NoGrass records. Gating "LTEX with no GRAS" at zero would fail on every healthy run.

```
cover cx=-20 cy=24 dim=4 texels=262144 coverMax=143 \
  paintedPts=4445398 alphaLayerPts=3059576 renorm=62 \
  maxDtexPainted=80.0 maxDtexBase=0.0 clipPainted=0 clipBase=0 \
  danglingLtex=0 danglingLtexIds=[] danglingGnam=0 danglingGnamIds=[] \
  ltexNoGnam=42/128 grasNoTint=0/107 \
  grasReads=5 nifReads=5 texLoads=3 ltexResolves=8 \
  pxNoLand=0 pxNoBase=0 pxUnresolvableLtex=0
```

| class | counters | must be |
|---|---|---|
| **error** | `danglingLtex`, `danglingGnam`, `clipPainted`, `clipBase` | **0 on vanilla.** Measured: 1 dangling LTEX worldspace-wide, 0 dangling GNAM, 0 clips on painted texels; the base-quadrant clip count is unmeasured and is why `clipBase` exists as its own counter. |
| **informational** | `ltexNoGnam`, `grasNoTint` | printed **with a denominator**, never gated at 0 |
| **census** | `grasReads`, `nifReads`, `texLoads`, `ltexResolves`, `texels`, `coverMax`, the two point totals, `renorm` | gated as **counts** by C15 — this is what makes a per-texel ESM lookup fail deterministically instead of hiding inside a 24.5 s parse |

Measured on chunk (−20,24) today all three existing counters are zero. `clip*` counts texels where `gate · Dtex ≥ COVER_FULL` before the clamp; it is expected non-zero under True Grass, which raises grass links to as many as 14 on one LTEX.

---

## 3. The grass tint

### 3.1 Why the stock engine needs it

`_data.DDS` is an **orphan**: `grep '_data.DDS' src/` outside docs returns exactly one hit, the write at `src/lodgen.cpp:4868`. No texture slot names it (the Land shader fills slots 0 and 1 of a 10-slot set, `src/lodgen.cpp:978-979`), no manifest line names it, and nothing in FO4CS reads it either. So the cover plane is worth nothing to the stock engine, which is the target that ships today. The tint is the stock-engine half: it puts the cover into the one texture the engine definitely samples.

### 3.2 Where the tint colour comes from

Not from the GRAS record. `Colour Range` (0..0.3, mode 0.15) is a per-instance random tint **spread**, not a colour, and the LTEX's own diffuse is unreliable — 31 of 105 base-game LTEX TXSTs, including **17 of the 52 grass-bearing Commonwealth ones**, have an empty `TX00` slot. The colour lives in the grass mesh's own diffuse.

Resolution chain per GRAS, all through machinery that already exists:

1. `GRAS.MODL` → the mesh path. Measured: 95 distinct paths over 107 records; 83 resolve against `E:/Tools/Fallout 4/DataUnpacked/Data`, **71 distinct NIFs**; all 24 misses are DLC (the DLC BA2s are not unpacked in that corpus) and every base-game GRAS resolves.
2. Read the NIF with `lodgenReadAsset` (`src/lodgen.cpp:1611-1643`) — which has the `.nif`-specific fourth path at `:1626-1640` precisely because the FO4 archive filter drops meshes. Every one of the 71 is 20.2.0.7 / user 12 / BS 130, parses with full byte accounting (0 failures), and has **exactly one shape** (70 `BSTriShape` + 1 `BSMeshLODTriShape`), 4..290 triangles, median 24, file size 1,184..10,109 B.
3. From that shape's `BSLightingShaderProperty`, take the `BSShaderTextureSet` slot 0 (measured: 59 texture sets, all exactly 10 slots, only slots 0/1/7 ever used, **28 distinct diffuse textures**); when the shape names a material instead, hand the `.bgsm` path straight to `lodgenLoadTexture`, which already resolves a material to its first texture slot (`src/lodgen.cpp:3745-3761`). Measured: 39 distinct BGSM paths, most-shared `Materials\Landscape\Grass\ForestGrass01.BGSM` (6 meshes).
4. `avg(g)` = the texture's **smallest mip**, sampled `getPixelT(0.5, 0.5, tex->getMaxMipLevel())`. That mip *is* the average colour; the loader already computes the whole chain.
5. **Un-premultiply.** 39 of the 71 grass NIFs carry a `NiAlphaProperty`, so the smallest mip's RGB is the coverage-weighted average and is dragged toward the atlas's transparent gaps. If `avg.w ≥ 0.05`, use `avg.rgb / avg.w`; otherwise the grass contributes **no tint** (it still contributes its density to `D`) and `grasNoTint` increments.

Everything is cached per GRAS form, **for the whole run** (§4.2). Cost: 71 tiny NIF reads plus 28 texture loads, once per run, against an ESM parse measured at 24.5 s. C15 gates `nifReads ≤ 71` and `texLoads ≤ 28` over a whole-worldspace bake.

### 3.3 The formula

Per texel, using the same weights as `Dtex`, restricted to grasses that resolved a tint:

```
Ttex   = [ wBase·D(base)·T(base) + Σ_i a_i·D(ltex_i)·T(ltex_i) ] / Dtint
         where Dtint sums only the tint-bearing part of each D
w      = (cover / 255) · tintStrength           cover = the QUANTISED byte from §2.3
if ( w > 0.0f && Dtint > 0.0f )
    for k in 0..2:  color[k] = color[k] + ( Ttex[k] − color[k] ) · w
```

Placed in the paint loop **after the VCLR multiply** (`src/lodgen.cpp:4676-4692`) and **before the quantise-and-pack** at `:4694-4698`. After VCLR, because VCLR is the hand-painted ground modulation the landscape shader applies to the *ground*, and the grass sits on top of it; a tint mixed in before VCLR would get darkened by the artist's dirt shading. Mutation M5 in §8.3 proves the placement is load-bearing by moving it before the multiply and requiring a named check to fail.

`cover/255` uses the **quantised byte**, not the float, so a consumer that has the data sheet can reproduce the tint exactly from the alpha it reads. `tintStrength` default **0.35**.

**The tint is not invertible.** Recovering the untinted albedo from `c' = c + (Ttex − c)·w` needs per-texel `Ttex`, which is stored nowhere, and the albedo is quantised to 8 bits and BC-compressed afterwards. `terrain.cover.tintStrength` in the index records what was folded in so a consumer can **match** it — reproduce the same mix for geometry it draws itself — not undo it. §9 records the design fork this opens.

### 3.4 Proof that a no-cover texel is byte-identical to today

Three layers of guard, in increasing strength:

1. **Feature off** (`--no-cover`, the default). The GRAS parse never runs, `Ttex`/`cover` are never computed, the diffuse loop is the code that is there today, and `_data.DDS` is written BC1 with `A = 0xFF`. All three files are byte-identical to today's; C1 proves it against **frozen sha256 literals** measured before any source is touched, not against a bake of the build under test.
2. **Feature on, texel has no cover.** The tint is inside `if ( w > 0.0f && Dtint > 0.0f )`, so for `cover == 0` the branch is **not taken** and `color[k]` is the same float object it was before the block. No IEEE reasoning about `c + (t−c)·0` is needed — the identity is structural. The pack at `:4694-4698` then produces the same three bytes. (This is per-*texel*; BC1 quantises per 4×4 block, so C10 gates at block granularity, not texel granularity.)
3. **Feature on, chunk has no cover anywhere.** `coverMax == 0` ⟹ the alpha stays `0xFF` and the writer takes `bc3=false` ⟹ `_data.DDS` is the same 174,888 bytes as today, and the diffuse never took the branch on any texel, so **all three files are byte-identical** even with the feature on. C2 runs a cover-on bake over a chunk whose LTEXes carry no GNAM, requires `cmp -s` against the cover-off bake — **and requires the census line to prove the cover pass actually ran** (`texels=262144 coverMax=0`, a non-empty grass-free LTEX set printed), because a `cmp` that passes because the code never executed is the `MISTAKES.md:356-376` shape.

`tintStrength = 0` is a fourth, weaker case: the alpha plane is still written (BC3, so `_data.DDS` differs) but the diffuse is byte-identical to the `--no-cover` bake of the same chunk. That is the setting for an FO4CS-only user who wants the cover data and no stock-engine colour change.

### 3.5 Panel row and CLI flag

**Panel** — two rows added to the `Legacy terrain chunks (.btr)` section's form (`src/lodgenmanager.cpp:833-875`), immediately after the `Bake terrain textures` span at `:842`:

| control | object name | type | default | tooltip |
|---|---|---|---|---|
| `Ground cover and grass tint` | `LodgenCoverCheck` | `QCheckBox`, `f.span` | **unchecked** | "Reads the landscape textures' grass records. Puts a cover value in the terrain data sheet's alpha and mixes a grass colour into the far albedo." |
| `Grass tint strength` | `LodgenTintSpin` | `QSpinBox` 0..100, suffix ` %`, `f.add` | **35** | "How far the far albedo moves toward the grass colour where cover is full. 0 keeps the albedo exactly as it is and still writes the cover plane." |

Both join `btrSub` at `src/lodgenmanager.cpp:864-865` so they grey with the section, and the `sync` lambda at `:866-872` additionally greys `LodgenTintSpin` **and its label** unless `texCheck && coverCheck` are both ticked (the same shape as the existing `shoreDensitySpin`/`shoreLabel` pair). `LodgenTintSpin` **must** be appended to the `wwMakeScrubField` list at `src/lodgenmanager.cpp:1013-1015` — the self-test's `plain == 0` at `src/nifskope_ui.cpp:26919` fails the moment a `QAbstractSpinBox` is not passed through it. Both names go into the inventory array at `src/nifskope_ui.cpp:26821-26829`. Neither label may contain `" - "` (the dashed check at `:26954`).

Available on **both targets**. The tint is for the stock engine; the alpha plane is inert there and costs nothing when the chunk has no grass.

**CLI**:

| flag | default | meaning |
|---|---|---|
| `--cover` | — | bake ground cover and the grass tint |
| `--no-cover` | **active** | do not (byte-identical to today) |
| `--grass-tint <f>` | `0.35` | tint strength 0..1; 0 = albedo untouched, cover plane still written |
| `--cover-full <n>` | `96` | the cover normalisation constant; 1..65535 |
| `--dump-cover <path>` | — | also write the raw pre-compression 512² u8 cover plane, north-up, no header — the harness's independent handle on the value before BC3 touches it |

---

## 4. The tile pyramid

### 4.1 Levels

The pyramid is per worldspace. A **level** is named by its `dim` — cells per tile edge — matching the generator's own vocabulary. Measured for the Commonwealth (`lodgen --worldspace 3c`: `cells 36864  grid [-96,-96]..[95,95]`, and an ESM walk confirming **all 36,864 cells carry a LAND record**):

| level `dim` | tiles | world span per tile | world units per texel (content 256) | in default set | in full set |
|---|---|---|---|---|---|
| 1 | 192×192 = 36,864 | 4,096 | 16 | — | ✔ |
| **2** | 96×96 = **9,216** | 8,192 | **32** | ✔ (finest) | ✔ |
| 4 | 48×48 = 2,304 | 16,384 | 64 | ✔ | ✔ |
| 8 | 24×24 = 576 | 32,768 | 128 | ✔ | ✔ |
| 16 | 12×12 = 144 | 65,536 | 256 | ✔ | ✔ |
| 32 | 6×6 = 36 | 131,072 | 512 | ✔ (root) | ✔ |

Default set = **5 levels, 12,276 tiles**. Full set = **6 levels, 49,140 tiles**.

**The ladder is ×2 from the finest level up, and it is normative** — §5.1 stores it in the header as `levelDims[8]` so a consumer holding one container can name its siblings without the `.lodm`.

**Why the pyramid stops at dim 32 and has no single root tile.** Chunk origins are floored to a multiple of `dim` in absolute cell coordinates (`src/nifcli.cpp:2928`). −96 divides by 1/2/4/8/16/32 but **not 64** (−96/64 = −1.5): dim 64 gives 4×4 = 16 tiles from a mis-seated grid, and even dim 256 gives 2×2. There is no root, ever. `.lodt` already hit this and solved it the same way — it stops at level 3 and ships a separate uncompressed overview grid as its root (`docs/LODGEN_BTD_FORMAT.md`). The dim-32 level, 36 tiles and 4.76 MiB, is small enough to be permanently resident; that is the root.

**A worldspace that is not tile-aligned shortens the ladder; it is not refused.** The rule is the one §4.1 already reasons with: the pyramid stops at the coarsest dim that both `west` and `south` divide, and `levelCount` shrinks to match. A note is printed, not an error. Refusing outright would give the first non-Commonwealth worldspace someone tries a coin-flip chance of failing.

**Why finest dim 2, not dim 4.** The owner approved 256-texel tiles with finest dim 2 by default and dim 1 as the full option, and the arithmetic vindicates it: **256 texels over a 2-cell tile is 32 world units per texel — exactly the density of vanilla's finest ring**, measured over all 6,120 shipped Commonwealth terrain LOD files (every one 512×512 DXT5, 10 mips, 349,680 bytes; 2,304/576/144/36 chunks × 2 sheets at dims 4/8/16/32). Halving the tile and halving the dim together holds density constant and buys 4× the streaming granularity. Full mode's dim 1 is 16 units per texel, twice vanilla's linear density.

For scale, both ends: the near landscape's own diffuses are 2048² for 228 of 291 `_d.dds` under `textures/landscape` (78.4%), which under our `TILE = 2048.0f` assumption is ~1.0 texel per world unit; the **authored splat is 128 units per sample**. So even the full option's 16 units per texel is 8× coarser than the near ground and 8× finer than anything an artist painted.

**Full mode does not change the `.btr` sheets.** Dim 2 is **always** baked from the paint, in both modes; dim 1 in full mode is *also* baked from the paint, as a leaf hanging off the ladder, not as dim 2's parent. Filtering starts at dim 4 and always reads dim 2. If dim 2 were a box filter of dim 1 in full mode, §4.4's assembled chunk sheets would differ between the two modes and V9's bar would hold for only one of them.

### 4.2 Tile geometry, sheets and formats

| knob | value | why |
|---|---|---|
| content | **256** texels | Owner's approved outline; lands the default level on vanilla's exact density. |
| border | **8** texels per side | Multiple of 4, so a BC 4×4 block never straddles the content/border line — otherwise re-baking a neighbour changes *this* tile's blocks and incremental re-bake and byte-identity both die. Halves cleanly to 4 at mip 1. Costs `(272/256)² = ` **+12.891%**. B=16 would buy one more mip of aniso slack for **+26.563%**; not worth it at 2 stored mips. |
| stored side | **272** = 256 + 2×8, **68 blocks** | |
| mips | **2** (272 → 136) | The redundancy is measured: the four coarser levels together are **33.203%** of the finest level's bytes (3,060 tiles against 9,216), and a 2-mip tile's own tail is **25%** of its mip 0 (1,156 blocks against 4,624), so its full chain is **1.25×** mip 0. Level *L*'s mip 1 has the same texels-per-world-unit as level *L+1*'s mip 0 — a deep per-tile chain re-stores the whole pyramid. Mip 1 exists solely so a trilinear blend to the parent level never has to page the parent. Mip 2 would need border 16 to stay block-aligned. |
| aniso declared | **8** | `B ≥ ⌈A/2⌉` at the sampled mip: mip 0 has 8 ≥ 4 ✔, mip 1 has 4 ≥ 4 ✔. 16× would need 8 at mip 1, i.e. border 16. The container **declares** what it supports (§5) and the consumer clamps its sampler — a border sized for a setting the consumer may not use is 13.7 percentage points of disk given away. |
| sheets | **3** | `colour`, `msn`, `data` — exactly the three the `.btr` path writes today. |
| formats | colour **BC1** (DXGI 71), msn **BC1** (71), data **BC1** (71) or **BC3** (77) **per tile** per §5.1 | The pyramid must be able to *supply* the `.btr` sheets (§4.4), so its formats are the `.btr` formats. Vanilla uses DXT5 for both its sheets, but its alpha carries nothing — decoded with the harness's own `decodeAlpha`, vanilla's `Commonwealth.4.-20.24.DDS` and `_msn.DDS` both give **1 distinct alpha value, 255, mean 255.0**. Moving `_msn` to BC3 is a separate decision with its own evidence; it is not made here. |

**Tile byte arithmetic** (block counts: mip 0 = 68² = 4,624; mip 1 = 34² = 1,156; total 5,780):

- BC1 sheet: 5,780 × 8 = **46,240 B**
- BC3 sheet: 5,780 × 16 = **92,480 B**
- tile, all BC1: **138,720 B** — tile with a BC3 data sheet: **184,960 B**

**Set totals**, all-BC1 (no cover) and worst case (every tile's data sheet BC3):

| set | tiles | all BC1 | all BC3-data |
|---|---|---|---|
| default (dim 2..32) | 12,276 | 1,702,926,720 B = **1,624.0 MiB = 1.586 GiB** | 2,270,568,960 B = **2,165.3 MiB = 2.115 GiB** |
| full (dim 1..32) | 49,140 | 6,816,700,800 B = **6,500.8 MiB = 6.349 GiB** | 9,088,934,400 B = **8,667.8 MiB = 8.465 GiB** |

**Expected case, not worst case** (per-tile BC3 probability from §2.5, 79.36% at dim 2): the default set lands at **2.006 GiB** and the `.btr` sheets at **1.992 GiB**.

**Delivered totals — what is actually on disk when the lane is used.** §4.4 assembles the `.btr` sheets from the pyramid; it does not stop them being written. Both live on disk:

| configuration | `.btr` sheets | pyramid | **delivered total** |
|---|---|---|---|
| today (no VT, no cover) | 1.495 GiB | — | **1.495 GiB** |
| VT on, cover off, default | 1.495 GiB | 1.586 GiB | **3.081 GiB** |
| VT on, cover on, default (worst case) | 1.993 GiB | 2.115 GiB | **4.108 GiB** |
| VT on, cover on, default (expected) | 1.992 GiB | 2.006 GiB | **3.998 GiB** |
| VT on, cover on, full (worst case) | 1.993 GiB | 8.465 GiB | **10.458 GiB** |

The `.btr` alone under cover is 1.993 GiB against today's 1.495 GiB. Padding at 4,096 B adds **6.37 MiB (0.392%)** to an all-BC1 default set and **40.46 MiB (1.869%)** to an all-BC3-data one — see the alignment note below. Tile tables add 294,624 B in total.

The "−20% under vanilla" comparison is only fair with its terms stated: the default pyramid's 1.586 GiB carries **three** sheets, borders and an extra level, against vanilla's 1.993 GiB of **two** sheets with no borders. The delivered comparison the owner pays is 3.081 GiB against 1.495 GiB.

**Padding is deterministic, not statistical.** Uncompressed tile payloads are fixed size, so the pad is `(−138,720) mod 4,096 = 544 B` per BC1 tile and `(−184,960) mod 4,096 = 3,456 B` per BC3-data tile — not the 2,048 B average that a random payload size would give. Default all-BC1: 12,276 × 544 = 6,678,144 B = **6.369 MiB = 0.392%**. Default all-BC3-data: 12,276 × 3,456 = 42,425,856 B = **40.46 MiB = 1.869%**. With `compression = 1` the payloads are variable and 2,048 B/tile is the right expectation again.

**Bake time.** The extrapolation the earlier draft carried is withdrawn and replaced by a measurement protocol, because it cannot support the claim it was asked to carry: three warm samples with a 2.4× spread (0.094 / 0.191 / 0.225 s per 512²×3-sheet chunk), taken **with Fallout4.exe running**, against the project's own standing rule that nothing is measured while the game is up. Its own arithmetic did not close either — 249–581 s × 1.2 = 299–697 s = **5.0–11.6 min**, not the 6–14 that was printed, and full mode is **19.9–46.4 min**, not 22–52. Those corrected figures are recorded here only so the error is not re-derived; they are not the estimate.

The required measurement, game down, before any figure is quoted to the owner:

- **≥ 64 dim-4 chunks** and **≥ 64 dim-2 tiles**, cold (drop the page cache between runs), on this machine.
- Report **per-unit** and **per-texel** cost separately — a linear fit of wall time against unit count at two texel sizes. The texel-ratio extrapolation (a 272²×3-sheet tile is 221,952 texels against a chunk's 786,432, i.e. 0.2822×) is valid only if the per-unit fixed cost is zero, and §4.2's cache hoist exists precisely because it is not.
- Only then quote a full-worldspace figure, and put it in `WW_CHANGES.md` with the sample count.

**Caches are hoisted above the tile loop.** `src/lodgen.cpp:4585` declares `QHash<QString, DDSTexture16 *> texCache;` as a **local** of `lodgenBakeTerrainTextures`, and `:4726` does `for ( DDSTexture16 * t : texCache ) delete t;` — every landscape diffuse is loaded and BC-decoded once per bake unit and thrown away. The finest pyramid level is 9,216 dim-2 tiles against today's 2,304 dim-4 chunks: exactly **4.0×** the units over the same ground at the same 32 units/texel, each amortising its texture loads over **3.5×** fewer texels (73,984 against 262,144). At 228 of 291 landscape `_d.dds` being 2048², one decode is ~21.3 MiB of BGRA+mips.

So `lodgenBakeTerrainVt` owns **one** texture cache, **one** LTEX/GRAS/tint cache and **one** channel cache for the whole pass, and passes them down. The texture cache is an **LRU with a stated budget, default 512 MiB** (≈ 24 landscape textures resident) — unbounded it would hold ~60 distinct diffuses at 21.3 MiB = 1.25 GiB. `lodgenTerrainChannels` (`:4791`) has the same shape and the same treatment: 9,216 calls against 3,060 today unless its result is cached per cell-ring.

**Peak memory is bounded and asserted.** §4.3 defines the filter over a whole-worldspace mosaic; it is **evaluated in a band**, never materialised. Held whole, level 2's content mosaic is 96·256 = 24,576 square × 4 B × 3 sheets = **6.75 GiB**, full mode's level 1 is **27.0 GiB**, and the finest level's staging held whole is **7.62 GiB** (30.48 GiB in full mode). None of those is ever allocated. One tile's staging is 272² × 4 B × 3 sheets = **887,808 B**; the band is 3 finest tile rows = 3 × 96 × 887,808 = **243.8 MiB** at level 2 (487.7 MiB at level 1), plus one parent row under assembly at 96 × 887,808 = **81.3 MiB**. With the 512 MiB texture LRU, the resident `EsmWorld`, and encoder scratch, the stated peak is **≤ 2.0 GiB RSS**, and V23 asserts it.

### 4.3 The exact box filter

**Coarser levels are built from the finer level's uncompressed staging buffers, never from decoded BC blocks.** Decoding BC1 and re-encoding accumulates error at every level; the staging images are the bake's own 8-bit BGRA and cost nothing extra because the encoder needs them anyway.

Definitions. Let level *L* have `tilesX(L) × tilesY(L)` tiles, content `C = 256`, border `B = 8`, stored `S = 272`. Define level *L*'s **content mosaic** `F_L` — a virtual image of the whole worldspace at that level's density, `tilesX(L)·C` wide by `tilesY(L)·C` tall, assembled from the **content regions only** of every tile at that level (borders excluded; a border is a duplicate of a neighbour's content and including it would double-count at every seam). Coordinates are north-up: mosaic row 0 is the worldspace's north edge, matching the bake's own `row 0 = NORTH` at `src/lodgen.cpp:4611` and the heightmap's convention, **not** `.lodt`'s row-0-south.

> **Why not drop the mosaic framing.** The mosaic is the *definition*, and it is what makes a parent's border correct: a parent border texel falls outside its own four children's footprint and must come from a fifth, sixth or seventh child. Defining the filter over "four tiles" cannot express that. The mosaic is never materialised — the **traversal** below is normative and bounds the memory.

A parent tile `(tx, ty)` at level `2·dim` takes its stored texel `(i, j)`, with `i, j ∈ [0, 272)`, from:

```
u0 = 2 · ( tx·256 + i − 8 )
v0 = 2 · ( ty·256 + j − 8 )

P(i,j) = ( F(u0,   v0  ) + F(u0+1, v0  )
         + F(u0,   v0+1) + F(u0+1, v0+1) + 2 ) >> 2      per 8-bit channel, independently
```

with `F` clamped (edge replicate) outside `[0, tilesX·256) × [0, tilesY·256)`.

**Rounding: `+2 >> 2` (round-half-up) is the one law, and the existing writer is changed to match.** `lodgenWriteDds`'s mip chain at `src/lodgen.cpp:3663-3668` computes `acc[k] / 4` — truncation — for all four channels. Two rounding rules for one filter cannot both hold when §4.2 says "mip 1 inside a tile uses the same filter", and §4.4's assembled `.btr` mips would truncate while the pyramid rounds; the two differ by up to 1/255 per level, which is inside V8's bar and outside V9's `cmp`. **`lodgenWriteDds` is changed to `(acc[k] + 2) >> 2`** and the whole-corpus identity gate (`lodgen_identity.sh`) is re-baselined in the same commit, with the changed hashes recorded in `WW_CHANGES.md`. State the rule in the code comment, because the harness compares against an independent Python implementation of exactly this line, re-typed rather than imported.

**Traversal (normative, and it fixes the payload order).** For each parent tile row `ty` at level `2·dim`, in increasing `ty`:

1. ensure finer tile rows `2·ty − 1`, `2·ty`, `2·ty + 1` are staged (a 3-row ring; row `2·ty − 1` is already staged from the previous parent row, so one new row is baked or read per step, and row `2·ty + 2` is staged when it becomes needed);
2. assemble every parent tile in row `ty`, left to right (increasing `tx`);
3. encode, optionally deflate, and **append** each parent tile to its container in that order;
4. release finer row `2·ty − 1`.

Tiles are therefore written in **table-index order** (`index = ty·tilesX + tx`), which is what makes the file byte-deterministic. The finest level is baked in the same raster order directly from the paint.

Two special rules:

1. **The `msn` sheet is renormalised after the average.** Decode each averaged channel as `n = c/255·2 − 1`, normalise the 3-vector, re-encode as `round((n·0.5 + 0.5)·255)`. A box average of two opposite slopes produces a short vector whose decoded tilt magnitude is wrong; the msn is the one sheet where the channels are not independent.
2. **The `data` sheet's alpha (cover) averages plainly** — cover is linear in `Dtex` (§2.3), so the mean of four cover bytes is the cover of the union at half the resolution. **A tile with no cover contributes alpha 0 to the filter.** The staging alpha for a no-cover tile is **0, never `0xFF`** — the `0xFF` of §2.5 is applied only at DDS pack time on the BC1 fallback path and never enters the filter. Without that rule a parent bordering one grassy child would inherit 255 = full cover across three quadrants of bare rock. A parent tile's data sheet is BC3 iff its averaged alpha is not everywhere zero, which is equivalent to "iff any of its four children's is".

**Coarse levels are downsamples of fine data, not measurements at that scale — and that is a documented limitation, not a bug.** Three of the four data channels are scale-dependent:

- **AO** is a fixed 2,048-unit horizon march (`for (dist = 128; dist <= 2048; dist *= 1.5)`, `src/lodgen.cpp:4823`, 7 steps × 8 directions = 56 samples/texel). A box filter of 32-unit-texel AO is not the AO of a 512-unit texel, whose horizon should be marched an order of magnitude farther. `mean(AO) ≠ AO(mean)`.
- **Shore proximity** is a distance field; box-filtering a distance field is not the distance field at half resolution, and it fails worst near the zero crossing, which is the only place it is read.
- **The msn** is renormalised, which fixes the magnitude, but the mean of fine normals is still not the normal of the coarse heightfield.

Only **cover** and **albedo** filter cleanly. `docs/LODGEN_TERRAIN_VT.md` states this in those words, so a consumer never treats level 16's R as an AO term measured at 256 units per texel. V8b measures the divergence directly (a filtered dim-8 tile against a **direct** dim-8 bake of the same ground) and V9c measures it at dim 32, four halvings down, where it is largest.

**Mip 1 inside a tile** uses the same filter with the same rounding, over the tile's own stored 272² staging (272 → 136), which is why the border must halve to an integer multiple of 4.

**Heightfield working set.** Each tile's bake loads the LAND of its own cells **plus a one-cell (4,096-unit) ring**. Two independent reasons, both measured: the 8-texel border reaches 8 × 32 = **256 units** past the content at the finest level, and the AO horizon march runs 2,048 units in 8 directions. Today's per-chunk bake clamps `heightAt` at the chunk edge (`:4759-4760`) and clamps the msn's central differences at `qBound(1, int(gx), hn-2)` (`:4709-4710`), so **both its AO and its outer-ring normals are wrong within the clamp distance of every chunk boundary**; the pyramid fixes both as a side effect. §4.4 and V9 bound the resulting difference per channel rather than pretending it is zero.

### 4.4 Taking the `.btr` chunk textures from the pyramid

When the VT toggle is on **and** `Bake terrain textures` / `--tex-dir` is on, the per-chunk sheets are no longer baked directly. A `.btr` chunk at `dim = D` is assembled from the **2×2 pyramid tiles at level `D/2`**, content regions only, borders cropped:

```
chunk (cx, cy) at dim D  ←  tiles ( cx/(D/2) + {0,1},  cy/(D/2) + {0,1} ) at level D/2
sheet = 512 × 512, the four 256×256 content blocks laid out north-up
```

D=4 → level 2 · D=8 → level 4 · D=16 → level 8 · D=32 → level 16. Level 32 feeds no chunk (there is no dim-64 chunk) and exists only as the resident root. In full mode the extra dim-1 level is read by nothing in the `.btr` path — the rule is always `D/2`.

**The pyramid replaces the dim-8/16/32 paint bakes rather than adding to them.** Once the VT pass has run, those chunk sheets are assemblies, not bakes; that is a real time credit against today's 3,060-chunk terrain pass, and the §4.2 measurement must report it.

**The assembled sheet is 512² at exactly today's density, and the sampling grid is identical.** Today, dim 4: texel centre `px` sits at `cwX + (px + 0.5)·32`, since `span/RES = 16384/512 = 32`. Pyramid dim 2: tile span 8,192, content 256, texel 32; the composite index `px = tileIndex·256 + p` gives centre `cwX + px·32 + 16 = cwX + (px + 0.5)·32`. **The same world points.** The mip-choice arithmetic is unchanged too: `footprint = span/RES` is 32 in both cases (16384/512 = 8192/256), so `lodgenLoadTexture`'s mip pick at `:4647-4652` is identical, and the AO march's `texel = dim*4096/RES` is 32 in both.

**`dominantBase` scope is load-bearing.** It is computed per bake unit over that unit's cells (`:4596-4607`) and it is what NULL-LTEX layers (3,227 of 55,679 Commonwealth alpha layers, 5.8%) and `baseTex == 0` texels paint. A tile baker scoped to 2×2 cells would compute a different `dominantBase` and paint those texels a different colour. **The tile baker therefore computes `dominantBase` over the same cell set the dim-4 chunk baker would** — the enclosing dim-4 chunk's 16 cells — and carries it into the four dim-2 tiles that compose that chunk. V9a gates the alternative: if the implementer scopes it differently, every differing texel must be one the model marks NULL-LTEX or `baseTex == 0`, with the count printed.

**The mixed-cover assembly rule.** A 2×2 group can mix cover-bearing and cover-free tiles. The assembled sheet's `coverMax` is taken over the assembled 512² alpha, exactly as the direct bake takes it over its own plane, so §2.5's BC1/BC3 switch has one definition on both paths. Content contributed by a cover-free tile is alpha **0** (the staging convention of §4.3), and the `'WWCV'` stamp is written whenever the assembled sheet is BC3.

**The assembled sheet's mip chain is built from the assembled 512² staging, not from the tiles' mips.** The pyramid supplies 2 mips of a 272-texel tile, whose texels are border-contaminated and whose 136 is not a submultiple of the 512 chain; re-decoding BC to get more is forbidden by §4.3's first sentence. So `lodgenVtChunkSheets` assembles the four content blocks into a 512² BGRA staging image and hands **that** to `lodgenWriteDds`, which builds the full 8-mip chain exactly as it does today. The tiles' own mip 1 is not read on this path. V9d gates every mip level, not just mip 0, because a mip-chain regression on every chunk ships as LOD shimmer nobody can attribute.

**Where it differs from today, and by how much.** Only where today's clamps were wrong: AO within the 2,048-unit march radius of a **chunk** boundary, and the msn within one heightfield sample of a **chunk** boundary. The bars are per channel and per region (V9), and the AO locality bar is stated against the **chunk's outer boundary** — not against an interior tile seam, where a ringed tile bake and a chunk bake both have full data. (Bounding it at "within 64 texels of a dim-2 tile boundary" would have admitted 75% of the sheet by union: boundaries at 0/256/512 put 50% of x and 50% of y inside 64 texels.)

The `.btr` file itself is untouched: the Land shader still names `<ws>.<dim>.<x>.<y>.DDS` and `_msn.DDS` (`src/lodgen.cpp:974-979`), still Shader Type 18, still flags 2151682048 / 3, still `Num Textures 10` with slots 2..9 empty, still `UV = (x/4096, 1 − y/4096)` in miniature chunk space (`:933-934`). No UV remap, no `UV Offset`/`UV Scale`. That is the whole point of assembling the sheet rather than pointing the mesh at a tile.

---

## 5. The container file — `.lodv` v1

**Name:** `Data\Terrain\<EDID>.VT.<dim>.lodv`, one per level. Example set for the Commonwealth default:
`Commonwealth.VT.2.lodv`, `.4.`, `.8.`, `.16.`, `.32.lodv`.

**Why that folder, that extension, one file per level.** `Data\Textures\Terrain\<WS>\` is enumerated by FO4CS with `FindFirstFileA(<dir>*.dds)` and the collection loop **stops at 4,096 names** (`FarFieldHeightmapRuntime.cpp:104-118`) — 9,216 tile files there would truncate the enumeration and could hide the heightmap. `Data\Terrain\` is read by exact name (`<WS>.lodt`) and is where the `.lodt` precedent already lives; it is occupied in his install by exactly 5 `.lodt` files under `E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain`, so there is no collision. One container per level is the owner's approved outline and keeps the coarse root — 36 tiles, 4.76 MiB — loadable on its own without opening a 1.2 GiB file.

**Endianness: little-endian throughout**, matching `.lodt`, DDS, and the F4FX provenance block. **All offsets are absolute file offsets.** All sizes are in bytes. **A reader computes `24·tileCount` and `tileTableOffset + 24·tileCount` in 64-bit**: `tileCount` is u32 and `tilesX·tilesY` can reach 4,294,836,225, so the product with 24 overflows u32 even though the count itself does not.

### 5.1 Header — 256 bytes at offset 0

The header is **256 bytes**, with 0xC0..0xFF reserved-must-be-zero, so the fields this format will predictably want next do not each cost a v2 and a 12,276-tile re-bake. A v1 reader ignores a zero-filled tail. `COVER_FULL`, the tint strength and the level ladder are **in the header**, not only in the `.lodm`, because §5's whole justification for per-level containers is that the 36-tile root is loadable on its own — and a tile's alpha byte is meaningless without its normalisation constant.

| off | type | name | meaning |
|---|---|---|---|
| 0x00 | char[4] | `magic` | `'L','O','D','V'` (4C 4F 44 56). Deliberately neither `DDS ` nor `LODT`: `docs/LODGEN_BTD_FORMAT.md:64-72`'s rule — a wrong-but-plausible parse is worse than a refusal. |
| 0x04 | u32 | `version` | 1 |
| 0x08 | u32 | `headerBytes` | 256. A v2 header may grow; a v1 reader refuses a version it does not know rather than reading 256 bytes of something else. |
| 0x0C | u32 | `flags` | bit 0 `ROW_ORDER_NORTH_UP` (**1**, and bit 0 clear is a refusal — rule 19); bit 1 `FULL_MODE` (the set's finest level is dim 1); bits 2..31 zero |
| 0x10 | u64 | `fileBytes` | total size of this file. A truncated download is a refusal, the way `.lodt` refuses at its own header offset 0x90. |
| 0x18 | u64 | `tileTableOffset` | ≥ 256, 8-aligned |
| 0x20 | u64 | `payloadOffset` | ≥ `tileTableOffset + 24·tileCount`, 4096-aligned |
| 0x28 | u64 | `vhgtCorpusHash` | FNV-1a 64 over every LAND's raw VHGT payload of this worldspace **in ascending cell order (y then x), not file order** — see the note below. Commonwealth = `0xD8337D022F637F22`. **Pins heights only.** |
| 0x30 | u64 | `paintCorpusHash` | FNV-1a 64 over the inputs the cover plane and the albedo actually read: every LAND's raw `BTXT`/`ATXT`/`VTXT` payloads, then every referenced LTEX's `TNAM` and `GNAM` list, then every referenced GRAS's `DATA` and `MODL`, all in ascending form-id order. This is the hash that catches True Grass, a retextured splat, or 26 overridden LTEXes. |
| 0x38 | char[32] | `worldspaceEdid` | ASCII, NUL-terminated, NUL-padded. `"Commonwealth"`. A 32-character EDID is **refused by the writer**, never truncated — a truncated EDID silently matches the wrong worldspace. Comparison against the `.lodm` is **case-sensitive**. |
| 0x58 | i16×4 | `south, west, north, east` | inclusive cell bounds of the **tile-aligned rectangle this level covers** (padded out to a whole number of tiles) |
| 0x60 | i16×4 | `worldSouth, worldWest, worldNorth, worldEast` | inclusive cell bounds of the **actual worldspace**, identical in every container of the set |
| 0x68 | u16 | `levelDim` | cells per tile edge at THIS level: 1, 2, 4, 8, 16 or 32 |
| 0x6A | u16 | `levelIndex` | 0 = the finest level of the set; must equal this level's position in `levelDims[]` |
| 0x6C | u16 | `levelCount` | how many levels the set has (5 default, 6 full) |
| 0x6E | u16 | `tilesX` | `(east − west + 1) / levelDim`, an **exact** divide |
| 0x70 | u16 | `tilesY` | `(north − south + 1) / levelDim`, an **exact** divide |
| 0x72 | u16 | `contentTexels` | 256 |
| 0x74 | u16 | `borderTexels` | 8 |
| 0x76 | u16 | `storedTexels` | 272 |
| 0x78 | u8 | `mipCount` | 2 |
| 0x79 | u8 | `sheetCount` | 3 |
| 0x7A | u8 | `anisoSupported` | 8 — the maximum anisotropy the border covers at every stored mip |
| 0x7B | u8 | `compression` | 0 = stored raw, 1 = zlib (RFC 1950 wrapper). Any other value is a refusal, so a future zstd is a named error and never a misparse. |
| 0x7C | u32 | `tileCount` | `tilesX · tilesY` |
| 0x80 | f32 | `coverNormalisation` | `COVER_FULL`, 96.0 by default. Meaningless-but-written (96.0) when no tile carries cover. |
| 0x84 | f32 | `tintStrength` | what was folded into the albedo, 0.35 by default |
| 0x88 | u16[8] | `levelDims` | the set's ladder, finest first, zero-padded: `{2,4,8,16,32,0,0,0}` by default. Makes a container able to name its siblings without the `.lodm`. |
| 0x98 | u32 | `indexCrc32` | CRC-32 (zlib polynomial 0xEDB88320) over the header's 256 bytes **with this field zeroed**, followed by the tile table's `24·tileCount` bytes. Closes the aliasing hole that per-payload CRCs cannot: a flipped bit in an `offset` points the reader at another tile's payload, whose own CRC is valid, and it loads the wrong tile and never notices. |
| 0x9C | u32 | `reserved0` | 0 |
| 0xA0 | ×4 | `sheets[4]` | 8 bytes each: **u16 `dxgiFormat`**, **u16 `dxgiFormatCover`**, **u8 `role`** (0 unused, 1 colour, 2 model-space normal, 3 data), **u8 `colorSpace`** (0 linear, 1 sRGB), **u8[2] `reserved`** = 0. Sheets beyond `sheetCount` are all zero. |
| 0xC0 | u8[64] | `reserved` | must be zero; a v1 reader ignores a zero-filled tail |
| | | | header ends at 0x100 = 256 |

**The role-3 sheet's format is per tile.** `sheets[k].dxgiFormat` is the format when that tile's `COVER` bit is **clear**; `sheets[k].dxgiFormatCover` is the format when it is **set**. For roles 1 and 2, `dxgiFormatCover` must equal `dxgiFormat`. For role 3 the default set is `dxgiFormat = 71` (BC1) and `dxgiFormatCover = 77` (BC3). Without this rule, reader rule 16 — "`rawBytes` must equal the size computed from the header and the entry's `COVER` bit" — is unimplementable, and a consumer that sized an upload from a single per-file format would mis-size every cover tile by 46,240 bytes.

**Colour space is declared, and both UNORM and UNORM_SRGB are legal.** `dxgiFormat` and `dxgiFormatCover` accept **{71 BC1_UNORM, 72 BC1_UNORM_SRGB, 77 BC3_UNORM, 78 BC3_UNORM_SRGB}**, and `colorSpace` says which the sheet's *content* is authored in. The `.btr` sheets are sampled as sRGB by the engine today; a VT consumer that samples 71 as linear gets visibly washed-out terrain, and nothing else in the file would tell it otherwise. This lane writes `dxgiFormat = 71` with `colorSpace = 1` for the colour sheet (matching the `.btr` bytes exactly), `colorSpace = 0` for msn and data.

**The world rectangle is derived, never stored twice** — but the *padded* and *unpadded* rectangles are genuinely different facts, so both are stored. The padded one differs per level (dim 32 needs a different pad from dim 2); the unpadded one is set-wide and is what `terrain.extent` in the `.lodm` carries. Derived world units are `[west·4096, (east+1)·4096] × [south·4096, (north+1)·4096]`, the identical arithmetic the FO4CS heightmap loader uses.

**Padding rule.** `west ≤ worldWest`, `south ≤ worldSouth`, `east ≥ worldEast`, `north ≥ worldNorth`, with `west ≡ 0 (mod levelDim)`, `south ≡ 0 (mod levelDim)` and both spans exact multiples of `levelDim`; the pad is the minimum that satisfies all of it. Cells outside the worldspace are baked as absent-neighbour edge replicate and their tiles are still `PRESENT` (they contain real, filtered edge data). Without the padded/unpadded split, `tilesX = (east − west + 1) / levelDim` is a **truncating** divide that reader rule 9 would validate against the same truncating divide: a worldspace whose span is not a multiple of `levelDim` would silently drop its east and north edge cells while passing every check. The Commonwealth is lucky (192 is divisible by 1/2/4/8/16/32/64; only the −96 corner fails at 64) — Far Harbor, Nuka-World and every mod worldspace are a coin flip.

**Two hashes, because one validates the wrong inputs.** `vhgtCorpusHash` is FNV-1a over LAND VHGT payloads only. The cover plane and the tint come from LTEX → GNAM → GRAS and from the ATXT/VTXT paint, none of which VHGT covers — so a plugin that adds True Grass or overrides 26 LTEXes leaves it unchanged and a stale bake would pass silently, which is the one failure mode the rule exists to catch. Hence `paintCorpusHash`. And the ordering is **ascending cell order, not file order**: hashing in file order makes a load-order permutation that changes nothing effective produce a different hash and hard-refuse a perfectly good 1.6 GiB bake. `EsmWorld::vhgtCorpusHash()` (`src/esmdata.h:141`) is changed to sort, and the F4FX heightmap block's copy is re-baselined in the same commit — the two must keep agreeing, and V19 checks that they do.

**Deliberately absent from the header and from the table:** per-tile world rectangles (derive them), per-tile min/max height (that is the heightmap's and the `.lodt`'s business, and a second source of truth for terrain height is a bug generator), LTEX form ids, material names, source paths, any per-tile string, and per-mip offsets (a tile's mips are contiguous inside its own payload so one offset addresses the whole tile).

### 5.2 Tile table — fixed stride 24 bytes, `tileCount` entries, at `tileTableOffset`

Row-major, `index = ty · tilesX + tx`, with **`ty = 0` the NORTH row** of the padded rectangle and `tx = 0` its west column.

**Tile → cells** (the cheapest line in the spec and the one most likely to be got wrong in a year, because north-up combined with inclusive north/south is exactly where an off-by-one lives):

```
cells x ∈ [ west  + tx·levelDim ,  west  + (tx+1)·levelDim − 1 ]
cells y ∈ [ north − (ty+1)·levelDim + 1 ,  north − ty·levelDim ]
```

| off | type | name | meaning |
|---|---|---|---|
| 0x00 | u64 | `offset` | absolute file offset of this tile's payload; **0 exactly when the tile is absent** |
| 0x08 | u32 | `storedBytes` | bytes on disk (after compression when `compression != 0`) |
| 0x0C | u32 | `rawBytes` | bytes after inflate; must equal the size computed from the header + this entry's COVER bit |
| 0x10 | u32 | `crc32` | CRC-32 (zlib polynomial 0xEDB88320) over the `storedBytes` on disk |
| 0x14 | u16 | `flags` | bit 0 `PRESENT`; bit 1 `COVER` (this tile's data sheet uses `sheets[2].dxgiFormatCover`, with cover in alpha); bits 2..15 zero |
| 0x16 | u16 | `reserved` | 0 |

**An absent tile's 24 bytes are all zero** — not just `offset`. Rule 16 validates the other five fields only when `PRESENT` is set, so without this a reader summing `storedBytes` over the table sums garbage.

An **explicit present flag** rather than "offset 0 means absent alone", because "offset 0" is exactly the sentinel that gets misread. The Commonwealth is fully dense (all 36,864 cells carry LAND, measured) so every tile is present there — but Far Harbor, Nuka-World and mod worldspaces are unmeasured, which is the reason the flag exists.

Table sizes: dim 2 = 221,184 B; dim 4 = 55,296; dim 8 = 13,824; dim 16 = 3,456; dim 32 = 864. **294,624 B for the whole default set.**

### 5.3 Payload

Each present tile's payload begins at a **4,096-byte-aligned** absolute offset ≥ `payloadOffset`, and tiles appear in the file in **table-index order**. **Every alignment pad byte is zero.** Those two rules plus the fixed traversal of §4.3 are what make two `--vt` runs byte-identical (V22) — which the repo's own identity culture demands and which the spec treats as load-bearing everywhere else.

**Row order inside a payload is north-up, west-first.** Block row 0 is the tile's **north** edge (including its north border), block column 0 its **west** edge, matching the `.btr` bake's own explicit flip (`aoTex[(RES-1-j)*RES + i]`) and the mosaic convention of §4.3 — and *not* `.lodt`'s row-0-south. Without this sentence the first FO4CS consumer has a 50% chance of a mirrored world and no way to tell from the file; the mismatch already cost FO4CS one Y mirror (`FarFieldLodtFormat.h:28-35`).

Raw payload for one tile is the concatenation, in this exact order, of:

```
sheet 0 mip 0, sheet 0 mip 1, sheet 1 mip 0, sheet 1 mip 1, sheet 2 mip 0, sheet 2 mip 1
```

Rows **tightly packed** at `blocksX · blockBytes` — 68·8 = 544 B/row at mip 0 BC1, 34·8 = 272 at mip 1; 68·16 = 1,088 and 34·16 = 544 for a BC3 data sheet. No row padding, no 256-byte alignment: FO4CS uploads through D3D11 `CreateTexture2D` and `UpdateSubresource` with a **caller-supplied `SysMemPitch`** (measured at `FarFieldHeightmapRuntime.cpp:1130-1135, :1418, :1639-1644`), which accepts any declared row pitch. The 256-byte-row / 512-byte-placement rules people quote are D3D12 upload-heap rules and FO4CS's D3D12 device (`src/D3D12Context.h`, 72 lines) exists only for frame-generation presenters.

Computed raw size: `Σ_sheets Σ_mips ((storedTexels >> mip)/4)² × blockBytes` = **138,720 B** all-BC1, **184,960 B** with a BC3 data sheet.

**Compression.** `compression = 0` (raw) is the **default**, and `--vt-compress zlib` turns it on. The case for deflate is weak against its costs: BC block data typically compresses 5–15%, against alignment padding already at 0.392%, a mandatory inflate on every tile read at runtime, and bake time nobody budgeted — 1.70 GB through zlib-6 at ~40 MB/s is ~45 s, and full mode's 6.82 GB is ~3 min, more at level 9. Whichever is chosen, the flag prints its cost into the pre-flight estimate.

When `compression == 1`, **every present tile is deflated as one whole-payload zlib stream** — one seek and one inflate, because a resident tile always needs every sheet at once, and a mixed raw/deflated file would make the reader guess. Writer constraints, because FO4CS's only decompressor is hand-written and header-only (`src/FarField/FarFieldInflate.h`, `InflateRaw` / `InflateZlib`) and it links no compression library: **CM = 8, CINFO ≤ 7, FDICT clear**, and `(CMF << 8 | FLG) % 31 == 0`. `storedBytes > rawBytes` is **legal** — zlib stored blocks add ~5 bytes per 32 KiB and incompressible BC data hits that. Reader rule 16b enforces the zlib header directly, because "every tile is deflated" is otherwise an invariant nothing checks.

The payload must **not** be progressive. Unlike `.lodt`'s height blocks, a parent's 4×4 BC block is not a subset of a child's texels, so "store only what the parent lacks" has no meaning for block-compressed colour, and the whole-tile upload is the operation the consumer actually performs.

**The writer streams; it never holds a level.** Naively buffering a level to fill in the table is **1.19 GiB** at dim 2 and **4.76 GiB** at dim 1 — the same 1.19 GiB figure §9 quotes to justify 64-bit offsets. Instead: reserve `tileTableOffset .. +24·tileCount`, write payloads at increasing 4,096-aligned offsets in table-index order keeping only the 24-byte row per tile in RAM (**221,184 B** at dim 2, 884,736 B at dim 1), then seek back and patch the table, `fileBytes` and `indexCrc32`. This is normative, and it is in the §10 work list.

### 5.4 Reader validation rules

Refuse — by name, with the field that failed — on any of:

1. file smaller than 256 bytes
2. `magic != 'LODV'`
3. `version != 1`
4. `headerBytes != 256`
5. `fileBytes` != the actual file size
6. `worldspaceEdid` not NUL-terminated inside its 32 bytes, empty, or containing a byte outside 0x20..0x7E
7. `north < south` or `east < west`; or `worldNorth < worldSouth` or `worldEast < worldWest`; or the padded rectangle does not contain the world rectangle
8. `levelDim ∉ {1,2,4,8,16,32}`, or `west % levelDim != 0`, or `south % levelDim != 0` (measured: −96 satisfies this for every listed dim and fails for 64 — the reason the ladder stops at 32)
9. `(east−west+1) % levelDim != 0` or `(north−south+1) % levelDim != 0`; or `tilesX != (east−west+1)/levelDim` or `tilesY != (north−south+1)/levelDim`, or either < 1
10. `tileCount != tilesX·tilesY` (computed in 64-bit)
11. `contentTexels` not a power of two in 128..1024; `borderTexels % 4 != 0`; `storedTexels != content + 2·border`
12. `mipCount < 1`, or `(borderTexels >> (mipCount−1)) % 4 != 0`, or `(borderTexels >> (mipCount−1)) << (mipCount−1) != borderTexels`, or `(contentTexels >> (mipCount−1)) < 4` — **a multiple of 4 at the coarsest stored mip, not merely ≥ 4 and even.** `--vt-border 12 --vt-mips 2` gives a mip-1 border of 6: the content/border boundary falls mid-block at mip 1, a tile's blocks depend on its neighbour's bake, and incremental re-bake and byte-identity both die. §7.3's CLI string catches it for our writer; this rule catches it for anyone else's.
13. `sheetCount` outside 1..4; any used sheet with `dxgiFormat ∉ {71,72,77,78}` or `dxgiFormatCover ∉ {71,72,77,78}`, `role == 0`, a duplicated role, `colorSpace > 1`, or `dxgiFormatCover != dxgiFormat` on a sheet whose role is not 3
14. `compression ∉ {0,1}`
15. `tileTableOffset < 256` or not 8-aligned; `tileTableOffset + 24·tileCount > payloadOffset` (64-bit); `payloadOffset > fileBytes` or not 4096-aligned
16. any entry where `PRESENT` disagrees with `offset != 0`; any absent entry whose 24 bytes are not all zero; or, if present: `offset < payloadOffset`, `offset % 4096 != 0`, `offset + storedBytes > fileBytes`, `storedBytes == 0`, `rawBytes` != the size computed from the header and the entry's `COVER` bit (using `sheets[2].dxgiFormatCover` when set), or `compression == 0 && storedBytes != rawBytes`
16b. `compression == 1` and any present tile whose first two bytes are not a valid zlib header with CM = 8, CINFO ≤ 7, FDICT clear and `(b0<<8|b1) % 31 == 0`
17. every tile is present-flagged 0 (a level with no tiles is a broken bake, not an empty world)
18. **`vhgtCorpusHash` or `paintCorpusHash` != the consumer's own hash** of the worldspace it is loading — the same refusal the heightmap loader already makes, so a VT baked against an edited plugin is a named error instead of a silent disagreement with the heightmap
19. `flags` bit 0 (`ROW_ORDER_NORTH_UP`) clear — no other row order is defined, so a reader that tolerated a clear bit would have to implement a south-up path with no spec
20. `indexCrc32` != the recomputed CRC over the zeroed-field header plus the tile table
21. `anisoSupported > 2 · (borderTexels >> (mipCount−1))` — the `B ≥ ⌈A/2⌉` relation §4.2 derives, so the declaration is checkable rather than an unvalidatable promise
22. `levelDims[levelIndex] != levelDim`; `levelDims` not strictly ascending in its non-zero prefix; the non-zero prefix length != `levelCount`; `levelIndex ≥ levelCount`

`crc32` is checked **per tile at load time**, not at open — checking 12,276 CRCs to open a file would cost the whole point of the index. `indexCrc32` **is** checked at open: it covers 294,624 B at most and it is what stops offset aliasing.

> **Why not make the reader's `contentTexels` range match the writer's.** Rule 11 accepts 128..1024; `--vt-content` refuses above 512. The asymmetry is deliberate and documented in both places: the writer's ceiling is a **cost guard** (`--vt-content 512 --vt-finest 1` is 36,864 tiles × 522,720 B = **17.946 GiB**), not a format limit, and a reader that refused a well-formed 1024 container written by a future tool would be wrong. Liberal reader, conservative writer.

---

## 6. The index — a `terrainVT` `.lodm`

**Path:** `Data\Terrain\<EDID>.VT.lodm` — deliberately **not** under `materials\`, so `lodmSourceCandidate()` (`src/io/lodmfile.cpp:84-106`) can never produce it and the two readers that ignore `kind` (`src/lodgen.cpp:4026-4036`, `src/nifskope_ui.cpp:21550-21562`) can never open it. Verified: that function unconditionally prepends `materials\` for a diffuse and strips only a leading `data\` for a material, so `Terrain\…` is unreachable — and V14 tests the property rather than trusting the reasoning.

**No parser change is needed.** The envelope stays version 1 (`src/io/lodmfile.cpp:8-10`), `lodm` stays 1, and `family` is `"legacy"` — the parser **hard-rejects** any third family (`:50-53`), so a new family word would be a version bump that splits the corpus. `kind` is read at `:56` as a free string with default `"source"` and is never validated; the only reader that keys on it is the card-array pass (`src/lodgen.cpp:5652-5656`), which skips an unknown kind cleanly. `textures` is absent — the parser tolerates it and leaves `color`/`normal`/`mask` empty. Unknown top-level keys are ignored, so `terrain` is a legal sibling of `card` and `array`.

`family: "legacy"` on a tile pyramid is a convenience: `family` carries the legacy/PBR **material** split and a pyramid is neither. It parses today, which is the whole argument, but it spends a discriminator someone will want the day a PBR terrain VT exists. So `src/io/lodmfile.h` documents in words that **`family` is vestigial for `kind: "terrainVT"` and the real discriminator is `kind`**, which is where the collision gets resolved rather than discovered.

**The per-tile table is not in the `.lodm`.** Measured: 49,140 tiles at ~30 bytes of compact JSON each is **1.41 MB** parsed on every load, against **294,624 B** of fixed-stride binary that needs no parse at all. The `.lodm` names the containers; the containers carry the tables.

**Hash strings are `0x` plus exactly 16 uppercase hex digits** (a JSON number would not survive a double), and the comparison against a container's u64 is **numeric**, so a lowercase or short-form writer is still readable. `levels[].container` paths are **Data-relative** with backslashes and no leading `Data\` — a reader that resolved them against the index's own folder would look in `Data\Terrain\Terrain\`.

### Example — `Data\Terrain\Commonwealth.VT.lodm`, default set, cover on

```json
{
  "lodm": 1,
  "family": "legacy",
  "kind": "terrainVT",
  "terrain": {
    "worldspace": "Commonwealth",
    "extent": { "south": -96, "west": -96, "north": 95, "east": 95 },
    "cellUnits": 4096,
    "content": 256,
    "border": 8,
    "stored": 272,
    "mips": 2,
    "aniso": 8,
    "rowOrder": "northUp",
    "compression": "none",
    "vhgtCorpusHash": "0xD8337D022F637F22",
    "paintCorpusHash": "0x4C1B90E7A5D3F208",
    "sheets": [
      { "role": "color", "dxgi": 71, "dxgiWithCover": 71, "colorSpace": "sRGB",
        "channels": "RGB albedo, grass tint folded in" },
      { "role": "msn",   "dxgi": 71, "dxgiWithCover": 71, "colorSpace": "linear",
        "channels": "model-space normal, 0.5+0.5 encoded" },
      { "role": "data",  "dxgi": 71, "dxgiWithCover": 77, "colorSpace": "linear",
        "channels": "R sky-free AO, G flow wetness, B shore proximity, A ground cover" }
    ],
    "cover": { "present": true, "normalisation": 96,
               "ordinal": true, "linearInComposition": true, "tintStrength": 0.35 },
    "coarseLevelsAreDownsamples": true,
    "levels": [
      { "index": 0, "dim": 2,  "tilesX": 96, "tilesY": 96, "unitsPerTexel": 32,
        "container": "Terrain\\Commonwealth.VT.2.lodv",  "tiles": 9216, "present": 9216 },
      { "index": 1, "dim": 4,  "tilesX": 48, "tilesY": 48, "unitsPerTexel": 64,
        "container": "Terrain\\Commonwealth.VT.4.lodv",  "tiles": 2304, "present": 2304 },
      { "index": 2, "dim": 8,  "tilesX": 24, "tilesY": 24, "unitsPerTexel": 128,
        "container": "Terrain\\Commonwealth.VT.8.lodv",  "tiles": 576,  "present": 576 },
      { "index": 3, "dim": 16, "tilesX": 12, "tilesY": 12, "unitsPerTexel": 256,
        "container": "Terrain\\Commonwealth.VT.16.lodv", "tiles": 144,  "present": 144 },
      { "index": 4, "dim": 32, "tilesX": 6,  "tilesY": 6,  "unitsPerTexel": 512,
        "container": "Terrain\\Commonwealth.VT.32.lodv", "tiles": 36,   "present": 36 }
    ]
  }
}
```

Field-by-field contract:

| field | type | meaning / rule |
|---|---|---|
| `lodm` | int | 1. The envelope's own version check. |
| `family` | string | `"legacy"` — required, and only `"legacy"`/`"pbr"` parse. Vestigial here; `kind` is the discriminator. |
| `kind` | string | `"terrainVT"`. |
| `terrain.worldspace` | string | must equal every container's `worldspaceEdid`, **case-sensitively**. |
| `terrain.extent` | object | the four inclusive **unpadded** cell fields, matching every container's `worldSouth/West/North/East`. Per-level padded rectangles live only in the containers. |
| `terrain.cellUnits` | int | 4096. Stated so a consumer never has to know it. |
| `terrain.content/border/stored/mips/aniso` | int | must equal every container's fields. |
| `terrain.rowOrder` | string | `"northUp"` only. Present because two conventions are live in this codebase; the container enforces it as reader rule 19. |
| `terrain.compression` | string | `"none"` or `"zlib"`. |
| `terrain.vhgtCorpusHash` | hex string | pins **heights**; a consumer refuses on mismatch. |
| `terrain.paintCorpusHash` | hex string | pins the **paint, LTEX and GRAS** inputs; a consumer refuses on mismatch. |
| `terrain.sheets[]` | array | one per sheet in payload order; `role`, `dxgi`, `dxgiWithCover`, `colorSpace`, and a human `channels` string. |
| `terrain.cover` | object | `present` false when no tile carries cover — then `dxgiWithCover` is never used; `normalisation` = COVER_FULL; `ordinal: true` with `linearInComposition: true` states that the value is an ordinal scalar that nonetheless averages correctly; `tintStrength` records what was folded into the albedo so a consumer can **match** it (not undo it — §3.3). |
| `terrain.coarseLevelsAreDownsamples` | bool | `true`. States in the file that levels above the finest are box filters of finer data, not measurements at their own scale, so a consumer never reads level 16's R as an AO term at 256 units per texel. |
| `terrain.levels[]` | array | finest first. `container` is a **Data-relative** path with backslashes and no leading `Data\`. `present` is the count of present tiles, so a consumer can size its worst case without opening the table. |

---

## 7. Panel and CLI surface

### 7.1 Panel

Existing anchors: `LodgenSection` (`src/lodgenmanager.cpp:291-336`) is the collapsible sub-panel, `Form`/`form(indent)` (`:407-430`) the two-column grid with `labelW = 176`, `wwHeading` the section title (never a `QGroupBox`), `wwMatchFieldStyle` mandatory on every combo, `wwMakeScrubField` mandatory on every spin box (applied in one sweep at `:1013-1015`).

**In the `Legacy terrain chunks (.btr)` section** (`:825-875`), after `f.span(texCheck)` at `:842`:

| row | object name | control | default | greyed when |
|---|---|---|---|---|
| `Ground cover and grass tint` | `LodgenCoverCheck` | check, `f.span` | off | `!btrCheck` or `!texCheck` |
| `Grass tint strength` | `LodgenTintSpin` | spin 0..100 `%` | 35 | `!btrCheck` or `!texCheck` or `!coverCheck` (label too) |

Both appended to `btrSub` (`:864-865`); `LodgenTintSpin` appended to the scrub list (`:1013-1015`); both names appended to the self-test inventory (`src/nifskope_ui.cpp:26821-26829`).

**New section**, placed after the `.btr` section and before `Chunk range`. It is a **`LodgenSection` folded by default**, exactly like the `.btr` one, whose header *is* its check box — not a `wwHeading`, because a `QGroupBox` would break `groups == 0` at `src/nifskope_ui.cpp:26928`:

| control | object name | type | default | notes |
|---|---|---|---|---|
| `Terrain virtual texture (.lodv)` | `LodgenVtCheck` | check, section header, `expandedByDefault = false` | off | tooltip: "A tile pyramid the consumer streams instead of loading whole chunk sheets. 256-texel tiles with an 8-texel border, five levels." |
| `Finest level` | `LodgenVtFinestBox` | combo, `wwMatchFieldStyle` | `2 cells per tile` | items: `2 cells per tile (32 units a texel)` data 2; `1 cell per tile (16 units a texel, full)` data 1 |
| `Chunk textures from the pyramid` | `LodgenVtBtrCheck` | check, `f.span` | on | greyed unless `LodgenVtCheck && btrCheck && texCheck` |
| summary | `LodgenVtSummary` | `QLabel` | — | one line, **live and computed**, never a fixed string |

**`LodgenVtSummary` renders the shared estimator (§7.4)**, not a hard-coded sentence. It must reflect the finest-level choice, the cover state, the compression choice, **and the `.btr` sheets it sits beside**, because the pyramid figure alone is not the cost the owner pays:

```
5 levels, 12,276 tiles. Pyramid about 2.01 GiB, chunk sheets about 1.99 GiB,
about 4.00 GiB on disk. Estimated bake: <from the §4.2 measurement>.
```

`wantVt()` follows the existing pattern at `src/lodgenmanager.cpp:1380-1386`:
`bool wantVt() const { return vtCheck->isChecked() && !vtSection->isHidden(); }`

**FO4CS-only.** The VT section is hidden when the target combo is the stock engine, alongside the other FO4CS-only rows at `:1020-1030`. Reason: the stock engine has no VT sampler and no reader for a `.lodv`; a tick under a hidden section is a setting saved under the other target, not a request — the rule already written at `:1381-1382`. **Ground cover is available on both targets** (the tint is *for* the stock engine); only the alpha plane is FO4CS-only, and it costs a stock user nothing on a chunk with no grass.

Border, content, mip count, compression and the tint's internals do **not** get panel rows. `feedback_end_menu_basics_only` in spirit: the panel gets the toggle and the one choice the owner named; the rest is CLI.

### 7.2 CLI

Threading a flag touches five places, all of them in `src/nifcli.cpp`: declare the `lg*` local (`:4605-4640`), parse it in the `else if ( t == ... )` chain (`:4684-4740`), document it in `usage()` (`:4422-4501`), add it to the `cmdLodgen` parameter list (`:2299-2311`), and pass it at the dispatch (`:5043-5053`). **Every one of the flags below must appear in `usage()`.** Measured today: 20 lodgen flags are parsed and undocumented, `--tex-dir` — the flag that turns the terrain bake on at all — among them, appearing only incidentally in the atlas paragraph's prose at `:4436`. This lane documents its own flags **and** `--tex-dir`.

| flag | arg | default | effect |
|---|---|---|---|
| `--cover` | — | off | bake ground cover + grass tint |
| `--no-cover` | — | **on** | do not |
| `--grass-tint` | float 0..1 | 0.35 | tint strength; 0 keeps the albedo byte-identical and still writes the cover plane |
| `--cover-full` | int 1..65535 | 96 | the cover normalisation constant |
| `--dump-cover` | path | — | also write the raw 512² u8 cover plane, north-up, headerless |
| `--vt` | dir | — | write the pyramid's containers and index under `<dir>` (the writer creates `<dir>/Terrain/`) |
| `--no-vt` | — | **on** | do not |
| `--vt-finest` | 1 or 2 | 2 | cells per tile at the finest level |
| `--vt-content` | int | 256 | power of two, 128..512 (a cost guard, not a format limit — the reader accepts 128..1024) |
| `--vt-border` | int | 8 | multiple of 4, and still a multiple of 4 after `mips−1` halvings |
| `--vt-mips` | int | 2 | stored mips per tile |
| `--vt-compress` | `none`\|`zlib` | `none` | payload compression |
| `--vt-btr` / `--no-vt-btr` | — | on with `--vt` + `--tex-dir` | assemble the `.btr` chunk sheets from the pyramid (§4.4) |
| `--vt-estimate` | — | — | print the estimate (§7.4) and exit 0 without baking |
| `--lodm-check` | path | — | parse a `.lodm` through the repo's own `lodmParse` and print keyword lines |

### 7.3 Pre-flight estimate, refusals and summaries

**One shared estimator**, used by `LodgenVtSummary`, by `--vt-estimate`, and printed to stdout by `--vt` **before it does any work**. It reports tiles, delivered bytes (pyramid **plus** the `.btr` sheets it sits beside), and minutes, and it accounts for the cover state and the compression choice. Without it, `--vt --vt-finest 1` silently begins a 6.35–8.47 GiB job, and `--vt-content 512 --vt-finest 1` a **17.946 GiB** one — the refusal list bounds each knob alone and never their product.

```
vt estimate: levels 6 tiles 49140 finest 1 content 256 border 8 mips 2 cover 1
  pyramid 9088934400 btr 2139740640 delivered 11228675040 minutes <measured>
```

CLI refusals, exact strings, all to `err()` with return 2:

```
error: --grass-tint takes a value in 0..1
error: --cover-full takes a value in 1..65535
error: --cover needs --tex-dir; the cover plane lives in the terrain data sheet
error: --vt needs --worldspace; a tile pyramid is one worldspace's
error: --vt-finest must be 1 or 2
error: --vt-content must be a power of two between 128 and 512
error: --vt-border must be a multiple of 4; a BC block is 4x4 and a border that
       splits one makes a tile depend on its neighbour's bake
error: --vt-border %1 cannot carry %2 mips; the border halves at every mip and
       must stay a multiple of 4 (that needs at least %3)
error: --vt-compress must be none or zlib
error: --vt-btr needs --tex-dir; there are no chunk sheets to take from the pyramid
error: worldspace %1 has a %2-character EDID; the container stores 32 bytes with a
       terminator, so this worldspace cannot be named in a .lodv
```

A **note**, not a refusal, printed to stdout and reflected in `levelCount`:

```
note: worldspace %1 is tile-aligned only to dim %2 (west %3, south %4);
      the pyramid stops there — %5 levels instead of %6
```

Panel — the one line beside the buttons (`src/lodgenmanager.cpp:1387-1392`: "a greyed button with no sentence next to it is a broken button"):

```
Ground cover needs the terrain textures ticked.
The terrain virtual texture is an FO4CS feature; the stock engine has no reader for it.
No grass here: this worldspace's landscape textures name no grass records, so the cover
plane would be all zero and nothing would change.
```

The third is a **summary**, not a refusal — shown next to a ticked `LodgenCoverCheck` when the worldspace's LTEX set yields `D == 0` everywhere, and Generate stays enabled.

---

## 8. The harness

Two new scripts, both in the house pattern (`tests/spells/lodt_write.sh` is the model): `set -u`; `ROOT="$(cd "$(dirname "$0")/../.." && pwd)"`; `NS/ESM/DATA/PY` all env-overridable; `W="$(mktemp -d)"` + `trap 'rm -rf "$W"' EXIT`; a preflight that exits **2** on a missing input as opposed to **1** on a failure; `checks=0; fails=0; check(){...}`; and a final `RESULT PASS` / `RESULT FAIL` plus a bare `[ $fails -eq 0 ]` so the script's exit code is the verdict. Do **not** source `tests/spells/_harness.sh` — both are headless and it exports `WW_WINDOW_AT` for GUI harnesses.

**Four rules applied to every check below.**

1. *Print the measured value and the bar on the same line, before the verdict* — a FAIL must be readable without a re-run.
2. *Derive the floor from the definition wherever one exists*; where it cannot be, say `[sampled]` in the printed line. The live warning is `tests/spells/lodgen_terrain.sh`, where `:221` asserts `aovals > 8` and today measures **9**, and `:150` asserts `skirtn > 100` and today measures **104** — two floors one step from failing on the very chunk they were calibrated on.
3. *Read the exe's mtime next to the verdict and refuse to run if it is not newer than* `src/lodgen.cpp`, `src/esmdata.cpp`, `src/nifcli.cpp`, **`src/io/lodvfile.cpp`, `src/io/lodmfile.cpp` and `src/lodgenmanager.cpp`** (`MISTAKES.md:233-247`). A pyramid built from a stale exe with a fresh `lodvfile.cpp` is that same mistake in new clothes.
4. *Pin the corpus.* Both scripts print the ESM's size and the worldspace's VHGT corpus hash in the preflight and **exit 2** unless the hash is `0xD8337D022F637F22`. Every floor in §8 is a property of one Fallout4.esm; a different plugin or load order silently redefines all of them while every check still prints ok.

Both scripts parse report lines **by keyword, never by field position** (`MISTAKES.md:34-38`; the live instance is `lodgen_water_subdiv.sh:96-98` reading `$8/$10/$12` off a line that grows) — including the new `cover` and `vt` census lines.

Reuse, do not reinvent: `decodeColor(b, off, w, h, stride, skip)` and `decodeAlpha(b, off, w, h)` from `tests/spells/lodgen_texture_arrays.sh:190-229` — for BC1 pass `(8, 0)`, for a BC3 colour half `(16, 8)`, and `decodeAlpha` reads a BC3/BC4 alpha block unchanged. `mipbytes(w,h,blockBytes)` at `:181-187` encodes our stop-at-4×4 rule. DDS header offsets: `hh, ww = unpack_from('<II', b, 12)`, `mips = unpack_from('<I', b, 28)[0]`, `fourcc = b[84:88]`, `dwReserved1` at 32..75, DX10 tail at 128, pixel data at 128 (legacy) / 148 (DX10). `decodeAlpha` reads mip 0 only, so C16 seeks to a named mip offset explicitly.

### 8.1 `tests/spells/lodgen_ground_cover.sh`

**Fixtures, with their premises asserted rather than described** (`MISTAKES.md:356-376`):

- **(−20, 24) at dim 4** — Sanctuary. Assert and print: 16 of 16 cells carry LAND; the paint's LTEX id set equals the frozen list of the 8 named forms (`LRootsEroded01`, `LRubbleRock01`, `LFallenLeaves01`, `LDriedGrass01`, `LRubbleRock01Grass`, `LRootsEroded01Grass01`, `LForestFloor01`, `LDriedGrass01NoGrass`, plus two NULL layers); `D(base) == 0` for both base LTEXes. Each FAILs if the ESM disagrees. This makes the check exercise the alpha path, where 98.9% of Commonwealth grass lives.
- **(0, 0)** for C2 — base `LOceanFloor01`/`LRiverbedSilt01` family. Assert and print that **all** its LTEXes, base *and* alpha-layer, carry 0 GNAM, and that the set is non-empty.
- **A chunk containing at least one `A > 1` texel** for C12, located from the model, named in the script header with its measured count. The 62 renormalising texels are the one place a cover-side change can leak into the colour composite and no existing fixture is known to contain one.
- **A second chunk of different terrain character** for C7 and C17, named in the script header.

**Frozen reference for C1**, measured against `release/NifSkope.exe` of 2026-09-06 17:17 with

```
lodgen <Fallout4.esm> --worldspace 3C --terrain-region -20 24 -17 27 --dim 4 \
  --out-dir <a> --tex-dir <a/tex> --data-root 'E:/Tools/Fallout 4/DataUnpacked/Data'
```

(26.9 s, rc 0, empty stderr), each file 174,888 bytes, and reproduced byte for byte by a second identical invocation:

| file | sha256 |
|---|---|
| `Commonwealth.4.-20.24.DDS` | `a918adfb4e507cd7b33d87aec3b91ce84df2e8259f0bca3be8109616b681115b` |
| `Commonwealth.4.-20.24_msn.DDS` | `1d817f76510e16204b60fa972eb38ecf65266c3847896afd592208bdd7e2d0c2` |
| `Commonwealth.4.-20.24_data.DDS` | `fe390a60741fc7365653a1c956d74f690c63b174b732f131705ea0f60316d2b7` |

These are **literals in the script**. The `$W/ref/` "pre-change exe" arm is deleted: the build skill renames the held exe aside on every build, so after the lane's second build that arm compares a build against itself.

**Bakes:** four `--terrain-region` invocations plus one short one — `--no-cover` and `--cover` on (−20,24), `--no-cover` and `--cover` on (0,0), and `--grass-tint 0` on one chunk. Cost is `(number of ESM-walking invocations) × ~27 s`.

**Independent paint.** C6 and C9 need `land.baseTex[q]`, `land.layers[q].ltex` and the 17×17 ATXT/VTXT opacities, none of which `--dump-land` emits (it carries heights only), and there is no Python ESM paint reader in the repo (`grep -rln 'VTXT|ATXT' tools/ tests/` = 0 hits). **The harness's Python parses LAND itself** — GRUP walk, zlib-decompressed records, `BTXT` + `ATXT` (formid, quadrant, layer) + `VTXT` (289 entries of u16 position / u8 pad / f32 opacity) — plus its own GRAS/LTEX walk. That is a few hundred lines and it is the only thing that makes C6 mean anything; adding a `--dump-paint` CLI instead would make C6 the writer checked against itself, which is `MISTAKES.md:672-700` verbatim ("a round-trip test that shared one table with the code it tested … passed 38 of 38 with the mapping deliberately wrong"). The Python's ESM extract is cached to the scratchpad the way `--dump-land` is.

**Two-stage value check.** `--dump-cover` writes the raw pre-compression plane, so C6 splits cleanly: *Python model vs raw plane* (a pure arithmetic comparison, max ≤ 1) and *raw plane vs decoded BC3* (a compression bound). This matters because **BC3 alpha's worst case is not 8/255**: the block is two 8-bit endpoints and 3-bit indices, so error scales with that block's own alpha **range** — a block spanning 0..255 has a step of 255/7 = 36.43 and a mid-ramp texel can miss by **18.21/255**. The ±5° gate makes cover near-binary at every grass edge, which is exactly where those high-contrast blocks are. The per-block bar is therefore `blockRange/14 + 1`, with **0 violations**, not a flat constant.

### 8.2 `tests/spells/lodgen_terrain_vt.sh`

Fixture: the region `-20 24 -13 31` (8×8 cells) — 16 dim-2 tiles, 4 dim-4 tiles, 1 dim-8 tile. Plus a direct dim-8 bake of the same ground for V8b, and a dim-32 comparison for V9c.

### 8.3 Mutation control — `MUTATE=<id>`

Fifteen checks with no "break it and watch it fail" is a green nobody has earned; `lodgen_terrain.sh` already carries one ("CONTROL: the shifted comparison degrades", `ctrl > 20`). Both scripts accept `MUTATE=<id>`, which builds a deliberately wrong exe into `$W` and requires the **named** check to FAIL. Any mutation caught by no check is a missing check.

| id | mutation | must fail |
|---|---|---|
| M1 | slope-gate half-width 5 → 50 | C9 |
| M2 | `COVER_FULL` 96 → 80 | C5, C6 |
| M3 | Density and Max Slope byte offsets swapped (0 ↔ 2) | C6, C14 |
| M4 | cover plane written Y-flipped | C6, C16 |
| M5 | tint lerp moved before the VCLR multiply | C11b |
| M6 | `Dtex` composited without the base term | C6 |
| M7 | renormalised `a_i` fed into the colour lerp | C1 |
| M8 | box filter `+2>>2` → `/4` | V8 |
| M9 | every tile written as tile (0,0) | V20c |
| M10 | tile table north/south flipped | V20b |
| M11 | `vhgtCorpusHash` written as 0 | V19 |
| M12 | no-cover tile staged as alpha 0xFF | V8, V9 |

### 8.4 Runtime budget, honestly

`lodgen_ground_cover.sh`: five region bakes × ~27 s = **135 s**; cold `--dump-land` **61 s** (0 s warm, cached to the scratchpad); the Python LAND+GRAS+LTEX walk, cached the same way, **~90 s cold / ~5 s warm**; ~6 `get`/`dump` reads at 1.2 s = 8 s; Python BC decodes of 8 sheets at 512² ≈ 16 k blocks each ≈ 20 s. **Cold ≈ 5.5 min, warm ≈ 3 min.**

`lodgen_terrain_vt.sh`: one `--vt` region invocation ≈ 35 s; one `--no-vt` control ≈ 27 s; one direct dim-8 bake ≈ 27 s; one `--vt-btr` + `--tex-dir` bake ≈ 30 s; a second `--vt` run for V22 ≈ 35 s; ~8 CLI reads ≈ 10 s; Python inflate + ~20 tile decodes ≈ 50 s; the 22-way mutation battery for V2 (in-Python file surgery, no rebuild) ≈ 15 s. **≈ 4 min.**

**The real per-iteration loop for the owner**, on a lane touching 8 files and 30+ anchors: build + `lodgen_ground_cover.sh` + `lodgen_terrain_vt.sh` + `lodgen_terrain.sh` (3 min 2 s, 21 checks) + `lodgen_identity.sh` — realistically **15–25 min a round**, not 6.

### 8.5 What neither harness can test, said out loud

(The `tests/spells/lodt_btd.sh:21-38` model.) Nothing here renders. `tests/spells/lod_channel_preview.sh` is the only thing that guards the terrain channels against rendering identically to one another, and it needs a GPU and a GUI. Its `cmp -s` checks at `:71` and `:77` are required to **fail**; a fourth channel makes that gate more important, not less. **This lane adds the cover channel to that script and the two new `cmp` pairs** (cover against AO, wetness and shore), and the ground-cover feature is **not finished** until that script has been run once with the game down and printed a named PASS line — an intention is not a gate.

Also un-independent and named as such: C11's `Ttex` reference resolves one grass's diffuse through the repo's own `nifparse` (`tools/rigging_prototype`, already used by `lodgen_terrain.sh`) plus a Python DDS smallest-mip read plus the §3.2 un-premultiply — independent of the bake's C++ path, but sharing the repo's NIF parser.

### 8.6 The check table

**`lodgen_ground_cover.sh`**

| check | what it measures | floor | failure it catches |
|---|---|---|---|
| C0 | preflight: exe mtime vs 6 sources; ESM size; VHGT corpus hash; the three fixture premises | mtime newer than all 6; hash `== 0xD8337D022F637F22`; 16/16 LAND; frozen 8-LTEX set; `D(base) == 0` both | a stale exe, a different plugin, a fixture that cannot violate the invariant |
| C1 | `--no-cover` bake of (−20,24) vs the three frozen sha256 literals; and the `--cover` bake vs the same literals | all three sha256 equal, all sizes `== 174888`, `_data.DDS` fourCC `== DXT1`; the `--cover` bake **must not** match | any byte moving with the feature off; a comparison that cannot distinguish; M7 (renormalisation leaking into colour) |
| C2 | `--cover` on grass-free (0,0) vs its `--no-cover` bake, **plus** the census line | three `cmp -s` clean; census present with `texels=262144 coverMax=0`; the chunk's LTEX set non-empty and all-zero-GNAM | a cover pass that changes a grass-free chunk — and a `cmp` that passes because the code never ran |
| C3 | `_data.DDS` fourCC on (−20,24), read twice (`b[84:88]` and `dd bs=1 skip=84 count=4`); `hdr[8]` at offset 32 | fourCC `== DXT5`; `hdr[8] == 'WWCV'`; `hdr[9] >> 24 == 1` | a cover sheet with no provenance stamp — the unrepairable failure |
| C3b | decoded RGB planes of the `--cover` and `--no-cover` `_data.DDS` | identical on every texel of every block whose 16 texels all have cover 0 | the BC1→BC3 switch silently moving AO/wetness/shore |
| C4 | `_data.DDS` size, both directions | `== 349648` on the grassy `--cover` bake; `== 174888` and fourCC `DXT1` on its `--no-cover` bake and on the grass-free `--cover` bake | arithmetic drift; a fourCC-as-switch contract that only holds one way |
| C5 | decoded mip-0 alpha vs the Python model's prediction **for this chunk** | nonzero-cover fraction within 2 pp of model; mean over nonzero within 2/255; max within 4/255; distinct ≥ 16 | an all-black or saturated plane; a bar calibrated to the first run; M2 |
| C6 | (a) Python model vs `--dump-cover` raw plane over stratified samples (1,024 with model cover > 0, 1,024 with cover == 0, reported separately); (b) raw plane vs decoded BC3, per block; (c) control: model recomputed with the chunk shifted one cell | (a) max ≤ 1; (b) per-block `blockRange/14 + 1`, 0 violations; (c) shifted max abs diff **> 32/255** | a wrong composite; a wrong BC3 model; both sides computing zero; M3, M4, M6 |
| C7 | Pearson r between decoded alpha and each of R, G, B, on **two** chunks; chunk named in the printed line | `|r| < 0.5` against all three on both; print all six | the recorded "sky visibility IS AO, r = 0.969" collapse |
| C8 | pairwise r among R, G, B | `|r| < 0.9` for all three pairs; print the matrix | `[sampled]` — a new instrument; the existing channels collapsing into one another |
| C9 | from the model, `G0 = {Dtex>0, gate==0}` and `G1 = {Dtex>0, gate==1}`, tested **against the file** | `|G0| > 0` and `|G1| > 0` (FAIL if either empty); every `G0` texel's decoded alpha ≤ 2/255; mean over `G1` > 8/255 and within the C6 bar | a build with the slope gate deleted; M1 |
| C10 | differing 4×4 blocks between the two diffuse bakes, crossed with the model's cover | every differing block contains ≥ 1 texel with cover > 0 (0 violations); differing blocks > 250 `[sampled]`; blocks entirely cover-0 are **byte-identical in raw BC1** | a tint applied to the wrong texels — at the granularity BC1 actually preserves |
| C11 | for the 256 highest-cover texels, angle between `(tinted − plain)` and `(T_independent − plain)`, `T` resolved through `nifparse` + Python DDS + un-premultiply | angle < 5°; **and** mean delta magnitude > 3/255 | a packing bug; an all-zero delta scoring as collinear |
| C11b | mean delta direction on texels whose VCLR is far from 1.0 | matches the after-VCLR model within 5°, and **not** the before-VCLR model | M5 (tint on the wrong side of the VCLR multiply) |
| C12 | `--grass-tint 0` on the A>1 fixture vs its `--no-cover` bake | `.DDS` `cmp -s` clean; `_data.DDS` fourCC `== DXT5`; the fixture's A>1 texel count printed and > 0 | the renormalisation leaking into the composite; a fixture that cannot expose it |
| C13 | the `cover …` census line under `--cover` | line **present** (absent is a FAIL); parsed by keyword; `danglingLtex`, `danglingGnam`, `clipPainted`, `clipBase` all 0; `ltexNoGnam` and `grasNoTint` printed with denominators, ungated | a counter that never prints; an error class conflated with a by-design nonzero |
| C14 | the bake's census line vs the Python walk, cross-implementation | exact agreement on LTEX 128, GRAS 107, GNAM links 153, LTEX-with-GNAM 63, `DATA` size 32 in all 107, bytes-left-over 0 on **both** sides | one reader auditing its own completeness; M3 |
| C15 | census **counts**, not wall clock | `grasReads ≤ 153`, `nifReads ≤ 71`, `texLoads ≤ 28`, `ltexResolves ≤ 8 × quadrants`; wall clocks printed as statistics | a per-texel ESM or hash lookup hiding inside a 24.5 s parse |
| C16 | decoded alpha at mip 1 (offset `128 + 16384·16`) and at the last mip | mip 1 == 2×2 box filter of mip 0 within the per-block bar, 0 violations; last mip alpha neither constant 255 nor constant 0 | BC1's forced-opaque alpha (`:3661`) surviving into the BC3 path; M4 |
| C17 | r between cover and (a) `_msn` B, (b) `θ` in degrees, over `Dtex > 0`; plus a least-squares fit of cover on `θ` | `|r| < 0.5` on both; residual variance **> 50%** of total | the "recoverable as `acos(n.z)`" failure that killed the slope candidate — which C7 cannot see |

**`lodgen_terrain_vt.sh`**

| check | what it measures | floor | failure it catches |
|---|---|---|---|
| V1 | all 256 header bytes of every container, every field printed | magic `LODV`, version 1, headerBytes 256, content 256, border 8, stored 272, mips 2, sheetCount 3, aniso 8, `levelDims == {2,4,8,16,32,0,0,0}`, reserved tail all zero | a header that drifted from §5.1 |
| V2 | **22-way mutation battery**: one mutated copy of the 4.76 MiB root per §5.4 rule | 22 of 22 refusals, each **naming the right field**; 0 refusals on the unmutated file | a validator that returns true unconditionally — the hole 'all 18 pass' left open |
| V3 | `fileBytes` vs stat, every container | equal | a truncated or over-long write |
| V4 | `tilesX`, `tilesY`, `tileCount` vs the padded rectangle and `levelDim` | exact from `(east−west+1)/levelDim`, with the divide proven exact | the truncating-divide edge-cell loss |
| V5 | **inflate every present tile**, then compare | `len(inflate(stored)) == rawBytes ==` 138,720 or 184,960, matching its `COVER` bit; no other value | a table that lies about a payload it was never opened against |
| V6 | sorted present offsets | each `% 4096 == 0`; `offset[i] + storedBytes[i] <= offset[i+1]`; every pad byte zero; tiles in table-index order | overlap; nondeterministic padding |
| V7 | recomputed CRC32 of every present tile, and `indexCrc32` | 0 mismatches | payload corruption; **offset aliasing**, which per-tile CRCs cannot see |
| V8 | a dim-4 tile vs the Python `(a+b+c+d+2)>>2` recompute over the four dim-2 children, **content and each of the four border strips separately**, filter re-typed not imported | content: median ≤ 2/255, p95 ≤ 4/255, **max ≤ 4/255, 0 above**; each border strip: median ≤ 2/255, max ≤ 4/255; msn after renormalising: median ≤ 2°, max ≤ 4° | a wrong filter; one wholly wrong border edge (2.9% of a tile, invisible under p95); M8, M12 |
| V8b | a **filtered** dim-8 tile vs a **direct** dim-8 bake of the same ground | colour and cover: median ≤ 4/255; AO and shore: the divergence **printed with its measured bar**, and recorded in `docs/LODGEN_TERRAIN_VT.md` | the implementation checked only against itself; the scale-dependence of AO and shore going unstated |
| V9a | `--vt-btr` dim-4 colour sheet vs a direct bake | byte-identical after decode **if** `dominantBase` is chunk-scoped; otherwise every differing texel is model-marked NULL-LTEX or `baseTex == 0`, count printed | a wrong tile→chunk mapping; an unnoticed `dominantBase` rescope |
| V9b | the same sheet's msn | byte-identical outside a 1-texel ring; inside the ring angular diff ≤ 2° | a bar that cannot hold, being loosened — or "fixed" by restoring the edge clamp |
| V9c | the same sheet's data-R (AO), at dim 4 **and at dim 32** | interior 384×384 byte-identical after decode; differences only within 64 texels of the **chunk's outer** boundary; max ≤ 8/255; the dim-32 bar measured and printed separately | the 2,048-unit march clamp; a locality bar that admits 75% of the sheet; four halvings of AO assumed from one |
| V9d | every mip level of the assembled sheet, not just mip 0 | mip count `== 8`; each mip within the V8 bar of the Python filter over the assembled staging | a mip-chain regression shipping as unattributable LOD shimmer |
| V10 | the container's dim-4 parent against **two** Python candidates: the box filter and a nearest downsample | matches the box filter within V8; **mismatches** the nearest downsample by max > 8/255 on > 5% of texels; both printed | "baked at the wrong dim"; "the coarse level is a nearest-resample" |
| V11 | border vs neighbour content, for a **horizontal and a vertical** adjacency | max ≤ 4/255, 0 violations, both axes; **and** the border differs from the tile's own edge column on > 25% of rows with a mean abs diff above the printed noise floor | a border filled by clamping; a low-detail seam scoring as a clamp |
| V12 | the `.lodm` through `lodgen --lodm-check`, **and** independently in Python | both: envelope ok, `lodm == 1`, `family == "legacy"`, `kind == "terrainVT"`, payload size == file size − 12, no trailing bytes | a harness reimplementation being mistaken for the repo's parser |
| V13 | every `terrain` scalar vs every container header; every `levels[].container` resolved **Data-relative** | 0 disagreements; every file exists; `tiles`/`present` match each table | an index that describes a different bake |
| V14 | `lodmSourceCandidate` over the region's `.btr` material/diffuse strings | the index path is not among the results | the index being opened by a reader that ignores `kind` |
| V15 | a container truncated by 1 byte | refusal names `fileBytes` | a reader that reads past the end |
| V16 | one byte flipped inside one payload | that tile's CRC fails and names the tile index; **the other tiles still load** | whole-file rejection on a single bad tile; a CRC that is never checked |
| V17 | the region's `.bto` and `.bto.manifest.txt` vs a `--no-vt` run | `cmp -s` identical | a terrain-only change moving the object path |
| V18 | container size, byte for byte | `fileBytes == 256 + (tileTableOffset − 256 pad) + 24·tileCount + Σ aligned payloads`, **exact**, not "within 1 KiB" | a padding bug living in a kilobyte of slop |
| V19 | each container's `vhgtCorpusHash` and `paintCorpusHash`, the `.lodm`'s strings, the CLI's own printed hash, and the F4FX heightmap block's copy | all equal; VHGT `== 0xD8337D022F637F22`; paint hash equal across all four | a writer storing 0 — well-formed, and it makes every consumer refuse in the field; M11 |
| V20 | georeferencing: (a) 4 named tiles per level vs a direct bake of the same world rect; (b) the north-most tile row vs the direct bake's north edge; (c) tile (0,0) vs tile (1,0) | (a) within the V9 bar; (b) matches — the only test of `ROW_ORDER_NORTH_UP`; (c) mean abs diff > 8/255, printed | X/Y transposition; a north/south flip; every tile being the same tile; M9, M10 |
| V21 | with `--vt-compress zlib`: every present tile's zlib header; with `none`: every `storedBytes == rawBytes`; plus 3 real tiles inflated by a 20-line test compiled against FO4CS's `FarFieldInflate.h` | 0 violations of CM=8 / CINFO≤7 / FDICT clear / `%31==0`; the consumer's own inflater succeeds on all 3 | a stream the actual consumer cannot read |
| V22 | two full `--vt` runs, containers and `.lodm` | byte-identical | a nondeterministic pyramid — which no other check would see |
| V23 | peak RSS of the `--vt` process | **≤ 2.0 GiB**, printed | a literal mosaic implementation (6.75 GiB) or a buffered writer (1.19 GiB) |

---

## 9. What is deliberately not done, and what is elastic

### Not done

- **No consumer.** FO4CS has no `.lodm` reader, no tile streamer, no residency manager, and no terrain-colour consumer of any kind: its whole terrain surface is heights for shadows (one `CreateTexture2D` of a whole-worldspace R16 image, `FarFieldHeightmapRuntime.cpp:1639-1644`, from either the `.dds` heightmap or a whole-file inflate of `.lodt`). This lane produces files. The owner's words: "there's no code for that yet, so we can be elastic for now."
- **No indirection texture, no feedback pass, no physical pool atlas, no residency policy, no anisotropy choice.** Every one of those depends on a pool size and a camera the bake does not have. Pre-packing tiles into a pool would freeze a consumer choice and destroy per-tile streaming. The container **declares** what its border supports (`anisoSupported = 8`, now checkable as rule 21) and the consumer clamps its own sampler.
- **No runtime compositing.** The Frostbite trade (ship the splat layers, composite pages on the GPU) is the opposite of this design. We spend disk, not GPU.
- **No water filter on cover.** §2.4. The B channel already carries shore proximity.
- **No per-GRAS channels.** One ordinal scalar plus one tint.
- **No `_msn` format change.** Vanilla ships DXT5 with a constant-255 alpha; we ship BC1 and lose nothing measurable. Changing it is a separate decision with its own evidence.
- **No dim-2 or dim-1 `.btr` chunks.** `src/lodgen.cpp:719` refuses any dim but 4/8/16/32, verified by running `--dim 1` and `--dim 2` → `error: dim must be 4, 8, 16 or 32`. The pyramid's finest levels are baked by the *tile* baker, which does not go through `lodgenBuildTerrainChunk`, so that guard stays exactly as it is.
- **No progressive tile payloads.** A parent's BC block is not a subset of a child's texels.
- **No untinted pyramid colour sheet.** §4.4 requires the pyramid to be able to supply the `.btr` bytes unchanged, so the pyramid inherits the tinted albedo. The consequence is a real fork, stated rather than hidden: the one consumer that could draw real grass (FO4CS) cannot recover the untinted ground from a tinted bake, and `--grass-tint 0` is an either/or chosen at bake time, not at load time. Deciding otherwise after a bake is a re-bake. This is Open Question 2.
- **No fix for the Land shader's Smoothness divergence** (ours 1, vanilla 0.0, confirmed by two independent readers on `Commonwealth.4.-20.24.BTR`). It is real, nothing gates it, and it is not this lane's.
- **No DLC-worldspace paint coverage.** Far Harbor and Nuka-World LAND need master-index formID remapping that the probe used here does not do. Their LTEX/GRAS records were measured; their paint was not.

### Load-bearing (changing these is a format break)

1. **`.lodv` magic, version, and the 256-byte header size.** A v2 header may grow; a v1 reader must refuse v2 and must ignore a zero-filled reserved tail.
2. **Little-endian, absolute 64-bit offsets, fixed 24-byte table stride, 64-bit table arithmetic.** An int32 relative offset dies inside one worldspace: the finest level's payload alone is 1.19 GiB.
3. **North-up row order, in the tile table and inside every payload.** Two conventions are already live in this codebase and the mismatch already cost FO4CS a Y mirror (`FarFieldLodtFormat.h:28-35`). Rule 19 makes it a refusal, not a hint.
4. **Border a multiple of 4, and `border >> (mips−1)` still a multiple of 4** (rule 12). Break it and a tile's blocks depend on its neighbour's bake, which kills incremental re-bake and every byte-identity gate.
5. **Both corpus hashes in the header and the index, hashed in ascending order.** A stale VT against an edited plugin must be a named refusal, exactly as a stale heightmap already is — and a load-order permutation must not be one.
6. **The `.lodm` envelope version 1, `lodm: 1`, `family: "legacy"`.** The parser hard-rejects a third family; a new family word splits the corpus. `kind` is the real discriminator and `lodmfile.h` says so.
7. **The index lives outside every `lodmSourceCandidate()` path.**
8. **`_data.DDS` fourCC as the cover switch, qualified by the `'WWCV'` stamp** (DXT1 = no cover; DXT5 + stamp = alpha is cover; DXT5 without the stamp, or with constant-255 alpha, = no cover). No sidecar.
9. **Cover normalisation against a fixed constant, written into the container.** A per-run maximum makes two chunks incomparable.
10. **Per-tile format selection for role 3** via `dxgiFormat` / `dxgiFormatCover` and the `COVER` bit.
11. **Payloads in table-index order, zero pad bytes, absent entries all-zero.** This is what makes the file byte-deterministic.
12. **`--no-cover` and `--no-vt` produce byte-identical output to today.** Gated by C1/C2/V17.

### Elastic (a number, changeable without a format break, and where it is written down)

| knob | value now | how to change it | who reads it |
|---|---|---|---|
| `COVER_FULL` | 96.0 | `--cover-full`, header `coverNormalisation`, index `terrain.cover.normalisation` | the container and the index both state it |
| slope gate half-width | ±5° | one constant | nobody outside the bake |
| `tintStrength` default | 0.35 | flag + panel + header + index | the container states what was folded in |
| tint un-premultiply threshold | alpha ≥ 0.05 | one constant | nobody |
| `contentTexels` | 256 | `--vt-content` (writer 128..512), header field (reader 128..1024) | header and index both carry it |
| `borderTexels` | 8 | `--vt-border`, header field | header carries it; `anisoSupported` is checked against it by rule 21 |
| `mipCount` | 2 | `--vt-mips`, header field | header carries it |
| finest level | dim 2 (dim 1 = full) | `--vt-finest`, panel combo | header `levelDims`, index `levels[]` |
| `compression` | none | `--vt-compress`; **any new codec must be a new id, never a reinterpretation of 1** | reader refuses unknown ids |
| payload alignment | 4,096 B | writer-side only; the reader validates whatever the header implies — **change the rule to 512 B if the padding measurement (0.392% all-BC1, 1.869% all-BC3-data) ever bites** | reader rule 16 |
| sheet formats and colour space | BC1 / BC1 / BC1-or-BC3, sRGB / linear / linear | header `sheets[]` | index `sheets[]` |
| texture LRU budget | 512 MiB | one constant | nobody; V23 bounds the total |

### Three things the investigators disagreed on, decided here

- **Tile size: 512 content at finest dim 4, or 256 at finest dim 2.** Decided **256 / dim 2**, the owner's approved outline. The two are the *same* texel density (32 units per texel, vanilla's exact finest ring); the owner's choice additionally gives 4× the streaming granularity, which is the only reason a VT exists. The 512-content argument's strongest point — that the Land vertex UV is a half float2 whose ULP in [0.5, 1) is 2⁻¹¹, i.e. 0.25 texel at 512 and 1.0 texel at 2048 — does not bite, because the `.btr` mesh never addresses a tile: §4.4 assembles a 512² sheet and the UV stays `(x/4096, 1 − y/4096)`.
- **One container per worldspace, or one per level.** Decided **one per level**, the owner's outline. The consumer investigator's argument for one file was the 4,096-name enumeration cap — which does not apply in `Data\Terrain\`, where names are read exactly and there are at most 6 per worldspace. Per-level containers let the 36-tile root be loaded on its own, which is why `COVER_FULL` and the level ladder had to move into the header.
- **Border 8 or 16.** Decided **8**. 16 buys a third block-aligned mip for +26.563%; 8 costs +12.891% and still carries 8× aniso at both stored mips, and the third mip is redundant with the next coarser level by the measured 33.203% / 25% redundancy.

---

## 10. Work list, file by file

### `src/esmdata.h`

- After `struct EsmLandLayer` (**:77-82**) and before `struct EsmLand` (**:84**): add `struct EsmGrass { quint32 form; quint8 density, minSlope, maxSlope, flags; quint16 unitsFromWater; quint32 waterType; float positionRange, heightRange, colourRange, wavePeriod; QString model; };` and `struct EsmLtexCover { quint16 density = 0; float maxSlope = 0.0f; float tint[3] = {0,0,0}; bool hasTint = false; int grasses = 0, grassesWithoutTint = 0; bool resolved = false; };`
- Beside `void ltexTextures(...)` (**:154**): `const EsmLtexCover & ltexCover( quint32 ltexForm, const QString & dataRoot ) const;` and `bool grass( quint32 grasForm, EsmGrass & out ) const;`
- Beside `mutable QHash<quint32, QPair<QString,QString>> ltexCache;` (**:175**): `mutable QHash<quint32, EsmLtexCover> ltexCoverCache; mutable QHash<quint32, EsmGrass> grasCache;`
- **:141** `vhgtCorpusHash()` — change the walk to **ascending cell order (y then x)**, not file order, and add `quint64 paintCorpusHash() const;` beside it per §5.1. Re-baseline the F4FX heightmap block's copy in the same commit; V19 gates that they still agree.

### `src/esmdata.cpp`

- New `EsmWorld::grass()` modelled on `lodBase()` (**:436-495**) — `esm->findRecord`, `*r == "GRAS"`, `ESMFile::ESMField` walk, `DATA` decoded per §2.1 with a `f.size() >= 32` guard, `MODL` through `fieldString` (**:31**), cached.
- New `EsmWorld::ltexCover()` modelled line-for-line on `ltexTextures()` (**:497-536**): walk the LTEX's fields, collect every `GNAM` through `esm->mapFormID( *lr, f.readUInt32() )` (the same mapping `TNAM` uses at **:511-513** — this is what makes BNS Landscape's 26 LTEX overrides and True Grass's 40 new GRAS records resolve under a real load order), sum `D`, weight `S`, and call the tint resolver. A GNAM whose target is not a GRAS record increments `danglingGnam`.
- New `EsmWorld::paintCorpusHash()` per §5.1.
- The tint resolver lives in `src/lodgen.cpp` (it needs `lodgenReadAsset` and `lodgenLoadTexture`, both internal-linkage there) and is reached through a function pointer or a small callback set by the bake — **do not** move the DDS/asset machinery into `esmdata.cpp`.

### `src/lodgen.h`

- **:103-183** `LodgenTerrainOptions`: add nothing. The bake takes loose parameters (**:314-315**) and nothing in this struct reaches it.
- **:314-315**: extend the declaration to
  `bool lodgenBakeTerrainTextures( const EsmWorld &, int chunkX, int chunkY, int dim, const QString & dataRoot, const QString & outDir, const LodgenCoverOptions &, LodgenBakeCaches *, QString * error );`
  and update the doc comment at **:305-311**, which still says "Uncompressed BGRA DDS (BC1 later)".
- New declarations beside it:
  `struct LodgenCoverOptions { bool cover = false; float tintStrength = 0.35f; float coverFull = 96.0f; QString dumpCoverPath; };`
  `struct LodgenBakeCaches;   // opaque; owns the texture LRU, the LTEX/GRAS/tint cache and the channel cache`
  `struct LodgenVtOptions { int finestDim = 2; int content = 256; int border = 8; int mips = 2; int compression = 0; LodgenCoverOptions cover; };`
  `bool lodgenBakeTerrainVt( const EsmWorld &, const QString & dataRoot, const QString & outDir, const LodgenVtOptions &, QString * report, QString * error );`
  `bool lodgenVtChunkSheets( const QString & vtDir, const EsmWorld &, int chunkX, int chunkY, int dim, const QString & texDir, QString * error );`
  `bool lodgenVtEstimate( const EsmWorld &, const LodgenVtOptions &, bool alsoBtr, LodgenVtEstimateOut * out );`

### `src/lodgen.cpp`

| anchor | change |
|---|---|
| **:3633** | `lodgenWriteDds( path, w, h, bgra, bool bc3 = false, int maxMips = 0, bool bc1Alpha = false )` — the declaration has **defaults** and the `_data.DDS` call site passes **four** arguments, taking `bc3` from the default. It is not "already passed positionally"; the call site changes. |
| **:3663-3668** | the mip box filter — `acc[k] / 4` becomes `(acc[k] + 2) >> 2` on all four channels, per §4.3. Re-baseline `lodgen_identity.sh` in the same commit and record the changed hashes. |
| **:3674-3687** | DDS header — stamp `hdr[8] = 0x56435757` (`'WWCV'`) and `hdr[9] = (1 << 24) | round(coverFull)` when writing a cover sheet; both are inside `dwReserved1[11]` (file offsets 32..75) and zero today |
| **:3725-3729** | `lodgenEncodeBC1Block(..., !bc3)` — unchanged, and load-bearing: it is why the RGB bytes survive the BC1→BC3 switch (§2.5) |
| **:1611-1643** | `lodgenReadAsset` — used unchanged by the tint resolver for the grass `.nif` |
| **:3735-3775** | `lodgenLoadTexture` — used unchanged; already resolves a `.bgsm` to its first slot at **:3745-3761** |
| **:4555-4556** | signature: `+ const LodgenCoverOptions &, LodgenBakeCaches *` |
| **:4563-4567** | `RES` and `TILE` unchanged; comment that the pyramid's tile baker shares both |
| **:4585** | `texCache` — **no longer a local.** It moves into `LodgenBakeCaches` as an LRU with a 512 MiB budget, owned by the caller, so 9,216 tiles do not re-decode 21.3 MiB per texture per tile |
| **:4726** | the `delete t` sweep moves to the cache's destructor |
| **:4596-4607** | `dominantBase` — unchanged in the chunk path; in the tile path it is computed over the **enclosing dim-4 chunk's** cell set and passed in (§4.4) |
| **:4609-4718** | the paint loop — accumulate `A`, `Dtex`, `Stex`, `Ttex` alongside the existing colour composite; renormalise into a **separate `aCover[]`**, never into the opacities the colour composite uses |
| **:4625** / **:4654** | resolve `D`/`S`/`T` for the quadrant's layer set **once on quadrant entry**, into a small fixed array |
| **:4659-4675** | the layer loop — index the per-quadrant array; no hash lookup here |
| **:4676-4692** | VCLR unchanged |
| **after :4692, before :4694** | the tint lerp, inside `if ( w > 0.0f && Dtint > 0.0f )` |
| **:4700-4716** | msn — reuse `nrm` for `θ = acos(nrm[2])`; store the cover byte into a new `std::vector<quint8> coverPlane`, tracking `coverMax` |
| **:4721-4728** | replace with the §2.7 census: **printed unconditionally under `--cover`**, one physical line, `key=value` tokens, error and informational counters separate |
| **:4731-4736** | names unchanged |
| **:4786-4787** | `lodgenTerrainChannels` — result cached per cell-ring in `LodgenBakeCaches` |
| **:4815** | `aoTex` seed stays `0xFFFFFFFFU`. **The earlier draft's "seed becomes `0xFF000000U`-based so a missed texel is not a plausible opaque white" is withdrawn**: every index is written via `aoTex[(RES-1-j)*RES+i]`, so the seed is unreachable and describes a texel that cannot exist — and `0xFF000000` *is* alpha 255, which with cover in alpha would mean "a missed texel is fully grassed", making `coverMax` 255 for every chunk and BC3 unconditional. The alpha term is the cover byte, and it is written for every texel. |
| **:4833-4849** | **DELETE** the stale "A = SLOPE" paragraph |
| **:4850-4861** | rewrite: the four rejects stay, cover joins as the fifth candidate with the §2.6 argument, the BC1/BC3 switch rule and the `'WWCV'` stamp |
| **:4862-4865** | pack: after the loop, if `coverMax > 0` OR `coverPlane` into `aoTex`'s alpha; otherwise leave `0xFF` (§2.5 — writing 0 into a BC1 sheet would trip punch-through on every block) |
| **:4868** | `lodgenWriteDds( base + "_data.DDS", RES, RES, aoTex, /*bc3=*/ coverMax > 0 )` |
| **new, after :4875** | `lodgenBakeTerrainVt`, `lodgenVtChunkSheets`, `lodgenVtEstimate`. The VT writer must live **inside** `src/lodgen.cpp`: `lodgenWriteDds` (**:3632**), `lodgenLoadTexture` (**:3735**) and `lodgenEncodeArrayLayer` (**:3783**) are all in nested anonymous namespaces (opened **:3508**/**:3514**, closed **:3630**/**:3896**) and none is declared in the header. The tile encoder reuses the BC1/BC3 block encoders from `lodgenWriteDds`'s body — factor them out into the same anonymous namespace rather than duplicating them. `lodgenBakeTerrainVt` implements the §4.3 band traversal and the §5.3 streaming writer. |
| **:708-1379** | `lodgenBuildTerrainChunk` — **no change**. UV at **:933-934**, shader at **:964-981**, `Num Textures 10` at **:971**, the `_msn` string-replace at **:976-977** all stay |
| **:719** | the dim guard — **no change** |

New file **`src/io/lodvfile.h` / `.cpp`**: the `.lodv` header/table structs, the streaming writer, and a `lodvValidate()` implementing all 22 rules of §5.4 so the harness, the CLI and any future reader share one implementation. The harness's Python re-implements the **layout**, not the C++ (`MISTAKES.md:672-700`: a round-trip test that shares one table proves nothing), and V2's 22-way mutation battery is what makes the shared validator trustworthy.

### `src/io/lodmfile.h` / `lodmfile.cpp`

- **No code change.** `kind` is unvalidated (**cpp:56**), unknown top-level keys are ignored, `family: "legacy"` parses. Add `"terrainVT"` to the `kind` line of the header comment (**h:29-35**), the `terrain { … }` line to the payload table, the `kind` doc on `LodmMaterial::kind` (**h:~68**), and one sentence recording that **`family` is vestigial for `kind: "terrainVT"`; `kind` is the discriminator**.
- The writer for the index is `lodmWriteFile( path, root )` (**cpp:107+**), called from `lodgenBakeTerrainVt`.

### `src/nifcli.cpp`

| anchor | change |
|---|---|
| **:2299-2311** | `cmdLodgen` parameter list — the new flags arrive as **two structs** (`LodgenCoverOptions`, `LodgenVtOptions`) plus `vtDir` and `vtBtr`, not as 8 loose parameters onto the existing 34. Converting the remaining 34 to an options struct is a separate, mechanical commit. |
| **:2906-2975** | the region path: validate the new flags before the world load; thread the cover options into `lodgenBakeTerrainTextures` at **:2955-2965**; **run `lodgenBakeTerrainVt` first**, then call `lodgenVtChunkSheets` for each chunk when `vtBtr` |
| **:2958-2961** | the hard-coded `E:/Tools/Fallout 4/DataUnpacked/Data` default for `dataRoot` stays, but note in the usage text that the panel passes `QString()` and relies on the resource stack instead (`src/lodgenmanager.cpp:1765`) — a live divergence between the two callers that this lane documents and does not fix |
| **new branch** | `--vt` prints the §7.3 estimate to stdout **before any work**, then runs `lodgenBakeTerrainVt` over the whole worldspace (or the region when one is given) and prints a report line **in keyword form**: `vt: levels 5 tiles 12276 present 12276 bytes 1702926720 cover 1 finest 2 compression 0 peakRssBytes <n>` |
| **new branch** | `--vt-estimate` prints the estimate and exits 0; `--lodm-check <path>` parses through `lodmParse` and prints keyword lines |
| **:4422-4501** | `usage()`: document all 15 new flags **and** `--tex-dir`, which is parsed at **:4727** and undocumented today |
| **:4605-4640** | declare the two option structs plus `lgVtDir`, `lgVtBtr = true` |
| **:4720-4740** | parse them, beside `--tex-dir` at **:4727** |
| **:5043-5053** | pass them at the dispatch |
| **:2572-2620** | `--dump-land` — unchanged. It carries **heights only** (header `<iiii>` minX,minY,cellsW,cellsH; presence bytes; 33×33 int16 in units of 8, row 0 south, column 0 west) and is **not** an input to C6, which parses the paint itself |

### `src/lodgenmanager.cpp`

| anchor | change |
|---|---|
| **:291-336** | `LodgenSection` — reused for the VT section, `expandedByDefault = false` |
| **:407-430** | `Form` / `form(24)` — reused |
| **:833-875** | add `LodgenCoverCheck` after **:842** and `LodgenTintSpin` after it |
| **:864-865** | append both to `btrSub` |
| **:866-872** | extend the `sync` lambda to grey `tintSpin` and its label unless `btr && tex && cover` |
| **after :875** | the new `Terrain virtual texture (.lodv)` section: `LodgenVtCheck`, `LodgenVtFinestBox` (through `wwMatchFieldStyle`), `LodgenVtBtrCheck`, `LodgenVtSummary` |
| **:1013-1015** | append `tintSpin` to the `wwMakeScrubField` list |
| **:1020-1030** | hide the VT section under the stock target |
| **:1380-1386** | add `wantVt()` beside `wantTerrainId()` |
| **:1387-1392** | the summary/refusal line gains the three sentences of §7.3; `LodgenVtSummary` is wired to `lodgenVtEstimate` and recomputed on every relevant toggle |
| **:1670-1674** | `vtDir = outputDir()` and `mkpath( vtDir + "/Terrain" )` when `wantVt()` — **not** `outputDir() + "/Terrain"`, because the writer creates `<dir>/Terrain/` itself and the earlier draft's path gave `outputDir()/Terrain/Terrain/` |
| **before the chunk queue starts, near :1690** | run `lodgenBakeTerrainVt` **once, first**. The earlier draft ran it after the queue drained while the per-chunk step read from it — the containers would not exist when `lodgenVtChunkSheets` opened them. Running it first also makes the chunk step nearly free and lets the pyramid replace the dim-8/16/32 paint bakes. |
| **:1745-1800** | the per-chunk step: pass the cover options into `lodgenBakeTerrainTextures` at **:1765**; when `wantVt() && vtBtrCheck`, call `lodgenVtChunkSheets` instead |

### `src/nifskope_ui.cpp`

- **:26821-26829** append `"LodgenCoverCheck"`, `"LodgenTintSpin"`, `"LodgenVtCheck"`, `"LodgenVtFinestBox"`, `"LodgenVtBtrCheck"`, `"LodgenVtSummary"` to the presence inventory.
- **:26919** `numbers >= 8 && plain == 0` — `plain == 0` is the one that will bite; raising the `8` floor is optional and the actual count is logged to `release/ww_lodgen_test.log`.
- **:26928** `groups == 0 && headings >= 4` — the VT section is a `LodgenSection`, not a `QGroupBox`; leave the floor.
- **:26937** `combos >= 5 && unmatched == 0` — `LodgenVtFinestBox` must go through `wwMatchFieldStyle` or `unmatched` rises.
- **:26945-26954** the tooltip/dash check — no new label may contain `" - "`; every new check box gets a tooltip.
- Add greying assertions in the shape of the existing ones (**:26843-26849**, **:26868-26889**): tint greys with cover; VT sub-rows grey with `LodgenVtCheck`; the VT section is hidden under the stock target.

### `src/lodtfile.cpp` / `.h`

**No change.** `SECT_GROUNDCOVER` (**cpp:42**), the `GCVR` table (**cpp:455**), the 8 slots per quadrant and `LodtFile::groundCover` (**h:142**, **cpp:1457-1462**) stay FO76-only, and **cpp:900**'s `g.clear(); // FO4 has no ground cover` stays true — measured, Fallout4.esm has **0 GCVR records**. Add one sentence to that comment naming where FO4 ground cover now lives instead (`_data.DDS` alpha, and the `.lodv` data sheet's alpha), so the next reader does not re-derive it.

### `tests/spells/lod_channel_preview.sh`

Add the cover channel and the two new `cmp -s` refutation pairs (cover against AO, wetness and shore), alongside the existing pairs at **:71** and **:77** which must continue to **fail**. §8.5 records that the feature is not finished until this has printed a named PASS with the game down.

### Docs

- **`docs/LODGEN_VERTEX_PACKING.md`** — fix the self-contradiction (lines 242 and 265-267 vs 250 and 281) to the new truth: BC1 with A unused when the chunk has no cover, BC3 with A = cover and a `'WWCV'` stamp when it has. Add cover to the alpha-candidate table at **:240-290** with the §2.6 argument and its measured `|r|` against R/G/B **and** against the msn.
- **`docs/LODGEN_TERRAIN_VT.md`** — new. §4, §5 and §6 of this spec, verbatim, as the format contract FO4CS will read the way it already reads `docs/LODGEN_VERTEX_PACKING.md` for the `.bto` channels (`FarFieldLodBtoChannels.h`). It must state in those words that **coarse levels are downsamples of fine data, not measurements at that scale**, and carry V8b's measured AO and shore divergence.
- **`docs/LODGEN_IMPOSTOR_SPEC.md`** — add `terrainVT` to the `kind` list and point at the new doc.
- **`docs/MISTAKES.md`** — one entry the moment anything below is recognised, unprompted.
- **`WW_CHANGES.md`** — a `2026-09-06m`-style entry at the top, unprompted, with the measured numbers, not adjectives: the re-baselined identity hashes from the rounding change, the measured per-chunk and per-tile BC3 fractions, and the game-down bake-time measurement with its sample count.
- **`HANDOFF.md`** and **`docs/LODGEN_PLAN.md`** — while here, fix the stale check counts: `LODGEN_PLAN.md:11` says `lodgen_terrain.sh 18/18` and `HANDOFF.md:375` says `14/14`; it is **21 checks** today.

### Build and verification order

Nothing in this list may be built or run while `Fallout4.exe` is up — including the §4.2 bake-time measurement, whose earlier attempt was taken with the game running and is withdrawn for that reason. When it is down: build per the `nifskope-ww-build-verify` skill (make's own exit code, never `grep`'s; the exe is held by his open NifSkope window — rename it aside, never kill it), then run `lodgen_ground_cover.sh` and `lodgen_terrain_vt.sh`, then re-run `lodgen_terrain.sh` (3 min 2 s, 21 checks) and `lodgen_identity.sh`. `lodgen_terrain.sh` must not move. **`lodgen_identity.sh` will move once**, on the `(acc+2)>>2` rounding change, and its new baseline is recorded in `WW_CHANGES.md` in that same commit; it must not move again.

---

## OPEN QUESTIONS FOR THE OWNER

1. **The delivered disk cost is 3.081 GiB (VT on, cover off) or ~4.00 GiB (cover on) against today's 1.495 GiB, because the `.btr` sheets stay on disk alongside the pyramid — is that acceptable, or should `--vt` delete the chunk sheets it can regenerate?** Recommendation: keep both for now (the stock engine needs the `.btr` sheets and there is no VT consumer yet), and revisit when FO4CS can read a `.lodv`.

2. **The pyramid inherits the tinted albedo, so an FO4CS consumer that draws real grass can never recover the untinted ground — do you want the tint at all, or `--grass-tint 0` as the default with the tint reserved for stock-engine bakes?** Recommendation: keep 0.35 as the default, since the stock engine is what ships today and the fork is reversible only by a re-bake.

3. **`COVER_FULL = 96` saturates to 255 across whole regions under your own load order (True Grass reaches ~1,344 on one LTEX) — should the default be raised, or should the bake pick it from the measured maximum and write it into the container?** Recommendation: keep the fixed 96 default and use `--cover-full` after the first census run reports your load order's real maximum; a run-derived constant would destroy cross-run comparability.

4. **The user-visible extension is `.lodv` and the containers are named `Commonwealth.VT.2.lodv` under `Data\Terrain\` — is that the name you want mod users to see in their file lists?** Recommendation: keep it; it sits beside `.lodt` and `.lodm` and refuses to be mistaken for a DDS.

5. **The mip rounding fix (`acc/4` → `(acc+2)>>2`) re-baselines `lodgen_identity.sh` once, changing hashes for every sheet the generator has ever written — do you want that in this lane, or split into its own commit first?** Recommendation: its own commit, landed and verified before TERRAIN1, so a single identity movement is never entangled with a feature.
