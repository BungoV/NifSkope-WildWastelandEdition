# HANDOFF block -- lane TILING4, 2026-09-12 (splice; the overseer owns HANDOFF.md)

## TILING4 DONE 02:3x -- `--land-sample stochastic` is a hex tiling now, the default did not move

Brief `scratchpad/brief_tiling4.md` + the resume brief `brief_tiling4b.md`
(grain/band gates replaced by G1/G2 and their band twins). Report
`scratchpad/lane_tiling4_report.md`, **960 lines, sections 0-9, appended never
rewritten, CR 0**. Lane dir `scratchpad/tiling4_20260912/` (`DONE` in,
`BUILDING` gone, `PENDING.md` current).

### The exe
- `release/NifSkope.exe` **2026-09-12 02:08:57, 21,487,616 B**, BUILD-RC=0, one
  build, no relinks, exe newer than all three changed sources, style.qss and
  the shaders in step. `Fallout4.exe` down at 02:07:01 and again inside the
  chain before the link.
- Rung `release/NifSkope.before_tiling4.exe` 00:06:03, 21,484,032 B, md5
  `8ec07038d14f4a973921d8a1c39a33b1`, untouched. New exe md5
  `8a1a1e718c6d6822ad0d60b90803fd69`.
- `release/NifSkope_inuse_60864.exe` is the renamed-aside launch copy. Nothing
  was killed. **RESTART: yes** for any window older than 02:08:57.
- **A GUI `release/NifSkope.exe` pid 59040 appeared at 02:24:09 with no
  arguments and this lane did not start it** (every bake was `-no-gui lodgen`
  and all 42 exited rc=0). It was left alone and had closed by itself at 02:35.
  **At 02:35 no NifSkope and no `Fallout4.exe` is running: the exe is free.**

### Landed code (three files, nothing else)
`src/lodgen.cpp` -- `g_landHexSize` (6081), the two lattice constants in
**double** (6086), `lodgenLandHexCell` (6096), `lodgenLandHexOffset` (6122,
through the warp's own hash), `lodgenLandHexTap` (6133, variance-preserving
blend, **alpha from the largest-weight tap, never blended**),
`lodgenSetLandHexSize` (6183), and the tap called at **both** sampling sites,
**7666** (chunk) and **8990** (VT pyramid). `src/lodgen.h` 225.
`src/nifcli.cpp` 6090 (`stochastic` = hex 256 + mip bias -0.22), 6116
(`--land-hex`). `--land-sample warp` = TILING3's warp, kept reachable.
**Default unchanged.** Way back: `--land-hex 0 --land-mip-bias 0`.

### Gates
| gate | result |
|---|---|
| F1 (instrument + split) | PASS, from the first agent: split frozen 00:11, law 00:27, controls 7/7, 7/7, 5/7 |
| parity C++ vs prototype | **0 of 75 disagree**, every setting moves the sample, `--sabotage` fails 56 of 75 |
| **F2 byte identity** | **PASS** -- off == rung 9/9 x2 tiles; the way back == rung; stochastic moves exactly 3 files (colour DDS + **both** `.lodt`, the second-site proof); `_msn` == vanilla 10/10; `warp` == TILING3's bake byte for byte; 1 vs 16 threads 97 files 0 differ. Comparator shown red on a flipped byte and a deleted file first. |
| **F3 on the product** | **NOT MET.** 14 sheets x 3 arms, real bakes (`f3_full.sh`/`f3_full.py`): hex repeat **4/7 and 5/7**, swirl **6/7 and 7/7**, G2 7/7 and 7/7, G1 -14.3 % (pass) and -36.3 % (fail). Warp: repeat 5/7 and 6/7, swirl 3/7 and 4/7, G2 0/7 and 1/7. Rung: repeat **0/7 and 0/7**. |
| **F4 the chain** | **PASS**, 11 of 11 at TILING2/TILING3 baselines: 23/0, 26/0, 41/1, 11/0, 29/5, 14/0, 18/0, 125/0, 116/0, 11/0, 82/0 (02:12-02:18). `lodgen_ground_cover` held at 29/5 untouched. |

### What this means in one line
The default stays `footprint` because the hex tiling is 9 of 14 on the repeat
where the warp is 11 of 14. What the lane ships is the **meaning** of the
experimental switch: against the warp bungo judged, the swirl goes 7/14 -> 13/14
and per-sheet grain-vs-previous-build 1/14 -> 14/14, at 11/14 -> 9/14 of the
repeat.

### Red, for the director
1. F3 not met (repeat), so no default change and no `BAKE_INSTRUCTION.md`.
2. The warp beats the hex tiling on the repeat on the product; the earlier "no
   cost in repeat" claim was the offline prototype's and is withdrawn in
   section 6.
3. The hex tiling's **blotchiness at its 256-unit cell scale is ungated** -- no
   instrument in the lane measures it. `images/sheet_tiling4.png` at 1:1.
4. **`offline_bake.py` must not be used for absolute numbers again** (wrong by
   up to 0.27 on the repeat, both directions, and ~1 unit on the swirl).
5. G1 and G1-band cannot separate the arms: the **rung fails both** (-24.0 %,
   -42.9 %; 2/6 and 1/6 bands against 6/6).
6. (-36,-20) fails TILING2's **ratio** law while its amplitude falls 1.501 ->
   0.073, because the ratio's denominator is that sheet's own no-repeat floor
   and collapses with it. The warp fails it the same way. Possible artefact;
   the law is frozen and it was counted as a failure.

### Deliverables for the overseer to splice
`scratchpad/tiling4_20260912/WW_CHANGES_ENTRY.md` (product numbers),
`MISTAKES_ENTRIES.md`, this block. `docs/LODGEN_TERRAIN_VT.md` **is already
amended in-tree** -- 2.5e, two CLI rows, a provenance block with per-claim
anchors; `d0_doc.py` wrote it and `d1_doc.py` corrected its counts to the
product's (116,939 B, sha256 `b5fff76b436404d0`, CR 0, both refuse on re-run).
Pictures `images/cmp_tiling4.png` (3-up crop, numbers burned in) and
`images/sheet_tiling4.png` (the three sheets whole at 1:1).

### Bungo's calls
Swirls or blotches (the 1:1 picture is the comparison); which meaning the
experimental switch should carry; whether a cell-size / second-octave sweep on
**real** bakes is worth a lane; whether the ratio law wants a floor under its
denominator. Nothing was committed and nothing stashed.
