## DONE

Lane ROADS3 closed 2026-09-12 04:30 (`date` read in the same turn, not
estimated). `scratchpad/roads3_20260911/DONE` written, `BUILDING` removed.

**The exe.**

| | |
|---|---|
| `release/NifSkope.exe` | **2026-09-12 04:10:38**, **21,489,152 B**, md5 `fe65cc978f3896881140c2eea57c69c6` |
| the rung it replaced, as found at launch | 2026-09-12 03:06:21, 21,489,152 B, md5 `6af74b4b4667ce50c4506a2d42a04fdf` |
| kept aside as | `release/NifSkope.before_roads3.exe` -- same md5 as the launch exe; its own file date is 03:37:13, which is when the copy was taken, not when it was linked |
| builds spent | **1** (BUILD-RC=0 first try) |
| extra relinks | **0** |
| sources changed | `src/lodgen.h` 03:57:07, `src/lodgen.cpp` 03:57:07, `src/nifcli.cpp` 03:58:08 -- all older than the exe |
| `Fallout4.exe` at close | not running |
| NifSkope processes at close | **0** |

The two exes are the same SIZE and different bytes; the md5s are what separate
them.

**The gate table.**

| gate | verdict | the number, beside its floor |
|---|---|---|
| **F1** vanilla's opacity law fitted with a floor and a ceiling BEFORE any code | **MET -- and the law is REFUSED by its own floor** | unexplained 17.6 % vs shuffled-ground floor 18.2 % on (-20,20); 52.6 % vs 51.5 % on (-8,8). Ceiling 18.1 % / 39.2 %. Alignment control: best shift over +-3 buys 0.005 |
| **F1** detail strength | **MET -- null result** | correlation +0.0275 vs phase-twin floor 0.0270 / 0.0644; +0.0162 vs 0.0124 / 0.0163. `--road-detail` stays 0 |
| **F1** hue named by a measurement | **MET -- null result** | road texels at (-20,20): vanilla b_y -12.63 vs ours -12.47; saturation 0.162 vs 0.161 |
| **F2** off value == the rung's bytes, every file `cmp`-ed, compare shown able to fail | **MET on both tiles** | no flag 9/9 and 10/10 identical; `--road-opacity 1` 9/9 and 10/10; `--roads-legacy` 9/9 and 10/10. Able to fail: 0.326 moves 3 and 4 files, **all colour**; `_msn`, `_data`, `.bto`, `.lodl`, `.lodm` identical at every setting |
| **F3a** road mean luminance within 3 of vanilla | **default is 6.53 and 12.09 out; a=0.83 lands (-20,20) to 0.09; (-8,8) REFUSED by arithmetic** | baked 0.83: 92.43 vs vanilla 92.52. On (-8,8) the composite can only land between ground 102.12 and paint 106.68 and vanilla is 94.59 -- **7.53 outside at every opacity** |
| **F3b** road hue within 3 of vanilla | **MET as a rise; unreachable as an absolute** | rises 1.05 / 1.11 apart on (-20,20), 2.14 / 1.83 on (-8,8). Absolute d(b_y) -6.5 to -6.7 for every setting including the rung |
| **F3c** road local SD within 20 % of vanilla's | **default MET on (-20,20); every calming setting breaks it** | rung 1.10x, baked 0.83 **1.02x**, baked 0.326 **0.64x**. On (-8,8) the rung is already 0.84x and no setting fixes it |
| **F3d** road-edge 10-90 % width inside vanilla's | **REFUSED AS UNRESOLVABLE, with the number** | 4.29-level rise under a 6.59-level local SD: SNR **0.65** |
| **F3e** no step where vanilla has none | **MET at 0.326 on (-20,20); the rung is already smoother on (-8,8)** | (-20,20) vanilla 1.31, rung 3.88, 0.326 **1.69**, 0.83 3.35. (-8,8) vanilla 4.43, rung 1.25 |
| **F3e'** the two-tone skirt itself | **the knob shrinks it and cannot remove it** | luminance vs mesh vertex alpha: vanilla +0.001, a=1 **-0.792**, 0.83 -0.694, 0.326 **-0.436**, our own ground's floor **-0.325** |
| **F3f** `lodgen_roads.sh` stays 11/0 | **MET, no bar touched** | 11/0. R5 floor 0.1354, after 0.3435, reference 0.4039, bars 0.2708 / 0.3231 cleared; centreline error 38.04 -> 22.34 |
| **F3g** ROADS2's S1 seam not worse than 4.242 | **MET at every setting** | feathered boundaries (54 texels, vanilla 4.242): rung **3.979**, 0.83 **3.352**, 0.326 **2.979**. Solid-boundary control 5.772 / 5.567 / 3.351 vs vanilla 5.291. Displaced floors 3.10-4.81 |
| **F3h** raised-highway clearance stays +0.001 | **MET by bytes** | the `.bto`, `.lodl` and `.lodm` are byte-identical at every setting (F2), so no geometry number can have moved |
| **F4** the chain at GRADE1's baselines; exe newer than every changed file; rung == launch bytes; no NifSkope left running | **MET** | `lodgen_roads` 11/0, `lodgen_terrain` 26/0, `lodgen_terrain_vt` 41/1, `lodgen_ground_cover` 29/5, `lodgen_terrain_pbrm` 14/0, `lodgen_native` 0 failures in all seven sections, `lodl_open` 23/0, `lod_generation` 116/0 -- every one equal to the baseline. Exe 04:10:38 newer than all three sources. Rung md5 == launch md5. 0 NifSkope processes |

