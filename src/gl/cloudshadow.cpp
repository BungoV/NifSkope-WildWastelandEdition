#include "gl/cloudshadow.h"
#include "gl/cloudshadowmath.h"
#include "gl/cloudmap.h"
#include "gl/cloudvol.h"
#include "gl/atmomath.h"
#include "gl/cellunits.h"
#include "gl/celluniform.h"	// lane CELLUNI2
#include "gl/celllights.h"
#include "gl/glscene.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"
#include "harnesswindow.h"
#include "wwswitch.h"

#include <QHash>
#include <QSettings>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

// lane CLOUD3: the clouds' Beer shadow map (cloudshadow.h). The build is res/shaders/cloud_shadowmap.frag.

namespace
{

namespace M = WwCloudShadowMath;
constexpr const char * kKey = "Settings/Render/Scene/Cloud Shadows";
constexpr GLenum kTimeElapsed = 0x88BF;	// GL_TIME_ELAPSED
constexpr double kSigma = 40.0;	// cloud_common.glsl kCvSigma
constexpr int kAmortize = 16;	// a refresh rebuilds size/16 rows a frame (a full rebuild when the fit moves)
constexpr int kGiEvery = 30;	// frames between the probe GI's readbacks

struct Preset
{
	int size, steps;
	bool tail;	// the far air's cloud taps (the aerial perspective)
};

// Low 512 from the map; Medium 512 / 12 steps; High 1024 / 16; Ultra 1024 / 16 + the air tail (BRIEF_CLOUD3)
const Preset & preset( int tier )
{
	static const Preset p[5] = { { 512, 12, false }, { 512, 0, false }, { 512, 12, false }, { 1024, 16, false },
		{ 1024, 16, true } };
	return p[std::clamp( tier, 0, 4 )];
}

const char * tierName( int t )
{
	static const char * n[5] = { "off", "low", "medium", "high", "ultra" };
	return n[std::clamp( t, 0, 4 )];
}

struct CsState
{
	bool init = false;
	quint64 liveGen = 0;
	int tier = 0;
	bool pinned = false;
	QString last;
	std::atomic<float> gi{ 1.0f };
};

CsState & cs()
{
	static CsState s;
	if ( !s.init || wwSwitchMovedFor( s.liveGen, { "WW_CLOUDSHADOW" } ) ) {
		s.init = true;
		s.liveGen = wwSwitchGen();
		s.pinned = false;
		const QString e = wwEnvStr( "WW_CLOUDSHADOW" ).trimmed().toLower();
		if ( !e.isEmpty() ) {
			bool ok = false;
			const int v = e.toInt( &ok );
			s.tier = ok ? std::clamp( v, 0, 4 )
				: e == QLatin1StringView( "low" ) ? 1 : e == QLatin1StringView( "medium" ) ? 2
				: e == QLatin1StringView( "high" ) ? 3 : e == QLatin1StringView( "ultra" ) ? 4 : 0;
			s.pinned = true;
		} else if ( wwHarnessRun() ) {
			s.tier = 0;	// a harness measures what it pins
			s.pinned = true;
		} else {
			s.tier = std::clamp( QSettings().value( QLatin1StringView( kKey ), 0 ).toInt(), 0, 4 );
		}
	}
	return s;
}

bool red( const char * word )
{
	const QString r = wwEnvStr( "WW_CLOUDSHADOW_RED" ).trimmed().toLower();
	for ( const QString & w : r.split( QChar( ',' ), Qt::SkipEmptyParts ) )
		if ( w.trimmed() == QLatin1StringView( word ) )
			return true;
	return false;
}

struct CsGpu
{
	GLuint fbo = 0, tex = 0;
	int size = 0;
	bool valid = false;	// every row built under the fit below
	int mode = 0;	// 1 volume, 2 map
	bool redVertical = false;
	bool variety = false;	// lane CLOUD5: built over the Cloud Variety density
	bool shape = false;	// lane CLOUDSHAPE1: built over the Cloud Shapes density
	M::Basis basis{};
	M::Fit fit{};
	double lightFromWorld[16] = {};
	int cursor = 0;	// the next refresh row
	quint64 builds = 0;
	float far[3] = { 1, 0.1f, 0 };
	// timing
	GLuint q[2] = { 0, 0 };
	bool pending[2] = { false, false };
	int qcur = 0;
	bool qFull[2] = { false, false };
	double msRows = -1.0, msFull = -1.0;
	quint64 timed = 0;
	// the probe GI's readback
	GLuint pbo = 0;
	GLsync fence = nullptr;
	quint64 fenceBuild = 0;
	int fenceN = 0;
	double fenceZKm = 0.0;
	quint64 frame = 0;
};

QHash<Renderer *, CsGpu> & gpus()
{
	static QHash<Renderer *, CsGpu> g;
	return g;
}

Renderer * g_last = nullptr;	// the renderer of the last pass (wwCloudShadowValid)

bool ensure( NifSkopeOpenGLContext::GLFunctions * fn, CsGpu & g, int n )
{
	if ( g.fbo && g.size == n )
		return true;
	if ( !g.fbo )
		fn->glGenFramebuffers( 1, &g.fbo );
	if ( g.tex )
		fn->glDeleteTextures( 1, &g.tex );
	fn->glGenTextures( 1, &g.tex );
	fn->glBindTexture( GL_TEXTURE_2D, g.tex );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, n, n, 0, GL_RGBA, GL_FLOAT, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.tex, 0 );
	const GLenum one = GL_COLOR_ATTACHMENT0;
	fn->glDrawBuffers( 1, &one );
	g.size = n;
	g.valid = false;
	return fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
}

