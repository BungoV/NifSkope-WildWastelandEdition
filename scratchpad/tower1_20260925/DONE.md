# TOWER1 -- are our downtown Boston LOD towers greyer than vanilla's LOD towers?

Lane TOWER1, branch tower1-20260925 from 8f8caebe, worktree E:\Projects\NifskopeWWE-tower1. No code change planned;
renders use a COPY of main's release (NifSkope.exe sha1 375b42b3, built 21:09) in scratchpad run/release.

## 0. Progress log
- 22:26 started (clock read). BAKE2's whole-map lodgen (NifSkope PID 18580, -no-gui) is running: no render until it ends.
- 22:28 exe copied (GREY1's run/release, sha1 375b42b3 = main release 21:09). Towers identified (section 1).
- 22:37 offline discriminator done (sections 2-3): FOUND A DEFECT -- vanilla's LOD applies material swaps that name
  LOD materials; our bake drops them. Renders still wait on BAKE2.

## 1. The towers (towers.py, tower_mats.py)
Placements of the installed library (dim-4 manifests) in BAKE1's Boston window, cells -5,-10..2,-3: 11,416. The
three tall towers are kit-built from ~100 HitTech exterior LOD pieces each (not one model), clustered from every
non-tree placement topping out above z 4500 (1024-unit linking). Projected into the 08 camera (ortho, rot
-63.56/0/133.31, look-at -2048,-26624,0, 20.48 units/px) they are the three tall shapes in the picture:

| tower | where in the 08 picture | world box x / y | top z | placements | slot-0 LOD models |
|---|---|---|---|---|---|
| A | left tall tower (shacks on top) | 2835..4403 / -25103..-23489 | 9596 | 508 | 106, top HitExtAWall01_LOD, HitExtAWallTallLongDummyLOD_LOD, HitExtATrimVBTall01_LOD |
| B | right tall tower (red frame) | -3647..-835 / -31540..-28388 | 8880 | 458 | 104, top HitExtStructureBeamAWallALong01_LOD, HitExtAWallTallLongDmg02_LOD |
| C | grey spire between them | 2496..3520 / -28289..-27264 | 7031 | 75 | 20, top HitExtAWallTall01_LOD, HitExtAWindowDShortCapTop02_LOD |

Their LOD materials (all resolve through his MO2 stack, 0 missing): materials\lod\hittechextalod01.bgsm (534
placements), hittechext03lod (283), hittechextblod01 (129), hittechstain_lod (77), 3 small others. The textures
come from DLCUltraHighResolution - Textures07.ba2; decoded, they are pixel-identical to the base-game copies (mean
abs diff 0.0000), so the HD pack is not a factor. Vanilla stock object LOD for the same cells: Meshes\Terrain\
Commonwealth\Objects\Commonwealth.4.0.-8.BTO (towers A, C) and Commonwealth.4.-4.-8.BTO (tower B), textured from
the one CK atlas Commonwealth.Objects.DDS (4096x2048). Stock .bto vertices are chunk-local and scaled by 1/dim.
Side note: in his stack BNS Trees - Textures.ba2 replaces that atlas (4096x4096 BC3); the comparison uses the
loose vanilla copy only.

## 2. The discriminator: what the vanilla atlas holds for these towers (atlas_use.py, pics/atlas_used_by_towers.png)
The texels the vanilla tower triangles sample (towers A+B) are not one grey HitTech sheet: the atlas holds the
HitTech sheet in SEVEN colourways (teal, blue, rust, orange, cream-white, grey...). Fallout4 - Materials.ba2 ships
them as materials\lod\hittechextalod01..08.bgsm and hittechextblod01..08.bgsm. Their diffuse means:

| LOD material | mean sRGB | mean S |
|---|---|---|
| hittechextalod01 (the one our bake uses) | 0.472 0.459 0.443 | 0.126 |
| hittechextalod03 | 0.523 0.664 0.748 | 0.366 |
| hittechextalod04 | 0.777 0.557 0.445 | 0.396 |
| hittechextalod05 | 0.580 0.698 0.632 | 0.266 |
| hittechextalod07 | 0.796 0.758 0.731 | 0.101 |
| hittechextalod08 | 0.754 0.417 0.340 | 0.503 |

Vanilla Fallout4.esm MSWP records carry LOD entries: e.g. HitTechMetalPanel07Full (000F0BF5) swaps
materials\lod\hittechextalod01.bgsm -> hittechextalod07.bgsm. The CK applies the placement's swap (REFR XMSP,
else the base's MODS) to the LOD model when it builds the .bto; our lodgen never reads XMSP/MODS/MSWP
(src/: 0 hits outside two viewer comments). Per tower (tower_swaps.py):
- A: 271 of 508 placements swapped; entries naming OUR LOD material: alod01->alod07 169, blod01->blod04 31, alod01->alod04 23.
- B: 203 of 458 swapped; alod01->alod07 137, blod01->blod07 53, alod01->alod03 11.
- C: 19 of 75 swapped; blod01->blod02 13, alod01->alod02 2.

## 3. Numbers per tower, offline (van_tower.py, swap_predict.py)
Area-weighted mean over the surfaces that draw each tower: vanilla = the stock dim-4 .bto triangles in the tower
box sampled from the atlas at their UVs; ours = every placement's LOD NIF sampled from its own BGSM diffuse
(GREY1's atlas_vs_full.measure), weighted by area x scale^2. "ours + swap" = the same with vanilla's MSWP entry
applied -- the refuter: if the swap is the cause, it must land on vanilla.

| tower | arm | mean sRGB | S of mean | mean per-texel S | linear luma Y |
|---|---|---|---|---|---|
| A | vanilla | 0.713 0.631 0.593 | 0.168 | 0.213 | 0.377 |
| A | ours as baked | 0.524 0.444 0.407 | 0.224 | 0.199 | 0.179 |
| A | ours + vanilla swap | 0.715 0.633 0.595 | 0.167 | 0.208 | 0.378 |
| B | vanilla | 0.730 0.629 0.590 | 0.191 | 0.258 | 0.379 |
| B | ours as baked | 0.582 0.456 0.408 | 0.298 | 0.266 | 0.199 |
| B | ours + vanilla swap | 0.729 0.630 0.592 | 0.189 | 0.256 | 0.380 |
| C | vanilla | 0.401 0.404 0.395 | 0.021 | 0.075 | 0.135 |
| C | ours as baked | 0.416 0.422 0.416 | 0.016 | 0.078 | 0.148 |
| C | ours + vanilla swap | 0.406 0.408 0.399 | 0.021 | 0.080 | 0.138 |

Verdict (offline): towers A and B differ from vanilla, C does not. With the swap applied ours lands on vanilla
to the third decimal on all three. A and B are HALF as bright as vanilla's (Y 0.18-0.20 vs 0.38) and lack the
cream/orange/blue panels; the average saturation is similar, so what reads as "grey" is a dark, uniform grey-brown
sheet where vanilla has a light cream tower with coloured panel runs.

Whole Commonwealth (lodswap_census.py): of 184,069 placements, 42,161 (Fallout4.esm refs) carry a swap and
21,064 carry one whose MSWP names their slot-0 LOD material (19,523 buildings, 1,360 misc, 181 rocks; 41 distinct
LOD swap pairs). Top: decomainlod -> decomainblod 8,062; decomainlod -> decomainfactorybricklod 3,626;
hittechextalod01 -> 07 1,947, -> 05 889, -> 03 714, -> 06 631, -> 02 629; metalindbgpipeslod01 -> rust 514;
decomainlod -> whitemarblebldlod 512. 22,087 placements come from other plugins and were not checked.

## 4. Proposed fix (NOT implemented, no re-bake, no install)
Defect: lodgen places every REFR with its base's LOD models and their own .bgsm; it drops the material swap the
CK applies when it bakes vanilla's .bto. Where the swap lives (xEdit wbDefinitionsFO4): REFR XMSP (formID of an
MSWP) overrides the base's MODS; MSWP = rows of BNAM (original material) -> SNAM (replacement), CNAM (colour
remap index; none of the LOD rows measured here needed it -- swap_predict lands on vanilla without it).
1. src/esmdata.h/.cpp: `EsmRefr::materialSwap` read from XMSP beside XSCL (esmdata.cpp:475); `EsmLodBase::
   materialSwap` from the base's MODS; a map MSWP formID -> [(BNAM, SNAM)] from every MSWP record, winning
   record by load order like every other record.