**The one red row in that table is not this lane's, and there is a control that
says so.** `lodgen_terrain_vt`'s failing check is V9c; the rung exe run through
the same harness fails V9c with digit-for-digit identical numbers (E/W seam
188.074, interior 13.243, ratio 14.20, edge step 14.348),
`logs/f4_vt_RUNG_control.txt`. It is carried forward as RED 4.

**Deliverables on disk.**

* `scratchpad/lane_roads3_report.md` -- this report, sections 0-7 and `## DONE`,
  LF-only.
* `scratchpad/roads3_20260911/WW_CHANGES_ENTRY.md` -- the changelog entry,
  starting `## 2026-09-12 —`.
* `scratchpad/roads3_20260911/HANDOFF_BLOCK.md` -- the handoff block.
* `scratchpad/roads3_20260911/MISTAKES_ENTRIES.md` -- **four** entries in the
  ledger's `--` heading format, not appended by the lane.
* `scratchpad/roads3_20260911/images/cmp_road_wash.png` and
  `cmp_road_profile.png` -- every panel a real bake or Bethesda's own sheet.
* `docs/LODGEN_TERRAIN_VT.md` -- amended in place, section 1a: new **1a.5d**
  (the whole finding with its provenance, its baked table, and what the built
  exe measured), plus 1a.4, 1a.7 and 1a.8. LF-only, byte-counted.
* `.claude/skills/ww-simulate-before-build/SKILL.md` -- **added**, and it now
  carries this lane's own audit of the technique's accuracy.
  `.claude/skills/ww-control-calibration/SKILL.md` -- **amended** with the
  NaN-floor paragraph. Both mirrored additively into
  `E:\Projects\NifskopeWWE_ui\.claude\skills\`; nothing in that tree was deleted
  or overwritten, because lane UINOTES1 is live in it.
* `scratchpad/roads3_20260911/PENDING.md` -- superseded, and says so at its top.

**`scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md` needs no change, and here is
why in one line:** no default moved. `--road-opacity` ships at 1.0, the multiply
is branched over there, and F2 shows every file of a default bake byte-identical
to the previous exe's -- so the instruction as written still produces exactly
the bytes it says it produces.

**What is not claimed.** Nothing here has been looked at in the game. The
default's bytes did not move, so there is nothing new to see at the default; the
two candidate settings have not been flown. bungo's open NifSkope window is
still on the 03:06:21 exe and needs a restart to pick this one up. And no
default is being proposed: the table in section 5 is for his call.
