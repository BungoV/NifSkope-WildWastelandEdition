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
(in progress)

## 5. Commits
(in progress)

## 6. Pictures
(in progress)

## 7. What is still not right
(in progress)

## 8. Skill review
(in progress)
