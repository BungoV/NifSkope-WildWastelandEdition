#include "cellocean.h"
#include "wwswitch.h"

#include "cellunits.h"
#include "celllooks.h"
#include "cellwater.h"
#include "cellworldanim.h"
#include "oceanspectrum.h"
#include "gl/glscene.h"
#include "gl/renderer.h"

#include <QDebug>
#include <QHash>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace
{

constexpr GLenum kTimestamp = 0x8E28;	// GL_TIMESTAMP
constexpr int kW = WwOcean::kN * WwOcean::kTiles;	// the spectrum's six tiles side by side
constexpr int kLevels = 8;	// 128 .. 1

struct OceanGpu
{
	GLuint h0 = 0, q = 0, ping[2][2] = {}, fb[2] = {}, arrB = 0, arrC = 0, arrD = 0, fbArr = 0, qry[4] = {};
	bool ok = false, pending[2] = {};
	int cur = 0;
	double U = -1.0, swell = -1.0;	// the wind the uploaded h0 is for
};

struct OceanState
{
	WwOcean::SetParams P[2];
	double tail[2] = {}, curv[2] = {};
	double U = 5.0, swell = 0.344;
	bool built = false;	// this frame
	int red = 0;		// 1 old, 2 tile, 4 nopatch, 8 novar, 16 nochop, 32 flat (step 2: no displacement)
	double ms = 0.0;
	int frames = 0;
	QString why;
};

OceanState & os()
{
	static OceanState s;
	return s;
}

QHash<Renderer *, OceanGpu> & gpus()
{
	static QHash<Renderer *, OceanGpu> g;
	return g;
}

int redNow()
{
	WW_SWITCH_STATIC( QByteArray, rr, wwEnv( "WW_CELL_WATER_OCEAN_RED" ).trimmed() );
	return rr == "old" ? 1 : rr == "tile" ? 2 : rr == "nopatch" ? 4 : rr == "novar" ? 8 : rr == "nochop" ? 16
		: rr == "flat" ? 32 : 0;
}

GLuint tex2d( NifSkopeOpenGLContext::GLFunctions * fn, GLint ifmt, int w, int h, GLenum fmt, const float * data )
{
	GLuint t = 0;
	fn->glGenTextures( 1, &t );
	fn->glBindTexture( GL_TEXTURE_2D, t );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, GL_FLOAT, data );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	return t;
}

GLuint texArr( NifSkopeOpenGLContext::GLFunctions * fn, GLint ifmt )
{
	GLuint t = 0;
	fn->glGenTextures( 1, &t );
	fn->glBindTexture( GL_TEXTURE_2D_ARRAY, t );
	for ( int lv = 0; lv < kLevels; lv++ )
		fn->glTexImage3D( GL_TEXTURE_2D_ARRAY, lv, ifmt, WwOcean::kN >> lv, WwOcean::kN >> lv, WwOcean::kTiles, 0, GL_RGBA,
			GL_FLOAT, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT );
	fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT );
	fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0 );
	fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, kLevels - 1 );
	GLfloat maxAniso = 1.0f;
	fn->glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso );
	fn->glTexParameterf( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min( 16.0f, maxAniso ) );
	return t;
}

bool create( NifSkopeOpenGLContext::GLFunctions * fn, OceanGpu & g )
{
	g.h0 = tex2d( fn, GL_RGBA32F, kW, WwOcean::kN, GL_RGBA, nullptr );
	g.q = tex2d( fn, GL_R32F, kW, WwOcean::kN, GL_RED, nullptr );
	fn->glGenFramebuffers( 2, g.fb );
	fn->glGenFramebuffers( 1, &g.fbArr );
	const GLenum bufs[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	bool ok = true;
	for ( int i = 0; i < 2; i++ ) {
		g.ping[i][0] = tex2d( fn, GL_RGBA32F, kW, WwOcean::kN, GL_RGBA, nullptr );
		g.ping[i][1] = tex2d( fn, GL_RGBA32F, kW, WwOcean::kN, GL_RGBA, nullptr );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fb[i] );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.ping[i][0], 0 );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g.ping[i][1], 0 );
		fn->glDrawBuffers( 2, bufs );
		ok = ok && fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
	}
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	g.arrB = texArr( fn, GL_RGBA16F );
	g.arrC = texArr( fn, GL_RGBA32F );
	g.arrD = texArr( fn, GL_RGBA16F );	// step 2: the heights and the chop for the displaced water
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbArr );
	fn->glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, g.arrB, 0 );
	fn->glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, g.arrC, 0 );
	fn->glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, g.arrD, 0 );
	fn->glDrawBuffers( 3, bufs );
	ok = ok && fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
	fn->glBindTexture( GL_TEXTURE_2D_ARRAY, 0 );
	fn->glGenQueries( 4, g.qry );
	return ok;
}

