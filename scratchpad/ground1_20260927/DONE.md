# Lane GROUND1 -- ground contact per vertex (2026-09-27, started 04:35)

Worktree E:\Projects\NifskopeWWE-ground1, branch ground1-20260927 from night-20260927 @ 8f58e7db.

## 1. Skills loaded
- nifskope-ww-lodgen (build line, CLI, gates)
- search-lean (scoped searches only)
- (more added below as they are loaded)

## 2. What was wrong (audit rank 2, section 2.1, picture 03)
- The ramp is defined per vertex: 1 at the terrain, 0 by 256 world units above it (docs/LODGEN_VERTEX_PACKING.md:44).
- The native .lodi keeps ONE byte per placement (instance offset 0x12), the average of that ramp over the piece.
  Every kit piece draws as one flat grey.
- Audit numbers: 11,416 placements, 213,939 vertices; stored mean 29.85 vs recompute 29.80, corr 0.99993
  (the byte is the correct average); 24% of placements span at least half the 256-unit fade inside themselves.

## 3. What changed
(in progress)

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
