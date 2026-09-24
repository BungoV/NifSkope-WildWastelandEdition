# PENDING2 -- lane UINOTES1b stopped because Fallout4.exe came up

Written 2026-09-12 05:50 (from `date +%H:%M`).

**Why this file exists.** `Fallout4.exe` is running: pid 48328, started
**05:42:58**, 373 s of CPU at 05:49:23. The standing rule is that the game
being up stops the lane. Nothing further is built and no further GUI harness is
launched from here.

**My own breach, recorded rather than hidden.** The game came up at 05:42:58 and
I did not notice until 05:49. Between those two times I built three times
(05:44:47, 05:46:26, 05:48:35) and ran the `animws` harness three times
(43113, 43114, 43115). My guard was `tasklist | grep -icE "Fallout4\.exe|NifSkope\.exe"`
and I read the "1" it returned as the harness's own window instead of asking
WHICH process it was. The chain script `ui_chain.sh` names them separately and
would have stopped; the hand-rolled one-liner did not. It is in
`MISTAKES_ENTRIES.md`.

## Where the work stands

**The exe on disk is 2026-09-12 05:48:33, 21,817,856 bytes, sha256
`b62c49989abf002ebacb32769e7e7057d72043b17942436f493c89eaa9e47c47`.**
`make rc=0`, 0 errors, one unrelated `-Wunused-function` warning (`len3`).
The rung it must be compared against is
`release/NifSkope.before_uinotes1.exe`, **2026-09-12 04:10:38, 21,489,152
bytes** -- never delete or rename it.

Between the 05:06:05 exe and this one, **the only file that changed is
`src/animworkspacetest.cpp`** -- the gate, not the product. No product source
has been touched since 05:06:05.

### animws: 210 checks, 1 failure, 1 skip (log `logs/after/animws6.log`)

It was 210 checks / 15 failures at 05:07. Thirteen of those fourteen repairs
were the gate's own defects (five scripts, `gatefix2.py` .. `gatefix6.py`, each
refusing unless every anchor matched exactly once and the file stayed pure LF).

The one failure left is **not a gate defect and must not be papered over**:

> `FAIL (o) Ctrl+A selects every row: 1 of 4 (one pump later 1, once the list rebuilds 1)`

Measured cause, in the same run: with the list's own signals blocked the same
`selectAll()` selects **4 of 4**. So the rows are selected and something the
selection drives takes them away again, synchronously. The chain that does it,
read from the source: `itemSelectionChanged` -> `AnimWorkspace::listRowChosen`
-> `selectEntry(..., drive = true)` -> `WwHkxAnimHub::activate`
-> `GLView::setSceneSequence` -> `GLView::sequenceChanged`
(`src/nifskope_ui.cpp:24752`) -> `AnimWorkspace::setSequenceByName`
-> `list->setCurrentItem( it )`, which is Qt's ClearAndSelect and drops every
other selected row. **Not fixed, by the brief's rule: a behaviour failure is
measured, named and stopped on.** It means multi-row Delete / Copy / Cut cannot
be reached with Ctrl+A today.

The skip is the fixture's: `10mmPistol.nif` has no `NiControllerSequence`, so
(i) has nothing to test with.

### What is NOT measured, and is owed

1. **The lodgen chain has not been run at all.** `lodgen_chain.sh` is written
   and ready (`bash scratchpad/uinotes1_20260912/lodgen_chain.sh`); it is
   ROADS3's eight harnesses in ROADS3's order. The row-for-row numbers to
   compare against: lodgen_roads 11/0, lodgen_terrain 26/0, lodgen_terrain_vt
   41/1 (V9c is the known red), lodgen_ground_cover 29/5, lodgen_terrain_pbrm
   14/0, lodgen_native all-green, lodl_open 23/0, lod_generation 116/0.
