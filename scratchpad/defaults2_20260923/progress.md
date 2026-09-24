# DEFAULTS2 progress (lane, Opus 5.5)

- 14:24 read brief, CONSTITUTION, HANDOFF lines (BLENDEDGES1 :373, RULED 09:3x :383, IMPOSTORDEPTH2 :422-458).
- 14:26 exe release/NifSkope.exe sha1 6b8ed793 (= IMPOSTORDEPTH2 close). bungo's window pid 25584 runs
  release/NifSkope_inuse_25584.exe (locked); release/NifSkope.exe itself NOT locked (open r+b ok).
  Game down. RUNG = release/NifSkope.before_defaults2.exe (cp -p, sha1 6b8ed793).

## Where the two defaults live (found 14:2x)
Ruling 1, edge blend:
- src/lodgen.cpp:6348 `static int g_blendEdges = 0;`
- src/lodgenmanager.cpp:1468 panel row LodgenBlendEdgesBox default 0 + tooltip "Hard is the default"
- src/nifskope_ui.cpp:29223 panel-run gate table dflt "0" for LodgenBlendEdgesBox
- src/nifcli.cpp:7876 comment "`off` is the default"; no help line existed
- src/lodgen.h:452 comment; docs/LODGEN_TERRAIN_VT.md:1323 "(default `off`)"
Ruling 2, card grid:
- OLD DEFAULT = 8 x 8 frames at 128 px a frame = 1024-texel sheet (driver OCT 8 / TILE 128; panel cardFrames 8 /
  cardRes 128; hook fallback tile 128 when WW_IMPOSTOR_TILE unset). N already 8 everywhere; only the frame size moves.
- tools/bake_impostor_cards.sh:126,130 TILE default 128
- src/lodgenmanager.cpp:1085-1087 cardRes default 128 (fallback index 1)
- src/nifskope_ui.cpp:22831 hook fallback tile = 128
- docs/LODGEN_IMPOSTOR_SPEC.md frame law (s "The frame law"), docs/LODGEN_CARD_SHEETS.md
- Every card gate (lodgen_octahedral, impostor_draw, impostor_aa, impostor_shrubs, render_shot) spells
  WW_IMPOSTOR_TILE explicitly -> no pinned bytes move by this ruling.

## Code (14:28) -- fix_defaults2.py (anchored, CR-checked), fix_docs.py
- g_blendEdges 0 -> 1; panel row default Hard -> Cross-faded (+ tooltip); panel-run gate table dflt "0" -> "1";
  CLI help line for --blend-edges added (none existed); nifcli + lodgen.h comments.
- driver TILE default 128 -> 256; panel cardRes default 128 -> 256 (fallback index 1 -> 2); hook fallback tile 128 -> 256.
- docs: LODGEN_TERRAIN_VT.md s2.5a default line; LODGEN_IMPOSTOR_SPEC.md frame law head; LODGEN_CARD_SHEETS.md header.

## Pre-registered gates (written 14:28, before any run)
- terrain_gate.sh / terrain_check.py: G1 new bare == rung +quadrant (all files); G2 new +off == rung bare;
  G3 new bare vs rung bare moves ONLY {chunk colour DDS, VT.2.lodt, VT.4.lodt, (VT.lodm)}; G4 seam 0.977/0.992 vs 1.236/1.250.
- card_gate.sh / card_check.py: G5 hook new bare == rung TILE=256; G6 rung bare (old) moves every oct sheet + .txt,
  albedo long side 2048 vs 1024, oct line tile 256 vs 128; G7 driver bare == driver OCT=8 TILE=256, library tile 256.
- plus lodgen_octahedral.sh, impostor_draw.sh, impostor_trunk.sh, lodgen_terrain_vt.sh, lodgen_panel_run.sh (the panel
  default table moved), native_lighting.sh not reached (shader untouched) -> not run.

## Build 14:29-14:31 BUILD-RC=0, exe 24,091,136 B 14:31:22 sha1 d9ca02ab; 9 objects rebuilt (main nifcli nifskope_ui
  lodgen lodgenmanager lodgenchunkpass lodbfile cellview nativeemit); no object stale vs lodgen.h. Rung bakes R0 51 s, R1 4 s.

