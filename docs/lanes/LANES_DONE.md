# NifSkope WW lanes: landed and dropped (archive of LANES.md; newest first)

## LANDED
| Lane | sha |
|---|---|
| BAKESPEED2 (native write: vertex-AO cast per receiver, fixed-order ray length; gates byte-identical, buried 241743; 10-22) | a67c34e6 |
| FXCHECK1 | fde1939c merge of 8ad3aadf (10-22, MERGE1; glass env mask + still-shot pre-roll, both off; barrel plume still open) |
| VOLAIRSKY1 (+ FOGTRICUBIC1) | e77c59db merge of 8e5dc845 (10-22, MERGE1; tricubic off; D2b/D3 lines run in BATCH2) |
| BAKESPEED1 | 174a4bc7 merge of 13775118, b63e9a15, e6d62172 (10-22; bake VT rows + chunk jobs on all cores, VT/chunk resume) |
| ALLON2 AO-unit fix + edge guard | caec245f + 9c624f47 (10-21, MERGEALL1) |
| SSGI1 unit guard fix | 2f6a7ccd (10-21, MERGEALL1) |
| LIGHTSYS2 | 2c69b160 (10-21, MERGEALL1; real-cell gate owed) |
| BAKEINCR1 | 250b73e5 (10-21, MERGEALL1) |
| MORPHCYC1 | 23908f49 (10-21, MERGEALL1) |
| NEAR2 | 7408b6cc (10-21, MERGEALL1) |
| OPENCELL1 | 6f59be4d (10-21, MERGEALL1; STRINGS + .esl owed) |
| CELL1 | 7515eef3 (10-21, MERGEALL1) |
| PGI0 | 66d851d3 (10-21, MERGEALL1) |
| LIGHTBOOTH1 | 59f38a5a (10-21, MERGEALL1) |
| FARGRID1 | ac8ecf26 + gates 4cdf645d, 0ff14fa0 (10-21, MERGEALL1) |
| SSGI1 | 0220f7af (10-21, MERGEALL1; gate owed -> SSGI2) |
| VOLFIT1 | f626c12d (10-21, MERGEALL1) |
| TERRSHADOW1 | 43632f85 (10-21, MERGEALL1) |
| CELLSTREAM1 | 443bd767 (10-21, MERGEALL1) |
| PERF1 | b13a8ce7 (10-21, MERGEALL1) |
| VOLAIR1 | 4af8d7f4 (10-21, MERGEALL1; sky gate -> VOLAIRSKY1) |
| CARDBAKE2 + BAKEMEM1 | 0ece5e89 (10-21) |
| HORIZON1 | 8cb3de5a (10-21) |
| allon1 | 771d67dc (LAND3 78f87254) |
| SKYPIC1 | 5a219957 + ac6f6049 |
| SKYSTR1 | af4882f1 |
| GIBLACK1 | f910b643 + 83eff728 (cherry-picked) |

