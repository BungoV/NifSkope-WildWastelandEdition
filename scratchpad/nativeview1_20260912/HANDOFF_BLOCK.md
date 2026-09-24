NATIVEVIEW1 (2026-09-12, DONE with one gate RED)

bungo said the showcase pictures were still .btr, which is the legacy format, so
the viewer had to learn to open the .lod files themselves. It now does:

  * a `.lodi` opens as a built document -- the placements in that file plus the
    geometry in the `.lodo` beside it, one shape per (base, material), the same
    materials the .BTO carries. `WW_LODI_REGION`, `WW_LODI_LEVEL`,
    `WW_LODI_BOXES=1` for wire occluder boxes, `WW_LODI_DUMP` for the census.
  * a `.lodl` is drawn LIT when its `.lodt` sheets are beside it (or at
    `WW_LODL_SHEETS`): colour sheet as diffuse, `_msn` as normal. No sheets, or
    any plane but Height, and it is exactly the data view it always was.
  * `WW_LODL_OBJECTS=<file.lodi>` puts both halves in one document.

Built 19:45:31, release/NifSkope.exe 22,275,072 B, BUILD-RC=0, three relinks.
Rung kept at release/NifSkope.before_nativeview1.exe (22,154,240 B, 18:07:58) --
never delete it.

MEASURED, chunk (-20,24) dim 4, `tests/spells/native_open.sh` whole run
**17 checks, 1 failure, 0 skipped**:

  * (a) module-off identity PASSES three ways, `lodl_open.sh` keeps 23/0, and the
    same one-chunk bake from both exes is byte-identical (`.BTO`, `.BTR`,
    manifest, `.lodi`, `.lodo`).
  * (b) PASSES: manifest rows inside the chunk 676 == `.lodi` chunk table 676 ==
    viewer census 676, worst world position 0.0046 u against a 1 u floor; the
    empty region gives 0 and 0.
  * (c) **FAILS: IoU 0.8179 against the briefed 0.95.** Refuter fires at 0.0001.
    The native scene contains 98.7 percent of the `.BTO`'s pixels and more,
    because it samples each base's full-resolution LOD material while the `.BTO`
    samples the downsampled atlas. Bungo's call: two-sided containment test, or
    make the view sample the atlas. The bar was not moved.
  * (d) PASSES: NCC 0.6008 (bar 0.45) against a different chunk's 0.2023 and a
    Y-mirror's -0.0242; MAD 35.821 (bar 48); empty sheets renders the data view
    byte for byte, and the lit render is not the data view (89.427).

One finding for anyone touching sheet lighting: the sheet normal map is an
`_msn`, and the shader block must SAY so or the land is 40 percent too dark. The
bake's `.BTR` carries Shader Flags 1 `0x80401000` and Shader Flags 2 `3`; the
sheet branch now sets those three bits.

Pictures (nothing sent -- the director sends), in
`scratchpad/nativeview1_20260912/images/`:
`i_terrain_and_objects.png` 2078x2156 4,802,724 B;
`ii_objects_only.png` 2078x2122 2,048,830 B;
`iii_far_region_levels.png` 2078x1098 399,567 B. This bake carries no impostor
cards (`bake_look.sh` passes no `--impostors`), so none is shown, and panel (i)'s
legacy column is a stated composite of the `.BTR` and `.BTO` frames because the
bake keeps the two halves in two files.

Report: `scratchpad/nativeview1_20260912/lane_nativeview1_report.md`. Skill
amendment drafted, not spliced: `SKILL_AMENDMENT.md` (a built document's render
recipe, plus the measured `WW_RENDER_SIZE` width floor near 1024 on this exe).
