# MERGE1 DONE -- lane report (started 2026-09-29 00:01)

## 0. Start
- main @ db4d92ea; night worktree on night-trial @ 5b338d39

## 1. Merges (00:05) -- all 7 clean, no conflicts
night-20260927 was 8f58e7db (= origin, nothing unpushed; the worktree sat on night-trial 5b338d39, also = origin).
Checked out night-20260927, merged with --no-ff in the brief's order, pushed:
| lane head | merge commit |
|---|---|
| water1 dfc243f5 | 3c2f2eb2 |
| ground1 625f0a44 | d687828a |
| tidy1 d0860071 | 454360ba |
| ident1 470a2632 | 67e05787 |
| terr1 4c04abe8 | 07c9779c |
| tiling5 995cffeb | 2499067b |
| flat2 e8a10631 | 3cde06d7 |
Textual conflicts: none (git ort merged every file). tiling5 and flat2 were cut from night-trial, so they carry the
four trial merge commits too (older heads of water1/tidy1/ident1/terr1, all ancestors of the heads merged first).
TILING5 switches: `g_landHeightBlend = false`, `g_landMacro = false` (src/lodgen.cpp 7457-7458); only the CLI
`--land-height-blend on`, `--land-macro on` or `--ground-sampler relief` turns them on; the panel never calls them. OFF confirmed.

## 2. Build (00:08)
scratchpad/merge1_20260929/build.sh (= night_20260927/build_trial.sh on branch night-20260927): touched the 25 sources
changed since 8f58e7db, 70 objects compiled, make rc 0, exe newer than sources, style/shader copies in step.
**release/NifSkope.exe sha1 6dc429d1085c87871a3dd893d65fd8e38be573c2**, 26,247,680 B, 00:08:24.
Lane strings present in the exe (one per lane): TIDY1 labels, FLAT2 --no-collapse-uniform, TILING5 --land-height-blend,
IDENT1 --identity-join, TERR1 --no-stamp-normals, WATER1 --no-water-bodies / WW_LODL_HEIGHT_RANGE, GROUND1 env switch.
Run copy for bakes: scratchpad/merge1_20260929/run/ (same bytes).
DeepSeek: surveyed the 7 lane DONE.md files for bake recipes, gates and harness legs -- deepseek-v4-pro, 231 s, $0.14 (3 citations spot-read, correct).

## 3. Boston re-gate (bakes 00:11-, Boston box cells -8..3 x -12..-1, full chunk recipe: objects + terrain VT + cards + arrays)
Three bakes into scratchpad/merge1_20260929/g/: **rung** = the night-20260927 exe before the merges (GROUND1's run_rung,
sha1 da128947); **off** = merged exe with every lane's way back on (`--no-water-bodies`, WW_LODGEN_NO_VERTEX_GROUND=1,
WW_LODGEN_KEEP_BLACK_EMISSIVE=1, WW_LODGEN_NO_LAYER_DEDUPE=1, `--identity-join proximity --occluder-fit piece`,
`--no-stamp-normals --no-sky-objects`, `--no-collapse-uniform`); **on** = merged defaults. Each ~11 min.

### 3a. All ways back = the old bytes (rung vs off), cmp_trees.py -> cmp_rung_off.txt: PASS
254 files each side. 228 identical; 26 differ, and every one is accounted for:
- 17 chunk-cache `.key` files and the `.lodb` bake record: bookkeeping only (exe size, bake time, output paths, the
  switch list, chunk digests). The `.lodb` carries no other change.
- `flat_objects_report.txt`: one header line, the path of the (empty) exceptions file beside the exe.
- 7 `.BTR` files: WATER1's own gate (btr_cmp.py) on all 17: **164 non-water blocks byte-identical, the 17 water
  shapes keep positions and triangles and lose their vertex colour -> PASS**; its planted one-byte change FAILS as it
  must. The water vertex-colour bake was dropped at bungo's word and has no way back by design.
