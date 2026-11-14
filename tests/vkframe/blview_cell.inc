/* lane VKWATER3 (plan row G4 by draws): the non-bindless cell draws. GL's own fo4_cell / fo4_cellcsm on the app's base
 * variant (fo4_default.vert + fo4_cell.frag@WW_CELL_FOGLEAK+WW_FRAME_UBO linked from the HLSL SPIR-V set as wwSpvAttach links
 * it) draws a sweep of 6 shapes one at a time (bsshape.cpp's single draw: glDrawElementsBaseVertex per command) into the
 * shared targets; then objCellShade (the app's crossing for one shape) draws the same 6 through the Vulkan twins
 * (objcell_*.spv.h) into the same targets, one crossing a shape.
 * Fixture: one VBO in the app's ShapeData layout (glcontext.cpp: the arrays one after the other; position 3, colour 4,
 * normal 3, tangent 3, bitangent 3, uv 2), as floats (stride 0, every array past position at a non-zero offset) and as
 * VTXPACK1 halves (n 3 at an 8-byte stride), two sub-shapes (base vertex 0 / 36), one 16-bit index buffer; draws 2 and 3
 * are two commands each. Maps: the texture cache's BC twins (objTexMake) with their sampler states: BaseMap DXT5 128,
 * NormalMap DXT1 32, SpecularMap BC5 signed (R,G,0,1: specMap.g is the gloss), GlowMap DXT1 64.
 * Seeding: draw i's state from mt19937( 101 + i ): the sun (off on every third draw), its direction and colour, the
 * specular map (on even draws), strength and gloss, glow (every third draw), the uv scale and offset, a colour override
 * (draw 4), the model-view matrix; the DALC ambient on every draw; the float / half layout alternating; draw 3 as the SSR
 * scene pass (cellProbe 60: the glow's share into target 1, cellEmitOut).
 * BAR (stated before the first run, never widened): depth as the shade bar (DepthCmp EXACT or BOUND). Colour, both RGBA16F
 * targets, over the pixels both sides cover: EXACT, or BOUND |Vulkan - GL| <= 4e-3 * max(1, |GL|) per channel (a NaN on
 * one side only is over). Lit: >= 2000 pixels both cover, GL's rgb non-zero on >= 90 % of them (added after run 1, which
 * showed target 1 never written: >= 200 of them with GL's target 1 non-zero). The crossing: 6 runs,
 * 0 failed, 6 cells, 24 twin images, objCellShade refused none.
 * Twin 3 (`BLVIEW cellcsm`): the same with the sun cascades on (ussOn, ussCsmOn, cascade 0) reading a shared D16 2D array
 * (the colour gate's 32-texel checker of 0.25 / 1.0, LINEAR, compare LEQUAL) on csmMap's unit; the map probe: csmForce 1
 * moves GL's colour on >= 20 % of the pixels GL covers.
 * Reds (WW_VKVIEW_OBJCELL_RED through objCellRed; each must FAIL against GL and move the Vulkan colour off the Vulkan
 * base): offset (every attribute read from offset 0), half (halves read as 16-bit unorm), swz (the twins' swizzle dropped),
 * key (each twin the one made before it). The BC4 pass (`BLVIEW cell bc4`, run 2 showed swz testing nothing: BC5's R,G,0,1
 * is the identity view): the same sweep with BC4 texs[4] (R,R,R,1) as SpecularMap, the same bar, the red swz there and only
 * there (red key cannot run there: texs[4]'s twin-before is texs[3]'s first, gone once frame 1 replaced it).
 * Twin 4 (`BLVIEW pbrm`): GL's pbrm_default (fo4_default.vert + pbrm_default.frag, the same base variant) against
 * objcell_pbrm_ps: maps BaseMap, NormalMap, RmaosMap, EmissiveMap; seeded per draw: the base colour, roughness, metal, F0,
 * the feature bits (base / normal / RMAOS / emissive maps), the emission, Legacy or Lookdev (sceneMode 0 / 2, the DALC axes),
 * the view transform (0 / 1 / 2), the tint masks (draw 5), the uv, the model-view. The same bar; target 1 is not written by
 * this variant (no WW_CELLLIGHTS: it writes target 0 only, so its frame is the colour gate's one-target frame), so its glow
 * line is not asked. Reds WW_VKVIEW_OBJPBRM_RED, the same four; swz on its BC4
 * pass (texs[4] as RmaosMap).
 * Refusals (each must refuse, nothing drawn, with its reason): a GL-only RGBA8 texture on BaseMap ("no twin"); a
 * normalised byte colour attribute ("not 1 to 4 floats or halves").
 * Variants (lane VKWATER3 11-14, `vars`; the same sweep, the same bar):
 * 1 lights: the cell's placed lights, a 3-light RGBA32F texel buffer on cellLights (8 texels a light: an omni, a spot, a
 *   non-specular omni, in view space = world as cellRow is the identity), draw i walks 3 - i % 3 of them; the crossing copies
 *   the buffer into its twin. Red `light` (5): the copy from one texel on. Light probe: the lights moved off GL's colour on
 *   >= 20 % of the covered pixels (cellLightCount 0 against the lit sweep).
 * 2 skin: fo4_default.vert's GPU skinning (doSkinning, boneWeights at locations 5 / 6, two bones a vertex, 6 distinct bone
 *   transforms in skinningUniforms). Red `bone` (6): the bone transforms one bone on. Skin probe: doSkinning 0 moves GL's
 *   depth on >= 20 % of the covered pixels.
 * 4 alpha: the alpha test and blend (alphaFlags per draw: '>' 0.5, '>' + blend, blend alone, '>=' 0.5, none, '<' + blend;
 *   GL_BLEND SRC_ALPHA / ONE_MINUS_SRC_ALPHA on the blended draws) over the DXT5 BaseMap's alpha. Red `blend` (7): GL's
 *   factors read swapped. Alpha probe: alphaFlags 0 everywhere moves GL's colour or depth on >= 20 % of the covered pixels.
 * Twins 5 / 6 (`BLVIEW pbrmcell`, `pbrmcellcsm`): pbrm_cell / pbrm_cellcsm (pbrm_default.frag with WW_CELLLIGHTS, and the
 * cascades) against objcell_pbrmcell_ps / objcell_pbrmcellcsm_ps, the pbrm state, two targets, always with the lights. */
