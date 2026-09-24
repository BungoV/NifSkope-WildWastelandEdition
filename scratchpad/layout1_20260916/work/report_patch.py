"""lane LAYOUT1 (2026-09-16): put the FINAL exe and the FINAL counts into the
report.  Sections 8 and 9 and the changelog are replaced whole; section 10
stays.  The harness table is READ OFF THE LOGS by verdicts.sh (stdin), never
typed here."""
import sys

P = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/layout1_20260916/'
     'lane_layout1_report.md')
d = open(P, 'rb').read().decode('utf-8')
table = sys.stdin.read().rstrip('\n')
table = '\n'.join('    ' + l for l in table.split('\n'))

start = d.index('## 8. The gate run')
end = d.index('## 10. Skill review')

new = """## 8. The gate run, on the final exe

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
   the panel's summary row naming the HeightMap DDS under `Textures\\Terrain\\`, which by
   the ruling did NOT move. The list now exempts comments in any file and the HeightMap
   by name, each with its reason written above the filter.

Leg (e)'s own listing is what then found the stale doc comment in `src/lodtfile.h` and
the panel summary sentence of section 6 -- neither of which any compiler would have
complained about.

## 9. Leg (g): every re-based harness, and the panel

Read off the logs on disk (`scratchpad/layout1_20260916/work/harness/`), newest run per
harness, printed by `work/verdicts.sh`:

%s

Everything above ran against the 22:48:35 exe.

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
    03_panel_root_label.png               the panel: "LOD root  FO4CSLOD\\Commonwealth\\",
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
    defaulting to `layout n/a (no FO4CS-target file written)`. The panel shows `FO4CSLOD\\<ws>\\`
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

    > LANDED 2026-09-16 23:xx — every FO4CS LOD file a bake writes now lands in one folder, `Data\\FO4CSLOD\\`, with a folder per worldspace inside it. release/NifSkope.exe 22,382,592 B, 22:48:35. NOT COMMITTED. Your open NifSkope needs a restart to be this build.
    >
    > Before, a bake scattered its files across four places in the mod folder — `Terrain\\`, `Textures\\Lodgen\\`, `Textures\\Terrain\\`, and the folder root — and you had to know which was which. Now there is one folder to copy, one to delete, one to zip: `Data\\FO4CSLOD\\Commonwealth\\` holds that worldspace's landscape file, its terrain texture levels, its object library and instance table, its aggregate sets and its manifests, and `Data\\FO4CSLOD\\Cards\\` holds the impostor sheets, which are shared by every worldspace. The stock-engine bake is untouched — its files are where they always were, and byte for byte the same.
    >
    > The files themselves did not change. That is measured, not asserted: the same region baked by the exe before this change and by this one comes out byte-identical on 83 of 84 files, and the 17 that do carry a path string differ ONLY in that string and in the length word it forces. The check that does not rewrite the paths fails, which is what makes the first one mean something.
    >
    > It caught a bug that had nothing to do with folders. The bake keeps a ledger of what it wrote, by path and checksum, so an `--incremental` run knows what is still good. One entry pointed at a file that had moved, so it recorded an EMPTY checksum — and an empty checksum never matches, so every later incremental run would have rebaked the whole region without saying a word. Fixed, and the gate now walks every ledger entry and re-hashes it.
    >
    > The panel says the same thing in two places now and both are composed, not typed: a "LOD root" row reading `FO4CSLOD\\Commonwealth\\` under the output field, and the summary sentence beneath it, which had been telling you the manifests were somewhere they have not been for weeks.
    >
    > One harness is owed: `lodgen_byte_gate.sh` is re-based but could not be run here — it needs a base tree that only its own opt-in phase leaves behind.

""" % table

d = d[:start] + new + d[end:]
open(P, 'wb').write(d.encode('utf-8'))
print('report patched, %d bytes' % len(d.encode('utf-8')))