- Comparator's own red controls (a flipped byte, a dropped file) fire.

### 3b. GROUND1 gates (ground1_gates.py = its gates.py pointed at the merged tree) -> ground1_gates.log
- recompute within 1 level: **99.9984%** (16 vertices > 1, max 7) -> PASS (bar 99.9%)
- nothing more than 256 u above ground reads non-zero -> PASS; the ramp law (within 5 u >= 250: 101,893 of 101,893;
  0..16 u >= 239: 10,165 of 10,165) -> PASS; red controls (old value, stream + 2) all fail as they must.
- "physical as briefed" (within 16 u >= 250) FAIL and "placement mean within 2 of the old byte" **98.353%** FAIL:
  the same two brief faults GROUND1 recorded (the brief's bar contradicts its own law; the two compare different
  vertex sets). Not loosened, not a code defect.
- its "identity" line FAILS on the 26 files of 3a (it does not mask bookkeeping or the dropped water colour); 3a
  accounts for every one.

### 3c. TIDY1 gates (off vs on, "on" mode) -> tidy1.log: 6 of 6 PASS
21 of 21 black glow files gone (50,064,092 B); 2 of 2 lit ones kept byte-identical; no other file gone; 114 of 114
sources resolve to identical texels; 114 -> 106 layers (8 merged spellings); 647 of 647 A lines resolve.
Same numbers as the lane.

### 3d. IDENT1 gates (on bake's group dump, tol 32, cap 4096) -> ident1_gates.log, ident1_occ.log, ident1_cov.log
- 21,140 groups (1:16,042, 2-4:4,474, 5-16:300, 17-64:149, 65-256:134, 257-1024:41, >1024:0); no multi-piece
  group over the cap (widest 331 pieces 4,096 x 3,376 u) -> PASS
- Hub tower east 1 group, west 4, Trinity 1 -> PASS. **Diamond City 15 groups -> FAIL; row houses 54 pieces in 1
  group -> FAIL** (IDENT1's known fails, need bungo's ruling on a landmark join rule; code: IDENT1's contact join).
- **Occluder poke gate: 511 boxes, 5 over 1%, worst 0.1605 -> FAIL** (IDENT1's known fail; its floor, the same boxes
  grown 1.25x, leaves 510 of 511 -> the gate can fail).
- Street coverage, IDENT1's three eyes: occluded 0.5781 / 0.6383 / 0.4637, **mean 0.560**, culled 0.7275 = the lane's.
  (coverage.py had to be pointed at the merged tree's decoder: IDENT1's tree reads .lodi up to v11 and refuses v12.)
- Its offline re-run line prints "partition == groups.py: NO (20 groups the emitter keeps apart; 1,308 pairs sit at
  exactly 32 u in the dump, which rounds to 0.001)". IDENT1's report never recorded this line; it reaches IDENT1's
  contact join vs its own Python re-run; most likely the rounding at the tolerance edge, not proven.
All IDENT1 numbers are identical to the lane's own after-bake: the merge changed nothing in its output.

### 3e. TERR1 gates (its scripts, on vs off vs no-roads; copies in terr1_tools/ point at the merged tree's decoder)
Every output file below is BYTE-IDENTICAL to TERR1's own final run, so its verdicts carry over unchanged:
- canyon.py -> terr1_canyon.json identical: street canyons 225.5 -> 122.1 (all canyon texels), open ground within
  1 level 99.36%, correlation 0.9996 -> PASS
- physlaw.py (ray cast through the LOD triangles, 312 samples) -> identical: canyon mean 87.4 vs physical 98.8
  (bar 70-114) PASS; every named street >= 52 (min 53.9 Theater) PASS; all-texel bias -6.0 (bar |10|) PASS
