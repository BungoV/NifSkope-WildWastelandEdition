#!/usr/bin/env python3
"""Re-derive every line number in docs/GLTF_INTERCHANGE.md from its anchor.

Skill `ww-contract-provenance`, step 3: the number is a convenience that rots
in hours, the anchor is the claim's real address. Run this AFTER the last edit
to the page and again to check idempotence -- the second run must report
`0 moved`.

Rules it enforces, each of which cost something somewhere:
  * EXACT and UNIQUE after whitespace normalisation, or no rewrite. A prefix
    match once pointed a row at a different function.
  * MISSING is a CONTENT question: the code the row named is gone. The number
    is left alone and the row is listed for hand work.
  * The stamp table is skipped -- its rows have the same three-pipe shape.

Usage:  python scratchpad/hkx4_20260910/anchors.py [--write]
"""
import os
import re
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
PAGE = os.path.join(REPO, "docs", "GLTF_INTERCHANGE.md")

ROW = re.compile(r"^\|\s*(?P<claim>[^|]+?)\s*\|\s*`(?P<file>[^`:]+):(?P<line>[0-9]+)`\s*\|\s*(?P<anchor>.+?)\s*\|\s*$")


def norm(s):
    return re.sub(r"\s+", " ", s).strip()


def first_span(cell):
    """The FIRST backticked span of the anchor column, tolerating an ellipsis
    inside the backticks (which leaves the span unclosed)."""
    m = re.match(r"\s*`([^`]+)`", cell)
    if m:
        text = m.group(1)
    else:
        m = re.match(r"\s*`(.+)", cell)
        if not m:
            return None
        text = m.group(1)
    text = text.split("…")[0].split("...")[0]
    return text.replace("\\|", "|")


def main(argv):
    write = "--write" in argv
    with open(PAGE, "r", encoding="utf-8", newline="") as fh:
        lines = fh.read().split("\n")
    cache = {}
    moved = same = missing = ambiguous = 0
    out = []
    for ln in lines:
        m = ROW.match(ln)
        if not m:
            out.append(ln)
            continue
        rel, want = m.group("file"), int(m.group("line"))
        anchor = first_span(m.group("anchor"))
        if anchor is None:
            out.append(ln)
            continue
        path = os.path.join(REPO, rel)
        if not os.path.exists(path):
            print("MISSING FILE  %-34s  %s" % (rel, m.group("claim")[:44]))
            missing += 1
            out.append(ln)
            continue
        if rel not in cache:
            with open(path, "r", encoding="utf-8", errors="replace", newline="") as fh:
                cache[rel] = [norm(x) for x in fh.read().split("\n")]
        needle = norm(anchor)
        hits = [i + 1 for i, s in enumerate(cache[rel]) if needle in s]
        if not hits:
            print("MISSING       %-34s  %s" % (rel, m.group("claim")[:44]))
            missing += 1
            out.append(ln)
            continue
        if len(hits) > 1:
            print("AMBIGUOUS x%d  %-34s  %s" % (len(hits), rel, m.group("claim")[:44]))
            ambiguous += 1
            out.append(ln)
            continue
        got = hits[0]
        if got == want:
            same += 1
            out.append(ln)
        else:
            moved += 1
            out.append(ln.replace("`%s:%d`" % (rel, want), "`%s:%d`" % (rel, got), 1))
    print("\n%d moved, %d unchanged, %d anchors not found, %d ambiguous" % (moved, same, missing, ambiguous))
    if write and moved:
        with open(PAGE, "w", encoding="utf-8", newline="") as fh:
            fh.write("\n".join(out))
        print("rewrote %s" % PAGE)
    elif not write:
        print("(dry run; pass --write to rewrite the page)")
    return 1 if (missing or ambiguous) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
