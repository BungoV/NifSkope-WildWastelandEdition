	// Lane VKSHADOW11 (2026-11-16, item 2 bar, written and committed before the first run), lines `SHADOWVIEW qalpha ...`.
	// Included inside the alpha case's block (shadowview_alpha.inc, after its reds): it reuses that case's BC twins (tw), meshes
	// (am), casters and their in-window placement (ac, amv, setAM), records (arecs, rangesOf) and bar (aOk / aWord); it draws
	// no rng, so the sway lines after it stay byte-equal.
	// The QAlpha queue (sunshadow.cpp drawJob: lookdev_csmdepth_atq.prog = lookdev_csmdepth_atq.vert + lookdev_csmdepth_at.frag,
	// one glMultiDrawElementsIndirect a caster span from the shared pool: position 0, first UV set 7, matrix 8-11) moved onto
	// the present device as mesh alpha draws: the pool holds each shape's triangles, verts and first UV set verbatim, so a span's
	// commands become mesh draws of the caster's pooled CsmMesh through WwVkPresent::csmSpanMDraws (the helper sunshadow.cpp
	// calls): mesh-local first index = command firstIndex - the entry's firstIndex, count = the command's count. One CsmDraw a span
	// (atTex, uvScale / uvOffset, atAlpha, atRef, CsmDraw::queued set: CsmStat::qAlphaDraws counts them).
	// GL arm: its own pool (the base pool's vertices and indices first, so every alpha entry has a nonzero baseVertex and
	// firstIndex; then am[0..2] with their UVs; UVs of the base part 0), the base queue's matrices then amv, and one command list:
	// the base commands, then each record's span (the whole shape, or two cullIdx ranges: a 2-command span). An empty span (no
	// commands) is recorded too and must add no draw. The app's R1 sampler on unit 0. Vulkan arm: the base queue draws and the
	// span draws in one csmRun, GL's ground first on both arms. Two frames (moved matrices).
	// BOUND (stated before the first run, never widened): the alpha case's, unchanged (EXACT expected; else <= 0.05 % of a
	// layer's covered texels differ AND <= 0.01 % past 1 unit; a layer with 0 covered texels FAILs).
	// Must hold: every layer EXACT or BOUND in both frames; alpha-cut texels (GL atq with the test against GL atq with threshold
	// -1) > 0 on every layer with alpha casters; the spans win > 0 texels against GL without them; CsmStat alphaDraws >= the
	// frame's span draws (8) and qAlphaDraws >= 8 (the queued spans ran as alpha draws).
	// Reds on frame 1 (each must FAIL: past 1 unit on > 0.01 % of all covered texels; each stays inside the meshes' ranges so it
	// fails by picture, never by a csmRun refusal): test off (atTex 0), wrong (every span samples T2), whole (a 2-command span
	// drawn as its whole mesh), matrix (each span with the next caster's matrix).
	qaOk = atOk;
	{
		const GLuint progAQ = program( readFile( root + "/res/shaders/features/near/lookdev_csmdepth_atq.vert" ),
			readFile( root + "/res/shaders/features/shadows/lookdev_csmdepth_at.frag" ) );
		GLint qClip = -1, qUvS = -1, qUvO = -1, qAlpha = -1, qRef = -1;
		if ( qaOk && progAQ ) {
			qClip = f.getUniformLocation( progAQ, "csmClipFromView" );
			qUvS = f.getUniformLocation( progAQ, "uvScale" );
			qUvO = f.getUniformLocation( progAQ, "uvOffset" );
			qAlpha = f.getUniformLocation( progAQ, "alpha" );
			qRef = f.getUniformLocation( progAQ, "alphaThreshold" );
			f.useProgram( progAQ );
			f.uniform1i( f.getUniformLocation( progAQ, "BaseMap" ), 0 );
		}
		if ( !progAQ || qClip < 0 || qUvS < 0 || qUvO < 0 || qAlpha < 0 || qRef < 0 ) {
			std::printf( "SHADOWVIEW qalpha FAIL (lookdev_csmdepth_atq or a uniform of it)\n" );
			qaOk = false;
		}
		// the pool: the base part, then the alpha meshes (entry = baseVertex, firstIndex)
		std::vector<float> qp( pos ), qt( pos.size() / 3 * 2, 0.0f );
		std::vector<std::uint16_t> qi( idx );
		std::vector<std::int32_t> eBase( am.size() );
		std::vector<std::uint32_t> eFirst( am.size() );
		for ( std::size_t m = 0; m < am.size(); m++ ) {
			eBase[m] = std::int32_t( qp.size() / 3 );
			eFirst[m] = std::uint32_t( qi.size() );
			qp.insert( qp.end(), am[m].p.begin(), am[m].p.end() );
			qt.insert( qt.end(), am[m].t.begin(), am[m].t.end() );
			qi.insert( qi.end(), am[m].i.begin(), am[m].i.end() );
		}
		// the commands: the base ones, then one span a record (and one empty span), as sunshadow.cpp's atSpan
		std::vector<DrawCmd> qc( cmds );
		struct QSpan
		{
			std::size_t rec, first, n;
		};
		std::vector<QSpan> qs;
		for ( std::size_t a = 0; a < arecs.size(); a++ ) {
			const ARec & R = arecs[a];
			const int m = meshOfA[R.caster];
			const std::size_t f0 = qc.size();
			for ( const auto & r : rangesOf( R ) )
				qc.push_back( { r[1], 1, eFirst[std::size_t( m )] + r[0], eBase[std::size_t( m )], std::uint32_t( kCasters + R.caster ) } );
			qs.push_back( { a, f0, qc.size() - f0 } );
			if ( a == 2 )
				qs.push_back( { a, qc.size(), 0 } );  // an empty span (every command culled): no draw
		}
		GLuint qPos = 0, qUv = 0, qIdx = 0, qMat = 0, qCmd = 0, qVao = 0;
		if ( qaOk ) {
			f.genBuffers( 1, &qPos );
			f.genBuffers( 1, &qUv );
			f.genBuffers( 1, &qIdx );
			f.genBuffers( 1, &qMat );
			f.genBuffers( 1, &qCmd );
			f.genVertexArrays( 1, &qVao );
			f.bindVertexArray( qVao );
			f.bindBuffer( kArrayBuf, qPos );
			f.bufferData( kArrayBuf, std::ptrdiff_t( qp.size() * 4 ), qp.data(), kDynamic );
			f.enableVertexAttribArray( 0 );
			f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
			f.bindBuffer( kArrayBuf, qUv );
			f.bufferData( kArrayBuf, std::ptrdiff_t( qt.size() * 4 ), qt.data(), kDynamic );
			f.enableVertexAttribArray( 7 );
			f.vertexAttribPointer( 7, 2, GL_FLOAT, GL_FALSE, 8, nullptr );
			f.bindBuffer( kArrayBuf, qMat );
			f.bufferData( kArrayBuf, std::ptrdiff_t( ( mats.size() + amv.size() ) * 4 ), nullptr, kDynamic );
			for ( int c = 0; c < 4; c++ ) {
				f.enableVertexAttribArray( GLuint( 8 + c ) );
				f.vertexAttribPointer( GLuint( 8 + c ), 4, GL_FLOAT, GL_FALSE, 64, reinterpret_cast<const void *>( std::size_t( c ) * 16 ) );
				f.vertexAttribDivisor( GLuint( 8 + c ), 1 );
			}
			f.bindBuffer( kElemBuf, qIdx );
			f.bufferData( kElemBuf, std::ptrdiff_t( qi.size() * 2 ), qi.data(), kDynamic );
			f.bindBuffer( kIndirect, qCmd );
			f.bufferData( kIndirect, std::ptrdiff_t( qc.size() * sizeof( DrawCmd ) ), qc.data(), kDynamic );
			f.bindVertexArray( 0 );
			std::printf( "SHADOWVIEW qalpha pool %zu vertices, %zu indices, %zu commands, %zu spans (alpha entries at baseVertex %d %d %d)\n",
				qp.size() / 3, qi.size(), qc.size(), qs.size(), eBase[0], eBase[1], eBase[2] );
		}
		std::vector<float> allQ;
		auto uploadQ = [&]() {
			allQ = mats;
			allQ.insert( allQ.end(), amv.begin(), amv.end() );
			f.bindBuffer( kArrayBuf, qMat );
			f.bufferSubData( kArrayBuf, 0, std::ptrdiff_t( allQ.size() * 4 ), allQ.data() );
		};
		// GL: the base content, then (spans) each span as one multi-draw; refOff < 0 replaces every threshold
		auto runGlQ = [&]( bool spans, float refOff, std::vector<std::uint16_t> out[2][3] ) {
			bool ok2 = clearAll( refT );
			glState();
			ok2 = ok2 && ground( refT );
			for ( const Rec & R : recs )
				ok2 = ok2 && glDraw( refT, R );
			if ( spans ) {
				f.useProgram( progAQ );
				f.bindVertexArray( qVao );
				f.bindBuffer( kIndirect, qCmd );
				activeTex( kTex0 );
				for ( const QSpan & q : qs ) {
					if ( !q.n )
						continue;
					const ARec & R = arecs[q.rec];
					if ( !attach( refT[R.target], R.layer ) ) {
						ok2 = false;
						break;
					}
					const Casc & c = casc[R.cascade];
					glViewport( 0, 0, c.vw, c.vh );
					const bool sc = R.sc[2] > R.sc[0];
					if ( sc ) {
						glEnable( GL_SCISSOR_TEST );
						glScissor( R.sc[0], R.sc[1], R.sc[2] - R.sc[0], R.sc[3] - R.sc[1] );
					}
					if ( f.polygonOffsetClamp )
						f.polygonOffsetClamp( slopeBias, unitBias, c.clamp );
					f.uniformMatrix4fv( qClip, 1, GL_FALSE, c.clip );
					uni2f( qUvS, R.uvS[0], R.uvS[1] );
					uni2f( qUvO, R.uvO[0], R.uvO[1] );
					f.uniform1f( qAlpha, R.a );
					f.uniform1f( qRef, refOff < 0.0f ? refOff : R.ref );
					glBindTexture( kTex2D, tw[R.tex] );
					const GLint wrapA = R.tex == 0 ? GL_REPEAT : GLint( kClampEdge );
					const GLuint smpA = GLuint( ShadowAt::sampler( csmGlProc, wrapA, wrapA, GL_LINEAR_MIPMAP_LINEAR ) );
					if ( !smpA ) {
						ok2 = false;
						break;
					}
					bindSmp( 0, smpA );
					R.cull ? glEnable( GL_CULL_FACE ) : glDisable( GL_CULL_FACE );
					f.multiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT,
						reinterpret_cast<const void *>( q.first * sizeof( DrawCmd ) ), GLsizei( q.n ), GLsizei( sizeof( DrawCmd ) ) );
					if ( sc )
						glDisable( GL_SCISSOR_TEST );
				}
				bindSmp( 0, 0 );
				glBindTexture( kTex2D, 0 );
				f.bindVertexArray( 0 );
			}
			readAll( refT, out );
			return ok2 && glGetError() == GL_NO_ERROR;
		};
		enum QRed { kQNone, kQTestOff, kQWrong, kQWhole, kQMatrix };
		int spanDraws = 0;
		auto runVkQ = [&]( QRed red, std::vector<std::uint16_t> out[2][3] ) {
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
			run.mats = allQ.data();
			run.nMats = allQ.size() / 16;
			run.pos = bPos;
			run.idx = bIdx;
			run.posBytes = pos.size() * 4;
			run.idxBytes = idx.size() * 2;
			run.cmds = reinterpret_cast<const std::uint32_t *>( cmds.data() );
			run.nCmds = cmds.size();
			auto base = [&]( int ci, int target, int layer, const int * scr, bool cull ) {
				WwVkPresent::CsmDraw d;
				const Casc & c = casc[ci];
				d.target = target;
				d.layer = layer;
				d.vw = c.vw;
				d.vh = c.vh;
				std::memcpy( d.sc, scr, sizeof( d.sc ) );
				std::memcpy( d.clip, c.clip, sizeof( d.clip ) );
				d.unit = unitBias;
				d.slope = slopeBias;
				d.clamp = c.clamp;
				d.cull = cull;
				return d;
			};
			for ( const Rec & R : recs ) {
				WwVkPresent::CsmDraw d = base( R.cascade, R.target, R.layer, R.sc, R.cull );
				d.first = R.first;
				d.n = R.n;
				run.d.push_back( d );
			}
			// the pooled meshes: key = the mesh, its arrays as the pool holds them
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
			spanDraws = 0;
			for ( const QSpan & q : qs ) {
				const ARec & R = arecs[q.rec];
				const std::uint32_t m = std::uint32_t( meshOfA[R.caster] );
				const std::uint32_t mat = std::uint32_t( kCasters + ( red == kQMatrix ? ( R.caster + 1 ) % kAC : R.caster ) );
				const std::uint32_t first = std::uint32_t( md.size() / 4 );
				if ( red == kQWhole && q.n > 1 )
					md.insert( md.end(), { m, mat, 0u, std::uint32_t( am[m].i.size() ) } );
				else
					WwVkPresent::csmSpanMDraws( reinterpret_cast<const std::uint32_t *>( qc.data() ), q.first, q.n, eFirst[m], m, mat, md );
				const std::uint32_t n = std::uint32_t( md.size() / 4 ) - first;
				if ( !n )
					continue;  // an empty span: no draw (sunshadow.cpp skips it)
				WwVkPresent::CsmDraw d = base( R.cascade, R.target, R.layer, R.sc, R.cull );
				d.mesh = true;
				d.queued = true;
				d.first = first;
				d.n = n;
				if ( red != kQTestOff ) {
					d.atTex = tw[red == kQWrong ? 2 : R.tex];
					d.uvScale[0] = R.uvS[0];
					d.uvScale[1] = R.uvS[1];
					d.uvOffset[0] = R.uvO[0];
					d.uvOffset[1] = R.uvO[1];
					d.atAlpha = R.a;
					d.atRef = R.ref;
				}
				run.d.push_back( d );
				spanDraws++;
			}
			run.meshes = cm.data();
			run.nMeshes = cm.size();
			run.mdraws = md.data();
			run.nMDraws = md.size() / 4;
			std::string w;
			if ( !WwVkPresent::csmRun( run, w ) ) {
				std::printf( "SHADOWVIEW qalpha csmRun refused: %s\n", w.c_str() );
				return false;
			}
			readAll( shT, out );
			return ok2 && glGetError() == GL_NO_ERROR;
		};

		std::vector<std::uint16_t> refQ[2][2][3], vkQ[2][3], noTestQ[2][3], bareQ[2][3];
		WwVkPresent::takeCsmStat();
		for ( int fr = 0; fr < 2 && qaOk; fr++ ) {
			upload( moves[fr] );
			setAM( double( fr ) );
			uploadQ();
			if ( !runGlQ( true, 0.0f, refQ[fr] ) || !runVkQ( kQNone, vkQ ) ) {
				std::printf( "SHADOWVIEW qalpha frame%d FAIL (a run failed)\n", fr );
				qaOk = false;
				break;
			}
			const WwVkPresent::CsmStat qst = WwVkPresent::takeCsmStat();
			std::printf( "SHADOWVIEW qalpha stat frame%d: %d runs on Vulkan, %d failed (%s), %d draws, %d mesh draws, span draws %d, "
				"alphaDraws=%d (>= %d) qAlphaDraws=%d (>= %d) alphaTex=%d\n", fr, qst.runs, qst.fails, qst.failWhy.c_str(), qst.draws, qst.mdraws,
				spanDraws, qst.alphaDraws, kAD, qst.qAlphaDraws, kAD, qst.alphaTex );
			qaOk = qaOk && spanDraws == kAD && qst.alphaDraws >= kAD && qst.qAlphaDraws >= kAD;
			for ( int t = 0; t < 2; t++ )
				for ( int l = 0; l < 3; l++ ) {
					const Diff v = compare( vkQ[t][l], refQ[fr][t][l] );
					std::printf( "SHADOWVIEW qalpha frame%d %s/%d %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d)\n", fr,
						t ? "static" : "live", l, aWord( v ), v.diff, v.covered, v.past1, v.maxu );
					qaOk = qaOk && aOk( v );
				}
			if ( runGlQ( true, -1.0f, noTestQ ) && runGlQ( false, 0.0f, bareQ ) ) {
				std::size_t won = 0;
				for ( int t = 0; t < 2; t++ )
					for ( int l = 0; l < 3; l++ ) {
						std::size_t cut = 0;
						for ( std::size_t i = 0; i < noTestQ[t][l].size(); i++ ) {
							cut += noTestQ[t][l][i] != refQ[fr][t][l][i];
							won += bareQ[t][l][i] != refQ[fr][t][l][i];
						}
						if ( hasA[t][l] ) {
							std::printf( "SHADOWVIEW qalpha frame%d %s/%d alpha-cut %zu texels (GL atq with the test against no test; must be > 0)\n",
								fr, t ? "static" : "live", l, cut );
							qaOk = qaOk && cut > 0;
						}
					}
				std::printf( "SHADOWVIEW qalpha frame%d won %zu texels against GL without the spans (must be > 0)\n", fr, won );
				qaOk = qaOk && won > 0;
			} else {
				std::printf( "SHADOWVIEW qalpha frame%d FAIL (the GL no-test / bare runs)\n", fr );
				qaOk = false;
			}
		}
		const struct
		{
			const char * name;
			QRed red;
		} qreds[4] = { { "test off (the spans drawn as plain mesh draws)", kQTestOff }, { "wrong (every span samples T2)", kQWrong },
			{ "whole (a 2-command span drawn as its whole mesh)", kQWhole }, { "matrix (each span with the next caster's matrix)", kQMatrix } };
		for ( const auto & r : qreds ) {
			if ( !qaOk )
				break;
			const bool ran = runVkQ( r.red, vkQ );
			Diff all;
			for ( int t = 0; t < 2; t++ )
				for ( int l = 0; l < 3; l++ )
					all.add( compare( vkQ[t][l], refQ[1][t][l] ) );
			const bool redFails = !ran || all.past1 * 10000 > all.covered;
			std::printf( "SHADOWVIEW qalpha RED %s %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d; must FAIL)\n",
				redFails ? "FAIL" : "TESTS NOTHING", r.name, all.diff, all.covered, all.past1, all.maxu );
			qaOk = qaOk && redFails;
		}
		WwVkPresent::takeCsmStat();
		f.useProgram( 0 );
	}
	std::printf( "SHADOWVIEW qalpha %s\n", qaOk ? "OK" : "FAILED" );
