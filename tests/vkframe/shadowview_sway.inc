	// Lane VKSHADOW10 (2026-11-16, item 3 bar, written and committed before the first run), lines `SHADOWVIEW sway ...` after
	// the alpha ones (those stay byte-equal). Included by shadowview.cpp's main after the alpha case.
	// The swaying per-draw casters (sunshadow.cpp drawJob pass 0, k.sway with colours: lookdev_csmdepth.vert with lodTreeAnim 1,
	// farSwayAmp / farSwayTime (this cascade's caster clock, swayTq) / farSwayBlock and globalUniforms.viewMatrix, the wind
	// weight = vertexColor.a) as CsmMesh draws with the sway fields (CsmDraw sway / swayAmp / swayTime / swayBlock / swayView,
	// CsmMesh wind = the vec4 colours). GL reference: the tree's own lookdev_csmdepth.vert / .frag (uniforms.glsl spliced in),
	// viewMatrix written into the bound globalUniforms block (std140 mat3: three vec4 columns).
	// Wires each get a value that would show: model coordinates in the hundreds (the phase dot( xy, (0.0021, 0.0017) ) spans
	// about +-1 rad over a mesh), wind weights 0..1 per vertex (some exactly 0), a distinct clock per cascade and per frame,
	// a view rotation that is not the identity, a block around caster 0 that holds part of it still (|world x|, |world y| <=
	// w), a scissor strip, two two-sided casters, a caster drawn as cullIdx ranges, the static target and its live duplicate.
	// The casters sit in light space inside their cascade's window at light z -45 (in front of the queued casters; the
	// VKSHADOW9 R2 lesson). Both arms first draw the base content (ground + the queued casters), then the sway casters: GL with
	// its own program, Vulkan in the same csmRun as the queue draws. Two frames (moved matrices, moved clocks).
	// BOUND (stated before the first run, never widened): EXACT expected (the products of lookdev_csmdepth.vert in GL's order).
	// Else a layer is BOUND when <= 0.05 % of its covered texels differ AND <= 0.01 % differ past 1 unit; past either: FAIL.
	// A layer with 0 covered texels FAILs.
	// Must hold: every layer EXACT or BOUND in both frames; on every layer with sway casters the wind moves > 0 texels (GL with
	// farSwayAmp 0 against GL with the wind); the block holds > 0 texels (GL with block w -1 against GL); the sway casters win
	// > 0 texels against GL without them; CsmStat swayDraws >= the frame's sway draws (7).
	// Reds on frame 1 (each must FAIL: past 1 unit on > 0.01 % of all covered texels): sway off (the sway casters as plain mesh
	// draws, sway false), phase (each draw takes the next cascade's clock), block (block w -1: the whole caster sways).
	bool swOk = true;
	{
		void( WINAPI * uni4f )( GLint, GLfloat, GLfloat, GLfloat, GLfloat ) = nullptr;
		void( WINAPI * disAttr )( GLuint ) = nullptr;
		std::string vsS = readFile( root + "/res/shaders/features/shadows/lookdev_csmdepth.vert" );
		const std::string inc = "#include \"uniforms.glsl\"";
		const std::size_t incAt = vsS.find( inc );
		if ( incAt != std::string::npos )
			vsS.replace( incAt, inc.size(), readFile( root + "/res/shaders/uniforms.glsl" ) );
		const GLuint progS = incAt == std::string::npos ? 0
			: program( vsS, readFile( root + "/res/shaders/features/shadows/lookdev_csmdepth.frag" ) );
		if ( !progS || !load( uni4f, "glUniform4f" ) || !load( disAttr, "glDisableVertexAttribArray" ) ) {
			std::printf( "SHADOWVIEW sway FAIL (the per-draw program or its entry points)\n" );
			swOk = false;
		}
		constexpr GLenum kUboS = 0x8A11;
		GLint sClip = -1, sMv = -1, sAmp = -1, sTime = -1, sBlock = -1, sAnim = -1;
		GLuint uboS = 0, vaoS = 0;
		// the view rotation (world -> view, GL column-major mat3) the colour pass would hold in globalUniforms
		double Vr[16];
		rotT( 0.2, 0.1, 1.0, 0.7, 0.0, 0.0, 0.0, 1.0, Vr );
		const float view9[9] = { float( Vr[0] ), float( Vr[1] ), float( Vr[2] ), float( Vr[4] ), float( Vr[5] ), float( Vr[6] ),
			float( Vr[8] ), float( Vr[9] ), float( Vr[10] ) };
		if ( swOk ) {
			sClip = f.getUniformLocation( progS, "csmClipFromView" );
			sMv = f.getUniformLocation( progS, "modelViewMatrix" );
			sAmp = f.getUniformLocation( progS, "farSwayAmp" );
			sTime = f.getUniformLocation( progS, "farSwayTime" );
			sBlock = f.getUniformLocation( progS, "farSwayBlock" );
			sAnim = f.getUniformLocation( progS, "lodTreeAnim" );
			f.useProgram( progS );
			f.uniform1i( f.getUniformLocation( progS, "farInst" ), 0 );
			std::vector<float> ublk( 1024, 0.0f );
			for ( int c = 0; c < 3; c++ )
				for ( int r = 0; r < 3; r++ )
					ublk[std::size_t( c ) * 4 + r] = view9[c * 3 + r];
			f.genBuffers( 1, &uboS );
			f.bindBuffer( kUboS, uboS );
			f.bufferData( kUboS, 4096, ublk.data(), kDynamic );
			const GLuint blk = f.getUniformBlockIndex( progS, "globalUniforms" );
			if ( blk != 0xFFFFFFFFu )
				f.uniformBlockBinding( progS, blk, 0 );
			f.genVertexArrays( 1, &vaoS );
			swOk = sClip >= 0 && sMv >= 0 && sAmp >= 0 && sTime >= 0 && sBlock >= 0 && sAnim >= 0 && blk != 0xFFFFFFFFu;
			if ( !swOk )
				std::printf( "SHADOWVIEW sway FAIL (a uniform of the per-draw program or its globalUniforms block is missing)\n" );
		}

		// the meshes, model coordinates in the hundreds, a vec4 colour per vertex (alpha = the wind weight)
		std::mt19937 rngS( 11 );
		struct SMesh
		{
			std::vector<float> p, c;
			std::vector<std::uint16_t> i;
			GLuint vb = 0, cb = 0, ib = 0;
		};
		std::vector<SMesh> sm( 3 );
		auto weight = [&]( float x, float y, float z ) {
			const float w = 0.5f + 0.6f * std::sin( 0.013f * x + 0.021f * y ) * std::cos( 0.017f * z + 0.011f * y );
			return std::clamp( w, 0.0f, 1.0f );  // a band clamps to exactly 0 (no wind) and 1
		};
		auto colour = [&]( SMesh & m, float x, float y, float z ) {
			m.c.push_back( 0.3f );
			m.c.push_back( 0.6f );
			m.c.push_back( 0.2f );
			m.c.push_back( weight( x, y, z ) );
		};
		for ( int k = 0; k < 50; k++ ) {
			const float qx = 220.0f * u( rngS ), qy = 220.0f * u( rngS ), qz = 60.0f * u( rngS );
			for ( int c = 0; c < 3; c++ ) {
				const float x = qx + 70.0f * u( rngS ), y = qy + 70.0f * u( rngS ), z = qz + 70.0f * u( rngS );
				sm[0].p.insert( sm[0].p.end(), { x, y, z } );
				colour( sm[0], x, y, z );
				sm[0].i.push_back( std::uint16_t( k * 3 + c ) );
			}
		}
		auto plateS = [&]( SMesh & m, int n, float e, float amp ) {
			for ( int y = 0; y <= n; y++ )
				for ( int x = 0; x <= n; x++ ) {
					const float px = e * ( -1.0f + 2.0f * float( x ) / n ), py = e * ( -1.0f + 2.0f * float( y ) / n );
					const float pz = amp * std::sin( 2.5f * px / e ) * std::cos( 1.5f * py / e );
					m.p.insert( m.p.end(), { px, py, pz } );
					colour( m, px, py, pz );
				}
			for ( int y = 0; y < n; y++ )
				for ( int x = 0; x < n; x++ ) {
					const std::uint16_t a = std::uint16_t( y * ( n + 1 ) + x ), b = std::uint16_t( a + 1 ), c = std::uint16_t( a + n + 1 ),
						d = std::uint16_t( c + 1 );
					const std::uint16_t q[6] = { a, b, d, a, d, c };
					m.i.insert( m.i.end(), q, q + 6 );
				}
		};
		plateS( sm[1], 12, 300.0f, 25.0f );
		plateS( sm[2], 22, 200.0f, 15.0f );
		if ( swOk ) {
			f.bindVertexArray( vaoS );
			for ( SMesh & m : sm ) {
				f.genBuffers( 1, &m.vb );
				f.genBuffers( 1, &m.cb );
				f.genBuffers( 1, &m.ib );
				f.bindBuffer( kArrayBuf, m.vb );
				f.bufferData( kArrayBuf, std::ptrdiff_t( m.p.size() * 4 ), m.p.data(), kDynamic );
				f.bindBuffer( kArrayBuf, m.cb );
				f.bufferData( kArrayBuf, std::ptrdiff_t( m.c.size() * 4 ), m.c.data(), kDynamic );
				f.bindBuffer( kElemBuf, m.ib );
				f.bufferData( kElemBuf, std::ptrdiff_t( m.i.size() * 2 ), m.i.data(), kDynamic );
			}
		}

		// the sway casters: mesh, rotation, scale, a light-space goal inside their cascade's window (light z -45)
		const int kSC = 6;
		const int meshOfS[kSC] = { 1, 2, 1, 0, 2, 1 };
		const double goal[kSC][2] = { { -18.0, 14.0 }, { 2.0, -2.0 }, { 10.0, 52.0 }, { -30.0, -30.0 }, { 40.0, -40.0 }, { -20.0, 20.0 } };
		const double scaleS[kSC] = { 0.035, 0.05, 0.05, 0.06, 0.12, 0.06 };
		std::vector<Caster> sc( kSC );
		for ( int i = 0; i < kSC; i++ )
			sc[i] = { u( rngS ), u( rngS ), u( rngS ) + 1.5, 3.14159 * u( rngS ), 0.0, 0.0, 0.0, scaleS[i] };
		std::vector<float> smv( std::size_t( kSC ) * 16 );
		double Ls[16];
		rotT( 0.3, 1.0, 0.2, 0.9, 0.0, 0.0, 0.0, 1.0, Ls );
		const double sx[3] = { Ls[0], Ls[4], Ls[8] }, sy[3] = { Ls[1], Ls[5], Ls[9] }, sz[3] = { Ls[2], Ls[6], Ls[10] };
		float block[4] = {};
		auto setSM = [&]( double move ) {
			for ( int i = 0; i < kSC; i++ ) {
				const Caster & c = sc[i];
				double M[16];
				rotT( c.ax, c.ay, c.az, c.ang + 0.2 * move, 0.0, 0.0, 0.0, c.s, M );
				// light-space goal moved with the frame (4 / -2 units in light x / y), light z -45: view = sum of the light axes
				const double gx = goal[i][0] + 4.0 * move, gy = goal[i][1] - 2.0 * move, gz = -45.0 + 60.0;
				for ( int k = 0; k < 3; k++ )
					M[12 + k] = gx * sx[k] + gy * sy[k] + gz * sz[k];
				for ( int k = 0; k < 16; k++ )
					smv[std::size_t( i ) * 16 + k] = float( M[k] );
			}
			// the block: centred on caster 0, half its plate's world span (300 * scale) as w, so part of it stays still
			for ( int k = 0; k < 3; k++ )
				block[k] = smv[12 + k];
			block[3] = float( 0.5 * 300.0 * scaleS[0] );
		};
		float swayT[3] = {};
		auto setClocks = [&]( int fr ) {
			const float base[3] = { 3.7f, 11.2f, 25.9f };
			for ( int c = 0; c < 3; c++ )
				swayT[c] = base[c] + 0.45f * float( fr );
		};
		const float kAmp = 2.5f;
		struct SRec
		{
			int target, layer, cascade;
			int sc[4];
			bool cull;
			int caster;
			bool ranges;
		};
		const std::vector<SRec> srecs = {
			{ 0, 0, 0, { 0, 0, 0, 0 }, true, 0, false },  // the block holds part of it
			{ 0, 0, 0, { 0, 0, 0, 0 }, false, 1, false },  // two-sided
			{ 0, 1, 1, { 0, 700, map - 24, map - 11 }, true, 2, false },  // scissor strip
			{ 0, 1, 1, { 0, 0, 0, 0 }, true, 3, true },  // cullIdx ranges
			{ 1, 2, 2, { 0, 0, 0, 0 }, true, 4, false },  // the static target
			{ 0, 2, 2, { 0, 0, 0, 0 }, true, 4, false },  // ... its live duplicate
			{ 1, 1, 1, { 0, 0, 0, 0 }, false, 5, false },  // two-sided, static
		};
		const int kSD = 7;
		auto rangesOfS = [&]( const SRec & R ) {
			std::vector<std::array<std::uint32_t, 2>> out;
			const std::uint32_t n = std::uint32_t( sm[std::size_t( meshOfS[R.caster] )].i.size() ), tris = n / 3;
			if ( R.ranges ) {
				out.push_back( { 0u, ( tris / 3 ) * 3 } );
				out.push_back( { ( tris / 2 ) * 3, ( tris / 5 ) * 3 } );
			} else {
				out.push_back( { 0u, n } );
			}
			return out;
		};
		// GL: amp 0 = no wind (the "moved" count), blockW < 0 replaces the block's w (the "held" count)
		auto glSway = [&]( const GLuint * tex, const SRec & R, float amp, float blockW ) {
			if ( !attach( tex[R.target], R.layer ) )
				return false;
			const Casc & c = casc[R.cascade];
			glViewport( 0, 0, c.vw, c.vh );
			const bool scr = R.sc[2] > R.sc[0];
			if ( scr ) {
				glEnable( GL_SCISSOR_TEST );
				glScissor( R.sc[0], R.sc[1], R.sc[2] - R.sc[0], R.sc[3] - R.sc[1] );
			}
			if ( f.polygonOffsetClamp )
				f.polygonOffsetClamp( slopeBias, unitBias, c.clamp );
			f.uniformMatrix4fv( sClip, 1, GL_FALSE, c.clip );
			f.uniformMatrix4fv( sMv, 1, GL_FALSE, &smv[std::size_t( R.caster ) * 16] );
			f.uniform1i( sAnim, 1 );
			f.uniform1f( sAmp, amp );
			f.uniform1f( sTime, swayT[R.cascade] );
			uni4f( sBlock, block[0], block[1], block[2], blockW < 0.0f ? blockW : block[3] );
			R.cull ? glEnable( GL_CULL_FACE ) : glDisable( GL_CULL_FACE );
			const SMesh & m = sm[std::size_t( meshOfS[R.caster] )];
			f.bindBuffer( kArrayBuf, m.vb );
			f.enableVertexAttribArray( 0 );
			f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
			f.bindBuffer( kArrayBuf, m.cb );
			f.enableVertexAttribArray( 1 );
			f.vertexAttribPointer( 1, 4, GL_FLOAT, GL_FALSE, 16, nullptr );
			f.bindBuffer( kElemBuf, m.ib );
			for ( const auto & r : rangesOfS( R ) )
				glDrawElements( GL_TRIANGLES, GLsizei( r[1] ), GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( std::size_t( r[0] ) * 2 ) );
			if ( scr )
				glDisable( GL_SCISSOR_TEST );
			return true;
		};
		// mode: 0 none, 1 the sway casters, 2 with no wind (amp 0), 3 with no block (w -1)
		auto runGlS = [&]( int mode, std::vector<std::uint16_t> out[2][3] ) {
			bool ok2 = clearAll( refT );
			glState();
			ok2 = ok2 && ground( refT );
			for ( const Rec & R : recs )
				ok2 = ok2 && glDraw( refT, R );
			if ( mode ) {
				f.useProgram( progS );
				f.bindVertexArray( vaoS );
				f.bindBufferBase( kUboS, 0, uboS );
				for ( const SRec & R : srecs )
					ok2 = ok2 && glSway( refT, R, mode == 2 ? 0.0f : kAmp, mode == 3 ? -1.0f : 0.0f );
				disAttr( 1 );
			}
			readAll( refT, out );
			return ok2 && glGetError() == GL_NO_ERROR;
		};
		enum SRed { kSNone, kSOff, kSPhase, kSBlock };
		auto runVkS = [&]( SRed red, std::vector<std::uint16_t> out[2][3] ) {
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
			std::vector<float> allM( mats );
			allM.insert( allM.end(), smv.begin(), smv.end() );
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
			std::vector<WwVkPresent::CsmMesh> cm( sm.size() );
			for ( std::size_t m = 0; m < sm.size(); m++ ) {
				cm[m].key = reinterpret_cast<std::uintptr_t>( &sm[m] );
				cm[m].gen = 9000 + m;
				cm[m].pos = sm[m].p.data();
				cm[m].wind = sm[m].c.data();
				cm[m].verts = std::uint32_t( sm[m].p.size() / 3 );
				cm[m].idx = sm[m].i.data();
				cm[m].indices = std::uint32_t( sm[m].i.size() );
			}
			std::vector<std::uint32_t> md;
			for ( const SRec & R : srecs ) {
				const std::uint32_t first = std::uint32_t( md.size() / 4 );
				for ( const auto & r : rangesOfS( R ) ) {
					md.push_back( std::uint32_t( meshOfS[R.caster] ) );
					md.push_back( std::uint32_t( kCasters + R.caster ) );
					md.push_back( r[0] );
					md.push_back( r[1] );
				}
				WwVkPresent::CsmDraw d = base( casc[R.cascade], R.cascade, R.target, R.layer, R.sc, R.cull );
				d.mesh = true;
				d.first = first;
				d.n = std::uint32_t( md.size() / 4 ) - first;
				if ( red != kSOff ) {
					d.sway = true;
					d.swayAmp = kAmp;
					d.swayTime = swayT[red == kSPhase ? ( R.cascade + 1 ) % 3 : R.cascade];
					std::memcpy( d.swayBlock, block, sizeof( d.swayBlock ) );
					if ( red == kSBlock )
						d.swayBlock[3] = -1.0f;
					std::memcpy( d.swayView, view9, sizeof( d.swayView ) );
				}
				run.d.push_back( d );
			}
			run.meshes = cm.data();
			run.nMeshes = cm.size();
			run.mdraws = md.data();
			run.nMDraws = md.size() / 4;
			std::string w;
			if ( !WwVkPresent::csmRun( run, w ) ) {
				std::printf( "SHADOWVIEW sway csmRun refused: %s\n", w.c_str() );
				return false;
			}
			readAll( shT, out );
			return ok2 && glGetError() == GL_NO_ERROR;
		};
		auto sOk = [&]( const Diff & v ) { return v.covered > 0 && v.diff * 2000 <= v.covered && v.past1 * 10000 <= v.covered; };
		auto sWord = [&]( const Diff & v ) { return v.covered == 0 ? "FAIL" : v.diff == 0 ? "EXACT" : sOk( v ) ? "BOUND" : "FAIL"; };
		bool hasS[2][3] = {};
		for ( const SRec & R : srecs )
			hasS[R.target][R.layer] = true;

		std::vector<std::uint16_t> refS[2][2][3], vkS[2][3], still[2][3], noBlock[2][3], bareS[2][3];
		WwVkPresent::takeCsmStat();
		for ( int fr = 0; fr < 2 && swOk; fr++ ) {
			upload( moves[fr] );
			setSM( double( fr ) );
			setClocks( fr );
			if ( !runGlS( 1, refS[fr] ) || !runVkS( kSNone, vkS ) ) {
				std::printf( "SHADOWVIEW sway frame%d FAIL (a run failed)\n", fr );
				swOk = false;
				break;
			}
			const WwVkPresent::CsmStat ss = WwVkPresent::takeCsmStat();
			std::printf( "SHADOWVIEW sway stat frame%d: %d runs on Vulkan, %d failed (%s), %d draws, %d mesh draws, swayDraws=%d (>= %d)\n",
				fr, ss.runs, ss.fails, ss.failWhy.c_str(), ss.draws, ss.mdraws, ss.swayDraws, kSD );
			swOk = swOk && ss.swayDraws >= kSD;
			for ( int t = 0; t < 2; t++ )
				for ( int l = 0; l < 3; l++ ) {
					const Diff v = compare( vkS[t][l], refS[fr][t][l] );
					std::printf( "SHADOWVIEW sway frame%d %s/%d %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d)\n", fr,
						t ? "static" : "live", l, sWord( v ), v.diff, v.covered, v.past1, v.maxu );
					swOk = swOk && sOk( v );
				}
			// the wind must move texels on every layer with sway casters, the block must hold some, the casters must win some
			if ( runGlS( 2, still ) && runGlS( 3, noBlock ) && runGlS( 0, bareS ) ) {
				std::size_t won = 0, held = 0;
				for ( int t = 0; t < 2; t++ )
					for ( int l = 0; l < 3; l++ ) {
						std::size_t moved = 0;
						for ( std::size_t i = 0; i < still[t][l].size(); i++ ) {
							moved += still[t][l][i] != refS[fr][t][l][i];
							held += noBlock[t][l][i] != refS[fr][t][l][i];
							won += bareS[t][l][i] != refS[fr][t][l][i];
						}
						if ( hasS[t][l] ) {
							std::printf( "SHADOWVIEW sway frame%d %s/%d wind-moved %zu texels (GL with the wind against amp 0; must be > 0)\n",
								fr, t ? "static" : "live", l, moved );
							swOk = swOk && moved > 0;
						}
					}
				std::printf( "SHADOWVIEW sway frame%d block-held %zu texels (GL against block w -1; must be > 0)\n", fr, held );
				std::printf( "SHADOWVIEW sway frame%d won %zu texels against GL without the sway casters (must be > 0)\n", fr, won );
				swOk = swOk && held > 0 && won > 0;
			} else {
				std::printf( "SHADOWVIEW sway frame%d FAIL (the GL still / no-block / bare runs)\n", fr );
				swOk = false;
			}
		}
		// the reds on frame 1's content: each must FAIL (past 1 unit on > 0.01 % of all covered texels)
		const struct
		{
			const char * name;
			SRed red;
		} sreds[3] = { { "sway off (the sway casters drawn as plain mesh draws)", kSOff },
			{ "phase (each draw takes the next cascade's clock)", kSPhase }, { "block (block w -1: the whole caster sways)", kSBlock } };
		for ( const auto & r : sreds ) {
			if ( !swOk )
				break;
			const bool ran = runVkS( r.red, vkS );
			Diff all;
			for ( int t = 0; t < 2; t++ )
				for ( int l = 0; l < 3; l++ )
					all.add( compare( vkS[t][l], refS[1][t][l] ) );
			const bool redFails = !ran || all.past1 * 10000 > all.covered;
			std::printf( "SHADOWVIEW sway RED %s %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d; must FAIL)\n",
				redFails ? "FAIL" : "TESTS NOTHING", r.name, all.diff, all.covered, all.past1, all.maxu );
			swOk = swOk && redFails;
		}
		WwVkPresent::takeCsmStat();
	}
	std::printf( "SHADOWVIEW sway %s\n", swOk ? "OK" : "FAILED" );
