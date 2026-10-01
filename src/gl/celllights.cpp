/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "celllights.h"

#include "gl/glscene.h"
#include "gl/renderer.h"

#include <QHash>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{

constexpr int kTextureUnit = 14;		// TexCache allocates from unit 0 upward; 15 is the CSM map
constexpr int kTexelsPerLight = 4;
constexpr int kGiUnit = 13;			// lane PRTPGI: the bounce grid (sampler3D)
constexpr int kLutUnit = 12;		// lane IMGS1: the imagespace LUT (sampler3D)
constexpr int kBloomUnit = 11;		// lane BLOOM1: the imagespace bloom (sampler2D, a quarter of the view)

//! one bloom per document: the blurred bright pass, rgb floats, w x h (rows bottom-up, as read back)
struct ClBloom
{
	int w = 0, h = 0;
	std::vector<float> rgb;
	int version = 0;
	double peak = 0.0;      // the brightest bloom texel's max channel (the echo)
	int lit = 0;            // texels the bright pass passed (the echo)
};

struct ClState
{
	bool loaded = false;
	bool on = false;
	bool pinned = false;
	int probe = 0;
	int red = 0;            // bit 1 linear, 2 axis, 4 nodalc
	QHash<const void *, WwCellLighting> docs;
	QHash<const void *, int> version;
	int nextVersion = 1;
	bool giOn = false, giPinned = false;
	QHash<const void *, WwCellGi> gi;
	QHash<const void *, int> giVersion;
	bool isOn = false, isPinned = false, measuring = false;
	int isRed = 0;          // 1 nolut, 2 noexp, 4 nograde, 8 nobloom
	QHash<const void *, float> adapted;     // lane IMGS1: the last measure per document
	QHash<const void *, int> adaptedPixels;
	QHash<const void *, ClBloom> bloom;     // lane BLOOM1: the last measure's bloom per document
	int nextBloomVersion = 1;
};

ClState & st()
{
	static ClState s;
	if ( !s.loaded ) {
		s.loaded = true;
		const QByteArray pin = qgetenv( "WW_CELL_LIT" );
		if ( !pin.isEmpty() ) {
			s.pinned = true;
			s.on = pin.trimmed() != "0";
		} else {
			s.on = QSettings().value( QStringLiteral( "WW/CellLights" ), false ).toBool();
		}
		const QByteArray giPin = qgetenv( "WW_CELL_GI" );
		if ( !giPin.isEmpty() ) {
			s.giPinned = true;
			s.giOn = giPin.trimmed() != "0";
		} else {
			s.giOn = QSettings().value( QStringLiteral( "WW/CellGi" ), false ).toBool();
		}
		s.probe = qEnvironmentVariableIntValue( "WW_CELL_LIT_PROBE" );
		const QByteArray isPin = qgetenv( "WW_CELL_IS" );
		if ( !isPin.isEmpty() ) {
			s.isPinned = true;
			s.isOn = isPin.trimmed() != "0";
		} else {
			s.isOn = QSettings().value( QStringLiteral( "WW/CellImageSpace" ), false ).toBool();
		}
		const QByteArray isRed = qgetenv( "WW_CELL_IS_RED" ).trimmed();
		s.isRed = isRed == "nolut" ? 1 : isRed == "noexp" ? 2 : isRed == "nograde" ? 4 : isRed == "nobloom" ? 8 : 0;
		const QByteArray red = qgetenv( "WW_CELL_LIT_RED" ).trimmed();
		if ( red == "linear" )
			s.red = 1;
		else if ( red == "axis" )
			s.red = 2;
		else if ( red == "nodalc" )
			s.red = 4;
	}
	return s;
}

