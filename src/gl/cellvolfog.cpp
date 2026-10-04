/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellvolfog.h"

#include "gl/celllights.h"
#include "gl/glscene.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"
#include "gl/sunshadow.h"

#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <vector>

/* ---- lane VOLFOG1: the lit medium (cellvolfog.h, res/shaders/cell_volfog.frag) ---- */
namespace
{

constexpr int kVolTexUnit = 21;		// the integrated volume the surfaces read (sampler3D); 8, 10..19 are taken
constexpr int kInjectUnit = 22;		// stage 2 reads stage 1
constexpr int kEmitUnit = 23;		// the placed lights' shaft intensities (samplerBuffer)
constexpr int kAirUnit = 20;		// reserved: the air grid (unbound today: the surface grid serves)
constexpr int kColumnPx = 12;		// a froxel column: 12 x 12 pixels
constexpr int kSlices = 64;
constexpr float kNearMin = 16.0f;	// game units

struct VfState
{
	bool loaded = false;
	bool on = false, pinned = false;
	int red = 0;			// 1 off (computed, not applied), 2 gioff, 4 flat, 8 noshadow, 16 wrongsrc
	int terms = 7;
	int probe = 0;
	QString dump;
	bool inPass = false;
	QString last = QStringLiteral( "off" );
};

VfState & vs()
{
	static VfState s;
	if ( !s.loaded ) {
		s.loaded = true;
		const QByteArray pin = qgetenv( "WW_VOLFOG" );
		if ( !pin.isEmpty() ) {
			s.pinned = true;
			s.on = pin.trimmed() != "0";
		} else {
			s.on = QSettings().value( QStringLiteral( "WW/VolFog" ), false ).toBool();
		}
		for ( const QByteArray & t : qgetenv( "WW_VOLFOG_RED" ).split( ',' ) ) {
			const QByteArray r = t.trimmed();
			s.red |= r == "off" ? 1 : r == "gioff" ? 2 : r == "flat" ? 4 : r == "noshadow" ? 8 : r == "wrongsrc" ? 16 : 0;
		}
		const QByteArray terms = qgetenv( "WW_VOLFOG_TERMS" ).trimmed();
		if ( !terms.isEmpty() )
			s.terms = terms.toInt() & 7;
		s.probe = qEnvironmentVariableIntValue( "WW_VOLFOG_PROBE" );
		s.dump = QString::fromLocal8Bit( qgetenv( "WW_VOLFOG_DUMP" ) );
	}
	return s;
}

struct VfGpu
{
	GLuint inject = 0, integ = 0, fbo = 0, emitBuf = 0, emitTex = 0;
	int cols = 0, rows = 0, slices = 0;
	const void * doc = nullptr;
	bool ready = false;
	float near = kNearMin, logRatio = 1.0f;
	int unitsOk = -1;
};

QHash<const void *, VfGpu> & vfGpus()
{
	static QHash<const void *, VfGpu> g;
	return g;
}

void vfAlloc3D( NifSkopeOpenGLContext::GLFunctions * fn, GLuint & tex, int w, int h, int d, bool linear )
{
	if ( !tex )
		fn->glGenTextures( 1, &tex );
	fn->glBindTexture( GL_TEXTURE_3D, tex );
	fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA32F, w, h, d, 0, GL_RGBA, GL_FLOAT, nullptr );
	fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0 );
	fn->glBindTexture( GL_TEXTURE_3D, 0 );
}

// the medium's phase uniforms from a packed WwGodRayMedium (air rgb, fwd rgb, back rgb, fwd g, back g, post)
struct VfPhase
{
	float w[3][4] = {};	// rgb weight, w = g
	float gi[4] = {};	// rgb tint (the three weights summed), w = the luminance-weighted g
	float post = 1.0f;
};

