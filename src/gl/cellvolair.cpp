/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellvolair.h"

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
#include <cstdio>
#include <vector>

/* ---- lane VOLAIR1: the air (cellvolair.h, res/shaders/cell_volair.frag) ---- */
namespace
{

constexpr int kVolTexUnit = 21;		// the integrated volume the surfaces read (sampler3D); 8, 10..19 are taken
constexpr int kInjectUnit = 22;		// stage 2 reads stage 1
constexpr int kColumnPx = 12;		// a froxel column: 12 x 12 pixels
constexpr int kSlices = 64;
constexpr float kNearMin = 16.0f;	// game units

// the reds (gates only): 1 off (computed, not applied), 2 gioff, 4 flat, 8 noshadow, 16 nolights, 32 wrongmed
struct VaState
{
	bool loaded = false;
	bool on = false, pinned = false;
	int red = 0;
	int terms = 15;
	int probe = 0;
	int fogBlend = 0;	// WW_CELL_VOLAIR_FOGBLEND=1 (option B, test only): the game's fog drawn over the lit air
	QString dump;
	bool inPass = false;
	QString last = QStringLiteral( "off" );
};

VaState & vs()
{
	static VaState s;
	if ( !s.loaded ) {
		s.loaded = true;
		const QByteArray pin = qgetenv( "WW_CELL_AIR" );
		if ( !pin.isEmpty() ) {
			s.pinned = true;
			s.on = pin.trimmed() != "0";
		} else {
			s.on = QSettings().value( QStringLiteral( "WW/VolAir" ), false ).toBool();
		}
		for ( const QByteArray & t : qgetenv( "WW_CELL_AIR_RED" ).split( ',' ) ) {
			const QByteArray r = t.trimmed();
			s.red |= r == "off" ? 1 : r == "gioff" ? 2 : r == "flat" ? 4 : r == "noshadow" ? 8 : r == "nolights" ? 16
				: r == "wrongmed" ? 32 : r == "blendnear" ? 64 : r == "farfit" ? 128 : 0;
		}
		const QByteArray terms = qgetenv( "WW_CELL_AIR_TERMS" ).trimmed();
		if ( !terms.isEmpty() )
			s.terms = terms.toInt() & 15;
		s.probe = qEnvironmentVariableIntValue( "WW_CELL_AIR_PROBE" );
		s.fogBlend = qEnvironmentVariableIntValue( "WW_CELL_VOLAIR_FOGBLEND" ) == 1 ? 1 : 0;
		if ( s.fogBlend && ( s.red & 64 ) )
			s.fogBlend = 2;	// red blendnear: the fog colour by the air's own 1 - T, no start distance (must fail B)
		s.dump = QString::fromLocal8Bit( qgetenv( "WW_CELL_AIR_DUMP" ) );
	}
	return s;
}

struct VaGpu
{
	GLuint inject = 0, integ = 0, fbo = 0;
	int cols = 0, rows = 0, slices = 0;
	const void * doc = nullptr;
	bool ready = false;
	float near = kNearMin, logRatio = 1.0f;
	int unitsOk = -1;
};

QHash<const void *, VaGpu> & vaGpus()
{
	static QHash<const void *, VaGpu> g;
	return g;
}

void vaAlloc3D( NifSkopeOpenGLContext::GLFunctions * fn, GLuint & tex, int w, int h, int d, bool linear )
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

// the phase uniforms from a packed god-ray record (air rgb, fwd rgb, back rgb, fwd g, back g, intensity)
struct VaPhase
{
	float w[3][4] = {};	// rgb weight, w = g
	float gi[4] = {};	// rgb tint (the three weights summed), w = the luminance-weighted g
};

VaPhase vaPhase( const float m[12] )
{
	VaPhase p;
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
	return p;
}

/* the medium fitted to the fog records in force (cellvolair.h): sigma0 (per game unit), zb, 1 / H (0: uniform), the
 * full distance, the far edge of the slices. The red wrongmed: twice the density and no height falloff (the "one
 * exponential" law); the red farfit: the density fitted at the full distance alone (the street-range air too thin). */
struct VaMedium
{
	float sigma0 = 0.0f, zb = 0.0f, invH = 0.0f, dFull = 0.0f, farD = 0.0f;
	bool band = false;
};

// the game's fog alpha at distance d and fog-frame height z (lookdev_fog.glsl wwFogEval, the same formula)
double vaGameAlpha( const float K[6][4], double d, double z )
{
	const double ramp = d * K[0][0] - K[0][2];
	const double f = std::min( std::max( ramp, 0.0 ), 1.0 );
	const double p0 = std::min( std::max( z * K[5][0] - K[5][2], 0.0 ), 1.0 );
	const double p1 = std::min( std::max( z * K[5][1] - K[5][3], 0.0 ), 1.0 );
	const double hb = p0 + ( p1 - p0 ) * f;
	const double mx = K[2][3];
	const double clampT = ramp > 0.75 ? std::min( ( f - 0.75 ) * 4.0 * ( 1.0 - mx ) + mx, 1.0 ) : mx;
	const double escape = ramp < 0.015 ? f * 66.666672 : 1.0;
	const double I = f > 0.0 ? std::min( clampT, std::pow( f, double( K[1][3] ) ) ) : 0.0;
	return ( hb * K[3][3] + ( 1.0 - hb ) ) * I * escape;
}

/* the density: least squares in optical depth against the game's fog over the street range (64 log-spaced distances
 * from 500 to 5000 units, below the height band), sigma0 = sum(d tau) / sum(d^2) with tau = -ln(1 - alpha), never
 * thinner than the far-end fit (1 - exp(-sigma0 dFull) = min(max, 0.99)). The slices end where the air reaches that
 * opacity (tau_max / sigma0, at most dFull), so what lies beyond keeps the game's 1 - max showing through. */
VaMedium vaFit( const float K[6][4], bool wrong, bool farFit )
{
	VaMedium m;
	m.dFull = ( 1.0f + K[0][2] ) / K[0][0];
	const float mx = std::min( std::max( K[2][3], 0.0f ), 0.99f );
	const float tauMax = mx > 0.0f ? -std::log( 1.0f - mx ) : 0.0f;
	const float sFar = m.dFull > 0.0f ? tauMax / m.dFull : 0.0f;
	const float k5y = K[5][1], k5w = K[5][3], top = K[3][3];
	if ( k5y > 1e-9f && top > 0.0f && top < 1.0f ) {
		const float z0 = k5w / k5y, z1 = ( 1.0f + k5w ) / k5y;
		m.zb = z0;
		m.invH = -std::log( std::max( top, 1e-3f ) ) / std::max( z1 - z0, 1e-3f );
		m.band = true;
	}
	double num = 0.0, den = 0.0;
	const double zLow = m.band ? double( m.zb ) - 10.0 : 0.0;
	for ( int i = 0; i < 64; i++ ) {
		const double d = 500.0 * std::pow( 10.0, i / 63.0 );
		const double a = std::min( std::max( vaGameAlpha( K, d, zLow ), 0.0 ), 0.9999 );
		num += d * -std::log( 1.0 - a );
		den += d * d;
	}
	const float sStreet = float( num / den );
	m.sigma0 = farFit ? sFar : std::max( sFar, sStreet );
	m.farD = m.dFull;
	if ( m.sigma0 > sFar && m.sigma0 > 0.0f )
		m.farD = std::min( m.dFull, tauMax / m.sigma0 );
	if ( wrong ) {
		m.sigma0 *= 2.0f;
		m.invH = 0.0f;
		m.band = false;
	}
	return m;
}

}	// namespace

