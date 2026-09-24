# VTNORMAL1 progress

Lane launched 2026-09-23 22:10 (clock read). Brief: scratchpad/brief_vtnormal1.md.

## 0. Setup (22:10)
- Game down, no NifSkope running (tasklist 0 hits).
- Rung: release/NifSkope.before_vtnormal1.exe = add1bf84 (copy of release/NifSkope.exe).
- Read: CONSTITUTION, skill nifskope-ww-lodgen, search-lean, VT contract s1 head, s2.1-2.4, s3, s4, s5, 7a.

## 1. Design (22:17)

### 1a. Density flag
Estimator on the rung (whole Commonwealth, no cover, no height / with --vt-height):
- finest 2 / content 256 = 32 u/texel: 1.71 GB / 4.02 GB
- finest 2 / content 512 = 16 u/texel: 6.44 GB / 14.98 GB
- finest 1 / content 256 = 16 u/texel: 6.84 GB / 16.10 GB (one more level, dim 1)
- finest 1 / content 512 =  8 u/texel: 25.76 GB / 59.98 GB
Today's flags CAN already express all three; nothing new is needed underneath. New flag
`--vt-density 32|16|8` is a name for them: 32 = finest 2 / content 256, 16 = finest 2 / content 512,
8 = finest 1 / content 512 (one knob per step, ladder dim 2..32 kept for 32 and 16). Refused by name
beside --vt-finest / --vt-content. CLI default stays 32 (byte gates pinned); PANEL default 16
(ruling). Panel: LodgenVtFinestBox becomes "Finest texel size" with 32/16/8 units, QSettings
LodGeneration/vtDensity (default 16); the separate content spin row goes (content follows density).
Gates: --vt-density 32 == rung default; 16 == rung --vt-content 512; 8 == rung --vt-finest 1 --vt-content 512.

### 1b. Half-resolution aux tiles (--vt-half-aux, panel row, key vtHalfAux, default OFF)
A half sheet = the full sheet with mip 0 dropped: stored mips 1..mipCount-1 (136 px at 272).
Border 4 at 136 is still a multiple of 4 at its coarsest mip -> rule 12 holds. Colour keeps mip 0.
Declared per sheet in descriptor byte 6 (`mipSkip`, 0 or 1, previously always zero); lodvSheetMipBytes
/ lodvTileRawBytes skip the dropped mips. Index sheets[] gains mipSkip + texels, top-level halfAux.
NO version bump: OFF is byte-identical (byte 6 = 0 as before); an old reader facing an ON file
refuses it at the rawBytes rule (16) instead of misreading it. Refused with --vt-mips 1.
Staging stays full size (filter, chunk assembly untouched); lodgenVtEncodeTile skips mip 0.

### 1c. Pyramid normal from his sheets
Active when the msn cache dir is set (--msn-cache / panel field). After each finest tile is baked,
st.msn is overwritten texel by texel: every sheet texel under the texel's world box ((upt/8)^2 of
them) is decoded to a unit vector, summed, renormalised, encoded by the shared lodgenTerrainMsnPixel.
At 8 u/texel it is a 1:1 copy. Texel boxes sit on the 8-u grid (tileW multiple of 4096, upt multiple
of 8). Sheet texel (px,py) of chunk (cx,cy): x = cx*4096 + (px+0.5)*8, y = (cy+4)*4096 - (py+0.5)*8
(row 0 north; PROVEN below with a feature, not assumed). Sheets are read once per chunk, pre-reduced
to the pyramid density, and dropped by chunk row. Coarser levels: the existing filter already
averages and renormalises -> a vector box filter per level. A second staging plane (heights normal)
is kept only while the cache is on, filtered alongside, and read by the .btr chunk-sheet assembly, so
chunk sheets stay byte-identical to the rung with the same cache. Texels with no sheet keep the
heights normal. Census: normalMsnCache / normalHeights / normalMixed (finest tiles), msnSheetsRead,
msnSheetsMissing, in the vt: line and in the index (terrain.normalSource).

