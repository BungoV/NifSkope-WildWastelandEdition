#!/usr/bin/env python3
"""RESUME3, SPLAT1 phase B: the landscape textures' world-space tiling.

SPLAT1 read the number out of the shipped binary rather than fitting it.
`Fallout4.exe` 1.10.155 carries ONE landscape-tiling setting,
`fLandTextureTilingMult:Landscape` (file 0x2C84DD8, VA 0x142C861D8), whose
Setting record (file 0x36E83A8) holds 0x3FC00000 = 1.5f, and its data slot
(VA 0x1436E97B0) has exactly one code reference, at 0x1403A74C6. The loop that
consumes it (0x1403A7620 / 0x1403A7650) walks a 17 x 17 landscape quadrant grid
and stores `u = col * mult/4 = col * 0.375` per vertex. 17 vertices span one
quadrant = 2,048 world units, so a vertex step is 128 units and

        WORLD UNITS PER REPEAT = 128 / 0.375 = 341.3333

-- 6 repeats a quadrant, 12 a cell. The bake used 2,048: every landscape texture
was stretched to exactly 6.0000x its size, which is the speckle bungo saw.

THE CHANGE, and it is one value. Both bake functions declare their own
`constexpr float TILE = 2048.0f` and all FOURTEEN uses -- colour, mask, emissive,
both bake paths -- read the declaration in scope. So the two declarations become
`const float TILE = lodgenLandTiling();` and every site moves together, which is
SPLAT1's red 2: a fix that moved only the colour path would leave the mask sheet
describing a different patch of ground from the colour beside it.

`_msn` is computed from VHGT and no tiling term reaches it (SPLAT1 red 3); the
gate is that it is byte-identical at BOTH tiling values.

`--land-tiling 2048` is the exact way back and is what the byte-identity gate
uses. NO PANEL ROW: the LOD Generation panel's rows belong to lane LODUI1; this
is CLI and INI only.

    python tiling.py --check
    python tiling.py --apply
"""
import sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
NL = chr(10)
TAB = chr(9)

EDITS = []

# ---------------------------------------------------------------- src/lodgen.h
EDITS.append(("src/lodgen.h", "after",
"void lodgenWarmSharedIndices();" + NL,
NL +
"/* --- The landscape textures' world-space tiling ------------------------------" + NL +
" *" + NL +
" * How many world units one repeat of a landscape diffuse covers. The engine's" + NL +
" * own number is 341.3333 = 128 / 0.375, read out of Fallout4.exe 1.10.155:" + NL +
" * `fLandTextureTilingMult:Landscape` = 1.5f (Setting record at file 0x36E83A8," + NL +
" * data slot VA 0x1436E97B0, its single code reference at 0x1403A74C6), and the" + NL +
" * 17x17 landscape quadrant loop at 0x1403A7620 stores u = column * mult/4 =" + NL +
" * column * 0.375 per vertex over a 2,048-unit quadrant, i.e. 128 units a" + NL +
" * vertex step. 6 repeats a quadrant, 12 a cell. (lane SPLAT1, 2026-09-11.)" + NL +
" *" + NL +
" * The bake used 2,048 -- exactly 6.0000x too large -- so every landscape" + NL +
" * texture was stretched six-fold and the far sheet printed a 64x64 image of" + NL +
" * the ground texture at full contrast, tiled 8x8 across the chunk. All" + NL +
" * FOURTEEN sampling sites read this one value: colour, mask and emissive, in" + NL +
" * both the stock chunk bake and the pyramid. The normal sheet (`_msn`) is" + NL +
" * computed from VHGT and no tiling term reaches it." + NL +
" *" + NL +
" * `--land-tiling 2048` is the exact way back and is byte-identical to the" + NL +
" * pre-2026-09-11 bake. Another user CAN set fLandTextureTilingMult in an INI," + NL +
" * which is why this is a value and not a new hard-coded constant. */" + NL +
"float lodgenLandTiling();" + NL +
"void lodgenSetLandTiling( float unitsPerRepeat );" + NL))

# -------------------------------------------------------------- src/lodgen.cpp
EDITS.append(("src/lodgen.cpp", "after",
"void lodgenWarmSharedIndices()" + NL,
"",   # placeholder, replaced below -- kept so the ordering is explicit
))
EDITS.pop()

EDITS.append(("src/lodgen.cpp", "replace",
'/*! Every "once, on first use" index the generator owns, built NOW, on the' + NL,
"/* The landscape tiling, set once by the CLI before the bake starts and read" + NL +
" * from every sampling site afterwards. 341.3333 = 128 / 0.375 is the engine's" + NL +
" * own number (see lodgen.h); 2048 is the pre-2026-09-11 bake, exactly." + NL +
" * A non-positive value is refused and leaves the default standing, so a" + NL +
" * mistyped switch cannot silently flatten the ground to one texel. */" + NL +
"static float g_landTiling = 341.3333f;" + NL +
NL +
"float lodgenLandTiling()" + NL +
"{" + NL +
TAB + "return g_landTiling;" + NL +
"}" + NL +
NL +
"void lodgenSetLandTiling( float unitsPerRepeat )" + NL +
"{" + NL +
TAB + "if ( unitsPerRepeat > 0.0f )" + NL +
TAB + TAB + "g_landTiling = unitsPerRepeat;" + NL +
"}" + NL +
NL +
'/*! Every "once, on first use" index the generator owns, built NOW, on the' + NL))