//! one texture buffer per GL context (the renderer), re-uploaded when the published version moves
struct Gpu
{
	GLuint buf = 0, tex = 0;
	const void * doc = nullptr;
	int version = 0;
	int count = 0;
	GLuint giTex = 0;
	const void * giDoc = nullptr;
	int giVersion = 0;
	GLuint lutTex = 0;
	const void * lutDoc = nullptr;
	int lutVersion = 0;
	GLuint bloomTex = 0;
	const void * bloomDoc = nullptr;
	int bloomVersion = 0;
};
QHash<const void *, Gpu> & gpus()
{
	static QHash<const void *, Gpu> g;
	return g;
}

}	// namespace

void wwCellLightsPublish( const void * nif, const WwCellLighting & lighting )
{
	ClState & s = st();
	s.docs.insert( nif, lighting );
	s.version.insert( nif, s.nextVersion++ );
}

const WwCellLighting * wwCellLightsFor( const void * nif )
{
	ClState & s = st();
	auto it = s.docs.constFind( nif );
	return it == s.docs.constEnd() ? nullptr : &*it;
}

void wwCellGiPublish( const void * nif, const WwCellGi & gi )
{
	ClState & s = st();
	s.gi.insert( nif, gi );
	s.giVersion.insert( nif, s.nextVersion++ );
}

const WwCellGi * wwCellGiFor( const void * nif )
{
	ClState & s = st();
	auto it = s.gi.constFind( nif );
	return it == s.gi.constEnd() ? nullptr : &*it;
}

bool wwCellGiOn()
{
	return st().giOn;
}

void wwCellGiSetOn( bool on )
{
	ClState & s = st();
	if ( s.giPinned )
		return;
	s.giOn = on;
	QSettings().setValue( QStringLiteral( "WW/CellGi" ), on );
}

bool wwCellLightsOn()
{
	return st().on;
}

void wwCellLightsSetOn( bool on )
{
	ClState & s = st();
	if ( s.pinned )
		return;
	s.on = on;
	QSettings().setValue( QStringLiteral( "WW/CellLights" ), on );
}

bool wwCellLightsWanted( Scene * scene )
{
	if ( !scene || !scene->renderer || !scene->nifModel || scene->selecting )
		return false;
	if ( !st().on || !wwCellLightsFor( scene->nifModel ) )
		return false;
	// an orthographic camera has no eye position to measure light distances from
	return scene->hasOption( Scene::DoLighting ) && scene->renderer->globalUniforms->projectionMatrix[3][3] != 1.0f;
}

