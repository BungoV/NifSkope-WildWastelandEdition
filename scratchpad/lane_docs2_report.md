# Lane DOCS2 -- stale line numbers, the FO4CS entry point, and three pending changelog entries

Repo `E:\Projects\NifskopeWildWastelandEdition`, branch `main` at `720762a`.
**DOCS ONLY**: no `src/` edit, no build, no exe launch (bungo was timing a bake
in the GUI). Nothing committed (CONSTITUTION 8).

Read first: `CONSTITUTION.md` (1, 1a, 1b, 2, 4 and the read-order rule), the
`HANDOFF.md` top block in full. Skills loaded: `ww-contract-provenance`
(mandatory, all five steps), `nifskope-ww-lodgen`, `ww-hkx-animation` (listed in
the brief; see section 5).

Scripts, all copied into the repo (CONSTITUTION 8: nothing lives only in
`%TEMP%`): `scratchpad/docs2_20260910/` -- `stamp.py` (step 1/5, the hashes),
`anchors.py` (step 3, the generalised pass), `p1_hand.py` / `p2_hand.py` (the
anchors the machine could not verify), `p3_inline.py` (citations outside the
footer tables), `p4_stamps.py` (step 5, the stamps), `p5_notes.py` (the dated
note on each page), `p6_splice.py` / `p7_ledgers.py` (WW_CHANGES + MISTAKES),
`p8_skill.py` (the skill amendment), and `WW_CHANGES.md.bak-docs2` (the file as
it stood before any splice).

---

## 1. The provenance pass, per page

**Step 1 first, before reading anything** (`stamp.py`): 30 sources hashed and
line-counted. Four had moved since the pages were stamped, and they are why the
numbers rotted:

| source | page's stamp | now | verdict |
|---|---|---|---|
| `src/lodgen.cpp` | `6d7388c53a13343e` 8,286 / `64191a7ed236ddb8` 8,619 / `3e4815416faa4aba` 8,850 | `c05fd079655ac03e` 391,673 B, **8,924** | **MOVED**, three vintages on seven pages |
| `src/nifskope_ui.cpp` | `dbf4540b51166e31` 31,316 | `f5d1e3bf9fab7a27` 1,472,482 B, **31,409** | **MOVED** |
| `src/lodtfile.cpp` | `10ad5f58262d8620` 140,908 / 3,638 | `601fb65136c8766d` 141,680 / **3,658** | **MOVED** |
| `src/lodtfile.h` | `128dcd3f4c011a08` 20,346 / 415 | `4ffeccdc9b581e5e` 21,491 / **435** | **MOVED** |
| `src/btdterrain.cpp` | `cbe06adb7394f59b` (VERTEX_PACKING) | `35c2adc319852901` 56,630 / 1,484 | **MOVED** (BTD_FORMAT's copy was already right) |
| `src/io/lodvfile.{h,cpp}`, `src/io/lodmfile.cpp`, `src/data/niftypes.h`, `src/gl/glmesh.cpp`, `src/hkxanim.{h,cpp}`, the six native files, `tests/spells/lodl_water.sh`, `lodgen_native_decode.py`, `hkxanim_decode.py` | -- | same sha256 | **UNMOVED** |

**Step 3, the re-derivation.** Every `| claim | line | anchor |` row located
again by its ANCHOR TEXT in the current source, EXACT and UNIQUE or not rewritten
at all. Never shifted by a delta.

| page | rows | moved | already right | missing | ambiguous | multi-site (by hand) |
|---|---:|---:|---:|---:|---:|---:|
| `docs/LODGEN_BTD_FORMAT.md` | 66 | 26 | 40 | 0 | 0 | 0 |
| `docs/LODGEN_CARD_SHEETS.md` | 36 | 24 | 12 | 0 | 0 | 0 |
| `docs/LODGEN_LODM_FORMAT.md` | 34 | 22 | 12 | 0 | 0 | 0 |
| `docs/LODGEN_MANIFEST_FORMAT.md` | 14 | 13 | 1 | 0 | 0 | 0 |
| `docs/LODGEN_TEXTURE_ARRAYS.md` | 15 | 7 | 7 | 0 | 0 | 1 |
| `docs/LODGEN_TERRAIN_VT.md` | 23 | 6 | 13 | 0 | 0 | 4 |
| `docs/LODGEN_NATIVE_LODO_LODI.md` | 49 | 4 | 45 | 0 | 0 | 0 |
| `docs/LODGEN_VERTEX_PACKING.md` | 11 | 9 | 2 | 0 | 0 | 0 |
| **total** | **248** | **111** | **132** | **0** | **0** | **5** |

**`src/nifskope_ui.cpp` moved TWICE MORE while this lane wrote.** A concurrent
lane took it from `f5d1e3bf9fab7a27` (31,409 lines) to `e820666628d97a51`
(31,496) and then to `1073ddef14f8562e` (31,495), each time after a pass had
already written. Re-running the pass caught both and re-derived **17 rows on
CARD_SHEETS and 5 on LODM_FORMAT** each time; the last apply + re-stamp was run
as ONE operation with the hash read before and after (unchanged), and the
verification run then reports `0 moved`. Both pages now carry a
line saying that file is under live edit. This is exactly what the skill's
"re-derive IMMEDIATELY before finishing, not while writing" is for, and the
cheap insurance was running the pass one more time at the end.

`docs/HKX_ANIMATION_FORMAT.md` was checked and needed **nothing**: its
provenance table carries source hashes and exe RVAs, not line numbers, and all
three of its source stamps (`src/hkxanim.h` `b606db5c8fbaf87b`,
`src/hkxanim.cpp` `dfb2347143dad6dc`, `tests/spells/hkxanim_decode.py`
`2e0011d380080899`) match the tree byte for byte.

**Idempotence:** a second run of `anchors.py` over the written pages reports
**0 moved, 0 missing, 0 ambiguous** on all 243 machine-checkable rows.

### The five multi-site rows, re-derived by hand

| page | row | verdict |
|---|---|---|
| TERRAIN_VT | `lodvfile.h:67`, `lodvfile.cpp:29-30` (magic / version / header) | correct, file unmoved |
| TERRAIN_VT | `lodvfile.cpp:142, 382-391, 568-576` (`indexCrc32`) | correct, file unmoved |
| TERRAIN_VT | `lodvfile.cpp:428, 434, 553, 588, 556, 562` (rules 1, 2, 19-22) | correct, file unmoved, each `// rule N` read at its line |
| TERRAIN_VT | `lodgen.cpp:7323, 7328` | **moved -> 7397, 7402** |
| TEXTURE_ARRAYS | `lodgen.cpp:3878, 3895` (mip law + BC1 alpha) | **moved -> 4203, 4220**, both inside `lodgenEncodeArrayLayer` (signature 4198) |

### Seven anchors repaired, not renumbered -- what changed in the code

1. **`lodtfile.cpp` "AO row 0 is SOUTH"** -- the comment now WRAPS; the anchor
   was trimmed to the line it starts on. The number 186 was already right.
2. **`lodtfile.cpp` "FO4 writes no ground cover"** -- same wrap; anchor trimmed,
   and the number **2424 -> 2429**.
3. **`lodgen.cpp` "`I` line and the >= 8 threshold"** -- the writer now puts
   `continue;` on its own line, so the anchor `if ( ... < 8 ) continue;` no
   longer exists; re-anchored on the `if` alone, **3288-3297 -> 3608-3617**.
4. **`nifskope_ui.cpp` "the bake states the coverage contract"** -- the cell
   carried a REAL newline where the C++ `\n` had been pasted literally, which
   had silently broken that markdown row in two. Number 22675 was right.
5. **`lodgen.cpp`, three `LODGEN_TEXTURE_ARRAYS.md` rows** (`sheets[4]`,
   `textures.emissive`, `array.emissiveScale`) -- the CARD-ARRAY pass now writes
   those lines VERBATIM, so each anchor matched twice. Lengthened with the
   unique neighbouring line until it names the array pass alone; numbers set by
   hand to 4585-4589, 4602-4603, 4613-4614.
6. **`lodgen.cpp` `OBJ_VERTEX_DESC`** -- the anchor is a strict prefix of
   `OBJ_VERTEX_DESC_COLORS` on the next line. Lengthened to the full
   declaration; **1357 -> 1358**.
7. **`lodvfile.cpp` "tile entry, 24 B"** -- the anchor spanned two source lines;
   re-written as a start...end pair. Numbers unmoved.

### Citations outside the footer tables

| file | was | is | why |
|---|---|---|---|
| `docs/LODGEN_VERTEX_PACKING.md` prose | `src/lodgen.cpp:1357-1358` | `1358-1359` | the two `OBJ_VERTEX_DESC` constants moved down one |
| `docs/LODGEN_VERTEX_PACKING.md` prose | `src/lodgen.cpp:57` | `58` | `LAND_VERTEX_DESC` moved down one |
| `docs/LODGEN_VERTEX_PACKING.md` footer | `niftypes.h:1907-1987` | `1907-1979` | a range END carried by the start's delta; see MISTAKES |
| `scratchpad/handoff_fo4cs/WRITER_CHANGES_NEEDED.md` | `src/lodtfile.h:58` | `159` | `LodtOptions::headerVersion` moved |

**Out of scope, and NOT touched:** twelve older plan/audit pages carry inline
`file:line` citations into files this lane did not stamp
(`BLOCKLIST_MENU_AUDIT.md`, `WORKSPACE_CONSISTENCY_AUDIT.md`,
`TO_BE_IMPLEMENTED.md`, `PERFORMANCE_PLAN.md`, `FOUR_FEATURES_PLAN.md`,
`MODELING_TOOLS_PLAN.md`, `RENDERER_MATCH_PLAN.md`, `SKELETON_AND_POSE_PLAN.md`,
`BONE_WEIGHT_TRANSFER_PLAN.md`, `UV_EDITOR_PLAN.md`, `CLI.md`,
`WW_PDB_COMPARISON.md`). They are plans and audits, not contracts, they carry no
provenance footer and no anchor text, and re-deriving them needs an anchor pass
of its own. **Named here so nobody reads their numbers as checked.**

**Step 4, last of all**: the version constants re-read from the source, not from
the page. `LODL_VERSION = 3` (`src/lodtfile.cpp:56`), and the writer's default is
still `LodtOptions::headerVersion = 2` (`src/lodtfile.h:159`), raised to 3 only
when `--water-bodies` is given. FO4CS still pins `kVersion = 1u`, so a file this
tree writes today is refused, not misread -- that is section 2's first item.

**Step 5**: 11 stamp rows and 3 prose stamps rewritten to the state the pages
were FINISHED against (`p4_stamps.py`), plus a `src/gl/glmesh.cpp` row added to
`LODGEN_LODM_FORMAT.md`, which cited that file with no stamp at all. Each page
carries a dated note saying how many rows moved and what was repaired.

---

## 2. `scratchpad/handoff_fo4cs/README.md`, rewritten

346 -> 504 lines. Sections rewritten, in order:

* **Header + "The two changes FO4CS must make up front"** (NEW section): the
  terrain loader reads `<WS>.lodl`, and it must **accept version 3**. Both stated
  with the fallback (`WW_LODL_VERSION=1`) and the writer's real default.
* **§1 the file family table** -- extended with the `.lodl` v3 water sections
  (built and gated; the dye plane BUILD PENDING) and `<WS>.water.json`; the
  native pair moved from "not yet built into the exe" to **built and hooked up
  by BUILD6**; the card row now names orthographic + `coverage 16 128 160`; the
  `.lodl` row now says v1, v2 **and** v3.
* **§2 read order** -- unchanged in order, re-pointed (BTD_FORMAT now covers
  water; NATIVE_LODO_LODI is AS BUILT, not spec-only; `WRITER_CHANGES_NEEDED.md`
  added as item 9).
* **§3 the reader checklists as landed today** (NEW, replaces the old
  draw-only §3), five subsections:
  * **3.1 the land file and its water** -- the section bit first, the body-ID
    plane as the MASK (nearest, never filtered, no mips), the body table lookup
    for **tint, flow and fog**, `bodyRecordBytes` as the forward-compatibility
    rule, **depth = `body.waterHeight - terrainHeight`**, the flow plane's
    word layout and the rule that it is filtered **only inside the mask** (a
    bilinear tap that reaches a dry sample or another body pulls a direction
    from water that is not this water) with its three named floors, the **dye
    plane's source/weight blend over the body's own tint**, and the shore plane
    with the **winter path: freeze from the shore inward** (a consumer rule;
    nothing about ice is in the file).
  * **3.2 impostor cards** -- orthographic and the `projection` word, THE
    COVERAGE CONTRACT `coverage 16 128 160` with the older-set rule (test at
    16/255), gap = `max(2, side/16)` and margin = gap/2, **`mips = log2(gap)`**
    and why one level shallower, **per-frame extents and `frameOffset`**, and
    `card.center` as the pivot -> centre offset.
  * **3.3 the native pair** -- the ten-step draw kept, plus the **two deviations
    a reader must know** (56-byte mesh row; `rotation` is the DRAWN rotation and
    a consumer that re-derives the yaw from `seed` will double it).
  * **3.4 the terrain pyramid** -- `LDTX`, NORTH-UP row order against the
    `.lodl`'s row-0-SOUTH, per-tile CRC at load, and `partial: true`.
  * **3.5 the flow PNG** -- bungo's DirectX ruling verbatim, against
    `src/watercurves.cpp:932`, which as written says *"This tool writes +green =
    north"*. **Recorded as OWED, not as done.**
* **§4** the version ladder and the five installed files (v2, would be refused).
* **§5** the sample set, with the card sets marked orthographic +
  `coverage 16 128 160`, the five real `.lodo`/`.lodi` region pairs added, and a
  new line saying the set is NOT in the public repo but regenerates.
* **§6 bake time -- UNMEASURED** (NEW): states plainly that no lane has ever
  timed a full Commonwealth bake, that the only measured figures are lane 0's
  **8 s** region set and the `.lodl`'s **6 s** write, and carries the empty
  four-row table for **bungo's GUI stage times: landscape / meshes / textures /
  impostors**.
* **§7 open items** -- nine, re-scored against today (the native pair's RED
  manifest leg at 0.125; WATER4/WATER5 BUILD PENDING and the two gates that fail
  as pre-registered; the green sign; the seam corner still red on -20.28).
* **§8** -- the tree is **committed and pushed** (`720762a`), which the old §7
  denied; the 2026-09-10 work is landed but uncommitted.

---

## 3. The WW_CHANGES.md splices

`WW_CHANGES.md` is a MIXED file (CONSTITUTION 8). Spliced in **binary** by
`p6_splice.py` and `p7_ledgers.py`, never `sed -i`; the file as it stood before
is `scratchpad/docs2_20260910/WW_CHANGES.md.bak-docs2`.

| entry | state | CR before | CR after | LF delta |
|---|---|---:|---:|---:|
| `scratchpad/native0_20260910/WW_CHANGES_ENTRY.md` | **ALREADY SPLICED** by lane BUILD6 -- grepped first; the heading is in the file, reworded from "not yet built into the exe" to "built into the exe 2026-09-10 (lane BUILD6)". **Not spliced again.** | -- | -- | -- |
| `scratchpad/hkx1_20260910/WW_CHANGES_ENTRY.md` | spliced, newest first | 19,020 | 19,020 | +157 (with WATER5) |
| `scratchpad/water5_20260910/WW_CHANGES_ENTRY.md` | spliced, second | 19,020 | 19,020 | (same run) |
| this lane's own entry (CONSTITUTION 8) | spliced at the top | 19,020 | 19,020 | +85 |

**CR count unchanged at 19,020 across all three writes**, asserted in the script
before every write, measured with Python byte counts and never with grep.

**Build state as of now, checked rather than copied** (`release/NifSkope.exe`
2026-09-10 **03:57:46**, 18,526,720 B; `Fallout4.exe` down):

* **HKX1 -- BUILD PENDING for the NifSkope link only.** `src/hkxanim.cpp` is
  05:10:14, ninety minutes newer than the exe, so the reader is not in it.
  `NifSkope.pro` **already names both files** (lines 187 and 312), so the resume
  is `qmake` + `make` and nothing else. Its own gates DID run, on
  `release/hkxanim_dump.exe` (05:10:22).
* **WATER5 -- BUILD PENDING, hook-ups unapplied.** The four sources are
  04:50:17-05:04:17, all newer than the exe, and `NifSkope.pro` does **not**
  name `src/watercurves.*` or `src/waterwindow.*`: hook-up H1 has not been
  applied. Resume is `scratchpad/water5_20260910/PENDING.md` in full, after
  WATER4's build.

Both statements were written into the changelog as a dated "Build state" block
under each heading, so the entry cannot be read later as "landed and built".

---

## 4. Mistakes

Three, all written into `MISTAKES.md` at the repo root the moment they were
recognised (CONSTITUTION 2):

1. **"anchor not found" was my extractor failing, not the anchor.** The first
   run reported 13 MISSING and 4 AMBIGUOUS; **11 of the 13 were the script** --
   an ellipsis inside the backticks leaves the span unclosed, a wrapped anchor
   cannot match a per-line scan, and one cell carried a real newline. Five of
   them were on `src/io/lodvfile.{h,cpp}`, whose sha256 matched the page's own
   stamp and therefore could not have moved a line. Found by grepping every
   MISSING by hand before writing anything. Rule: hand-check every MISSING and
   AMBIGUOUS against the source, and check the hash first.
2. **A range's END shifted by its START's delta.** `niftypes.h:1899-1979`
   became `1907-1987`, which runs into `ClearAttributeOffsets`, on a file that
   had not changed at all. Rule: the end is re-derived, not carried.
3. **A heredoc ate a line continuation** and wrote a literal `\n` into the
   middle of an `if` in `anchors.py`. `nifskope-ww-lodgen` says in bold that no
   text carrying a backslash goes through a heredoc, and the skill was loaded in
   this same lane. Rule: the existing one, applied.

---

## 5. Skill review (CONSTITUTION 1a)

**Loaded:** `ww-contract-provenance` (all five steps, in order, and it is the
whole of section 1); `nifskope-ww-lodgen` (the CRLF rule, the heredoc trap, and
the writer/reader facts used to judge which of two duplicate anchor sites is the
array pass).

**`ww-hkx-animation` was NOT loaded and is not needed here.** The brief named it,
but `docs/HKX_ANIMATION_FORMAT.md` needed no change at all: its provenance table
carries hashes and RVAs, not line numbers, and all three source hashes match.
Loading a format skill to confirm that three sha256 prefixes are unchanged would
have cost context for nothing. Naming the refusal rather than staying silent, per
rule 1a.

**Amended, in BOTH trees** (`<repo>/.claude/skills` and
`E:\Projects\Claude\.claude\skills`, verified identical afterwards):
`ww-contract-provenance` now points at `scratchpad/docs2_20260910/anchors.py` as
the worked script instead of lane RENAME's single-file one, and carries the four
extractor rules and the two hand-work failures above, each of which cost a wrong
report in this lane. It also now says to run the pass twice and require
`0 moved`.

**No new skill written.** The one candidate -- "bring a handoff README current" --
is not repeatable as a procedure: what goes in it is whatever the lanes landed
that day, and the part that IS repeatable (trace every claim to a stamped source)
is `ww-contract-provenance`, which already exists and now covers the multi-page
case. Declining rather than writing a skill that would only restate it.

---

## 6. Files changed

`docs/LODGEN_BTD_FORMAT.md`, `LODGEN_CARD_SHEETS.md`, `LODGEN_LODM_FORMAT.md`,
`LODGEN_MANIFEST_FORMAT.md`, `LODGEN_TEXTURE_ARRAYS.md`, `LODGEN_TERRAIN_VT.md`,
`LODGEN_NATIVE_LODO_LODI.md`, `LODGEN_VERTEX_PACKING.md`;
`scratchpad/handoff_fo4cs/README.md`, `WRITER_CHANGES_NEEDED.md`;
`WW_CHANGES.md`, `MISTAKES.md`;
`.claude/skills/ww-contract-provenance/SKILL.md` and its live-tree twin;
`scratchpad/docs2_20260910/` (11 files) and this report.

**Line endings, by Python byte count** (never grep): every `docs/*.md` this lane
touched is LF-only, 0 CR, before and after. `scratchpad/handoff_fo4cs/README.md`
0 CR. `MISTAKES.md` 0 CR. `WW_CHANGES.md` 19,020 CR before and 19,020 after all
three splices. Nothing was normalised and nothing was committed.
