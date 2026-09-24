# LANE CLAMP2 -- the cell owns its own boundary rows

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, **no commits**.
Read first: `CONSTITUTION.md`, `HANDOFF.md` top block (the 2026-09-10 ruling and
the BUILD4 CLAMP results), `scratchpad/lane_clamp_report.md` §B.5.
Skills loaded and used: `nifskope-ww-lodgen`, `ww-sheet-diff`,
`ww-control-calibration`, `nifskope-ww-build-verify`,
`nifskope-ww-resume-pending`.

All work under `scratchpad/clamp2_20260910/`.

**This is the relaunch of lane CLAMP2, which died on an API rate limit before
writing anything.** Confirmed rather than assumed: `scratchpad/clamp2_20260910/`
did not exist, and `src/lodgen.cpp`'s working-tree modifications at the card
sidecar and the card `.lodm` are lane CARDWIDTH's landed work (already in the
02:07:48 exe), not a half-applied CLAMP2 patch.

**STATUS: see §7.** The code, the control, the document, the ledger entry and
the resume are on disk. `src/lodgen.cpp` compiles (`g++ -fsyntax-only`, rc 0,
and not one warning in the new code -- the four it prints are all pre-existing).

---

## 0. The ruling, and what it binds

bungo, 2026-09-10, verbatim: **"The cell owns it then."**

Read as a rule the code can be held to: `lodgenTerrainFillRing` fills ONLY the
texels beyond the chunk -- the one-cell ring -- and never overwrites the chunk's
own boundary rows or columns; where a neighbour's shared row disagrees with the
cell's own, the cell's copy stays; the seam keeps Bethesda's hairline
disagreement. The same rule reaches `lodgenBakeVtTile` through the shared
sampler, from ONE home, so the two paths stay byte-identical.

## 1. The defect, restated from BUILD4's measurement (not re-derived)

Bethesda's landscape disagrees with itself across exactly one shared vertex row
in the fixture's neighbourhood. From the MASTER, `--dump-land`, cells
x = -24..-17:

| shared row | max &#124;difference&#124;, VHGT units of 8 |
|---|---|
| y=23 &#124; y=24 | 0 across all eight columns |
| y=27 &#124; y=28 | 0 |
| **y=31 &#124; y=32** | **2, 1, 4, 6, 9, 8, 7, 4** (16..72 world units) |
| y=32 &#124; y=33 | 0 |
| both east seams (x=-21, x=-17), y=24..31 | 0 |

The old filler visited cells south to north, west to east, later-wins. The
inner unit's grid box runs `[32 .. hn-1-32]` at `LODGEN_TERRAIN_RING_CELLS = 1`,
and the cells that write its two far edges come AFTER the inner cells in that
order, so:

* the **south** boundary row and the **west** boundary column were already the
  chunk's own -- the ring cell writes them first and the inner cell overwrites;
* the **north** boundary row and the **east** boundary column were the
  NEIGHBOUR's -- the ring cell writes them last.

At y=31|32 that put a row up to 72 world units off inside the chunk, and a
bilinear tap reaches one grid step, so a texel at distance `t` reads grid index
`0` out to `t = 7`: BUILD4 measured 994 and 1,405 `_msn` texels beyond the
4-texel band on the two y=28 chunks, **every one of them on the north border, at
distances 4, 5, 6 and 7**, with a passing border showing one distance only (3).

## 2. The change

`src/lodgen.cpp`, terrain functions only. **Named**, as the brief asks:

| function | what happened |
|---|---|
| `lodgenTerrainFillRing` | the rule: a RING cell skips any sample inside the inner unit's closed grid box. The box is DERIVED here from `LODGEN_TERRAIN_RING_CELLS` and `hn` rather than passed, because both callers lay the ring out identically -- one description, one home. |
| `lodgenTerrainRingSelfTest` | NEW. The known-answer control, §4. |
| `lodgenTerrainRingSelfTestOnce` | NEW. Runs it once per process, only when `WW_TERRAIN_RING_TEST` is set. |
| `lodgenBakeTerrainTextures` | one line: the self-test call at the top. Nothing else. |
| `lodgenBakeVtTile` | **not edited, and that is the point** -- it gets the rule from the shared filler. |

Inside the inner unit the order is unchanged (south to north, west to east,
later wins), which is also the mesh path's own convention in
`lodgenWriteLandChunk`, so the two conventions stay the same one.