## SUPERSEDED / DROPPED
| Lane | why |
|---|---|
| FIXGATES1 (BATCH1 fails: SSGI2, RTBAKE1, SHADOWPAR1, PBRALBEDO2) | DONE 10-22: its four fixes ride BATCH2/BATCH4 (RESUME1); was: DONE 10-22 10:2x: all four BATCH ready (rows above); run in BATCH2 |
| MERGEALL1 (merge every built branch into main, clean merged worktrees (bungo 10-21 "merge everything")) | DONE 10-22: 17 landed at 9c624f47; its 3 holds have their own rows; was: DONE 10-22 01:5x: 17 landed (main 9c624f47, exe sha1 edd43625), 3 held, ~30 GB freed; gates: gn_vis PASS, cluster twin PASS, cluster build 0.4 ms; giblack FAIL (march reds fail to link fo4_cellcsm: too many instructions, LIGHTSYS2 suspect); real-cell cluster on/off/red inconclusive at concord daylight; concord close5 all-on 114.9 GPU ms (second window). notes/mergeall1/FINAL.md |
| DRIFT1 (find the BATCH2 lane that changes the default look (c3 0.37 %, mem 0.20 % px > 8 vs main); fix to byte-identical (10-22)) | DONE 10-22: cause STREAM1 swizzle, fix 9e77b49d in batch2; was: DONE 10-22: STREAM1 DdsView swizzles() always RGBA -> BGRA DDS load red/blue swapped; fix 9e77b49d on stream1, batch merge 1ce39dfa exe 087fda31 NOT shot. Open: FXRES chain 74k px within bar unattributed; bring dc0138f3 into batch; remove worktrees drift1f/drift1h |
| GATEFIX3 (BATCH2 lane gate defects: VRS1 red4 not caught, AAFORMAT1 1080 size + TAA pin, FXRES1 opaque depth refused (10-22)) | DONE 10-22: fixes in batch2; MSAA black -> MSAABLACK1; was: DONE 10-22: VRS1 check fixed; FXRES1 fix dc0138f3 exe ec0e84df PROVEN by shots (cause TONEMAP1; cherry-pick onto TONEMAP1); AAFORMAT1: MSAA1+TAA renders black = real defect, needs a code lane; Memory Den r11g11b10f pair -> bungo |
| GLASSLIGHT1 (measure first: lamp light bleeding/cost on pixels behind window glass; if real, skip/limit it the RDR2 way (stencil); ships OFF (bungo 10-22)) | CLOSED 10-22: measured, nothing to fix (glasslight1/FINDINGS.md); was: CLOSED 10-22: nothing to fix (notes/glasslight1/FINDINGS.md) |
| PBRALBEDO1 (fixpack1 (superseded by PBRALBEDO2, BATCH ready)) | SUPERSEDED by PBRALBEDO2 (batch4); was: bake reads .pbrm base color x (1 - metallic) | its own gate fails: 0/26 fixture materials resolved |
| VOLPART1 (volpart1-20261014 (superseded by VOLPART1R, BATCH ready)) | SUPERSEDED by VOLPART1R (batch2); was: steam/fog cards lit as volumes (ships off) | built on old VOLAIR1: 9 files conflict; needs a rebase lane |
| BATCH1 (one combined build + one gate session for PERF2, SSGI2, SHADOWPAR1, VOLAIRSKY1, FXCHECK1 (bungo 10-21; batch1/README.md); also runs RTBAKE1 pics + PBRALBEDO2 gates on their own exes, + Goodneighbor su) | DONE 10-22: gates run; FXCHECK1 + VOLAIRSKY1 landed, the rest carried by RESUME1 batches 2/4; was: GATES DONE 10-22 09:30 (batch1/RESULTS.md; branch not merged, not pushed): FXCHECK1 PASS (its G3 red = PERF2 AO opaque cut); PERF2 timing win c3 39->29 ms, mem 41->27 ms, but exact-cut G1a/G6/G7 FAIL; VOLAIRSKY1 T/L/M PASS, Z so + X FAIL, S known red; SHADOWPAR1 6 FAIL (mostly gate/scene) + perf +3.9 ms; SSGI2 FAIL (DC2 0.80, noise v2, timing); RTBAKE1 rays FAIL in-cell; PBRALBEDO2 R/R2 stale expe |
| MERGE1+BATCH2 (land FXCHECK1 (+ VOLAIRSKY1 if its looks ship off and diffs meet the bar), push; then BATCH2 build + gates (notes/batch2)) | PART 1 landed 4e90b6c0; the BATCH2 rerun is RESUME1's; was: PART 1 LANDED 10-22 13:4x (main 4e90b6c0, pushed). BATCH2 STOPPED 18:40 (batch2/RESULTS.md, not merged): TESTDRIVER1 gate FAIL (hour twins DIFF, a refused shader ends the live session, port_smoke argv bug + frame size change after reopen); batch default picture differs from main past the bar (lane unattributed); VRS1/AAFORMAT1/FXRES1 FAIL explained in RESULTS; rest not run. SSGI2FIX merge ready on |
| TDFIX1 (TESTDRIVER1 gate fixes: hour row live!=relaunch, refused shader must not end a live session, port_smoke arg order, reopen frame size, unknown switches = relaunch (10-22)) | folded into the TESTDRIVER1 row (its fixes = a4d8d45a, which failed its gate 10-23); was: PAUSED 10-22 21:00: all 5 coded+built a4d8d45a (testdriver1-20261022), exe 0736a38e; gate NOT rerun (75 min, machine alone; line in notes/testdriver1/BATCH.md). march refusal = real defect in march mode, likely LIGHTSYS2 2c69b160, not isolated |
- LIT1 (his call: cell view Far light covers it); testint..testint4 integration branches (test only)
