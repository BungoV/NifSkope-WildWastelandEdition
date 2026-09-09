# Lane RENAME — report

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, **no commits**.
**BUILD PENDING** — the brief's gate failed its second condition (§3). All code,
harnesses and docs are finished and on disk; the resume is
`scratchpad/rename_20260909/PENDING.md`.

Read first, in order: `CONSTITUTION.md`; the `HANDOFF.md` top block (the FINAL
FILE NAMES ruling and the BUG INTAKE block); `scratchpad/handoff_fo4cs/README.md`
and `WRITER_CHANGES_NEEDED.md`; `scratchpad/lane_contracts_report.md`;
`scratchpad/lane_terrain_fix_report.md`. Skills loaded: `nifskope-ww-lodgen`,
`nifskope-ww-build-verify`, `nif` (§6).

---

## 1. Every rename, file by file

Every edit was made by a patch script under `scratchpad/rename_20260909/`
(`p01`…`p17`), each asserting its anchor count before replacing and its CR count
after. All sources and harnesses are LF-only and stayed so; `WW_CHANGES.md` is
mixed and its CR count is unchanged at 19,020.

### Sources

| file | lines before → after | what moved |
|---|---|---|
| `src/lodtfile.h` | 232 → 252 | `LODL_MAGIC` promoted out of the .cpp so the texture reader can name this format; the class doc says `.lodl` and says the C++ names did NOT move; `WW_LODL_VERSION` |
| `src/lodtfile.cpp` | 1,682 → 1,706 | writes `Terrain/<EDID>.lodl`; 4 constants `LODT_*` → `LODL_*`; `WW_LODT_VERSION` **refused**, `WW_LODL_VERSION` honoured; `LodtFile::open` refuses `LDTX` **by name** |
| `src/io/lodvfile.h` | 173 → 196 | `LODTEX_MAGIC` = `0x5854444C` (`LDTX`) and `LODTEX_MAGIC_RETIRED_LODV`; the doc block explains the repurposing |
| `src/io/lodvfile.cpp` | 724 → 744 | writes and validates `LDTX`; rule 2 now refuses `LODT` (names the landscape file) and `LODV` (names the retirement) separately; `describe` prints `magic LDTX` |
| `src/lodgen.cpp` | 8,283 → 8,286 | the VT container path `%1/%2.VT.%3.lodt` and its index string; the three hex comments (§3 of this section) |
| `src/btdterrain.h` | 178 → 185 | prose `.lodt` → `.lodl`; `WW_LODL_REGION` / `WW_LODL_PLANE` |
| `src/btdterrain.cpp` | 1,350 → 1,366 | the same; the retired `WW_LODT_*` are named in a `qCritical` REFUSED line and then ignored; the scene note prints `lodl <file>:` |
| `src/nifcli.cpp` | 5,885 → 5,913 | command `lodl`, flags `--lodl` and `--lodt-check`; three retired spellings refuse by name; `--candidates trees` is tree-only; help text |
| `src/lodgenmanager.cpp` | 2,392 → 2,392 (12 strings) | visible panel strings only — objectNames and `LodGeneration/lodt` deliberately untouched (§4) |

### The command surface

| was | is | the retired spelling |
|---|---|---|
| `lodt <file.lodt>` | `lodl <file.lodl>` | refused, names `lodl` |
| `--lodt <dir>` | `--lodl <dir>` | refused, names `--lodl` |
| `--lodv-check FILE.lodv` | `--lodt-check FILE.lodt` | refused, names `--lodt-check` |
| `WW_LODT_VERSION` | `WW_LODL_VERSION` | the write FAILS with the new name in the message |
| `WW_LODT_REGION` / `WW_LODT_PLANE` | `WW_LODL_REGION` / `WW_LODL_PLANE` | `qCritical` REFUSED, then ignored |
| stdout `lodt: <path>` | `lodl: <path>` | — |
| stdout `lodv ok 1`, `lodv <key>` | `lodt ok 1`, `lodt <key>` | — |
| `Landscape file (.lodt)` | `Landscape file (.lodl)` | — |
| `Terrain virtual texture (.lodv)` | `Terrain virtual texture (.lodt)` | — |

### Harnesses, tools and docs

Renamed with `git mv` (a plain rename where the file was untracked, which is
most of `tests/spells/`):

