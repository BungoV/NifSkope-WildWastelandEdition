# CARDFIX1 step 7 (IMPOSTORPBRM1) job 3: the card bake reads a .pbrm (src/nifskope_ui.cpp, the bake hook only).
# The design is DONE.md step 7 "Design" 1-4: a shape resolving a .pbrm through io/pbrmresolve is pbr-sourced; the
# law is evaluated on the CPU in TEXTURE space into uncompressed DDS files in a per-bake temporary folder that is
# registered as a resource root, and the existing retarget hook (wwTextureOverride) points slots 0/1/2/7 at them.
# The specular quantities go to a new `_s` sheet (RGB sqrt(F0'), A weight) from two extra channel-10 passes.
# No shader change. LF-only file: CR count must stay 0.
P = 'E:/Projects/NifskopeWWE-cardfix1/src/nifskope_ui.cpp'


def T(block, base):
    """4-space indent units -> tabs, plus `base` tabs; blank lines stay empty."""
    out = []
    for l in block.split('\n'):
        if not l.strip():
            out.append('')
            continue
        n = len(l) - len(l.lstrip(' '))
        out.append('\t' * (base + n // 4) + ' ' * (n % 4) + l.lstrip(' '))
    return '\n'.join(out)


HELPERS = r'''/* IMPOSTORPBRM1 (lane CARDFIX1 step 7, 2026-09-25): a card baked from a model whose materials are
 * .pbrm files carries the .pbrm's quantities (docs/LODGEN_IMPOSTOR_SPEC.md "Ours, for LOD").
 *
 * The bake photographs data channels through the LEGACY program (channel views never take the PBRM
 * program, src/gl/renderer.cpp:271-278), so the .pbrm law is evaluated HERE, on the CPU, in texture
 * space, and handed to that program as ordinary textures through the retarget hook:
 *   slot 0  sRGB( decode(map) x colour x tintMix ), A = the map's A (overrideOpacity off) or the constant
 *   slot 7  R roughness, G metallic, B AO -- a map channel while its override is off, else the constant
 *   slot 2  sRGB( colour x mask ) when the emissive is on, else empty (black); the multiple = luminance/100
 *   spec    RGB = sqrt(F0'), F0' = clamp( w x decode(tint) x min(((ior-1)/(ior+1))^2, 1) )
 *   weight  RGB = w, the specular weight
 * The law is NifSkope's viewport law (renderer.cpp:1056-1095, pbrm_default.frag), which is the PBRM
 * editor's; the gate's Python evaluation (tests/spells/impostor_pbrm.py) is written from the contract. */
namespace {

int wwPbrmBakeSerial = 0;

struct WwPbrmCardShape
{
    int block = -1;
    BSShaderLightingProperty * bsp = nullptr;
    QString served, route;
    PbrmMaterial m;
    int tree = 0;
    QString colour, normal, rmaos, emissive, spec, weight;    // the retarget names (game paths)
    float emissiveScale = 0.0f;
    bool specDefault = true;
    QString refusal;
};

//! One shape's slot 7 for the specular passes: the spec source, the weight source, and what to restore.
struct WwPbrmSpecSlot
{
    BSShaderLightingProperty * bsp;
    QString spec, weight, restore;
};

float wwSrgbToLinear( float c )
{
    c = qBound( 0.0f, c, 1.0f );
    return c <= 0.04045f ? c / 12.92f : std::pow( ( c + 0.055f ) / 1.055f, 2.4f );
}

float wwLinearToSrgb( float c )
{
    c = qBound( 0.0f, c, 1.0f );
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow( c, 1.0f / 2.4f ) - 0.055f;
}

int wwByte( float c )
{
    return qBound( 0, int( c * 255.0f + 0.5f ), 255 );
}

//! A texture by its .pbrm lookup path (normalised, no `textures\`): the loose root first, then the stack.
std::unique_ptr<DDSTexture16> wwPbrmLoadTexture( const NifModel * nif, const QString & looseRoot,
    const QString & lookup, QString & why )
{
    QString rel = lookup;
    rel.replace( QChar( '\\' ), QChar( '/' ) );
    if ( !rel.startsWith( QStringLiteral( "textures/" ), Qt::CaseInsensitive ) )
        rel.prepend( QStringLiteral( "textures/" ) );
    QByteArray b;
    if ( !looseRoot.isEmpty() ) {
        QFile f( looseRoot + "/" + rel );
        if ( f.open( QIODevice::ReadOnly ) )
            b = f.readAll();
    }
    if ( b.isEmpty() && nif && !nif->findResourceFile( rel, "textures", ".dds" ).isEmpty() )
        nif->getResourceFile( b, rel, "textures", ".dds" );
    if ( b.isEmpty() ) {
        why = QStringLiteral( "unreadable %1" ).arg( rel );
        return nullptr;
    }
    try {
        // raw stored values (noSRGBExpand): the law decodes what it decodes itself
        return std::make_unique<DDSTexture16>( reinterpret_cast<const unsigned char *>( b.constData() ),
            size_t( b.size() ), 0, true );
    } catch ( std::exception & e ) {
        why = QStringLiteral( "undecodable %1 (%2)" ).arg( rel, QString::fromUtf8( e.what() ) );
    }
    return nullptr;
}

//! An uncompressed B8G8R8A8 DDS (the legacy header) with a box-filtered mip chain down to 1 x 1.
bool wwWriteDdsBgra( const QString & path, const QImage & top )
{
    QImage lv = top.convertToFormat( QImage::Format_ARGB32 );
    int levels = 1;
    for ( int s = qMax( lv.width(), lv.height() ); s > 1; s >>= 1 )
        levels++;
    QByteArray out( 128, 0 );
    auto put = [&out]( int off, quint32 v ) { qToLittleEndian<quint32>( v, out.data() + off ); };
    put( 0, 0x20534444u );                                  // "DDS "
    put( 4, 124 );
    put( 8, 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000 );           // caps, height, width, pixelformat, mipmapcount
    put( 12, quint32( lv.height() ) );
    put( 16, quint32( lv.width() ) );
    put( 20, quint32( lv.width() * 4 ) );
    put( 28, quint32( levels ) );
    put( 76, 32 );
    put( 80, 0x41 );                                        // RGB | ALPHAPIXELS
    put( 88, 32 );
    put( 92, 0x00FF0000u );
    put( 96, 0x0000FF00u );
    put( 100, 0x000000FFu );
    put( 104, 0xFF000000u );
    put( 108, 0x1000 | 0x8 | 0x400000 );                    // texture, complex, mipmap
    for ( int l = 0; l < levels; l++ ) {
        for ( int y = 0; y < lv.height(); y++ )
            out.append( reinterpret_cast<const char *>( lv.constScanLine( y ) ), lv.width() * 4 );
        if ( l + 1 == levels )
            break;
        const int w2 = qMax( 1, lv.width() / 2 ), h2 = qMax( 1, lv.height() / 2 );
        QImage nx( w2, h2, QImage::Format_ARGB32 );
        for ( int y = 0; y < h2; y++ ) {
            QRgb * d = reinterpret_cast<QRgb *>( nx.scanLine( y ) );
            const int y0 = qMin( 2 * y, lv.height() - 1 ), y1 = qMin( 2 * y + 1, lv.height() - 1 );
            const QRgb * r0 = reinterpret_cast<const QRgb *>( lv.constScanLine( y0 ) );
            const QRgb * r1 = reinterpret_cast<const QRgb *>( lv.constScanLine( y1 ) );
            for ( int x = 0; x < w2; x++ ) {
                const int x0 = qMin( 2 * x, lv.width() - 1 ), x1 = qMin( 2 * x + 1, lv.width() - 1 );
                const QRgb q[4] = { r0[x0], r0[x1], r1[x0], r1[x1] };
                int s[4] = {};
                for ( const QRgb c : q ) {
                    s[0] += qRed( c ); s[1] += qGreen( c ); s[2] += qBlue( c ); s[3] += qAlpha( c );
                }
                d[x] = qRgba( ( s[0] + 2 ) / 4, ( s[1] + 2 ) / 4, ( s[2] + 2 ) / 4, ( s[3] + 2 ) / 4 );
            }
        }
        lv = nx;
    }
    QFile f( path );
    return f.open( QIODevice::WriteOnly ) && f.write( out ) == out.size();
}

/*! Evaluate one shape's .pbrm law in texture space and write its retarget sources into `texDir`
 *  (game path `gameDir`), file names `<stem>_<c|rmaos|spec|w|e>.dds`. False, with `why`, when a map
 *  the law SAMPLES cannot be read: the shape is then refused and the card stays legacy. */
bool wwPbrmCardSources( const NifModel * nif, const QString & looseRoot, const QString & texDir,
    const QString & gameDir, const QString & stem, WwPbrmCardShape & s, QString & why )
{
    const PbrmMaterial & m = s.m;
    const quint32 f = m.features;
    std::unique_ptr<DDSTexture16> base, rm, sp, tint, em;
    auto need = [&]( const PbrmMaterial::Slot & slot, std::unique_ptr<DDSTexture16> & t ) {
        t = wwPbrmLoadTexture( nif, looseRoot, slot.lookupPath, why );
        return bool( t );
    };
    const bool rmSampled = ( f & ( PbrmMaterial::RmaosRoughness | PbrmMaterial::RmaosMetallic
        | PbrmMaterial::RmaosAo | PbrmMaterial::RmaosSpecularWeight ) ) != 0;
    const bool spSampled = ( f & ( PbrmMaterial::SpecularColorTexture | PbrmMaterial::SpecularIorTexture ) ) != 0;
    bool tintTex = false;
    for ( int c = 0; c < 4; c++ )
        tintTex = tintTex || ( m.tintEnabled && m.tintUseTexture[c] );
    const bool emOn = m.emissive.enabled && m.emissiveIntensity > 0.0f;
    if ( ( f & PbrmMaterial::BaseColorTexture ) && !need( m.baseColor, base ) )
        return false;
    if ( rmSampled && !need( m.rmaos, rm ) )
        return false;
    if ( spSampled && !need( m.specularColor, sp ) )
        return false;
    if ( tintTex && !need( m.tintMask, tint ) )
        return false;
    if ( emOn && ( f & PbrmMaterial::EmissiveTexture ) && !need( m.emissive, em ) )
        return false;

    // a grid: the map's own, halved until neither side exceeds 2048; 4 x 4 for a constant
    struct Grid { int w = 4, h = 4; };
    auto gridOf = []( std::initializer_list<const DDSTexture16 *> ts ) {
        Grid g;
        bool any = false;
        for ( const DDSTexture16 * t : ts ) {
            if ( !t )
                continue;
            int w = t->getWidth(), h = t->getHeight();
            while ( w > 2048 || h > 2048 ) {
                w = qMax( 1, w / 2 ); h = qMax( 1, h / 2 );
            }
            if ( !any || w * h > g.w * g.h ) {
                g.w = w; g.h = h;
            }
            any = true;
        }
        return g;
    };
    auto mipOf = []( const DDSTexture16 * t, int w ) {
        float mm = 0.0f;
        for ( int s2 = t->getWidth(); s2 > w; s2 /= 2 )
            mm += 1.0f;
        return mm;
    };
    auto at = [&]( const std::unique_ptr<DDSTexture16> & t, float u, float v, int w ) {
        return t ? t->getPixelT( u, v, mipOf( t.get(), w ) ) : FloatVector4( 0.0f, 0.0f, 0.0f, 0.0f );
    };
    auto write = [&]( const QImage & img, const QString & suffix, QString & name ) {
        const QString file = stem + "_" + suffix + ".dds";
        if ( !wwWriteDdsBgra( texDir + "/" + file, img ) ) {
            why = QStringLiteral( "cannot write %1/%2" ).arg( texDir, file );
            return false;
        }
        name = gameDir + "\\" + file;
        return true;
    };

    // slot 0: the colour, tinted
    {
        const Grid g = gridOf( { base.get(), tint.get() } );
        QImage img( g.w, g.h, QImage::Format_ARGB32 );
        for ( int y = 0; y < g.h; y++ ) {
            QRgb * d = reinterpret_cast<QRgb *>( img.scanLine( y ) );
            for ( int x = 0; x < g.w; x++ ) {
                const float u = ( float( x ) + 0.5f ) / float( g.w ), v = ( float( y ) + 0.5f ) / float( g.h );
                float rgb[3] = { m.baseColorRGB[0], m.baseColorRGB[1], m.baseColorRGB[2] };
                float a = m.opacity;
                if ( base ) {
                    const FloatVector4 c = at( base, u, v, g.w );
                    for ( int k = 0; k < 3; k++ )
                        rgb[k] *= wwSrgbToLinear( c[k] );
                    if ( f & PbrmMaterial::OpacityTexture )
                        a = c[3];
                }
                if ( m.tintEnabled ) {
                    const FloatVector4 tv = at( tint, u, v, g.w );
                    float k4[4];
                    for ( int c = 0; c < 4; c++ )
                        k4[c] = ( m.tintUseTexture[c] && tint ) ? tv[c] : m.tintMaskConst[c];
                    float sum = k4[0] + k4[1] + k4[2] + k4[3];
                    if ( m.tintOverlap == 0 && sum > 1.0f ) {
                        for ( float & k : k4 )
                            k /= sum;
                    } else if ( m.tintOverlap == 2 ) {
                        const float r = k4[0], gg = k4[1], bb = k4[2];
                        k4[1] = gg * ( 1.0f - r );
                        k4[2] = bb * ( 1.0f - r ) * ( 1.0f - gg );
                        k4[3] = k4[3] * ( 1.0f - r ) * ( 1.0f - gg ) * ( 1.0f - bb );
                    }
                    sum = k4[0] + k4[1] + k4[2] + k4[3];
                    for ( int k = 0; k < 3; k++ ) {
                        float mix = 1.0f - sum;
                        for ( int c = 0; c < 4; c++ )
                            mix += m.tintColor[c][k] * k4[c];
                        rgb[k] *= qMax( 0.0f, mix );
                    }
                }
                d[x] = qRgba( wwByte( wwLinearToSrgb( rgb[0] ) ), wwByte( wwLinearToSrgb( rgb[1] ) ),
                    wwByte( wwLinearToSrgb( rgb[2] ) ), wwByte( a ) );
            }
        }
        if ( !write( img, QStringLiteral( "c" ), s.colour ) )
            return false;
    }
    // slot 7: roughness, metallic, AO
    {
        const Grid g = gridOf( { rm.get() } );
        QImage img( g.w, g.h, QImage::Format_ARGB32 );
        for ( int y = 0; y < g.h; y++ ) {
            QRgb * d = reinterpret_cast<QRgb *>( img.scanLine( y ) );
            for ( int x = 0; x < g.w; x++ ) {
                const FloatVector4 r = at( rm, ( float( x ) + 0.5f ) / float( g.w ), ( float( y ) + 0.5f ) / float( g.h ), g.w );
                d[x] = qRgba( wwByte( ( f & PbrmMaterial::RmaosRoughness ) ? r[0] : m.roughness ),
                    wwByte( ( f & PbrmMaterial::RmaosMetallic ) ? r[1] : m.metallic ),
                    wwByte( ( f & PbrmMaterial::RmaosAo ) ? r[2] : m.ao ), 255 );
            }
        }
        if ( !write( img, QStringLiteral( "rmaos" ), s.rmaos ) )
            return false;
    }
    // the specular: sqrt(F0') and the weight. A v4/v5 document: weight 1, white, its own F0.
    {
        const bool v6 = m.specularV6;
        const bool wMap = v6 && ( f & PbrmMaterial::RmaosSpecularWeight ) && rm;
        const bool f0Map = !v6 && ( f & PbrmMaterial::RmaosF0 ) && rm;
        const float tintC[3] = { wwSrgbToLinear( m.specularTint[0] ), wwSrgbToLinear( m.specularTint[1] ),
            wwSrgbToLinear( m.specularTint[2] ) };
        const float f0c = v6 ? std::min( pbrmIorF0( m.specularIor ), 1.0f ) : pbrmDielectricF0( m );
        s.specDefault = !spSampled && !wMap && !f0Map && ( !v6 || m.specularWeight >= 1.0f )
            && tintC[0] >= 1.0f && tintC[1] >= 1.0f && tintC[2] >= 1.0f && std::fabs( f0c - 0.04f ) < 1.0e-4f;
        if ( s.specDefault ) {
            s.spec = QStringLiteral( "#ff333333" );    // sqrt(0.04) = 0.2 -> 51
            s.weight = QStringLiteral( "#ffffffff" );
        } else {
            const Grid g = gridOf( { ( wMap || f0Map ) ? rm.get() : nullptr, sp.get() } );
            QImage img( g.w, g.h, QImage::Format_ARGB32 ), wimg( g.w, g.h, QImage::Format_ARGB32 );
            for ( int y = 0; y < g.h; y++ ) {
                QRgb * d = reinterpret_cast<QRgb *>( img.scanLine( y ) );
                QRgb * dw = reinterpret_cast<QRgb *>( wimg.scanLine( y ) );
                for ( int x = 0; x < g.w; x++ ) {
                    const float u = ( float( x ) + 0.5f ) / float( g.w ), v = ( float( y ) + 0.5f ) / float( g.h );
                    const FloatVector4 r = at( rm, u, v, g.w ), sv = at( sp, u, v, g.w );
                    float w = v6 ? ( wMap ? r[3] : m.specularWeight ) : 1.0f;
                    w = qBound( 0.0f, w, 1.0f );
                    float lvl = f0c;
                    if ( v6 && ( f & PbrmMaterial::SpecularIorTexture ) && sp )
                        lvl = std::min( pbrmIorF0( sv[3] * m.specularIorMax ), 1.0f );
                    if ( f0Map )
                        lvl = qBound( 0.0f, r[3], 0.16f );
                    float t3[3] = { tintC[0], tintC[1], tintC[2] };
                    if ( v6 && ( f & PbrmMaterial::SpecularColorTexture ) && sp )
                        for ( int k = 0; k < 3; k++ )
                            t3[k] = wwSrgbToLinear( sv[k] );
                    int o[3];
                    for ( int k = 0; k < 3; k++ )
                        o[k] = wwByte( std::sqrt( qBound( 0.0f, ( v6 ? w : 1.0f ) * t3[k] * lvl, 1.0f ) ) );
                    d[x] = qRgba( o[0], o[1], o[2], 255 );
                    dw[x] = qRgba( wwByte( w ), wwByte( w ), wwByte( w ), 255 );
                }
            }
            if ( !write( img, QStringLiteral( "spec" ), s.spec ) || !write( wimg, QStringLiteral( "w" ), s.weight ) )
                return false;
        }
    }
    // slot 2: the emissive, or black
    if ( !emOn ) {
        s.emissive.clear();
        s.emissiveScale = 0.0f;
    } else {
        const Grid g = gridOf( { em.get() } );
        QImage img( g.w, g.h, QImage::Format_ARGB32 );
        for ( int y = 0; y < g.h; y++ ) {
            QRgb * d = reinterpret_cast<QRgb *>( img.scanLine( y ) );
            for ( int x = 0; x < g.w; x++ ) {
                const FloatVector4 e = at( em, ( float( x ) + 0.5f ) / float( g.w ), ( float( y ) + 0.5f ) / float( g.h ), g.w );
                const bool mapColour = em && !m.overrideEmissiveColor, mapMask = em && !m.overrideEmissiveMask;
                const float mask = mapMask ? e[3] : m.emissiveMask;
                int o[3];
                for ( int k = 0; k < 3; k++ )
                    o[k] = wwByte( wwLinearToSrgb( wwSrgbToLinear( mapColour ? e[k] : m.emissiveRGB[k] ) * mask ) );
                d[x] = qRgba( o[0], o[1], o[2], 255 );
            }
        }
        if ( !write( img, QStringLiteral( "e" ), s.emissive ) )
            return false;
        s.emissiveScale = m.emissiveIntensity;
    }
    // slot 1: the .pbrm's normal map as it is, when the stack can serve it (the bake's normal sheet is
    // the GEOMETRIC normal, channel 8, so this is for the lit passes only)
    if ( ( f & PbrmMaterial::NormalTexture ) && nif ) {
        const QString n = QStringLiteral( "textures\\" ) + m.normal.lookupPath;
        if ( !nif->findResourceFile( n, "textures", ".dds" ).isEmpty() )
            s.normal = n;
    }
    return true;
}

}    // namespace

'''

FAMILY_DECL_OLD = "\t\t\t\t\tQHash<int, float> lodmEmissiveScale;\n"
FAMILY_DECL_NEW = FAMILY_DECL_OLD + T('''/* IMPOSTORPBRM1: the per-bake folder holding the .pbrm law's sources
 * (alive until the bake ends), each pbr shape's slot 7 for the specular
 * passes, and whether a `_s` sheet is owed (a shape departs from the
 * default specular: weight 1, white, IOR 1.5). */
std::unique_ptr<QTemporaryDir> pbrmTmp;
std::vector<WwPbrmSpecSlot> pbrmSpecSlots;
bool pbrmSpec = false;
''', 5) + '\n'

READER_OLD = "\t\t\t\t\t\tint texturedShapes = 0, pbrShapes = 0;\n"
READER_NEW = T('''/* THE .pbrm ROUTE (IMPOSTORPBRM1): a shape with no usable .lodm that
 * resolves a .pbrm through the viewport's one resolver (io/pbrmresolve:
 * the direct name, the same-name sibling, the diffuse stem), read from
 * the loose root first and then the stack -- the mesh-LOD mask path's
 * order (src/lodgen.cpp ~1849-1869). Counted pbr-sourced here; its law
 * is evaluated after the loop, and a map it cannot read refuses it. */
std::vector<WwPbrmCardShape> pbrmShapes;
auto pbrmTry = [&]( int b, const QModelIndex & iShader, const QString & matName, const QString & diffuse ) {
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
        if ( nif && !nif->findResourceFile( rel, "materials", ".pbrm" ).isEmpty() )
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
    WwPbrmCardShape s;
    s.block = b;
    s.bsp = bsp;
    s.served = rr.path;
    s.served.replace( QChar( '\\\\' ), QChar( '/' ) );
    s.route = QLatin1String( pbrmRouteName( rr.route ) );
    s.m = rr.material;
    s.tree = bsp->isVertexAlphaAnimation ? 1 : 0;
    pbrmShapes.push_back( s );
    return true;
};
''', 6) + '\n' + READER_OLD

NONE_OLD = ("\t\t\t\t\t\t\t\tms << \"lodm \" << candidate << \" none \" << diffuse << \"\\n\";\n"
            "\t\t\t\t\t\t\t\tcontinue;\n")
NONE_NEW = ("\t\t\t\t\t\t\t\tms << \"lodm \" << candidate << \" none \" << diffuse << \"\\n\";\n"
            "\t\t\t\t\t\t\t\tif ( pbrmTry( b, iShader, matName, diffuse ) )\n"
            "\t\t\t\t\t\t\t\t\tpbrShapes++;\n"
            "\t\t\t\t\t\t\t\tcontinue;\n")
REJ_OLD = ("\t\t\t\t\t\t\t\tms << \"lodm \" << candidate << \" rejected \" << diffuse << \" \" << lm.error << \"\\n\";\n"
           "\t\t\t\t\t\t\t\tcontinue;\n")
REJ_NEW = ("\t\t\t\t\t\t\t\tms << \"lodm \" << candidate << \" rejected \" << diffuse << \" \" << lm.error << \"\\n\";\n"
           "\t\t\t\t\t\t\t\tif ( pbrmTry( b, iShader, matName, diffuse ) )\n"
           "\t\t\t\t\t\t\t\t\tpbrShapes++;\n"
           "\t\t\t\t\t\t\t\tcontinue;\n")
LODMPBR_OLD = ("\t\t\t\t\t\t\tif ( lm.pbr )\n"
               "\t\t\t\t\t\t\t\tpbrShapes++;\n"
               "\t\t\t\t\t\t\tms << \"lodm \" << candidate << \" \" << lm.family << \" \" << diffuse << \"\\n\";\n")
LODMPBR_NEW = ("\t\t\t\t\t\t\tif ( lm.pbr ) {\n"
               "\t\t\t\t\t\t\t\tpbrShapes++;\n"
               "\t\t\t\t\t\t\t\t// a pbr .lodm has no specular of its own: the default, F0 0.04 and weight 1\n"
               "\t\t\t\t\t\t\t\tpbrmSpecSlots.push_back( { bsp, QStringLiteral( \"#ff333333\" ), QStringLiteral( \"#ffffffff\" ),\n"
               "\t\t\t\t\t\t\t\t\tbsp->wwTextureOverride.value( 7 ) } );\n"
               "\t\t\t\t\t\t\t}\n"
               "\t\t\t\t\t\t\tms << \"lodm \" << candidate << \" \" << lm.family << \" \" << diffuse << \"\\n\";\n")

AFTER_OLD = ("\t\t\t\t\t\tfamilyPbr = ( texturedShapes > 0 && pbrShapes == texturedShapes );\n"
             "\t\t\t\t\t\tif ( lodmShapes )\n"
             "\t\t\t\t\t\t\tskope->ogl->update();\n")
AFTER_NEW = ("\t\t\t\t\t\tfamilyPbr = ( texturedShapes > 0 && pbrShapes == texturedShapes );\n" + T('''/* The .pbrm shapes' sources, only when they make the card pbr: a MIXED
 * model stays legacy exactly as before (DONE.md step 7 design 2) and its
 * sidecar names the .pbrm shapes whose data went unused. */
if ( familyPbr && !pbrmShapes.empty() ) {
    pbrmTmp = std::make_unique<QTemporaryDir>();
    const QString sub = QStringLiteral( "wwpbrmcard%1" ).arg( ++wwPbrmBakeSerial );
    const QString texDir = pbrmTmp->path() + "/textures/" + sub;
    bool okAll = pbrmTmp->isValid() && QDir().mkpath( texDir );
    for ( auto & s : pbrmShapes ) {
        if ( !okAll ) {
            if ( s.refusal.isEmpty() )
                s.refusal = QStringLiteral( "not-evaluated" );
            continue;
        }
        if ( !wwPbrmCardSources( nif, looseRoot, texDir, QStringLiteral( "textures\\\\" ) + sub,
                QString::number( s.block ), s, s.refusal ) )
            okAll = false;
    }
    if ( !okAll ) {
        familyPbr = false;
    } else {
        /* THE `textures` FOLDER ITSELF IS THE ROOT (ImpostorDraw::
         * registerLooseSheets says why), and through the model. */
        const_cast< NifModel * >( nif )->addResourceRoot( QDir::toNativeSeparators( pbrmTmp->path() + "/textures" ) );
        for ( auto & s : pbrmShapes ) {
            s.bsp->wwTextureOverride.insert( 0, s.colour );
            if ( !s.normal.isEmpty() )
                s.bsp->wwTextureOverride.insert( 1, s.normal );
            s.bsp->wwTextureOverride.insert( 2, s.emissive );    // empty = black: a pbr set's emissive is its own or none
            s.bsp->wwTextureOverride.insert( 7, s.rmaos );
            lodmEmissiveScale.insert( s.block, s.emissiveScale );
            pbrmSpecSlots.push_back( { s.bsp, s.spec, s.weight, s.rmaos } );
            if ( !s.specDefault )
                pbrmSpec = true;
        }
        lodmShapes++;
    }
}
for ( const auto & s : pbrmShapes ) {
    ms << "pbrm " << s.served << " " << s.route << " "
        << ( familyPbr ? "used" : s.refusal.isEmpty() ? "unused" : "refused" ) << " tree " << s.tree << "\\n";
    if ( !s.refusal.isEmpty() )
        ms << "pbrmrefused " << s.served << " " << s.refusal << "\\n";
}
if ( !familyPbr )
    pbrmSpec = false;
''', 6) + "\n"
             "\t\t\t\t\t\tif ( lodmShapes )\n"
             "\t\t\t\t\t\t\tskope->ogl->update();\n")

SHEET_OLD = "\t\t\t\t\t\temissive.fill( qRgba( 0, 0, 0, 255 ) );\t\t\t// the emissive sheet: black, and opaque - it ships as BC1\n"
SHEET_NEW = SHEET_OLD + T('''/* THE SPECULAR SHEET `_s` (IMPOSTORPBRM1), only when a shape departs
 * from the default: RGB sqrt(F0'), A the specular weight; the empty texel
 * is the default, sqrt(0.04) = 51 and weight 1. Slot 7 is pointed at
 * each shape's spec source, then its weight source, for one channel-10
 * pass each, and restored. */
QImage specS;
if ( pbrmSpec ) {
    specS = QImage( S_W, S_H, QImage::Format_ARGB32 );
    specS.fill( qRgba( 51, 51, 51, 255 ) );
}
auto pbrmSlot7 = [&]( int which ) {
    for ( const auto & p : pbrmSpecSlots )
        p.bsp->wwTextureOverride.insert( 7, which == 0 ? p.spec : which == 1 ? p.weight : p.restore );
};
''', 6) + '\n'

NOAA_OLD = ("\t\t\t\t\t\t\t\tQImage tA, tN, tD, tS, tM, tE;\n")
NOAA_NEW = ("\t\t\t\t\t\t\t\tQImage tA, tN, tD, tS, tM, tE, tP, tW;\n")
NOAA2_OLD = ("\t\t\t\t\t\t\t\t\ttE = frameOf( channel( 13 ), ox, oy );\t\t// the emissive: a .lodm's texture raw, or the vanilla glow rule\n")
NOAA2_NEW = NOAA2_OLD + T('''if ( pbrmSpec ) {
    pbrmSlot7( 0 );
    tP = frameOf( channel( 10 ), ox, oy );    // the specular: sqrt(F0')
    pbrmSlot7( 1 );
    tW = frameOf( channel( 10 ), ox, oy );    // the specular weight
    pbrmSlot7( 2 );
}
''', 9) + '\n'

AA_OLD = ("\t\t\t\t\t\t\t\t\tconst QImage s13 = channelOff( 13, RW, RH, halfH, ox, oy );\n"
          "\t\t\t\t\t\t\t\t\tQImage * outs[6] = { &tA, &tN, &tD, &tS, &tM, &tE };\n")
AA_NEW = ("\t\t\t\t\t\t\t\t\tconst QImage s13 = channelOff( 13, RW, RH, halfH, ox, oy );\n" + T('''QImage s10p, s10w;
if ( pbrmSpec ) {
    pbrmSlot7( 0 );
    s10p = channelOff( 10, RW, RH, halfH, ox, oy );
    pbrmSlot7( 1 );
    s10w = channelOff( 10, RW, RH, halfH, ox, oy );
    pbrmSlot7( 2 );
}
const int nch = pbrmSpec ? 6 : 4;
''', 9) + "\n"
          "\t\t\t\t\t\t\t\t\tQImage * outs[8] = { &tA, &tN, &tD, &tS, &tM, &tE, &tP, &tW };\n")
AA2_OLD = ("\t\t\t\t\t\t\t\t\tconst QImage * chans[4] = { &s9, &s10, &s11, &s13 };\n"
           "\t\t\t\t\t\t\t\t\tQImage * chOut[4] = { &tD, &tS, &tM, &tE };\n")
AA2_NEW = ("\t\t\t\t\t\t\t\t\tconst QImage * chans[6] = { &s9, &s10, &s11, &s13, &s10p, &s10w };\n"
           "\t\t\t\t\t\t\t\t\tQImage * chOut[6] = { &tD, &tS, &tM, &tE, &tP, &tW };\n")
AA3_OLD = "\t\t\t\t\t\t\t\t\t\t\tint ch[4][3] = {};\n"
AA3_NEW = "\t\t\t\t\t\t\t\t\t\t\tint ch[6][3] = {};\n"
AA4_OLD = ("\t\t\t\t\t\t\t\t\t\t\t\tfor ( int c = 0; c < 4; c++ ) {\n"
           "\t\t\t\t\t\t\t\t\t\t\t\t\tconst QRgb pc = chans[c]->pixel( sx, sy );\n")
AA4_NEW = ("\t\t\t\t\t\t\t\t\t\t\t\tfor ( int c = 0; c < nch; c++ ) {\n"
           "\t\t\t\t\t\t\t\t\t\t\t\t\tconst QRgb pc = chans[c]->pixel( sx, sy );\n")
AA5_OLD = ("\t\t\t\t\t\t\t\t\t\t\tfor ( int c = 0; c < 4; c++ )\n"
           "\t\t\t\t\t\t\t\t\t\t\t\tchOut[c]->setPixel(")
AA5_NEW = ("\t\t\t\t\t\t\t\t\t\t\tfor ( int c = 0; c < nch; c++ )\n"
           "\t\t\t\t\t\t\t\t\t\t\t\tchOut[c]->setPixel(")

COMP_OLD = ("\t\t\t\t\t\t\t\t\t\temissive.setPixel( i * tw + x, j * th + y,\n"
            "\t\t\t\t\t\t\t\t\t\t\tqRgba( unp( qRed( pe ) ), unp( qGreen( pe ) ), unp( qBlue( pe ) ), 255 ) );\n")
COMP_NEW = COMP_OLD + T('''if ( pbrmSpec ) {
    const QRgb pp = tP.pixel( x, y ), pw = tW.pixel( x, y );
    specS.setPixel( i * tw + x, j * th + y,
        qRgba( unp( qRed( pp ) ), unp( qGreen( pp ) ), unp( qBlue( pp ) ), unp( qRed( pw ) ) ) );
}
''', 10) + '\n'

SAVE_OLD = ("\t\t\t\t\t\temissive.save( outDir + \"/\" + base + QStringLiteral( \"_oct\" ) + QLatin1String( lodmEmissiveSuffix( familyPbr ) )\n"
            "\t\t\t\t\t\t\t+ QStringLiteral( \".png\" ) );\n")
SAVE_NEW = SAVE_OLD + ("\t\t\t\t\t\t// the fifth, a pbr card's specular, only when owed: _oct_s.png\n"
                       "\t\t\t\t\t\tif ( pbrmSpec )\n"
                       "\t\t\t\t\t\t\tspecS.save( outDir + \"/\" + base + QStringLiteral( \"_oct_s.png\" ) );\n")

CLASS_OLD = "\t\t\t\t\t\tms << \"class \" << tw << \" \" << th << \"\\n\";\n"
CLASS_NEW = CLASS_OLD + "\t\t\t\t\t\tms << \"specular \" << ( pbrmSpec ? \"_s\" : \"none\" ) << \"\\n\";\n"

INC_OLD = '#include "io/lodmfile.h"\n'
INC_NEW = INC_OLD + '#include "io/pbrmfile.h"\n#include "io/pbrmresolve.h"\n#include "libfo76utils/src/ddstxt16.hpp"\n'
INC2_OLD = '#include <memory>\n'
INC2_NEW = INC2_OLD + '#include <QTemporaryDir>\n#include <QtEndian>\n#include <vector>\n'

CW_OLD = "NifSkope * NifSkope::createWindow( const QString & fname, bool background )\n"
CW_NEW = T(HELPERS, 0) + CW_OLD

b = open(P, 'rb').read()
assert b.count(b'\r') == 0
s = b.decode('utf-8')
for o, n in [(INC_OLD, INC_NEW), (INC2_OLD, INC2_NEW), (CW_OLD, CW_NEW), (FAMILY_DECL_OLD, FAMILY_DECL_NEW),
             (READER_OLD, READER_NEW), (NONE_OLD, NONE_NEW), (REJ_OLD, REJ_NEW), (LODMPBR_OLD, LODMPBR_NEW),
             (AFTER_OLD, AFTER_NEW), (SHEET_OLD, SHEET_NEW), (NOAA_OLD, NOAA_NEW), (NOAA2_OLD, NOAA2_NEW),
             (AA_OLD, AA_NEW), (AA2_OLD, AA2_NEW), (AA3_OLD, AA3_NEW), (AA4_OLD, AA4_NEW), (AA5_OLD, AA5_NEW),
             (COMP_OLD, COMP_NEW), (SAVE_OLD, SAVE_NEW), (CLASS_OLD, CLASS_NEW)]:
    assert s.count(o) == 1, (o[:70], s.count(o))
    s = s.replace(o, n)
out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('patched nifskope_ui.cpp: +%d bytes' % (len(out) - len(b)))
