# Amend ww-contract-provenance in BOTH trees (CONSTITUTION 1a: they drift, and a
# lane that amends one applies it to the other and says so).
import os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")

TREES = [r".claude\skills\ww-contract-provenance\SKILL.md",
         r"E:\Projects\Claude\.claude\skills\ww-contract-provenance\SKILL.md"]

OLD = """A worked script is
`E:\\Projects\\NifskopeWildWastelandEdition\\scratchpad\\rename_20260909\\p14_anchors.py`;
copy it rather than re-writing it."""

NEW = """A worked script is
`E:\\Projects\\NifskopeWildWastelandEdition\\scratchpad\\docs2_20260910\\anchors.py`
(2026-09-10, lane DOCS2: eight pages, eleven sources, 243 rows in one run); it
supersedes `scratchpad\\rename_20260909\\p14_anchors.py`, which handled one file
per page. Copy it rather than re-writing it: four of its rules were each learned
by a wrong report."""

EXTRA = """
**Four things the extractor must handle, or it reports rot that is not there**
(lane DOCS2, 2026-09-10: 11 of its 13 first-run MISSING rows were the script,
not the tree):

* **An ellipsis INSIDE the backticks** — `` `const qint16 rect[8] = { f.south, … ` ``
  — leaves the span unclosed, so a `` `([^`]+)` `` regex finds nothing at all and
  the row reads as a dead anchor. Split on the ellipsis AFTER extracting the
  span, and fall back to "text after the opening backtick, up to the ellipsis".
* **Normalise whitespace on both sides before matching.** An anchor that was
  copied across a wrapped markdown cell carries a newline; a source line carries
  tabs. Collapse runs of whitespace in the anchor and in each source line, then
  require the match to be exact and unique as before.
* **Skip the STAMP table.** Its rows have the same three-pipe shape as the claim
  table, and a 16-hex sha256 in the "line" column parses as a number.
* **A cite may carry a path** (`gl/glmesh.cpp:732`), not just a basename.

**Two failures that survive a correct extractor**, and are hand work:

* **A range's END is re-derived, not carried.** Adding the start's delta to the
  end moved `niftypes.h:1899-1979` to `1907-1987`, which runs into the next
  function — on a file whose sha256 had not changed at all. If the source's hash
  matches the page's stamp, a "moved" row on it is a bug in the pass.
* **An anchor that a SECOND site now writes verbatim** is not ambiguous by
  accident: a pass was copied. Lengthen the anchor with the unique neighbouring
  line rather than picking a number, or the next run reports it ambiguous
  forever. Three `LODGEN_TEXTURE_ARRAYS.md` rows needed this once the card-array
  pass grew its own copy of the texture-array writer.

**Run the pass twice.** The second run over the written pages must report
`0 moved`; that is the idempotence check, and it is cheap.
"""

for t in TREES:
    b = open(t, "rb").read()
    assert b.count(b"\r") == 0, t
    s = b.decode("utf-8")
    assert s.count(OLD) == 1, t
    s = s.replace(OLD, NEW)
    key = "\n**Three rules the script must enforce"
    i = s.index(key)
    s = s[:i] + EXTRA + s[i:]
    d = s.encode("utf-8")
    assert d.count(b"\r") == 0
    open(t, "wb").write(d)
    print("amended %s (%d -> %d bytes)" % (t, len(b), len(d)))
