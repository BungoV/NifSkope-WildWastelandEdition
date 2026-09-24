STATUS: BUILT AND MEASURED (lane BUILD4, 2026-09-10). `qmake` rc 0, `make -j2`
rc 0 (`Nothing to be done` -- lane WATER2's 01:01:04 link already carried this
code, confirmed at the object level, `nifskope_ui.o` 00:44:24 against
`src/nifskope_ui.cpp` 00:38:50). 0 stale over all 15 changed files; stylesheet
in step.

**The harnesses.** `lodgen_octahedral.sh` **100 ok, 0 FAIL**;
`lodgen_card_arrays.sh` **35 ok, 0 FAIL**; and the two floors did not move --
`lodgen_impostor_cards.sh` **12 ok**, `lodgen_identity.sh` **8 ok**. CARDFINAL's
gap, mip and per-frame laws sit inside that 100 and are now measured rather than
expected.

**The camera reaches the format.** Bake 1's sidecar says it was photographed
orthographically and its read-back is `orthofit 972.833 972.833 0` -- asked and
achieved agree EXACTLY and the projection field is 0. The perspective control
says `persp` on the same line, so both the switch and the line move. The card
array carries `projection` per LAYER and one array holds both states:
`{'0004a074': 'ortho', '0004a075': None}` -- the layer whose sidecar named no
camera carries no key at all, which is what gives absence its defined meaning.

**THE CUBE PROOF PASSES AGAINST THE PRE-REGISTRATION AS PRE-REGISTERED.** All 64
frames of the 512-unit cube are within **1.78** texels of the table the harness
recomputes at run time and within **1.87** of the FROZEN table in
`prereg_cube.md` (bar 2); those two tables differ by at most 0.10 texels, so the
recomputation did not loosen the bar. Central asymmetry **0.000** and near-edge
vs far-edge width difference **0.000** (bar 0.05 each). The `WW_IMPOSTOR_PERSP=1`
control exceeds every one: **18.70** texels (23.19 against the frozen table),
**0.495**, **0.850**. The fixture's own size, measured through a DIFFERENT code
path (`WW_RENDER_ORTHO`), is **256.5097** units against 256 +- 3.

**The library was re-baked**: 19 baked, 0 failed, **19 of 19** card sidecars
saying `projection ortho`, **0 of 19** `orthofit` lines read through a
perspective camera, **18 of 18** card `.lodm` files carrying
`projection: "ortho"` and none absent. The FO4CS sample set was regenerated on
it: **38 card entries, every one ortho, no `(absent)` count at all.** (`run.sh`
reports `19 of 20` because its denominator counts `cards/library.txt`, the run
manifest, which has no camera and correctly no line.)

**THE TRANSITION GATE MISSES: 1 of 12 card rows**, against a pre-registered 12
of 12 -- centre within one card texel AND both extents within 2%, over 3 trees x
2 axis views x 2 distances. The zeroed-offset control passes **0 of 12**, so the
measurement is sensitive to the thing it names. The row that passes is
`0004a074` front at the ring distance (centre **0.81** texels, extents 0.92% and
0.43%); four more meet the centre bar and are turned away by an extent, the
sharpest being `00038599` right at mid, **0.15** texels off centre with 18.70%
on dx.

**What the trunk-width half says, and it is the honest half.** The card's HEIGHT
matches the source model to **0.00% on all three trees** -- the vertical world
scale, which is what the orthographic camera was changed to fix, is right. The
WIDTHS are 18.14% / 65.62% / 44.94% wider at the trunk and 90.03% / 120.54% /
37.88% wider at the crown, and the three pictures
(`scratchpad/cardortho_20260910/cardortho_transition_*.png`, all opened) show a
filled blob where the mesh is lacy. That is the shape of coverage-threshold
dilation at a 128-texel frame, not of a projection error -- named as a candidate
with its discriminator (re-measure at two frame sizes; dilation scales with the
texel, a projection error does not). It is NOT stated as the cause.

**And a finding about the instrument** (CONSTITUTION 4, rule 1): comparing every
card row with its own control, the centre column differs on **12 of 12** rows,
the dy extent on 4 of 12, and the **dx extent on 0 of 12**. The control therefore
fails only through the centre; the 2% extent bars -- which turn away four of the
eleven failing rows -- have no floor under them yet.

**Nothing was fixed and nothing was re-pinned** (a resuming lane measures a
failure and stops). NOT COMMITTED; bungo's open window needs a restart.
