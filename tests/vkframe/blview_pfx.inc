/* lane VKOBJ5 (plan row G4 round 3): particles_cell.prog on the present device. GL's own program from the HLSL SPIR-V set
 * (particles.vert + particles.geom + particles_cell.frag@WW_CELL_AIRZONE+WW_CELL_FOGLEAK+WW_FRAME_UBO, the app's base variant)
 * draws one system as GL_POINTS into the shared targets; then objPfxShade (the app's crossing, glparticles.cpp's draw site)
 * draws the same system through the record pass (objpfx_cs), the strip stage (objpfx_vs) and the pixel twin (objpfx_cell_ps).
 * Fixture: 203 points (not a multiple of 64), view space x -4..4, y -3..3, z -5..-15, seeded mt19937( 31 ): colour, uv offset,
 * angle, size; the app's sequential arrays in one VBO (position 3, colour 4, uv 2, angle 1, size 1 at locations 0..4).
 * Units as the app binds them (glparticles.cpp, round 4): all 10 textureUnits[i] and NormalMap / GreyscaleMap on the
 * sprite's unit (textureUnits[0] = the texture cache's DXT5 twin, textures[0].textureUnit 1), CubeMap on its own unit
 * holding a 1x1 grey cube (bindCube( "#FF555555c" )), hasNormalMap / hasGreyscaleMap / hasCubeMap 0; every other sampler on
 * its own empty unit. Cull on, back faces, stencil off (StencilProperty::glProperty( nullptr ), the app's last word).
 * State: the app's (blend SRC_ALPHA / ONE_MINUS_SRC_ALPHA, depth mask off, target 0 only); the frame's depth cleared to a
 * slab at view z -10 (between particle depths). Air off (fogOn 0, volOn 0, atAp 0). Lit: fxLitMode 2, cellInterior 1, the
 * 3 placed lights of the cell gate (8 RGBA32F texels a light) on cellLights.
 * Cases: 0 `leq` (LEQUAL), 1 `always` (ALWAYS, fxSoft 1 + pfxGame 2: fxDepth a shared D32F target holding a 32-texel checker
 * of view z -8 / -12), 2 `hard` (ALWAYS, soft off), 3 `write` (LEQUAL with depth writes on), 4 `nocol` (the colour array
 * absent: GL's generic attribute).
 * BAR (stated before the first run, never widened): depth EXACT (DepthCmp exact). Colour, target 0, every pixel either side
 * wrote: EXACT, or BOUND |Vulkan - GL| <= 4e-3 * max(1, |GL|) per channel. Covered: >= 2000 pixels GL wrote. Crossing: 1 run,
 * 0 failed. Seen-checks (each >= 1000 pixels of GL's colour or depth move, else the case's reds test nothing): lights
 * (cellLightCount 0), soft (fxSoft 0, case always), depth func (LEQUAL vs ALWAYS, cases leq and hard). Repeat: a second GL draw and a second
 * Vulkan run, each EXACT against its first.
 * Reds (WW_VKVIEW_OBJPFX_RED through objPfxRed; each must FAIL against GL and move the Vulkan colour off the Vulkan base):
 * offset (the strip stage reads its records 16 bytes on), light (the light buffer copied from one texel on), depth (ALWAYS as LEQUAL,
 * case hard only). A refused red is "not judged" with its reason, never a pass.
 * Round 4, the maps (a .bgem's, glparticles.cpp binds each with its flag): 5 `normal` (hasNormalMap 1, useFalloff 1,
 * NormalMap = a DXT1 twin on its own unit), 6 `grey` (greyscaleColor 1, hasGreyscaleMap 1, GreyscaleMap = a DXT1 twin),
 * 7 `cube` (hasCubeMap 1, envReflection 1; round 5: CubeMap = the .bgem's env cube, 8x8 DXT1 with 4 levels, drawn on Vulkan
 * through its plain-cube copy twin under the same bar, 0 refused, >= 1 copy twin; envMapRotation a half turn to the
 * (1,1,1) corner, GL_TEXTURE_CUBE_MAP_SEAMLESS on as bindCube leaves it; a printed per-face probe counts the faces reached),
 * 8 `elem` (textures[0].textureUnit 3: textureUnits[2] a DXT1 twin on its own unit). Seen-check 4: the case's map off
 * (its flag 0, textureUnit 1). Reds 4 `nmap`, 5 `grey` (the map a stand-in with its flag 1), 6 `cubeoff` (CubeMap a
 * stand-in with hasCubeMap 1), 7 `elem0` (only element 0 of an array resolved, round 3's rule), each on its own case;
 * round 5: 8 `cubegrey` on `cube` (the gate binds the grey stand-in cube where GL drew the env cube: must FAIL).
 * Round 5 item 3, the RT arm (rt true; `pfx rt` lines): particles_cell.frag@...+WW_CELL_RESTIR+WW_CELL_RTDDGI+
 * WW_CELL_RTDDGI_CASCADES+... against objpfx_cell_rt_ps, case leq only. The system outdoors on the volume law (cellInterior 0,
 * vpSun (0, 0, 0, 1): the local law with no sun; volPart 1, volTerms 2: the placed lights through cellLightE, each shadowed
 * (texel 4 .y 1, cellShadowOn 1); the air's phase isotropic (volPhase[0] and volGi 1: at 0 every lamp is 0); cellRow the identity) with the cell gate's RT inputs (blview_cell.inc): cellRestir RG32F
 * (ratio, depth 9), one DDGI cascade in a texel store, the screen GI's half target and its key from GL's own `write` depth.
 * Seen-checks: cellRestirOn 0, cellDdgiOn 0, cellGiHalfOn 0 each move >= 1000 GL pixels. Reds 9 restir, 10 atlas (each
 * its texel buffer read one texel on), 11 gihalf, 12 gikey (each binding dropped).
 * Not judged here: stencil (off at the app's draw: StencilProperty::glProperty( nullptr ) runs after glparticles.cpp's
 * write setup, and FO4 particles carry no NiStencilProperty), the air's textures (off by uniform), the probe passes. */
