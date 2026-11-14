/* lane LIGHTS3 (LENSFLARE1): the placed lights' lens flares, a screen pass on the tone-mapped frame.
 * The law and what is inferred: celllensflare.h. */

#include "features/post/celllensflare.h"
#include "wwswitch.h"

#include "features/vkframe/vkpresent.h"	// lane VKPOST2: a deferred tone pass crosses before the flare draws into the view
#include "features/weather/esmweather.h"
#include "gl/celllights.h"
#include "gl/celllooks.h"
#include "gl/glscene.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"

#include <QDebug>
#include <QHash>
#include <QOpenGLContext>
#include <QSet>
#include <QVector>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string>
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
	QString sunLens;	// lane LIGHTS4: the weather's sun flare (EDID), empty = none
	bool sunShown = false;
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

// lane VKFAR2 P9: the sprite maps named so far (TexCache::loadTex makes these as BC twins), keyed from "textures/" on
QString lensTexKey( const QString & path )
{
	QString f = path.toLower();
	f.replace( QChar( '\\' ), QChar( '/' ) );
	const qsizetype at = f.indexOf( QLatin1StringView( "textures/" ) );
	return at > 0 ? f.mid( at ) : f;
}

QSet<QString> & lensTexNames()
{
	static QSet<QString> s;
	return s;
}

// "vk lensflare:" when the route changes, and every 5 s while it holds (the stats since the last line)
void lensVkSay( bool vk )
{
	static int was = -1;
	static auto at = std::chrono::steady_clock::now();
	static WwVkPresent::FxStat sum;
	const WwVkPresent::FxStat st = WwVkPresent::takeToneFlareStat();
	sum.runs += st.runs;
	sum.gl += st.gl;
	sum.fails += st.fails;
	sum.cpuMs += st.cpuMs;
	sum.maxMs = std::max( sum.maxMs, st.maxMs );
	sum.copies += st.copies;
	sum.pipelines = st.pipelines;
	sum.buildMs = st.buildMs;
	if ( !st.glWhy.empty() )
		sum.glWhy = st.glWhy;
	if ( !st.failWhy.empty() )
		sum.failWhy = st.failWhy;
	const auto now = std::chrono::steady_clock::now();
	if ( was == int( vk ) && std::chrono::duration<double>( now - at ).count() < 5.0 )
		return;
	was = int( vk );
	at = now;
	QString line = sum.runs > 0
		? QStringLiteral( "vk lensflare: in the deferred tone pass %1 frames (%2 pipelines built in %3 ms; %4 depth copies; "
			"drawing thread %5 ms, max %6)" ).arg( sum.runs ).arg( sum.pipelines ).arg( sum.buildMs, 0, 'f', 1 ).arg( sum.copies )
			.arg( sum.cpuMs / sum.runs, 0, 'f', 3 ).arg( sum.maxMs, 0, 'f', 3 )
		: QStringLiteral( "vk lensflare: 0 frames in the tone pass" );
	if ( sum.gl > 0 )
		line += QStringLiteral( ", on GL %1 frames (last: %2)" ).arg( sum.gl ).arg( QString::fromStdString( sum.glWhy ) );
	if ( sum.fails > 0 )
		line += QStringLiteral( ", refused %1 (last: %2)" ).arg( sum.fails ).arg( QString::fromStdString( sum.failWhy ) );
	qInfo().noquote() << line;
	sum = WwVkPresent::FxStat();
}
}	// namespace