## Terrain gate 14:32-14:33 (new exe d9ca02ab vs rung 6b8ed793) -- PASS after one named amendment
- G1 N0(new bare) == R1(rung +quadrant): 9 of 10 files byte-identical; G2 N1(new off) == R0(rung bare): 9 of 10.
  The 10th both times = obj/Commonwealth.lodb, the bake RECORD: line 1 exe size, `baked` UTC time, output dirs,
  the TYPED switches, census wall times + peak memory. AMENDMENT (after the first run; the pre-registered gate was
  wrong to include a file that carries a clock): amend_lodb.py drops those volatile lines -> 13 stable lines,
  0 differ in N0/R1, N1/R0 AND N0/R0.
- G3 bare new vs bare rung: moves exactly tex/Commonwealth.4.-20.24.DDS (chunk colour), VT.2.lodt, VT.4.lodt
  (pyramid colour) + the record's volatile lines. Unmoved: _data.DDS, _msn.DDS, BTR, BTO, manifest, VT.lodm. PASS.
- G4 seam (TILING2 instrument via BLENDEDGES1 pics.py): rung bare 1.236 / 1.250 -> new bare 0.977 / 0.992. PASS.
- FINDING (measured, not fixed -- out of scope): the record's `switches` digest is the TYPED argument vector, so a
  bare bake on the rung and a bare bake on the new exe carry the SAME digest (65439eba...) and identical stable
  lines; the record's `out` rows list only BTR/BTO/manifest, not the colour sheets. `--incremental` compares the
  digest and no exe identity, so an incremental run on the new exe over a record written by an older exe will not
  rebake anything for this default flip: old hard-edged colour sheets stay. Same class as DEFAULTS1 (2026-09-12).

## Card gate 14:34-14:38 -- PASS after one named checker amendment
- G5 PASS: hook C1 (new exe, WW_IMPOSTOR_TILE unset) == C2 (rung, TILE=256), all 7 files.
- G6 first read FAIL, a CHECKER bug: the sidecar oct line now ends in a 13th token `spec1`, so the base
  is token 11, not the last. AMENDED 14:38 (named, after the numbers were in): card_check.py reads o[11].
  Measured: C1 albedo 1920x2048, base 256; C3 (rung bare = old default) albedo 1024x1024, base 128.
  Moved: the .txt + all 4 oct sheets; unmoved: front/side PNGs. G6 PASS.
- G7 PASS: driver bare D1 == OCT=8 TILE=256 D2, all 8 files; library.txt oct 8 / tile 256 / ref 1183.3.

## lodgen_terrain_vt.sh
- rung 6b8ed793: 45 checks, 1 failure (only "exe newer than sources", expected on a rung).
- new d9ca02ab before re-rung: 45 / 3 (V9a-1, -2, -3: assembled vs direct colour sheet, all 4 chunks).
- FINDING 2 (measured, not fixed): with the blend ON the two colour writers disagree. Stock chunk composite
  (no --vt, S0) vs pyramid-assembled (N0): 2,790 of 262,144 texels (1.06%), max 25 levels, 278 4x4 blocks,
  every one within 4 px of a quadrant line and within 8 px of the chunk edge. Both read seam 0.977. S0 vs S1
  (stock, blend on/off): 7.21% differ, so the stock path does blend. S1 == R0. Existed before, behind the flag.
- RE-RUNG (because of the ruling): V9a-3's OLDLAND gained `--blend-edges off` (fix_vtgate.py, dated comment).
  V9a-1/-2 NOT re-rung, left red at their bar.
- 14:38-14:40 new exe after re-rung: 45 checks, 2 failures = V9a-1 and V9a-2 only. V9a-3 passes.
- lodgen_defaults.sh: LAND_OLD (line 112) feeds only phase (b), which asserts old != default and that a BTR
  and a BTO moved; no pinned bytes, so the ruling moves nothing it checks. Not re-rung. (Its "way back"
  string no longer spells the full old look; noted, not changed.)

