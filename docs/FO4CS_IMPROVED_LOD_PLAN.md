# Improved LOD — the FO4CS build plan, written against the files this generator writes today

> **This page is a PLAN, not a contract.** It does not define a byte, a channel
> or a field. Everything it describes reading is defined somewhere else, and
> every claim below names the section it came from. Where this page and a
> contract page disagree, **the contract page wins** and this page is the thing
> that is wrong.
>
> **Improved LOD is ONE FO4CS module with its own master switch, and it ships
> off.** bungo, 2026-09-09 15:34, verbatim: *"This will be a new module for
> Fo4cs, called Improved LOD"*. And 15:35, verbatim: *"Without it enabled, the
> LOD read is gonna remain the vanilla LOD as the fallback. It should work the
> same."*
>
> **It is built LAST.** bungo, 2026-09-09: *"we're building fo4cs last"*. The
> generator side (the `.lodl` / `.lodt` / `.lodo` / `.lodi` / `.lodm` family, the
> card sheets, the texture arrays) is finished first and frozen; this module
> consumes those files and never redefines them. The contract versions and
> hashes it is planned against are in the provenance footer.

Written by lane PLAN-FO4CS, 2026-09-11, in the NifSkope Wild Wasteland tree. The
FO4CS repository was read and not written.

---

## CHANGED SINCE 2026-09-16 (lane PLANSYNC1, 2026-09-23)

bungo asked 2026-09-23 whether this page was up to date for the new LOD. **It was not.** Every item below was re-checked
against the writer source and the contract pages as they stood on 2026-09-23. Where the page was wrong, the old text is
kept ~~struck through~~ with the date beside it. An FO4CS session that already built against this page re-checks these
items first. **The source wins over this page, and the contract pages win over it too.**

**Corrections by kind: 9 version and refusal facts, 16 pinned numbers re-measured or re-labelled, 10 status changes
(rows closed, rulings reversed, things that now exist), 7 wrong statements corrected, 3 citations moved, 9 new owed rows
and 7 new open rulings. 61 in all.**

1. **Versions (§8).**
   * `.lodo` is **v4**, not 3. Versions 1, 2 **and 3** are refused by name.
   * `.lodi` is written as v3 to v9 depending on options, and **a default bake writes v7**, with a 512-byte header.
     * The retired v8 is still read.
     * `--scrappable` writes v9.
     * 1 and 2 are refused by name.
     * The header CRC covers 0x10 up to that version's header size.
   * `.lodl`: a default bake writes v2. v3 is written only with `--water-bodies`. The reader accepts 1 to 3, and row 0 is SOUTH.
   * `.lodt` v2: the reader also accepts role 7 (retired) and up to 10 sheets.
   * `.lodm`, the manifest and the texture arrays sidecar did not change version.
   * §8 gives one line per `.lodo`/`.lodi` bump since v3, each with the rung that reads it.
2. **Authored LODs only, the default since 2026-09-17** (§6 (a), R0, R3, R5, §4.4, §7).
   * The library is the vanilla LOD models (`--library mnam`) and there is **no cluster ladder**: `levelMax` 0, every cluster a root.
   * The distance steps are the four MNAM slots.
   * §6 (a) said near was the default. That ruling was reversed.
   * Every ladder number on this page is now labelled as the opt-in `--native-ladder` bake.
3. **R0.**
   * Gate 1 is re-pinned from the default bake on disk: `.lodo` v4, 6,204,388 B, 10,634 clusters, `levelMax` 0.
   * The complete hard refusal list, read from the two readers, is added.
   * A mismatch **between** the two files is HARD. Only a mismatch against the running game is soft.
   * Gate 3 adds v3 `.lodo`.
   * Gate 5 and the `.lodl` v1 question are open, §6 (r).
4. **R1.**
   * The far-shadow key is the GROUP (`.lodi` v7), not the instance index. `shadowIdentityUnique` now means unique per group.
   * New rule: mirror U about the shape's or the material's own U range, never the whole mesh's.
   * The per-source caster count is now on the census page.
5. **R2.**
   * The §8.5 clipmap ruling is folded in as the design.
   * A default tile is **138,720 / 184,960 B**, not 323,680 / 369,920; the larger pair is for `--vt-height` bakes only.
   * The tiling is **T = 341.3333**, not 2048. The sampler defaults changed on 2026-09-12.
   * VT 2.5 now has eight steps, not seven.
   * The AO march stops at 1,458 units.
   * The first Commonwealth pyramid was written today by VTBAKE1. Its validation is still running.
6. **R4.**
   * The card law as the generator writes it today: grid, frame ladder, aspect, `--card-half-aux`, sheets, families and `conv`.
   * The 22.5-degree ring ruling is recorded (held).
   * Sway is in the normal sheet's alpha, not in the height.
   * Aggregate cards SHIPPED on 2026-09-11.
   * The coverage-cut domain conflicts between this page and the viewer. That is open.
   * The slider's middle arm has now been measured.
   * The gate fixture must be a set baked after 2026-09-19.
7. **§4 census.**
   * 63 fields, not 60.
   * CENSUS 6.2 has 16 checks, not 13.
   * The checker's 59/0/31 was measured on v3 pairs that today's decoder refuses.
8. **§5.**
   * Rows 4, 8, 9, 10 and 17 are closed. Row 12 is half closed.
   * Row 6 has a code fix that no run has verified yet.
   * Row 14 is PENDING on VTBAKE1.
   * Rows 20-28 are new: IMPOSTORPBRM1, IMPOSTORWIND1, the card re-bake, the unstored MNAM slot, the census `version` word, the checker re-run, a default Sanctuary pair, the `.lodt` sheet rule and the `cardCount` recount.
9. **§6.** (a) is rewritten. The open rulings (o)-(u) are new: the card grid and cut defaults, the cut domain, card lighting and layout, `.lodl` v1, the `.lodt` sheet rule, `cardCorpusHash`, and the MNAM slot.
10. **§8.3.** The viewer impostor preview exists: IMPOSTORSHOW and the lanes after it, from 2026-09-19.
11. **Contract-page defects** found on the way are NOT fixed here, because this lane edits no other page. They are listed in `scratchpad/plansync1_20260923/DELIVERABLE_TEXT.md`.

---

## 0. How to read this page

* **§1** is the scope: what the module reads, what it draws, what it suppresses.
* **§2** is the work, as six rungs. Each rung is one FO4CS wave with its own
  flight, and each is shippable on its own.
* **§3** is the seams — the places two rungs meet and a user sees a line.
* **§4** is the census, which is how any of this is known to work.
* **§5** is what the generator still owes, so an FO4CS session never discovers a
  missing field in the middle of a wave.
* **§6** is every open question, in one list, for bungo.
* **§7** is the glossary, because four different things in this system are
  called a "level" and two different things are called a "mask".
* **§8** is the provenance footer.

Citations use the same page keys `docs/LODGEN_CENSUS.md` uses (§1.3 there):

| key | page |
|---|---|
| `NATIVE` | `docs/LODGEN_NATIVE_LODO_LODI.md` |
| `VT` | `docs/LODGEN_TERRAIN_VT.md` |
| `BTD` | `docs/LODGEN_BTD_FORMAT.md` (the `.lodl`) |
| `CARDS` | `docs/LODGEN_CARD_SHEETS.md` |
| `ARRAYS` | `docs/LODGEN_TEXTURE_ARRAYS.md` |
| `MANIFEST` | `docs/LODGEN_MANIFEST_FORMAT.md` |
| `LODM` | `docs/LODGEN_LODM_FORMAT.md` |
| `CENSUS` | `docs/LODGEN_CENSUS.md` |

---

## 1. Scope, and the one rule

### 1.1 The one rule

**The module is built against FROZEN contracts and consumes them. It never
redefines a file.** A rung that finds a file cannot answer its question does not
change the file; it names the gap, and the gap goes in §5 as something a
GENERATOR lane owes. That is the whole reason this page exists before the code
does.

The second half of the rule is bungo's 15:35 ruling: **master off is bit-exact
vanilla.** With `[ImprovedLOD] bEnabled = 0` no hook fires, no file is opened,
the engine's own LOD tree is never hidden, and the frame is the stock one. The
module may never make the vanilla path depend on it. Its gate is the same
altered-capture control the shadow work uses (`fo4cs-altered-capture` §2): the
far field with the module off is identical to the pre-module build against a
measured noise floor.

### 1.2 What the module reads

