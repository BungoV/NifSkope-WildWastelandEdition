# Lane IMAGES5 — pictures of what the generator makes today, + the FO4CS sample set

bungo's ask, verbatim: *"before handoff to fo4cs, I need images of the terrain
chunks we generate now (the ones with geometry) and especially the octahedral
impostors generated too"*.

**IMAGES5 is the third launch of this lane.** IMAGES3 was stopped by the
save-confirmation hang; IMAGES4 ran with the 17:22:05 exe and got as far as the
card bake. IMAGES5 runs on `release/NifSkope.exe` **2026-09-09 19:35:14** — the
build that carries lane RENAME's FINAL FILE NAMES (built 18:43:13 by BUILD2) and
lane OFFSCREEN2's invisible-headless-window change (19:35:14). **No build
happened in this lane.** `Fallout4.exe` and any other `NifSkope.exe` were checked
absent before every launch; the gate is inside the driver scripts, not in my head.

Everything is under `scratchpad/images_20260909/`: `gen/` the generated chunks
and sheets, `img/` the raw single renders, `handoff_*.png` the composed
deliverables, `cand/` the candidate lists, `log_*.txt` the run logs, plus the
scripts. The FO4CS sample set is under `scratchpad/handoff_fo4cs/samples/`.

---

## 0. What was inherited from IMAGES3/IMAGES4, and what was thrown away

The brief said to reuse what is valid and regenerate what is not, and to say
which. The rule I applied: **anything produced by an exe older than 19:35:14 is
an output and was regenerated; anything derived from vanilla or from the ESM
alone is an input and was kept.**

| inherited | verdict |
|---|---|
| `pick_handoff_region.py` and its winner `Commonwealth.4.28.24` | **KEPT.** It scores vanilla's shipped sheets and the ESM's own heights; it never reads anything we generated (CONSTITUTION rule 4). Re-running it could only produce the same answer. |
| `gen/van4 van8 van16 van32` (vanilla `.BTR` + sheets, staged) | **KEPT.** Copies of Bethesda's shipped files. |
| `gen_terrain.sh`, `hide_water.sh`, `stage_and_shoot_terrain.sh`, `wireplan.py`, `draw_wireplans.sh`, `compose_terrain.sh`, `gen_objects.sh`, `shoot_objects.sh`, `geomstats.py`, `sheetgrid.py`, `cardframe.py`, `contact_sheet.py` | **KEPT as drivers, RE-RUN.** They are the commands, not the pictures. Where a caption held a measured number, the number was re-read from the new artefact — see §2. |
| `gen/ours4 ours8 ours16 ours32`, `gen/obj_ring0`, `gen/obj_far16` | **REGENERATED** with 19:35:14. |
| every `handoff_*.png` and everything in `img/` | **REGENERATED.** IMAGES4's composed PNGs were 16:29/16:36 — composed from IMAGES3's 15:30:27 output and never recomposed after IMAGES4 regenerated `gen/`. They were stale against their own inputs. |
| `cands_trees.txt` (33 bases: 19 trees, 7 shacks, 7 rock cliffs) | **THROWN AWAY.** It is the old `--candidates trees` meaning, `tree \|\| missing`. Lane RENAME made it `tree` only. |
| `gen/cards/` (4 bases, 15:30:27) and `gen/cards_trees/` (49 bases, 17:22:05) | **THROWN AWAY as deliverables**, kept on disk as the earlier evidence. Both are the old candidate meaning and the old exe. |

---

## 1. Regions and bases chosen, and why

### Terrain — one region, nested, at all four far levels (KEPT from IMAGES3)

Chosen from **vanilla's shipped files and the ESM only**, never from anything we
generated (CONSTITUTION rule 4). `scratchpad/images_20260909/pick_handoff_region.py`
scores every dim-4 tile whose 4x4 cell block lies wholly **inside** the painted
box (x -36..32, y -41..32) on three numbers: `colour` (mean of
`max(RGB) - min(RGB)` on vanilla's diffuse at mip 2), `relief` (mean tilt of
vanilla's `_msn` away from up) and `shore` (fraction of the block's ESM height
samples below -300 world units; Commonwealth's flat sea bed is exactly -352, so
a tile with `shore` strictly between 0 and 1 holds both sea bed and dry land).