void upload( NifSkopeOpenGLContext::GLFunctions * fn, OceanGpu & g, OceanState & s )
{
	WwOcean::params( s.U, s.swell, s.P );
	for ( int k = 0; k < 2; k++ )
		WwOcean::tail( k, s.U, s.P[k], s.tail[k], s.curv[k] );
	std::vector<float> h0, q;
	WwOcean::h0Tiles( s.P, h0, q );
	fn->glBindTexture( GL_TEXTURE_2D, g.h0 );
	fn->glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, kW, WwOcean::kN, GL_RGBA, GL_FLOAT, h0.data() );
	fn->glBindTexture( GL_TEXTURE_2D, g.q );
	fn->glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, kW, WwOcean::kN, GL_RED, GL_FLOAT, q.data() );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	g.U = s.U;
	g.swell = s.swell;
	qInfo().noquote() << QStringLiteral( "cell ocean: spectrum for wind %1 m/s, sea swell %2 m: Hs sea %3 m, fresh %4 m"
		" (Cox-Munk factor %5 / %6)" ).arg( s.U, 0, 'f', 2 ).arg( s.swell, 0, 'f', 3 ).arg( s.P[0].hsSet, 0, 'f', 4 )
		.arg( s.P[1].hsSet, 0, 'f', 4 ).arg( s.P[0].fw, 0, 'f', 3 ).arg( s.P[1].fw, 0, 'f', 3 );
}

void timeRead( NifSkopeOpenGLContext::GLFunctions * fn, OceanGpu & g, OceanState & s, int slot, bool wait )
{
	if ( !g.pending[slot] )
		return;
	if ( !wait ) {
		GLuint avail = 0;
		fn->glGetQueryObjectuiv( g.qry[slot * 2 + 1], GL_QUERY_RESULT_AVAILABLE, &avail );
		if ( !avail )
			return;
	}
	GLuint64 a = 0, b = 0;
	fn->glGetQueryObjectui64v( g.qry[slot * 2], GL_QUERY_RESULT, &a );
	fn->glGetQueryObjectui64v( g.qry[slot * 2 + 1], GL_QUERY_RESULT, &b );
	s.ms = b > a ? double( b - a ) * 1e-6 : 0.0;
	g.pending[slot] = false;
	if ( ++s.frames % 120 == 1 )
		qInfo().noquote() << QStringLiteral( "cell ocean: gpu %1 ms" ).arg( s.ms, 0, 'f', 4 );
}

void quad( Renderer * r )
{
	static const float verts[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = verts;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
}

}	// namespace

