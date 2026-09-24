# DEFAULTS2 -- text for the overseer to splice (the lane edited none of HANDOFF / WW_CHANGES / MISTAKES)

Exe: `release/NifSkope.exe` sha1 90e8a57e (24,091,648 B, 15:26:42). The defaults build was d9ca02ab
(14:31:22); the second build added only the byte-gate self-test fix (`WW_LODGEN_GATE` block), so every
gate run on d9ca02ab stands. Rung: `release/NifSkope.before_defaults2.exe` sha1 6b8ed793 (= the
IMPOSTORDEPTH2 close). Nothing committed.

## HANDOFF entry

- **2026-09-23 DEFAULTS2 (lane, Opus 5.5)** -- bungo's two 09:3x rulings are now what a bare bake does.
  (1) `--blend-edges quadrant` is the default (`g_blendEdges` 0 -> 1; panel row *Cross-faded* by default;
  the panel-run gate table default "1"; a CLI help line added -- none existed); `--blend-edges off` is the
  way back. (2) Tree cards default to 8 x 8 at 2k: the frame is 256 px, the sheet 2048 (driver `TILE`,
  panel cardRes, hook fallback). **The old default was 8 x 8 at 128 px = a 1024 sheet**; N was already 8,
  so only the frame size moved. `TILE=128` / cardRes 128 is the way back. The frame-law ladders did not
  move. Shrub cards keep their baked AO ("Keep AO") -- recorded, no code change.
  Byte gates (pre-registered in `scratchpad/defaults2_20260923/progress.md`): bare new == rung with the
  flags typed out, terrain and cards both; bare new vs bare rung moves ONLY the chunk colour DDS
  (`tex/Commonwealth.4.-20.24.DDS`) + `VT.2.lodt` + `VT.4.lodt` on the terrain side and the card `.txt` +
  its 4 oct sheets on the card side. Seam 1.236 / 1.250 -> 0.977 / 0.992. Two named amendments (the
  `.lodb` bake record carries a clock; the card checker read a stale token index).
  Re-rung: `lodgen_terrain_vt.sh` V9a-3 OLDLAND gains `--blend-edges off`. **Red by the ruling:**
  V9a-1 / V9a-2 -- with the blend on, the pyramid-assembled chunk colour sheet and the direct (stock)
  one differ on 2,790 of 262,144 texels (1.06%, max 25 levels), all within 4 px of a quadrant line and
  8 px of the chunk edge; the difference was there before, behind the flag. Open.
  Open finding: `--incremental` will not rebake for this flip (the record's switch digest is the typed
  argv, identical for a bare bake on either exe, and no exe identity is checked) -- same class as DEFAULTS1.
  Second re-rung: the panel byte gate's row "blendMargin (with blendEdges on)" turned its dependency on
  by bumping it, which now flips the blend OFF; the row names depVal "1" and a dependency already at its
  named value counts as on (`src/nifskope_ui.cpp`, self-test only). `lodgen_byte_gate.sh` PHASES=bc then
  reads exactly as on the rung: (b) 132/0; (c) 2 differ (chunk colour DDS + `.lodi`), which is the
  rung's own pre-existing red. GUI gates: octahedral PASS (116 ok); impostor_draw 33/1 = named 4x4 row 5,
  row 5t 0.7029; impostor_trunk 38/3 = the 2 named crisp tears + the smooth-end el20 open finding,
  identical to IMPOSTORDEPTH2's run; panel_run 137/0.

## WW_CHANGES entry

### Defaults: edge blend on, tree cards 8x8 at 2k (lane DEFAULTS2, 2026-09-23)
- `--blend-edges quadrant` is the default (bungo 2026-09-23, "Yes, default on"). `--blend-edges off`
  bakes the hard quadrant lines as before, byte for byte. The LOD panel's edge-blend row now starts on
  *Cross-faded*; the CLI help lists the switch.
- Tree cards bake 8 x 8 frames at 256 px, a 2048 sheet (was 128 px, a 1024 sheet):
  `tools/bake_impostor_cards.sh` `TILE`, the panel's card resolution row (only where no value is saved)
  and the bake hook's fallback when `WW_IMPOSTOR_TILE` is unset. `TILE=128` is the way back.
