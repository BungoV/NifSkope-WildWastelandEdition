# ww-contract-provenance steps 1 + 5: rewrite every source stamp (sha256, bytes,
# lines) to the state the pages were finished against, in the table rows AND in
# the prose forms. Run LAST, after the anchors are re-derived.
import hashlib, os, re, sys
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")

def stat(rel):
    b = open(rel, "rb").read()
    return hashlib.sha256(b).hexdigest()[:16], len(b), b.count(b"\n")

DOCS = ["docs/LODGEN_BTD_FORMAT.md", "docs/LODGEN_CARD_SHEETS.md",
        "docs/LODGEN_LODM_FORMAT.md", "docs/LODGEN_MANIFEST_FORMAT.md",
        "docs/LODGEN_TEXTURE_ARRAYS.md", "docs/LODGEN_TERRAIN_VT.md",
        "docs/LODGEN_NATIVE_LODO_LODI.md", "docs/LODGEN_VERTEX_PACKING.md",
        "docs/HKX_ANIMATION_FORMAT.md"]

ROW4 = re.compile(r"^\| `(src/[^`]+|tests/[^`]+)` \| `([0-9a-f]{16})` \| ([\d,]+) \| ([\d,]+) \|$", re.M)
ROW3 = re.compile(r"^\| `(src/[^`]+|tests/[^`]+)` \| `([0-9a-f]{16})` \| ([\d,]+) \|$", re.M)

PROSE = [
 ("docs/LODGEN_MANIFEST_FORMAT.md",
  "`src/lodgen.cpp` sha256 `6d7388c53a13343e`,8286 lines, at the time of reading;",
  "`src/lodgen.cpp` sha256 `%(h)s`, %(l)s lines, at the time of reading;"),
 ("docs/LODGEN_TEXTURE_ARRAYS.md",
  "`src/lodgen.cpp` sha256 `6d7388c53a13343e`,8286 lines;\n`src/lodgenmanager.cpp` read at the same time.",
  "`src/lodgen.cpp` sha256 `%(h)s`, %(l)s lines;\n`src/lodgenmanager.cpp` sha256 `%(mh)s`, %(ml)s lines, read at the same time."),
 ("docs/LODGEN_VERTEX_PACKING.md",
  "`src/lodgen.cpp` sha256 `6d7388c53a13343e` (8286 lines),\n`src/btdterrain.cpp` `cbe06adb7394f59b`, `src/data/niftypes.h` `28ad54471e9c5044`,\nall read 2026-09-09.",
  "`src/lodgen.cpp` sha256 `%(h)s` (%(l)s lines),\n`src/btdterrain.cpp` `%(bh)s` (%(bl)s lines), `src/data/niftypes.h` `%(nh)s`\n(%(nl)s lines), re-derived 2026-09-10 (lane DOCS2)."),
]

gh, gb, gl = stat("src/lodgen.cpp")
mh, mb, ml = stat("src/lodgenmanager.cpp")
bh, bb, bl = stat("src/btdterrain.cpp")
nh, nb, nl = stat("src/data/niftypes.h")
SUB = {"h": gh, "l": "{:,}".format(gl), "mh": mh, "ml": "{:,}".format(ml),
       "bh": bh, "bl": "{:,}".format(bl), "nh": nh, "nl": "{:,}".format(nl)}

total = 0
for p in DOCS:
    b = open(p, "rb").read()
    assert b.count(b"\r") == 0, p
    s = b.decode("utf-8")
    n = [0]

    def r4(m):
        rel = m.group(1)
        if not os.path.exists(rel):
            print("  ?? %s: no such file %s" % (p, rel)); return m.group(0)
        h, by, ln = stat(rel)
        new = "| `%s` | `%s` | %s | %s |" % (rel, h, "{:,}".format(by), "{:,}".format(ln))
        if new != m.group(0):
            n[0] += 1
            print("  %-34s %s -> %s  %s/%s -> %s/%s"
                  % (rel, m.group(2), h, m.group(3), m.group(4), "{:,}".format(by), "{:,}".format(ln)))
        return new

    def r3(m):
        rel = m.group(1)
        if not os.path.exists(rel):
            print("  ?? %s: no such file %s" % (p, rel)); return m.group(0)
        h, by, ln = stat(rel)
        new = "| `%s` | `%s` | %s |" % (rel, h, "{:,}".format(ln))
        if new != m.group(0):
            n[0] += 1
            print("  %-34s %s -> %s  %s -> %s lines"
                  % (rel, m.group(2), h, m.group(3), "{:,}".format(ln)))
        return new

    s = ROW4.sub(r4, s)
    s = ROW3.sub(r3, s)
    for pp, old, new in PROSE:
        if pp == p:
            if s.count(old) != 1:
                print("  !! prose stamp not found in %s" % p); sys.exit(1)
            s = s.replace(old, new % SUB); n[0] += 1
    d = s.encode("utf-8")
    assert d.count(b"\r") == 0
    open(p, "wb").write(d)
    print("%-40s %d stamps rewritten" % (p, n[0]))
    total += n[0]
print("TOTAL stamps rewritten:", total)