EVERY PATH IN THIS TABLE MOVED ON 2026-09-16 (lane LAYOUT1). bungo, 19:3x: "The
folder should be called FO4CSLOD maybe, so it'd be Data/FO4CSLOD, sound fine?" --
so the generator now writes every FO4CS-target file under one root,
`Data\FO4CSLOD\`, with a folder per worldspace and the per-tree impostor cards at
`Data\FO4CSLOD\Cards\`. The shadow HeightMap is the one exception and is still at
`Data\Textures\Terrain\<WS>\`, because a SHIPPED FO4CS reader composes that path;
moving it is a later FO4CS change and bungo's call. **The module's own path
composition is owed this change** -- it is the reader half of the same ruling.

| file | what it is | defined by | on disk today? |
|---|---|---|---|
| `Data\FO4CSLOD\<WS>\<WS>.lodo` | the object geometry LIBRARY: bases, meshes, clusters, the cluster ladder, materials, a local-index blob, a vertex blob | NATIVE 1, NATIVE 3 | **yes** — ~~9,657,316 B for Sanctuary (NATIVE 3, STATUS block)~~ (the MNAM-plus-ladder bake of 2026-09-11; struck 2026-09-23) **6,204,388 B, `.lodo` v4, on today's authored-only default** (bake of 2026-09-19 18:15, `scratchpad/cellview2b_20260919/lodibake/nat`) |
| `Data\FO4CSLOD\<WS>\<WS>.lodi` | the INSTANCE tables: chunk directory, cell ranges, 24-byte instance records, an 8-byte cold record, occluder boxes | NATIVE 1, NATIVE 4 | **yes** — ~~128,256 B, 3,526 placements~~ (a v3 file of 2026-09-11; struck 2026-09-23). A default bake writes **v7**. The newest default pair on disk is one chunk (4,-20,4): 778 placements, 130,666 B. **No default-settings Sanctuary `.lodi` exists on disk** (§5 row 26) |
| `Data\FO4CSLOD\<WS>\<WS>.lodl` | the whole-worldspace LANDSCAPE file: heights, per-texel LTEX opacities, per-cell water, AO, and (v3) water bodies | BTD Header, BTD Blocks | **yes**. ~~and FO4CS already reads it~~ (struck 2026-09-23) FO4CS's shipped parser reads only a v1 file at the OLD name and folder, `Data\Terrain\<EDID>.lodt`, so it reads no file a bake writes today — see §1.5 |
| `Data\FO4CSLOD\<EDID>\<EDID>.VT.<dim>.lodt` | one terrain-texture PYRAMID container per level: colour, model-space normal, mask, optional height, optional emissive | VT 3.1, VT 2.2 | ~~**no** — VT 7: nothing has ever been written to disk~~ (struck 2026-09-23) **The first whole-Commonwealth pyramid was written 2026-09-23 08:17 by lane VTBAKE1**, into `scratchpad/vtbake1_20260923/bake/FO4CSLOD/Commonwealth/`: 5 levels, 12,276 tiles, about 4.0 GB, height sheet ON, cover OFF. Its validation is still running, and it is not in any mod folder |
| `Data\FO4CSLOD\<EDID>\<EDID>.VT.lodm` | the pyramid INDEX: which containers, which sheets, the corpus hashes, the cover normalisation | VT 4, LODM 5 | ~~**no** — VT 7~~ (struck 2026-09-23) yes, from the same VTBAKE1 bake: `Commonwealth.VT.lodm`, 2,505 B. VT 4 still prints the old `Data\Terrain\` path. The writer uses this one |
| `Data\FO4CSLOD\Cards\<formid8hex>_oct*.DDS` + `.lodm` | per-base octahedral impostor CARD sets | CARDS 1.1, LODM 3 | four libraries of 19 Sanctuary trees exist (CARDS 9). **All four were baked 2026-09-09, before the orthographic camera, the coverage contract and the 2026-09-19 azimuth repair, so every one is owed a re-bake** (HANDOFF 2026-09-19 09:5x; §5 row 22; found 2026-09-23); **no worldspace bake has ever written a card layer** — NATIVE 11, Deviation 5 |
| `<ws>.LodgenCards.<family>.<SW>x<SH>_*.DDS` + `.lodm` | CARD ARRAYS, one per (family, sheet size) | CARDS 1.2, LODM 4 | no |
| `<ws>.LodgenArrays[PBR].<W>x<H>_*.DDS` + `.lodm` | mesh LOD TEXTURE ARRAYS, one per (family, texture size class) | ARRAYS 1, LODM 4 | **no** — ARRAYS 10 |
| `<chunk>.bto.manifest.txt` | the per-placement manifest beside each stock `.BTO` | MANIFEST 1 | v2 has never been written to disk — MANIFEST 10 |

Two of those the module reads **only while its stock-fallback arm is serving**:
the `.bto` chunks and their manifests. Everything else it reads on its native
arms.

### 1.3 What the module draws

Far OBJECTS, from the `.lodo`/`.lodi` pair, as instanced indirect draws that
**create no vertex or index buffer at all** (NATIVE 7 steps 2 and 5: three
StructuredBuffers, `DrawInstancedIndirect` per bucket, no IA, no input layout,
no index buffer), seated at `DeferredPrePass_Post` (NATIVE 7 step 5).

Far TERRAIN, hybrid by band (bungo 2026-09-11 09:2x, *"Your solution sounds
good"*): the band touching the loaded 5x5 cell grid is blended at RUNTIME from
the `.lodl`'s per-texel LTEX opacities so the loaded-cell edge is seamless; the
band beyond it samples the `.lodt` pyramid; the two cross-fade across the inner
band. The runtime's blend formula is stated ONCE, in VT 2.5, and the generator
already gates itself against an independent implementation of it.

Impostor CARDS for trees, from the card sheets or the card arrays.

FAR SHADOWS, which are a second complete consumer of the same data and are
counted separately (NATIVE 7 step 8, CENSUS 5.3).

### 1.4 What the module suppresses

bungo, 2026-09-11 10:0x, on the engine drawing the same ground twice, verbatim:
*"This will be solved through FO4CS, vanilla LOD will be suppressed"*. So:

* the engine's far OBJECT tree (`spLODObjectRoot`) is hidden — but **only after
  a non-zero drawn-primitive count has been read back for one frame** (NATIVE 7
  step 9). The census field `drawnPrimitives` is that read-back and
  `engineFarHidden` is the claim it gates (CENSUS 5.4).
* the engine's far TERRAIN for the chunks the module serves, and only those.
* per-object, inside the loaded grid, once R5 lands — the join is
  `cold[i].refFormId` at the instance's own index (NATIVE 4.1a).

**The bake emits NO stock override chunks** (bungo's same ruling): suppression is
entirely a runtime act.

**Cell streaming stays the engine's**, at every rung. That is the founding
principle of the whole campaign (the hybrid-LOD memory: leave residency to
Bethesda, re-decide only the rendering spend) and bungo restated it on
2026-09-11 10:5x with the grid-edge items.

### 1.5 What FO4CS already has, today

Not a plan item — the state a first lane starts from, read out of
`E:\Projects\Fo4CommunityShaders\wt-fixfirst` on 2026-09-11:

* `src/FarField/FarFieldLodtFormat.h` + `FarFieldLodtSource.h` read the `.lodl`
  as the far-field heightmap source, gated by `tests/lodt_format_tests.cpp`.
  **Its `kVersion` is `1u` and it refuses anything else** (BTD, the "A CONSUMER
  EXISTS" block). The writer here defaults to version 2, so a freshly generated
  file is refused by the shipped parser until that lane teaches it 2 and 3. The
  zero-effort way back needs no rebuild on our side: `WW_LODL_VERSION=1`.
  **Re-checked 2026-09-23:**
  * It is still `kVersion = 1u` in every worktree that carries it.
  * It composes the OLD path `Data\Terrain\<EDID>.lodt` (`FarFieldLodtSource.h`, `kLodtDirPrefix`). That is the old
    folder AND the old extension, and the extension now names the LDTX texture sheets.
  * Its one caller is the far-shadow heightmap source, not an Improved LOD module.
  * A default bake writes `.lodl` v2. It writes v3 only with `--water-bodies`.
  * Lane IMPROVEDLOD-R0 (worktree `wt-lod0`) went live on 2026-09-23 and is at the design stage, with no loader code yet.
* `src/FarField/FarFieldLodBtoChannels.h` decodes the packed `.bto` channels —
  identity in vertex colour R+G, AO in B, sway in A, sky visibility in UV2.x,
  ground contact in Eye Data — **off the vertex descriptor and never off a
  remembered offset**. That header is the existing home of the identity the far
  shadows key on, and R1 is where it gains a second source.
* `src/FeatureModule.h` is the module base: `Name()`, `Category()`,
  `LoadSettings` / `SaveSettings`, `DrawSettings()`, an optional
  `ModuleDescriptor`, and a `bool enabled`. `[FarFieldShadows]` is the worked
  example of a whole INI section introduced at once with every key inert until
  the stage that reads it.

---

## 2. The rungs

Six rungs, in the order the far field becomes visible and measurable. Each is
one wave (`fo4cs-wave-integrate`) with its own flight (`fo4cs-flight-brief`).

Every rung states nine things: **reads / does / switch and keys / fallback and
its arm word / census / gates / flight / must not / RE candidates.**

**On the keys.** The END menu is end-user controls only and no row lands without
bungo. The module's rows are therefore its master switch, plus the four
screen-size fade thresholds bungo asked for by name on 2026-09-11 10:4x
(*"one threshold per engine fade class, default ~1 percent, a menu row + INI key
each"*). **Everything else is INI-only policy**, and every key below is a
PROPOSAL that has not been ruled on unless this page says bungo named it.

---

### R0 — The loaders, and staleness. Nothing is drawn.

**READS.** Both headers and both index CRCs of the `.lodo`/`.lodi` pair
(NATIVE 3, NATIVE 4). **The `.lodi` header is 256 bytes on v3-v6 and 512 bytes on
v7, v8 and v9, and its `headerCrc32` covers 0x10 up to that version's header size**
(`src/lodifile.h`, `lodiHeaderBytes`; added 2026-09-23); the `.lodl` header (BTD Header); the pyramid's `.lodm`
index and the container headers it names (VT 4, VT 3.1); the card and array
`.lodm` envelopes (LODM 1, LODM 1.1).

**DOES.** Validates and classifies. NATIVE 5 splits the keys and the split is not
optional:

* **hard** — magic, version (~~1 and 2 refused BY NAME~~, struck 2026-09-23: **`.lodo`
  accepts 4 only and refuses 1, 2 and 3 by name; `.lodi` accepts 3 to 9, including the
  retired 8 that nothing writes any more, and refuses 1 and 2 by name**), `vertexStride`,
  `instanceStride`, `clusterMaxTris`, `clusterLodStride`, `occluderStride`, a set
  reserved bit, `ROW_ORDER_NORTH_UP` clear, `chunkCount` over cap, a zero
  `lodoIdentity` without `NOLIB`, a `scale` of 0, a `drawKey` out of order, a
  cluster whose `geometricError` exceeds its `parentError`, a `CONE_OPEN` cluster
  carrying a cone, an occluder naming an instance outside its own cell, any CRC
  mismatch → **refuse to load, and never hide the engine's own LOD tree.**
* **soft** — `pluginCorpusHash`, `objectCorpusHash`, `modelCorpusHash`,
  `cardCorpusHash`, `loadOrderHash` → **load anyway, log it, raise `stale`, keep
  rendering.** Three of those are hashes of the user's own data and move the
  first time any mod is installed after a bake.

**ADDED 2026-09-23 (PLANSYNC1): the complete hard list, read out of the two readers.** The sources are
`src/lodofile.cpp` `lodoRead`, `src/lodifile.cpp` `lodiRead`, and the pairing check in `src/nativeemit.cpp`.
The list above is NATIVE 5's list from 2026-09-11. The readers enforce everything below, and NATIVE 5 does not
list it yet (that is a finding against that page).

* **Pairing is HARD.** The two files must agree on:
  * the worldspace;
  * `pluginCorpusHash` and `objectCorpusHash`;
  * `loadOrderHash`;
  * `lodoIdentity`, unless `NOLIB` is set.

  A mismatch between them refuses. Only a mismatch against the RUNNING game is in the soft class.
  NATIVE 4 row 0x90 and NATIVE 5 disagree on this, and the pairing check in the source settles it.
* **`.lodo`:**
  * **Header:** the header CRC; flags bit 0 clear; reserved flag bits and reserved bytes (0xAC, 0xCE-0xCF,
    0xD4-0xFF).
  * **Fixed values:** `clusterMaxTris` 16, `vertexStride` 16, `clusterLodStride` 48; `levelMax` at most 15.
  * **Header consistency:** the `LADDER` flag must agree with `ladderGroup` and `levelMax`; `cardCount` must not
    exceed `baseCount`.
  * **Payload:** alignment, order, bounds, padding and the index CRC.
  * **Mesh rows:** counts recounted; sort order; no flags beyond ALPHA / SWAY / WATERTIGHT.
  * **Cluster rows:**
    * 1-48 vertices and 1-16 triangles; the size class; index ranges;
    * sorted by mesh, material and level;
    * a consistent parent chain, with `geometricError <= parentError` and a level-0 error of 0;
    * a `CONE_OPEN` cluster never carries a cone.
  * **Material rows:** layer 2048..0xFFFE; family; sort order.
  * **Base rows:** sort order; `fullTriangles` recounted from the clusters.
* **`.lodi`:**
  * **Header:** the header size for the version; the header CRC; `ROW_ORDER_NORTH_UP` must be set;
    `lodoIdentity` must agree with `NOLIB`; `chunkCells` 4; strides 24 and 40.
  * **Per version:**
    * v5: the placement-AO stride and count; `slotInstances` must sum to `instanceCount`.
    * v6: the vertex-AO offsets.
    * v7 and v9: the group table (stride 2, ids dense per chunk, `groupCount` equal to the sum), and the
      sky slice must match the AO slice.
    * v8: its horizon stream.
    * A lower version must not carry a higher version's header words, and the reserved bytes are checked
      version by version.
  * **Aggregate rows** are checked.
  * **Chunks:** the chunk and cell partition, and the chunk CRCs.
  * **Occluder rows:** each one inside its own cell.
  * **Instances:**
    * no flags beyond 0x7F;
    * **no bit 6 (scrappable) below v9**;
    * no scale of 0;
    * the cell-quantisation ambiguity band (`lodiCellAgrees`);
    * sorted by cell, `drawKey`, ref and part;
    * `drawKey` must be the base's rank.
* ~~**The writer's own reader has one gap, and R0 should close it:** `cardCount` is bounded but never RECOUNTED.~~
  **Closed 2026-09-24 (lane CARDLINK1):** the writer's reader now recounts it (§5 row 28). R0 should recount it too.

The full list, with a line anchor for each rule, is in part 4 of `scratchpad/plansync1_20260923/audit_native.md`.

The `.lodt` reader has its own 22-rule refusal list (VT 3.4) and a **version 1
file is named for what it is**, not lumped into "bad version".

**Added 2026-09-23:**
* **`.lodt` sheets.** The tree's reader also ACCEPTS up to 10 sheets and role 7, the retired horizon sheet
  (at most 4, RGBA8, placed last). VT 3.4 rule 13 still says 1-6 sheets and refuses any role above 6. Nothing
  writes role 7 any more. Which rule the FO4CS loader follows is open, §6 (s).
* **`.lodt` tile size.** A tile's byte size comes from the header's sheet list plus the tile's cover bit
  (`lodvTileRawBytes`), never from a constant.
* **`.lodl` versions and row order.** The `.lodl` reader accepts versions 1, 2 and 3, and **row 0 is SOUTH in
  every grid** (BTD, "ROW 0 IS SOUTH"), so a north-up reader mirrors Y.
* **`.lodl` v1 is not ruled.** It is not ruled whether the new module refuses a v1 `.lodl`. Our own reader
  accepts it, and `WW_LODL_VERSION=1` is the documented way back (§6 (r)).

**SWITCH AND KEYS.**

| key | kind | default | why |
|---|---|---|---|
| `[ImprovedLOD] bEnabled` | **the one END-menu row**, master | **0** | bungo 2026-09-09 15:34 / 15:35 |
| `bStaleRefuses` | INI-only policy | 0 | turns NATIVE 5's soft class into a refusal, for testing only |
| `iCensusIntervalMs` | INI-only policy | 5000 | the frame-window `frames` and `windowMs` are measured over (CENSUS 2) |

**FALLBACK, AND THE ARM WORD.** Four independently switchable consumers and
therefore four arm words, all in the `[ImprovedLOD]` row (CENSUS 2):
`arm.objects`, `arm.terrain`, `arm.cards`, `arm.arrays`, each reading `native` /
`stock-fallback` / `off`, with a refusal vocabulary per arm (`no_lodo`,
`no_lodi`, `no_lodm`, `no_lodt`, `no_sheet`, `no_layer`, `no_array`, `no_uv2`,
`refused_hard`, `no_worldspace`). At R0 every arm reads `off` or a refusal by
design, because nothing draws yet.

**CENSUS.** The whole `[ImprovedLOD]` row: `arm.objects`, `arm.terrain`,
`arm.cards`, `arm.arrays`, `worldspace`, `version`, `pairing`, `stale`,
`staleFiles`, `frames`, `windowMs` (CENSUS 2). `staleFiles` is a LIST, not a
count, because a count alone cannot be acted on.

**GATES**, pre-registered from the contracts:

1. The pair on disk loads and its header numbers are reproduced exactly.
   ~~`.lodo` **9,657,316 B**, 2,970 bases, 2,982 meshes, **20,678 clusters**,
   `levelMax` **7**, 4,715 root clusters covering **142,138** level-0 triangles;
   `.lodi` **128,256 B**, **3,526** placements, **10 present chunks of 12 dense**
   (NATIVE 1, NATIVE 3.5.3, NATIVE 6).~~
   **Struck 2026-09-23:** that was a `.lodo` v3 / `.lodi` v3 pair on the MNAM-plus-ladder library of 2026-09-11,
   and today's reader refuses both files by name.
   **Re-pinned 2026-09-23 from the default (authored-only) bake on disk.** The pair is
   `scratchpad/cellview2b_20260919/lodibake/nat`, from 2026-09-19 18:15, decoded read-only by
   `tests/spells/lodgen_native_decode.py` with 6 checks and 0 failures:
   * `.lodo`: **v4, 6,204,388 B**, flags 0x5 (LADDER clear), 2,970 bases, 2,982 meshes, 136 materials,
     **10,634 clusters**, 273,695 vertices, `levelMax` **0**, every cluster a root, covering **142,138**
     level-0 triangles.
   * `.lodi`: **v7, 130,666 B**, one chunk (4,-20,4), **778** placements, 3 occluders.

   The Sanctuary numbers, 3,526 placements and 10 of 12 chunks, describe the placements and do not depend
   on the library. **But no default-settings Sanctuary pair exists on disk**, so the Sanctuary `.lodi` size
   gate needs a re-bake first (§5 row 26).
2. `loadOrderHash` reads **0xa056a596e2bb16e7** on a load order of
   `Fallout4.esm` alone (NATIVE 8.1), and moves when a plugin is added.
3. A v2 `.lodo` and a v2 `.lodi` are each refused **by name** (NATIVE 0), and so
   is a v1 `.lodt` (VT 3.4 rule 3). **Added 2026-09-23:** a v3 `.lodo` is refused by
   name too (NATIVE 11, Deviation 14). The v3 pair of 2026-09-11 (`native1b_20260911`)
   is exactly such a pair, so it is this gate's refusal fixture and no longer gate 1's.
4. `occluderCount` reads **0** on the Sanctuary pair and **280** on the
   downtown-Boston pair (NATIVE 4.5.3) — and `ringCulledOccluder` must refuse
   `no_boxes` on the first, never print 0 (CENSUS 3).
5. The `.lodl` parser accepts version 2 and version 3 (BTD Header) — today's
   FO4CS parser pins 1 and refuses both. **Added 2026-09-23:** a default bake writes v2.
   Whether the new module also accepts v1 is open, §6 (r). Our reader accepts it, and
   `WW_LODL_VERSION=1` is the documented way back.
6. Master off: an altered capture of the far field is identical to the
   pre-module build against a measured noise floor (`fo4cs-altered-capture` §2).

**FLIGHT.** One step. Launch, stand anywhere outdoors, send the log. The pass
sentence: *"does the log carry an `[ImprovedLOD]` line naming a worldspace, and
do all four arm words read `off` rather than a blank?"* Refuter: a row of
`unwired` defaults means the module registered but nothing assigned the fields —
the plumbing accusing itself, which is what those defaults are for (CENSUS 1.2
rule 5). **Nothing changes on screen at this rung, and the flight brief says so.**

**MUST NOT.** Open a file, install a hook or touch the engine's LOD tree while
`bEnabled = 0`. Print `0` where a refusal word exists. Treat a soft hash
mismatch as a refusal.

**RE CANDIDATES.** None. R0 is host code.

---

### R1 — Vanilla far objects suppressed, the native far field drawn, and one shadow representation per placement

**READS.** The whole `.lodo` library and `.lodi` instance tables (NATIVE 3,
NATIVE 4); the cold blob's `refFormId` and `identity` (NATIVE 4.1); the chunk
directory's `maxBoundRadius` (NATIVE 4); the mesh texture arrays if they exist,
else the stock atlas (ARRAYS 1, ARRAYS 6).

**DOES.**

1. Uploads three StructuredBuffers and derives `chunkIndex[instanceCount]` by
   walking the chunk table once — it is not in the file, and it is 636 KiB of
   VRAM against zero bytes of disk (NATIVE 7 step 3).
2. Culls in one Dispatch over all instances: frustum against
   `base.boundRadius x scale`, each chunk box expanded by its own
   `maxBoundRadius` (NATIVE 7 step 4). **`maxBoundRadius` is load-bearing** — the
   chunk box is built from instance ORIGINS, so an 8,224-unit tree's bound leaves
   the box and a naive per-chunk cull would pop it (NATIVE 4).
3. Draws `DrawInstancedIndirect` per bucket, with **three vertex-count classes
   12 / 24 / 48** chosen by the cluster's `flags` bits 0-1 (NATIVE 7 step 6).
   Nothing branches on ladder level.
4. Writes the identity the far-shadow pass keys on into the G-buffer: the GROUP id
   on a v7 or v9 `.lodi`, §9.
   **Added 2026-09-23:**
   * **U mirror.** An instance with flag bit 0 `MIRRORED` (the tree repetition breaker) mirrors U **about the
     shape's or the material's own U range, never the whole mesh's.** The NifSkope viewer once mirrored about
     the mesh and threw TreeMapleForest2's branch cards onto the bark half of its atlas (WW_CHANGES 2026-09-17,
     the viewer hotfix).
   * **AO and sky.** A default `.lodi` carries per-placement AO (v5) and per-vertex AO and sky (v6, v7).
   * **Matching the vertex slices.** The file does not store which MNAM slot an instance drew, so the vertex
     slices are matched to a mesh by LENGTH (§5 row 23).
5. Suppresses the engine's far object tree, **after** the primitive read-back.
6. **Shadows: one representation per placement.** RULINGS bungo 2026-09-11
   14:4x -> 15:0x, from his question *"the LODs won't conflict with shadow
   casters? Especially cascaded and far shadows ... far shadows falling on me
   from behind, from objects a few kilometers away at low sun angle"*:
   * **every placement has exactly ONE shadow representation at a time, chosen
     the same way its draw is chosen.** Cells inside the loaded grid drop out of
     the far-shadow casters by **the same cell-range table** that drops their
     draws — the 8-byte `(instanceFirst, instanceCount)` row per cell
     (NATIVE 4), which is exactly what that blob exists for (NATIVE 2.1).
   * **the cascades never contain LOD geometry.**
   * **the grid-edge band blends shadows the way it blends draws** (see §3).
   * **the census counts casters per source.**
   * **far casters at low sun come ONLY from the far-shadow pass**, which sees
     the whole world the light sees. Self-shadowing stays excluded by identity,
     per his 08:4x ruling: *"Our shadow casters are based on the color id right
     now I think, so they can only occlude other objects and terrain, never
     themselves since that causes visual issues"*.

**THE IDENTITY, AND THE TRAP IN IT.** ~~NATIVE 4.1c says there are TWO identities~~
Struck 2026-09-23: NATIVE 4.1c now names THREE identity words, and **the far-shadow caster identity is the
GROUP**, `.lodi` v7's group table (NATIVE 4.9). §9 is the law for it. The two words below are still true of
what they are, and the difference decides whether a shadow is wrong:

* the format's own per-placement identity is the **instance INDEX**, a u32,
  unique across the whole file by construction;
* `cold[i].identity` is the **STOCK bake's** index for that placement — the
  manifest's `index` column, R + G·256 of the `.bto` vertex colour — and it is
  unique only inside the STOCK CHUNK the placement was first drawn in. Measured:
  **7 of 3,526 Sanctuary instances collide** inside a `.lodi` chunk.

So ~~the module keys its far shadows on the instance index, and~~ (struck 2026-09-23:
the far-shadow key is the GROUP id, §9) the module carries
`cold.identity` only to join an instance to the engine's own far chunk. The stock
16-bit channel wraps at 65,536 placements a chunk with a measured headroom of
only **1.54x** on the two densest dim-32 chunks (NATIVE 4.1c).

**SWITCH AND KEYS.**

| key | kind | default | why |
|---|---|---|---|
| `bDrawNativeObjects` | INI-only | 1 | the exact way back inside the module |
| `bSuppressEngineFarObjects` | INI-only | 1 | inert until `drawnPrimitives` is non-zero (NATIVE 7 step 9) |
| `bFarShadowsFromNative` | INI-only | 1 | his 14:4x ruling; 0 leaves the far shadows entirely to the existing path |
| `bCascadesExcludeLod` | INI-only | 1 | his 14:4x ruling, second clause |

**FALLBACK, AND THE ARM WORD.** `arm.objects` = `native` / `stock-fallback` /
`off`. **The fallback is per CHUNK, not per worldspace**: a chunk the module does
not serve keeps the engine's own `.bto`. Rename the `.lodo` aside and reload —
the word must read `stock-fallback` in the next window (CENSUS 2). A hard refusal
falls to `stock-fallback` and the engine's tree is never hidden.

**CENSUS.** The `[ImprovedLOD.Ring]` row in full: `ringEdges`, `ringInstances`,
`ringCulledFrustum`, `ringCulledOccluder`, `ringCulledScreen`, `ringDrawn`,
`ringTriangles`, `instancesTotal`, `trianglesTotal` (CENSUS 3), under the law

```
ringInstances[r] == ringCulledFrustum[r] + ringCulledOccluder[r]
                  + ringCulledScreen[r]  + ringDrawn[r]        for every r
