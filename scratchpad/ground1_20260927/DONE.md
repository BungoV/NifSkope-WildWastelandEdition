# Lane GROUND1 -- ground contact per vertex (2026-09-27, started 04:35)

Worktree E:\Projects\NifskopeWWE-ground1, branch ground1-20260927 from night-20260927 @ 8f58e7db.

## 1. Skills loaded
- nifskope-ww-lodgen (build line, CLI, gates)
- search-lean (scoped searches only)
- nifskope-ww-worktree-build (section 5b: water1 was a clean built sibling at 8f58e7db; objects copied from it)
- nifskope-ww-build-verify (gated in-tree build `tools/ww_build.sh`, exe-newer, strings in the exe)
- ww-module-off-is-identical (off switch, red comparator, rung exe, harness fleet for a new default)
- (render/picture skills listed in section 6 when used)

## 2. What was wrong (audit rank 2, section 2.1, picture 03)
- The ramp is defined per vertex: 1 at the terrain, 0 by 256 world units above it (docs/LODGEN_VERTEX_PACKING.md:44).
- The native .lodi keeps ONE byte per placement (instance offset 0x12), the average of that ramp over the piece.
  Every kit piece draws as one flat grey.
- Audit numbers: 11,416 placements, 213,939 vertices; stored mean 29.85 vs recompute 29.80, corr 0.99993
  (the byte is the correct average); 24% of placements span at least half the 256-unit fade inside themselves.

