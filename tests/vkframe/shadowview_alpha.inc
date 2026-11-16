	// Lane VKSHADOW7 (2026-11-16, item 1 bar, written and committed before the first run), lines `SHADOWVIEW alpha ...` after
	// the remap ones (those stay byte-equal). Included by shadowview.cpp's main after the remap case.
	// The alpha-tested per-draw casters (sunshadow.cpp drawJob pass 1: lookdev_csmdepth_at.vert + lookdev_csmdepth_at.frag,
	// `texture( BaseMap, texCoord * uvScale + uvOffset ).a * alpha < alphaThreshold` discards) as CsmMesh draws with the
	// alpha fields (CsmDraw atTex / uvScale / uvOffset / atAlpha / atRef, CsmMesh uv): one CsmDraw a caster, as the app emits
	// them. The textures are the texture cache's BC twins (objTexMake: the Vulkan image GL uploads into), so both arms sample
	// the same bytes with the GL texture's own sampler state: T0 BC1 RGBA (punch-through: alpha 0 or 1) 64x64, 7 levels,
	// LINEAR_MIPMAP_LINEAR, REPEAT, anisotropy 4; T1 BC3 128x128, 8 levels, an 8-step alpha ramp per texel (a cut edge falls
	// between texels: the filter weights decide it), LINEAR_MIPMAP_LINEAR, CLAMP_TO_EDGE; T2 BC3 like T1, another ramp (the
	// wrong-texture red only). Wires each get a value that would show: uvScale != 1, uvOffset != 0, UVs outside [0, 1] (plate
	// UVs from -0.7 to 2.1), alpha 0.7 / 0.8 / 0.9 / 0.95, thresholds 0.2 / 0.31 / 0.45 / 0.5 / 0.62, a scissor strip, two
	// two-sided casters, a caster drawn as cullIdx ranges, the static target and its live duplicate. Both arms first draw the
	// base content (ground + the queued casters, GL on the GL arm, the crossing's queue draws on the Vulkan arm), then the
	// alpha casters: GL with its own program, Vulkan in the same csmRun as the queue draws. Two frames (moved matrices).
	// BOUND (stated before the first run, never widened): EXACT expected (same texels, the same sampler state on the same
	// fixed-function sampler, the same products as csm_view_vs). Else a layer is BOUND when <= 0.05 % of its covered texels
	// differ AND <= 0.01 % differ past 1 unit (an alpha flip at a cut edge moves a texel's depth by more than a unit); past
	// either: FAIL. A layer with 0 covered texels FAILs.
	// Must hold: every layer EXACT or BOUND in both frames; alpha-cut texels (GL with the test against GL with the same
	// casters and no test, alphaThreshold -1) > 0 on every layer that has alpha casters; the alpha casters win > 0 texels
	// against GL without them; CsmStat alphaDraws >= the frame's alpha draws (8) and alphaTex >= 2 each frame (the draws
	// ran on the alpha pipeline with their own textures).
	// Reds on frame 1 (each must FAIL: past 1 unit on > 0.01 % of all covered texels): test off (the alpha casters as plain
	// mesh draws, atTex 0), wrong (every alpha draw samples T2), uv (uvScale 1, uvOffset 0), alpha (atAlpha 1).
	bool atOk = rmOk;
	{
		void( WINAPI * cTexSub )( GLuint, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void * ) = nullptr;
		void( WINAPI * uni2f )( GLint, GLfloat, GLfloat ) = nullptr;
		void( WINAPI * activeTex )( GLenum ) = nullptr;
		std::string vsA = readFile( root + "/res/shaders/features/shadows/lookdev_csmdepth_at.vert" );
		const std::string inc = "#include \"uniforms.glsl\"";
		const std::size_t incAt = vsA.find( inc );
		if ( incAt != std::string::npos )
			vsA.replace( incAt, inc.size(), readFile( root + "/res/shaders/uniforms.glsl" ) );
		const GLuint progA = incAt == std::string::npos ? 0
			: program( vsA, readFile( root + "/res/shaders/features/shadows/lookdev_csmdepth_at.frag" ) );
		if ( !progA || !load( cTexSub, "glCompressedTextureSubImage2D" ) || !load( uni2f, "glUniform2f" )
			|| !load( activeTex, "glActiveTexture" ) ) {
			std::printf( "SHADOWVIEW alpha FAIL (the alpha-tested program or its entry points)\n" );
			atOk = false;
		}
		constexpr GLenum kUboA = 0x8A11, kTex2D = 0x0DE1, kClampEdge = 0x812F, kAniso = 0x84FE, kTex0 = 0x84C0;
		GLint aClip = -1, aMv = -1, aUvS = -1, aUvO = -1, aAlpha = -1, aRef = -1;
		GLuint uboA = 0, vaoA = 0;
		if ( atOk ) {
			aClip = f.getUniformLocation( progA, "csmClipFromView" );
			aMv = f.getUniformLocation( progA, "modelViewMatrix" );
			aUvS = f.getUniformLocation( progA, "uvScale" );
			aUvO = f.getUniformLocation( progA, "uvOffset" );
			aAlpha = f.getUniformLocation( progA, "alpha" );
			aRef = f.getUniformLocation( progA, "alphaThreshold" );
			f.useProgram( progA );
			f.uniform1i( f.getUniformLocation( progA, "farInst" ), 0 );
			f.uniform1i( f.getUniformLocation( progA, "lodTreeAnim" ), 0 );
			f.uniform1f( f.getUniformLocation( progA, "farSwayAmp" ), 0.0f );
			f.uniform1i( f.getUniformLocation( progA, "BaseMap" ), 0 );
			const std::vector<char> zeros( 4096, 0 );
			f.genBuffers( 1, &uboA );
			f.bindBuffer( kUboA, uboA );
			f.bufferData( kUboA, 4096, zeros.data(), kDynamic );
			const GLuint blk = f.getUniformBlockIndex( progA, "globalUniforms" );
			if ( blk != 0xFFFFFFFFu )
				f.uniformBlockBinding( progA, blk, 0 );
			f.bindBufferBase( kUboA, 0, uboA );
			f.genVertexArrays( 1, &vaoA );
			atOk = aClip >= 0 && aMv >= 0 && aUvS >= 0 && aUvO >= 0 && aAlpha >= 0 && aRef >= 0;
			if ( !atOk )
				std::printf( "SHADOWVIEW alpha FAIL (a uniform of the alpha-tested program is missing)\n" );
		}

		// the textures: the cache's BC twins, blocks written here (no encoder), the cache's sampler state set on the GL name
		GLuint tw[3] = {};
		auto makeTex = [&]( int t ) {
			const bool bc1 = t == 0;
			const unsigned fmt = bc1 ? 0x83F1u : 0x83F3u;
			const int size = bc1 ? 64 : 128, levels = bc1 ? 7 : 8;
			unsigned tex = 0;
			std::string w;
			if ( !WwVkPresent::objTexMake( fmt, size, size, levels, tex, w ) ) {
				std::printf( "SHADOWVIEW alpha FAIL (objTexMake T%d: %s)\n", t, w.c_str() );
				return false;
			}
			tw[t] = GLuint( tex );
			for ( int l = 0; l < levels; l++ ) {
				const int lw = std::max( size >> l, 1 ), bw = ( lw + 3 ) / 4;
				std::vector<unsigned char> blocks;
				for ( int by = 0; by < bw; by++ )
					for ( int bx = 0; bx < bw; bx++ ) {
						if ( bc1 ) {
							// colour0 0x0000 <= colour1 0xFFFF: the 3-colour mode, index 3 = transparent black
							std::uint32_t bits = 0;
							for ( int p = 0; p < 16; p++ ) {
								const int x = bx * 4 + ( p & 3 ), y = by * 4 + ( p >> 2 );
								const bool cut = ( ( x / 3 + y / 2 + l ) % 3 ) == 0 || ( ( x * 5 + y * 3 ) % 11 ) == 0;
								bits |= std::uint32_t( cut ? 3 : 0 ) << ( 2 * p );
							}
							const unsigned char b[8] = { 0x00, 0x00, 0xFF, 0xFF, std::uint8_t( bits ), std::uint8_t( bits >> 8 ),
								std::uint8_t( bits >> 16 ), std::uint8_t( bits >> 24 ) };
							blocks.insert( blocks.end(), b, b + 8 );
						} else {
							// alpha0 255 > alpha1 0: 8 steps (index 0 255, 1 0, 2..7 the six between); 3-bit indices
							std::uint64_t bits = 0;
							for ( int p = 0; p < 16; p++ ) {
								const int x = bx * 4 + ( p & 3 ), y = by * 4 + ( p >> 2 );
								const int step = t == 1 ? ( x + 2 * y + l ) % 8 : ( 3 * x + y + 5 * l ) % 8;
								bits |= std::uint64_t( step ) << ( 3 * p );
							}
							unsigned char b[16] = { 0xFF, 0x00 };
							for ( int k = 0; k < 6; k++ )
								b[2 + k] = std::uint8_t( bits >> ( 8 * k ) );
							b[8] = 0xFF;  // colour0 0xFFFF > colour1 0x0000: the colours do not matter here
							b[9] = 0xFF;
							blocks.insert( blocks.end(), b, b + 16 );
						}
					}
				cTexSub( tex, l, 0, 0, lw, lw, fmt, GLsizei( blocks.size() ), blocks.data() );
			}
			glBindTexture( kTex2D, tex );
			glTexParameteri( kTex2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
			glTexParameteri( kTex2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			glTexParameteri( kTex2D, GL_TEXTURE_WRAP_S, bc1 ? GL_REPEAT : GLint( kClampEdge ) );
			glTexParameteri( kTex2D, GL_TEXTURE_WRAP_T, bc1 ? GL_REPEAT : GLint( kClampEdge ) );
			glTexParameterf( kTex2D, kAniso, bc1 ? 4.0f : 1.0f );
			glBindTexture( kTex2D, 0 );
			return glGetError() == GL_NO_ERROR;
		};
		for ( int t = 0; t < 3 && atOk; t++ )
			atOk = makeTex( t );

		// the meshes, each with a UV per vertex: a soup, a plate (UVs -0.7..2.1), a finer plate
		struct AMesh
		{
			std::vector<float> p, t;
			std::vector<std::uint16_t> i;
			GLuint vb = 0, tb = 0, ib = 0;
		};
		std::vector<AMesh> am( 3 );
		for ( int k = 0; k < 60; k++ ) {
			const float qx = u( rng ), qy = u( rng ), qz = u( rng );
			for ( int c = 0; c < 3; c++ ) {
				am[0].p.push_back( qx + 0.5f * u( rng ) );
				am[0].p.push_back( qy + 0.5f * u( rng ) );
				am[0].p.push_back( qz + 0.5f * u( rng ) );
				am[0].t.push_back( 0.5f + 0.8f * u( rng ) );
				am[0].t.push_back( 0.5f + 0.8f * u( rng ) );
				am[0].i.push_back( std::uint16_t( k * 3 + c ) );
			}
		}
		auto plateA = [&]( AMesh & m, int n, float amp ) {
			for ( int y = 0; y <= n; y++ )
				for ( int x = 0; x <= n; x++ ) {
					const float px = -1.0f + 2.0f * float( x ) / n, py = -1.0f + 2.0f * float( y ) / n;
					m.p.push_back( px );
					m.p.push_back( py );
					m.p.push_back( amp * std::sin( 2.5f * px ) * std::cos( 1.5f * py ) );
					m.t.push_back( 0.7f + 1.4f * px );
					m.t.push_back( 0.7f + 1.4f * py );
				}
			for ( int y = 0; y < n; y++ )
				for ( int x = 0; x < n; x++ ) {
					const std::uint16_t a = std::uint16_t( y * ( n + 1 ) + x ), b = std::uint16_t( a + 1 ), c = std::uint16_t( a + n + 1 ),
						d = std::uint16_t( c + 1 );
					const std::uint16_t q[6] = { a, b, d, a, d, c };
					m.i.insert( m.i.end(), q, q + 6 );
				}
		};
		plateA( am[1], 10, 0.2f );
		plateA( am[2], 24, 0.1f );
		if ( atOk ) {
			f.bindVertexArray( vaoA );
			for ( AMesh & m : am ) {
				f.genBuffers( 1, &m.vb );
				f.genBuffers( 1, &m.tb );
				f.genBuffers( 1, &m.ib );
				f.bindBuffer( kArrayBuf, m.vb );
				f.bufferData( kArrayBuf, std::ptrdiff_t( m.p.size() * 4 ), m.p.data(), kDynamic );
				f.bindBuffer( kArrayBuf, m.tb );
				f.bufferData( kArrayBuf, std::ptrdiff_t( m.t.size() * 4 ), m.t.data(), kDynamic );
				f.bindBuffer( kElemBuf, m.ib );
				f.bufferData( kElemBuf, std::ptrdiff_t( m.i.size() * 2 ), m.i.data(), kDynamic );
			}
		}

		// the alpha casters: mesh, model-view (moved per frame)
		const int kAC = 8;
		const int meshOfA[kAC] = { 1, 1, 0, 2, 1, 0, 2, 1 };
		std::vector<Caster> ac( kAC );
		for ( int i = 0; i < kAC; i++ )
			ac[i] = { u( rng ), u( rng ), u( rng ) + 1.5, 3.14159 * u( rng ), 90.0 * u( rng ), 90.0 * u( rng ), -150.0 + 80.0 * u( rng ),
				10.0 + 14.0 * ( 0.5 + 0.5 * u( rng ) ) };
		std::vector<float> amv( std::size_t( kAC ) * 16 );
		auto setAM = [&]( double move ) {
			for ( int i = 0; i < kAC; i++ ) {
				const Caster & c = ac[i];
				double M[16];
				rotT( c.ax, c.ay, c.az, c.ang + 0.25 * move, c.tx + 5.0 * move, c.ty - 3.0 * move, c.tz + 2.0 * move, c.s, M );
				for ( int k = 0; k < 16; k++ )
					amv[std::size_t( i ) * 16 + k] = float( M[k] );
			}
		};
		// one record a caster (one CsmDraw): target, layer, cascade, scissor, cull, caster, texture, uvScale, uvOffset, alpha,
		// threshold, ranges (two cullIdx ranges: the first third, a fifth from the middle)
		struct ARec
		{
			int target, layer, cascade;
			int sc[4];
			bool cull;
			int caster, tex;
			float uvS[2], uvO[2], a, ref;
			bool ranges;
		};
		const std::vector<ARec> arecs = {
			{ 0, 0, 0, { 0, 0, 0, 0 }, true, 0, 0, { 1.3f, 0.8f }, { 0.1f, -0.2f }, 0.9f, 0.5f, false },
			{ 0, 0, 0, { 0, 0, 0, 0 }, false, 1, 1, { 2.1f, 1.7f }, { -0.35f, 0.4f }, 1.0f, 0.31f, false },  // two-sided
			{ 0, 1, 1, { 0, 700, map - 24, map - 11 }, true, 2, 1, { 1.1f, 1.2f }, { 0.05f, 0.15f }, 0.8f, 0.45f, false },  // scissor strip
			{ 0, 1, 1, { 0, 0, 0, 0 }, true, 3, 0, { 0.9f, 1.4f }, { -0.3f, 0.2f }, 0.9f, 0.5f, true },  // cullIdx ranges
			{ 1, 2, 2, { 0, 0, 0, 0 }, true, 4, 1, { 1.6f, 0.7f }, { 0.25f, -0.1f }, 0.95f, 0.62f, false },  // the static target
			{ 0, 2, 2, { 0, 0, 0, 0 }, true, 4, 1, { 1.6f, 0.7f }, { 0.25f, -0.1f }, 0.95f, 0.62f, false },  // ... its live duplicate
			{ 1, 1, 1, { 0, 0, 0, 0 }, false, 5, 0, { 1.2f, 1.2f }, { 0.4f, 0.4f }, 0.7f, 0.2f, false },  // two-sided, static
			{ 0, 2, 2, { 0, 0, 0, 0 }, true, 6, 1, { 0.8f, 2.2f }, { -0.15f, -0.45f }, 0.8f, 0.45f, false },
		};
		const int kAD = 8;  // alpha draws a frame (one CsmDraw a record)
		auto rangesOf = [&]( const ARec & R ) {
			std::vector<std::array<std::uint32_t, 2>> out;
			const std::uint32_t n = std::uint32_t( am[std::size_t( meshOfA[R.caster] )].i.size() ), tris = n / 3;
			if ( R.ranges ) {
				out.push_back( { 0u, ( tris / 3 ) * 3 } );
				out.push_back( { ( tris / 2 ) * 3, ( tris / 5 ) * 3 } );
			} else {
				out.push_back( { 0u, n } );
			}
			return out;
		};
		// GL: refOff < 0 replaces every threshold (the "no test" picture for the alpha-cut count)
		auto glAlpha = [&]( const GLuint * tex, const ARec & R, float refOff ) {
			if ( !attach( tex[R.target], R.layer ) )
				return false;
			const Casc & c = casc[R.cascade];
			glViewport( 0, 0, c.vw, c.vh );
			const bool sc = R.sc[2] > R.sc[0];
			if ( sc ) {
				glEnable( GL_SCISSOR_TEST );
				glScissor( R.sc[0], R.sc[1], R.sc[2] - R.sc[0], R.sc[3] - R.sc[1] );
			}
			if ( f.polygonOffsetClamp )
				f.polygonOffsetClamp( slopeBias, unitBias, c.clamp );
			f.uniformMatrix4fv( aClip, 1, GL_FALSE, c.clip );
			f.uniformMatrix4fv( aMv, 1, GL_FALSE, &amv[std::size_t( R.caster ) * 16] );
			uni2f( aUvS, R.uvS[0], R.uvS[1] );
			uni2f( aUvO, R.uvO[0], R.uvO[1] );
			f.uniform1f( aAlpha, R.a );
			f.uniform1f( aRef, refOff < 0.0f ? refOff : R.ref );
			glBindTexture( kTex2D, tw[R.tex] );
			R.cull ? glEnable( GL_CULL_FACE ) : glDisable( GL_CULL_FACE );
			const AMesh & m = am[std::size_t( meshOfA[R.caster] )];
			f.bindBuffer( kArrayBuf, m.vb );
			f.enableVertexAttribArray( 0 );
			f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
			f.bindBuffer( kArrayBuf, m.tb );
			f.enableVertexAttribArray( 7 );
			f.vertexAttribPointer( 7, 2, GL_FLOAT, GL_FALSE, 8, nullptr );
			f.bindBuffer( kElemBuf, m.ib );
			for ( const auto & r : rangesOf( R ) )
				glDrawElements( GL_TRIANGLES, GLsizei( r[1] ), GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( std::size_t( r[0] ) * 2 ) );
			if ( sc )
				glDisable( GL_SCISSOR_TEST );
			return true;
		};
		// alphaMode: 0 none, 1 the alpha casters, 2 the alpha casters with no test (threshold -1)
		auto runGlA = [&]( int alphaMode, std::vector<std::uint16_t> out[2][3] ) {
			bool ok2 = clearAll( refT );
			glState();
			ok2 = ok2 && ground( refT );
			for ( const Rec & R : recs )
				ok2 = ok2 && glDraw( refT, R );
			if ( alphaMode ) {
				f.useProgram( progA );
				f.bindVertexArray( vaoA );
				activeTex( kTex0 );
				for ( const ARec & R : arecs )
					ok2 = ok2 && glAlpha( refT, R, alphaMode == 2 ? -1.0f : 0.0f );
				glBindTexture( kTex2D, 0 );
			}
			readAll( refT, out );
			return ok2 && glGetError() == GL_NO_ERROR;
		};
		enum ARed { kANone, kATestOff, kAWrong, kAUv, kAAlpha };
		auto runVkA = [&]( ARed red, std::vector<std::uint16_t> out[2][3] ) {
			bool ok2 = clearAll( shT );
			glState();
			ok2 = ok2 && ground( shT );
			WwVkPresent::CsmRun run;
			run.map = map;
			run.layers = 3;
			for ( int t = 0; t < 2; t++ ) {
				run.tex[t] = shT[t];
				run.key[t] = reinterpret_cast<std::uintptr_t>( &keyHome[t] );
			}
			// the matrices: the queue's casters, then the alpha casters (one buffer, as sunshadow.cpp appends them)
			std::vector<float> allM( mats );
			allM.insert( allM.end(), amv.begin(), amv.end() );
			run.mats = allM.data();
			run.nMats = allM.size() / 16;
			auto base = [&]( const Casc & c, int ci, int target, int layer, const int * scr, bool cull ) {
				WwVkPresent::CsmDraw d;
				d.target = target;
				d.layer = layer;
				d.vw = c.vw;
				d.vh = c.vh;
				std::memcpy( d.sc, scr, sizeof( d.sc ) );
				std::memcpy( d.clip, casc[ci].clip, sizeof( d.clip ) );
				d.unit = unitBias;
				d.slope = slopeBias;
				d.clamp = c.clamp;
				d.cull = cull;
				return d;
			};
			run.pos = bPos;
			run.idx = bIdx;
			run.posBytes = pos.size() * 4;
			run.idxBytes = idx.size() * 2;
			run.cmds = reinterpret_cast<const std::uint32_t *>( cmds.data() );
			run.nCmds = cmds.size();
			for ( const Rec & R : recs ) {
				WwVkPresent::CsmDraw d = base( casc[R.cascade], R.cascade, R.target, R.layer, R.sc, R.cull );
				d.first = R.first;
				d.n = R.n;
				run.d.push_back( d );
			}
			std::vector<WwVkPresent::CsmMesh> cm( am.size() );
			for ( std::size_t m = 0; m < am.size(); m++ ) {
				cm[m].key = reinterpret_cast<std::uintptr_t>( &am[m] );
				cm[m].gen = 7000 + m;
				cm[m].pos = am[m].p.data();
				cm[m].uv = am[m].t.data();
				cm[m].verts = std::uint32_t( am[m].p.size() / 3 );
				cm[m].idx = am[m].i.data();
				cm[m].indices = std::uint32_t( am[m].i.size() );
			}
			std::vector<std::uint32_t> md;
			for ( const ARec & R : arecs ) {
				const std::uint32_t first = std::uint32_t( md.size() / 4 );
				for ( const auto & r : rangesOf( R ) ) {
					md.push_back( std::uint32_t( meshOfA[R.caster] ) );
					md.push_back( std::uint32_t( kCasters + R.caster ) );
					md.push_back( r[0] );
					md.push_back( r[1] );
				}
				WwVkPresent::CsmDraw d = base( casc[R.cascade], R.cascade, R.target, R.layer, R.sc, R.cull );
				d.mesh = true;
				d.first = first;
				d.n = std::uint32_t( md.size() / 4 ) - first;
				if ( red != kATestOff ) {
					d.atTex = tw[red == kAWrong ? 2 : R.tex];
					const bool noUv = red == kAUv;
					d.uvScale[0] = noUv ? 1.0f : R.uvS[0];
					d.uvScale[1] = noUv ? 1.0f : R.uvS[1];
					d.uvOffset[0] = noUv ? 0.0f : R.uvO[0];
					d.uvOffset[1] = noUv ? 0.0f : R.uvO[1];
					d.atAlpha = red == kAAlpha ? 1.0f : R.a;
					d.atRef = R.ref;
				}
				run.d.push_back( d );
			}
			run.meshes = cm.data();
			run.nMeshes = cm.size();
			run.mdraws = md.data();
			run.nMDraws = md.size() / 4;
			std::string w;
			if ( !WwVkPresent::csmRun( run, w ) ) {
				std::printf( "SHADOWVIEW alpha csmRun refused: %s\n", w.c_str() );
				return false;
			}
			readAll( shT, out );
			return ok2 && glGetError() == GL_NO_ERROR;
		};
		auto aOk = [&]( const Diff & v ) { return v.covered > 0 && v.diff * 2000 <= v.covered && v.past1 * 10000 <= v.covered; };
		auto aWord = [&]( const Diff & v ) { return v.covered == 0 ? "FAIL" : v.diff == 0 ? "EXACT" : aOk( v ) ? "BOUND" : "FAIL"; };
		// the layers that carry alpha casters
		bool hasA[2][3] = {};
		for ( const ARec & R : arecs )
			hasA[R.target][R.layer] = true;

		std::vector<std::uint16_t> refA[2][2][3], vkA[2][3], noTest[2][3], bareA[2][3];
		WwVkPresent::takeCsmStat();
		for ( int fr = 0; fr < 2 && atOk; fr++ ) {
			upload( moves[fr] );
			setAM( double( fr ) );
			if ( !runGlA( 1, refA[fr] ) || !runVkA( kANone, vkA ) ) {
				std::printf( "SHADOWVIEW alpha frame%d FAIL (a run failed)\n", fr );
				atOk = false;
				break;
			}
			const WwVkPresent::CsmStat as = WwVkPresent::takeCsmStat();
			std::printf( "SHADOWVIEW alpha stat frame%d: %d runs on Vulkan, %d failed (%s), %d draws, %d mesh draws, alphaDraws=%d (>= %d) "
				"alphaTex=%d (>= 2)\n", fr, as.runs, as.fails, as.failWhy.c_str(), as.draws, as.mdraws, as.alphaDraws, kAD, as.alphaTex );
			atOk = atOk && as.alphaDraws >= kAD && as.alphaTex >= 2;
			for ( int t = 0; t < 2; t++ )
				for ( int l = 0; l < 3; l++ ) {
					const Diff v = compare( vkA[t][l], refA[fr][t][l] );
					std::printf( "SHADOWVIEW alpha frame%d %s/%d %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d)\n", fr,
						t ? "static" : "live", l, aWord( v ), v.diff, v.covered, v.past1, v.maxu );
					atOk = atOk && aOk( v );
				}
			// the test must cut texels on every layer with alpha casters, and the casters must win texels, or this proves nothing
			if ( runGlA( 2, noTest ) && runGlA( 0, bareA ) ) {
				std::size_t won = 0;
				for ( int t = 0; t < 2; t++ )
					for ( int l = 0; l < 3; l++ ) {
						std::size_t cut = 0;
						for ( std::size_t i = 0; i < noTest[t][l].size(); i++ ) {
							cut += noTest[t][l][i] != refA[fr][t][l][i];
							won += bareA[t][l][i] != refA[fr][t][l][i];
						}
						if ( hasA[t][l] ) {
							std::printf( "SHADOWVIEW alpha frame%d %s/%d alpha-cut %zu texels (GL with the test against no test; must be > 0)\n",
								fr, t ? "static" : "live", l, cut );
							atOk = atOk && cut > 0;
						}
					}
				std::printf( "SHADOWVIEW alpha frame%d won %zu texels against GL without the alpha casters (must be > 0)\n", fr, won );
				atOk = atOk && won > 0;
			} else {
				std::printf( "SHADOWVIEW alpha frame%d FAIL (the GL no-test / bare runs)\n", fr );
				atOk = false;
			}
		}
		// the reds on frame 1's content: each must FAIL (past 1 unit on > 0.01 % of all covered texels)
		const struct
		{
			const char * name;
			ARed red;
		} areds[4] = { { "test off (the alpha casters drawn as plain mesh draws)", kATestOff },
			{ "wrong (every alpha draw samples T2)", kAWrong }, { "uv (uvScale 1, uvOffset 0)", kAUv },
			{ "alpha (atAlpha 1)", kAAlpha } };
		for ( const auto & r : areds ) {
			if ( !atOk )
				break;
			const bool ran = runVkA( r.red, vkA );
			Diff all;
			for ( int t = 0; t < 2; t++ )
				for ( int l = 0; l < 3; l++ )
					all.add( compare( vkA[t][l], refA[1][t][l] ) );
			const bool redFails = !ran || all.past1 * 10000 > all.covered;
			std::printf( "SHADOWVIEW alpha RED %s %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d; must FAIL)\n",
				redFails ? "FAIL" : "TESTS NOTHING", r.name, all.diff, all.covered, all.past1, all.maxu );
			atOk = atOk && redFails;
		}
		WwVkPresent::takeCsmStat();
	}
	std::printf( "SHADOWVIEW alpha %s\n", atOk ? "OK" : "FAILED" );