### 1d. Gates (bar fixed in section 2 BEFORE code)
- new row: L02 normal over Sanctuary vs his sheet box-downsampled, r >= BAR on east and north.
- no cache -> every output byte-identical to the rung.
- with cache -> colour/mask/height per tile identical to the rung; only msn differs.
- density equivalences (1a); half-aux OFF identical.
- terrain_vt.sh, lodgen_perf.sh, tiling gate as reached; WW_LODGEN_TEST for the panel.

## 2. Pre-code measurements and the bar
Rung bakes of Sanctuary chunk (-20,24) dim 4 (out/rung_nocache, out/rung_cache; bake.sh): 4 s / 6 s,
945,123 B. The rung's .lodt is byte-identical with and without --msn-cache (cache reaches only chunk sheets).
measure.py (L02 = level dim 2, mip 0, content cropped, 512 px mosaic north-up; his sheet box-averaged
4x4 in vector space and renormalised = 32 u/texel):
- his downsampled SD east 0.299, north 0.270 (plenty of signal; a correlation is meaningful)
- rung L02 vs his (row 0 = north): r east 0.657, north 0.745  <- the rung (brief quoted 0.58 on its own crop)
- rung L02 vs his (row 0 = south, flipped): r east 0.153, north 0.136
- vanilla _msn (512 px, BC3/DXT5) vs his row0=north: east 0.970, north 0.910; flipped: 0.127 / 0.125
- ORIENTATION PROVEN: his sheet's row 0 is NORTH (like the .lodt), R = +east, B = +north. Flipping
  drops every r to ~0.13 against both the heights normal and vanilla's own sheet.
- codec ceiling: his downsampled through a simulated BC1 (PCA range fit, 565) vs itself: east 0.970, north 0.925.
BAR (set here, before any code): new row passes when the L02 normal vs his downsampled sheet reads
r >= 0.88 on BOTH east and north. The rung reads 0.657 / 0.745 -> FAILS. The codec ceiling is 0.970 / 0.925.

## 3. Build 1 + first gate run (22:30 build, 22:37 notes)
Build 1 clean (0 error:, exe 22:30:20). gate.sh phases a-c:
- (a) no cache: tex/, obj/ (.BTO .BTR manifest), mod/ (.lodt .lodm) all byte-identical to the rung.
  Only the .lodb differs; it is the provenance record (exe size, clock, output paths) and it copies the
  census vt: line, which now carries the new words. Rung vs rung-with-cache .lodb differs too. Gate now
  compares the .lodb minus those.
- (b) with cache: census normalMsnCache 4 normalHeights 0 normalMixed 0 msnSheetsRead 9 missing 0.
  VT.2 and VT.4: colour + mask byte-identical to the rung on every tile, normal changed on every tile;
  --lodt-check accepts. FAIL: the chunk _msn.DDS moved 3 bytes (of 22 MB) by one step. The rung is
  deterministic (rung_cache2 == rung_cache). Cause: the cache reader's loop got a vector branch inside
  it, which compiles differently under -march=haswell. Fix (edit_split.py): the rung's two loops
  (DDS and PNG) restored verbatim as their own functions; the vector path has its own loops. Build 2.
- (c) bar: L02 vs his downsampled r east 0.9449, north 0.8624 -> the pre-set bar (0.88) FAILS on north.
  Diagnosis (shift.py, encceil.py): no offset (every +-1 px shift drops r to 0.55-0.77). The cause is
  lodgen's own BC1 encoder: endpoints = min/max-LUMINANCE texels, and luminance weights B (= north) at
  0.114, so north is nearly ignored when endpoints are picked. Ported that encoder to numpy: its ceiling
  on his downsampled sheet is exactly east 0.9449 north 0.8624, and the tile equals
  lodgen-BC1(his downsampled) on 100% of texels within 1/64 (r 0.9999 all three axes). The transfer
  is exact; the shortfall is the encoder, which the heights normal goes through too.
  MY MISTAKE: the bar's ceiling came from a PCA encoder (measure.py bc1_sim), not the encoder the bake
  uses. The bar stays as set and is reported as FAILED on north; the transfer row is added after the
  bar and is labelled so. A PCA endpoint fit for normal sheets would lift north to ~0.925 (follow-up,
  not in this lane: it would move the no-cache bytes).