2. src/nativeemit.cpp (library build ~1588 and base rows ~1904): key the library by (base, effective swap)
   instead of base alone. A swapped pair gets its own base row whose models are the same NIFs with each shape's
   material replaced when its path (folded) equals a BNAM; only pairs whose MSWP names a material the LOD
   model actually uses get a row (41 distinct pairs on the Commonwealth, so the library grows by tens of
   meshes, not thousands). Placements of that pair point at that row.
3. Census line in the bake log: placements read / carrying a swap / swap names one of their LOD materials
   (the Commonwealth answer must be 21,064 from Fallout4.esm alone, more with the DLC plugins).
Refuter (pre-registered): re-bake cells 0,-8 (towers A and C) and -4,-8 (tower B) with the fix, then
 (a) swap_predict-style offline mean over the NEW library: tower A linear Y must move from 0.179 to 0.378 +- 0.01,
     B 0.199 -> 0.380, C must stay 0.14 +- 0.01 (the control: few of its surfaces are swapped);
 (b) render the same 08 camera, base-colour arm (WW_LOD_CHANNEL=12): tower A's mask mean must move toward
     vanilla's by the same ratio; if it stays at ~0.18 the swap is not the cause, or not reaching the render;
 (c) the census line must say 21,064 on Fallout4.esm; 0 = the parse is not wired.

