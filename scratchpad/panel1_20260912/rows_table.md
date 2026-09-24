| row | section | switch | default | kind | gate (b): does the bake move? |
|---|---|---|---|---|---|
| Tile content | vt | `--vt-content` | 256 | whole number | not exercised here: the terrain virtual texture module is off in this gate module set -- the pyramid is worldspace-wide, not one chunk; tests/spells/lodgen_terrain_vt.sh reads the pyramid rows |
| Tile border | vt | `--vt-border` | 8 | whole number | not exercised here: the terrain virtual texture module is off in this gate module set -- the pyramid is worldspace-wide, not one chunk; tests/spells/lodgen_terrain_vt.sh reads the pyramid rows |
| Tile mips | vt | `--vt-mips` | 2 | whole number | not exercised here: the terrain virtual texture module is off in this gate module set -- the pyramid is worldspace-wide, not one chunk; tests/spells/lodgen_terrain_vt.sh reads the pyramid rows |
| Tile compression | vt | `--vt-compress` | 0 | selector | not exercised here: the terrain virtual texture module is off in this gate module set -- the pyramid is worldspace-wide, not one chunk; tests/spells/lodgen_terrain_vt.sh reads the pyramid rows |
| Carry a height layer | vt | `--vt-height` | false | tick | not exercised here: the terrain virtual texture module is off in this gate module set -- the pyramid is worldspace-wide, not one chunk; tests/spells/lodgen_terrain_vt.sh reads the pyramid rows |
| Ground cover in the colour layer | vt | `--vt-cover-in-color / --vt-cover-in-mask` | false | tick | not exercised here: the terrain virtual texture module is off in this gate module set -- the pyramid is worldspace-wide, not one chunk; tests/spells/lodgen_terrain_vt.sh reads the pyramid rows |
| Water subdivision | Terrain | `--water-subdiv` | 3 | whole number | MOVES the bake (15 files, 50975596 bytes) |
| Texture repeat | Land detail | `--land-tiling` | 341.3333 | number | MOVES the bake (15 files, 50965198 bytes) |
| Sample rule | Land detail | `--land-sample` | 0 | selector | MOVES the bake (15 files, 50965198 bytes) |
| Detail over the average | Land detail | `--land-detail` | 0.0 | number | MOVES the bake (15 files, 50965198 bytes), with landSample on |
| Hex tile size | Land detail | `--land-hex` | 0.0 | number | MOVES the bake (15 files, 50965198 bytes) |
| Warp amplitude | Land detail | `--land-warp` | 0.0 | number | MOVES the bake (15 files, 50965198 bytes) |
| Warp lattice | Land detail | `--land-warp-lattice` | 1024.0 | number | MOVES the bake (15 files, 50965198 bytes), with landWarp on |
| Warp octaves | Land detail | `--land-warp-octaves` | 1 | whole number | MOVES the bake (15 files, 50965198 bytes), with landWarp on |
| Mip bias | Land detail | `--land-mip-bias` | 0.0 | number | MOVES the bake (15 files, 50965198 bytes) |
| Guide rule | Land detail | `--land-guide` | 0 | selector | MOVES the bake (15 files, 50965198 bytes) |
| Guide strength | Land detail | `--land-guide` | 1.0 | number | MOVES the bake (15 files, 50965198 bytes), with landGuide on |
| Guide scale | Land detail | `--land-guide-scale` | 1024.0 | number | MOVES the bake (15 files, 50965198 bytes), with landGuide on |
| Guide slope reference | Land detail | `--land-guide-slope` | 0.5 | number | MOVES the bake (15 files, 50965198 bytes), with landWarp on, landGuide at 4 |
| Fine detail from | Land detail | `--land-detail-source` | 1 | selector | MOVES the bake (15 files, 50790406 bytes) |
| Vanilla LOD root | Land detail | `--vanilla-lod-root` | QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ) | path | not bumped (a path, not a setting) |
| Crevice shading | Land detail | `--land-shade` | -3.242 | number | MOVES the bake (15 files, 50965198 bytes) |
| Colour grade | Land detail | `--grade` | 1.0 | number | MOVES the bake (15 files, 50965198 bytes) |
| Quadrant edges | Land detail | `--blend-edges` | 0 | selector | MOVES the bake (15 files, 50965198 bytes) |
| Quadrant margin | Land detail | `--blend-margin` | 128.0 | number | MOVES the bake (15 files, 50965198 bytes), with blendEdges on |
| Strength | Erosion | `--erosion` | 0.0 | number | MOVES the bake (15 files, 50965198 bytes) |
| Rounds | Erosion | `--erosion-iterations` | 1 | whole number | MOVES the bake (15 files, 50965198 bytes), with erosion on |
| Seed | Erosion | `--erosion-seed` | 1 | whole number | MOVES the bake (15 files, 50965198 bytes), with erosion on |
| Sheet format | Sheets and cache | `--sheet-format` | 0 | selector | MOVES the bake (15 files, 51139990 bytes) |
| Normal cache folder | Sheets and cache | `--msn-cache` | QString() | path | not bumped (a path, not a setting) |
| Full cover at | Ground cover | `--cover-full` | 96.0 | number | MOVES the bake (15 files, 50965198 bytes) |
| Diffuse detail kept | roads | `--road-detail` | 1.0 | number | not exercised here: the road pass writes nothing on this chunk -- the command line own --no-roads moves only the ledger here (measured 2026-09-12); tests/spells/lodgen_roads.sh reads the road rows |
| Verge painted as road | roads | `--road-ground-paint` | 1.0 | number | not exercised here: the road pass writes nothing on this chunk -- the command line own --no-roads moves only the ledger here (measured 2026-09-12); tests/spells/lodgen_roads.sh reads the road rows |
| Cover suppressed under | roads | `--road-cover-suppress` | 1.0 | number | not exercised here: the road pass writes nothing on this chunk -- the command line own --no-roads moves only the ledger here (measured 2026-09-12); tests/spells/lodgen_roads.sh reads the road rows |
| Pieces combine by | roads | `--road-composite` | 0 | selector | not exercised here: the road pass writes nothing on this chunk -- the command line own --no-roads moves only the ledger here (measured 2026-09-12); tests/spells/lodgen_roads.sh reads the road rows |
| Paint raised road families | roads | `--road-raised / --no-road-raised` | false | tick | not exercised here: the road pass writes nothing on this chunk -- the command line own --no-roads moves only the ledger here (measured 2026-09-12); tests/spells/lodgen_roads.sh reads the road rows |
| Paint sidewalks | roads | `--road-sidewalks / --no-road-sidewalks` | false | tick | not exercised here: the road pass writes nothing on this chunk -- the command line own --no-roads moves only the ledger here (measured 2026-09-12); tests/spells/lodgen_roads.sh reads the road rows |
| Strength | terrainObjAo | `--terrain-object-ao-strength` | 0.5 | number | MOVES the bake (15 files, 50965198 bytes), with terrainObjectAo on |
| Bridge gap | waterBodies | `--water-bridge` | 2 | whole number | MOVES the bake (15 files, 53580262 bytes), with waterBodies on |
| Near texels | waterBodies | `--water-near` | 64 | whole number | MOVES the bake (15 files, 53585048 bytes), with waterBodies on |
| Body samples | waterBodies | `--water-body-samples` | 0 | whole number | MOVES the bake (15 files, 52751466 bytes), with waterBodies on |
| Flow samples | waterBodies | `--water-flow-samples` | 0 | whole number | MOVES the bake (15 files, 53585048 bytes), with waterBodies on |
| Write the shore line | waterBodies | `--water-no-shore` | true | tick | MOVES the bake (15 files, 52265425 bytes), with waterBodies on |
| Velocity plugin | waterBodies | `--water-velocities` | QString() | path | not bumped (a path, not a setting) |
| Forested at | aggregate | `--aggregate-min` | 8 | whole number | not exercised here: aggregate sheets need a card library and this gate arms none, so the run has nothing to aggregate; tools/bake_impostor_cards.sh builds one and tests/spells/lodgen_card_arrays.sh reads it |
| Frame size | aggregate | `--aggregate-tile` | 64 | whole number | not exercised here: aggregate sheets need a card library and this gate arms none, so the run has nothing to aggregate; tools/bake_impostor_cards.sh builds one and tests/spells/lodgen_card_arrays.sh reads it |
| Views | aggregate | `--aggregate-views` | 8 | whole number | not exercised here: aggregate sheets need a card library and this gate arms none, so the run has nothing to aggregate; tools/bake_impostor_cards.sh builds one and tests/spells/lodgen_card_arrays.sh reads it |
| Build the distance ladder | Object modules | `--native-no-ladder` | true | tick | MOVES the bake (15 files, 47512270 bytes) |
| Build the occluder boxes | Object modules | `--native-no-occluders` | true | tick | not exercised here: no occluder box is found on this chunk -- the command line own --native-no-occluders moves only the ledger here (measured 2026-09-12); tests/spells/lodgen_native.sh reads it |
| Merge the chunk shapes | Object modules | `--merge / --no-merge` | true | tick | not exercised here: hidden under the FO4 Community Shaders target this gate arms, and a hidden row reads as its default by design; the stock-engine target is where it reaches the bake |
| Atlas format | Object modules | `--atlas-bc1` | -1 | selector | not exercised here: hidden under the FO4 Community Shaders target this gate arms, and a hidden row reads as its default by design; the stock-engine target is where it reaches the bake |
| Model threads | Run | `--threads` | 0 | whole number | reaches nothing |
| Chunk threads | Run | `--chunk-threads` | 1 | whole number | reaches nothing |
| Roads | Roads (section head) | `--roads / --no-roads` | true | tick | not exercised here: the road pass writes nothing on this chunk -- the command line own --no-roads moves only the ledger here (measured 2026-09-12); tests/spells/lodgen_roads.sh reads the road rows |
| Object occlusion in the terrain | Terrain object AO (section head) | `--terrain-object-ao / --no-terrain-object-ao` | false | tick | MOVES the bake (15 files, 50965198 bytes) |
| Water bodies in the landscape file | Water bodies (section head) | `--water-bodies` | false | tick | MOVES the bake (15 files, 53585048 bytes) |
| Aggregate impostor sheets | Aggregate (section head) | `--aggregate / --no-aggregate` | false | tick | not exercised here: aggregate sheets need a card library and this gate arms none, so the run has nothing to aggregate; tools/bake_impostor_cards.sh builds one and tests/spells/lodgen_card_arrays.sh reads it |

rows found: 57, of them with a gate verdict: 57
