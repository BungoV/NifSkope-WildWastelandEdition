Two entries, for splicing into `MISTAKES.md` at the repo root (newest first).

---

## 2026-09-10 — a placeholder check sat where a measurement belonged (lane WATER3)

**What was done.** The first draft of `lodtWaterMarkSelfTest`
(`src/watermark.cpp`) wrote the body-table identity gate as

```cpp
check( "the body table re-encodes to the bytes the writer wrote",
    doc.strokes().isEmpty() );
```

because the accessor that would really compare the bytes did not exist yet.

**What was true instead.** That condition is true of any freshly opened
unmarked file. It cannot fail on its input, so it is not a check — and it was
the FIRST check in a harness whose every later case depends on the twin of the
writer's encoder being correct. A green line would have read as proof of the one
thing nothing was measuring.

**How it was found.** Re-reading the draft before the syntax pass, not by
running it. It was replaced with `WaterMarkDoc::tableRepackMatches()`, which
compares `encodeTable()` against the bytes read at open, and a second gate
`flowRepackMatches()` was added beside it.

**The rule that prevents it.** When a check has to be stubbed because its
accessor does not exist yet, WRITE THE ACCESSOR. A stub that returns a true-ish
expression is indistinguishable from a passing gate for as long as nobody reads
it. If it must be stubbed, stub it FALSE: a red line gets read.

---

## 2026-09-10 — a lane report written at the end, not incrementally (lane WATER3)

**What was done.** Lane WATER3 wrote its code, its scripts and its spec edits to
disk as it went, but kept `scratchpad/lane_water3_report.md` to the last step.

**What was true instead.** CONSTITUTION rule 1 says a lane writes each report
section as it finishes it, and rule 1b says a lane past half its window writes
its report and its PENDING resume FIRST. A window that dies with finished,
unexplained work has produced a pile of files nobody can grade.

**How it was found.** The finished-work review at the end of the lane, reading
its own charter back.

**The rule that prevents it.** The report's section 0 and its housekeeping
section are written when the FIRST deliverable lands, not when the last one
does; every later section appends.
