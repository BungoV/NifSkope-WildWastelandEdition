# Inline `file:line` citations in doc BODIES (not the footer tables), plus one
# footer range whose END the mechanical shift carried past the real end of the
# function it names.
import os, sys
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")

EDITS = {
"docs/LODGEN_VERTEX_PACKING.md": [
    ("annotates (`src/lodgen.cpp:1357-1358` say `0x1B00000650405` and",
     "annotates (`src/lodgen.cpp:1358-1359` say `0x1B00000650405` and"),
    ("comment at `src/lodgen.cpp:57`. The *integer* is right (52,776,558,133,763);",
     "comment at `src/lodgen.cpp:58`. The *integer* is right (52,776,558,133,763);"),
    # GetVertexSize 1907 .. the closing brace of ResetAttributeOffsets 1979;
    # the scripted pass shifted the END by the same delta as the start, which
    # ran it into ClearAttributeOffsets.
    ("| the bit layout and the offset accessors | `niftypes.h:1907-1987` |",
     "| the bit layout and the offset accessors | `niftypes.h:1907-1979` |"),
],
"scratchpad/handoff_fo4cs/WRITER_CHANGES_NEEDED.md": [
    ("(`src/lodtfile.h:58`), and FO4CS's parser pins `kVersion = 1u`. A file generated",
     "(`src/lodtfile.h:159`), and FO4CS's parser pins `kVersion = 1u`. A file generated"),
],
}

for p, pairs in EDITS.items():
    b = open(p, "rb").read()
    assert b.count(b"\r") == 0, p
    s = b.decode("utf-8")
    for old, new in pairs:
        n = s.count(old)
        if n != 1:
            print("  !! %s: %d hits for %r" % (p, n, old[:70]))
            sys.exit(1)
        s = s.replace(old, new)
    d = s.encode("utf-8")
    assert d.count(b"\r") == 0
    open(p, "wb").write(d)
    print("%-50s %d inline cites fixed" % (p, len(pairs)))
