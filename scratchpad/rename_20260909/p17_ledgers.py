# Patch 17 -- WW_CHANGES.md (MIXED line endings, spliced in binary, CR count
# asserted unchanged) and MISTAKES.md (LF-only).
import os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")

# ------------------------------------------------------------- WW_CHANGES.md
P = "WW_CHANGES.md"
b = open(P, "rb").read()
CR = b.count(b"\r")
HEAD = b"# NifSkope \xe2\x80\x94 Wild Wasteland Edition: Change Log\n\n"
assert b.startswith(HEAD), b[:60]

entry = """## 2026-09-09 - THE FINAL FILE NAMES: `.lodl` land, `.lodt` textures, `.lodo` objects

`src/lodtfile.{h,cpp}`, `src/io/lodvfile.{h,cpp}`, `src/btdterrain.{h,cpp}`,
`src/nifcli.cpp`, `src/lodgen.cpp`, `src/lodgenmanager.cpp`,
`tools/bake_impostor_cards.sh`, five renamed files under `tests/spells/`, every
`docs/LODGEN_*.md`, the FO4CS handoff package.
**CODE AND DOCS ONLY - BUILD PENDING.** Lane OFFSCREEN's `src/nifskope_ui.cpp`
was finished but unbuilt when this lane ended, so no build was started (the
brief's gate). `g++ -fsyntax-only` on all six changed translation units: RC=0
each. NOTHING was renamed in bungo's mod folder - that step needs the new exe to
verify each file after the move. Resume: `scratchpad/rename_20260909/PENDING.md`.

**bungo's ruling, final and approved.** `.lodl` = LAND (heights, AO, LTEX blend,
colour, water, ground cover, overview - the v2 file, what `.lodt` held until
today). `.lodt` = TEXTURES (the terrain sheets per pyramid level, what `.lodv`
held). `.lodo` = OBJECTS (the FO4CS-native mesh library, which the 16:1x ruling
had called `.lodg`). `.lodi` = instances, `.lodm` = materials. `.lodv` and
`.lodg` are retired. `.bto`/`.btr` stay the stock bake.

**`.lodt` is REPURPOSED, not renamed, and that is the whole engineering problem.**
An extension that meant one format yesterday and a different one today is the
setup for a silent misparse, so:

* the LAND file keeps its magic, `LODT` on disk (`LODL_MAGIC`, promoted out of
  `lodtfile.cpp`'s anonymous namespace into `lodtfile.h`). **Not one byte moved**
  - the gate on the rename is that a `.lodl` is byte-identical to the `.lodt`
  the same worldspace wrote yesterday, and `lodl_write.sh` still asserts
  `magic is still LODT after the .lodl rename`;
* the TEXTURE container takes a NEW magic, `LDTX` (`LODTEX_MAGIC`,
  `0x5854444C`), where it was `LODV`. No `.lodv` was ever written to disk
  anywhere, so nothing needs converting;
* **each reader refuses the other's file BY NAME.** `LodtFile::open` handed
  `LDTX` says *"refused: <name> is a terrain TEXTURE file (.lodt, magic LDTX),
  not a .lodl landscape file"*; `lodvValidate` handed `LODT` says *"this is the
  whole-worldspace LANDSCAPE file ... open it as .lodl"*, and handed the retired
  `LODV` says so too and asks for a re-bake. Neither says only "bad magic".

**Every command, flag, environment variable and panel string followed, and the
retired spellings REFUSE rather than fall through to a generic error**, because
the shell history and the scripts that carry them were written for the other
format:

| was | is | the retired spelling now |
|---|---|---|
| `lodt <file.lodt>` | `lodl <file.lodl>` | *"the 'lodt' command is retired ... use 'lodl <file.lodl>'"* |
| `--lodt <dir>` | `--lodl <dir>` | *"--lodt is retired ... use --lodl <dir>"* |
| `--lodv-check FILE` | `--lodt-check FILE` | *"--lodv-check is retired ... use --lodt-check"* |
| `WW_LODT_VERSION` | `WW_LODL_VERSION` | the writer FAILS and names the new one |
| `WW_LODT_REGION` / `_PLANE` | `WW_LODL_REGION` / `_PLANE` | `qCritical` REFUSED line, then ignored |
| `lodt: <path>` on stdout | `lodl: <path>` | - |
| `lodv ok 1` / `lodv <key>` | `lodt ok 1` / `lodt <key>` | - |

Panel rows: *Landscape file (.lodl)*, *Terrain virtual texture (.lodt)*, and the
summary sentence now names `Terrain\\<ws>.lodl`. The panel's objectNames and its
`LodGeneration/lodt` settings key were deliberately NOT renamed: the first is
how `src/nifskope_ui.cpp`'s self-test finds the rows (another lane's file), the
second would silently reset bungo's own ticks.

**A NEW GATE, both directions, with a control on each side.**
`tests/spells/lodl_write.sh` gained a refusal section: a real `.lodl` must still
open (control), a 0x98-byte `LDTX` header must be refused with the words
*TEXTURE file* in it, the shipped Commonwealth landscape file copied to
`old_meaning.lodt` must be refused by `--lodt-check` with the word *lodl* in it,
each of the three retired spellings must fail AND name its replacement, and the
renamed file must still `--verify-only` at 0 mismatches.
`tests/spells/lodgen_terrain_vt.sh` does the real-file half: the actual baked
`Commonwealth.VT.8.lodt` handed to the `lodl` route.
Renamed with `git mv`: `lodt_write.sh` -> `lodl_write.sh`, `lodt_open.sh` ->
`lodl_open.sh`, `lodt_btd.sh` -> `lodl_btd.sh`, `lodt_open_authority.py` ->
`lodl_open_authority.py`, `docs/LODGEN_NATIVE_LODG_LODI.md` ->
`docs/LODGEN_NATIVE_LODO_LODI.md`.

**Three other things owed in these files, done:**

1. **`--candidates trees` meant `tree || missing`** (`src/nifcli.cpp`), which is
   a SUPERSET of the default, not a tree list: 14 of a 33-candidate Sanctuary
   run were shacks and rock cliffs (measured by another lane, `HANDOFF.md` bug
   intake 3). It is `want = tree;` now, and the comment in
   `tools/bake_impostor_cards.sh` says so.
2. **The three wrong hex comments** beside correct vertex descriptors in
   `src/lodgen.cpp` (`WRITER_CHANGES_NEEDED.md` item 1). `0x300000000303` ->
   `0x0000300000000203`, `0x1B00000650405` -> `0x0001B00000430205`,
   `0x3B00000650406` -> `0x0003B00005430206`, each verified equal to its own
   integer, and the computed 32-byte object profile `0x0013F07006543208` written
   beside `OBJ_VERTEX_DESC_COLORS` where there was nowhere to read it off.
   **No value moved**; the patch script asserts all three integers survive.
3. **The qmake re-run owed since BUILD1** is NOT done, because there is no build
   slot - and it now matters more, not less: `lodtfile.cpp` gained an include of
   `io/lodvfile.h` and `lodvfile.cpp` one of `lodtfile.h`, and qmake freezes its
   dependency lists at generation time. The resume runs `qmake` before `make`
   and then greps `Makefile.Release` for both new dependencies.

**The provenance footers were re-derived, not merely re-stamped.** Every
`sha256`/bytes/lines row for a source this lane touched was recomputed, and every
line number in `LODGEN_BTD_FORMAT.md` (45 rows) and `LODGEN_TERRAIN_VT.md` (16)
was re-found from its own anchor text against the current file - 54 of 61 moved,
and several had been stale since before this lane (the seam-maximum row pointed
at 862 and the anchor sits at 1066). Two rows' anchors no longer exist at all
because another lane extracted `lodtHeightWord`; those were rewritten to name the
helper. The procedure is now the skill `ww-contract-provenance`.

**What is NOT done, and who owns it:** the GUI open route.
`src/nifskope.cpp` (five suffix comparisons and the file-type list) and one line
of `src/nifskope_ui.cpp`'s `WW_LODGEN_TEST` belong to another lane; the exact
edits are written out in `scratchpad/rename_20260909/GUI_CHANGE_NEEDED.md`.
Until that line lands, `tests/spells/lod_generation.sh` fails exactly one check
- *"with one, the panel says what it will write"* - and that is expected.

"""
out = HEAD + entry.encode("utf-8") + b[len(HEAD):]
assert out.count(b"\r") == CR, (out.count(b"\r"), CR)
open(P, "wb").write(out)
print("OK", P, len(out), "bytes, CR", out.count(b"\r"), "(was", CR, ")")

