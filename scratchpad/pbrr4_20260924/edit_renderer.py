p = r'E:\Projects\NifskopeWildWastelandEdition\src\gl\renderer.cpp'
s = open(p, 'rb').read().decode()


def rep(old, new):
    global s
    assert s.count(old) == 1, old[:80]
    s = s.replace(old, new)


rep("""		struct SlotBind
		{
			const char * uniform;
			const char * slot;
			const PbrmMaterial::Slot * s;
			quint32 bit;
			QString fallback;
		};
		const SlotBind binds[] = {
			{ "BaseMap", "baseColor", &m.baseColor, PbrmMaterial::BaseColorTexture, white },
			{ "NormalMap", "normal", &m.normal, PbrmMaterial::NormalTexture, ::default_n },
			{ "RmaosMap", "rmaos", &m.rmaos, PbrmMaterial::RmaosTexture, white },
			{ "EmissiveMap", "emissive", &m.emissive, PbrmMaterial::EmissiveTexture, black },
			// v6 (lane PBRR3): the specular colour map -- RGB the sRGB tint (bit 25), A the IOR over
			// [0, iorMax] (bit 30). Sampled under either bit, so either makes it required.
			{ "SpecColorMap", "specularColor", &m.specularColor,
				quint32( PbrmMaterial::SpecularColorTexture | PbrmMaterial::SpecularIorTexture ), white },
		};
		for ( const SlotBind & sb : binds ) {
			const bool required = ( m.features & sb.bit ) && sb.s->enabled && sb.s->pathValid;""",
"""		struct SlotBind
		{
			const char * uniform;
			const char * slot;
			const PbrmMaterial::Slot * s;
			quint32 bit;
			QString fallback;
			bool sampled;	//!< the shader reads the texture (for the feature-bit slots: the bit)
		};
		// the tint mask (lane PBRR4) has no feature bit: it is sampled when any mask channel reads it
		const bool tintSampled = m.tintEnabled
			&& ( m.tintUseTexture[0] || m.tintUseTexture[1] || m.tintUseTexture[2] || m.tintUseTexture[3] );
		const SlotBind binds[] = {
			{ "BaseMap", "baseColor", &m.baseColor, PbrmMaterial::BaseColorTexture, white, false },
			{ "NormalMap", "normal", &m.normal, PbrmMaterial::NormalTexture, ::default_n, false },
			{ "RmaosMap", "rmaos", &m.rmaos, PbrmMaterial::RmaosTexture, white, false },
			{ "EmissiveMap", "emissive", &m.emissive, PbrmMaterial::EmissiveTexture, black, false },
			// v6 (lane PBRR3): the specular colour map -- RGB the sRGB tint (bit 25), A the IOR over
			// [0, iorMax] (bit 30). Sampled under either bit, so either makes it required.
			{ "SpecColorMap", "specularColor", &m.specularColor,
				quint32( PbrmMaterial::SpecularColorTexture | PbrmMaterial::SpecularIorTexture ), white, false },
			{ "TintMaskMap", "tintMask", &m.tintMask, 0u, black, tintSampled },
		};
		for ( const SlotBind & sb : binds ) {
			const bool required = ( sb.bit ? ( m.features & sb.bit ) != 0 : sb.sampled )
				&& sb.s->enabled && sb.s->pathValid;""")