```

plus `draws`, `overflow` (CENSUS 4), `drawnPrimitives`, `engineFarHidden`
(CENSUS 5.4), and from `[ImprovedLOD.Shadow]`: `shadowDraws`,
`shadowIdentityUnique` (CENSUS 5.3). ~~**This rung adds one census word the page
does not yet carry** — a per-SOURCE caster count, which is his 14:4x ruling's
own instrument; it is named in §5 as owed to the census page.~~ Struck 2026-09-23:
the census page now carries it. R1 fills `shadowCasters[src]` (CENSUS 5.3), checked
against the bake's `native-casters:` line (CENSUS 6.1, 6.2).

**GATES.**

1. `instancesTotal <= instanceCount` always, and **equal** when the whole
   worldspace is loaded (CENSUS 6.2). On the Sanctuary pair `instanceCount` is
   **3,526**, which is the stock manifests' placement count exactly, with
   **0 dropped** (NATIVE 9).
2. The four-way sum law of CENSUS 3 holds every frame in every bin.
3. `draws` reads **8-14 mesh buckets plus 1-2 card buckets** for a whole
   worldspace, against 343-1,795 resident engine draws (NATIVE 7 step 7).
4. `shadowIdentityUnique` reads 1. ~~Doctoring a duplicate identity into the
   `.lodi` makes it read 0.~~ Struck 2026-09-23, because the field has meant unique per
   GROUP since 2026-09-18: doctoring two different groups onto one group id makes it
   read 0. On a v3-v6 `.lodi` it falls back to the instance index and says so (CENSUS 5.3).
5. `engineFarHidden` may only become 1 after `drawnPrimitives` was non-zero, and
   a build where the read-back stays 0 must never hide the tree (CENSUS 5.4).
6. **The altered-capture picture.** The shipped frame and the R1 frame from the
   same camera, at a low sun, with the per-region table
   `fo4cs-altered-capture` §3 requires, and the control taken FIRST against a
   measured noise floor. A control that misses by more than a few pixels is the
   wrong permutation, not a noise floor (`fo4cs-altered-capture` §3d).
7. **No double shadow.** In the same capture, a placement inside the loaded grid
   appears in exactly one of the two caster sets, and the per-source caster
   counts sum to the placements considered.

**FLIGHT.** Restart first, the DLL changed.

1. **The step that decides the most:** stand outdoors where the far field fills
   the screen and tick the module on. *"Does the distant landscape still show its
   objects, and do they sit where they did?"* Refuter: objects present but
   shifted means the position decode or the chunk box is wrong, not the draw.
2. Turn until the far field is behind you. *"Do `ringCulledFrustum` and
   `ringDrawn` roughly swap in the log?"*
3. Low sun, look away from it, at a distant tower. *"Is there ONE shadow from it,
   not two?"* Refuter: two shadows of one object means the cascades still contain
   LOD geometry — read the per-source caster counts before blaming the far pass.
4. Send the log and one `.rdc` named `LodOn.rdc`, and a second one turn later at
   the same spot named `LodOff.rdc`.

**MUST NOT.** Alter cell streaming. Select geometry by RING — the measured 172x
spread of bound heights inside one chunk (47.8 to 8,224.1 units, median 1,086.4)
makes a per-chunk distance wrong by two orders of magnitude for most of its
contents (NATIVE 4.4). Hide the engine's tree on a hard refusal. Build a vertex
or index buffer (NATIVE 7). Read a `drawKey` as anything but a sort check — the
rank itself is re-derivable from the library and both directions are already
gated (NATIVE 2.1).

**RE CANDIDATES, named as candidates and not as facts.**

* The site that hides `spLODObjectRoot`, and whether hiding it also stops the
  engine's own shadow submission for that tree.
* Which channel the far-shadow pass reads its identity from today, and its width.
  `FarFieldLodBtoChannels.h` decodes 16 bits out of vertex colour R+G; the native
  identity is a u32, so either the pass widens or the module supplies a
  chunk-local 16-bit index and accepts the wrap the format was built to escape.
* The engine call that admits a caster to a cascade, for `bCascadesExcludeLod`.

---

### R2 — Terrain: the pyramid beyond, the runtime blend in the inner band

**READS.** The `.lodt` pyramid: header, tile table, tile payloads (VT 3.1, 3.2,
3.3) through its `.lodm` index (VT 4). The `.lodl` for the inner band's LTEX
opacities, heights and water planes (BTD Blocks, BTD Tables). The `.lodl`'s row 0 is
SOUTH. **The pyramid is read through camera-centred CLIPMAP RINGS, and the terrain
geometry is the quadtree over the `.lodl`**: RULED 2026-09-23 08:0x (§8.5), folded in
here the same day.

**DOES.** This is the ruled design (§8.5). bungo, 2026-09-23 08:0x, verbatim: *"Let's do clipmaps then
for textures"*.

0. **Far-terrain colour, normal and mask are sampled from camera-centred clipmap rings.**
   * Each level has a fixed-size window, updated in toroidal strips as the camera moves. The ring is chosen
     by distance, and rings blend at their edges.
   * The rings are fed from the `.lodt` pyramid: an aligned tile grid, with `worldUnitsPerTile` and
     `contentTexels` stated per level, and the height sheet when it was baked. Nothing camera-relative is
     baked (VT 6, "Nothing clipmap-specific").
   * **Geometry stays the quadtree over the `.lodl`** (§8, §8.4), with the CDLOD geomorph and stitched seams.
     The rings are sampled by WORLD POSITION, never by chunk.
   * **Not chosen:** the ring count, the window size, the per-sheet formats, and the inner-band cross-fade
     width.
1. Streams pyramid tiles into the rings at a fixed byte budget. ~~A tile is **323,680 bytes**
   without ground cover and **369,920** with it~~ (struck 2026-09-23: those are tiles WITH the
   opt-in height sheet). A default tile is **138,720 bytes** without ground cover and **184,960**
   with it, and 323,680 / 369,920 on a `--vt-height` bake, at content 256 / border 8 / 2 mips
   (VT 3.3, VT 2.2). **Size a tile from the header's sheet list and the tile's cover bit,
   never from a constant.**
2. Samples the pyramid from the band beyond the inner one outward.
3. **Blends the inner band at runtime from the `.lodl` weights**, by the ~~seven~~ **eight**
   (2026-09-23) steps VT 2.5 states once: cell and quadrant, bilinear opacity over the
   quadrant's 17x17 grid, the base diffuse, each ATXT layer in RECORD ORDER with
   opacities at or below 0.001 **skipped and not blended with a tiny weight**,
   the VCLR multiply, then the grass tint **after** the VCLR multiply. Step 8 is the
   colour grade `colour *= grade`. Its default is 1.0 and the step is skipped at that value;
   a runtime that ships a grade reads the bake's `landGrade`.
4. Cross-fades the two across the inner band.
5. Suppresses the engine's far terrain for the chunks it serves.
6. Subtracts shore proximity at runtime from the `.lodl` water planes and derives
   wetness from the weather — neither is baked any more (VT, the version-2 block).

**THE SAMPLING RULES THAT DECIDE WHETHER THERE IS A SEAM**, all from VT:

* each source diffuse is sampled at ~~`u = frac(wx/2048)`, `v = frac(wy/2048)`~~
  `u = frac(wx/T)`, `v = frac(wy/T)` with **T = 341.3333**, the engine's own landscape repeat
  (corrected 2026-09-23; 2048 survives only as `--land-tiling 2048`, the way back to
  pre-2026-09-11 sheets) — the bake's world-space tiling — at the mip
  `clamp(log2(max(1, unitsPerTexel / (T/textureWidth))), 0, maxMip)`. ~~trilinear~~
  The shipped sampler is no longer plain trilinear. Since 2026-09-12 (lane DEFAULTS1) the
  defaults are hex tiling 256, warp 341, mip bias -0.22 and land guide flatwarp 1.0, and
  **the ring-0 runtime must use the SAME sampler the bake used**. VT 5's CLI table still
  lists these as off, which is a finding against VT. **Get the mip wrong and the two bands differ by the texture's own
  high-frequency detail, which is the visible half of a seam** (VT 2.5).
* the pyramid is **NORTH-UP** in its tile table and inside every payload
  (VT, the row-order block); the `.lodl` is **row-0-SOUTH** (BTD Header). Both
  conventions are live and the mismatch has already cost one consumer a Y mirror.
* the mask sheet's channels are `rmaos` — **R roughness, G metallic, B AO,
  A ground cover** (VT 2.2). Role 3 `data` is RETIRED and refused by name.
* coarse levels are **downsamples of fine data, not measurements at that scale**
  (VT 2.3). Only cover, albedo, roughness, metallic, emissive and height filter
  cleanly; AO is a fixed horizon march that ~~reaches 2,048 units~~ stops at **1,458** units in both
  sheet composites (2,048 is only the loop's bound; VT 2.2, census `objAoReach 1458`;
  corrected 2026-09-23), and `mean(AO) != AO(mean)`.
  The index says `coarseLevelsAreDownsamples: true` so a consumer never reads a
  coarse level's AO as measured at that density.
* exactly ONE sheet may declare two formats, and it is the cover carrier
  (VT 2.2, VT 3.4 rule 13). A consumer sizing an upload from a single per-file
  format mis-sizes every cover tile.

**THE HEIGHT SHEET, AND R3 DEPENDS ON IT.** The pyramid's fourth sheet is
`R16_UNORM` height, `pixel = height/8 + 32767` — the same encoding the
whole-worldspace shadow heightmap uses, so the two agree without conversion
(VT 2.2). **It is opt-in** (`--vt-height`) and it more than doubles a tile:
184,960 bytes of height against 138,720 of colour classes. R3's terrain shadow
march reads it, so a bake without it leaves R3 with no height source beyond the
inner band. See §5 and §6. **The VTBAKE1 fixture of 2026-09-23 carries it** (4 sheets);
whether bungo's full bake does is still §6 (e).

**SWITCH AND KEYS.**

| key | kind | default | why |
|---|---|---|---|
| `bDrawNativeTerrain` | INI-only | 1 | the way back inside the module |
| `bSuppressEngineFarTerrain` | INI-only | 1 | bungo's "vanilla LOD will be suppressed" |
| `iTerrainResidencyBudgetMB` | INI-only | to be measured | binds `tileBytes`; `evictions` reads 0 when it never binds (CENSUS 5.2) |
| `fInnerBandBlendUnits` | INI-only | to be measured | the width of the cross-fade across the inner band |

**FALLBACK, AND THE ARM WORD.** `arm.terrain` = `native` (the pyramid) /
`stock-fallback` (the `.btr` chunk meshes) / `off`, refusing `no_lodm`,
`no_lodt`, `refused_hard` (CENSUS 2). Rename the VT `.lodm` aside and the word
must move. **The `.btr` chunk sheets are not deleted by any of this** — the
pyramid supplies their bytes, it does not replace the files (VT 2.4), so the
fallback arm has something to fall to.

**CENSUS.** `[ImprovedLOD.Residency]`: `tilesResident`, `tilesPresent`,
`tileBytes`, `residentBytes`, `evictions` (CENSUS 5.2).

**GATES.**

1. **The ring-0 formula reproduced at runtime on the same texel.** The generator
   already gates its own half: an INDEPENDENT implementation of VT 2.5's seven
   steps, with its own ESM walk and its own BC1/BC3/BC5U decoding, against the
   pyramid's level-0 colour — two Sanctuary tiles at mean **3.26** and **3.59**
   of 255, p95 **8**, max **14** and **17**, against a FLOOR of **13.70** and
   **15.15** (a blend that ignores the per-texel weights, 4.2x worse) and a
   CEILING of **0** over 73,984 texels (VT 2.5). The runtime must land inside the
   same band on the same texels, with the same floor shown red.
2. `tilesResident <= tilesPresent` at every level (CENSUS 6.2).
3. The grass tint reproduced from the cover byte and `tintStrength` as VT 1.5
   states it — measured size, mean **26.97/255** over the 256 highest-cover
   texels of a chunk.
4. `evictions` rises off 0 when the budget is lowered until it bites, and
   `libraryBudgetBytes` prints beside it so a 0 can be read (CENSUS 5.2).
5. An altered capture at the inner-band boundary, before and after, with the
   seam's own pixels cropped.

**FLIGHT.** Restart first.

1. Stand at the edge of the loaded cells looking out. *"Is the ground the same
   colour on both sides of the line, and is there no crack?"* Refuter: a colour
   step means the mip rule or the tint; a geometric crack means the inner band is
   not being drawn from the `.lodl`'s finest level (§3 item 2).
2. Walk a hundred metres and stop. *"Does `tilesResident` at the finest level
   move while the coarsest does not?"*
3. Send the log, plus a screenshot of the line if step 1 fails.

**MUST NOT.** Read a coarse level's mask B as an AO term measured at that
density. Alpha-test the colour sheet's alpha when the ground cover lives in the
mask (VT 2.2a). Assume a row order. Delete the `.btr` chunk sheets. Renormalise
the layer opacities for the colour — the cover side renormalises, the diffuse
does not, and the two must not be confused (VT 1.2).

**RE CANDIDATES.** The engine's own far-terrain draw, per chunk, for
`bSuppressEngineFarTerrain`. The `[TerrainManager]` distance family
(`fBlockLevel0Distance` and its siblings), read live, because the band edges must
follow the user's own INI and never a literal.

---

### R3 — Screen-size selection, GPU culling, the fades, and the far shadow pass

**READS.** The cluster ladder table, 48 bytes a cluster: bounding sphere, normal
cone, `geometricError`, `parentError`, `parentFirst`, `parentCount`, `level`,
`sourceTriangles` (NATIVE 3.2). The occluder box table and its per-cell ranges
(NATIVE 4.5.1). The pyramid's height sheet, and the `.lodl`'s finest level.

**ON TODAY'S DEFAULT BAKE THERE IS NO LADDER** (added 2026-09-23). bungo, 2026-09-17: *"Authored LODs only,
only the trees get baked for the cards"* (WW_CHANGES 2026-09-17, "Authored LOD models only").

* The default library is the vanilla LOD models (`--library mnam`) with no cluster ladder. `--native-ladder`
  is the opt-in. So `levelMax` is 0, the `LADDER` flag is clear, and every cluster is a root.
* The distance steps are then **the four MNAM slots** `rep[0..3]`. NATIVE 4.4 says a consumer picks the mesh
  slot by base size.
* The cluster cut below is correct and it runs, but on a default bake it selects level 0 everywhere.
* Gates 1, 2 and 5 below were written against the opt-in ladder (the bake of 2026-09-11). They cannot tell
  anything apart on a default bake.
* On the default bake, `CONE_OPEN` is **6,982 of 10,634**.

**DOES.**

1. **The cut.** For a cluster of an instance,

   ```
   screenErrorPx = geometricError x scale x projectionScale / distance
   ```

   and the cluster is drawn when

   ```
   screenErrorPx <= tolerance   AND   parentError x scale x projectionScale / distance > tolerance
   ```

   Exactly one cluster of every leaf's ancestry satisfies that, so **the cut is a
   partition of the surface** (NATIVE 4.4). `parentError` is `FLT_MAX` at a root,
   so a chain whose every error is under the tolerance terminates at its root
   rather than selecting nothing.

   **`projectionScale` is a reference constant, not a format constant.**
   `960/tan(35 degrees) = 1371.0` assumes 1920 wide and `fDefaultWorldFOV = 70`;
   **the consumer recomputes it from the live projection** (NATIVE 4.4).

2. **Sphere and cone culling.** Both describe the STORED geometry, dequantised,
   which is a format rule and not an implementation detail (NATIVE 3.6). A
   cluster with `CONE_OPEN` (flags bit 2) **must never be backface-culled** — its
   normals span more than a hemisphere. Measured: **14,604 of 20,678 clusters are
   open** on the opt-in ladder bake of 2026-09-11 (**6,982 of 10,634** on today's default,
   2026-09-23), which is what a worldspace of trees and cut-out fences looks like.

3. **Occluder rejection** against the cell's boxes, each fitted INSIDE a
   watertight LOD mesh, shaved by a voxel and probed at a hundred interior points
   before it was written (NATIVE 4.5.1).

4. **Screen-size fade**, bungo's spec of 2026-09-11 10:4x, Unity's model:
   projected size = `boundRadius / distance x projection scale` as a **fraction
   of SCREEN HEIGHT** — resolution-independent and FOV-aware; one threshold per
   engine fade class (objects, actors, items, grass), default about 1 percent;
   hysteresis about 20 percent above the threshold for fade-in; **and the fade IS
   the dither cross-fade**. The per-instance radius is the RULE
   `base.boundRadius x scale`, not a stored float — more accurate than a 1-unit
   u16 at zero bytes an instance, with a stored `scale` of 0 refused and
   `maxBoundRadius` computed from the QUANTISED scale so it is a true upper bound
   (NATIVE 4.1b).

5. **Dithered cross-fade at every switch** (bungo 2026-09-11 08:3x, verbatim:
   *"Dithered cross fade is good"*). FO4CS's own standing rule applies: a
   no-temporal-filter arm must be smooth within a single frame, so raw dither is
   only ever input to a temporal filter, never a shipped look (FO4CS
   CONSTITUTION 8, amended 2026-09-01).

6. **The far shadow pass, as bungo ruled it on 2026-09-11 14:4x -> 15:0x.**

   * **One height source for draw AND shadow.** His words: *"for this new LOD
     system, the way terrain shadows get sampled from needs to change"* → the far
     terrain shadow march samples **the `.lodl` finest level in the inner band
     and the pyramid's resident height sheet beyond** — the same data the ground
     is drawn from. **`HeightMap.dds` becomes the fallback arm** (module off).
   * **The HYBRID is the default.** March the terrain — heightfield-exact
     contact, **estimated** 0.3-0.8 ms, cost rising with a low sun, cannot do
     overhangs — and render **only the object cluster cut plus the cards** into a
     far shadow map — **estimated** 1-3 ms for everything, small for objects
     alone — and **combine with a max**. **Both costs are census fields and the
     first FO4CS capture round measures them. The two figures above are estimates
     and are labelled as such until captured.** Map-only is the fallback arm if
     the march misbehaves.
   * **The shadow view is a second, coarser selection of the same data**, with
     its own tolerance. Nothing in the file is per-view, so a cluster culled for
     the camera is still addressable for the light (NATIVE 4.4). His 08:4x ruling
     is what makes the coarser cut safe: the pass keys on identity and excludes
     self-shadowing, so there is no self-occlusion to get wrong.
   * **Cards cast from their height channel, oriented to the light.** The height
     is the normal sheet's blue: **0.5 is the card plane** and
     `units = (B - 0.5) x depthSpan` (CARDS 4).

**SWITCH AND KEYS.**

| key | kind | default | why |
|---|---|---|---|
| `fClusterTolerancePx` | INI-only | **1.0** | bungo 10:4x, *"one global pixel tolerance"*; NATIVE 4.4 |
| `fShadowTolerancePx` | INI-only | to be measured, coarser than the camera's | CENSUS 5.3 |
| `fFadeObjectsPct` | **END-menu row** + INI | 1.0 | bungo 10:4x, *"a menu row + INI key each"* |
| `fFadeActorsPct` | **END-menu row** + INI | 1.0 | same |
| `fFadeItemsPct` | **END-menu row** + INI | 1.0 | same |
| `fFadeGrassPct` | **END-menu row** + INI | 1.0 | same |
| `fFadeHysteresisPct` | INI-only | 20 | bungo 10:4x |
| `bDitherCrossFade` | INI-only | 1 | bungo 08:3x |
| `bOccluderCull` | INI-only | 1 | NATIVE 4.5 |
| `bConeCull` | INI-only | 1 | NATIVE 3.6 |
| `iFarShadowMode` | INI-only | **1 = hybrid** | bungo 15:0x; **0 = map only**, the fallback arm |
| `iLibraryBudgetMB` | INI-only | to be measured | binds `libraryBytes`; his zoom answer of 10:5x |

**Four END-menu rows in one wave is more than the module's one row**, and they
exist because he asked for them by name. The rows go in on his confirmation, not
on this page's say-so (`fo4cs-menu-row`, and the standing rule that no row lands
without him). Each row passes its own `"ImprovedLOD.fFade...Pct"` key to the menu
shim, or the restart note cannot be generated, the row is invisible to the search
index, and the key lands on the no-row list that is pinned by an exact count
(`fo4cs-menu-row` §8).

**FALLBACK, AND THE ARM WORD.** Three arms with three floors, each named:
the cut falls back to level 0 alone (`no_ladder`, ~~on a `--native-no-ladder`
bake~~ which **is the DEFAULT bake since 2026-09-17**, with `--native-ladder` the opt-in,
where `levelMax` reads 0); occluder culling falls back to none
(`no_boxes`, with the container's own `occluderCount` as the number that says
which); the shadow pass falls back from hybrid to map-only. A fade with no
threshold refuses `no_threshold` rather than fading at a guess.

**CENSUS.** `[ImprovedLOD.Cluster]` in full — `clusterTolerancePx`,
`projectionScale`, `levelMax`, `clusterSelected` per level, `cutOverlap`,
`cutGap`, `clusterTriangles`, `draws`, `overflow` (CENSUS 4); `[ImprovedLOD.Fade]`
in full — the four thresholds, `fadeHysteresisPct`, `crossFadeInFlight`,
`crossFadePeak`, `gridEdgeBand` (CENSUS 5.4); `[ImprovedLOD.Shadow]` in full —
`shadowTolerancePx`, `shadowClusters`, `shadowTriangles`, `shadowDraws`,
`shadowIdentityUnique` (CENSUS 5.3). ~~**This rung adds two census words the page
does not carry:** the march's own cost and the shadow map's own cost, which are
his 15:0x ruling's instrument. Named in §5.~~ Struck 2026-09-23: the census page now
carries both, `shadowMarchMs` and `shadowMapMs` (CENSUS 5.3, default `unmeasured`). The
Shadow row is eight fields with `shadowCasters[src]`, which R1 fills.

**GATES.**

1. **Triangles per distance against the reference cut's own numbers**, at the
   same tolerance and distance. NATIVE 4.4.1, measured on the ~~real pair~~ opt-in
   LADDER pair of 2026-09-11 (`--native-ladder`; labelled 2026-09-23). On a default bake
   every tolerance selects level 0, and the expectation, derived and not measured, is
   124,205 at every distance. The table was measured over all
   3,526 instances with the camera at the centre of the instance cloud:

   | tolerance | 2,000 u | 8,000 u | 32,000 u |
   |---|---|---|---|
   | 0.5 px | 124,205 | 124,181 | 121,898 |
   | 1 px | 124,205 | 124,121 | 115,583 |
   | 4 px | 124,121 | 115,583 | 104,090 |

2. **Two floors in the same run**: tolerance 0 selects exactly the level-0
   clusters of the drawn set, and a tolerance of 1e12 selects exactly the roots
   (NATIVE 4.4.1).
3. `cutOverlap` and `cutGap` both **0**, because the cut is a partition by
   construction. A doctored `parentError` in the file must make `cutOverlap`
   non-zero (CENSUS 4).
4. `projectionScale` **must not read exactly 1371.0** on a display that is not
   1920 wide — that is the consumer having taken the page's reference constant
   instead of its own projection, and the census page says so in the field's own
   row (CENSUS 4).
5. `clusterSelected[l]` may never exceed the library's own count at that level:
   10,634 / 5,676 / 2,781 / 1,029 / 414 / 123 / 19 / 2 at levels 0-7
   (NATIVE 3.5.3). Those are the opt-in ladder bake's counts; the default bake has
   10,634 at level 0 and none above (2026-09-23).
6. Raising `fFadeObjectsPct` makes `ringCulledScreen` rise and `ringDrawn` fall
   **by the same number** (CENSUS 3).
7. **The march and the map are each a measured millisecond**, in the first
   capture round, and the sum is compared against `iFarShadowMode = 0`.
8. Altered capture at a LOW sun, both ends measured, near and far, with numbers
   and not adjectives (`fo4cs-altered-capture` §4): a fix that shortens the
   march, caps the reach or fades occlusion with distance deletes the thing the
   far-shadow module exists for.

**THE ONE NUMBER THIS RUNG MUST CARRY INTO THE ROOM.** The ladder is correct, it
is a partition, and **at a one-pixel tolerance its first step is not selected
anywhere in the Commonwealth**: the median level-1 cluster deviates by **3.80
percent of its model's own diagonal**, which at a 1,000-unit building is 38 units
and reaches one pixel only past **52,100 units** (NATIVE 3.5.4). The measured cut
therefore saves **between 0 and 16 percent** of triangles on the Sanctuary region
whatever the distance. **The cause is not the ladder** — it is that "full detail"
in the library is already Bethesda's LOD mesh, a mean of **47.7 triangles for a
whole building**. Building the library from each base's near `MODL` fixes it with
**no format change at all**, ~~and it is bungo's call, open in §6. Until it is
taken, R3's selection gate is honest about spending its effort on culling and
fades rather than on the cut.~~

**Status 2026-09-23:**
* That call was taken 2026-09-16 (the near library, made the default) and REVERSED 2026-09-17: authored LOD
  models only, the MNAM library, and no ladder (§6 (a)).
* The numbers in this paragraph describe the opt-in ladder.
* On the default bake the cut has nothing above level 0 to select. R3's selection gate spends its effort on
  culling, the MNAM slot pick and the fades.

**FLIGHT.** Restart first.

1. **The step that decides the most:** low sun, stand with a distant tower behind
   you. *"Does its shadow reach you, and is it one shadow?"* Refuter: no shadow at
   all with `iFarShadowMode = 1` and a shadow at `0` means the march is wrong, not
   the map — read the two cost fields before changing anything.
2. Aim a scope at a distant building. *"Does it gain detail as you zoom, without
   popping?"* Refuter: no change at all is the finding of NATIVE 3.5.4 arriving in
   the game, not a bug in the cut — check `clusterSelected` and read `levelMax`.
   On a default bake `levelMax` is 0 by design since 2026-09-17, and that is not a fault.
3. Walk toward a stand of trees. *"Do they fade in smoothly rather than
   appearing?"* Refuter: a visible dither pattern that never resolves means the
   cross-fade is shipping raw dither as a look.
4. One INI bisect he can run with no build: `fClusterTolerancePx = 0` (every
   cluster at level 0) against `4`. *"If the frame time does not move between
   those two, the cut is not where the spend is, which is the most valuable
   answer you can give."*
5. Send the log and `ShadowLow.rdc` taken at a low sun.

**MUST NOT.** Backface-cull a `CONE_OPEN` cluster. Use the reference projection
constant. Select by ring. Fade on a distance rather than a projected size. Let the
shadow view's coarser tolerance change the camera view's cut. Read the estimated
millisecond figures as measurements.

**RE CANDIDATES.** The engine's fade-compare site, and the per-class fade
settings it reads — the module needs to know whether a placement is an object, an
actor, an item or grass, and **that is the engine's own record, read at runtime,
not a bake product** (CENSUS 6.3 item 7). The cascade admission site, shared with
R1.

---

### R4 — Cards

**READS.** Per-base card sets (CARDS 1.1) or card arrays (CARDS 1.2), with their
`.lodm` sidecars (LODM 3, LODM 4); the base row's `cardLayer`, whose low 11 bits
are the layer and whose high 5 bits are the card array set, `0xFFFF` meaning no
card (NATIVE 3.2); the `C` lines of the manifests on the stock fallback arm
(MANIFEST 4).

**Added 2026-09-23:**
* **The view-convention token `conv`,** on the card set and on each array layer. It has read `"spec1"` since
  the 2026-09-19 azimuth repair. When it is ABSENT, the set was baked before that repair: its azimuth is
  turned 180 degrees and it is owed a re-bake (IMPOSTOR_SPEC). LODM 3 does not list `conv` yet, which is a
  finding.
* **`kind: "aggregate"` sets,** with their `.lodi` aggregate rows (LODM 3a, CARDS 10).

**DOES.** Draws one quad per card, from the hemi-octahedral N x N frame grid —
**`oct = N` is frames per SIDE, so the sheet holds N squared views: `OCT=8` is 64
views, not 81** (CARDS 2). Blends the three nearest frame centres, writes depth
from the height channel for the true silhouette, lights the card from the normal
and height channels, ~~sways it from the height~~ sways it by the sway weight in the normal
sheet's ALPHA (CARDS 4; height is the normal sheet's BLUE; corrected 2026-09-23), casts its
shadow from the height,
and multiplies its emissive by `emissiveScale` at night (bungo 2026-09-11 08:3x,
verbatim: *"Yeah"*).

**THE CARD LAW AS THE GENERATOR WRITES IT, 2026-09-23.** Sources: CARDS 2, 3.1-3.5, 3.7 and 4; IMPOSTOR_SPEC;
HANDOFF 2026-09-19 to 2026-09-23. Where IMPOSTOR_SPEC and CARDS disagree, CARDS wins, by SPEC's own rule.

* **Grid.** N x N hemi-octahedral, `oct = N`, N squared views.
  * The bake accepts N = 2..16. The panel offers 4 / 6 / 8; the default is 8, with card tile 128.
  * CARDS 2 says "the centre frame is the exact top". That holds only for ODD N.
  * CARDS 2 does not specify the diagonal of the three-frame blend's triangles.
  * The NifSkope viewer fixes both and names them SPEC GAPs (`src/impostoroct.h`). Both are findings against
    CARDS.
* **Frame size ladder.** 1, 1/2, 1/4 or 1/8 of the reference size, never below 32 px, rounded to the nearest
  in log. With no reference set, there is no ladder (CARDS 3.3).
* **Aspect.** The short side is the smallest multiple of 16 whose inner rectangle fits (CARDS 3.2).
  * The five-rung aspect ladder is retired.
  * IMPOSTOR_SPEC's frame-law section still prints it, which is a finding.
* **Gap and pad.** gap = max(2, side/16), rounded up to even; pad = gap/2; mips = log2(gap). `half` spans the
  whole frame (CARDS 3.1).
* **`--card-half-aux`.** Normal, mask and emissive sheets at half size; colour never. OFF by default. It saves
  42.9 percent of a card's bytes (CARDS 3.5).
* **Camera.** Orthographic, `projection "ortho"` (CARDS 3.7). The view convention is `conv "spec1"`, from the
  2026-09-19 azimuth repair. bungo: *"Okay, fix the 180 issue"*.
* **Sheets:**
  * `_d` / `_bc`: BC3 colour, with coverage in alpha (encoded; 160 and up is inside).
  * `_n`: BC3, view normal in RG, height in B, sway in A.
  * `_gsaos` / `_rmaos`: BC3 mask.
  * `_g` / `_e`: BC1 emissive.
  * The coverage contract is `{floor 16, test 128, base 160}`, encoded (CARDS 4).
* **Families.** `legacy` and `pbr` only. A third `family` word is a hard refusal (LODM).
* **Crisp over smooth.** bungo, 2026-09-23 06:0x: *"Fortnite impostors are a lil bit more choppy, but I think
  it's a fair tradeoff"*.
* **Ring layout for tree cards: RULED and HELD.** bungo, 2026-09-23 04:5x: *"for fo4cs use the convention was
  22.5 degrees per take"*.
  * Tree cards become 16 azimuths at 22.5 degrees on the horizon ring. They reuse the aggregate ring layout
    (`views`, a two-frame blend) instead of the N x N grid.
  * Lane IMPOSTORRING1 is HELD behind IMPOSTOR16, and nothing bakes a tree ring yet. So a reader built today
    reads the N x N grid and must also expect `views` (LODM 3a.2).
  * A raised second row is open, §6 (q).
* **Queued on the generator side** (§5 rows 20-22):
  * specular weight, colour and IOR, and the TintMask, in the card bake (IMPOSTORPBRM1);
  * sway from the tree model's own wind weights (IMPOSTORWIND1);
  * a per-pixel depth search for the doubled trunk (IMPOSTORDEPTH1);
  * a re-bake of every set older than 2026-09-19.

**TRANSITION SMOOTHNESS SLIDER** (bungo 2026-09-23 06:0x, verbatim: *"fo4cs can implement a slider for
how choppy the transition should be for these, from instant rotation that's snappy, to something very smooth
but possibly noisy"*). One user control on the card reader, a row in the FO4CS menu, live (no restart):
- **0 = snap**: nearest frame only, no blend; the card jumps from angle to angle, crisp.
- **middle**: the three frames' weights sharpened (`w_i^p` renormalised, `p` from the slider) so the blend
  lives only in a band around the frame boundary; the alpha test is on the strongest contributing frame (or
  the average, whichever the NifSkope IMPOSTOR16 N8 round measured as tearing less); crisp edges, no noise.
  **Measured 2026-09-23 07:2x (IMPOSTOR16 N8):**
  * The average cut tears less than the strongest-frame cut at elevation 20: N8 5.7 against 7.1 percent,
    N16 3.3 against 4.3.
  * The average cut also pops 2.6-3x less. The lane recommends the average.
  * Director findings 07:4x-07:5x: the crisp average makes a TRUNK vanish midway between angles, and the
    blend doubles the trunk. Snap removes both.
  * IMPOSTORDEPTH1, a per-pixel depth search, is queued.
  * The default is still his to pick, §6 (o).
- **1 = smooth**: the full three-frame blend with the stippled cut (NifSkope's IMPOSTORTEAR1 cut); with the
  game's TAA the stipple pattern should vary per frame so TAA averages it out.
The DEFAULT sits on the crisp side (bungo 2026-09-23: *"they're LOD objects ... it's fine if they're
choppy"*); the exact default value is his call after the NifSkope preview. NifSkope's card drawer carries
the SAME control first, so he can judge it before FO4CS reads a single card.

**THE FOUR THINGS THAT MAKE A CARD SIT WHERE THE MESH WAS**, each one a measured
defect on the generator side already:

1. **Place the quad at `pivot + center + frameOffset[2k]·right(i,j) +
   frameOffset[2k+1]·up(i,j)`**, `k = j·oct + i` (LODM 3.1). Treating `center` as
   zero puts TreeHero01's card **1,070 units low**; honouring `center` and
   ignoring `frameOffset` makes the tree step sideways as the mesh hands over.
2. **Alpha-test at the sheet's own `coverage.test`, not at 0.5 by habit.** A set
   with a `coverage` object states `{floor, test, base}` and its `test` is 128;
   **a set with NO `coverage` key has a raw-fraction alpha and must be tested at
   16/255** — reading such a set at 0.5 draws a tree up to **5.41 texels of
   half-width** narrower than its `.lodm` declares, which measured as TreeHero01
   handing over **9.1 percent narrower and 3.2 percent shorter than its own
   mesh** (CARDS 4). **A conflict found 2026-09-23, not ruled:**
   * Since 2026-09-22 (IMPOSTORFIN1) the NifSkope viewer's default cuts the DECODED coverage fraction at
     128/255, which is vanilla's own LOD alpha test. bungo, 2026-09-22 21:5x: *"what vanilla game had worked
     pretty well"*.
   * The encoded `test` 128 used above corresponds to a coverage fraction of 16/255.
   * Which domain FO4CS tests in is §6 (p).
3. **`projection` absent is not "unknown".** Every bake that did not say drew a
   60-degree perspective frustum, so the set's `half`, `center` and `frameOffset`
   are approximate and its frames are foreshortened. A reader **may refuse such a
   set by name; it must not silently treat absence as `ortho`** (LODM 7, invariant 8,
   CARDS 3.7). **The same holds for `conv`** (added 2026-09-23). An absent `conv` is a set
   baked before 2026-09-19, with its azimuth turned 180 degrees. A reader may draw it under
   the old reading as a diagnostic, and must then say so; otherwise it refuses the set by
   name (CARDS 3.7, IMPOSTOR_SPEC).
4. **The quad IS the frame.** `half` spans the full frame, padding included; a
   consumer that insets by the padding shrinks the object (CARDS 3.1).

**AGGREGATE CARDS.** bungo accepted them on 2026-09-11 08:3x (*"1 sounds good"*):
one sheet per forested cell, photographed from the horizon views over the trees'
repetition-broken cards, so the outermost band's instances become one per cell,
with a cross-fade at the border into the band inside it. ~~**Lane CARDS-AGG on the
generator side owns them and they do not exist yet** (NATIVE 12, CENSUS 5.1). R4
ships without them and `cardsAggregate` refuses `not_baked`.~~

**Struck 2026-09-23: lane CARDS-AGG SHIPPED them on 2026-09-11,** behind `--aggregate`. It is OFF by default,
and a bake with it off is byte-identical (LODM 3a, CARDS 10, NATIVE 4.6). What it writes:
* `kind: "aggregate"` `.lodm` sets: three sheets, no emissive, 8 horizon azimuths;
* `.lodi` aggregate rows (v4, or v5 with placement AO);
* `aggSwitchPx` 96 and `aggBandRatio` 1.2.

The forested-cell count bungo asked for: **2,631 of 3,685 tree cells, 60,605 trees**. The aggregates have never
been flown. They were built from pre-repair cards, so they are owed a re-bake (§5 row 22). On a bake without
`--aggregate`, `cardsAggregate` still refuses `not_baked`.

**SWITCH AND KEYS.**

| key | kind | default | why |
|---|---|---|---|
| `bDrawCards` | INI-only | 1 | the way back inside the module |
| `bCardsEmissiveAtNight` | INI-only | 1 | bungo 08:3x |
| `bRefuseNonMetricCards` | INI-only | 1 | LODM 7, invariant 8 — refuse a set with no `projection`, by name |

**FALLBACK, AND THE ARM WORD.** `arm.cards` = `native` (card arrays) /
`stock-fallback` (the crossed quads baked into the `.bto`) / `off`, refusing
`no_sheet`, `no_layer` (CENSUS 2). The crossed `_fs` quads stay in the mesh for
exactly this reason (CARDS 1.1); a consumer that reads `C` lines draws the sheets
instead and kills those quads by object index (MANIFEST 4). Under the FO4CS target the
`.BTO` is scratch and is removed (§5 row 7), so the crossed quads exist only on a
stock-target bake or a `--keep-bto` bake (added 2026-09-23).

**CENSUS.** `[ImprovedLOD.Cards]`: `cardsDrawn`, `cardsTotal`, `cardBases`,
`cardFrames`, `cardsAggregate` (CENSUS 5.1), and `cardBytes` in the residency row.

**GATES.**

1. **Today every card field must refuse**, and the census page already says so:
   the bake writes what the stock ring bakes and nothing more, so `cardLayer` is
   `0xFFFF` on every base and `cardCorpusHash` is 0 (NATIVE 11, Deviation 5). So
   **R4 cannot be gated on a Commonwealth bake until a card layer is written** —
   its gate runs on a bake that has one, or ~~on the 19-tree Sanctuary libraries
   that exist on disk (CARDS 9)~~ (struck 2026-09-23: those libraries predate the
   ortho camera, the coverage contract and the azimuth repair) on a card set baked after
   2026-09-19 that carries `projection "ortho"`, `coverage` and `conv "spec1"`.
2. A card set rendered in-engine against the bake's own frame at one view, within
   a stated error, with the two controls the generator's own transition gate
   already uses and requires to FAIL: `center` treated as zero, and `frameOffset`
   ignored (LODM 3.1).
3. `cardBases` may not exceed the count of bases with `cardLayer != 0xFFFF`
   (CENSUS 6.2).
4. `cardFrames` climbs toward N squared when a tree is orbited (CENSUS 5.1), or
   toward `views` on a ring-layout set.

**FLIGHT.** Restart first.

1. Walk toward a tree line until the trees become cards and back again. *"Does
   any tree jump, change size, or step sideways at the moment it changes?"*
   Refuter: a sideways step is `frameOffset` ignored; a size change is the
   coverage test; a tree that is suddenly thin is a non-metric set being drawn as
   if it were metric.
2. At night, look at a distant lit sign. *"Does it still glow?"*
3. Send the log and a screenshot of any tree that jumps, from the same spot.

**MUST NOT.** Print 0 in a card field where nothing was baked — refuse. Infer aux
sheet sizing on a mesh array (LODM 4: a `kind: "array"` file carries no `aux*`
keys). Decimate a card: impostor cards are **never** touched by the far-ring
simplifier and are excluded from it a second time by object index off the `C`
lines (CARDS 7, invariant 6). Read a mesh array's normal blue as height or its mask blue as
AO — on a mesh layer those are neutral on purpose, because a mesh carries its own
sway in vertex alpha and its own AO in vertex colour B (ARRAYS 3).

**RE CANDIDATES.** None new. The engine's night state is read the way the
existing weather modules read it.

---

### R5 — The 5x5 loaded grid, after R1-R4 have flown

bungo, 2026-09-11 10:3x, asked whether the loaded 5x5 grid is performant and
answered himself: *"Screen size fade to lower LODs would be nice to have within
that 5x5 grid, 2 sounds good too, 3 sounds ogod"* — all three accepted, each its
own wave with its own flight.

**READS.** The same `.lodo` cluster ladder and `.lodi` instance tables R1 and R3
already read (NATIVE 3.5, NATIVE 4), plus the cold blob's `refFormId` at the
instance's own index, which is the whole of the join to the engine's placed REFR
(NATIVE 4.1a), and the cell-range blob, which is what makes a per-cell hand-over
describable at all (NATIVE 2.1).

**DOES.** Three separate waves, in this order, each with its own flight.

**R5a — screen-size fade and lower-LOD selection inside the grid.** The same
projected-size test as R3, applied to the engine's own fade classes inside the
loaded cells; render-only, with hysteresis. AI, script and persistence distances
stay Bethesda's.

**R5b — shadow casters inside the grid at lower detail**, from the LOD model or
from our cluster set, **never the full mesh**. This is the same law as R1's
"one shadow representation per placement", applied one band inward.

**R5c — the cluster LOD extended INWARD.** Objects in the outer cells draw from
the `.lodo` cluster set by screen error and the engine's full draw is suppressed
**per object**. The join is `cold[i].refFormId` at the instance's own index —
one array read, no search, in the load-order-mapped ID space (NATIVE 4.1a). The
census field is `suppressedDraws`, which refuses
`not_suppressing:no_refmap` / `:no_lodi` / `:off` rather than printing 0
(CENSUS 5.4).

**THE DATA IMPLICATION, AND IT IS ALREADY IN THE BYTES.** R5c needs the ladder to
have NEAR levels to select, which is why bungo's 10:3x ruling asked for the error
ladder to run from full detail down. It does (NATIVE 3.5). But **"full detail" in
today's library is Bethesda's LOD mesh**, so R5c has almost nothing to select
until the library is built from each base's near `MODL` — the open call in §6
item (a). **R5c should not be chartered before that call is taken**, or it will
ship a mechanism with nothing to choose from and the flight will read as a
failure of the mechanism.
**Status 2026-09-23:** the call was taken 2026-09-16 (the near library) and reversed
2026-09-17 (authored LOD models only). A default bake has no ladder and no near levels;
both are opt-in (`--native-ladder`, `--library near`). On a default bake, R5c has only
the four MNAM slots to choose between.

**PRECOMBINES (director's splice 2026-09-11 16:2x, from bungo: "vanilla
fallout 4 has some precombines placed in some places, that contain several
trees, right?").** Inside the loaded cells the engine draws precombined meshes
-- one merged mesh per group of statics per cell -- not the placed references
one by one. An object that is part of a precombined mesh therefore has NO draw
of its own to suppress: "suppressed per object" is only true for references
the precombine system left alone (trees with the tree-animation flag, movable
statics, anything a mod's cell edit broke out of its precombine). For the rest,
suppression is per precombined BATCH or not at all, and suppressing a batch
means EVERY object in it must be served by the cluster set in that frame. The
join for a batch is the cell's precombined-reference list (`XCRI`, ESM layouts
line 34), not the instance record. R5c's RE candidates gain: the precombined
draw site and its per-cell reference list; its census gains
`suppressedBatches` beside `suppressedDraws`, and a refusal
`not_suppressing:precombined` where an object cannot be suppressed alone. SCOL
tree clusters are NOT this case: the generator already expands each SCOL into
its parts (~~NATIVE 2.1 / lodgen.cpp ~3389~~: the SCOL expansion is in `src/lodgen.cpp`
and NATIVE 2.1 does not describe it; re-cited 2026-09-23) and the engine draws a SCOL as one
reference, so a SCOL suppresses as one draw for all its parts.

**THE DISSOLVE (bungo 2026-09-11 16:3x: "maybe we could dissolve these
precombines by matching vanilla objects to them, at their rotation, scale,
position, etc.").** No geometric matching is needed: the cell record's `XCRI`
list already names the member references of each precombined mesh. The runtime
takes a batch, reads its members, suppresses the batch, and draws every member
from the cluster set at the member's own transform (the instance record's
position / quaternion / scale, and the engine's reference data agree by
construction). The dissolve is exact. It pays only where the instanced,
GPU-culled path is cheaper than the merged mesh -- true at the far side of the
grid, not necessarily up close -- so the rung's flight measures both and the
census prints `dissolvedBatches` and the draw count either way. CANDIDATE, not
fact, to verify against the plugin before chartering: references with the
tree-animation flag are never precombined (they need per-object wind), so the
tree case is the easy case and buildings are the hard one.

**SWITCH AND KEYS.**

| key | kind | default | why |
|---|---|---|---|
| `bGridScreenSizeFade` | INI-only | 0 until flown | R5a |
| `bGridShadowCastersLowDetail` | INI-only | 0 until flown | R5b |
| `bClusterLodInward` | INI-only | 0 until flown | R5c |
| `fGridEdgeBandUnits` | INI-only | to be measured | `gridEdgeBand`, §3 item 1 |

**FALLBACK, AND THE ARM WORD.** Each of the three is its own arm with the engine
as its floor; `suppressedDraws` falls to 0 when the inward extension is turned
off, which is its own movement test (CENSUS 5.4).

**CENSUS.** `gridEdgeBand`, `suppressedDraws` (CENSUS 5.4), plus the Ring and
Cluster rows already in place.

**GATES.** `suppressedDraws` may not exceed the instances actually drawn
(CENSUS 6.2) and falls to 0 with the extension off. Every suppressed engine draw
has exactly one native draw at the same `refFormId`. An altered capture across the
grid edge, before and after.

**FLIGHT.** One question per wave: *"Does anything inside the cells you are
standing in look worse, or disappear?"* Refuter: an object that vanishes at close
range is the projected-size test running on a bound the engine and the file
disagree about — read `suppressedDraws` and the refusal word before anything else.

**MUST NOT.** Change what is streamed. Suppress an engine draw the module did not
itself draw. Apply the fade to AI, script or persistence distances.

**RE CANDIDATES, named as candidates and not as facts.** The engine's fade
compare site (shared with R3). The per-object draw-suppression site for a placed
REFR inside a loaded cell. Both need the Todd's treat-first treatment FO4CS's own
CONSTITUTION rule 5 requires for vanilla-engine research, with an RVA quoted per
build for every fact relied on.

---

## 3. The seams

bungo, 2026-09-11 10:5x, on the four places the loaded grid's edge shows,
verbatim: *"note these things"*. Each is listed with the rung that owns it.

| # | the seam | the rule | owner |
|---|---|---|---|
| 1 | **draws at the grid edge** | dither cross-fade `.lodi` instances OUT and the engine's full models IN over a band, instead of the engine's hard swap. The band's width is `gridEdgeBand` and it is a census field (CENSUS 5.4) | **R3** ships the band; **R5c** is what makes both sides ours |
| 2 | **terrain at the grid edge** | the inner band is drawn from the `.lodl`'s FINEST level, which equals the LAND heights exactly (BTD Height encoding: at quantum 8 the encoding is exactly lossless against `VHGT`'s own storage), plus the ring-0 weights blend — so no crack, no visible skirt, and a matching texture | **R2** |
| 3 | **shadows at the grid edge** | a blend band where the engine's cascades end and the identity far shadows begin. His 14:4x ruling makes this the same band as item 1: the grid-edge band blends shadows the way it blends draws, and a placement is in exactly one caster set at a time | **R1** states the law, **R3** ships the band |
| 4 | **one fade law** | the SAME screen-size fade thresholds inside and outside the grid. One set of four keys, read by both | **R3** owns the keys, **R5a** consumes them |

A fifth seam is not bungo's but the files': **the two row orders**. `.lodt` is
north-up in its tile table and inside every payload; `.lodl` is row-0-south. Both
conventions are live in this codebase and the mismatch has already cost one
consumer a Y mirror (VT, the row-order block). R2 owns it, and a `.lodt` with
`ROW_ORDER_NORTH_UP` clear is a refusal, not a hint (VT 3.4 rule 19).

---

## 4. The census is the acceptance instrument

`docs/LODGEN_CENSUS.md` is the spec, and it is not this page's to change. ~~Sixty~~
**Sixty-three** (re-counted 2026-09-23) field names in seven rows — `[ImprovedLOD]`, `.Ring`, `.Cluster`, `.Cards`,
`.Residency`, `.Shadow`, `.Fade` — each with its unit, what it counts, the
contract section it is read from, the scene change that must MOVE it, its complete
refusal list, and a default that accuses its own plumbing.

### 4.1 Which rung ships which rows

| rung | rows it fills |
|---|---|
| R0 | `[ImprovedLOD]` entire |
| R1 | `[ImprovedLOD.Ring]` entire; `draws`, `overflow`; `drawnPrimitives`, `engineFarHidden`; `shadowDraws`, `shadowIdentityUnique`, `shadowCasters[src]` (added 2026-09-23) |
| R2 | `[ImprovedLOD.Residency]` entire |
| R3 | `[ImprovedLOD.Cluster]` entire; `[ImprovedLOD.Fade]` entire; `[ImprovedLOD.Shadow]` entire |
| R4 | `[ImprovedLOD.Cards]` entire; `cardBytes` |
| R5 | `gridEdgeBand`, `suppressedDraws` |

Overlaps found 2026-09-23, not ruled:
* R3's "Shadow entire" now includes R1's `shadowCasters[src]`.
* "Fade entire" overlaps R1's `drawnPrimitives` and `engineFarHidden`, and R5's `suppressedDraws`.
* "Residency entire" overlaps R4's `cardBytes`.

The owner stated in each rung's own CENSUS paragraph is the proposal.

### 4.2 Every field obeys six rules, and they are not this page's invention

CENSUS 1.2, which is the three rules of 2026-09-04 21:33 plus the five properties
of `fo4cs-census-field`: **written and moves**; one key, one meaning, one
occurrence per row; the composed row fits its buffer, asserted against the
EXPORTED capacity constant and driven at the SATURATED case; a refusal names its
reason in words; a default accuses its own plumbing (`uncounted`, `unread`,
`unset`, `unchecked`, `unwired` — and `0` is never a default except where zero is
a measurement); a metric on no data refuses as `n/a`.

**The lane that ships a field owes its movement case RED with the assignment
removed.** A formatter test that builds the struct by hand proves the formatter
and not the wire — that is exactly the defect `fo4cs-census-field` was written
from, where a field had a formatter, a change detector, a nine-value enum, an
accessor, and no assignment, and four readers in a row diagnosed a feature from a
row that said nothing.

### 4.3 The cross-checks, which is what makes a runtime number falsifiable

CENSUS 6.2 states ~~thirteen~~ **sixteen** (2026-09-23). The ones a first flight can run with the file open:

* `instancesTotal <= instanceCount`, equal when the whole worldspace is loaded.
* The four-way sum law per bin.
* **0 occluder boxes forces the refusal `no_boxes`, never a silent 0** — this is
  the shape to look for in every module, and there is usually one.
* `clusterTriangles` at tolerance 0 equals the level-0 triangle count of the
  drawn set, exactly.
* `clusterTriangles` at any tolerance lies between the roots' covered triangles
  and the level-0 count, because the ladder is a partition of its own surface.
* `levelMax` equals the bake's own `levels 0..L`, or the loaded file is not the
  baked one.
* `tilesResident <= tilesPresent` at every level.
* `stale` is raised by exactly the soft key list of NATIVE 5 and by nothing else.

### 4.4 The first flight's expected numbers, where they are already known

| number | value | source |
|---|---|---|
| instances in the Sanctuary pair | 3,526, 0 dropped | NATIVE 9 |
| chunks present / dense | 10 / 12 | NATIVE 6 |
| occluder boxes, Sanctuary | 0, and the refusal is `no_boxes` | NATIVE 4.5.3 |
| occluder boxes, downtown Boston | 280 over 87 of 147 populated cells | NATIVE 4.5.3 |
| clusters per level 0..7 | ~~10,634 / 5,676 / 2,781 / 1,029 / 414 / 123 / 19 / 2~~ (that was the opt-in ladder) **10,634 at level 0 and none above, on the default bake** (2026-09-23) | NATIVE 3.5.3; bytes on disk |
| `CONE_OPEN` clusters | ~~14,604 of 20,678~~ **6,982 of 10,634** (default bake, 2026-09-23) | NATIVE 3.6; bytes on disk |
| draws for a whole worldspace | 8-14 mesh + 1-2 card buckets | NATIVE 7 step 7 |
| resident engine draws it replaces | 343-1,795 | NATIVE 7 step 7 |
| triangles at 1 px | ~~124,205 / 124,121 / 115,583 at 2k / 8k / 32k units~~ (that was the opt-in ladder). On the default bake it is level 0 at every tolerance, expected 124,205 at every distance (derived, not measured, 2026-09-23) | NATIVE 4.4.1 |
| ring-0 colour error vs the bake | mean 3.26 and 3.59 of 255; floor 13.70 and 15.15; ceiling 0 | VT 2.5 |
| a pyramid tile | ~~323,680 B without cover, 369,920 B with~~ **138,720 B without cover, 184,960 B with** (default); 323,680 / 369,920 with `--vt-height` (2026-09-23) | VT 3.3, VT 2.2 |
| resident `.lodo` for one worldspace | ~~about 67.8 MiB~~ **about 64.3 MiB** on the default bake (67.8 was the ladder; 2026-09-23), against 125.9-370.0 MiB of resident object geometry today, of which **textures are 76 percent** | NATIVE 7 |
| card fields, today | every one refuses | CENSUS 5.1 |

### 4.5 The checker that already exists

`tests/spells/lodgen_census_check.py` is read-only, needs no exe, and decodes a
pair with the INDEPENDENT decoder rather than the writer. Three verdicts and only
one is a pass: `ok`, `RED`, and **`not-derivable`** — a bake-time fact the
container does not carry, named one by one and counted separately, never as
passes. Measured: **59 checks, 0 failures, 31 census words the files cannot
carry** on both Sanctuary pairs (CENSUS 7). **That figure is historical since 2026-09-23.** Both pairs are `.lodo`
v3, which today's independent decoder refuses by name, so it cannot be re-run on them. No v4 re-measurement exists
yet (§5 row 25).

---

## 5. What the generator still owes the runtime

Named so an FO4CS session never discovers one in the middle of a wave. Nothing in
this table is a blocker for the rung it sits against unless the row says so.

| # | what is missing | what the runtime cannot do without it | owning lane | blocks |
|---|---|---|---|---|
| 1 | ~~**per-MNAM-slot instance totals in the `.lodi` header**~~ — **DONE 2026-09-16, lane NATIVE1c**: `slotInstances[4]` at `.lodi` **0xD4..0xE3**, v5. The reader refuses a file whose four totals do not sum to `instanceCount`, by name; the field gate checks that they also MOVE | `ringInstances[r]` has no bake-side denominator at all and can only be checked against the whole-file total. A *ring* is camera-relative, so no file can state one — this is the honest version | NATIVE1c/d | nothing; weakens an R1 cross-check |
| 2 | ~~**a per-base full-detail triangle count**~~ — **DONE 2026-09-16, lane NATIVE1c**: `crossPx16[0..1]` is now one u32 `fullTriangles`, `.lodo` v4. Measured 2,974 of 2,974 bases non-zero, 1,120 distinct values, max 149,282. The reader RECOUNTS it from the cluster table and refuses a mismatch. `crossPx16[2..3]` is still free | `ringTriangles` and `clusterTriangles` can only be checked by walking every selected cluster | NATIVE1c/d | nothing; weakens an R3 cross-check |
| 3 | ~~**a card count in the `.lodo` header**~~ — **DONE 2026-09-16, lane NATIVE1c**: `cardCount` at `.lodo` **0xD0**, v4. `cardCount > baseCount` is refused by name | `cardBases` has no denominator short of walking the whole base table | NATIVE1c/d | nothing |
| 4 | ~~**the aggregate outermost-band cards, and the forested-cell count bungo asked for before anything is built**~~ — **DONE 2026-09-11, lane CARDS-AGG** (found 2026-09-23): `--aggregate`, OFF by default; 2,631 of 3,685 tree cells, 60,605 trees. Never flown, and its source cards predate the 2026-09-19 repair (row 22) | `cardsAggregate` refuses `not_baked` and keeps refusing | **CARDS-AGG** | R4's aggregate half |
| 5 | ~~**a watertight bit in the `.lodo` mesh row's free flags**~~ — **DONE 2026-09-16, lane NATIVE1c**: `LODO_MESH_WATERTIGHT = 4`, set from zero boundary edges, `.lodo` v4. Measured 654 of 5,567 meshes; the gate requires the count to be strictly between 0 and the mesh count, because all-set and all-clear are both the bit not working | `no_boxes` can say THAT a worldspace has no occluders but not WHY. The generator knows: 2,617 of 2,982 meshes were refused for not being watertight | NATIVE1c/d | nothing |
| 6 | **the decoder's cell rule.** **Status 2026-09-23:** the code carries a fix. The writer's reader and the independent decoder both apply a cell-quantisation ambiguity band (`lodiCellAgrees`, `LODI_CELL_QUANT_TOL`). On a default pair the decoder printed `cellQuantAmbiguous 49`, band 0.126955 u. But no run on the downtown-Boston pair is recorded, and NATIVE does not document the band, so the row stays open until that run. Originally: The independent decoder refuses the downtown-Boston pair at instance 3359 — the only pair with occluder boxes — because a neighbour sits at 2.999985 cells from its chunk origin after quantisation and the decoder re-derives a different cell than the writer sorted on | no gate can decode the only pair that has boxes, so R3's occluder gate has no fixture | NATIVE1d | **R3's occluder gate** |
| 7 | ~~**`.BTO` chunk files are still written under the FO4CS target**~~ — **CLOSED 2026-09-16, lane BTOFREE1**: the five read-back passes still get their chunk, in `<mod folder>/lodgen_bto_scratch`, and the teardown moves the manifest sidecars into `meshes/terrain/<ws>/` and removes the chunks and the folder. The mod folder receives `.lodl .lodt .lodo .lodi .lodm`, the arrays, the heightmap and the sidecars. Way back: `--keep-bto` / panel row *Keep legacy .BTO chunks* (OFF), byte-identical to a pre-2026-09-16 bake. The stock target is untouched. Gate `tests/spells/lodgen_btofree.sh` | nothing left | — | nothing |
| 8 | ~~**the resource stack cannot see a `.pbrm` or a `.lodm`**: the vendored `BA2File` loose-file whitelist has neither extension, so `--resource` silently ignores both~~ — **DONE 2026-09-16, lane GENSMALL1** (found 2026-09-23): `--resource <dir>` serves loose `.pbrm` and `.lodm`; gate `resource_ext.sh` 12/0 | a PBRM-backed model bakes its objects legacy and its terrain PBR | a small generator lane | R4's pbr arm on a modded setup |
| 9 | ~~**our far-terrain sheets are DXT1 with 8 mips; all 6,120 of Bethesda's Commonwealth sheets are DXT5 with 10.** A DXT1 `_msn` has no alpha, and our chunk renders blue-purple beside vanilla's~~ — **CLOSED 2026-09-12 13:45, lane TERRAINFMT1** (found 2026-09-23). It shipped `--sheet-format vanilla` (DXT5, 10 mips; OFF, so `legacy` DXT1 with 8 mips stays the default) and `--msn-cache`. It MEASURED that the blue-purple was not the sheet format: it was the `.BTR`'s identity vertex colours, which have been OFF by default since 2026-09-12 (lane DEFAULTS1) | the stock-fallback arm of R2 looks wrong beside vanilla | TERRAINFMT1 | nothing native; the fallback's look |
| 10 | ~~**splat grading vs vanilla.** Vanilla's sheets are graded x0.82 uniformly and smoothed; ours are the raw blend. Measured mean difference 19.96/255 on one chunk, and it is NOT the roads~~ — **CLOSED 2026-09-12 03:23, lanes SPLAT1 + GRADE1** (found 2026-09-23). The "smoothed" half was the T = 341.3333 tiling, which landed 2026-09-11. GRADE1 REFUTED a uniform grade: the best gain per tile runs 0.615..1.241, on both sides of 1. It shipped `--grade K`, default 1.0, with no value recommended. What remains is which textures blend where, not a grade | R2's colours read brighter than vanilla's at the same ground | SPLAT1 | nothing; a look gap |
| 11 | ~~**no bake writes a card layer.** `cardLayer` is `0xFFFF` on every base and `cardCorpusHash` is 0~~ — **DONE 2026-09-24, lane CARDLINK1** (NOT FLOWN): a `--native --impostors --arrays` bake links its card arrays and writes cardLayer, cardCount, cardCorpusHash (**proposed R19**, NATIVE §4.13, not ruled) and FORCE_CARD; gate `tests/spells/lodgen_cardlink.sh` on Sanctuary (9 chunks, dim 4, 23 real tree card sets, `--impostors-from-level 0`): cardCount 23 of 2,970 bases, 23 of 23 layers resolve over 10 arrays, cardCorpusHash `65d2bf61ff72c5b2` = the contract recomputed outside the exe, moves to `f1b93d3ee8c01b3a` when one albedo texel changes, FORCE_CARD on 3,446 of 3,526 placements and none on a card-less base; the rung exe writes 0 / 0xFFFF / 0 | R4 has nothing to draw on a Commonwealth bake | OBJM / OBJC / OBJP | **R4** |
| 12 | **the pyramid's height sheet is opt-in** (`--vt-height`, default off)~~, and the CLI table (VT 5) does not list the flag although VT 2.2 names it~~. The CLI half is DONE: VT 5 and the command-line help list it (found 2026-09-23). The bake half is still bungo's (§6 (e)), and the VTBAKE1 fixture was baked WITH it | **R3's terrain shadow march has no height source beyond the inner band** on a bake that did not ask for it | a generator lane for the CLI row; bungo for the bake | **R3's march**, on a bake without it |
| 13 | **the asymmetric-drop proof on the (-32,0) dim-32 chunk has never been run** | the claim that the 16-bit index cap silently drops 6.17 percent of that chunk's placements is measured on the stock path but not re-measured against the native one | a generator lane | nothing |
| 14 | ~~**no `.lodt` container and no VT `.lodm` has ever been written to disk**~~ **PENDING: lane VTBAKE1 (2026-09-23 08:0x) wrote the first whole-Commonwealth `--vt` pyramid** into `scratchpad/vtbake1_20260923/bake/FO4CSLOD/Commonwealth/` at 08:17: 5 levels, 12,276 tiles, height ON, cover OFF, index 2,505 B. Its `--lodt-check` passed level 2 and was still running at 08:28. The row is not DONE until the lane closes, and the pyramid is not in any mod folder | R2 has no sample to develop against until the first pyramid bake | **VTBAKE1** | **R2** |
| 15 | **no v2 manifest and no texture-array output exists on disk** | R1's stock-fallback arm and R4's array arm have no fixture | the same bake | R1's fallback arm's gate |
| 16 | **the shipped FO4CS `.lodl` parser pins `kVersion = 1u`** and refuses 2 and 3 | a freshly generated landscape file is refused by the parser FO4CS already ships. The zero-effort way back needs no rebuild on our side: `WW_LODL_VERSION=1` | an FO4CS lane, R0 | **R0** on a v2/v3 file |
| 17 | **DONE on the census page** (found 2026-09-23; the page does not name the lane). `shadowCasters[src]`, `shadowMarchMs` and `shadowMapMs` are in CENSUS 5.3, and the caster count has two CENSUS 6.2 checks. Was: ~~**three census words this page needs that `docs/LODGEN_CENSUS.md` does not carry**~~: a caster count per SOURCE (his 14:4x ruling's instrument), and the march's own cost and the shadow map's own cost in milliseconds (his 15:0x ruling's instrument) | R1 and R3 cannot report what he asked them to report | a census-page lane here | R1's and R3's shadow gates |
| 18 | **the module still composes the OLD paths.** (2026-09-23: the one shipped reader composes `Data\Terrain\<EDID>.lodt`, the old folder AND the old extension; that extension now names the LDTX texture sheets.) The generator moved every FO4CS-target output under `Data\FO4CSLOD\` on 2026-09-16 (lane LAYOUT1, bungo 19:3x "The folder should be called FO4CSLOD maybe, so it'd be Data/FO4CSLOD, sound fine?"); §1.2 above is the new table. The shadow HeightMap under `Data\Textures\Terrain\<WS>\` did NOT move, precisely because a shipped reader composes it | the module opens nothing a bake from today wrote: every `.lodl`, `.lodt`, `.lodo`, `.lodi`, card and array path it builds points at the pre-move layout | an FO4CS lane (reader side); the generator half is done and gated by `tests/spells/lodgen_layout.sh` | R1 onward, the first time a fresh bake is flown |
| 19 | ~~**a stale pair cannot name the plugin that went stale**~~ — **DONE 2026-09-17, lane BAKEREC1**: every bake now writes `Data\FO4CSLOD\<ws>\<ws>.lodb`, a plain-text bake record carrying the load order with a per-file FNV-1a 64 over each plugin's BYTES. `lodgen --native-verify` reads it beside the pair and a `loadOrderHash` / `pluginCorpusHash` refusal now names the file and says ADDED / REMOVED / REORDERED / RESIZED / **EDITED at the same byte size** — the last being the case `loadOrderHash` is structurally blind to. `lodgen --bake-record <ws.lodb> [<plugins>]` prints the whole record as keyword lines and exits 1 when a listed plugin moved. Format: `docs/LODGEN_BAKE_RECORD.md`; gate: `tests/spells/lodgen_bakerec.sh` | an operator handed *this pair is stale* had to guess which of thirty plugins to re-bake for, and a same-size in-place edit did not fire the refusal at all | BAKEREC1 | nothing; it unblocks lane INCR1, which reads this record to decide which chunks to re-bake |
| 20 | **PENDING: IMPOSTORPBRM1.** Card specular weight, colour and IOR, and the TintMask applied in the bake. The scope was RULED 2026-09-23 07:5x and the lane is QUEUED. Where these live in the card sheets or the `.lodm` is not written yet (added 2026-09-23) | R4's pbr family cannot light a card the way its mesh is lit | IMPOSTORPBRM1 | R4's pbr look |
| 21 | **PENDING: IMPOSTORWIND1.** Sway from the tree model's own wind weights (bungo 2026-09-23: *"sway from the tree's model's own wind weights would be neat"*). The lane is QUEUED: research first, then the sheet layout for his ruling (added 2026-09-23) | R4's sway reads the baked `h^2 x (0.35 + 0.65 r)` weight | IMPOSTORWIND1 | nothing; a look gap |
| 22 | **PENDING: re-bake every card set older than 2026-09-19** (the azimuth repair; HANDOFF 2026-09-19 09:5x). That includes CARDS-AGG's sources and the four CARDS 9 libraries (added 2026-09-23) | R4 has no current-convention fixture at Commonwealth scale | a generator lane, or bungo's bake | R4's gate 1 fixture |
| 23 | **CANDIDATE, not ruled: the instance's MNAM slot is not stored.** The emitter knows which slot each instance drew (`LodiSrcInstance::mnamSlot`, `src/lodifile.h`), but the file only counts them (`slotInstances`). The vertex-AO and vertex-sky slices are matched to a mesh by LENGTH alone, and the viewer counts the mismatches (added 2026-09-23) | a reader cannot prove it picked the slot the AO was baked for when two slots' meshes share a vertex count | unowned | R1's AO and sky on a default (MNAM-slot) bake |
| 24 | **the census `version` word still reads `lodo3/lodi3`** with refusals `refused:v1` / `refused:v2`, and it has no slot for the `.lodl` or `.lodt` version (CENSUS 2) (added 2026-09-23) | R0's `version` word has no correct spelling to copy | a census-page lane | R0's census row |
| 25 | **the census checker's 59 / 0 / 31 has not been re-run on a v4 pair**, and the cited pairs are v3, which the decoder refuses (added 2026-09-23) | §4.5's figure cannot be reproduced today | a small generator lane | nothing; weakens §4.5 |
| 26 | **no default-settings Sanctuary pair on disk** (authored-only library, `.lodi` v7) (added 2026-09-23) | R0 gate 1 can pin only the `.lodo` and a one-chunk `.lodi` | a small bake | R0 gate 1's Sanctuary half |
| 27 | **the `.lodt` reader accepts up to 10 sheets and role 7, but VT 3.4 rule 13 says 1-6** (added 2026-09-23) | FO4CS cannot know which rule to copy | a VT-page lane, after §6 (s) | R0's `.lodt` validation |
| 28 | ~~**`cardCount` is bounded but never recounted** by the writer's own reader (added 2026-09-23)~~ — **DONE 2026-09-24, lane CARDLINK1**: `lodoRead` recounts it and refuses a mismatch by name; `--native-verify` on a pair whose cardCount was edited 23 -> 24 (CRC recomputed) refuses with `cardCount 24 but 23 base row(s) name a card layer`, the rung exe does not name cardCount | a wrong `cardCount` passes validation | a small generator lane; R0 recounts it anyway | nothing |

---

## 6. Open rulings for bungo

Collected in one list so none of them is discovered mid-wave. The first four are
the ones that change what gets built.

**(a) Build the object library from each base's near `MODL` instead of its `MNAM`
slots?** — ~~**RULED 2026-09-16 by bungo ("Also do the parked", 11:1x), BUILT the
same day by lane NATIVE1c, `--library near` is the default.**~~

**RULED 2026-09-16, then REVERSED by bungo 2026-09-17:** *"Authored LODs only, only the trees get baked for
the cards"* (WW_CHANGES 2026-09-17, "Authored LOD models only").

* **The default is `--library mnam` with NO ladder.** `--native-ladder` and `--library near` are opt-in
  (`src/nifcli.cpp`: `bool lgNativeLadder = false, lgNativeOccluders = true;`, `bool lgLibraryNear = false;`).
* Measured on the urban region, 33,123 placements: `.lodo` 223,498,874 B -> 6,204,388 B, 142,138 triangles,
  136 materials, every mesh one level.
* Corrected 2026-09-23. The history below is kept, because its measurements are still true of the two opt-in
  arms.

The question as it stood: the ladder is correct and barely selectable — the
median level-1 cluster deviates 3.80 percent of its model's diagonal and reaches
one pixel only past 52,100 units, because "full detail" in the file is already
Bethesda's LOD mesh at a mean of 47.7 triangles for a whole building. Building
from the near model needs **no format change at all** and is what his 10:3x
"extend the cluster LOD inward" ruling implies. Cost: a much bigger library and
bake time. (NATIVE 3.5.4.)

**What it measured**, both arms of the same exe on the same 9-chunk Sanctuary
region, from `tests/spells/lodgen_ladder.sh` and `lodgen_ladder_select.py`:

| | `--library mnam` (~~the way back~~ the DEFAULT since 2026-09-17, without the ladder) | `--library near` (~~default~~ opt-in since 2026-09-17) |
|---|---|---|
| median level-1 deviation | 65.8751 units, **3.8028 %** of the diagonal | 3.5537 units, **0.1521 %** |
| median first step reaches 1 px at | **90,316 units** | **4,872 units** |
| level-1 clusters | 5,676 | 152,577 |
| `Commonwealth.lodo` | 9,657,316 B | 225,399,755 B (**23.3x**) |
| library triangles | 252,268 | 7,572,082 (**30.0x**) |
| ladder depth | levels 0..7 | levels 0..10 |
| bake, whole arm | 13 s | 83 s |

The 3.8028 reproduces this page's own 3.80 from the file's bytes, which is the
control that says the two arms are the thing this row was about. **The first step
now lands 18.5 times closer**, and the way back, `--library mnam`, is gated
against a bake on the exe that predates the lane. `.lodi` unaffected by the
switch. ~~**R3's selection and R5c can be written against 4,872 units.**~~ (Struck 2026-09-23: this is true only of the
opt-in near library.) The cost
is the 23x library and it is stated, not hidden: `docs/LODGEN_NATIVE_LODO_LODI.md`
§3.5.7.

**(b) The pixel tolerance, given (a).** 1 px is his own number (10:4x). Until (a)
lands, 1 px selects the first ladder step nowhere in the Commonwealth. Ship 1 px
and accept that the cut saves 0-16 percent, or ship a coarser default meanwhile?
**2026-09-23:** a default bake has no ladder for the tolerance to act on. This question
matters only on an opt-in ladder bake.

**(c) Four END-menu rows for the fade thresholds.** He asked for *"a menu row +
INI key each"* on 10:4x. The standing rule is that no row lands without him, so
this page asks rather than assumes. Four rows, or one row and four INI keys?

**(d) The shadow numbers.** `iFarShadowMode` defaults to hybrid by his 15:0x
ruling, but three numbers are unstated: the shadow view's own tolerance, the far
shadow map's resolution, and whether the march's cost rising with a low sun needs
a cap. His own acceptance bar forbids a cap that shortens the far reach
(`fo4cs-altered-capture` §4, his 2026-09-04 words: *"if the sun position is low,
the far shadows genuinely get longer, for trees at very far distance in the LOD
range"*), so a cap would need its own dispensation.

**(e) The height sheet.** Must the full Commonwealth bake run with `--vt-height`?
R3's terrain shadow march has no height source beyond the inner band without it,
and it more than doubles a pyramid tile (184,960 bytes of height against 138,720
of the three colour-class sheets).

**(f) The residency budgets.** The library budget for the finest levels (his zoom
answer of 10:5x makes the finest levels the thing a budget must bind on) and the
pyramid's tile budget. Both are numbers nobody has measured.

**Carried from the generator lanes, each already in the handoff's owed list:**

**(g) The mesh/material sort sits inside the CELL**, not mesh-major across the
chunk, because mesh-major would leave one cell's instances in up to sixteen runs
and the 8-byte cell-range row cannot describe that — and that blob is what the
near-field suppression reads. Keep, or mesh-major with a second index?
(NATIVE 2.1, NATIVE 11 Deviation 6.) **R5c reads the cell ranges, so this is R5c's
question too.**

**(h) The placed REFR form id stays in the COLD record**, at the instance's own
index, rather than growing the hot record from 24 to 32 bytes — which is +33
percent on the one buffer the per-frame cull dispatch reads, about 1.2 MiB on a
full Commonwealth, to duplicate a number already there. Keep, or duplicate?
(NATIVE 4.1a, NATIVE 11 Deviation 7.)

**(i) The ladder simplifies on a POSITION weld**, so levels 1 and up carry the
first contributor's UVs where two vertices shared a quantised position — 117,722
welds merged differing UVs over 263,876 source vertices. Level 0 never uses the
weld. Accept the texture shift on coarse levels, or weld on position AND UV and
accept far fewer laddered meshes? (NATIVE 11 Deviation 9.) **Moot on the default bake
since 2026-09-17**, which has no ladder and so no weld (2026-09-23).

**(j) Where the ground cover lives** — the mask sheet's alpha (shipped) or the
colour sheet's alpha (`--vt-cover-in-color`). Identical bytes either way,
measured; the engine tolerates a BC3 colour sheet (2,001 of 2,001 of vanilla's own
chunk colour sheets are DXT5); the only discriminator is meaning, because the
colour sheet's alpha is the object family's OPACITY slot and a consumer written
against that law alpha-tests it (VT 2.2a). The director's standing recommendation
is to keep the mask.

**(k) `.BTO` under the FO4CS target** — **RULED 2026-09-16** (bungo, 2026-09-12
18:3x: "essentially, no legacy vanilla file types are now used by us or baked in
the FO4CS lod bake" — "Except the data we're reading from for the bakes"). They
are DROPPED. The bake still builds one per chunk, because five passes read it
back, but it builds it in `<mod folder>/lodgen_bto_scratch` and removes it once
they have; the `.BTO.manifest.txt` sidecar moves into the mod folder and stays.
The exact way back is `--keep-bto` on the command line and the panel row *Keep
legacy .BTO chunks* (default OFF, FO4CS only), and it is byte-identical to a bake
from before that date. The stock target does not change at all. Lane BTOFREE1;
gate `tests/spells/lodgen_btofree.sh`.

**(l) The CLI's `--candidates` default** is still `missing` while the panel's
"Trees only" row is ON by default. Make them agree?

**(m) The shared material resolver.** 65 of 270 road shapes name their material as
an absolute Bethesda build path and carry an empty texture set. The road pass
fixes it for itself; the shared object loader still drops those textures, and
fixing it there moves the byte-identity gates. Extend the fix?

**(n) `--road-cover-suppress` default 1.0** — no grass and no grass tint under a
road. Nothing was measured about vanilla here, because vanilla ships no cover
plane to measure.

**Added 2026-09-23 (PLANSYNC1). Each of these is OPEN. Every line quotes where it is recorded as open; none is
decided here.**

**(o) The card grid and transition defaults.**
* The grid: N4, N8 or N16.
* The cut: stipple, average or strongest frame.
* The slider's default value. He ruled "the crisp side" but gave no number.

HANDOFF 2026-09-23 07:2x: "open: his pick of grid + cut default, slider lane owed". The IMPOSTOR16 N8 measurement
is in R4.

**(p) The coverage-cut domain FO4CS tests in.** Either:
* the encoded `coverage.test` 128 (CARDS 4, R4 point 2), which is a coverage fraction of 16/255; or
* vanilla's decoded 128/255, the NifSkope viewer's default since 2026-09-22 (IMPOSTORFIN1).

No ruling for FO4CS was found.

**(q) Card lighting and layout:**
* **Gloss and AO default.** HANDOFF 2026-09-23 05:5x: "open: gloss/AO default = his call".
* **The `_n` height <-> sway swap.** It is PREPARED and not applied. If ruled, it adds `"nlayout": 2` and older
  sets are refused by name (WW_CHANGES 2026-09-22).
* **Whether to keep the lit card** (WW_CHANGES 2026-09-22).
* **The minimum frame size for fine-twig trees.** IMPOSTORFIX5 2026-09-19 20:06: "NEW RULING OWED".
* **Coverage as opacity** (IMPOSTORLOOK1 2026-09-19).
* **A raised second row on the 22.5-degree ring** (IMPOSTORRING1).

**(r) Does the new module accept a v1 `.lodl`?**
* The FO4CS R0 brief says to refuse v1 by name.
* Our reader accepts 1-3, and `WW_LODL_VERSION=1` is the documented zero-effort way back (BTD, "A CONSUMER
  EXISTS").
* Refusing v1 removes that way back.

**(s) Which `.lodt` sheet rule FO4CS follows** (§5 row 27).
* The source accepts up to 10 sheets and role 7.
* VT 3.4 rule 13 says 1-6 sheets and refuses any role above 6.

**(t) `cardCorpusHash` is zero on every bake.** HANDOFF 2026-09-17 AUDIT1, "ROWS FOR BUNGO".

**(u) Store the instance's MNAM slot in the `.lodi`, or keep matching AO and sky slices by length?** (§5 row 23.)
This is a candidate question from this lane, not one bungo has been asked.

---

## 7. Glossary — the words that mean different things on the two sides

Written because four things in this system are called a "level" and two are called
a "mask", and an FO4CS session reading this page has its own vocabulary already.

| word | here | in FO4CS |
|---|---|---|
| **far field** | the whole distant world: objects, terrain, cards | the far SHADOW module, `src/FarField/` — heightmap-marched distant shadows. **These are different things and the module names collide.** Improved LOD consumes what FarField produces and vice versa |
| **ring** | a census BIN, never a selection rule. NATIVE 4.4 forbids selecting by ring; CENSUS 1.1 keeps the word only because it is the vocabulary the stock bake and bungo both use. The bin edges are the census field `ringEdges` and a per-ring number printed without them refuses | the `[TerrainManager]` distance bands — `fBlockLevel0Distance` and its siblings, read live from the user's own INI |
| **level** (four of them) | `.lodo` cluster `level`: 0 = FULL detail, deeper = coarser. `.lodt` `levelIndex`: 0 = the FINEST level of the pyramid set. `.lodl` block level: 0 = the FINEST, `levelCount - 1` the coarsest. `rep[0..3]`: MNAM's four positional mesh SLOTS, which are not a ladder at all — **but on the default bake since 2026-09-17 they are the only distance steps there are**: the cluster ladder is opt-in and `.lodo` `level` is 0 everywhere (2026-09-23) | `dim` 4 / 8 / 16 / 32, the chunk's edge in cells |
| **mask** | `.lodt` role 5 — the `rmaos` sheet, R roughness, G metallic, B AO, A ground cover | the far-shadow visibility mask (`FarFieldMaskPolicy.h`), a per-pixel occlusion term |
| **identity** | two things, and NATIVE 4.1c is the law: the per-placement identity is the instance INDEX (u32, unique file-wide); `cold.identity` is the STOCK bake's 16-bit index, unique only inside the stock chunk; **the far-shadow caster identity is the GROUP** (`.lodi` v7 group table, NATIVE 4.9, §9; added 2026-09-23) | the 16-bit object index in vertex colour R+G that the far-shadow pass keys on today (`FarFieldLodBtoChannels.h`) |
| **arm** | a serving arm word in a census row — `native` / `stock-fallback` / `off`, or a refusal by name | the same. Both constitutions carry MODULES AND FALLBACKS verbatim |
| **census** | the same in both: `[Tag] key=value` lines a reader hits first | the same |
| **cluster** | a unit of at most 16 triangles and 48 vertices of ONE mesh and ONE material, with a sphere, a cone and an error (NATIVE 3.2) | no existing meaning; the word is free |
| **tolerance** | the ONE global pixel tolerance the cluster cut runs at, and the shadow view's own coarser one | no existing meaning |
| **height** | the `.lodt` role-4 sheet and the `.lodl`'s stored heights, both `height/8 + 32767` | `HeightMap.dds`, which R3's ruling makes the fallback arm |

---

## 8. Provenance

`ww-contract-provenance`. Hashes were taken BEFORE the pages were read and
re-derived AFTER the last edit of this page. **This page cites no `src/` line
number**, in either repository: lane BAKEPERF1 owned `src/` in this tree while it
was written, and the FO4CS tree is another project's live worktree. **Since 2026-09-19 (§9) and
2026-09-23 (this footer's version table), the page also cites source by ANCHOR**, with the
quoted text beside each cite, re-derived at the end of lane PLANSYNC1. Every other claim
above is anchored to a contract SECTION, which is stable across edits, or to a
ruling paragraph in `HANDOFF.md` quoted verbatim.

### The contract pages this plan is written against

**Re-stamped 2026-09-23 by lane PLANSYNC1.** Every page except ARRAYS had moved since the
2026-09-11 stamps, which are kept struck below the new table for comparison. IMPOSTOR_SPEC and
BAKE_RECORD are new:

| page | key | sha256 (16) | bytes | lines |
|---|---|---|---|---|
| `docs/LODGEN_NATIVE_LODO_LODI.md` | `NATIVE` | `16ea97a12f7d1357` | 146,324 | 2,276 |
| `docs/LODGEN_TERRAIN_VT.md` | `VT` | `34ea00bc46a0bb12` | 197,970 | 3,032 |
| `docs/LODGEN_BTD_FORMAT.md` | `BTD` | `400f74b5f223c504` | 80,402 | 1,424 |
| `docs/LODGEN_LODM_FORMAT.md` | `LODM` | `901892e3ff76489b` | 35,381 | 500 |
| `docs/LODGEN_CARD_SHEETS.md` | `CARDS` | `ded33067ce283368` | 51,335 | 876 |
| `docs/LODGEN_TEXTURE_ARRAYS.md` | `ARRAYS` | `64f9154b71f83631` | 13,608 | 256 |
| `docs/LODGEN_MANIFEST_FORMAT.md` | `MANIFEST` | `14ba22c35d3bf1e9` | 12,941 | 271 |
| `docs/LODGEN_CENSUS.md` | `CENSUS` | `7ccb5728c48f55b6` | 54,185 | 510 |
| `docs/LODGEN_IMPOSTOR_SPEC.md` | `IMPOSTOR_SPEC` | `328a3e3205c4ec8f` | 39,300 | 659 |
| `docs/LODGEN_BAKE_RECORD.md` | `BAKE_RECORD` | `cb695654b17817cf` | 19,913 | 427 |

The 2026-09-11 stamps (superseded):

| page | key | sha256 (16) | bytes | lines |
|---|---|---|---|---|
| ~~`docs/LODGEN_NATIVE_LODO_LODI.md`~~ | ~~`NATIVE`~~ | ~~`067eeaa70e2924fe`~~ | ~~79,504~~ | ~~1,253~~ |
| ~~`docs/LODGEN_TERRAIN_VT.md`~~ | ~~`VT`~~ | ~~`13efc51896021bae`~~ | ~~75,094~~ | ~~1,211~~ |
| ~~`docs/LODGEN_BTD_FORMAT.md`~~ | ~~`BTD`~~ | ~~`376b372c32a47dfb`~~ | ~~75,795~~ | ~~1,358~~ |
| ~~`docs/LODGEN_LODM_FORMAT.md`~~ | ~~`LODM`~~ | ~~`47f2b4ee6b4938ba`~~ | ~~28,378~~ | ~~411~~ |
| ~~`docs/LODGEN_CARD_SHEETS.md`~~ | ~~`CARDS`~~ | ~~`4edb4a9c8703185d`~~ | ~~42,585~~ | ~~723~~ |
| ~~`docs/LODGEN_TEXTURE_ARRAYS.md`~~ | ~~`ARRAYS`~~ | ~~`64f9154b71f83631`~~ | ~~13,608~~ | ~~256~~ |
| ~~`docs/LODGEN_MANIFEST_FORMAT.md`~~ | ~~`MANIFEST`~~ | ~~`ba5ca72c83991ad5`~~ | ~~12,399~~ | ~~264~~ |
| ~~`docs/LODGEN_CENSUS.md`~~ | ~~`CENSUS`~~ | ~~`521d0a5fa0de05f8`~~ | ~~35,284~~ | ~~448~~ |

(2026-09-11:) Those six of the eight that `docs/LODGEN_CENSUS.md` stamps in its own provenance
footer carry **the same hashes there**, so nothing moved under either page between
lane CENSUS1's close and this one. **All eight hashes were re-derived after this
page's last edit and none had moved** (`ww-contract-provenance` step 5), so this
footer describes the state the page was FINISHED against and not the state it was
started against.

### The contract VERSIONS this plan consumes

**Rewritten 2026-09-23 by lane PLANSYNC1**, from the WRITER SOURCE first and the contract pages second. The
version constants were read last of all (`ww-contract-provenance` step 4). The 2026-09-11 table said `.lodo` 3 and
`.lodi` 3. That was wrong by the time lane NATIVE1c landed on 2026-09-16, and this page's own §5 had already
contradicted it.

**Writer, reader, and what a consumer refuses:**

* **`.lodo`**
  * Writer: **4**, `src/lodofile.h` `constexpr quint32 LODO_VERSION = 4;`.
  * Reader accepts: 4 only.
  * Refuse: 1, 2 **and 3** BY NAME. A v2 has no ladder table. A v3 has `crossPx16` where v4 has
    `fullTriangles`, and has no `cardCount` and no WATERTIGHT bit. Any other version is refused as "this reader
    knows 4".
* **`.lodi`**
  * Writer: **3 to 9 by option; the DEFAULT bake writes 7**. The constants run from `src/lodifile.h`
    `constexpr quint32 LODI_VERSION = 3;` to `constexpr quint32 LODI_VERSION_SCRAPPABLE = 9;`.
  * Reader accepts: 3, 4, 5, 6, 7, 8 (retired and still read) and 9.
  * Refuse: 1 and 2 BY NAME, and anything outside 3..9. **The header is 256 B on v3-v6 and 512 B on v7-v9**, and
    the CRC window follows it.
* **`.lodt`**, magic `LDTX`
  * Writer: **2**.
  * Reader accepts: 2 only, with up to 10 sheets **including the retired role 7**. VT 3.4 rule 13 still says 1-6
    (§6 (s)).
  * Refuse: version 1 BY NAME (its third sheet was role 3 `data`); `LODT` and the retired `LODV`, each named;
    role 3, by name.
* **`.lodl`**, magic `LODT`
  * Writer: **2 by default**, `src/lodtfile.h` `int headerVersion = 2;`. It writes 3 only with
    `--water-bodies`. `WW_LODL_VERSION` overrides the version (1..3), and `WW_LODT_VERSION` is refused as
    retired.
  * Reader accepts: 1, 2 and 3 (`src/lodtfile.cpp` `constexpr quint32 LODL_VERSION = 3;`).
  * Refuse: `LDTX`, by name; anything outside 1..3. Row 0 is SOUTH. **The shipped FO4CS parser pins
    `kVersion = 1u`** (§5 item 16), and whether the new module accepts v1 is §6 (r).
* **`.lodm`**
  * Writer: envelope **1**, payload **1**, `src/io/lodmfile.cpp` `static const quint32 LODM_VERSION = 1;`.
  * Reader accepts: 1.
  * Refuse: a third `family` word. That is a hard refusal, not a fallback. New optional keys since 2026-09-11:
    `card.conv` / layer `conv`, and `kind: "aggregate"`. `"nlayout": 2` arrives only if the `_n` swap is ruled.
* **manifest**
  * Writer: `# lodgen manifest 2`.
  * Refuse: a v1 manifest, which has no header line at all and whose rows carry only eight fields.
