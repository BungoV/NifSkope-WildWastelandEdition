/* lane LIGHTS3 (LENSFLARE1): the placed lights' lens flares, a screen pass on the tone-mapped frame.
 * The law and what is inferred: celllensflare.h. */

#include "celllensflare.h"
#include "wwswitch.h"

#include "esmweather.h"
#include "gl/celllights.h"
#include "gl/celllooks.h"
#include "gl/glscene.h"
#include "gl/renderer.h"

#include <QHash>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
constexpr int kMaxLights = 16;				// the game's lens-flare light slots (the sun has its own)
constexpr float kFalloffRange = 256.0f;		// fLensFlareFalloffRange, the exe's default
constexpr float kSlack = 10.0f;				// game units a surface may stand before the light (inferred)

struct LfState
{
	QHash<quint32, WwLens> lens;	// LENS form -> record; a form read and refused maps to no sprites
	int lastLights = 0, lastSprites = 0;
	bool grabbed = false;
};

LfState & st()
{
	static LfState s;
	return s;
}

bool flareOn()
{
	return wwLookOn( WwLook::LensFlares );
}

QString texPath( QString t )
{
	t.replace( QChar( '\\' ), QChar( '/' ) );
	if ( !t.startsWith( QStringLiteral( "textures/" ), Qt::CaseInsensitive ) )
		t.prepend( QStringLiteral( "textures/" ) );
	return t;
}
}	// namespace

bool wwCellLensKnown( quint32 form, ESMFile & esm )
{
	LfState & s = st();
	auto it = s.lens.constFind( form );
	if ( it == s.lens.constEnd() ) {
		WwLens L;
		if ( !wwLensRead( esm, form, L ) )
			L = WwLens();
		it = s.lens.insert( form, L );
	}
	return !it->sprites.isEmpty();
}

void wwCellLensFlareGrab( Scene * scene )
{
	LfState & s = st();
	s.grabbed = false;
	if ( !scene || !flareOn() || !scene->renderer )
		return;
	const WwCellLighting * L = wwCellLightsFor( scene->nifModel );
	if ( !L || !std::any_of( L->lights.cbegin(), L->lights.cend(), []( const WwCellLight & l ) { return l.lens != 0; } ) )
		return;
	s.grabbed = scene->grabEffectDepth();	// the opaque depth (translucent draws write none), viewport-sized
}

