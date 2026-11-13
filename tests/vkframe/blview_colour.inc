/* lane VKOBJ4 step 7: the colour arm. GL's own fo4_cellbl (fo4_defaultbl.vert + fo4_cellbl.frag linked from the HLSL SPIR-V
 * set as the app's wwSpvAttach links it: the binaries unchanged, no uniform GL can mirror by name) draws the shade fixture's
 * class (blview_shade.inc: its pool, commands and wwDrawBase 5) with glMultiDrawElementsIndirect into the shared targets;
 * then objBlShade (the app's crossing: GL's framebuffer and fixed state, the mirror through the locs.txt registry, the
 * texture units, the pool, objBlRun) draws the same class through the Vulkan twin into the same targets.
 * State: the rows hold real resident bindless handles (the harness's 12 textures, every slot; texture anisoAt as its
 * test-local anisotropy-1 twin, below), globalUniforms /
 * skinningUniforms on buffers at bindings 0 / 1, a lit LooseUniforms state at the registry's locations (cellOn, the sun
 * cellHasDir, the DALC ambient, a specular map, glow), every active sampler on its own empty unit.
 * BAR (stated before the first run, never widened): depth as the shade bar (DepthCmp EXACT or BOUND). Colour, both
 * RGBA16F targets, over the pixels both sides cover: EXACT (0 values differ in bits), or BOUND (every channel
 * |Vulkan - GL| <= 4e-3 * max(1, |GL|), four half steps at 1; a NaN on one side only is over). The run is lit: >= 2000
 * pixels both cover, GL's colour rgb non-zero on >= 90 % of them. The crossing: one run, 0 failed, objBlShade not refused.
 * Correction (written before the anisotropy-1 run, manager 11-13): the first statement also asked GL's glow target
 * non-zero on >= 50 % of those pixels. fo4_cellbl writes SV_Target1 (cellEmitOut) as the constant 0 (fo4_cellbl_ps.hlsl;
 * only the debug probe cellProbe 60 writes it, and this state never sets it), so no mirror could meet that clause: it was
 * a wrong statement of the subject and is removed, not a wider bound. The 4e-3 bound is unchanged; the glow count stays
 * printed; the glow red still moves colour target 0 through glowMult.
 * Anisotropy (manager 11-13): texture 5 is the fixture's one def at anisotropy 4, and NV GL's anisotropic filter and
 * Vulkan's differ on it (the per-texture reading of the first runs). The bar runs at anisotropy 1 on both sides through a
 * test-local twin of it (blview.cpp: the same def and levels, 0x84FE 1, registered as its arm registers; the Vulkan
 * sampler mirrors GL's own 0x84FE), so it tests the mirroring itself; no shipped code path changes. Texture 5 at the
 * fixture's anisotropy is still read and printed as the known driver difference (a reading, not the bar).
 * Reds (each must FAIL against GL): glow (glowMult 0 after GL drew: the mirror carries GL's state), slot (each row's maps
 * one slot on in the rows given to objBlShade). Each red must also move the Vulkan colour off the Vulkan base (a red
 * that changes nothing tests nothing). No registry red here: NV reflects OpName, so this program's names resolve and
 * the registry is not on its path; the locs gate holds that red on a nameless program.
 * 1-buffer arm (manager 11-13; stated before its first run, never widened): the app's cell frame has one draw buffer while
 * fo4_cellbl writes two (SV_Target1 the constant 0). GL's frame with draw buffer 0 only, both targets cleared to 0.25: GL
 * draws, then objBlShade takes the frame as 1 colour target and objBlPipe builds the twin with SV_Target1 dropped (Vulkan
 * discards an output with no attachment). BAR: not refused, 1 run 0 failed, 1 output dropped; depth vs GL as the shade
 * bar; target 0 vs GL over the pixels both cover as the colour bar above (EXACT, or BOUND 4e-3), >= 2000 of them; target
 * 0 vs the 2-buffer Vulkan run EXACT over those pixels (dropping the output moves nothing); target 1 still 0.25 in every
 * value on both sides. Red strict (ObjBlShade.red 1: objBlPipe's old strict count) must refuse with "the pixel stage writes
 * 2 targets, the frame has 1". Check under: a 3-colour target still refuses ("writes 2 targets, the frame has 3"). */