* `tests/spells/lodt_write.sh` → `lodl_write.sh` (349 → 454 lines; +105 is the
  new refusal gate, §4)
* `tests/spells/lodt_open.sh` → `lodl_open.sh` (346 lines, 18 edited; the
  fixture variable is `LODL` now)
* `tests/spells/lodt_btd.sh` → `lodl_btd.sh`
* `tests/spells/lodt_open_authority.py` → `lodl_open_authority.py` (237 → 239)
* `docs/LODGEN_NATIVE_LODG_LODI.md` → `docs/LODGEN_NATIVE_LODO_LODI.md`
  (`.lodg` → `.lodo` ×12, magic `LODG` → `LODO`, `lodgIdentity` →
  `lodoIdentity` ×5, and the superseded ~16:1x ruling marked superseded)

Also updated: `tests/spells/lodgen_terrain_vt.sh` (327 → 342, 24 container
references plus the real-file refusal check), `tests/spells/lodgen_vt_check.py`
(the independent decoder's magic assertion, `LODV` → `LDTX`),
`tests/spells/lod_generation.sh` (one comment),
`tools/bake_impostor_cards.sh` (153 → 170, the `CANDIDATES=trees` comment).

Documents: 11 `docs/LODGEN_*.md` swept — `.lodt` → `.lodl` ×26, `.lodv` →
`.lodt` ×15, `.lodg` → `.lodo` ×12, plus the flag, environment-variable and
harness-filename spellings. `LODGEN_BTD_FORMAT.md` and `LODGEN_TERRAIN_VT.md`
each gained a banner at the very top saying what moved, what did NOT (the bytes,
the C++ names) and why the texture container took a new magic.
`scratchpad/handoff_fo4cs/README.md` gained the whole family as one table plus
the one line an FO4CS reader must change; `WRITER_CHANGES_NEEDED.md` item 1 is
marked **DONE**.

**Provenance re-derived, not merely re-stamped** (the procedure is now the skill
`ww-contract-provenance`, §6). Every `sha256`/bytes/lines row for a source this
lane touched was recomputed — 9 rows across 6 documents — and every line number
in `LODGEN_BTD_FORMAT.md` (46 rows) and `LODGEN_TERRAIN_VT.md` (21) was re-found
from its own anchor text: **54 moved, 1 was already right, 12 were re-derived by
hand** (multi-site rows and two whose anchors no longer exist because another
lane extracted `lodtHeightWord`). Several had been stale since before this lane
— the seam-maximum row pointed at line 862 and its anchor sits at 1,066.

---

## 2. The magic, and the refusal design

**The land file keeps its magic.** `LODL_MAGIC = 0x54444F4C` still spells `LODT`
on disk. This is deliberate and load-bearing: the gate on the rename is that a
`.lodl` is **byte-identical** to the `.lodt` the same worldspace wrote yesterday,
and a magic change would have made that gate impossible to state. What used to
be `constexpr quint32 LODT_MAGIC` inside `lodtfile.cpp`'s anonymous namespace is
now `LODL_MAGIC` in `lodtfile.h`, so the texture validator can name it.

**The texture container takes a NEW magic**, `LODTEX_MAGIC = 0x5854444C`,
`LDTX` on disk, where it was `LODV`. That is what makes repurposing `.lodt`
safe: the extension is ambiguous, the bytes are not. Changing it costs nothing —
**no `.lodv` has ever been written to disk anywhere** (`handoff_fo4cs/README.md`
§5 audits that), so there is no file to convert. The retired `LODV` value is
kept as `LODTEX_MAGIC_RETIRED_LODV` for one purpose only: naming a stale
container instead of guessing at it.

**Three named refusals, not one "bad magic".**

| route | handed | says |
|---|---|---|
| `LodtFile::open` (the `.lodl` land reader) | an `LDTX` file | *"refused: `<name>` is a terrain TEXTURE file (.lodt, magic LDTX), not a .lodl landscape file"* |
| `lodvValidate` (the `.lodt` texture reader) | a `LODT` file | *"refused: this is the whole-worldspace LANDSCAPE file (magic LODT), which is called .lodl since 2026-09-09; .lodt names the terrain texture sheets now — open it as .lodl"* |
| `lodvValidate` | a `LODV` file | *"refused: this is a retired .lodv container (magic LODV) … re-bake it"* |

Each names the file the holder actually has and the route they should have
taken. `LodtFile::open` includes the file's own name, because the land route is
the one a user reaches by double-clicking.

The two headers now include each other's (`lodtfile.cpp` → `io/lodvfile.h`,
`lodvfile.cpp` → `lodtfile.h`) for exactly these two constants. **That is the
qmake trap** (§5): a NEW include is invisible to `make` until `qmake` reruns.