int wwCellLensFlareDraw( Scene * scene )
{
	LfState & s = st();
	s.lastLights = s.lastSprites = 0;
	if ( !scene || !flareOn() || !scene->renderer )
		return 0;
	const WwCellLighting * L = wwCellLightsFor( scene->nifModel );
	if ( !L )
		return 0;
	WW_SWITCH_STATIC( int, red, wwEnv( "WW_CELL_LENSFLARE_RED" ).trimmed() == "noocclude" ? 1 : 0 );
	const int redNow = s.grabbed ? red : 1;	// no depth kept: drawn unoccluded rather than read garbage

	Renderer * r = scene->renderer;
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	float cam[3], fwd[3];
	for ( int j = 0; j < 3; j++ ) {
		float w = 0.0f;
		for ( int k = 0; k < 3; k++ )
			w -= vt.rotation( k, j ) * vt.translation[k];
		cam[j] = w / sc;
		fwd[j] = -vt.rotation( 2, j );	// the view looks down its -Z
	}
	const auto & pm = r->globalUniforms->projectionMatrix;
	GLint vp[4] = { 0, 0, 1, 1 };
	glGetIntegerv( GL_VIEWPORT, vp );
	if ( vp[2] < 2 || vp[3] < 2 )
		return 0;
	const float aspectHW = float( vp[3] ) / float( vp[2] );

	struct Pick
	{
		const WwCellLight * l;
		const WwLens * lens;
		float d, alpha, ang, ndc[3], viewZ;
	};
	std::vector<Pick> picks;
	for ( const WwCellLight & l : L->lights ) {
		if ( !l.lens )
			continue;
		const auto lt = s.lens.constFind( l.lens );
		if ( lt == s.lens.constEnd() || lt->sprites.isEmpty() )
			continue;
		const float v[3] = { l.pos[0] - cam[0], l.pos[1] - cam[1], l.pos[2] - cam[2] };
		const float d = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
		if ( !( d > 1e-3f ) )
			continue;
		float alpha = 1.0f - std::clamp( std::max( d - l.radius * lt->fadeRadiusScale, 0.0f ) / kFalloffRange, 0.0f, 1.0f );
		if ( l.spot ) {	// off the cone: c = cos(shine axis, toward the camera); t = (1 - c) / (1 - cos outer)
			const float c = std::clamp( -( l.dir[0] * v[0] + l.dir[1] * v[1] + l.dir[2] * v[2] ) / d, 0.0f, 1.0f );
			const float co = std::clamp( l.cosOuter, 0.0f, 1.0f );
			const float t = co < 1.0f ? std::clamp( ( 1.0f - c ) / ( 1.0f - co ), 0.0f, 1.0f ) : 1.0f;
			alpha *= 1.0f - t;
		}
		const float ang = ( v[0] * fwd[0] + v[1] * fwd[1] + v[2] * fwd[2] ) / d;
		if ( !( alpha > 0.0f ) || !( ang > 0.0f ) )
			continue;
		// world -> view -> clip; a light off the screen has no depth to test and is left out
		float pv[3];
		for ( int j = 0; j < 3; j++ )
			pv[j] = sc * ( vt.rotation( j, 0 ) * l.pos[0] + vt.rotation( j, 1 ) * l.pos[1] + vt.rotation( j, 2 ) * l.pos[2] )
				+ vt.translation[j];
		float clip[4];
		for ( int i = 0; i < 4; i++ )
			clip[i] = pm[0][i] * pv[0] + pm[1][i] * pv[1] + pm[2][i] * pv[2] + pm[3][i];
		if ( !( clip[3] > 1e-6f ) )
			continue;
		Pick p { &l, &lt.value(), d, alpha, ang, { clip[0] / clip[3], clip[1] / clip[3], clip[2] / clip[3] }, -pv[2] };
		if ( std::abs( p.ndc[0] ) > 1.0f || std::abs( p.ndc[1] ) > 1.0f )
			continue;
		picks.push_back( p );
	}
	if ( picks.empty() )
		return 0;
	std::sort( picks.begin(), picks.end(), []( const Pick & a, const Pick & b ) { return a.d < b.d; } );
	if ( picks.size() > size_t( kMaxLights ) )
		picks.resize( size_t( kMaxLights ) );

	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_lensflare.prog" );
	if ( !prog )
		return 0;
	auto fn = r->fn;
	const bool wasBlend = fn->glIsEnabled( GL_BLEND ), wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ),
		wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	GLboolean depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glEnable( GL_BLEND );
	fn->glBlendFunc( GL_ONE, GL_ONE );
	fn->glActiveTexture( GL_TEXTURE1 );
	glBindTexture( GL_TEXTURE_2D, s.grabbed ? scene->fxDepthTexId : 0 );
	prog->uni1i( "flareDepth", 1 );
	fn->glUniformMatrix4fv( prog->uniLocation( "flareProj" ), 1, GL_FALSE, &pm[0][0] );
	prog->uni2f( "flareViewport", float( vp[2] ), float( vp[3] ) );
	prog->uni1f( "flareSlack", kSlack * sc );
	prog->uni1i( "flareRed", redNow );
	fn->glActiveTexture( GL_TEXTURE0 );
	prog->uni1i( "flareTex", 0 );

	static const quint16 idx[6] = { 0, 1, 2, 0, 2, 3 };
	static const float corner[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };	// +-1: inferred
	int sprites = 0;
	for ( const Pick & p : picks ) {
		const WwCellLight & l = *p.l;
		const WwLens & lens = *p.lens;
		float infl[3];	// white + (light colour - white) x CNAM
		for ( int c = 0; c < 3; c++ )
			infl[c] = 1.0f + ( l.byteColor[c] - 1.0f ) * lens.colorInfluence;
		// the light's screen angle from the centre, x corrected by the aspect (Rotates)
		const float sx = p.ndc[0] * 0.5f, sy = p.ndc[1] * 0.5f;
		const float angle = std::atan2( sy * aspectHW, sx );
		for ( const WwLensSprite & sp : lens.sprites ) {
			const float k = std::pow( p.ang, std::pow( 255.0f, sp.angularFade ) - 1.0f ) * sp.opacity * p.alpha;
			if ( !( k > 0.0f ) || sp.texture.isEmpty() )
				continue;
			if ( scene->bindTexture( QStringView( texPath( sp.texture ) ), true ) <= 0 )
				continue;
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			const float a = ( sp.flags & 1 ) ? angle : 0.0f;
			const float ca = std::cos( a ), sa = std::sin( a );
			const float m = 1.0f - 2.0f * sp.position;	// inferred: the centre slides along the line through the screen centre
			const float cx = p.ndc[0] * m, cy = p.ndc[1] * m;
			float pos[16], col[16], uv[16], lit[16];
			for ( int v = 0; v < 4; v++ ) {
				const float x = corner[v][0] * sp.width, y = corner[v][1] * sp.height;
				pos[v * 4 + 0] = cx;
				pos[v * 4 + 1] = cy;
				pos[v * 4 + 2] = ( x * ca - y * sa ) * aspectHW;
				pos[v * 4 + 3] = x * sa + y * ca;
				for ( int c = 0; c < 3; c++ )
					col[v * 4 + c] = sp.tint[c] * infl[c] * k;
				col[v * 4 + 3] = ( sp.flags & 2 ) ? 1.0f : 0.0f;
				uv[v * 4 + 0] = corner[v][0] * 0.5f + 0.5f;
				uv[v * 4 + 1] = 0.5f - corner[v][1] * 0.5f;
				uv[v * 4 + 2] = uv[v * 4 + 3] = 0.0f;
				lit[v * 4 + 0] = p.ndc[0];
				lit[v * 4 + 1] = p.ndc[1];
				lit[v * 4 + 2] = p.viewZ;
				lit[v * 4 + 3] = 0.0f;
			}
			const float * attrs[4] = { pos, col, uv, lit };
			r->drawShape( 4, 0x4444, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, attrs, idx );
			sprites++;
		}
	}
	r->stopProgram();
	fn->glActiveTexture( GL_TEXTURE1 );
	glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glActiveTexture( GL_TEXTURE0 );
	fn->glDepthMask( depthMask );
	if ( !wasBlend )
		fn->glDisable( GL_BLEND );
	if ( wasDepth )
		fn->glEnable( GL_DEPTH_TEST );
	if ( wasCull )
		fn->glEnable( GL_CULL_FACE );
	if ( wasStencil )
		fn->glEnable( GL_STENCIL_TEST );
	s.lastLights = int( picks.size() );
	s.lastSprites = sprites;
	return sprites;
}

QString wwCellLensFlareEcho( Scene * scene )
{
	const LfState & s = st();
	int withLens = 0;
	if ( const WwCellLighting * L = scene ? wwCellLightsFor( scene->nifModel ) : nullptr )
		withLens = int( std::count_if( L->lights.cbegin(), L->lights.cend(), []( const WwCellLight & l ) { return l.lens != 0; } ) );
	return QStringLiteral( "lens flares: %1, %2 lights with a LENS, %3 flaring last frame, %4 sprites%5" )
		.arg( flareOn() ? QStringLiteral( "on" ) : QStringLiteral( "off" ) ).arg( withLens ).arg( s.lastLights )
		.arg( s.lastSprites ).arg( s.grabbed ? QString() : QStringLiteral( " (no depth kept: unoccluded)" ) );
}