## 4. Builds 2-3, the gate green but for the bar (23:04)
- Build 2 (loop split) still moved the same 3 bytes. fmaprobe.py re-encoded his whole 2048 sheet
  under every contraction of e*e+n*n+up*up: the rung's chunk bytes are fma(up,up,fma(n,n,e*e)) on
  4194304 of 4194304 texels, build 2's are fma(up,up,fma(e,e,n*n)). A texel whose north lands at
  37.999997 flips 37 -> 38. Build 3 (edit_fma.py) spells the rung's fused form with std::fma in the
  DDS reader (and the vector loop). PNG reader: identical to the rung already (pngcache/ made from his
  sheets, mkpng.py; chunk _msn byte-identical).
- gate.sh on build 3 (exe 22:49:07): 27 ok, 1 fail = the pre-set bar on north (0.8624), explained in
  section 3. (a) no cache byte-identical; (b) chunk sheets + objects identical with the cache, colour and
  mask identical per tile, normal changed per tile; (c) transfer 100% within 1/64, rung 0.5%;
  (d) density 32/16/8 == rung pairs; (e) half-aux: aux sheets == full sheet mips 1.., colour whole,
  --lodt-check ok, the RUNG's reader refuses a half-aux file (rawBytes vs header), OFF == rung;
  (f) three refusals rc 2.
- lodgen_vt_check: header 12 ok, tiles 7 ok on a half-aux file; filter now says it does not apply to
  a half-aux height sheet instead of an IndexError (edit_check2.py); border errors on both full and
  half 2x2-tile bakes (the checker indexes an east neighbour that a 2-tile row does not have: its own
  limit, not this lane).
- 2x2-chunk region (-24..-17 x 16..23) with his sheets: rung vs new tex/ byte-identical INCLUDING the
  dim-8 chunk _msn assembled from the kept heights plane. census normalMsnCache 16, sheetsRead 16.
- Seams (seam.py): step across the chunk edge 0.3967 vs other BC1 block edges 0.3930 (1.01x); his own
  sheets 1.03x at the same edge; tile border texels == neighbour content exactly (0.0000, 24 edges).
- Picture: sanctuary_L02_normal.png (heights old | his downsampled | vanilla _msn; row 2 lit by one sun
  from the south-west, 35 degrees up).
- Sizes, whole Commonwealth, the estimator (exact tile arithmetic, not a region), with ground cover:
  | density | full | half-aux |
  | 32 u | 2.22 GB | 0.93 GB |
  | 16 u | 8.26 GB | 3.40 GB |
  |  8 u | 29.71 GB | 12.90 GB |
  (+2.14 GB of chunk sheets when --tex-dir; without cover: 1.71/0.80, 6.44/3.02, 25.76/12.08 GB;
  with --vt-height: 4.02/1.26, 14.98/4.73, 59.98/18.92 GB.)
- Times: 16x16-cell region (256 cells, 85 tiles; 341 at 8 u), textures stage with his sheets:
  32 u 16.3 s (half 13.2), 16 u 33.6 s (33.0), 8 u 118.9 s (117.9); no sheets 11.2 s / 32.3 s.
  Whole Commonwealth has 144x the tiles -> ESTIMATE from that region: ~39 min / ~81 min / ~4.8 h
  (half-aux about the same: the encode of mip 0 is skipped but the staging is not).

## 5. Harnesses, sizes, times, the panel (23:17)
- lodgen_terrain_vt.sh on exe 22:49:07: 45 checks, 0 failures, RESULT PASS.
- Seams (2x2-chunk bake with his sheets): step across the chunk edge 1.01x an ordinary block edge (his own sheets 1.03x, rung 1.03x); border law 0.0000 over 24 edges.
- 8 u bake: L01 vs his r 0.966 / 0.935; L02 0.942 / 0.866, filtered from L01 (docs 2.1 stale; text in DELIVERABLE_TEXT.md).
- Sizes, whole Commonwealth, estimator, with cover (pyramid only; +2.14 GB chunk sheets): 32 u 2.22 GB full / 0.93 half; 16 u 8.26 / 3.40; 8 u 29.71 / 12.90. No cover: 1.71/0.80, 6.44/3.02, 25.76/12.08. With --vt-height: 4.02/1.26, 14.98/4.73, 59.98/18.92.
- Times (256-cell region, textures stage, his sheets on): 32 u 16.3 s full / 13.2 half (11.2 without sheets); 16 u 33.6 / 33.0 (32.3); 8 u 118.9 / 117.9. Commonwealth ESTIMATE x144: ~39 min / ~81 min / ~4.8 h.
- Job 5: msnCache persists (LodGeneration/msnCache); in his profile it is EMPTY, so his upscaled normals are not in use. Row relabelled "Normal sheets folder". His vtFinest/vtContent keys are superseded by vtDensity (absent), so his panel opens at 16.
- DELIVERABLE_TEXT.md written 23:16. lodgen_perf.sh running (phase a at 23:17).