bool colourGate( const std::string & spirvDir, const std::vector<GLuint64> & fixture, int anisoAt, GLuint64 aniso1 )
{
	using namespace WwVkPresent;
	std::vector<GLuint64> handles = fixture;  // the fixture's handles, texture anisoAt as its anisotropy-1 twin
	handles[std::size_t( anisoAt )] = aniso1;
	struct ColGl
	{
		void( WINAPI * shaderBinary )( GLsizei, const GLuint *, GLenum, const void *, GLsizei ) = nullptr;
		void( WINAPI * specialize )( GLuint, const char *, GLuint, const GLuint *, const GLuint * ) = nullptr;
		void( WINAPI * ifaceIv )( GLuint, GLenum, GLenum, GLint * ) = nullptr;
		void( WINAPI * resIv )( GLuint, GLenum, GLuint, GLsizei, const GLenum *, GLsizei, GLsizei *, GLint * ) = nullptr;
		void( WINAPI * uf[4] )( GLint, GLsizei, const GLfloat * ) = {};
		void( WINAPI * ui1 )( GLint, GLint ) = nullptr;
		void( WINAPI * uu1 )( GLint, GLuint ) = nullptr;
		void( WINAPI * bindTextures )( GLuint, GLsizei, const GLuint * ) = nullptr;
		void( WINAPI * bindSamplers )( GLuint, GLsizei, const GLuint * ) = nullptr;
		void( WINAPI * deleteProgram )( GLuint ) = nullptr;
	} u;
	const char * n4 = "1234";
	bool okLoad = load( u.shaderBinary, "glShaderBinary" ) && load( u.specialize, "glSpecializeShader" ) && load( u.ifaceIv, "glGetProgramInterfaceiv" )
		&& load( u.resIv, "glGetProgramResourceiv" ) && load( u.ui1, "glUniform1i" ) && load( u.uu1, "glUniform1ui" )
		&& load( u.bindTextures, "glBindTextures" ) && load( u.bindSamplers, "glBindSamplers" ) && load( u.deleteProgram, "glDeleteProgram" );
	for ( int i = 0; i < 4; i++ )
		okLoad = okLoad && load( u.uf[i], ( std::string( "glUniform" ) + n4[i] + "fv" ).c_str() );
	if ( !okLoad || !sx.prog ) {
		std::printf( "BLVIEW colour FAIL (GL entry points missing, or no shade fixture)\n" );
		return false;
	}
	std::string why;
	if ( !objBlLocsLoad( spirvDir + "/locs.txt", why ) ) {
		std::printf( "BLVIEW colour FAIL (no registry: %s)\n", why.c_str() );
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
	// GL's program from the set, the binaries as they are
	const GLuint prog = f.createProgram();
	const char * stage[2] = { "fo4_defaultbl.vert", "fo4_cellbl.frag" };
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
			std::printf( "BLVIEW colour FAIL (%s did not specialize)\n", stage[s] );
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
		std::printf( "BLVIEW colour FAIL (fo4_cellbl from SPIR-V did not link: %s)\n", log );
		return false;
	}
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW colour FAIL (GL error 0x%x building the program)\n", unsigned( e ) );
		return false;
	}
	// GL's active default-block locations and their types
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
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW colour FAIL (GL error 0x%x reading the program's %d uniforms)\n", unsigned( e ), int( nRes ) );
		return false;
	}
	{  // the registry against the names NV reflects: a stale or shifted locs.txt shows here
		GLint( WINAPI * loc )( GLuint, const char * ) = nullptr;
		int named = 0, agree = 0;
		std::string firstOff;
		if ( load( loc, "glGetUniformLocation" ) )
			for ( const auto & e : reg ) {
				const GLint l = loc( prog, e.first.c_str() );
				if ( l < 0 )
					continue;
				named++;
				if ( l == e.second )
					agree++;
				else if ( firstOff.empty() )
					firstOff = e.first + " registry " + std::to_string( e.second ) + " GL " + std::to_string( l );
			}
		std::printf( "BLVIEW colour registry vs GL's names: %d named, %d agree%s%s\n", named, agree, firstOff.empty() ? "" : "; first off: ",
			firstOff.c_str() );
	}
	auto isSampler = []( GLint t ) {
		return ( t >= 0x8B5D && t <= 0x8B64 ) || ( t >= 0x8DC0 && t <= 0x8DD8 ) || ( t >= 0x900C && t <= 0x900F ) || ( t >= 0x9108 && t <= 0x910D );
	};
	f.useProgram( prog );
	int units = 0, inactive = 0;
	std::string putBad;
	for ( const auto & e : glType )
		if ( isSampler( e.second ) )
			u.ui1( e.first, 1 + units++ );  // each on its own empty unit
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW colour FAIL (GL error 0x%x placing %d samplers on units)\n", unsigned( e ), units );
		return false;
	}
	auto put = [&]( const char * name, std::initializer_list<float> vals, int elem = 0 ) {
		const auto it = reg.find( name );
		const int at = it == reg.end() ? -1 : it->second + elem;
		if ( at < 0 || !glType.count( at ) ) {
			inactive++;
			return;
		}
		const GLint t = glType[at];
		const float * v = vals.begin();
		f.useProgram( prog );  // glUniform writes the current program: a red set after frameOff must still reach it
		if ( t == 0x1406 || ( t >= 0x8B50 && t <= 0x8B52 ) )  // float, vec2..4
			u.uf[t == 0x1406 ? 0 : t - 0x8B50 + 1]( at, 1, v );
		else if ( t == 0x1405 )  // uint
			u.uu1( at, GLuint( v[0] ) );
		else  // int, bool
			u.ui1( at, GLint( v[0] ) );
		if ( glGetError() != GL_NO_ERROR && putBad.empty() )
			putBad = std::string( name ) + " (GL type 0x" + std::to_string( t ) + ")";
	};
	put( "cellOn", { 1 } );
	put( "cellHasDir", { 1 } );
	put( "cellDirTo", { 0.32f, 0.45f, 0.834f } );
	put( "cellDirColor", { 1.6f, 1.4f, 1.1f } );
	put( "cellSunScale", { 1 } );
	put( "cellExtDalc", { 1 } );
	const float dalc[3][4] = { { 0.10f, 0.05f, 0.02f, 0.12f }, { 0.09f, 0.06f, 0.03f, 0.11f }, { 0.12f, 0.04f, 0.02f, 0.10f } };
	for ( int c = 0; c < 3; c++ ) {
		put( "cellDalc", { dalc[c][0], dalc[c][1], dalc[c][2], dalc[c][3] }, c );
		put( "cellRow", { c == 0 ? 1.0f : 0.0f, c == 1 ? 1.0f : 0.0f, c == 2 ? 1.0f : 0.0f, 0.0f }, c );
	}
	put( "specStrength", { 0.7f } );
	put( "specGlossiness", { 0.6f } );
	put( "hasSpecularMap", { 1 } );
	put( "specColor", { 1.0f, 0.9f, 0.8f } );
	put( "hasEmit", { 1 } );
	put( "glowColor", { 0.3f, 0.2f, 0.1f } );
	put( "glowMult", { 1.5f } );
	put( "hasGlowMap", { 1 } );
	put( "alpha", { 1 } );
	put( "fresnelPower", { 5 } );
	put( "uvScale", { 1, 1 } );
	put( "paletteScale", { 1 } );
	put( "cellIsLinear", { 1 } );
	put( "wwDrawBase", { float( sx.base ) } );
	// globalUniforms (binding 0) at the twin's layout, skinningUniforms (binding 1) zero
	SpvModule psM;
	const SpvBind * gb = nullptr;
	if ( spvReflect( vkframe_objbl_cell_ps_spv, sizeof( vkframe_objbl_cell_ps_spv ) / 4, psM, why ) )
		gb = uniBlock( psM, "globalUniforms" );
	if ( !gb ) {
		std::printf( "BLVIEW colour FAIL (the twin's globalUniforms: %s)\n", why.c_str() );
		return false;
	}
	std::vector<std::uint8_t> gBytes( std::max<std::size_t>( gb->bytes, 16 ), 0 ), sBytes( 256 * 48, 0 );
	const float ident[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	for ( const SpvMember & m : gb->members ) {
		auto vec = [&]( std::initializer_list<float> v, std::uint32_t e = 0 ) {
			std::memcpy( gBytes.data() + m.offset + e * m.arrayStride, v.begin(), std::min<std::size_t>( v.size(), m.comps ) * 4 );
		};
		if ( m.name == "projectionMatrix" )
			shadePutMat( gBytes, m, sx.proj );
		else if ( m.name == "viewMatrix" || m.name == "envMapRotation" )
			shadePutMat( gBytes, m, ident );
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
			std::memcpy( gBytes.data() + m.offset, vp, 16 );
		}
	}
	GLuint ubo[2] = {}, rowsBuf = 0, vao = 0;
	f.genBuffers( 2, ubo );
	f.bindBuffer( 0x8A11, ubo[0] );
	f.bufferData( 0x8A11, std::ptrdiff_t( gBytes.size() ), gBytes.data(), 0x88E4 );
	f.bindBuffer( 0x8A11, ubo[1] );
	f.bufferData( 0x8A11, std::ptrdiff_t( sBytes.size() ), sBytes.data(), 0x88E4 );
	f.bindBuffer( 0x8A11, 0 );
	// the rows: the fixture's matrices, 8 real resident handles each (every texture in every slot somewhere)
	std::vector<unsigned char> rows = sx.rows, rowsSlot = sx.rows, rowsT = sx.rows;
	const std::size_t nRows = rows.size() / kBlRowBytes;
	for ( std::size_t r = 0; r < nRows; r++ )
		for ( std::size_t i = 0; i < 8; i++ ) {
			const GLuint64 h = handles[( r * 5 + i * 7 ) % handles.size()], hn = handles[( r * 5 + ( ( i + 1 ) % 8 ) * 7 ) % handles.size()];
			std::memcpy( &rows[r * kBlRowBytes + kBlTexAt + 8 * i], &h, 8 );
			std::memcpy( &rowsSlot[r * kBlRowBytes + kBlTexAt + 8 * i], &hn, 8 );
		}
	f.genBuffers( 1, &rowsBuf );
	f.bindBuffer( kSsbo, rowsBuf );
	f.bufferData( kSsbo, std::ptrdiff_t( rows.size() ), rows.data(), 0x88E4 );
	f.bindBuffer( kSsbo, 0 );
	// the vertex attributes at their TEXCOORD locations, the class's index buffer
	f.genVertexArrays( 1, &vao );
	f.bindVertexArray( vao );
	for ( int i = 0; i < 16; i++ ) {
		const int n = int( ( sx.pool.attrMask >> ( 4 * i ) ) & 15 );
		if ( !n )
			continue;
		f.bindBuffer( 0x8892, sx.pool.vbo[i] );
		sf.vertexAttribPointer( GLuint( i ), n, GL_FLOAT, GL_FALSE, 0, nullptr );
		sf.enableVertexAttribArray( GLuint( i ) );
	}
	f.bindBuffer( 0x8893, sx.pool.ebo );
	f.bindVertexArray( 0 );
	f.bindBuffer( 0x8892, 0 );
	// GL's frame and fixed state, as the app's opaque pass (nBufs: the 1-buffer arm's draw buffer 0 only)
	int nBufs = 2;
	auto frameState = [&]() {
		f.bindFramebuffer( kFb, sx.fbo );
		const GLenum dbs[2] = { kColorAtt0, kColorAtt0 + 1 };
		sf.drawBuffers( nBufs, dbs );
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
		f.bindBufferBase( 0x8A11, 0, ubo[0] );
		f.bindBufferBase( 0x8A11, 1, ubo[1] );
		f.bindBufferBase( kSsbo, 12, rowsBuf );
	};
	auto frameOff = [&]() {
		glDisable( GL_CULL_FACE );
		glDisable( GL_DEPTH_TEST );
		f.useProgram( 0 );
		f.bindFramebuffer( kFb, 0 );
	};
	struct ColOut
	{
		std::vector<float> depth;
		std::vector<std::uint16_t> c0, c1;
	};
	ColOut gl;
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR || !putBad.empty() ) {
		std::printf( "BLVIEW colour FAIL (GL error 0x%x setting the state; first bad value %s)\n", unsigned( e ), putBad.c_str() );
		return false;
	}
	shadeClear();
	frameState();
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW colour FAIL (GL error 0x%x binding the frame)\n", unsigned( e ) );
		return false;
	}
	GLuint( WINAPI * debugLog )( GLuint, GLsizei, GLenum *, GLenum *, GLuint *, GLenum *, GLsizei *, char * ) = nullptr;
	if ( load( debugLog, "glGetDebugMessageLog" ) ) {
		glEnable( 0x92E0 );  // GL_DEBUG_OUTPUT: the driver's reason for an error lands in the log
		glEnable( 0x8242 );  // _SYNCHRONOUS
	}
	f.bindVertexArray( vao );
	f.bindBuffer( 0x8F3F, sx.ind );
	sf.multiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT, nullptr, GLsizei( sx.cmds.size() / 5 ), 20 );
	const GLenum drawErr = glGetError();
	f.bindBuffer( 0x8F3F, 0 );
	f.bindVertexArray( 0 );
	frameOff();
	if ( drawErr != GL_NO_ERROR ) {
		std::string msg;
		char buf[4096] = {};
		GLenum ty = 0;
		for ( int i = 0; debugLog && i < 8 && debugLog( 1, sizeof( buf ), nullptr, &ty, nullptr, nullptr, nullptr, buf ); i++ )
			if ( ty == 0x824C )  // GL_DEBUG_TYPE_ERROR
				msg = buf;
		std::printf( "BLVIEW colour FAIL (GL error 0x%x in GL's fo4_cellbl draw: %s)\n", unsigned( drawErr ), msg.c_str() );
		return false;
	}
	shadeRead( gl.depth, &gl.c0, &gl.c1 );
	glFinish();
	{  // does the sun reach this fixture's colour at all (GL against GL)
		ColOut g2;
		put( "glowMult", { 0 } );
		shadeClear();
		frameState();
		f.bindVertexArray( vao );
		f.bindBuffer( 0x8F3F, sx.ind );
		sf.multiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT, nullptr, GLsizei( sx.cmds.size() / 5 ), 20 );
		f.bindBuffer( 0x8F3F, 0 );
		f.bindVertexArray( 0 );
		frameOff();
		shadeRead( g2.depth, &g2.c0, &g2.c1 );
		put( "glowMult", { 1.5f } );
		std::size_t ch = 0;
		for ( std::size_t i = 0; i < gl.c0.size(); i++ )
			ch += gl.c0[i] != g2.c0[i];
		std::printf( "BLVIEW colour GL glow probe: glowMult 0 changes %zu GL colour values\n", ch );
	}
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW colour FAIL (GL error 0x%x reading GL's fo4_cellbl draw)\n", unsigned( e ) );
		return false;
	}
	// the app's crossing, the same state; red: 1 glow, 2 slot, 10 strict (objBlPipe's old target count); clearK: both targets
	auto vkRun = [&]( int red, ColOut & out, std::string & w, float clearK = 0.0f ) {
		if ( red == 1 )
			put( "glowMult", { 0 } );
		shadeClear( clearK );
		frameState();
		ObjBlShade s;
		s.twin = 0;
		s.prog = prog;
		s.poolKey = sx.key;
		s.pool = sx.pool;
		s.rows = red == 9 ? rowsT.data() : red == 2 ? rowsSlot.data() : rows.data();
		s.nRows = nRows;
		s.cmds = sx.cmds.data();
		s.nCmds = sx.cmds.size() / 5;
		s.red = red == 10 ? 1 : 0;
		const bool ran = objBlShade( glProc, s, w );
		frameOff();
		if ( red == 1 )
			put( "glowMult", { 1.5f } );
		if ( ran )
			shadeRead( out.depth, &out.c0, &out.c1 );
		glFinish();
		const GLenum e = glGetError();
		if ( ran && e != GL_NO_ERROR )
			w = "GL error " + std::to_string( e ) + " at the crossing";
		return ran && e == GL_NO_ERROR;
	};
	struct ColCmp
	{
		std::size_t both = 0, differ = 0, over = 0, lit = 0, glow = 0, first = 0, over1 = 0, over10 = 0;
		float maxRel = 0.0f;
		bool exact() const { return differ == 0; }
		bool bound() const { return over == 0; }
	};
	auto colCmp = [&]( const ColOut & v ) {
		ColCmp c;
		const std::size_t px = gl.depth.size();
		for ( std::size_t i = 0; i < px; i++ ) {
			if ( !( gl.depth[i] < 1.0f ) || !( v.depth[i] < 1.0f ) )
				continue;
			c.both++;
			bool lit = false, glow = false;
			for ( int t = 0; t < 2; t++ )
				for ( int ch = 0; ch < 4; ch++ ) {
					const std::uint16_t hg = ( t ? gl.c1 : gl.c0 )[i * 4 + ch], hv = ( t ? v.c1 : v.c0 )[i * 4 + ch];
					const float g = fromHalf( hg ), a = fromHalf( hv );
					if ( t == 0 && ch < 3 && g != 0.0f )
						lit = true;
					if ( t == 1 && g != 0.0f )
						glow = true;
					if ( hg == hv )
						continue;
					c.differ++;
					const float rel = std::fabs( a - g ) / std::max( 1.0f, std::fabs( g ) );
					if ( !( rel <= 4e-3f ) && c.over == 0 )
							c.first = i * 8 + std::size_t( t * 4 + ch );
						c.over1 += !( rel <= 1e-2f );
						c.over10 += !( rel <= 0.1f );
						if ( !( rel <= 4e-3f ) )
						c.over++;
					if ( rel > c.maxRel || rel != rel )
						c.maxRel = rel != rel ? 1e30f : rel;
				}
			c.lit += lit;
			c.glow += glow;
		}
		return c;
	};
	bool ok = true;
	takeObjBlRunStat();
	ColOut vk0;
	std::string w;
	const bool ran = vkRun( 0, vk0, w );
	const ObjBlRunStat rs = takeObjBlRunStat();
	if ( !ran ) {
		std::printf( "BLVIEW colour FAIL (objBlShade refused: %s)\n", w.c_str() );
		ok = false;
	} else {
		const DepthCmp dc = depthCmp( vk0.depth, gl.depth );
		const ColCmp cc = colCmp( vk0 );
		const bool litOk = cc.both >= 2000 && double( cc.lit ) >= 0.9 * double( cc.both );
		const bool crossOk = rs.runs == 1 && rs.fails == 0;
		std::printf( "BLVIEW colour state: %d samplers on their own units, %d values with no active location; set 0 units: %d shared, %d GL-only, "
			"%d GL texel buffers; crossing %s (%d runs, %d failed)\n", units, inactive, rs.shared, rs.glOwn, rs.texelGl, crossOk ? "OK" : "FAIL",
			rs.runs, rs.fails );
		std::printf( "BLVIEW colour depth vs GL %s (%zu covered, %zu on one side, %zu differ, max %.3g)\n", dc.word(), dc.covered, dc.oneSide,
			dc.differ, double( dc.maxAbs ) );
		std::printf( "BLVIEW colour vs GL %s (%zu pixels both cover, %zu lit, %zu with glow: %s; %zu values differ, %zu over the bound, max %.3g of max(1, |GL|))\n",
			cc.exact() ? "EXACT" : cc.bound() ? "BOUND" : "FAIL", cc.both, cc.lit, cc.glow, litOk ? "lit" : "NOT LIT", cc.differ, cc.over,
			double( cc.maxRel ) );
		if ( cc.over ) {
			const std::size_t p = cc.first / 8, t = ( cc.first % 8 ) / 4;
			const auto & g = t ? gl.c1 : gl.c0;
			const auto & a = t ? vk0.c1 : vk0.c0;
			std::printf( "BLVIEW colour spread: %zu values over 1e-2, %zu over 0.1 (of max(1, |GL|))\n", cc.over1, cc.over10 );
			std::printf( "BLVIEW colour first over: pixel (%zu, %zu) target %zu channel %zu; GL %g %g %g %g, Vulkan %g %g %g %g\n", p % kSW, p / kSW, t,
				cc.first % 4, double( fromHalf( g[p * 4] ) ), double( fromHalf( g[p * 4 + 1] ) ), double( fromHalf( g[p * 4 + 2] ) ),
				double( fromHalf( g[p * 4 + 3] ) ), double( fromHalf( a[p * 4] ) ), double( fromHalf( a[p * 4 + 1] ) ), double( fromHalf( a[p * 4 + 2] ) ),
				double( fromHalf( a[p * 4 + 3] ) ) );
		}
		ok = ok && crossOk &&( dc.exact() || dc.bound() ) && litOk && ( cc.exact() || cc.bound() );
	}
	const char * redName[2] = { "glow (glowMult 0 after GL drew)", "slot (each row's maps one slot on)" };
	for ( int r = 1; r <= 2; r++ ) {
		ColOut o;
		std::string rw;
		const bool rr = vkRun( r, o, rw );
		bool passes = false;
		std::size_t over = 0, vch = 0;
		if ( rr ) {
			const DepthCmp dc = depthCmp( o.depth, gl.depth );
			const ColCmp cc = colCmp( o );
			over = cc.over;
			for ( std::size_t i = 0; ran && i < o.c0.size() && i < vk0.c0.size(); i++ )
				vch += o.c0[i] != vk0.c0[i];
			passes = ( dc.exact() || dc.bound() ) && ( cc.exact() || cc.bound() );
		}
		std::printf( "BLVIEW RED colour%s %s (%zu values over the bound, %zu Vulkan colour values moved off the Vulkan base%s%s)\n", redName[r - 1],
			passes ? "PASSED" : vch ? "FAIL" : "TESTS NOTHING", over, vch, rr ? "" : "; refused: ", rr ? "" : rw.c_str() );
		ok = ok && !passes && vch > 0;
	}
	{  // the 1-buffer arm: GL's frame with draw buffer 0 only, both targets cleared to 0.25
		const float k = 0.25f;
		nBufs = 1;
		ColOut g1;
		shadeClear( k );
		frameState();
		f.bindVertexArray( vao );
		f.bindBuffer( 0x8F3F, sx.ind );
		sf.multiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT, nullptr, GLsizei( sx.cmds.size() / 5 ), 20 );
		f.bindBuffer( 0x8F3F, 0 );
		f.bindVertexArray( 0 );
		frameOff();
		shadeRead( g1.depth, &g1.c0, &g1.c1 );
		takeObjBlRunStat();
		ColOut v1;
		std::string w1;
		const bool r1 = vkRun( 0, v1, w1, k );
		const ObjBlRunStat rs1 = takeObjBlRunStat();
		std::size_t both = 0, differ = 0, over = 0, vsTwo = 0, t1Gl = 0, t1Vk = 0;
		float maxRel = 0.0f;
		bool armOk = r1 && rs1.runs == 1 && rs1.fails == 0 && rs1.dropped == 1;
		if ( r1 ) {
			const DepthCmp dc = depthCmp( v1.depth, g1.depth );
			const std::uint16_t hk = 0x3400;  // 0.25 as a half
			for ( std::size_t i = 0; i < g1.c1.size(); i++ ) {
				t1Gl += g1.c1[i] != hk;
				t1Vk += v1.c1[i] != hk;
			}
			for ( std::size_t i = 0; i < g1.depth.size(); i++ ) {
				if ( !( g1.depth[i] < 1.0f ) || !( v1.depth[i] < 1.0f ) )
					continue;
				both++;
				for ( int ch = 0; ch < 4; ch++ ) {
					const std::uint16_t hg = g1.c0[i * 4 + ch], hv = v1.c0[i * 4 + ch];
					vsTwo += ran && hv != vk0.c0[i * 4 + ch];
					if ( hg == hv )
						continue;
					differ++;
					const float g = fromHalf( hg ), rel = std::fabs( fromHalf( hv ) - g ) / std::max( 1.0f, std::fabs( g ) );
					over += !( rel <= 4e-3f );
					if ( rel > maxRel || rel != rel )
						maxRel = rel != rel ? 1e30f : rel;
				}
			}
			armOk = armOk && ran && ( dc.exact() || dc.bound() ) && both >= 2000 && over == 0 && vsTwo == 0 && t1Gl == 0 && t1Vk == 0;
			std::printf( "BLVIEW colour 1buf depth vs GL %s (%zu covered, %zu on one side, %zu differ)\n", dc.word(), dc.covered, dc.oneSide,
				dc.differ );
		}
		std::printf( "BLVIEW colour 1buf %s (refused %d%s%s; %d runs, %d failed, %d outputs dropped; target 0 vs GL %s: %zu pixels both cover, "
			"%zu values differ, %zu over the bound, max %.3g; vs the 2-buffer Vulkan run %zu values differ; target 1 off 0.25: GL %zu, Vulkan %zu)\n",
			armOk ? "OK" : "FAIL", int( !r1 ), r1 ? "" : ": ", r1 ? "" : w1.c_str(), rs1.runs, rs1.fails, rs1.dropped,
			differ == 0 ? "EXACT" : over == 0 ? "BOUND" : "FAIL", both, differ, over, double( maxRel ), vsTwo, t1Gl, t1Vk );
		ok = ok && armOk;
		// red strict: the old count must refuse this frame
		ColOut vr;
		std::string wr;
		const bool rr = vkRun( 10, vr, wr, k );
		takeObjBlRunStat();
		const bool said = wr.find( "the pixel stage writes 2 targets, the frame has 1" ) != std::string::npos;
		std::printf( "BLVIEW RED colour1bufstrict %s (the strict target count; %s%s)\n", !rr && said ? "FAIL" : "PASSED",
			rr ? "drew" : "refused: ", rr ? "" : wr.c_str() );
		ok = ok && !rr && said;
		nBufs = 2;
		// under: a frame with more targets than the stage writes still refuses
		WwVkPresent::ObjBlTarget t3;
		t3.colors[0] = t3.colors[1] = t3.colors[2] = 97;  // VK_FORMAT_R16G16B16A16_SFLOAT
		t3.colorCount = 3;
		t3.depth = 129;  // VK_FORMAT_D24_UNORM_S8_UINT
		WwVkPresent::ObjBlPipeInfo i3;
		std::string w3;
		const bool b3 = objBlPipe( 0, vkframe_objbl_vs_spv, sizeof( vkframe_objbl_vs_spv ) / 4, vkframe_objbl_cell_ps_spv,
			sizeof( vkframe_objbl_cell_ps_spv ) / 4, t3, {}, {}, i3, w3 );
		const bool under = !b3 && w3.find( "the pixel stage writes 2 targets, the frame has 3" ) != std::string::npos;
		std::printf( "BLVIEW colour 1buf under %s (a 3-colour frame: %s%s)\n", under ? "OK" : "FAIL", b3 ? "built" : "refused: ", w3.c_str() );
		ok = ok && under;
	}
	// where the gap lives: every slot of every row on one texture, GL against Vulkan (a reading, not the bar); the last
	// reading is the fixture's own texture anisoAt at its anisotropy (the known driver difference)
	std::string perTex, fixtureTex;
	for ( std::size_t t = 0; t <= handles.size(); t++ ) {
		const GLuint64 ht = t < handles.size() ? handles[t] : fixture[std::size_t( anisoAt )];
		for ( std::size_t r = 0; r < nRows; r++ )
			for ( std::size_t i = 0; i < 8; i++ )
				std::memcpy( &rowsT[r * kBlRowBytes + kBlTexAt + 8 * i], &ht, 8 );
		f.bindBuffer( kSsbo, rowsBuf );
		f.bufferData( kSsbo, std::ptrdiff_t( rowsT.size() ), rowsT.data(), 0x88E4 );
		f.bindBuffer( kSsbo, 0 );
		const ColOut glKeep = gl;
		shadeClear();
		frameState();
		f.bindVertexArray( vao );
		f.bindBuffer( 0x8F3F, sx.ind );
		sf.multiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT, nullptr, GLsizei( sx.cmds.size() / 5 ), 20 );
		f.bindBuffer( 0x8F3F, 0 );
		f.bindVertexArray( 0 );
		frameOff();
		shadeRead( gl.depth, &gl.c0, &gl.c1 );
		ColOut o;
		std::string tw;
		const bool tr = vkRun( 9, o, tw );
		const ColCmp cc = tr ? colCmp( o ) : ColCmp();
		const std::string reading = tr ? std::to_string( cc.over ) + "/" + std::to_string( cc.over10 ) : std::string( "refused" );
		if ( t < handles.size() )
			perTex += " " + std::to_string( t ) + ":" + reading;
		else
			fixtureTex = reading;
		gl = glKeep;
	}
	std::printf( "BLVIEW colour per texture, %d at anisotropy 1 (over the bound / over 0.1):%s\n", anisoAt, perTex.c_str() );
	std::printf( "BLVIEW colour known driver difference: texture %d at the fixture's anisotropy, GL's anisotropic filter against Vulkan's "
		"(a reading, not the bar): %s over the bound / over 0.1\n", anisoAt, fixtureTex.c_str() );
	objBlLocsLoad( spirvDir + "/no_locs.txt", why );  // the GLSL gates never see it
	f.useProgram( 0 );
	u.deleteProgram( prog );
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		std::printf( "BLVIEW colour FAIL (GL error 0x%x left)\n", unsigned( e ) );
		ok = false;
	}
	return ok;
}
