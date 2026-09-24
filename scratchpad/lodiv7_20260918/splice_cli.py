p = 'src/nifcli.cpp'
s = open(p, encoding='utf-8', newline='').read()


def rep(o, n, c=1):
    global s
    assert s.count(o) == c, (s.count(o), o[:80])
    s = s.replace(o, n)


rep("	bool libraryNear, bool ladderFoliage, float silhouetteMin, bool placementAo, bool vertexAo, bool treesOnly,",
    "	bool libraryNear, bool ladderFoliage, float silhouetteMin, bool placementAo, bool vertexAo, bool lodiV7, bool treesOnly,")
rep("			lodgenNativeVertexAoOption( vertexAo );",
    "			lodgenNativeVertexAoOption( vertexAo );\n			lodgenNativeLodiV7Option( lodiV7 );")
rep("	bool lgNativeVertexAo = true;",
    "	bool lgNativeVertexAo = true;\n	bool lgLodiV7 = true;")
rep(" *   --native-no-vertex-ao     no per-instance vertex-AO stream, no .lodi version 6 */",
    " *   --native-no-vertex-ao     no per-instance vertex-AO stream, no .lodi version 6\n"
    " *   --lodi-v6                 no group table, no per-vertex sky stream; the .lodi\n"
    " *                             stays at version 6, byte for byte */")
rep('		else if ( t == QLatin1String( "--native-no-vertex-ao" ) ) lgNativeVertexAo = false;',
    '		else if ( t == QLatin1String( "--native-no-vertex-ao" ) ) lgNativeVertexAo = false;\n'
    '		else if ( t == QLatin1String( "--lodi-v6" ) ) lgLodiV7 = false;')
rep("			lgLibraryNear, lgNativeLadderFoliage, lgNativeSilhouette, lgNativePlacementAo, lgNativeVertexAo,",
    "			lgLibraryNear, lgNativeLadderFoliage, lgNativeSilhouette, lgNativePlacementAo, lgNativeVertexAo, lgLodiV7,")

HELP_OLD = '  << "  lodgen ... --native <dir> --native-no-vertex-ao\\n"\n'
HELP_NEW = ('  << "  lodgen ... --native <dir> --native-no-vertex-ao\\n"\n'
            '	  << "  lodgen ... --native <dir> --lodi-v6\\n"\n'
            '	  << "                                          write no group table and no\\n"\n'
            '	  << "                                          per-vertex sky stream; the\\n"\n'
            '	  << "                                          .lodi stays at version 6,\\n"\n'
            '	  << "                                          byte for byte\\n"\n')
rep(HELP_OLD, HELP_NEW)

open(p, 'w', encoding='utf-8', newline='').write(s)
print('nifcli ok')
