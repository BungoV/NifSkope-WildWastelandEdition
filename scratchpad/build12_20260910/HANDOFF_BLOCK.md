# BUILD12 handoff block (for the director to splice into HANDOFF.md)

**BUILD12 LANDED, EXE FREE.** `release/NifSkope.exe` **17:45:29**,
**20,751,360 bytes** (BUILD11's was 17:08:39, 20,693,504). Markers:
`scratchpad/build12_20260910/DONE` in, `BUILDING` gone. Report
`scratchpad/lane_build12_report.md`; WATER7's own report gained
`## Build (BUILD12)`.

**What it carries.** Lane WATER7 / UI2, built from its resume
(`scratchpad/water7_20260910/PENDING.md`): the water tool as a **fourth tab of
the LOD Generation workspace's left strip** (Header | Blocks | Files | Water,
shown while `LodGenerationDock` is visible), the `Water window` and
`Water Marking` entries gone from the Workspaces menu, the old `WaterMarkDock`
retired, **every top bar at one 35 px row** including the MENU BAR and the
buttons inside the bars (`wwAlignBarRow` + `wwBarRowButtonQss`,
`UI/CompactTopBars` default **true**), and BUILD10's five reds.

**THE 35-PX CHECK: the row is 35 px and the top did NOT grow.**
`wwBarRowHeight() = 35`, `UI/CompactTopBars = true`, five visible bars all at
the row height, menu bar 35 against the dock tab strip's 35 with the 4-px floor
firing. 35 is BUILD9's number. Against the old exe's own dump: tab strip top
35, viewport toolbar top 35, search row top 70 -- identical; `tMode` and
`tRender` moved 33 px @ top 36 -> 35 px @ top 35, i.e. they JOINED the row.
The registered refuter did not fire, so nothing was set to false and the
shipped default stands.

## Gates (all on the 17:45:29 exe, sequential, one instance)

| gate | numbers |
|---|---|
| `water_ui.sh` (first run ever) | **30 checks, 0 failures, 0 skips**, PASS (floor 24) |
| `water_weights.sh` | X-gates green **17** (floor 16), PASS; selftest 64 / 8 on body 2 |
| `water_flow.sh` | F-gates green **17** (floor 17); **FAIL (3)** = selftest exit + F2 island bank + F5 p99, all pre-registered red |
| `water_mark.sh` | dock **20 / 0** (floor 16), `WW_WATER_MARK_BODY=3` printed; **FAIL (2)** = the same reds through the model half |
| `water_window.sh` | **46 / 0** (floor 24), PASS |
| `lodl_water.sh` | **33 `ok`, 0 `FAIL`**, control PASS |
| `lodl_open.sh` | **23 / 0**, PASS |
| `ui_align.sh` | **11 / 0**, PASS -- unchanged from the old exe |
| `top_bar.sh` | **43 / 5** = baseline |
| `loaded_nifs.sh` | **166 / 2** -- baseline was 166 / **3** |
| `files_tab.sh` | **28 / 2** = baseline |
| `hkxanim_ui.sh` | **48 / 1** = baseline |
| `animws.sh` | **57 / 0**, 1 skip, PASS |

Skipped, with the reason: every suite the change does not reach (lodgen,
terrain, impostor, gltf, hkxfile, collision, block) and `skeleton_overlay.sh`
(SKELFIX's, flaky by BUILD11's own four-run measurement, untouched here).

## Pictures for bungo

`scratchpad/build12_20260910/images/`

* `seam_before.png` -- the 17:08:39 exe, taken with `ui_align.sh` BEFORE the
  hook-up (`water_ui.sh`'s harness does not exist in that exe, so it could not
  take it).
* `seam_after.png` -- the same crop and spell on the new exe.
* `topbar_after.png` (1512x107) -- the whole top of the window on one row.
* `watertab.png` (278x741) -- the left dock with four tabs, Water selected.

## Reds for the director

1. **X2b is half green.** The new net-flux ring reads **0.595** on body 2
   (green against the > 0.5 floor registered before the code, control 0.173 <
   0.2) and **0.407 on body 3 (RED)**, control 0.155 < 0.2. Both controls hold,
   so the ring reads the pin and not the channel. The retired disc metric
   printed beside it still reads 0.742 / 0.371, so the disagreement between the
   two bodies is 1.46x where the old instrument's was 2.00x -- smaller, not
   gone. Candidates, named as candidates: the floor was registered off body 2's
   number, or the ring at two pin widths clips body 3's narrower channel (647
   ring texels against 1052; 25,114 wet texels against 29,312). Discriminator:
   the same ring at one and at four pin widths on both bodies, one run, no code
   change. NOT landed -- a build lane's product is a verdict.
2. **The buttons are 39 px inside the 35 px row**, and `water_ui.sh` R3 passes
   because it allows 8 px where its own header promises 1 (the 1 px is enforced
   only between the buttons, `39..39`). "The buttons" is half of what bungo
   asked for. The arithmetic is `wwBarRowButtonQss`: `min-height: rowHeight - 4`
   plus `(rowHeight - 18) / 2` of padding. His call whether 39 is compact
   enough or the sheet should state the row height itself.
3. **`water_flow.sh` and `water_mark.sh` cannot print PASS as written.** Their
   verdict loops still count F2 and F5, the two PRE-REGISTERED reds, while the
   floor arithmetic subtracts them (17 = 19 - 2). Either the loop skips the two
   by name or the spells stay red forever; nothing regressed either way.
4. **`water_weights.sh` still does not pin its body** and runs on body 2, the
   marsh -- which is why its selftest carries 8 failures against body 3's 3.
   Four of the five extra are the dye gates that have no mouth on that body
   (BUILD10's red 4, applied to two of the three spells). Its own X-gates are
   green on body 2, so this is not a regression.
5. **`loaded_nifs.sh` improved 166 / 3 -> 166 / 2 and nobody can say which
   check turned green**, because BUILD9 recorded only the count.
   `release/NifSkope.before12.exe` (a copy of the 17:08:39 exe, KEPT for this)
   answers it in one run.

## Restart

**YES.** bungo's open window, whatever its age, predates 17:45:29. The next
launch of `release\\NifSkope.exe` is the one with the Water tab and the compact
top row.

## State

Nothing committed (CONSTITUTION 8). `WW_CHANGES.md` 1,574,208 -> 1,578,992 B
with **CR 19,020 unmoved**; `MISTAKES.md` 212,575 -> 216,188 B, CR 0, two
BUILD12 entries; `scratchpad/lane_water7_report.md` gained
`## Build (BUILD12)`; `.claude/skills/nifskope-ww-resume-pending/SKILL.md`
gained section 11 (**the director mirrors it to
`E:\\Projects\\Claude\\.claude\\skills`**). Game down (`rc=1`) at every check.
