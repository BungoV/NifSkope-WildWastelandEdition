# Lane SLAB1 — the slab lattice: the object term of `--terrain-object-ao` stops turning a deck into a solid block

Tree `E:/Projects/NifskopeWildWastelandEdition`, branch `main`, only lane in the tree. Opened 2026-09-18 06:00 CEDT
(`date`-read). Report written incrementally, a section per finished step.

## 0. The exe at launch, and the rung

| what | value |
|---|---|
| `release/NifSkope.exe` mtime | 2026-09-18 **05:18:11.109723500 +0200** (`ls --time-style=full-iso`) |
| size | **22,663,680 B** |
| sha1 | **`a6d213e23258a9c22d0f10277ed48401bf9fd923`** |
| rung to be cut before the first build | `release/NifSkope.before_slab1.exe` (same bytes) |
| processes at launch | `tasklist \| grep -i -E "Fallout4\|NifSkope"` → no match, rc=1. No game, no NifSkope. |

That is the exe hotfix 7d left (HANDOFF: 05:18:11, 22,663,680 B, sha1 a6d213e23258) — read here, not copied from the
brief.

Read order actually followed: `scratchpad/brief_slab1.md` in full → `CONSTITUTION.md` in full → `HANDOFF.md` top block
and the DIRECTOR HOTFIX 7d / 7c / 7b / 7 / 6 entries and the GROUND1 LANDED block → root `MISTAKES.md` top entries →
`WW_CHANGES.md` hotfix 7 entry + addenda 1–3 → `src/lodgen.cpp` 8146–8420 in full, the three use sites (8971, 9740 +
9857, 11531 + 11082) and the report census clause 11986–12006 → `docs/LODGEN_TERRAIN_VT.md` §2.5h (and §2.3's AO byte
paragraph at 957–962) → `docs/LODGEN_CENSUS.md` §6.1 (it does **not** name `objAo*` anywhere: see §9).

## 1. The before measurement (step 1, on the exe at launch)

### 1.1 chunkD3's switch set, recovered VERBATIM and not retyped

`scratchpad/viewfix_20260917/chunkD3/bake.log` does **not** carry its own command line. The bake record does:
`chunkD3/vt/Commonwealth.lodb` stores the argument vector one token a line (lane BAKEREC1; `src/lodbfile.cpp:389`),
`27 switch token(s)`, and all 27 came out of it:

```
<esm> --worldspace 3C --terrain-region 4 -12 7 -9 --dim 4
      --data-root "E:/Tools/Fallout 4/DataUnpacked/Data"
      --out-dir <root>/vt --tex-dir <root>/vt/tex --vt <root>/vt
      --vt-finest 1 --vt-content 512
      --msn-cache "E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals/Textures/Terrain/Commonwealth"
      --road-detail 1 --terrain-object-ao
```
with `<esm>` = `X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm` (330,776,576 B, present) and only the
three output paths repointed. Nothing was unrecoverable, so nothing had to be dropped from either side.

**One correction to the brief.** The brief writes `--terrain-object-ao 0.5`. There is no such form:
`--terrain-object-ao` is a bare flag (`src/nifcli.cpp:7866`) and the strength is the separate
`--terrain-object-ao-strength` (`:7874`, default 0.5f, `src/lodgen.h:1109`). chunkD3 passed the bare flag and took the
default, which is why its census reads `objAoStrength 0.500`. Both sides of this lane pass the bare flag too.

The recipe is in `scratchpad/slab1_20260918/bake.sh` (`bake.sh <exe> <out-root> [extra switches]`), so the before and
after bakes cannot differ in anything but the exe and the extra switches. Every path in it is absolute
(root MISTAKES 2026-09-18 02:2x: a relative `--out-dir` resolves against `release/`).

### 1.2 The `--msn-cache` folder, verified before the bake

`E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals/Textures/Terrain/Commonwealth`: **1,781 files, all 1,781 of
them `*_msn.DDS`, every one exactly 16,777,364 B** (`ls -l | awk '{print $5}' | sort | uniq -c` → one line,
`1781 16777364`). The one tile this chunk needs, `Commonwealth.4.4.-12_msn.DDS`, is there (16,777,364 B, 02:37).
That is the same 1,781 the HANDOFF hotfix 5 entry records (his re-upscale of all 2,304 is still unfinished), so the
cache is in the state chunkD3 was baked against. The before bake's own census line reads
**`msnCacheDir <that folder> msnCacheHit 1 msnCacheMiss 0 msnCacheRenorm 0`** — one hit, no miss.

### 1.3 The before bake, and the census against chunkD3

`scratchpad/slab1_20260918/before/`, exe `release/NifSkope.before_slab1.exe` (the launch bytes), `rc=0`, **1,017 s**.
Process check immediately before the launch: no `Fallout4`, no `NifSkope` (rc=1).

| census field | chunkD3 (05:08) | before/ (06:25) |
|---|---|---|
| `objAoPlacements` | 8693 | **8693** |
| `objAoMeshes` | 908 | **908** |
| `objAoTriangles` | 96009 | **96009** |
| `objAoSquares` | 24729 | **24729** |
| `objAoTexels` | 4162845 | **4162845** |
| `objAoMeanDark` | 88.9169 | **88.9169** |
| `objAoRefusedNoLod` / `NoLodBases` / `NoLoad` | 27425 / 2477 / 2 | **27425 / 2477 / 2** |
| `objAoStrength` / `objAoReach` | 0.500 / 1458 | **0.500 / 1458** |

Every field matches, so the before bake IS chunkD3's bake and there is no delta to explain. (It was not free to assume:
the launch exe is hotfix 7d's, one hotfix newer than chunkD3's 05:11 exe, but 7d changed a material's alpha-test ref,
not geometry, and the lattice is rasterised from geometry.)

### 1.4 The reader, and the number

`scratchpad/slab1_20260918/mask_b_mean.py`. It imports `Lodv` and `decode_bc1` from
`tests/spells/lodgen_vt_check.py` and re-implements neither. The far terrain's AO is the **mask sheet's B texel**
(role 5, RMAOS: R roughness, G metallic, **B sky AO**, A cover). In this bake that sheet is `dxgi 71` = BC1, so B
carries **5 bits** — a single texel is one of 32 steps, which is why the values are 16, 24, 32 ... 255. A mean over
thousands of texels is unaffected; a single texel is quoted as coarse.

Georef the script asserts, and then CHECKS (§1.5): tile index `ty*tilesX + tx`, **row 0 is NORTH**
(`lodgen_vt_check.py` V20); tile (tx,ty) is cell (`west+tx*levelDim`, `north-ty*levelDim`); a stored texel (i,j) is
content texel (i-8, j-8) with centre `x = cellX*4096 + (i-8+0.5)*8`, `y = (cellY+1)*4096 - (j-8+0.5)*8`.

**Independent confirmation of the reader against the app.** Run on chunkD3's own finest sheet it reads
`values 16..255, mean 134.689` over all 4,194,304 content texels. The viewer's note line on the same file
(`scratchpad/viewfix_20260917/images/oao_full_L0.log`) reads *"terrain AO from the MASK SHEET'S B ... values 16..255,
mean 134.1"* — sampled per grid vertex, bilinear, a different instrument, agreeing to 0.6 of a byte on the mean and
exactly on the range. Two independent decoders of the same channel.

### 1.5 Where the deck actually is — and the georef control

The brief points at *"a rectangle you define IN WORLD UNITS under the deck near 24900,-41300 (read the placement list
for the overpass REFRs to bound it)"*. Two instruments were read to bound it, and they agree with each other and
**not** with that coordinate, so the coordinate is reported and measured as well as the deck.

1. **The bake's own refusal line** (`before/bake.log`, the chunk census) names the highway explicitly:
   `roadRaisedBases 30 roadRefusedRaised 70 ... roadRefusals [raised-haslod
   Landscape\Roads\HighwayOverpass\HWDoubleChunkBoth01.nif; ...]`. Seventy raised-highway placements are in this
   chunk. They are refused by the ROAD painter (`raised-haslod` = it has its own LOD mesh, so the road pass leaves it
   alone) — which is precisely why they ARE in the object height field.
2. **The lattice itself** (`probe/objh_before.bin`, the `--dump-object-ao` v1 dump: origin 64,-448, 256x256 squares,
   cell 128.0, 24,729 occupied — equal to the census `objAoSquares 24729`). A flat deck writes ONE value into many
   squares, so a histogram of the exact max-Z values finds it: of 24,729 occupied squares, **687 hold exactly
   2415.9** and they form a straight strip **12 squares (1,536 units) wide** running north-south the whole height of
   the chunk, `x 19584..21120, y -44544..-33536`. A second plane, **1920.0, 238 squares**, runs inside the same strip
   (the lower deck and the ramps). That is the elevated highway. The `.BTO` manifest's name index is no help here
   (it lists 64 `I` rows for 402 distinct bases), which is why the lattice, the instrument the march itself reads,
   was used instead.

**Why the deck is black and the pier is grey — measured, not guessed.** The march never samples its own square; it
samples 8 directions at 128, 192, 288, 432, 648, 972, 1458 units. The highway deck is 12 squares wide, so a texel
beneath it has all eight directions blocked at the 128-unit step and `occl` saturates. The park pier
(12 `ParkPierStr01` placements at `z 816`) is **one square wide**: its lattice column is `x 31360..31488` reading
814.0, with a regular row of 318.0 squares beside it (the pile bracing), so only the two along-pier directions are
blocked and six escape. Same law, same strength, different width. This is exactly the director's observation
(*"Terrain AO is so dark, but not the rock or the support of the highway"*) with a number on it.

**A third deck, independent of the highway, as corroboration:** the industrial catwalk grid, 19 `IndCat1WaySide02` /
`IndCat2WaySideR01` placements **all at `z 1308.0`, height 181.0**, over `x 31232..32542, y -41532..-40450`. Its
lattice plateau reads exactly **1308.0** (the deck plate; the placement z, since the plate's own triangles sit at
local z 0) and the ground under it is pinned dark. It is measured below as `a3-catwalk`.