**What can move, exactly, and it is arithmetic rather than hope.** Every sample
the rule newly skips was, in the old order, overwritten by a ring cell AFTER the
inner cell had written it. On the south and west borders, and at the SW corner,
the inner cell wrote last already, so skipping changes nothing. Only the north
boundary row, the east boundary column and the three corners on them can move --
and only where the master's two copies of that row differ. Everything outside
the inner box is written by the same cell in the same order as before.

**A landless inner cell now keeps `empty` on its shared row** instead of
borrowing the neighbour's. That is the same rule applied to a cell whose own
answer is the worldspace default height, it is stated in the code, and it moves
nothing in the Commonwealth fixture (all 64 cells have LAND).

## 3. The land VERTEX channels: measured, and the answer is NO ring needed

The brief asks whether `src/lodgen.cpp:903` already reads the cell's own row.
**It does, and it never had the defect.**

`lodgenTerrainChannels` is called there on `grid`, built 170 lines earlier at
`src/lodgen.cpp:726-743`:

```
const int n = dim * 32 + 1;
std::vector<float> grid( size_t( n ) * size_t( n ), world.defaultLandHeight() );
for ( int cy = 0; cy < dim; cy++ )
    for ( int cx = 0; cx < dim; cx++ ) { ... }
```

`dim` cells, `n = dim*32+1` samples, **no ring at all**. Its north boundary row
`n-1` is written only by `cy = dim-1`'s row 32 and its east column only by
`cx = dim-1`'s col 32 -- the chunk's own cells. There is no neighbour in that
loop to overwrite anything, so the cell already owns every boundary sample and
the ruling asks for no change here.

What that grid does NOT have is the ring, so `lodgenTerrainChannels`'s AO march
(2,048 units) and the shore field are still CLAMPED at the chunk edge on the
mesh path. That is lane CLAMP's owed item 3, it is a different defect from this
ruling, and it is untouched here on purpose: it moves `.bto`/`.btr` bytes and
`lodgen_identity.sh`'s own baseline. `lodgen_identity.sh` staying at 8/0 with an
unmoved baseline is therefore also the empirical check on this paragraph -- the
mesh path was not touched, so if its bytes move, something reached further than
this lane believes.

## 4. The known-answer control, and its refuter

`scratchpad/clamp2_20260910/ringcontrol.sh`, driving the exe's own
`WW_TERRAIN_RING_TEST` self-test. It is exact, not statistical.

A synthetic pair of cells whose shared VHGT row disagrees by **72 world units**
-- the 9-unit worst case measured on the real y=31|32 seam -- is filled through
the SHIPPED `lodgenTerrainFillRing`. Then:

| assertion | value |
|---|---|
| the inner unit's NORTH boundary row, every sample | the inner cell's, exactly |
| its EAST boundary column, every sample | the inner cell's |
| its SOUTH row and WEST column (the regression guard) | the inner cell's |
| one grid step BEYOND the north border | the **neighbour's** |
| one grid step BEYOND the east border | the neighbour's |
| the bilinear tap ON the north border | the inner cell's |
| the bilinear tap half a step beyond it | the exact midpoint |

The two "beyond" rows are what stops the bar being satisfied by a filler that
simply stopped ringing, and the two taps are what make the claim about a
**texel's operand** rather than only about a grid cell -- `lodgenTerrainGridSample`
is the function the sheets read through.

**The refuter** (`ww-control-calibration` part 3): the same synthetic pair filled
by the OLD south-to-north order, reproduced verbatim inside the self-test. It
must give the NEIGHBOUR's value on the inner boundary -- it must FAIL the bar
above -- and if it does not, the self-test fails on that alone and says the bar
does not discriminate.

**The floor** (`ww-control-calibration` part 2): the runner counts the self-test's
own output lines and requires 12 before believing any of them, so an env var
nobody reads, an inlined-away test or a grep against an empty log fails rather
than passing silently. A failing assertion still prints its line, so the count is
a floor on the test HAVING RUN and never a restatement of its result.

It is deliberately NOT a check inside `tests/spells/lodgen_terrain_vt.sh`: that
file's count is pre-registered at **35** and a lane does not grow the number it
is judged by. Folding it in as check 36 is one `ok`/`bad` pair and is offered
here for the director rather than taken.

## 5. Documents