void wwCellLightsUniforms( Scene * scene )
{
	if ( !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "cellOn" ) < 0 )
		return;
	const bool on = wwCellLightsWanted( scene );
	prog->uni1b( "cellOn", on );
	auto fn = r->fn;
	ClState & s = st();
	Gpu & g = gpus()[r];
	const WwCellLighting * L = on ? wwCellLightsFor( scene->nifModel ) : nullptr;
	if ( L && ( g.doc != scene->nifModel || g.version != s.version.value( scene->nifModel ) ) ) {
		std::vector<float> t;
		t.reserve( size_t( L->lights.size() ) * kTexelsPerLight * 4 + 16 );
		for ( const WwCellLight & l : L->lights ) {
			float dir[3] = { l.dir[0], l.dir[1], l.dir[2] };
			const float tex[16] = {
				l.pos[0], l.pos[1], l.pos[2], l.radius,
				l.color[0], l.color[1], l.color[2], l.spot ? l.cosOuter : -2.0f,
				dir[0], dir[1], dir[2], l.cone,
				l.bias, l.scale, l.exponent, l.noSpecular ? 1.0f : 0.0f };
			t.insert( t.end(), tex, tex + 16 );
		}
		if ( t.empty() )
			t.assign( 16, 0.0f );	// a buffer texture must have a store
		if ( !g.buf ) {
			fn->glGenBuffers( 1, &g.buf );
			fn->glGenTextures( 1, &g.tex );
		}
		fn->glBindBuffer( GL_TEXTURE_BUFFER, g.buf );
		fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( t.size() * sizeof( float ) ), t.data(), GL_STATIC_DRAW );
		fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
		g.doc = scene->nifModel;
		g.version = s.version.value( scene->nifModel );
		g.count = int( L->lights.size() );
	}
	/* The sampler is bound whether or not this draw lights: a samplerBuffer left on unit 0
	 * beside BaseMap's sampler2D is a draw-time INVALID_OPERATION. */
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kTextureUnit ) );
	if ( g.tex && g.buf ) {
		fn->glBindTexture( GL_TEXTURE_BUFFER, g.tex );
		fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, g.buf );
	} else {
		fn->glBindTexture( GL_TEXTURE_BUFFER, 0 );
	}
	// lane PRTPGI: the bounce grid, bound (like the buffer above) whether or not this draw uses it
	const WwCellGi * G = on && s.giOn ? wwCellGiFor( scene->nifModel ) : nullptr;
	if ( G && !G->rgba.empty() && ( g.giDoc != scene->nifModel || g.giVersion != s.giVersion.value( scene->nifModel ) ) ) {
		if ( !g.giTex )
			fn->glGenTextures( 1, &g.giTex );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kGiUnit ) );
		fn->glBindTexture( GL_TEXTURE_3D, g.giTex );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
		fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA16F, G->dims[0], G->dims[1], G->dims[2] * 6, 0, GL_RGBA, GL_FLOAT,
			G->rgba.data() );
		g.giDoc = scene->nifModel;
		g.giVersion = s.giVersion.value( scene->nifModel );
	}
	const bool giDraw = G && g.giTex && g.giDoc == scene->nifModel;
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kGiUnit ) );
	fn->glBindTexture( GL_TEXTURE_3D, giDraw ? g.giTex : 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni1i( "cellGi", kGiUnit );
	prog->uni1b( "cellGiOn", giDraw );
	if ( giDraw ) {
		prog->uni3f( "cellGiOrigin", G->origin[0], G->origin[1], G->origin[2] );
		prog->uni1f( "cellGiVoxel", G->voxel );
		prog->uni3f( "cellGiDims", float( G->dims[0] ), float( G->dims[1] ), float( G->dims[2] ) );
	}
	// lane IMGS1: the imagespace, once this document has a measure; its LUT bound like the grid above
	const float adapted = s.adapted.value( scene->nifModel, -1.0f );
	const bool isDraw = L && !s.measuring && adapted >= 0.0f && wwCellImageSpaceWanted( scene );
	if ( isDraw && L->isLut.size() == 16 * 16 * 16 * 3
		&& ( g.lutDoc != scene->nifModel || g.lutVersion != s.version.value( scene->nifModel ) ) ) {
		if ( !g.lutTex )
			fn->glGenTextures( 1, &g.lutTex );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kLutUnit ) );
		fn->glBindTexture( GL_TEXTURE_3D, g.lutTex );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_RGB8, 16, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, L->isLut.data() );
		fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
		g.lutDoc = scene->nifModel;
		g.lutVersion = s.version.value( scene->nifModel );
	}
	const bool lutDraw = isDraw && g.lutTex && g.lutDoc == scene->nifModel && L->isLut.size() == 16 * 16 * 16 * 3;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kLutUnit ) );
	fn->glBindTexture( GL_TEXTURE_3D, lutDraw ? g.lutTex : 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	// lane BLOOM1: the measure's bloom, a quarter of the view, sampled at the fragment's place in the view
	const ClBloom * B = nullptr;
	if ( isDraw ) {
		auto it = s.bloom.constFind( scene->nifModel );
		if ( it != s.bloom.constEnd() )
			B = &*it;
	}
	if ( B && B->w > 0 && ( g.bloomDoc != scene->nifModel || g.bloomVersion != B->version ) ) {
		if ( !g.bloomTex )
			fn->glGenTextures( 1, &g.bloomTex );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kBloomUnit ) );
		fn->glBindTexture( GL_TEXTURE_2D, g.bloomTex );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB32F, B->w, B->h, 0, GL_RGB, GL_FLOAT, B->rgb.data() );
		g.bloomDoc = scene->nifModel;
		g.bloomVersion = B->version;
	}
	const bool bloomDraw = B && B->w > 0 && g.bloomTex && g.bloomDoc == scene->nifModel && !( s.isRed & 8 );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kBloomUnit ) );
	fn->glBindTexture( GL_TEXTURE_2D, bloomDraw ? g.bloomTex : 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni1i( "cellIsBloom", kBloomUnit );
	prog->uni1b( "cellIsBloomOn", bloomDraw );
	if ( bloomDraw ) {
		GLint vp[4] = { 0, 0, 1, 1 };
		fn->glGetIntegerv( GL_VIEWPORT, vp );
		prog->uni4f_l( prog->uniLocation( "cellIsBloomRect" ), FloatVector4( float( vp[0] ), float( vp[1] ),
			1.0f / float( std::max( vp[2], 1 ) ), 1.0f / float( std::max( vp[3], 1 ) ) ) );
	}
	prog->uni1i( "cellIsLut", kLutUnit );
	prog->uni1b( "cellIsOn", isDraw );
	prog->uni1b( "cellIsLutOn", lutDraw );
	if ( isDraw ) {
		const float * h = L->isHdr;
		// the tonemap PS: max( mid / (lum + 0.001), cb.y ) then min( .., cb.x ); cb (x, y) = HNAM (max, min)
		const float e = std::min( std::max( h[8] / ( adapted + 0.001f ), h[5] ), h[4] );
		prog->uni1f( "cellIsExposure", e );
		prog->uni1f( "cellIsE", h[1] );
		prog->uni1f( "cellIsAdapted", adapted );
		prog->uni3f( "cellIsCine", L->isCine[0], L->isCine[1], L->isCine[2] );
		prog->uni4f_l( prog->uniLocation( "cellIsTint" ), FloatVector4( L->isTint[0], L->isTint[1], L->isTint[2], L->isTint[3] ) );
		prog->uni1i( "cellIsRed", s.isRed );
	}
	prog->uni1i( "cellLights", kTextureUnit );
	if ( !L )
		return;
	prog->uni1i( "cellLightCount", g.count );
	// world = R^T (posView - t) / sc  (lookdevstage.cpp's fog uses the same inverse)
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	for ( int k = 0; k < 3; k++ ) {
		float w = 0.0f;
		for ( int j = 0; j < 3; j++ )
			w -= vt.rotation( j, k ) * vt.translation[j];
		prog->uni4f_l( prog->uniLocation( "cellRow[%d]", k ), FloatVector4( vt.rotation( 0, k ) / sc,
			vt.rotation( 1, k ) / sc, vt.rotation( 2, k ) / sc, w / sc ) );
	}
	// PRTP2 section 4: per channel, ((p - n) / 2 per axis, mean of the six), gamma; the shader powers it
	const bool dalc = L->hasDalc && !( s.red & 4 );
	prog->uni1b( "cellHasDalc", dalc );
	for ( int c = 0; c < 3 && dalc; c++ ) {
		float mean = 0.0f;
		for ( int a = 0; a < 6; a++ )
			mean += L->dalc[a][c] / 6.0f;
		prog->uni4f_l( prog->uniLocation( "cellDalc[%d]", c ), FloatVector4(
			( L->dalc[0][c] - L->dalc[1][c] ) * 0.5f, ( L->dalc[2][c] - L->dalc[3][c] ) * 0.5f,
			( L->dalc[4][c] - L->dalc[5][c] ) * 0.5f, mean ) );
	}
	prog->uni1b( "cellHasDir", L->hasDirectional );
	prog->uni3f( "cellDirColor", L->dirColor[0], L->dirColor[1], L->dirColor[2] );
	prog->uni3f( "cellDirTo", L->dirTo[0], L->dirTo[1], L->dirTo[2] );
	prog->uni1b( "cellInterior", L->interior );
	prog->uni3f( "cellCenter", L->center[0], L->center[1], L->center[2] );
	prog->uni1i( "cellProbe", s.measuring ? 6 : s.probe );
	prog->uni1i( "cellRed", s.red );
}

