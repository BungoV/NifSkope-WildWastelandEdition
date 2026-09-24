import io, sys

p = 'src/lodgen.cpp'
s = open(p, 'r', encoding='utf-8', newline='').read()


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:70], s.count(a))
    s = s.replace(a, b)


rep("""bool lodgenBakeVtTile( const EsmWorld & world, const QString & dataRoot,
	LodgenBakeCaches & bc, const LodgenCoverOptions & coverOpts,
	LodgenVtLandCache & landCache, int cellX0, int cellY0, int dim,
	int content, int border, LodgenVtStage & out )
{""",
"""bool lodgenBakeVtTile( const EsmWorld & world, const QString & dataRoot,
	LodgenBakeCaches & bc, const LodgenCoverOptions & coverOpts,
	LodgenVtLandCache & landCache, LodgenVtMaskCache & maskCache, bool wantEmissive,
	int cellX0, int cellY0, int dim,
	int content, int border, LodgenVtStage & out )
{""")

rep("""	out.colour.assign( size_t( S ) * S, 0xFF808080U );
	out.msn.assign( size_t( S ) * S, LODGEN_MSN_FLAT );
	out.data.assign( size_t( S ) * S, 0x00FFFFFFU );
	out.height.assign( size_t( S ) * S, 32767 );
	out.cover = false;""",
"""	out.colour.assign( size_t( S ) * S, 0xFF808080U );
	out.msn.assign( size_t( S ) * S, LODGEN_MSN_FLAT );
	out.data.assign( size_t( S ) * S, 0x00FFFFFFU );
	/* The mask's default is the honest unknown: FULLY ROUGH (R 255), metallic 0,
	 * AO open (B 255), cover 0. A texel with no land is not a mirror. */
	out.mask.assign( size_t( S ) * S, 0x00FF00FFU );
	if ( wantEmissive )
		out.emissive.assign( size_t( S ) * S, 0xFF000000U );
	out.height.assign( size_t( S ) * S, 32767 );
	out.cover = false;""")

rep("""			FloatVector4 color( 0.5f, 0.5f, 0.5f, 1.0f );
			int coverByte = 0;
			float coverTintD = 0.0f;
			float coverTint[3] = { 0.0f, 0.0f, 0.0f };
			if ( haveLand[ci] ) {
				const EsmLand & land = cells[ci];
				const float clx = lx - float( cx ) * 4096.0f;
				const float cly = ly - float( cy ) * 4096.0f;
				const int q = ( cly >= 2048.0f ? 2 : 0 ) + ( clx >= 2048.0f ? 1 : 0 );
				const float qx = ( clx - ( q & 1 ? 2048.0f : 0.0f ) ) / 2048.0f;
				const float qy = ( cly - ( q & 2 ? 2048.0f : 0.0f ) ) / 2048.0f;
				auto sampleLtex = [&]( quint32 ltex ) -> FloatVector4 {""",
"""			FloatVector4 color( 0.5f, 0.5f, 0.5f, 1.0f );
			/* THE MASK, through the SAME blend as the colour (2.2). Roughness
			 * starts fully rough and metallic at zero, which is what a texel
			 * with no resolvable material keeps. */
			float rough = 1.0f, metal = 0.0f;
			float emisRgb[3] = { 0.0f, 0.0f, 0.0f };
			int coverByte = 0;
			float coverTintD = 0.0f;
			float coverTint[3] = { 0.0f, 0.0f, 0.0f };
			if ( haveLand[ci] ) {
				const EsmLand & land = cells[ci];
				const float clx = lx - float( cx ) * 4096.0f;
				const float cly = ly - float( cy ) * 4096.0f;
				const int q = ( cly >= 2048.0f ? 2 : 0 ) + ( clx >= 2048.0f ? 1 : 0 );
				const float qx = ( clx - ( q & 1 ? 2048.0f : 0.0f ) ) / 2048.0f;
				const float qy = ( cly - ( q & 2 ? 2048.0f : 0.0f ) ) / 2048.0f;
				/* The mask maps are sampled at the SAME world point, the same
				 * 2,048-unit tiling and the same footprint-chosen mip as the
				 * diffuse, or the roughness would describe a different patch of
				 * ground from the colour beside it. */
				auto sampleMaskChannel = [&]( const QString & path, int channel,
					bool * got ) -> float {
					if ( got )
						*got = false;
					if ( path.isEmpty() )
						return 0.0f;
					const DDSTexture16 * tex = lodgenCachedTexture( bc, dataRoot, path );
					if ( !tex )
						return 0.0f;
					float u = std::fmod( wx / TILE, 1.0f );
					float v = std::fmod( wy / TILE, 1.0f );
					if ( u < 0.0f ) u += 1.0f;
					if ( v < 0.0f ) v += 1.0f;
					const float texelWorld = TILE / float( tex->getWidth() );
					const float mip = qBound( 0.0f,
						std::log2( qMax( 1.0f, upt / texelWorld ) ),
						float( tex->getMaxMipLevel() ) );
					if ( got )
						*got = true;
					return tex->getPixelT( u, v, mip )[channel];
				};
				auto layerRough = [&]( const LodgenMaterialMask & m ) -> float {
					if ( !m.haveRoughnessMap )
						return m.roughnessConst;
					bool got = false;
					const float c = sampleMaskChannel( m.roughnessTex, m.roughnessChannel, &got );
					if ( !got )
						return m.roughnessConst;     // named but unreadable: the constant, counted in the census
					// THE INVERSION, through the one gloss law (lodgen.h)
					return m.invertRoughness
						? 1.0f - lodgenLegacyGloss( m.glossScale, c )
						: qBound( 0.0f, c, 1.0f );
				};
				auto layerMetal = [&]( const LodgenMaterialMask & m ) -> float {
					if ( !m.haveMetallicMap )
						return m.metallicConst;      // legacy: 0, never a guess
					bool got = false;
					const float c = sampleMaskChannel( m.metallicTex, m.metallicChannel, &got );
					return got ? qBound( 0.0f, c, 1.0f ) : m.metallicConst;
				};
				auto layerEmis = [&]( const LodgenMaterialMask & m, float * rgb ) {
					rgb[0] = rgb[1] = rgb[2] = 0.0f;
					if ( !m.haveEmissive || m.emissiveTex.isEmpty() )
						return;
					const DDSTexture16 * tex = lodgenCachedTexture( bc, dataRoot, m.emissiveTex );
					if ( !tex )
						return;
					float u = std::fmod( wx / TILE, 1.0f );
					float v = std::fmod( wy / TILE, 1.0f );
					if ( u < 0.0f ) u += 1.0f;
					if ( v < 0.0f ) v += 1.0f;
					const float texelWorld = TILE / float( tex->getWidth() );
					const float mip = qBound( 0.0f,
						std::log2( qMax( 1.0f, upt / texelWorld ) ),
						float( tex->getMaxMipLevel() ) );
					const FloatVector4 e = tex->getPixelT( u, v, mip );
					for ( int k = 0; k < 3; k++ )
						rgb[k] = e[k];
				};
				auto sampleLtex = [&]( quint32 ltex ) -> FloatVector4 {""")