Winner of 306 candidates: **`Commonwealth.4.28.24`** — colour 15.08, relief
20.99 deg, shore 0.150, height span 2,704 units. The coarser levels are the
chunks that CONTAIN it, so all four pictures are the same ground at four scales:

| level | tile | cells |
|---|---|---|
| 4 | `Commonwealth.4.28.24` | 28..31 x 24..27 |
| 8 | `Commonwealth.8.24.24` | 24..31 x 24..31 |
| 16 | `Commonwealth.16.16.16` | 16..31 x 16..31 |
| 32 | `Commonwealth.32.0.0` | 0..31 x 0..31 |

Vanilla ships a `.BTR` and both sheets for all four, so every level is a real
vanilla-vs-ours comparison and none of it is "ours alone".

### Object chunks

* **ring 0** — `Commonwealth.4.-20.24`, Sanctuary. Vanilla ships it, so the
  comparison is real, and it is the chunk every byte-identity gate in the tree
  stands on.
* **far ring** — `Commonwealth.16.16.16`, the same ground as terrain level 16.
  Chosen because it is the case that only exists with `--slot-fallback`: **not
  one** of this chunk's refs fills the ring-16 MNAM slot, so without that flag
  (or `--impostors`) the chunk holds none of them, which is vanilla's own
  behaviour and why vanilla's own chunk is nearly empty.

### The impostor bases — the 19 trees, and why 19

`--candidates trees` **changed meaning in this build.** Until lane RENAME
(2026-09-09) it was `tree || missing`, a SUPERSET of the default `missing` set;
that is why the bake bungo saw held ShackBalconyFloor03 and RockCliff04. It is
`tree` only now (`src/nifcli.cpp`), and over the Sanctuary region
`-20 24 -17 27` it lists **exactly 19 bases, all of them trees**:

```
0004a074 835.0  TreeMapleForest2      0004a075 915.9  TreeMapleForest3
0004a073 712.9  TreeMapleForest1      00038599 463.9  TreeBlasted01
000393cd 437.0  TreeBlasted02         000531ae 636.3  TreeMapleblasted04
000531b3 345.8  TreeMapleblasted05    0003e0d1 781.0  TreeMapleForest7
000503b6 560.2  TreeMapleblasted02    000d9ca9 1007.8 TreeElmForest02
0003e08d 451.9  TreeBlasted05         0003e08f 566.2  TreeMapleblasted07
00049532 645.1  TreeBlasted04         0004d93b 581.9  TreeMapleblasted01
000d9ca8 1101.2 TreeElmForest01       0012154f 367.4  BlastedForestBurntTreeUpright03
00121550 439.9  BlastedForestBurntTreeUpright02
0003a28b 1183.3 TreeHero01            000a7206 465.4  TreeBlasted01Lichen
```

Three regions were listed to find the one the brief's number names, and they
differ — trees are a property of what is PLACED in the rectangle, not of the
worldspace: `-20 24 -17 27` (Sanctuary) **19**, `16 16 31 31` **21**,
`0 0 3 3` **16**. All three lists are in `scratchpad/images_20260909/cand/`.
The card library is baked over the 19.

---

## 2. Commands

Every path handed to the exe is ABSOLUTE (a relative `--out-dir` resolves
against `release/`, `MISTAKES.md` 2026-09-09). `ESM` =
`X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm`, `DATA` =
`E:/Tools/Fallout 4/DataUnpacked/Data`, `B` =
`E:/Projects/NifskopeWildWastelandEdition/scratchpad/images_20260909`.

**The command surface is the RENAMED one** (`scratchpad/lane_rename_report.md`
section 1). Nothing this lane runs used a retired spelling: `lodgen` and its
flags did not move, and the two that did (`lodt` -> `lodl`, `--lodv-check` ->
`--lodt-check`) are not on any path here except the sample set's validation.

### Terrain, four levels — `$B/gen_terrain.sh`