bool wwCellImageSpaceOn()
{
	return st().isOn;
}

void wwCellImageSpaceSetOn( bool on )
{
	ClState & s = st();
	if ( s.isPinned )
		return;
	s.isOn = on;
	QSettings().setValue( QStringLiteral( "WW/CellImageSpace" ), on );
}

bool wwCellImageSpaceWanted( Scene * scene )
{
	if ( !st().isOn || !wwCellLightsWanted( scene ) )
		return false;
	const WwCellLighting * L = wwCellLightsFor( scene->nifModel );
	return L && L->hasImageSpace;
}

void wwCellImageSpaceMeasuring( bool on )
{
	st().measuring = on;
}

bool wwCellImageSpaceIsMeasuring()
{
	return st().measuring;
}

void wwCellImageSpaceSetAdapted( Scene * scene, float lum, int pixels )
{
	if ( !scene || !scene->nifModel )
		return;
	st().adapted.insert( scene->nifModel, lum );
	st().adaptedPixels.insert( scene->nifModel, pixels );
}

void wwCellImageSpaceSetBloom( Scene * scene, const float * rgba, int w, int h, int step )
{
	if ( !scene || !scene->nifModel || !rgba || w <= 0 || h <= 0 || step < 1 )
		return;
	const WwCellLighting * L = wwCellLightsFor( scene->nifModel );
	if ( !L || !L->hasImageSpace )
		return;
	ClState & s = st();
	ClBloom & B = s.bloom[scene->nifModel];
	// the source: the measure at a quarter of the view (a full-size measure box-averaged step x step)
	const int bw = std::max( 1, w / step ), bh = std::max( 1, h / step );
	const float thresh = L->isHdr[2], scale = L->isHdr[3];
	std::vector<float> src( size_t( bw ) * size_t( bh ) * 3 );
	B.lit = 0;
	for ( int y = 0; y < bh; y++ )
		for ( int x = 0; x < bw; x++ )
			for ( int c = 0; c < 3; c++ ) {
				double sum = 0.0;
				for ( int j = 0; j < step; j++ )
					for ( int i = 0; i < step; i++ ) {
						const float v = rgba[( size_t( y * step + j ) * size_t( w ) + size_t( x * step + i ) ) * 4 + size_t( c )];
						if ( std::isfinite( v ) )
							sum += v;
					}
				src[( size_t( y ) * size_t( bw ) + size_t( x ) ) * 3 + size_t( c )] = float( sum / double( step * step ) );
			}
	// the game's blur: radius 7, 15 taps exp(-2 x^2 / 49), normalised; vertical with the bright pass
	// scale * max(0, c - threshold) per tap, then horizontal plain (docs/PRTP_PLAN.md 2j)
	constexpr int r = 7;
	float wt[2 * r + 1];
	float wsum = 0.0f;
	for ( int x = -r; x <= r; x++ )
		wsum += ( wt[x + r] = std::exp( -2.0f * float( x * x ) / float( r * r ) ) );
	for ( float & v : wt )
		v /= wsum;
	for ( size_t i = 0; i < src.size(); i += 3 ) {
		bool any = false;
		for ( int c = 0; c < 3; c++ ) {
			src[i + size_t( c )] = scale * std::max( 0.0f, src[i + size_t( c )] - thresh );
			any = any || src[i + size_t( c )] > 0.0f;
		}
		B.lit += any ? 1 : 0;
	}
	std::vector<float> mid( src.size() );
	for ( int y = 0; y < bh; y++ )
		for ( int x = 0; x < bw; x++ )
			for ( int c = 0; c < 3; c++ ) {
				float a = 0.0f;
				for ( int k = -r; k <= r; k++ ) {
					const int yy = std::min( std::max( y + k, 0 ), bh - 1 );
					a += wt[k + r] * src[( size_t( yy ) * size_t( bw ) + size_t( x ) ) * 3 + size_t( c )];
				}
				mid[( size_t( y ) * size_t( bw ) + size_t( x ) ) * 3 + size_t( c )] = a;
			}
	B.rgb.assign( src.size(), 0.0f );
	B.peak = 0.0;
	for ( int y = 0; y < bh; y++ )
		for ( int x = 0; x < bw; x++ )
			for ( int c = 0; c < 3; c++ ) {
				float a = 0.0f;
				for ( int k = -r; k <= r; k++ ) {
					const int xx = std::min( std::max( x + k, 0 ), bw - 1 );
					a += wt[k + r] * mid[( size_t( y ) * size_t( bw ) + size_t( xx ) ) * 3 + size_t( c )];
				}
				B.rgb[( size_t( y ) * size_t( bw ) + size_t( x ) ) * 3 + size_t( c )] = a;
				B.peak = std::max( B.peak, double( a ) );
			}
	B.w = bw;
	B.h = bh;
	B.version = s.nextBloomVersion++;
}

