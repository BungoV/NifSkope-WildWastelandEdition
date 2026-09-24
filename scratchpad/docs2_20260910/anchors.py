# ww-contract-provenance step 3, generalised over every LODGEN/HKX contract page.
# Re-derives EVERY provenance line number from its own anchor text against the
# CURRENT source. Never re-stamps: EXACT + UNIQUE or the number is left alone and
# reported MISSING / AMBIGUOUS. Multi-site rows are listed for hand work.
import os, re, sys
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")

APPLY = "--apply" in sys.argv

SRCPATHS = [
    "src/lodtfile.cpp", "src/lodtfile.h", "src/btdterrain.cpp", "src/btdterrain.h",
    "src/lodgen.cpp", "src/lodgen.h", "src/nifskope_ui.cpp", "src/nifcli.cpp",
    "src/lodgenmanager.cpp", "src/io/lodmfile.cpp", "src/io/lodmfile.h",
    "src/io/lodvfile.cpp", "src/io/lodvfile.h", "src/gl/glmesh.cpp",
    "src/lodofile.h", "src/lodofile.cpp", "src/lodifile.h", "src/lodifile.cpp",
    "src/nativeemit.h", "src/nativeemit.cpp", "src/data/niftypes.h",
    "src/hkxanim.h", "src/hkxanim.cpp",
    "tests/spells/lodgen_native_decode.py", "tests/spells/hkxanim_decode.py",
    "tests/spells/lodl_water.sh",
]
SRC = {}
for f in SRCPATHS:
    if os.path.exists(f):
        b = open(f, "rb").read()
        assert b.count(b"\r") == 0, f
        SRC[os.path.basename(f)] = b.decode("utf-8").split("\n")


def load(p):
    b = open(p, "rb").read()
    assert b.count(b"\r") == 0, "%s has CRs" % p
    return b.decode("utf-8")


def unescape(a):
    return a.replace("\\|", "|").replace("\\*", "*").replace("\\_", "_").strip()


def find(basename, anchor, log):
    lines = SRC.get(basename)
    if lines is None:
        log.append("  NOFILE   %s" % basename)
        return None
    spans = re.findall(r"`([^`]+)`", anchor)
    if not spans:
        # an ellipsis inside the backticks leaves the span unclosed; take the
        # text after the opening backtick up to the ellipsis instead.
        m2 = re.search(r"`([^`]+?)(?:…|\.\.\.)", anchor)
        if not m2:
            return None
        spans = [m2.group(1)]
    a = unescape(re.split(r"…|\.\.\.", spans[0])[0])
    if len(a) < 8:
        return None
    norm = lambda t: re.sub(r"\s+", " ", t).strip()
    an = norm(a)
    hits = [i for i, ln in enumerate(lines, 1) if an in norm(ln)]
    if len(hits) == 1:
        return hits[0]
    if len(hits) > 1:
        log.append("  AMBIGUOUS %-22s %r -> %s" % (basename, a[:46], hits[:6]))
        return "AMB"
    return None


ROW = re.compile(r"^\| (?P<claim>[^|\n]+) \| (?P<line>[^|\n]+) \| (?P<anchor>(?:[^|]|\n)*?) \|[ \t]*$", re.M)

DOCS = {
    "docs/LODGEN_BTD_FORMAT.md":     "lodtfile.cpp",
    "docs/LODGEN_CARD_SHEETS.md":    None,
    "docs/LODGEN_LODM_FORMAT.md":    None,
    "docs/LODGEN_MANIFEST_FORMAT.md": "lodgen.cpp",
    "docs/LODGEN_TEXTURE_ARRAYS.md": None,
    "docs/LODGEN_TERRAIN_VT.md":     None,
    "docs/LODGEN_NATIVE_LODO_LODI.md": None,
    "docs/LODGEN_VERTEX_PACKING.md": None,
}

TOTAL = {"moved": 0, "same": 0, "missing": 0, "amb": 0, "multi": 0}

for p, default in DOCS.items():
    s = load(p)
    i = s.find("## Provenance")
    if i < 0:
        i = s.find("### Provenance")
    if i < 0:
        i = s.find("## Source, as built")
    body = s[i:]
    out = []
    moved = missing = same = amb = multi = 0
    log = []
    for m in ROW.finditer(body):
        claim = m.group("claim").strip()
        lineref = m.group("line").strip()
        anchor = m.group("anchor")
        if (lineref in ("line", "---", ":---", "sha256 (16)", "stamp", "role")
                or "sha256" in lineref
                or re.fullmatch(r"`?[0-9a-f]{16}`?", lineref)
                or claim.startswith("`src/") or claim.startswith("`tests/")):
            continue
        nums = re.findall(r"\d{2,}", lineref)
        if not nums:
            continue                      # a bare filename: no number to check
        sites = re.findall(r"[a-zA-Z_][\w.]*\.(?:cpp|h|py|sh)", lineref)
        # MULTI-SITE: more than one file named, or a comma/'also' between numbers
        ranges = re.findall(r"\b\d+(?:-\d+)?\b", lineref)
        if len(sites) > 1 or len(ranges) > 2 or (len(ranges) == 2 and "-" not in lineref):
            log.append("  MULTISITE %-34s | %s" % (lineref, anchor.replace("\n", " ")[:56]))
            multi += 1
            continue
        mm = re.match(r"^`?(?:(?:[\w/]+/)?(?P<f>[a-zA-Z_][\w.]*\.(?:cpp|h|py|sh))[: ])?"
                      r"(?P<n>\d+)(?:-(?P<n2>\d+))?", lineref)
        if not mm:
            log.append("  UNPARSED %s" % lineref)
            continue
        base = mm.group("f") or default
        if base is None:
            log.append("  NODEFAULT %-24s | %s" % (lineref, claim[:44]))
            missing += 1
            continue
        got = find(base, anchor, log)
        if got == "AMB":
            amb += 1
            continue
        if got is None:
            log.append("  MISSING  %-20s %-16s | %s" % (base, lineref, claim[:60]))
            missing += 1
            continue
        old = int(mm.group("n"))
        if got == old:
            same += 1
            continue
        moved += 1
        delta = got - old
        if mm.group("n2"):
            new_ref = lineref.replace("%s-%s" % (mm.group("n"), mm.group("n2")),
                                      "%d-%d" % (got, int(mm.group("n2")) + delta), 1)
        else:
            new_ref = re.sub(r"(?<![\d])%d(?![\d])" % old, str(got), lineref, count=1)
        log.append("  moved    %-20s %-22s -> %-22s %s" % (base, lineref, new_ref, claim[:40]))
        old_row = m.group(0)
        new_row = old_row.replace("| " + lineref + " |", "| " + new_ref + " |", 1)
        assert new_row != old_row, old_row[:90]
        out.append((old_row, new_row))
    print("=== %s: %d unchanged, %d moved, %d not found, %d ambiguous, %d multi-site"
          % (p, same, moved, missing, amb, multi))
    for L in log:
        print(L.encode("ascii", "replace").decode())
    TOTAL["moved"] += moved; TOTAL["same"] += same
    TOTAL["missing"] += missing; TOTAL["amb"] += amb; TOTAL["multi"] += multi
    if APPLY and out:
        for a, b in out:
            assert s.count(a) == 1, a[:90]
            s = s.replace(a, b)
        d = s.encode("utf-8")
        assert d.count(b"\r") == 0
        open(p, "wb").write(d)
        print("  WRITTEN %d rows" % len(out))

print("TOTAL", TOTAL)
