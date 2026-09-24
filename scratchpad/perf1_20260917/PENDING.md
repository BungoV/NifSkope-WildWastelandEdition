# PENDING -- lane PERF1 -- RESOLVED 2026-09-17 15:4x

**Nothing is pending.** Both neighbour sides are complete, the report has
all thirteen sections, and `DONE` is written beside this file. What follows
is the record as it stood while the lane was halted, kept because section
8.3 of the report points at it.


State at the moment of writing, so that a resumed session does not have to
re-measure anything. Everything below is on disk.

## Done and proven

- Steps 1-6 of the brief: measured first; the object pass parallel (both
  fan-outs, retired in job order); the texture statement; the chunk soak on
  BOTH regions; library reuse under `--incremental`; the stage-time split.
- Report sections 1-7.6 are written into
  `scratchpad/perf1_20260917/lane_perf1_report.md`, incrementally.
- MISTAKES.md (repo root) carries PERF1 entries 1-7.
- `lodgen --help` corrected for `--threads` and `--chunk-threads` (the 5.80 GB
  figure was another lane's; this lane measures 9.32 / 10.48 GB).

## The exe

- Rung (never delete): `release/NifSkope.before_perf1.exe`, 22,534,144 bytes,
  2026-09-17 08:55.
- Final: `release/NifSkope.exe`, 22,567,424 bytes, 2026-09-17 12:45:27,
  sha1 `a843fca68c18c2740efddcb20e9fe715b7732a22`. This is the exe every gate
  result from here on describes. It differs from the 12:02 binary ONLY in the
  two `--help` blocks.
- `scratchpad/perf1_20260917/NifSkope.final_backup.exe` is the PREVIOUS
  (12:02) binary. The link is not bit-reproducible, so never rebuild to "get
  back" a binary -- copy it.

## HALTED TWICE -- 2026-09-17 13:59 (pid 44844) and 14:57 (pid 26328)

The neighbour runner checks `tasklist` before EVERY gate, so it stopped itself
between gates both times: `STOPPED: Fallout4.exe came up before lodgen_native`
in `logs/s7_after_run.log`. No bake of this lane was running when the game came
up either time, and none was started after. Nothing of this lane may build or
launch an exe until the game is gone.

Both halts fell at the same place, because the "after" side was being re-run
from the top and `lodgen_defaults` alone costs 14-17 minutes -- twice spent,
twice for a result already known (28 checks, 0 failures, both sides). So the
runner now takes a GATE LIST and appends:

```
bash scratchpad/perf1_20260917/s7_neighbours.sh after \
     lodgen_identity lodgen_stage_times lodgen_btofree lodgen_native \
     lodgen_incremental lodgen_bakerec lodgen_layout
```

That is the seven that have never run on the new exe, **cheapest first** by the
"before" side's own timings (2 s, 71 s, 191 s, 220 s, 323 s, 475 s, 854 s), so
that the next interruption costs the least and each gate that DID finish is
kept. Every gate is still run whole, one at a time, on one exe; the report says
plainly that the side was assembled across sittings.

| the nine, "after" side | state |
|---|---|
| `lod_generation` | DONE, PASS (128 checks, 0 failures) -- run twice, same both times |
| `lodgen_defaults` | DONE, PASS (28 checks, 0 failures) -- run twice, same both times |
| `lodgen_native` | not run |
| `lodgen_bakerec` | not run |
| `lodgen_layout` | not run |
| `lodgen_btofree` | not run |
| `lodgen_incremental` | not run |
| `lodgen_stage_times` | not run |
| `lodgen_identity` | not run |

The "before" side is COMPLETE (all nine, `s7_before.txt`).

## Still owed when this was written

1. `tests/spells/lodgen_perf.sh` re-run on the 12:45 exe --
   `logs/gate_perf_final3.log`; then report 7.2 updated with the new
   mtime/size/sha1 and the new counts.
2. The nine neighbour gates, before and after, one at a time:
   `bash scratchpad/perf1_20260917/s7_neighbours.sh before` then `... after`.
   Summaries land in `s7_before.txt` / `s7_after.txt`, one log each under
   `logs/neighbours/`. Known carried reds to name, not to fix:
   `lodgen_native.sh` section 5 (2 FAIL, BTOFREE1's stock `.BTO` widened by
   `.lodj`), `lodgen_btofree.sh` 21/3 legs (a)(b) (`.lodj` present on the new
   side only -- SAY whether the rung `before_perf1` already shows them, since
   the rung is INCR1's exe) and leg (c) (BAKEREC1's v1-vs-v2 record).
3. Report section 8 (the neighbour table), then splice sections 9-13 from the
   session scratchpad files `sec910.md`, `sec11.md`, `sec12.md`, `sec13.md`
   (in `C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/
   31afe9b6-84f6-433a-b116-38fc09f87de9/scratchpad/`). All four are CORRECTED
   for the 5.80 GB misquote already; splice them as they stand.
4. `scratchpad/perf1_20260917/DONE` (first word `perf`).

## Rules that bit this lane

- Game check (`tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?`) is its
  OWN command before every build and every exe launch.
- Never commit, never `git stash`, never edit `WW_CHANGES.md` or `HANDOFF.md`.
- Scripts containing a backslash go through the Write tool, never a heredoc.
- A waiter that greps `^lodgen_perf: ` matches the gate's HEADER line; wait on
  `^lodgen_perf: (PASS|[0-9]+ FAIL)`.