* `docs/LODGEN_TERRAIN_VT.md` §2.4 gains the ownership rule with the measured
  seam table beside it, and the reason the two paths still agree (one filler).
* Its provenance footer was **re-derived from the anchors**, not shifted by this
  lane's delta (`ww-contract-provenance` step 3;
  `scratchpad/clamp2_20260910/p2_anchors.py`). It was already stale before this
  lane -- it stamped 373,908 bytes against a file that was 380,327 -- and all
  five `lodgen.cpp` rows were found exactly once and moved by the same +257,
  which is the cross-check on the re-derivation and not its method. New stamp
  `3e4815416faa4aba`, 388,314 bytes, 8,850 lines. The stamp and all five rows were
  re-derived a SECOND time after a two-line comment fix late in the lane, because
  a stamp that is two lines stale is exactly as wrong as one that is two hundred.
* `WW_CHANGES.md`: the entry, spliced with Python byte counts. Mixed file,
  **dCR +0**, +56 LF lines, and the newest-entry block at the top is LF like its
  neighbours.
* `tests/spells/lodgen_terrain_vt.sh`: a comment beside V9b naming the rule and
  pointing at the control. No check added, no check removed; the three embedded
  Python blocks still compile (`3 blocks ok`, the skill's gate, run after the
  edit).

## 6. What was NOT done

* **Nothing was committed** (CONSTITUTION 8).
* No band was re-pinned and no bar was moved. The 4/4/64 bands are lane CLAMP's
  pre-registration read off the code, BUILD4 reported a miss against them rather
  than fitting them, and this lane's job is to make the miss go away, not the
  bar.
* The `_data` wetness domain is untouched (lane CLAMP's owed item 2). If the 16
  beyond-band `_data` texels survive the fix, they are that defect, not this
  rule, and the resume says to report them rather than widen anything.
* The mesh path's missing ring (owed item 3) is untouched -- see §3.
* `tests/spells/lodgen_terrain.sh` was read and left alone: its 26 checks do not
  touch the fill order, and its bar is that it still reads 26/0.

## 7. Build

**BUILD PENDING.** The slot was checked ONCE, at the end, after every file was
written, as the brief requires -- never polled:

```
ls scratchpad/water3_20260910/DONE   -> No such file or directory (rc 2)
tasklist | grep -i -E "Fallout4|NifSkope" -> rc=1  (both absent)
```

The game is down but **lane BUILD5b's `water3_20260910/DONE` is not there**, so
BUILD5b still owns the link and this lane does not take it. The paste-able
resume is `scratchpad/clamp2_20260910/PENDING.md`; it carries the build command,
the game-down check, the staleness sweep, the seven gates with their bars
pre-registered BEFORE any of them ran, the exact two directories the edge band
must be measured between, and a written-down prediction (§4 of that file) that
the y=24 fixture chunks must come back **byte-identical** against BUILD4's own
sheets, because their seams agree by 0 in the master.

What was done in place of a build, so the resume starts from something proved:

| check | result |
|---|---|
| `g++ -fsyntax-only` on `src/lodgen.cpp` with `Makefile.Release`'s own flags | **rc 0**; 4 warnings, all pre-existing (`lodgenTriangulateGrid`'s unused `grid`, the `"\shack"` escape, the `LODV_FLAG_FULL_MODE` conditional, the atlas `%d`/`qsizetype`), **none in the new code** |
| the harness's embedded Python (`nifskope-ww-lodgen`'s two-second gate) | `3 blocks ok` in `lodgen_terrain_vt.sh`, `1 block ok` in `lodgen_terrain.sh` -- the same counts as before the edit |
| `sh -n` on the harness and on `ringcontrol.sh` | clean |
| line endings, Python byte counts only | `src/lodgen.cpp` CR 0 (8,673 -> 8,850 LF); the doc CR 0; the harness CR 0; `WW_CHANGES.md` **dCR +0**, LF +56 |
| anchor counts before every replace | 1 of 1 on all nine anchors (two in `lodgen.cpp`, six in the doc, one in the harness) |

A syntax-only compile writes no object and no exe, and does not touch the link.

## 8. Finished-work skill review (CONSTITUTION 1a)