bool wwVolAirOn()
{
	return vs().on;
}

void wwVolAirSetOn( bool on )
{
	VaState & s = vs();
	if ( s.pinned )
		return;
	s.on = on;
	QSettings().setValue( QStringLiteral( "WW/VolAir" ), on );
}

QString wwVolAirSummary()
{
	return QStringLiteral( "volair=" ) + vs().last;
}

// the air's state on stderr whenever it changes (a shot's notes say whether the air was drawn, and why not)
static void vaEcho()
{
	static QString said;
	const VaState & s = vs();
	const QString now = QStringLiteral( "volair=%1 fogblend=%2" ).arg( s.last ).arg( s.fogBlend );
	if ( now != said ) {
		said = now;
		std::fprintf( stderr, "cell air: %s\n", qPrintable( now ) );
	}
}

static void vaPassInner( Scene * scene, bool run );

void wwVolAirPass( Scene * scene, bool run )
{
	vaPassInner( scene, run );
	if ( vs().on && run )
		vaEcho();
}

static void vaPassInner( Scene * scene, bool run )
{
	if ( !scene || !scene->renderer )
		return;
	VaState & s = vs();
	Renderer * r = scene->renderer;
	VaGpu & g = vaGpus()[r];
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
		g.unitsOk = units > kInjectUnit ? 1 : 0;
	}
	if ( !g.unitsOk ) {
		s.last = QStringLiteral( "refused(texture units)" );
		return;
	}
	QElapsedTimer timer;
	const bool timed = !s.dump.isEmpty() || qEnvironmentVariableIsSet( "WW_CELL_AIR_TIME" );
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

	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_volair.prog" );
	if ( !prog ) {
		s.last = QStringLiteral( "refused(cell_volair.prog did not link)" );
		restore();
		return;
	}
	// the setters the surfaces get, on this program: the fog records (weather or, indoors, the cell's), the cascades,
	// the cell's lights, its GI and its ambient
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
		s.last = QStringLiteral( "refused(no fog records: the Fog row, Lookdev's weather or the interior's fog)" );
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
	const VaMedium M = vaFit( K, ( s.red & 32 ) != 0, ( s.red & 128 ) != 0 );
	// the slices: exponential from 16 units to where the air reaches the records' max opacity (vaFit)
	const float farD = std::max( M.farD, kNearMin * 4.0f );
	const float nearD = kNearMin;
	const float logRatio = std::log( farD / nearD );
	// the medium's top: the scene's highest point, in the fog's height frame (world z - the frame's ground)
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	float camZw = 0.0f;
	for ( int j = 0; j < 3; j++ )
		camZw -= vt.rotation( j, 2 ) * vt.translation[j] / sc;
	const BoundSphere bs = scene->bounds();
	const float topW = bs.center[2] + bs.radius;
	const float zTop = topW + fogView[3] - camZw;

	// the phase: indoors the cell's god-ray record, outdoors the weather's, else the fallback
	const WwCellLighting * L = wwCellLightsWanted( scene ) ? wwCellLightsFor( scene->nifModel ) : nullptr;
	float med[12] = { 0.60f, 1.52f, 3.31f, 2, 2, 2, 1, 1, 1, 0.75f, 0, 1 };
	QString medNote = QStringLiteral( "fallback" );
	if ( L && L->interior ) {
		std::copy( L->godRay, L->godRay + 12, med );
		medNote = L->godRayNote;
	} else if ( !wwLookdevGodRays( med, &medNote ) ) {
		medNote = QStringLiteral( "fallback (no weather)" );
	}
	const VaPhase ph = vaPhase( med );

	const int W = std::max( vp[2], 1 ), H = std::max( vp[3], 1 );
	const int cols = ( W + kColumnPx - 1 ) / kColumnPx, rows = ( H + kColumnPx - 1 ) / kColumnPx;
	if ( g.cols != cols || g.rows != rows || g.slices != kSlices || !g.inject ) {
		vaAlloc3D( fn, g.inject, cols, rows, kSlices, false );
		vaAlloc3D( fn, g.integ, cols, rows, kSlices, true );
		if ( !g.fbo )
			fn->glGenFramebuffers( 1, &g.fbo );
		g.cols = cols;
		g.rows = rows;
		g.slices = kSlices;
	}

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
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kVolTexUnit ) );
	fn->glBindTexture( GL_TEXTURE_3D, 0 );
	prog->uni1i( "volInject", kInjectUnit );
	prog->uni1i( "volTex", kVolTexUnit );
	prog->uni1b( "volOn", false );
	fn->glUniform3i( prog->uniLocation( "volDims" ), cols, rows, kSlices );
	prog->uni2f( "volProj", p00, p11 );
	prog->uni4f( "volSlices", FloatVector4( nearD, logRatio, float( kSlices ), 1.0f ) );
	prog->uni4f( "airMed", FloatVector4( M.sigma0, M.zb, M.invH, zTop ) );
	prog->uni1b( "volCsm", csm );
	prog->uni1i( "volRed", s.red & ~1 );
	prog->uni1i( "volTerms", s.terms );
	for ( int k = 0; k < 3; k++ )
		prog->uni4f_l( prog->uniLocation( "volPhase[%d]", k ), FloatVector4( ph.w[k][0], ph.w[k][1], ph.w[k][2], ph.w[k][3] ) );
	prog->uni4f( "volGi", FloatVector4( ph.gi[0], ph.gi[1], ph.gi[2], ph.gi[3] ) );
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
	// the numbers the dump echoes, read before the program stops
	float cellRowV[3][4], cellDirColV[3] = { 0, 0, 0 }, cellDirToV[3] = { 0, 0, 0 };
	for ( int k = 0; k < 3; k++ )
		uniF( "cellRow[%d]", k, cellRowV[k], 4 );
	uniF( "cellDirColor", -1, cellDirColV, 3 );
	uniF( "cellDirTo", -1, cellDirToV, 3 );
	const int cellOnV = uniI( "cellOn" ), cellHasDirV = uniI( "cellHasDir" ), cellLightCountV = uniI( "cellLightCount" );
	const int cellGiOnV = uniI( "cellGiOn" ), cellHasDalcV = uniI( "cellHasDalc" ), cellExtDalcV = uniI( "cellExtDalc" );
	r->stopProgram();
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kInjectUnit ) );
	fn->glBindTexture( GL_TEXTURE_3D, 0 );
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
	s.last = QStringLiteral( "on(%1x%2x%3 near=%4 far=%5 sigma0=%6 zb=%7 H=%8 top=%9 phase=%10 tint=%11,%12,%13 lights=%14 "
		"gi=%15 csm=%16 terms=%17 red=%18 ms=%19)" )
		.arg( cols ).arg( rows ).arg( kSlices ).arg( double( nearD ), 0, 'f', 1 ).arg( double( farD ), 0, 'f', 1 )
		.arg( double( M.sigma0 ), 0, 'g', 6 ).arg( double( M.zb ), 0, 'f', 1 )
		.arg( M.invH > 0.0f ? QString::number( double( 1.0f / M.invH ), 'f', 1 ) : QStringLiteral( "uniform" ) )
		.arg( double( zTop ), 0, 'f', 1 ).arg( medNote )
		.arg( double( ph.gi[0] ), 0, 'f', 4 ).arg( double( ph.gi[1] ), 0, 'f', 4 ).arg( double( ph.gi[2] ), 0, 'f', 4 )
		.arg( cellLightCountV ).arg( cellGiOnV ).arg( csm ? 1 : 0 ).arg( s.terms ).arg( s.red ).arg( ms, 0, 'f', 3 );

	/* WW_CELL_AIR_DUMP=<file>: int32 cols rows slices; the injected and the integrated volumes (cols x rows RGBA float
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
			const float airV[4] = { M.sigma0, M.zb, M.invH, zTop };
			const float sl[2] = { nearD, logRatio }, pr[2] = { p00, p11 };
			QString o;
			o += QStringLiteral( "dims %1 %2 %3\nviewport %4 %5 %6 %7\n" ).arg( cols ).arg( rows ).arg( kSlices ).arg( vp[0] ).arg( vp[1] ).arg( W ).arg( H );
			o += QStringLiteral( "slices %1\nproj %2\n" ).arg( v( sl, 2 ), v( pr, 2 ) );
			for ( int k = 0; k < 6; k++ )
				o += QStringLiteral( "fogK%1 %2\n" ).arg( k ).arg( v( K[k], 4 ) );
			o += QStringLiteral( "fogView %1\nfogSun %2\nfogSunColour %3\nfogDistScale %4\n" ).arg( v( fogView, 4 ), v( fogSun, 4 ), v( fogSunCol, 4 ), v( &distScale, 1 ) );
			o += QStringLiteral( "airMed %1\nband %2\nsceneTopW %3\ncamZw %4\n" ).arg( v( airV, 4 ) ).arg( M.band ? 1 : 0 )
				.arg( double( topW ), 0, 'g', 9 ).arg( double( camZw ), 0, 'g', 9 );
			o += QStringLiteral( "medium %1\n" ).arg( v( med, 12 ) );
			for ( int k = 0; k < 3; k++ )
				o += QStringLiteral( "phase%1 %2\n" ).arg( k ).arg( v( ph.w[k], 4 ) );
			o += QStringLiteral( "gi %1\n" ).arg( v( ph.gi, 4 ) );
			o += QStringLiteral( "cellOn %1\ncellInterior %2\ncellHasDir %3\ncellLightCount %4\ncellGiOn %5\ncellHasDalc %6\ncellExtDalc %7\n" )
				.arg( cellOnV ).arg( L && L->interior ? 1 : 0 ).arg( cellHasDirV ).arg( cellLightCountV ).arg( cellGiOnV )
				.arg( cellHasDalcV ).arg( cellExtDalcV );
			for ( int k = 0; k < 3; k++ )
				o += QStringLiteral( "cellRow%1 %2\n" ).arg( k ).arg( v( cellRowV[k], 4 ) );
			o += QStringLiteral( "cellDirColor %1\ncellDirTo %2\n" ).arg( v( cellDirColV, 3 ), v( cellDirToV, 3 ) );
			o += QStringLiteral( "csm %1\nterms %2\nred %3\nmedNote %4\nms %5\n" ).arg( csm ? 1 : 0 )
				.arg( s.terms ).arg( s.red ).arg( medNote ).arg( ms, 0, 'f', 3 );
			t.write( o.toUtf8() );
		}
	}
	restore();
}

void wwVolAirUniforms( Scene * scene )
{
	if ( !scene || !scene->renderer )
		return;
	VaState & s = vs();
	if ( s.inPass )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "volOn" ) < 0 )
		return;
	VaGpu & g = vaGpus()[r];
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
	prog->uni1i( "volFogBlend", s.fogBlend );
}
