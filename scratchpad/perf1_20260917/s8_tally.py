#!/usr/bin/env python3
"""PERF1 section 8 -- read the neighbour logs back and count what they SAY.

The runner's own summary line counts grep hits, and a grep for FAIL matches the
word COMPARED and any prose that contains it. This reads each log's own tally
lines instead -- `N checks, M failures` and the `ok`/`FAIL` markers at the
start of a line -- and prints one row a gate a side.
"""
import os
import re
import sys

D = "E:/Projects/NifskopeWildWastelandEdition/scratchpad/perf1_20260917/logs/neighbours"
GATES = ["lod_generation", "lodgen_defaults", "lodgen_native", "lodgen_bakerec",
         "lodgen_layout", "lodgen_btofree", "lodgen_incremental",
         "lodgen_stage_times", "lodgen_identity"]

TALLY = re.compile(r"^\s*(\d+) checks?, (\d+) failures?")
MARK = re.compile(r"^\s*(ok|FAIL|RED|VACUOUS)\b")


def read(side, gate):
    p = os.path.join(D, "%s_%s.log" % (side, gate))
    if not os.path.exists(p):
        return None
    ok = fail = 0
    checks = fails = 0
    reds = []
    for ln in open(p, encoding="utf-8", errors="replace"):
        m = MARK.match(ln)
        if m:
            if m.group(1) == "ok":
                ok += 1
            else:
                fail += 1
                reds.append(ln.rstrip())
        t = TALLY.match(ln)
        if t:
            checks += int(t.group(1))
            fails += int(t.group(2))
    return dict(ok=ok, fail=fail, checks=checks, fails=fails, reds=reds)


for g in GATES:
    row = []
    for side in ("before", "after"):
        r = read(side, g)
        row.append("%s: %s" % (side, "MISSING" if r is None else
                               "ok=%d red=%d | tallies %d checks %d failures"
                               % (r["ok"], r["fail"], r["checks"], r["fails"])))
    print("%-20s %s || %s" % (g, row[0], row[1]))
    if "-v" in sys.argv:
        for side in ("before", "after"):
            r = read(side, g)
            for ln in (r or {}).get("reds", []):
                print("      %-6s %s" % (side, ln.strip()[:150]))
