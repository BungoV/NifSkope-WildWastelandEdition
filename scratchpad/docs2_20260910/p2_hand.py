# Second hand pass: give the last three rows anchors that are UNIQUE, so a future
# scripted pass verifies them instead of reporting them ambiguous forever. The
# numbers were derived by hand first; these anchors only make the derivation
# repeatable.
import os, sys
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")

EDITS = {
"docs/LODGEN_TEXTURE_ARRAYS.md": [
    ('| `textures.emissive` always written on an array | `lodgen.cpp:4603` | `tex.insert( QStringLiteral( "emissive" ), gameBase + emSfx );` (the card-array pass writes the same line; this one follows `lodmMaskKey( cls.pbr )`) |',
     '| `textures.emissive` always written on an array | `lodgen.cpp:4602-4603` | `lodmMaskKey( cls.pbr ) ), gameBase + maskSfx` then `tex.insert( QStringLiteral( "emissive" ), gameBase + emSfx );` \u2014 the card-array pass writes that second line verbatim, so the mask line above it is the address |'),
    ('| `array.emissiveScale` parallel to `layers` | `lodgen.cpp:4614-4622` | `arr.insert( QStringLiteral( "emissiveScale" ), scales );` (the card-array pass writes `g.scales`) |',
     '| `array.emissiveScale` parallel to `layers` | `lodgen.cpp:4613-4614` | `two layers of one array` (the comment above it) then `arr.insert( QStringLiteral( "emissiveScale" ), scales );` \u2014 the card-array pass writes the same insert under a different comment |'),
],
"docs/LODGEN_TERRAIN_VT.md": [
    ('| \u00a73.2 tile entry, 24 B, field by field | `lodvfile.cpp:379-384`, read at `607-612` | `put64( p + 0x00, e.offset ); put32( p + 0x08, e.storedBytes );` |',
     '| \u00a73.2 tile entry, 24 B, field by field | `lodvfile.cpp:379-384`, read at `607-612` | `put64( p + 0x00, e.offset );` \u2026 `put16( p + 0x16, e.reserved );`; the read side is `get64( p + 0x00 )` \u2026 `get16( p + 0x16 )` |'),
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
    print("%-38s %d rows re-anchored" % (p, len(pairs)))