**The georef control (`ww-control-calibration`).** The script correlates, per 128-unit square, the mean mask-B against
the lattice's max Z — two files written by two different code paths, one a texture, one a height dump, joined only by
the georef formula of §1.4. Over the 7,562 occupied squares that fall inside the sheet's 16 tiles:

    r = -0.6611   (a taller object over a square => a darker B texel: the expected sign)

and the known-answer refuter, the same correlation with the lattice shifted **one cell (4,096 units) east**:

    r = +0.1937   (6,908 squares)

The georef is not a guess: shifting it by one cell destroys the relation and flips its sign.

### 1.6 The rectangles, in world units, and the three numbers

All five printed by `python mask_b_mean.py before/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.1.lodt --lattice
probe/objh_before.bin <rects>` at 2026-09-18 06:44:40; raw output kept at
`scratchpad/slab1_20260918/before_means.txt`.

| tag | world rectangle | size | texels | **mean B (before)** | min..max | what it is |
|---|---|---|---|---|---|---|
| **(a) `a-deck`** | x 19712..20992, y -41856..-40576 | 1280 x 1280 | 25,600 | **57.316** | 49..65 | under the elevated highway; **all 100 of its lattice squares hold max Z exactly 2416.0** |
| (a2) `a2-brief` | x 24644..25156, y -41556..-41044 | 512 x 512 | 4,225 | **122.528** | 65..172 | the brief's coordinate, 24900,-41300 +/- 256: a dense DecoKit block with maple canopies, NOT the overpass (nearest deck plane is 4,100 units west) |
| (a3) `a3-catwalk` | x 31616..32640, y -41472..-40576 | 1024 x 896 | 14,336 | **74.640** | 65..123 | under the catwalk grid, every square max Z 1308..1516 |
| **(b) whole chunk** | x 16384..32768, y -49152..-32768 | 16384 x 16384 | 4,194,304 | **134.689** | 16..255 | every content texel of the finest sheet; the viewer says 134.1 (§1.4) |
| **(c) `c-open`** | x 23936..24192, y -34816..-34560 | 256 x 256 | 1,024 | **234.488** | 213..255 | open ground: chosen by a summed-area scan for the 256x256 block with the largest ring of EMPTY lattice squares round it — **1,024 units clear in every direction** (no 512x512 block in the chunk has a full 1,458-unit clear ring; that is stated rather than pretended) |
| **(d) `d-pierfoot`** | x 31232..31616, y -45568..-44160 | 384 x 1408 | 8,448 | **139.295** | 90..189 | the pier and its own shadow column: the 814.0 lattice line and the 318.0 pile squares either side. Step 5's refuter |

The brief's three numbers, in its own order: **(a) 57.316 under the deck, (b) 134.689 whole chunk, (c) 234.488 open
ground.** The deck rectangle is 2.3x darker than the chunk and 4.1x darker than open ground, and its whole 1,280-unit
width sits within 16 bytes of the BC1 floor — that is the "black under the deck" bungo asked about, as a number.

## 2. The ceiling formula — WRITTEN BEFORE THE CODE

### 2.1 What is wrong with the march as it stands

`lodgenObjectSkyVis()` (`src/lodgen.cpp:8353`) reads ONE float a 128-unit square — the highest object surface over it —
and, per direction, keeps the largest `(top - h0)/d` over the seven steps 128, 192, 288, 432, 648, 972, 1458. It then
maps that tangent to a blocked fraction of that direction's 0°..90° elevation sweep with

```
F(s) = s / (1 + s)          F(0) = 0, F(inf) = 1
```

and averages the eight directions. A max-Z lattice cannot tell *a wall whose top is at 1,000* from *a deck whose
underside is at 1,000*: both give `top - h0 = 1000`, so the deck reads as a wall standing on the ground in all eight
directions, `F ~ 0.89`, and at strength 0.5 the texel keeps 0.29 of its sky. The pier and the rocks are shaded by the
`.lodi`/`.BTO` 8-ray triangle cast instead, whose side rays escape, so they read grey while the ground reads black —
which is the picture bungo pointed at.

### 2.2 The lattice gains a second plane

Beside `maxZ` a square, `minZ` a square: the LOWEST object surface over it, written by the same two rules that write
the max (the triangle's plane height at every lattice centre it covers, and each corner's height into the square that
corner stands in). A square with no geometry stays empty and is still detected on the max plane alone.

The classification then falls out of the sample's own height `h0` (the terrain height the texel stands on, which the
march already carries):

* `minZ <= h0` — the geometry over that square reaches down to the sample's own level or below. **A WALL.** A pier, a
  building side, a rock, a tree trunk: its bottom vertices are seeded at the ground, so this is nearly everything.
* `minZ > h0` — the geometry is entirely above the sample. **A CEILING.** A deck, an overhang, a canopy whose trunk
  stands in another square.

### 2.3 The law

`u` = the 8 directions, `d` = the same 7 steps, unchanged.

```
per direction k:
    wall     = 0
    ceilOpen = +inf,  covered = true
    for d in 128, 192, 288, 432, 648, 972, 1458:          # NEAREST FIRST
        square = the lattice square at (wx + ux*d, wy + uy*d)
        if square is empty:            covered = false;  continue
        if square.minZ <= h0:                                     # A WALL
            covered = false
            if square.maxZ > h0:  wall = max( wall, (square.maxZ - h0) / d )
        else:                                                     # A CEILING
            if covered:           ceilOpen = min( ceilOpen, (square.minZ - h0) / d )

    wallBlocked = wall / (1 + wall)                               # today's expression, unchanged
    ceilBlocked = (ceilOpen < +inf) ? 1 - ceilOpen / (1 + ceilOpen) : 0
    occl       += min( 1, wallBlocked + ceilBlocked )

visObj = clamp( 1 - occl/8 * 1.6 * strength, 0, 1 )               # unchanged
```

Three things are worth naming, because each is a decision:

1. **A ceiling blocks UP TO THE ZENITH, not a band.** Under a horizontal cover that passes over the sample, every ray
   steeper than the cover's near edge hits it; only the rays that slip out under the edge see sky. So the blocked set
   is elevations `atan(ceilOpen) .. 90`, whose measure in the same `F` metric is `1 - F(ceilOpen)`. Reading it as a
   band `F(minZ/d) .. F(maxZ/d)` instead would leave the zenith counted as open, which is the one thing a ceiling
   certainly is not. `ceilOpen` is minimised over the steps, so for a flat cover reaching out to step `d_last` it is
   exactly `H / d_last` — the elevation of the cover's far edge, i.e. the highest ray that still escapes.
2. **`covered` — the contiguity test — is what keeps a distant canopy from behaving like a roof.** The ceiling term is
   only accumulated while every step from the NEAREST outward has been a ceiling square. A tree 1,000 units away does
   not pass over the sample, so the first step is empty, `covered` goes false, and it contributes nothing to the
   ceiling term (its trunk square is a wall and shades as before). Without this test a canopy at `d = 1000` would be
   read as a roof at 1,000 units and would darken the ground MORE than today. The cost is stated in section 11: a
   cover whose first sampled step is empty but which does pass overhead between 0 and 128 units is counted as no cover
   at all, so the term is conservative toward BRIGHT at a deck's outer edge.
3. **`min(1, wallBlocked + ceilBlocked)`, not `max`.** The two blocked sets are `0 .. F(wall)` from below and
   `F(ceilOpen) .. 1` from above; their union measure is the sum, capped at 1 when they meet. A wall under a ceiling
   therefore still blocks everything, as it should.

### 2.4 Identity on a wall, bit for bit

When every occupied square in a direction is a wall, `ceilOpen` stays `+inf`, `ceilBlocked` is exactly `0.0f`, the set
of squares feeding `wall` is exactly the set that fed `maxSlope`, the divisions are the same divisions in the same
order, so `wall == maxSlope` bit for bit; `wall/(1+wall)` is the same expression as before; `x + 0.0f == x` and
`qMin(1.0f, x) == x` for `0 <= x < 1`. **A region with no ceiling square produces the same AO byte as today, by
construction rather than by tolerance** — the same standard section 2.5h already holds the switch's OFF state to.

### 2.5 The three canonical values, at the shipped strength 0.5

Write `H` for the height of the cover's underside above the sample, `s` for strength.

| case | new | today | note |
|---|---|---|---|
| **(A) directly under an infinite flat slab** | `1 - 1.6*s*1458/(1458+H)` -> **0.5255** at H = 1000, **0.2965** at H = 200, **0.2** as H -> 0 | `1 - 1.6*s*H/(128+H)` -> 0.2908 at H = 1000, 0.5122 at H = 200 | never 0 at s = 0.5: the floor is 0.2, because 1,458 units out the cover is only `atan(H/1458)` above the horizon and the march can see sky under it |
| **(B) beside a solid wall H tall, one direction, nearest step** | `1 - 0.1*H/(128+H)` -> **0.9204** at H = 500 | identical, bit for bit | section 2.4 |
| **(C) under a deck EDGE** (4 directions covered to the reach, 4 empty) | `1 - 0.8*(4/8)*1458/(1458+H)` -> **0.7627** at H = 1000 | 0.2908, the same as the centre | strictly between the centre (0.5255) and the beside-a-wall reading (0.9113 at H = 1000), and strictly below open ground (1.0) |

**The one number in this table that must be said out loud.** The two laws cross at
`H = sqrt(128 * 1458) = 432.0 units` exactly. Above 432 the slab lattice is BRIGHTER than today (the overpass case, and
the whole point of the lane: a deck at H = 1000 goes 0.2908 -> 0.5255, +81 %). **Below 432 it is DARKER** — a cover 200
units up goes 0.5122 -> 0.2965, because a ceiling that low really does shut out almost all the sky and the max-Z
reading was letting it off. That is a consequence of the law, not a tuning choice, and the brief's pre-registered G3
bar ("under the slab centre > 0.3" on a slab **200 units up**) sits 0.0035 on the wrong side of it. Section 6 reports
that gate as REFUSED WITH THE NUMBER and pins the harness on the law's actual values instead, at both heights, with the
today-versus-new comparison at each. (`ww-spec-gate-audit`: the bar assumed the fix could only brighten.)

### 2.6 What is NOT changed

