## LANE LAND1 — TILING5 (Part A) + INCR1 (Part B), 2026-09-12

Splice into `HANDOFF.md` under the running ledger. Two independent features in
one build; both ship behind a switch and both defaults are yesterday's bake,
proved byte for byte.

### Tree
- `src/lodgen.cpp`, `src/lodgen.h`, `src/nifcli.cpp` — the land-guide sampler
  (Part A) and the `.lodb` ledger + `--incremental` (Part B). No UI file
  touched; `E:/Projects/NifskopeWWE_ui` not entered.
- `docs/LODGEN_LEDGER_FORMAT.md` — NEW, the ledger contract, the dependency
  map, the switch-digest skip list and the refusal table.
- `docs/LODGEN_TERRAIN_VT.md` — `--land-guide`, `--land-guide-scale`,
  `--land-guide-slope` and `--incremental` in the §5 flag table; the LAND1
  section under §8.
- `tests/spells/lodgen_roads.sh` — the inherited R5 red cleared (MARG 0.8 →
  0.72, derivation recorded in the file). Suite reads 11 checks / 0 failures.
- `tests/spells/lodgen_native_baseline.sh` — `*.LODB` added to the excluded set
  beside `*.BTR`, with the reasoning in the header. The frozen baseline was NOT
  re-written: it is a list from ONE NAMED BUILD of 2026-09-10 and re-freezing it
  from a mid-lane exe would bless every other lane's drift since.
- NOTHING committed. `git stash` never run.

### Exe on disk
- `release/NifSkope.exe` — 2026-09-12 **09:32:37, 21,951,488 B,
  sha1 `3e1914a0637b66f438d873e0230b1e8c04d7c806`**, carrying both parts and the
  switch-digest fix. `GeneratedFiles/.obj/nifcli.o` 09:20:58 and `lodgen.o`
  08:32:06 — the object timestamps are the check, because `exe -nt src` is
  satisfied by a link the *other* file triggered.
- Two earlier exes are named in the report and in the `docs/LODGEN_TERRAIN_VT.md`
  provenance block: 08:42:33 (`1e4e2c5c…`), which is the exe gates B2–B5 and
  the picture were measured on, and 09:21:04 (`0e5b65d6…`), the switch-digest
  fix. **No lodgen code differs between 09:21:04 and the shipping exe**: the
  last build exists only because six files from another lane's merge arrived
  with their mtimes preserved (08:26–08:46), so the exe-newer-than-sources gate
  passed over stale objects — `animdopesheet.o` was 04:34:58 against a
  `.cpp` of 08:32:23. **An exe newer than a source file is not an exe built from
  it**; `make -n` is the two-second check that says so.
- `scratchpad/land1_20260912/NifSkope.partA_prerelink.exe` — the Part A-only
  exe (07:42:22, 21,861,376 B, sha1 `902223bd99dba4bfaf5d621fe36eb12fc4cf0272`),
  kept as a rollback rung and as the exe Part A's numbers were taken on.
- Fallout4.exe was down for every build and every launch, checked separately
  from NifSkope.exe each time. bungo's open window needs a RESTART.

### What is switched on by default
Nothing. `--land-guide` defaults to `off` and was proved byte-identical to the
previous bake on all fourteen frozen sheets, every file of every tile.
`--incremental` is opt-in; what *is* unconditional is the ledger write — every
region bake now spends one input digest per chunk so that tomorrow's
`--incremental` has something to diff against. That cost is measured in the
report's B5 section, not asserted.

### The two findings a reader should not miss
1. **Part A's winner is a passenger on the hex lattice, not the effect the
   design expected.** `--land-hex 256` alone passes the repeat law on 4 sheets
   of 7; the winning rule on top of it passes 5 of 7. The honest size of the
   win is one sheet and 0.054 of worst-sheet repeat, and it is paid for in the
   band-error gate (7 of 7 → 5 of 7). No rule is recommended; the default stays
   `off`. Where it clearly earns its keep is steep ground.
2. **Part B's refusal list, written from the dependency map before the code,
   refused the default command.** The merge is on by default and the map named
   it a whole-region pass; reading the function proved it is a per-file loop
   with no cross-file state. The feature was 100 % unusable and the refusal
   looked like caution. Both this and a ledger-timing defect were found by the
   byte-identity gate, not by reading — see `MISTAKES_ENTRIES.md`.
3. **A ledger in every out-dir changes the contract of every byte-identity gate
   in the tree.** `lodgen_roads` R1 and `lodgen_native` check 5 both went red on
   the ledger's `switches` field, because the digest was eating a DESTINATION
   PATH (`--vt <dir>`) and a flag that cannot make a chunk stale (`--native`).
   The digest has two skip lists now — token+value, and token-kept/value-dropped
   — and both harnesses are green. Fixing it found a live defect the gates had
   not: `--incremental --native` would have written a `.lodo`/`.lodi` pair
   covering only the dirty chunks, and unlike a quarter-sized atlas **that pair
   loads, verifies and matches its own staleness hashes**. `--native` is now
   refused with `--atlas`, `--arrays` and `--impostors`.

### Reds and what was not measured
- Part A: the band-error gate loses two sheets of seven to the winner. This is
  a real cost, reported rather than hidden, and it is the reason the default is
  `off`.
- Part B: the refusals are gated on two regions of a 5×5-chunk shape in
  Commonwealth. A whole-worldspace incremental run has not been made and must
  not be inferred from these — the brief's region-bakes-only rule stands.
- The VT pyramid path (`--vt`): its TOKEN is in the switch digest and its path
  is not, and `lodgen_roads.sh` R1 now exercises exactly that (two `--no-roads`
  bakes into different `--vt` directories, byte-identical). What is still not
  baked is an `--incremental` arm that toggles `--vt` itself.
- `--native` is refused under `--incremental` by reading the collection sites,
  not by a gate arm: no arm asserts the refusal fires. It is the cheapest arm
  the next lane could add, after the `assets` one.

### Documents
`scratchpad/land1_20260912/` — `WW_CHANGES_ENTRY.md` (one entry, a subsection
per part), `MISTAKES_ENTRIES.md` (nine entries), `CHANGED_FILES.txt`, this
block. Report: `scratchpad/lane_land1_report.md`. Pictures:
`scratchpad/land1_20260912/images/`, `a_*.png` and `b_*.png`, every one baked
with `--road-detail 1`.
