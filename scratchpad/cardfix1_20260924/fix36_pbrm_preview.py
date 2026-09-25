# CARDFIX1 step 7 (IMPOSTORPBRM1) job 5 support: the pictures' "3D model" column needs the MESH to show the
# .pbrm's own roughness / metallic through LOD channel 10, which only the bake's retarget makes it do. So:
#  - the bake's per-shape .pbrm resolve moves into one helper (wwPbrmResolveShape), used by the bake and by
#  - wwPbrmRetargetScene(), which the orbit preview calls when WW_IMPOSTOR_MESH_PBRM=1 (harness only; no
#    user-facing path reaches it). Both files LF-only.
P = 'E:/Projects/NifskopeWWE-cardfix1/src/'


def T(block, base):
    out = []
    for l in block.split('\n'):
        if not l.strip():
            out.append('')
            continue
        n = len(l) - len(l.lstrip(' '))
        out.append('\t' * (base + n // 4) + ' ' * (n % 4) + l.lstrip(' '))
    return '\n'.join(out)


def patch(name, pairs):
    b = open(P + name, 'rb').read()
    assert b.count(b'\r') == 0
    s = b.decode('utf-8')
    for o, n in pairs:
        assert s.count(o) == 1, (name, o[:70], s.count(o))
        s = s.replace(o, n)
    out = s.encode('utf-8')
    assert out.count(b'\r') == 0
    open(P + name, 'wb').write(out)
    print('patched %s: %+d bytes' % (name, len(out) - len(b)))


s = open(P + 'nifskope_ui.cpp', 'rb').read().decode('utf-8')
head = "\t\t\t\t\t\tauto pbrmTry = [&]( int b, const QModelIndex & iShader, const QString & matName, const QString & diffuse ) {\n"
i = s.index(head) + len(head)
j = s.index("\n\t\t\t\t\t\t};\n", i) + 1    # 6 tabs exactly: the reader lambda inside closes at 7
BODY_OLD = s[i:j]
assert BODY_OLD.startswith("\t\t\t\t\t\t\tPbrmResolveInput rin;\n") and BODY_OLD.endswith("\t\t\t\t\t\t\treturn true;\n")
BODY_NEW = T('''WwPbrmCardShape s;
if ( !wwPbrmResolveShape( sc, looseRoot, b, iShader, matName, diffuse, s ) )
    return false;
pbrmShapes.push_back( s );
return true;
''', 7) + '\n'

RESOLVE = T('''/*! Resolve one shape's .pbrm through the viewport's one resolver (io/pbrmresolve: the direct name, the
 *  same-name sibling, the diffuse stem), reading the loose root first and then the stack -- the mesh-LOD
 *  mask path's order (src/lodgen.cpp ~1849-1869). Fills `s` (no sources yet) and returns true when one served. */
bool wwPbrmResolveShape( Scene * sc, const QString & looseRoot, int b, const QModelIndex & iShader,
    const QString & matName, const QString & diffuse, WwPbrmCardShape & s )
{
    const NifModel * nif = sc ? sc->nifModel : nullptr;
    if ( !nif )
        return false;
    PbrmResolveInput rin;
    rin.material = matName;
    rin.cutAuthoringPath = true;
    rin.sibling = true;
    rin.fo76 = false;
    rin.stemDiffuse = diffuse;
    auto reader = [&]( const QString & path, QByteArray & out ) -> bool {
        QString rel = path;
        rel.replace( QChar( '\\\\' ), QChar( '/' ) );
        if ( rel.startsWith( QStringLiteral( "Materials/" ) ) )
            rel.replace( 0, 1, QChar( 'm' ) );
        out.clear();
        if ( !looseRoot.isEmpty() ) {
            QFile f( looseRoot + "/" + rel );
            if ( f.open( QIODevice::ReadOnly ) )
                out = f.readAll();
            if ( !out.isEmpty() )
                return true;
        }
        if ( !nif->findResourceFile( rel, "materials", ".pbrm" ).isEmpty() )
            nif->getResourceFile( out, rel, "materials", ".pbrm" );
        return !out.isEmpty();
    };
    const PbrmResolveResult rr = pbrmResolve( rin, reader );
    if ( rr.route == PbrmRoute::Legacy || !rr.material.ok )
        return false;
    Property * p = sc->getProperty( nif, iShader );
    auto * bsp = p ? p->cast<BSShaderLightingProperty>() : nullptr;
    if ( !bsp )
        return false;
    s.block = b;
    s.bsp = bsp;
    s.served = rr.path;
    s.served.replace( QChar( '\\\\' ), QChar( '/' ) );
    s.route = QLatin1String( pbrmRouteName( rr.route ) );
    s.m = rr.material;
    s.tree = bsp->isVertexAlphaAnimation ? 1 : 0;
    return true;
}

}    // namespace

/*! HARNESS ONLY (impostorpreviewtest.cpp, WW_IMPOSTOR_MESH_PBRM=1): the bake's .pbrm retarget on the preview's
 *  mesh, so LOD channel 10 shows each .pbrm shape's own roughness / metallic / AO -- the "3D model" column of
 *  IMPOSTORPBRM1's pictures. Every shape that resolves one is retargeted, mixed models included. One line per
 *  shape for the harness log. The sources' folder lives until the next call. */
QStringList wwPbrmRetargetScene( Scene * sc, const QString & looseRoot )
{
    static std::unique_ptr<QTemporaryDir> tmp;
    QStringList lines;
    const NifModel * nif = sc ? sc->nifModel : nullptr;
    if ( !nif )
        return { QStringLiteral( "pbrm preview: no scene" ) };
    tmp = std::make_unique<QTemporaryDir>();
    const QString sub = QStringLiteral( "wwpbrmcard%1" ).arg( ++wwPbrmBakeSerial );
    const QString texDir = tmp->path() + "/textures/" + sub;
    if ( !tmp->isValid() || !QDir().mkpath( texDir ) )
        return { QStringLiteral( "pbrm preview: no temporary folder" ) };
    const_cast< NifModel * >( nif )->addResourceRoot( QDir::toNativeSeparators( tmp->path() + "/textures" ) );
    for ( int b = 0; b < nif->getBlockCount(); b++ ) {
        const QModelIndex iShader = nif->getBlockIndex( b );
        if ( !nif->isNiBlock( iShader, "BSLightingShaderProperty" ) )
            continue;
        QString diffuse;
        const QModelIndex iTexSet = nif->getBlockIndex( nif->getLink( iShader, "Texture Set" ) );
        if ( iTexSet.isValid() ) {
            const QModelIndex iArr = nif->getIndex( iTexSet, "Textures" );
            if ( iArr.isValid() )
                diffuse = nif->get<QString>( nif->getIndex( iArr, 0 ) );
        }
        WwPbrmCardShape s;
        if ( !wwPbrmResolveShape( sc, looseRoot, b, iShader, nif->get<QString>( iShader, "Name" ), diffuse, s ) )
            continue;
        QString why;
        if ( !wwPbrmCardSources( nif, looseRoot, texDir, QStringLiteral( "textures\\\\" ) + sub,
                QString::number( b ), s, why ) ) {
            lines << QStringLiteral( "pbrm preview: %1 refused: %2" ).arg( s.served, why );
            continue;
        }
        s.bsp->wwTextureOverride.insert( 0, s.colour );
        s.bsp->wwTextureOverride.insert( 2, s.emissive );
        s.bsp->wwTextureOverride.insert( 7, s.rmaos );
        lines << QStringLiteral( "pbrm preview: %1 %2 retargeted (tree %3)" ).arg( s.served, s.route ).arg( s.tree );
    }
    return lines;
}
''', 0) + '\n'

patch('nifskope_ui.cpp', [(head + BODY_OLD, head + BODY_NEW), ("}    // namespace\n", RESOLVE)])

HOOK_OLD = ("\t\t\tconst int meshChannel = qEnvironmentVariableIntValue( \"WW_IMPOSTOR_MESH_CHANNEL\" );\n")
HOOK_NEW = HOOK_OLD + T('''/* WW_IMPOSTOR_MESH_PBRM=1 (IMPOSTORPBRM1's pictures): the mesh takes the
 * bake's .pbrm retarget, so channel 10 on the mesh half is the source's
 * own roughness / metallic. The .pbrm is read from WW_LODGEN_DATA_ROOT
 * first, as the bake reads it. */
if ( qEnvironmentVariableIntValue( "WW_IMPOSTOR_MESH_PBRM" ) == 1 ) {
    for ( const QString & l : wwPbrmRetargetScene( ogl->getScene(),
            qEnvironmentVariable( "WW_LODGEN_DATA_ROOT" ) ) )
        st.log << l;
}
''', 3) + '\n'
DECL_OLD = "#include <cstdlib>\t// std::_Exit -- see endRun()\n"
DECL_NEW = DECL_OLD + ("\n//! nifskope_ui.cpp: the card bake's .pbrm retarget, applied to the preview's mesh (harness only)\n"
                       "QStringList wwPbrmRetargetScene( Scene * sc, const QString & looseRoot );\n")
patch('impostorpreviewtest.cpp', [(HOOK_OLD, HOOK_NEW), (DECL_OLD, DECL_NEW)])