---

## 3. Gates — BUILD PENDING

The brief's gate was checked **once**, at the end of the lane:

```
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?     ->  rc=1   PASS
ls scratchpad/offscreen_20260909/DONE                     ->  absent FAIL
```

Lane OFFSCREEN ended BUILD PENDING itself (`Fallout4.exe` was up while it ran)
and left `PENDING.md` in that folder instead of `DONE`. Its finished but
**unbuilt** +123 lines sit in `src/nifskope_ui.cpp`. So: **no build, nothing
under `release/` touched, no harness run, and bungo's installed files NOT
renamed** (§4). Never polled.

**What was run instead**, and it proves compilation and nothing else:
`g++ -fsyntax-only` on all six changed translation units, **RC=0 each**
(`scratchpad/rename_20260909/syntax.txt` has the exact command and the flags it
was lifted from). It says nothing about linking or behaviour.

**The gates, pre-registered in the brief, all still owed**, with the resume in
`scratchpad/rename_20260909/PENDING.md`:

| gate | state |
|---|---|
| `lodl_write.sh` — byte-identical land file under the new name | NOT RUN |
| the new refusal case, both directions | NOT RUN (written; §4) |
| `lodl_open.sh` — 23 checks, 0 failures | NOT RUN |
| `lodgen_terrain.sh` with the sheets written as `.lodt` | NOT RUN |
| `lodgen_identity.sh` — stock bake byte-identical | NOT RUN |
| the four-worldspace verify, 0 differing | NOT RUN |
| the qmake re-run and the surviving `lodtfile.h` dependency | NOT RUN — and now covers two NEW cross-includes as well |

**One expected red when it does run.** `tests/spells/lod_generation.sh` will
fail exactly one check, *"with one, the panel says what it will write"*: the
panel summary says `.lodl` now and the self-test in `src/nifskope_ui.cpp` still
looks for `.lodt`. That file belongs to another lane and the one-line fix is
written out in `scratchpad/rename_20260909/GUI_CHANGE_NEEDED.md` §2, together
with the five suffix comparisons and the file-type row in `src/nifskope.cpp`.
Any OTHER failure in that harness is this lane's.

### The refusal gate as written

`tests/spells/lodl_write.sh` gained a section with a control on **each** side,
because a refusal that fires on any error at all is not a check:

* CONTROL: a real `.lodl` must still open through `-no-gui lodl … --info`.
* the land route handed a 0x98-byte `LDTX` header must exit non-zero **and**
  its message must contain `TEXTURE file`.
* the shipped Commonwealth landscape file copied to `old_meaning.lodt` and fed
  to `--lodt-check` must be refused **and** the refusal must contain `lodl`.
* each of the three retired spellings (`lodt` the command, `--lodt`,
  `WW_LODT_VERSION`) must fail **and** name its replacement.
* the renamed file must still `--verify-only` at **0 mismatches**.

`tests/spells/lodgen_terrain_vt.sh` carries the real-file half: the actual baked
`Commonwealth.VT.8.lodt` handed to the `lodl` route, which must be named rather
than misparsed.

---

## 4. The installed files — NOT renamed, and why

Step 5 of the brief is explicitly *"after a green build"*, and the verify it
asks for (*"verify each with the new command"*) needs an exe that knows the new
command. Renaming five files and then not being able to check that each still
reads is the one outcome worse than leaving them alone. They stand as lane
BUILD1 left them at 17:27, version 2:

| file | bytes | becomes |
|---|---|---|
| `Commonwealth.lodt` | 35,953,294 | `Commonwealth.lodl` |
| `DLC03FarHarbor.lodt` | 9,195,933 | `DLC03FarHarbor.lodl` |
| `DiamondCity.lodt` | 53,148 | `DiamondCity.lodl` |
| `NukaWorld.lodt` | 7,182,356 | `NukaWorld.lodl` |
| `NukaWorldAmphitheater.lodt` | 38,303 | `NukaWorldAmphitheater.lodl` |

