# Lane PANEL1 — every bake setting the CLI has, as a row in the LOD Generation panel

Tree `E:/Projects/NifskopeWildWastelandEdition`, branch main. Exe at launch
`release/NifSkope.exe` 2026-09-12 12:58:48, 22,007,808 B, sha1
`ba7585cba389c8b38f0e07c6c11063e0cec1124c`. Rung taken 15:49:03 as
`release/NifSkope.before_panel1.exe` (same 22,007,808 B, same sha1 — verified).

bungo's words this lane answers: *"Erosion is a knob in the menu, corret?"* — it was
not — then *"What? They should all be configurable in the gen menu, anything else we're
missing in that menu?"*

## 0. Audit table

Every `--switch` the `lodgen` sub-command parses, read out of `src/nifcli.cpp`
lines 6535–7014 (the switches above and below that range belong to other
sub-commands and are not in scope). Defaults read from the declaration that
holds them, named in the "default" cell's source: `src/nifcli.cpp` locals,
`src/lodgen.h` option structs, `src/lodgen.cpp` file statics, `src/lodtfile.h`
`LodtWaterOptions`.

Classes:

* **BAKE** — a user-facing choice about the output. Belongs in the panel.
* **PATH** — where something is read from or written to. Belongs in the panel
  unless the panel derives it from the output mod folder.
* **DIAG** — a question about a file or the stack, or a fixture/stress/verify
  driver. Answers and stops; never part of a bake. Stays on the command line.

"Panel row" is the row's label today, `NEW` if this lane adds one, or
`CLI-ONLY` with the reason.

### 0.0 Rulings received while the lane ran

bungo ruled on four of the rows after the audit was read to him, at 15:56 and
16:0x. They are written here because each one CHANGES what the table below says
a switch should get, and a later reader must see the reason and not only the
outcome.

1. **No row for `--road-opacity`, and `--roads-legacy` stays on the command
   line.** His words: *"we don't use that opacity at all, we render roads at
   their full diffuse"*. A knob nobody is to turn is not a knob. `--roads-legacy`
   is the one tick that would restore the whole of the older road pipeline,
   opacity included, so it goes with it. `--road-ground-paint` KEEPS its row: it
   is a separate choice about how much of the road is painted into the ground.
2. **No rows for `--terrain-identity` / `--no-terrain-identity` and
   `--identity` / `--no-identity`.** His words, 15:56: *"Legacy terrain bakes
   stay as they were, no extra data for FO4CS to be included in them. Only the
   .lod ones have new data in them."* Both defaults are due to flip in a later
   lane (DEFAULTS1); this lane leaves them exactly where they are and gives them
   no face in the panel, so nothing can move them by accident in between.
3. **The land guide gets its whole rule list, and its three numbers get rows.**
   The selector offers every rule the command line parses — off, drag, aspect,
   aspecthex, slopewarp, flatwarp — and `--land-guide-scale`,
   `--land-guide-slope`, `--land-warp` and `--land-hex` each get a row. He picked
   `flatwarp:1.0` with `--land-warp 341` on the hex tiling as the look he wants;
   that is a DEFAULTS decision and this build still ships the guide **off**.
4. **The erosion section is wanted**, with strength, rounds, seed and the land
   detail source in it. Default stays 0 (off).
5. **`--road-ground-paint` will default to 0, in a later lane, not this one.**
   His ruling of 16:1x: the Landscape and Ground-material shapes that sit INSIDE
   the road meshes -- the verge planes, for instance `SancRoadCrvCustom02.nif`'s
   shape `Line003:3` with `CommonwealthDefault01.bgsm` -- are not part of the
   road and are excluded from the road plane. The switch that decides how much
   of them is painted into the ground therefore belongs at 0. This lane KEEPS
   the row (ruling 1 already said so) and does NOT move the default: lane
   DEFAULTS1 moves it after this one lands, so the byte-identity gate here still
   compares like with like. The row's tooltip is the place a reader will meet
   this, and the value it will open on after DEFAULTS1 is 0.

   Because this lane still ships the old default, the picture and the gate in
   this report both show the row at its OLD value. That is not a disagreement
   with the ruling; it is the ruling being carried out in the right order.

### 0.1 Selection, source and output

| switch | sets | default | class | panel row |
|---|---|---|---|---|
| `--worldspace <hex>` | `lgWorldspace` | 0 | BAKE | Worldspace |
| `--terrain-region x0 y0 x1 y1` | `lgRegion` | none | BAKE | West/East/South/North cell |
| `--terrain x y` | one chunk | none | BAKE | (the range, set to one chunk) |
| `--objects x y` | one object chunk | none | BAKE | (the range + Object LOD chunks) |
| `--cell x y` | `lgCell` | none | DIAG | CLI-ONLY: a one-cell question, answers and stops |
| `--dim N` | `lgDim` | 4 | BAKE | Chunk size |
| `--out-dir <dir>` | `lgOutDir` | none | PATH | Output mod |
| `--tex-dir <dir>` | `lgTexDir` | none | PATH | CLI-ONLY: the panel derives it from the output mod folder (a mod folder IS a Data folder) |
| `--vt <dir>` / `--no-vt` | `lgVtDir` | off | BAKE | Terrain virtual texture (.lodt) |
| `--native <dir>` | `lgNativeDir` | off | BAKE | Native object files (.lodo/.lodi) |
| `--lodl <dir>` | `lgLodtDir` | off | BAKE | Landscape file (.lodl) |
| `--heightmap <dir>` | `lgHeightmapDir` | off | BAKE | Shadow heightmap (.HeightMap.dds) |
| `--impostors <dir>` | `lgImpostors` | empty | PATH | Impostor cards |
| `--data-root <dir>` | `lgDataRoot` | empty | PATH | CLI-ONLY by house rule: assets come from the game's own folders and archives (Settings > Resources); there is no "unpacked Data folder" field |
| `--resource <path>` | `lgResources` | empty | PATH | Resources |
| `--plugins-txt <file>` | `lgPluginsTxt` | MO2's own | PATH | CLI-ONLY: the MO2 source reads Mod Organizer's own `plugins.txt` at its known location |
| `--mo2` | `lgMo2` | false | BAKE | Source = Mod Organizer 2 |
| `--refresh-ao` | `lgRefreshAo` | false | BAKE | Only refresh the AO plane in the existing file |
| `--incremental <dir>` | `gLgIncremental` | empty | BAKE | **OWED**: the switch only arms a flag; the ledger-diff leg that acts on it lives in `cmdLodgen`, not in the run the panel drives, so a row would be a tick with nothing behind it. Needs a run-driver change — see Owed |
| `--threads N` | thread budget | machine | BAKE | **NEW** (Run) |
| `--chunk-threads N` | chunk queue | 1 | BAKE | **NEW** (Run) |

### 0.2 Land sampling and detail (`src/lodgen.cpp` statics — the panel sets none of them today)