void wwCellOceanPass( Scene * scene, bool cellPasses )
{
	OceanState & s = os();
	s.built = false;
	s.red = redNow();
	if ( !cellPasses || !scene || !scene->renderer || scene->selecting || !wwLookOn( WwLook::WaterOcean ) ) {
		s.why.clear();
		return;
	}
	if ( !wwCellWaterOn() || !wwCellWaterHasShapes( scene->nifModel ) ) {
		s.why = QStringLiteral( "ocean: idle (no water drawn)" );
		return;
	}
	if ( s.red & 1 ) {	// red old: today's normals, nothing built
		s.why = QStringLiteral( "ocean: RED old (today's normals)" );
		return;
	}
	float windMs = 5.0f, swellM = 0.344f;
	wwCellWaterDrive( windMs, swellM );
	s.U = double( windMs );
	s.swell = double( swellM );

	Renderer * r = scene->renderer;
	auto fn = r->fn;
	OceanGpu & g = gpus()[r];
	GLint prevDraw = 0, prevRead = 0, prevActive = 0, vp[4] = { 0, 0, 1, 1 }, prevTex[3][2] = {};
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	const int units[3] = { WwCellUnit::WaterOceanB, WwCellUnit::WaterOceanC, WwCellUnit::WaterOceanD };
	for ( int k = 0; k < 3; k++ ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + units[k] ) );
		fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prevTex[k][0] );
		fn->glGetIntegerv( GL_TEXTURE_BINDING_2D_ARRAY, &prevTex[k][1] );
	}
	auto restoreBindings = [&]() {
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
		for ( int k = 0; k < 3; k++ ) {
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + units[k] ) );
			fn->glBindTexture( GL_TEXTURE_2D, GLuint( prevTex[k][0] ) );
			fn->glBindTexture( GL_TEXTURE_2D_ARRAY, GLuint( prevTex[k][1] ) );
		}
		fn->glActiveTexture( GLenum( prevActive ) );
	};
	if ( !g.fb[0] ) {
		g.ok = create( fn, g );
		if ( !g.ok )
			qInfo().noquote() << "cell ocean: a target is incomplete; the water keeps today's normals";
	}
	if ( !g.ok ) {
		restoreBindings();
		s.why = QStringLiteral( "ocean: refused (incomplete target)" );
		return;
	}
	auto pSpec = r->useProgram( "ocean_spectrum.prog" );
	if ( !pSpec ) {
		restoreBindings();
		s.why = QStringLiteral( "ocean: refused (no ocean_spectrum.prog)" );
		return;
	}
	if ( g.U != s.U || g.swell != s.swell )
		upload( fn, g, s );

	timeRead( fn, g, s, 1 - g.cur, false );
	timeRead( fn, g, s, g.cur, true );
	fn->glQueryCounter( g.qry[g.cur * 2], kTimestamp );

	fn->glGetIntegerv( GL_VIEWPORT, vp );
	GLboolean colorMask[4], depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasStencil = fn->glIsEnabled( GL_STENCIL_TEST ),
		wasBlend = fn->glIsEnabled( GL_BLEND ), wasCull = fn->glIsEnabled( GL_CULL_FACE ),
		wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	auto bind2 = [&]( GLuint t0, GLuint t1 ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + units[0] ) );
		fn->glBindTexture( GL_TEXTURE_2D_ARRAY, 0 );
		fn->glBindTexture( GL_TEXTURE_2D, t0 );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + units[1] ) );
		fn->glBindTexture( GL_TEXTURE_2D_ARRAY, 0 );
		fn->glBindTexture( GL_TEXTURE_2D, t1 );
	};

	// the spectrum at this frame's clock (the water's world clock, looped in double at T = 1024 s)
	const double phase = wwCellWorldAnimOn() ? std::fmod( wwCellWorldSeconds(), WwOcean::kLoop ) / WwOcean::kLoop : 0.0;
	float dk[WwOcean::kTiles];
	for ( int t = 0; t < WwOcean::kTiles; t++ )
		dk[t] = float( 2.0 * WwOcean::kPi / WwOcean::set( t / 3 ).L[t % 3] );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fb[0] );
	fn->glViewport( 0, 0, kW, WwOcean::kN );
	bind2( g.h0, g.q );
	pSpec->uni1i( "oceanH0", units[0] );
	pSpec->uni1i( "oceanQ", units[1] );
	pSpec->uni1f( "oceanPhase", float( phase ) );
	pSpec->uni1fv_l( pSpec->uniLocation( "oceanDk" ), dk, WwOcean::kTiles );
	quad( r );
	int cur = 0;
	// the inverse FFT, rows then columns, 7 radix-2 Stockham stages each
	if ( auto pFft = r->useProgram( "ocean_fft.prog" ) ) {
		pFft->uni1i( "oceanIn0", units[0] );
		pFft->uni1i( "oceanIn1", units[1] );
		for ( int ax = 0; ax < 2; ax++ ) {
			pFft->uni1i( "oceanAxis", ax );
			for ( int st = 0; st < 7; st++ ) {
				fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fb[1 - cur] );
				bind2( g.ping[cur][0], g.ping[cur][1] );
				pFft->uni1i( "oceanNs", 1 << st );
				quad( r );
				cur = 1 - cur;
			}
		}
		// the fields into the arrays' level 0, all six layers in one draw (ocean_layers.geom)
		if ( auto pFin = r->useProgram( "ocean_final.prog" ) ) {
			bind2( g.ping[cur][0], g.ping[cur][1] );
			pFin->uni1i( "oceanX0", units[0] );
			pFin->uni1i( "oceanX1", units[1] );
			fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbArr );
			fn->glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, g.arrB, 0 );
			fn->glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, g.arrC, 0 );
			fn->glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, g.arrD, 0 );
			fn->glViewport( 0, 0, WwOcean::kN, WwOcean::kN );
			quad( r );
			// the mips: a 2 x 2 mean a level (C's mean of squares keeps the lost variance), not glGenerateMipmap
			// (0.6-0.85 ms on these arrays against 0.14 ms for the whole build)
			if ( auto pMip = r->useProgram( "ocean_mip.prog" ) ) {
				pMip->uni1i( "oceanB", units[0] );
				pMip->uni1i( "oceanC", units[1] );
				pMip->uni1i( "oceanD", units[2] );
				const GLuint arr[3] = { g.arrB, g.arrC, g.arrD };
				for ( int k = 0; k < 3; k++ ) {
					fn->glActiveTexture( GLenum( GL_TEXTURE0 + units[k] ) );
					fn->glBindTexture( GL_TEXTURE_2D, 0 );
					fn->glBindTexture( GL_TEXTURE_2D_ARRAY, arr[k] );
				}
				for ( int lv = 1; lv < kLevels; lv++ ) {
					for ( int k = 0; k < 3; k++ ) {
						fn->glActiveTexture( GLenum( GL_TEXTURE0 + units[k] ) );
						fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, lv - 1 );
						fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, lv - 1 );
					}
					fn->glViewport( 0, 0, WwOcean::kN >> lv, WwOcean::kN >> lv );
					fn->glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, g.arrB, lv );
					fn->glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, g.arrC, lv );
					fn->glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, g.arrD, lv );
					quad( r );
				}
				for ( int k = 0; k < 3; k++ ) {
					fn->glActiveTexture( GLenum( GL_TEXTURE0 + units[k] ) );
					fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0 );
					fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, kLevels - 1 );
				}
				s.built = true;
			}
		}
	}
	r->stopProgram();
	fn->glQueryCounter( g.qry[g.cur * 2 + 1], kTimestamp );
	g.pending[g.cur] = true;
	g.cur = 1 - g.cur;

	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
	fn->glDepthMask( depthMask );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST );
	if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST );
	if ( wasBlend ) fn->glEnable( GL_BLEND );
	if ( wasCull ) fn->glEnable( GL_CULL_FACE );
	if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST );
	restoreBindings();
	static const char * const redName[6] = { "old", "tile", "nopatch", "novar", "nochop", "flat" };
	QString red;
	for ( int k = 1; k < 6; k++ )
		if ( s.red & ( 1 << k ) )
			red = QStringLiteral( ", RED %1" ).arg( QLatin1String( redName[k] ) );
	s.why = s.built ? QStringLiteral( "ocean: wind %1 m/s, swell %2 m, Hs sea %3 m, fresh %4 m, gpu %5 ms, displaced %6%7" )
			.arg( s.U, 0, 'f', 2 ).arg( s.swell, 0, 'f', 3 ).arg( s.P[0].hsSet, 0, 'f', 4 ).arg( s.P[1].hsSet, 0, 'f', 4 )
			.arg( s.ms, 0, 'f', 4 ).arg( wwCellOceanDispOn() ? 1 : 0 ).arg( red )
		: QStringLiteral( "ocean: refused (a pass program did not link)" );
}