in `E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\`. The five
`*.lodt.bak-20260909` copies (2026-09-05, version 1, 35,953,286 / 9,195,806 /
53,220 / 7,182,348 / 38,104 bytes) **stay exactly as they are, name included** —
they are the way back to the pre-version-2 set and renaming them would make the
ladder unreadable. The `mv` loop and the per-file verify are step 5 of
`PENDING.md`.

**Still owed to bungo on these files, and not this lane's to decide:** the
shipped FO4CS reader pins `kVersion = 1u` and will refuse all five as they now
stand, whatever they are called (`handoff_fo4cs/README.md` §4). Renaming them
does not change that either way.

---

## 5. The three small owed items

1. **`--candidates trees` was `tree || missing`** — `src/nifcli.cpp`, now
   `want = tree;`. It was a SUPERSET of the default rather than a tree list,
   which is why 14 of a 33-candidate Sanctuary run were shacks and rock cliffs
   (`HANDOFF.md` bug intake item 3). The `CANDIDATES=trees` comment in
   `tools/bake_impostor_cards.sh` now says "TREES AND ONLY TREES" and records
   what it used to do.
2. **The three wrong hex comments** — `src/lodgen.cpp` lines 57, 1357, 1358.
   `0x300000000303` → `0x0000300000000203`, `0x1B00000650405` →
   `0x0001B00000430205`, `0x3B00000650406` → `0x0003B00005430206`. Each new hex
   was checked equal to its own integer in Python before it was written, and the
   patch script asserts all three integers still appear exactly once — **no value
   moved**. The computed 32-byte object profile `0x0013F07006543208` is now a
   comment beside `OBJ_VERTEX_DESC_COLORS`, where there was nowhere to read it
   off.
3. **The qmake re-run: NOT DONE, and it matters more now.** There was no build
   slot. It was owed because BUILD1 hand-patched `Makefile.Release` for the
   `lodtfile.h` dependency; this lane adds two NEW cross-includes
   (`lodtfile.cpp` → `io/lodvfile.h`, `lodvfile.cpp` → `lodtfile.h`), and qmake
   freezes its dependency lists at generation time, so `make` has no reason to
   rebuild either object when the other's header changes. `PENDING.md` step 1
   runs `qmake` before `make`; step 2 greps `Makefile.Release` for both new
   dependencies and for the `lodtfile.h` one BUILD1 patched in, which is the
   check that the hand patch is no longer standing in for anything.

---

## 6. Mistakes

Both written into `MISTAKES.md` at the root the moment they were recognised.

1. **A fuzzy anchor match was about to write a line number that reads as
   verified.** The provenance pass fell back to a 60-character prefix when the
   full anchor failed, and offered `lodtfile.cpp:1281-1282` for a claim whose
   code no longer exists anywhere (another lane replaced the expression with the
   helper `lodtHeightWord` earlier the same day). Found by grepping the full
   anchor by hand before applying — it returned nothing at all. The pass now
   requires an EXACT and UNIQUE match, prints `MISSING` and `AMBIGUOUS`, and
   refuses to rewrite either. A stale number announces itself; a plausible wrong
   one does not.
2. **This lane edited two files its brief did not list.**
   `src/io/lodvfile.{h,cpp}` are not in the brief's ownership list, but the work
   it was given — *"texture sheets written and read as `.lodt` with a NEW
   magic"* — lives there and nowhere else. They were edited. The brief named one
   other lane and one set of files to stay out of (`src/nifskope.cpp`,
   `nifskope.h`, `nifskope_ui.cpp`, `glview.*`); those were left alone and
   written up in `GUI_CHANGE_NEEDED.md` instead. Recorded so the director
   reconciles it rather than finding it in a diff.

**Not a mistake but for the director's eye.** `HANDOFF.md` (lines 82-84, 493-494)
and `docs/MISTAKES.md` (line 490) name `tests/spells/lodt_write.sh`,
`lodt_open.sh` and `lodt_btd.sh`, which no longer exist. Those are HISTORICAL
records of what was true when written and this lane did not rewrite them —
`HANDOFF.md` is the director's file. A reader pasting one of those commands gets
"file not found", which is loud rather than silent, but the handoff's live
sections should be updated when it is next rewritten.

---

## 7. Finished-work skill review (CONSTITUTION 1a)

**Loaded and used.** `nifskope-ww-lodgen` — the CLI table (which is where the
full inventory of flags and environment variables to rename came from, before
any source was opened), the byte-identity gates, and the editing traps: the
"write patch scripts with the Write tool, never a heredoc" rule and the
"measure line endings with Python byte counts" rule were both applied literally
to all seventeen patch scripts. `nifskope-ww-build-verify` — its build gate and,
in the end, only its *"when you CANNOT build"* section, which is the reason a
syntax pass was run at all and the reason `PENDING.md` puts `qmake` before
`make` and greps the dependency afterwards. `nif` — consulted for the vertex
descriptor decoding behind the three hex comments; no contradiction found.

**Written, and it is the one the brief asked for:
`ww-contract-provenance`** — lane CONTRACTS' five steps (hash and line-count
first, anchor text beside every line number, re-derive every number last from
the anchors, re-read the version constant last of all, diff the hash end to
end), plus two things this lane learned by doing it: the anchor match must be
EXACT and UNIQUE (mistake 1), and a MISSING anchor is a **content** question,
not a numbering one — the code it named is gone and the row must be re-anchored,
not renumbered. It also carries the backwards case, which is the one that
actually recurs: *a lane changed a source that contract pages already cite*.
Written to the LIVE tree
`E:\Projects\Claude\.claude\skills\ww-contract-provenance\SKILL.md` **and**
copied to the repo tree `.claude/skills/ww-contract-provenance/SKILL.md`, because
the two drift and a repo-cwd lane reads only the second (CONSTITUTION 1a). The
working script it points at is
`scratchpad/rename_20260909/p14_anchors.py`.

**A second candidate, and I am declining it with a reason.** "Rename a format's
extension across a tree" looked like a skill for about an hour. It is not:
the *mechanics* are one grep and a replace, and everything that made this round
hard was specific to `.lodt` being **repurposed** rather than merely renamed —
the new magic, the two-way named refusals, the byte-identity constraint that
forbade touching the land magic. The general lesson that survives is one
paragraph, and it belongs in `nifskope-ww-lodgen` beside the file family rather
than as a skill of its own: *when an extension changes meaning rather than
merely name, the new meaning gets a new magic and both readers refuse the other
by name; when it merely changes name, nothing in the bytes may move and the
gate is byte identity.* I have not spliced it, because `nifskope-ww-lodgen` is
loaded by every lane in this repo and rewriting it mid-round while other lanes
hold it is the collision `lane_terrain_fix_report.md` §4 already reported once
today. **Recommend the director splices that one paragraph.**

**A third, declined outright.** The seventeen patch scripts are a pattern
(assert the anchor count, replace, assert the CR count, report what is left),
but that pattern is already the `nifskope-ww-lodgen` editing-traps section and
the `nifskope-ww-build-verify` patch-with-a-script rule. Writing it a third time
would be the duplication rule 1a exists to prevent.

## Build (BUILD2), 2026-09-09

**`qmake` FIRST, as the resume asked**: `qmake NifSkope.pro` RC=0, then
`make -j2` RC=0, `release/NifSkope.exe` **18:43:13**, newer than every changed
source, `release/style.qss` in step. The three dependencies the resume named all
survive the regenerated `Makefile.Release`: `src/lodtfile.h` appears 7 times;
`GeneratedFiles/.obj/lodtfile.o` lists `io/lodvfile.h`;
`GeneratedFiles/.obj/lodvfile.o` lists `lodtfile.h`; and
`GeneratedFiles/.obj/btdterrain.o` lists `lodtfile.h` on its own now, so lane
BUILD1's hand patch is no longer standing in for anything. Every object that
includes `lodtfile.h` is newer than it.

**The GUI half was applied by this lane**, exactly as
`scratchpad/rename_20260909/GUI_CHANGE_NEEDED.md` wrote it: five one-line
changes plus three comment mentions in `src/nifskope.cpp` (all CRLF lines,
replaced at equal byte length, CR 9,370 and LF 10,493 both unchanged) and the
one self-test line in `src/nifskope_ui.cpp` (LF-only, unchanged counts). The
C++ names (`LodtWorldInfo`, `lodtQueryRegion`, `lodtPendingRegion`), the panel
objectNames and `LodGeneration/lodt` were left alone. **Two strings in
`src/nifskope.cpp` were NOT changed because the note did not list them** and are
owed to whoever owns that file: the log lines `qInfo() << "lodt terrain:"` and
`qWarning() << "lodt terrain:"` at ~10,096 and ~10,098 still say `lodt` about
the landscape route.

**bungo's installed set, renamed and read back one at a time.**

| file | bytes | `lodl ... --info` | `--verify-only` vs its plugin |
|---|---|---|---|
| `Commonwealth.lodl` | 35,953,294 | rc=0, cells [-96,-96]..[95,95], 48,960 blocks | **0 mismatched** of 36,864 samples; alpha 0 of 36,864, colour 0 of 36,864 |
| `DLC03FarHarbor.lodl` | 9,195,933 | rc=0, 26,286 blocks | **0 mismatched** of 5,568 |
| `DiamondCity.lodl` | 53,148 | rc=0, 226 blocks | **0 mismatched** of 256 |
| `NukaWorld.lodl` | 7,182,356 | rc=0, 5,684 blocks | **0 mismatched** of 69,696 |
| `NukaWorldAmphitheater.lodl` | 38,303 | rc=0, 154 blocks | **0 mismatched** of 128 |

All five in `E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\`, mtime
unchanged at 2026-09-09 17:27 (the rename moved the name, not the bytes). The
five `*.lodt.bak-20260909` copies were left exactly as they were, name included.

**`--candidates trees` measured** on Fallout4.esm, terrain region
(-20,24)..(-17,27): **19 candidates, and every one of them is under
`Landscape\Trees\`** -- three `TreeMapleForest`, seven `TreeMapleblasted` /
`TreeBlasted`, two `TreeElmForest`, two `BlastedForestBurntTreeUpright`,
`TreeHero01`, `TreeBlasted01Lichen`. The default filter returns **33** over the
same region, so the 14 shacks and rock cliffs of the bug report are gone.

| gate | result | numbers |
|---|---|---|
| `render_shot.sh` | **FAIL** | 28 checks, 6 failures. Sections 1-4 green; all six are section 5: each of `render_plain`, `bake_plain`, `bake_lod` recorded `2-3 of 4-5` window records `onscreen=1` and the outside sampler saw the window on `\\.\DISPLAY5`. Section 6's floors both fired and its identity check read `off fedab869884174fb / on fedab869884174fb` -- which proves nothing here, because both runs were on a screen |
| `lodl_write.sh` | PASS | 34 checks, 0 failures. `magic is still LODT after the .lodl rename`; heights round-trip exactly (36,864 samples, 0 mismatched); both refusal directions name the other format; the renamed file `--verify-only` at 0 mismatches |
| `lodl_open.sh` | PASS | **23 checks, 0 failures**, run against bungo's own renamed `Commonwealth.lodl`; whole-worldspace render coverage 0.0959, luminance SD 39.05 |
| `lodgen_terrain.sh` | PASS | 26 checks, 0 failures |
| `lodgen_identity.sh` | PASS | 8 checks, 0 failures; 406 objects shared between the rings, all with the same base and position |
| `lodgen_terrain_vt.sh` (the sheets as `.lodt`) | **FAIL** | 31 checks, 1 failure: V9a, the assembled colour sheet vs a direct bake -- same size (174,888 B both), different bytes. Not a naming or parse failure and not attributed to either lane: it is the `dominantBase` scoping question `scratchpad/specs_20260906/spec_terrain_vt.md` V9a was written for. NOT measured further |
| `lod_generation.sh` | PASS | **97 checks, 0 failures** -- the one expected red, *"with one, the panel says what it will write"*, is green now that the `src/nifskope_ui.cpp` line is in |
| `btd_terrain.sh` | PASS | 13 checks, 0 failures |
| `lodgen_impostor_cards.sh` | PASS | 10 checks, 0 failures (`RESULT PASS`); candidate `0004a074`, the chunk shrinks 1,280,239 -> 1,001,308 B when cards replace the meshes |

**Skipped, and why.** `lodl_btd.sh` and the Fallout 76 `.btd` route (a ~25-minute
conversion; the same reader is already exercised by `btd_terrain.sh` and by
`lodl_write.sh`'s round trip). `lodgen_octahedral.sh` (two real GUI bakes, ~3
min): `lodgen_impostor_cards.sh` covers the same candidate filter change and is
the harness the brief named first. `lodgen_texture_arrays.sh`,
`lodgen_card_arrays.sh`, `lodgen_merge.sh`, `lodgen_farring.sh`,
`lodgen_resources.sh`, `lod_channel_preview.sh` and every non-LOD harness: no
file either lane touched is on their path. The one real tree bake of
`offscreen_20260909/PENDING.md` step 6 was NOT run -- it is the 580-repaint run,
and section 5 says the window is still on a monitor, so running it would have
put the hazard on bungo's screen for no new information.

**mtimes, one table (CONSTITUTION 4).**

| artefact | time |
|---|---|
| `release/NifSkope.exe` (final) | 2026-09-09 18:43:13 |
| `release/style.qss` | 2026-09-09 18:43:14 (`cmp res/style.qss release/style.qss` equal) |
| every changed source | older than the exe; checked file by file over `git status` |
| bungo's five installed files | 2026-09-09 17:27 (content untouched, name only) |
| the gate logs | `scratchpad/build2_20260909/logs/`, 18:45-18:47 |

**`release/NifSkope.before.exe` was NOT deleted.** The brief conditions that on
the off-screen gate passing with its same-build comparison, and it did not, so
the before-picture is still the only picture of the pre-fix renderer. It sits in
`release/` (17,796,608 B, sha256 `5ca38e28...`).

**Nothing was committed** (CONSTITUTION 8).

### Finished-work skill review (BUILD2)

**Loaded and used.** `nifskope-ww-build-verify` -- the gated chain, make's own
exit code, the stylesheet copy, and above all *"A successful build is not a
consistent one"*, which is why `qmake` ran first and the three dependencies were
read back per object. `nifskope-ww-lodgen` -- the CLI table (`--lodl`,
`--verify-only`, the worldspace IDs) and the editing traps; every patch here was
a script file with an anchor count and a CR assert, never a heredoc.
`nifskope-ww-render-shot` -- the switches, and its off-screen section, which this
build disproved.

**Amended, in the LIVE tree** `E:\Projects\Claude\.claude\skills`:
`nifskope-ww-render-shot`. Its first section said, as fact, that a headless run
never puts a window on a screen. That is false as built, and the skill is what
every lane in this repo reads before a bake. It now opens with what is TRUE as
built -- the fix is inert, the strobe is live, and the one-line change that does
move the window off screen stops rendering altogether -- and its front-matter
description no longer advertises the off-screen behaviour. No repo-tree copy of
that skill exists, so there was nothing to reconcile.

**Written:** `nifskope-ww-resume-pending` -- "build and gate a BUILD PENDING
lane". Two lanes have now done this exact job in one day (BUILD1, BUILD2) and
both re-derived the same steps from memory: the PENDING/`*_CHANGE_NEEDED` read
order, applying another lane's owed change at equal byte length, qmake before
make with the dependency read BACK per object (the `awk` walk, because `grep -A3`
misses a dependency ten continuation lines down), the exe-newer sweep over every
changed file rather than the one you edited, the sequential harness chain with
its summary echo, and the four documents that have to stop saying "not built".
It also carries the rule this lane learned the expensive way: **a build lane that
finds a design failure measures the cause and stops.** Written to the live tree
and copied to the repo tree `.claude/skills/`, because a lane whose cwd is this
repo reads only the second.

**Declined, with the reason.** A skill for "compare two window placements from
`ww_headless_windows.log`" -- one file, four lines, and the reading of it is now
a paragraph in `nifskope-ww-render-shot` where a lane will actually meet it. And
a skill for the `--verify-only` sweep over bungo's five installed worldspaces:
the command is one line in `nifskope-ww-lodgen` already, and the only thing this
lane added -- that the mod folder itself is the `--lodl` argument, because the
writer appends `Terrain/` -- belongs beside that line rather than in a document
of its own.
