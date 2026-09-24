# INCRGATE1 (LOD-E) -- text for the director to splice

Lane INCRGATE1, 2026-09-24, worktree `E:\Projects\NifskopeWWE-incrgate1`, branch `incrgate1-20260924`
from 47b2cad. Exe b2 sha1 `3b3808fd60c8e961b6a34e2aa54b702335e3faa4` (21:45). Rung
`release/NifSkope.before_incrgate1.exe` sha1 `b2a2b8b4f9ec913aa6b56814a15e5df477fc766d`.

## HANDOFF text

**2026-09-24 22:05 -- lane INCRGATE1 (LOD-E): INCR1's ledger now reaches the panel, a moved default refuses, plan 5 rows 6/13/25/26 closed with evidence.** Not merged. Branch `incrgate1-20260924`, commits listed in the lane's DONE.md.

- **The ledger code moved.** It went from `src/nifcli.cpp` to `src/lodgenchunkpass.{h,cpp}`, with no change in behaviour. The command line and the panel now run one implementation. `lodgen_incremental.sh`: 11/0 on the rung and 12 ok / 0 failures on b2.
- **The identity word.** The record's `switches` is now sha1(argv digest, identity word). The word is `gen1:<sha1>` over every EFFECTIVE setting (112 on a bare Sanctuary bake), plus a manual generator revision `kLodgenGeneratorRevision` = 1.
  - **Why:** before this, a default that moved inside the exe left `switches` unchanged, and `--incremental` kept stale chunks. On the rung, a record baked by the before_defaults2 exe was accepted with "0 of 1 chunks dirty".
  - **Gate G1** `tests/spells/lodgen_incr_identity.py`: 11/0 on this exe; 11 checks / 6 failures on the rung (the red run).
  - **One-time cost:** every record written before today refuses once, as "the switches differ", and asks for one full bake.
- **The panel row "Rebake only what changed".** It sits in the LOD Generation panel's Run section and is OFF by default.
  - **Row OFF:** byte-identical to the rung: 10 files, 45,582,390 B.
  - **Row ON:** the same bake plus the bake record and the `.lodj` caches. The record carries `switch --panel` and the identity word. When there is no record yet, or the record cannot vouch for the run, the row bakes the whole range and (re)writes the record, and the census says why. It never refuses.
  - **Gate G2** `tests/spells/lodgen_panel_incremental.sh`: PASS. Red control: the rung's tree read as the ON tree fails.
  - **Heads-up:** the panel's texture arrays are ON by default and are built from the whole range. With the default settings every run is therefore a full bake. The row only saves time with arrays, the atlas and cards unticked.
- **Plan section 5 rows** (evidence marked in `docs/FO4CS_IMPROVED_LOD_PLAN.md`):
  - **Row 6:** `lodgen_native.sh` leg 13b. The decoder reads the downtown-Boston pair (33,123 placements, 280 boxes) with 14 instances inside the cell-line band, worst 0.0625 u of 0.127. Red control: with the band at 0 it refuses at instance 3358. Note: the decoder's band is one float ulp narrower than the C++ reader's.
  - **Row 13:** `lodgen_native_baseline.sh --drop-proof`. The stock (-32,0) dim-32 chunk drops 2,628 of 42,560 placements (6.17 %). The native pair keeps all 42,560, and `--native-verify` finds 0 instances with neither geometry nor a card.
  - **Row 25:** the census checker was re-run on a v4 pair: 38 ok / 0 RED / 32 not-derivable. Its first run had 1 RED, and that was a checker defect, fixed (see MISTAKES text).
  - **Row 26:** `tests/spells/lodgen_sanctuary_pair.sh` makes a default-settings Sanctuary pair: `.lodo` v4 6,204,388 B, `.lodi` v7 527,989 B, 3,526 placements in 10 chunks. It is written to a scratch folder and never committed.