- opengate.py -> identical: 0 of 3,860 blocks with no object in reach change; planted 1 -> PASS
- nrmgate.py -> identical: G1 stamp share, G3 orientation, G4 colour + height sheets identical -> PASS; G2 here is
  the old proxy (324) that TERR1 replaced with g2gate.py (0 of 178,906, PASS). g2gate needs the stamp record from
  TERR1's diagnostic build, so it was NOT re-run: carried over, not re-measured.
- railprof.py (on / off / no flat objects / no roads) -> terr1_rail.json identical: rail track cell (-2,-10) ON 4
  sign crossings, OFF 0 -> PASS
- Still not right (TERR1's own note): the deepest canyons are darker than physical (Theater 53.9 vs 92.3,
  Beacon Hill 61.7 vs 96.6).

### 3f. FLAT2 (sea-edge bakes: rung / off = --no-collapse-uniform / on)
FLAT2's gates.py, run through a `bakes/` junction to my g/ folder (its path masking only knows `bakes/<name>/`):
- Every .lodt check PASS with the lane's numbers: G1 every .lodt OFF == RUNG byte for byte (2 files); G2 same .lodt
  set; G2 every tile (flags, non-collapsed sheets identical, collapsed = one value = its record, expanded == OFF, crc)
  0 failures; refuters R1, R2, R3 all come back red; no uniform sheet left uncollapsed. 180 sheets, 48 collapsed,
  files 54,933,696 -> 34,087,616 B (37.95% saved) -- same as FLAT2's own run.
