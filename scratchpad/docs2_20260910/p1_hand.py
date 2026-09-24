# Hand repairs the scripted pass may not make: anchors that no longer sit on ONE
# source line (so the machine cannot verify them), anchors that are no longer
# unique, and the multi-site rows the skill says are re-derived by hand.
import os, sys
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")

EDITS = {
"docs/LODGEN_BTD_FORMAT.md": [
    # anchor wrapped across two source lines -> trim to the line it starts on
    ("| AO row 0 is SOUTH | 186 | `Row 0 is SOUTH (the grid's own order, cell row 0 first)` |",
     "| AO row 0 is SOUTH | 186 | `Row 0 is SOUTH (the grid's own` |"),
    ("| FO4 writes no ground cover | 2424 | `FO4 has no ground cover HERE: measured, Fallout4.esm carries 0 GCVR records` |",
     "| FO4 writes no ground cover | 2429 | `FO4 has no ground cover HERE: measured, Fallout4.esm carries 0 GCVR` |"),
],
"docs/LODGEN_LODM_FORMAT.md": [
    # the cell carried a real newline (the C++ \\n was pasted literally), which
    # broke the markdown row in two and hid the anchor from every checker
    ('| the bake states the coverage contract on the sidecar | `nifskope_ui.cpp:22675` | `ms << "coverage " << covFloor << " " << covTest << " " << covBase << "\n";` |',
     '| the bake states the coverage contract on the sidecar | `nifskope_ui.cpp:22675` | `ms << "coverage " << covFloor << " " << covTest << " " << covBase` |'),
],
"docs/LODGEN_MANIFEST_FORMAT.md": [
    # `continue;` moved onto its own line
    ("| `I` line and the \u2265 8 threshold | `3288-3297` | `if ( it.value().second.size() < 8 ) continue;` |",
     "| `I` line and the \u2265 8 threshold | `3608-3617` | `if ( it.value().second.size() < 8 )` |"),
],
"docs/LODGEN_TEXTURE_ARRAYS.md": [
    # three anchors that the card-array pass now duplicates verbatim: lengthened
    # until each names the ARRAY pass alone
    ("| the four sheets and their suffixes | `lodgen.cpp:4260-4264` | `const struct { QString suffix; \u2026 } sheets[4]` |",
     "| the four sheets and their suffixes | `lodgen.cpp:4585-4589` | `const struct { QString suffix; const std::vector<std::vector<quint32>> * px; bool bc3; } sheets[4]` |"),
    ('| `textures.emissive` always written on an array | `lodgen.cpp:4278` | `tex.insert( QStringLiteral( "emissive" ), gameBase + emSfx );` |',
     '| `textures.emissive` always written on an array | `lodgen.cpp:4603` | `tex.insert( QStringLiteral( "emissive" ), gameBase + emSfx );` (the card-array pass writes the same line; this one follows `lodmMaskKey( cls.pbr )`) |'),
    ('| `array.emissiveScale` parallel to `layers` | `lodgen.cpp:4281-4289` | `arr.insert( QStringLiteral( "emissiveScale" ), scales );` |',
     '| `array.emissiveScale` parallel to `layers` | `lodgen.cpp:4614-4622` | `arr.insert( QStringLiteral( "emissiveScale" ), scales );` (the card-array pass writes `g.scales`) |'),
    ('| `A` line appended to the manifest | `lodgen.cpp:4344` | `lines.append( QString( "A %1 %2 %3" )` |',
     '| `A` line appended to the manifest | `lodgen.cpp:4669` | `lines.append( QString( "A %1 %2 %3" ).arg( b ).arg( lit.value().second )` |'),
    # multi-site row, re-derived BY HAND against lodgenEncodeArrayLayer
    ("| mip law and BC1 alpha forcing | `lodgen.cpp:3878, 3895` | `while ( mw > 4 && mh > 4 \u2026`, `( bc3 ? \u2026 : 0xFFU ) << 24` |",
     "| mip law and BC1 alpha forcing | `lodgen.cpp:4203, 4220` | `while ( mw > 4 && mh > 4 \u2026`, `( bc3 ? \u2026 : 0xFFU ) << 24` (both inside `lodgenEncodeArrayLayer`, whose signature is at 4198) |"),
],
"docs/LODGEN_TERRAIN_VT.md": [
    # multi-site row, re-derived BY HAND
    ('| `coarseLevelsAreDownsamples` / `alignedToWorldOrigin` | `lodgen.cpp:7323, 7328` | `t.insert( QStringLiteral( "coarseLevelsAreDownsamples" ), true );` |',
     '| `coarseLevelsAreDownsamples` / `alignedToWorldOrigin` | `lodgen.cpp:7397, 7402` | `t.insert( QStringLiteral( "coarseLevelsAreDownsamples" ), true );`, `t.insert( QStringLiteral( "alignedToWorldOrigin" ), true );` |'),
],
"docs/LODGEN_VERTEX_PACKING.md": [
    # the shorter anchor is a prefix of OBJ_VERTEX_DESC_COLORS on the next line
    ("| `OBJ_VERTEX_DESC = 474989027590661` | `lodgen.cpp:1357` | `constexpr std::uint64_t OBJ_VERTEX_DESC` |",
     "| `OBJ_VERTEX_DESC = 474989027590661` | `lodgen.cpp:1358` | `constexpr std::uint64_t OBJ_VERTEX_DESC = 474989027590661ULL;` |"),
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
    print("%-38s %d rows repaired" % (p, len(pairs)))
