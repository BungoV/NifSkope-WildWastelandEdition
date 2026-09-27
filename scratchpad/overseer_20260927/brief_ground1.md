# Lane GROUND1 -- ground contact per vertex, not one grey per kit piece

Worktree: E:\Projects\NifskopeWWE-ground1, branch ground1-20260927 from night-20260927 @ 8f58e7db.
FIRST read the shared rules: C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\b560e4ec-6e66-4c21-9572-1ad4acca0043\scratchpad\night_rules.md
Report: scratchpad\ground1_20260927\DONE.md in the worktree.

## His words
"Ground contact on buildings, does that look right to you, it's a texture map that is not usable?"

## What is wrong (audit rank 2, section 2.1, picture 03)
- docs/LODGEN_VERTEX_PACKING.md:44 defines ground contact per VERTEX: 1 at the terrain surface, 0 by 256 world units
  above it (the stock path has it per vertex). The native .lodi stores ONE byte per placement (instance 0x12), an
  average: each kit piece is one flat grey. The audit's recompute matches the stored byte (corr 0.99993) and found
  24% of pieces span half the 256-unit fade inside themselves.
- Target look (offline): ...\scratchpad\audit1\pics\A2_ground_contact_per_vertex.png; today: A1_ground_contact_as_stored.png.

## The work
1. A per-vertex ground-contact stream in the .lodi, built the same way as the existing per-vertex streams (the
   vertex-AO stream, docs/LODGEN_NATIVE_LODO_LODI.md 4.8, and the sky stream 4.10): one byte per drawn vertex, the
   fade from the terrain height under that vertex (the same terrain the bake already samples for the placement byte).
   Format change: next .lodi version, header offsets from the reserved range (the doc lists what is free), reader
   refuses a malformed stream by name, doc section written. Keep the placement byte 0x12 (older readers).
   Another lane (IDENT1) may also touch the .lodi tonight but is told NOT to bump the version; you own the bump.
2. The viewer's `ground` channel draws the stream when present (per vertex), the placement byte when not, and its
   note line says which.
3. Gates:
   - off (env/command-line, gate only) = night-20260927's Boston bake byte for byte;
   - the stream agrees with an independent recompute (the audit's method, audit1 DONE.md 2.1) within 1 level on
     >= 99.9% of vertices;
   - physical: vertices within 16 u of the terrain read >= 250; vertices more than 256 u above read 0 (counts);
   - the stream's mean per placement equals the old placement byte within 2 levels (the byte was its average);
   - file size cost on the Boston box and the whole-map estimate;
   - the FO4CS-side change listed in DELIVERABLE_TEXT (a reader that skips the new stream must still read the file).
4. Pictures, each full size with its title bar, maps1 Boston camera: ground contact as stored today (renderer, from
   night-20260927's bake); ground contact per vertex (renderer, your bake); a 2x close crop at the ballpark and at
   one tower foot, per vertex.
