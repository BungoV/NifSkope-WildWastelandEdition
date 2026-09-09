# Patch 15 -- the provenance rows the automatic anchor pass would not touch:
# multi-site rows, and rows whose ANCHOR TEXT itself moved (a constant renamed
# or a helper extracted). Each new number was grepped against the current file
# before it was written here.
import os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")


def load(p):
    b = open(p, "rb").read()
    assert b.count(b"\r") == 0, p
    return b.decode("utf-8")


def patch(p, pairs):
    s = load(p)
    for a, b in pairs:
        c = s.count(a)
        assert c == 1, "%s: %d hits for %r" % (p, c, a[:80])
        s = s.replace(a, b)
    out = s.encode("utf-8")
    assert out.count(b"\r") == 0
    open(p, "wb").write(out)
    print("OK", p, len(out), "bytes,", len(pairs), "rows")


patch("docs/LODGEN_TERRAIN_VT.md", [
    ("| magic `'LDTX'`, version 1, header 256 B | `lodvfile.cpp:22-24` |",
     "| magic `'LDTX'`, version 1, header 256 B | `lodvfile.h:67`, `lodvfile.cpp:29-30` |"),
    ("| `indexCrc32` at 0x98, zeroed while hashing | `lodvfile.cpp:136, 382-391, 568-576` |",
     "| `indexCrc32` at 0x98, zeroed while hashing | `lodvfile.cpp:142, 382-391, 568-576` |"),
    ("| the rect field order south/west/north/east ×2 | `lodvfile.cpp:116-118` |",
     "| the rect field order south/west/north/east ×2 | `lodvfile.cpp:121-123` |"),
    ("| §3.2 tile entry, 24 B, field by field | `lodvfile.cpp:373-378`, read at `587-592` |",
     "| §3.2 tile entry, 24 B, field by field | `lodvfile.cpp:379-384`, read at `607-612` |"),
    ("| payload offset = table end aligned up to 4096 | `lodvfile.cpp:278-280` |",
     "| payload offset = table end aligned up to 4096 | `lodvfile.cpp:284-286` |"),
    ("| §3.4 rules 1, 2, 19, 20, 21, 22 by number | `lodvfile.cpp:422, 428, 533, 536, 542, 568` |",
     "| §3.4 rules 1, 2, 19, 20, 21, 22 by number | `lodvfile.cpp:428, 434, 553, 588, 556, 562` |"),
    ("| flags: north-up = refusal, full mode | `lodvfile.h:69-73` |",
     "| flags: north-up = refusal, full mode | `lodvfile.h:92-96` |"),
    ("| `dxgiFormatCover` differs only on role 3 | `lodvfile.h:82-88` |",
     "| `dxgiFormatCover` differs only on role 3 | `lodvfile.h:104-110` |"),
    ("`lodgen.cpp:6809, 6814`", "`lodgen.cpp:6812, 6817`"),
])
