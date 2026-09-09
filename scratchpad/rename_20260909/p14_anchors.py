# Patch 14 -- re-derive EVERY provenance line number from its own anchor text
# against the current source, and rewrite the ones that moved. Reports every
# anchor it could not find, so a silent miss is impossible.
import os
import re
import sys
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")

APPLY = "--apply" in sys.argv


def load(p):
    b = open(p, "rb").read()
    assert b.count(b"\r") == 0, p
    return b.decode("utf-8")


SRC = {}
for f in ("src/lodtfile.cpp", "src/lodtfile.h", "src/io/lodvfile.cpp",
          "src/io/lodvfile.h", "src/lodgen.cpp", "src/nifcli.cpp",
          "src/lodgenmanager.cpp", "src/data/niftypes.h"):
    SRC[os.path.basename(f)] = load(f).split("\n")


def unescape(a):
    a = a.replace("\\|", "|").replace("\\*", "*").replace("\\_", "_")
    return a.strip()


def find(basename, anchor):
    """First line (1-based) whose text contains the anchor. The anchor is
    trimmed at the ellipsis, which marks a RANGE's start, and the FIRST
    backticked span is the code -- prose around it is not in the source."""
    lines = SRC.get(basename)
    if lines is None:
        return None
    head = anchor.split("…")[0]
    spans = re.findall(r"`([^`]+)`", head)
    if not spans:
        return None
    a = unescape(spans[0])
    if len(a) < 8:
        return None
    hits = [i for i, ln in enumerate(lines, 1) if a in ln]
    # EXACT, and UNIQUE. A softened prefix match once pointed a row at a
    # different function with the same opening tokens, which is worse than a
    # stale number because it reads as verified.
    if len(hits) == 1:
        return hits[0]
    if len(hits) > 1:
        print("  AMBIGUOUS %s: %r at %s" % (basename, a[:50], hits))
    return None


ROW = re.compile(r"^\| (?P<claim>[^|]+) \| (?P<line>[^|]+) \| (?P<anchor>.+?) \|\s*$", re.M)

DOCS = ["docs/LODGEN_BTD_FORMAT.md", "docs/LODGEN_TERRAIN_VT.md"]
DEFAULT = {"docs/LODGEN_BTD_FORMAT.md": "lodtfile.cpp",
           "docs/LODGEN_TERRAIN_VT.md": None}

for p in DOCS:
    s = load(p)
    head = s.index("## Provenance")
    body = s[head:]
    out = []
    moved = missing = same = 0
    for m in ROW.finditer(body):
        claim, lineref, anchor = m.group("claim"), m.group("line").strip(), m.group("anchor")
        if lineref in ("line", "---") or "sha256" in claim:
            continue
        # `lodvfile.cpp:22-24`  or  43-46  or  38
        mm = re.match(r"^`?(?:(?P<f>[a-zA-Z_][\w.]*\.(?:cpp|h)):)?"
                      r"(?P<n>\d+)(?:-(?P<n2>\d+))?", lineref)
        if not mm:
            continue
        if "," in lineref:          # multi-site rows are re-derived BY HAND
            print("  MULTISITE %s | %s" % (lineref, anchor[:60].encode("ascii", "replace").decode()))
            continue
        base = mm.group("f") or DEFAULT[p]
        if base is None:
            continue
        got = find(base, anchor)
        if got is None:
            print("  MISSING  %-32s %s | %s" % (base, lineref, anchor[:70].encode("ascii","replace").decode()))
            missing += 1
            continue
        old = int(mm.group("n"))
        if got == old:
            same += 1
            continue
        moved += 1
        delta = got - old
        new_ref = lineref
        if mm.group("n2"):
            new_ref = new_ref.replace("%s-%s" % (mm.group("n"), mm.group("n2")),
                                      "%d-%d" % (got, int(mm.group("n2")) + delta), 1)
        else:
            new_ref = re.sub(r"(?<![\d])%d(?![\d])" % old, str(got), new_ref, count=1)
        print("  moved    %-20s %-16s -> %-16s  %s" % (base, lineref, new_ref, claim[:44].encode("ascii","replace").decode()))
        out.append((m.group(0), m.group(0).replace("| " + lineref + " |",
                                                   "| " + new_ref + " |", 1)))
    print("%s: %d unchanged, %d moved, %d anchors not found" % (p, same, moved, missing))
    if APPLY and out:
        for a, b in out:
            assert s.count(a) == 1, a[:80]
            s = s.replace(a, b)
        open(p, "wb").write(s.encode("utf-8"))
        print("  written")