## 5. Rendered, the 08 camera (pics.sh, pics_bto.sh, measure_pics.py) -- 23:08
Side by side: `pics/TOWER1_vanilla_vs_ours.png` (untracked, it holds game art): the full 08 frame (vanilla | ours as
bungo saw it, WW_LODL_AO=1), then the tower crop x2. Exe = run/release copy, sha1 375b42b3 (main's 21:09 build).
Frame read back 1600x1624, upp 20.480 (ours .cam). Camera facts measured, not assumed:
* NifSkope draws a stock .BTO in WORLD space (auto-fit look-at of 4.0.-8 = 10012,-24730,1135), so the BTOs take the
  native camera unchanged. pics.sh's chunk-local /4 camera drew 14 of 18 BTOs blank (11,411 B files); re-shot by
  pics_bto.sh. A .BTR is drawn chunk-local (the world camera draws it blank), so the terrain keeps the /4 camera; it
  is context only and is not measured. Towers land on the same pixels in both halves (see the picture).
* One mask per tower = the vanilla tower triangles projected with the view-8 basis, eroded 2 px (A 22,613 px,
  B 21,261, C 3,656). Every masked pixel is covered on every picture.

| tower | arm | mean sRGB | S of mean | mean S | linear Y |
|---|---|---|---|---|---|
| A | vanilla base colour (ch 12) | 0.768 0.693 0.655 | 0.147 | 0.196 | 0.458 |
| A | ours base colour (ch 12) | 0.511 0.458 0.430 | 0.158 | 0.160 | 0.185 |
| A | vanilla lit | 0.832 0.760 0.722 | 0.132 | 0.176 | 0.560 |
| A | ours lit, vertex colour on | 0.485 0.450 0.428 | 0.117 | 0.152 | 0.176 |
| B | vanilla base colour | 0.700 0.625 0.590 | 0.158 | 0.217 | 0.367 |
| B | ours base colour | 0.552 0.458 0.416 | 0.247 | 0.226 | 0.193 |
| B | vanilla lit | 0.763 0.686 0.652 | 0.146 | 0.199 | 0.449 |
| B | ours lit, vertex colour on | 0.474 0.410 0.378 | 0.202 | 0.214 | 0.149 |
| C | vanilla base colour | 0.384 0.386 0.375 | 0.030 | 0.070 | 0.123 |
| C | ours base colour | 0.396 0.402 0.392 | 0.026 | 0.071 | 0.133 |
| C | vanilla lit | 0.450 0.454 0.442 | 0.027 | 0.067 | 0.173 |
| C | ours lit, vertex colour on | 0.399 0.407 0.398 | 0.022 | 0.068 | 0.136 |

The render agrees with the offline numbers: A and B at 40-53% of vanilla's base-colour luma, C (the control, few
swapped surfaces) within 0.01. Saturation is NOT lower on ours (mean S 0.16/0.23 vs 0.20/0.22): the "grey" is a
darker, blue-grey panel sheet where vanilla has cream/white panels with orange runs. Vertex colour + AO costs ours
a further 5-23% of luma (A 0.185 -> 0.176 lit; B lit 0.149); that is GREY1's territory, not this defect. The lit
arms also carry vanilla's stock shader constants (smoothness 1, spec white), so compare causes on the base arm.
Answer to task 4: the colours do NOT match, so GREY1's per-placement multiplier is not the fix for these towers.

## 6. Skills
* Loaded: nifskope-ww-render-shot, nifskope-ww-vanilla-compare, ww-whole-map-picture, search-lean.
* Wished for: one that said "a stock .bto is drawn in world space, a .btr chunk-local" -- now in the new skill.
* Written: `fo4-lod-material-swap-check` (E:\Projects\Claude\.claude\skills\ and E:\Tools\AISkills\): name the
  buildings from the manifests, the offline atlas-vs-ours discriminator, the MSWP swap refuter, the whole-map
  LOD swap census, and rendering stock chunks at a native camera with the composite + projected mask.
