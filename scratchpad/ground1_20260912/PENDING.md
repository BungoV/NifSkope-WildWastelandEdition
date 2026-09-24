# Lane GROUND1 resume steps -- SUPERSEDED, the lane is COMPLETE

**2026-09-12 12:2x: both parts landed and every gate has run. Nothing below is
a pending step any more; it is kept only as a record of what the mid-lane state
looked like.** Read `scratchpad/lane_ground1_report.md` (sections A1-A6 for
Part A, B1-B8 for Part B) and `HANDOFF_BLOCK.md` instead.

---

# Lane GROUND1 resume steps

Written 2026-09-12 after Part A landed. Nothing is committed and nothing should
be. Read `scratchpad/lane_ground1_report.md` first; it is written a section per
gate as the gate lands, so its tail is the truth about where the lane is.

## Which part is finished

* **Part A (TERRAIN-AO1): FINISHED.** Code, build 4, gates A1-A5, two pictures,
  the report through section A6, `WW_CHANGES_ENTRY.md` Part A subsection,
  `HANDOFF_BLOCK.md`, `CHANGED_FILES.txt`, `MISTAKES_ENTRIES.md` (two entries),
  and the two procedures added to `.claude/skills/nifskope-ww-lodgen/SKILL.md`.
  One red item, stated in all three documents: `--terrain-object-ao` with
  `--lodl` is refused rather than implemented.
* **Part B (EROSION1): NOT STARTED.** No code, no gate, no picture.

## State to re-establish before touching anything

1. `tasklist //FI "IMAGENAME eq Fallout4.exe"` and
   `tasklist //FI "IMAGENAME eq NifSkope.exe"` -- two lines, two numbers, both
   must read 0 before a build, before an exe launch, and (this lane learned it
   the hard way) before launching a harness batch.
2. The shipped exe is `release/NifSkope.exe` 2026-09-12 10:34:01, 21,975,040 B,
   sha1 `bad64999afb4020cfcda91b67ca36814a7a4d8b9`. The rung
   `release/NifSkope.before_ground1.exe` is the launch exe, 09:32:37,
   21,951,488 B, sha1 `3e1914a0637b66f438d873e0230b1e8c04d7c806`. **The rung is
   taken ONCE and has been taken. Do not re-copy it.**
3. `scratchpad/ground1_20260912/BUILDING` still exists; it is replaced by `DONE`
   only when Part B is finished too.

## Part B, in the order it should be done

1. **F1 BEFORE any code.** Vanilla erosion statistics over TILING2's 22 sheets:
   what fluvial structure vanilla's own LOD normal and diffuse actually carry,
   with a floor and a CEILING (a ceiling because the gate has to be able to say
   "this pass overshot", not only "it did something"). bungo's words the brief
   is built on: *"We lose all the fluvial, erosion features and other
   topographical features"* and *"the diffuse of vanilla lod land textures, it
   looks like there's variety to it, some geological features shown"*.
2. The deterministic hydraulic erosion pass at bake resolution, feeding the
   `_msn` normal sheet and the colour shading. Deterministic means the same
   bytes for the same seed regardless of thread count and regardless of
   single-chunk versus region -- the same property Part A's height field has,
   and for the same reason.
3. Switches `--erosion <strength>` (0 = off = rung bytes, by construction not by
   tolerance), `--erosion-iterations N`, `--erosion-seed`. Compose with
   `--land-detail-source`.
4. **ONE build** for Part B. Then `make -n` in the MSYS2 shell
   (`MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'cd
   /e/Projects/NifskopeWildWastelandEdition && make -n'`) and it must print
   `Nothing to be done for 'first'.`; then check every `.o` against every header
   touched (the six sources that include `lodgen.h` are `lodgen.cpp`,
   `lodgenmanager.cpp`, `main.cpp`, `nativeemit.cpp`, `nifcli.cpp`,
   `nifskope_ui.cpp`).
5. Gates F1-F4 on the exe carrying BOTH parts. Re-run the harness chain on that
   exe, sequentially, one instance at a time.
6. Pictures `b_cmp_erosion_msn.png`, `b_cmp_erosion_colour.png`,
   `b_cmp_erosion_render.png` into `scratchpad/ground1_20260912/images/`, every
   bake with `--road-detail 1`.
7. Amend `docs/LODGEN_TERRAIN_VT.md` (the composition order of the two lanes'
   terms is already stated in section 2.5h; erosion has to be placed relative to
   it, and it runs BEFORE the object march because it reshapes the ground the
   march reads) and `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md`.
8. Append a Part B subsection to `WW_CHANGES_ENTRY.md` (ONE entry, already
   opened with `## 2026-09-12 -- ...`), update `HANDOFF_BLOCK.md`'s Part B
   section, add Part B's rows to `CHANGED_FILES.txt` with byte counts before and
   after, and write the report sections as each gate lands.
9. Replace `BUILDING` with `DONE`.

## Standing rules that do not lapse

Nothing committed, ever; no `git stash`; never bungo's installed `Data\Terrain`;
**region bakes only, never the whole Commonwealth**; `--road-detail 1` on every
picture; a NifSkope with no `--port` is bungo's window and is never killed; own
instances use `--port <unused>` and the second monitor; ONE GUI instance at a
time; never touch `E:/Projects/NifskopeWWE_ui`, `scratchpad/land1_20260912/`,
`scratchpad/uinotes2_20260912/`, `src/anim*`, `src/ui/**`, `res/**`. The lane
never edits `WW_CHANGES.md`, `MISTAKES.md` or `HANDOFF.md`; the director splices
from the deliverables in this folder.

## Traps this lane already paid for

* A `.lodt` mask sheet is BC1 on a cover-free tile and **BC3 on a cover tile**.
  Use `scratchpad/ground1_20260912/work/maskdec.py`, never a fixed stride. The
  skill entry is in `.claude/skills/nifskope-ww-lodgen/SKILL.md`.
* Heredocs mangle backslashes and turn tabs into spaces. Build search strings
  with `chr(9)`, or use a file-writing tool.
* Em dashes in `docs/` are U+2014. A patch written with `--` will not match.
* Python on Windows here is
  `/c/Users/bungo/AppData/Local/Programs/Python/Python39/python` (PIL 10.0.1,
  numpy 1.26.0). MSYS2's python has neither.
* A gate that selects its victim with `sed -n '5p'` over a four-file list
  deletes nothing and passes. Name the victim explicitly.
