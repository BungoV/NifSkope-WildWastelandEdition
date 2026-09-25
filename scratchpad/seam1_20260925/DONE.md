BUILD PENDING -- lane SEAM1, worktree E:\Projects\NifskopeWWE-seam1, branch seam1-20260925 (from main ea0ca708)

Fallout4.exe was running at every check (11:59 through 13:32), so nothing has been built. Everything below is code on the branch plus measurements that did not need a build. mods\FO4CSLOD is untouched.

## Commits (nothing pushed, nothing merged)

| commit | what |
|---|---|
| c21eb26a | Sanctuary edge root fix, docs/LODGEN_TERRAIN_VT.md §2.5 step 4. A LAND quadrant with no BTXT, and an ATXT layer that names LTEX 0, now paint the engine's one default land texture (CommonwealthDefault01) instead of the chunk's dominant base. |
| a6e5e8de | `--vt-fill-vanilla` (CLI and panel, OFF by default). It blends ground that his LAND does not paint toward Bethesda's dim-4 LOD colour (§2.6). The vanilla sheets are read loose at bake time, never shipped: `<root>/Textures/Terrain/<WS>/<WS>.4.<x>.<y>.DDS`. |
| 62e53a3b | `.lodo` v5, the optional per-vertex colour stream (W4, bungo's ruling). The stream is written only for shapes with a colour channel AND Vertex_Colors. RGB and A stay separate channels, and the viewer applies the colour only there. |

## Findings

- **Sanctuary edge.**
  - The edge is in our file, on the cell grid: block x -20..-16, y 20..24, which is one dim-4 chunk.
  - C1 (plugins) is out: control b, Fallout4.esm alone, has the edge. Ground cover is out too: control c, `--cover off`, has it.
  - C4 is out: the edge is in the texels.
  - The cause is our colour law (C2), §2.5 step 4.
  - Model replay of the step across the block border:
    - shipped law: 18.9 lum
    - engine default: 0.7 to 4.8 lum
    - neighbouring cells: about 1.3 to 1.9 lum
- **W1-W3 share the Sanctuary root.**
  - W1: 2023 of 2304 dim-4 chunks have LAND with no BTXT and no ATXT. The shipped VT paints 2016 of them one flat grey (132,128,132). By the FG1 definition, colour SD below 2, 2019 chunks are flat.
  - W2: 1,058 cells hold placements (14,714 of them) but have no painted LAND. They stand on the grey fill.
  - W3: `pics/w3_overview_before_oblique.png` shows the grey ring around the painted land.
- **A2.**
  - `a2_grey_before.txt` lists the 2019 flat chunks.
  - Vanilla covers all 2304 chunks.
  - 0 chunks are missing .lodl geometry.
  - The after list waits on the build.
- **Fill, offline model (doc_fill.md).** At every region the fill adds no border step over the bar that the engine-default law does not already have. The step at the line is under the line bar (Sanctuary north 9.86 against a bar of 10.15). These are model numbers, not the built file.
- **W4, measured on the source.**
  - Of 1,086 Boston shapes, 28 carry a colour channel, and all 28 have Vertex_Colors.
  - None of the 28 has Vertex_Alpha. 4 have A below 255.
  - The gate instruments pass their self-test (`w4_synth.py`).
  - On bakes from the old exe the gate reads G1 GREEN and G2 RED, so the refuter holds.
- **Boston look (measurement only, nothing fixed).**
  - Vanilla's downtown object-LOD chunks have no vertex colour and no Vertex_Colors bit.
  - Their shader words are the ones our viewer writes, and their emissive is black.
  - Their atlas is near-grey at their own UVs: lum 80, sat 0.067.
  - So no LOD tint exists in the files. The candidate is the engine's weather light: the sun colour plus the DALC ambient.

## Out of reach in this lane

All post-build verification is still to do, because the game never closed. P1-P6 are code-complete, P5 included (the VHGT geometry under the fill). No Pn is out of reach in design.

## Resume when Fallout4.exe is down (in order)

1. Game gate, then build with skill nifskope-ww-worktree-build. Keep a copy of the pre-build exe: BAKE1's `run/release/NifSkope.exe` is the OLD exe.
2. Fill off vs the OLD exe on the rung controls: byte-identical (`controls.sh <exe> <tag> a b c`, then `g2_compare.py`).
3. G1 edge gate, `edge_gate.py <VT.2.lodt>`: RED on the shipped file, GREEN on the new one.
4. G2, `g2_compare.py <before> <after>`.
5. Whole-map bakes with and without `--vt-fill-vanilla --vanilla-lod-root "E:/Tools/Fallout 4/DataUnpacked/Data"`, then:
   - `fill_gate.py <off> <on>` (FG1-FG3)
   - `VT2=<on> fill_model.py` on the 3 regions in doc_fill.md
   - `a2_lists.py <on VT.2> a2_grey_after.txt`
6. W4: `w4_bakes.sh <new exe> new`, then `w4_gate.py w4/old w4/new` must give G1 and G2 GREEN. Also `tests/spells/lodgen_native.sh` (fixture + fields j0) and a Boston oblique render with the colour on.
7. After pictures, with a fresh WW_LODL_SHEET_CACHE per render:
   - `w3_overview.sh`, `edge_zoom.sh`, the Sanctuary before/after, `ctl_oblique.sh`
   - the vanilla tile beside ours, from the file
8. Only when gates 2-6 are green: re-bake into mods\FO4CSLOD.
   - First copy the files it replaces to `replaced/`.
   - List sha1 before and after.

## Pictures (untracked, pics/)

- `controls_side_by_side_oblique.png`: the three controls, perspective.
- `w3_overview_before_oblique.png`: W3 overview, before.
- `edge_zoom4x_before_oblique.png`: the edge zoomed 4x, before.
- `fill_model_sanctuary_north.png`: our tile beside vanilla's, from the model.
- `ao/`: the AO set.

## Skill review

- **Loaded:**
  - ww-artefact-localise
  - ww-texel-picture
  - ww-lodl-offline-census
  - nifskope-ww-render-shot
  - fo4-nif-vertex-channel-census
- **Wished for:** a skill that says which exe and which sheet cache a native render really used. The stale sheet cache cost one set of pictures.
- **Written:** ww-lodo-version-bump (E:\Projects\Claude\.claude\skills\ww-lodo-version-bump). It covers where the `.lodo` version is known, the layout that keeps a file without the stream at "old version but 0x04", and the strip gate with its self-test.