- **Rulings owed to bungo** (INCR1's behaviour is kept unchanged on each):
  1. Should `.lodj` caches be written by default? Today every `--native` bake writes them, and `--no-native-cache` is the way back.
  2. `--native` together with `--incremental`: today it is allowed, and the pair is rebuilt from rebuilt plus replayed chunks.
  - `.lodo` reuse under the panel row waits for CARDLINK1 and was not touched.
- **Owed, not in this lane's files:**
  - (a) `g_ledgerAssetDigest` in `src/lodgen.cpp` is a process-wide cache that is never cleared. If a model or texture is edited while the panel stays open, the next run's diff does not see it until a restart. The row's tooltip says so; the fix is one clear at the start of a run.
  - (b) A second panel run over the first run's record (the 0-dirty replay) has no gate. The `WW_LODGEN_RUN` harness in `src/nifskope_ui.cpp` wipes its folder at every launch.
- **The checked-in stock baseline is stale** (`tests/baselines/stock_baseline.sha256`, from exe 2026-09-10). `lodgen_native_baseline.sh --check` is red on the RUNG and on b2 with the same 6 files. Those are the dim 4/8/16 chunks and the region `.BTO`, which moved when the defaults moved on 09-12. Against a baseline written from the rung, b2 is 25 of 25 byte-identical. Re-writing the checked-in file is the director's call.
- **What the final bake needs:** nothing new. The row ships OFF. The first `--incremental` on any old record is one full bake.

## WW_CHANGES text

### 2026-09-24 -- lane INCRGATE1: "Rebake only what changed" in the LOD panel; the incremental ledger sees moved defaults

- **LOD Generation panel, Run section: new row "Rebake only what changed"**, OFF by default. When ticked, only the chunks whose inputs changed since the last bake are rebuilt. The first run, and any run the last bake cannot vouch for, rebuilds everything and says why on the result line. The command-line equivalent is `--incremental`.
- **`--incremental` now notices a changed default.** The bake record's switch digest now includes a hash of every setting the bake actually used, typed or defaulted. The command line prints it as `identity: gen1:<hash>, N setting(s)`. **Records written before this change are refused once**, so the first `--incremental` after updating asks for one full bake.
- Internal: the incremental ledger code moved from `nifcli.cpp` to `lodgenchunkpass.cpp`, so the panel and the command line share one implementation.
- New gates:
  - `lodgen_incr_identity.py` (a moved default refuses)
  - `lodgen_panel_incremental.sh` (row OFF is byte-identical, row ON adds only the record and caches)
  - `lodgen_native_baseline.sh --drop-proof` (the stock far chunk drops 6.17 % of placements, the native pair none)
  - `lodgen_sanctuary_pair.sh` (a default-settings Sanctuary `.lodo`/`.lodi` pair, and the census checker on it)
- Fixed: the census checker (`tests/spells/lodgen_census_check.py`) reported a false RED on `ladderGroup` for every pair baked with the ladder OFF (the default).

## MISTAKES text

### 2026-09-24 -- a checker written against a default that later moved (lane INCRGATE1)

`tests/spells/lodgen_census_check.py` was written on 2026-09-11 against pairs baked with the cluster ladder ON. It compared the `native-ladder:` line's `(group 4, ...)` with the `.lodo` header's `ladderGroup`. When the authored-only library became the default, the ladder went OFF. The census line kept printing `group 4`, which is the target of a ladder that was not built. The file correctly writes 0 ("0 iff the LADDER flag is clear", NATIVE 3 0xCD). No one re-ran the checker on a v4 pair, so the false RED stayed hidden until plan 5 row 25 asked for that run. **Rule:** when a default moves, re-run every checker that parses the census line on a pair baked with the NEW default. This is the harness skill's "sweep for stale premises", applied to Python checkers. Fixed: with the ladder OFF the checker claims 0 for the file, and the printed target becomes a not-derivable word. A red leg shows that a doctored 4 is still caught.

### 2026-09-24 -- the incremental switch digest could not see a moved default (lane INCRGATE1; INCR1's open finding, closed)

INCR1's `switches` hashed the typed argument vector only. When DEFAULTS1 moved seven defaults, a record from the old exe still matched, and `--incremental` kept chunks baked under the old defaults. The rung accepted a before_defaults2 record with "0 of 1 chunks dirty". **Rule:** a digest that decides whether bytes may be reused must hash the EFFECTIVE settings, not what was typed. It also needs a manual revision number for changes no setting names. Fixed by the identity word, gate G1.

## Skill review

- **nifskope-ww-worktree-build:** followed as written. The main tree was clean, so objects were copied; the first build was the rung. No change.
- **nifskope-ww-panel-style:** the new row follows it: one row, a whole-word label, the explanation in the tooltip, and the run reads it through `xb()`. `lod_generation.sh` counts 40 check boxes (was 39), 0 with a dash, 0 without a tooltip. **Add:** a standing row that wraps a command-line REFUSAL must not refuse. A row stays ticked, and a refusing row has no way back except unticking it, and an unticked row writes no record. So promote the refusal to a full bake and say why. (Worth a section, since the next row that wraps a flag will meet it.)
- **ww-test-harness-add:** G2 reuses the `WW_LODGEN_RUN` harness through an env hook (`WW_LODGEN_INCREMENTAL=0|1`), not a new harness; the skill has no line for "force a row from the environment to compare two runs of one harness". **Add** it under section 6 (a harness forces the state it measures).
- **nifskope-ww-lodgen:** **add** to the gates list: `lodgen_native_baseline.sh --drop-proof` (row 13), `lodgen_sanctuary_pair.sh` (a default Sanctuary pair plus the census checker), and the fact that the ladder is OFF by default, so a census checker must not compare `group N`.
- **search-lean:** followed. No change.