void quad( Renderer * r )
{
	static const float q[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = q;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
}

//! world (game units) from view: Q = R^T (p - t) / sc, as shadowset.cpp's ussWorldFromView
void worldFromView( const Transform & vt, double m[16] )
{
	const double sc = vt.scale != 0.0f ? double( vt.scale ) : 1.0;
	for ( int i = 0; i < 3; i++ ) {
		double k = 0.0;
		for ( int j = 0; j < 3; j++ ) {
			m[j * 4 + i] = double( vt.rotation( j, i ) ) / sc;
			k -= double( vt.rotation( j, i ) ) * double( vt.translation[j] );
		}
		m[12 + i] = k / sc;
	}
	m[3] = m[7] = m[11] = 0.0;
	m[15] = 1.0;
}

//! the map mode's cloud_common.glsl band uniforms (cloudvol.cpp bandsOf, the same numbers); windKm 0: the map
//! mode reads coverage only (the wind moves only the noise). Mode 1 gets the drawn wind from wwCloudVolBindDensity.
void mapUniforms( Renderer * r, NifSkopeOpenGLContext::Program * p, const WwCloudMap & m )
{
	auto fn = r->fn;
	const float u = WwAtmoMath::kUnitKm;
	float base[3], top[3], dens[3];
	for ( int i = 0; i < 3; i++ ) {
		base[i] = m.band[i].baseZ * u;
		top[i] = std::max( m.band[i].topZ * u, base[i] + 1e-3f );
		dens[i] = m.band[i].layers > 0 ? m.band[i].density : 0.0f;
	}
	p->uni3f( "cvBands.baseKm", base[0], base[1], base[2] );
	p->uni3f( "cvBands.topKm", top[0], top[1], top[2] );
	p->uni3f( "cvBands.density", dens[0], dens[1], dens[2] );
	p->uni3f( "cvBands.coverage", m.band[0].coverage, m.band[1].coverage, m.band[2].coverage );
	p->uni1f( "cvBands.sheet", m.band[0].sheet );
	const float he = std::max( m.halfExtent * u, 1e-3f );
	const bool bound = m.tex != 0 && !m.stub;
	p->uni4f( "cvBands.mapRect", FloatVector4( m.centerX * u - he, m.centerY * u - he, 1.0f / ( 2.0f * he ), bound ? 1.0f : 0.0f ) );
	p->uni3f( "cvBands.windKm", 0.0f, 0.0f, 0.0f );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::CloudBuildMap ) );
	fn->glBindTexture( GL_TEXTURE_2D, bound ? m.tex : 0 );
	p->uni1i( "cvMap", WwCellUnit::CloudBuildMap );
	// the 3D samplers are unused here, but a sampler left on unit 0 beside another type fails the draw's validation
	p->uni1i( "cvShape", WwCellUnit::CloudBuildShape );
	p->uni1i( "cvDetail", WwCellUnit::CloudBuildDetail );
}

