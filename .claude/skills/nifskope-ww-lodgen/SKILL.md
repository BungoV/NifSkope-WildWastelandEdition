---
name: nifskope-ww-lodgen
description: Build, run and verify the NifSkope Wild Wasteland Edition LOD generator (E:\Projects\NifskopeWildWastelandEdition) -- the MSYS2 build incantation and its two traps, the exe lock, the lodgen CLI (.lodt writer/reader/verify/refresh-ao, native heightmaps with F4FX provenance, .btd import and probe, --dump-land), the byte-identity gates that stand in for "it works", the GUI harness rules (second monitor, one instance, log + PASS), and the editing traps (heredoc backslashes, CRLF, Qt keyword macros). Use for any change to src/lodgen.cpp, src/lodtfile.cpp, src/lodgenmanager.cpp, src/esmdata.cpp or their harnesses.
---

# NifSkope WW: LOD generation work

Repo `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, ALL of 2026-09-04/05 uncommitted by
bungo's "Not yet" -- never commit without his word. Read `HANDOFF.md` (top section) first; the
`.lodt` contract is `docs/LODGEN_BTD_FORMAT.md`.

**Mistakes go in `MISTAKES.md` AT THE REPO ROOT** (CONSTITUTION rule 2, ratified by bungo
2026-09-09): written the moment one is recognised, unprompted, newest at the top.
`docs/MISTAKES.md` is the OLDER engineering-trap ledger for the generator and the renderer -- read
it before working in those areas, but new entries do not go there. This line said the opposite
until 2026-09-09 (lane LODTOPEN2 found it): the root file was called a duplicate to be avoided,
which the constitution now overrides.

## Build (the only way that works)
```bash
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && make -j2 2>&1 | grep -E "error:|Error [0-9]" | head; echo BUILD-DONE'
```
* `git` is not on that PATH without the export -- the link step runs `git rev-parse` and fails
  with `Error 127` otherwise.
* A running `release/NifSkope.exe` (a 25-minute Appalachia conversion, a GUI harness) holds the
  exe: `make` fails at the link. Check `Get-Process NifSkope` first. For a long CLI run that must
  not block builds, `cp -r release <scratch>/ns_run` and run the copy.
* One build at a time; ~4 minutes; the Bash tool's effective timeout is 600 s -- run
  build+test chains with `run_in_background: true` and poll the task output.
* GATE THE CHAIN ON `make`'S OWN EXIT CODE, not on the grep after it: `make -j2 > build.log 2>&1;
  rc=$?; grep -E "error:" build.log; exit $rc`. A `make | grep | head` reports `head`'s status,
  the chain runs on, and the harness tests the PREVIOUS exe (2026-09-06). Read the exe's
  timestamp next to every harness verdict.
* `bash tools/ww_build.sh <sources>` is that whole gated chain as one IN-TREE script (game-up check,
  the exe renamed aside, `make -j2` on its own exit code, exe-newer-than-sources, the link-time
  copies); use it when the session executes only under the repo, as an account-B lane does, since
  such a lane cannot invoke `/c/msys64/usr/bin/bash` itself.

## The CLI (`release/NifSkope.exe -no-gui lodgen ...`)
| what | command |
|---|---|
| write `.lodt`, read it back, cross-check every plane vs the ESM | `lodgen <esm> --worldspace 3C --lodt <dir>` (exit 1 on any mismatch; prints a `timing:` line) |
| verify an existing `.lodt` against its source, no write | `... --lodt <dir> --verify-only` |
| recompute only the AO plane in place | `... --lodt <dir> --refresh-ao` (byte-identical to a fresh write) |
| native shadow heightmap with F4FX provenance | `lodgen <esm> --worldspace HEX --heightmap <dir>` (`--heightmap-size N` resamples) |
| Fallout 76 `.btd` -> `.lodt` (no plugin) | `lodgen --from-btd <file.btd> --lodt <dir>` (~25 min, 1.55 GB) |
| measure a `.btd`'s layouts in 30 s | `lodgen --from-btd <file.btd> --btd-probe` |
| every cell's full 33x33 VHGT, for offline rule tests | `lodgen <esm> --worldspace HEX --dump-land <file>` |
Worldspace IDs: Fallout4.esm 3C Commonwealth, F94 DiamondCity, F93 DiamondCityFX, 54BD5 Goodneighbor;
DLCCoast.esm B0F DLC03FarHarbor, 4EA4 DLC03VRWorldspace; DLCNukaWorld.esm 290F NukaWorld,
52931 NukaWorldAmphitheater, 53C58 NukaWorldMarket. `--objects` takes its OWN chunk coords.
FO4 corpus: `X:\Programs\Steam\steamapps\common\Fallout 4\Data\*.esm`; FO76:
`E:\SteamLibrary\steamapps\common\Fallout 76 Playtest\Data\Terrain\Appalachia.btd` (the Pitt one
has zero land textures and one colour: it cannot test alphas or colour).

**THE DEFAULTS MOVED ON 2026-09-12 (bungo's rulings, lane DEFAULTS1).** A bake with no switches
is not what it was, and a harness written before that date measures something else unless it
spells the switch:
* **Object identity is OFF** -- `--identity` is the opt-in, `--no-identity` still there. A default
  `.BTO` carries the plain descriptor `474989027590661`: no vertex colours (no object index, no
  AO, no sway), no UV2 layer, no Eye Data. Anything reading colour bytes as an id or AO needs
  `--identity` on its command line now.
* **Terrain identity is OFF** -- `--terrain-identity` is the opt-in. A default `.BTR` carries
  vanilla's `52776558133763`, neutral colours.
* **The land look** is hex 256 / warp amplitude 341 / mip bias -0.22 / guide `flatwarp:1.0`. The
  exact way back, byte for byte, is
  `--land-hex 0 --land-warp 0 --land-mip-bias 0 --land-guide off`. Lattice 1024, octaves 1, guide
  scale and slope did not move; the base rule is still FOOTPRINT.
* **`--road-ground-paint` is 0** (the grass planes inside the road NIFs are not painted into the
  road plane). `--road-ground-paint 1` is the way back. `--road-detail` is ALWAYS 1, never 0.
* The manifest sidecar, `--arrays`, the impostor cards and the native `.lodo/.lodi/.lodt/.lodl`
  no longer depend on the identity flag: they are written either way, byte for byte the same.
* Gate: `tests/spells/lodgen_defaults.sh` (phases a-e, `PHASES=` to pick, `RUNG=` the old exe).

**Terrain pyramid switches (lane VTNORMAL1, 2026-09-23/24).** Contract: `docs/LODGEN_TERRAIN_VT.md`
§2.1, §2.2b, §3.1, §5.
* `--vt-density 32|16|8` -- world units a texel at the finest level; names the pair 32 = dim 2 /
  content 256, 16 = dim 2 / content 512, 8 = dim 1 / content 512 (byte-identical to the
  `--vt-finest` / `--vt-content` pairs). Refused beside either of them. CLI default stays 32 (the
  old bytes); the panel row *Finest texel size* defaults to 16 and the *Tile content* row is gone.
* `--vt-half-aux` -- normal, mask, height and emissive sheets store only mips 1.. (header descriptor
  byte 6 `mipSkip`, no version bump); colour keeps its size. Refused with `--vt-mips 1`. Default OFF.
* `--msn-cache DIR|auto` now feeds the PYRAMID's normal as well as the chunk sheets (a chunk with no
  sheet keeps the heights normal; `vt:` census words `normalMsnCache / normalHeights / normalMixed /
  msnSheetsRead / msnSheetsMissing`). DIR may be a MOD ROOT, Data or Textures folder: the reader looks
  in DIR, then `Textures/Terrain/<world>/`, then `Terrain/<world>/`. `auto` = the LAST `--resource`
  folder (archives skipped) holding a `*.4.*_msn.DDS` wider than 512 px; census `msnCacheDir <folder>
  (auto)`. The panel's EMPTY row is `auto`, `none` is off (panel only). A miss is silent by design, so
  read the census before believing the sheets were used.

## THE TWO BAKE TARGETS, AND WHAT EACH ONE LEAVES ON DISK (2026-09-16, lane BTOFREE1)

There is no `--target` flag. **`--native <dir>` IS the FO4CS target** on the command line; the panel
picks it with `LodgenTargetBox` index 0 (`fo4cs()`). No `--native` at all is the stock engine target.

| target | what lands in the mod folder |
|---|---|
| **FO4CS** (`--native <dir>`) | `.lodl .lodt .lodo .lodi .lodm`, the texture / card arrays, the heightmap DDS, and `<chunk>.BTO.manifest.txt` |
| **stock engine** (no `--native`) | exactly what it always wrote: `.BTR`, `.BTO`, the sidecars, the atlas. **Byte-identical, and that is a hard gate** |

**The `.BTO` is SCAFFOLDING under the FO4CS target, not output.** Five passes read a chunk back --
texture arrays, atlas, `lodgenMergeChunkShapes`, `lodgenSimplifyFarRings`, card arrays -- and each of
them opens `<chunk>.BTO.manifest.txt` beside the file it is reading. So the bake builds the chunks in
`<mod folder>/lodgen_bto_scratch`, runs every read-back there, then moves the **manifests** into
`meshes/terrain/<ws>/` and deletes the chunks and the folder. One function does the teardown for both
front ends: `lodgenDropBtoScratch()` in `src/lodgenchunkpass.cpp`.

* **Way back:** `--keep-bto`, or the panel row *Keep legacy .BTO chunks* (`keepBto`, default OFF,
  shown under FO4CS only). Byte-identical to a bake from before 2026-09-16.
* **The census says which happened**, and `lodgen_btofree.sh` leg (d) reads the numbers:
  `bto built in scratch <dir>, N chunk(s), N dropped, B bytes freed` against
  `bto built in the mod folder, N chunk(s), 0 dropped, 0 bytes freed`.
* **Writing a harness that opens a `.BTO` from a `--native` bake?** Spell `--keep-bto` on that bake
  (`tests/spells/lodgen_native.sh` check 4 does) or read the **manifest**, which is kept either way
  (`lodgen_ladder.sh` does). A harness that just globs `*.BTO` after a default FO4CS bake finds
  nothing and reads like a writer defect.
* Gate: `tests/spells/lodgen_btofree.sh` -- legs (a) default drop, (b) `--keep-bto` == rung,
  (c) stock == rung, (d) the census clause. `RUNG=` names the exe the bytes are pinned to.

## The gates (what "works" means here)
* **Byte identity.** After a refactor the Commonwealth `.lodt` must hash the same
  (`cmp` against the previous file); the native Commonwealth heightmap must be byte-identical to
  `E:\Projects\Fallout 4 Mods\mods\FO4CS\Textures\Terrain\Commonwealth\Commonwealth_fine.HeightMap.-96.-96.95.95.-8320.44872.dds`
  (header, F4FX block, payload -- all 75,497,620 bytes).
* **Round trips, not statistics.** Heights exact vs the ESM (half a quantum + 8 float ulps for a
  `.btd`), alpha/colour/ground-cover words exact. A check that cannot fail on its input is not a
  check: Pitt "confirmed" an alpha invariant that Appalachia broke on 72% of samples.
* Harnesses: `tests/spells/lodt_write.sh`, `lodt_btd.sh`, `lodgen_terrain.sh`,
  `lod_generation.sh` (the workspace), `lod_channel_preview.sh`, `lodgen_impostor_cards.sh`.
  Run the ones the change reaches. Impostor candidates come from `--terrain-region X0 Y0 X1 Y1
  --list-impostor-candidates` and a card only substitutes at a FAR chunk (dim 16/32).
* **`lodgen_perf.sh` leg (c) assumes a rung from before 2026-09-17.** It hands the exe under test
  `--library near --native-ladder`; with a newer rung that is a different bake and (c) reads RED
  (`.lodo` 6.2 MB vs 223 MB, lane VTNORMAL1). Read both logs' `native-library:` lines before
  believing it; `scratchpad/vtnormal1_20260923/waybk.sh` is the equal-switch rerun (56 files, 0 differ).
* **The `.lodb` is never byte-equal across two exes** (exe size, clock, output paths, the census
  lines). A tree-identity gate excludes it and compares it with those lines dropped.

## GUI harness rules
`WW_WINDOW_AT=1960,40` (set by `tests/spells/_harness.sh`) -- second monitor, never
`SetForegroundWindow`; one NifSkope instance ever; `--port <unused>` (it exits silently on a bound
port); the app writes `release/ww_<name>_test.log` and the script greps `^PASS`. Self-tests live in
`src/nifskope_ui.cpp` behind `WW_<NAME>_TEST` env vars in the `WW_UNFUCK_TEST` family. Headless
renders: `WW_RENDER_SHOT=<png> WW_RENDER_FLAT=1 WW_LOD_CHANNEL=<1..7> WW_RENDER_SIZE=WxH`.

## Compile a harness's embedded Python BEFORE running the harness (2026-09-09)
A `tests/spells/*.sh` gate is mostly Python inside `<<'PYEOF'` heredocs. A SyntaxError in one of
those blocks does NOT stop the script: the block dies, its checks never run, the shell's `fails`
counter never moves for them, and the suite prints `RESULT FAIL` that reads exactly like a failing
check. One stray apostrophe cost a ten-minute run this way. Two seconds of gate first:

```python
import re, sys
s = open('tests/spells/<name>.sh', encoding='utf-8').read()
for i, blk in enumerate(re.findall(r"<<'PYEOF'\n(.*?)\nPYEOF", s, re.S)):
    try:
        compile(blk, '<block %d>' % i, 'exec'); print('block %d ok' % i)
    except SyntaxError as e:
        print('block %d line %s: %s' % (i, e.lineno, e.msg)); sys.exit(1)
```

Run it after every edit to a harness, and treat a suite whose `ok` COUNT DROPPED as a broken block
until proved otherwise -- the count is the tell, not the verdict. (47 ok where the run before had
68 was the signal; `RESULT FAIL` was not.)

## Editing traps (each cost a build)
* Heredocs halve backslashes and mangle `\n` inside Python; write patch scripts with the Write
  tool and run them, or build escapes from `bytes([92])` / `chr(92)`. **This includes an apostrophe
  escaped as `\'` inside a single-quoted Python string** -- the heredoc eats the backslash and the
  string ends early, twenty lines from where the error is reported. Four occurrences in one lane,
  2026-09-09; the rule is that no text carrying a backslash or an apostrophe goes through a heredoc
  at all.
* Python on Windows needs `C:/...` paths, not `/c/...`.
* **A spell that shells out to `python` measures whichever `python` is on the
  PATH, and MSYS2's has no `numpy`.** Run the harness chain through
  `MSYSTEM=UCRT64 ... /c/msys64/usr/bin/bash -lc` and `tests/spells/lodl_open.sh`
  reports `23 checks, 1 failures`: the failing line is
  `whole-worldspace render: 89019 bytes, coverage , luminance SD ` with BOTH
  numbers empty, above a `ModuleNotFoundError: No module named 'numpy'` in the
  traceback — which reads exactly like a render regression in the tree. The same
  exe from the Git-Bash shell, whose `python` is
  `/c/Users/bungo/AppData/Local/Programs/Python/Python39/python`, reads
  `coverage 0.0951, luminance SD 38.86` and 23/0. **Build the chain with the
  interpreter named**, or read an empty number as a missing module before
  reading it as a defect (2026-09-11, lane ROADS1).
* Measure line endings with Python byte counts (`b.count(b'\r')`); src is MOSTLY LF-only, but
  `src/glview.cpp` is mixed and mostly CRLF (22743 CR / 22814 LF): an anchor there must carry
  `\r\n`, and the CR count must grow by exactly the lines inserted. `WW_CHANGES.md` is mixed too --
  match neighbours, never normalise.
* `emit` and `slots` are Qt keyword macros: `auto emit = ...` and `quint16 slots[6]` both
  silently vanish. Name them `emitPlane`, `qslots`.
* An anchor that matches twice (probe and cross-check both call `getCellLandTexture`) needs a
  two-line anchor. Assert `count == 1` before every replace.
* A memo/cache that hands out pointers: fetch neighbours FIRST, the cell LAST (4-entry
  round-robin in `lodtWrite`'s ESM source).
* **Float contraction moves bytes** (lane VTNORMAL1, 2026-09-23). `src/lodgen.cpp` is compiled
  `-O3 -march=haswell`, where GCC fuses `a*b + c` into FMA by its own choice. An unrelated edit near
  a plain sum that is rounded to 8 bits can flip that choice and move a byte-identity gate by one
  step on a few texels (2 texels of a 2048 sheet; restoring the loop verbatim did NOT restore the
  bytes). Probe the rung's form over a whole sheet (`scratchpad/vtnormal1_20260923/fmaprobe.py` is
  the pattern) and spell it with `std::fma` -- `lodgenMsnEncodeAssembled` is the example.
* **A BC1 ceiling uses OUR encoder.** `lodgenEncodeBC1Block` picks min/max-LUMINANCE endpoints
  (0.299 / 0.587 / 0.114), so on a normal sheet north (blue) is nearly ignored: his sheet's best r
  through it is 0.9449 east / 0.8624 north, not a PCA fit's 0.97 / 0.925. Measure a bar against a
  port of the codec the bake runs before writing it; a numpy port is
  `scratchpad/vtnormal1_20260923/encceil.py`.

## Panel style (the docks' house rules; the LOD Generation panel shipped without them once)
* Sections: `wwHeading( text, parent )` from `wwskin.h`, never a `QGroupBox` title. A section that
  must grey as one thing is a plain `QWidget` with the heading inside it.
* Numbers: `QSpinBox`/`QDoubleSpinBox` + `wwMakeScrubField( s )` (or `WwNumberField`) from
  `ui/widgets/wwnumberfield.h`; nothing sweeps a dock for you, the sweep covers only the Settings panes.
* Selectors: `wwMatchFieldStyle( combo )`, same header, or a combo is a different species beside a number.
* Both helpers apply `wwGuardWheel()`: a field takes the wheel only while focused, so a scrolling panel
  never changes a value under the pointer. Any other field in a scroll area gets it by hand.
* Layout: one label | field `QGridLayout` per section, `setColumnStretch( 1, 1 )`, ONE setting per row,
  whole-word labels, the explanation in the tooltip -- never after a dash in the label. Muted hints:
  `wwSkinColor( "textMuted" )`.
* The `WW_LODGEN_TEST` self-test counts each of these with a floor (8 numbers, 4 headings, 5 selectors);
  keep the floors when adding controls, and add the same counts to any new dock's test.
* Organisation (2026-09-06b): three bands -- settings in a `QScrollArea`, map + bar on a `QSplitter`
  below it, summary + buttons pinned under both. An output is a `LodgenSection( check, key,
  expandedByDefault, parent )` (arrow folds, box enables, fold persists). `refreshSummary()` owns
  Generate's enabled state and the sentence beside it; the run reads `wantLodt()`/`wantHeightmap()`/
  `wantIdentity()`/`wantTerrainId()` (tick AND visible), never the box alone.
* A bake hook is gated by a REAL bake with its output measured (covered pixels, distinct values), never
  by synthetic inputs: the card matte was opaque for a month behind a synthetic-card harness. Setting
  `cfg.background` reaches nothing; `GLView::setBackground()` applies it under the context.
* `res/style.qss` reaches the app only as `release/style.qss`, copied by `QMAKE_POST_LINK` at link
  time: after a sheet edit without a relink, `cp res/style.qss release/style.qss`, and read the
  harness's contrast NUMBER -- it scored 0 on a stale copy and said so before the picture did.
* Ticked boxes are Blender's: `${toggle}` blue + `image: url(:/wnd/check.png)` (radio.png for radios),
  PNGs at 1x/2x under `res/icon/` (no SVG plugin ships). The self-test counts white mark pixels
  inside a ticked box, so a missing resource fails, not just a wrong colour.
* The output is the MOD FOLDER itself, one field (`outEdit`); `outputDir()` is the only way to the
  Data path. bungo said so twice -- a mods-root + name split was built and reverted 2026-09-06.
* Assets: every model/material/texture read in `lodgen.cpp` goes through `lodgenReadAsset()` -- loose
  `dataRoot` first (CLI `--data-root`), then `Game::GameManager::get_file` for textures/materials, and for
  `.nif` the generator's OWN `BA2File` index (`lodgenMeshArchives()`): the manager's Fallout 4 filter
  (`archiveFilterFunction_2`) drops every .nif at index time, so `get_file` can never return a mesh.
  The GUI passes an empty root; there is no "Game data" row. `-no-gui` never initialises the game
  manager (a QProgressDialog in its init), so CLI runs still need `--data-root`. The gate is byte
  identity of the NEAR chunk (-20,24) dim 4 built both ways, in `WW_LODGEN_TEST`.
  THE RESOURCE STACK (2026-09-06k): an ordered list of mod folders and archives in MOD ORGANIZER's
  order -- the LAST entry overrides the earlier ones, and a loose file beats an archive wherever the
  archive sits. `lodgenSetResources()` installs it; it is ONE `BA2File` fed backwards (that map is
  first-wins) in two passes, loose files then archives, and `lodgenReadAsset()` consults it before
  `--data-root` and before the game manager. An EMPTY stack is a no-op, which keeps the byte-identity
  gates honest. CLI: `--resource <folder|archive>` (repeatable), `--plugins-txt <file>`, `--mo2`, and
  the questions `--probe <relpath>` (which entry supplied it, loose or archived, size and sha1;
  `--probe-out` writes the bytes), `--print-source`, `--list-files N`. The panel's Source row picks
  Specified (plugins + a drag-ordered Resources list) or Mod Organizer 2 (detected by `usvfs_x64.dll`,
  refuses off it: launch NifSkope from MO2's executable list like FO4Edit). The card bake takes
  `WW_LODGEN_RESOURCES=a;b` or `WW_LODGEN_MO2=1`. Gate: `tests/spells/lodgen_resources.sh`.
  FAR RINGS (2026-09-06n): `lodgenSimplifyFarRings` runs LAST, after the merge, and cuts rings
  2 and 3 to `ratio16` 0.35 / `ratio32` 0.20 (`--no-simplify`, `--simplify8|16|32 R`,
  `--simplify-error UNITS`, panel row *Far-ring simplification*, both targets). Ring 0 is never
  touched — the byte-identity gates depend on it. The cut is per (identity index, UV2.y layer)
  group so no collapse crosses an object or a layer, meshoptimizer creates no vertices so no
  channel is interpolated, and every group is asked for ≥2 triangles and restored whole on a
  null result: **the identity set of a chunk is invariant under the pass**. Alpha-tested shapes
  and `C`-line cards are never cut. MEASURED: 0 of 19,507 refs in the ring-2 chunk (−32,16) and
  0 of 148,362 in the ring-3 chunk (−32,0) fill their ring's MNAM slot (456 and 51 bases in the
  whole plugin do), so a far ring is EMPTY without `--slot-fallback` (new on the CLI) or
  `--impostors`; vanilla ships 20 dim-16 chunks and 4 dim-32 for the whole Commonwealth against
  344 at dim 4, at 17.5 and 2.6 triangles a cell against 673.8. Vanilla's atlas diffuse is
  **DXT1** (4096x2048, 13 mips, 5,592,552 bytes), its `_n` and `_s` both BC5U — `--atlas-bc1`
  (the stock target's default) matches the diffuse; `lodgenWriteDds` needed `bc1Alpha` because
  its mip filter forced BC1 alpha opaque. New question: `lodgen --dump-geometry FILE.BTO`
  (per-shape weight + `segbad`/`outofchunk`/`sphereout`/`aabbout` + an `i` identity line).
  Gate `tests/spells/lodgen_farring.sh`; ring 3 opt-in with `FARRING_RING3=1`.
  EMISSIVE SCALE (2026-09-06k, EMIS1): THE EMISSIVE, BOTH HALVES (2026-09-06l). A legacy `_g` texel is the diffuse × its own alpha
  × the SOURCE'S EMISSIVE COLOUR (black where alpha-tested); the emissive MULTIPLE cannot go in
  an eight-bit sheet and rides in the `.lodm` as `emissiveScale` — top level on a source/card
  file, an `array.emissiveScale` list parallel to `layers` on an array one, absent = 1. It is
  the source's multiple where the source own-emits with a colour that is not black, else 0, and
  a source `.lodm` that supplied the emissive picture supplies the multiple with it. The chunk
  shape carries the source's `Emissive Color`, `Emissive Multiple` and Own-Emit bit (bucket key
  AND merge key); the bake's channel 13 multiplies by `lodEmissiveColor`, which the renderer
  writes UNCONDITIONALLY because the bake photographs with lighting off; the meta line is
  `emissive <scale> shapes <n>`; the arrays sidecar is version 5 with `emissiveScale` on the
  END. `lodgen --dump-shapes <file.BTO>` prints a chunk's shader constants so a gate checks a
  LOD material against its SOURCE, not against the pass that wrote it. MEASURED: not one of
  Fallout4.esm's 3430 LOD shader blocks has a lit emissive colour, no LOD model has an effect
  shader or a glow slot, none of their 121 materials emits, and the Diamond City stadium's
  floodlights (emittance (1,1,1) × 6.0) have ZERO MNAM slots — vanilla's LOD emits nothing.
  `tools/lod_emission_probe.py` is that measurement, offline, no exe needed.
* "no LOD-bearing refs in chunk (X,Y)xD (N placed, M without a usable LOD model)": an empty MNAM
  slot DROPS a ref at that ring (vanilla parity). The far chunk (-32,16) at dim 16 is empty without
  `--impostors`; it is only a valid object-chunk test WITH cards. Read the counters before blaming
  the loader (2026-09-06: an hour on a loader that was fine).
* A window of bungo's holds `release/NifSkope.exe`: NEVER kill it. `mv` the exe aside as
  `NifSkope_inuse_<pid>.exe` (Windows allows the rename), then link; his process keeps the old
  image, and the leftover is deletable once his window closes.

## The manifest (`<chunk>.bto.manifest.txt`)
First line `# lodgen manifest 2 ws <edid> dim <d> chunk <x> <y> columns index base type x y z scale
class height ref part`; rows as named (index = R + G*256 per chunk; ref = the placed reference's
form ID, part = a SCOL part's ordinal or -1); `I base model count ids` = instance groups; `A block
layer array` = texture-array layers. The stable key across rings and bakes is `(ref, part)` --
471 of Sanctuary (-20,24)'s 678 objects are SCOL parts, so never key on ref alone. Gate:
`tests/spells/lodgen_identity.sh` (byte-identical rebake, unique keys, 406 shared with dim 8).
LOD MATERIAL SPEC = `docs/LODGEN_IMPOSTOR_SPEC.md` (read it first). TWO FAMILIES, one `.lodm` (our LOD
material, `src/io/lodmfile.h`: `LODM` envelope + compact JSON) beside every set: LEGACY = vanilla-sourced,
`_d` diffuse+coverage, `_n` X Y height sway, `_gsaos` gloss specular AO subsurface-mask (the vanilla `_s`
composed as the engine does: gloss = smoothness x _s.G, specular = _s.R x strength; NEVER inverted);
PBR = from a source `.lodm` of family pbr, `_bc`, `_n`, `_rmaos` roughness metallic AO subsurface-mask,
the third texture RAW. FOUR textures since 2026-09-06j: the fourth is the EMISSIVE, `_g` legacy / `_e` pbr,
BC1, RGB only, key `emissive` under both families; legacy law = the colour x its OWN alpha where the
material is not alpha-tested (vanilla carries LOD glow in the diffuse alpha of opaque chunk shapes), black
where it is; pbr law = the source `.lodm`'s `emissive` texture RAW, black when it names none; bake =
shader channel 13 (the glow slot raw where a `.lodm` retargeted slot 2). OPEN (CARDS1 doubt): an opaque
source with alpha 255 throughout yields `_g` = the full albedo; a consumer adding it unscaled lights every
wall -- the multiplier (Own-Emit / emissive colour, or a `.lodm` factor) is undecided. Card frames are
quantised to SIZE CLASSES (shorter side up to a multiple of 16, the extents widened, never shrunk, to the
frame's aspect; a `class <w> <h>` meta line) so a worldspace's trees share a few card arrays. A source `.lodm` sits beside the material (`foo.bgsm` -> `foo.lodm`) or at the
diffuse's path under `materials\`; loose root first (`--data-root`, bake: `WW_LODGEN_DATA_ROOT`), then
the game's resources. Vanilla LOD sources DO name BGSMs and carry an `_s` in slot 7 (the maple:
`Materials\LOD\PreWarMapleGrLOD.BGSM`); the chunk shape now carries slot 7 + smoothness + strength.
Texture arrays: `--arrays` (region mode, BEFORE the atlas) -> `<tex>/Objects/<ws>.LodgenArrays.<WxH>_d/_n/_gsaos.DDS`
and `<ws>.LodgenArraysPBR.<WxH>_bc/_n/_rmaos.DDS` (DX10 BC3, dxgi 77) + `<stem>.lodm` per set + `.txt`
sidecar (`family class layer lodm color normal mask source`); manifest `A block layer <lodm>` and
`M block <material>`; layer in UV2.y ((desc >> 10) & 0x3C, second half); gate
`tests/spells/lodgen_texture_arrays.sh` (legacy run, then a loose root with one pbr `.lodm`).
Atlas + merge: `--atlas` writes THREE sheets under `<tex-dir>/Objects` (the directory the game path
baked into every atlased shape names -- it wrote them one level ABOVE that until 2026-09-06,
`docs/MISTAKES.md`): `<ws>.LodgenObjects.DDS`, `_n.DDS` and `_s.DDS`, the last BC5 like vanilla's
`Commonwealth.Objects_s.DDS` and composed with each shape's CONSTANTS FOLDED IN (R = the map's R x
specular strength, G = the map's G x smoothness, 255/255 where a cell has no map); every atlased
shape then reads slot 7 = the sheet at smoothness 1 / strength 1, as vanilla's chunks do.
`--merge` (`lodgenMergeChunkShapes`, on by default in region mode, `--no-merge` off) runs LAST,
after the atlas AND the arrays, and concatenates every shape the engine cannot tell apart: name,
ten texture slots, alpha property, shader type/flags/constants, vertex descriptor, and the array
`.lodm`. Per SEGMENT, so the dim x dim segment grid survives; union bounds; never past 65535
vertices. Sanctuary (-20,24) dim 4: 10 shapes -> 4, vertices and triangles unchanged (vanilla's
chunk has 3). A merged shape can span layers, so `A <block> -1 <lodm>` means the layer is PER
VERTEX in UV2.y; a positive layer still means the whole shape is on that one. Gate
`tests/spells/lodgen_merge.sh`. SINCE 2026-09-12 identity is OFF by default, so a default `.BTO`
has no UV2 to carry that -1: measured, every one of the 21 `A` lines of a dim-16
`--arrays --impostors --slot-fallback` bake is a POSITIVE layer with identity off, the same 21 as
with it on, but a `-1` in a file baked without `--identity` is unresolvable and a consumer must
treat it as unknown rather than read UV2 that is not there (`docs/LODGEN_IMPOSTOR_SPEC.md`).
Card arrays: `--arrays` WITH `--impostors` packs the card sets the chunks' `C` lines stand on into
`<ws>.LodgenCards.<family>.<WxH>_d/_n/_gsaos.DDS` (or `_bc/_n/_rmaos`) + a `cardArray` `.lodm`
(class, grid, frame and mips shared by the set; id, half, centre, depthSpan and source per layer),
grouped by family AND sheet size -- sets differing in grid or frame cannot share an array. Each
such `C` line gains TWO tokens on the END: the array `.lodm` and the layer, so a reader that stops
at the tenth token is unaffected. Runs after the merge. Gate `tests/spells/lodgen_card_arrays.sh`.
Cards bake from the BASE's near model (`--list-impostor-candidates [--candidates missing|trees|all]`
prints `formid <near MODL>`, SCOL parts walked; driver `CANDIDATES=trees`); the bake hides `_L1`..`_L9`
detail-step shapes (meta `hidden`, `model` lines; `.lodm` `card.source`). `--impostors-from-level N`
(panel "Cards from ring", FO4CS only) puts a placement on its card from MNAM level N on even where the
ring has a mesh; one 128 px bake serves every ring through its mips.
Octahedral impostors: `OCT=N bash tools/bake_impostor_cards.sh ...` -> `<id>_oct_{albedo,normal,gsaos|rmaos}.png`
+ an `oct N tileW tileH halfW halfH cx cy cz depthspan family` meta line + `lodm <cand> <family|none> <diffuse>`
lines (frames on the grid vertices, rectangular, fitted to the silhouette over all views by a first pass);
`--impostors <dir>` converts them to `<id>_oct_{d,n,gsaos}.DDS` or `_{bc,n,rmaos}.DDS` + `<id>_oct.lodm` and
writes `C index cx cy cz halfW halfH N depthspan <lodm>` manifest lines; the crossed quads stay for stock.
Shader channels 8 normal, 9 height, 10 the material pair (legacy) or slot 7 raw (`lodMaskRaw`, set when a
`.lodm` retargeted the shape via `wwTextureOverride`), 11 alpha-test label. A card set is pbr only when
EVERY textured shape has a pbr `.lodm`. Gate `tests/spells/lodgen_octahedral.sh` (two real GUI bakes,
N=4, ~3 min).

## Impostor cards: the frame law (2026-09-06)

Three settings compose, and none of them is obvious from one place in the code.

* **Card frames** `WW_IMPOSTOR_OCT` / `OCT=` / panel row: 4, 6 or 8 FRAMES PER
  SIDE, so 8 is 64 views, not 81. The bake maps `u = i/(N-1)*2-1` over
  `i` in `0..N-1`. Trades angular smoothness against per-view sharpness.
* **Card resolution** `WW_IMPOSTOR_TILE` / `TILE=` / panel row: 64, 128 or 256 px
  on the long side, snapped to a multiple of 32, and it is what the run's
  LARGEST base gets, not everyone's.
* **The reference** `WW_IMPOSTOR_REF`: the largest extent among the run's
  candidates, in world units. The hook photographs ONE model per process so it
  cannot know this; `--list-impostor-candidates` reports each base's extent and
  the driver takes the maximum. Unset = no size ladder.

Two ladders, each nearest-in-log, both COARSE on purpose (a card array holds only
sets sharing a grid AND a frame, so each extra frame shape is another bind):

* SIZE: pure halving, three rungs, floor 32. A quarter the size is a quarter the
  long side.
* ASPECT: five rungs `1, 3/4, 1/2, 3/8, 1/4` for the short side, rounded to 4,
  floored above the gutter. The 3/4 rung is MEASURED - TreeMapleForest2 is 0.75
  of its height at 8 x 8 and squaring it costs 33%.

The fit GROWS whichever extent is loose rather than cropping, so a coarse rung
buys air in the frame, never a cut silhouette.

`--card-half-aux` halves each side of the normal, mask and emissive sheets and
leaves the base colour alone; measured 46.4% of the payload. The base colour never
divides - its ALPHA is the coverage, so it is the silhouette.

### Two positional formats that changed, and both bite

* The sidecar's `oct` line is `oct N tw th halfW halfH cx cy cz span family base`.
  The family is NO LONGER the last token. Anything anchoring `pbr$` or `legacy$`
  breaks. (A `.trimmed()` bug on this same line once made every pbr card fall
  back to legacy names.)
* `--list-impostor-candidates` prints `formid extent model`. The extent is column
  TWO deliberately: the model is the only token that can hold a space, so it must
  stay the line's remainder for `read -r id extent model`. Consumers using
  `cut -d' ' -f1` are unaffected.
* Every `lodgen` sub-command needs the `<file>` positional (`nifcli.cpp`:
  `error: 'lodgen' needs a <file>`), INCLUDING `--native-fixture <dir>` and
  `--native-verify <lodo> <lodi>`, which return before they would open it.
  Pass the ESM anyway; a hook-up note that writes them without one describes
  a command the CLI refuses (2026-09-10, lane BUILD6).

A card set's `.lodm` carries `card.oct`, `card.frame` and `card.base`. A frame
BELOW the base is that base's rung on the size ladder and is correct; the panel
refuses only when `oct` or `base` disagrees with its rows.

## Measured facts (do not re-derive)
FO4 LAND = 32 samples a cell (33x33 VHGT, shared edge); FO76 `.btd` = 128 (4x linear = "16x the
detail"). Commonwealth native heightmap 6144x6144. Shared VHGT edges take the MAXIMUM over every
cell holding the sample (0 mismatches vs the reference over 2.3M edge texels; bungo's call for
`.lodt` too). `.btd` alpha field s <-> texture slot s (0.13% residue vs 38% reversed); ground cover
bit b <-> g[b] (0 vs 89%); bit 15 never set; `.btd` colour is A1R5G5B5. F4FX: FNV-1a 64 pixel hash
over the R16 payload, corpus hash over every VHGT payload in file order (Commonwealth pins to
0xD8337D022F637F22, checked by the CLI). Writer: 7.4 s Commonwealth, one decode per cell; the AO
pass had been 61 s from cache thrash.

## Two CLI/measurement traps (lane NATIVE1a, 2026-09-11)

* **`release/NifSkope.exe` resolves a RELATIVE output path against `release/`, not against
  the shell's cwd.** `lodgen ... --native-fixture scratchpad/x` printed
  `wrote scratchpad/x/Synthetic.lodo` and the directory the command named stayed EMPTY --
  the bytes were under `release/scratchpad/x/`. Every path handed to `-no-gui` is ABSOLUTE
  (`E:/...`), every time, including `--out-dir`, `--native`, `--native-mesh-report` and the
  fixture. The symptom is a successful-looking run and an empty `ls`.
* **A Bethesda sidecar's own printf can be coarser than our tolerance, and then the bar
  measures the printf.** `<chunk>.bto.manifest.txt` prints coordinates with SIX SIGNIFICANT
  DIGITS, so the print step depends on the magnitude: 0.1 at five-digit coordinates, and
  **1.0 at six-digit ones** (`143360` comes back with no fraction at all). Measured on the
  nine-chunk Sanctuary region: 3,356 of 3,526 y values and 385 x values print at step 1.0.
  Any positional bar tighter than 0.5 u therefore fails on the MANIFEST, not on the writer --
  lane BUILD6 hit the 0.1 form and lane NATIVE1a the 1.0 form. The rule: compute the step
  from the PRINTED TOKEN (count the fraction digits; a bare integer longer than six digits
  has step `10^(len-6)`), budget half of it per axis on top of the format's own quantisation
  bound, and then PROVE the bar still catches a deliberately shifted position. The same trap
  waits in `.lodm`, the card sidecars and the terrain pyramid.

## A spell's own path line, and the region a gate is vacuous on (lane NATIVE1b, 2026-09-11)

* **`a && b || c && d` is `(((a && b) || c) && d)`, and that line is in
  `tests/spells/lodgen_native.sh`.** It read
  `WA="$(cd "$W" && pwd -W 2>/dev/null || cd "$W" && pwd)"`, so on any shell
  where `pwd -W` SUCCEEDS the `pwd` after the `||` runs as well and `$WA` comes
  back as TWO LINES. Every path built from it carries an embedded newline, the
  bake writes into one directory and every checker looks in another, and the
  suite fails in nine places that all read like writer defects
  (`1 stock files compared, 1 differ`, `DIFFER *`, `cannot open`). The
  diagnosis is two characters wide and it is in the traceback:
  `'E:/...gate/after\n/e/...Commonwealth.lodo'`. **Group the fallback:**
  `x="$(cd "$d" && { pwd -W 2>/dev/null || pwd; })"`. And note WHY it had
  survived a day: the suite had only ever been run without `OUT=`, into a
  `mktemp -d`. **A gate run with a new argument is a NEW gate** — run it the way
  the lane will actually run it, and treat that first run as a gate on the gate.

* **THE SANCTUARY REGION HAS NO WATERTIGHT LOD MESH.** Measured: the nine-chunk
  region (cells −20 24 −9 35) draws **41 distinct LOD meshes and 0 of them are
  watertight**, against 365 watertight in the whole worldspace library (335 of
  those over 256 units). Any rule that needs a closed shell — an occluder box, a
  ray-parity interior test, a solid-volume fit — is **vacuous on Sanctuary**,
  and a zero there is correct behaviour and not a defect. Bake a SECOND small
  region for it: cells **0 −12 11 −1** (downtown Boston) gives 33,123 placements
  and 280 occluder boxes over 87 of 147 populated cells, in about a minute. Do
  not loosen the rule until Sanctuary passes; make the vacuous case a NAMED SKIP
  and gate the rule where it has something to bite on.

* **The mean LOD mesh in the Commonwealth is 47.7 triangles.** 2,982 meshes,
  142,138 triangles at full detail. Anything that plans to simplify these
  further should size its expectations against that number first: halving a
  whole building's 54 triangles costs 98 percent of its diagonal, and a
  cluster ladder built on the `MNAM` slots is barely selectable at a one-pixel
  tolerance anywhere in the worldspace (lane NATIVE1b measured the median
  level-1 deviation at 3.80 percent of the model diagonal — one pixel at
  52,100 units).

## The colour sheet has TWO writers, and the one that SHIPS is the pyramid (lane TILING2, 2026-09-11)

The seven-step landscape colour law (`docs/LODGEN_TERRAIN_VT.md` §2.5) is
implemented **twice** in `src/lodgen.cpp`: once in the stock per-chunk composite
and once in the virtual-texture tile composite. With `--vt` on — which is how
every region bake is run — the chunk's `<ws>.<dim>.<x>.<y>.DDS` is **assembled
from the pyramid's level-D/2 tiles** (§2.4) and **the stock composite is never
reached**. A colour change made at one site only builds, links, runs, reports
chunks written, and changes **nothing on disk**.

Lane TILING2 lost a build and a bake cycle to this: a quadrant cross-fade added
to the chunk composite produced a byte-identical sheet even at
`--blend-margin 1024`, and the first instinct — "my parameter is too small" —
was wrong in a way no bigger parameter could reveal.

**The rules:**

* A change to the colour law goes into **both** `sampleLtex` lambdas and **both**
  composites, or it does nothing. Anchors: `auto quadComposite = [&](` (stock)
  and `auto quadColorAt = [&](` (pyramid) in `lodgen.cpp`.
* The pyramid copy should be **colour only** when that is what the change is.
  The tile loop also blends roughness, metalness, emissive and the cover
  opacities; leaving those alone is what keeps `_data`, `_msn`, the `.lodm`, the
  BTO, the BTR and the manifest byte-identical, which is the gate that proves
  the change is confined.
* **When a flag produces a byte-identical artefact, the first hypothesis is "the
  code I changed does not run", not "the parameter is too small."** The cheapest
  probe is to bake the same region without `--vt`: no tex files are written at
  all, which names the writer in one run.
* `--tex-dir` writes nothing without `--vt`, and `--cover` is required as well.
  A bake with `--tex-dir` alone exits 0 and produces an empty directory.

## Reading a `.lodt` mask sheet: the codec is PER TILE, not per sheet (lane GROUND1, 2026-09-12)

**This cost a whole round of numbers, all of them published inside the lane
before they were caught.** The mask sheet is **BC1 (dxgi 71) on a cover-free
tile and BC3 (dxgi 77) on a cover tile**, chosen per tile by the `COVER` bit
(`flags & 2`) against the header's format PAIR. `lodgen_vt_check.sheetMipBytes()`
already knows this and returns the right byte counts; its `decode_bc1()` does
not, and walks 8-byte blocks. Decode a BC3 tile with it and every channel is
garbage that still LOOKS like terrain, because the offsets stay plausible and
the alpha block in front of the colour block shifts the colour endpoints rather
than destroying them. On the measured region 12 of 16 tiles carried cover, so
three quarters of every mask number was wrong and the means were still in a
believable range.

Read a mask tile like this, never with a fixed stride:

```python
e = v.table[index]; p = v.payload(index)
ms = mask_sheet_index(v); sd = v.sheets[ms]
cover = bool(e['flags'] & 2)
fmt = sd['dxgiCover'] if (cover and sd['dxgiCover'] != sd['dxgi']) else sd['dxgi']
stride = 16 if fmt in (77, 78) else 8            # BC3/BC3_SRGB carry an alpha block
o = v.sheetOffset(cover, ms, mip); side = v.stored >> mip
rows = decode_rgb(p, o, side, side, stride, 8 if stride == 16 else 0)
```

The working reader is `scratchpad/ground1_20260912/work/maskdec.py`.

**The tell that would have caught it in one run, and the general rule:** decode
the SAME tile at two mips and check the means agree to within a few counts. A
wrong stride disagrees wildly because mip 1's offset is not a wrong-stride
multiple of mip 0's. Any decoder that has a format switch above it needs a
refuter that fires on the wrong branch, or the wrong branch produces terrain.

## Measuring the REACH of a horizon march: use its own sample points (lane GROUND1)

The question "how far can this term move a texel" has an exact answer and two
tempting wrong instruments.

* A **chamfer distance transform** over the occupied map reported 1,992 texels
  past the bound. It is a distance-to-nearest-anything, and the march does not
  sample everything -- it samples 8 directions x 7 distances = **56 points**.
* An **exact Euclidean transform** (Felzenszwalb) reported ~1,901 at up to
  1,717 u. Same defect, better arithmetic. Twice I adjusted the BOUND instead
  of the instrument, which is the failure this entry exists for.
  (It also NaNs: `float('inf')` arithmetic in `edt1d` gives `inf - inf`. Use a
  large finite sentinel, `BIG = 1.0e12`.)

The instrument that answers the question asked: **for each texel, evaluate the
march's own 56 world points and ask whether ANY of them lands on an occupied
square.** A texel where none does cannot have moved, because the term returns
exactly `1.0f` by early return there. Then classify every darkened texel by the
FARTHEST sample that was occupied -- that maximum IS the reach, measured.

Expect a residue of darkened texels with no occupied sample of their own. Prove
they are block-codec spill rather than the term: check that each shares a 4x4
block with a texel that does have one, and count the texels that came out
BRIGHTER. A multiplication by a number at most 1 cannot brighten anything, so
the bright count is the codec noise floor and the residue has to be the same
size or smaller to be explained by it.

**The loop bound is not the reach.** `for ( float dist = 128; dist <= 2048;
dist *= 1.5 )` samples 128, 192, 288, 432, 648, 972, **1458** -- 2,048 is never
sampled. The per-vertex march in `lodgenTerrainChannels` steps 1,2,3,4,7,10,13,16
x 128 u and DOES reach 2,048. Two different numbers under one description; the
ledger's dependency map needs the right one.

## Added by lane HORIZON2 (2026-09-18)

**A `.lodt` sheet's channel order in the FILE is not the packing order in the
C++.** The bake builds a Qt `0xAARRGGBB` u32 (`shift[4] = {16,8,0,24}`) and the
writer hands the plane out as **R8G8B8A8**, so on disk bin *j* of each group of
four is **byte j**, shifts `{0,8,16,24}`. Any new reader validates against the
shipped CONSUMER (`src/btdterrain.cpp` -> `LodtSheets::sheetChannel(role, tx,
ty, bin % 4, …)`), never against the writer's constants; reading the file with
the writer's shifts swaps bins 0 and 2 of every four and looks exactly like a
bake defect. And note what the in-bake refuter cannot do: it reads its bytes
back out of the in-memory `planes`, never out of the file, so no gate in the
tree can currently catch a wrong swizzle.

## Added by lane HORIZONOUT (2026-09-19) -- THE BAKED HORIZON IS GONE

bungo ruled on the pictures, not on the design: *"As you can see, the end result
is terrible ... So, for now, we revert back to identity data per LOD object from
the preauthored LODs ... We're not doing the horizon thing ... So yeah, horizon
goes bye bye now, we're back to identity."*

Two measurements say why, and both are in the tree. A baked per-object horizon
disagreed with a ray-cast sun on **50-58 % of object pixels** at low sun
(lane SUNSIM1). The plain identity far-shadow map, simulated at a 64 u join,
disagreed on **about 9 %** (lane HORIZON4). The cheap thing was also the
accurate thing, which is not the way that argument usually goes.

**What went, and there is no switch that brings it back:** the `--horizon-*`
switches, `--horizon-subdivide` and its `.lodo` **v5** (no exe ever wrote one,
so v5 was removed WHOLE and `.lodo` stays at v4), `--horizon-face-sheet`
(parsed-only, never implemented), the per-vertex `.lodi` **v8** stream, the
`.lodt` **role 7** terrain horizon sheet PRODUCER, the `horizon` and
`horizonbin=<n>` viewer channels with `WW_SUN` / `WW_HORIZON_SOFT_DEG` /
`WW_HORIZON_BIN_ROT`, and the gates `lodgen_horizon.sh`, `lodgen_horizon3.sh`
and `lodgen_horizon_witness.py`.

**What the READERS still do, deliberately.** `.lodi` v8 still opens and still
names its stream in the dump; `LODV_ROLE_HORIZON` and the role-7 validation in
`src/io/lodvfile.*` + `src/lodtsheets.*` still stand. A file met in the wild is
opened honestly rather than refused. v8 is NOT in the line of descent: **v9 is
v7 plus one instance bit**, not v8 plus anything, and a v9 file carries no
horizon stream at all. Proof, and the shape of the argument to copy: a
`--native-verify` of lane HORIZON1's v8 bake on the shipped exe (`lodi version
8`, `vertexHorizonBytes` non-zero, rc 0), kept as G5 of
`tests/spells/lodgen_scrappable.sh`.

### The two things that outlived the route

**1. The workshop-scrappable bit -- `--scrappable`, `.lodi` v9, instance flags
0x14 bit 6 (`0x0040`).** It never depended on the horizon; it answers a need of
its own, because a placement the player can scrap is a placement that will not
be there, and a far field that keeps drawing it is wrong about a settlement from
the first hour of a save. Three clauses, all read OUT OF THE PLUGIN
(`EsmScrapIndex`, `src/esmdata.h`): the base is the `CNAM` of a `COBJ` carrying
`00106D8F WorkshopRecipeFilterScrap` (FormLists expanded transitively), AND the
placement stands inside an `XPRM` Box build area linked by `000B91E6
WorkshopLinkedPrimitive` to a workshop bench, AND the base does not carry
`001CC46A UnscrappableObject`. **On the measured urban region that is 14 of
33,123 placements (0.04 %), and 14 is the gate.** One clause is a heuristic and
is labelled one in the code: the bench is found by an editor id containing both
`workshop` and `workbench`, because the link runs from the primitive to the
bench and there is no keyword on the bench side to read.

*The version guard that is easy to miss:* v9 implies v7's 512-byte header. A
bake run with `--lodi-v6` has 256, so the writer **drops the bit** rather than
write a version claiming a layout the file does not have -- and drops it
visibly, because the census then reads `scrappablePlacements 0`.

**2. The identity route itself, and the join under it.** The far shadow map is
keyed on the `.lodi` GROUP id (`offGroup` 0x100, `groupCount` 0x108,
`src/lodifile.h:556`), and self-shadow is excluded BY IDENTITY: a caster never
darkens a receiver of the same group. The group is therefore the whole contract,
which is why bungo also ruled the join that builds it (lane IDENTPROX, the same
day). The default is now **proximity**: every NON-TREE placement with a drawn
LOD mesh joins at a **MESH-TO-MESH gap of 64 u** (`--identity-join-gap <u>`),
measured over dense sample points -- level-0 vertices, three edge midpoints and
the centroid of every triangle, placed. A mesh measure and not a box measure is
what stops two buildings welding across a street. `--identity-join legacy`
restores the pre-2026-09-19 rule (an `architecture` path component and a world
axis-aligned box gap of 16 u) and is the red control every gate uses.

**THE GROUP IDS ARE DENSE PER CHUNK, NOT GLOBAL** (`src/lodifile.cpp:646`, and
the reader enforces it at `:1394`). A chunk holding C groups uses exactly
{0 .. C-1}, and the header's `groupCount` (0x108) is those per-chunk counts
SUMMED -- so the identity of a group is the pair **(chunk, id)**, never the u16
alone. Any independent reader that counts the raw u16 over the whole table
merges chunk 0's group 3 with chunk 1's group 3 and reports FEWER groups than
the file holds. A region that looks like one chunk is usually not: `4 -12 7 -9`
at dim 4 carries three -- the big one plus a two-placement and a one-placement
chunk -- which is exactly the 588-vs-586 that cost this lane a debugging round
(root `MISTAKES.md`). The same rule is why the u16 does not overflow in
practice, and the writer refuses BY NAME at 65,535 (`src/lodifile.cpp:679`)
rather than wrapping.

*A number that will bite a comparison:* on chunk 4.4.-12 the ruled rule gives
**588 groups -> 167**, singletons 468 -> 62, `DN135_GwinnettExt` 205 -> 6. Those
are the DEFAULT library (`--library mnam`). Baking the same chunk with
`--library near` gives a different placement set and therefore different group
counts (584 -> 158), which is not a defect and is not comparable to the standing
numbers. **A before/after `.lodi` pair is byte-identical only when BOTH runs use
the same library AND `--identity-join legacy`**; forgetting the second one makes
a perfectly good exe look like it broke byte-identity.

### Census words the gates grep for, verbatim

`native-identity-join:` (`PROXIMITY` / `LEGACY` / `OFF (--lodi-v6)`),
`native-scrappable:` (`ON: N of M ...` / `OFF`, carrying `scrappablePlacements`
either way). Both print on every bake, so a missing word means a missing exe,
not a missing feature. **The retired words print NOTHING and must not print a
zero** -- `horizonSubdivVertices`, `horizonSubdivTriangles`,
`vertexHorizonBytes` on the writer side and the rest of them are gone with the
pass, and a census that still says `0` for a pass that no longer exists is a
census telling a reader the feature is merely switched off.

**Gates:** `tests/spells/lodgen_scrappable.sh` (the bit, its plugin re-derivation
and a one-byte red control) and `tests/spells/lodgen_identjoin.sh` (the join, its
layer cross-check against the Creation Kit's own `XLYR` layers, and `legacy` as
the red control). `lodgen_horizon*.sh` is retired; no board script referenced it.

**Docs:** `docs/LODGEN_NATIVE_LODO_LODI.md` s3.7 (there is no `.lodo` v5),
s4.11 (v8 RETIRED, reader-only) and s4.12 (`--scrappable`);
`docs/LODGEN_TERRAIN_VT.md` s3.5 (role 7 retired, reader-only);
`docs/LODGEN_CENSUS.md` (the retired-words row, `native-scrappable:`, the
identity-join clause); `docs/FO4CS_IMPROVED_LOD_PLAN.md` s9 -- the far-shadow
contract as a **caster x receiver matrix**, marked the director's draft for
bungo to rule.