**Loaded and used.** `nifskope-ww-lodgen` -- the editing traps did the real work
here: every patch is a file written with the Write tool and run (never a
heredoc), every anchor count asserted at 1, line endings measured with Python
byte counts, and the embedded-Python compile gate run after the harness edit
with its COUNT compared against the previous run rather than its verdict.
`ww-control-calibration` -- §4 is its parts 2, 3 and 5: the floor that fires when
the self-test did not run, the refuter built from the code this change replaced,
and the known-answer input whose right answer is known before the run.
`nifskope-ww-resume-pending` -- the shape of `PENDING.md`, the staleness sweep
over EVERY changed file rather than the one edited, the rule that a resuming lane
measures a failure and does not land a cure, and §8's rule that the clamped
`before/` set is kept while a gate is red. `nifskope-ww-build-verify` was loaded
and its procedure is quoted into `PENDING.md` §2; **no build was attempted**, so
it was applied to the resume and not to a result, and saying so rather than
implying otherwise.

`ww-contract-provenance` was **not named in the brief** and was loaded anyway,
because this change moves `src/lodgen.cpp` by 177 lines and the one document
this lane owns cites it by line. It is what stopped the footer being "fixed" by
adding 175 to five numbers: two other lanes had already moved the file, and the
true shift is +259.

`ww-sheet-diff` was named in the brief and **does not exist as a skill** -- the
listing has the name, and lane CLAMP §7 recommended writing it. Nothing was
loaded under it; the working code it would carry is
`scratchpad/clamp_20260910/edgeband.py`, which this lane reuses UNCHANGED rather
than writing a fifth BC1 decoder. That reuse is itself the argument for the
skill: three lanes, one instrument, and the only thing this lane had to add was
a sentence in `PENDING.md` §4 saying WHICH two directories the diff is between,
which is the piece that goes wrong when a band is measured against the wrong
baseline.

**Declined, with reasons.** A skill for the ownership rule: it is one `continue`
with its contract in its own comment. A skill for "run the exe's env-gated
self-test and grep": that is `ringcontrol.sh` itself, twenty lines, and a skill
nobody would think to load is a skill nobody loads.

## Build (BUILD6)

Built and gated 2026-09-10 03:5x by lane BUILD6; nothing committed. Every
reading below is off `release/NifSkope.exe` **03:38:56** (18,517,504 B). That is
lane BUILD5b's link, not a fresh one: `src/lodgen.cpp` was written at 03:33:44,
`GeneratedFiles/.obj/lodgen.o` is 03:38:54 and holds the
`lodgenTerrainRingSelfTest` symbols (`nm`, 2 hits), and this lane's `make -j2`
(rc 0, game check rc=1) compiled nothing. The staleness sweep over every changed
file under `src/ res/ tools/ tests/ NifSkope.pro` found one file newer than that
exe, `tests/spells/lodgen_native_baseline.sh` (a harness, not a source). Logs:
`scratchpad/build6_20260910/logs/g*.log`, summary `chain1_summary.txt`.

| # | gate | bar (PENDING.md) | reading | verdict |
|---|---|---|---|---|
| 1 | `ringcontrol.sh` | 12 checks, CONTROL says the old order is REFUSED | **13 checks, 0 failures, RESULT PASS**; CONTROL: the old order gives 1096.000 north / 1096.000 east on the inner boundary, REFUSED (the 13th check is "the self-test's own verdict is PASS"; the count is reported, not re-pinned) | PASS |
| 2 | `lodgen_terrain_vt.sh` | 35 checks, 0 failures | **35 checks, 1 failure, RESULT FAIL** | **FAIL** |
| 3 | V9a / V9b | byte-identical on all four chunks, tint ON and OFF, and `_msn`; FLOOR >= 2 of 8 | V9a ON and OFF: byte-identical on all four; FLOOR 8 of 8 pairs differ; **V9b: `msn differs: Commonwealth.4.-20.28`** (the other three identical) | **FAIL on V9b** |
| 4 | V9c | edge step <= 2.60 (1.961 or better); E/W <= 3.20, N/S <= 3.30; interior 1.20..2.20 | E/W seam 4.955 interior 1.803 ratio **2.75** (edge step **1.961**); N/S seam 4.600 interior 1.603 ratio **2.87** (edge step 1.773) -- the same digits BUILD4 read | PASS |
| 5 | `lodgen_terrain.sh` | 26/0, `UP=G D0=76 D1=32 D2=51 D3=32` both paths | **26 checks, 0 failures**; pyramid-assembled and direct both `UP=G D0=76 D1=32 D2=51 D3=32` (vanilla control `D0=99 D1=67 D2=67 D3=67`) | PASS |
| 6 | `lodgen_identity.sh` | 8 ok, RESULT PASS, baseline unmoved | **8 ok, RESULT PASS**; dim 4: 678 rows, 471 SCOL parts; dim 8: 1,536 rows; 406 shared | PASS |
| 7 | edge band vs lane CLAMP's clamped bake | 0 beyond 4 / 4 / 64 on all four borders; colour + `_msn` must move | colour **0 beyond** (4 chunks, cover and nocover), `_msn` **0 beyond** on all four (BUILD4's 994 / 1,405 north misses -> 0); `_data` **16 beyond 64 on -20.28** in both runs (maxd 79); colour moved 127 texels with `--cover`, 0 without (the same 0 BUILD4 read: the no-cover colour floor is pre-existing, not new); `_msn` moved 22,127 | PASS on colour/`_msn`; `_data` 16 = the wetness-domain defect, left as the resume says |