* **texture arrays**
  * Writer: sidecar `# lodgen texture arrays 5`.
  * Refuse: nothing listed.

**Every `.lodo` / `.lodi` bump since v3: what it added, and the rung that reads it.** The authority is the
version comment block in `src/lodofile.h` and `src/lodifile.h`.

* **`.lodo` v4** (lane NATIVE1c, 2026-09-16). `crossPx16[0..1]` became one u32, `fullTriangles`; `cardCount`
  went in at 0xD0; the mesh flag `LODO_MESH_WATERTIGHT` took bit 2.
  * R0 validates `fullTriangles` and recounts it.
  * R3 cross-checks `clusterTriangles` against it.
  * R4 uses `cardCount` as the `cardBases` denominator.
  * R3's `no_boxes` names its reason from the WATERTIGHT bit.
* **`.lodo` v5** (HORIZON3, a subdivided library) was never written by any exe and was removed whole. No rung
  reads it.
* **`.lodi` v4** (lane CARDS-AGG, 2026-09-11) added the aggregate impostor rows. R4's aggregate half reads them.
* **`.lodi` v5** (lane NATIVE1c, 2026-09-16) added a placement-AO byte per instance (0xFF = not measured), and
  `slotInstances[4]` at 0xD4..0xE3.
  * R1 applies the AO on the draw.
  * R0 and R1 use `slotInstances` for the `ringInstances` cross-check.
