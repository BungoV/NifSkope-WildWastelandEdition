# IDENT2 deliverable text (director splice)

## HANDOFF text
IDENT2 (2026-09-29): landed on branch ident2-20260929, not merged. What it adds:
- Named landmarks are one building group each, with no size cap. The list is `res/lodgen_landmarks.txt`
  (Diamond City, Hub tower east and west, Trinity Church). A line matches by LOD model path prefix, with an
  optional X/Y centre and radius. `--landmarks <file>` reads another list; `--landmarks none` turns it off.
  The census prints one `native-landmark:` line per landmark, and the group dump gets a `# landmarks` line.
- Occluder-box probe: a hit within 0.5 u of an open seam edge counts as a miss. The box as the file stores it
  (x 0.999) and the box grown 0.5 u a face must both pass. If every even shrink fails, one face moves in by a
  quarter or half voxel. Otherwise the building gets no box.

Gates (Boston bake, final exe 358b9abc5f93):
- Landmarks: east 1 (was 1), west 1 (was 4), Trinity 1 (was 1), Diamond City 1 (was 15; 165 pieces,
  0 foreign). Row houses stay 1 group of 54.
  - Red control, the same exe with `--landmarks none`: DC 15, west 4.
- Chunk line: Diamond City has one id in each chunk, 85 in (-2,-2) and 30 in (-1,-2). No other instance of
  either chunk shares them. One id across chunks would need a file-wide group word, which is a format change.
- Poke gate: PASS. 520 boxes, 0 over 1 percent (was 5 of 511), worst 0.0041.
  - Red controls: boxes grown 1.02x -> 230 over, 1.05x -> 427 over.
- Coverage: 0.560 -> 0.570; Hi-Z 0.728 -> 0.736.
  - The probe fix alone gives 0.588. The landmark rule takes 0.018 of that back at the Diamond City eye
    (one box for a ring-shaped group).
- Way back: main exe and final exe both run with `--identity-join proximity --occluder-fit piece`. SAME, 233
  files. Red controls: a flipped byte -> DIFF; a removed .lodo -> MISSING.
- lodgen_native.sh legs 4/13/13c: HARNESS_LINE
- Row-house far-shadow harness: not run. It is not cheap: it is fixed to chunk (4,-12) with cached cameras and
  tables.

Pictures: PICS_LINE

Owed:
- (a) File-wide group ids (a u32 group word or a chunk-crossing table), so Diamond City reads as one id.
- (b) More than one occluder box per landmark group, to win back the 0.018 at the Diamond City eye.
- (c) The row-house far-shadow check, whenever that harness is next set up for a new chunk.
- (d) bungo's look at the pictures; merge is his call.

## WW_CHANGES text
Already in the branch (top of WW_CHANGES.md, "Building groups: named landmarks are one group; occluder boxes
stay inside their walls").

## MISTAKES text
Already in the branch (MISTAKES.md "### IDENT2": heredoc backslashes eaten twice; use chr(92) or the Write/Edit
tools for any text with backslashes).

## Skills
- Loaded: nifskope-ww-worktree-build, ww-lodi-drawn-mesh, ww-gui-launch-silent-exit, deepseek-offload.
- Wished for: an occluder-box poke diagnosis skill. The procedure is in `boxdiag.py`: find the failing lattice
  points, then check the file lattice against the grown lattice and look for seam gaps.
- Written: ww-lodgen-landmark-add (E:\Projects\Claude\.claude\skills and E:\Tools\AISkills).
