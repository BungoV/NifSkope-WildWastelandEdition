"""DEFAULTS2: bungo's two rulings of 2026-09-23 09:3x become what a bare bake does.
Every edit asserts its anchor occurs exactly once and the CR count is unchanged.
Run with --check to write nothing."""
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'
CHECK = '--check' in sys.argv

EDITS = {
    'src/lodgen.cpp': [
        ("/* Lane TILING2. Both default to the 2026-09-11 bake exactly: `footprint`\n"
         " * sampling, full detail, no edge blend. See lodgen.h for the measurements. */\n"
         "static bool  g_landSampleAverage = false;\n"
         "static float g_landDetail        = 0.0f;\n"
         "static int   g_blendEdges        = 0;\n",
         "/* Lane TILING2. Sampling and detail default to the 2026-09-11 bake exactly:\n"
         " * `footprint` sampling, full detail. The EDGE BLEND defaults ON since\n"
         " * 2026-09-23 (bungo, \"Yes, default on\"; lane DEFAULTS2): `quadrant`, and\n"
         " * `--blend-edges off` is the exact way back. See lodgen.h. */\n"
         "static bool  g_landSampleAverage = false;\n"
         "static float g_landDetail        = 0.0f;\n"
         "static int   g_blendEdges        = 1;\n"),
    ],
    'src/lodgen.h': [
        (" * is not blended -- and is not blended from the other side either, so no new\n"
         " * seam is created. `off` is byte-identical to the bake before this. */\n",
         " * is not blended -- and is not blended from the other side either, so no new\n"
         " * seam is created. `off` is byte-identical to the bake before this.\n"
         " * DEFAULT `quadrant` since 2026-09-23 (bungo: \"Yes, default on\"; lane\n"
         " * DEFAULTS2); `--blend-edges off` is the way back, byte for byte. */\n"),
    ],
    'src/nifcli.cpp': [
        ("\t\t/* THE QUADRANT BORDER (lane TILING2). `off` is the default and the bake\n"
         "\t\t * before this lane exactly; `quadrant` cross-fades the neighbouring\n",
         "\t\t/* THE QUADRANT BORDER (lane TILING2). `quadrant` is the DEFAULT since\n"
         "\t\t * 2026-09-23 (bungo, lane DEFAULTS2); `off` is the bake before that\n"
         "\t\t * ruling exactly. `quadrant` cross-fades the neighbouring\n"),
        ("\t\t  << \"  lodgen ... --terrain-region ... [--land-hex UNITS]\\n\"\n",
         "\t\t  << \"  lodgen ... --terrain-region ... [--blend-edges off|quadrant]\\n\"\n"
         "\t\t  << \"                                          the 2,048-unit quadrant lines of the\\n\"\n"
         "\t\t  << \"                                          land colour. DEFAULT quadrant since\\n\"\n"
         "\t\t  << \"                                          2026-09-23 (bungo): cross-faded over\\n\"\n"
         "\t\t  << \"                                          --blend-margin units (default 128)\\n\"\n"
         "\t\t  << \"                                          either side; colour sheets only.\\n\"\n"
         "\t\t  << \"                                          off = hard lines, the exact way back\\n\"\n"
         "\t\t  << \"  lodgen ... --terrain-region ... [--land-hex UNITS]\\n\"\n"),
    ],
    'src/lodgenmanager.cpp': [
        ("\t\t\t\ttr( \"Quadrant edges\" ), 0,\n"
         "\t\t\t\t{ { tr( \"Hard\" ), 0 }, { tr( \"Cross-faded\" ), 1 } },\n"
         "\t\t\t\ttr( \"Cross-fades the neighbouring quadrant's composite over the margin below,\\n\"\n"
         "\t\t\t\t\t\"either side of every 2,048-unit quadrant line. Hard is the default and is\\n\"\n"
         "\t\t\t\t\t\"the bake without it.\\nCommand line: --blend-edges\" ) );\n",
         "\t\t\t\ttr( \"Quadrant edges\" ), 1,\n"
         "\t\t\t\t{ { tr( \"Hard\" ), 0 }, { tr( \"Cross-faded\" ), 1 } },\n"
         "\t\t\t\ttr( \"Cross-fades the neighbouring quadrant's composite over the margin below,\\n\"\n"
         "\t\t\t\t\t\"either side of every 2,048-unit quadrant line. Cross-faded is the default;\\n\"\n"
         "\t\t\t\t\t\"Hard is the bake without it.\\nCommand line: --blend-edges\" ) );\n"),
        ("\t\t\t\tconst int saved = settings.value( QStringLiteral( \"LodGeneration/cardRes\" ), 128 ).toInt();\n"
         "\t\t\t\tconst int idx = cardResBox->findData( saved );\n"
         "\t\t\t\tcardResBox->setCurrentIndex( idx >= 0 ? idx : 1 );\n",
         "\t\t\t\t// 256 px since 2026-09-23 (bungo: tree cards \"8x8 at 2k\", lane DEFAULTS2)\n"
         "\t\t\t\tconst int saved = settings.value( QStringLiteral( \"LodGeneration/cardRes\" ), 256 ).toInt();\n"
         "\t\t\t\tconst int idx = cardResBox->findData( saved );\n"
         "\t\t\t\tcardResBox->setCurrentIndex( idx >= 0 ? idx : 2 );\n"),
    ],
    'src/nifskope_ui.cpp': [
        ("{ \"LodgenBlendEdgesBox\", \"blendEdges\", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, \"0\" },\n",
         "{ \"LodgenBlendEdgesBox\", \"blendEdges\", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, \"1\" },\n"),
        ("\t\t\t\t\t\tif ( tile < 32 || tile > 512 )\n"
         "\t\t\t\t\t\t\ttile = 128;\n",
         "\t\t\t\t\t\tif ( tile < 32 || tile > 512 )\n"
         "\t\t\t\t\t\t\ttile = 256;\t// 8 x 8 at 2k (bungo 2026-09-23, lane DEFAULTS2); was 128\n"),
    ],
    'tools/bake_impostor_cards.sh': [
        ("# that is a 4096-texel sheet per channel, about 12 MB for the largest tree type.\n"
         "case \"${TILE:-128}\" in\n",
         "# that is a 4096-texel sheet per channel, about 12 MB for the largest tree type.\n"
         "# DEFAULT 256 since 2026-09-23 (bungo: tree cards \"8x8 at 2k\", lane DEFAULTS2):\n"
         "# OCT 8 x TILE 256 = a 2048-texel sheet for the largest base. Was 128 (1024).\n"
         "case \"${TILE:-256}\" in\n"),
        ("TILE=\"${TILE:-128}\"\n", "TILE=\"${TILE:-256}\"\n"),
    ],
}


def main():
    ok = True
    for rel, reps in EDITS.items():
        p = ROOT + rel
        b = open(p, 'rb').read()
        s = b.decode('utf-8')
        cr0 = b.count(b'\r')
        for old, new in reps:
            n = s.count(old)
            if n != 1:
                print('ANCHOR x%d in %s: %r' % (n, rel, old[:70]))
                ok = False
                continue
            s = s.replace(old, new)
        nb = s.encode('utf-8')
        if nb.count(b'\r') != cr0:
            print('CR count moved in', rel); ok = False
        if ok and not CHECK:
            open(p, 'wb').write(nb)
        print(('checked ' if CHECK else 'patched ') + rel, len(reps), 'edit(s)')
    print('OK' if ok else 'REFUSED')
    sys.exit(0 if ok else 1)


main()