```
release/NifSkope.exe -no-gui lodgen "$ESM" --worldspace 3C \
  --terrain-region 28 24 31 27 --dim 4 --no-terrain-identity \
  --out-dir $B/gen/ours4 --tex-dir $B/gen/ours4/textures/terrain/Commonwealth \
  --data-root "$DATA"
```
and the same with `--terrain-region 24 24 31 31 --dim 8`,
`16 16 31 31 --dim 16`, `0 0 31 31 --dim 32`. All four exited 0.

`--no-terrain-identity` is required for a like-for-like picture: our chunks
default to writing the FO4CS identity channel into `Vertex Colors`, which
vanilla's do not carry and which this renderer multiplies into the diffuse.

### Hiding the water shape, symmetrically — `$B/hide_water.sh`

Both sides' terrain chunks carry a `WATER` `BSMultiBoundNode` whose shape has no
source texture, so NifSkope draws it as an opaque white sheet over the land. It
is hidden on **both** halves with one field, `Flags` 14 -> 15 (NiAVObject bit 0),
into a `*_nowater.BTR` beside the original. Nothing in the game folder is
written.

```
release/NifSkope.exe -no-gui set <abs .BTR> -b 4 -f "Flags" -v 15 -o <abs _nowater.BTR>
```

### Terrain renders — `$B/stage_and_shoot_terrain.sh`

Each side is staged as its own miniature data root so neither can borrow the
other's sheets, and both halves get one pinned camera:

```
WW_WINDOW_AT=1960,40 WW_RENDER_SHOT=<abs png> WW_RENDER_SIZE=1400x900 \
WW_RENDER_VIEW=8 WW_RENDER_CENTER=<x,y,z> WW_RENDER_DIST=<units> WW_RENDER_TIME=1 \
  release/NifSkope.exe --port <unused> <abs _nowater.BTR>
```

| level | centre | distance |
|---|---|---|
| 4 | 8192,8192,0 | 14000 |
| 8 | 16384,16384,0 | 28000 |
| 16 | 32768,32768,0 | 55000 |
| 32 | 65536,65536,0 | 111000 |

1400x900 is **delivered as 1507x841** (the hook clamps to one screen), the same
for both halves, read back with PIL rather than quoted from the request.

**Camera-pin control, pre-registered:** the same file at two distances must give
two different pictures. `img/ctl_dist14000.png` **185,566 B** against
`img/ctl_dist22000.png` **70,302 B** — the pin takes. And `ctl_dist14000.png` is
byte-for-byte the SIZE of `lit_van4.png` (185,566), which is the renderer's
determinism floor: two independent runs of one scene agree.

### Triangulation — `$B/draw_wireplans.sh`

The renderer has **no headless wireframe mode** (the only wire path is the
selection outline, `src/glview.cpp:7245`), and LOD channel 8 photographs BLACK on
terrain because no `.BTR` ships vertex normals. So the triangulation is drawn
offline from each chunk's own bytes, with the reader that measured far-terrain
vertex spacing:

```
python $B/wireplan.py <abs .BTR> <abs out.png> <dim> "<label>"
```

### Object chunks — `$B/gen_objects.sh` and `$B/gen_objects_noid.sh`

```
release/NifSkope.exe -no-gui lodgen "$ESM" --worldspace 3C \
  --terrain-region -20 24 -17 27 --dim 4 [--no-identity] \
  --out-dir $B/gen/obj_ring0[_noid] --tex-dir <that>/textures/terrain/Commonwealth \
  --data-root "$DATA"

release/NifSkope.exe -no-gui lodgen "$ESM" --worldspace 3C \
  --terrain-region 16 16 31 31 --dim 16 --slot-fallback [--no-identity] \
  --out-dir $B/gen/obj_far16[_noid] --tex-dir <that>/textures/terrain/Commonwealth \
  --data-root "$DATA"
```

All four exited 0. Renders: `$B/shoot_objects.sh` and `shoot_objects_noid.sh`,
same hook, `WW_RENDER_VIEW=8`, ring 0 centre `-73400,106170,9800` distance
24000, far ring `91800,99000,1000` distance 78000.

### The card library — 19 trees

```
OCT=8 TILE=128 CANDIDATES=trees bash tools/bake_impostor_cards.sh "$ESM" \
  -20 24 -17 27 $B/gen/cards_trees19
```

