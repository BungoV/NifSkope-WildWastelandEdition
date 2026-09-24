# IMPOSTORFIX1 -- text for the director to splice. I edit neither file.

## For WW_CHANGES.md (new section at the top)

## The impostor card is a photograph again (2026-09-19, lane IMPOSTORFIX1)

The octahedral card drew as a spray of fragments (silhouette IoU 0.35 against
the mesh it was baked from). The director refused the previous lane's
explanation -- *"a single un-blended frame viewed from exactly its own bake
direction is a PHOTOGRAPH of the mesh and must overlap it almost perfectly"* --
and he was right: measured at the sixteen directions the frames were
photographed from, with the blend off, a card scores **0.8823**. The draw was
never the defect. Two others were, and both are repaired.

**The height channel outside the silhouette was the frame's average, not a
depth.** `lodgenDilateFrames` dilates every channel out from the silhouette and
floods the rest with the frame's mean; the drawer's parallax step then reads
that mean as a displacement and fetches colour from somewhere else on the
sheet. On the shipped blast_n4 sheets the uncovered texels decoded to +264
world units on average and +743 at the 95th percentile, against a card
half-width of 135 -- which is what turned a bare trunk into detached flakes.
`lodgenRepairOctHeight` (src/lodgen.cpp) now dilates the height out from FULLY
covered texels, writes that onto partially covered ones, and puts the card
plane outside the coverage floor, where the parallax is an exact no-op. Chosen
from a five-candidate table scored against a numpy reference card; the table
and the losers are in the code comment. **`depthSpan` was deliberately not
touched**: it is `GLView::glProjection`'s orthographic clip range written down,
not a free parameter.

**`cardOrtho` was written false unconditionally**, so an orthographic camera
was handed the perspective ray fan (up to 14 degrees of lean at a frame edge)
and the parallax stopped being the no-op the algebra makes it at a bake
direction. `res/shaders/impostor_oct.vert` now decides from the projection
matrix it already holds. Note this fix lives in a RUNTIME ASSET, not in the
exe.

Twenty-four orbit views, blend and parallax on, shipped -> repaired:
TreeMapleblasted05 N=4 **0.3546 -> 0.5038**, N=8 **0.4152 -> 0.6754**,
BlastedForestDestroyedTreeUpright01 **0.3957 -> 0.5721**, RockCliff02_Alt
0.7944 -> 0.7724, TreeMapleForest2 0.3206 -> 0.3545. Card-to-mesh ink ratio
1.63 -> 1.48, 2.21 -> 1.26, 1.56 -> 1.13, 1.12 -> **1.02**. On the distance
strip at azimuth 45 -- the three-frame blend's worst case -- 0.19 -> 0.29 at
256 px and 0.18 -> 0.34 at 16 px.

Gate `tests/spells/impostor_draw.sh`: 21 steps, 0 failures, floor raised 0.22
-> **0.35** on measurement (the old exe scores 0.2778 there, the new exe on old
sheets 0.3047, both halves right 0.4469). Two new rows, each watched failing on
the build it convicts: a sheet row that decodes the two BC3 sheets with no
application running and fails any covered texel whose height leaves the band
that frame's own whole texels occupy (shipped sheets: 16 frames of 16, worst
excursion 1530 world units; repaired: 0 of 16), and a photograph row that scores
the bake directions with parallax on and off and requires them EQUAL (old
shader: 0.7472 vs 0.8701).

Still wrong, and said plainly: flakes remain near the trunk base of the blasted
maple; TreeMapleForest2's crown is still a blob on a stick, because only 401 of
its 32,768 texels are fully covered, which is a photography-resolution problem
the height repair cannot reach; and RockCliff02_Alt lost 2.8%, because on a
solid object the old flood average happened to sit near its real depth.

## For HANDOFF.md (block for the director)

**IMPOSTORFIX1 landed, not committed.** Changed `src/lodgen.cpp`,
`src/impostorpreviewtest.cpp`, `res/shaders/impostor_oct.vert` (+ the deployed
copy), `tests/spells/impostor_draw.sh`, `MISTAKES.md`; new
`tests/spells/impostor_sheet_check.py`, `impostor_bc_decode.py`,
`impostor_bake_views.py`, skill `ww-reference-card-diagnose` in both skill
trees. Exe `release/NifSkope.exe` 23,353,856 B, 2026-09-19 14:03:07, sha1
`161568a58be437a679aff747a172de2084bc9275`; rung
`release/NifSkope.before_impostorfix1.exe` sha1 `88d6abb3...` taken once at
13:45:15. **The shader fix is a runtime asset** -- an exe copied without
`release/shaders/impostor_oct.vert` carries only half the repair. Report and
numbers: `scratchpad/impostorfix1_20260919/PENDING.md`; pictures in
`images/` beside it.

**EVERY BAKED `_oct_n.DDS` IN THE TREE IS STALE.** The repair is bake-side, so
sheets produced before this exe still carry the flood; run the sheet row over
anything shipped. The lane re-baked its own six fixtures only.

**RULING OWED -- the `_n` sheet's channel assignment.** Spec line 45 reads
`| _n | BC3 | normal X | normal Y | height | sway weight |`. BC3 gives ALPHA an
8-bit interpolated block and gives R/G/B one shared four-entry palette per 4x4,
so height sits in the coarse half and sway in the precise one. Swapping them
costs **zero bytes** and would cut the residual height error by roughly four
(after the repair: blast_n4 mean 2.2 levels = 26 world units, p95 5.1 = 62;
maple_n4 mean 4.1 = 49, p95 11.6 = 140), moving the same error onto sway, which
no parallax reads. It is a format-contract change, so the in-spec repair
shipped and nothing was changed silently. A yes costs a spec edit, a bake-side
encode change, a draw-side read change and a re-bake of every `_n`.

**Next defect, if someone wants it:** the maple's crown. `frameOf` downsamples
the photograph with a BOX average, which turns a fan of twigs into a grey blob
and leaves 401 of 32,768 texels fully covered. A nearest or median DEPTH filter
in that downsample (src/nifskope_ui.cpp) is the candidate; it is a photography
change and needs a full re-bake, so it is a lane of its own, not a follow-up.