# --------------------------------------------------------------- MISTAKES.md
P = "MISTAKES.md"
b = open(P, "rb").read()
assert b.count(b"\r") == 0
s = b.decode("utf-8")
MARK = "Newest at the top.\n\n"
assert s.count(MARK) == 1
m = """## 2026-09-09 -- a FUZZY anchor match was about to write a line number that reads as verified

- **The provenance re-derivation pass fell back to a shortened prefix when the
  full anchor text was not found, and the first thing that fallback produced was
  wrong.** `docs/LODGEN_BTD_FORMAT.md` said the height encoding was at
  `lodtfile.cpp:1058-1059`, anchored on
  `const double v = double( hh ) / double( quantum ) + 32767.0;`. That
  expression no longer exists anywhere -- lane TERRAINFIX replaced it with the
  helper `lodtHeightWord` earlier the same day -- so the exact match failed, the
  60-character prefix matched a different line, and the script offered
  `1281-1282`.
- **What was true:** the claim's anchor was stale in CONTENT, not merely in
  position, and the honest repair was to re-anchor the row on the new helper at
  `67-70`, which is what was written.
- **How it was found:** the derived number was checked by hand with a `grep -n`
  of the full anchor before applying, and the grep returned nothing at all.
- **The rule:** an anchor match is EXACT and UNIQUE or it is not a match. A
  softened match is worse than a stale line number, because a stale number
  announces itself the moment a reader looks and a plausible wrong one does not.
  The pass now prints `MISSING` and `AMBIGUOUS` and refuses to rewrite either,
  and multi-site rows (`422, 428, 533, ...`) are re-derived by hand. This is
  step 3 of the skill `ww-contract-provenance`, written this session.

## 2026-09-09 -- a lane edited two files its own brief did not list

- **Lane RENAME's brief listed the files it owned and `src/io/lodvfile.{h,cpp}`
  were not among them, yet the work it was given -- "texture sheets written and
  read as `.lodt` with a NEW magic" -- lives in exactly those two files and
  nowhere else.** They were edited.
- **What was true:** the list was a survey miss, not a boundary. The brief named
  ONE other lane and ONE set of files to stay out of (`src/nifskope.cpp`,
  `nifskope.h`, `nifskope_ui.cpp`, `glview.*`), and those were left alone and
  written up in `scratchpad/rename_20260909/GUI_CHANGE_NEEDED.md` instead. No
  other lane was in `lodvfile`.
- **How it was found:** grepping for every producer of the texture container
  before starting, which is `docs/MISTAKES.md`'s own "a defect is a property of
  an OUTPUT" rule applied to a rename.
- **The rule:** when the work names a behaviour that the file list cannot
  deliver, say so in the report and name the files taken -- do not silently
  widen the list, and do not silently deliver half the work. Recorded here so
  the director can reconcile rather than discover it in a diff.

"""
s = s.replace(MARK, MARK + m, 1)
out = s.encode("utf-8")
assert out.count(b"\r") == 0
open(P, "wb").write(out)
print("OK", P, len(out), "bytes, CR", out.count(b"\r"))