19 baked, **0 failed, 0 missing**. `log_bake_cards19.txt` is the whole run.

### The card chunk, and the card pictures

```
bash $B/gen_and_shoot_cardchunk.sh $B/gen/cards_trees19      # manifest chunk
bash $B/shoot_cardchunk2.sh                                   # four renders
bash $B/shoot_source_vs_card.sh $B/gen/cards_trees19 0004a074 "Landscape\Trees\TreeMapleForest2.nif"
python $B/sheetgrid.py $B/gen/cards_trees19 <formid> $B/img/cards_sheets_<formid>.png 1.0
```

---

## 3. The pictures

All under `scratchpad/images_20260909/`. Every one was OPENED and looked at
before delivery; the captions fit inside the frame.

| picture | what it is |
|---|---|
| `handoff_contact_sheet.png` | every picture below, thumbnailed and captioned |
| `handoff_terrain_L4_lit.png` | terrain `.BTR` dim 4, vanilla / ours, lit |
| `handoff_terrain_L8_lit.png` | dim 8 |
| `handoff_terrain_L16_lit.png` | dim 16 |
| `handoff_terrain_L32_lit.png` | dim 32 |
| `handoff_terrain_L4_triangulation.png` | every triangle edge of the LAND shape, dim 4 |
| `handoff_terrain_L8_triangulation.png` | dim 8 |
| `handoff_terrain_L16_triangulation.png` | dim 16 |
| `handoff_terrain_L32_triangulation.png` | dim 32 |
| `handoff_objects_ring0_dim4.png` | object chunk, ring 0, Sanctuary, vanilla / ours |
| `handoff_objects_far_dim16.png` | object chunk, far ring, vanilla / ours with `--slot-fallback` |
| `handoff_objects_identity_channel.png` | the same chunk with and without the FO4CS identity channel |
| `handoff_card_sheets_0003a28b.png` | the four card sheets + the coverage, TreeHero01, frame class 128x128 |
| `handoff_card_sheets_0004a074.png` | the same for TreeMapleForest2, class 96x128 |
| `handoff_card_sheets_000531b3.png` | the same for TreeMapleblasted05, class 32x32 — the bottom of the size ladder |
| `handoff_card_vs_source_0004a074.png` | the source model beside its card, three directions |
| `handoff_cards_in_chunk.png` | 882 cards standing in a far chunk, three directions + one close-up |

---

## 4. What the pictures show

Two sentences each, no cause verdicts.

**Terrain, lit, all four levels.** Ours carries the same triangle budget as
vanilla at every level (2,084 / 3,140 at dim 4, 2,134 / 2,364 at 8, 2,429 /
2,606 at 16, 3,358 / 3,450 at 32) on an identical vertex descriptor,
52,776,558,133,763, and both halves serve their own sheets from a staged root.
Ours reads visibly smoother and warmer than vanilla's at every level, and part
of that is in the frame rather than in the textures: our chunks ship Smoothness
1 / Specular #000000 / Clamp 3 against vanilla's Smoothness 0 / Specular
#ffffff / Clamp 0.

**Terrain, triangulation.** At dim 4 and 8 the two triangulations are the same
kind of thing — irregular, denser where the ground bends (vanilla 2,060 and
2,074 triangles, ours 2,265 and 2,304). At dim 16 and 32 vanilla's LAND shape
carries far more VERTICES than triangles (5,930 verts / 2,103 tris at 16;
5,872 / 2,168 at 32) where ours stays near 1,180 verts for a similar triangle
count, so the two files reach a similar surface by different vertex economies.

**Object chunk, ring 0.** Vanilla ships this chunk as 2 shapes, 33,703 verts,
24,482 tris; ours is 10 shapes, 34,958 verts, 25,475 tris, with the same rocks
and the same tree LOD silhouettes in the same places. The far-ring
simplification does not touch ring 0 by design, and the run reported
`merged: 10 shapes -> 10`.

**Object chunk, far ring.** Vanilla's dim-16 chunk holds 2 shapes, 4,170 verts,
2,564 tris — almost nothing, because an empty MNAM slot drops a ref at that
ring; ours with `--slot-fallback` holds 38 shapes, 129,814 verts, 80,827 tris of
the same ground. Some of ours photographs magenta, which is this renderer
failing to resolve a texture, and the run named one such case out loud: an
absolute Bethesda build path inside a material record.