## 6. lodgen_perf, the director's addition, build 4 (01:24)
- lodgen_perf.sh on exe 22:49:07: (a) serial == parallel, regions A (57 files) and B (99 files) every byte; (b) workers ran; (d) reuse ok x4; (e) ok x2. (c) RED: 3 files differ (.lodo 6.2 MB vs 223 MB, .lodi, .lodb). NOT this lane: the harness hands the exe under test "--library near --native-ladder" to match a rung from before 2026-09-17, but this lane's rung (add1bf84) already has the new defaults (its log: "--native-no-ladder", "--library mnam"). So leg (c) compared two different switch sets. Re-run with equal switches: waybk.sh (after build 4).
- Director addition (bungo 2026-09-24 00:3x): his Upscaled Terrain Normals mod as the sheets' source.
  - The reader took ONLY the folder that holds the files (<dir>/<name>_msn.DDS); his mod ROOT gave nothing. Fixed: lodgenMsnSheetPath tries <dir>, then <dir>/Textures/Terrain/<world>, then <dir>/Terrain/<world> (world = name before the first dot). His 2304 files match the name pattern exactly (0 others); format is what the DDS reader already takes.
  - "auto" (CLI --msn-cache auto; the panel's EMPTY row): the last resource-stack FOLDER holding <world>.4.*_msn.DDS wider than vanilla's 512 (vanilla dim-4 _msn is 512, his 2048). "none" in the panel = off. Census: "msnCacheDir <folder> (auto)".
  - Panel saves settings only on Generate (and the harness property), never on close; so closing his window does not overwrite msnCache, but pressing Generate in an OLD open window writes its field (empty) back.
  - His profile: source 0 (Specified), resources EMPTY, msnCache EMPTY, so auto finds nothing for him; his key must name the folder.
  - Gate: gate_auto.sh (A subfolder, B mod root == A, Br rung + mod root != A, C auto+resource == A, D auto over a vanilla-512 fixture == no cache, F auto without resources == no cache). cmp16.py for the 16 u on/off numbers.
- Build 4 started 01:24.

## 7. Addition results, final gates (01:40)
- Build 4 01:25:34 (7212fdc9): gate_auto.sh 7 ok 0 fail; sharper old-code proof on chunk sheets alone: rung + sheets' folder == new A; rung + MOD ROOT == no cache (so the old reader read nothing there).
- waybk.sh (perf leg c, equal switches, region A): 56 files, 0 differ. Leg (c)'s RED is the harness's switch mismatch.
- 16 u on vs off (cmp16.py, 2x2-chunk region): mean 16.0 deg apart (median 14.7, p95 32.8; 92 % > 5 deg). ON vs his r 0.939 / 0.836 (8.8 deg), OFF 0.576 / 0.633 (17.5 deg). Mean slope: his 22.6, ON 21.0, OFF 12.5 deg.
- gate.sh on build 4: 27 ok, 1 fail (the same pre-set bar).
- lod_generation.sh on build 4: 127/1 -- MY check: it moved the density row to index 1, now the default. Fixed (step away from the current entry). Build 5 01:35:28 (97d2b3a2, nifskope_ui.cpp only): 128 checks, 0 failures, PASS.
- His profile: no NifSkope running; LodGeneration/msnCache set to "E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals" (was empty). vtDensity reads 16. The panel writes keys only on Generate, not on close.
- DELIVERABLE_TEXT.md updated. DONE marker written.
