## HANDOFF text

TINT1 (2026-09-25, branch tint1-20260925, not merged):
- Object library re-baked at `.lodo` v5, which keeps vertex colour: 52,925 colour rows, 166 meshes.
- Installed at 20:13 into mods\FO4CSLOD\...\Commonwealth: .lodo, .lodi and 15 legacy _n arrays. The .lodb was kept (BAKE1's). Backups and sha1s are in scratchpad/tint1_20260925/replaced + install_record.tsv.
- FO4CS ImprovedLOD refuses v5 cleanly ("version 5; this reader knows 4"; module ships off). Teaching it v5 is owed, and comes last.
- Census over 184,431 placements:
  - 16.3% carry colour + the Vertex_Colors flag.
  - Only 3.5% carry a real hue, and almost all of that is trees (TreeBlasted02_LOD_1 alone has 4,055 placements).
  - Building LOD models carry no hue: not in vertex colour, not as a palette flag, not in the stock .bto.
  - So the grey buildings bungo sees are NOT fixed by v5. Next place to look: the atlas diffuse textures and whether they resolve.
- The viewer needed 61d920ab to draw the v5 colour; merge it with SEAM1's commit.
- A headless lit picture inherits the Vertex Color option, so use WW_LODL_AO=1 for colour pictures.

## WW_CHANGES text

- Native far field (src/lodinative.cpp):
  - A bucket whose `.lodo` v5 mesh carries a colour stream now gets the vertex colour field in its layout.
  - Before this, the library colour was written into a layout with no colour field, and the viewer drew those meshes white.
  - Measured with a doctored all-red library: 185,943 red pixels in a flat render after the fix, 0 before.
  - Spells on the fixed exe: lodl_channels 54/0 and lod_generation 128/0.

## MISTAKES text

2026-09-25 TINT1: I read "v4 and v5 render byte-identical" as a viewer defect for about an hour. I patched the bucket layout, which was a real defect, and still got 0 px changed.

The real gate was that the lit path multiplies vertex colour only under Scene::DoVertexColors, and a headless run inherits that option from the saved UI state.

The cheap test that settled it came last: a doctored file with every colour row set to pure red, rendered flat, lit, and lit with WW_LODL_AO=1. It should have come first.

Rule: when a payload does not show, doctor the payload to an extreme first, then find which switch hides it.
