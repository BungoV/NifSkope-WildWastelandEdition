# Lane TERRAIN-R — the terrain pyramid takes the object texture family

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, working tree.
Nothing committed (CONSTITUTION 8). Written incrementally.

Exe at launch: `release/NifSkope.exe` **2026-09-11 10:14:23, 21,101,056 B**
(NATIVE1b). Rollback rung taken before any link:
`release/NifSkope.before_terrain_r.exe` **10:39:04, 21,101,056 B** — byte
count equal to the launch exe, which is the rung check (T7's third clause).

Game down (`tasklist` rc=1 for `Fallout4.exe`) and no NifSkope running at the
moment the rung was taken.

---

## 0. Pre-registered gates

Registered BEFORE any line was written (CONSTITUTION 1, "a gate invented after
the numbers are in is not a gate"). Each has the floor that must go red.

| id | the gate | prediction | floor (must fail) |
|---|---|---|---|
| **T1** | per-layer roughness: the resolver's answer on an un-blended layer equals the near material's roughness at that layer, within 1/255 | equal on every resolvable layer | a legacy layer read WITHOUT the `1 - gloss` inversion differs by more than 1/255 on at least one layer |
| **T2** | metallic present iff a PBRM-backed layer carries a metallic map | PBRM fixture -> metallic map; legacy fixture -> 0 | a legacy layer that reports a non-zero metallic fails; a PBRM layer with a metallic map that reports 0 fails |
| **T3** | the emissive sheet exists iff at least one layer's material carries an emissive map | Sanctuary: absent, index `emissive: none` | the writer asked for an emissive sheet with no emissive layer must refuse; a container built WITH an emissive layer and no sheet must refuse |
| **T4** | ring-0 gate: an INDEPENDENT implementation of the contract's runtime blend vs the pyramid's level-0 baked colour, same texel | mean error small, max bounded, printed per tile | a blend with VCLR omitted reads visibly higher (red); the bake against itself reads exactly 0 (ceiling) |
| **T5** | stock path (`--no-vt`, no `--cover`) byte-identical with the rung | every file `cmp`-equal | — (an identity gate's own floor is that it fails when a sheet changes) |
| **T6** | standalone container gate before/after; one mutation per NEW role and stamp refused BY NAME | every mutation refused, unmutated accepted | the unmutated file must be accepted, or the battery proves nothing |
| **T7** | exe newer than every changed file; every driver rebuilt; rung bytes == launch bytes | — | — |
| **T8** | no NifSkope left running; game down at every launch | — | — |

### Baselines, read off the rung's own logs (NATIVE1b, the exe on disk)

| suite | baseline | log |
|---|---|---|
| `lodgen_terrain.sh` | **26 checks / 0 failures, PASS** | `scratchpad/native1b_20260911/logs/lodgen_terrain.log` |
| `lodl_open.sh` | **23 / 0, PASS** | same folder |
| `lodgen_native.sh` | **18 / 0, PASS** | `logs/lodgen_native_final.log` (the FIRST `lodgen_native.log` of that lane reads 18/1, before its counted relink) |
| `ui_align.sh` | **11 / 0, PASS** | same folder |
| `water_ui.sh` | **82 checks, floor 72, PASS** (1 `FAIL` token in the log is the pre-registered self-test line) | same folder |

**A correction to the brief, measured not assumed:** the brief names
`water_ui.sh (86/0)`. The rung's own log says **82 checks, floor 72**. 86 was
never this exe's number; the 82 is what the rung produces and is the baseline
this lane is held to.

`lodgen_terrain_vt.sh`, `lodgen_ground_cover.sh` and the card/array suites were
not run by NATIVE1b, so their baselines are measured on the rung by this lane
(section 4/5) rather than quoted.

---

## 1. The shared material resolver

### What the object bake did

`lodgenLoadModel` (`src/lodgen.cpp`) reads a shape's shader property, then lets a
`.bgsm` named by that property win over the texture set -- its textures where it
names them, its constants always, the specular slot only while it enables
specular. The arrays pass then composes the legacy mask sheet from those two
numbers: **gloss = smoothness x the `_s` map's GREEN channel**, specular = `_s`
red x strength, AO neutral. `.pbrm` was read nowhere in the generator; only the
renderer read one.

### What terrain now shares

Two things moved into shared code, and one deliberately did not.

1. **`lodgenLegacyGloss( smoothness, specGreen )`** (`src/lodgen.h`) is now the
   one definition of the legacy gloss. The arrays pass calls it -- the same
   number it computed inline before, so no object sheet moves -- and the terrain
   resolver inverts *that* function's result. The two cannot drift.
2. **`lodgenResolveMaterialMask( dataRoot, matName, specularTex, smoothness )`**
   is the one home for "what roughness and metallic does this material have". It
   uses the object bake's own material resolution (`ShaderMaterial`) and the
   RENDERER's own PBRM discovery rule -- the same-name `.pbrm` beside a `.bgsm`,
   as `BSShaderLightingProperty::resolvePbrm` does -- so an asset that previews
   as PBR in this application bakes as PBR.
3. **The object bake does not yet CONSUME it.** Stated plainly rather than
   hidden: wiring `lodgenLoadModel` onto the resolver would add a `.pbrm` probe
   per material for a consumer that does not exist (the object path takes its
   PBR answer from a `.lodm` sidecar today, not from a `.pbrm`), and it would
   put the object suites' byte-identity at risk for nothing. The resolver is
   shared CODE with one consumer today; the gloss law is shared code with two.
   Wiring the object/arrays path onto the resolver is owed and is named in
   section 7.

### The three rules, and what each answers

| rule | when | roughness | metallic | emissive |
|---|---|---|---|---|
| `pbrm` | a `.pbrm` parses beside (or as) the material | its RMAOS **R**, or its `roughness` constant | its RMAOS **G**, or its `metallic` constant | its emissive slot |
| `legacy-inverted` | a legacy material or an `_s` map | **`1 - smoothness x _s.G`** | **0** | the BGSM's glow slot |
| `none-default` | nothing to read | **1.0** -- fully rough, the honest unknown | 0 | none |

The terrain layer's material is the TXST's `MNAM` and its `_s` map is `TX07`;
`EsmWorld::ltexTextureSet()` is new and reads both. `EsmWorld::ltexTextures()`,
which every existing caller uses, is now a two-line read of that set, so the
diffuse and normal paths it answers with are the same strings resolved by the
same code.

### A measured fact the contract did not state, and that the law rests on

**Every Fallout 4 landscape `_s` map is BC5U** -- a TWO-channel block format, R
then G, with no blue and no alpha (measured over the unpacked corpus:
`Textures/Landscape` holds 824 DDS files; 495 are BC5U and every one of them is
an `_s` or a normal; the diffuses are 100 DXT1, 226 DXT5, 2 DXT3). So "the
specular map's gloss channel" is its GREEN channel and nothing else, which is
what this tree already composed and what the inversion now reads. This is in the
rewritten contract.

### The per-layer rule census, Sanctuary

Region **cells -20..-17 x 24..27** (a 4x4 cell block, the dim-4 Sanctuary chunk
and its ladder), `--vt --vt-height --cover`, FO4CS target. Printed by the bake
itself, one physical line of `key=value` tokens:

```
maskPbrm 0  maskLegacyInverted 14  maskNoneDefault 0
maskRoughMaps 14  maskMetalMaps 0  maskEmissiveMaps 0
maskDistinctLtex 14  maskLayerRefs 279
emissive none  sheets 4  coverIn mask
```

**14 of 14 distinct landscape textures on this region were served by
`legacy-inverted`, all 14 carry a roughness (`_s`) map, none carries a metallic
map and none carries an emissive map.** 279 layer references resolved to those
14 forms. Zero `none-default` -- so no layer fell through to the 1.0 floor here,
which is the thing that would have made the sheet a guess.

The same words are in the index as `terrain.maskRules`, so a consumer can audit
`family: "pbr"` instead of taking it on trust.

---

## 2. The sheets

### What the container carries now, and what it cost

Container **version 2**. Version 1's four sheets were colour, msn, `data` (R AO,
G wetness, B shore, A cover) and height. Version 2:

| sheet | role | format, no cover | with cover |
|---|---|---|---|
| 0 | 1 colour | BC1 (71) | BC1 |
| 1 | 2 msn | BC1 (71) | BC1 |
| 2 | **5 mask — `rmaos`: R roughness, G metallic, B AO, A ground cover** | BC1 (71) | **BC3 (77)** |
| 3 | 4 height (with `--vt-height`) | R16 (56) | R16 |
| 4 | **6 emissive — RGB, no alpha** | BC1 (71) | BC1 |

Role **3 is retired** and refused by name in a v2 file. The header grew from
four sheet descriptors to six (0xA0..0xCF); the reserved tail moved from 0xC0 to
0xD0 and is 48 bytes.

**Sizes, per tile, content 256 / border 8 / 2 mips** (a BC1 sheet is 46,240 B, a
BC3 sheet 92,480, the R16 height sheet 184,960):

| | before (v1) | after (v2) | after + emissive |
|---|---|---|---|
| no cover | **323,680** | **323,680** | 369,920 |
| cover | **369,920** | **369,920** | 416,160 |

**The mask sheet costs exactly what the retired data sheet cost.** Measured on
disk, cells −20..−19 × 24..25, `--vt-height --cover`: level 2
**746,752 B**, level 4 **374,016 B**, `storedBytesTotal` **739,840** = 2 cover
tiles × 369,920. With the emissive fixture: level 2 **1,258,304** for 3 tiles
(+45,845 a tile, which is 46,240 less the 4,096-alignment pad it absorbs).

### Cover's home — bungo's call, with both costs measured

`.lodm` §2.1 gives the object family two alpha slots: the colour sheet's is
**coverage** (opacity) and the mask's is **subsurface**. Terrain's fourth channel
is ground cover and had to take one.

| | **Option A — mask alpha** (shipped) | **Option B — colour alpha** (`--vt-cover-in-color`) |
|---|---|---|
| bytes, cover tile | 369,920 | **369,920 — identical** |
| bytes, cover-free tile | 323,680 | **323,680 — identical** |
| measured on disk | 746,752 / 374,016, `storedBytesTotal` 739,840 | **the same three numbers, byte for byte** |
| stock `.btr` tolerance | BC1 colour, as today | **BC3 colour — which the engine already reads** |
| what the slot means | subsurface, which nothing alpha-tests | **opacity, which `.lodm` §2.1 tells a consumer to ALPHA-TEST** |

**The byte argument is a null result and I am reporting it as one.** My first
estimate said Option B would cost +46,240 on every cover-free tile; the bake
disproved it. The cover format is selected **per tile** by the `COVER` bit, so a
cover-free tile is BC1 whichever sheet nominally carries the alpha. Measured both
ways with `--no-cover` (655,456 / 327,776) and with `--cover` (746,752 /
374,016): **identical in all four numbers.**

**The stock-tolerance argument is also a null result, and also measured.**
Vanilla's own shipped terrain sheets are DXT5: **2,001 of 2,001** colour sheets
and **1,999 of 1,999** `_msn` sheets under `Textures\Terrain\Commonwealth\`.
DXT5 is the only format the engine has ever been given for that slot, so a BC3
colour sheet is not a risk.

**So the decision rests on one thing: what the slot MEANS.** The colour sheet's
alpha is the one slot the object family defines as opacity, and a consumer
written against `.lodm` §2.1 alpha-tests it — which would punch holes in the
ground wherever grass is thin. **Proposed: Option A, cover in the mask's alpha**,
a named substitution the index records. Shipped that way, with
`--vt-cover-in-color` reaching Option B exactly: one flag, one format pair, no
second code path. **This is bungo's to rule on and it is open.**

### The stamps a reader can qualify

* The **`.btr` chunk `_data.DDS`** keeps §1.4's `'WWCV'` stamp and its channels
  unchanged — the stock engine reads those files and their bytes are pinned.
* The **container** says which sheet carries the cover by declaring
  `dxgiFormatCover != dxgiFormat` on exactly that sheet. **Exactly one sheet may
  do so, and only the mask or the colour sheet**; two carriers, or a carrier on
  the msn or the height sheet, is a refusal by name.
* A **v1 file is refused, not converted**, and the refusal says what role 3 used
  to mean. bungo's installed `Data\Terrain` was listed READ-ONLY and holds no
  `.lodt` at all (the folder does not exist), so there is nothing anywhere to
  convert.

### The emissive sheet

Written **only** when at least one layer's material carries an emissive map,
decided by a pre-pass over the bake rectangle's LAND records **before any
container opens** (its presence is a header field, so a tile's payload size
depends on it). Absent otherwise, and the index says `"emissive": "none"` in
words — a consumer must be able to tell "this worldspace emits nothing" from
"the writer forgot", and a black sheet says neither.

### The index

`family` is **`"pbr"` and it means it**. New keys: `emissive` (`"none"` /
`"present"`), `dropped` (what version 1 carried, and where to get it instead),
and `maskRules` — the per-layer census, whose three rule counts must sum to
`distinctLtex`. The `sheets[]` block lists the new roles and channels.

### What was dropped

**Shore proximity** and **wetness**, from the container only. The census names
both and the index's `dropped` object says where a consumer gets them: shore is
`body.waterHeight − height(sample)` from the `.lodl`'s water planes, which
`docs/LODGEN_BTD_FORMAT.md` already states of the landscape file; far wetness is
a weather state the runtime owns.

---

## 3. The ring-0 formula and its gate

bungo's ruling of 09:2x makes far terrain **hybrid by band**: ring 0 blends at
runtime from the `.lodl`'s per-texel LTEX weights, ring 1 and out sample the
pyramid, and the two cross-fade across ring 0. That only works if the two agree
on the same texel, so the runtime's formula is now stated ONCE, in the contract
(`docs/LODGEN_TERRAIN_VT.md` §2.5), and the gate implements it from that
statement.

The seven steps: the cell and quadrant from the world point; the layer opacity
as a bilinear tap over the quadrant's 17x17 VTXT grid; the base layer's diffuse
(the enclosing dim-4 chunk's DOMINANT base where BTXT is 0); each ATXT layer in
record order, **skipped** below opacity 0.001 rather than blended with a tiny
weight; the VCLR multiply; then the grass tint from the cover byte and
`tintStrength`, AFTER the VCLR. Every source diffuse at `u = frac(wx/2048)`,
`v = frac(wy/2048)`, at the footprint-chosen mip, trilinear.

### The gate

`tests/spells/lodgen_terrain_model.py ring0` — an INDEPENDENT implementation:
its own ESM walk (`lodgen_cover_model.py`, which parses the plugin itself), its
own DXT1/DXT3/DXT5/**BC5U** decoding, its own mip choice and its own taps.
Nothing is imported from the generator; the only thing read from it is its
OUTPUT.

| tile (sw cell) | texels | mean | p95 | max | FLOOR: weights ignored | CEILING |
|---|---|---|---|---|---|---|
| (−20, 24) | 2,704 | **3.26** | 8 | 14 | **13.70** — a 4.2x separation | **0** over 73,984 texels |
| (−18, 24) | 2,704 | **3.59** | 8 | 17 | **15.15** — a 4.2x separation | **0** over 73,984 texels |

All in sRGB 8-bit units, 2,704 of 2,704 sampled texels modelled (none refused).

### Two things about the floor, measured rather than assumed

**The floor bungo named — "a deliberately wrong grading" — does not bite here,
and the reason is in his data, not in the gate.** Dropping the VCLR multiply
changes the error by **0.00**: only **2,362 of the Commonwealth's 36,864 cells
carry a VCLR at all**, and over the Sanctuary region every byte of every VCLR
present is in **249..255** — white to within 6/255. So the floor that DOES bite
is a blend that ignores the per-texel LTEX weights, which is the thing the gate
actually claims, and it reads 4.2x worse on both tiles. The VCLR arm is printed
beside it and explicitly NOT gated, with that measurement as the reason.

**The ceiling is a real check and not decoration.** It re-opens the container,
re-decodes the sheet through the same path, and differences the two arrays over
all 73,984 texels. (My first draft compared an array with itself, which cannot
read non-zero — that is in the mistakes section.)

### What the gate does NOT carry, said plainly

The **grass tint** (step 7). It needs the grass mesh's own average diffuse, and
the independent model deliberately does not open meshes. So the gate runs against
a `--grass-tint 0` bake, and the tint's size is stated separately:
`lodgen_ground_cover.sh` measures a mean tint delta of **26.97/255** over the 256
highest-cover texels of a chunk. A runtime that folds the tint in from the cover
byte and `tintStrength`, as §1.5 states, reproduces it exactly.

---

## 4. Standalone gate, before and after

The `.lodt` fixture set is `tests/spells/lodgen_terrain_vt.sh`'s own: it bakes
the cells (−24,24)..(−17,31) — 8x8, west and south both dividing 8 and not 16, so
the ladder is dim 2, 4 and 8 — and then mutates the root container one header
field at a time, each copy handed to the shipped validator.

| | mutations | refused | refused BY NAME |
|---|---|---|---|
| **before** (the rung, `release/NifSkope.before_terrain_r.exe`) | 23 | 23 of 23 | not checked |
| **after** | **30** | **30 of 30** | **5 of 5** |

The seven new mutations are one per NEW role or stamp, and five of them are
matched against the TEXT of the refusal, not merely against "it was refused" — a
validator that answers "invalid" to everything passes a count and cannot be
mutation-tested:

| mutation | refusal |
|---|---|
| `versionOneRetired` | *refused: version 1 container -- four sheets whose role 3 is `data` (R sky AO, G flow wetness, B shore proximity, A ground cover). Version 2 replaced it with role 5 `mask` ... No conversion exists -- re-bake with --vt* |
| `versionFuture` | *refused: version 3, this reader knows 2* |
| `roleDataRetired` | *refused: sheet 2 has role 3 `data` (R AO, G wetness, B shore, A cover), which version 1 carried and version 2 retired; the mask sheet is role 5 (RMAOS)* |
| `maskSheetMissing` | *refused: a version 2 container must carry the colour (role 1), msn (role 2) and mask (role 5) sheets* |
| `twoCoverCarriers` | *refused: sheets 0 and 2 both declare a dxgiFormatCover; exactly one sheet carries the ground cover* |
| `coverCarrierOnMsn` | *refused: sheet 1 (role 2) declares a dxgiFormatCover; only the mask (role 5) or the colour sheet (role 1) may carry the ground-cover alpha* |
| `roleOutOfRange` / `sheetPastCount` | *refused: sheet 2 has role 9* / *refused: sheet 5 is past sheetCount and not zero* |

**One of those seven was a gate bug I caught and fixed.** `maskSheetMissing`
first fired the COVER-CARRIER rule, not the missing-mask rule, because renaming
the mask to `emissive` left its differing cover format behind — a mutation named
for a rule it was not reaching. The mutation now equalises the formats in the
same edit, so only the rule it is named for can fire.

The unmutated root is accepted by the same validator (the check that makes the
battery mean anything), and both containers of the PBRM fixture — the one with an
emissive sheet and the one without — pass every rule of §3.4 including every tile
CRC.

---

## 5. Build and gates

### The exe, and the clocks in one table

| artefact | time | bytes |
|---|---|---|
| exe at lane launch (NATIVE1b) | 2026-09-11 10:14:23 | 21,101,056 |
| rollback rung `release/NifSkope.before_terrain_r.exe` | 10:39:04 | **21,101,056** — equal to the launch exe |
| the ONE build (qmake + make) | 11:01:06 | 21,137,408 |
| counted relink 1 (PBRM reachable off a diffuse stem; the absolute-MNAM fix) | 11:23:53 | 21,137,920 |
| counted relink 2 (the `.pbrm` extension off-by-a-dot) | **11:27:12** | **21,137,920** |
| `release/style.qss` | 11:27:12 | byte-identical with `res/style.qss` |

One build plus **two counted relinks**, both declared. `QMAKE-RC=0 BUILD-RC=0`
on the build; `BUILD-RC=0` on both relinks. `qmake` was run before `make` because
three headers changed (`lodgen.h`, `esmdata.h`, `lodvfile.h`) and qmake's
dependency lists are frozen when the Makefile is generated.

**Consistency, not just success.** All **7 of 7** changed source files are older
than the exe. Every translation unit that includes a changed header was rebuilt
and checked by object mtime: `lodgen`, `lodgenmanager`, `main`, `nativeemit`,
`nifcli`, `nifskope_ui`, `esmdata`, `lodtfile`, `lodifile`, `lodofile`,
`lodvfile` — **11 of 11**.

### The gate table

| suite | after | baseline | note |
|---|---|---|---|
| `lodgen_terrain_vt.sh` | **41 / 1** | 35 / 1 (rung) | +6 checks, all new and green: V2b (five mutations refused by name) and five V12 index checks. The single failure is **V9b**, the assembled-vs-direct `_msn` byte identity, which was already red on the rung |
| `lodgen_ground_cover.sh` | **29 / 5** | 29 / 5 (rung) | the same five, unmoved: C1 (no frozen baseline file on disk), C2 x3 (the fixture chunk has cover, `coverMax=69`, where the check assumes none), C6a, C9, C16, C11b |
| `lodgen_terrain.sh` | **26 / 0 PASS** | 26 / 0 | |
| `lodgen_native.sh` | **18 / 0 PASS** | 18 / 0 | |
| `lodgen_card_arrays.sh` | **35 ok, PASS** | 35 ok, PASS (rung, run by this lane) | |
| `lodgen_texture_arrays.sh` | **40 ok, PASS** | 40 ok, PASS (rung, run by this lane) | |
| `lodl_open.sh` | **23 / 0 PASS** | 23 / 0 | |
| `ui_align.sh` | **11 / 0 PASS** | 11 / 0 | |
| `water_ui.sh` | **82 checks, PASS** (floor 72) | 82, floor 72 | the brief's "86/0" was never this exe's number |
| **`lodgen_terrain_pbrm.sh`** | **14 / 0 PASS** | NEW | T2 and T3 both ways on a fixture |

Logs: `scratchpad/terrain_r_20260911/logs/`. Baselines measured on the rung are
`base_*.log`; the final run is `f_*.log` and the un-prefixed names.

Skipped, with the reason: every suite the change does not reach — impostor,
gltf, hkx*, collision, block, water solve / flow / weights / mark / window,
skeleton, files/top-bar/loaded-NIFs. The change touches `src/lodgen.*`,
`src/esmdata.*`, `src/io/lodvfile.*` and one CLI flag in `src/nifcli.cpp`;
`ui_align.sh` and `water_ui.sh` were run anyway because `nifskope_ui.cpp` was
recompiled against the changed `lodgen.h`.

### The identity gates (T5)

| path | files | result |
|---|---|---|
| stock terrain, `--no-vt --no-cover`, cells −20..−17 x 24..27 at dim 4 | 6 | **every file byte-identical with the rung** |
| the object arrays bake, `--arrays --no-ao`, cells −20..−19 x 24..25 | 12 | **every file byte-identical with the rung** |

Both re-run against the final relinked exe as well as the first build.

### T8

`Fallout4.exe` was down at every launch and at the end (`tasklist` count 0). No
NifSkope process is left running: the GUI suites were run sequentially, one
instance at a time, and the process list is empty at the close of the lane.

---

## 6. Pictures

All four are in `scratchpad/terrain_r_20260911/images/`. Each is built on a fixed
cell grid — one panel per cell, captions in a band of their own below it — and
every number a caption quotes is computed from the SAME array the panel is drawn
from, so a caption cannot describe a different measurement from the one on
screen.

**`mask_sheet_tile.png`** (1232x449). One tile's mask sheet, a 96x96-texel
window of the CONTENT region chosen automatically as the window where the paint
changes most, magnified 3x, with each channel as its own greyscale panel and its
law and its numbers beneath it. It shows R roughness varying over 74..255 with 71
distinct values, **G metallic flat at 0 with exactly one distinct value** (which
is what a legacy-only region must look like), B AO over 156..255, and A ground
cover over 0..66 with a shoreline of grass in it.

**`emissive_presence.png`** (928x449). The same region baked twice: left, the
PBRM fixture, where the container carries roles `[1, 2, 5, 4, 6]` and the
emissive sheet holds the fixture's colour; middle, the shipped-data bake, where
there is **no role 6 at all** and the index says `"emissive": "none"` — the panel
says so in words rather than showing a black sheet, because a black sheet and a
missing sheet are the thing a consumer must be able to tell apart; right, the
colour sheet of the same texels so a reader can see what ground it is.

**`ring0_blend_vs_bake.png`** (1024x481). The same 64x64-texel window three
times: the runtime formula blended by the independent model, the pyramid's level
0 as baked, and the absolute difference **multiplied by eight** (at 1x it is
black). Mean 3.28, p95 8, max 16 of 255 over that window. The two left panels are
indistinguishable, which is the claim.

**`ours_vs_vanilla_tile.png`** (1600x666). Our pyramid's level-0 colour beside
**Bethesda's own shipped** `Commonwealth.4.-20.24.DDS`, same ground, no
resampling on either side — vanilla's dim-4 sheet is 512 texels over 4 cells and
our dim-2 tile is 256 over 2, both exactly 32 world units a texel, so one tile IS
one quadrant of one chunk sheet, texel for texel. Mean difference **19.96 of
255**, and the difference panel says what it is: **vanilla's sheet carries the
ROADS and the rubble patch and ours does not.** That is lane ROADS1's job
(bungo, 10:0x: *"We do the same with roads and decals as vanilla"*), and this
picture is the case for it.

**Owed, and named:** the brief also asked for a render-hook TOP VIEW of the
region with the pyramid's colour on. It was not taken. What is above is a
texel-level comparison of the sheets, which is what carries the claim this lane
makes; a rendered view of the assembled `.btr` would carry a different one (that
the mesh, the UVs and the sheet agree) and belongs with ROADS1, which will want
the same framing before and after.

---

## 7. Owed / red / bungo's calls

### bungo's calls, open

1. **WHERE THE GROUND COVER LIVES.** Shipped in the MASK sheet's alpha;
   `--vt-cover-in-color` reaches the other arm exactly. **Both cost the same
   bytes and the stock engine tolerates both** — measured, section 2 — so the
   decision is only about meaning: the colour sheet's alpha is the one slot
   `.lodm` §2.1 defines as OPACITY and tells a consumer to alpha-test. Proposed:
   leave it in the mask.
2. **THE ROADS.** `ours_vs_vanilla_tile.png` shows what the 19.96/255 mean
   difference against vanilla's own sheet mostly IS: Bethesda's sheet carries the
   roads and a rubble patch and ours does not. That is lane ROADS1, already
   chartered; the picture is the case for running it before he bakes.

### Red

* **V9b in `lodgen_terrain_vt.sh` is still red** — the assembled and direct
  `_msn` sheets are not byte-identical. It was red on the rung too, and this lane
  did not touch it. The contract's §2.4 claims that identity; either the claim or
  the code is wrong, and nobody has measured which.
* **`lodgen_ground_cover.sh` 29 / 5**, unmoved. Two of the five are the suite
  describing a world that has changed under it: C1 wants a frozen
  `lodgen_ground_cover.sha256` that is not on disk, and C2 asserts its fixture
  chunk has no cover when it now reads `coverMax=69`. Three are real
  measurements the suite reports and gates at the same time.
* **The resource stack cannot see a `.pbrm` — or a `.lodm`.** `BA2File`'s
  loose-file extension whitelist (`lib/libfo76utils/src/ba2file.cpp`) lists
  `bgsm`, `bgem`, `dds`, `nif`, `mat` and a dozen others, and neither `pbrm` nor
  `lodm`. So `--resource <folder>` silently ignores both, and `lodgen.h`'s own
  paragraph — *"it is consulted FIRST, for meshes, textures, materials and .lodm
  alike"* — is **not true today**. Found by the PBRM fixture, which had to be
  mounted as the `--data-root` instead. Two lines in a vendored library; not
  taken here because it is another project's file and another lane's build.
* **A PBRM landscape texture is reachable only through the diffuse stem or an
  MNAM.** That now works (a `.pbrm` beside the diffuse's own name under
  `materials\`), but it is a convention a user has to be told; it is in the
  contract and in `lodgen.h`.
* **The object bake does not consume the shared resolver.** The mask LAW has one
  home and the gloss is genuinely shared code called by both, but the object path
  still takes its PBR answer from a `.lodm` sidecar rather than from a `.pbrm`.
  Until that is wired, a model whose material is a PBRM bakes its objects legacy
  and its terrain PBR. Named in the contract's §6.

### Owed

* The render-hook top view of the region (section 6).
* The PBRM arm has never run on shipped data, because there is none: 14 of 14
  landscape textures on Sanctuary resolve `legacy-inverted`. It is gated on a
  fixture and that is stated rather than implied.
* The grass tint is the one term the ring-0 model does not carry, so the gate is
  run against `--grass-tint 0`. Folding the tint into the independent model needs
  a grass-mesh reader it deliberately does not have.

---

## 8. Mistakes

Five, written as they were recognised, in
`scratchpad/terrain_r_20260911/MISTAKES_ENTRIES.md` for the director to splice:

1. A byte cost reported from arithmetic instead of from the bake — and it was
   wrong; the two cover homes cost exactly the same.
2. A ceiling check that could not fail (`abs(x - x)`), rewritten to re-decode and
   difference over all 73,984 texels.
3. A mutation named `maskSheetMissing` that was refused by a different rule than
   the one it is named for.
4. A `.pbrm` path built by dropping five characters and adding four, losing the
   dot — caught by the census field that must move.
5. Two heredocs that halved backslashes, on a machine where
   `nifskope-ww-build-verify` already says not to use them for that.

---

## 9. Finished-work skill review

**Loaded and used:** `nifskope-ww-lodgen` (the CLI, the byte-identity gates, the
editing traps), `nifskope-ww-build-verify` (the gated chain, qmake before make
after a header change, the object-mtime sweep over every dependent translation
unit, the exe-newer rule), `ww-standalone-writer-gate` (the mutation battery, the
independent decoder, refusals by name), `ww-contract-provenance` (hash and
line-count first, anchor text beside every line number, every number re-derived),
`ww-control-calibration` (a floor that carries the signal's own amplitude — which
is what sent the ring-0 floor from "drop VCLR" to "drop the weights" once VCLR
turned out to be white), `ww-texel-picture` (the crop where the defect is, the
fixed cell layout, the caption arithmetic), `fo4cs-census-field` (written AND
moves, a refusal that names its reason, a default that accuses its own
plumbing), `nifskope-ww-vanilla-compare` (the same tile, the same density, the
two things that silently put a difference in the picture that is not the one
under test).

**Not needed:** `ww-anchored-hookup` — no file another live lane owns was
touched, and no new translation unit needed a build hook-up, so there was nothing
to refuse.

**The skill this lane wishes had existed, and wrote:**
`ww-corpus-absent-fixture` — *what to do when the gate's POSITIVE side does not
exist in the shipped data*. It cost this lane about an hour and four false
starts, and it will recur on every PBR lane, because the Fallout 4 corpus
contains no PBRM anywhere: the procedure is (1) prove the absence with a count
before building anything (14 of 14 `legacy-inverted`, 0 `pbrm`), (2) decide
whether the fixture rides on the resource stack or the data root by testing what
the stack's index actually holds rather than what the header comment says it
holds, (3) author the fixture's constants away from 0, 1 and from each other so a
channel swap cannot alias, (4) state the fixture's values as the ROUND TRIP
through the codec the bake will use, not as the authored floats, and (5) gate
both directions in one run of one script, so the absent side is measured by the
same code that measures the present side. It is written to
`.claude/skills/ww-corpus-absent-fixture/SKILL.md` in the REPO tree; the
director mirrors it to the live tree.

**The second one, and it is smaller but it bit twice:**
`ww-independent-sampler` — before writing a Python model that must reproduce a
C++ texture tap, CENSUS THE FORMATS FIRST. This lane wrote a DXT1/DXT3/DXT5
decoder, ran the gate, and got "0 of 1369 texels modelled" because every Fallout
4 landscape `_s` map is BC5U, a two-channel format the decoder had never heard
of. One `python` pass over the corpus's fourCCs before the decoder was written
would have cost two minutes. Folded into `ww-corpus-absent-fixture` as its step
0 rather than given a page of its own, because the two are always done together.
