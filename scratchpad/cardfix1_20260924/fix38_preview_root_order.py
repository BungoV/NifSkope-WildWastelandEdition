# CARDFIX1 step 7 job 5: the preview's retarget registered its resource root BEFORE writing the sources, and
# NifModel::addResourceRoot rebuilds the file index on the NEXT lookup -- wwPbrmCardSources' own normal lookup,
# with the folder still empty -- so every retargeted name missed (the mesh half drew the missing-texture magenta,
# pbrm/pics run 1). The bake's order is sources first, root second; the preview now does the same. LF-only.
P = 'E:/Projects/NifskopeWWE-cardfix1/src/nifskope_ui.cpp'
OLD = ("\tconst_cast< NifModel * >( nif )->addResourceRoot( QDir::toNativeSeparators( tmp->path() + \"/textures\" ) );\n"
       "\tfor ( int b = 0; b < nif->getBlockCount(); b++ ) {\n")
NEW = ("\tstd::vector<WwPbrmCardShape> done;    // the root goes on AFTER the sources exist: the index is built at\n"
       "\t                                      // the next lookup, and the sources' own normal lookup is one\n"
       "\tfor ( int b = 0; b < nif->getBlockCount(); b++ ) {\n")
OLD2 = ("\t\ts.bsp->wwTextureOverride.insert( 0, s.colour );\n"
        "\t\ts.bsp->wwTextureOverride.insert( 2, s.emissive );\n"
        "\t\ts.bsp->wwTextureOverride.insert( 7, s.rmaos );\n"
        "\t\tlines << QStringLiteral( \"pbrm preview: %1 %2 retargeted (tree %3)\" ).arg( s.served, s.route ).arg( s.tree );\n"
        "\t}\n"
        "\treturn lines;\n")
NEW2 = ("\t\tdone.push_back( s );\n"
        "\t}\n"
        "\tconst_cast< NifModel * >( nif )->addResourceRoot( QDir::toNativeSeparators( tmp->path() + \"/textures\" ) );\n"
        "\tfor ( const WwPbrmCardShape & s : done ) {\n"
        "\t\ts.bsp->wwTextureOverride.insert( 0, s.colour );\n"
        "\t\ts.bsp->wwTextureOverride.insert( 2, s.emissive );\n"
        "\t\ts.bsp->wwTextureOverride.insert( 7, s.rmaos );\n"
        "\t\tconst QString found = nif->findResourceFile( s.colour, \"textures\", \".dds\" );\n"
        "\t\tlines << QStringLiteral( \"pbrm preview: %1 %2 retargeted (tree %3), colour %4\" ).arg( s.served, s.route )\n"
        "\t\t\t.arg( s.tree ).arg( found.isEmpty() ? QStringLiteral( \"NOT FOUND\" ) : QStringLiteral( \"found\" ) );\n"
        "\t}\n"
        "\treturn lines;\n")
b = open(P, 'rb').read()
cr = b.count(b'\r')
s = b.decode('utf-8')
for o, n in [(OLD, NEW), (OLD2, NEW2)]:
    assert s.count(o) == 1, (o[:70], s.count(o))
    s = s.replace(o, n)
out = s.encode('utf-8')
assert out.count(b'\r') == cr
open(P, 'wb').write(out)
print('patched nifskope_ui.cpp: %+d bytes, CR %d' % (len(out) - len(b), cr))
