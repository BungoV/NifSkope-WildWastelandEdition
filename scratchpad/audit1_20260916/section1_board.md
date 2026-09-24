## 1. Every lodgen gate on the final exe

Every `tests/spells/lodgen_*.sh` in the tree was run once against
`release/NifSkope.exe` of 2026-09-17 12:45:27 (22,567,424 B, sha1
`a843fca68c18c2740efddcb20e9fe715b7732a22`), before any edit of mine to the
product. 27 gates, 5,369 s of exe time in total. Logs:
`scratchpad/audit1_20260916/gates1/*.log`, wall clocks from the runner's own
`DONE <name> rc=<n> secs=<n>` lines (`gates1_runner.log`); the five run one at
a time afterwards carry their own timings.

### 1.1 The board

Counts are taken from each gate's own summary line where it prints one, and
otherwise by counting its `ok` and `FAIL` lines; where the two disagree the
column says so rather than averaging them. `WHOSE` is the column the brief
asked for: who owns the red.

| gate | rc | s | checks / fails | ok lines | FAIL lines | WHOSE |
|---|---|---|---|---|---|---|
| `bakerec` | 0 | 243 | (no summary line) | 69 | 0 | -- |
| `btofree` | 1 | 123 | 21 / 3 | 18 | 4 | **STALE GATE**, standing red 2 of the addendum; adjudicated 1.3 |
| `byte_gate` | 1 | 1969 | (see note) | -- | 3 | **KNOWN**, named by two LANDED lines; 1.4 |
| `card_arrays` | 0 | 5 | (no summary line) | 35 | 0 | -- |
| `defaults` | 0 | 757 | 28 / 0 | 28 | 0 | -- |
| `farring` | 0 | 34 | (no summary line) | 21 | 0 | -- |
| `ground_cover` | 1 | 25 | 29 / 4 (inner block 21 / 3) | 43 | 7 | **KNOWN**, pre-existing grass-feature red; 1.4 |
| `identity` | 0 | 2 | (no summary line) | 8 | 0 | -- |
| `impostor_cards` | 0 | 3 | (no summary line) | 12 | 0 | -- |
| `incremental` | 0 | 174 | (no summary line) | 10 | 0 | -- |
| `ladder` | 0 | 125 | 32 / 0 | 40 | 0 | -- |
| `layout` | 0 | 865 | (no summary line) | 23 | 0 | -- |
| `merge` | 1 | 20 | (no summary line) | 9 | 1 | **STALE GATE**, unattributed until this lane; adjudicated 1.2, FIXED |
| `native` | 1 | 170 | 309 / 1 over 7 blocks | 123 | 2 | **STALE GATE**, standing red 1 of the addendum; adjudicated 1.3 |
| `native_baseline` | 0 | 11 | (no summary line) | 3 | 0 | -- |
| `octahedral` | 1 | 69 | (no summary line) | 108 | 3 | 2 **STALE GATE** (LAYOUT1 moved the card path), 1 **KNOWN** (CARDWIDTH, 1.74 texels); 1.3 / 1.4 |
| `panel_run` | 0 | 45 | 137 / 0 | 137 | 0 | -- |
| `perf` | 0 | 549 | (no summary line) | 11 | 0 | -- |
| `resources` | 0 | 3 | 4 / 0 | 4 | 0 | -- |
| `roads` | 1 | 21 | 11 / 1 | 10 | 1 | **STALE GATE**, unattributed until this lane; adjudicated 1.2, FIXED |
| `stage_times` | 1 | 34 | 16 / 1 | 15 | 2 | **STALE GATE**, standing red 3 of the addendum; adjudicated 1.3 |
| `terrain` | 0 | 13 | 26 / 0 | 26 | 0 | -- |
| `terrain_pbrm` | 0 | 34 | 14 / 0 | 14 | 0 | -- |
| `terrain_vt` | 0 | 56 | 58 / 0 | 112 | 0 | -- |
| `texture_arrays` | 0 | 7 | (no summary line) | 40 | 0 | -- |
| `tree_sway` | 0 | 8 | 4 / 0 | 4 | 0 | -- |
| `water_subdiv` | 0 | 4 | 7 / 0 | 7 | 0 | -- |

Nineteen gates PASS. Eight are red, and not one of the eight is a red of the
product introduced by this campaign: three are the standing reds the addendum
named, two are stale gates nobody had attributed, and three are pre-registered
known reds. The adjudications are below, each with the evidence that decides
gate against product.

Two bookkeeping notes on the table itself, so the numbers are not read as more
than they are. (1) `byte_gate` prints its own `137 checks, 0 failures` line --
that is the PANEL self-test it runs inside itself, not its own count; its
three failures are phase lines with a different prefix, which is why the
generic ok-line counter reads 0 for it. Its failures are listed by name in
1.4. (2) `stage_times` prints `16 checks, 1 failures` while carrying 15 `ok`
and 2 `FAIL` lines: the second FAIL is the suite's own trailing `FAIL` verdict
word, not a check. Both discrepancies are in the gates' output, not in the
counting.