* **`.lodi` v6** (2026-09-18) added the per-vertex AO stream at 0xF4 / 0xFC. R1 reads it for the draw's AO.
* **`.lodi` v7, `GROUP_SKY`** (2026-09-18), **the default**, added:
  * the group table at 0x100 / 0x108 / 0x10C, stride 2, ids dense per chunk;
  * the per-vertex sky stream at 0x110 / 0x118;
  * the 512-byte header.

  R1 and §9 read the group as the far-shadow caster identity; R1 reads the sky visibility.
* **`.lodi` v8, `HORIZON`** (lane HORIZON1, 2026-09-18), RETIRED 2026-09-19 by lane HORIZONOUT. Readers still
  accept it, and nothing draws its stream. No rung reads it.
* **`.lodi` v9, `SCRAPPABLE`** (lane HORIZON3, 2026-09-19) is the v7 layout plus instance flag bit 6. It is
  written only with `--scrappable`. §9 rule 4 reads it, to drop workshop-owned casters.

**The sources the table above was read from** were hashed before reading and re-hashed after this page's last
edit (steps 1 and 5):

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodofile.h` | `44b11f9ebe0ea0cb` | 31,969 | 584 |
| `src/lodofile.cpp` | `f7c78706f0d6d8d0` | 100,894 | 2,231 |
| `src/lodifile.h` | `5d993d1ae8eebe8d` | 44,463 | 798 |
| `src/lodifile.cpp` | `f0a608321df133f5` | 122,812 | 2,282 |
| `src/lodtfile.h` | `c61cc41ea8a75f6e` | 21,597 | 436 |
| `src/lodtfile.cpp` | `6809fe6576ff84eb` | 142,033 | 3,664 |
| `src/io/lodvfile.h` | `c09ca070809c2cb9` | 11,637 | 255 |
| `src/io/lodvfile.cpp` | `0ee14f45e3609212` | 35,888 | 842 |
| `src/io/lodmfile.cpp` | `f3d9a99b7a12677b` | 4,334 | 115 |
| `src/nifcli.cpp` | `c791e48b49aca102` | 416,682 | 8,780 |
| `src/nativeemit.cpp` | `faa09ee1dee50ff9` | 139,924 | 3,135 |
| `src/lodgen.cpp` | `871c4d2082efee81` | 636,250 | 14,451 |

~~The 2026-09-11 table~~ (superseded 2026-09-23; kept for comparison):



| container | version | a consumer must refuse |
|---|---|---|
| ~~`.lodo`~~ | ~~**3**~~ | ~~versions 1 and 2, BY NAME — a v2 library has no ladder table at all, so a v3 reader that accepted it would draw the whole library at full detail at every distance~~ |
| ~~`.lodi`~~ | ~~**3**~~ | ~~versions 1 and 2, BY NAME — a v2 instance table has both occluder offsets at zero, which reads as "this worldspace occludes nothing"~~ |
| ~~`.lodt`~~ | ~~**2**, magic `LDTX`~~ | ~~version 1 BY NAME (its third sheet was role 3 `data`, and three of those four channels no longer exist); and `LODT` and the retired `LODV` each named rather than lumped into "bad magic"~~ |
| ~~`.lodl`~~ | ~~writer defaults **2**, reader accepts **1, 2, 3**, magic `LODT`~~ | ~~**the shipped FO4CS parser pins `kVersion = 1u`** — §5 item 16~~ |
| ~~`.lodm`~~ | ~~envelope **1**, payload **1**~~ | ~~a third `family` word, which is a hard refusal and not a fallback~~ |
| ~~manifest~~ | ~~`# lodgen manifest 2`~~ | ~~a v1 manifest, which has no header line at all and whose rows carry only eight fields~~ |
| ~~texture arrays~~ | ~~sidecar `# lodgen texture arrays 5`~~ | ~~—~~ |