rep("""		bindPath( "SpecColorMap", QString(), white, false );
	}
""", """		bindPath( "SpecColorMap", QString(), white, false );
		bindPath( "TintMaskMap", QString(), black, false );
	}
""")
rep("""	prog->uni3f( "pbrEmissiveColor", m.emissiveRGB[0], m.emissiveRGB[1], m.emissiveRGB[2] );
	prog->uni1f( "pbrEmissiveIntensity", m.emissiveIntensity );
""", """	prog->uni3f( "pbrEmissiveColor", m.emissiveRGB[0], m.emissiveRGB[1], m.emissiveRGB[2] );
	prog->uni1f( "pbrEmissiveIntensity", m.emissiveIntensity );
	// lane PBRR4 (docs s3.2 items 4, 7, 9): emission replace semantics, tint masks, composition
	prog->uni1f( "pbrEmissiveMask", m.emissiveMask );
	prog->uni1b( "pbrEmissiveMapColor", !m.overrideEmissiveColor );
	prog->uni1b( "pbrEmissiveMapMask", !m.overrideEmissiveMask );
	{
		int useTex = 0;
		for ( int c = 0; c < 4; c++ )
			useTex |= ( m.tintUseTexture[c] ? 1 << c : 0 );
		prog->uni1b( "pbrTintEnabled", m.tintEnabled );
		prog->uni1i( "pbrTintUseTex", m.tintEnabled ? useTex : 0 );
		prog->uni4f( "pbrTintMasks", FloatVector4( m.tintMaskConst[0], m.tintMaskConst[1], m.tintMaskConst[2],
			m.tintMaskConst[3] ) );
		for ( int c = 0; c < 4; c++ )
			prog->uni3f_l( prog->uniLocation( "pbrTintColors[%d]", c ), m.tintColor[c][0], m.tintColor[c][1],
				m.tintColor[c][2] );
		prog->uni1i( "pbrTintMode", m.tintOverlap );
	}
	// -1 = the NIF alpha property: derived (non-.pbrm) shapes, and the red "nocomp"
	const int r4Red = wwR4RedBits();
	const int composition = ( lsp->pbrmValid && !( r4Red & 16 ) ) ? std::clamp( m.composition, 0, 7 ) : -1;
	prog->uni1i( "pbrComposition", composition );
	prog->uni1f( "pbrGlobalOpacity", m.globalOpacity );
	prog->uni1f( "pbrAlphaThreshold", m.alphaThreshold );
	prog->uni1b( "pbrAlphaConst", m.alphaSourceConstant );
	prog->uni1i( "r4Red", r4Red );
""")
rep("""	if ( mesh->translucent && scene->hasOption( Scene::DoBlending ) ) {
		glEnable( GL_BLEND );""", """	if ( composition >= 0 ) {
		/* The .pbrm composition replaces the NIF alpha property (lane PBRR4; FO4CS
		 * PBRM.cpp:679-705 rewrites the NiAlphaProperty flags from it; the blend
		 * functions are the editor's, ED:5247-5260). 0/1 draw opaque (the test is the
		 * shader's discard), the rest blend. Draw order is not re-sorted (owed). */
		prog->uni1i( "alphaFlags", 0 );
		prog->uni1f( "alphaThreshold", m.alphaThreshold );
		if ( composition >= 2 && scene->hasOption( Scene::DoBlending ) ) {
			glEnable( GL_BLEND );
			switch ( composition ) {
			case 3:	// Premultiplied
				fn->glBlendFuncSeparate( GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
				break;
			case 4:	// Additive
				fn->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE );
				break;
			case 5:	// Multiply
				fn->glBlendFuncSeparate( GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE );
				break;
			default:	// Alpha Blend, and Transmission / Water until the merge
				fn->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
				break;
			}
		} else {
			glDisable( GL_BLEND );
		}
	} else if ( mesh->translucent && scene->hasOption( Scene::DoBlending ) ) {
		glEnable( GL_BLEND );""")
rep("""	const bool pbrmDepthTest = lsp->pbrmValid || mesh->depthTest;
	const bool pbrmDepthWrite = lsp->pbrmValid ? !mesh->translucent : ( mesh->depthWrite && !mesh->translucent );""",
"""	const bool pbrmDepthTest = lsp->pbrmValid || mesh->depthTest;
	// lane PBRR4: a .pbrm composition's Transparency/Depth depthWrite (default: composition <= 1)
	const bool pbrmDepthWrite = composition >= 0 ? m.depthWrite
		: lsp->pbrmValid ? !mesh->translucent : ( mesh->depthWrite && !mesh->translucent );""")
open(p, 'wb').write(s.encode())
print("renderer ok")
