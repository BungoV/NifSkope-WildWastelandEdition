# Lane TIDY1 -- empty channels, duplicate texture layers, misleading views (2026-09-27)

Worktree E:\Projects\NifskopeWWE-tidy1, branch tidy1-20260927 from ground1-20260927 @ da045481
(night-20260927 + GROUND1's .lodi v12). Off baseline = a bake of ground1-20260927's exe.
The NifSkope turn lock was stuck (held by "anon", process gone) at launch; not touched.

## 1. Skills loaded
- nifskope-ww-lodgen, search-lean (more below as loaded)

## 2. What was wrong (audit ranks 7, 8, 9)

### Rank 7 -- empty channels: is there any glow to bake?
`tools/lod_emission_probe.py` over Fallout4.esm (Commonwealth, DiamondCity, DiamondCityFX), extended this lane to
also read each material's glow-map flag and glow texture slot:
- LOD models named by a base: 3361, read 3354 (7 missing). Shader blocks 3430. Own-Emit set on 100% of them, but
  0 with a colour that is not black. 0 models with an effect shader. 0 with a filled glow slot.
- Materials those LOD models name: 121, all read. Emit enabled 0. Glow-map flag 0. Glow texture slot filled 0.
  Emitting with a colour: 0.
- The only emitters in the plugin are the full stadium-light models, and they have no LOD slots.
**Count of LOD materials with a glow source: 0.** The bake did not miss anything: vanilla LOD has no glow.

Decisions, one per channel:

| channel | decision | why | FO4CS reader impact |
|---|---|---|---|
| emissive `_g` sheet on every texture array (mesh and card) | **DROP** when every layer in the set is black with emissive scale 0 | 0 glow sources (above); the file is pure black. Boston box: 52,538,372 B of 368,010,044 B in the arrays (14%) | the `.lodm` no longer names `textures.emissive` for such a set; absent = no emission. The reader must treat a missing emissive as "none" (card sets included) |
| mesh `_n` blue/alpha (constant 132/0) | KEEP | a BC3 block is 16 bytes whatever the channels hold; dropping B/A saves 0 bytes. Only BC1/BC5 would save, and that is a different codec for X/Y | none; readers already ignore B/A of a mesh normal |
| mesh `_gsaos` blue (constant 255) | KEEP | same: 0 bytes to save inside BC3 | none |
| `.lodl` ground-cover plane | already ABSENT on FO4 | Fallout4.esm has 0 GCVR records; GCVR is a Fallout 76 terrain concept, so the writer already leaves section bit 1 clear (the installed file has flags 13 = colour + AO + overview). 0 bytes shipped. The real FO4 ground cover is the VT mask sheet's A channel | none; the viewer's label now says so |
| `.lodl` AO plane (coarser copy of mask B) | KEEP | it is the floor under mask B for a bake or a reader with no VT sheets, and `--refresh-ao` rewrites it in place. 2.4 MB whole map. Its absence is already legal (bit 2) | none |
| per-piece sky byte 0x11 (coarser copy of the per-vertex stream) | KEEP | it sits inside the fixed-size instance record: dropping it saves 0 bytes without a record-layout change, and it is the fallback for a reader without the stream (GROUND1's v12 must not be collided with) | none |

### Rank 8 -- duplicate array layers
The ground1 Boston bake's sidecars list 114 layers. 10 are the same material under two spellings,
`c:\projects\fallout4\build\pc\data\materials\lod\X.bgsm` beside `materials\lod\X.bgsm`
(128x128: BldgBrickLarge, DecoLarge, HitTechStructureLarge, NCALarge; 256x256: Billboard02, HitTechExtALarge,
HitTechStreaks, HWOverpass, RWRiver, WhiteMarbleBLD). Three more pairs share the same three textures but are keyed
once by material and once by diffuse (RockSlab01, RockSlab02, Wrhs01).

### Rank 9 -- views that mislead
(in progress)

## 3. What changed
(in progress)

## 4. Gates
(in progress)

## 5. Commits
(in progress)

## 6. Pictures
(in progress)

## 7. Still not right / resume steps
(in progress)

## 8. Skills review
(in progress)