void wwCellOceanUniforms( Scene * scene, float windDeg )
{
	if ( !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "waterOceanOn" ) < 0 )
		return;
	OceanState & s = os();
	const bool on = s.built && wwLookOn( WwLook::WaterOcean );
	prog->uni1b( "waterOceanOn", on );
	prog->uni1i( "waterOceanRed", s.red );
	auto fn = r->fn;
	OceanGpu * g = on ? &gpus()[r] : nullptr;
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::WaterOceanB ) );
	fn->glBindTexture( GL_TEXTURE_2D_ARRAY, g ? g->arrB : 0 );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::WaterOceanC ) );
	fn->glBindTexture( GL_TEXTURE_2D_ARRAY, g ? g->arrC : 0 );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::WaterOceanD ) );
	fn->glBindTexture( GL_TEXTURE_2D_ARRAY, g ? g->arrD : 0 );
	prog->uni1i( "waterOceanB", WwCellUnit::WaterOceanB );
	prog->uni1i( "waterOceanC", WwCellUnit::WaterOceanC );
	prog->uni1i( "waterOceanD", WwCellUnit::WaterOceanD );
	/* step 2 (fo4_waterlookd.prog): full displacement out to 4096 u (one cell), none past 8192 u; an edge split to about
	 * 8 px, at most 64 times (GL's least GL_MAX_TESS_GEN_LEVEL), so a 4096 u quad's finest step is 64 u (0.9 m).
	 * Engineering choices: the waves' heights are a few metres at most, invisible as relief past two cells */
	prog->uni4f( "waterOceanDisp", FloatVector4( 4096.0f, 8192.0f, 8.0f, 64.0f ) );
	if ( !on )
		return;
	FloatVector4 invL[2], set[2], setB[2];
	for ( int k = 0; k < 2; k++ ) {
		const WwOcean::SetDef & d = WwOcean::set( k );
		invL[k] = FloatVector4( float( 1.0 / d.L[0] ), float( 1.0 / d.L[1] ), float( 1.0 / d.L[2] ), float( d.lam ) );
		set[k] = FloatVector4( float( s.P[k].hsSet ), float( s.P[k].hw ), float( s.P[k].hs2 ), float( s.tail[k] ) );
		setB[k] = FloatVector4( float( d.fetch ), float( s.curv[k] ), float( d.patch ), 0.0f );
	}
	prog->uni4fv( "waterOceanInvL", invL, 2 );
	prog->uni4fv( "waterOceanSet", set, 2 );
	prog->uni4fv( "waterOceanSetB", setB, 2 );
	/* the tiles run along the WATR's first noise layer's wind (the compass, 0 = +Y, as the swell and the scroll); the
	 * caustic net's own curvature 3.61 /m (CAUSTICPATTERN1's net at its sun path) */
	const double a = double( windDeg ) * WwOcean::kPi / 180.0;
	const double dx = std::sin( a ), dy = std::cos( a );
	prog->uni4f( "waterOceanWind", FloatVector4( float( dx ), float( dy ), float( s.U ), 3.61f ) );
	/* the wind patches drift downwind at half the wind (an engineering choice: cat's-paws run with the gusts), wrapped
	 * in double by the patch noise's period (1024 cells) so the float shader sees a small, continuous offset */
	const double t = wwCellWorldAnimOn() ? wwCellWorldSeconds() : 0.0;
	const double run = 0.5 * s.U / 0.0142875 * t;	// u
	double drift[4];
	for ( int k = 0; k < 2; k++ ) {
		const double per = 1024.0 * WwOcean::set( k ).patch;
		drift[k * 2] = -std::fmod( dx * run, per );
		drift[k * 2 + 1] = -std::fmod( dy * run, per );
	}
	prog->uni4f( "waterOceanPatch", FloatVector4( float( drift[0] ), float( drift[1] ), float( drift[2] ), float( drift[3] ) ) );
}

QString wwCellOceanEcho()
{
	return os().why;
}

bool wwCellOceanDispOn()
{
	const OceanState & s = os();
	return s.built && !( s.red & 32 );
}