## GUI gates 14:41 -- started in sequence (gui_chain.sh): octahedral, impostor_draw, impostor_trunk, panel_run
- native_lighting.sh not run: no shader or lighting source changed (not reached).
- 14:43 lodgen_octahedral.sh: RESULT PASS, 116 ok, 63 s. impostor_draw.sh first run REFUSED (step 4: IMPOSTOR_LODM unset, 11 steps/1 failure in 4 s) -- my chain did not pass the subject; re-run queued after the chain with IMPOSTORDEPTH2's subject (blast_n4 lodm + TreeMapleblasted05.nif, gates2.sh).
- 14:52 impostor_trunk.sh (14:42:56-14:52:45): 38 checks, 3 failures = the 2 named crisp-end tear rows (el0 4.68%, el20 10.14%) + smooth end el20 T2 at 2 of 360 views (IMPOSTORDEPTH2's open finding). Every FAIL/measure line identical to impostordepth2 gates/impostor_trunk.flat.out. Nothing new.
- 14:54 lodgen_panel_run.sh: 137 checks, 0 failures (14 s). It does NOT read the panel default table; that table (LodgenBlendEdgesBox dflt now "1") is consumed by lodgen_byte_gate.sh phase (b) (forces every row to its default) and (c) (panel default bake vs bare CLI bake, chunk colour DDS included). Queued PHASES=bc after impostor_draw. Phase (a) not run: rung is before_panel1.exe and it asserts no default moved, which this ruling moves on purpose.
- 14:57 impostor_draw.sh (14:53-14:57, IMPOSTORDEPTH2's subject): 33 steps, 1 failure = row 5, the named 4x4 KNOWN RED (IoU 0.3920, same number as IMPOSTORDEPTH2). Row 5t (8x8 at 2k = the new default) PASS 0.7029.
- 15:11 lodgen_byte_gate.sh PHASES=bc on the NEW exe (14:57-15:08): 2 failures.
  (b) 132 checks, 1 failure: "blendMargin (with blendEdges on) 128 -> 144: SAME bytes". CAUSE (read in code): the
      row turns its dependency on with bumpRow(), which flips a combo 0 <-> 1; the blend now defaults to 1, so the bump
      turned it OFF and the margin was measured with nothing to reach. A gate artefact caused by the ruling.
      RE-RUNG prepared: fix_bytegate.py (row gets depVal "1"; a dependency already at its named value counts as on;
      no other of the 58 rows sets a depVal, so only this row is reached). Needs a rebuild of nifskope_ui.o.
  (c) panel vs CLI: 15 identical, 2 differ = chunk colour DDS + Commonwealth.lodi. The gate's forced-default table
      still holds the pre-09-12 land values (landHex 0, warp 0, mip 0, guide 0, roadGroundPaint 1, nativeLadder 0),
      while the bare CLI bakes today's look -- a CLI bake with those land values typed out still differs from the
      panel's, so more rows than land disagree. Baseline owed: running the same PHASES=bc on the rung (15:09-).
- 15:24 RUNG baseline, lodgen_byte_gate.sh PHASES=bc on 6b8ed793 (15:10-15:24): (b) 132 checks, 0 failures
  (blendMargin MOVED there); (c) the SAME 2 differ (chunk colour DDS + .lodi), 15 identical -> phase (c)'s red is
  PRE-EXISTING, not this lane's. New vs rung: panel DDS moved, CLI DDS moved (both front ends took the blend),
  .lodi unmoved on both sides. So the only red the ruling adds is (b) blendMargin -> re-rung.
- 15:24 bungo's window gone (no NifSkope in tasklist), game down. fix_bytegate.py applied; rebuild started.
  The patch touches only the WW_LODGEN_GATE self-test block; gates already run on d9ca02ab stand.
- 15:26 rebuild BUILD-RC=0, only nifskope_ui.o recompiled, exe 24,091,648 B 15:26:42 sha1 90e8a57e, MZ, newer than source.
- 15:27-15:37 lodgen_byte_gate.sh PHASES=bc on 90e8a57e: (b) 132 checks, 0 failures -- "blendMargin (with blendEdges at 1)
  128 -> 144: bytes MOVED (14 files)"; (c) 15 identical, 2 differ (DDS + .lodi) = the rung's pre-existing red, unchanged.
  Byte gate now reads exactly as on the rung.
- 15:38 DELIVERABLE_TEXT.md + DONE (PARTIAL) written.