void timeRead( NifSkopeOpenGLContext::GLFunctions * fn, CsGpu & g, int slot )
{
	if ( !g.pending[slot] )
		return;
	GLuint avail = 0;
	fn->glGetQueryObjectuiv( g.q[slot], GL_QUERY_RESULT_AVAILABLE, &avail );
	if ( !avail )
		return;
	GLuint64 ns = 0;
	fn->glGetQueryObjectui64v( g.q[slot], GL_QUERY_RESULT, &ns );
	( g.qFull[slot] ? g.msFull : g.msRows ) = double( ns ) * 1e-6;
	g.pending[slot] = false;
	if ( ++g.timed % 120 == 1 )
		std::fprintf( stderr, "cloudshadow: gpu rows %.4f ms full %.4f ms (size %d)\n", g.msRows, g.msFull, g.size );
}

//! the probe GI's factor: the mean receiver transmittance over a 5x5 block at the eye, from the last readback
void giRead( NifSkopeOpenGLContext::GLFunctions * fn, CsGpu & g, CsState & s )
{
	if ( !g.fence )
		return;
	const GLenum w = fn->glClientWaitSync( g.fence, 0, 0 );
	if ( w != GL_ALREADY_SIGNALED && w != GL_CONDITION_SATISFIED )
		return;
	fn->glDeleteSync( g.fence );
	g.fence = nullptr;
	if ( g.fenceBuild != g.builds || !g.valid || g.fenceN <= 0 )
		return;	// the map moved since: the next readback
	fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, g.pbo );
	const auto * px = static_cast<const float *>( fn->glMapBufferRange( GL_PIXEL_PACK_BUFFER, 0,
		GLsizeiptr( g.fenceN * 4 * sizeof( float ) ), GL_MAP_READ_BIT ) );
	if ( px ) {
		double sum = 0.0;
		for ( int i = 0; i < g.fenceN; i++ )
			sum += M::receiverT( px + i * 4, g.fenceZKm );
		fn->glUnmapBuffer( GL_PIXEL_PACK_BUFFER );
		const double T = sum / g.fenceN;
		const float pub = s.gi.load();
		// 1/32 steps with hysteresis: every change relights the probes (probesky.cpp), so small drifts must not
		if ( std::fabs( T - double( pub ) ) > 1.5 / 32.0 )
			s.gi.store( float( std::clamp( std::round( T * 32.0 ) / 32.0, 0.0, 1.0 ) ) );
	}
	fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
}

void giIssue( NifSkopeOpenGLContext::GLFunctions * fn, CsGpu & g, const double eyeW[3] )
{
	const double * L = g.lightFromWorld;
	const double u = L[0] * eyeW[0] + L[4] * eyeW[1] + L[8] * eyeW[2] + L[12];
	const double v = L[1] * eyeW[0] + L[5] * eyeW[1] + L[9] * eyeW[2] + L[13];
	if ( u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0 )
		return;
	const int n = g.size;
	const int x0 = std::clamp( int( u * n ) - 2, 0, n - 5 ), y0 = std::clamp( int( v * n ) - 2, 0, n - 5 );
	if ( !g.pbo ) {
		fn->glGenBuffers( 1, &g.pbo );
		fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, g.pbo );
		fn->glBufferData( GL_PIXEL_PACK_BUFFER, GLsizeiptr( 25 * 4 * sizeof( float ) ), nullptr, GL_STREAM_READ );
	}
	fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, g.pbo );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.fbo );
	fn->glReadBuffer( GL_COLOR_ATTACHMENT0 );
	fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
	fn->glReadPixels( x0, y0, 5, 5, GL_RGBA, GL_FLOAT, nullptr );
	fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
	g.fence = fn->glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
	g.fenceBuild = g.builds;
	g.fenceN = 25;
	g.fenceZKm = eyeW[2] * double( WwAtmoMath::kUnitKm );
}

void status( CsState & s, const QString & line )
{
	if ( line == s.last )
		return;
	s.last = line;
	std::fprintf( stderr, "cloudshadow: %s\n", qPrintable( line ) );
}

} // namespace