2. **The rest of the UI chain has not been re-run on the 05:48:33 exe.**
   `hkxanim_ui` 48/1, `files_tab` 29/1, `top_bar` 43/5, `water_ui` 84/0,
   `ui_align` PASS and `skeleton_overlay`'s render-size FAIL are all from the
   **05:06:05** exe. The only difference between the two exes is the gate file,
   which none of those six harnesses runs, but they are still numbers from the
   older exe and are written as such everywhere.
3. **The drop's last inch.** Measured: a `QDropEvent` handed to
   `QApplication::sendEvent` is **not delivered** -- the gate's own filter on
   the same viewport counted **0** Drop events and the event came back ignored,
   so `AnimWorkspace::eventFilter` never sees it. The dock's own half is proved
   in the same run: move a row and call the slot that filter queues
   (`commitListOrder`) and the playback, the hub and `Scene::animGroups` all
   follow, with the dock saying "The animations are in a new order." **Owed to
   bungo: one drag of a row with the mouse**, to show Qt delivers the Drop in a
   hand.
4. **bungo's freeze is not reproduced.** Full numbers in the report's Build
   section: the merged exe opens the same files in the same time or faster than
   the rung on every route I can drive.

## To resume

1. `date +%H:%M`; check `Fallout4.exe` is gone (`tasklist | grep -i fallout4`),
   and check NifSkope separately from Fallout4, never in one count.
2. `bash scratchpad/uinotes1_20260912/lodgen_chain.sh` and compare row for row
   with the numbers in item 1 above.
3. `bash scratchpad/uinotes1_20260912/ui_chain.sh after` for the other six
   harnesses on the 05:48:33 exe.
4. Nothing needs rebuilding: `mingw32-make -f Makefile.Release` is at rc=0 with
   the exe newer than every changed source.
5. The four documents are already written from what IS measured
   (`WW_CHANGES_ENTRY.md`, `MISTAKES_ENTRIES.md`, `HANDOFF_BLOCK.md`, and
   `scratchpad/lane_uinotes1_report.md` section `## Build (UINOTES1b)`); a
   resume adds numbers to them, it does not rewrite them.

**Nothing is committed. Nothing was stashed. bungo's game folder was not
touched. The rung exe is untouched.**

## Amendment, 05:59 (from `date +%H:%M`)

The four documents are now written, and this is what each one holds:

* `WW_CHANGES_ENTRY.md` -- the changelog text, plus a "Built and measured" block
  with the exe identity, the seven-harness table and the one red.
* `MISTAKES_ENTRIES.md` -- two new entries: the collapsed process guard that let
  me build while the game was up, and retyping the build command instead of
  copying the skill's line.
* `HANDOFF_BLOCK.md` -- rewritten whole. It no longer says BUILD PENDING and no
  longer names the superseded 23:26:29 rung; it names the 04:10:38 rung and the
  05:48:33 exe, the gate table, the one red with its chain, the freeze numbers
  and everything owed. The previous version is kept beside it as
  `HANDOFF_BLOCK.prev_uinotes1.md`.
* `scratchpad/lane_uinotes1_report.md` -- section `## Build (UINOTES1b)`
  appended; nothing above it was touched.

One more thing owed that was not in the list above: **pictures for rulings 3, 6,
6a, 7, 7a and 9.** What exists is a before/after pair for ruling 1
(`viewport_gizmo`, 857x359 -> 857x383, the 24 px the retired status bar was
taking), a before/after pair of the dock (`dock_frame46`), and after-only shots
for rulings 2/7b/8 (`sheet_colours`), 4 (`transport_1x` and `transport_2x`) and 5
(`animws_list_menu`). A "before" cannot exist for a panel the rung has no code
for; the missing "after" shots are simply not taken, because the game came up.

Two findings from looking at the pictures, both recorded in the report: the
`seam.png` and `topbar.png` pairs are byte-identical before and after (that shot
is of the TOP seam, so it cannot show item 1 -- correct, not a bug), and the
annotation labels overlap each other in the dock shot **in the before shot too**,
so that is pre-existing and not this lane's.