- Docs: `docs/LODGEN_TERRAIN_VT.md` s2.5a, `docs/LODGEN_IMPOSTOR_SPEC.md` (frame law head),
  `docs/LODGEN_CARD_SHEETS.md` (header).
- Gates: `tests/spells/lodgen_terrain_vt.sh` V9a-3 re-rung (the old look now spells `--blend-edges off`);
  V9a-1/-2 red (the two colour writers disagree near quadrant lines once the blend is on). The panel
  byte gate's `blendMargin` row now names the blend value it needs (a bumped combo turned it off).

## MISTAKES entries

- **2026-09-23 DEFAULTS2 -- a byte gate that included a file carrying a clock.** The pre-registered
  terrain gates G1-G3 compared every output file, `obj/<ws>.lodb` included. That record carries the exe
  size, the bake's UTC time, the output dirs, the typed switches and their digest, wall times and peak
  memory, so it can never match across two runs. The first run read 9 of 10 identical and the gate had
  to be amended after the numbers were in. Rule: before pre-registering "every file identical", list
  the bake record's volatile lines and drop them in the gate (`amend_lodb.py` keeps 13 stable lines).
- **2026-09-23 DEFAULTS2 -- a checker that read "the last token".** `card_check.py` took the card
  sidecar's base frame size as the `oct` line's last token; IMPOSTORDEPTH2 had appended a 13th token
  `spec1`, so G6 read FAIL on correct bytes. Rule: read a sidecar field by its index in the documented
  line, never by position from the end.
- **2026-09-23 DEFAULTS2 -- a gate chain that ran a harness without its subject.** The lane's GUI chain
  ran `impostor_draw.sh` bare; it refused at step 4 (`IMPOSTOR_LODM` unset) in 4 s and the chain moved
  on. Re-run with IMPOSTORDEPTH2's subject. Rule: before chaining a harness, grep it for `:=}` env
  refusals and copy the last lane's invocation, not just its name.
- **2026-09-23 DEFAULTS2 -- a Grep over `scratchpad/*/progress.md`** timed out at 20 s; search-lean
  already says scratchpad is searched only inside one named lane folder.

## Findings (measured, not fixed)

1. **Incremental rebakes are blind to a default flip.** The `.lodb` `switches` digest is the TYPED
   argument vector: a bare bake on the rung and on the new exe both carry `65439eba...`, and `--incremental`
   checks no exe identity; the record's `out` rows list BTR/BTO/manifest, not the colour sheets. An
   incremental run over a record from an older exe keeps the old hard-edged colour. Same class as
   DEFAULTS1 (2026-09-12).
2. **The two terrain colour writers disagree once the blend is on.** Stock chunk composite (no `--vt`)
   vs pyramid-assembled (`--vt`) on chunk 4.-20.24: 2,790 of 262,144 texels, max 25 levels, 278 4x4
   blocks, all within 4 px of a quadrant line and 8 px of the chunk edge. Both read seam 0.977. With the
   blend off they are identical. This is what turns `lodgen_terrain_vt.sh` V9a-1/-2 red.
3. `tests/spells/lodgen_defaults.sh` `LAND_OLD` no longer spells the full old look (it lacks
   `--blend-edges off`). Its phase (b) only asserts old != default, so nothing it checks moved; not changed.
4. **`lodgen_byte_gate.sh` phase (c) was already red on the rung** (panel default bake vs bare CLI: chunk
   colour DDS and `Commonwealth.lodi` differ, 15 identical). The gate's forced-default table still holds
   pre-2026-09-12 land values (`landHex` 0, warp 0, mip bias 0, guide 0, `roadGroundPaint` 1,
   `nativeLadder` 0) while the bare CLI bakes today's; a CLI bake with those land values typed out still
   differs, so more than the land rows disagree. Not this lane's; the blend moved BOTH sides' DDS, and
   the `.lodi` moved on neither.
5. A `WW_LODGEN_GATE` row whose dependency defaults ON must name its `depVal`: `bumpRow` flips a combo
   0 <-> 1 and a checkbox either way, so a bump turns a default-on dependency OFF. `blendMargin` was the
   only such row (58 rows read).

## Tell bungo