QString wwCellImageSpaceEcho( Scene * scene )
{
	ClState & s = st();
	const WwCellLighting * L = scene && scene->nifModel ? wwCellLightsFor( scene->nifModel ) : nullptr;
	QString o = QStringLiteral( "imagespace=%1(asked=%2)" ).arg( wwCellImageSpaceWanted( scene ) ? "on" : "off" ).arg( s.isOn ? 1 : 0 );
	if ( !L || !L->hasImageSpace )
		return o + QStringLiteral( " (the cell has none)" );
	const float a = s.adapted.value( scene->nifModel, -1.0f );
	const float * h = L->isHdr;
	const float e = std::min( std::max( h[8] / ( a + 0.001f ), h[5] ), h[4] );
	o += QStringLiteral( " %1 adapted=%2 over %3 px exposure=%4 tonemapE=%5 lut=%6" ).arg( L->isName )
		.arg( double( a ), 0, 'g', 7 ).arg( s.adaptedPixels.value( scene->nifModel ) ).arg( double( e ), 0, 'g', 7 )
		.arg( double( h[1] ), 0, 'g', 4 ).arg( L->isLut.empty() ? QStringLiteral( "none" ) : L->isLutPath );
	const ClBloom bl = s.bloom.value( scene->nifModel );
	o += QStringLiteral( " bloom=%1x%2 threshold=%3 scale=%4 lit=%5 peak=%6" ).arg( bl.w ).arg( bl.h )
		.arg( double( h[2] ), 0, 'g', 7 ).arg( double( h[3] ), 0, 'g', 7 ).arg( bl.lit ).arg( bl.peak, 0, 'g', 6 );
	if ( s.isRed )
		o += QStringLiteral( " red=%1" ).arg( s.isRed );
	return o;
}