| switch | sets | default | class | panel row |
|---|---|---|---|---|
| `--land-tiling F` | `g_landTiling` | 341.3333 | BAKE | **NEW** Land detail |
| `--land-sample <mode>` | `g_landSampleAverage` + the four/one geometry numbers | footprint | BAKE | **NEW** Land detail |
| `--land-detail F` | `g_landDetail` | 0.0 | BAKE | **NEW** Land detail |
| `--land-hex F` | `g_landHexSize` | 0.0 (off) | BAKE | **NEW** Land detail |
| `--land-warp F` | `g_landWarpAmp` | 0.0 (off) | BAKE | **NEW** Land detail |
| `--land-warp-lattice F` | `g_landWarpLattice` | 1024.0 | BAKE | **NEW** Land detail |
| `--land-warp-octaves N` | `g_landWarpOctaves` | 1 | BAKE | **NEW** Land detail |
| `--land-mip-bias F` | `g_landMipBias` | 0.0 | BAKE | **NEW** Land detail |
| `--land-guide <rule>[:k]` | `g_landGuideRule` / `g_landGuideStrength` | off / 1.0 | BAKE | **NEW** Land detail |
| `--land-guide-scale F` | `g_landGuideScale` | 1024.0 | BAKE | **NEW** Land detail |
| `--land-guide-slope F` | `g_landGuideSlopeRef` | 0.5 | BAKE | **NEW** Land detail |
| `--land-detail-source <s>` | `g_landDetailSource` | vanilla | BAKE | **NEW** Land detail |
| `--vanilla-lod-root <dir>` | `g_vanillaLodRoot` | `E:/Tools/Fallout 4/DataUnpacked/Data` | PATH | **NEW** Land detail |
| `--land-shade F` | `g_landShade` | -3.242 | BAKE | **NEW** Land detail |
| `--grade F` | `g_landGrade` | 1.0 | BAKE | **NEW** Land detail |
| `--blend-edges <mode>` | `g_blendEdges` | off | BAKE | **NEW** Land detail |
| `--blend-margin F` | `g_blendMargin` | 128.0 | BAKE | **NEW** Land detail |
| `--erosion F` | `g_erosion` | 0.0 (off) | BAKE | **NEW** Erosion — *this is bungo's question* |
| `--erosion-iterations N` | `g_erosionIterations` | 1 | BAKE | **NEW** Erosion |
| `--erosion-seed N` | `g_erosionSeed` | 1 | BAKE | **NEW** Erosion |

### 0.3 Sheets and cache

| switch | sets | default | class | panel row |
|---|---|---|---|---|
| `--sheet-format <f>` | `g_sheetFormat` | legacy | BAKE | **NEW** Sheets and cache |
| `--msn-cache <dir>` | `g_msnCacheDir` | empty | PATH | **NEW** Sheets and cache |

### 0.4 Ground cover, roads and object occlusion (`LodgenCoverOptions`)

The panel fills exactly two of these fields today (`cover`, `tintStrength`);
every other field reaches a panel run at its struct default.

| switch | sets | default | class | panel row |
|---|---|---|---|---|
| `--cover` / `--no-cover` | `cover` | false | BAKE | Ground cover and grass tint |
| `--grass-tint F` | `tintStrength` | 0.35 | BAKE | Grass tint strength |
| `--cover-full F` | `coverFull` | 96.0 | BAKE | **NEW** |
| `--roads` / `--no-roads` | `roads` | true | BAKE | **NEW** Roads |
| `--road-cover-suppress F` | `roadCoverSuppress` | 1.0 | BAKE | **NEW** Roads |
| `--road-opacity F` | `roadOpacity` | 1.0 | BAKE | **NONE by ruling** (bungo 16:0x): *"we don't use that opacity at all, we render roads at their full diffuse"* |
| `--road-composite <m>` | `roadComposite` | max-z | BAKE | **NEW** Roads |
| `--road-detail F` | `roadDetail` | 1.0 | BAKE | **NEW** Roads |
| `--road-ground-paint F` | `roadGroundPaint` | 1.0 | BAKE | **NEW** Roads |
| `--road-raised` / `--no-road-raised` | `roadRaised` | false | BAKE | **NEW** Roads |
| `--road-sidewalks` / `--no-` | `roadSidewalks` | false | BAKE | **NEW** Roads |
| `--roads-legacy` | the four above, together | off | BAKE | **NONE by ruling** (bungo 16:0x): it restores the older road pipeline, opacity included; stays a command-line escape hatch |
| `--terrain-object-ao` / `--no-` | `terrainObjectAo` | false | BAKE | **NEW** Object occlusion |
| `--terrain-object-ao-strength F` | `terrainObjectAoStrength` | 0.5 | BAKE | **NEW** Object occlusion |
| `--dump-cover <file>` | `dumpCoverPath` | empty | DIAG | CLI-ONLY: writes a raw plane for a gate to read |
| `--dump-object-ao <file>` | `dumpObjectAoPath` | empty | DIAG | CLI-ONLY: same |

### 0.5 Legacy terrain chunks (`LodgenTerrainOptions`)

| switch | sets | default | class | panel row |
|---|---|---|---|---|
| `--target-tris N` | `targetTrisPerCell` | 130 | BAKE | Triangles per cell at dim 4 |
| `--water-subdiv N` | `waterSubdiv` | 3 | BAKE | **NEW** (under LOD water) |
| `--shore-denser` / `--no-` | `shoreDenser` | false | BAKE | Denser geometry at shorelines |
| `--shore-density N` | `shoreDensity` | 1 | BAKE | Shoreline density |
| `--geomorph` | `geomorph` | false | BAKE | Geomorph weights |
| `--terrain-identity` / `--no-` | `terrainIdentity` | true | BAKE | **NONE by ruling** (bungo 15:56): *"Legacy terrain bakes stay as they were, no extra data for FO4CS to be included in them."* The existing row is left exactly as it is; the default flips in DEFAULTS1, not here |
| (no switch) | `water` | true | BAKE | LOD water — *panel row with no CLI face* |
| (no switch) | `waterChannels` | true | BAKE | no row, no switch — outside this lane's mandate (the lane adds rows for SWITCHES); listed under Owed |
| (no switch) | `waterCullBuried` | true | BAKE | same |

### 0.6 Objects (`LodgenObjectOptions` and the object pass)

| switch | sets | default | class | panel row |
|---|---|---|---|---|
| `--no-identity` | `identity` | true | BAKE | Identity channels and manifests |
| `--no-ao` | `bakeAO` | true | BAKE | Bake vertex AO |
| `--ao-skirt N` | `aoSkirtCells` | 1 | BAKE | AO skirt (cells) |
| `--ao-grey` | `aoGrey` | false | DIAG | CLI-ONLY: `lodgen.h` calls it a debug view, not a shipping profile |
| `--cull-buried` | `cullBuried` | false | BAKE | Drop geometry buried in the terrain |
| `--cull-margin F` | `cullMargin` | 128.0 | BAKE | Buried margin (units) |
| `--slot-fallback` | `slotFallback` | false | BAKE | Use a nearer LOD slot when the ring's is empty |
| `--trees-only` / `--no-` | `treesOnly` | true | BAKE | Trees only |
| `--impostors-from-level N` | `impostorFromLevel` | -1 | BAKE | Tree cards from ring |
| `--card-half-aux` | `cardAuxDiv` | 1 | BAKE | Half-resolution normal, mask and emissive sheets |
| `--atlas` / `--no-atlas` | `lgAtlas` | false | BAKE | Pack an object texture atlas |
| `--atlas-bc1` | `lgAtlasBc1` | false | BAKE | **NEW** (under the atlas) |
| `--arrays` / `--no-arrays` | `lgArrays` | false | BAKE | Texture arrays |
| `--merge` / `--no-merge` | `lgMerge` | true | BAKE | **NEW** |
| `--no-simplify` | `lgSimplify.enabled` | true | BAKE | Far-ring simplification |
| `--simplify8/16/32 F` | ratios | 1.00 / 0.35 / 0.20 | BAKE | Ring 1/2/3 triangles kept |
| `--simplify-error F` | `errorWorld` | 128.0 (struct) | BAKE | Simplification error — **row default is 32.0, the struct's is 128.0; flagged, not changed** |
| `--native-no-ladder` | ladder off | on | BAKE | **NEW** (under Native object files) |
| `--native-no-occluders` | occluders off | on | BAKE | **NEW** (under Native object files) |
| `--aggregate` / `--no-` | `lgAggregate` | false | BAKE | **NEW** |
| `--aggregate-min N` | `lgAggMin` | 8 | BAKE | **NEW** |
| `--aggregate-tile N` | `lgAggTile` | 64 | BAKE | **NEW** |
| `--aggregate-views N` | `lgAggViews` | 8 | BAKE | **NEW** |
| `--candidates <t>` | `lgCandidates` | missing | DIAG | CLI-ONLY: read only by `--list-impostor-candidates`, the card-bake list |
| (no switch) | `treeSway` | true | BAKE | Tree sway weights — *panel row with no CLI face* |
| (no switch) | `objectChannels` | true | BAKE | Sky and ground channels — *panel row with no CLI face* |