bool cellGate( const std::string & spirvDir, int twin, const GLuint maps[4], unsigned reds = 15u, const char * label = nullptr, unsigned vars = 0u )
{
	using namespace WwVkPresent;
	static const char * twinTag[8] = { "", "", "cell", "cellcsm", "pbrm", "pbrmcell", "pbrmcellcsm", "effectcell" };
	static const char * twinFrag[8] = { "", "", "fo4_cell", "fo4_cellcsm", "pbrm_default", "pbrm_cell", "pbrm_cellcsm", "fo4_effectcell" };
	if ( twin < 2 || twin > 7 ) {
		std::printf( "BLVIEW cell FAIL (no twin %d)\n", twin );
		return false;
	}
	// twin 7: fo4_effectcell (the game's effect shader, cell-lit): one target, its own maps and state, the soft fade's depth
	const bool fxT = twin == 7, pbrmT = twin >= 4 && !fxT, csmT = twin == 3 || twin == 6, lightsV = ( vars & 1u ) != 0 || twin >= 5;
	const bool oneT = twin == 4 || fxT;  // writes target 0 only
	const bool skinV = ( vars & 2u ) != 0, alphaV = ( vars & 4u ) != 0;
	const std::string tagS = std::string( twinTag[twin] ) + ( label ? std::string( " " ) + label : std::string() );
	const char * tag = tagS.c_str();
	struct CellGl
	{
		void( WINAPI * shaderBinary )( GLsizei, const GLuint *, GLenum, const void *, GLsizei ) = nullptr;
		void( WINAPI * specialize )( GLuint, const char *, GLuint, const GLuint *, const GLuint * ) = nullptr;
		void( WINAPI * ifaceIv )( GLuint, GLenum, GLenum, GLint * ) = nullptr;
		void( WINAPI * resIv )( GLuint, GLenum, GLuint, GLsizei, const GLenum *, GLsizei, GLsizei *, GLint * ) = nullptr;
		void( WINAPI * uf[4] )( GLint, GLsizei, const GLfloat * ) = {};
		void( WINAPI * ui1 )( GLint, GLint ) = nullptr;
		void( WINAPI * uu1 )( GLint, GLuint ) = nullptr;
		void( WINAPI * um3 )( GLint, GLsizei, GLboolean, const GLfloat * ) = nullptr;
		void( WINAPI * um4 )( GLint, GLsizei, GLboolean, const GLfloat * ) = nullptr;
		void( WINAPI * bindTextures )( GLuint, GLsizei, const GLuint * ) = nullptr;
		void( WINAPI * bindSamplers )( GLuint, GLsizei, const GLuint * ) = nullptr;
		void( WINAPI * drawBv )( GLenum, GLsizei, GLenum, const void *, GLint ) = nullptr;
		void( WINAPI * bufSub )( GLuint, std::intptr_t, std::ptrdiff_t, const void * ) = nullptr;
		void( WINAPI * clearTexSub )( GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void * ) = nullptr;
		void( WINAPI * texParami )( GLuint, GLenum, GLint ) = nullptr;
		void( WINAPI * disableAttr )( GLuint ) = nullptr;
		void( WINAPI * texBuffer )( GLenum, GLenum, GLuint ) = nullptr;
		GLint( WINAPI * loc )( GLuint, const char * ) = nullptr;
	} u;
	const char * n4 = "1234";
	bool okLoad = load( u.shaderBinary, "glShaderBinary" ) && load( u.specialize, "glSpecializeShader" ) && load( u.ifaceIv, "glGetProgramInterfaceiv" )
		&& load( u.resIv, "glGetProgramResourceiv" ) && load( u.ui1, "glUniform1i" ) && load( u.uu1, "glUniform1ui" )
		&& load( u.um3, "glUniformMatrix3fv" ) && load( u.um4, "glUniformMatrix4fv" ) && load( u.bindTextures, "glBindTextures" )
		&& load( u.bindSamplers, "glBindSamplers" ) && load( u.drawBv, "glDrawElementsBaseVertex" ) && load( u.bufSub, "glNamedBufferSubData" )
		&& load( u.clearTexSub, "glClearTexSubImage" ) && load( u.texParami, "glTextureParameteri" )
		&& load( u.disableAttr, "glDisableVertexAttribArray" ) && load( u.loc, "glGetUniformLocation" ) && load( u.texBuffer, "glTexBuffer" );
	for ( int i = 0; i < 4; i++ )
		okLoad = okLoad && load( u.uf[i], ( std::string( "glUniform" ) + n4[i] + "fv" ).c_str() );
	if ( !okLoad || !sx.fbo ) {
		std::printf( "BLVIEW %s FAIL (GL entry points missing, or no shade targets)\n", tag );
		return false;
	}
	std::string why;
	if ( !objBlLocsLoad( spirvDir + "/locs.txt", why ) ) {
		std::printf( "BLVIEW %s FAIL (no registry: %s)\n", tag, why.c_str() );
		return false;
	}
	std::map<std::string, std::string> index;
	std::map<std::string, int> reg;
	for ( int k = 0; k < 2; k++ ) {
		std::istringstream in( readFile( spirvDir + ( k ? "/locs.txt" : "/index.txt" ) ) );
		std::string ln;
		while ( std::getline( in, ln ) ) {
			const std::size_t t = ln.find( '\t' );
			if ( t == std::string::npos )
				continue;
			if ( k )
				reg[ln.substr( 0, t )] = std::atoi( ln.c_str() + t + 1 );
			else
				index[ln.substr( 0, t )] = ln.substr( t + 1 );
		}
	}
	// GL's program from the set, the app's base variant, the binaries as they are
	const GLuint prog = f.createProgram();
	const std::string stage[2] = { "fo4_default.vert", std::string( twinFrag[twin] ) + ( fxT ? ".frag@WW_CELL_AIRZONE+WW_CELL_FOGLEAK+WW_FRAME_UBO"
		: ".frag@WW_CELL_FOGLEAK+WW_FRAME_UBO" ) };
	for ( int s = 0; s < 2; s++ ) {
		const std::string bin = index.count( stage[s] ) ? readFile( spirvDir + "/" + index[stage[s]] ) : std::string();
		const GLuint sh = f.createShader( s ? kFrag : kVert );
		GLint okS = 0;
		if ( !bin.empty() ) {
			u.shaderBinary( 1, &sh, 0x9551, bin.data(), GLsizei( bin.size() ) );  // SHADER_BINARY_FORMAT_SPIR_V
			u.specialize( sh, "main", 0, nullptr, nullptr );
			f.getShaderiv( sh, kCompiled, &okS );
		}
		if ( !okS ) {
			std::printf( "BLVIEW %s FAIL (%s %s)\n", tag, stage[s].c_str(), bin.empty() ? "is not in the set" : "did not specialize" );
			return false;
		}
		f.attachShader( prog, sh );
	}
	f.linkProgram( prog );
	GLint linked = 0;
	f.getProgramiv( prog, kLinked, &linked );
	if ( !linked ) {
		char log[1024] = {};
		f.getProgramInfoLog( prog, GLsizei( sizeof( log ) - 1 ), nullptr, log );
		std::printf( "BLVIEW %s FAIL (%s from SPIR-V did not link: %s)\n", tag, stage[1].c_str(), log );
		return false;
	}
	// GL's active default-block locations and their types; every sampler on its own empty unit
	std::map<int, GLint> glType;
	GLint nRes = 0;
	u.ifaceIv( prog, 0x92E1, 0x92F5, &nRes );  // UNIFORM, ACTIVE_RESOURCES
	for ( GLint i = 0; i < nRes; i++ ) {
		const GLenum props[4] = { 0x930E, 0x92FB, 0x92FA, 0x92FD };  // LOCATION, ARRAY_SIZE, TYPE, BLOCK_INDEX
		GLint v[4] = { -1, 0, 0, -1 };
		u.resIv( prog, 0x92E1, GLuint( i ), 4, props, 4, nullptr, v );
		for ( GLint e = 0; v[0] >= 0 && v[3] < 0 && e < std::max( v[1], 1 ); e++ )
			glType[v[0] + e] = v[2];
	}
	auto isSampler = []( GLint t ) {
		return ( t >= 0x8B5D && t <= 0x8B64 ) || ( t >= 0x8DC0 && t <= 0x8DD8 ) || ( t >= 0x900C && t <= 0x900F ) || ( t >= 0x9108 && t <= 0x910D );
	};
	f.useProgram( prog );
	int units = 0;
	std::map<int, int> unitAt;
	for ( const auto & e : glType )
		if ( isSampler( e.second ) ) {
			unitAt[e.first] = 1 + units;
			u.ui1( e.first, 1 + units++ );
		}
	auto locOf = [&]( const char * name ) {
		GLint l = u.loc( prog, name );
		if ( l < 0 && reg.count( name ) )
			l = reg[name];
		return l;
	};
	const char * cellMaps[4] = { "BaseMap", "NormalMap", "SpecularMap", "GlowMap" }, * pbrmMaps[4] = { "BaseMap", "NormalMap", "RmaosMap", "EmissiveMap" };
	const char * fxMaps[4] = { "BaseMap", "GreyscaleMap", "NormalMap", "SpecularMap" };
	const char * const * mapName = fxT ? fxMaps : pbrmT ? pbrmMaps : cellMaps;
	int mapUnit[4] = {};
	for ( int m = 0; m < 4; m++ ) {
		const GLint l = locOf( mapName[m] );
		mapUnit[m] = unitAt.count( l ) ? unitAt[l] : -1;
		if ( mapUnit[m] < 0 ) {
			std::printf( "BLVIEW %s FAIL (%s has no unit: location %d)\n", tag, mapName[m], int( l ) );
			return false;
		}
	}
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW %s FAIL (GL error 0x%x building the program, %d samplers)\n", tag, unsigned( e ), units );
		return false;
	}
	// the named blocks at the twins' layout (objBlBlock checks GL's offsets), on GL's bindings (build_spv.py BLOCKS)
	struct Blk
	{
		std::string name;
		GLuint glBind = 0, buf = 0;
		const SpvBind * b = nullptr;
		std::vector<std::uint8_t> bytes;
	};
	SpvModule mods[2];
	const std::uint32_t * words[2] = { vkframe_objcell_vs_spv, nullptr };
	std::size_t counts[2] = { sizeof( vkframe_objcell_vs_spv ) / 4, 0 };
	objCellPs( twin, words[1], counts[1] );
	std::vector<Blk> blks;
	for ( int m = 0; m < 2; m++ ) {
		if ( !spvReflect( words[m], counts[m], mods[m], why ) ) {
			std::printf( "BLVIEW %s FAIL (the twin's module %d: %s)\n", tag, m, why.c_str() );
			return false;
		}
		for ( const SpvBind & b : mods[m].binds ) {
			if ( b.type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || b.name.find( "LooseUniforms" ) != std::string::npos )
				continue;
			static const std::pair<const char *, GLuint> known[3] = { { "globalUniforms", 0 }, { "skinningUniforms", 1 }, { "wwShadowFrame", 2 } };
			int k = -1;
			for ( int i = 0; i < 3; i++ )
				if ( b.name.find( known[i].first ) != std::string::npos )
					k = i;
			if ( k < 0 ) {
				std::printf( "BLVIEW %s FAIL (the twin reads block %s, which has no GL binding here)\n", tag, b.name.c_str() );
				return false;
			}
			Blk * have = nullptr;
			for ( Blk & x : blks )
				if ( x.name == known[k].first )
					have = &x;
			if ( !have ) {
				blks.push_back( Blk() );
				have = &blks.back();
				have->name = known[k].first;
				have->glBind = known[k].second;
			}
			if ( !have->b || b.bytes > have->b->bytes )
				have->b = &b;
		}
	}
	for ( Blk & x : blks ) {
		x.bytes.assign( std::max<std::size_t>( x.b->bytes, 16 ), 0 );
		f.genBuffers( 1, &x.buf );
		f.bindBuffer( 0x8A11, x.buf );
		f.bufferData( 0x8A11, std::ptrdiff_t( x.bytes.size() ), nullptr, kDynamic );
	}
	f.bindBuffer( 0x8A11, 0 );
	// a value: the loose uniform at GL's location, else the block member of that name
	int inactive = 0;
	std::string putBad, offNames;  // offNames: the names neither side reads (the cell's on pbrm_default; printed for twin 4)
	auto set = [&]( const char * name, std::initializer_list<float> vals, int elem = 0 ) {
		const GLint l0 = locOf( name );
		const int at = l0 < 0 ? -1 : l0 + elem;
		if ( at >= 0 && glType.count( at ) ) {
			const GLint t = glType[at];
			const float * v = vals.begin();
			f.useProgram( prog );
			if ( t == 0x1406 || ( t >= 0x8B50 && t <= 0x8B52 ) )
				u.uf[t == 0x1406 ? 0 : t - 0x8B50 + 1]( at, 1, v );
			else if ( t == 0x8B5C && vals.size() == 16 )
				u.um4( at, 1, GL_FALSE, v );
			else if ( t == 0x8B5B && vals.size() == 9 )
				u.um3( at, 1, GL_FALSE, v );
			else if ( t == 0x1405 )
				u.uu1( at, GLuint( v[0] ) );
			else
				u.ui1( at, GLint( v[0] ) );
			if ( glGetError() != GL_NO_ERROR && putBad.empty() )
				putBad = std::string( name ) + " (GL type " + std::to_string( t ) + ")";
			return;
		}
		for ( Blk & x : blks )
			for ( const SpvMember & m : x.b->members ) {
				if ( m.name != name )
					continue;
				if ( vals.size() == 16 && m.cols == 4 ) {
					SpvMember me = m;
					me.offset += std::uint32_t( elem ) * m.arrayStride;
					shadePutMat( x.bytes, me, vals.begin() );
				} else {
					const std::size_t at2 = m.offset + std::size_t( elem ) * m.arrayStride;
					const std::size_t n = std::min<std::size_t>( vals.size(), m.comps );
					if ( at2 + n * 4 <= x.bytes.size() )
						std::memcpy( x.bytes.data() + at2, vals.begin(), n * 4 );
				}
				return;
			}
		if ( offNames.find( std::string( " " ) + name + " " ) == std::string::npos )
			offNames += std::string( " " ) + name + " ";
		inactive++;
	};
	// globalUniforms as the colour gate's
	const float ident[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	for ( Blk & x : blks ) {
		if ( x.name != "globalUniforms" )
			continue;
		for ( const SpvMember & m : x.b->members ) {
			auto vec = [&]( std::initializer_list<float> v ) {
				std::memcpy( x.bytes.data() + m.offset, v.begin(), std::min<std::size_t>( v.size(), m.comps ) * 4 );
			};
			if ( m.name == "projectionMatrix" )
				shadePutMat( x.bytes, m, sx.proj );
			else if ( m.name == "viewMatrix" || m.name == "envMapRotation" )
				shadePutMat( x.bytes, m, ident );
			else if ( m.name == "lightSourcePosition" )
				vec( { 0.32f, 0.45f, 0.834f, 0.0f } );
			else if ( m.name == "lightSourceDiffuse" )
				vec( { 1.0f, 0.95f, 0.9f, 1.0f } );
			else if ( m.name == "lightSourceAmbient" )
				vec( { 0.2f, 0.2f, 0.25f, 1.0f } );
			else if ( m.name == "toneMapScale" || m.name == "brightnessScale" || m.name == "glowScale" || m.name == "glowScaleSRGB" )
				vec( { 1.0f } );
			else if ( m.name == "viewportDimensions" ) {
				const std::int32_t vp[4] = { 0, 0, kSW, kSH };
				std::memcpy( x.bytes.data() + m.offset, vp, 16 );
			}
		}
	}
	// the state every draw shares
	set( "cellOn", { 1 } );
	set( "cellSunScale", { 1 } );
	set( "cellExtDalc", { 1 } );
	const float dalc[3][4] = { { 0.10f, 0.05f, 0.02f, 0.12f }, { 0.09f, 0.06f, 0.03f, 0.11f }, { 0.12f, 0.04f, 0.02f, 0.10f } };
	for ( int c = 0; c < 3; c++ ) {
		set( "cellDalc", { dalc[c][0], dalc[c][1], dalc[c][2], dalc[c][3] }, c );
		set( "cellRow", { c == 0 ? 1.0f : 0.0f, c == 1 ? 1.0f : 0.0f, c == 2 ? 1.0f : 0.0f, 0.0f }, c );
	}
	set( "alpha", { 1 } );
	set( "fresnelPower", { 5 } );
	set( "paletteScale", { 1 } );
	set( "cellIsLinear", { 1 } );
	set( "specColor", { 1.0f, 0.9f, 0.8f } );
	set( "glowColor", { 0.3f, 0.2f, 0.1f } );
	if ( pbrmT ) {  // pbrm_default's constants every draw shares
		set( "pbrOpacity", { 1 } );
		set( "pbrAo", { 1 } );
		set( "pbrSpecV6", { 0 } );
		set( "pbrSpecWeight", { 1 } );
		set( "pbrSpecTint", { 1.0f, 1.0f, 1.0f } );
		set( "pbrIorMax", { 3 } );
		set( "pbrNormalStrength", { 1 } );
		set( "pbrEmissiveMapColor", { 1 } );
		set( "pbrEmissiveMapMask", { 0 } );
		set( "pbrGlobalOpacity", { 1 } );
		set( "pbrAlphaThreshold", { alphaV ? 0.5f : 0.0f } );
		set( "sceneExposure", { 1 } );
		set( "studioSun", { 1 } );
		set( "studioProbe", { -1 } );  // run 7: 0 (>= 0) replaced the Lookdev draws' lit result by black
		const float ax[6][3] = { { 0.20f, 0.15f, 0.10f }, { 0.10f, 0.12f, 0.15f }, { 0.25f, 0.22f, 0.20f }, { 0.05f, 0.06f, 0.08f },
			{ 0.30f, 0.32f, 0.35f }, { 0.08f, 0.07f, 0.06f } };
		for ( int c = 0; c < 6; c++ )
			set( "lookdevDalc", { ax[c][0], ax[c][1], ax[c][2] }, c );
		set( "pbrTintMasks", { 0.6f, 0.3f, 0.0f, 0.0f } );
		const float tc[4][3] = { { 0.9f, 0.4f, 0.3f }, { 0.3f, 0.8f, 0.4f }, { 0.2f, 0.3f, 0.9f }, { 0.7f, 0.7f, 0.2f } };
		for ( int c = 0; c < 4; c++ )
			set( "pbrTintColors", { tc[c][0], tc[c][1], tc[c][2] }, c );
	}
	// twin 3: the sun cascades on a shared D16 2D array (cascade 0: u = 0.5 + x / 16, v = 0.5 + y / 16, depth 0.5)
	static int csmKeyAt = 0;
	const std::uintptr_t csmKey = reinterpret_cast<std::uintptr_t>( &csmKeyAt );
	GLuint csmTex = 0;
	int csmUnit = -1;
	if ( csmT ) {
		if ( !sharedTarget( csmKey, 2, 256, 256, 3, 0x81A5, csmTex, why ) ) {
			std::printf( "BLVIEW %s FAIL (the shared cascade map: %s)\n", tag, why.c_str() );
			return false;
		}
		const GLenum pv[7][2] = { { 0x2801, 0x2601 }, { 0x2800, 0x2601 }, { 0x2802, 0x812F }, { 0x2803, 0x812F }, { 0x8072, 0x812F },
			{ 0x884C, 0x884E }, { 0x884D, 0x0203 } };  // LINEAR, CLAMP_TO_EDGE, COMPARE_REF_TO_TEXTURE, LEQUAL
		for ( const auto & p : pv )
			u.texParami( csmTex, p[0], GLint( p[1] ) );
		for ( int l = 0; l < 3; l++ )
			for ( int by = 0; by < 8; by++ )
				for ( int bx = 0; bx < 8; bx++ ) {
					const float d = ( ( bx + by ) & 1 ) != 0 ? 0.25f : 1.0f;
					u.clearTexSub( csmTex, 0, bx * 32, by * 32, l, 32, 32, 1, 0x1902, GL_FLOAT, &d );  // DEPTH_COMPONENT
				}
		const GLint l = locOf( "csmMap" );
		csmUnit = unitAt.count( l ) ? unitAt[l] : -1;
		const float k = 1.0f / 16.0f;
		for ( int c = 0; c < 3; c++ )
			set( "csmMat", { k, 0, 0, 0, 0, k, 0, 0, 0, 0, 0, 0, 0.5f, 0.5f, 0.5f, 1 }, c );
		set( "csmParams", { 1.0f, 1000.0f, 256.0f, 10.0f } );
		set( "csmSplitOffset", { 1000.0f, 2000.0f, 0.0f, 0.0f } );
		set( "csmForce", { -1.0f } );
		set( "csmRed", { 0 } );
		set( "ussOn", { 1 } );
		set( "ussCsmOn", { 1 } );
		set( "ussCsmFilter", { 1.0f, 1.5f, 12.0f, 0.0f } );
		if ( csmUnit < 0 || glGetError() != GL_NO_ERROR ) {
			std::printf( "BLVIEW %s FAIL (csmMap has no unit (location %d), or a GL error filling the map)\n", tag, int( l ) );
			return false;
		}
	}
	// variant 1 (and twins 5 / 6): the placed lights, 8 RGBA32F texels a light, view space (cellRow is the identity)
	GLuint lightTex = 0, lightBuf = 0;
	int lightUnit = -1;
	if ( lightsV ) {
		const float lt[3][8][4] = {
			{ { -3.0f, 1.5f, -6.0f, 8.0f }, { 1.2f, 0.9f, 0.6f, -2.0f }, { 0, 0, -1, 1 }, { 0, 1, 2, 0 }, { -1, 0, 0, 0 } },  // omni
			{ { 2.0f, 0.0f, -3.0f, 9.0f }, { 0.6f, 0.9f, 1.4f, 0.5f }, { 0, 0, -1, 2 }, { 0, 1, 2, 0 }, { -1, 0, 0, 0 } },    // spot
			{ { 5.0f, 2.0f, -8.0f, 9.0f }, { 0.8f, 1.3f, 0.7f, -2.0f }, { 0, 0, -1, 1 }, { 0, 1, 2, 1 }, { -1, 0, 0, 0 } } };  // omni, no specular
		f.genBuffers( 1, &lightBuf );
		f.bindBuffer( 0x8C2A, lightBuf );  // TEXTURE_BUFFER
		f.bufferData( 0x8C2A, std::ptrdiff_t( sizeof( lt ) ), lt, 0x88E4 );
		f.bindBuffer( 0x8C2A, 0 );
		glGenTextures( 1, &lightTex );
		glBindTexture( 0x8C2A, lightTex );
		u.texBuffer( 0x8C2A, kRgba32fi, lightBuf );
		glBindTexture( 0x8C2A, 0 );
		const GLint l = locOf( "cellLights" );
		lightUnit = unitAt.count( l ) ? unitAt[l] : -1;
		if ( lightUnit < 0 || glGetError() != GL_NO_ERROR ) {
			std::printf( "BLVIEW %s FAIL (cellLights has no unit (location %d), or a GL error making the light buffer)\n", tag, int( l ) );
			return false;
		}
	}
	/* twin 7: the soft fade's depth (Scene::grabEffectDepth's copy), a plain GL DEPTH32F texture the crossing copies into a
	 * twin once a frame; a saw of view depths 10 .. 17.75 behind the cards (7 .. 11; 0.25 a column, so the copy red's one-texel
	 * shift moves the fade; nearer, half the cards faded out and the lit share fell under 0.9) */
	GLuint fxDepthTex = 0;
	int fxDepthUnit = -1;
	if ( fxT ) {
		std::vector<float> dz( std::size_t( kSW ) * std::size_t( kSH ) );
		for ( int y = 0; y < kSH; y++ )
			for ( int x = 0; x < kSW; x++ ) {
				const float wv = 10.0f + 0.25f * float( ( x + y / 8 ) % 32 );
				dz[std::size_t( y ) * std::size_t( kSW ) + std::size_t( x )] = 0.5f * ( sx.proj[14] / wv - sx.proj[10] + 1.0f );
			}
		glGenTextures( 1, &fxDepthTex );
		glBindTexture( GL_TEXTURE_2D, fxDepthTex );
		glTexImage2D( GL_TEXTURE_2D, 0, 0x8CAC, kSW, kSH, 0, 0x1902, GL_FLOAT, dz.data() );  // DEPTH_COMPONENT32F
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glBindTexture( GL_TEXTURE_2D, 0 );
		const GLint l = locOf( "fxDepth" );
		fxDepthUnit = unitAt.count( l ) ? unitAt[l] : -1;
		set( "cellInterior", { 1 } );
		set( "hasSourceTexture", { 1 } );
		set( "hasNormalMap", { 1 } );
		set( "hasCubeMap", { 0 } );
		set( "fxSoftDepth", { 10 } );
		set( "fxDistScale", { 2 } );
		set( "falloffParams", { 0.1f, 0.9f, 0.2f, 1.0f } );
		set( "fxLitScale", { 1.0f, 1.0f, 1.0f, 1.0f } );
		set( "fxLit", { -1.0f, -1.0f, -1.0f, -1.0f } );
		set( "cellFxUnlitK", { 1.5f } );
		set( "cellFxEmitK", { 1.25f } );
		if ( fxDepthUnit < 0 || glGetError() != GL_NO_ERROR ) {
			std::printf( "BLVIEW %s FAIL (fxDepth has no unit (location %d), or a GL error making the depth)\n", tag, int( l ) );
			return false;
		}
	}
	// variant 2: 6 distinct bones (a turn about z, a shift; mat3x4 rows as GL's std140 columns), doSkinning on
	if ( skinV ) {
		Blk * sk = nullptr;
		for ( Blk & x : blks )
			if ( x.name == "skinningUniforms" )
				sk = &x;
		const SpvMember * bt = nullptr;
		for ( std::size_t m = 0; sk && m < sk->b->members.size(); m++ )
			if ( sk->b->members[m].name == "boneTransforms" )
				bt = &sk->b->members[m];
		bool skinOn = false;
		for ( Blk & x : blks )
			for ( const SpvMember & m : x.b->members )
				if ( x.name == "globalUniforms" && m.name == "doSkinning" && m.offset + 4 <= x.bytes.size() ) {
					const std::uint32_t one = 1;
					std::memcpy( x.bytes.data() + m.offset, &one, 4 );
					skinOn = true;
				}
		if ( !bt || !skinOn ) {
			std::printf( "BLVIEW %s FAIL (no skinningUniforms.boneTransforms or globalUniforms.doSkinning in the twins)\n", tag );
			return false;
		}
		const std::size_t stride = bt->arrayStride ? bt->arrayStride : 48;
		for ( int k = 0; k < 6; k++ ) {
			const float a = 0.12f * float( k - 2 ), c = std::cos( a ), s = std::sin( a );
			const float rows[12] = { c, -s, 0, 0.08f * float( k ) - 0.2f, s, c, 0, 0.06f * float( k % 3 ) - 0.06f, 0, 0, 1, 0.25f * float( k % 2 ) - 0.1f };
			if ( bt->offset + std::size_t( k ) * stride + 48 <= sk->bytes.size() )
				std::memcpy( sk->bytes.data() + bt->offset + std::size_t( k ) * stride, rows, 48 );
		}
	}
	// the shape: two 6 x 6 grids with a bump (base vertex 0 / 36), the app's sequential arrays; values exact in half; with
	// variant 2 the bone codes (bone + weight) at locations 5 / 6: bones v % 6 and (v + 1) % 6, weights 0.75 / 0.25 or 0.5 / 0.5
	const int g = 6, nv = g * g, ni = ( g - 1 ) * ( g - 1 ) * 6;
	const int nAttr = skinV ? 8 : 6;
	const int locs[8] = { 0, 1, 2, 3, 4, 7, 5, 6 }, comps[8] = { 3, 4, 3, 3, 3, 2, 4, 4 };
	std::vector<float> arr[8];
	std::mt19937 vr( 7 );
	std::uniform_real_distribution<float> v01( 0.0f, 1.0f );
	auto hq = []( float x ) { return fromHalf( toHalf( x ) ); };
	for ( int s = 0; s < 2; s++ )
		for ( int y = 0; y < g; y++ )
			for ( int x = 0; x < g; x++ ) {
				const float px = -1.0f + x * ( 2.0f / float( g - 1 ) ), py = -1.0f + y * ( 2.0f / float( g - 1 ) );
				const float pz = 0.3f * std::sin( px * 2.3f + py * ( s ? 1.9f : 1.1f ) + float( s ) );
				for ( float v : { px, py, pz } )
					arr[0].push_back( hq( v ) );
				for ( int c = 0; c < 4; c++ )
					arr[1].push_back( hq( c == 3 ? 1.0f : 0.5f + 0.5f * v01( vr ) ) );
				const float nx = -0.3f * std::cos( px * 2.3f ), nz = 1.0f, ln = std::sqrt( nx * nx + nz * nz );
				for ( float v : { nx / ln, 0.0f, nz / ln } )
					arr[2].push_back( hq( v ) );
				for ( float v : { nz / ln, 0.0f, -nx / ln } )
					arr[3].push_back( hq( v ) );
				for ( float v : { 0.0f, 1.0f, 0.0f } )
					arr[4].push_back( hq( v ) );
				for ( float v : { px * 0.75f + 0.5f, py * 0.75f + 0.5f } )
					arr[5].push_back( hq( v ) );
				const int vi = s * nv + y * g + x, b0 = vi % 6, b1 = ( vi + 1 ) % 6;
				const float w0 = vi % 2 ? 0.5f : 0.75f;
				for ( float v : { float( b0 ) + w0, float( b1 ) + 1.0f - w0, 0.0f, 0.0f } )
					arr[6].push_back( v );
				for ( int c = 0; c < 4; c++ )
					arr[7].push_back( 0.0f );
			}
	std::vector<std::uint16_t> idx;
	for ( int y = 0; y + 1 < g; y++ )
		for ( int x = 0; x + 1 < g; x++ ) {
			const std::uint16_t i0 = std::uint16_t( y * g + x ), i1 = std::uint16_t( i0 + 1 ), i2 = std::uint16_t( i0 + g ), i3 = std::uint16_t( i0 + g + 1 );
			idx.insert( idx.end(), { i0, i1, i3, i0, i3, i2 } );  // CCW seen from +z
		}
	GLuint ebo = 0, vbo[3] = {}, vao[3] = {};
	f.genBuffers( 1, &ebo );
	f.bindBuffer( 0x8F37, ebo );
	f.bufferData( 0x8F37, std::ptrdiff_t( idx.size() * 2 ), idx.data(), 0x88E4 );
	// layout 0 floats, 1 halves (VTXPACK1), 2 the refusal: colour as 4 normalised bytes
	for ( int lay = 0; lay < 3; lay++ ) {
		std::vector<std::uint8_t> bytes;
		std::size_t offs[8] = {};
		for ( int a = 0; a < nAttr; a++ ) {
			offs[a] = bytes.size();
			if ( lay == 1 || ( lay == 2 && a == 1 ) ) {
				const int n = comps[a], slot = lay == 1 && n == 3 ? 4 : n;
				for ( std::size_t v = 0; v < std::size_t( 2 * nv ); v++ )
					for ( int c = 0; c < slot; c++ ) {
						const float x = c < n ? arr[a][v * std::size_t( n ) + std::size_t( c )] : 0.0f;
						if ( lay == 2 )
							bytes.push_back( std::uint8_t( std::lround( x * 255.0f ) ) );
						else {
							const std::uint16_t h = toHalf( x );
							bytes.insert( bytes.end(), { std::uint8_t( h & 0xFF ), std::uint8_t( h >> 8 ) } );
						}
					}
			} else {
				const std::size_t at = bytes.size();
				bytes.resize( at + arr[a].size() * 4 );
				std::memcpy( bytes.data() + at, arr[a].data(), arr[a].size() * 4 );
			}
		}
		f.genBuffers( 1, &vbo[lay] );
		f.genVertexArrays( 1, &vao[lay] );
		f.bindVertexArray( vao[lay] );
		f.bindBuffer( 0x8892, vbo[lay] );
		f.bufferData( 0x8892, std::ptrdiff_t( bytes.size() ), bytes.data(), 0x88E4 );
		for ( int a = 0; a < nAttr; a++ ) {
			const int n = comps[a];
			const void * off = reinterpret_cast<const void *>( offs[a] );
			if ( lay == 2 && a == 1 )
				sf.vertexAttribPointer( GLuint( locs[a] ), n, GL_UNSIGNED_BYTE, GL_TRUE, 0, off );
			else if ( lay == 1 )
				sf.vertexAttribPointer( GLuint( locs[a] ), n, kHalf, GL_FALSE, GLsizei( ( n == 3 ? 4 : n ) * 2 ), off );
			else
				sf.vertexAttribPointer( GLuint( locs[a] ), n, GL_FLOAT, GL_FALSE, 0, off );
			sf.enableVertexAttribArray( GLuint( locs[a] ) );
		}
		f.bindBuffer( 0x8893, ebo );
		f.bindVertexArray( 0 );
	}
	f.bindBuffer( 0x8892, 0 );
	// the sweep: layout, commands (count, instances, first index, base vertex, base instance), the seeded state
	struct CellDraw
	{
		int lay = 0;
		std::vector<std::uint32_t> cmds;
	};
	const std::uint32_t h = std::uint32_t( ni / 2 ) / 3 * 3;
	const CellDraw draws[6] = { { 0, { std::uint32_t( ni ), 1, 0, 0, 0 } }, { 1, { std::uint32_t( ni ), 1, 0, std::uint32_t( nv ), 0 } },
		{ 0, { h, 1, 0, 0, 0, std::uint32_t( ni ) - h, 1, h, std::uint32_t( nv ), 0 } },
		{ 1, { h, 1, 0, std::uint32_t( nv ), 0, std::uint32_t( ni ) - h, 1, h, 0, 0 } },
		{ 0, { std::uint32_t( ni ), 1, 0, std::uint32_t( nv ), 0 } }, { 1, { std::uint32_t( ni ), 1, 0, 0, 0 } } };
	const int nDraws = 6;
	int probe = 0;  // the GL-only variant probes: 1 the lights off, 2 the skinning off, 3 the alpha test and blend off
	bool oldBrdf = false;  // pbrm_default's red 8: r3Red 64, the BRDF before OpenPBR (GL's reference always draws without it)
	static const int alphaMode[6] = { 4, 4 | 8, 8, 6, 0, 1 | 8 };  // '>', '>' + blend, blend, '>=', none, '<' + blend
	static const int fxAlpha[6] = { 8, 4 | 8, 8, 6, 0, 8 };  // twin 7: blended (soft) draws 0, 1, 2, 5 (5 additive); a test on 1 and 3
	auto drawState = [&]( int i ) {
		if ( fxT ) {  // the effect's own state, seeded apart from the shared draw below (its names are inactive on the effect)
			std::mt19937 r( std::uint32_t( 701 + i ) );
			std::uniform_real_distribution<float> q( 0.0f, 1.0f );
			const int am = fxAlpha[i];
			set( "alphaFlags", { float( am ) } );
			set( "alphaThreshold", { 0.3f } );
			set( "fxSoft", { ( am & 8 ) != 0 ? 1.0f : 0.0f } );
			set( "fxAdditive", { i == 5 ? 1.0f : 0.0f } );
			if ( ( am & 8 ) != 0 ) {
				glEnable( GL_BLEND );
				glBlendFunc( GL_SRC_ALPHA, i == 5 ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA );
			}
			set( "glowColor", { 0.3f + 0.7f * q( r ), 0.3f + 0.7f * q( r ), 0.3f + 0.7f * q( r ), 0.6f + 0.4f * q( r ) } );
			set( "glowMult", { 0.5f + q( r ) } );
			set( "greyscaleColor", { i == 3 ? 1.0f : 0.0f } );
			set( "useFalloff", { i == 1 || i == 4 ? 1.0f : 0.0f } );
			set( "hasRGBFalloff", { i == 4 ? 1.0f : 0.0f } );
			set( "hasEnvMask", { i % 2 == 0 ? 1.0f : 0.0f } );
			set( "lightingInfluence", { 0.4f + 0.6f * q( r ) } );
			set( "fxLitMode", { i % 3 != 2 ? 3.0f : 0.0f } );  // lit by every light (cellLightCount), or unlit
			set( "fxEmit", { 0.8f, 0.6f, 0.4f, i == 4 ? 1.0f : 0.0f } );
		}
		if ( lightsV )
			set( "cellLightCount", { float( probe == 1 ? 0 : 3 - i % 3 ) } );
		if ( skinV )
			for ( Blk & x : blks )
				for ( const SpvMember & m : x.b->members )
					if ( x.name == "globalUniforms" && m.name == "doSkinning" ) {
						const std::uint32_t on = probe == 2 ? 0u : 1u;
						std::memcpy( x.bytes.data() + m.offset, &on, 4 );
					}
		const int am = alphaV && probe != 3 ? alphaMode[i] : 0;
		if ( alphaV ) {
			set( "alphaFlags", { float( am ) } );
			set( "alphaThreshold", { 0.5f } );
			if ( pbrmT )  // .pbrm composition: draw 0 the material's own test (pbrAlphaThreshold), draw 2 its blend, the rest alphaFlags
				set( "pbrComposition", { float( probe == 3 ? 0 : i == 0 ? 1 : i == 2 ? 2 : -1 ) } );
			if ( ( am & 8 ) != 0 ) {
				glEnable( GL_BLEND );
				glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			}
		}
		std::mt19937 r( std::uint32_t( 101 + i ) );
		std::uniform_real_distribution<float> q( 0.0f, 1.0f );
		const bool sun = i % 3 != 2;
		float dx = q( r ) - 0.5f, dy = q( r ) * 0.8f + 0.1f, dz = q( r ) * 0.6f + 0.4f;
		const float dl = std::sqrt( dx * dx + dy * dy + dz * dz );
		set( "cellHasDir", { sun ? 1.0f : 0.0f } );
		set( "cellDirTo", { dx / dl, dy / dl, dz / dl } );
		set( "cellDirColor", { 1.0f + q( r ), 1.0f + 0.5f * q( r ), 0.8f + 0.4f * q( r ) } );
		set( "hasSpecularMap", { i % 2 == 0 ? 1.0f : 0.0f } );
		set( "specStrength", { 0.3f + 0.7f * q( r ) } );
		set( "specGlossiness", { 0.2f + 0.7f * q( r ) } );
		const bool glow = i % 3 == 0;
		set( "hasEmit", { glow ? 1.0f : 0.0f } );
		set( "hasGlowMap", { glow ? 1.0f : 0.0f } );
		const float gm = glow ? 0.5f + q( r ) : 0.0f;
		if ( !fxT )  // twin 7 keeps its own glowMult (the effect's colour scale: 0 would draw every card black)
			set( "glowMult", { gm } );
		set( "uvScale", { 0.5f + 1.5f * q( r ), 0.5f + 1.5f * q( r ) } );
		set( "uvOffset", { q( r ), q( r ) } );
		set( "vertexColorOverride", { i == 4 ? 0.7f : 0.0f, 0.0f, 0.0f, 0.0f } );
		set( "cellProbe", { i == 3 ? 60.0f : 0.0f } );
		if ( pbrmT ) {  // pbrm_default: the material and the scene mode per draw
			set( "pbrBaseColor", { 0.2f + 0.7f * q( r ), 0.2f + 0.7f * q( r ), 0.2f + 0.7f * q( r ) } );
			set( "pbrRoughness", { 0.1f + 0.8f * q( r ) } );
			set( "pbrMetallic", { i % 2 ? 0.0f : 0.5f * q( r ) } );  // run 5: a full metal with no cube map is black off its highlight
			set( "pbrF0", { 0.02f + 0.06f * q( r ) } );
			set( "pbrDiffuseRoughness", { i == 2 ? 0.6f : 0.0f } );
			// base, normal, the RMAOS map (8) with its rough / AO channels (16, 64; run 5: bit 8 alone reads no channel), its metal
			// channel (32) on draws 2 and 4, emissive
			const int feat = 1 | ( i % 2 == 0 ? 2 : 0 ) | ( i != 1 ? 8 | 16 | 64 : 0 ) | ( i == 2 || i == 4 ? 32 : 0 ) | ( glow ? 256 : 0 )
				| ( alphaV ? 2048 : 0 );  // F_OPACITY: the base map's alpha is the opacity
			set( "pbrFeatures", { float( feat ) } );
			set( "pbrEmissiveColor", { 0.8f, 0.6f, 0.4f } );
			set( "pbrEmissiveIntensity", { glow ? 0.5f + q( r ) : 0.0f } );
			set( "pbrEmissiveMask", { 1 } );
			set( "pbrTintEnabled", { i == 5 ? 1.0f : 0.0f } );
			set( "pbrTintMode", { float( i % 3 ) } );
			set( "sceneMode", { i == 1 || i == 4 ? 2.0f : 0.0f } );
			set( "viewTransform", { float( i % 3 ) } );
			set( "r3Red", { oldBrdf ? 64.0f : 0.0f } );  // a loose int uniform: set() writes it with glUniform1i
		}
		// the model-view: about y, scale 2.2, x across the frame, depth 7..11 (the shapes overlap: the depth test decides)
		const float ang = 1.1f * ( q( r ) - 0.5f ), sc = 2.2f, c = std::cos( ang ), s = std::sin( ang );
		const float tx = -5.0f + 2.0f * float( i ), ty = 0.8f * ( q( r ) - 0.5f ), tz = -7.0f - 4.0f * q( r );
		set( "modelViewMatrix", { sc * c, 0, -sc * s, 0, 0, sc, 0, 0, sc * s, 0, sc * c, 0, tx, ty, tz, 1 } );
		set( "normalMatrix", { c, 0, -s, 0, 1, 0, s, 0, c } );
		for ( Blk & x : blks )
			u.bufSub( x.buf, 0, std::ptrdiff_t( x.bytes.size() ), x.bytes.data() );
	};
	GLuint glOnly = 0;  // the refusal: a GL-only RGBA8 texture on BaseMap
	glGenTextures( 1, &glOnly );
	glBindTexture( GL_TEXTURE_2D, glOnly );
	{
		const std::uint32_t px[16] = { 0xFF8040C0u, 0xFF40C080u, 0xFFC08040u, 0xFF808080u, 0xFF8040C0u, 0xFF40C080u, 0xFFC08040u, 0xFF808080u,
			0xFF8040C0u, 0xFF40C080u, 0xFFC08040u, 0xFF808080u, 0xFF8040C0u, 0xFF40C080u, 0xFFC08040u, 0xFF808080u };
		glTexImage2D( GL_TEXTURE_2D, 0, GLint( kRgba8i ), 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, px );
	}
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glBindTexture( GL_TEXTURE_2D, 0 );
	auto frameState = [&]( bool glOnlyBase ) {
		f.bindFramebuffer( kFb, sx.fbo );
		const GLenum dbs[2] = { kColorAtt0, kColorAtt0 + 1 };
		sf.drawBuffers( oneT ? 1 : 2, dbs );  // pbrm_default's base variant and the effect write target 0 only (the colour gate's 1buf frame)
		glViewport( 0, 0, kSW, kSH );
		glDisable( GL_BLEND );
		glDisable( GL_SCISSOR_TEST );
		glDisable( GL_STENCIL_TEST );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LESS );
		glDepthMask( GL_TRUE );
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		glEnable( GL_CULL_FACE );
		glCullFace( GL_BACK );
		glFrontFace( GL_CCW );
		f.useProgram( prog );
		u.bindTextures( 0, 64, nullptr );
		u.bindSamplers( 0, 64, nullptr );
		for ( int m = 0; m < 4; m++ ) {
			const GLuint t = m == 0 && glOnlyBase ? glOnly : maps[m];
			u.bindTextures( GLuint( mapUnit[m] ), 1, &t );
		}
		if ( csmTex )
			u.bindTextures( GLuint( csmUnit ), 1, &csmTex );
		if ( lightTex )
			u.bindTextures( GLuint( lightUnit ), 1, &lightTex );
		if ( fxDepthTex )
			u.bindTextures( GLuint( fxDepthUnit ), 1, &fxDepthTex );
		for ( Blk & x : blks )
			f.bindBufferBase( 0x8A11, x.glBind, x.buf );
	};
	auto frameOff = [&]() {
		f.bindVertexArray( 0 );
		glDisable( GL_BLEND );
		glDisable( GL_CULL_FACE );
		glDisable( GL_DEPTH_TEST );
		f.useProgram( 0 );
		f.bindFramebuffer( kFb, 0 );
	};
	struct CellOut
	{
		std::vector<float> depth;
		std::vector<std::uint16_t> c0, c1;
	};
	auto glSweep = [&]( CellOut & out, int only = -1 ) {  // only: that draw alone (twin 4's per-draw lit line)
		shadeClear();
		for ( int i = 0; i < nDraws; i++ ) {
			if ( only >= 0 && i != only )
				continue;
			frameState( false );
			drawState( i );
			f.bindVertexArray( vao[draws[i].lay] );
			for ( std::size_t c = 0; c < draws[i].cmds.size(); c += 5 )
				u.drawBv( GL_TRIANGLES, GLsizei( draws[i].cmds[c] ), GL_UNSIGNED_SHORT,
					reinterpret_cast<const void *>( std::size_t( draws[i].cmds[c + 2] ) * 2 ), GLint( draws[i].cmds[c + 3] ) );
			frameOff();
		}
		shadeRead( out.depth, &out.c0, &out.c1 );
		glFinish();
		return glGetError();
	};
	static int poolKeyAt = 0;
	const std::uintptr_t poolKey = reinterpret_cast<std::uintptr_t>( &poolKeyAt );
	// the crossing per shape; lay / glOnlyBase: the refusal arms (one draw, nothing drawn)
	auto vkSweep = [&]( int red, CellOut & out, std::string & w, int onlyLay = -1, bool glOnlyBase = false ) {
		shadeClear();
		bool ran = true;
		for ( int i = 0; i < ( onlyLay >= 0 ? 1 : nDraws ) && ran; i++ ) {
			frameState( glOnlyBase );
			drawState( i );
			f.bindVertexArray( vao[onlyLay >= 0 ? onlyLay : draws[i].lay] );
			ObjCellShade s;
			s.twin = twin;
			s.prog = prog;
			s.poolKey = poolKey;
			s.verts = std::size_t( 2 * nv );
			s.cmds = draws[i].cmds.data();
			s.nCmds = draws[i].cmds.size() / 5;
			s.red = red;
			ran = objCellShade( glProc, s, w );
			frameOff();
		}
		if ( ran )
			shadeRead( out.depth, &out.c0, &out.c1 );
		glFinish();
		const GLenum e = glGetError();
		if ( ran && e != GL_NO_ERROR )
			w = "GL error " + std::to_string( e ) + " at the crossing";
		return ran && e == GL_NO_ERROR;
	};
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR || !putBad.empty() ) {
		std::printf( "BLVIEW %s FAIL (GL error 0x%x setting the state; first bad value %s)\n", tag, unsigned( e ), putBad.c_str() );
		return false;
	}
	CellOut gl;
	if ( const GLenum e = glSweep( gl ); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW %s FAIL (GL error 0x%x in GL's sweep)\n", tag, unsigned( e ) );
		return false;
	}
	std::printf( "BLVIEW %s state: %d samplers on their own units, %d values with no active location or member, blocks:", tag, units, inactive );
	for ( const Blk & x : blks )
		std::printf( " %s@%u (%zu B)", x.name.c_str(), x.glBind, x.bytes.size() );
	std::printf( "\n" );
	if ( pbrmT || fxT ) {
		std::printf( "BLVIEW %s not read:%s\n", tag, offNames.c_str() );
		std::printf( "BLVIEW %s GL per draw (covered / rgb non-zero):", tag );
		for ( int i = 0; i < nDraws; i++ ) {
			CellOut one;
			glSweep( one, i );
			std::size_t cov = 0, lit = 0;
			for ( std::size_t p = 0; p < one.depth.size(); p++ )
				if ( one.depth[p] < 1.0f ) {
					cov++;
					lit += one.c0[p * 4] != 0 || one.c0[p * 4 + 1] != 0 || one.c0[p * 4 + 2] != 0;
				}
			std::printf( " %d: %zu / %zu", i, cov, lit );
		}
		std::printf( "\n" );
	}
	bool csmReach = true;
	if ( csmT ) {  // does the cascade map reach GL's colour (csmForce 1: no shadow)
		CellOut g2;
		set( "csmForce", { 1.0f } );
		glSweep( g2 );
		set( "csmForce", { -1.0f } );
		std::size_t cov = 0, moved = 0;
		for ( std::size_t i = 0; i < gl.depth.size(); i++ ) {
			if ( !( gl.depth[i] < 1.0f ) )
				continue;
			cov++;
			bool mv = false;
			for ( int ch = 0; ch < 4; ch++ )
				mv = mv || gl.c0[i * 4 + ch] != g2.c0[i * 4 + ch];
			moved += mv;
		}
		csmReach = cov > 0 && double( moved ) >= 0.2 * double( cov );
		std::printf( "BLVIEW %s GL map probe %s: csmForce 1 moves GL's colour on %zu of %zu covered pixels (bar >= 20 %%)\n", tag,
			csmReach ? "OK" : "FAIL", moved, cov );
	}
	// the variant probes: each variant's input reaches GL's picture (else its red could test nothing)
	static const char * probeName[4] = { "", "lights (cellLightCount 0)", "skin (doSkinning 0)", "alpha (alphaFlags 0, no blend)" };
	for ( int pr = 1; pr <= 3; pr++ ) {
		if ( !( pr == 1 ? lightsV : pr == 2 ? skinV : alphaV ) )
			continue;
		CellOut g2;
		probe = pr;
		glSweep( g2 );
		probe = 0;
		std::size_t cov = 0, moved = 0;
		for ( std::size_t i = 0; i < gl.depth.size(); i++ ) {
			if ( !( gl.depth[i] < 1.0f ) )
				continue;
			cov++;
			bool mv = gl.depth[i] != g2.depth[i];
			for ( int ch = 0; ch < 4; ch++ )
				mv = mv || gl.c0[i * 4 + ch] != g2.c0[i * 4 + ch];
			moved += mv;
		}
		const bool reach = cov > 0 && double( moved ) >= 0.2 * double( cov );
		std::printf( "BLVIEW %s GL %s probe %s: moves GL's colour or depth on %zu of %zu covered pixels (bar >= 20 %%)\n", tag, probeName[pr],
			reach ? "OK" : "FAIL", moved, cov );
		csmReach = csmReach && reach;
	}
	struct CellCmp
	{
		std::size_t both = 0, differ = 0, over = 0, lit = 0, glow = 0, first = 0;
		float maxRel = 0.0f;
	};
	auto colCmp = [&]( const CellOut & v ) {
		CellCmp c;
		for ( std::size_t i = 0; i < gl.depth.size(); i++ ) {
			if ( !( gl.depth[i] < 1.0f ) || !( v.depth[i] < 1.0f ) )
				continue;
			c.both++;
			bool lit = false, glow = false;
			for ( int t = 0; t < 2; t++ )
				for ( int ch = 0; ch < 4; ch++ ) {
					const std::uint16_t hg = ( t ? gl.c1 : gl.c0 )[i * 4 + ch], hv = ( t ? v.c1 : v.c0 )[i * 4 + ch];
					const float gv2 = fromHalf( hg ), a = fromHalf( hv );
					lit = lit || ( t == 0 && ch < 3 && gv2 != 0.0f );
					glow = glow || ( t == 1 && gv2 != 0.0f );
					if ( hg == hv )
						continue;
					c.differ++;
					const float rel = std::fabs( a - gv2 ) / std::max( 1.0f, std::fabs( gv2 ) );
					if ( !( rel <= 4e-3f ) ) {
						if ( c.over == 0 )
							c.first = i * 8 + std::size_t( t * 4 + ch );
						c.over++;
					}
					if ( rel > c.maxRel || rel != rel )
						c.maxRel = rel != rel ? 1e30f : rel;
				}
			c.lit += lit;
			c.glow += glow;
		}
		return c;
	};
	bool ok = csmReach;
	takeObjBlRunStat();
	CellOut vk0;
	std::string w;
	const bool ran = vkSweep( 0, vk0, w );
	const ObjBlRunStat rs = takeObjBlRunStat();
	if ( !ran ) {
		std::printf( "BLVIEW %s FAIL (objCellShade refused: %s)\n", tag, w.c_str() );
		ok = false;
	} else {
		const DepthCmp dc = depthCmp( vk0.depth, gl.depth );
		const CellCmp cc = colCmp( vk0 );
		const bool litOk = cc.both >= 2000 && double( cc.lit ) >= 0.9 * double( cc.both ) && ( oneT || cc.glow >= 200 );
		int blendDraws = 0;
		for ( int i = 0; i < nDraws; i++ )
			blendDraws += ( alphaV && ( alphaMode[i] & 8 ) != 0 ) || ( fxT && ( fxAlpha[i] & 8 ) != 0 ) ? 1 : 0;
		const bool varOk = rs.cellLit == ( lightsV ? nDraws : 0 ) && rs.cellSkinned == ( skinV ? nDraws : 0 ) && rs.cellBlended == blendDraws;
		// twin 7: the depth copied once for the sweep (one frame), and its cost counted
		const bool copyOk = fxT ? rs.copies == 1 && rs.copyBytes == std::uint64_t( kSW ) * std::uint64_t( kSH ) * 4u : rs.copies == 0;
		const bool crossOk = rs.runs == nDraws && rs.fails == 0 && rs.cells == nDraws && rs.twins == 4 * nDraws && varOk && copyOk;
		std::printf( "BLVIEW %s crossing %s (%d runs, %d failed, %d cells, %d twin images, %d shared, %d GL-only; lit %d skinned %d blended %d; "
			"copies %d, %.1f KB, %.3f ms)\n", tag, crossOk ? "OK" : "FAIL", rs.runs, rs.fails, rs.cells, rs.twins, rs.shared, rs.glOwn, rs.cellLit,
			rs.cellSkinned, rs.cellBlended, rs.copies, rs.copyBytes / 1024.0, rs.copyMs );
		std::printf( "BLVIEW %s depth vs GL %s (%zu covered, %zu on one side, %zu differ, max %.3g)\n", tag, dc.word(), dc.covered, dc.oneSide,
			dc.differ, double( dc.maxAbs ) );
		const bool colOk = cc.over == 0;
		std::printf( "BLVIEW %s vs GL %s (%zu pixels both cover, %zu lit, %zu with glow: %s; %zu values differ, %zu over the bound, max %.3g of "
			"max(1, |GL|))\n", tag, cc.differ == 0 ? "EXACT" : colOk ? "BOUND" : "FAIL", cc.both, cc.lit, cc.glow, litOk ? "lit" : "NOT LIT",
			cc.differ, cc.over, double( cc.maxRel ) );
		if ( cc.over ) {
			const std::size_t p = cc.first / 8, t = ( cc.first % 8 ) / 4;
			const auto & gg = t ? gl.c1 : gl.c0;
			const auto & a = t ? vk0.c1 : vk0.c0;
			std::printf( "BLVIEW %s first over: pixel (%zu, %zu) target %zu channel %zu; GL %g %g %g %g, Vulkan %g %g %g %g\n", tag, p % kSW, p / kSW, t,
				cc.first % 4, double( fromHalf( gg[p * 4] ) ), double( fromHalf( gg[p * 4 + 1] ) ), double( fromHalf( gg[p * 4 + 2] ) ),
				double( fromHalf( gg[p * 4 + 3] ) ), double( fromHalf( a[p * 4] ) ), double( fromHalf( a[p * 4 + 1] ) ), double( fromHalf( a[p * 4 + 2] ) ),
				double( fromHalf( a[p * 4 + 3] ) ) );
		}
		ok = ok && crossOk && ( dc.exact() || dc.bound() ) && litOk && colOk;
	}
	if ( pbrmT && ( reds >> 7 & 1u ) ) {  // lane VKWATER3: the look before OpenPBR against the new one, both on GL (a number, not a gate)
		CellOut g2;
		oldBrdf = true;
		glSweep( g2 );
		oldBrdf = false;
		const CellCmp cc = colCmp( g2 );
		double sum = 0.0;
		std::size_t n = 0;
		for ( std::size_t i = 0; i < gl.depth.size(); i++ ) {
			if ( !( gl.depth[i] < 1.0f ) || !( g2.depth[i] < 1.0f ) )
				continue;
			for ( int ch = 0; ch < 3; ch++ ) {
				const float a = fromHalf( g2.c0[i * 4 + ch] ), b = fromHalf( gl.c0[i * 4 + ch] );
				sum += std::fabs( a - b ) / std::max( 1.0f, std::fabs( b ) );
				n++;
			}
		}
		std::printf( "BLVIEW %s OpenPBR vs the old BRDF (GL, target 0 rgb): %zu pixels, %zu values differ, max %.4g, mean %.4g of max(1, |new|)\n",
			tag, cc.both, cc.differ, double( cc.maxRel ), n ? sum / double( n ) : 0.0 );
	}
	const char * redName[9] = { "offset (every attribute read from offset 0)", "half (halves read as 16-bit unorm)",
		"swz (the twins' swizzle dropped)", "key (each twin the one made before it)", "light (the light texel buffer copied from one texel on)",
		"bone (the bone transforms one bone on)", "blend (GL's source / destination factors swapped)",
		"oldbrdf (r3Red 64: the BRDF before OpenPBR)", "copy (the GL inputs' copy twins copied from one texel on)" };
	for ( int r = 1; r <= 9; r++ ) {  // reds bit r - 1; red 8 is a GL look (oldbrdf), bit 8 the crossing's red 8 (copy)
		if ( !( reds >> ( r - 1 ) & 1u ) )
			continue;
		CellOut o;
		std::string rw;
		oldBrdf = r == 8;
		takeObjBlRunStat();  // a fresh frame: the copy twins copy again
		const bool rr = vkSweep( r == 8 ? 0 : r == 9 ? 8 : r, o, rw );
		oldBrdf = false;
		bool passes = false;
		std::size_t over = 0, vch = 0;
		if ( rr ) {
			const DepthCmp dc = depthCmp( o.depth, gl.depth );
			const CellCmp cc = colCmp( o );
			over = cc.over;
			for ( std::size_t i = 0; ran && i < o.c0.size() && i < vk0.c0.size(); i++ )
				vch += o.c0[i] != vk0.c0[i];
			for ( std::size_t i = 0; ran && i < o.depth.size() && i < vk0.depth.size(); i++ )
				vch += o.depth[i] != vk0.depth[i];
			passes = ( dc.exact() || dc.bound() ) && over == 0;
		}
		std::printf( "BLVIEW RED %s %s %s (%zu values over the bound, %zu Vulkan values moved off the Vulkan base%s%s)\n", tag, redName[r - 1],
			passes ? "PASSED" : vch ? "FAIL" : "TESTS NOTHING", over, vch, rr ? "" : "; refused: ", rr ? "" : rw.c_str() );
		ok = ok && !passes && vch > 0;
	}
	if ( !label ) {  // the refusals: nothing drawn, the reason given
		const struct
		{
			const char * name, * want;
			int lay;
			bool glOnlyBase;
		} rf[2] = { { "a GL-only RGBA8 texture on BaseMap", "GL textures with no twin", 0, true },
			{ "a normalised byte colour attribute", "not 1 to 4 floats or halves", 2, false } };
		for ( const auto & x : rf ) {
			CellOut o;
			std::string rw;
			takeObjBlRunStat();
			const bool rr = vkSweep( 0, o, rw, x.lay, x.glOnlyBase );
			const ObjBlRunStat s2 = takeObjBlRunStat();
			const bool good = !rr && rw.find( x.want ) != std::string::npos && s2.runs == 0 && s2.cells == 0;
			std::printf( "BLVIEW %s refusal %s: %s (%s; %d runs)\n", tag, x.name, good ? "OK" : "FAIL", rr ? "drawn" : rw.c_str(), s2.runs );
			ok = ok && good;
		}
	}
	f.bindVertexArray( 0 );
	glDeleteTextures( 1, &glOnly );
	if ( lightTex )
		glDeleteTextures( 1, &lightTex );  // its buffer stays with the gate's other buffers
	if ( fxDepthTex )
		glDeleteTextures( 1, &fxDepthTex );  // its copy twin stays shared until the present's shutdown (as the app's would)
	objBlLocsLoad( spirvDir + "/no_locs.txt", why );  // the other gates never see it
	std::printf( "BLVIEW %s %s\n", tag, ok ? "OK" : "FAILED" );
	return ok;
}
