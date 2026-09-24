# src/nifskope_ui.cpp moved AGAIN at 05:35:59, under a concurrent lane, after the
# first pass had written. Record that on the two pages that cite it, in the
# report, and in the changelog entry -- it is the exact failure the skill's step 3
# exists for, caught by re-running the pass.
import os, sys
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")

EDITS = {
"docs/LODGEN_CARD_SHEETS.md": [
 ("24 of 36 rows moved, 12 were already right, 0 anchors missing or ambiguous.",
  "24 of 36 rows moved, 12 were already right, 0 anchors missing or ambiguous.\n"
  "Then `src/nifskope_ui.cpp` moved AGAIN under a concurrent lane at 05:35:59\n"
  "(+87 lines) and 17 of those rows were re-derived a second time; the stamp above\n"
  "is the state after that, verified by re-running the pass to `0 moved`. **That\n"
  "file is under live edit: check its sha256 before trusting a number here.**"),
],
"docs/LODGEN_LODM_FORMAT.md": [
 ("22 of 34 rows moved, 12 were already right; the coverage-sidecar row's anchor\n"
  "had a literal newline pasted into it, which had broken that markdown row in two.",
  "22 of 34 rows moved, 12 were already right; the coverage-sidecar row's anchor\n"
  "had a literal newline pasted into it, which had broken that markdown row in two.\n"
  "Then `src/nifskope_ui.cpp` moved AGAIN under a concurrent lane at 05:35:59\n"
  "(+87 lines) and 5 of those rows were re-derived a second time; the stamp above\n"
  "is the state after that. **That file is under live edit: check its sha256\n"
  "before trusting a number here.**"),
],
"scratchpad/lane_docs2_report.md": [
 ("| **total** | **248** | **111** | **132** | **0** | **0** | **5** |",
  "| **total** | **248** | **111** | **132** | **0** | **0** | **5** |\n\n"
  "**`src/nifskope_ui.cpp` moved a SECOND time while this lane wrote.** At\n"
  "05:35:59 a concurrent lane took it from `f5d1e3bf9fab7a27` (31,409 lines) to\n"
  "`e820666628d97a51` (1,476,325 B, 31,496 lines), after the first pass had already\n"
  "written. Re-running the pass caught it and re-derived **17 more rows on\n"
  "CARD_SHEETS and 5 on LODM_FORMAT**; both stamps were rewritten to the new state\n"
  "and step 5's end-to-end hash check passes against it. Both pages now carry a\n"
  "line saying that file is under live edit. This is exactly what the skill's\n"
  "\"re-derive IMMEDIATELY before finishing, not while writing\" is for, and the\n"
  "cheap insurance was running the pass one more time at the end."),
],
"WW_CHANGES.md": [
 ("**243 rows checked: 111 moved, 132 were already right, 0 anchors missing, 0\n"
  "ambiguous, 5 multi-site rows re-derived by hand.** A second run of the same\n"
  "script over the written pages reports 0 moved, which is the idempotence check.",
  "**243 rows checked: 111 moved, 132 were already right, 0 anchors missing, 0\n"
  "ambiguous, 5 multi-site rows re-derived by hand.** A second run of the same\n"
  "script over the written pages reports 0 moved, which is the idempotence check.\n"
  "It also caught `src/nifskope_ui.cpp` moving a SECOND time at 05:35:59 under a\n"
  "concurrent lane (`f5d1e3bf9fab7a27` 31,409 lines -> `e820666628d97a51` 31,496):\n"
  "17 more rows on the card-sheets page and 5 on the `.lodm` page were re-derived,\n"
  "the stamps rewritten again, and both pages now say that file is under live\n"
  "edit."),
],
}

for p, pairs in EDITS.items():
    b = open(p, "rb").read()
    cr = b.count(b"\r")
    s = b.decode("utf-8")
    for old, new in pairs:
        if s.count(old) != 1:
            print("  !! %s: %d hits" % (p, s.count(old))); sys.exit(1)
        s = s.replace(old, new)
    d = s.encode("utf-8")
    assert d.count(b"\r") == cr, "%s CR moved %d -> %d" % (p, cr, d.count(b"\r"))
    open(p, "wb").write(d)
    print("%-38s updated, CR %d unchanged" % (p, cr))