**The V9b failure, measured (not cured).** Reproduced outside the harness into
`scratchpad/build6_20260910/v9b/` with the harness's own two commands (region
-24 24 -17 31, dim 4, `--cover`; run1 adds `--vt ... --vt-height`). The direct
`_msn` of -20.28 is byte-identical to this lane's gate-7 cover bake. The
pyramid-assembled and direct `_msn` sheets of `Commonwealth.4.-20.28` differ by
**32 texels of 262,144 (0.0122%)**: rows 0-3 (the NORTH border, one BC1 block
row) at x = 252..259 (two blocks), i.e. centred on **x = 256, the -19|-18 cell
corner on the region's outer y=31|32 edge**, maxd 3, nothing on S/E/W, nothing
interior. -24.28 and both y=24 chunks: 0 texels between the two paths. Lane
BUILD4 read V9b green at 35/0 on the exe before this rule, so this is the
ownership rule's own regression: at a cell CORNER on the region's outer ring
the chunk baker and `lodgenBakeVtTile` now resolve the corner sample to
different owners (both call the one filler, but the tile's ring cells and the
chunk's ring cells are not the same set at the region edge). Candidates, named
as candidates for the director: (a) the corner sample of the north ring row
where two ring cells meet -- the rule says "a ring cell never writes the inner
unit's boundary row", and at a corner the inner unit's boundary is one sample
wide while the ring row is two cells wide; (b) the tile path's inner unit being
the tile, not the chunk, so its "own boundary" is a different line at x=256.
Neither was tested; `borders.py` beside the sheets is the instrument.

**The written prediction (PENDING.md §4), checked.** Against BUILD4's own after
sheets: the y=24 chunks came back **byte-identical, all 12 files, cover and
no-cover** (`cmp`). The y=28 `_msn` sheets moved 3,199 (-20.28) and 2,519
(-24.28) texels, **every one in rows 0-7 of the north border** (maxd 7; the
15-28 texels the edge-band classifier files under W/E are corner texels of
those same rows; interior 0; the east COLUMN beyond the corner rows: 0). The
prediction said "north or east"; the reading is north only, which is narrower,
not wider. The `_data` sheets of the y=28 chunks moved 3,278 / 3,800, maxd 47,
0 beyond 64 -- so the 16 beyond-64 texels on -20.28 are NOT this rule's (they
are identical between BUILD4's bake and this one).

Mtimes in one table: `src/lodgen.cpp` 03:33:44; `lodgen.o` 03:38:54; exe
03:38:56; gates run 03:52:17-03:53:23; v9b reproduction 03:5x. Skipped: none
of the seven. `WW_CHANGES.md` paragraph replaced (CR 19,020 unchanged, LF
+25); `scratchpad/clamp2_20260910/DONE` written 03:57 with the verdict.
Owed: the V9b corner (the director's call: revert the ownership rule, or scope
it, or accept a 32-texel corner difference and re-pin V9b -- this lane did not
re-pin); the `_data` 16 (lane CLAMP's owed item 2, unchanged); the mesh path's
missing ring (owed item 3, unchanged). The exe this was gated on has since been
replaced by BUILD6's 03:57:46 link (the native hook-up); the terrain code in
it is the same bytes of `lodgen.cpp` §terrain, and the CLAMP2 gates were NOT
re-run on it.