int wwCellLightsRed()
{
	return st().red;
}

QString wwCellLightsEcho( Scene * scene )
{
	ClState & s = st();
	const WwCellLighting * L = scene && scene->nifModel ? wwCellLightsFor( scene->nifModel ) : nullptr;
	QString o = QStringLiteral( "celllit=%1(asked=%2)" ).arg( wwCellLightsWanted( scene ) ? "on" : "off" )
		.arg( s.on ? 1 : 0 );
	if ( L )
		o += QStringLiteral( " %1" ).arg( L->summary );
	else
		o += QStringLiteral( " (no cell lighting published for this document)" );
	const WwCellGi * G = scene && scene->nifModel ? wwCellGiFor( scene->nifModel ) : nullptr;
	o += QStringLiteral( " gi=%1(asked=%2%3)" ).arg( wwCellLightsWanted( scene ) && s.giOn && G ? "on" : "off" )
		.arg( s.giOn ? 1 : 0 ).arg( G ? QStringLiteral( ", %1" ).arg( G->summary ) : QStringLiteral( ", none published" ) );
	if ( s.probe )
		o += QStringLiteral( " probe=%1" ).arg( s.probe );
	if ( s.red )
		o += QStringLiteral( " red=%1" ).arg( s.red );
	return o;
}
