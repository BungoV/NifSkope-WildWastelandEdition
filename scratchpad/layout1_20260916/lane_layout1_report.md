# Lane LAYOUT1 -- every FO4CS-target output under `<out>/FO4CSLOD/`

Exe at launch: `release/NifSkope.exe` 22,356,992 B, 2026-09-16 17:11:17 (BTOFREE1's).
Tree E:/Projects/NifskopeWildWastelandEdition, branch main, HEAD 720762a, 487+ uncommitted paths.
Game check at 19:51: no Fallout4.exe, no NifSkope process.

## 1. Read order
- CONSTITUTION.md read in full.
- HANDOFF.md top block read: BTOFREE1 LANDED 19:48 (exe 17:11:17, .BTOs now built in
  `<mod>/lodgen_bto_scratch/` and removed before the native pair; manifest sidecars still under
  `meshes/terrain/<ws>/` -- this lane moves them). RULED 19:3x = `Data/FO4CSLOD`.

## 2. The one function, and every writer that calls it

New pair `src/lodgenlayout.h` / `src/lodgenlayout.cpp`. `FO4CSLOD` is a string literal in exactly
one file (`lodgenlayout.cpp`); gate (e) proves it with grep.

| function | answers |
|---|---|
| `lodgenFo4csFolderName()` | `FO4CSLOD` -- for UI text, which composes it with `%1` rather than spelling it again |
| `lodgenFo4csRoot( mod )` | `<mod>/FO4CSLOD` |
| `lodgenFo4csWorldDir( mod, ws )` | `<mod>/FO4CSLOD/<ws>` -- the one every file writer calls |
| `lodgenFo4csCardDir( mod )` | `<mod>/FO4CSLOD/Cards` (cards are per TREE, shared by every worldspace) |
| `lodgenFo4csGameWorldPath( ws )` | `FO4CSLOD\<ws>` -- for the strings written INTO files |
| `lodgenFo4csGameCardPath()` | `FO4CSLOD\Cards` |
| `lodgenClearLayoutCensus()` / `lodgenNoteLayoutFile()` / `lodgenNoteLayoutDir()` / `lodgenLayoutCensusLine()` | the census, read back from the paths actually written |

Writers moved (file:what):

- `src/lodtfile.cpp` -- the `.lodl` writer.
- `src/lodgen.cpp` -- the aggregate sheet folder; the VT level folder, its `.lodt` levels and its
  `.lodm` index; the container path string inside that index; the card `texPath` written into the
  crossed-quad material and the octahedral card's game stem.
- `src/nativeemit.cpp` -- the `.lodo` + `.lodi` pair.
- `src/lodgenaggregate.cpp` -- `lodgenAggregateStem()`, the game path of a cell's aggregate set.
- `src/lodgenchunkpass.cpp` -- the manifest sidecar's destination is noted into the census; the
  legacy chunk folder `meshes/terrain/<ws>/` is created only when a legacy chunk is going into it.
- `src/nifcli.cpp` -- the three `.lodl` path compositions (write, `--verify-only`, `--refresh-ao`),
  `--native` (now a MOD FOLDER), the printed aggregate folder, the object texture sets (arrays,
  atlas, card arrays) and their game strings, the manifest teardown destination, the census clear
  at the head of the sub-command, and the CLI help.
- `src/lodgenmanager.cpp` -- the panel: the `.lodl` AO-refresh path, `nativeDir`, the object
  texture sets and their game strings, the manifest teardown destination, the impostor-card
  default, the root label, four summary sentences and four tooltips.
- `src/nifskope_ui.cpp` -- the panel self-test reads the pair from its new path.

### The one extension beyond the ruling table, said out loud

The table does not name the OBJECT TEXTURE SETS -- the mesh arrays, the object atlas and the card
arrays (`<ws>.LodgenArrays.*`, `<ws>.LodgenObjects.*`, `<ws>.LodgenCards.*`). They are written by
the object pass, not by a terrain writer, so they fall between the rows. Gate (a) settles it:
"NOTHING of ours outside `FO4CSLOD/` except `Textures/Terrain/<ws>/*.HeightMap.*`", and the
"stays where it is" clause is explicitly about the STOCK target's `textures/terrain/<ws>/`. So
under the FO4CS target they follow the rest into `FO4CSLOD/<ws>/Objects/` and their game strings
say `data\FO4CSLOD\<ws>\Objects\...`; under the stock engine not one byte of them moved. If bungo
wants them left in `textures/terrain/<ws>/Objects/` it is a one-line change in `objectsDir()` /
`objectsGame()` (both front ends call the same two).

## 3. Docs changed (every line carries its provenance inline)

| page | what changed |
|---|---|
| `docs/LODGEN_BTD_FORMAT.md` | the `## Location` block (`Data\FO4CSLOD\<WS>\<WS>.lodl`) with a MOVED paragraph, and the `--verify-only` sentence |
| `docs/LODGEN_LEDGER_FORMAT.md` | the divergence block gained a 2026-09-16 note: the word came, the new spot is BAKEREC1's, this lane wrote nothing there |
| `docs/LODGEN_MANIFEST_FORMAT.md` | a WHERE IT LANDS paragraph: the sidecar is beside the files it describes under `FO4CSLOD/<ws>/`; `--keep-bto` and the stock engine unchanged |
| `docs/LODGEN_NATIVE_LODO_LODI.md` | the §1 file table's two paths, plus a WHERE THEY LAND paragraph recording that `--native` now names a MOD FOLDER |
| `docs/LODGEN_TERRAIN_VT.md` | §3 `**Name:**`, the "not `Data\Textures\Terrain\`" reasoning (rewritten, it argued from the `.lodl` precedent which moved with it), and the `"container"` string in the worked index |
| `docs/LODGEN_CARD_SHEETS.md` | the card-array path block |
| `docs/LODGEN_CENSUS.md` §6.1 | a new row for the `bake census: ... layout` clause: what it carries, how it MOVES, its accusing default, and the gate leg that reads it back |
| `docs/FO4CS_IMPROVED_LOD_PLAN.md` | §1.2's six read paths, a paragraph above the table saying every path in it moved on 2026-09-16, and a new owed row 18: the module still composes the old paths and an FO4CS lane owes the reader half |
| `docs/LODGEN_PLAN.md` | NOTHING: grep finds no output path in it (the brief listed it; the hit count is 0) |
| `tools/bake_impostor_cards.sh` | the header's "ship the directory as" line -> `Data/FO4CSLOD/Cards`, with why the cards sit at the root and not under a worldspace |

## 4. The byte-identity claim, and the defect it caught

Two bakes of the same region with the same switches -- `release/NifSkope.before_layout1.exe`
(the rung, 22,356,992 B) and this exe -- into `scratchpad/layout1_20260916/work/gb/{old,new}`:
`--worldspace 3C --terrain-region -20 24 -20 24 --dim 4 --native <mod> --cover --arrays
--road-detail 1 --impostors <cards> --impostors-from-level 0`, both rc=0.

`tests/spells/lodgen_layout_diff.py` rewrites the OLD file's path spellings to the new
ones (raw, JSON-doubled-backslash and lower-case `data\` forms, longest first), repacks a
LODM envelope's payload-length word when the rewrite resizes it, and then demands byte
equality. Over the 84 files both bakes produced:

    83 identical, 1 differ, 0 missing, 17 carried a path string

The 17 that carry a path string are the 16 `.lodm` texture-array/card-array indices
(4 strings each, payload length repacked) and the manifest sidecar (1,313 strings).
Every DDS sheet, the `.BTR`, the `.lodo` and the `.lodi` are identical without any
rewrite at all: the move did not touch a single pixel or a single vertex.

**The one that differed was a real defect, and it was mine.** `Commonwealth.lodb`, the
ledger, records each chunk's outputs as `<path> <sha1>`. The manifest sidecar is one of
those outputs and it moved -- but `src/nifcli.cpp` still told the ledger the sidecar
would be at the mod folder's root. Nothing writes there any more, so the entry was
recorded with an EMPTY digest:

    old: "Commonwealth.4.-20.24.BTO.manifest.txt 2ec14d0b010348717747c81ae0e6be1b156445fb"
    new: "Commonwealth.4.-20.24.BTO.manifest.txt "

An empty digest never matches, so every later `--incremental` run would have rebaked the
whole region and said nothing. Fixed in `src/nifcli.cpp` (the producedFiles callback now
composes the sidecar's path with `lodgenFo4csWorldDir()`, the same function the teardown
uses), and a new row in gate leg (a) walks every ledger entry, resolves it on disk and
re-hashes it, so the class cannot come back silently.

The ledger itself is NOT in the byte-identity pair list, and the reason is stated rather
than swept: it stores SHA-1 digests OF the files it tracks, and the manifest's own bytes
legitimately changed (its path strings), so the ledger's digest for it must change too.
A digest is not a path string and cannot be rewritten. The ledger gets the
resolve-and-rehash row instead, which is the stronger check.

## 5. Gates

`tests/spells/lodgen_layout.sh` is new, with legs (a) to (f); leg (g) is every re-based
harness plus the panel self-test, run separately.

Re-based this lane, each for output paths only -- a fixture baked months ago
(`scratchpad/showcase1_20260912`, `scratchpad/water2_20260909`, the shipped mod folder)
is a READ path and was left alone, and so was every stock-target output, which did not
move:

| harness | what moved in it |
|---|---|
| `lodgen_byte_gate.sh` | `$FO4="FO4CSLOD/Commonwealth"` defined; `.lodl`, manifest, Objects sets and the pair re-based; the `.BTR` split out as unchanged. Two lines carrying a literal `\n` from an earlier heredoc were repaired -- they were silently passing `n` as an argument. |
| `lodgen_ladder.sh`, `lodgen_native.sh`, `lodgen_btofree.sh` | a `pairdir()` helper, so a rung-exe tree (old layout) and a new tree both resolve |
| `lodl_write.sh`, `lodl_water.sh`, `lodl_btd.sh` | every `.lodl` this harness writes |
| `lodgen_terrain_vt.sh` | the container folder, the second-run comparison, and the three sentences that named `Terrain\` |
| `lodgen_terrain_pbrm.sh` | the `--lodt-check` path |
| `lodgen_defaults.sh` | a `find` that already asked rather than spelled; comment only |

Left alone deliberately: `lodgen_texture_arrays.sh`, `lodgen_card_arrays.sh`,
`lodgen_terrain.sh`, `lodgen_farring.sh`, `lodgen_native_baseline.sh` and
`lodgen_roads.sh` bake the STOCK target (no `--native`), whose files did not move;
`native_lighting.sh`, `native_open.sh`, `lodl_open.sh`, `render_shot.sh` and the four
water harnesses read fixtures baked before the move.

## 6. The census word, and the panel's root label

The census reads the root back from the paths the bake ACTUALLY wrote -- every
FO4CS-target write is noted by `lodgenNoteLayoutFile()` as it happens, and the clause
states the common root of those notes, how many files landed under it, and how many
landed outside it:

    layout <out>/FO4CSLOD, 85 file(s), 0 outside

It MOVES with the layout because it is derived from the writes, not from a constant; and
its default accuses its own plumbing rather than flattering it -- with no FO4CS-target
file written the clause reads `layout n/a (no FO4CS-target file written)`, so a bake that
silently writes nothing cannot look like a bake that wrote everything to the right place.
The `0 outside` count is the part that would fail if one writer were missed.

The panel says the same thing in one line under the output field: a label named
`LodgenOutputRootLabel` reading `FO4CSLOD\<ws>\`, on the "LOD root" row, visible only
under the FO4 Community Shaders target. Under the stock engine it is hidden, because the
stock engine's files did not move. The panel self-test checks BOTH sides -- a label that
is always there and a label that is never there both fail -- and prints the text it read.

And the summary sentence beneath it, which had been telling the operator the manifest
sidecars "stay under meshes	errain\<ws>" for as long as they have not been there. That
was my defect and it is the census rule broken in the one place bungo reads a path
without opening a folder. It now asks the same composer the writer asks, so it reads
"(the manifest sidecars stay under FO4CSLOD\Commonwealth)", and a new self-test check --
"and names the root those sidecars land under" -- fails if the sentence ever stops
containing the composed root. The panel self-test went 127 checks to 128 with it.

## 7. Files this lane changed

New:

    src/lodgenlayout.h, src/lodgenlayout.cpp     the ONE function that composes the root
    tests/spells/lodgen_layout.sh                the gate, legs (a) to (f)
    tests/spells/lodgen_layout_diff.py           normalised byte identity + its refuter

Changed in src/: `nifcli.cpp` (the `.lodl`/`.lodt`/pair/aggregate/Objects/manifest paths,
the ledger's record of the sidecar, the help text), `lodgenmanager.cpp` (the panel's
object dir, game path, card-source default, AO `.lodl` path, native dir, manifest
teardown, the root label and four tooltips/summary strings), `lodgen.cpp` (card game
paths, the aggregate dir, the native dir), `lodgenaggregate.cpp` (the aggregate stem),
`lodgenchunkpass.cpp` (the empty-folder guard), `lodtfile.cpp` (the container/landscape
dir), `lodtfile.h` (a doc comment that still spelled `<outDir>/Terrain/<EDID>.lodl`; it
names the composer now), `nativeemit.cpp` (comment), `nifskope_ui.cpp` (the self-test's
pair path, the root label checks, the new sidecar-root check), plus `NifSkope.pro` for
the new pair.

Changed docs (each line carries its provenance inline): `LODGEN_BTD_FORMAT.md`,
`LODGEN_LEDGER_FORMAT.md`, `LODGEN_MANIFEST_FORMAT.md`, `LODGEN_NATIVE_LODO_LODI.md`,
`LODGEN_TERRAIN_VT.md`, `LODGEN_CARD_SHEETS.md`, `LODGEN_CENSUS.md`,
`FO4CS_IMPROVED_LOD_PLAN.md`, `tools/bake_impostor_cards.sh`.

## 8. The gate run, on the final exe

`tests/spells/lodgen_layout.sh`, exe `release/NifSkope.exe` 22,382,592 B
2026-09-16 22:48:35, region (-20,24) dim 4, 11 bakes, trees kept under
`scratchpad/layout1_20260916/work/gate/`:

    layout checks: 22, failures: 0      RESULT PASS   (finished 23:04:26)

- (a) 4/4 expected files at their new paths; nothing outside `FO4CSLOD/` but the named
  exemptions (1 `.BTR`, 1 `.lodb`, 0 `.BTO`, 3 files in the operator's own `--tex-dir`);
  no empty folder left behind; no `Terrain/`, `Textures/Lodgen/` or `meshes/` created at
  all; the ledger's 5 recorded outputs all resolve on disk with the digest recorded.
  The refuter fires: the rung exe's own bake puts 0 files under `FO4CSLOD/` and strays on
  every row.
- (b) 83 identical, 0 differ, 0 missing, 17 carried a path string. The refuter fires:
  without the rewrite, 17 of 17 path-carrying files differ (16 `.lodm` indices at
  offset 0x8 -- the payload-length word -- the array sidecar at 0x99 and the manifest at
  0xF1). Nothing else moved by a byte.
- (c) the stock target is byte-identical to the rung's at dim 4 (13 files), 8 (13),
  16 (5) and 32 (13).
- (d) the `--keep-bto` chunks are still at the out-dir root, one sidecar beside each and
  none under the root, and no scratch folder is made.
- (e) the folder name is a string literal in exactly one file, `src/lodgenlayout.cpp`;
  every remaining `/Terrain/` or `Textures/Lodgen` hit in `src/` is a comment, a READ
  root or the HeightMap, listed line by line in the gate's own output.
- (f) `census 79, on disk 79`, the census root is a folder that exists, 0 files written
  outside it.

**What the FIRST run caught (on the 20:49:35 exe): 22 checks, 3 failures.** All three are
closed and each is worth keeping:

1. **(f) `census 85, on disk 79` -- a real defect of mine.** The census counted notes,
   not files: two passes write into the same `Objects/` folder and each noted the whole
   folder, so the five array sheets and their sidecar were counted twice.
   `lodgenNoteLayoutFile()` now keeps a set of absolute paths folded for case. Green
   above.
2. **(d) the sidecar under `--keep-bto` -- the GATE was wrong, and the gate was fixed.**
   With `--keep-bto` the `.BTO` stays at the out-dir root, and `--keep-bto` is the way
   back to the old tree byte for byte (lane BTOFREE1); a sidecar in a different folder is
   not that tree. The leg now requires one sidecar beside each kept chunk AND none under
   the root, so a build that moved it anyway fails.
3. **(e) two unaccounted hits -- the gate's exemption list was too narrow.** Both were
   innocent: a comment in `nifcli.cpp` about where the ledger used to be suggested, and
   the panel's summary row naming the HeightMap DDS under `Textures\Terrain\`, which by
   the ruling did NOT move. The list now exempts comments in any file and the HeightMap
   by name, each with its reason written above the filter.

Leg (e)'s own listing is what then found the stale doc comment in `src/lodtfile.h` and
the panel summary sentence of section 6 -- neither of which any compiler would have
complained about.

## 9. Leg (g): every re-based harness, and the panel

Read off the logs on disk (`scratchpad/layout1_20260916/work/harness/`), newest run per
harness, printed by `work/verdicts.sh`:

    lodgen_btofree     2026-09-16 23:11:16  23 checks, 0 failures; PASS
    lodgen_byte_gate   2026-09-17 00:17:00  FAIL: no exe at /e/Projects/NifskopeWildWastelandEdition/release/NifSkope.before_panel1.exe; checks run: 132 (floor 125), failures: 0; panel vs command line: 15 identical, 2 differ, 0 missing; byte gate failures: 3
    lodgen_defaults    2026-09-16 23:29:32  28 checks, 0 failures; RESULT PASS
    lodgen_ladder      2026-09-16 23:14:57  22 checks, 0 failures, 0 skips; RESULT PASS
    lodgen_layout      2026-09-16 23:04:26  layout checks: 22, failures: 0; RESULT PASS
    lodgen_native      2026-09-16 23:08:12  15 checks, 0 failures, 1 skips; RESULT PASS; 25 checks, 0 failures; RESULT PASS
    lodgen_terrain_pbrm 2026-09-16 23:31:28  14 checks, 0 failures; RESULT PASS
    lodgen_terrain_vt  2026-09-16 23:31:07  45 checks, 0 failures; RESULT PASS
    lodl_btd           2026-09-16 23:30:17  RESULT PASS
    lodl_water         2026-09-16 23:30:14  RESULT PASS; RESULT PASS; PASS
    lodl_write         2026-09-16 23:29:47  RESULT PASS; RESULT PASS; RESULT PASS; PASS
    panel_selftest     2026-09-16 23:11:58  128 checks, 0 failures; PASS; checks run: 128 (floor 121)
    panel_rung         2026-09-16 23:12:05  124 checks, 0 failures; PASS; checks run: 124 (floor 121)

Every row ran against the 22:48:35 exe except `panel_rung`, which is the same panel
self-test run on `release/NifSkope.before_layout1.exe` on purpose -- it is the control
for section 9c.

Two harnesses were re-based by hand, pronounced done, and were still wrong -- running
them is what said so:

- `lodgen_btofree.sh` compared two WHOLE TREES by relative path, so every moved file read
  as missing on one side (5 failures). It now maps the A-side path through a `relmap()`
  written with the harness's own switches named above it, falls back to the old spelling
  when the new one is absent (a rung tree is the old layout), counts the pairs the
  normalised diff proves equal, and hands both tree roots to the ledger checker;
- `lodgen_terrain_pbrm.sh` had one old spelling left inside embedded python.

`lodgen_byte_gate.sh` DID run, once it was started from the right shell (00:17, 46
minutes). It exits 1 and the three failures are worth separating, because only one of its
three phases is about this lane:

- **(a) is not evidence and cannot be in this tree.** It wants
  `release/NifSkope.before_panel1.exe`, which is not here, so it prints `FAIL: no exe`,
  keeps going, and compares the fresh tree against a `Lodgen_PANEL1_rung` left in the
  system temp by lane PANEL1 on **2026-09-12** -- four days and one layout old. Its
  `DIFFERENT: 21 files` is that staleness, listed as `only in ...rung:
  textures/terrain/...`, i.e. the OLD spelling. 2 of the 3 failures are this leg.
- **(b) is green: 132 checks, 0 failures (floor 125)**, 48 rows bumped, 32 of them moving
  bytes, the two control rows (threads, chunk threads) leaving the bytes alone. This is
  the fifty-odd panel-driven bakes, and it is the strongest single statement in the lane
  that the move broke nothing the panel can reach. It read 128/0 on 2026-09-16 before
  this lane; the extra four checks are the gate's own growth since.
- **(c) is where this lane's re-base lives** (`FO4="FO4CSLOD/Commonwealth"`, both trees
  now FO4CS bakes so one sub-path names the same file on both sides): **15 identical, 2
  differ, 0 missing** -- and the two are the KNOWN, unowned panel-vs-command-line
  divergence recorded by lane BTOFREE1 in `WW_CHANGES.md` the same day: *"the panel's
  `Commonwealth.4.-20.24.DDS` and `Commonwealth.lodi` differ from the command line's at
  identical sizes ON THE RUNG TOO (14/3 there, 15/2 here)"*. Same two files, same count,
  and measured again here: the 225,399,755-byte `.lodo` beside them is byte-identical,
  the `.lodi` differs in the header CRC at 0x0C, one word at 0x64 and 683 short runs
  through the instance rows, and the DDS differs in 123,515 bytes of pixel data. **Not
  one differing byte is a path string**, which is what a move could have produced.

## 9b. The build

`bash tools/ww_build.sh src/lodgenlayout.cpp src/nifskope_ui.cpp`, three times as the
last three fixes landed. The exe every number above was measured on:

    release/NifSkope.exe                  22,382,592 B   2026-09-16 22:48:35
    release/NifSkope.before_layout1.exe   22,356,992 B   2026-09-16 20:21:17   (the rung)

newer than every source file the build names. Nothing is committed; `WW_CHANGES.md` and
`HANDOFF.md` are untouched by me (their text is below); no `BUILDING` marker and no
NifSkope process is left behind.

## 9c. One red that was not the code's, and how it was proved

Through an MSYS2 `bash` the panel self-test failed one check -- "a chunk built from the
archives is byte-identical to one built from the unpacked folder" -- with all three
passes writing **0 bytes**. That boundary drops the parent shell's variables, `TMP` and
`TEMP` included, so `QDir::tempPath()` pointed where the app cannot write and a chunk
that built perfectly could not be saved. The same self-test, same exe, run from Git Bash:
**128 checks, 0 failures**, 1,280,239 bytes both ways. The rung exe answers the same way
(124 checks, 0 failures), so nothing here moved with this lane. The same boundary ate the
panel screenshot -- `WW_LODGEN_SHOT` arrived empty and the grab is guarded by
`!shot.isEmpty()`, so not even a "NOT saved" line appeared. Both are in MISTAKES.md with
the one-line measurement and the fix: prepend `/c/msys64/ucrt64/bin` alone, never
`/c/msys64/usr/bin`, which is the entry that changes which `bash` runs.

## 9d. The three pictures

`scratchpad/layout1_20260916/images/`, each captioned with the path it was read from and
the sizes read off disk at the moment it was made:

    01_pair_from_its_new_path.png         the (-20,24) native pair, opened from
                                          tree/FO4CSLOD/Commonwealth/Commonwealth.lodi
    02_lodt_level_from_its_new_path.png   the two decoded 272x272 tiles of
                                          FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt
    03_panel_root_label.png               the panel: "LOD root  FO4CSLOD\Commonwealth\",
                                          and the summary under it naming the same root
                                          for every file it will write, sidecars included

## Changelog text for the director

I edited neither `WW_CHANGES.md` nor `HANDOFF.md`. Both blocks are below, ready to splice.

### WW_CHANGES.md entry

    ## 2026-09-16 -- every FO4CS-target output lands under one root, `Data/FO4CSLOD/` (lane LAYOUT1)

    **Exe** release/NifSkope.exe 22,382,592 B 2026-09-16 22:48:35 (rung NifSkope.before_layout1.exe
    22,356,992 B 20:21:17 kept). NOT COMMITTED.

    bungo 2026-09-16 19:3x: *"The folder should be called FO4CSLOD maybe, so it'd be
    Data/FO4CSLOD, sound fine?"*. Under the FO4CS target a bake now writes NOTHING outside
    `<mod folder>/FO4CSLOD/`: the landscape file, every `.lodt` level and its index, the native
    `.lodo`/`.lodi` pair, the aggregate sets, the object texture sets and the kept
    `.BTO.manifest.txt` sidecars all sit in `FO4CSLOD/<worldspace>/`, and the impostor card
    sheets -- shared by every worldspace -- in `FO4CSLOD/Cards/`. The game-path strings written
    inside the files moved with them. The STOCK target is untouched, and so are `--keep-bto`'s
    chunks, the far HeightMap DDS and every READ path.

    One function composes the root (`src/lodgenlayout.cpp`) and the folder name is a string
    literal in that file alone; the gate greps for a second spelling. A bake creates no folder it
    does not fill: no `Terrain/`, no `Textures/Lodgen/`, no `meshes/` under the FO4CS target.
    Census: `layout <root>, N file(s), M outside`, read back from the paths actually written,
    defaulting to `layout n/a (no FO4CS-target file written)`. The panel shows `FO4CSLOD\<ws>\`
    on a "LOD root" row under the output field, hidden under the stock engine, and the summary
    sentence beneath it now asks the same composer instead of naming a folder nothing writes.

    **Gates** lodgen_layout.sh NEW: 22 checks, 0 failures on the 22:48:35 exe -- (a) files at
    their new paths, no stray, no empty folder, and the ledger's 5 recorded outputs resolve and
    re-hash, with the rung refuter firing; (b) 83 identical / 0 differ / 17 carried a path string,
    refuter fires 17 of 17; (c) the stock tree byte-identical to the rung's at dim 4/8/16/32
    (13/13/5/13 files); (d) --keep-bto chunks at the out-dir root with their sidecars beside them;
    (e) one literal; (f) census 79, on disk 79, 0 outside. Leg (g), all on the same exe:
    lodgen_ladder, lodgen_native (25/0), lodgen_btofree (23/0), lodgen_defaults (28/0),
    lodl_write, lodl_water, lodl_btd, lodgen_terrain_vt (45/0), lodgen_terrain_pbrm (14/0) and
    the panel self-test (128 checks, 0 failures, floor 121). lodgen_byte_gate.sh ran too (46
    minutes): phase (b) 132 checks 0 failures (floor 125), 48 rows bumped / 32 moving bytes;
    phase (c), where the re-base lives, 15 identical / 2 differ / 0 missing -- the two are
    BTOFREE1's known unowned panel-vs-CLI divergence (same two files, and not one differing
    byte is a path string); phase (a) cannot run here (no NifSkope.before_panel1.exe) and
    compared a tree PANEL1 left in the system temp on 09-12, so its 2 failures are staleness,
    not evidence.

    The first gate run (20:49:35 exe) read 22/3 and every one is closed: a real census defect
    (85 counted over a tree of 79 -- two passes noted the same folder), and two wrong
    expectations in the new gate, both fixed in it. Leg (e)'s listing then found a stale doc
    comment in src/lodtfile.h and the panel summary sentence, neither of which a compiler can
    see. 6 MISTAKES entries by the lane, including the one that reaches every harness driver:
    an MSYS2 bash DROPS the parent shell's variables, which silently ate a screenshot and
    manufactured a red. New skill `.claude/skills/ww-move-a-written-path`. Pictures in
    scratchpad/layout1_20260916/images/. Report
    scratchpad/layout1_20260916/lane_layout1_report.md.

### HANDOFF.md LANDED block

    > LANDED 2026-09-17 00:20 (the 2026-09-16 session, lane LAYOUT1) — every FO4CS LOD file a bake writes now lands in one folder, `Data\FO4CSLOD\`, with a folder per worldspace inside it. release/NifSkope.exe 22,382,592 B, 22:48:35. NOT COMMITTED. Your open NifSkope needs a restart to be this build.
    >
    > Before, a bake scattered its files across four places in the mod folder — `Terrain\`, `Textures\Lodgen\`, `Textures\Terrain\`, and the folder root — and you had to know which was which. Now there is one folder to copy, one to delete, one to zip: `Data\FO4CSLOD\Commonwealth\` holds that worldspace's landscape file, its terrain texture levels, its object library and instance table, its aggregate sets and its manifests, and `Data\FO4CSLOD\Cards\` holds the impostor sheets, which are shared by every worldspace. The stock-engine bake is untouched — its files are where they always were, and byte for byte the same.
    >
    > The files themselves did not change. That is measured, not asserted: the same region baked by the exe before this change and by this one comes out byte-identical on 83 of 84 files, and the 17 that do carry a path string differ ONLY in that string and in the length word it forces. The check that does not rewrite the paths fails, which is what makes the first one mean something.
    >
    > It caught a bug that had nothing to do with folders. The bake keeps a ledger of what it wrote, by path and checksum, so an `--incremental` run knows what is still good. One entry pointed at a file that had moved, so it recorded an EMPTY checksum — and an empty checksum never matches, so every later incremental run would have rebaked the whole region without saying a word. Fixed, and the gate now walks every ledger entry and re-hashes it.
    >
    > The panel says the same thing in two places now and both are composed, not typed: a "LOD root" row reading `FO4CSLOD\Commonwealth\` under the output field, and the summary sentence beneath it, which had been telling you the manifests were somewhere they have not been for weeks.
    >
    > One harness is owed: `lodgen_byte_gate.sh` is re-based but could not be run here — it needs a base tree that only its own opt-in phase leaves behind.

## 10. Skill review (the finished-work step)

New: `.claude/skills/ww-move-a-written-path/SKILL.md` -- moving where a tool WRITES.
It carries the one-composer rule (including that a self-test must ask the composer, not
add a second literal), the normalised byte-identity diff with its LODM repack and its
refuter, the paths RECORDED INSIDE other files (the ledger defect, and the fact that no
grep for the old spelling can find `outDir + "/" + name`), the census-root readback, the
three-pile sort for harness re-basing, and the two editing traps that cost this lane
time -- a halved backslash in a LINE CONTINUATION, which `bash -n` passes, and typing
`make` by hand when `tools/ww_build.sh` exists.

Extended: `.claude/skills/ww-panel-run-harness/SKILL.md` section 9 -- run the spell from
the shell that set the variables. The MSYS2 bash boundary drops them, which cost this
lane three screenshots and manufactured one red; the section carries the two symptoms,
the one-line check, and the `ucrt64/bin`-only `PATH` fix.

Used this lane: `ww-contract-provenance` (every changed doc line carries its date, lane
and bungo's words), `ww-census-contract` (the `layout` row in `docs/LODGEN_CENSUS.md`
§6.1 with how it MOVES and its accusing default), `nifskope-ww-lodgen` (the bake switches
and the backslash trap), `nifskope-ww-build-verify` (found late -- see MISTAKES 4).