int wwCloudShadowTier()
{
	return cs().tier;
}

void wwCloudShadowSetTier( int tier )
{
	CsState & s = cs();
	if ( s.pinned )
		return;
	s.tier = std::clamp( tier, 0, 4 );
	QSettings().setValue( QLatin1StringView( kKey ), s.tier );
}

bool wwCloudShadowPinned()
{
	return cs().pinned;
}

bool wwCloudShadowValid()
{
	if ( cs().tier <= 0 || !g_last )
		return false;
	auto it = gpus().constFind( g_last );
	return it != gpus().constEnd() && it->valid;
}

float wwCloudShadowGiFactor()
{
	CsState & s = cs();
	if ( s.tier <= 0 || red( "nogi" ) )
		return 1.0f;
	return s.gi.load();
}

QString wwCloudShadowSummary()
{
	return cs().last;
}

void wwCloudShadowPass( Scene * scene )
{
	CsState & s = cs();
	if ( !scene || !scene->renderer || scene->selecting )
		return;
	Renderer * r = scene->renderer;
	g_last = r;
	CsGpu & g = gpus()[r];
	auto off = [&]( const QString & why ) {
		g.valid = false;
		s.gi.store( 1.0f );
		status( s, why );
	};
	if ( s.tier <= 0 )
		return off( QStringLiteral( "off" ) );
	if ( !wwLookdevActive() )
		return off( QStringLiteral( "refused(no weather-lit view)" ) );
	const WwCellLighting * cl = wwCellLightsFor( scene->nifModel );
	if ( cl && cl->interior )
		return off( QStringLiteral( "refused(interior)" ) );
	float sunTo[3], disc[3];
	wwLookdevShadowLight( sunTo, disc );
	if ( disc[2] <= 0.0f )
		return off( QStringLiteral( "refused(the light is under the horizon)" ) );
	const WwCloudMap & m = wwCloudMapUpdate( scene );
	if ( !m.valid )
		return off( QStringLiteral( "refused(needs the Cloud Map row)" ) );
	auto fn = r->fn;
	static GLint units = 0;
	if ( !units )
		fn->glGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &units );
	if ( units <= WwCellUnit::CloudBuildMap )
		return off( QStringLiteral( "refused(too few texture units)" ) );
	const Preset & pr = preset( s.tier );
	const bool redV = red( "vertical" ), every = red( "everyframe" );
	g.frame++;

	// the fit: the sun and the eye's line; it moves only on a snap step or a 0.25 deg sun turn
	const M::Basis b = M::basis( sunTo, redV );
	double wfv[16];
	worldFromView( scene->view, wfv );
	const double eyeW[3] = { wfv[12], wfv[13], wfv[14] };
	const double unitKm = double( WwAtmoMath::kUnitKm );
	const double eyeKm[3] = { eyeW[0] * unitKm, eyeW[1] * unitKm, eyeW[2] * unitKm };
	const double extKm = m.halfExtent > 0.0f ? 2.0 * double( m.halfExtent ) * unitKm : 48.0;
	const M::Fit f = M::fit( eyeKm, extKm, pr.size, b );

	GLint prevDraw = 0, prevRead = 0, prevActive = 0, prevTex = 0, vp[4], sb[4];
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_SCISSOR_BOX, sb );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasBlend = fn->glIsEnabled( GL_BLEND );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	const bool wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	GLboolean depthMask = GL_TRUE, cmask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, cmask );
	auto restore = [&]() {
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
		fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
		fn->glScissor( sb[0], sb[1], sb[2], sb[3] );
		for ( int u : { int( WwCellUnit::CloudBuildMap ), int( WwCellUnit::CloudBuildDetail ), int( WwCellUnit::CloudBuildShape ) } ) {
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
			fn->glBindTexture( GL_TEXTURE_2D, 0 );
			fn->glBindTexture( GL_TEXTURE_3D, 0 );
		}
		fn->glActiveTexture( GLenum( prevActive ) );
		( wasDepth ? fn->glEnable( GL_DEPTH_TEST ) : fn->glDisable( GL_DEPTH_TEST ) );
		( wasBlend ? fn->glEnable( GL_BLEND ) : fn->glDisable( GL_BLEND ) );
		( wasCull ? fn->glEnable( GL_CULL_FACE ) : fn->glDisable( GL_CULL_FACE ) );
		( wasScissor ? fn->glEnable( GL_SCISSOR_TEST ) : fn->glDisable( GL_SCISSOR_TEST ) );
		( wasStencil ? fn->glEnable( GL_STENCIL_TEST ) : fn->glDisable( GL_STENCIL_TEST ) );
		fn->glDepthMask( depthMask );
		fn->glColorMask( cmask[0], cmask[1], cmask[2], cmask[3] );
	};
	fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prevTex );
	const bool fboOk = ensure( fn, g, pr.size );
	fn->glBindTexture( GL_TEXTURE_2D, GLuint( prevTex ) );
	if ( !fboOk ) {
		restore();
		return off( QStringLiteral( "refused(the map's target is incomplete)" ) );
	}
	if ( !g.q[0] )
		fn->glGenQueries( 2, g.q );
	timeRead( fn, g, 0 );
	timeRead( fn, g, 1 );
	giRead( fn, g, s );

	// lane CLOUD5: Cloud Variety on, the twin over the variety density (wwCloudVolBindDensity sets its uniforms)
	// lane CLOUDSHAPE1: Cloud Shapes on, its twin (it supersedes Variety's density; wwCloudVolBindDensity binds cvCell)
	const bool shape = wwCloudVolShapeOn();
	const bool variety = !shape && wwCloudVolVarietyOn() && m.variety;
	NifSkopeOpenGLContext::Program * p = r->useProgram( shape ? "cloud_shadowmap_s.prog"
														   : variety ? "cloud_shadowmap_v.prog" : "cloud_shadowmap.prog" );
	if ( !p ) {
		restore();
		return off( shape ? QStringLiteral( "refused(cloud_shadowmap_s.prog did not link)" )
			: variety ? QStringLiteral( "refused(cloud_shadowmap_v.prog did not link)" )
					  : QStringLiteral( "refused(cloud_shadowmap.prog did not link)" ) );
	}
	// the volume's density at Medium and up (it needs the Volume Clouds row at Medium or up), else the map's coverage
	int mode = 2;
	if ( pr.steps > 0 && wwCloudVolBindDensity( scene, WwCellUnit::CloudBuildShape ) )
		mode = 1;
	else
		mapUniforms( r, p, m );
	const bool full = every || !g.valid || g.mode != mode || g.redVertical != redV || f.a0 != g.fit.a0 || f.b0 != g.fit.b0
		|| f.extentKm != g.fit.extentKm || M::angleDeg( b.s, g.basis.s ) > 0.25 || g.variety != variety || g.shape != shape;
	if ( full ) {
		g.basis = b;
		g.fit = f;
		g.mode = mode;
		g.redVertical = redV;
		g.variety = variety;
		g.shape = shape;
		g.cursor = 0;
		M::lightFromWorld( b, f, unitKm, g.lightFromWorld );
	}
	const int n = g.size;
	const int rows = full ? n : std::max( 1, n / kAmortize );
	const int row0 = full ? 0 : g.cursor;
	const int rowN = std::min( rows, n - row0 );

	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.fbo );
	fn->glViewport( 0, 0, n, n );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glEnable( GL_SCISSOR_TEST );
	fn->glScissor( 0, row0, n, rowN );
	const M::Basis & cb = g.basis;
	p->uni3f( "csE1", float( cb.e1[0] ), float( cb.e1[1] ), float( cb.e1[2] ) );
	p->uni3f( "csE2", float( cb.e2[0] ), float( cb.e2[1] ), float( cb.e2[2] ) );
	p->uni3f( "csS", float( cb.s[0] ), float( cb.s[1] ), float( cb.s[2] ) );
	p->uni4f( "csFit", FloatVector4( float( g.fit.a0 ), float( g.fit.b0 ), float( g.fit.extentKm ), float( g.fit.texelKm() ) ) );
	p->uni2f( "csSize", float( n ), float( n ) );
	p->uni1i( "csMode", mode );
	p->uni1i( "csSteps", std::max( pr.steps, 1 ) );
	const int slot = g.qcur;
	const bool timing = !g.pending[slot];
	if ( timing )
		fn->glBeginQuery( kTimeElapsed, g.q[slot] );
	quad( r );
	if ( timing ) {
		fn->glEndQuery( kTimeElapsed );
		g.pending[slot] = true;
		g.qFull[slot] = full;
		g.qcur = 1 - g.qcur;
	}
	r->stopProgram();
	g.cursor = full ? 0 : ( row0 + rowN >= n ? 0 : row0 + rowN );
	g.valid = true;
	if ( full )
		g.builds++;
	if ( !red( "nogi" ) && !g.fence && g.frame % kGiEvery == 0 )
		giIssue( fn, g, eyeW );
	restore();

	// the far value: the bands' statistical transmittance at this sun; the edge fade; Ultra's air taps
	const float cov[2] = { m.band[0].coverage, m.band[1].coverage };
	const float thick[2] = { std::max( m.band[0].topZ - m.band[0].baseZ, 0.0f ) * WwAtmoMath::kUnitKm,
		std::max( m.band[1].topZ - m.band[1].baseZ, 0.0f ) * WwAtmoMath::kUnitKm };
	const float dens[2] = { m.band[0].layers > 0 ? m.band[0].density : 0.0f, m.band[1].layers > 0 ? m.band[1].density : 0.0f };
	g.far[0] = float( M::bandFarT( cov, thick, dens, cb.s[2], kSigma ) );
	g.far[1] = 0.1f;
	g.far[2] = pr.tail ? 1.0f : 0.0f;
	status( s, QStringLiteral( "on tier=%1 mode=%2 size=%3 side_km=%4 texel_m=%5 sun=%6,%7,%8 far_t=%9 gi=%10 mem_mb=%11%12" )
		.arg( QLatin1StringView( tierName( s.tier ) ) ).arg( mode == 1 ? QStringLiteral( "volume" ) : QStringLiteral( "map" ) )
		.arg( n ).arg( g.fit.extentKm, 0, 'f', 1 ).arg( g.fit.texelKm() * 1000.0, 0, 'f', 1 )
		.arg( cb.s[0], 0, 'f', 3 ).arg( cb.s[1], 0, 'f', 3 ).arg( cb.s[2], 0, 'f', 3 ).arg( double( g.far[0] ), 0, 'f', 3 )
		.arg( double( s.gi.load() ), 0, 'f', 4 ).arg( double( n ) * n * 8.0 / ( 1024.0 * 1024.0 ), 0, 'f', 1 )
		.arg( redV ? QStringLiteral( " red=vertical" ) : QString() ) );
}