bool pfxGate( const std::string & spirvDir, GLuint baseTex, GLuint nrmTex, GLuint greyTex, unsigned reds = 0xFFu, bool rt = false )
{
	using namespace WwVkPresent;
	struct PfxGl
	{
		void( WINAPI * shaderBinary )( GLsizei, const GLuint *, GLenum, const void *, GLsizei ) = nullptr;
		void( WINAPI * specialize )( GLuint, const char *, GLuint, const GLuint *, const GLuint * ) = nullptr;
		void( WINAPI * ifaceIv )( GLuint, GLenum, GLenum, GLint * ) = nullptr;
		void( WINAPI * resIv )( GLuint, GLenum, GLuint, GLsizei, const GLenum *, GLsizei, GLsizei *, GLint * ) = nullptr;
		void( WINAPI * uf[4] )( GLint, GLsizei, const GLfloat * ) = {};
		void( WINAPI * ui1 )( GLint, GLint ) = nullptr;
		void( WINAPI * uu1 )( GLint, GLuint ) = nullptr;
		void( WINAPI * um4 )( GLint, GLsizei, GLboolean, const GLfloat * ) = nullptr;
		void( WINAPI * bindTextures )( GLuint, GLsizei, const GLuint * ) = nullptr;
		void( WINAPI * bindSamplers )( GLuint, GLsizei, const GLuint * ) = nullptr;
		void( WINAPI * bufSub )( GLuint, std::intptr_t, std::ptrdiff_t, const void * ) = nullptr;
		void( WINAPI * clearTexSub )( GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void * ) = nullptr;
		void( WINAPI * texParami )( GLuint, GLenum, GLint ) = nullptr;
		void( WINAPI * texBuffer )( GLenum, GLenum, GLuint ) = nullptr;
		void( WINAPI * disableAttr )( GLuint ) = nullptr;
		void( WINAPI * attrib4f )( GLuint, GLfloat, GLfloat, GLfloat, GLfloat ) = nullptr;
		void( WINAPI * compressed2D )( GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void * ) = nullptr;
		GLint( WINAPI * loc )( GLuint, const char * ) = nullptr;
	} u;
	const char * n4 = "1234";
	bool okLoad = load( u.shaderBinary, "glShaderBinary" ) && load( u.specialize, "glSpecializeShader" ) && load( u.ifaceIv, "glGetProgramInterfaceiv" )
		&& load( u.resIv, "glGetProgramResourceiv" ) && load( u.ui1, "glUniform1i" ) && load( u.uu1, "glUniform1ui" )
		&& load( u.um4, "glUniformMatrix4fv" ) && load( u.bindTextures, "glBindTextures" ) && load( u.bindSamplers, "glBindSamplers" )
		&& load( u.bufSub, "glNamedBufferSubData" ) && load( u.clearTexSub, "glClearTexSubImage" ) && load( u.texParami, "glTextureParameteri" )
		&& load( u.texBuffer, "glTexBuffer" ) && load( u.disableAttr, "glDisableVertexAttribArray" ) && load( u.attrib4f, "glVertexAttrib4f" )
		&& load( u.loc, "glGetUniformLocation" ) && load( u.compressed2D, "glCompressedTexImage2D" );
	for ( int i = 0; i < 4; i++ )
		okLoad = okLoad && load( u.uf[i], ( std::string( "glUniform" ) + n4[i] + "fv" ).c_str() );
	if ( !okLoad || !sx.fbo ) {
		std::printf( "BLVIEW pfx FAIL (GL entry points missing, or no shade targets)\n" );
		return false;
	}
	std::string why;
	if ( !objBlLocsLoad( spirvDir + "/locs.txt", why ) ) {
		std::printf( "BLVIEW pfx FAIL (no registry: %s)\n", why.c_str() );
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
	// GL's program: the three stages from the set as the app links them
	const GLuint prog = f.createProgram();
	const std::string stage[3] = { "particles.vert", "particles.geom",
		rt ? "particles_cell.frag@WW_CELL_AIRZONE+WW_CELL_FOGLEAK+WW_CELL_RESTIR+WW_CELL_RTDDGI+WW_CELL_RTDDGI_CASCADES+WW_FRAME_UBO"
		   : "particles_cell.frag@WW_CELL_AIRZONE+WW_CELL_FOGLEAK+WW_FRAME_UBO" };
	const GLenum kind[3] = { kVert, 0x8DD9u, kFrag };  // GEOMETRY_SHADER
	for ( int s = 0; s < 3; s++ ) {
		const std::string bin = index.count( stage[s] ) ? readFile( spirvDir + "/" + index[stage[s]] ) : std::string();
		const GLuint sh = f.createShader( kind[s] );
		GLint okS = 0;
		if ( !bin.empty() ) {
			u.shaderBinary( 1, &sh, 0x9551, bin.data(), GLsizei( bin.size() ) );  // SHADER_BINARY_FORMAT_SPIR_V
			u.specialize( sh, "main", 0, nullptr, nullptr );
			f.getShaderiv( sh, kCompiled, &okS );
		}
		if ( !okS ) {
			std::printf( "BLVIEW pfx FAIL (%s %s)\n", stage[s].c_str(), bin.empty() ? "is not in the set" : "did not specialize" );
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
		std::printf( "BLVIEW pfx FAIL (particles_cell from SPIR-V did not link: %s)\n", log );
		return false;
	}
	// active default-block locations and types; every sampler on its own empty unit
	std::map<int, GLint> glType;
	GLint nRes = 0;
	u.ifaceIv( prog, 0x92E1, 0x92F5, &nRes );
	for ( GLint i = 0; i < nRes; i++ ) {
		const GLenum props[4] = { 0x930E, 0x92FB, 0x92FA, 0x92FD };
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
	auto unitOf = [&]( const char * name ) {
		const GLint l = locOf( name );
		return unitAt.count( l ) ? unitAt[l] : -1;
	};
	const int baseUnit = unitOf( "textureUnits" ) >= 0 ? unitOf( "textureUnits" ) : unitOf( "textureUnits[0]" );
	const int lightUnit = unitOf( "cellLights" ), depthUnit = unitOf( "fxDepth" );
	if ( baseUnit < 0 || lightUnit < 0 || depthUnit < 0 || glGetError() != GL_NO_ERROR ) {
		std::printf( "BLVIEW pfx FAIL (units: textureUnits %d, cellLights %d, fxDepth %d; %d samplers)\n", baseUnit, lightUnit, depthUnit, units );
		return false;
	}
	// round 4: the maps' own units (used by their cases), textureUnits[2]'s, the app's grey cube
	const char * tuName = locOf( "textureUnits" ) >= 0 ? "textureUnits" : "textureUnits[0]";
	const int nrmUnit = unitOf( "NormalMap" ), greyUnit = unitOf( "GreyscaleMap" ), cubeUnit = unitOf( "CubeMap" );
	const int elemUnit = unitAt.count( locOf( tuName ) + 2 ) ? unitAt[locOf( tuName ) + 2] : -1;
	if ( nrmUnit < 0 || greyUnit < 0 || cubeUnit < 0 || elemUnit < 0 ) {
		std::printf( "BLVIEW pfx FAIL (units: NormalMap %d, GreyscaleMap %d, CubeMap %d, textureUnits[2] %d)\n", nrmUnit, greyUnit, cubeUnit, elemUnit );
		return false;
	}
	GLuint cubeTex = 0;
	glGenTextures( 1, &cubeTex );
	glBindTexture( 0x8513, cubeTex );  // TEXTURE_CUBE_MAP
	const std::uint32_t greyTexel = 0xFF555555u;
	for ( GLenum fc = 0; fc < 6; fc++ )
		glTexImage2D( 0x8515 + fc, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &greyTexel );
	glTexParameteri( 0x8513, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( 0x8513, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	// round 5: the .bgem's env cube (glparticles.cpp's bindCube( cubeName ) with hasCubeMap 1): 8x8 DXT1, 4 levels, mipmapped,
	// each face its own two end colours, the indices seeded mt19937( 37 ) (every level and texel its own value)
	// (TexCache::bindCube's state: clamp to edge, linear mipmapped, GL_TEXTURE_CUBE_MAP_SEAMLESS on). whiteFace >= 0: that
	// face solid white on every level (the per-face probe: which faces the system's lookups reach)
	auto makeEnv = [&]( int whiteFace ) {
		GLuint t = 0;
		glGenTextures( 1, &t );
		glBindTexture( 0x8513, t );
		std::mt19937 cr( 37 );
		static const std::uint16_t ends[6][2] = { { 0xF800, 0x2104 }, { 0x07E0, 0x0841 }, { 0x001F, 0x4208 }, { 0xFFE0, 0x18C3 }, { 0xF81F, 0x0000 },
			{ 0x07FF, 0x3186 } };
		for ( int l = 0; l < 4; l++ ) {
			const int lw = 8 >> l, nb = ( ( lw + 3 ) / 4 ) * ( ( lw + 3 ) / 4 );
			for ( int fc = 0; fc < 6; fc++ ) {
				std::vector<std::uint8_t> blk( std::size_t( nb ) * 8 );
				for ( int b = 0; b < nb; b++ ) {
					std::uint32_t idx = std::uint32_t( cr() );
					std::uint16_t e0 = ends[fc][0], e1 = ends[fc][1];
					if ( fc == whiteFace )
						e0 = e1 = 0xFFFF, idx = 0;
					std::memcpy( &blk[std::size_t( b ) * 8], &e0, 2 );
					std::memcpy( &blk[std::size_t( b ) * 8 + 2], &e1, 2 );
					std::memcpy( &blk[std::size_t( b ) * 8 + 4], &idx, 4 );
				}
				u.compressed2D( GLenum( 0x8515 + fc ), l, 0x83F0, lw, lw, 0, GLsizei( blk.size() ), blk.data() );
			}
		}
		for ( GLenum wr : { GLenum( GL_TEXTURE_WRAP_S ), GLenum( GL_TEXTURE_WRAP_T ), GLenum( 0x8072 ) } )  // WRAP_R
			glTexParameteri( 0x8513, wr, 0x812F );                                                                   // CLAMP_TO_EDGE
		glTexParameteri( 0x8513, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameteri( 0x8513, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glBindTexture( 0x8513, 0 );
		return t;
	};
	GLuint envCube = makeEnv( -1 );
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW pfx FAIL (GL error 0x%x making the env cube)\n", unsigned( e ) );
		return false;
	}
	bool greyCube = false;  // red 8 `cubegrey`: the Vulkan draw finds the grey stand-in where GL bound the env cube
	// the named blocks at the twins' layout, on GL's bindings (build_spv.py BLOCKS)
	struct Blk
	{
		std::string name;
		GLuint glBind = 0, buf = 0;
		const SpvBind * b = nullptr;
		std::vector<std::uint8_t> bytes;
	};
	SpvModule mods[3];
	const std::uint32_t * words[3] = { vkframe_objpfx_cs_spv, vkframe_objpfx_vs_spv, rt ? vkframe_objpfx_cell_rt_ps_spv : vkframe_objpfx_cell_ps_spv };
	const std::size_t counts[3] = { sizeof( vkframe_objpfx_cs_spv ) / 4, sizeof( vkframe_objpfx_vs_spv ) / 4,
		rt ? sizeof( vkframe_objpfx_cell_rt_ps_spv ) / 4 : sizeof( vkframe_objpfx_cell_ps_spv ) / 4 };
	std::vector<Blk> blks;
	for ( int m = 0; m < 3; m++ ) {
		if ( !spvReflect( words[m], counts[m], mods[m], why ) ) {
			std::printf( "BLVIEW pfx FAIL (the twin's module %d: %s)\n", m, why.c_str() );
			return false;
		}
		for ( const SpvBind & b : mods[m].binds ) {
			if ( b.type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || b.name.find( "LooseUniforms" ) != std::string::npos )
				continue;
			static const std::pair<const char *, GLuint> known[2] = { { "globalUniforms", 0 }, { "wwShadowFrame", 2 } };
			int k = -1;
			for ( int i = 0; i < 2; i++ )
				if ( b.name.find( known[i].first ) != std::string::npos )
					k = i;
			if ( k < 0 ) {
				std::printf( "BLVIEW pfx FAIL (the twin reads block %s, which has no GL binding here)\n", b.name.c_str() );
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
	int inactive = 0;
	std::string putBad, offNames;
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
			else if ( t == 0x1405 )
				u.uu1( at, GLuint( v[0] ) );
			else if ( t == 0x8B54 || t == 0x8B55 || t == 0x8B53 ) {  // ivec: one int a component
				GLint iv[4] = {};
				for ( std::size_t c = 0; c < vals.size() && c < 4; c++ )
					iv[c] = GLint( v[c] );
				void( WINAPI * ui4 )( GLint, GLsizei, const GLint * ) = nullptr;
				const char * nm = t == 0x8B53 ? "glUniform2iv" : t == 0x8B54 ? "glUniform3iv" : "glUniform4iv";
				if ( load( ui4, nm ) )
					ui4( at, 1, iv );
			} else
				u.ui1( at, GLint( v[0] ) );
			if ( glGetError() != GL_NO_ERROR && putBad.empty() )
				putBad = std::string( name ) + " (GL type " + std::to_string( t ) + ")";
			return;
		}
		for ( Blk & x : blks )
			for ( const SpvMember & m : x.b->members ) {
				if ( m.name != name )
					continue;
				const std::size_t at2 = m.offset + std::size_t( elem ) * m.arrayStride;
				const std::size_t n = std::min<std::size_t>( vals.size(), m.comps );
				if ( at2 + n * 4 <= x.bytes.size() )
					std::memcpy( x.bytes.data() + at2, vals.begin(), n * 4 );
				return;
			}
		if ( offNames.find( std::string( " " ) + name + " " ) == std::string::npos )
			offNames += std::string( " " ) + name + " ";
		inactive++;
	};
	const float ident[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	// round 5: envMapRotation a half turn that takes +z to the cube's (1,1,1) corner (symmetric: either storage order), so the
	// sprites' reflections (about +z) spread over three faces and their seams; only case `cube` samples it
	// round 5b: the six axis cases' half turns taking +z to face 0..5 (+x -x +y -y +z -z; symmetric too), 6 the corner
	static const float kEnvRot[7][16] = { { 0, 0, 1, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1 }, { 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1 },
		{ -1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1 }, { -1, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 }, { 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1 },
		{ -0.78867513f, 0.21132487f, 0.57735027f, 0, 0.21132487f, -0.78867513f, 0.57735027f, 0, 0.57735027f, 0.57735027f, 0.57735027f, 0, 0, 0,
			0, 1 } };
	Blk * envBlk = nullptr;
	const SpvMember * envMem = nullptr;
	for ( Blk & x : blks ) {
		if ( x.name != "globalUniforms" )
			continue;
		for ( const SpvMember & m : x.b->members ) {
			if ( m.name == "projectionMatrix" )
				shadePutMat( x.bytes, m, sx.proj );
			else if ( m.name == "viewMatrix" )
				shadePutMat( x.bytes, m, ident );
			else if ( m.name == "envMapRotation" ) {
				shadePutMat( x.bytes, m, kEnvRot[6] );
				envBlk = &x;
				envMem = &m;
			}
			else if ( m.name == "toneMapScale" || m.name == "brightnessScale" || m.name == "glowScale" || m.name == "glowScaleSRGB" ) {
				const float one = 1.0f;
				std::memcpy( x.bytes.data() + m.offset, &one, 4 );
			} else if ( m.name == "viewportDimensions" ) {
				const std::int32_t vp[4] = { 0, 0, kSW, kSH };
				std::memcpy( x.bytes.data() + m.offset, vp, 16 );
			}
		}
	}
	// the app's particle uniforms (glparticles.cpp) and the cell's (lit, air off)
	set( "modelViewMatrix", { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } );
	set( "vertexColorOverride", { 0, 0, 0, 0 } );
	set( "particleScale", { 1.0f, 1.0f } );
	set( "particleUV", { 0.0f, 0.0f, 1.0f, 1.0f } );
	set( "textures[0].uvCenter", { 0.5f, 0.5f } );
	set( "textures[0].uvScale", { 1.0f, 1.0f } );
	set( "textures[0].uvOffset", { 0.0f, 0.0f } );
	set( "textures[0].uvRotation", { 0.0f } );
	set( "textures[0].coordSet", { 0 } );
	set( "textures[0].textureUnit", { 1 } );
	set( "glowColor", { 1.0f, 0.9f, 0.8f, 1.0f } );
	set( "glowMult", { 1.0f } );
	set( "falloffParams", { 1.0f, 0.0f, 1.0f, 1.0f } );
	set( "cellOn", { 1 } );
	set( "cellIsLinear", { 1 } );
	set( "cellInterior", { 1 } );
	set( "fxLitMode", { 2 } );
	set( "lightingInfluence", { 1.0f } );
	set( "fogOn", { 0 } );
	set( "volOn", { 0 } );
	set( "atAp", { 0, 0, 0, 0 } );
	set( "fxDistScale", { 1.0f } );
	set( "fxSoftDepth", { 2.0f } );
	// the placed lights: the cell gate's three (omni, spot, omni with no specular), view space
	const float lt[3][8][4] = {
		{ { -3.0f, 1.5f, -6.0f, 8.0f }, { 1.2f, 0.9f, 0.6f, -2.0f }, { 0, 0, -1, 1 }, { 0, 1, 2, 0 }, { -1, 0, 0, 0 } },
		{ { 2.0f, 0.0f, -9.0f, 9.0f }, { 0.6f, 0.9f, 1.4f, 0.5f }, { 0, 0, -1, 2 }, { 0, 1, 2, 0 }, { -1, 0, 0, 0 } },
		{ { 3.0f, -1.0f, -12.0f, 9.0f }, { 0.8f, 1.3f, 0.7f, -2.0f }, { 0, 0, -1, 1 }, { 0, 1, 2, 1 }, { -1, 0, 0, 0 } } };
	GLuint lightBuf = 0, lightTex = 0;
	f.genBuffers( 1, &lightBuf );
	f.bindBuffer( 0x8C2A, lightBuf );
	f.bufferData( 0x8C2A, std::ptrdiff_t( sizeof( lt ) ), lt, 0x88E4 );
	f.bindBuffer( 0x8C2A, 0 );
	glGenTextures( 1, &lightTex );
	glBindTexture( 0x8C2A, lightTex );
	u.texBuffer( 0x8C2A, kRgba32fi, lightBuf );
	glBindTexture( 0x8C2A, 0 );
	// round 5 item 3 (rt): the RT inputs as the cell gate makes them (blview_cell.inc), the system on the volume law outdoors
	static int pfxGiHalfAt = 0, pfxGiKeyAt = 0;
	const std::uintptr_t giHalfKey = reinterpret_cast<std::uintptr_t>( &pfxGiHalfAt ), giKeyKey = reinterpret_cast<std::uintptr_t>( &pfxGiKeyAt );
	GLuint restirTex = 0, restirBuf = 0, atlasTex = 0, giHalfTex = 0, giKeyTex = 0;
	unsigned atlasBuf = 0;
	int restirUnit = -1, atlasUnit = -1, giHalfUnit = -1, giKeyUnit = -1;
	const int hw = kSW / 2, hh = kSH / 2;
	if ( rt ) {
		float ls[3][8][4];
		std::memcpy( ls, lt, sizeof( lt ) );
		for ( auto & l : ls )
			l[4][1] = 1.0f;  // shadowed: cellShadowF takes the pixel's traced ratio (cellRestir)
		f.bindBuffer( 0x8C2A, lightBuf );
		f.bufferData( 0x8C2A, std::ptrdiff_t( sizeof( ls ) ), ls, 0x88E4 );
		f.bindBuffer( 0x8C2A, 0 );
		std::vector<float> rs( std::size_t( kSW ) * std::size_t( kSH ) * 2 );
		for ( int y = 0; y < kSH; y++ )
			for ( int x = 0; x < kSW; x++ ) {
				const std::size_t p = ( std::size_t( y ) * std::size_t( kSW ) + std::size_t( x ) ) * 2;
				rs[p] = 0.1f + 0.09f * float( ( x * 7 + y * 3 ) % 11 );
				rs[p + 1] = 9.0f;
			}
		f.genBuffers( 1, &restirBuf );
		f.bindBuffer( 0x8C2A, restirBuf );
		f.bufferData( 0x8C2A, std::ptrdiff_t( rs.size() * 4 ), rs.data(), 0x88E4 );
		f.bindBuffer( 0x8C2A, 0 );
		glGenTextures( 1, &restirTex );
		glBindTexture( 0x8C2A, restirTex );
		u.texBuffer( 0x8C2A, 0x8230, restirBuf );  // RG32F
		glBindTexture( 0x8C2A, 0 );
		const int nP = 13 * 7 * 9;
		std::vector<float> at( std::size_t( 193 * nP ) * 4, 0.0f );
		std::mt19937 ar( 31 );
		std::uniform_real_distribution<float> a01( 0.0f, 1.0f );
		for ( std::size_t t = 0; t < std::size_t( 64 * nP ); t++ )
			for ( int c = 0; c < 4; c++ )
				at[t * 4 + std::size_t( c )] = c < 3 ? 0.3f + 0.6f * a01( ar ) : 0.4f + 0.6f * a01( ar );
		for ( std::size_t t = std::size_t( 64 * nP ); t < std::size_t( 192 * nP ); t++ ) {
			at[t * 4] = at[t * 4 + 2] = 1000.0f;
			at[t * 4 + 1] = at[t * 4 + 3] = 1.0e6f;
		}
		for ( std::size_t t = std::size_t( 192 * nP ); t < std::size_t( 193 * nP ); t++ )
			at[t * 4 + 3] = 2.0f;
		const std::size_t aBytes = at.size() * 4;
		if ( !objBlTexelStore( atlasBuf, aBytes, why ) || !objBlTexelStoreWrite( atlasBuf, 0, aBytes, at.data(), why ) ) {
			std::printf( "BLVIEW pfx rt FAIL (the atlas store: %s)\n", why.c_str() );
			return false;
		}
		glGenTextures( 1, &atlasTex );
		glBindTexture( 0x8C2A, atlasTex );
		u.texBuffer( 0x8C2A, kRgba32fi, GLuint( atlasBuf ) );
		glBindTexture( 0x8C2A, 0 );
		if ( !sharedTarget( giHalfKey, 0, hw, hh, 1, kRgba32fi, giHalfTex, why ) || !sharedTarget( giKeyKey, 0, hw, hh, 1, kRgba32fi, giKeyTex, why ) ) {
			std::printf( "BLVIEW pfx rt FAIL (the screen GI's shared targets: %s)\n", why.c_str() );
			return false;
		}
		std::vector<float> gh( std::size_t( hw ) * std::size_t( hh ) * 4 ), gk( gh.size(), -1.0f );
		for ( int y = 0; y < hh; y++ )
			for ( int x = 0; x < hw; x++ ) {
				float * p = gh.data() + ( std::size_t( y ) * std::size_t( hw ) + std::size_t( x ) ) * 4;
				p[0] = 0.2f + 0.075f * float( ( x + 2 * y ) % 9 );
				p[1] = 0.2f + 0.1f * float( ( 3 * x + y ) % 7 );
				p[2] = 0.3f + 0.125f * float( ( x * y ) % 5 );
				p[3] = 0.5f + 0.25f * float( ( x + y ) % 3 );
			}
		for ( int k = 0; k < 2; k++ ) {
			glBindTexture( GL_TEXTURE_2D, k ? giKeyTex : giHalfTex );
			glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, hw, hh, GL_RGBA, GL_FLOAT, k ? gk.data() : gh.data() );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		}
		glBindTexture( GL_TEXTURE_2D, 0 );
		const char * nm[4] = { "cellRestir", "cellDdgiAtlas", "cellGiHalf", "cellGiHalfKey" };
		int * un[4] = { &restirUnit, &atlasUnit, &giHalfUnit, &giKeyUnit };
		for ( int k = 0; k < 4; k++ ) {
			*un[k] = unitOf( nm[k] );
			if ( *un[k] < 0 ) {
				std::printf( "BLVIEW pfx rt FAIL (%s has no unit: location %d)\n", nm[k], int( locOf( nm[k] ) ) );
				return false;
			}
		}
		set( "cellInterior", { 0 } );
		set( "vpSun", { 0.0f, 0.0f, 0.0f, 1.0f } );  // the local law (fxLocalOn), no sun
		set( "volPart", { 1 } );
		set( "volTerms", { 2 } );  // the placed lights
		set( "volPhase", { 1.0f, 1.0f, 1.0f, 0.0f }, 0 );  // the air's phase (isotropic weights): 0 leaves vpPhase, so every lamp, at 0
		set( "volGi", { 1.0f, 1.0f, 1.0f, 0.0f } );
		set( "cellRow", { 1, 0, 0, 0 }, 0 );  // view = world
		set( "cellRow", { 0, 1, 0, 0 }, 1 );
		set( "cellRow", { 0, 0, 1, 0 }, 2 );
		set( "cellShadowOn", { 1 } );
		set( "cellRestirOn", { 1 } );
		set( "cellRestirSize", { float( kSW ), float( kSH ), 0.0f, 0.0f } );
		set( "cellDdgiOn", { 1 } );
		set( "cellDdgiCount", { 1 } );
		set( "cellDdgiCOrigin", { -12.0f, -6.0f, -18.0f, 2.0f }, 0 );
		set( "cellDdgiCStep", { 2.0f, 2.0f, 2.0f, 0.1f }, 0 );
		set( "cellDdgiCDims", { 13, 7, 9, float( nP ) }, 0 );
		set( "cellDdgiCBase", { 0 }, 0 );
		set( "cellDdgiRed", { 0 } );
		set( "cellGiLoop8", { 8 } );
		set( "cellGiHalfOn", { 0 } );  // on once the key holds GL's surfaces
		set( "cellGiHalfRect", { 0.0f, 0.0f, float( hw ), float( hh ) } );
		set( "cellGiHalfScale", { 0.5f } );
		if ( glGetError() != GL_NO_ERROR ) {
			std::printf( "BLVIEW pfx rt FAIL (a GL error making the RT inputs)\n" );
			return false;
		}
	}
	// fxDepth: a shared D32F target, a 32-texel checker of view z -8 / -12 as window depth
	static int depthKeyAt = 0;
	GLuint depthTex = 0;
	if ( !sharedTarget( reinterpret_cast<std::uintptr_t>( &depthKeyAt ), 0, kSW, kSH, 1, 0x8CAC, depthTex, why ) ) {
		std::printf( "BLVIEW pfx FAIL (the shared fxDepth: %s)\n", why.c_str() );
		return false;
	}
	auto winDepth = [&]( float zv ) {  // GL: ndc z = (P[10] zv + P[14]) / -zv, window = ndc / 2 + 1 / 2
		return ( ( sx.proj[10] * zv + sx.proj[14] ) / -zv ) * 0.5f + 0.5f;
	};
	const char * flatZ = std::getenv( "BLVIEW_PFX_FLATZ" );  // diagnostic split only: one view z everywhere, no checker
	for ( int by = 0; by < ( kSH + 31 ) / 32; by++ )
		for ( int bx = 0; bx < ( kSW + 31 ) / 32; bx++ ) {
			const float d = winDepth( flatZ ? float( std::atof( flatZ ) ) : ( ( bx + by ) & 1 ) != 0 ? -8.0f : -12.0f );
			u.clearTexSub( depthTex, 0, bx * 32, by * 32, 0, std::min( 32, kSW - bx * 32 ), std::min( 32, kSH - by * 32 ), 1, 0x1902, GL_FLOAT, &d );
		}
	u.texParami( depthTex, 0x2801, 0x2600 );  // NEAREST
	u.texParami( depthTex, 0x2800, 0x2600 );
	{  // what GL holds in fxDepth: two texels read back against the values written
		std::vector<float> rb( std::size_t( kSW ) * kSH, -1.0f );
		glBindTexture( GL_TEXTURE_2D, depthTex );
		glGetTexImage( GL_TEXTURE_2D, 0, 0x1902, GL_FLOAT, rb.data() );
		glBindTexture( GL_TEXTURE_2D, 0 );
		std::printf( "BLVIEW pfx fxDepth read back: texel (80, 0) %.7g, (16, 16) %.7g; written %.7g (z -12) and %.7g (z -8)\n", double( rb[80] ),
			double( rb[16 * kSW + 16] ), double( winDepth( -12.0f ) ), double( winDepth( -8.0f ) ) );
	}
	// the system: 203 points, the app's sequential arrays
	const int np = 203;
	std::vector<float> arr[5];
	std::mt19937 r( 31 );
	std::uniform_real_distribution<float> q( 0.0f, 1.0f );
	for ( int i = 0; i < np; i++ ) {
		for ( float v : { -4.0f + 8.0f * q( r ), -3.0f + 6.0f * q( r ), -5.0f - 10.0f * q( r ) } )
			arr[0].push_back( v );
		for ( float v : { 0.3f + 0.7f * q( r ), 0.3f + 0.7f * q( r ), 0.3f + 0.7f * q( r ), 0.4f + 0.6f * q( r ) } )
			arr[1].push_back( v );
		for ( float v : { 0.25f * float( int( 4.0f * q( r ) ) ), 0.0f } )
			arr[2].push_back( v );
		arr[3].push_back( 6.2831853f * q( r ) );
		arr[4].push_back( 0.4f + 1.2f * q( r ) );
	}
	const int comps[5] = { 3, 4, 2, 1, 1 };
	GLuint vbo = 0, vao = 0;
	std::vector<std::uint8_t> bytes;
	std::size_t offs[5] = {};
	for ( int a = 0; a < 5; a++ ) {
		offs[a] = bytes.size();
		const std::size_t at = bytes.size();
		bytes.resize( at + arr[a].size() * 4 );
		std::memcpy( bytes.data() + at, arr[a].data(), arr[a].size() * 4 );
	}
	f.genBuffers( 1, &vbo );
	f.genVertexArrays( 1, &vao );
	f.bindVertexArray( vao );
	f.bindBuffer( 0x8892, vbo );
	f.bufferData( 0x8892, std::ptrdiff_t( bytes.size() ), bytes.data(), 0x88E4 );
	for ( int a = 0; a < 5; a++ ) {
		sf.vertexAttribPointer( GLuint( a ), comps[a], GL_FLOAT, GL_FALSE, 0, reinterpret_cast<const void *>( offs[a] ) );
		sf.enableVertexAttribArray( GLuint( a ) );
	}
	f.bindVertexArray( 0 );
	f.bindBuffer( 0x8892, 0 );
	struct PfxCase
	{
		const char * name;
		GLenum func;
		bool soft, write, noCol;
		int map;       //!< round 4: 1 normal, 2 grey, 3 cube, 4 elem (0 none)
		int face = 6;  //!< round 5b: envMapRotation kEnvRot[face] (0..5 the axis cases, 6 the corner)
	};
	// `always` is the app's soft case: the soft fade already zeroes every fragment behind fxDepth, so the depth func moves
	// nothing there (first run: 0 pixels). `hard` (ALWAYS, soft off) is where the func, and its red, can be seen.
	const PfxCase cases[15] = { { "leq", GL_LEQUAL, false, false, false, 0 }, { "always", GL_ALWAYS, true, false, false, 0 },
		{ "hard", GL_ALWAYS, false, false, false, 0 }, { "write", GL_LEQUAL, false, true, false, 0 }, { "nocol", GL_LEQUAL, false, false, true, 0 },
		{ "normal", GL_LEQUAL, false, false, false, 1 }, { "grey", GL_LEQUAL, false, false, false, 2 }, { "cube", GL_LEQUAL, false, false, false, 3 },
		{ "elem", GL_LEQUAL, false, false, false, 4 }, { "cube+x", GL_LEQUAL, false, false, false, 3, 0 }, { "cube-x", GL_LEQUAL, false, false, false, 3, 1 },
		{ "cube+y", GL_LEQUAL, false, false, false, 3, 2 }, { "cube-y", GL_LEQUAL, false, false, false, 3, 3 },
		{ "cube+z", GL_LEQUAL, false, false, false, 3, 4 }, { "cube-z", GL_LEQUAL, false, false, false, 3, 5 } };
	int probe = 0;  // 1 lights off, 2 soft off, 3 the depth func flipped (LEQUAL <-> ALWAYS), 4 the case's map off
	auto frameState = [&]( const PfxCase & c ) {
		f.bindFramebuffer( kFb, sx.fbo );
		const GLenum dbs[1] = { kColorAtt0 };
		sf.drawBuffers( 1, dbs );
		glViewport( 0, 0, kSW, kSH );
		glDisable( GL_SCISSOR_TEST );
		glDisable( GL_STENCIL_TEST );
		glEnable( GL_CULL_FACE );  // the app's: StencilProperty::glProperty( nullptr )
		glCullFace( GL_BACK );
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glEnable( GL_DEPTH_TEST );
		glEnable( 0x884F );  // TEXTURE_CUBE_MAP_SEAMLESS: on from glparticles.cpp's first bindCube (Vulkan's cubes always are)
		const GLenum func = probe == 3 ? ( c.func == GL_ALWAYS ? GL_LEQUAL : GL_ALWAYS ) : c.func;
		glDepthFunc( func );
		glDepthMask( c.write ? GL_TRUE : GL_FALSE );
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		f.useProgram( prog );
		set( "cellLightCount", { probe == 1 ? 0.0f : 3.0f } );
		set( "fxSoft", { c.soft && probe != 2 ? 1.0f : 0.0f } );
		set( "pfxGame", { c.soft ? 2.0f : 0.0f } );
		// round 4: the maps as glparticles.cpp sets them (the sampler uniforms on the sprite's unit unless the case binds its map)
		const bool mapOn = c.map != 0 && probe != 4;
		set( "hasNormalMap", { c.map == 1 && mapOn ? 1.0f : 0.0f } );
		set( "useFalloff", { c.map == 1 ? 1.0f : 0.0f } );
		if ( c.map == 1 )
			set( "falloffParams", { 0.0f, 1.0f, 0.2f, 1.0f } );
		else
			set( "falloffParams", { 1.0f, 0.0f, 1.0f, 1.0f } );
		set( "greyscaleColor", { c.map == 2 ? 1.0f : 0.0f } );
		set( "hasGreyscaleMap", { c.map == 2 && mapOn ? 1.0f : 0.0f } );
		set( "hasCubeMap", { c.map == 3 && mapOn ? 1.0f : 0.0f } );
		set( "envReflection", { c.map == 3 ? 1.0f : 0.0f } );
		set( "textures[0].textureUnit", { c.map == 4 && mapOn ? 3.0f : 1.0f } );
		set( "NormalMap", { float( c.map == 1 ? nrmUnit : baseUnit ) } );
		set( "GreyscaleMap", { float( c.map == 2 ? greyUnit : baseUnit ) } );
		set( "CubeMap", { float( cubeUnit ) } );
		for ( int e = 0; e < 10; e++ )
			set( tuName, { float( c.map == 4 && e == 2 ? elemUnit : baseUnit ) }, e );
		if ( envBlk )
			shadePutMat( envBlk->bytes, *envMem, kEnvRot[c.face] );
		for ( Blk & x : blks ) {
			u.bufSub( x.buf, 0, std::ptrdiff_t( x.bytes.size() ), x.bytes.data() );
			f.bindBufferBase( 0x8A11, x.glBind, x.buf );
		}
		u.bindTextures( 0, 64, nullptr );
		u.bindSamplers( 0, 64, nullptr );
		u.bindTextures( GLuint( baseUnit ), 1, &baseTex );
		u.bindTextures( GLuint( lightUnit ), 1, &lightTex );
		u.bindTextures( GLuint( depthUnit ), 1, &depthTex );
		u.bindTextures( GLuint( cubeUnit ), 1, c.map == 3 && mapOn && !greyCube ? &envCube : &cubeTex );
		if ( c.map == 1 )
			u.bindTextures( GLuint( nrmUnit ), 1, &nrmTex );
		if ( c.map == 2 )
			u.bindTextures( GLuint( greyUnit ), 1, &greyTex );
		if ( c.map == 4 )
			u.bindTextures( GLuint( elemUnit ), 1, &greyTex );
		if ( rt ) {
			u.bindTextures( GLuint( restirUnit ), 1, &restirTex );
			u.bindTextures( GLuint( atlasUnit ), 1, &atlasTex );
			u.bindTextures( GLuint( giHalfUnit ), 1, &giHalfTex );
			u.bindTextures( GLuint( giKeyUnit ), 1, &giKeyTex );
		}
		f.bindVertexArray( vao );
		if ( c.noCol ) {
			u.disableAttr( 1 );
			u.attrib4f( 1, 0.9f, 0.6f, 0.4f, 0.8f );
		} else
			sf.enableVertexAttribArray( 1 );
	};
	auto frameOff = [&]() {
		f.bindVertexArray( 0 );
		glDisable( GL_BLEND );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_CULL_FACE );
		glDisable( 0x884F );
		glDepthMask( GL_TRUE );
		f.useProgram( 0 );
		f.bindFramebuffer( kFb, 0 );
	};
	struct PfxOut
	{
		std::vector<float> depth;
		std::vector<std::uint16_t> c0;
	};
	const float slab = winDepth( -10.0f );
	auto clearSlab = [&]() {
		shadeClear();
		f.bindFramebuffer( kFb, sx.fbo );
		glDepthMask( GL_TRUE );
		glClearDepth( double( slab ) );
		glClear( GL_DEPTH_BUFFER_BIT );
		glClearDepth( 1.0 );
		f.bindFramebuffer( kFb, 0 );
	};
	auto glDraw = [&]( const PfxCase & c, PfxOut & out ) {
		clearSlab();
		frameState( c );
		glDrawArrays( GL_POINTS, 0, np );
		frameOff();
		shadeRead( out.depth, &out.c0, nullptr );
		glFinish();
		return glGetError();
	};
	auto vkDraw = [&]( const PfxCase & c, int red, PfxOut & out, std::string & w ) {
		clearSlab();
		greyCube = red == 8;
		frameState( c );
		greyCube = false;
		ObjPfxShade s;
		s.prog = prog;
		s.pos = arr[0].data();
		s.col = c.noCol ? nullptr : arr[1].data();
		s.uv = arr[2].data();
		s.angle = arr[3].data();
		s.size = arr[4].data();
		s.n = std::size_t( np );
		s.red = red == 8 ? 0 : red;  // red 8 is the gate's own (the binding), objPfxShade runs as shipped
		s.rt = rt;
		bool ran = objPfxShade( glProc, s, w );
		frameOff();
		if ( ran )
			shadeRead( out.depth, &out.c0, nullptr );
		glFinish();
		const GLenum e = glGetError();
		if ( ran && e != GL_NO_ERROR )
			w = "GL error " + std::to_string( e ) + " at the crossing";
		return ran && e == GL_NO_ERROR;
	};
	struct PfxCmp
	{
		std::size_t cov = 0, differ = 0, over = 0, first = 0;
		float maxRel = 0.0f;
	};
	auto colCmp = [&]( const PfxOut & g, const PfxOut & v ) {
		PfxCmp c;
		for ( std::size_t i = 0; i * 4 < g.c0.size() && i * 4 < v.c0.size(); i++ ) {
			bool any = false;
			for ( int ch = 0; ch < 4; ch++ )
				any = any || g.c0[i * 4 + ch] != 0 || v.c0[i * 4 + ch] != 0;
			if ( !any )
				continue;
			c.cov++;
			for ( int ch = 0; ch < 4; ch++ ) {
				const std::uint16_t hg = g.c0[i * 4 + ch], hv = v.c0[i * 4 + ch];
				if ( hg == hv )
					continue;
				c.differ++;
				const float a = fromHalf( hv ), b = fromHalf( hg );
				const float rel = std::fabs( a - b ) / std::max( 1.0f, std::fabs( b ) );
				if ( !( rel <= 4e-3f ) ) {
					if ( c.over == 0 )
						c.first = i;
					c.over++;
				}
				if ( rel > c.maxRel || rel != rel )
					c.maxRel = rel != rel ? 1e30f : rel;
			}
		}
		return c;
	};
	auto moved = [&]( const PfxOut & a, const PfxOut & b ) {
		std::size_t n = 0;
		for ( std::size_t i = 0; i < a.depth.size() && i < b.depth.size(); i++ ) {
			bool mv = a.depth[i] != b.depth[i];
			for ( int ch = 0; ch < 4 && i * 4 + 3 < a.c0.size(); ch++ )
				mv = mv || a.c0[i * 4 + ch] != b.c0[i * 4 + ch];
			n += mv;
		}
		return n;
	};
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR || !putBad.empty() ) {
		std::printf( "BLVIEW pfx FAIL (GL error 0x%x setting the state; first bad value %s)\n", unsigned( e ), putBad.c_str() );
		return false;
	}
	{  // GL warm-up: the first draw of a three-stage program is the driver's first use
		PfxOut warm;
		glDraw( cases[0], warm );
	}
	if ( rt ) {  // the screen GI's key: GL's surfaces (case `write`'s depth, the screen GI off; cellRow the identity), normal +z
		const PfxCase wr = { "write", GL_LEQUAL, false, true, false, 0 };
		PfxOut g0;
		glDraw( wr, g0 );
		std::vector<float> gk( std::size_t( hw ) * std::size_t( hh ) * 4, -1.0f );
		std::size_t keyed = 0;
		for ( int y = 0; y < hh; y++ )
			for ( int x = 0; x < hw; x++ ) {
				const int px = 2 * x, py = 2 * y;
				const float d = g0.depth[std::size_t( py ) * std::size_t( kSW ) + std::size_t( px )];
				if ( !( d < 1.0f ) )
					continue;
				const float wv = sx.proj[14] / ( 2.0f * d - 1.0f + sx.proj[10] );
				const float nx = 2.0f * ( float( px ) + 0.5f ) / float( kSW ) - 1.0f, ny = 2.0f * ( float( py ) + 0.5f ) / float( kSH ) - 1.0f;
				float * p = gk.data() + ( std::size_t( y ) * std::size_t( hw ) + std::size_t( x ) ) * 4;
				p[0] = nx * wv / sx.proj[0];
				p[1] = ny * wv / sx.proj[5];
				p[2] = -wv;
				p[3] = 1024.0f * 2048.0f + 1024.0f;  // cellGiKeyNormal: (0, 0, 1)
				keyed++;
			}
		glBindTexture( GL_TEXTURE_2D, giKeyTex );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, hw, hh, GL_RGBA, GL_FLOAT, gk.data() );
		glBindTexture( GL_TEXTURE_2D, 0 );
		set( "cellGiHalfOn", { 1 } );
		std::printf( "BLVIEW pfx rt RT inputs: ratio %d x %d, atlas %u B (a store), screen GI %d x %d, %zu key texels on GL's surfaces\n", kSW, kSH,
			unsigned( 193 * 13 * 7 * 9 * 16 ), hw, hh, keyed );
	}
	std::printf( "BLVIEW %s state: %d samplers on their own units, %d values with no active location or member (%s), blocks:", rt ? "pfx rt" : "pfx", units, inactive,
		offNames.c_str() );
	for ( const Blk & x : blks )
		std::printf( " %s@%u (%zu B)", x.name.c_str(), x.glBind, x.bytes.size() );
	std::printf( "\n" );
	const char * caseOnly = std::getenv( "BLVIEW_PFX_CASES" );  // gates only: a case list while a first run is localised
	bool ok = true, cubeRan = false;
	bool faceCov[6] = {};  // round 5b: the faces any cube case moves >= 1000 pixels on
	for ( const PfxCase & c : cases ) {
		if ( caseOnly && std::string( caseOnly ).find( c.name ) == std::string::npos )
			continue;
		if ( rt && c.name != std::string( "leq" ) )
			continue;  // the RT arm: case leq only
		const std::string tag = std::string( rt ? "pfx rt " : "pfx " ) + c.name;
		PfxOut gl, gl2;
		if ( const GLenum e = glDraw( c, gl ); e != GL_NO_ERROR ) {
			std::printf( "BLVIEW %s FAIL (GL error 0x%x in GL's draw)\n", tag.c_str(), unsigned( e ) );
			ok = false;
			continue;
		}
		glDraw( c, gl2 );
		const std::size_t glRep = moved( gl, gl2 );
		// the seen-checks: each input reaches GL's picture
		bool seen = true;
		for ( int pr = 1; pr <= 4 && !rt; pr++ ) {
			if ( ( pr == 2 && !c.soft ) || ( pr == 4 && !c.map ) )
				continue;
			if ( pr == 3 && c.name != std::string( "leq" ) && c.name != std::string( "hard" ) )
				continue;
			PfxOut g2;
			probe = pr;
			glDraw( c, g2 );
			probe = 0;
			const std::size_t mv = moved( gl, g2 );
			static const char * pn[5] = { "", "lights (cellLightCount 0)", "soft (fxSoft 0)", "depth func (LEQUAL <-> ALWAYS)",
				"map (its flag 0 / textureUnit 1)" };
			std::printf( "BLVIEW %s GL %s probe %s: moves %zu pixels (bar >= 1000)\n", tag.c_str(), pn[pr], mv >= 1000 ? "OK" : "FAIL", mv );
			seen = seen && mv >= 1000;
		}
		if ( rt )  // round 5 item 3: each RT input reaches GL's picture (else its red could test nothing)
			for ( const char * nm : { "cellRestirOn", "cellDdgiOn", "cellGiHalfOn" } ) {
				PfxOut g2;
				set( nm, { 0 } );
				glDraw( c, g2 );
				set( nm, { 1 } );
				const std::size_t mv = moved( gl, g2 );
				std::printf( "BLVIEW %s GL RT probe %s: %s 0 moves %zu pixels (bar >= 1000)\n", tag.c_str(), mv >= 1000 ? "OK" : "FAIL", nm, mv );
				seen = seen && mv >= 1000;
			}
		bool faceOk = true;
		if ( c.map == 3 ) {  // round 5: which env cube faces GL's picture reads (one face white at a time); 5b: an axis case needs its own
			const GLuint keep = envCube;
			int reached = 0;
			std::string per;
			for ( int fc = 0; fc < 6; fc++ ) {
				envCube = makeEnv( fc );
				PfxOut g2;
				glDraw( c, g2 );
				glDeleteTextures( 1, &envCube );
				const std::size_t mv = moved( gl, g2 );
				reached += mv > 0;
				faceCov[fc] = faceCov[fc] || mv >= 1000;
				per += ( fc ? " " : "" ) + std::to_string( mv );
				if ( fc == c.face )
					faceOk = mv >= 1000;
			}
			envCube = keep;
			std::printf( "BLVIEW %s GL faces reached: %d of 6 (pixels moved with face 0..5 white: %s)%s\n", tag.c_str(), reached, per.c_str(),
				c.face < 6 ? faceOk ? "; its own face OK (bar >= 1000)" : "; its own face FAIL (bar >= 1000)" : "" );
		}
		takeObjBlRunStat();
		PfxOut vk0, vk1;
		std::string w;
		const bool ran = vkDraw( c, 0, vk0, w );
		const ObjBlRunStat rs = takeObjBlRunStat();
		bool caseOk = seen && glRep == 0 && faceOk;
		if ( c.map == 3 ) {
			// round 5: the env cube through its plain-cube copy twin; the app's `vk pfx:` refusal count for this system must be 0
			std::printf( "BLVIEW %s vk pfx: 1 system, %d drawn, %d refused (%s); %d copy twins, %llu bytes\n", tag.c_str(), ran ? 1 : 0, ran ? 0 : 1,
				ran ? "CubeMap through its cube twin" : w.c_str(), rs.copies, static_cast<unsigned long long>( rs.copyBytes ) );
			caseOk = caseOk && ran && rs.copies >= 1;
		}
		if ( !ran ) {
			std::printf( "BLVIEW %s FAIL (objPfxShade refused: %s)\n", tag.c_str(), w.c_str() );
			caseOk = false;
		} else {
			std::string w1;
			const bool ran1 = vkDraw( c, 0, vk1, w1 );
			const std::size_t vkRep = ran1 ? moved( vk0, vk1 ) : std::size_t( -1 );
			const DepthCmp dc = depthCmp( vk0.depth, gl.depth );
			const PfxCmp cc = colCmp( gl, vk0 );
			const bool crossOk = rs.runs == 1 && rs.fails == 0;
			const bool covOk = cc.cov >= 2000;
			std::printf( "BLVIEW %s crossing %s (%d runs, %d failed, %d twin images, %d shared, %d GL-only)\n", tag.c_str(), crossOk ? "OK" : "FAIL", rs.runs,
				rs.fails, rs.twins, rs.shared, rs.glOwn );
			std::printf( "BLVIEW %s repeat: GL %zu pixels moved, Vulkan %zu (both must be 0)\n", tag.c_str(), glRep, vkRep );
			std::printf( "BLVIEW %s depth vs GL %s (%zu covered, %zu on one side, %zu differ, max %.3g)\n", tag.c_str(), dc.exact() ? "EXACT" : "FAIL", dc.covered,
				dc.oneSide, dc.differ, double( dc.maxAbs ) );
			std::printf( "BLVIEW %s vs GL %s (%zu pixels written, %s; %zu values differ, %zu over the bound, max %.3g of max(1, |GL|))\n", tag.c_str(),
				cc.differ == 0 ? "EXACT" : cc.over == 0 ? "BOUND" : "FAIL", cc.cov, covOk ? "covered" : "NOT COVERED", cc.differ, cc.over, double( cc.maxRel ) );
			if ( cc.over ) {
				const std::size_t p = cc.first;
				std::printf( "BLVIEW %s first over: pixel (%zu, %zu); GL %g %g %g %g, Vulkan %g %g %g %g\n", tag.c_str(), p % kSW, p / kSW,
					double( fromHalf( gl.c0[p * 4] ) ), double( fromHalf( gl.c0[p * 4 + 1] ) ), double( fromHalf( gl.c0[p * 4 + 2] ) ),
					double( fromHalf( gl.c0[p * 4 + 3] ) ), double( fromHalf( vk0.c0[p * 4] ) ), double( fromHalf( vk0.c0[p * 4 + 1] ) ),
					double( fromHalf( vk0.c0[p * 4 + 2] ) ), double( fromHalf( vk0.c0[p * 4 + 3] ) ) );
			}
			caseOk = caseOk && crossOk && covOk && dc.exact() && cc.over == 0 && vkRep == 0 && ( c.map != 3 || cc.differ == 0 );  // 5b: cube EXACT
			// the reds: offset and light on every case, depth on `hard`
			static const char * redName[13] = { "", "offset (the strip stage reads its records 16 bytes on)", "light (the light buffer copied from one texel on)",
				"depth (ALWAYS read as LEQUAL)", "nmap (NormalMap a stand-in with hasNormalMap 1)", "grey (GreyscaleMap a stand-in with hasGreyscaleMap 1)",
				"cubeoff (CubeMap a stand-in with hasCubeMap 1)", "elem0 (only element 0 of textureUnits resolved)",
				"cubegrey (the grey stand-in cube bound in place of the env cube)", "restir (cellRestir read one texel on)",
				"atlas (cellDdgiAtlas read one texel on)", "gihalf (cellGiHalf unbound)", "gikey (cellGiHalfKey unbound)" };
			for ( int rd = rt ? 9 : 1; rd <= ( rt ? 12 : 8 ); rd++ ) {
				if ( !( reds >> ( rd - 1 ) & 1u ) || ( rd == 3 && ( c.func != GL_ALWAYS || c.soft ) ) || ( c.noCol && rd != 1 )
					|| ( !rt && ( c.map ? rd != 3 + c.map && !( c.map == 3 && rd == 8 ) : rd > 3 ) ) )
					continue;
				PfxOut o;
				std::string rw;
				const bool rr = vkDraw( c, rd, o, rw );
				if ( !rr ) {
					std::printf( "BLVIEW RED %s %s NOT JUDGED (refused: %s)\n", tag.c_str(), redName[rd], rw.c_str() );
					caseOk = false;
					continue;
				}
				const PfxCmp rc = colCmp( gl, o );
				const bool passes = depthCmp( o.depth, gl.depth ).exact() && rc.over == 0;
				const std::size_t vch = moved( o, vk0 );
				std::printf( "BLVIEW RED %s %s %s (%zu values over the bound, %zu pixels moved off the Vulkan base)\n", tag.c_str(), redName[rd],
					passes ? "PASSED" : vch ? "FAIL" : "TESTS NOTHING", rc.over, vch );
				caseOk = caseOk && !passes && vch > 0;
			}
		}
		std::printf( "BLVIEW %s %s\n", tag.c_str(), caseOk ? "OK" : "FAILED" );
		ok = ok && caseOk;
		cubeRan = cubeRan || c.map == 3;
	}
	if ( cubeRan ) {  // round 5b: the union over the cube cases
		int nCov = 0;
		for ( bool b : faceCov )
			nCov += b;
		std::printf( "BLVIEW pfx cube faces covered: %d of 6 %s\n", nCov, nCov == 6 ? "OK" : "FAIL" );
		ok = ok && ( nCov == 6 || caseOnly );
	}
	f.bindVertexArray( 0 );
	objBlTexelForget( lightTex );  // GL reuses the names: a stale twin under one refused the next gate's buffer texture
	objBlTexelForget( restirTex );
	objBlTexelForget( atlasTex );
	glDeleteTextures( 1, &lightTex );
	glDeleteTextures( 1, &cubeTex );
	glDeleteTextures( 1, &envCube );
	sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &depthKeyAt ) );
	if ( rt ) {
		glDeleteTextures( 1, &restirTex );
		glDeleteTextures( 1, &atlasTex );
		objBlTexelStoreDrop( atlasBuf );
		sharedTargetDrop( giHalfKey );
		sharedTargetDrop( giKeyKey );
	}
	objBlLocsLoad( spirvDir + "/no_locs.txt", why );
	std::printf( "BLVIEW %s %s\n", rt ? "pfx rt" : "pfx", ok ? "OK" : "FAILED" );
	return ok;
}