- Two lines print FAIL. Neither is FLAT2's code, and no gate was loosened:
  - G1 OFF == RUNG: 11 data files differ = 9 sea .BTR (WATER1 dropped the water-shape vertex colour on purpose;
    WATER1's btr_cmp on these: 150 non-water blocks byte-identical, 15 water shapes = PASS) + the .lodb + the flat
    objects report. The .lodb differences are exactly: exe size in line 1 (rung exe vs merged exe), baked time, the
    bake folder paths (the .lodb records g/, not the junction), the extra ways-back switch words, and the chunk and
    .BTR hashes that follow from them.
  - G2 ON == OFF (non-.lodt): 1 data file differs, the .lodb: baked time, folder paths, the `--no-collapse-uniform`
    switch word and its switches hash. Nothing else.
- WATER1's own lodl_cmp / bake_cmp no longer apply to the merged tree (they compare against the rung's v3 file,
  and the header grew to 0x100 in the lane's last commits); the v3 writer is covered by lodl_water.sh and
  lodl_write.sh in section 3g (below the bake: they ran after it).
- TILING5: its switches are off by default, and 3a (rung vs off) shows off = rung bytes on every .lodt. Its old
  "off == old rung" check against the pre-TERR1 rung is superseded: TERR1 changes the ground sheets on purpose
  (and its own ways back give the old sheets byte for byte, 3e).

## 5. Whole-Commonwealth bake (00:58:48 - 02:52:08, 6,800 s, rc 0)
- Merged run exe 6dc429d1, INCR2's bake_ws.sh plus `--no-collapse-uniform`, under turn MERGE1, into
  scratchpad/merge1_20260929/stage (not in git). Game was down (tasklist checked).
- GPU ran: RTX 5070 Ti, OpenGL 4.3; BC7 self-check error GPU 26,082,378 <= CPU 26,090,366; 163 images /
  6,073,344 blocks on the GPU in 22.9 s, 0 fell back to the CPU.
- 3,060 chunks baked, 486 .BTO; card sets reused (1,028 files, digest 5052e2c9bca5).
- stage/mod/FO4CSLOD/Commonwealth: 3,558 files, 15,905,725,868 B. .lodl = version 3, .lodi = version 12,
  5 .lodt = version 2 written in full (the .lodb records `--no-collapse-uniform`).

## 6. Install (game down, tasklist checked)
- Whole installed mods\FO4CSLOD backed up first (section 1b): E:\Projects\Fallout 4 Mods\backups\FO4CSLOD_before_MERGE1_20260929
  (5,072 files, 21,038,690,722 B, both sides counted).
- `python install.py <stage>/mod/FO4CSLOD Commonwealth` (INCR2's installer; only its backup and log paths changed):
  3,656 staged, 3,135 already equal, 521 copied, 22 stale removed. rc 0.
- Every replaced or removed file was also copied aside first: E:\Projects\Fallout 4 Mods\backups\FO4CSLOD_replaced_MERGE1_20260929
  (542 files, 15,569,820,560 B). Per-file log: scratchpad/merge1_20260929/install_log.tsv (543 lines).
- Check after install: installed Commonwealth folder = stage, 3,656 files and 15,905,725,868 B each; .lodl, .lodi
  and VT.2.lodt byte-identical.
- **FO4CS today refuses this .lodl (v3) and .lodi (v12)**: far LOD for the Commonwealth may be missing in game until
  the FO4CS readers land. To go back: copy FO4CSLOD_before_MERGE1_20260929 over mods\FO4CSLOD.

## 3g. Harness legs on the merged run exe (02:52 - 03:30, turn MERGE1, second monitor, own ports)
- lodl_channels.sh: 54 checks, 0 failures, PASS.
- lodi_v7.sh: PASS (G3 Python reader accepts the v7 pair; 6 checks, 0 failures).
- lodl_write.sh: RESULT PASS.
- lodgen_native.sh: 34 checks, 3 failures, RESULT FAIL. Legs 1, 2, 4, 6-13b and 14 pass (leg 14: 2 SKIPs, the bake
  leaves no version-5 .lodi to doctor). The three failures:
  - Leg 3, "v3 lodo unknown header flag bit": STALE CHECK, FIXED. The mutation set bit 0x10, which lane NEAR1 (09-26)
    made the NEAR flag, so the reader refused it by the NEAR rule, not as an unknown bit. The check's meaning is clear
    ("a bit outside the known set is refused as reserved"), so it now sets 0x20, the lowest bit outside
    LODO_FLAGS_KNOWN (0x1F) (tests/spells/lodgen_native_mutate.py). Rerun on the leg's own fixture: 48 checks,
    0 failures; the old run is the red control (it went red on the wrong refusal).
  - Leg 5, "unaccounted: products" (and the "differ ONLY in the command-line digest" line after it, which fails
    only because the tool exits red on the first): STALE CHECK, NOT FIXED. Lane INCR2 added `product` rows to the
    .lodb; the ledger comparer's key groups (tests/spells/lodgen_btofree_ledger.py SHAPE_KEYS / TARGET_KEYS) were
    never told. Both records carry 0 product rows here. Whether products are "outputs the two bakes must agree on"
    or "things the FO4CS target alone writes" is a decision, not a typo, so the check is left red for the owner.
    Every other line of leg 5 passes, including its refuter.
  - Leg 13c (IDENT1, default building boxes): 302 boxes, 6 poke out by more than 1%, worst 0.1605; floor (grown
    boxes leak) fires 300 of 302. IDENT1's own known FAIL, same worst box; needs bungo's ruling.
- lodl_water.sh: RESULT FAIL. Reaches WATER1's last commits (sloped placed water, header 0x100), and the harness
  predates them:
  - G5 census vs the offline oracle: C++ 353 bodies vs oracle 346 (river 118 vs 115, lake 234 vs 230), 16 vs 15
    WATR forms, wet area 21,755,123 vs 21,754,958 texels. The 7 extra bodies are WATER1's 7 new sloped bodies; the
    oracle does not model sloped water. Not fixed: that needs the oracle extended, not a check changed.
  - G8 header layout: "sections moved by exactly the 88 bytes the header grew" and "first section at 0xF8" -- the
    v3 header is 0x100 now. "0x08..0x44 identical" also fails; which field moved was not diagnosed.
  - G7 (independent decoder, 353 bodies, every rule) PASS; G8 byte-identical second write PASS.
  Owed to WATER1's owner: teach lodl_water.sh the sloped bodies and the 0x100 header. Not loosened.