**The identity channel.** The same chunk generated twice on the same camera:
with `--no-identity` it draws its textures, with the default it draws the FO4CS
object-identity channel, because that channel lives in `Vertex Colors` and this
renderer multiplies vertex colour into the diffuse. The flag also changes the
file (860,743 bytes against 1,280,239) and the far-ring simplification, which
groups by identity index — 78,154 -> 72,022 triangles with it on against
78,154 -> 71,411 with it off.

**The card sheets.** Each card is an 8x8 grid of 64 views on one sheet, in four
textures — base colour (its ALPHA is the coverage, and the silhouette), normal
with height and sway, GSAOS, and emissive — with the frame grid drawn on top and
every number read from the bake's own sidecar. The emissive sheet is BLACK on
every one of the 19 trees and their `emissiveScale` is 0, which is what the
corpus measurement predicted: no vanilla LOD source emits.

**Source model vs card.** TreeMapleForest2 drawn by this renderer from ViewLeft,
ViewFront and ViewRight, with the card frame the bake's octahedral law puts
exactly on each of those directions underneath it, nearest-scaled so the texels
are visible. The three card frames are visibly three different views of the same
tree and line up with the model above them.

**Cards in a chunk.** 882 cards from the 19-tree library, standing in the far
chunk that has no mesh LOD of its own, from three directions and one close-up.
The quads draw OPAQUE rather than cut to the silhouette — see the
observation in section 5.

---

## 5. Observations, none of them fixed

The brief's item (f) asked for evidence, not repairs. Nothing in `src/` was
touched and nothing was built.

### 5.1 The impostor card quads draw OPAQUE in a chunk

`handoff_cards_in_chunk.png` is the evidence and it names the base:
**0003a28b, TreeHero01**, manifest index 200 of
`gen/cardchunk/Commonwealth.16.16.16.BTO`.

What is measured, and only that:

* the chunk's card shapes name `Data\Textures\Lodgen\Cards\<id>_fs.DDS` — the
  **stock crossed quads**, not the octahedral sheet (`lodgen --dump-shapes`);
* every `_fs.DDS` this run wrote is **DXT1, `pfflags 0x4`** (fourCC only, no
  `DDPF_ALPHAPIXELS`), 3014x501, 8 mips, 1,014,600 bytes — the same for every
  base;
* vanilla's own alpha-tested LOD tree textures are **DXT5**:
  `Textures/LOD/Trees/MapleBranchesLOD_d.dds` and `ElmBranchesLOD_d.dds`,
  256x256, 9 mips, 87,536 bytes each;
* the **octahedral** sheets are DXT5 — `0003a28b_oct_d.DDS`, 1024x1024, 5 mips —
  so the FO4CS-native path is NOT implicated by this;
* the card shapes do carry an alpha property (`alpha 1` in the dump), and the
  same renderer alpha-cuts our own `ElmBranchesLOD_d` / `MapleBranchesLOD_d`
  shapes in the ring-0 chunk in the same session.

**Candidate**, named as a candidate: the `_fs` conversion writes BC1 without
punch-through alpha, so there is no coverage left to test against.
**Discriminator**: re-convert one `_fs` as DXT5, or as BC1 with `bc1Alpha`, and
photograph the same card on the same camera — if it cuts, that is the cause; if
it still draws solid, the alpha property or its threshold is.

### 5.2 `WW_RENDER_CENTER` / `WW_RENDER_DIST` do not take on the axis views

Measured on one `.BTO`, with a control on each side:

| view | what changed | result |
|---|---|---|
| `WW_RENDER_VIEW=8` (ViewUser) | distance 3000 vs 60000, one centre | 8,723 B vs 10,983 B — **the pin takes** |
| `WW_RENDER_VIEW=3/5/4` | centre AND distance both changed | 11,277 / 11,598 / 10,801 bytes, **twice over, unchanged** — the pin does not take |