# the two declarations. THEY CARRY THE SAME TEXT, so each is anchored with the
# lines around it -- the stock chunk bake and the pyramid, in that order.
# NOTE: the third line carries an EM DASH (U+2014), not two hyphens. SPLAT1's
# report retyped it as "--" and the anchor counted 0 -- read out of the file's
# own bytes here (ww-anchored-hookup section 5).
STOCK_OLD = (
"\t// world-space tiling of the source landscape textures; near-terrain\n"
"\t// repeats roughly every half cell (calibration against vanilla bakes is\n"
"\t// an open refinement " + chr(0x2014) + " the constant only affects apparent texel density)\n"
"\tconstexpr float TILE = 2048.0f;\n")

TILE_NEW = (
"\t/* World-space units per repeat of a landscape diffuse. 341.3333 = 128/0.375\n"
"\t * is the engine's own tiling, read out of Fallout4.exe 1.10.155 at\n"
"\t * 0x1403A74C6 / 0x1403A7620 (lane SPLAT1); `--land-tiling 2048` restores the\n"
"\t * pre-2026-09-11 bake byte for byte. Every TILE site below reads this. */\n"
"\tconst float TILE = lodgenLandTiling();\n")


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else '--check'
    edits = list(EDITS)

    # the two `constexpr float TILE` declarations, found from the file's own
    # bytes rather than by line number: one carries the old comment block, the
    # other is bare. Both become the same line.
    p = os.path.join(ROOT, 'src', 'lodgen.cpp')
    b = open(p, 'rb').read()
    n_stock = b.count(STOCK_OLD.encode())
    n_bare = b.count(b"\tconstexpr float TILE = 2048.0f;\n")
    print('src/lodgen.cpp   the commented declaration  count=%d' % n_stock)
    print('src/lodgen.cpp   `constexpr float TILE = 2048.0f;` total count=%d' % n_bare)
    if n_stock != 1 or n_bare != 2:
        print('RESULT REFUSED - expected 1 commented and 2 total declarations')
        return 3
    edits.append(('src/lodgen.cpp', 'replace', STOCK_OLD, TILE_NEW))
    # after that replace the only remaining bare one is the pyramid's
    edits.append(('src/lodgen.cpp', 'replace',
                  "\tconstexpr float TILE = 2048.0f;\n", TILE_NEW))

    # ------------------------------------------------------------ src/nifcli.cpp
    edits.append(("src/nifcli.cpp", "after",
'\t\telse if ( t == QLatin1String( "--chunk-threads" ) ) lodgenSetChunkThreadCount( next().toInt() );' + NL,
'\t\t/* THE LANDSCAPE TEXTURES\' WORLD-SPACE TILING (lane SPLAT1 measured it,' + NL +
'\t\t * lane RESUME3 landed it). Default 341.3333 = 128/0.375, the engine\'s' + NL +
'\t\t * own number out of Fallout4.exe 1.10.155. `--land-tiling 2048` is the' + NL +
'\t\t * exact way back to the pre-2026-09-11 bake. */' + NL +
'\t\telse if ( t == QLatin1String( "--land-tiling" ) ) lodgenSetLandTiling( next().toFloat() );' + NL))

    edits.append(("src/nifcli.cpp", "after",
'\t\t  << "  lodgen ... --terrain-region ... [--chunk-threads N]' + chr(92) + 'n"' + NL,
'\t\t  << "  lodgen ... --terrain-region ... [--land-tiling UNITS]' + chr(92) + 'n"' + NL +
'\t\t  << "                                          world units one repeat of a' + chr(92) + 'n"' + NL +
'\t\t  << "                                          landscape texture covers.' + chr(92) + 'n"' + NL +
'\t\t  << "                                          DEFAULT 341.333 = 128/0.375,' + chr(92) + 'n"' + NL +
'\t\t  << "                                          the engine\'s own tiling;' + chr(92) + 'n"' + NL +
'\t\t  << "                                          2048 is the exact way back' + chr(92) + 'n"' + NL))

    files = {}
    ok = True
    for path, mth, anchor, text in edits:
        full = os.path.join(ROOT, path)
        if path not in files:
            files[path] = open(full, 'rb').read()
        cur = files[path]
        a = anchor.encode('utf-8')
        n = cur.count(a)
        print('%-18s %-8s count=%d  %s' % (path, mth, n, anchor.strip().split(NL)[0][:58]))
        if n < 1:
            ok = False
            print('   REFUSE: anchor matched 0 times')
            continue
        t = text.encode('utf-8')
        files[path] = cur.replace(a, (a + t) if mth == 'after' else t, 1)

    for path in files:
        before = open(os.path.join(ROOT, path), 'rb').read()
        print('%-18s CR before=%d after=%d  bytes %d -> %d'
              % (path, before.count(b'\r'), files[path].count(b'\r'),
                 len(before), len(files[path])))
        if before.count(b'\r') != files[path].count(b'\r'):
            ok = False
            print('   REFUSE: line endings changed')

    # after the edits there must be NO `2048.0f` TILE declaration left and
    # exactly two `lodgenLandTiling()` declarations
    lg = files['src/lodgen.cpp']
    left = lg.count(b"constexpr float TILE = 2048.0f;")
    got = lg.count(b"const float TILE = lodgenLandTiling();")
    print('src/lodgen.cpp   TILE = 2048.0f left = %d (want 0);  '
          'TILE = lodgenLandTiling() = %d (want 2)' % (left, got))
    if left != 0 or got != 2:
        ok = False
        print('   REFUSE: the two declarations did not both move')

    if not ok:
        print(NL + 'RESULT REFUSED - nothing written')
        return 2
    if mode == '--apply':
        for path, data in files.items():
            open(os.path.join(ROOT, path), 'wb').write(data)
            print('wrote ' + path)
        print(NL + 'RESULT APPLIED - %d edits' % len(edits))
    else:
        print(NL + 'RESULT CHECK OK - %d edits, nothing written' % len(edits))
    return 0


if __name__ == '__main__':
    sys.exit(main())
