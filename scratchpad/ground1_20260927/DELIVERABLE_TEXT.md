# GROUND1 -- ledger text for the overseer to splice (2026-09-27)

Branch ground1-20260927 (from night-20260927 @ 8f58e7db). Commits: see DONE.md section 5.

## HANDOFF (NifSkope WW) -- one block

**GROUND1 (2026-09-27): `.lodi` version 12 = per-vertex ground contact.**
- The `ground` value used to be one byte per placement (instance offset 0x12): the average of a ramp that is really
  defined per vertex (1 at the terrain, 0 by 256 units above it). A wall with its foot in the ground drew as one grey.
- Now the `.lodi` also carries one byte per vertex (header 0x130 offset, 0x138 bytes), the same vertex population as
  the AO stream, written last and folded into indexCrc32 after the sky stream. The 0x12 byte is kept.
- ON by default. Way back (gate only): env `WW_LODGEN_NO_VERTEX_GROUND=1` writes the old file byte for byte.
- Viewer: the `ground` channel draws the stream when the file has it, the 0x12 byte otherwise; the note line says which.
- Cost: Boston box .lodi +1,286,144 B (+25.3%); whole Commonwealth estimate +13.9 MB (+40%) on a 34.4 MB .lodi.
- Doc: `docs/LODGEN_NATIVE_LODO_LODI.md` section 4.16. Gates and pictures: lane report `scratchpad/ground1_20260927/DONE.md`.
- Owed: the FO4CS reader change below; bungo's look at the two pictures.

## WW_CHANGES.md entry

### GROUND1 -- ground contact per vertex (.lodi v12)
- Distant objects now carry ground contact per vertex instead of one value per object, so a building fades from
  "in the ground" at its foot to "in the air" at its top instead of drawing as one flat grey.
- The `.lodi` moves to version 12 when it carries the new stream (Boston: +1.3 MB, about +25%).
- The viewer's `ground` channel shows the per-vertex values and says on its note line which source it drew.

## MISTAKES.md candidates (root file)
- The brief's physical gate said "within 16 units of the terrain reads >= 250". The ramp's own law gives 239 at 16 u
  above (255 * (1 - 16/256)); >= 250 holds only within about 5 u. The gate was measured both ways and reported;
  the law was not changed to fit the gate. Lesson: derive a physical threshold from the law before writing it
  into a brief.
- The brief's "each placement's stream mean equals its old 0x12 byte within 2 levels" compares two different vertex
  sets: the 0x12 byte averages the stock .BTO ring vertices, the stream is the .lodo library mesh. 98.35% of
  placements agree within 2; the 761 that do not are nearly all tall trees spanning half the ramp (see DONE.md 4).

## FO4CS reader change (owed; FO4CS is built last by standing order)
1. Accept `.lodi` version 12 in the version whitelist. Version 12 keeps every v9/v10/v11 flag bit meaning
   (scrappable, wide-scale, initially-disabled).
2. Header: 0x130 u64 `offVertexGround`, 0x138 u32 `vertexGroundBytes`; reserved zero from 0x13C to 0x1FF.
   A reader that checks reserved bytes must move its zero sweep start from 0x130 to 0x13C at version 12 only.
3. Bound the stream by 0x130/0x138. It is the LAST payload: every v11 offset is unchanged.
4. If the reader checks indexCrc32: fold the stream's bytes (offsets table + bytes, `vertexGroundBytes` long) LAST,
   after the sky stream.
5. To draw it: `u32 first[instanceCount+1]` then one byte a vertex; placement i owns bytes
   `[first[i], first[i+1])`, in the vertex order of `bases[baseId].rep[mnamSlot]` -- exactly the AO slice.
   An empty slice means "use the 0x12 byte". Value / 255 = ground-contact weight for that vertex.
6. A reader that does not draw it may ignore it entirely (after steps 1-4).
