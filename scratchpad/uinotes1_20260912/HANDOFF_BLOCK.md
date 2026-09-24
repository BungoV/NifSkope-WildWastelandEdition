# HANDOFF block -- lanes UINOTES1 + UINOTES1b (for HANDOFF.md's top block)

**Lane UINOTES1 / UINOTES1b -- bungo's nine animation-workspace rulings of
2026-09-12 01:3x-02:0x. All nine are CODE IN, MERGED INTO THE MAIN TREE AND
BUILT. `release/NifSkope.exe` is 2026-09-12 05:48:33, 21,817,856 bytes, sha256
`b62c49989abf002ebacb32769e7e7057d72043b17942436f493c89eaa9e47c47`, built at
`make rc=0` with 0 errors and one warning that is not this lane's
(`-Wunused-function` on `len3`). His open NifSkope window needs a restart to see
any of it.**

**The lane is SUSPENDED, not finished.** `Fallout4.exe` came up (pid 48328,
started 05:42:58) and the standing rule stops the lane. The marker is
`scratchpad/uinotes1_20260912/PENDING2.md`, not a DONE marker. The lodgen chain
has not been run at all and six of the seven UI harnesses have not been re-run on
the newest exe.

**Resume from** `scratchpad/uinotes1_20260912/PENDING2.md` (steps 1-5 at its
foot). The report is `scratchpad/lane_uinotes1_report.md`, sections 0-15 plus
`## Build (UINOTES1b)`; the merge list is
`scratchpad/uinotes1_20260912/CHANGED_FILES.txt`.

**The rung -- never delete or rename it:**
`release/NifSkope.before_uinotes1.exe`, **2026-09-12 04:10:38, 21,489,152
bytes**. (The 23:26:29 / 21,484,032-byte exe named in the previous version of
this block is superseded; the 04:10:38 one is the baseline every "before" number
and picture in this lane came from.)

**What is measured, with every number beside its floor:**

| harness | rung 04:10:38 | merged exe | note |
|---|---|---|---|
| `animws` | 72 checks, 0 fail, 1 skip | **210 checks, 1 fail, 1 skip** | 05:48:33 exe; the rung carries the older 72-check gate |
| `hkxanim_ui` | 48 / 1 | 48 / 1 | 05:06:05 exe |
| `ui_align` | 11 / 0 | 15 / 0 | 05:06:05 exe |
| `water_ui` | 84 / 0 | 84 / 0 | 05:06:05 exe |
| `files_tab` | 29 / 1 | 29 / 1 | 05:06:05 exe |
| `top_bar` | 43 / 5 | 43 / 5 | 05:06:05 exe |
| `skeleton_overlay` | 5 / 1 | 5 / 1 | 05:06:05 exe |

Every red except `animws`'s is the same count on both exes, so this work added
none of them. `animws` went 210/15 at 05:07 to 210/1 at 05:48; **thirteen of
those fourteen repairs were the gate's own defects**, made with five refusing
patch scripts (`gatefix2.py` .. `gatefix6.py`). Only `src/animworkspacetest.cpp`
changed between the 05:06:05 exe and the 05:48:33 one -- no product source has
moved since 05:06:05.

**The one red that is the product's, and it stays red:**
`FAIL (o) Ctrl+A selects every row: 1 of 4`. Control in the same run: with the
list's own signals blocked the same `selectAll()` takes **4 of 4**. The chain
that eats the selection, read from source: `itemSelectionChanged` ->
`listRowChosen` -> `selectEntry(drive=true)` -> `WwHkxAnimHub::activate` ->
`GLView::setSceneSequence` -> `GLView::sequenceChanged`
(`src/nifskope_ui.cpp:24752`) -> `AnimWorkspace::setSequenceByName` ->
`list->setCurrentItem( it )` (Qt ClearAndSelect). A behaviour failure is measured
and stopped on, never fixed to make a gate green. Cost today: multi-row Delete /
Copy / Cut cannot be reached with Ctrl+A. Three candidate fixes are named in the
report's Build section; bungo picks.

**bungo's 05:06 report that the merged exe freezes on opening any nif is NOT
reproduced.** Same files, same window, through the exe's own `--port` IPC:
merged **2.02-2.68 s** per open against the rung's **2.93-3.86 s**. The ~+0.6 s
per-open growth as files pile up is identical on both exes. The three-minute
`animws` run was my own modal dialog and is now 7 seconds. Still open until he
says which file and whether his window already had files in it.

**Owed:** the whole lodgen chain (`lodgen_chain.sh` is written; ROADS3's numbers
to match row for row are in `PENDING2.md`); the other six UI harnesses on the
05:48:33 exe; one drag of a row with a real mouse (a `QDropEvent` sent with
`QApplication::sendEvent` is not delivered inside the application -- the gate's
own filter on that viewport counted **0**, while the dock's own half, the commit
through to the clips, playback and `Scene::animGroups`, is green); pictures for
rulings 3, 6, 6a, 7, 7a and 9 (rulings 1, 2/7b/8, 4 and 5 have shots, listed in
the report); and, from lane UINOTES1 itself, the key-level clipboard, the
QPainterPath-instead-of-svg icon decision and the four questions in report
section 13. An older defect found in passing and equal on both exes: the `--port`
IPC command is space-separated (`src/main.cpp`), so a path with a space in it
never opens.

**Process:** I breached the game guard -- three builds and three harness runs
between 05:42:58 and 05:49 because my one-line guard counted `Fallout4.exe` and
`NifSkope.exe` together and I read the `1` as the harness. Both entries are in
`scratchpad/uinotes1_20260912/MISTAKES_ENTRIES.md` (the other is retyping the
build command instead of copying the skill's line, which cost two builds).

**Nothing was committed, nothing was stashed, bungo's game folder was not
touched, the rung exe is untouched, and one GUI NifSkope at a time was used on
the second monitor with its own unused `--port`.**
