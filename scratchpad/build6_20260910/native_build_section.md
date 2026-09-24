
## Build (BUILD6)

Hook-up applied, built and gated 2026-09-10 03:56-04:02 by lane BUILD6; nothing
committed. Logs under `scratchpad/build6_20260910/logs/`, summaries
`build2_summary.txt`, `native_summary.txt`, `chain3_summary.txt`.

**The hook-up.** `scratchpad/build6_20260910/apply_hookup.py` takes the note's
own fenced blocks by index and applies A1-A4, the two `lodgen.h` lines, and
B1-B6 exactly; every anchor counted 1 of 1 (the A2 anchor was taken as the
`rung 3` banner itself, the note's "`} // namespace` + one blank line" text has
two blank lines in the file). Byte counts, 0 CR before and after: `lodgen.cpp`
388,314 -> 391,673 (LF 8,850 -> 8,924, +74), `nifcli.cpp` 261,671 -> 263,848
(LF 5,990 -> 6,030, +40), `lodgen.h` 30,436 -> 30,578 (+3 LF). Nothing outside
the note was changed. `qmake NifSkope.pro` rc 0, `make -j2` rc 0 (game check
rc=1), 03:56:58-03:57:48; recompiled `nifcli.o`, `lodgen.o`, `nativeemit.o`
(+ `main.o`, `nifskope_ui.o`, `lodgenmanager.o` from the regenerated Makefile);
`lodofile.o` / `lodifile.o` were already 03:38 (BUILD5b's qmake had the `.pro`
lines). No new warning in the touched lines (the four `lodgen.cpp` warnings are
the pre-existing ones). `Makefile.Release` read back by owning object:
`nativeemit.h` under `nifcli.o` (line 3933), `lodgen.o` (4231),
`nativeemit.o` (4299); `lodifile.h` under `nifcli.o` (3935), `lodofile.o`,
`lodifile.o`, `nativeemit.o`. Exe **03:57:46**, 18,526,720 B, sha256
`664e0de4483792b5...`; newer than every changed file under `src/ res/ tools/
tests/ NifSkope.pro`; no object older than a header it includes.

**Two corrections to the note, found by running it.** (1) `lodgen
--native-fixture <dir>` and `lodgen --native-verify a b` are refused by the
CLI with `error: 'lodgen' needs a <file>` (`nifcli.cpp:5613`); both work with
the ESM as the positional, which the early return never opens. (2) The
decoder's manifest leg holds X/Y to 0.125 u, but the manifest prints
`%g`-style 6 significant digits, so a coordinate >= 10,000 u carries a print
step of 0.1 -- see the gate table.