The `nifskope-ww-render-shot` skill already records this for a GENERATED
document (a `.btd` / `.lodl` opened through the terrain route). This is a
plain parsed `.BTO`, so the skill's condition is too narrow: the axis views
lose the pin too. No cause claimed; the hook applies `setPosition` /
`setDistance` after `setOrientation( view, true )` in both cases
(`src/nifskope_ui.cpp:21575-21591`). **The skill has been amended** — section 7.

### 5.3 One of the 19 trees has no converted card

`000a7206 TreeBlasted01Lichen` baked its PNGs and its sidecar like the other 18,
but `lodgen --impostors` wrote no `_oct.lodm` and no DDS for it: 18 `.lodm` and
90 `.DDS` against 19 sidecars. Candidate, unverified: no placement in the region
being generated stood on that base, and the conversion is per placed card.
Stated so the sample set's counts are not read as a loss.

### 5.4 The far chunk's magenta

`handoff_objects_far_dim16.png`. The run printed, twice,
`File " materials/c:/projects/fallout4/build/pc/data/materials/lod/metalindbeamslod01.bgsm " not found in archives` — an absolute Bethesda BUILD path
inside a record. The sample run printed a longer list of the same shape for
`.lodm` sidecars that do not exist (`materials/lod/churchlod_d.lodm` and seven
others), which is the "no source `.lodm`, fall back to legacy" path working, not
an error. Both stated as observed.

### 5.5 No rock with floating decals appeared

The brief asked me to keep a frame if one showed up. It did not, and the reason
is structural rather than lucky: `--candidates trees` is tree-only in this
build, so no rock base was baked at all. Every one of the 19 sidecars names a
`Landscape\Trees\*.nif`. There is nothing to keep.

---

## 6. The FO4CS sample set

`scratchpad/handoff_fo4cs/samples/` — **129 files, 170,004,485 bytes (162.1 MB)**.
`samples/MANIFEST.md` is the authority: every file with its size, the command
that made it, and the contract document it obeys, with every size read from disk
by `samples/make_manifest.py` rather than typed. `samples/make_samples.sh` is
the run and `samples/make_samples.log` its whole stdout.

**Region:** the one containing cell (0,0), cells 0..3 x 0..3, at four far levels.
The sweep bakes every chunk touching that rectangle, which here is exactly the
one chunk that contains it, so the four levels are nested views of the same
ground: `Commonwealth.4.0.0` (cells 0..3), `8.0.0` (0..7), `16.0.0` (0..15),
`32.0.0` (0..31).

**Profile:** `--arrays --impostors <the 19-tree library> --cover`, identity ON
(the default), `--slot-fallback` at 16 and 32, and **no `--atlas`** — the atlas
is the stock engine's draw-call optimisation and the native target drops it
(`README.md` §1). That is `README.md` §5's own former sentence for what a first
lane must run.

| level | `.BTO` | manifest | array classes | card arrays |
|---|---:|---:|---|---|
| 4 | 2,058,014 | 307,970 | 128x128, 256x256 | **none** — cards do not substitute at ring 0, and the run says so |
| 8 | 4,807,296 | 709,561 | 128x128, 256x256 | 1 group, 157 `C` lines |
| 16 | 11,294,220 | 1,927,822 | 128x128, 256x256 | 18 card sets in 16 arrays, 4 groups, 2,662 `C` lines |
| 32 | 24,704,237 | 4,080,485 | 128x128, 256x256 | 18 card sets in 16 arrays, 4 groups, 8,316 `C` lines |

**The `.lodt` pyramid** is `samples/vt/Terrain/`: five levels, 2 / 4 / 8 / 16 /
32, plus `Commonwealth.VT.lodm`. `--vt-height` was passed deliberately (it is
off by default and costs +133% a tile) because a sample set that omits a sheet
cannot be used to write a reader for it. `samples/vt/tex/` is that run's own
chunk output, kept because `--vt-btr` (on by default) ASSEMBLES the `.btr`
sheets from the pyramid rather than baking them again, so it is the one artefact
proving the assembly path ran.

**Everything was read back through this tree's own validators**, which is the
difference between "it wrote a file" and a sample set. `lodgen --lodt-check`
walks every rule of `docs/LODGEN_TERRAIN_VT.md` §3.4 and verifies **every tile's
CRC**; all five levels exited 0:

| level | present tiles | cover tiles | stored bytes |
|---|---:|---:|---:|
| 2 | 32 | 30 | 11,744,960 |
| 4 | 8 | 8 | 2,959,360 |
| 8 | 4 | 4 | 1,479,680 |
| 16 | 2 | 2 | 739,840 |
| 32 | 1 | 1 | 369,920 |

`lodgen --lodm-check` on one sidecar of each kind — `terrainVT`, `array`,
`cardArray` — all `lodm ok 1`, version 1, family legacy. The index says
`"partial": true` with `extent {0,0,3,3}`, which a consumer must read rather than
assume a whole worldspace.

### What the README now says, and one correction it did not ask for

`README.md` §5 was rewritten (`ww-contract-provenance`: hash and line count
first, every number re-derived from the artefact, the version constant read
last, hash diffed end to end — `6eac4668a62afacf` 16,823 B / 269 lines ->
`c786bfabd2f6af8e` 19,835 B / 330 lines, CR count unchanged at 0 on both sides).

**§4 had to be corrected as well, and it is the one thing in this lane an FO4CS
engineer must not miss.** That section said the five `.lodl` files installed in
bungo's mod folder are version 1 and unaffected by the v1/v2 split. Read from
the files at byte 0x04, all five are **version 2**, re-baked and renamed
2026-09-09 17:27:

| file | bytes | version | written |
|---|---:|---|---|
| `Commonwealth.lodl` | 35,953,294 | **2** | 17:27 |
| `DLC03FarHarbor.lodl` | 9,195,933 | **2** | 17:27 |
| `NukaWorld.lodl` | 7,182,356 | **2** | 17:27 |
| `DiamondCity.lodl` | 53,148 | **2** | 17:27 |
| `NukaWorldAmphitheater.lodl` | 38,303 | **2** | 17:27 |

`Commonwealth.lodl` carries the version-2 fields: default water height **450.0**
at 0x98, WATR form **0x18** at 0x9C. The version-1 twins are the
`*.lodt.bak-20260909` copies beside them (2026-09-05 03:14). FO4CS's parser pins
`kVersion = 1u`, so **the installed set would now be refused**, not misread.
Nothing was changed in bungo's mod folder by this lane.

§6 open item 3 was also softened from a claim to a question: it said the
installed files still carry the landless-cell defect, but they were re-baked
after that fix landed, and nobody has re-run the heightmap comparison against
them. Stated as unverified rather than corrected in either direction.

### The mtime table, because these are four different clocks

| artefact | mtime |
|---|---|
| `release/NifSkope.exe` | 19:35:14 |
| `gen/ours4/…BTR` (terrain) | 19:47:42 |
| `img/lit_ours4.png` (a render) | 19:49:02 |
| `handoff_terrain_L4_lit.png` (composed) | 19:51:42 |
| `gen/cards_trees19/0003a28b.txt` (the bake) | 19:55:57 |
| `gen/obj_ring0_noid/…BTO` | 19:56:31 |
| `handoff_cards_in_chunk.png` | 20:08:07 |
| `handoff_contact_sheet.png` | 20:11:09 |
| `samples/vt/Terrain/Commonwealth.VT.2.lodt` | 20:11:38 |
| `samples/MANIFEST.md` | 20:15:27 |
| `handoff_fo4cs/README.md` | 20:16:55 |

Everything is newer than the exe, every composed picture is newer than every
render it holds, and **the contact sheet is newer than all sixteen pictures in
it** (`find … -newer handoff_contact_sheet.png` returns nothing). That last
check is the one IMAGES4 failed.

---

## 7. Mistakes

All three are in `MISTAKES.md` at the repo root, written the moment they were
recognised (CONSTITUTION rule 2). In brief:

1. **I rendered a picture on an unverified camera, twice, and only the picture
   told me.** The card-in-chunk shots were framed on the manifest `C` line's
   `cx cy cz` — which are an OFFSET FROM THE PLACEMENT, not a world position —
   and taken on `WW_RENDER_VIEW=3/5/4`, where the framing does not take at all.
   Three plausible-looking PNGs came back and I composed with them. Found by
   opening one, and then by running the two-distance control the
   `nifskope-ww-render-shot` skill says to run BEFORE trusting any framing —
   which the terrain half of this same lane had run and passed an hour earlier.
   A plausible file size is not evidence a render took.