void wwCloudShadowUniforms( Scene * scene, NifSkopeOpenGLContext::Program * prog, bool volume, float airAmb )
{
	if ( !scene || !scene->renderer || !prog || prog->uniLocation( "ussCloudOn" ) < 0 )
		return;
	Renderer * r = scene->renderer;
	auto it = gpus().find( r );
	const bool valid = cs().tier > 0 && it != gpus().end() && it->valid;
	// red noinject keeps the clouds out of the fog's air (the shafts go); surfaces still take them
	const bool on = valid && !( volume && red( "noinject" ) );
	prog->uni1i( "ussCloudOn", on ? 1 : 0 );
	auto fn = r->fn;
	WwCellUni::Units<std::remove_pointer_t<decltype( fn )>> u( fn, wwCellUniDsa() );	// lane CELLUNI2: no query with DSA
	u.bind( WwCellUnit::CloudShadow, GL_TEXTURE_2D, on ? it->tex : 0 );
	u.done();
	prog->uni1i( "ussCloudMap", WwCellUnit::CloudShadow );
	if ( !on )
		return;
	double wfv[16], cfv[16];
	worldFromView( scene->view, wfv );
	M::mul( it->lightFromWorld, wfv, cfv );
	float mf[16];
	for ( int i = 0; i < 16; i++ )
		mf[i] = float( cfv[i] );
	const int l = prog->uniLocation( "ussCloudFromView" );
	if ( l >= 0 ) {
		fn->glUniformMatrix4fv( l, 1, GL_FALSE, mf );
		WwCellUni::wrote( prog->wwUni, l, 1 );	// lane CELLUNI2: a raw write
	}
	prog->uni4f( "ussCloudFar", FloatVector4( it->far[0], it->far[1], it->far[2], airAmb ) );
}