| gate (HOOKUP §C) | reading | verdict |
|---|---|---|
| C1 fixture through the exe | `Synthetic.lodo` 28,903 B, `.lodi` 16,408 B, `.expect.txt` 1,901 B; **byte-identical to the standalone tool's** (`cmp` both); decoder `--expect` **46 checks, 0 failures, RESULT PASS**; `--native-verify` accepted, `pair identity ok` | PASS |
| C2 the (0,0) 4x4-cell region, dim 4, `--no-ao`, `--native` | `Commonwealth.lodo` **5,696,484 B** (bases 2,970 of 2,974 in the census, 4 without a loadable model = the four `LOD\Architecture\Warehouse\WrhsLeanTo*_LOD.nif`; 2,982 meshes, 10,634 clusters, 142,138 triangles, 273,969 vertices, 136 materials); `Commonwealth.lodi` **136,992 B** (3,812 instances from 3,812 arrivals over 701,131 census refs, 0 dropped, 3,812 unlit, 1 chunk, max scale 1.7334); census line `1530.3 B a placement`; beside it `Commonwealth.4.0.0.BTO` 2,058,014 B + manifest 305,056 B | written |
| the ring set, same region, dim 8 / 16 / 32 (`--no-ao`) | `.lodo` 5,696,484 B at every dim (the base library is worldspace-wide); `.lodi` 279,624 B (8,329 instances, 4 chunks, `717.5 B a placement`) / 33,064 B (549, 13 of 16 chunks, `10436.3`) / 16,392 B (1 instance, `5712876.0`); the `.BTO` beside each 4,762,238 / 606,382 / 5,078 B | written |
| per-placement bytes vs the audit's 703.3 | the emitter's figure is (`.lodo` + `.lodi`) / instances of THAT bake, so it is the fixed 5.7 MB library divided by however many instances the region had: 1,530.3 at dim 4, **717.5 at dim 8**, 10,436 at dim 16. Over the four bakes together, 6,162,556 B / 12,691 instances = 485.6 B. The `.lodi` alone is 35.9 B an instance at dim 4 (24 B hot + cold + chunk headers). The audit's 703.3 is not reproduced by any one of these and the audit's own denominator is not recorded here; the number is reported, not matched | measured |
| C3 `--native-verify` on all four pairs | accepted, `pair identity ok`, every check on (logs `c3_verify_d*.log`) | PASS |
| C3 decoder, ESM leg (`--esm`, `--chunk 0 0 <dim>`) | all four dims: every present ref has its base form; **X/Y within 0.125 u (worst 0.1254)**; **rotation within 0.02 deg incl. the tree yaw (worst 0.0048)**; dim 4: 47,260 plain refs in the ESM, 3,690 in the table | PASS |
| C3 decoder, manifest leg | every row in the table (3,812 / 8,329 / 549 / 1; 0 not in the table), base forms match, scale within 1/16384 on all four; **X/Y within 0.125 u FAILS at dim 4 / 8 / 16**: 288 / 1,118 / 92 rows, worst 0.1711 / 0.1740 / 0.1686; dim 32 passes (**12 checks, 1 failure** x3; 12/0 at dim 32) | **FAIL as pinned; cause measured below** |
| the manifest miss, measured (`manifest_precision.py`) | 296 / 1,145 / 92 coordinates exceed 0.125; **275 / 1,109 / 91 of them are coordinates >= 10,000 u, where the manifest's 6-significant-digit print has a step of 0.1**; with half that step budgeted on top of the 0.125 quantisation bound, **0 coordinates exceed at every dim** (worst residual 0.0000). The writer's positions are inside the `.lodi` quantisation bound (the ESM leg says so at 0.1254); the decoder's manifest bar is tighter than the manifest's own print. NOT re-pinned by this lane -- the director decides whether the bar budgets the print or the manifest prints more digits | verdict |
| C4 / LANE 0 `lodgen_native_baseline.sh --write` | **RESULT PASS**, `tests/baselines/stock_baseline.sha256` written (31 lines: 6 header + **25 files**: 4 chunks, their manifests, the region's `.BTO`s, `Objects/` arrays + atlas), header `exe 664e0de4... 2026-09-10T03:57:46`, `git v0.3.3-68-g720762a-dirty`, `profile ao=1 identity=1 arrays=1 atlas=1 slot-fallback=dim16 exclude=BTR`, **`bake-seconds 8`**; `PENDING.txt` removed | PASS |
| `--selftest` | a one-digit flip names exactly `chunks/Commonwealth.16.-32.16.bto`, 1 differ; the baseline against itself 0 differ; **0 failures, RESULT PASS** | PASS |
| `--check` (once) | **25 files in the baseline, 25 baked, 0 differ**; profile matches; same exe sha both sides; wall 8 s | PASS |
| C5 the (-32,0) dim-32 `--slot-fallback --native` asymmetric-drop proof | **not run** -- not in BUILD6's brief; the baseline bakes that chunk stock (without `--native`) and hashes it | skipped, named |

**The wall time, stated exactly.** The lane-0 harness bakes a REGION SET, not
a worldspace: chunks (-20,24) d4, (-24,24) d8, (-32,16) d16 `--slot-fallback`,
(-32,0) d32 (the bucket-cap chunk, 42,560 placements), and the 2x2-cell region
(-20,24)..(-19,25) at d4 with `--arrays --atlas`, AO on, identity on. That set
took **8 s wall** on this exe (`--write` 04:00:07-04:00:15; `--check` the same
8 s). It is the first measured stock-bake number for this tree and it goes into
`scratchpad/handoff_fo4cs/README.md` as what it is. **A full Commonwealth bake
was NOT timed by this lane** -- the brief read the harness as a whole-worldspace
bake and it is not one; bungo's GUI bake will be the first full-worldspace
number, and no figure is invented for it here.

**CLAMP2's gates 1 and 2 re-run on this exe** (the terrain code is the same
`lodgen.cpp` bytes, relinked): `ringcontrol.sh` 13/0 PASS; `lodgen_terrain_vt.sh`
35 checks, 1 failure -- the same V9b `msn differs: Commonwealth.4.-20.28` (see
`lane_clamp2_report.md` "## Build (BUILD6)"); the hook-up moved nothing there.

Mtimes in one table: `lodgen.cpp` / `nifcli.cpp` / `lodgen.h` 03:56:5x (the
patch), `Makefile.Release` 03:56:5x, objects 03:57:15-03:57:44, exe 03:57:46,
native bakes 03:59:20-04:00:07, baseline 04:00:07-04:00:24, chain3 04:01:31-
04:02:13. Owed after this lane: the manifest-leg bar (director's call); C5; the
writer-mutation half of lane 0 (flip a stock vertex-writer constant, rebuild,
`--check` must name the file -- needs a build the director schedules); the
`(cell, ref, part)` two-instance fixture; a consumer in FO4CS; and
`scratchpad/handoff_fo4cs/README.md`'s note on the audit's 703.3 denominator.