`strength` semantics, the eight directions, the seven steps, the 1,458-unit reach, `F`, the `/8 * 1.6` scaling, the
multiplication into the terrain term, which sheet channel it lands in, the `.lodl` refusal, the OFF state. The terrain
march, the roads, the erosion and the hotfix-7 vertex cast are untouched. `--terrain-object-ao` still ships OFF.

### 2.7 The way back, and why it is a switch and not a number

CONSTITUTION 7 asks a visible behavioural change to carry a way back that is exact at its off value. The brief allows
one sub-toggle inside `--terrain-object-ao` if the ceiling needs a dial. It is not a dial — there is nothing to tune —
but the OLD reading is wanted for exactly two reasons, so it gets a boolean:
**`--terrain-object-ao-slab 0|1`, default 1 (the new law); `--no-terrain-object-ao-slab` = 0 = the max-Z
block-everything reading, byte-exact.** The two reasons: it is what makes "red on the old formula" runnable on ONE exe
(section 6 (d)), and it is the exact way back to the bytes of the sheets bungo has already looked at (chunkD3,
hotfix 7c). Divergence row in section 11.

## 3. The code (step 2 and step 3): functions touched, lines

Three files, no format change to any shipped file, and the object term only. Line numbers are in the tree as it
stands at 2026-09-18 07:08.

### 3.1 `src/lodgen.h`

| line | what |
|---|---|
| 1125 | `bool terrainObjectAoSlab = true;` in `LodgenCoverOptions`, beside `terrainObjectAoStrength`, with the doc block that says it is a sub-toggle and not a dial: default = the new law, `0` = the max-Z reading bit for bit |
| `LodgenObjectAoCensus` | `int slabSquares = 0;` — the new census word's field |
| `dumpObjectAoPath` | comment extended: the dump now appends a SECOND `gw*gh` float32 plane, it is a DEBUG FILE and not a shipped format, and a reader tells the versions apart by file length (`24 + n*4` against `24 + n*8`) |

### 3.2 `src/lodgen.cpp` — `class LodgenObjectHeightField`

| line | what |
|---|---|
| 8157 | `static constexpr float NONE_LOW = 1.0e30f;` — the MIN plane's sentinel, at the other end. It is **never** the emptiness test: a square is empty when its MAX is `< SENTINEL_TEST`, on the max plane alone, exactly as before |
| 8178 | `gridMin.assign( size_t( gw ) * size_t( gh ), NONE_LOW );` beside the existing `grid.assign( …, NONE )` |
| 8207 | `countSlabSquares( world, cx0 - margin, cy0 - margin, cx1 + margin, cy1 + margin );` — the last statement of `gather()`, over the same rectangle including the same margin 2 |
| 8253 | `void spanAt( float wx, float wy, float & lo, float & hi ) const` — `topAt`'s two-plane twin: nearest square, never interpolated, `hi < SENTINEL_TEST` means empty. `topAt` is left in place and untouched |
| 8272 | `void spanInto( int gx, int gy, float z )` — replaces `maxInto`, writing MAX and MIN in the same call so a square can never carry a min from a surface whose max it never saw. All four write sites converted: the three vertex seeds and the barycentric plane scan |
| 8304 | `void countSlabSquares( const EsmWorld &, int cx0, int cy0, int cx1, int cy1 )` — the census word, counted once against the heightfield itself and NOT kept as a third plane (the terrain arrives one cell at a time and is thrown away) |
| 8426 | `std::vector<float> gridMin;` beside `std::vector<float> grid;`, both commented with which end they hold |
| 8441 | the class doc block gains THE SLAB LATTICE paragraph: the wall/ceiling rule, the measured 57.3 of 255 under the deck, and the `sqrt( 128 * 1458 ) = 432` crossover with the sentence that it is the law and not a tuning choice |

`dump()` appends `gridMin` after `grid` (the v2 debug dump). `LodgenObjectAoCensus::add()` sums `slabSquares`.

### 3.3 `src/lodgen.cpp` — the march

`static float lodgenObjectSkyVis(…)` at **8460** gains one parameter, `bool slab`, and the body of section 2.3:
`wall`, `ceilOpen`, `haveCeil`, `covered`; an empty square clears `covered` and continues; `if ( !slab || lo <= h0 )`
takes the WALL branch, which is the whole of the old loop; `else if ( covered )` collects the lowest escape; and the
accumulator is

```
const float wallBlocked = wall / ( 1.0f + wall );
if ( !haveCeil ) occl += wallBlocked;                       // the old float, unchanged
else             occl += qMin( 1.0f, wallBlocked + ( 1.0f - ceilOpen / ( 1.0f + ceilOpen ) ) );
```

The `!haveCeil` arm adds **no** term and applies **no** `qMin`, which is why a direction with no ceiling square
produces the identical float it produced before, and why `--no-terrain-object-ao-slab` is the old bytes rather than an
approximation of them (measured in section 4.2).

### 3.4 `src/lodgen.cpp` — the use sites and the census