- His window (pid 25584) closed during the lane; the next launch of `release\NifSkope.exe` (15:26) has
  the new defaults. Four `NifSkope_inuse_<pid>.exe` copies sit in `release/` (2000, 8728, 25584, 28576);
  none is running now and the lane left them alone.
- The panel's card resolution is saved per user: if his panel was ever set, it may still say 128, and a
  panel on 256 refuses 128 px card libraries ("Those cards were baked at 128 px, not 256"). Existing card
  libraries need a rebake at 2k, or the row set back to 128.
- A bake with `--incremental` over an existing record will not pick up the blend (finding 1): do a full bake.

## Gates

| gate | result |
|---|---|
| terrain G1 new bare == rung +quadrant | PASS (all files; `.lodb` stable lines) |
| terrain G2 new +off == rung bare | PASS |
| terrain G3 new bare vs rung bare moves only colour DDS + VT.2/VT.4.lodt | PASS |
| terrain G4 seam | 1.236 / 1.250 -> 0.977 / 0.992 PASS |
| card G5 hook bare new == rung TILE=256 | PASS, 7 files |
| card G6 old default moved: .txt + 4 oct sheets; albedo 2048 vs 1024, base 256 vs 128 | PASS (amended checker) |
| card G7 driver bare == OCT=8 TILE=256; library tile 256 | PASS, 8 files |
| lodgen_terrain_vt.sh | rung 45/1 (exe-newer only); new 45/2 = V9a-1, V9a-2 (red by the ruling) |
| lodgen_octahedral.sh | RESULT PASS, 116 ok |
| impostor_draw.sh | 33 steps / 1 = row 5 named 4x4 KNOWN RED (0.3920); row 5t (8x8 at 2k) PASS 0.7029 |
| impostor_trunk.sh | 38 / 3 = 2 named crisp-end tears + smooth end el20 T2 2/360 (IMPOSTORDEPTH2's open finding); identical to its run |
| lodgen_panel_run.sh | 137 / 0 |
| lodgen_byte_gate.sh PHASES=bc | new before re-rung: (b) 132/1 blendMargin, (c) 2 differ. Rung: (b) 132/0, (c) 2 differ. New after re-rung (90e8a57e): (b) 132/0, (c) 2 differ = rung. Phase (a) not run (rung is before_panel1, asserts no default moved) |
| native_lighting.sh | not run: no shader or lighting source touched |
| lodgen_defaults.sh | not run: phase (b) asserts only old != default; the ruling moves nothing it pins |

## Skill review (finished work)

- `nifskope-ww-lodgen`, the "DEFAULTS MOVED" section: add "2026-09-23 (DEFAULTS2): `--blend-edges
  quadrant` is the default, `--blend-edges off` the way back; the full old land look is now the four land
  switches AND `--blend-edges off`. Tree cards bake 8 x 8 at 256 px (2048 sheet); `TILE=128` is the way
  back. A byte gate over a bake folder drops the `.lodb` record's volatile lines. `--incremental` does not
  rebake for a default flip."
- `ww-test-harness-add`: add "a `WW_LODGEN_GATE` row whose dependency defaults ON names its `depVal`
  (a bump flips it OFF); and before chaining a harness, copy the last lane's full invocation (env subject
  included), since several harnesses refuse fast and silently without it."
- `nifskope-ww-build-verify`: followed as written (rung, make RC, MZ, exe-newer, rebuilt-object list); no change.
- `search-lean`: one timeout from breaking its own scratchpad rule; the skill is right, no change.

## Files this lane touched (for an explicit-path commit, when bungo says so)

src/lodgen.cpp, src/lodgen.h, src/nifcli.cpp, src/lodgenmanager.cpp, src/nifskope_ui.cpp,
tools/bake_impostor_cards.sh, tests/spells/lodgen_terrain_vt.sh, docs/LODGEN_TERRAIN_VT.md,
docs/LODGEN_IMPOSTOR_SPEC.md, docs/LODGEN_CARD_SHEETS.md. New: release/NifSkope.before_defaults2.exe (rung).
Patch scripts: fix_defaults2.py, fix_docs.py, fix_vtgate.py, fix_bytegate.py (all anchored, CR-checked).