2. **I turned off the flag that writes the file the next step reads.**
   `--no-identity` was added to the card-chunk script in place, to stop the
   chunk photographing in the identity hash; it also suppresses the
   `.BTO.manifest.txt`, which is where the next step gets the card to frame on,
   and the chunk that had one had been overwritten. Cost one regeneration. The
   fix is the twin pair `gen/cardchunk` / `gen/cardchunk_noid`, which is a better
   picture than either alone.
3. **IMAGES4 delivered composed pictures older than the files they were composed
   from** (mine to find, not to have made): it regenerated `gen/` at 17:33–17:34
   and left `handoff_*.png` written at 16:29 and 16:36. Their captions happened
   to still be right, which is worse than being wrong, because nothing announced
   it.

## 8. Finished-work skill review (CONSTITUTION 1a)

**Loaded and used.** `nifskope-ww-render-shot` — the invisible-headless section
first, before anything that repaints in a loop, then the switch table, the
two-distance camera-pin verification, the "every shot function prints the size
of the file it just wrote" rule, and the `env ${flag:+…}` rule (both of which
this lane's scripts already followed and both of which earned their place).
`nifskope-ww-lodgen` — the CLI table, the far-ring / MNAM-slot facts that
decided which object chunks are worth photographing, the card frame law, and the
editing traps (every in-place edit here was a Python patch script that asserts
its anchor count and its CR count, never a heredoc). `ww-contract-provenance`
— for the README edit: hash and line count before, the version constant read
LAST and from the file, the hash diffed end to end. `CONSTITUTION.md` and
`MISTAKES.md` at the top, `docs/LODGEN_TERRAIN_VT.md` in full before the VT run.

**Amended, in BOTH trees** (they drift; a repo-cwd lane reads only the repo one
— CONSTITUTION 1a): `nifskope-ww-render-shot` gained the axis-view half of the
camera-pin failure. It said the pin is lost on a GENERATED document; it is also
lost on the axis views of an ordinary parsed `.BTO`, which is a different and
wider claim, and the section now carries both with the control numbers.
`E:\Projects\Claude\.claude\skills\…` and
`E:\Projects\NifskopeWildWastelandEdition\.claude\skills\…` are byte-identical
after the edit (`cmp` clean), 13,462 -> 14,271 bytes, CR 0 both.

**The skill I wish had existed, and I am writing it: none — and here is why,
named rather than passed over in silence.** The one procedure this lane
re-derived from first principles was *"a lane's own outputs are stale the moment
the exe moves: classify every file in the working folder as an INPUT (from
vanilla or the ESM) or an OUTPUT (from our exe), regenerate every output, and
prove the composed artefact is newer than everything in it."* That is §0 and the
mtime table of this report, it is the exact thing IMAGES4 got wrong, and it is
the third relaunch of this lane. It is a real, recurring procedure — but it is
one paragraph and it belongs beside the existing mtime rule in CONSTITUTION
rule 4 ("before putting two artefacts in one sentence, put their MTIMES in one
table"), not as a skill of its own. **Recommend the director extends that
constitution bullet with the input/output classification and the
newer-than-everything-in-it check**, rather than a fifth skill for this repo.
I have not edited `CONSTITUTION.md` myself: only bungo amends it.

**A second candidate, declined with a reason.** "Compose a vanilla-vs-ours
picture pair" looked like a skill for a moment. It is not: it already IS
`nifskope-ww-vanilla-compare`, and every mechanic this lane used —
staged data roots, one pinned camera, the crop box, labels burned in — came
straight out of it through IMAGES3's scripts. Writing it again would be the
duplication rule 1a exists to prevent. What I added instead is the one thing it
did not have: `--no-identity` / `--no-terrain-identity` as a REQUIREMENT of a
like-for-like picture, which is now stated in three captions and in §4 of this
report, and which the vanilla-compare skill's "two things that silently put a
difference in the picture that is not the difference under test" section would
be the right home for. **Recommend the director splices that sentence there.**
