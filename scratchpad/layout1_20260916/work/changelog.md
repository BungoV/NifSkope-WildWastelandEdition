## 2026-09-16 -- every FO4CS-target output lands under one root, `Data/FO4CSLOD/` (lane LAYOUT1)

**Exe** release/NifSkope.exe __SIZE__ B __STAMP__ (rung NifSkope.before_layout1.exe 22,356,992 B 20:21:17 kept).

bungo 2026-09-16 19:3x: *"The folder should be called FO4CSLOD maybe, so it'd be Data/FO4CSLOD, sound fine?"*. Under the
FO4CS target a bake now writes NOTHING outside `<mod folder>/FO4CSLOD/`: the landscape file, every `.lodt` level and its
index, the native `.lodo`/`.lodi` pair, the aggregate sets, the object texture sets and the kept `.BTO.manifest.txt`
sidecars all sit in `FO4CSLOD/<worldspace>/`, and the impostor card sheets -- shared by every worldspace -- in
`FO4CSLOD/Cards/`. The game-path strings written inside the files moved with them. The STOCK target is untouched, and so
are `--keep-bto`'s chunks, the far HeightMap DDS and every READ path.

One function composes the root (`src/lodgenlayout.cpp`) and the folder name is a string literal in that file alone; the
gate greps for a second spelling. A bake creates no folder it does not fill: no `Terrain/`, no `Textures/Lodgen/`, no
`meshes/` under the FO4CS target. Census: `layout <root>, N file(s), M outside`, read back from the paths actually
written, defaulting to `layout n/a (no FO4CS-target file written)`. The panel shows `FO4CSLOD\<ws>\` on a "LOD root" row
under the output field, and hides it under the stock engine.

**Gates** __GATES__

**Red, not this lane's:** __RED__

__EXTRA__