VfPhase vfPhase( const float m[12] )
{
	VfPhase p;
	const float lumK[3] = { 0.2126f, 0.7152f, 0.0722f };
	float tot[3];
	for ( int c = 0; c < 3; c++ )
		tot[c] = m[c] + m[3 + c] + m[6 + c];
	const float lum = tot[0] * lumK[0] + tot[1] * lumK[1] + tot[2] * lumK[2];
	const float inv = lum > 1e-6f ? 1.0f / lum : 0.0f;
	for ( int k = 0; k < 3; k++ )
		for ( int c = 0; c < 3; c++ )
			p.w[k][c] = m[k * 3 + c] * inv;
	p.w[0][3] = 0.0f;
	p.w[1][3] = m[9];
	p.w[2][3] = m[10];
	auto lumOf = [&]( int k ) { return p.w[k][0] * lumK[0] + p.w[k][1] * lumK[1] + p.w[k][2] * lumK[2]; };
	for ( int c = 0; c < 3; c++ )
		p.gi[c] = p.w[0][c] + p.w[1][c] + p.w[2][c];
	p.gi[3] = lumOf( 1 ) * m[9] + lumOf( 2 ) * m[10];	// the air's g is 0; the lobes' luminances sum to 1
	p.post = m[11];
	return p;
}

}	// namespace

bool wwVolFogOn()
{
	return vs().on;
}

void wwVolFogSetOn( bool on )
{
	VfState & s = vs();
	if ( s.pinned )
		return;
	s.on = on;
	QSettings().setValue( QStringLiteral( "WW/VolFog" ), on );
}

QString wwVolFogSummary()
{
	return QStringLiteral( "volfog=" ) + vs().last;
}