### 0.7 Terrain virtual texture (`LodgenVtOptions`)

| switch | sets | default | class | panel row |
|---|---|---|---|---|
| `--vt-finest N` | `finestDim` | 2 | BAKE | Finest level |
| `--vt-content N` | `content` | 256 | BAKE | **NEW** |
| `--vt-border N` | `border` | 8 | BAKE | **NEW** |
| `--vt-mips N` | `mips` | 2 | BAKE | **NEW** |
| `--vt-compress <c>` | `compression` | none (0) | BAKE | **NEW** |
| `--vt-height` | `height` | false | BAKE | **NEW** |
| `--vt-cover-in-color` / `--vt-cover-in-mask` | `coverInColor` | false (mask) | BAKE | **NEW** |
| `--vt-btr` / `--no-vt-btr` | `lgVtBtr` | auto | BAKE | Chunk textures from the pyramid |
| `--vt-estimate` | `lgVtEstimate` | false | DIAG | CLI-ONLY: prints the pyramid cost and stops (the panel shows it live in the summary) |

### 0.8 Landscape file water bodies (`LodtWaterOptions`)

The panel builds `LodtOptions` and leaves `water` at its struct default, so the
whole module is unreachable from the panel today.

| switch | sets | default | class | panel row |
|---|---|---|---|---|
| `--water-bodies` | `enabled` | false | BAKE | **NEW** Water |
| `--water-bridge N` | `bridgeGap` | 2 | BAKE | **NEW** Water |
| `--water-near N` | `nearTexels` | 64 | BAKE | **NEW** Water |
| `--water-body-samples N` | `bodySamples` | 0 (the file's own) | BAKE | **NEW** Water |
| `--water-flow-samples N` | `flowSamples` | 0 (the file's own) | BAKE | **NEW** Water |
| `--water-no-shore` | `shore` | true | BAKE | **NEW** Water |
| `--water-velocities <plugin>` | `velocityPlugin` | empty | PATH | **NEW** Water |
| `--water-report <file>` | `reportPath` | empty | DIAG | CLI-ONLY: a census file for a gate |
| (no switch) | `LodtOptions::aoSamples` | 8 | BAKE | AO samples per cell — *panel row with no CLI face* |
| (no switch) | `LodtOptions::overviewSamples` | 8 | BAKE | Overview samples per cell — *panel row with no CLI face* |
| `--heightmap-size <n\|native>` | `lgHeightmapSize` | 0 (native) | BAKE | Size |

### 0.9 Diagnostics and drivers — all CLI-ONLY

Each answers a question about a file, a stack or the parser and then stops, or
drives a fixture; none of them changes a bake.

`--list-worldspaces`, `--print-source`, `--list-files N`, `--probe <path>`,
`--probe-out <file>`, `--dump-land`, `--dump-layers`, `--dump-shapes`,
`--dump-geometry`, `--dump-cover`, `--dump-object-ao`, `--vt-estimate`,
`--lodm-check`, `--lodt-check`, `--corpus-hash`, `--verify-only`,
`--btd-probe`, `--from-btd <file>` (a fixture source, not the game's data),
`--native-verify <lodo> <lodi>`, `--native-verify-corpus`,
`--native-fixture <dir>`, `--native-mesh-report <file>`,
`--list-impostor-candidates`, `--candidates`, `--stress-file`,
`--stress-threads`, `--stress-reps`, `--stress-sabotage`, `--water-report`,
`--ao-grey`, `--cell`.

Retired spellings that refuse by name and are not settings: `--lodt`,
`--lodv-check`.

### 0.10 The count

Counted by SETTING, not by spelling: a `--x` / `--no-x` pair is one setting, and
the two retired spellings (`--lodt`, `--lodv-check`) are excluded because they
only print an error naming their replacement. 150 spellings, 15 pairs, 2
retired → 133 settings. The count is produced by
`scratchpad/panel1_20260912/audit_count.py`, which reads `src/nifcli.cpp`
itself, so it cannot drift from a hand tally.

| class | settings | already a row | NEW row this lane | CLI-ONLY |
|---|---|---|---|---|
| BAKE | 93 | 37 | 56 | 0 |
| PATH | 9 | 3 | 3 | 3 |
| DIAG | 31 | 0 | 0 | 31 |
| **total** | **133** | **40** | **59** | **34** |

That is what the audit says before the rulings. Three of the 59 do NOT get a row,
and the reason for each is in 0.0: `--road-opacity` and `--roads-legacy` (ruled
out), and `--incremental` (owed, its acting leg is not in the panel's run). So
**56 switches got a row**, in **57 rows** — one more row than switches because
`--land-guide <rule>:<k>` carries two settings in one argument and the panel
gives the strength its own field rather than asking anybody to type a colon.

| | count |
|---|---|
| NEW in the audit | 59 |
| ruled out (0.0 items 1) | 2 |
| owed (`--incremental`) | 1 |
| **switches given a row** | **56** |
| extra row for the `:k` half of `--land-guide` | 1 |
| **rows added** | **57** |

Five settings the PANEL has that the CLI cannot reach at all (the traffic runs
both ways, and these are stated so nobody later calls the panel a subset of the
command line): `LOD water`, `Tree sway weights`, `Sky and ground channels`,
`AO samples per cell`, `Overview samples per cell`. `Card frames` and
`Card resolution` are the card-bake script's, not `lodgen`'s.

One disagreement found while reading, reported and **not** changed because a
default is bungo's call: the `Simplification error` row loads 32.0 while
`LodgenSimplifyOptions::errorWorld` is 128.0 and the header calls 128 "the
measured knee". A panel run and a CLI run with no switch therefore simplify to
different rails today.

## 1. Rows added

57 rows, covering 56 command-line switches. One switch carries two settings in
one argument (`--land-guide <rule>:<k>`) and the panel gives the strength its own
field, which is the 57th row; the arithmetic is in 0.10.

Every default in the "default" column is READ OUT OF THE CODE that holds it --
`src/lodgen.cpp`'s file statics, `src/lodgen.h`'s option structs,
`src/lodtfile.h`'s `LodtWaterOptions` -- and the table below is generated from
`src/lodgenmanager.cpp` itself by
`scratchpad/panel1_20260912/../../scratchpad/panel1_20260912/rows_table.md`'s
script, so a row renamed in the code renames itself here. **No default moved.**
The one that looks like a new one is not: `Atlas format` is a three-way selector
whose default, "Match the target", is exactly the rule the panel already used
(BC1 off under FO4 Community Shaders, on under the stock engine).

Every row: a whole-word label with no explanation beside it, the explanation in
the tooltip with the switch named at the end of it, numbers through
`wwMakeScrubField` and selectors through `wwMatchFieldStyle` (both of which
guard the wheel), one setting to a row, and its own `LodGeneration/<key>` in
QSettings.

| row | section | switch | default | kind |
|---|---|---|---|---|
| Tile content | Terrain virtual texture | `--vt-content` | 256 | whole number |
| Tile border | Terrain virtual texture | `--vt-border` | 8 | whole number |
| Tile mips | Terrain virtual texture | `--vt-mips` | 2 | whole number |
| Tile compression | Terrain virtual texture | `--vt-compress` | 0 | selector |
| Carry a height layer | Terrain virtual texture | `--vt-height` | false | tick |
| Ground cover in the colour layer | Terrain virtual texture | `--vt-cover-in-color / --vt-cover-in-mask` | false | tick |
| Water subdivision | Terrain | `--water-subdiv` | 3 | whole number |
| Texture repeat | Land detail | `--land-tiling` | 341.3333 | number |
| Sample rule | Land detail | `--land-sample` | 0 | selector |
| Detail over the average | Land detail | `--land-detail` | 0.0 | number |
| Hex tile size | Land detail | `--land-hex` | 0.0 | number |
| Warp amplitude | Land detail | `--land-warp` | 0.0 | number |
| Warp lattice | Land detail | `--land-warp-lattice` | 1024.0 | number |
| Warp octaves | Land detail | `--land-warp-octaves` | 1 | whole number |
| Mip bias | Land detail | `--land-mip-bias` | 0.0 | number |
| Guide rule | Land detail | `--land-guide` | 0 | selector |
| Guide strength | Land detail | `--land-guide` | 1.0 | number |
| Guide scale | Land detail | `--land-guide-scale` | 1024.0 | number |
| Guide slope reference | Land detail | `--land-guide-slope` | 0.5 | number |
| Fine detail from | Land detail | `--land-detail-source` | 1 | selector |
| Vanilla LOD root | Land detail | `--vanilla-lod-root` | QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ) | path |
| Crevice shading | Land detail | `--land-shade` | -3.242 | number |
| Colour grade | Land detail | `--grade` | 1.0 | number |
| Quadrant edges | Land detail | `--blend-edges` | 0 | selector |
| Quadrant margin | Land detail | `--blend-margin` | 128.0 | number |
| Strength | Erosion | `--erosion` | 0.0 | number |
| Rounds | Erosion | `--erosion-iterations` | 1 | whole number |
| Seed | Erosion | `--erosion-seed` | 1 | whole number |
| Sheet format | Sheets and cache | `--sheet-format` | 0 | selector |
| Normal cache folder | Sheets and cache | `--msn-cache` | QString() | path |
| Full cover at | Ground cover | `--cover-full` | 96.0 | number |
| Diffuse detail kept | Roads | `--road-detail` | 1.0 | number |
| Verge painted as road | Roads | `--road-ground-paint` | 1.0 | number |
| Cover suppressed under | Roads | `--road-cover-suppress` | 1.0 | number |
| Pieces combine by | Roads | `--road-composite` | 0 | selector |
| Paint raised road families | Roads | `--road-raised / --no-road-raised` | false | tick |
| Paint sidewalks | Roads | `--road-sidewalks / --no-road-sidewalks` | false | tick |
| Strength | Object occlusion in the terrain | `--terrain-object-ao-strength` | 0.5 | number |
| Bridge gap | Water bodies in the landscape file | `--water-bridge` | 2 | whole number |
| Near texels | Water bodies in the landscape file | `--water-near` | 64 | whole number |
| Body samples | Water bodies in the landscape file | `--water-body-samples` | 0 | whole number |
| Flow samples | Water bodies in the landscape file | `--water-flow-samples` | 0 | whole number |
| Write the shore line | Water bodies in the landscape file | `--water-no-shore` | true | tick |
| Velocity plugin | Water bodies in the landscape file | `--water-velocities` | QString() | path |
| Forested at | Aggregate impostor sheets | `--aggregate-min` | 8 | whole number |
| Frame size | Aggregate impostor sheets | `--aggregate-tile` | 64 | whole number |
| Views | Aggregate impostor sheets | `--aggregate-views` | 8 | whole number |
| Build the distance ladder | Object modules | `--native-no-ladder` | true | tick |
| Build the occluder boxes | Object modules | `--native-no-occluders` | true | tick |
| Merge the chunk shapes | Object modules | `--merge / --no-merge` | true | tick |
| Atlas format | Object modules | `--atlas-bc1` | -1 | selector |
| Model threads | Run | `--threads` | 0 | whole number |
| Chunk threads | Run | `--chunk-threads` | 1 | whole number |
| Roads | (section header) | `--roads / --no-roads` | true | tick |
| Object occlusion in the terrain | (section header) | `--terrain-object-ao / --no-terrain-object-ao` | false | tick |
| Water bodies in the landscape file | (section header) | `--water-bodies` | false | tick |
| Aggregate impostor sheets | (section header) | `--aggregate / --no-aggregate` | false | tick |

### 1.1 How each family reaches the bake

* **Land, erosion, grade, blend, sheets, cache, threads** are file statics
  inside `src/lodgen.cpp`, reachable only through their `lodgenSet*()` setters.
  The panel writes ALL of them at the top of every run (`applyGeneratorSettings()`,
  called from `start()` right after `saveSettings()`), not only when a row moves
  -- otherwise the LAST run's value would still be standing.
* **Ground cover, roads, object occlusion** are fields of the
  `LodgenCoverOptions` the panel already built; they were simply never filled.
* **The pyramid's own numbers** are fields of `LodgenVtOptions`, likewise.
* **Water bodies** are a `LodtWaterOptions` the panel never filled at all.
* **The distance ladder and the occluder boxes** are trailing arguments of
  `lodgenNativeBegin()` that the panel was letting default.
* **Aggregate sheets** need three calls the panel did not make:
  `lodgenAggregateCards()`, `lodgenNativeSetAggregate()` and a
  `lodgenAggregateWrite()` after the native write. The refusal, when there is no
  card library to aggregate, is carried in the run's own message.
* **Hidden means default**: `xvar()` returns the command line's default for any
  row the target has hidden, so a setting the target cannot use cannot reach the
  bake either. The save writes what the WIDGET says (`xraw()`), so hiding a row
  never overwrites what a person typed into it.
* **Three sync rules** keep the panel from disagreeing with itself: the two
  compound sample modes HIDE the four numbers they set; the guide's three
  numbers grey while the rule is off; erosion's rounds and seed grey at
  strength 0.

### 1.2 Does the row reach the bake? — the per-row gate, row by row

`tests/spells/lodgen_byte_gate.sh` phase (b), on the 17:39:04 exe. The panel
bakes Sanctuary (-20,24) at dim 4 once with every new row forced to its own
default, then once more per row with that row alone moved, and compares the two
trees byte for byte. Where a row hangs off another, the gate turns the parent on
(and, where the parent is a rule rather than a switch, sets it to the rule that
READS this row), bakes a local baseline under that parent, and only then moves
the row — so a quiet row is the row's own verdict and not the setup's.

Before the baseline it writes every new row's default into its widget and says
which ones this machine had saved differently -- one, `waterSubdiv`, which this
machine held at 4 where the code default is 3 -- because a bake measured against
remembered settings measures the machine, not the rows (§4.4).

| verdict | rows |
|---|---|
| **the bake MOVES** | 32 |
| **identical bake** (reaches nothing on this chunk, or is the control) | 2 |
| **named skip** (this chunk cannot exercise it; each names the spell that does) | 23 |
| the panel refused the run | 0 |
| the row could not be moved off its default | 0 |
| **asked in total** | 57 |

**Every row this chunk can exercise moves the bake, and the only two that do not
are the control.** The two thread counts MUST leave the bytes alone -- a bake
whose output depends on how many cores ran it would be a bug, not a setting --
and the gate prints that as its own line: `gate control (threads, chunk threads)
leave the bytes alone: yes`. Nothing else came back quiet. The two rows that did
come back quiet on the earlier builds were the instrument, and §4.3 and §4.4
carry what the command line said about them.

The 23 skips are 20 rows this one chunk cannot reach (roads with no road data in
the region, the aggregate sheets with no card library, the legacy terrain path,
and so on -- each names in the log the spell that does exercise it) and the 3
path rows, which are a folder and not a setting. That is phase (b)'s own count:
**49 bakes** (19 rows that stand alone, one bake each; 15 rows that hang off a
parent, a local baseline plus the row).

The per-row verdicts are the last column of
`scratchpad/panel1_20260912/rows_table.md`, one line per row, generated from the
log. The full gate output is `scratchpad/panel1_20260912/byte_gate4.txt` and the
panel's own log of it is `release/ww_lodgen_test.log`.

## 2. Build and chain

Seven builds, each gated on make's own exit code, each with Fallout 4 checked
down first (`tasklist` count 0) and `release/NifSkope.exe` checked for a window
holding it (never held; nothing was renamed aside and nothing was killed). Lane
SHOWCASE1 was baking throughout from its own copy under
`scratchpad/showcase1_20260912/ns_run/`, headless (`-no-gui`, no `--port`), so no
GUI harness of this lane ever ran beside one of its.

| build | finished | exe bytes | translation units rebuilt | rc |
|---|---|---|---|---|
| 1 | 16:19:01 | 22,110,720 | `lodgenmanager.o`, `nifskope_ui.o` | 0 |
| 2 | 16:31:05 | 22,128,640 | `nifskope_ui.o` | 0 |
| 3 | 16:36:41 | 22,128,640 | `nifskope_ui.o` | 0 |
| 4 | 17:01:39 | 22,137,856 | `nifskope_ui.o` | 0 |
| 5 | 17:20:42 | 22,141,952 | `nifskope_ui.o` | 0 |
| 6 | 17:39:04 | 22,151,168 | `nifskope_ui.o` | 0 |
| 7 | 18:07:58 | 22,154,240 | `nifskope_ui.o` | 0 |

**Only build 1 compiled the panel.** `src/lodgenmanager.cpp` -- the file the 57
rows live in -- was compiled once, in build 1, and not touched again. Builds 2
to 7 are the SELF-TEST (`src/nifskope_ui.cpp`) learning to ask the rows a fair
question: the grab-width knob, then the per-row gate's dependency pass, then the
two rows the command line proved the gate was asking wrongly (§4.3), and last
the forcing loop that writes every new row's DEFAULT into its widget before the
baseline bake, so that "every row at its default" is made true rather than
assumed (§4.4 — this machine had `Water subdivision` saved at 4 where the code
default is 3, and that one number was the whole panel-versus-command-line
disagreement). So the
panel that gate (a) measured and the panel in the shipped exe are the same
machine code, and the later builds cannot have moved a bake.

The rung: `release/NifSkope.before_panel1.exe`, 2026-09-12 15:49:03,
22,007,808 B, sha1 `ba7585cba389c8b38f0e07c6c11063e0cec1124c` — the same bytes as
the 12:58:48 `release/NifSkope.exe` this lane started from, verified before the
first build.

The chain after the last build:

* `BUILD-RC=0` — make's own status, not a grep's.
* `grep -oE "-o GeneratedFiles/.obj/[a-z_0-9]+.o"` over the build log: one unit.
  The new exe is the old exe plus this lane's diff, and nothing another lane
  left in the tree was dragged into it.
* The `git status --porcelain -- src res tools tests` sweep: every changed file
  is OLDER than `release/NifSkope.exe`. Nothing newer, so no gate ran against a
  source the exe does not carry.
* Stale objects: no header was touched this lane (the two changed files are
  `.cpp`, and neither gained or lost an `#include` of a project header — only two
  Qt headers, `QDirIterator` and `QCryptographicHash`, were added to
  `nifskope_ui.cpp`), so qmake's frozen dependency lists cannot be wrong here.
* `MSYSTEM=UCRT64 ... make -n | grep -c "g++ -c"` = **0** compile lines. Nothing
  is left to build.
* `cmp res/style.qss release/style.qss` — in step; the link-time copy was made
  beside the exe at every link.
* `release/NifSkope.exe`, 2026-09-12 **18:07:58**, **22,154,240 bytes** — the exe
  the pictures of §3 and the re-run of gate (a) were taken on. Build 7 is the
  picture knob and nothing else: the row plumbing did not change, so gates (b)
  and (c) were NOT re-run and their numbers below are the 17:39:04 exe's; gate
  (a) was re-run on 18:07:58 and came back identical again
  (`scratchpad/panel1_20260912/byte_gate5_a.txt`), and the structural spell was
  re-run too: 121 checks, 0 failures. The command-line numbers in §4.3 and
  §4.4 were measured on the 17:01:39 and 17:20:42 exes; the command line is the
  same code in all three (no build after 1 compiled anything but the self-test),
  and each is named where it is used.

### 2.1 Harnesses

| harness | why | result |
|---|---|---|
| `tests/spells/lod_generation.sh` | the panel's structure, which is where 57 rows land | **121 checks, 0 failures, PASS**, re-run on the 18:07:58 exe; floor raised 116 → 121 |
| `tests/spells/lodgen_byte_gate.sh` (new) | the rows have to reach the BAKE, not just exist | **PASS**, `byte gate failures: 0`; §1.2 per row, §2.3 for the two identity gates |
| `tests/spells/lodgen_panel_run.sh` | it presses Generate; its floor moves with the suite | floor raised 125 → 130; run inside the byte gate's phase (a) |
| `tests/spells/lodgen_terrain.sh` | the CLI defaults must not move | NOT RUN this lane -- the byte gate's phase (c) asks the same question in bytes and harder (the panel's bake against the command line's, 15 files identical, §2.3); the spell itself is owed to whoever next changes a terrain default |
| `tests/spells/ui_align.sh` | SKIPPED: no dock chrome moved — the rows go inside the existing scrolling settings, and no splitter, bar or heading weight changed |
| the card, road, cover and native spells | SKIPPED: this lane adds no generator behaviour; `src/lodgen.cpp` was not touched |

### 2.2 The floors, and the old exe watching them fail

Run against `release/NifSkope.before_panel1.exe` first, 16:2x, so the new floors
could be seen going red rather than asserted:

| counted | rung | new exe | floor set | rung's verdict |
|---|---|---|---|---|
| number fields | 15 | 49 | 45 | below the floor |
| headings | 4 | 11 | 10 | below the floor |
| selectors | 10 | 18 | 14 | below the floor |
| checks in the suite | 116 | 121 | 121 | below the floor |

Every floor is under the MEASURED count, not over it: 45 under 49, 10 under 11,
14 under 18, 121 at 121. The rung fails all four.

### 2.3 The two byte-identity gates

Both on the 17:39:04 exe, both on Sanctuary (-20,24) at dim 4, both in
`scratchpad/panel1_20260912/byte_gate4.txt`; gate (a) was re-run on the
18:07:58 exe after the picture knob went in and came back with the same digest
and the same 11 files and 47,645,750 bytes
(`scratchpad/panel1_20260912/byte_gate5_a.txt`, `byte gate failures: 0`). The whole spell ends
`byte gate failures: 0`.

**(a) Fifty-seven new rows at their defaults move not one byte.** The same
panel-driven run on the rung (`NifSkope.before_panel1.exe`, 15:49:03) and on the
new exe, each tree hashed whole by `tests/spells/lodgen_tree_digest.py`:

```
f77f8410b893a2ba514d2baa34e27f36f25d81da  ...Lodgen_PANEL1_rung  11 files  47645750 bytes
f77f8410b893a2ba514d2baa34e27f36f25d81da  ...Lodgen_PANEL1_new   11 files  47645750 bytes
IDENTICAL: 11 files, 47645750 bytes
```

The panel's own structural spell ran inside it: 125 checks 0 failures on the
rung, 130 checks 0 failures on the new exe.

**(b)'s baseline, for the record**: 15 files, 50,965,198 bytes, digest
`8ac6e650cee00ccce3c61d57cbdefe8eedb8054b`, 11,954 ms. It is larger than (a)'s
tree because the gate's own run asks for the object arrays and the landscape
file as well.

**(c) The panel's bake and the command line's bake of the same chunk, with the
same settings, are byte-identical in every file: `15 identical, 0 differ, 0
missing`.** Compared BY NAME, not by tree shape -- the panel nests its output the
way the game does and the command line writes where it is pointed -- across the
landscape file (`Commonwealth.lodl`), the terrain mesh and its object mesh and
manifest (`.BTR`, `.BTO`, `.BTO.manifest.txt`), the three terrain textures
(colour, data, `_msn`), the six object-array files, and `Commonwealth.lodo` /
`Commonwealth.lodi`. `Commonwealth.lodb` is excluded everywhere and named as
excluded: it is the ledger the generator keeps of its own runs, and it differs
between any two runs by design.

**One caution on reading phase (c).** "The same settings" means the same NEW
rows: the forcing loop writes the defaults of the 57 rows this lane added and
touches nothing else. The panel and the command line still disagree about
`Simplification error` -- the panel opens on 32, the bake's own default is 128
(0.10, and it is bungo's call, not this lane's) -- and the trees still came out
identical, because on THIS chunk that rail is not what the mesh is cut against:
`--simplify-error 32` changes that file not one byte here, measured, 17:2x. So
phase (c) proves the 57 rows agree with the command line; it does not prove the
panel and the command line agree about everything, and that difference is named
in §4.2 rather than quietly folded in.

This is the gate the brief asked for, and it took two corrections to say
anything true: the panel had to be forced to the code defaults rather than left
holding this machine's saved ones (§4.4), and the comparison had to be file by
file rather than tree against tree.

## 3. Pictures

**The first three pictures were red, and the director caught it.** A dock grab
photographs only the scroll area's VISIBLE viewport, and the harness centres on
the object head, so `panel_after.png` and `panel_before_rung.png` are the same
frame -- Source down to Native object files -- and every one of the 57 new rows
is below it. `panel_min.png` is byte-identical to `panel_after.png`, which is
true and useless. A picture of the panel that does not show the change is no
proof of the change.

So the harness gained a second grab, `WW_LODGEN_SHOT_FULL=<png>`: same dock
arrangement, but it photographs the settings scroll's INNER widget, whose height
is the whole column, with every folding section opened for the grab and folded
back afterwards. The fold state is the operator's and it persists
(`LodGeneration/expanded/<key>`), so each section is clicked open and clicked
shut again -- the click is what writes the setting, so the stored value comes
back with the widget. The log says what it did:

```
full screenshot saved: ...shots/panel_full_after.png, the whole settings column
483x2897 px, 6 folding section(s) opened for it and folded back
```

| picture | exe | what it is | size |
|---|---|---|---|
| `shots/panel_full_after.png` | `NifSkope.exe`, 18:07:58 | **the whole settings column, every section open** | 483x2897, 130,540 B |
| `shots/panel_after.png` | `NifSkope.exe`, 18:07:58 | the dock as a person first sees it | 497x765, 43,549 B |
| `shots/panel_before_rung.png` | `NifSkope.before_panel1.exe`, 15:49:03 | the same viewport on the rung | 497x765, 43,597 B |
| `shots/panel_min.png` | `NifSkope.exe`, 17:01:39 | the dock at its minimum width | 497x765, 43,549 B |

Sizes read back with PIL, not from the log. The full picture carries 55 of the
57 new rows; the two it does not are hidden by the target rule, named below. **The old grab moved not one pixel**:
the 18:07:58 exe's `panel_after.png` is byte-identical to the one the 17:01:39
exe took (`cmp`, 43,549 B both).

**There is no full-column BEFORE picture, and there cannot be one from this
lane**: `WW_LODGEN_SHOT_FULL` is code that only exists in the new exe, and the
rung cannot be asked for a picture it has no knob for. The before side is
therefore `panel_before_rung.png` plus the rung's own counts, printed by the
same self-test on the same panel (`ww_lodgen_test.before.log`, 16:20):
**15 number fields, 4 headings, 10 selectors, 116 checks** -- against
**49 number fields, 11 headings, 18 selectors, 121 checks** on the new exe. The
column the picture shows is what those 34 extra fields and 7 extra headings look
like.

### 3.1 What the full picture shows, heading by heading

Reading the column top to bottom, the new rows sit under:

* **Landscape file (.lodl), Shadow heightmap, Native object files** -- nothing
  new. Everything down to `Trees only`, `Card frames` and `Card resolution` was
  already there before this lane; it is in the picture because the picture is
  the whole column, and it is where the eye should stop expecting new rows.
* **Terrain virtual texture (.lodt)** -- six new, under a heading that already
  existed: `Tile content`, `Tile border`, `Tile mips`, `Tile compression`, and
  the two ticks `Carry a height layer` and `Ground cover in the colour layer`.
  (`Finest level` above them is older.)
* **Terrain** (new heading) -- `Water subdivision`.
* **Land detail** (new heading) -- the eighteen: `Texture repeat`, `Sample rule`,
  `Detail over the average`, `Hex tile size`, `Warp amplitude`, `Warp lattice`,
  `Warp octaves`, `Mip bias`, `Guide rule` and its `Guide strength`, `Guide
  scale`, `Guide slope reference`, `Fine detail from`, `Vanilla LOD root`,
  `Crevice shading`, `Colour grade`, `Quadrant edges`, `Quadrant margin`.
* **Erosion** (new heading) -- `Strength`, `Rounds`, `Seed`.
* **Sheets and cache** (new heading) -- `Sheet format`, `Normal cache folder`.
* **Ground cover** (new heading) -- `Full cover at`, and under it the folding
  **Roads painted into the far terrain**: `Diffuse detail kept`, `Verge painted
  as road`, `Cover suppressed under`, `Pieces combine by`, `Paint raised road
  families`, `Paint sidewalks`.
* **Far terrain shaded by the objects on it** (folding) -- `Strength`.
* **Water bodies in the landscape file** (folding) -- `Bridge gap`,
  `Near texels`, `Body samples`, `Flow samples`, `Write the shore line`,
  `Velocity plugin`.
* **Aggregate impostors for forested cells** (folding) -- `Forested at`,
  `Frame size`, `Views`.
* **Object modules** (new heading) -- `Build the distance ladder` and
  `Build the occluder boxes`. The other two rows under this heading,
  `Merge the chunk shapes` and `Atlas format`, are **not in the picture and
  should not be**: they belong to the stock engine's chunk path and the panel
  hides them under the FO4 Community Shaders target
  (`src/lodgenmanager.cpp:1919-1920`, `showExtra("merge", !cs)` /
  `showExtra("atlasFormat", !cs)`), exactly as the ladder and occluder rows are
  hidden under the stock target. So this picture shows **55 of the 57** new
  rows, and the missing two are a target rule, not a missing row. **Owed**: the
  full-column grab does not switch targets, so there is no full stock-target
  picture; `WW_LODGEN_SHOT_FULL` would need the target switch
  `WW_LODGEN_SHOT_STOCK` already has, and that is a build this lane is not
  taking (lane NATIVEVIEW1 has the tree next).
* **Run** (new heading) -- `Model threads`, `Chunk threads`.

Three things the picture settles that no count can:

* **The greying rules are visible and correct.** `Guide strength`, `Guide scale`
  and `Guide slope reference` are grey under `Guide rule: Off`; `Rounds` and
  `Seed` are grey at `Strength 0.000`; every row inside an unticked folding
  section is grey. Nothing is hidden that a person would go looking for.
* **Every row is label-left, one control right, at the same label column**, and
  the four new folding sections read as a heading with a tick. No new kind of
  control, no colour, weight, spacing or bar moved: `res/style.qss` was not
  touched.
* **`Water subdivision` reads 4 in this picture, and its default is 3.** That is
  this machine's saved setting showing through, which is exactly the thing §4.4
  is about -- the picture is of the panel as bungo's machine opens it, not of
  the code's defaults. The gate forces the defaults; a photograph does not.

The panel is 2,897 px tall with everything open, which is the argument for the
folding sections: folded, it is about a screen and a half, and a person finds a
row by its heading rather than by scrolling past everything.

## 4. Owed / red / bungo's calls

### 4.1 Owed — named, with the reason each one is owed

1. **`--incremental` has no row.** Its acting leg is not in the panel's run: it
   lives in the command line's run driver (`src/nifcli.cpp`, the block around
   3707-3866) which reads `Commonwealth.lodb`, compares the argument-vector
   digest, refuses the run outright when the region or the flags do not match
   the ledger, and prints `incremental: N of M chunks dirty`. The panel's run
   never goes through that driver, so a tick wired to an option field would be a
   tick that changes nothing. Giving it a row needs a run-driver change, and the
   brief's rule is plumbing only. **Owed: a lane that moves the ledger-diff into
   the shared run, then the row.**
2. **Two bake fields have no switch AND no row**: `waterChannels` and
   `waterCullBuried` (`src/lodgen.h:526` and `:535`, in `LodgenTerrainOptions`).
   The audit counts COMMAND-LINE SETTINGS, so neither is in the 133 and neither
   is a miss against the brief -- but "every CLI setting is now a row" is not the
   same sentence as "every bake setting is now a row", and this is the gap
   between them. **Owed: a decision on whether they become switches, rows, or
   stay internal.**
3. **Twenty rows the gate could not exercise on this chunk** (7 road, 4
   aggregate, 6 terrain-virtual-texture, plus `nativeOccluders`, `merge` and
   `atlasFormat`). Each is a NAMED SKIP in the gate log with the reason and the
   spell that does read it -- never a silent pass. **Owed: a second gate pass
   with those module sets armed** -- a card library from
   `tools/bake_impostor_cards.sh` for the aggregate four, the VT module on for
   the six, a chunk that actually has an occluder box, and the stock-engine
   target for `merge` and `atlasFormat`, which are hidden (and therefore read as
   their defaults, by design) under the FO4 Community Shaders target.
4. **A full-column picture of the STOCK target.** `WW_LODGEN_SHOT_FULL`
   photographs the panel as it stands and does not switch targets, so the two
   rows hidden under FO4 Community Shaders -- `Merge the chunk shapes` and
   `Atlas format` -- appear in no picture this lane took (§3.1). The switch is
   four lines, copied from `WW_LODGEN_SHOT_STOCK`; it is owed rather than done
   because lane NATIVEVIEW1 has this tree next and the brief holds this lane to
   one relink.
5. **The `--road-ground-paint` default move to 0** belongs to lane DEFAULTS1 by
   bungo's 16:1x ruling, not here, so the byte-identity gate in this report
   still compares like with like. This lane ships the row at the OLD default on
   purpose.

### 4.2 Red — measured, reported, NOT changed, because a default is bungo's call

1. **`Simplification error`: the panel opens on 32.0, the bake's own default is
   128.0** (`LodgenSimplifyOptions::errorWorld`, whose comment calls 128 "the
   measured knee"). A panel run and a command-line run with no switch therefore
   simplify to different rails today, and they did before this lane as well --
   the row only makes it visible. Changing either number changes a default, so
   it is not mine to change. Measured on this chunk before blaming it for the
   panel-versus-command-line difference: `--simplify-error 32` changes the
   terrain mesh not one byte here, so the two rails happen to meet on
   Sanctuary (-20,24) at dim 4 and will not everywhere.
   **His call: 32, 128, or one of them moves.**
2. **`vanillaLodRoot` opens on a hardcoded machine path**,
   `E:/Tools/Fallout 4/DataUnpacked/Data`. It was already the code's default; the
   row makes it a default a person can SEE, which is the first time it reads as
   a shipped choice rather than as somebody's directory. **His call: leave it,
   blank it, or derive it from the loaded plugin's data folder.**
3. **`threads` and `chunkThreads` move no bytes, and must not.** The gate reports
   them as reaching nothing and that is the correct result, not a defect: the
   same chunk baked on a different thread count being byte-identical is the
   check the harness makes by name. They are listed here so nobody later reads
   "reaches nothing" in the table as a broken row.

### 4.3 The two rows the gate could not explain, and what the command line said

The per-row gate's second run named two rows it could reach and could not move:
`Guide slope reference` and `Water drainage proximity`. Under this lane's own
rule -- a row whose non-default value bakes identically is a row that reaches
nothing -- that is a red result, so it was MEASURED on the command line rather
than argued about. Same chunk, same dim, four pairs of bakes,
`scratchpad/panel1_20260912/quiet_rows.txt`, 17:16:

| asked | result |
|---|---|
| `--land-guide slopewarp`, slope reference 0.5 vs 0.55 | the colour sheet MOVES |
| `--land-guide drag`, slope reference 0.5 vs 0.55 | the same bytes |
| `--water-near 64` vs `65` | the same landscape file |
| `--water-near 64` vs `512` | MOVES (38,612,038 → 38,613,632 B) |
| `--water-near 64` vs `4` | MOVES (38,612,038 → 38,610,892 B) |

**Neither row reaches nothing. The gate was asking both of them a question
nobody means.**

* The guide's slope reference is read by `lodgenLandGuideWeight`, which only the
  `slope warp`, `flat warp` and `aspect` rules call. The gate turned the guide
  "on" by stepping its selector one place, which lands on `drag`, and drag
  steers by the downhill direction alone -- it never asks how steep a full slope
  is. The dependency is not "the guide is on"; it is "the guide is on a rule that
  reads this".
* The drainage proximity is a RADIUS in texels (`lodtfile.cpp`, the drainage
  relation is bucketed at it). 64 → 65 asks whether two bodies on this one chunk
  sit exactly 65 texels apart. None does. 64 → 512 and 64 → 4 both move the
  file. A one-step bump is the wrong instrument for a threshold.

Both are now written into the gate itself, so it asks correctly next time and
nobody has to remember this page: the row table gained `depVal` (the value the
dependency must TAKE -- `slope warp` for the guide) and `bump` (the value this
row is moved to instead of one step -- 512 for the radius), and the log prints
`(with landGuide at 4)` rather than `(with landGuide on)` so the reader can see
which question was asked. That is build 5, and it is the only thing build 5
changed.

Build 5 fixed one of the two and left the other still quiet: `Guide slope
reference` under `slope warp` STILL baked identically, and §4.4 says why (slope
warp multiplies an amplitude whose default is 0, so the chain is two deep). On
build 6, with `landWarp` on AND the guide at rule 4, it moves. **No row in the
panel reaches nothing.**

**What this cost, said plainly**: four separate times in one lane, the gate
condemned the rows and the gate was wrong (56 of 57, then 37 of 54, then 2 of
49, then 1 of 49), and a fifth time it measured this machine's saved settings
instead of the defaults it claimed to be measuring (§4.4). Each time the fix was
in the instrument. The rule is in `MISTAKES.md` and in two skills now, because
four is not a coincidence.

### 4.4 The panel and the command line disagreed on one file, and it was this machine

Phase (c) of the byte gate bakes the one-chunk Sanctuary region (-20,24) at dim 4
from the PANEL, bakes it again from the COMMAND LINE with the same settings, and
compares the two trees file by name. On build 5 it came out 14 identical, 1
different, 0 missing -- and the one difference was the terrain mesh,
**56,916 B from the panel against 46,518 B from the command line**.

Two candidates, both measured rather than argued:

| asked | result |
|---|---|
| is it the known `Simplification error` gap (the panel opens on 32, the bake's own default is 128)? | NO: `--simplify-error 32` changes that file not one byte on this chunk |
| is it `Water subdivision`? | YES: `--water-subdiv 4` makes ALL FIFTEEN files byte-identical |

`Water subdivision` has a code default of 3. This machine had **4** saved in
`HKCU\SOFTWARE\NifTools\NifSkope 2.0\LodGeneration\waterSubdiv` from earlier use of
the panel, the panel loaded it, and the command line -- which reads no settings
-- used 3. One remembered number was the whole disagreement.

The gate now FORCES what it claims to measure. Before the baseline bake it writes
every new row's own default into its widget, counts the rows this machine had
saved differently, and names them:

```
rows this machine had saved away from their default: 1
  forced back: waterSubdiv (this machine had 4, the default is 3)
```

That line is both the correction and the explanation, so a future run on a
different machine says which numbers it had to put back. (The gate already saved
the operator's whole `LodGeneration` group and restored it at the end; that
protects bungo's settings and does nothing whatever for the measurement. Both are
needed.)

The same build also fixed the last row the gate could not move. `Guide slope
reference` under `slope warp` still baked identically from the panel, because
**slope warp MULTIPLIES the land-warp amplitude, and that amplitude's default is
0**: anything times zero is zero. The chain is two deep -- land warp on AND the
guide on rule 4 -- so the row table gained a second dependency (`dep2`,
`depVal2`) and the log now names the whole chain it built:
`(with landWarp on, landGuide at 4)`. No default was changed to achieve this;
the gate moves the parents for the duration of one bake and puts them back.

## 5. Mistakes

All five are in the root `MISTAKES.md`, newest first, under
`## 2026-09-12 -- lane PANEL1 (every bake setting becomes a row)`, and in full in
`scratchpad/panel1_20260912/MISTAKES_ENTRIES.md`.

**1. A check that grabbed pixels at a HIDDEN widget's coordinates had been
passing by accident.** `WW_LODGEN_TEST`'s "an unticked box can be seen (24+
levels against the ground)" read `LodgenAtlasCheck`. The FO4 Community Shaders
target HIDES that box, and the same log said so three lines above: `FO4CS: the
object atlas hidden: yes`. A hidden widget keeps its last geometry, so the grab
was of whatever was painted at those coordinates -- with four rows above it,
another row's box: 32 levels, green. This lane put fifty-seven rows above it,
the coordinates landed on empty ground, the check read 0 and went red on a
change that touched nothing about check-box contrast. It now walks the panel for
the first VISIBLE unticked box inside the settings scroll and NAMES it in the
log: `unticked box (LodgenCullCheck): ... 32 levels`.

**2. `setProperty` is a no-op when the value does not move, and the gate blamed
the rows for it.** The harness asks the panel to save through a dynamic property
answered by an `event()` override. It fired for the first row and for none of
the other fifty-six, and the gate reported "56 of 57 rows do not round-trip
through QSettings" -- a clean, plausible, completely false result.
`QObject::setProperty` returns early, WITHOUT sending
`QDynamicPropertyChangeEvent`, when the stored value equals the new one: the
second `true` is not a change. The hook now carries `++saveTick`.

**3. The per-row gate bumped a row while the row it hangs off was still off,
and blamed the row.** The first per-row run reported "37 of 54 rows reach
nothing". Erosion rounds cannot move a bake whose erosion strength is 0; the
guide's three numbers cannot move a bake with the guide off; the water numbers
cannot move a landscape file with water bodies unticked. The instrument was
measuring its own setup. The gate now carries a `dep` per row -- it turns the
parent on, bakes a LOCAL baseline, THEN moves the row and bakes again -- and,
where the bake genuinely cannot reach a row at all, a `why` in plain language
that makes it a NAMED SKIP, printed with the spell that does read it, never
counted as a pass. **That is the second time in one hour that this lane's gate
blamed the rows for its own blindness** (mistake 2 was the same shape), which is
why the lesson below is written as a rule rather than as an anecdote.

**4. The gate said "every new row at its default" and was reading this machine's
saved settings instead.** Phase (c) bakes the same chunk from the panel and from
the command line and compares file by file. Fourteen of fifteen matched; the
terrain mesh did not (56,916 B from the panel, 46,518 B from the command line).
The obvious suspect was the known `Simplification error` disagreement -- and it
was innocent: `--simplify-error 32` changes that file not at all on this chunk.
The real one was `Water subdivision`, code default 3, saved on THIS MACHINE at 4
in `HKCU\SOFTWARE\NifTools\NifSkope 2.0\LodGeneration\waterSubdiv` from
earlier use of the panel. The panel loaded 4, the command line used 3, and
`--water-subdiv 4` makes all fifteen files byte-identical. The gate saved the
operator's settings group and put it back at the end, which protects HIS settings
and does nothing at all for the measurement. It now WRITES every new row's
default into its widget before the baseline bake and names the rows this machine
had saved differently: `rows this machine had saved away from their default: 1 /
forced back: waterSubdiv (this machine had 4, the default is 3)`. **That is the
last in a run of instrument errors in this one lane** -- §4.3 counts them -- and
it is the only one that was not the gate blaming a row. §4.4 carries the
measurement.

**5. Three pictures of the panel, and none of them showed the change.** The
deliverable was "show him the new rows". `dock->grab()` photographs the scroll
area's VIEWPORT, so `panel_after.png` and `panel_before_rung.png` came out as the
same frame -- the top of the settings -- with all 57 new rows below it, and
`panel_min.png` byte-identical to `panel_after.png`. Both tells were in the
report already: two pictures of a change coming out identical is first a
statement about the instrument. The director saw it in one look. The fix is
§3's `WW_LODGEN_SHOT_FULL`, and the rule is: **a picture is a gate, so read it
before shipping it and name the thing in it the change made.**

The last four are the ones worth carrying. **56 of 57, and 37 of 54, is the
shape of a broken reader, not of broken rows; and one file out of fifteen
disagreeing is the shape of a machine that remembers something.** Suspect the instrument before the items, and
prove the instrument on one item you can check by hand before believing it about
fifty.

Three smaller ones, not ledger-worthy but worth the next lane's minute: a splice
plan rejected because its top-level key was `splices` where the tool wants
`edits`; two heredoc patches that failed on an `AssertionError: 0` because a
backslash inside a C++ string literal in the anchor was halved on its way through
the shell (the fix is the house rule -- patches go in script FILES, and a literal
backslash is built with `chr(92)`); and an anchor that missed because the file
indents that block with eight tabs and the script wrote six plus four.

## 6. Skill review

Nothing the skills say was disproved. Three gained a section (one of them two),
each written from something this run cost:

| skill | added | why |
|---|---|---|
| `.claude/skills/ww-test-harness-add/SKILL.md` | `## 6a. A check that grabs pixels names the widget, and asserts it is visible` | the hidden-box check above; 17,492 -> 19,047 B |
| `.claude/skills/ww-panel-run-harness/SKILL.md` | `## 8. A property used as a doorbell carries a value that moves` and `## 9. A per-row bake gate carries each row's dependency, its mode, and whether it is a dial or a threshold` | the deaf save hook, the "suspect the instrument" rule, and the six things the per-row gate needed before it could be believed -- a dependency per row, the MODE that dependency must take, dial versus threshold, a named skip, a chain that can be two deep, and "force the defaults, never inherit QSettings"; 6,543 -> 11,110 B |
| `.claude/skills/nifskope-ww-panel-style/SKILL.md` | `## A hidden row reads as its default, and never overwrites what was typed` | with 57 new rows the panel now has enough target-specific rows that the `xraw()` / `xvar()` split has to be a written rule rather than a habit; 8,663 -> 9,679 B |

What the skills got RIGHT, and saved this lane from re-deriving:

* `nifskope-ww-panel-style`: one implementation of each control
  (`wwMakeScrubField`, `wwMatchFieldStyle`, `wwGuardWheel`), label + control with
  the explanation in the tooltip, one `label | field` grid per section. Fifty-
  seven rows went in without a single new widget helper and without a style
  change, which is the whole point of the page.
* `ww-panel-run-harness` §7, "delete the output folder before the run" and "turn
  the viewport preview OFF": the per-row gate bakes fifty-five times, and either
  trap would have made every bake after the first meaningless.
* `ww-test-harness-add` §5c, "a check-count floor is MEASURED, never predicted":
  121 and 130 are what the 16:19:01 and 16:36:41 exes printed, and both were
  watched failing on the rung first.
* `nifskope-ww-build-verify`: `make -n` printing zero compile lines, and the
  `git status --porcelain` sweep for a source newer than the exe, are what let
  this report say the exe carries this diff rather than assume it.
* `ww-module-off-is-identical` is the shape gate (a) borrowed: a change that is
  off by default has to leave the bytes alone, and the way to show it is the
  digest of the whole tree, not a spot check.