bool wwCellLensFlareTexWanted( const QString & fileName )
{
	return lensTexNames().contains( lensTexKey( fileName ) );
}

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
	const WwLens * sun = nullptr;
	float sd[3], sc[3];
	if ( !L || ( !std::any_of( L->lights.cbegin(), L->lights.cend(), []( const WwCellLight & l ) { return l.lens != 0; } )
	             && !wwLookdevSunLens( sun, sd, sc ) ) )
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
		const float * tint;	// the colour CNAM pulls the sprites toward (a light's byte colour, the sun's colour)
		const WwLens * lens;
		float d, alpha, ang, ndc[3], viewZ;
	};
	std::vector<Pick> picks;
	s.sunShown = false;
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
		Pick p { l.byteColor, &lt.value(), d, alpha, ang, { clip[0] / clip[3], clip[1] / clip[3], clip[2] / clip[3] }, -pv[2] };
		if ( std::abs( p.ndc[0] ) > 1.0f || std::abs( p.ndc[1] ) > 1.0f )
			continue;
		picks.push_back( p );
	}
	std::sort( picks.begin(), picks.end(), []( const Pick & a, const Pick & b ) { return a.d < b.d; } );
	if ( picks.size() > size_t( kMaxLights ) )
		picks.resize( size_t( kMaxLights ) );
	/* lane LIGHTS4: the sun's own slot, its LENS from the weather (WTHR GNAM). Inferred: a point at infinity along
	 * the sun direction, seen only where the opaque depth is clear, with the placed lights' sprite rule */
	const WwLens * sunLens = nullptr;
	float sunTint[3], sunDir[3];
	s.sunLens.clear();
	if ( wwLookdevSunLens( sunLens, sunDir, sunTint ) ) {
		s.sunLens = sunLens->edid.isEmpty() ? QStringLiteral( "%1" ).arg( sunLens->formID, 8, 16, QChar( '0' ) ) : sunLens->edid;
		for ( int c = 0; c < 3; c++ )
			sunTint[c] = std::clamp( sunTint[c], 0.0f, 1.0f );
		const float ang = sunDir[0] * fwd[0] + sunDir[1] * fwd[1] + sunDir[2] * fwd[2];
		float pv[3];
		for ( int j = 0; j < 3; j++ )
			pv[j] = vt.rotation( j, 0 ) * sunDir[0] + vt.rotation( j, 1 ) * sunDir[1] + vt.rotation( j, 2 ) * sunDir[2];
		float clip[4];
		for ( int i = 0; i < 4; i++ )
			clip[i] = pm[0][i] * pv[0] + pm[1][i] * pv[1] + pm[2][i] * pv[2];
		if ( sunDir[2] > 0.0f && ang > 0.0f && clip[3] > 1e-6f ) {
			Pick p { sunTint, sunLens, 0.0f, 1.0f, ang, { clip[0] / clip[3], clip[1] / clip[3], 1.0f }, 1e30f };
			if ( std::abs( p.ndc[0] ) <= 1.0f && std::abs( p.ndc[1] ) <= 1.0f ) {
				picks.push_back( p );
				s.sunShown = true;
			}
		}
	}
	if ( picks.empty() )
		return 0;

	static const quint16 idx[6] = { 0, 1, 2, 0, 2, 3 };
	static const float corner[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };	// +-1: inferred
	/* one sprite's four corners as GL's four attributes (pos, col, uv, lit); false: not drawn. The GL draw and the Vulkan
	 * table (lane VKFAR2 P9) read the same numbers */
	auto spriteOf = [&]( const Pick & p, const WwLensSprite & sp, float * pos, float * col, float * uv, float * lit ) {
		const WwLens & lens = *p.lens;
		const float k = std::pow( p.ang, std::pow( 255.0f, sp.angularFade ) - 1.0f ) * sp.opacity * p.alpha;
		if ( !( k > 0.0f ) || sp.texture.isEmpty() )
			return false;
		float infl[3];	// white + (light colour - white) x CNAM
		for ( int c = 0; c < 3; c++ )
			infl[c] = 1.0f + ( p.tint[c] - 1.0f ) * lens.colorInfluence;
		// the light's screen angle from the centre, x corrected by the aspect (Rotates)
		const float sx = p.ndc[0] * 0.5f, sy = p.ndc[1] * 0.5f;
		const float angle = std::atan2( sy * aspectHW, sx );
		const float a = ( sp.flags & 1 ) ? angle : 0.0f;
		const float ca = std::cos( a ), sa = std::sin( a );
		const float m = 1.0f - 2.0f * sp.position;	// inferred: the centre slides along the line through the screen centre
		const float cx = p.ndc[0] * m, cy = p.ndc[1] * m;
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
		return true;
	};

	/* lane VKFAR2 P9: while the tone pass is deferred (no GL pass crossed since the tone map) the sprites are recorded in it,
	 * after its modes, and GL draws nothing; otherwise (a refusal: a map with no BC twin, the red WW_VKVIEW_LENS_RED=1) the
	 * tone pass crosses first and GL draws them as before. Each sprite map is named for the twin before its first bind */
	WW_SWITCH_STATIC( bool, vkRed, wwEnvInt( "WW_VKVIEW_LENS_RED" ) == 1 );
	std::string vkWhy = vkRed ? "red WW_VKVIEW_LENS_RED=1" : "no deferred tone pass (the view is not on Vulkan, or a GL pass crossed first)";
	if ( !vkRed && WwVkPresent::tonePending() ) {
		static std::vector<float> rows;
		static std::vector<unsigned> texs;
		rows.clear();
		texs.clear();
		bool ok = true;
		r->fn->glActiveTexture( GL_TEXTURE0 );
		for ( const Pick & p : picks ) {
			for ( const WwLensSprite & sp : p.lens->sprites ) {
				float pos[16], col[16], uv[16], lit[16];
				if ( !ok || !spriteOf( p, sp, pos, col, uv, lit ) )
					continue;
				const QString path = texPath( sp.texture );
				lensTexNames().insert( lensTexKey( path ) );
				if ( scene->bindTexture( QStringView( path ), true ) <= 0 )
					continue;
				const GLuint name = scene->textures->wwBound;
				if ( !name || texs.size() >= size_t( WwVkPresent::kFlareMaxSprites ) ) {
					ok = false;
					vkWhy = name ? "more sprites than the table holds" : "a sprite map bound by its first load (no cache name yet)";
					continue;
				}
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
				for ( int v = 0; v < 4; v++ )
					for ( const float * a : { pos, col, uv, lit } )
						rows.insert( rows.end(), a + v * 4, a + v * 4 + 4 );
				texs.push_back( unsigned( name ) );
			}
		}
		if ( ok && texs.empty() ) {
			s.lastLights = int( picks.size() );
			return 0;
		}
		if ( ok ) {
			WwVkPresent::FlareRun f;
			f.n = int( texs.size() );
			f.rows = rows.data();
			f.tex = texs.data();
			std::memcpy( f.proj, &pm[0][0], sizeof( f.proj ) );
			f.slack = kSlack * sc;
			f.red = redNow;
			f.depth = s.grabbed ? unsigned( scene->fxDepthTexId ) : 0u;
			std::memcpy( f.vp, vp, sizeof( f.vp ) );
			f.gp = []( const char * n ) -> void * {
				QOpenGLContext * gc = QOpenGLContext::currentContext();
				return gc ? reinterpret_cast<void *>( gc->getProcAddress( n ) ) : nullptr;
			};
			if ( WwVkPresent::toneFlareQueue( f, vkWhy ) ) {
				s.lastLights = int( picks.size() );
				s.lastSprites = f.n;
				lensVkSay( true );
				return f.n;
			}
		}
	}
	if ( WwVkPresent::shareWanted( nullptr ) ) {	// the view on Vulkan: the route line says why GL drew them
		WwVkPresent::toneFlareOnGl( vkWhy.c_str() );
		lensVkSay( false );
	}
	WwVkPresent::toneFlush();	// lane VKPOST2: additive over the view: the tone map lands first

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

	int sprites = 0;
	for ( const Pick & p : picks ) {
		for ( const WwLensSprite & sp : p.lens->sprites ) {
			float pos[16], col[16], uv[16], lit[16];
			if ( !spriteOf( p, sp, pos, col, uv, lit ) )
				continue;
			const QString path = texPath( sp.texture );
			lensTexNames().insert( lensTexKey( path ) );
			if ( scene->bindTexture( QStringView( path ), true ) <= 0 )
				continue;
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
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
	return QStringLiteral( "lens flares: %1, %2 lights with a LENS, %3 flaring last frame, %4 sprites%5, sun flare %6%7" )
		.arg( flareOn() ? QStringLiteral( "on" ) : QStringLiteral( "off" ) ).arg( withLens ).arg( s.lastLights )
		.arg( s.lastSprites ).arg( s.grabbed ? QString() : QStringLiteral( " (no depth kept: unoccluded)" ) )
		.arg( s.sunLens.isEmpty() ? QStringLiteral( "none" ) : s.sunLens )
		.arg( s.sunShown ? QStringLiteral( " (in view)" ) : QString() );
}
