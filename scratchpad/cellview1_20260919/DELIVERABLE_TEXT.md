# CELLVIEW1 -- text for the overseer to splice

This lane never edits `WW_CHANGES.md` or `HANDOFF.md`. Both blocks below are
ready to paste.

---

## For `WW_CHANGES.md`

### Open a whole exterior cell (2026-09-19, lane CELLVIEW1)

File > Open a `.wwcell` file -- one line,
`<plugin path>|<worldspace editor id>|<x>,<y>|<n>` -- and NifSkope builds the
whole N x N block of an exterior worldspace as a scene: every persistent and
temporary reference whose base has a model, at the reference's own position,
rotation and scale, with its FULL near mesh and its real materials, plus the
LAND terrain, the water plane and a cell grid. It is the Creation Kit's cell
view (bungo, 2026-09-19: "I think a prerequisite would be, to be able to view a
whole cell like the CK editor"), and it is READ ONLY -- there is no save path.

* `STAT`, `SCOL`, `MSTT`, `FURN`, `CONT`, `DOOR`, `ACTI`, `TREE`, `FLOR`,
  `LIGH` and in fact any base record that carries a `MODL`. Static collections
  are expanded to their parts.
* Initially-disabled references, and references whose enable parent runs the
  other way, are hidden by default; so are markers.
* Each distinct model is loaded ONCE however many times it is placed.
* Every placement is in a pick table with its own world bounding box, a CPU ray
  test and the flat Name|Value rows a panel would show -- form id, base, record
  type, editor id, position, rotation, scale, cell, layer, enable parent, LOD
  models. **That table has no reader yet**: the click handler lives in
  `src/glview.cpp` and the dock in `src/nifskope_ui.cpp`, so picking is built
  and not wired.
* One colour overlay at a time -- record type, XLYR layer, or which LOD slots
  the base fills -- with deterministic colours and a legend that counts each
  bucket.
* Headless: `WW_CELL_OPEN=<plugin>|<world>|<x>,<y>|<n>`, with `WW_CELL_DUMP`
  writing a full placement census for gates.

Measured, `release/NifSkope.exe` 2026-09-19 15:14:27: a wilderness cell
(-30,-30) builds in 846 ms from 74 references and 23 models; Sanctuary (-20,7)
in 1787 ms from 150 references, 255 placements and 123 models; a downtown cell
(5,-11) places 1578 parts from 395 models. Geometry is WELDED per material
rather than instanced, which needs no renderer change at all; the measured cost
on a 5x5 downtown is 365.3 MB welded against 96.6 MB unique-resident, and every
placement keeps its own transform in the pick table, so instancing remains
available as a later renderer change.

NOT implemented, and the census line says so by name rather than drawing silent
grey: `.lodi` identity-group colouring, and XCRI precombined meshes.

---

## For `HANDOFF.md`

**CELLVIEW1 -- whole exterior cell view -- LANDED, gate green, not committed.**
`release/NifSkope.exe` 23,504,384 B, 2026-09-19 15:14:27, sha1
`af4577556f2b80ee71a048c637cbe218643ee8d7`; the rung taken once beforehand is
`release/NifSkope.before_cellview1.exe` 23,367,168 B, 14:51:48, sha1
`68ffb42ff00754b09d0b9de3f2a802dde05d5d12`. **bungo's open window predates it --
the next launch of `release\NifSkope.exe` (15:14) is the one with the cell
view.**

`bash tests/spells/cell_open.sh` -> **PASS, 8 rows, 0 failures** (15:36:27):
wilderness 74 placements / Sanctuary 240 / downtown 1578, every world box
recomputed from the plugin and the NIFs by an independent reader and matched;
five named references picked back by form id (STAT, SCOL, MSTT, FURN, CONT),
5 ok; three overlays with 4 / 4 / 2 legend buckets. `--red` -> **PASS**
(15:38:20): the wrong euler convention moves 62 / 220 / 853 boxes and the log
names them.

New files: `src/cellview.{h,cpp}`, `src/cellpick.{h,cpp}`,
`tests/spells/cell_{census,tri_budget,open_check,five_refs}.py`,
`tests/spells/cell_open.sh`, `tests/fixtures/{empty,sanctuary}.wwcell`.
Hook-up (applied): `NifSkope.pro`, `src/esmdata.h` (`ESM_HAS_CELL_FIELDS`,
XLYR/XESP on `EsmRefr`, `edid` on `EsmLodBase`), `src/esmdata.cpp` (parse those,
widen the `MODL` branch past TREE/STAT without touching the STAT/TREE route or
`models[]`), `src/nifskope.cpp` (include, file-type row, `.wwcell` open branch).
`tests/spells/cell_census.py` gained one defaulted `types=` parameter; its own
census numbers are unchanged. Root `MISTAKES.md` appended by byte splice
(9820 -> 9902 CRLF, 0 bare LF, +5418 bytes).

**Owed / not done:** PICKING IS NOT WIRED -- `src/cellpick.*` is complete and
filled but `cellPickTableMutable()` has one caller in the tree (the builder) and
no reader, because the click handler is `src/glview.cpp` and the dock is
`src/nifskope_ui.cpp`, both other lanes' files; it wants a hook-up lane of its
own. Also `.lodi` identity-group overlay and XCRI precombined (both refuse by
name); a dedicated File > Open Cell... dialog; instanced rather than welded
draws. **Not ours, reported:** vanilla NIFs whose material name is an
absolute Bethesda build path (`materials/c:/projects/fallout4/build/pc/...`,
98 distinct in downtown) resolve to nothing and draw magenta -- that is
`lodgenReadAsset`, and it affects the LOD bake the same way. **Quirk for
whoever owns `src/glview.cpp`:** `WW_RENDER_VIEW` cannot select `ViewTop`,
because `glview.cpp:6450` maps `v == 0` to `ViewFront` while `ViewTop` IS 0.

Report and pictures: `scratchpad/cellview1_20260919/PENDING.md` and
`scratchpad/cellview1_20260919/images/`.