### The rulings quoted, and where they live

Every bungo quotation above is from `HANDOFF.md` in this tree. **That file moved
while this lane was writing** — the director appended the shadow rulings of
2026-09-11 14:4x -> 15:0x during the lane — so its hash is stated as two values
rather than one: **`81fdc08dfa8b7707`** (297,028 bytes, 4,452 lines) when this
lane started and **`8a7fce2f67c7f898`** (302,554 bytes, 4,531 lines) when it
finished. The shadow rulings arrived in that difference and were read in full from
the file, not from a summary; they are quoted in R1, in R3 and in §3 item 3.

### The FO4CS tree, read READ-ONLY

`E:\Projects\Fo4CommunityShaders`, 2026-09-11: `wt-fixfirst/CONSTITUTION.md`,
`wt-fixfirst/ROADMAP.md` (the LOD campaign block), `Codex/HANDOFF.md` (the
2026-09-09 15:34 and 15:35 rulings), `Codex/lod-fo4-vs-fo76-comparison.md`,
and the FarField module as the worked example of a loader plus a draw path
(`src/FeatureModule.h`, `src/FarField/FarFieldShadowsSettings.h`,
`src/FarField/FarFieldLodBtoChannels.h`, `src/FarField/FarFieldLodtSource.h`).
**Nothing in that tree was written.**

