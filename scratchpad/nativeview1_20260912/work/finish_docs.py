"""Close the lane's two hand-off documents with the session's measured numbers.

Written with the Write tool because it carries backticks and quotes -- mistake 10
of this lane's ledger is exactly the shortcut this file refuses to take.
"""
LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview1_20260912/'

# ---------------------------------------------------------------- WW_CHANGES
p = LANE + 'WW_CHANGES_ENTRY.md'
s = open(p, 'rb').read().decode('utf-8')

anchor = """Nothing found, nothing readable, or any plane other than Height: the build says
so in words and draws exactly the data view it always drew.
"""
add = """Nothing found, nothing readable, or any plane other than Height: the build says
so in words and draws exactly the data view it always drew.

One thing had to be learned the hard way and is worth writing down: the sheet's
normal map is an `_msn`, a MODEL-SPACE map, and a shader block that does not say
so lights the land about 40 percent too dark. The bake's own `.BTR` of the same
chunk is the authority for the flags it should carry -- Shader Flags 1
`0x80401000` (`Model_Space_Normals` set, `Specular` clear) and Shader Flags 2 `3`
(`ZBuffer_Write` with `LOD_Landscape`) -- and the sheet branch now sets exactly
those three bits. Measured against the `.BTR` of the same cells: mean absolute
colour difference 50.457 before, 35.821 after, with the two lumas correlating at
+0.6008 (a different chunk: +0.2023; the same chunk mirrored in Y: -0.0242).
"""
assert s.count(anchor) == 1
s = s.replace(anchor, add)

tail = """
### How it was checked

`tests/spells/native_open.sh`, whose right-hand side is the independent decoder
`tests/spells/lodgen_native_decode.py`, on chunk (-20,24) dim 4 of the look bake:

  * **no sheets, nothing changes** -- a height document from this exe is byte for
    byte the one from the exe before the change, three different ways of having
    no sheets; `lodl_open.sh` keeps its 23 checks at 0 failures; and the same
    one-chunk BAKE from both exes produces byte-identical `.BTO`, `.BTR`,
    manifest, `.lodi` and `.lodo`.
  * **the placements are the file's own** -- the manifest's 676 rows inside the
    chunk, the `.lodi` chunk table's 676 and the viewer's own census of 676 all
    agree, with the worst world position off by 0.0046 units against a floor of
    1; an empty region yields 0 and the harness sees 0.
  * **the lit terrain is the sheets** -- see the numbers above; an empty sheets
    directory renders the data view byte for byte, and the lit render is not the
    data view (mean difference 89.427).
  * **the scene against the chunk's own `.BTO`** -- coverage IoU 0.8179 against a
    pre-registered bar of 0.95: **this one is RED**. The refuter fires at 0.0001
    on a different chunk, so the test discriminates; the native scene is simply
    larger, containing 98.7 percent of the `.BTO`'s pixels plus more. Cause
    measured: the native view samples each base's own full-resolution LOD
    material while the `.BTO` samples the bake's downsampled object atlas, so
    alpha-tested canopies keep texels the atlas lost. The bar was not moved.

Whole run: 17 checks, 1 failure, 0 skipped.
"""
assert 'How it was checked' not in s
s = s.rstrip('\n') + '\n' + tail
open(p, 'wb').write(s.encode('utf-8'))
print('WW_CHANGES_ENTRY.md', len(s))

# ------------------------------------------------------------------ HANDOFF
p2 = LANE + 'HANDOFF_BLOCK.md'
h = """NATIVEVIEW1 (2026-09-12, DONE with one gate RED)

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
"""
open(p2, 'wb').write(h.encode('utf-8'))
print('HANDOFF_BLOCK.md', len(h))