| line | what |
|---|---|
| 10014 | call site 1, the chunk composite (`_data.DDS`, the stock engine's own mask), passes `coverOpts.terrainObjectAoSlab` |
| 11239 | call site 2, the pyramid tile (`.lodt` role-5 mask sheet, every level through the existing box filter), same |
| 9893–9897 | the chunk census `fprintf` gains `objAoSlab %d` and `objAoSlabSquares %d` |
| 12152–12158 | the `.lodb` report clause gains `objAoSlab` and `objAoSlabSquares`, the second with the comment that a zero is WRITTEN and not omitted |

### 3.5 `src/nifcli.cpp`

| line | what |
|---|---|
| 7885 | `--terrain-object-ao-slab 0|1` (`next().toInt() != 0`) |
| 7887 | `--no-terrain-object-ao-slab` |
| help | `[--terrain-object-ao-slab 0|1, default 1; 0 = the old max-Z reading, where a deck blocks from the ground up]` |

Nothing else was touched. `src/nativeemit.cpp` and `src/lodgenao.h` (hotfix 7's vertex-AO cast), the terrain march,
the roads and the erosion are untouched, and no GUI row was added in `src/lodgenmanager.cpp` (divergence row in
section 11).

## 4. OFF identity, and the way back (step 4 — gate G1)

Four bakes, all of chunk 4.4.-12 with chunkD3's recipe verbatim (`bake.sh`, section 1.1), run one at a time by
`run_bakes.sh` with the game and NifSkope verified down before each:

| root | exe | switches added |
|---|---|---|
| `off_rung/` | `release/NifSkope.before_slab1.exe` (the rung) | none |
| `off_new/` | `release/NifSkope.exe` (mine) | none |
| `after/` | mine | `--terrain-object-ao` |
| `oldlaw/` | mine | `--terrain-object-ao --no-terrain-object-ao-slab --dump-object-ao …/probe/objh_after.bin` |

### 4.1 G1: OFF is identical — `off_rung` against `off_new`, every file

12 files under the output root, `cmp` on each:

| file | verdict |
|---|---|
| `vt/Commonwealth.4.4.-12.BTO` | **identical** |
| `vt/Commonwealth.4.4.-12.BTO.manifest.txt` | **identical** |
| `vt/Commonwealth.4.4.-12.BTR` | **identical** |
| `vt/FO4CSLOD/Commonwealth/Commonwealth.VT.1.lodt` | **identical** |
| `vt/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt` | **identical** |
| `vt/FO4CSLOD/Commonwealth/Commonwealth.VT.4.lodt` | **identical** |
| `vt/FO4CSLOD/Commonwealth/Commonwealth.VT.lodm` | **identical** |
| `vt/tex/Commonwealth.4.4.-12.DDS` | **identical** |
| `vt/tex/Commonwealth.4.4.-12_data.DDS` | **identical** |
| `vt/tex/Commonwealth.4.4.-12_msn.DDS` | **identical** |
| `bake.log` | differs — **my own file**, not an output: `bake.sh` redirects stderr into it, so it holds the exe's size and build time and the stage timings |
| `vt/Commonwealth.lodb` | differs — the bake RECORD, and it is supposed to: it stores the argument vector verbatim, the exe identity, the stage times, the peak working set, and the census line, which now carries `objAoSlab 1 … objAoSlabSquares 0` |

**10 of 10 output files byte-identical. The two that differ are the log I create and the record whose job is to record
the exe and the arguments; no sheet hash moves in it.** `objAoSlabSquares 0` in the OFF record is the census word
behaving: with `--terrain-object-ao` absent the field is never gathered.

### 4.2 The way back, by bytes — `before` (the RUNG, AO on) against `oldlaw` (my exe, AO on, slab 0)

The same 12 files, the same `cmp`:

**All 10 output files byte-identical, including all three `.lodt` sheets and both `.DDS` masks.** Only `bake.log` and
`Commonwealth.lodb` differ, for the same two reasons as above plus the two extra switches the record stores verbatim.

That is CONSTITUTION 7's way back demonstrated by bytes and not by argument: `--no-terrain-object-ao-slab` reproduces
the exact sheets bungo has already looked at (chunkD3, hotfix 7c), on the new exe.

### 4.3 The flipped-byte refuter — the comparison can go red

A `cmp` that cannot fail is not a gate. One bit of one byte of a copy of `Commonwealth.VT.1.lodt`, at offset 4256
(inside the first sheet's payload), `0xC9 -> 0xC8`, file length unchanged at 8,391,136 B:

```
cmp rc = 1  -> RED, the comparison detects one bit
```

### 4.4 The bake WITH the switch: every file that differs, and why

`off_new` (switch absent) against `after` (switch present) — this is the "must differ ONLY in the mask sheets" check:

| file | differs | why |
|---|---|---|
| `Commonwealth.VT.1.lodt` | yes | the finest mask sheet, role 5 — B is the sky AO. This is the intended change |
| `Commonwealth.VT.2.lodt`, `Commonwealth.VT.4.lodt` | yes | the coarser pyramid levels of the same mask, through the existing box filter |
| `Commonwealth.VT.lodm` | **no** | the pyramid index is geometry and layout, not content: it carries no per-sheet hash, so it does not move when a sheet's texels do (measured, `cmp` says identical) |
| `Commonwealth.4.4.-12_data.DDS` | yes | **the second call site**: the stock engine's own chunk-level mask, which `lodgen.cpp` packs as `0xFF000000 | ( ao8 << 16 ) | ( wet8 << 8 ) | sho8` — so the sky AO is its **R** byte |
| `Commonwealth.lodb`, `bake.log` | yes | record and log, as in 4.1 |
| `Commonwealth.4.4.-12.BTO`, `.BTO.manifest.txt`, `.BTR`, `.DDS` (diffuse), `_msn.DDS` | **no** | the object bake, the terrain mesh, the diffuse and the normal sheet are untouched |

So of 12 files, 5 move: the three mask sheets, the chunk mask DDS, and the record; 7 do not.

**Which channel moved in `_data.DDS`** (`dds_channels.py`, mip 0 of both bakes decoded to RGB — a decoded channel
value, never a file byte presented as a pixel):

| channel | what it carries | differs in | whole-texture mean before → after |
|---|---|---|---|
| **R** | **sky AO** | 954,658 of 1,048,576 texels | **134.532 → 158.247 (+23.72)** |
| G | wetness | 363,903 texels | 23.927 → 23.909 (−0.018) |
| B | shore | 173,776 texels | 123.009 → 122.986 (−0.022) |

R moved by +23.72 and the other two by two hundredths of a step. The two channels that "differ" in a texel count but
not in a mean are BC1's endpoint fitting: a 4×4 block whose R changed gets new endpoints, and G and B are re-quantised
around them. **That also sets the noise floor for every mask measurement in section 5: on a texel whose own value did
not change, BC1 re-encoding can still move it by a step or two.** The floor is measured, not assumed — see 5.4.

A correction to my own earlier note in this report's working notes: the sky AO is **B** in the `.lodt` role-5 mask
sheet and **R** in the chunk `_data.DDS`. Two files, two layouts, one quantity.

## 5. The witness (step 5 — gate G2)

`after/` is the same command as `off_new/` with `--terrain-object-ao` added, and nothing else. Every number below is
the mean of the role-5 mask sheet's **B** channel, mip 0, over a rectangle given in WORLD units, read by
`mask_b_mean.py` out of `Commonwealth.VT.1.lodt` (8 world units a texel). B is BC1, so it carries 5 bits: a single
texel is one of 32 steps and only a mean is worth quoting.

### 5.1 The rectangles and the three numbers

| tag | world rectangle | texels | before | after | move |
|---|---|---|---|---|---|
| **a-deck** — the elevated highway, 100 lattice squares all holding max Z 2415.9 | x 19712..20992, y −41856..−40576 | 25,600 | **57.316** | **83.948** | **+26.632 (+46.5 %)** |
| a2-brief — the coordinate the brief named | x 24644..25156, y −41556..−41044 | 4,225 | 122.528 | 158.924 | +36.396 |
| a3-catwalk | x 31616..32640, y −41472..−40576 | 14,336 | 74.640 | **72.764** | **−1.876 (darker)** |
| **b** — the whole chunk, every content texel | cells x 4..7, y −12..−9 | 4,194,304 | **134.689** (16..255) | **158.619** (32..255) | **+23.930** |
| **c-open** — the open-ground control, no object over any of its squares | x 23936..24192, y −34816..−34560 | 1,024 | **234.488** | **234.488** | **0.000** |
| d-pierfoot | x 31232..31616, y −45568..−44160 | 8,448 | 139.295 | **163.401** | **+24.106** |
| e-wall — the wall refuter, picked from geometry (5.5) | x 24192..24704, y −34944..−34432 | 4,096 | 232.999 | **233.059** | **+0.060** |

G2's three clauses: **(a) the under-deck mean RISES, by 26.6 of 255 (+46.5 %). (c) the open-ground control moves
0.000, exactly. (b) the whole-chunk mean is 134.689 → 158.619.** The fourth clause, the pier foot, FAILED AS WORDED
and is reported as such in 5.3 with its number.

Census, from the bakes' own stderr (`objAo*`):

| bake | `objAoSlab` | `objAoSquares` | `objAoSlabSquares` | `objAoTexels` | `objAoMeanDark` |
|---|---|---|---|---|---|
| `before` (the rung) | *(word does not exist)* | 24729 | *(word does not exist)* | 4162845 | 88.9169 |
| `off_new` (switch absent) | 1 | 0 | **0** | 0 | 0.0000 |
| `after` | 1 | 24729 | **13678** | 3807692 | 71.6092 |
| `oldlaw` (slab 0) | 0 | 24729 | 13678 | 4162845 | 88.9169 |

`oldlaw`'s two numbers are the rung's two numbers to the digit, which is the census half of the way back. Fewer texels
are darkened at all (4,162,845 → 3,807,692) and the ones that are, are darkened less (88.92 → 71.61).

### 5.2 The instrument that explains the moves: clearance, from two files

`--dump-land` (a mode that already existed in `nifcli.cpp`) writes every LAND cell of the worldspace as qint16 of
height/8 — the ESM terrain, quantised to 8 units, 76 MB, `probe/land.bin`. Together with the v2 lattice dump's MIN
plane that gives **clearance = minZ − terrain** per 128-unit square, which is the number the march's `lo <= h0` test
turns on. `clearance.py` prints it:

| rectangle | occupied squares | minZ range | maxZ range | terrain | clearance (mean) | WALL (clr ≤ 0) | CEILING clr ≤ 432 | CEILING clr > 432 |
|---|---|---|---|---|---|---|---|---|
| a-deck | 100 | −261.8 .. 1894.0 | 2415.9 (all) | 672 .. 804 | **+694.2** | 7 | 19 | **74** |
| a2-brief | 11 | 592.2 .. 1926.2 | 700.4 .. 2105.7 | 678 .. 780 | +449.6 | 5 | 1 | 5 |
| a3-catwalk | 56 | −85.7 .. 1190.3 | 1307.9 .. 1516.3 | 650 .. 804 | **+402.3** | 2 | 14 | 40 |
| d-pierfoot | 19 | 318.2 .. 814.0 | 318.2 .. 1262.3 | 536 .. 752 | **−52.2** | 10 | 9 | **0** |
| whole chunk | 7562 | — | — | — | — | 2878 (38.1 %) | 1791 (23.7 %) | 2893 (38.3 %) |

**The census word, verified against this instrument and not against itself.** Counting the dump's own two planes plus
`land.bin` in Python, over the whole gathered rectangle (256×256 squares = the chunk plus the margin 2 cells), with the
same bar of one cell: **occupied 24,729 and clearance > 128 in 13,678** — the exe's `objAoSquares 24729` and
`objAoSlabSquares 13678` to the unit, from a reader that shares no code with the counter. That is G4's "moves and is
right": 0 with the switch off, 13,678 with it on, and independently reproduced.

### 5.3 The two rectangles that did not do what the brief expected — with numbers

**a3-catwalk, −1.876 (darker).** Its mean clearance is **402.3**, and the two laws cross at
`sqrt( 128 × 1458 ) = 432` (section 2.5, pre-registered before the code). A cover that low really does shut the sky
out and the max-Z reading was letting it off; below the crossover the new law is DARKER, by the law. 14 of its 56
squares are low ceilings, 2 are walls, and the mean sits 30 units under the crossover. This rectangle is the law's
other side, visible, and it is not a defect.

**d-pierfoot, +24.106 — the pre-registered clause "does not brighten by more than 2" FAILS AS WORDED.** The number is
+24.106 and it is recorded as a failure of the clause. What it is not is a failure of the law, and the reason the
clause could not hold is that the rectangle is not a pier foot in the sense the brief meant:

* the brief's assumption was "a model whose triangles reach the ground (a pier, a wall, a rock) has min Z at ground".
  For `ParkPierStr01_LOD_0.nif` it does not hold: **0 of its 19 occupied squares is a high-clearance ceiling and 10
  are walls, but the squares that carry the pier's deck hold min 716.4 .. 814.0 against terrain 536 .. 752** — the
  LOD model is a deck plate on stubs, and where the piles should be the lattice square is simply empty.
* the rectangle sits UNDER and beside that deck, so the squares its march READS are deck plates, not piles. It was
  never a wall test. I chose it from a placement name instead of from geometry, which is the mistake (MISTAKES entry
  in section 12), and the replacement refuter in 5.5 is chosen from geometry before any mask is opened.

### 5.4 An independent reimplementation of both laws agrees with the shipped exe to 0.9 %

`march.py` writes both marches a second time, in Python, from the C++ — over the lattice the exe itself dumped, with
h0 bilinear from `land.bin`. It computes the mean object sky-visibility of each rectangle under each law. It does not
compute the mask (the exe did that); it predicts the DIRECTION and the SIZE of each move from geometry alone:

| tag | predicted old vis | predicted new vis | predicted ratio | measured B ratio | difference |
|---|---|---|---|---|---|
| a-deck | 0.2566 | 0.3731 | 1.4539 | 1.4647 | −0.73 % |
| a2-brief | 0.5418 | 0.6995 | 1.2911 | 1.2971 | −0.46 % |
| a3-catwalk | 0.3372 | 0.3302 | **0.9792** | **0.9749** | +0.44 % |
| c-open | 1.0000 | 1.0000 | 1.0000 | 1.0000 | 0 |
| d-pierfoot | 0.6473 | 0.7538 | 1.1646 | 1.1730 | −0.72 % |
| whole chunk | 0.6132 | 0.7157 | 1.1671 | 1.1777 | −0.90 % |

Every rectangle's sign and size is predicted, including the catwalk's darkening, from the lattice and the ESM heights
with no knowledge of the sheets. The residual under 1 % is what a 5-bit B channel, the terrain term multiplying into
the same channel and the 8-unit terrain quantisation cost.

### 5.5 The refuter, re-cut: the ceiling did not become "ignore objects"

G2's intent for the pier foot was: ground beside an occluder that REACHES the ground must not brighten. Two
measurements answer it, and both are chosen from the lattice and `land.bin` before a mask is opened
(`pick_wall.py`, `refuter_pop.py`).

**(i) One rectangle, branch-identical by construction.** `pick_wall.py` scans every 512×512-unit block of the chunk
for one where (1) every square of the block is EMPTY, so its texels are ground, and (2) every square the march
actually visits — the union over a 32-unit sample grid of 8 directions × 7 steps, not a bounding ring — is a WALL
under the march's own test `min <= h0`. Under those two conditions the new law is ALGEBRAICALLY the old one, so the
answer is known before the file is read. The darkest such block in the chunk:

> **e-wall, x 24192..24704, y −34944..−34432**, terrain 302.2, 29 visited squares all walls, steepest wall tangent
> 0.514. **B 232.999 → 233.059, move +0.060.**

Its honest limitation, stated rather than hidden: a steepest tangent of 0.514 is light shading (B 233 of 255 against
234.5 for fully open ground), and 0.514 is the MAXIMUM over every all-wall block in the chunk — there is no heavily
shaded all-wall neighbourhood here to test, because on sloping ground a building standing uphill has its lowest
surface above a downhill sample's own terrain and is therefore, correctly, a ceiling to it.

**(ii) The whole chunk as a population, which is the stronger test.** `refuter_pop.py` classifies all 16,384 squares
of the chunk by what the march reads around them (conservatively: the union over 16 sample positions per square, and
h0 = the LOWEST of the square's four LAND nodes, so "wall" means wall for every texel in it), then compares that
square's mask-B mean between the two bakes:

| group | squares | mean move | largest \|move\| |
|---|---|---|---|
| NOTHING VISIBLE — all 56 samples empty, so the object term is provably identical | 30 | +0.387 | 2.488 |
| **WALL-ONLY — the ceiling branch is never entered, so the new law is the old law by algebra** | **131** | **+0.153** | **1.559** |
| CEILING-FED — at least one visited square is a ceiling | 16,223 | **+24.166** | 173.406 |

The first group is the **noise floor, measured**: those texels cannot have changed in the object term, and they still
move by up to 2.488, because B is BC1 and a 4×4 block whose neighbours changed gets new endpoints (same effect as
section 4.4's G and B columns). **The WALL-ONLY group moves LESS than the provably-unchanged group** — mean +0.153
against +0.387, worst 1.559 against 2.488 — so nothing leaked into the wall branch, and the signal in the
CEILING-FED group is 158× the floor. The pre-registered "not more than 2" bar is, as it turns out, the compression
noise floor itself; that the wall population sits under it is the result.

Only 161 of 16,384 squares are wall-only or empty, which is worth saying plainly: with a 1,458-unit reach almost
every square of this chunk sees SOME ceiling, and the commonest ceiling is not a deck but a **tree canopy**, whose
lattice square holds a min well above the ground. So the law brightens the ground under canopies too, at
`--terrain-object-ao-strength 0.5`, and by the same formula. That is a consequence bungo may want to look at in a
picture rather than a number; divergence row in section 11.

### 5.6 The pictures (G6)

`render.sh` is `viewfix_20260917/render_slots_e.sh` with three changes and nothing else: the output root is this
lane's `images/`, only level 0 is rendered, and `WW_LODL_AO` / `WW_RENDER_FLAT` come from the environment. The other
lane's script is not edited. The viewer reads the SAME `.lodi` / `.lodl` / `.lodo` in both arms — only
`WW_LODL_SHEETS` changes, `before/vt/FO4CSLOD/Commonwealth` against `after/...` — so anything visible in a pair is
the mask sheet and nothing else. One NifSkope at a time, `--port 12077`, `WW_WINDOW_AT=1960,40`, game verified down.

**The viewer's own note line, quoted for both bakes** (it is the viewer's independent read of the same channel, at
vertices rather than texels):

* BEFORE: `WW_LODL_AO: terrain AO from the MASK SHEET'S B (the texture), 512 texels a cell, bilinear a vertex; 16 tiles read, 257 vertices without a tile (drawn open); values 16..255, mean 134.1`
* AFTER: the same line, `values 33..255, mean 158.8`

134.1 → 158.8 at vertices against 134.689 → 158.619 at texels (5.1): two readers, one quantity. The floor rising
from 16 to 33 is the deck's own texels leaving the floor.

| page | framing | pixels that differ | mean move where it moved |
|---|---|---|---|
| `images/page_deck_flat.png` | deck, centre 20352,−41216,700 ortho 2600 view 8, FLAT | **34.96 %** | **35.2** of 255 |
| `images/page_deck_lit.png` | the same camera, LIT (objects and diffuse drawn) | 37.12 % | 15.6 |
| `images/page_close.png` | close, centre 24900,−41300,450 ortho 2600 view 8, FLAT — the brief's coordinate | 25.55 % | 21.4 |
| `images/page_full.png` | full, centre 24576,−40960,0 ortho 8192 view 1, FLAT — the whole baked region | 67.45 % | 28.3 |
| `images/page_texels_deck.png` | the TEXEL page: mask sheet B, 32 × 32 texels, world x 20228..20476, y −41340..−41092, ×16, grid drawn | — | crop mean **57.00 → 74.38** |

Each page is three panels — BEFORE, AFTER, and `|AFTER − BEFORE|` × 4 — same framing, same camera arguments, with
the percentage measured pixel by pixel and printed in the panel label, not asserted in prose.

**The `full` framing's arguments are RECONSTRUCTED, and the page says so.** The brief pointed at
`viewfix_20260917/images/*.log` for the hotfix 7c camera; those logs record the region, the sheets and the AO note
line but **not** `WW_RENDER_CENTER` / `WW_RENDER_ORTHO` / `WW_RENDER_VIEW`. I derived the framing from the region
itself — centre of x 16384..32768, y −49152..−32768, half-extent 8192 — and printed my arguments in the page caption
rather than claim they are the same ones.

**What the texel page shows, and it is the clearest single fact in this report:** over those 1,024 texels under the
deck, BEFORE is `min 57, max 57` — **every texel pinned at exactly the same value**, which is what "a deck reads as a
solid block" looks like in the sheet — and AFTER is `min 57, max 90` with visible structure, the deck's own shadow
kept and the sides opened. The crop's mean, 57.00 → 74.38, is the crop's; **the verdict number is the whole sheet's,
134.689 → 158.619** (skill `ww-texel-picture` section 7: print both, say which one gates).

The old artefact under the new law is the third reading the skill asks for; it is the file `oldlaw/`, byte-identical
to BEFORE on all ten output files (4.2), so the page names it instead of drawing an identical third panel.

## 6. The gate (step 6 — gates G3 and G1's floor)

Two new files, both under `tests/spells/`, and one new in-process control inside `src/lodgen.cpp`. Nothing existing was
edited.

| file | bytes | what |
|---|---|---|
| `tests/spells/lodgen_slab.sh` | 15,720 (LF) | the spell: four bakes of chunk 4.4.-12 into a `mktemp -d`, then 16 checks |
| `tests/spells/lodgen_slab_mask.py` | 9,177 (LF) | the spell's mask reader — `mask_b_mean.py` promoted out of the lane folder, with its lane wording generalised and nothing else changed. It imports `Lodv` and `decode_bc1` from `tests/spells/lodgen_vt_check.py` (skill `ww-one-reader-per-format`), so the container and the BC1 decoder still exist once |
| `src/lodgen.cpp` `lodgenObjectSlabSelfTest()` | — | the law on three synthetic fields, read through the SHIPPED `lodgenObjectSkyVis`, armed by `WW_OBJAO_SLAB_TEST` |

### 6.1 The run — 2026-09-18 07:37, exe 07:31:05, 16 checks, 0 failures, RESULT PASS

```
== preflight ==
       exe: 2026-09-18 07:31:05
  ok   the exe is newer than every source this answer depends on
  ok   the corpus is the shipped Commonwealth
== the bakes ==
       off: rc 0, 13s     off_noslab: rc 0, 12s     on: rc 0, 14s     oldlaw: rc 0, 15s
== (a) the sub-toggle is inert while the master is off ==
       10 of 10 output files byte-identical
  ok   with --terrain-object-ao absent, --no-terrain-object-ao-slab moves not one byte of any output file
  ok   REFUTER the same comparison goes RED on one flipped bit at offset 4256
== (b) the census word ==
       off:    objAoSlab 1 objAoSquares 0     objAoSlabSquares 0
       on:     objAoSlab 1 objAoSquares 24729 objAoSlabSquares 13678
       oldlaw: objAoSlab 0 objAoSquares 24729 objAoSlabSquares 13678
  ok   with the term off the census WRITES objAoSlabSquares 0 rather than omitting the field
  ok   the census word MOVES with the term: 24729 occupied squares, 13678 of them slabs
  ok   objAoSlab follows the switch while objAoSlabSquares stays a property of the lattice
  ok   the bake RECORD carries objAoSlabSquares, so a .lodb says which law wrote the sheet
== (c) the mask sheet ==
       WHOLE:  134.689 -> 158.619  (move +23.930)
       a-deck:  57.316 ->  83.948  (move +26.632)
       c-open: 234.488 -> 234.488  (move +0.000)
       e-wall: 232.999 -> 233.059  (move +0.060)
  ok   under-deck mask B RISES by at least 20 of 255 (measured +26.632, bar +20)
  ok   the whole chunk's mask B rises by at least 20 of 255 (measured +23.930)
  ok   the open-ground control does not move (measured 0.000, bar 1.0)
  ok   the all-wall refuter stays inside the BC1 noise floor (measured +0.060, bar 2.0)
  ok   --no-terrain-object-ao-slab reproduces the pre-lane whole-chunk mean 134.689
== (d) the law itself, on three synthetic fields, in process ==
  ok   the in-process law control passes: 12 checks, 0 failures
  ok   that control carries its own refuter: the OLD reading of the same plate is 0.290780 and the same bar REFUSES it
  ok   slab = false is the old formula to the LAST BIT on all three fields (float equality, not a tolerance)

16 checks, 0 failures
RESULT PASS
```

Four bakes, 54 seconds of bake, about a minute end to end. **The spell's four bakes run from a clean `mktemp -d` with no
`--msn-cache`**, and reproduce section 5's numbers to the digit — 134.689 → 158.619, +26.632 under the deck, 0.000 on
the control — which is section 5 re-measured by a second path.

### 6.2 G3: the law on three synthetic fields, through the shipped function

`WW_OBJAO_SLAB_TEST=1` builds three height fields by hand — no ESM, no mesh, no bake — and reads all three through
`lodgenObjectSkyVis` itself, so what is pinned is the function the sheets are written with and not a second copy of the
formula. The bake log:

```
slab: self-test the ceiling law, three synthetic fields (WW_OBJAO_SLAB_TEST), strength 0.50, sample (64,64) at h0 0
slab:   ok   PLATE H=1000, slab law = 0.525468
slab:   ok   PLATE H=1000, slab law, above the bar = 0.525468 > 0.500000
slab:   ok   PLATE H=1000, old max-Z law = 0.290780
slab:   ok   REFUTER PLATE H=1000 under the OLD law against the same bar = 0.290780, which the bar 0.500000 REFUSES
slab:   ok   PLATE H=200, slab law (the brief's > 0.3 bar is REFUSED WITH THIS NUMBER: 200 < the 432 crossover) = 0.296502
slab:   ok   PLATE H=200, old max-Z law = 0.512195
slab:   ok   PLATE H=200 is darker under the slab law (0.296502 < 0.512195), which is the crossover at 432 working
slab:   ok   WALL H=500, slab law = 0.363057
slab:   ok   WALL H=500 the two laws return the SAME float, 0.363057315, difference exactly 0
slab:   ok   EDGE H=1000, slab law = 0.703417
slab:   ok   EDGE 0.703417 is strictly between the slab centre 0.525468 and open sky 1.0
slab:   ok   slab=false reproduces the old formula exactly on all three fields
slab: self-test 12 checks, 0 failures, RESULT PASS
```

Against G3's four clauses as the brief wrote them:

| G3 clause | result |
|---|---|
| wall unchanged to 1e-6 | **exceeded: the two laws return the SAME float, 0.363057315, difference exactly 0** |
| slab centre > 0.3 | **REFUSED WITH THE NUMBER at the brief's H = 200: the law gives 0.296502.** 200 units of clearance is BELOW the crossover `sqrt( 128 × 1458 ) = 432` (pre-registered in 2.5, before the code), and below it a cover really does shut the sky out. At H = 1000 the same field reads **0.525468**, and the test pins BOTH: the H = 1000 value against a > 0.5 bar, and the H = 200 value as a number that must stay DARKER than the old law's 0.512195, which is the law's other side and must not change silently |
| edge between | **0.703417, strictly between the slab centre 0.525468 and open sky 1.0** |
| red on the old formula | **the refuter line: the same plate read with `slab = false` is 0.290780 and the > 0.5 bar refuses it.** Every bar in the test is evaluated a second time under the old reading, and the plate bars must fail there — if they passed, the bars would not discriminate between the two laws and the test says so and fails |

The last line is the strongest one and it is exact, not a tolerance: the old loop is written out a **second time** in the
test, through `topAt` — the max-plane accessor this lane did not touch — accumulating the eight directions in the same
order and the same float precision, and `off == want` is compared with `==`.

**A defect this control found in its own first run, recorded because the fix is the interesting part.** The first version
of that re-derivation used the closed form `8 * ( mx / ( 1 + mx ) )` instead of eight additions, and the test went RED:
`0.290780187 against 0.290780127`. The shipped function was right and my control was wrong — a closed form agrees to six
decimals and differs in the seventh. Two consequences, both now in the code's own comment: the exactness claim has to be
tested by the same accumulation, and the four pre-registered bars from section 2.5, which were computed at double
precision in Python, land at **0.296502** and **0.703417** in shipped float, so they are checked to 1.0e-5 and not for
equality.

### 6.3 Every check's floor, and whether the floor can fire

| check | floor | can it fire? |
|---|---|---|
| OFF is identical, 10 files | the flipped-bit copy at offset 4256 | **shown red in the same run** |
| census word written / moves | exact numbers 0, 24729, 13678 | a build that omitted the field, or gathered nothing, reads empty and the `[ = ]` refuses |
| under-deck rises ≥ 20 | measured +26.632 against bar +20 | **a build that ignored the flag writes the old sheet: move 0.000, RED.** Demonstrated by feeding the bars the `oldlaw` sheet in both slots |
| whole chunk rises ≥ 20 | measured +23.930 | same, RED under the flag-ignored sheet |
| open-ground control ≤ 1 | measured 0.000 | it is a CONTROL, so it is green under both sheets by design; its floor is the signal bar in the same run — the population that does see ceilings moves +24.166, which the same 2.0 bar refuses 12-fold |
| all-wall refuter ≤ 2 | measured +0.060, bar = the MEASURED BC1 re-encode floor (2.488 worst on provably-unchanged texels, 5.5) | same: the bar refuses the ceiling-fed population's +24.166, so it is not a bar that swallows everything |
| the way back as a number | 134.689 ± 0.002 | a law change that leaked into `slab = false` moves it by tens of steps |
| the in-process control | 12 checks, 0 failures, plus its own refuter line | the refuter is red by construction every run |

The count floor is **16**, and it is the measured green count, not a predicted one — I had written 14 into the file by
counting the blocks by eye, ran the spell, read 16, and wrote 16 back with the date and the exe timestamp beside it.
That is `ww-test-harness-add` 5c, and it cost one run to obey rather than a red gate to discover.

### 6.4 Where the in-process control had to be called from, and how that was found

It is a one-shot behind an environment variable, and I first placed the call at the top of `lodgenBakeTerrainTextures`,
the per-chunk bake entry, on the argument that every bake goes through it. **That argument was wrong and the measurement
caught it**: a `--vt` bake printed no `slab:` line at all. The diagnosis is in the same log — run with
`WW_TERRAIN_RING_TEST=1`, the terrain ring self-test printed **once**, from `lodgenWarmSharedIndices`, and not twice, so
`lodgenBakeTerrainTextures` is never entered on the pyramid path. The call now also sits at the top of
`lodgenBakeTerrainVt`, which is that path's own entry and runs single-threaded before any tile worker starts; the reason
is written into the code beside it so the next lane does not re-derive it.

Adding the control changed no output byte: the ten output files of `off_new`, `after` and `oldlaw`, re-baked on the final
exe, hash identically to the same three roots baked before the control existed (`sha1sum` of all thirty files, compared
against the lists taken at 07:23), and section 4's two `cmp` matrices — `off_rung` vs `off_new` and `before` vs `oldlaw`
— are still 10 of 10 identical on the final exe.

## 7. The neighbours (step 6 — gate G5)

Seven harnesses, **one at a time**, the game and NifSkope verified down before each, all on the final exe
(2026-09-18 07:31:05). Logs in `scratchpad/slab1_20260918/neigh/`.

| harness | verdict | time | what it covers, and why this lane could reach it |
|---|---|---|---|
| `lodgen_terrain_vt.sh` | **45 checks, 0 failures, PASS** | 61 s | the container, the filter law, the border, the validator battery, determinism. The pyramid tile baker is call site 2 of the function I changed, so this is the one that matters most |
| `lodgen_terrain.sh` | **26 checks, 0 failures, PASS** | 14 s | the chunk sheets and the terrain march — the term I did NOT touch, beside the one I did |
| `lodgen_defaults.sh` | **31 checks, 0 failures, PASS** | 802 s | every default switch's position, including that no default moved |
| `lodgen_identity.sh` | **PASS** | 2 s | `(ref, part)` identity and the ring share; untouched, run because the object walk feeds my lattice |
| `lod_generation.sh` | **128 checks, 0 failures, PASS** (floor 121) | 9 s | the LOD Generation panel. GUI, second monitor, `WW_WINDOW_AT=1960,40`, `PORT=12088`. Relevant because I deliberately did NOT add a panel row for the new sub-toggle (divergence row, section 11) |
| `lodgen_ground_cover.sh` | **29 checks, 4 failures, FAIL** | 32 s | **pre-existing, proved.** The same spell on the RUNG exe (`release/NifSkope.before_slab1.exe`, `EXE=` override) gives the SAME four failures by name — C2 ×3, C6a, C9, C16 — plus one more (`C0 the exe is newer than every source`, which the rung cannot pass because it is older than my edits). `diff` of the two logs' FAIL lines is one line, and that line is C0. Within the brief's "known 4–7 grass reds". Owner: the ground-cover lane, not this one |
| `lodgen_native_baseline.sh --check` | **25 files baked, 5 differ, FAIL** | 14 s | **pre-existing, proved the same way.** The rung produces the SAME FIVE changed files, byte for byte the same list: `chunks/Commonwealth.16.-32.16.bto`, `chunks/Commonwealth.4.-20.24.bto`, `chunks/Commonwealth.8.-24.24.bto`, `region/Commonwealth.4.-20.24.BTO` and its manifest. The baseline was recorded on the exe of **2026-09-10T03:57:46** and all five files are on the OBJECT path (`.bto`/`.BTO`), which this lane does not touch at all — the terrain mask sheets are not in the baseline's file list. The likeliest author is hotfix 7's vertex-AO cast in `src/nativeemit.cpp`, which writes exactly those files. **Owed to its owner: re-record the baseline or explain the five.** Not mine to re-record — a baseline re-recorded by the lane that trips over it is a baseline that has stopped testing anything |

G5's clause "`lodgen_terrain_vt.sh` 45/0" is met exactly. Two harnesses are red and **both were red before I started**,
each demonstrated by re-running the identical spell against the rung exe rather than by assertion.

## 8. The build

| | |
|---|---|
| exe | `release/NifSkope.exe` |
| built | **2026-09-18 07:31:05** (`date` read in the same shell) |
| size | **22,686,720 B** |
| sha1 | `a63e26b9e7e7bbf7cb4ebd2676ac552bb0fd812c` |
| make | `MAKE_RC=0`, `mingw32-make -f Makefile.Release -j8` under `MSYSTEM=UCRT64` |
| new warnings | none. The one warning the build prints, `src/lodgen.cpp:124:63: unused parameter 'grid'`, is at line 124 — nowhere near this lane's edits — and is present on the rung |
| the rung | `release/NifSkope.before_slab1.exe`, 2026-09-18 05:18:11, 22,663,680 B, sha1 `a6d213e23258a9c22d0f10277ed48401bf9fd923` — the launch exe, copied aside before the first build and never deleted |
| staleness | `find src tests res docs -newer release/NifSkope.exe` names **no file under `src/`**. The four it does name are `tests/spells/lodgen_slab.sh`, `tests/spells/lodgen_slab_mask.py`, `docs/LODGEN_CENSUS.md`, `docs/LODGEN_TERRAIN_VT.md` — a spell, its reader and two prose pages, none of which is compiled. The exe therefore carries every source change this lane made |

Three builds were made in the lane and each was preceded by its own
`tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` returning `rc=1`, as was every exe run: the first carried the
law, the second the in-process control, the third the control's own fix. `scratchpad/slab1_20260918/BUILDING` was
touched before the first.

## 9. The docs (step 7)

Three pages, every line carrying its own provenance, and no shipped-format wording touched.

### 9.1 `docs/LODGEN_TERRAIN_VT.md` — section 2.5h

2.5h is GROUND1's page for `--terrain-object-ao` and it already stated the old law as an equation. I did not rewrite
it: the old equation is still what `--no-terrain-object-ao-slab` computes, so deleting it would lose the way back's
definition. Added at the end of 2.5h, before the rule:

* the `--dump-object-ao` paragraph extended for the **v2 dump** — a second `gw*gh` float32 MIN plane, empty = `+1e30f`,
  a debug file and not a shipped format, versions told apart by length `24 + n*4` against `24 + n*8`, with the reader
  named (`tests/spells/lodgen_slab_mask.py:read_objh`);
* a new block **2.5h(2) THE SLAB LATTICE: a deck is not a block (lane SLAB1, 2026-09-18)** carrying, in this order:
  the defect with its measured number (57.3 of 255 under the deck against 234.5 on open ground, and the rectangle in
  WORLD units); the law as the pseudo-code above; why the two blocked sets are summed and not maxed; what `covered`
  is for; the crossover `sqrt( 128 * 1458 ) = 432` stated as a law and not a tuning choice, with the four synthetic
  numbers beside it; the four measured rectangles as a table; the switch, with the sentence that it is a SUB-TOGGLE
  and that `--terrain-object-ao` is unchanged and still off by default; the byte-for-byte way back; the two census
  words with a pointer to LODGEN_CENSUS 6.1; and the gate's name and check count.

### 9.2 `docs/LODGEN_CENSUS.md` — section 6.1

That page did not name `objAo*` at all: the object-AO clause has been printed since 2026-09-12 and was never in the
inventory. One new row, written to that page's own discipline (**no `src/` line numbers** — its provenance footer
forbids them, because a line number from a file another lane is editing is a rumour by the time it is read; the row
anchors on the census words themselves and on `LodgenObjectAoCensus`):

* every field of the clause in the order it is printed, `objAoReach` through the four refusal fields, with `objAoSlab`
  and `objAoSlabSquares` defined exactly (the second: occupied squares whose LOWEST object surface stands more than one
  cell — 128 units — above the ESM terrain under them);
* the rule that **a zero is WRITTEN, never omitted**, so a reader can tell "off" from "an older build";
* a **HOW THEY MOVE** clause with real numbers, which is what that page requires of every row: 0/0 with the term off;
  24,729 / 13,678 with it on; `--no-terrain-object-ao-slab` moves `objAoSlab` to 0 and LEAVES `objAoSlabSquares` at
  13,678 because it is a property of the lattice and not of the march, while `objAoTexels` goes 3,807,692 → 4,162,845
  and `objAoMeanDark` 71.6092 → 88.9169;
* the three places the fields are written (the `vt:` line, the per-chunk composite's stderr line, the `.lodb` record),
  and the gate that reads them back.

The page's artefact table gains the three bake logs the numbers were read off, with sha256 and byte counts:

| artefact | sha256 (16) | bytes | written |
|---|---|---|---|
| `scratchpad/slab1_20260918/off_new/bake.log` | `e8db0705971ac7ef` | 67,367 | 2026-09-18 07:32:13 |
| `scratchpad/slab1_20260918/after/bake.log` | `77a6372219108e82` | 129,213 | 2026-09-18 07:32:28 |
| `scratchpad/slab1_20260918/oldlaw/bake.log` | `9d969f1f5427fa98` | 129,216 | 2026-09-18 07:32:42 |

### 9.3 `lodgen --help`

`src/nifcli.cpp:6958`, beside the other `--terrain-object-ao*` lines:

```
             [--terrain-object-ao-slab 0|1, default 1; 0 = the old
              max-Z reading, where a deck blocks from the ground up]
```

### 9.4 Line endings

All three pages were LF-only before and are LF-only after, measured with Python byte counts and not with `grep`
(`feedback_crlf_python_edits`): `LODGEN_TERRAIN_VT.md` 194,846 B / 0 CRLF, `LODGEN_CENSUS.md` 48,128 B / 0 CRLF,
`nifcli.cpp` untouched this step. Every edit was applied by a Python script that asserts its anchor occurs exactly once
and rewrites with `newline=''`.

## 10. Text for the director to splice

I did not touch `WW_CHANGES.md` or `HANDOFF.md`. Both blocks below are text, to be spliced by the director.

### 10.1 WW_CHANGES paragraph

```markdown
## The slab lattice: an elevated deck no longer shades the ground as a solid block (2026-09-18, lane SLAB1)

`--terrain-object-ao` marched one number per 128-unit square — the object's MAXIMUM Z — and treated it as the top of
something standing on the ground. An overpass deck 1,000 units up therefore shaded the road under it as though the deck
reached down to the tarmac: on chunk 4.4.-12 the mask sheet's B under the elevated highway read **57.3 of 255**, against
234.5 on open ground beside it, and every one of those 1,024 texels was pinned at exactly 57 — a flat block, not a
shadow.

The lattice now carries a MINIMUM plane beside the maximum one. A square whose lowest object surface reaches down to the
sample's own ground is a WALL and blocks from the horizon up to itself, exactly as before; a square whose whole span
stands above it is a CEILING and blocks from its nearest escape UP TO THE ZENITH. The two blocked sets are summed and
capped at 1, not maxed, so a wall standing under a ceiling still blocks everything. The two readings cross at
`sqrt( 128 × 1458 ) = 432` units of clearance, the geometric mean of the march's nearest and furthest steps: above it
the new law is brighter, below it darker, because a cover that low really does shut the sky out and the old reading was
letting it off. That is the law, not a dial.

- `src/lodgen.cpp` `LodgenObjectHeightField`: a second `gridMin` plane with its own sentinel, `spanInto()` replacing
  `maxInto()` so a square can never carry a min from a surface whose max it never saw, `spanAt()` beside `topAt()`, and
  `countSlabSquares()` for the census. `lodgenObjectSkyVis()` gains `bool slab` and the wall/ceiling branch; the
  no-ceiling arm adds nothing and applies no clamp, which is why the old path is the old FLOAT and not an approximation
  of it.
- `src/nifcli.cpp`: `--terrain-object-ao-slab 0|1` (default 1) and `--no-terrain-object-ao-slab`, plus the help line. It
  is a SUB-TOGGLE: `--terrain-object-ao` itself is unchanged, still OFF by default, and the strength semantics are
  untouched.
- Census: `objAoSlab` (which law ran) and `objAoSlabSquares` (occupied squares standing more than one cell clear of the
  ground) on the `vt:` line, the chunk composite's line and the `.lodb` record. 24,729 occupied and 13,678 slabs on this
  chunk, reproduced out of the `--dump-object-ao` lattice and `--dump-land` by a reader sharing no code with the counter.
  `--dump-object-ao` is v2: the same file with a second MIN plane appended, told apart by length.
- MEASURED, chunk 4.4.-12, mask sheet B: under the deck **57.316 → 83.948 (+46.5 %)**; the whole chunk 134.689 →
  158.619; open ground with nothing over it **0.000**; a rectangle whose every marched square is a wall +0.060, inside
  the measured BC1 re-encode floor of 2.5. Fewer texels are darkened at all (4,162,845 → 3,807,692) and the ones that
  are, less (mean darkening 88.92 → 71.61). An independent Python re-implementation of both laws over the exe's own
  lattice predicts every rectangle's direction and size to better than 0.9 %.
- THE WAY BACK IS BYTES: `--no-terrain-object-ao-slab` on the new exe reproduces the pre-lane bake's ten output files
  byte for byte, `cmp` on each, including all three `.lodt` sheets and both `.DDS` masks.
- Gate `tests/spells/lodgen_slab.sh` — 16 checks, four bakes, the OFF identity with a flipped-bit refuter shown red, the
  census words, the four rectangles, and the law on three synthetic fields read through the shipped function with the
  old reading refused by the same bars (`WW_OBJAO_SLAB_TEST`). Docs: LODGEN_TERRAIN_VT 2.5h(2), LODGEN_CENSUS 6.1.
```

### 10.2 HANDOFF LANDED block (the lane's own, verbatim)

```markdown
> LANDED 2026-09-18 07:5x — an elevated deck no longer paints the ground under it as a solid block.
> release/NifSkope.exe 22,686,720 B, 07:31:05, sha1 a63e26b9. NOT COMMITTED.
>
> The object shading read one number for each 128-unit square of ground — the highest thing in it — and assumed that
> thing stood ON the ground. Under an overpass that is wrong: the deck is a thousand units up and the road under it was
> being shaded as if the deck were a wall reaching down to the tarmac. Every texel under the highway was the same value,
> 57 of 255, with no structure at all.
>
> Now each square is also asked how LOW its object reaches. If it reaches the ground it is a wall and shades exactly as
> before. If it floats above, it is a ceiling and blocks only the sky straight up, not the sides. Under the deck the
> shading goes 57.3 → 83.9 of 255 and the deck's own shadow is still there; over the whole chunk 134.7 → 158.6. Ground
> with nothing over it does not move by one step, and ground beside something that does reach the ground moves 0.06 of
> 255, which is under the compression's own noise.
>
> The switch stays OFF by default and nothing about its strength changed. `--no-terrain-object-ao-slab` gives back the
> old sheets BYTE FOR BYTE — all ten files of a chunk compared and identical — so nothing you have already looked at is
> lost.
>
> The one thing that is his to look at: with the far reach of 1,458 units almost every square of a city chunk sees SOME
> ceiling, and the commonest ceiling is not a deck but a TREE CANOPY. So the ground under trees is brighter too now, by
> the same formula. Pictures are in the lane folder.
```

## 11. Divergence rows for bungo

Written before the thing stands, each with the number that provoked it.

| # | what I chose | why | what he may rule differently |
|---|---|---|---|
| 1 | **TWO census words, where the brief said ONE** — `objAoSlab` beside `objAoSlabSquares` | a sub-toggle that moves output bytes must be identifiable FROM THE `.lodb`, or a record cannot say which law wrote the sheet it describes. `objAoSlabSquares` alone cannot: it is a property of the lattice and reads 13,678 under both laws | drop `objAoSlab` and infer the law from the switch tokens the record already stores verbatim |
| 2 | **no GUI row** in `src/lodgenmanager.cpp` for `--terrain-object-ao-slab` | `feedback_end_menu_basics_only` and the brief's "no default moves": it is a way back for a bug, not a setting a person tunes. The panel has no row for `--terrain-object-ao` itself either | add both rows if he wants the panel to reach the term at all |
| 3 | **the law also brightens ground under TREE CANOPIES** | 16,223 of 16,384 squares of this chunk are "ceiling-fed" at reach 1,458, and the commonest ceiling is a canopy, not a deck. Mean move +24.166 of 255 over that population | this is the one to look at in a picture. If canopy ground should stay dark, the fix is a minimum-clearance bar on the CEILING branch (a square is only a ceiling above N units), not a strength change — and it would want its own number from him |
| 4 | **the brief's G3 bar "slab centre > 0.3" REFUSED at H = 200** | the law gives 0.296502 there, and 200 < the 432 crossover. Refused with the number rather than met by moving the crossover | if he wants H = 200 bright, the crossover is the dial, and moving it changes the wall/ceiling balance everywhere |
| 5 | **the brief's G2 clause "the pier foot does not brighten by more than 2" FAILED AS WORDED** (+24.106) | the rectangle is not what the brief assumed: `ParkPierStr01_LOD_0.nif` is a deck plate on stubs, 0 of 19 squares a high ceiling, 10 walls, the deck squares min 716..814 over terrain 536..752. Replaced by two geometry-chosen refuters (5.5), both green | he may want the pier LOD models themselves looked at — a pier whose piles are missing from the LOD is a separate defect, and it is not this lane's |
| 6 | **`--dump-object-ao` changed shape** (a second plane appended) | it is a debug file, named as such in the header comment and in the doc, and readers tell versions apart by length. No shipped format moved | if he wants debug files frozen too, the second plane needs its own switch |
| 7 | **`--no-terrain-object-ao-slab` is the way back, not a revert** | CONSTITUTION 7 wants a way back; bytes prove this one | — |
| 8 | **two neighbour harnesses left RED**, both proved pre-existing on the rung | `lodgen_ground_cover.sh` 4 grass failures; `lodgen_native_baseline.sh` 5 `.bto`/`.BTO` files against a 2026-09-10 baseline. Re-recording a baseline in the lane that trips over it destroys the baseline | he may want the object-path baseline re-recorded by whoever owns hotfix 7's `src/nativeemit.cpp` cast |

## 12. MISTAKES entries

Three entries written by me at the top of root `MISTAKES.md` (516,385 B, LF-only), the moment each was recognised:

1. **2026-09-18 07:2x — a refuter rectangle chosen from a PLACEMENT NAME, and a control that could not fail.** The
   pier-foot rectangle was picked because the placement sounded like piles reaching the water; the geometry says deck
   plates on stubs. Plus the population classifier's first version, which sampled a square's centre against the mean
   terrain and reported provably-unchanged squares moving by 16.8.
2. **2026-09-18 07:2x — the AO channel named from memory: it is B in the sheet and R in the chunk DDS.** Found by
   decoding all three channels of both bakes rather than by remembering the sibling format.
3. **2026-09-18 07:3x — a one-shot control placed in an entry the path never enters.** `lodgenBakeTerrainTextures` is
   not on the `--vt` path; the proof was the ring self-test printing once instead of twice. Plus the same control's
   closed-form re-derivation of the old formula going red at the seventh decimal against correct code.

## 13. The finished-work skill review (CONSTITUTION 1a)

**Skills loaded and used:** `nifskope-ww-lodgen` (the bake recipe and the switch surface), `ww-texel-picture` (rule 5 —
open the picture; it caught two layout defects the script's own output could not reveal, and rule 7 — print the crop's
mean AND the gating mean), `ww-module-off-is-identical` (the OFF matrix and its flipped-byte refuter),
`ww-one-reader-per-format` (the mask reader imports `Lodv` and `decode_bc1` from `lodgen_vt_check.py` rather than
re-implementing BC1), `ww-census-contract` (a zero is WRITTEN, and every word has a HOW IT MOVES clause),
`ww-contract-provenance` (no `src/` line numbers on the census page), `ww-test-harness-add` (section 5c's measured
count floor, which I then broke and obeyed: 14 predicted, 16 measured, 16 written back), `ww-control-calibration` and
`feedback_measure_dont_eyeball` (every bar with a refuter that fires), `feedback_crlf_python_edits` (Python byte counts,
never `grep`), `feedback_read_the_clock` (`date` in the same shell for every timestamp).

**Skills wished for and NOT found:**

1. **`ww-clearance-instrument`** — how to get ground truth about what stands over a square without trusting the map
   under test: `--dump-land` (which already existed in `nifcli.cpp` and I nearly rebuilt) plus the lattice dump's MIN
   plane, the `[row=y][col=x]` SW-origin convention, `height/8` quantisation, and the 128-unit spacing that happens to
   BE the lattice pitch. This lane would have been an hour shorter with that page.
2. **`ww-population-refuter`** — classify every unit of the domain by what the code under test can SEE of it
   (conservatively: the union over many sample positions, and the worst-case reference value), then compare the
   provably-unchanged population against the changed one in the same run. It gives the noise floor as a MEASUREMENT
   rather than an assumption, which is exactly what turned the pier-foot failure into a result.
3. **`ww-self-test-call-site`** — where a one-shot environment-armed control must be called from, and how to prove the
   call site is reached before believing a silent pass (count an existing one-shot's lines on the same path).

**Skills WRITTEN this session** (`feedback_use_skills`: every repeatable procedure becomes a skill in the same
session), as proposals for the director to move into `.claude/skills/`:

* `scratchpad/slab1_20260918/skills_proposed/ww-clearance-instrument/SKILL.md` — look for the instrument inside the
  product first; the `--dump-land` layout and its three traps; the `OBJH` v1/v2 length test; the clearance join and the
  terrain-reference choice that has to be stated; "a constant copied from a caster carries its units"; and
  cross-validating the shipped code with an independent re-implementation rather than with itself.
* `scratchpad/slab1_20260918/skills_proposed/ww-population-refuter/SKILL.md` — one rectangle is an anecdote;
  conservative classification and the symptom of a broken classifier; reading the noise floor off the
  provably-unchanged group instead of assuming it; a control's floor is the signal in the same run; and the
  bit-exactness trap in a re-implementation.

Wish 3 is a MISTAKES entry rather than a page, which is the right home until it happens twice.

## DONE

```
slab LANDED 2026-09-18 08:01. The object term of --terrain-object-ao no longer reads an
elevated deck as a solid block: the lattice carries a MIN plane, a square whose whole span
stands above the sample is a CEILING and blocks from its nearest escape up to the zenith,
and the two laws cross at sqrt(128*1458) = 432 units of clearance. Under the elevated
highway on chunk 4.4.-12 the mask sheet's B goes 57.316 -> 83.948 (+46.5 %); the whole
chunk 134.689 -> 158.619; open ground with nothing over it moves 0.000; a rectangle whose
every marched square is a wall moves +0.060, inside the measured BC1 noise floor of 2.5.
--terrain-object-ao stays OFF by default and its strength semantics are unchanged;
--no-terrain-object-ao-slab reproduces the pre-lane bake BYTE FOR BYTE on all ten output
files. exe release/NifSkope.exe 2026-09-18 07:31:05, 22,686,720 B, sha1
a63e26b9e7e7bbf7cb4ebd2676ac552bb0fd812c, rung release/NifSkope.before_slab1.exe
05:18:11 22,663,680 B. NOT COMMITTED. Gate tests/spells/lodgen_slab.sh 16 checks 0
failures; neighbours lodgen_terrain_vt 45/0, lodgen_terrain 26/0, lodgen_defaults 31/0,
lodgen_identity PASS, lod_generation 128/0; two red and both proved pre-existing on the
rung (lodgen_ground_cover 4 grass, lodgen_native_baseline 5 object-path files against a
2026-09-10 baseline). Report scratchpad/slab1_20260918/lane_slab1_report.md, sections
0-13. His open NifSkope window needs a RESTART.
```

The `BUILDING` marker was touched at 06:51 before the first build and REMOVED at 08:01: it says a build is in
progress, three builds are finished, and a stale marker would stop the next lane for no reason.

### Five plain sentences for bungo

1. The ground under an overpass is no longer painted as if the deck came all the way down to it — a square of ground
   is now asked how LOW the thing over it reaches, not just how high, and a deck that floats blocks only the sky
   straight up.
2. Under the elevated highway the far-terrain shading goes from **57.3 of 255 to 83.9**, and where it used to be one
   flat value across every texel it now has the deck's own shadow in it.
3. Ground with nothing over it did not move by a single step, the switch is still OFF by default, nothing about its
   strength changed, and `--no-terrain-object-ao-slab` gives you back the exact files you have already looked at —
   byte for byte, all ten of them.
4. The one thing that is yours to rule on: almost every square of a city chunk sees SOME cover at the far reach, and
   the commonest cover is a tree canopy rather than a deck, so the ground under trees is brighter now too — the
   pictures are in the lane folder and I would rather you looked at that than at my number for it.
5. Your open NifSkope window needs a **restart** to pick up the new exe.
