# The dated re-derivation note on every page whose numbers moved, plus the two
# stamp rows that were missing for files the pages cite.
import hashlib, os, sys
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")

def stat(rel):
    b = open(rel, "rb").read()
    return hashlib.sha256(b).hexdigest()[:16], len(b), b.count(b"\n")

NOTE = ("\n**Re-derived 2026-09-10 by lane DOCS2** (`ww-contract-provenance` step 3, script\n"
        "`scratchpad/docs2_20260910/anchors.py`): every line number below was found again\n"
        "from its own anchor text against the sources stamped above, never shifted by a\n"
        "delta. %s\n")

COUNTS = {
 "docs/LODGEN_BTD_FORMAT.md":
   "26 of 66 rows moved, 40 were already right, 0 anchors missing after two were\n"
   "trimmed to the single source line they start on (the AO row-0 comment and the\n"
   "no-ground-cover comment now wrap in the writer).",
 "docs/LODGEN_CARD_SHEETS.md":
   "24 of 36 rows moved, 12 were already right, 0 anchors missing or ambiguous.",
 "docs/LODGEN_LODM_FORMAT.md":
   "22 of 34 rows moved, 12 were already right; the coverage-sidecar row's anchor\n"
   "had a literal newline pasted into it, which had broken that markdown row in two.",
 "docs/LODGEN_MANIFEST_FORMAT.md":
   "13 of 14 rows moved; the `I`-line row's anchor lost its `continue;` (the writer\n"
   "now puts it on its own line) and was re-anchored on the `if` alone.",
 "docs/LODGEN_TEXTURE_ARRAYS.md":
   "7 of 14 rows moved; three anchors that the card-array pass now duplicates\n"
   "verbatim were lengthened until each names the array pass alone, and the\n"
   "multi-site mip row was re-derived by hand inside `lodgenEncodeArrayLayer`.",
 "docs/LODGEN_TERRAIN_VT.md":
   "6 of 19 rows moved -- all six in `src/lodgen.cpp`, which two lanes grew since\n"
   "the last stamp; `src/io/lodvfile.{h,cpp}` did not move at all (same sha256), so\n"
   "its four multi-site rows were checked line by line and left as they stood.",
 "docs/LODGEN_NATIVE_LODO_LODI.md":
   "4 of 49 rows moved -- the four `src/lodgen.cpp` rows; the six native files this\n"
   "page is about have not changed since lane NATIVE0b wrote them (same sha256).",
 "docs/LODGEN_VERTEX_PACKING.md":
   "9 of 11 rows moved; the `OBJ_VERTEX_DESC` anchor was a prefix of\n"
   "`OBJ_VERTEX_DESC_COLORS` on the next line and was lengthened, and the\n"
   "accessor row's range end was put back on `ResetAttributeOffsets`'s own\n"
   "closing brace rather than carried by the same delta as its start.",
}

for p, c in COUNTS.items():
    b = open(p, "rb").read()
    assert b.count(b"\r") == 0, p
    s = b.decode("utf-8")
    key = "\n| claim | line | anchor |\n"
    i = s.index(key)
    s = s[:i] + NOTE % c + s[i:]
    d = s.encode("utf-8")
    assert d.count(b"\r") == 0
    open(p, "wb").write(d)
    print("noted %s" % p)

# the one file LODM_FORMAT cites without a stamp
p = "docs/LODGEN_LODM_FORMAT.md"
b = open(p, "rb").read(); s = b.decode("utf-8")
h, by, ln = stat("src/gl/glmesh.cpp")
old = "| `src/nifskope_ui.cpp` | `%s` | 31,409 |\n" % stat("src/nifskope_ui.cpp")[0]
assert s.count(old) == 1
s = s.replace(old, old + "| `src/gl/glmesh.cpp` | `%s` | %s |\n" % (h, "{:,}".format(ln)))
d = s.encode("utf-8"); assert d.count(b"\r") == 0
open(p, "wb").write(d)
print("added glmesh.cpp stamp to LODM_FORMAT")