### Skills invoked

`ww-contract-provenance` (this footer, and the read order), `ww-census-contract`
(§4's shape and the read-from citations), `fo4cs-census-field` (§4.2),
`fo4cs-menu-row` (§2's key tables and the row rule), `fo4cs-ini-edit` (why this
page proposes keys and writes none), `fo4cs-wave-integrate` (why a rung is a
wave), `fo4cs-flight-brief` (every flight above), `fo4cs-altered-capture` (every
picture gate above).

Written by lane PLAN-FO4CS, 2026-09-11.

## 8. RULED 2026-09-16 12:1x: the Cell Manager campaign (after the lodgen queue)

bungo, verbatim: "we should probably start by being able to load all cell types
in the 3d viewport, this would get a new manager type. Cell manager. Then we
need to replicate the lighting 1:1 with the vanilla game". Context: an offline
probe bake for FO4CS (his 2026-08-03 hybrid ruling would be reopened by it) and
FO4 lighting in NifSkope. PARKED behind the lodgen queue (NATIVEVIEW2, VT1,
GENSMALL1, NATIVE1c, BTOFREE1, Sanctuary bake, commit). No lane until he names one.

Phases as understood, each its own brief later:

1. **Cell Manager** (new manager beside `gamemanager` / `lodgenmanager`): load
   any CELL by form ID or editor ID, exterior (worldspace cell + LAND, the
   lodgen ESM reader already does this) and interior (no LAND; CELL XCLL
   lighting, LGTM template, XCLW water, room/portal markers), every REFR with
   base, transform, scale, enable-parent chain, SCOL expansion, LIGH refs with
   their XLIG/XRDS data. Draws the cell in the viewport as placed NIFs. Fallback:
   the current one-NIF document. Module-level switch, ships OFF by default.
2. **Vanilla lighting 1:1**: the BSLightingShaderProperty model as the shipped
   game evaluates it. VANILLA research = Todd's treat FIRST (RVA per build), ground truth =
   RenderDoc captures of the stock game, plus the stock shader bytecode
   (`Fallout4 - Shaders.ba2`) and FO4CS's own ported vanilla HLSL. Inputs from
   the ESM: WTHR/CLMT sun + ambient (DALC directional ambient), LGTM/XCLL
   interior ambient, fog, placed lights (point/spot, falloff, shadow flags).
   Gate: measure, don't eyeball -- pixel comparisons against captures, a
   documented invariant per term. This IS viewport renderer work: it needs his
   explicit signal per the "PBR awaits signal" rule; the vanilla evaluator is
   separate from the PBR (FO4CS) evaluator.
3. **Probe bake** (`.lodp` or similar, lodgen sibling): path-traced transport per
   probe using the FO4CS material model, relit live in-game (Division trick).
   Exteriors first, interiors once phase 1 loads them. Reopens the 2026-08-03
   hybrid ruling only as a third feed; FO4CS reader last by standing order.

Related parked rows: `docs/LODGEN_WEATHER_SHEETS.md` (weather sheets, enable
groups, cell-override `.lodl`).

### 8.1 The two bricks after it (bungo 2026-09-16 12:2x, thinking, NOT ruled)

"there's two major remaining bricks then, .esp and .esm record editing directly
in nifskope, previewing each record and the data it holds. And then cell editor
already built into nifskope".

Order that makes each brick land on the one below: Cell Manager (read) ->
record VIEWER (read, definition-driven from the cached wbDefinitionsFO4.pas,
flat Name|Value tree per his UI rule) -> record EDITOR (write; gate = the
vanilla corpus round-trips byte-identical, whole ESM not a sample) -> cell
editor (place/move/scale REFRs, writes an .esp overriding the cell) ->
precombine/previs regeneration (the real wall: an edited FO4 cell needs its
precombined NIFs + previs rebuilt or it breaks; lodgen's merge path is the
starting point). Python precedent: tools/esp_lib.py (AEGIS/BoS builders).

### 8.2 RULED 2026-09-16 16:5x: per-asset LOD mesh generation (after the lodgen queue)

bungo: "I think it should be done on individual asset level, but if a vanilla
model features a lod mesh, it wins over ours." then "Except tree impostors".

- A "Generate LOD mesh" spell per asset: open the full NIF, run the lodgen
  ladder simplifier (meshoptimizer + foliage refusal + the 70 % silhouette floor
  over eight horizon directions, all landed by NATIVE1c) per level, write each
  level as a BSTriShape NIF beside the source in the vanilla layout
  (`meshes\LOD\<path>\<name>_LOD_0..3.nif`); second lane = the single-sheet
  material bake + UV remap that makes it one draw (`textures\LOD\<name>_LOD_d/_n/_s.DDS`).
- PRECEDENCE: a base record whose MNAM names a shipped LOD model keeps it; ours
  fills only empty MNAM slots / bases with no LOD model. EXCEPTION: tree
  impostors. Vanilla's `_LOD_FLAT` tree billboards (meshes\Landscape\Trees) LOSE
  to our cards/aggregates (measured AO byte per placement, NATIVE1c).
- ADDED 2026-09-16 17:1x, bungo: "In the future, we should be able to bake lods
  for objects individually, then have the lod gen use these if they do exist."
  -> the lodgen library resolver takes a THREE-step precedence per base:
  (1) vanilla MNAM LOD models (shipped) win; (2) else our per-asset baked
  `_LOD_0..3.nif` beside the source in meshes\LOD (found by path, or by an
  MNAM entry the record editor wrote later) become the ladder's levels 1..N
  under the near MODL at level 0; (3) else the ladder simplifies on the fly as
  NATIVE1c landed. Tree impostors always ours (cards). Gate: a base with a
  per-asset bake produces a `.lodo` whose ladder triangle counts equal the baked
  files', and removing the files reverts to (3) byte-for-byte.
- Vanilla corpus for the gate: 1,186 `_LOD_*.nif` under meshes\LOD (8 folders),
  529 DDS under textures\LOD, both in E:\Tools\Fallout 4\DataUnpacked\Data.

### 8.3 ASKED 2026-09-16 17:0x: preview an octahedral tree impostor in the viewer as the game will react

bungo: "Is there a way for me to preview a generated octahedral impostor for a
tree in nifskope? I want to see it reacts as it does in game." ~~Today: NO.~~

**DONE since 2026-09-19, under other lane names** (found 2026-09-23):
* Lanes IMPOSTORSHOW and IMPOSTORFIX1 built the viewer card draw: view to grid, the three-frame blend,
  `frameOffset`, decoded coverage, height parallax and pixel depth, and baked AO.
* Its gate is `tests/spells/impostor_draw.sh`, run against an independent Python reference.
* IMPOSTORLIGHT1, AA1, TEAR1, FIN1 and IMPOSTOR16 followed on 2026-09-22 and 2026-09-23.
* "IMPOSTORVIEW1" never launched under that name.

The original text follows. The
bake photographs the N x N hemi-octahedral frames (docs/LODGEN_IMPOSTOR_SPEC.md,
gate lodgen_octahedral.sh) and the viewer is the camera for that bake, but the
runtime BLEND (view direction -> the three frames of the (N-1)^2 triangle mesh
between frame centres, spec s"Cards") is the consumer's and FO4CS's reader is
last by standing order. Parked lane IMPOSTORVIEW1: a viewer card mode that draws
one camera-facing quad per card placement with the spec's three-frame blend in
the FO4 program (sheet N from the header, view direction from the viewport
camera, the card's AO byte applied), plus a numeric gate: frame weights per
direction against a table computed from the spec by an independent Python, and
the same table is the contract the FO4CS shader must later match. Not PBR
renderer work, but it IS viewport work: his signal before it launches.


### 8.4 RULED 2026-09-16 17:37: quadtree seams = stitching, skirts the fallback

bungo, shown the three-panel seam diagram (T-junction crack / skirt / stitch):
"Stitching looks good". Ruling for the quadtree-over-`.lodl` rung (hybrid LOD
rung 3, DS2 pass-23 evidence):

- Fine-to-coarse seams are STITCHED: the odd fine vertex on the shared edge is
  dropped and the fine side fans to its interior vertex, so both levels share
  the same two vertices and the same straight edge. The `.lodl` subsample
  pyramid makes those shared heights bit-identical, no crack to hide.
- Balancing rule stays at most one level difference across an edge (DS2
  IndexBuffer mode), so the stitch is always drop-every-other-vertex.
- Skirts remain the zero-effort fallback (a flag, DS2 margin = S/gridSize),
  never the default.
- This is a runtime (FO4CS reader) job: index-buffer variant per edge
  code, chosen from the four neighbours' levels each frame. Lodgen bakes
  nothing extra for it. FO4CS readers come last by standing order.

OPEN 2026-09-16 17:5x (his friend's ChatGPT thread "CDLOD Quadtree Seams"):
the thread ranks CDLOD morph + matched edges > stitch > T-junction > skirts.
Our stitch IS the "matched edges" half. The other half, a CDLOD geomorph
(each odd vertex lerps toward the midpoint of its even neighbours as the node
nears its swap distance), costs nothing extra with the `.lodl` subsample
pyramid: the coarse position of every dropped vertex is exactly that midpoint,
so the vertex shader computes it from two neighbour height samples, no data.
It replaces the DS2 cross-fade for TERRAIN node swaps (fade draws both nodes
for the transition; morph draws one). Objects (`.lodo` clusters, cards) keep
the fade. The thread's hardware tessellation + displacement stack is their
near-terrain design, not ours: our far ring is static heights, near terrain
is the engine's LAND. RULED 2026-09-16 17:55, bungo "Morph for terrain, yes": terrain node
swaps use the CDLOD geomorph (odd vertices lerp to the midpoint of their even
neighbours over the approach; surviving vertices never move; runs in reverse
when walking in); objects and tree cards keep the DS2 cross-fade. Precondition
already in place: one halving per level and one-level balancing. Runtime FO4CS
job, last by standing order; lodgen bakes nothing extra.

### 8.5 RULED 2026-09-23 08:0x: terrain TEXTURES = clipmap rings; terrain GEOMETRY stays the quadtree

bungo, verbatim: *"Let's do clipmaps then for textures"*, after the comparison
with today's per-chunk textures (detail set by chunk level, whole-chunk pop on
swap, sharpness steps at chunk borders) and with virtual texturing (feedback pass,
tile pool, residency manager: more precise, far more machinery).

* **R2's far-terrain colour, normal and mask are sampled from camera-centred
  clipmap rings** (a fixed-size window per level, toroidal strip updates as the
  camera moves, ring chosen by distance, blended at ring edges). The rings are fed
  from the `.lodt` pyramid, which already carries what a clipmap needs (aligned
  tile grid, `worldUnitsPerTile` and `contentTexels` stated per level) and
  deliberately nothing camera-relative (`docs/LODGEN_TERRAIN_VT.md`, "Nothing
  clipmap-specific").
* **Geometry is unchanged:** quadtree over `.lodl` (8, 8.4), CDLOD geomorph,
  stitched seams. Texture and geometry are decoupled: the rings are sampled by
  world position, never by chunk.
* **Fallback:** the stock per-chunk `.btr` sheets remain what draws with the
  module off or `arm.terrain` refused.
* **Not decided here:** ring count, window size, per-sheet formats, the inner-band
  cross-fade width. R2's own design names them with numbers.
* **Unblocker:** section 5 row 14 -- no `.lodt` pyramid has been written for a
  whole worldspace; R2 needs one Commonwealth `--vt` bake as its fixture.

## 9. The far-shadow contract -- the IDENTITY route (ruled 2026-09-19)

**What this section used to say, and why it does not any more.** Between
2026-09-18 and 2026-09-19 the contract here was a BAKED far shadow: a per-vertex
horizon field on every LOD object (`.lodi` v8) and a role-7 horizon sheet on the
terrain, looked up instead of cast, chosen when bungo answered *"B sounds
good"*. Lanes HORIZON1, HORIZON2 and HORIZON3 built it. On 2026-09-19, the first
perspective picture of it against a ray-cast sun settled it the other way:

> *"As you can see, the end result is terrible."*
> *"So, for now, we revert back to identity data per LOD object from the
> preauthored LODs."* -- *"We're not doing the horizon thing."* -- *"So yeah,
> horizon goes bye bye now, we're back to identity."*

The two measurements behind that: at a low sun the baked object horizons
disagreed with a ray-cast sun on **50-58 %** of object pixels (lane SUNSIM1),
while **the identity far shadow map simulated at 64 u disagreed on about 9 %**
(lane HORIZON4). The baked route is not lost --
`release/NifSkope.before_horizonout.exe` still bakes both streams, and both
readers still open a file that carries one -- but nothing in the shipped exe
writes it, and FO4CS is not to consume it.

**So the far object shadow IS a map again**, and the thing that was missing when
bungo said *"Fo4CS far map covers the terrain, but the lod objects needed
identity, so it never worked"* is the thing the `.lodi` now carries: **a group
identity per placement**.

### 9.1 The four rules that replace rulings (i)-(iv)

1. **The far shadow map keys on the `.lodi` GROUP id.** One u16 an instance,
   `offGroup` at 0x100 with `groupCount` at 0x108 (`src/lodifile.h:556`), the
   contract in `docs/LODGEN_NATIVE_LODO_LODI.md` s4.9. A group is what a person
   would call ONE BUILDING -- since lane HORIZONOUT the default rule is the
   proximity join (`--identity-join`, default gap 64 units in `src/nifcli.cpp`, anchor
   `float lgIdentityJoinGap = 64.0f;`; re-cited 2026-09-23 because NATIVE s4.9 documents only the
   legacy 16-unit rule), which puts every non-tree LOD mesh
   within 64 units of another into the same identity.
2. **Self-shadow is excluded by identity**: a caster carrying the receiver's own
   group id is not allowed to shadow it. That is the whole point -- bungo,
   2026-09-19: *"in the game with fo4cs far shadows would self occlude, so a
   flat wall would self occlude and cause an artifact shadow to appear in the
   middle"*.
3. **Trees cast from their authored 3D LOD mesh with alpha test. Cards never
   cast.** A card is a billboard that turns to face the viewer; its shadow would
   turn with it.
4. **The workshop-scrappable bit drops workshop-owned casters** (`.lodi` v9 bit
   6, s4.12): a placement the player can scrap is a placement that will not be
   there, and it must not keep casting a shadow the settlement no longer owns.

### 9.2 Caster x receiver -- THE DIRECTOR'S DRAFT, FOR BUNGO TO RULE

**This matrix is not ruled yet.** It is the director's reading of bungo's words
of 2026-09-19 06:1x-06:2x, written down so he can correct one row instead of the
whole idea. His words, verbatim: *"on fo4cs side these will only be used for
shadows out of the cascade range, not within the cascade range, but the far
shadows will still cast shadows into the cascade range"*, and, on the reason for
rule 2 above, *"in the game with fo4cs far shadows would self occlude, so a flat
wall would self occlude and cause an artifact shadow to appear in the middle"*.

| # | receiver | what shadows it | identity used? |
|---|---|---|---|
| 1 | **inside the cascade range** | the engine cascades, from near casters, exactly as today; **and IN ADDITION** the far pass, whose casters are the authored LOD meshes BEYOND the cascade range -- this is how a distant tower's shadow reaches the player | no (cascades); the far caster SET is what keeps it honest |
| 2 | **beyond the cascade range, drawn as a LOD object** | the far pass, with the identity rule AS RULED 2026-09-19: a caster carrying the receiver's own group id is ignored **only within D of the receiver along the sun ray** (s9.2a) | **yes, distance-gated** -- this is what kills the mid-wall patch without losing the building's own long shadow |
| 3 | **beyond the cascade range, still inside the loaded cells, drawn as its FULL model** | the far pass -- and **THIS IS THE GAP** (below) | **owed**: the full model has no identity of its own |
| 4 | **terrain** | the far pass, always; terrain receives from everything | no |
| 5 | **trees** | as receivers, like any LOD object; as casters, the authored 3D LOD mesh with alpha test -- **cards never cast** | yes, as any object |
| 6 | **the self-shadow the identity rule gives up on purpose** (wing on wing inside one identity, small contact shadows) | **FO4CS's existing screen-space shadows**, which already run on LOD pixels -- bungo 2026-09-19: *"for the gaps that the identity does not fill, we have screen space shadows too"* | no (screen space has no identity) |

**Row 1, the double-darkening trap.** If the far pass casts from a placement
whose full model is already a cascade caster, the same occluder darkens the same
pixel twice. The far-caster set is therefore **the LOD placements whose full
model is NOT inside the cascade caster range**, and that set is a runtime
decision FO4CS makes per frame, not something the bake can pre-compute.

**Row 3, the gap, stated as a contract requirement.** A REFR beyond the cascade
range but inside the loaded cells draws as its FULL model while its LOD proxy is
one of the far casters -- so the full model gets shadowed by its own proxy, and
carries no group id to be excluded by. **The data to close it already exists**:
the `.lodi` cold record holds `refFormId`, the placed REFR's form id in the
load-order-mapped id space, at the instance's OWN index so the join needs no
search (`src/lodifile.h:490` and the comment at `:412`;
`docs/LODGEN_NATIVE_LODO_LODI.md` s4.1a). FO4CS can therefore map **loaded REFR
-> instance index -> group id** once at cell load and bind that id per draw, at
which point a full model is excluded by the same rule as its proxy. **The
contract requirement is that it does so**; until it does, row 3 is a known
artefact and not a mystery.

**Row 6, the limits, stated -- and now measured.** Screen-space shadows only
know about occluders that are ON SCREEN, and they guess thickness from depth;
they recover contact shadows and wing-on-wing, not a shadow cast by something
behind the camera. Lane IDENTGAP measured the recovery with bias and thickness
swept rather than guessed (best cell: bias 0.040 x depth, thickness 0.05 x
depth, 64 steps, reach 10% of frame height): the march gives back **9-27%** of
the base row's false-LIT and invents **0.55 to 2.30 points** of new false-dark,
a net of **+0.10 / -1.61 / +0.38 points** on the three cameras. So it is NOT a
replacement for the gate of s9.2a -- which returns five to twenty-five times
more good per unit of harm -- but it **composes** with it, and the best 16 u
result measured anywhere in that lane is the two together: hwydeck object
false-LIT 6.84% (pure identity) -> 5.34% (the gate) -> **4.10%** (gate +
march). If the march is already running, leave it on.

#### 9.2a The identity gate is a DISTANCE rule -- ruled 2026-09-19

bungo, 07:3x, on lane IDENTGAP's tables: *"Okay, distance rule, okay it's
fine"*. The rule as adopted:

> A caster carrying the receiver's OWN group id is ignored **only when it lies
> within D of the receiver along the sun ray**. A same-identity caster FARTHER
> than D shadows normally. A caster of a DIFFERENT identity always shadows.
> **D = (far-map texel size) / sin(sun elevation), floor 64 units** -- about
> 92 u for today's 16 u far cascade at a 10-degree sun. Fixed D = 64 u is the
> fallback. Screen-space shadows (row 6) run on top.

It is **shader-only**: one subtract and one compare on the depth and the group
id the far tap already fetches. No new texture, no second tap, no `.lodi`
change, nothing owed by the bake.

What it buys, from `scratchpad/identgap_20260919/report.md` (16 u texel, 3x3
PCF, camera `hwydeck`, sun azimuth 180, elevation 10, object pixels; *false-LIT*
= self-shadow the row LOSES, *false-DARK* = shadow it INVENTS):

| row | false-DARK | false-LIT |
|---|---|---|
| pure identity, joined table | 0.52% | **12.89%** |
| pure identity, unjoined table | 0.55% | 6.84% |
| **the rule, D = 64 u, joined table** | 0.64% | **5.35%** |
| the rule, D = 64 u, unjoined table | 0.64% | 5.34% |

Two things to read off it. First, the rule **pays for the ruled proximity
join**: under pure identity a joined building loses 12.89% of its object pixels
to false-LIT because the whole building is one identity, and under the rule the
joined and unjoined tables give the same answer, 5.35% against 5.34%. Second,
D must stay SMALL -- at D = 1024 u the numbers collapse back toward pure
identity (6.32%), which is the rule doing nothing.

**The honest caveat, stated because the lane stated it.** At a 16 u cascade a
properly tuned slope + normal-offset bias, with NO identity rule at all, TIES
the gate (object totals 5.58 vs 5.59, 4.17 vs 4.19, 8.15 vs 8.19 on the three
cameras) and beats pure identity outright. The mid-wall patch is a shadow-map
RESOLUTION artefact, not a geometric one. The rule earns its place at coarse
texels and as insurance against a bias that is not tuned per cascade; it is not
a substitute for tuning the bias.

### 9.3 What the bake owes this route

* the group table and its rule (s4.9), which is the identity;
* the scrappable bit (s4.12), which drops workshop-owned casters;
* authored LOD meshes as the casters -- **no decimation**, bungo's standing rule
  of 2026-09-17, and trees baked to cards only for DRAWING;
* `refFormId` in the cold record (s4.1a), which is what row 3 needs.

Nothing in that list is new work for the generator. **The far shadow itself is
FO4CS's, and it is last by standing order.**

**Provenance.** The ruling of 2026-09-19 ("the end result is terrible"; "we
revert back to identity data per LOD object from the preauthored LODs"; "horizon
goes bye bye now"), the cascade-range and self-occlusion words of 06:1x, the
screen-space-shadow word of 06:2x, and the distance-rule ruling of 07:3x
("Okay, distance rule, okay it's fine") that turned s9.2a from a placeholder
into a rule. Numbers for s9.2a and row 6:
`scratchpad/identgap_20260919/report.md` and its `DONE`. Numbers: lane SUNSIM1 (50-58 %) and lane
HORIZON4 (~9 % at 64 u). Formats: `docs/LODGEN_NATIVE_LODO_LODI.md` s4.1a, s4.9,
s4.11 (the retired v8, reader-only), s4.12; `docs/LODGEN_TERRAIN_VT.md` s3.5
(the retired role 7, reader-only). Lane reports:
`scratchpad/horizon1_20260918/lane_horizon1_report.md`,
`scratchpad/horizon2_20260918/lane_horizon2_report.md`,
`scratchpad/horizonout_20260919/lane_horizonout_report.md`.