rep("""				const quint32 baseTex = land.baseTex[q] ? land.baseTex[q] : dominantBase;
				if ( baseTex )
					color = sampleLtex( baseTex );
				int nLayers = 0;
				for ( const EsmLandLayer & layer : land.layers[q] ) {
					const float fx = qBound( 0.0f, qx * 16.0f, 15.999f );""",
"""				const quint32 baseTex = land.baseTex[q] ? land.baseTex[q] : dominantBase;
				if ( baseTex ) {
					color = sampleLtex( baseTex );
					const LodgenMaterialMask & bm =
						maskCache.resolve( world, dataRoot, baseTex ).mat;
					rough = layerRough( bm );
					metal = layerMetal( bm );
					if ( wantEmissive )
						layerEmis( bm, emisRgb );
				}
				int nLayers = 0;
				for ( const EsmLandLayer & layer : land.layers[q] ) {
					const float fx = qBound( 0.0f, qx * 16.0f, 15.999f );""")

rep("""					const FloatVector4 lc = sampleLtex( layer.ltex ? layer.ltex : dominantBase );
					color = color + ( lc - color ) * qBound( 0.0f, a, 1.0f );
				}
				if ( doCover ) {
					const LodgenVtQuadCover & qc = quadCover[ci * 4 + q];""",
"""					const quint32 lform = layer.ltex ? layer.ltex : dominantBase;
					const FloatVector4 lc = sampleLtex( lform );
					const float aw = qBound( 0.0f, a, 1.0f );
					color = color + ( lc - color ) * aw;
					/* THE SAME BLEND, on the same operands, in the same order --
					 * the layer opacities, over the base, un-renormalised. A
					 * different composite for the mask would put the roughness of
					 * one material on the colour of another. VCLR is NOT applied
					 * (it is the artist's shading of the ground's COLOUR) and
					 * neither is the grass tint. */
					const LodgenMaterialMask & lm = maskCache.resolve( world, dataRoot, lform ).mat;
					rough = rough + ( layerRough( lm ) - rough ) * aw;
					metal = metal + ( layerMetal( lm ) - metal ) * aw;
					if ( wantEmissive ) {
						float le[3];
						layerEmis( lm, le );
						for ( int k = 0; k < 3; k++ )
							emisRgb[k] = emisRgb[k] + ( le[k] - emisRgb[k] ) * aw;
					}
				}
				if ( doCover ) {
					const LodgenVtQuadCover & qc = quadCover[ci * 4 + q];""")

rep("""			out.data[size_t( j ) * S + i] =
				( quint32( coverByte ) << 24 ) | ( ao8 << 16 ) | ( wet8 << 8 ) | sho8;""",
"""			out.data[size_t( j ) * S + i] =
				( quint32( coverByte ) << 24 ) | ( ao8 << 16 ) | ( wet8 << 8 ) | sho8;

			/* THE MASK SHEET: R roughness, G metallic, B the SAME sky AO the
			 * retired data sheet carried in its R, A ground cover. Wetness and
			 * shore proximity are gone -- shore is a runtime subtraction from
			 * the .lodl water planes and wetness is a close-up effect. */
			const quint32 r8 = quint32( qBound( 0.0f, rough * 255.0f + 0.5f, 255.0f ) );
			const quint32 m8 = quint32( qBound( 0.0f, metal * 255.0f + 0.5f, 255.0f ) );
			out.mask[size_t( j ) * S + i] =
				( quint32( coverByte ) << 24 ) | ( r8 << 16 ) | ( m8 << 8 ) | ao8;
			if ( wantEmissive ) {
				auto e8 = []( float f ) {
					return quint32( qBound( 0.0f, f * 255.0f + 0.5f, 255.0f ) );
				};
				out.emissive[size_t( j ) * S + i] = 0xFF000000U
					| ( e8( emisRgb[0] ) << 16 ) | ( e8( emisRgb[1] ) << 8 ) | e8( emisRgb[2] );
			}""")

open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))