void wwVolFogPass( Scene * scene, bool run )
{
	if ( !scene || !scene->renderer )
		return;
	VfState & s = vs();
	Renderer * r = scene->renderer;
	VfGpu & g = vfGpus()[r];
	g.ready = false;
	if ( !s.on ) {
		s.last = QStringLiteral( "off" );
		return;
	}
	if ( !run )
		return;
	if ( r->globalUniforms->projectionMatrix[3][3] == 1.0f ) {
		s.last = QStringLiteral( "refused(orthographic)" );
		return;
	}
	auto fn = r->fn;
	if ( g.unitsOk < 0 ) {
		GLint units = 0;
		fn->glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &units );
		g.unitsOk = units > kEmitUnit ? 1 : 0;
	}
	if ( !g.unitsOk ) {
		s.last = QStringLiteral( "refused(texture units)" );
		return;
	}
	QElapsedTimer timer;
	const bool timed = !s.dump.isEmpty() || qEnvironmentVariableIsSet( "WW_VOLFOG_TIME" );
	if ( timed ) {
		fn->glFinish();
		timer.start();
	}

	GLint prevFbo = 0, prevRead = 0, vp[4] = { 0, 0, 1, 1 }, prevActive = 0;
	GLboolean depthMask = GL_TRUE, colorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevFbo );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasCull = fn->glIsEnabled( GL_CULL_FACE );
	const bool wasBlend = fn->glIsEnabled( GL_BLEND ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	const bool wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	auto restore = [&]() {
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevFbo ) );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
		fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
		fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
		fn->glDepthMask( depthMask );
		if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST ); else fn->glDisable( GL_DEPTH_TEST );
		if ( wasCull ) fn->glEnable( GL_CULL_FACE ); else fn->glDisable( GL_CULL_FACE );
		if ( wasBlend ) fn->glEnable( GL_BLEND ); else fn->glDisable( GL_BLEND );
		if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST ); else fn->glDisable( GL_SCISSOR_TEST );
		if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST ); else fn->glDisable( GL_STENCIL_TEST );
		fn->glActiveTexture( GLenum( prevActive ) );
	};

	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_volfog.prog" );
	if ( !prog ) {
		s.last = QStringLiteral( "refused(cell_volfog.prog did not link)" );
		restore();
		return;
	}
	// the setters the surfaces get, on this program: the fog (weather or, indoors, the cell's), the cascades, the lights
	s.inPass = true;
	wwLookdevFogUniforms( scene );
	wwSunShadowUniforms( scene );
	wwCellLightsUniforms( scene );
	s.inPass = false;
	auto uniI = [&]( const char * n ) { GLint v = 0; const int l = prog->uniLocation( n ); if ( l >= 0 ) fn->glGetUniformiv( prog->id, l, &v ); return v; };
	auto uniF = [&]( const char * n, int i, float * o, int cnt ) {
		std::fill( o, o + cnt, 0.0f );
		const int l = i < 0 ? prog->uniLocation( n ) : prog->uniLocation( n, i );
		if ( l >= 0 )
			fn->glGetUniformfv( prog->id, l, o );
	};
	if ( !uniI( "fogOn" ) ) {
		r->stopProgram();
		s.last = QStringLiteral( "refused(no fog: the Fog row, Lookdev's weather or the interior's fog)" );
		restore();
		return;
	}
	float K[6][4], fogView[4], fogSun[4], fogSunCol[4], distScale = 1.0f;
	for ( int k = 0; k < 6; k++ )
		uniF( "fogK[%d]", k, K[k], 4 );
	uniF( "fogView", -1, fogView, 4 );
	uniF( "fogSun", -1, fogSun, 4 );
	uniF( "fogSunColour", -1, fogSunCol, 4 );
	uniF( "fogDistScale", -1, &distScale, 1 );
	if ( !( K[0][0] > 0.0f ) ) {
		r->stopProgram();
		s.last = QStringLiteral( "refused(fog scale %1)" ).arg( double( K[0][0] ) );
		restore();
		return;
	}
	// the slices: from where the fog starts (ramp 0) to its full distance (ramp 1), exponential
	const float fogStart = K[0][2] / K[0][0], fogFull = ( 1.0f + K[0][2] ) / K[0][0];
	const float farD = std::max( fogFull, kNearMin * 4.0f );
	const float nearD = std::min( std::max( kNearMin, fogStart ), farD * 0.25f );
	const float logRatio = std::log( farD / nearD );

	// the medium: indoors the cell's (XGDR, LGTM WGDR, fallback), outdoors the weather's WGDR, else the fallback
	const WwCellLighting * L = wwCellLightsWanted( scene ) ? wwCellLightsFor( scene->nifModel ) : nullptr;
	float med[12] = { 0.60f, 1.52f, 3.31f, 2, 2, 2, 1, 1, 1, 0.75f, 0, 1 };
	QString medNote = QStringLiteral( "fallback" );
	if ( L && L->interior ) {
		std::copy( L->godRay, L->godRay + 12, med );
		medNote = L->godRayNote;
	} else if ( !wwLookdevGodRays( med, &medNote ) ) {
		medNote = QStringLiteral( "fallback (no weather)" );
	}
	const VfPhase ph = vfPhase( med );

	const int W = std::max( vp[2], 1 ), H = std::max( vp[3], 1 );
	const int cols = ( W + kColumnPx - 1 ) / kColumnPx, rows = ( H + kColumnPx - 1 ) / kColumnPx;
	if ( g.cols != cols || g.rows != rows || g.slices != kSlices || !g.inject ) {
		vfAlloc3D( fn, g.inject, cols, rows, kSlices, false );
		vfAlloc3D( fn, g.integ, cols, rows, kSlices, true );
		if ( !g.fbo )
			fn->glGenFramebuffers( 1, &g.fbo );
		g.cols = cols;
		g.rows = rows;
		g.slices = kSlices;
	}

	// the placed lights that draw shafts, compacted: (the light's index in the cell's light buffer, its shaft
	// intensity) pairs; the rest (0: the game draws no shaft for them) never enter the shader's loop
	std::vector<float> emitK;
	int emitters = 0;
	if ( L )
		for ( int i = 0; i < int( L->lights.size() ); i++ )
			if ( L->lights[i].godRay > 0.0f ) {
				emitK.push_back( float( i ) );
				emitK.push_back( L->lights[i].godRay );
				emitters++;
			}
	if ( emitK.empty() ) {
		emitK.push_back( 0.0f );
		emitK.push_back( 0.0f );
	}
	if ( !g.emitBuf ) {
		fn->glGenBuffers( 1, &g.emitBuf );
		fn->glGenTextures( 1, &g.emitTex );
	}
	fn->glBindBuffer( GL_TEXTURE_BUFFER, g.emitBuf );
	fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( emitK.size() * sizeof( float ) ), emitK.data(), GL_DYNAMIC_DRAW );
	fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );

	// red flat: one cube for every froxel, the GI grid's mean per axis
	float flat[6][3] = {};
	const WwCellGi * gi = L ? wwCellGiFor( scene->nifModel ) : nullptr;
	if ( gi && ( s.red & 4 ) ) {
		const size_t per = size_t( gi->dims[0] ) * size_t( gi->dims[1] ) * size_t( gi->dims[2] );
		for ( int a = 0; a < 6; a++ ) {
			double sum[3] = { 0, 0, 0 };
			size_t n = 0;
			for ( size_t v = 0; v < per && ( size_t( a ) * per + v ) * 4 + 3 < gi->rgba.size(); v++ ) {
				const float * p = &gi->rgba[( size_t( a ) * per + v ) * 4];
				if ( p[3] > 0.01f ) {
					for ( int c = 0; c < 3; c++ )
						sum[c] += double( p[c] / p[3] );
					n++;
				}
			}
			for ( int c = 0; c < 3 && n; c++ )
				flat[a][c] = float( sum[c] / double( n ) );
		}
	}

	const auto & pm = r->globalUniforms->projectionMatrix;
	const float p00 = pm[0][0], p11 = pm[1][1];
	const bool csm = wwSunShadowWanted( scene );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	// every sampler on its own unit (the setters above bound theirs)
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kEmitUnit ) );
	fn->glBindTexture( GL_TEXTURE_BUFFER, g.emitTex );
	fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RG32F, g.emitBuf );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kAirUnit ) );
	fn->glBindTexture( GL_TEXTURE_3D, 0 );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kVolTexUnit ) );
	fn->glBindTexture( GL_TEXTURE_3D, 0 );
	prog->uni1i( "volEmit", kEmitUnit );
	prog->uni1i( "volEmitCount", emitters );
	prog->uni1i( "volAir", kAirUnit );
	prog->uni1i( "volInject", kInjectUnit );
	prog->uni1i( "volTex", kVolTexUnit );
	prog->uni1b( "volOn", false );
	prog->uni1b( "volAirOn", false );
	fn->glUniform3i( prog->uniLocation( "volDims" ), cols, rows, kSlices );
	prog->uni2f( "volProj", p00, p11 );
	prog->uni4f( "volSlices", FloatVector4( nearD, logRatio, float( kSlices ), 1.0f ) );
	prog->uni1b( "volCsm", csm );
	prog->uni1i( "volRed", s.red & ~1 );
	prog->uni1i( "volTerms", s.terms );
	for ( int k = 0; k < 3; k++ )
		prog->uni4f_l( prog->uniLocation( "volPhase[%d]", k ), FloatVector4( ph.w[k][0], ph.w[k][1], ph.w[k][2], ph.w[k][3] ) );
	prog->uni4f( "volGi", FloatVector4( ph.gi[0], ph.gi[1], ph.gi[2], ph.gi[3] ) );
	prog->uni1f( "volK", ph.post );
	for ( int a = 0; a < 6; a++ )
		prog->uni3f_l( prog->uniLocation( "volFlat[%d]", a ), flat[a][0], flat[a][1], flat[a][2] );

	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
	fn->glViewport( 0, 0, cols, rows );
	bool complete = true;
	for ( int stage = 1; stage <= 2 && complete; stage++ ) {
		prog->uni1i( "volStage", stage );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kInjectUnit ) );
		fn->glBindTexture( GL_TEXTURE_3D, stage == 2 ? g.inject : 0 );
		for ( int k = 0; k < kSlices; k++ ) {
			fn->glFramebufferTextureLayer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, stage == 1 ? g.inject : g.integ, 0, k );
			if ( k == 0 && fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
				complete = false;
				break;
			}
			prog->uni1i( "volLayer", k );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		}
	}
	r->stopProgram();
	for ( int u : { kInjectUnit, kEmitUnit } ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( u == kEmitUnit ? GL_TEXTURE_BUFFER : GL_TEXTURE_3D, 0 );
	}
	if ( !complete ) {
		s.last = QStringLiteral( "refused(froxel framebuffer incomplete)" );
		restore();
		return;
	}
	g.ready = true;
	g.doc = scene->nifModel;
	g.near = nearD;
	g.logRatio = logRatio;
	double ms = -1.0;
	if ( timed ) {
		fn->glFinish();
		ms = double( timer.nsecsElapsed() ) / 1.0e6;
	}
	s.last = QStringLiteral( "on(%1x%2x%3 near=%4 far=%5 medium=%6 w=%7,%8,%9 g=%10,%11 post=%12 emitters=%13 csm=%14 terms=%15 red=%16 ms=%17)" )
		.arg( cols ).arg( rows ).arg( kSlices ).arg( double( nearD ), 0, 'f', 1 ).arg( double( farD ), 0, 'f', 1 ).arg( medNote )
		.arg( double( ph.gi[0] ), 0, 'f', 4 ).arg( double( ph.gi[1] ), 0, 'f', 4 ).arg( double( ph.gi[2] ), 0, 'f', 4 )
		.arg( double( med[9] ), 0, 'f', 3 ).arg( double( med[10] ), 0, 'f', 3 ).arg( double( ph.post ), 0, 'f', 3 )
		.arg( emitters ).arg( csm ? 1 : 0 ).arg( s.terms ).arg( s.red ).arg( ms, 0, 'f', 3 );

	/* WW_VOLFOG_DUMP=<file>: int32 cols rows slices; the injected and the integrated volumes (cols x rows RGBA float
	 * per layer, layer 0 first, bottom row first); <file>.txt the numbers an independent rebuild needs */
	if ( !s.dump.isEmpty() ) {
		QFile f( s.dump );
		if ( f.open( QIODevice::WriteOnly ) ) {
			const qint32 hd[3] = { cols, rows, kSlices };
			f.write( reinterpret_cast<const char *>( hd ), sizeof( hd ) );
			std::vector<float> layer( size_t( cols ) * size_t( rows ) * 4 );
			fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
			fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.fbo );
			for ( GLuint tex : { g.inject, g.integ } )
				for ( int k = 0; k < kSlices; k++ ) {
					fn->glFramebufferTextureLayer( GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, k );
					fn->glReadPixels( 0, 0, cols, rows, GL_RGBA, GL_FLOAT, layer.data() );
					f.write( reinterpret_cast<const char *>( layer.data() ), qint64( layer.size() * sizeof( float ) ) );
				}
		}
		QFile t( s.dump + QStringLiteral( ".txt" ) );
		if ( t.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			auto v = [&]( const float * a, int n ) { QStringList l; for ( int i = 0; i < n; i++ ) l << QString::number( double( a[i] ), 'g', 9 ); return l.join( ' ' ); };
			QString o;
			o += QStringLiteral( "dims %1 %2 %3\nviewport %4 %5 %6 %7\n" ).arg( cols ).arg( rows ).arg( kSlices ).arg( vp[0] ).arg( vp[1] ).arg( W ).arg( H );
			o += QStringLiteral( "slices %1 %2\nproj %3\n" ).arg( v( &nearD, 1 ), v( &logRatio, 1 ) ).arg( v( std::vector<float>{ p00, p11 }.data(), 2 ) );
			for ( int k = 0; k < 6; k++ )
				o += QStringLiteral( "fogK%1 %2\n" ).arg( k ).arg( v( K[k], 4 ) );
			o += QStringLiteral( "fogView %1\nfogSun %2\nfogSunColour %3\nfogDistScale %4\n" ).arg( v( fogView, 4 ), v( fogSun, 4 ), v( fogSunCol, 4 ), v( &distScale, 1 ) );
			o += QStringLiteral( "medium %1\n" ).arg( v( med, 12 ) );
			for ( int k = 0; k < 3; k++ )
				o += QStringLiteral( "phase%1 %2\n" ).arg( k ).arg( v( ph.w[k], 4 ) );
			o += QStringLiteral( "gi %1\npost %2\n" ).arg( v( ph.gi, 4 ), v( &ph.post, 1 ) );
			o += QStringLiteral( "cellOn %1\ncellInterior %2\ncellHasDir %3\n" ).arg( uniI( "cellOn" ) ).arg( L && L->interior ? 1 : 0 )
				.arg( L && L->hasDirectional ? 1 : 0 );
			for ( int k = 0; k < 3; k++ ) {
				float row[4];
				uniF( "cellRow[%d]", k, row, 4 );
				o += QStringLiteral( "cellRow%1 %2\n" ).arg( k ).arg( v( row, 4 ) );
			}
			if ( L ) {
				o += QStringLiteral( "cellDirColor %1\ncellDirTo %2\n" ).arg( v( L->dirColor, 3 ), v( L->dirTo, 3 ) );
				const Transform & vt = scene->view;
				float rot[9];
				for ( int j = 0; j < 3; j++ )
					for ( int k = 0; k < 3; k++ )
						rot[j * 3 + k] = vt.rotation( j, k );
				o += QStringLiteral( "viewRot %1\nviewT %2\nviewScale %3\n" ).arg( v( rot, 9 ) )
					.arg( v( std::vector<float>{ vt.translation[0], vt.translation[1], vt.translation[2] }.data(), 3 ) ).arg( double( vt.scale ), 0, 'g', 9 );
			}
			o += QStringLiteral( "emitters %1\ncsm %2\nterms %3\nred %4\nmedNote %5\nms %6\n" ).arg( emitters ).arg( csm ? 1 : 0 )
				.arg( s.terms ).arg( s.red ).arg( medNote ).arg( ms, 0, 'f', 3 );
			t.write( o.toUtf8() );
		}
	}
	restore();
}