## 3. What changed
Format change: `.lodi` version 12 = version 11 plus a per-vertex ground-contact stream.
- Header 0x130 (u64) = stream offset, 0x138 (u32) = stream bytes; 0x13C..0x1FF stay reserved (zero).
- Stream = `u32 first[instanceCount+1]` then one byte a vertex, the SAME population as the vertex-AO stream
  (bases[baseId].rep[mnamSlot], the mesh's vertex order). Written last, 4096-aligned, folded into indexCrc32
  after the sky stream. Byte = round(255 * clamp(1 - (z - ground)/256)), ground = bilinear ESM height under
  the vertex (the stock CONTACT_RANGE law, src/lodgen.cpp).
- Placement byte 0x12 kept unchanged (older readers).
- Version 12 is written only when the stream is written; the way back is `WW_LODGEN_NO_VERTEX_GROUND=1`
  (env, gate only), which writes exactly what night-20260927 writes.

File by file:
- `src/lodifile.h`: LODI_VERSION_VERTEX_GROUND = 12; header words offVertexGround/vertexGroundBytes;
  LodiSrcInstance::vertexGround; LodiSrcSet::vertexGround; LodiTable vertexGroundFirst/vertexGround;
  write stats.
- `src/lodifile.cpp`: writer (refuses a stream without AO, a slice length that differs from AO, and a stream
  on a pre-v7 file; payload last; CRC fold); reader (accepts 12; refuses by name: v12 with a zero offset or
  a stream too small for its offsets, a v7..v11 file carrying words at 0x130..0x13B, non-monotone offsets,
  an end offset that does not end the stream, a slice that disagrees with the AO slice); describe output.
- `src/nativeemit.cpp`: fills the stream in the AO pass's final receiver loop from the receiver's own placed
  vertex position and the chunk's ground field; census clause "vertex ground contact ON: ..." in the bake
  line (only when the stream is on).
- `src/lodinative.cpp`: viewer channel `ground` draws the stream per vertex when present, the placement byte
  when not; the note line says which.
- `tests/spells/lodgen_native_decode.py`: reads v12 with the same refusals.
- `tests/spells/lodgen_native_fields.py`, `tests/spells/near_format_selftest.py`: version lists taught 12
  (a far file may now be 12; 11 stays near-only); new check j0d = stream present exactly at version 12.
- `docs/LODGEN_NATIVE_LODO_LODI.md`: title v12, version row, header table rows 0x130/0x138, section 4.16,
  viewer table `ground` row.

## 4. Gates
Exes: rung = night 8f58e7db build (md5 2411b228bbc308f14b8d934d46eef3b4, water1's `NifSkope.before_water1.exe`);
new = this branch (md5 8501f7f5df32a8132789350b4c07a1a8, 25,915,392 B, linked 04:50:27).
Bakes: Boston box -8 -12 3 -1, AO2's switches (`bake.sh`), `WW_SETTINGS_SCOPE=ground1`. Measured by `gates.py`.
Population: 46,205 placements / 999,977 vertices measured; 327 skipped (the mesh slot is ambiguous: two reps with
the slice's vertex count); 23,274 empty slices (card-drawn or no land -- they draw 0x12).

Bake census (on): `vertex ground contact ON: 46532 placements streamed (1005166 bytes, mean 41.1, 100766 at the
terrain = 255, 771502 at 256 u or more above it = 0, 12363 placement(s) spanning half the ramp or more)`.
The off bake's .lodi is version 7, zeros at 0x130, no census clause.

| gate | result |
|---|---|
| off == night bake, byte for byte | IDENTITY_PENDING |
| independent recompute within 1 level | **99.9984%** (16 vertices off by more than 1, max 7); 99.62% exact. PASS (>= 99.9%) |
| more than 256 u above the terrain reads 0 | **768,197 of 768,197**. PASS |
| within 16 u of the terrain reads >= 250 (as briefed) | **14,464 of 20,827**. FAIL -- by the law, not the code: 16 u above reads 255*(1-16/256) = 239; >= 250 holds only within ~5 u |
| the same, taken from the law | 0..5 u above -> >= 250: **101,893 of 101,893**; 0..16 u above -> >= 239: **10,165 of 10,165**. PASS |
| placement stream mean vs its 0x12 byte within 2 | **98.35%** (761 placements outside); within 4: 99.30%; mean abs diff 0.1955; max 15.0; correlation 0.999917. FAIL as briefed |
| red: old byte broadcast over each placement's vertices | recompute 34.9% within 1 (FAIL), >256 u -> 0 FAIL, law FAIL. The gates can go red |
| red: stream + 2 | recompute FAIL |
| decoder refuses a bad v12 by name (`redread.py`, CRCs re-sealed) | good ACCEPTED; 7 of 7 doctored copies REFUSED by name: v12 offset 0, v12 no stream, v11 carrying 0x130, pad 0x13C non-zero, slice length != AO slice, end offset short, offsets not monotone |
| C++ reader (`--native-verify`) on the same copies | CPP_PENDING |
| harness `lodgen_native_fields.py` | on and off give the same 40 checks / 6 failures / 2 skips; j0, j0b, j0c, j0d ok on both (on: v12 with stream; off: v7 without). The 6 failures (e1/g1 no mesh report given, f3, h1/h2/h4b ladder) are this bake's switches, identical on both arms |

Why the placement-mean gate misses on 761 placements: the 0x12 byte averages the stock `.BTO` ring vertices of the
placement; the stream is the `.lodo` library mesh's vertices. Different vertex sets, same law. 756 of the 761 span
at least 128 levels (half the ramp) inside themselves, and they are mostly tall trees (TreeMapleForest1 122,
TreeMaple06Green 92, TreeMapleblasted04 83, ...), where a different vertex distribution moves the mean most.
The stream agrees with the independent recompute on its own vertices (row 2), so the byte is what differs.

Size: Boston .lodi 5,089,578 -> 6,375,722 B (+1,286,144 B, +25.3%); the stream is 1,284,394 B = the AO stream's size.
Whole Commonwealth estimate: the ao2 whole-map .lodi is 34,371,499 B with an AO stream of 13,866,923 B, so about
+13.87 MB (+40.4%).

## 5. Commits
(in progress)

## 6. Pictures
(in progress)

## 7. What is still not right
(in progress)

## 8. Skill review
(in progress)
