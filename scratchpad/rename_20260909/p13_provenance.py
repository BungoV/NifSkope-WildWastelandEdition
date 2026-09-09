# Patch 13 -- the last of the doc text, then the PROVENANCE pass:
# re-stamp every footer's sha256/bytes/lines and re-derive every line number
# from its own anchor text against the CURRENT source. This is the procedure
# lane CONTRACTS described and it is written up as the skill
# `ww-contract-provenance`.
import hashlib
import os
import re
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")


def load(p):
    b = open(p, "rb").read()
    assert b.count(b"\r") == 0, p
    return b.decode("utf-8")


def save(p, s):
    out = s.encode("utf-8")
    assert out.count(b"\r") == 0
    open(p, "wb").write(out)


def sub(s, old, new, p, n=1):
    c = s.count(old)
    assert c == n, "%s: anchor count %d != %d for %r" % (p, c, n, old[:80])
    return s.replace(old, new)


# ------------------------------------------------ 1. the last of the VT text
P = "docs/LODGEN_TERRAIN_VT.md"
s = load(P)
s = sub(s,
        "| 0x00 | char[4] | `magic` | `'L','O','D','V'`. Deliberately neither `DDS ` nor "
        "`LODT`: a wrong-but-plausible parse is worse than a refusal. |",
        "| 0x00 | char[4] | `magic` | `'L','D','T','X'` (`LODTEX_MAGIC`, 0x5854444C). "
        "Deliberately neither `DDS `, nor `LODT` (the LANDSCAPE file's, which this "
        "extension named until 2026-09-09), nor the retired `LODV` this container "
        "carried before that date: a wrong-but-plausible parse is worse than a refusal, "
        "and both of those are refused BY NAME. |", P)
s = sub(s, "2. `magic != 'LODV'`",
        "2. `magic != 'LDTX'` — and `LODT` (the landscape file, now `.lodl`) and the "
        "retired `LODV` are each named in the refusal rather than lumped into "
        "\"bad magic\"", P)
s = sub(s, "| magic `'LODV'`, version 1, header 256 B |",
        "| magic `'LDTX'`, version 1, header 256 B |", P)
s = sub(s, "`constexpr quint32 LODV_MAGIC = 0x56444F4CU;`",
        "`constexpr quint32 LODTEX_MAGIC = 0x5854444CU;` (in `lodvfile.h`)", P)
s = sub(s, "`put32( h + 0x00, LODV_MAGIC );`", "`put32( h + 0x00, LODTEX_MAGIC );`", P)
save(P, s)

P = "docs/LODGEN_BTD_FORMAT.md"
s = load(P)
s = sub(s, "| magic `'LODT'` | 38 | `constexpr quint32 LODT_MAGIC = 0x54444F4CU;` |",
        "| magic `'LODT'` (unchanged by the `.lodl` rename) | `lodtfile.h` | "
        "`constexpr quint32 LODL_MAGIC = 0x54444F4CU;` |", P)
s = sub(s, "| versions 1..2, header sizes 0x98 / 0xA0 | 43-46 | "
           "`constexpr quint32 LODT_VERSION = 2;` … `LODT_HEADER_V2 = 0xA0;` |",
        "| versions 1..2, header sizes 0x98 / 0xA0 | 43-46 | "
        "`constexpr quint32 LODL_VERSION = 2;` … `LODL_HEADER_V2 = 0xA0;` |", P)
save(P, s)

# --------------------------------------------- 2. the sha / bytes / lines rows
SRC = ["src/lodtfile.cpp", "src/lodtfile.h", "src/io/lodvfile.cpp",
       "src/io/lodvfile.h", "src/lodgen.cpp", "src/nifcli.cpp",
       "src/lodgenmanager.cpp", "src/nifskope_ui.cpp"]
stamp = {}
for f in SRC:
    b = open(f, "rb").read()
    stamp[f] = (hashlib.sha256(b).hexdigest()[:16], len(b), b.count(b"\n"))
    print("%-28s %s  %d bytes  %d lines" % ((f,) + stamp[f]))

DOCS = ["docs/LODGEN_BTD_FORMAT.md", "docs/LODGEN_TERRAIN_VT.md",
        "docs/LODGEN_CARD_SHEETS.md", "docs/LODGEN_LODM_FORMAT.md",
        "docs/LODGEN_MANIFEST_FORMAT.md", "docs/LODGEN_TEXTURE_ARRAYS.md",
        "docs/LODGEN_VERTEX_PACKING.md", "docs/LODGEN_IMPOSTOR_SPEC.md"]

# table rows: | `src/x.cpp` | `hash` | 64,360 | 1,682 |
ROW = re.compile(r"^\| `(src/[^`]+)` \| `([0-9a-f]{16})` \| ([\d,]+) \| ([\d,]+) \|$", re.M)
# prose: `src/lodgen.cpp` sha256 `fc655702d783622d`, 8283 lines
PROSE = re.compile(r"`(src/[^`]+)` sha256 `([0-9a-f]{16})`(,| \()\s*(\(?)([\d,]+) lines")

changed = []
for p in DOCS:
    s = load(p)
    orig = s

    def row(m):
        f = m.group(1)
        if f not in stamp:
            return m.group(0)
        h, n, l = stamp[f]
        changed.append((p, f, m.group(2), h))
        return "| `%s` | `%s` | %s | %s |" % (f, h, "{:,}".format(n), "{:,}".format(l))
    s = ROW.sub(row, s)

    def prose(m):
        f = m.group(1)
        if f not in stamp:
            return m.group(0)
        h, n, l = stamp[f]
        changed.append((p, f, m.group(2), h))
        return "`%s` sha256 `%s`%s%s%d lines" % (f, h, m.group(3), m.group(4), l)
    s = PROSE.sub(prose, s)

    if s != orig:
        save(p, s)
        print("stamped", p)

print("\n---- re-stamped ----")
for p, f, was, now in changed:
    print("  %-34s %-24s %s -> %s" % (p, f, was, now))