void wwVolFogUniforms( Scene * scene )
{
	if ( !scene || !scene->renderer )
		return;
	VfState & s = vs();
	if ( s.inPass )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "volOn" ) < 0 )
		return;
	VfGpu & g = vfGpus()[r];
	const bool on = s.on && g.ready && g.doc == scene->nifModel && g.unitsOk > 0;
	prog->uni1b( "volOn", on );
	if ( g.unitsOk <= 0 )
		return;
	// bound whether or not this draw reads it (a sampler left on unit 0 would sit beside BaseMap)
	GLint prevActive = 0;
	r->fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + kVolTexUnit ) );
	r->fn->glBindTexture( GL_TEXTURE_3D, on ? g.integ : 0 );
	r->fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni1i( "volTex", kVolTexUnit );
	if ( !on )
		return;
	GLint vp[4] = { 0, 0, 1, 1 };
	r->fn->glGetIntegerv( GL_VIEWPORT, vp );
	prog->uni4f_l( prog->uniLocation( "volRect" ), FloatVector4( float( vp[0] ), float( vp[1] ),
		1.0f / float( std::max( vp[2], 1 ) ), 1.0f / float( std::max( vp[3], 1 ) ) ) );
	prog->uni4f( "volSlices", FloatVector4( g.near, g.logRatio, float( g.slices ), ( s.red & 1 ) ? 0.0f : 1.0f ) );
	prog->uni1i( "volProbe", s.probe );
}
