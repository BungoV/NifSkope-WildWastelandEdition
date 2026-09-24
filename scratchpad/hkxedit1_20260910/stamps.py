"""Provenance pass for docs/HKX_PACKFILE_MODEL.md (ww-contract-provenance steps
1, 3 and 5): hash + line-count the sources into the two stamp tables, and check
every anchor in the claim table matches EXACTLY ONCE in its file (reporting
MISSING / AMBIGUOUS, never rewriting a number). Run twice; the second run must
report 0 changes."""
import hashlib, os, re, sys

REPO = r"E:\Projects\NifskopeWildWastelandEdition"
DOC = os.path.join(REPO, "docs", "HKX_PACKFILE_MODEL.md")
FILES = ["res/hkclasses_fo4.json", "src/hkxfile.h", "src/hkxfile.cpp", "src/hkxmodel.h", "src/hkxmodel.cpp",
         "tools/hkclassdb_extract.py", "tests/spells/hkxfile_oracle.py"]


def stamp(path):
    b = open(os.path.join(REPO, path), "rb").read()
    return hashlib.sha256(b).hexdigest()[:16], len(b), b.count(b"\n")


doc = open(DOC, "rb").read().decode("utf-8")
assert b"\r" not in open(DOC, "rb").read()
changed = 0
# table 1 (top): "| `file` | see section 7 | | |" or a filled row -> filled
for f in FILES:
    sha, nb, nl = stamp(f)
    row = "| `%s` | `%s` | %s | %s |" % (f, sha, "{:,}".format(nb), "{:,}".format(nl))
    pat = re.compile(r"^\| `%s` \| .*\|$" % re.escape(f), re.M)
    m = pat.search(doc)
    if m and m.group(0) != row and m.group(0).count("|") == 5:
        doc = doc[:m.start()] + row + doc[m.end():]
        changed += 1
# table 2 (section 7): the STAMPS placeholder row -> one row per file
stamps_rows = "\n".join("| `%s` | `%s` | %s | %s | this page's source |" % (f, s, "{:,}".format(nb), "{:,}".format(nl))
                        for f in FILES for (s, nb, nl) in [stamp(f)])
if "| STAMPS |" in doc:
    doc = re.sub(r"^\| STAMPS \|.*$", stamps_rows, doc, flags=re.M)
    changed += 1
else:
    # refresh existing rows
    for f in FILES:
        sha, nb, nl = stamp(f)
        row = "| `%s` | `%s` | %s | %s | this page's source |" % (f, sha, "{:,}".format(nb), "{:,}".format(nl))
        pat = re.compile(r"^\| `%s` \| `[0-9a-f]{16}` \| [0-9,]+ \| [0-9,]+ \| this page's source \|$" % re.escape(f), re.M)
        m = pat.search(doc)
        if m and m.group(0) != row:
            doc = doc[:m.start()] + row + doc[m.end():]
            changed += 1
# anchors
missing = ambiguous = ok = 0
for m in re.finditer(r"^\| (.+?) \| `([^`]+)` \| `(.+?)`(?: … `(.+?)`)? \|$", doc, re.M):
    claim, path, anchor, tail = m.groups()
    src = open(os.path.join(REPO, path), "rb").read().decode("utf-8", "replace")
    a = anchor.replace("\\|", "|")
    n = src.count(a)
    if n == 1:
        ok += 1
    elif n == 0:
        missing += 1
        print("MISSING   %-40s %s: %r" % (claim, path, a))
    else:
        ambiguous += 1
        print("AMBIGUOUS %-40s %s: %r x%d" % (claim, path, a, n))
open(DOC, "wb").write(doc.encode("utf-8"))
print("stamps: %d rows changed; anchors: %d ok, %d missing, %d ambiguous" % (changed, ok, missing, ambiguous))
sys.exit(1 if (missing or ambiguous) else 0)
