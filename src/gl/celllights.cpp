/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "celllights.h"

#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"
#include "gl/cellhdr.h"

#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{

constexpr int kTextureUnit = 14;		// TexCache allocates from unit 0 upward; 15 is the CSM map
constexpr int kTexelsPerLight = 5;		// lane SHADOW1 added the 5th: shadow slot, kind, near clip, XLIG bias
constexpr int kGiUnit = 13;			// lane PRTPGI: the bounce grid (sampler3D)
constexpr int kLutUnit = 12;		// lane IMGS1: the imagespace LUT (sampler3D)
constexpr int kBloomUnit = 11;		// lane BLOOM1: the imagespace bloom (sampler2D, a quarter of the view)
constexpr int kShadowUnit = 10;		// lane SHADOW1: the lights' depth cubes (samplerCubeArrayShadow)
constexpr int kShadowSlots = 16;	// shadowed lights at once, the nearest the camera
constexpr int kShadowFace = 512;	// texels a cube face edge (the game's 2048 paraboloid's centre texel at 1024)
constexpr int kAmbientSlots = 16;	// lane AMBO2: Ambient Only volumes a cell (Fallout4.esm's most in one cell: 5, measured)

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
	int isRed = 0;          // 1 nolut, 2 noexp, 4 nograde, 8 nobloom, 16 nofx
	QHash<const void *, float> adapted;     // lane IMGS1: the last measure per document
	QHash<const void *, int> adaptedPixels;
	QHash<const void *, ClBloom> bloom;     // lane BLOOM1: the last measure's bloom per document
	int nextBloomVersion = 1;
	bool shadowOn = true;   // lane SHADOW1: WW_CELL_SHADOW=0 turns the maps off (the harness's unshadowed pass)
	bool shadowRed = false; // WW_CELL_SHADOW_RED=noshadow: the maps rendered, every factor read as 1
	QString shadowLast = QStringLiteral( "none yet" );
	int fogProbe = 0;       // lane FOG2: WW_CELL_FOG_PROBE=6 (alpha, height blend) | 7 (fog colour ^ 1/2.2) | 8 (its d, z), per fragment
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
		s.isRed = isRed == "nolut" ? 1 : isRed == "noexp" ? 2 : isRed == "nograde" ? 4 : isRed == "nobloom" ? 8
			: isRed == "nofx" ? 16 : 0;
		s.shadowOn = qgetenv( "WW_CELL_SHADOW" ).trimmed() != "0";
		s.shadowRed = qgetenv( "WW_CELL_SHADOW_RED" ).trimmed() == "noshadow";
		s.fogProbe = qEnvironmentVariableIntValue( "WW_CELL_FOG_PROBE" );
		const QByteArray red = qgetenv( "WW_CELL_LIT_RED" ).trimmed();
		if ( red == "linear" )
			s.red = 1;
		else if ( red == "axis" )
			s.red = 2;
		else if ( red == "nodalc" )
			s.red = 4;
		else if ( red == "lambert" )
			s.red = 8;	// lane ON1: the diffuse back to Lambert
		else if ( red == "normalised" )
			s.red = 16;	// lane ON1: the textbook (normalised) Oren-Nayar azimuth
		else if ( red == "norim" )
			s.red = 32;	// lane RIM1: the back-light rim term dropped
		else if ( red == "rimflags" )
			s.red = 64;	// lane RIM1: the lights' No Rim / Ignore Roughness flags ignored
		else if ( red == "ambientlit" )
			s.red = 128;	// lane AMBO1: the Ambient Only lights drawn as ordinary lights
		else if ( red == "cubeold" )
			s.red = 512;	// lane CUBE1: the interior cube map at its old scale (squared, x Lambert, default cube)
		else if ( red == "ambientfull" )
			s.red = 1024;	// lane AMBO2: the Ambient Only lights' ambient adjustment ignored
		// lane POOL1: the lights' Non Specular flag ignored (a bit clear of WW_CELL_LIT_RED's)
		if ( qgetenv( "WW_CELL_SPEC_RED" ).trimmed() == "nonspec" )
			s.red |= 65536;
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
	// lane SHADOW1: the depth cube array, the light each slot holds (-1 free), and a stamp the light
	// buffer re-uploads on (a light's slot is its 5th texel)
	GLuint shTex = 0, shFbo = 0;
	const void * shDoc = nullptr;
	int shVersion = 0;
	int shSlot[kShadowSlots];
	int shStamp = 1, bufShStamp = 0;
	Gpu() { std::fill( shSlot, shSlot + kShadowSlots, -1 ); }
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

/* Lane EFX1: the effect program takes no cell uniforms, so in a probe pass a see-through effect (the Vault 111
 * steam, once it drew as itself) wrote its own colour over the surface the probes measure: Vault view 2's legacy
 * share fell from 23.1% to 14.7%. Probe passes are harness-only; the pictures bungo looks at keep every effect. */
bool wwCellProbePass( Scene * scene )
{
	const ClState & s = st();
	return ( s.probe > 0 || s.fogProbe > 0 ) && wwCellLightsWanted( scene );
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
	if ( L && ( g.doc != scene->nifModel || g.version != s.version.value( scene->nifModel ) || g.bufShStamp != g.shStamp ) ) {
		// lane SHADOW1: the slot each light holds (the slots belong to the document the shadow pass last saw)
		std::vector<float> slotOf( size_t( L->lights.size() ), -1.0f );
		if ( g.shDoc == scene->nifModel && g.shVersion == s.version.value( scene->nifModel ) )
			for ( int k = 0; k < kShadowSlots; k++ )
				if ( g.shSlot[k] >= 0 && g.shSlot[k] < L->lights.size() )
					slotOf[size_t( g.shSlot[k] )] = float( k );
		std::vector<float> t;
		t.reserve( size_t( L->lights.size() ) * kTexelsPerLight * 4 + 20 );
		for ( qsizetype i = 0; i < L->lights.size(); i++ ) {
			const WwCellLight & l = L->lights.at( i );
			float dir[3] = { l.dir[0], l.dir[1], l.dir[2] };
			const float tex[20] = {
				l.pos[0], l.pos[1], l.pos[2], l.radius,
				l.color[0], l.color[1], l.color[2], l.spot ? l.cosOuter : -2.0f,
				dir[0], dir[1], dir[2], l.cone,
				l.bias, l.scale, l.exponent,
				float( ( l.noSpecular ? 1 : 0 ) | ( l.noRim ? 2 : 0 ) | ( l.ignoreRoughness ? 4 : 0 ) ),	// lane RIM1
				slotOf[size_t( i )], float( l.shadow ), l.nearClip, l.shadowBias };
			t.insert( t.end(), tex, tex + 20 );
		}
		g.bufShStamp = g.shStamp;
		if ( t.empty() )
			t.assign( 20, 0.0f );	// a buffer texture must have a store
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
	// lane SHADOW1: the depth cubes, bound (a sampler left on unit 0 beside BaseMap would be a type clash)
	const bool shadowDraw = L && s.shadowOn && !s.shadowRed && g.shTex && g.shDoc == scene->nifModel;
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kShadowUnit ) );
	fn->glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, shadowDraw ? g.shTex : 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni1i( "cellShadow", kShadowUnit );
	prog->uni1b( "cellShadowOn", shadowDraw );
	prog->uni1f( "cellShadowTexel", 2.0f / float( kShadowFace ) );
	prog->uni1i( "cellIsLut", kLutUnit );
	prog->uni1b( "cellIsOn", isDraw );
	prog->uni1b( "cellIsLinear", isDraw && wwCellHdrActive() );	// lane HDR1: write linear light, tone-mapped once
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
	prog->uni1f( "fxDistScale", 1.0f / sc );	// lane EFX2: view units -> game units for the soft fades
	float cellRowW[3];	// the camera, world (WW_CELL_CAM_DUMP)
	for ( int k = 0; k < 3; k++ ) {
		float w = 0.0f;
		for ( int j = 0; j < 3; j++ )
			w -= vt.rotation( j, k ) * vt.translation[j];
		cellRowW[k] = w / sc;
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
	// lane AMBO2: the Ambient Only volumes, in plugin order (the shader takes the first that holds a point)
	const int ambo = ( dalc && !( s.red & 1024 ) ) ? std::min( int( L->ambientLights.size() ), kAmbientSlots ) : 0;
	prog->uni1i( "cellAmboCount", ambo );
	for ( int i = 0; i < ambo; i++ ) {
		const WwCellAmbientLight & a = L->ambientLights[i];
		prog->uni4f_l( prog->uniLocation( "cellAmbo[%d]", i ), FloatVector4( a.pos[0], a.pos[1], a.pos[2], a.volume ) );
		prog->uni4f_l( prog->uniLocation( "cellAmboK[%d]", i ), FloatVector4( a.k[0], a.k[1], a.k[2], 0.0f ) );
	}
	prog->uni1b( "cellHasDir", L->hasDirectional );
	prog->uni3f( "cellDirColor", L->dirColor[0], L->dirColor[1], L->dirColor[2] );
	prog->uni3f( "cellDirTo", L->dirTo[0], L->dirTo[1], L->dirTo[2] );
	prog->uni1b( "cellInterior", L->interior );
	prog->uni3f( "cellCenter", L->center[0], L->center[1], L->center[2] );
	prog->uni1i( "cellProbe", s.measuring ? 6 : s.probe );
	prog->uni1i( "cellRed", s.red );
	// WW_CELL_CAM_DUMP=<file>: the camera in world units, rewritten when it moves (the fog and diffuse gates measure from it)
	static const QString camDump = QString::fromLocal8Bit( qgetenv( "WW_CELL_CAM_DUMP" ) );
	static QString camLast;
	if ( !camDump.isEmpty() ) {
		const QString line = QStringLiteral( "cam=%1,%2,%3 fogprobe=%4 probe=%5\n" ).arg( double( cellRowW[0] ), 0, 'f', 2 )
			.arg( double( cellRowW[1] ), 0, 'f', 2 ).arg( double( cellRowW[2] ), 0, 'f', 2 ).arg( s.fogProbe ).arg( s.probe );
		QFile f( camDump );
		if ( line != camLast && f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
			f.write( line.toUtf8() );
			camLast = line;
		}
	}
	/* lane FOG2: an interior's fog in place of the Lookdev weather fog (whose uniforms ran just before): the
	 * same formula, the cell's packing, height = world z. No fog sun indoors (INFERRED: the game's directional
	 * fog term follows the sun, which an interior lacks). */
	if ( L->hasFog && prog->uniLocation( "fogOn" ) >= 0 ) {
		prog->uni1b( "fogOn", true );
		for ( int k = 0; k < 6; k++ )
			prog->uni4f_l( prog->uniLocation( "fogK[%d]", k ),
				FloatVector4( L->fogK[k][0], L->fogK[k][1], L->fogK[k][2], L->fogK[k][3] ) );
		float zr[4] = { 0, 0, 0, 0 };
		for ( int k = 0; k < 3; k++ ) {
			zr[k] = vt.rotation( k, 2 ) / sc;
			zr[3] -= vt.rotation( k, 2 ) * vt.translation[k] / sc;
		}
		prog->uni4f( "fogView", FloatVector4( zr[0], zr[1], zr[2], zr[3] ) );
		prog->uni1f( "fogDistScale", 1.0f / sc );
		prog->uni4f( "fogSun", FloatVector4( 0.0f, 0.0f, 1.0f, 0.0f ) );
		prog->uni4f( "fogSunColour", FloatVector4( 0.0f, 0.0f, 0.0f, 1.0f ) );
		prog->uni4f( "fogProbe", FloatVector4( 0.0f, 0.0f, float( s.fogProbe ), 0.0f ) );
		prog->uni1i( "fogRed", 0 );
	}
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

bool wwCellImageSpaceMeasuresEffects()
{
	return !( st().isRed & 16 );
}

int wwCellFxRed()
{
	static const int red = [] {
		int r = 0;
		for ( const QByteArray & t : qgetenv( "WW_CELL_FX_RED" ).split( ',' ) )
			r |= t == "legacy" ? 1 : t == "nosoft" ? 2 : t == "nolin" ? 4 : t == "hide" ? 8 : 0;
		return r;
	}();
	return red;
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

int wwCellCubeTag( const QString & material )
{
	// lane CUBE1: WW_CELL_CUBE_DUMP=<file> numbers each env-mapped material (1..255) and writes "tag|material"
	static const QString dump = QString::fromLocal8Bit( qgetenv( "WW_CELL_CUBE_DUMP" ) );
	static QHash<QString, int> tags;
	if ( dump.isEmpty() || material.isEmpty() )
		return 0;
	const QString key = material.toLower().replace( QLatin1Char( '\\' ), QLatin1Char( '/' ) );
	auto it = tags.constFind( key );
	if ( it != tags.constEnd() )
		return it.value();
	if ( tags.size() >= 255 )
		return 0;
	const int tag = int( tags.size() ) + 1;
	tags.insert( key, tag );
	QFile f( dump );
	if ( f.open( QIODevice::WriteOnly | ( tag == 1 ? QIODevice::Truncate : QIODevice::Append ) ) )
		f.write( QStringLiteral( "cubetag=%1|%2\n" ).arg( tag ).arg( key ).toUtf8() );
	return tag;
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
	return o + QLatin1Char( ' ' ) + wwCellShadowEcho( scene );
}

//! WW_CELL_SHADOW_DUMP=<file>: the echo, rewritten whenever the slots change (the gate's light list)
static void shadowDump( Scene * scene )
{
	static const QString path = QString::fromLocal8Bit( qgetenv( "WW_CELL_SHADOW_DUMP" ) );
	if ( path.isEmpty() )
		return;
	QFile f( path );
	if ( f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		f.write( ( wwCellShadowEcho( scene ) + QLatin1Char( '\n' ) ).toUtf8() );
}

/* lane SHADOW1: the depth cubes (celllights.h). Each face: world relative to the light, GL's cube face
 * axes (major m, s = u.r, t = v.r over |m.r|), 90 degrees; depth = distance / radius (the fragment stage). */
void wwCellShadowPass( Scene * scene )
{
	if ( !wwCellLightsWanted( scene ) )
		return;
	ClState & s = st();
	const WwCellLighting * L = wwCellLightsFor( scene->nifModel );
	if ( !L )
		return;
	Renderer * r = scene->renderer;
	Gpu & g = gpus()[r];
	const int ver = s.version.value( scene->nifModel );
	if ( g.shDoc != scene->nifModel || g.shVersion != ver ) {
		std::fill( g.shSlot, g.shSlot + kShadowSlots, -1 );
		g.shDoc = scene->nifModel;
		g.shVersion = ver;
		g.shStamp++;
	}
	if ( !s.shadowOn ) {
		s.shadowLast = QStringLiteral( "off(WW_CELL_SHADOW=0)" );
		return;
	}
	// the camera in the world (the view's inverse, as cellRow)
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	float cam[3];
	for ( int k = 0; k < 3; k++ ) {
		float w = 0.0f;
		for ( int j = 0; j < 3; j++ )
			w -= vt.rotation( j, k ) * vt.translation[j];
		cam[k] = w / sc;
	}
	// the wanted set: the shadow lights whose reach comes nearest the camera
	std::vector<std::pair<float, int>> cand;
	for ( qsizetype i = 0; i < L->lights.size(); i++ ) {
		const WwCellLight & l = L->lights.at( i );
		if ( !l.shadow )
			continue;
		const float dx = l.pos[0] - cam[0], dy = l.pos[1] - cam[1], dz = l.pos[2] - cam[2];
		cand.emplace_back( std::max( 0.0f, std::sqrt( dx * dx + dy * dy + dz * dz ) - l.radius ), int( i ) );
	}
	const int want = std::min( int( cand.size() ), kShadowSlots );
	std::partial_sort( cand.begin(), cand.begin() + want, cand.end() );
	std::vector<char> wanted( size_t( L->lights.size() ), 0 ), held( size_t( L->lights.size() ), 0 );
	for ( int i = 0; i < want; i++ )
		wanted[size_t( cand[size_t( i )].second )] = 1;
	bool changed = false;
	for ( int k = 0; k < kShadowSlots; k++ ) {
		const int i = g.shSlot[k];
		if ( i < 0 )
			continue;
		if ( i >= L->lights.size() || !wanted[size_t( i )] ) {
			g.shSlot[k] = -1;
			changed = true;
		} else {
			held[size_t( i )] = 1;
		}
	}
	std::vector<int> dirty;
	for ( int i = 0; i < want; i++ ) {
		const int li = cand[size_t( i )].second;
		if ( held[size_t( li )] )
			continue;
		for ( int k = 0; k < kShadowSlots; k++ ) {
			if ( g.shSlot[k] < 0 ) {
				g.shSlot[k] = li;
				dirty.push_back( k );
				break;
			}
		}
	}
	if ( changed || !dirty.empty() )
		g.shStamp++;
	if ( dirty.empty() ) {
		if ( changed )
			shadowDump( scene );
		return;
	}

	QElapsedTimer timer;
	timer.start();
	auto fn = r->fn;
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	if ( !g.shTex ) {
		fn->glGenTextures( 1, &g.shTex );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kShadowUnit ) );
		fn->glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, g.shTex );
		fn->glTexImage3D( GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT16, kShadowFace, kShadowFace, 6 * kShadowSlots, 0,
			GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
		fn->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );
		fn->glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, 0 );
		fn->glActiveTexture( GLenum( prevActive ) );
		fn->glGenFramebuffers( 1, &g.shFbo );
	}

	// the casters: what the sun's map takes (opaque, depth-writing, no effect), less the alpha-tested
	struct ShCaster
	{
		const Shape * sh;
		Vector3 c;
		float rad;
		Matrix4 mv;
	};
	std::vector<ShCaster> casters;
	for ( Node * node : scene->nodes.list() ) {
		const Shape * sh = dynamic_cast<const Shape *>( node );
		if ( !sh || !sh->isVisible() || !sh->wwCastsSunShadow() || sh->wwAlphaTested() )
			continue;
		if ( sh->verts.isEmpty() || sh->triangles.isEmpty() )
			continue;
		const BoundSphere b = sh->bounds();
		casters.push_back( { sh, b.center, b.radius, sh->viewTrans().toMatrix4() } );
	}

	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_shadowdepth.prog" );
	if ( !prog ) {
		for ( int k : dirty )
			g.shSlot[k] = -1;
		g.shStamp++;
		s.shadowLast = QStringLiteral( "refused(no cell_shadowdepth.prog)" );
		return;
	}
	// save what the pass touches
	GLint prevRead = 0, prevDraw = 0, vp[4], polyMode[2], depthFunc = 0;
	GLboolean depthMask = GL_TRUE;
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_POLYGON_MODE, polyMode );
	fn->glGetIntegerv( GL_DEPTH_FUNC, &depthFunc );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasOffset = fn->glIsEnabled( GL_POLYGON_OFFSET_FILL );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	const bool wasBlend = fn->glIsEnabled( GL_BLEND );

	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.shFbo );
	fn->glDrawBuffer( GL_NONE );
	fn->glReadBuffer( GL_NONE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );			// both sides cast (a cube face mirrors the winding anyway)
	fn->glDisable( GL_POLYGON_OFFSET_FILL );	// the depth is written by hand: the receiver biases
	fn->glEnable( GL_DEPTH_TEST );
	fn->glDepthFunc( GL_LEQUAL );
	fn->glDepthMask( GL_TRUE );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	fn->glViewport( 0, 0, kShadowFace, kShadowFace );

	// GL's cube faces: major axis m, s along u, t along v (the spec's sc / tc table)
	static const float M[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	static const float U[6][3] = { { 0, 0, -1 }, { 0, 0, 1 }, { 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { -1, 0, 0 } };
	static const float V[6][3] = { { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 } };
	const int lRel = prog->uniLocation( "shRelFromView" ), lClip = prog->uniLocation( "shClipFromRel" );
	const int lRn = prog->uniLocation( "shRadiusNear" );
	bool complete = true;
	qint64 drawn = 0;
	for ( int k : dirty ) {
		if ( !complete )
			break;
		const WwCellLight & l = L->lights.at( g.shSlot[k] );
		float rel[16];
		for ( int row = 0; row < 3; row++ ) {
			for ( int j = 0; j < 3; j++ )
				rel[j * 4 + row] = vt.rotation( j, row ) / sc;
			rel[12 + row] = cam[row] - l.pos[row];
		}
		rel[3] = rel[7] = rel[11] = 0.0f;
		rel[15] = 1.0f;
		if ( lRel >= 0 )
			fn->glUniformMatrix4fv( lRel, 1, GL_FALSE, rel );
		if ( lRn >= 0 )
			fn->glUniform2f( lRn, l.radius, l.nearClip );
		const float n = 1.0f, f = l.radius * 1.01f + 2.0f;
		const float A = ( f + n ) / ( f - n ), B = -2.0f * f * n / ( f - n );
		std::vector<const ShCaster *> mine;
		for ( const ShCaster & c : casters ) {
			const float dx = c.c[0] - l.pos[0], dy = c.c[1] - l.pos[1], dz = c.c[2] - l.pos[2];
			const float reach = c.rad + l.radius;
			if ( dx * dx + dy * dy + dz * dz < reach * reach )
				mine.push_back( &c );
		}
		for ( int face = 0; face < 6; face++ ) {
			fn->glFramebufferTextureLayer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, g.shTex, 0, 6 * k + face );
			if ( fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
				complete = false;
				break;
			}
			fn->glClearDepth( 1.0 );
			fn->glClear( GL_DEPTH_BUFFER_BIT );
			float clip[16];
			for ( int c = 0; c < 3; c++ ) {
				clip[c * 4 + 0] = U[face][c];
				clip[c * 4 + 1] = V[face][c];
				clip[c * 4 + 2] = A * M[face][c];
				clip[c * 4 + 3] = M[face][c];
			}
			clip[12] = clip[13] = clip[15] = 0.0f;
			clip[14] = B;
			if ( lClip >= 0 )
				fn->glUniformMatrix4fv( lClip, 1, GL_FALSE, clip );
			for ( const ShCaster * c : mine ) {
				prog->uni4m( "modelViewMatrix", c->mv );
				const float * attrs = &( c->sh->verts.constFirst()[0] );
				r->drawShape( (unsigned int) ( c->sh->verts.size() ), 3, (unsigned int) ( c->sh->triangles.size() * 3 ),
					GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, c->sh->triangles.constData() );
				drawn++;
			}
		}
	}
	r->stopProgram();

	// restore
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GLenum( polyMode[0] ) );
	fn->glDepthFunc( GLenum( depthFunc ) );
	fn->glDepthMask( depthMask );
	wasCull ? fn->glEnable( GL_CULL_FACE ) : fn->glDisable( GL_CULL_FACE );
	wasOffset ? fn->glEnable( GL_POLYGON_OFFSET_FILL ) : fn->glDisable( GL_POLYGON_OFFSET_FILL );
	wasDepth ? fn->glEnable( GL_DEPTH_TEST ) : fn->glDisable( GL_DEPTH_TEST );
	wasScissor ? fn->glEnable( GL_SCISSOR_TEST ) : fn->glDisable( GL_SCISSOR_TEST );
	wasBlend ? fn->glEnable( GL_BLEND ) : fn->glDisable( GL_BLEND );

	if ( !complete ) {
		for ( int k : dirty )
			g.shSlot[k] = -1;
		g.shStamp++;
		s.shadowLast = QStringLiteral( "refused(shadow framebuffer incomplete)" );
		return;
	}
	s.shadowLast = QStringLiteral( "on shadowLights=%1 rendered=%2 casters=%3 draws=%4 ms=%5 face=%6" )
		.arg( cand.size() ).arg( dirty.size() ).arg( casters.size() ).arg( drawn ).arg( timer.elapsed() ).arg( kShadowFace );
	shadowDump( scene );
}

QString wwCellShadowEcho( Scene * scene )
{
	ClState & s = st();
	QString o = QStringLiteral( "shadow=%1" ).arg( s.shadowLast );
	if ( s.shadowRed )
		o += QStringLiteral( " red=noshadow" );
	if ( !scene || !scene->renderer || !scene->nifModel )
		return o;
	const WwCellLighting * L = wwCellLightsFor( scene->nifModel );
	const Gpu & g = gpus()[scene->renderer];
	if ( !L || g.shDoc != scene->nifModel )
		return o;
	int used = 0;
	for ( int k = 0; k < kShadowSlots; k++ )
		used += g.shSlot[k] >= 0 ? 1 : 0;
	o += QStringLiteral( " slots=%1/%2" ).arg( used ).arg( kShadowSlots );
	// the gate's three: position, radius, kind, near clip, direction (a hemisphere's plane)
	for ( int k = 0; k < 3; k++ ) {
		const int i = g.shSlot[k];
		if ( i < 0 || i >= L->lights.size() )
			continue;
		const WwCellLight & l = L->lights.at( i );
		o += QStringLiteral( " slot%1=%2,%3,%4,%5,%6,%7,%8,%9,%10" ).arg( k )
			.arg( double( l.pos[0] ), 0, 'f', 3 ).arg( double( l.pos[1] ), 0, 'f', 3 ).arg( double( l.pos[2] ), 0, 'f', 3 )
			.arg( double( l.radius ), 0, 'f', 3 ).arg( l.shadow ).arg( double( l.nearClip ), 0, 'f', 3 )
			.arg( double( l.dir[0] ), 0, 'f', 5 ).arg( double( l.dir[1] ), 0, 'f', 5 ).arg( double( l.dir[2] ), 0, 'f', 5 );
	}
	return o;
}

/* ---- lane AO1: the ambient obscurance (celllights.h, res/shaders/cell_ao.frag) ---- */
namespace
{

constexpr int kAoUnit = 16;			// the obscurance (sampler2D, half the view); 9 on a GPU with only 16 units
constexpr int kAoAngles = 8;		// a still view's mean over the game's per-frame angle in [0, pi)
constexpr float kAoRadius = 108.2f, kAoBias = 0.6f, kAoIntensity = 7.1f;	// the game's INI defaults, game units

struct AoState
{
	bool loaded = false;
	bool on = true;			// WW_CELL_AO=0: none computed
	int red = 0;			// 1 off (computed, not applied), 2 radius (halved), 4 noblur, 8 noreset
	QString dump;			// WW_CELL_AO_DUMP
	bool pass = false;		// the opaque pass is drawing
	QString last = QStringLiteral( "none yet" );
};

AoState & ao()
{
	static AoState a;
	if ( !a.loaded ) {
		a.loaded = true;
		a.on = qgetenv( "WW_CELL_AO" ).trimmed() != "0";
		const QByteArray red = qgetenv( "WW_CELL_AO_RED" ).trimmed();
		a.red = red == "off" ? 1 : red == "radius" ? 2 : red == "noblur" ? 4 : red == "noreset" ? 8 : 0;
		a.dump = QString::fromLocal8Bit( qgetenv( "WW_CELL_AO_DUMP" ) );
	}
	return a;
}

struct AoTarget
{
	GLuint tex = 0, fbo = 0;
	int w = 0, h = 0;
};

struct AoGpu
{
	AoTarget gbuf;			// full size: view normal + linear depth (RGBA32F), with a depth-stencil buffer
	GLuint gbufDepth = 0;
	AoTarget mip[5];		// 1..4: the min-of-2x2 depth (R32F)
	AoTarget raw, across, fin;	// half size: (A, key) RG32F, then the blurs; fin R32F, bilinear
	const void * doc = nullptr;
	bool ready = false;
	int unit = -1;
};

QHash<const void *, AoGpu> & aoGpus()
{
	static QHash<const void *, AoGpu> g;
	return g;
}

void aoAlloc( NifSkopeOpenGLContext::GLFunctions * fn, AoTarget & t, int w, int h, GLenum ifmt, GLenum fmt, bool linear )
{
	if ( t.tex && t.w == w && t.h == h )
		return;
	if ( !t.tex ) {
		fn->glGenTextures( 1, &t.tex );
		fn->glGenFramebuffers( 1, &t.fbo );
	}
	fn->glBindTexture( GL_TEXTURE_2D, t.tex );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GLint( ifmt ), w, h, 0, fmt, GL_FLOAT, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, t.fbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0 );
	t.w = w;
	t.h = h;
}

bool aoIsCellProgram( const NifSkopeOpenGLContext::Program * p )
{
	return p && ( p->name == std::string_view( "fo4_cell.prog" ) || p->name == std::string_view( "pbrm_cell.prog" ) );
}

}	// namespace

void wwCellAoPass( Scene * scene, bool run )
{
	if ( !scene || !scene->renderer )
		return;
	AoState & a = ao();
	Renderer * r = scene->renderer;
	AoGpu & g = aoGpus()[r];
	g.ready = false;
	if ( !run || !a.on || !wwCellLightsWanted( scene ) )
		return;
	QElapsedTimer timer;
	timer.start();
	auto fn = r->fn;
	if ( g.unit < 0 ) {
		GLint units = 0;
		fn->glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &units );
		g.unit = units > kAoUnit ? kAoUnit : 9;
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
	const int W = std::max( vp[2], 1 ), H = std::max( vp[3], 1 );
	const int hw = ( W + 1 ) / 2, hh = ( H + 1 ) / 2;
	int ms[5][2] = { { W, H } };
	for ( int m = 1; m < 5; m++ ) {
		ms[m][0] = std::max( ms[m - 1][0] / 2, 1 );
		ms[m][1] = std::max( ms[m - 1][1] / 2, 1 );
	}

	// the targets
	aoAlloc( fn, g.gbuf, W, H, GL_RGBA32F, GL_RGBA, false );
	if ( !g.gbufDepth )
		fn->glGenRenderbuffers( 1, &g.gbufDepth );
	fn->glBindRenderbuffer( GL_RENDERBUFFER, g.gbufDepth );
	GLint rbw = 0, rbh = 0;
	fn->glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &rbw );
	fn->glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &rbh );
	if ( rbw != W || rbh != H )
		fn->glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, W, H );
	fn->glBindRenderbuffer( GL_RENDERBUFFER, 0 );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.gbuf.fbo );
	fn->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g.gbufDepth );
	for ( int m = 1; m < 5; m++ )
		aoAlloc( fn, g.mip[m], ms[m][0], ms[m][1], GL_R32F, GL_RED, false );
	aoAlloc( fn, g.raw, hw, hh, GL_RG32F, GL_RG, false );
	aoAlloc( fn, g.across, hw, hh, GL_RG32F, GL_RG, false );
	aoAlloc( fn, g.fin, hw, hh, GL_R32F, GL_RED, true );

	// 1. the opaque cell-lit fragments: the view normal and the linear depth (probe 20); the background far away
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.gbuf.fbo );
	const bool complete = fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
	NifSkopeOpenGLContext::Program * prog = complete ? r->useProgram( "cell_ao.prog" ) : nullptr;
	if ( prog )
		r->stopProgram();
	if ( !prog ) {
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevFbo ) );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
		a.last = complete ? QStringLiteral( "refused(no cell_ao.prog)" ) : QStringLiteral( "refused(target incomplete)" );
		return;
	}
	fn->glViewport( 0, 0, W, H );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glDepthMask( GL_TRUE );
	const GLfloat far4[4] = { 0.0f, 0.0f, 0.0f, 1.0e6f };
	fn->glClearBufferfv( GL_COLOR, 0, far4 );
	fn->glClear( GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
	fn->glDisable( GL_BLEND );
	fn->glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );	// wwCellAoDraw opens it for the opaque cell-lit draws
	a.pass = true;
	{
		NodeList second;
		scene->collectShapes( second );
		Scene::drawDeferredShapes( second );
		scene->drawShapeEffects();
	}
	a.pass = false;
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glDepthMask( GL_TRUE );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDisable( GL_POLYGON_OFFSET_FILL );

	// 2. the full-screen passes (cell_ao.frag)
	prog = r->useProgram( "cell_ao.prog" );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_BLEND );
	fn->glDepthMask( GL_FALSE );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	const AoTarget * bound[6] = { &g.gbuf, &g.mip[1], &g.mip[2], &g.mip[3], &g.mip[4], nullptr };
	for ( int u = 0; u < 5; u++ ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, bound[u]->tex );
	}
	prog->uni1i( "gbuf", 0 );
	prog->uni1i( "zMip1", 1 );
	prog->uni1i( "zMip2", 2 );
	prog->uni1i( "zMip3", 3 );
	prog->uni1i( "zMip4", 4 );
	prog->uni1i( "src", 5 );
	fn->glUniform2i( prog->uniLocation( "fullSize" ), W, H );
	fn->glUniform2i( prog->uniLocation( "halfSize" ), hw, hh );
	for ( int m = 0; m < 5; m++ )
		fn->glUniform2i( prog->uniLocation( "mipSize[%d]", m ), ms[m][0], ms[m][1] );
	const auto & pm = r->globalUniforms->projectionMatrix;
	const float p00 = pm[0][0], p11 = pm[1][1];
	const float radius = kAoRadius * ( ( a.red & 2 ) ? 0.5f : 1.0f );
	prog->uni2f( "proj", p00, p11 );
	prog->uni3f( "aoParams", radius, kAoBias, kAoIntensity );
	prog->uni1i( "aoAngles", kAoAngles );
	prog->uni1b( "aoNoBlur", ( a.red & 4 ) != 0 );
	prog->uni1b( "aoNoReset", ( a.red & 8 ) != 0 );
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	auto pass = [&]( int stage, const AoTarget & out, const AoTarget * in ) {
		fn->glActiveTexture( GL_TEXTURE5 );
		fn->glBindTexture( GL_TEXTURE_2D, in ? in->tex : 0 );
		prog->uni1i( "aoStage", stage );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, out.fbo );
		fn->glViewport( 0, 0, out.w, out.h );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	};
	for ( int m = 1; m < 5; m++ ) {
		prog->uni1i( "mipLevel", m );
		pass( 1, g.mip[m], m > 1 ? &g.mip[m - 1] : nullptr );
	}
	pass( 2, g.raw, nullptr );
	pass( 3, g.across, &g.raw );
	pass( 4, g.fin, &g.across );
	r->stopProgram();
	for ( int u = 0; u < 6; u++ ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
	}
	fn->glActiveTexture( GLenum( prevActive ) );

	g.ready = true;
	g.doc = scene->nifModel;
	a.last = QStringLiteral( "on %1x%2 half %3x%4 radius %5 bias %6 intensity %7 angles %8 p00 %9 p11 %10 p20 %11 p21 %12 "
		"unit %13 red %14 ms %15" ).arg( W ).arg( H ).arg( hw ).arg( hh ).arg( double( radius ), 0, 'f', 2 )
		.arg( double( kAoBias ), 0, 'f', 2 ).arg( double( kAoIntensity ), 0, 'f', 2 ).arg( kAoAngles )
		.arg( double( p00 ), 0, 'f', 6 ).arg( double( p11 ), 0, 'f', 6 ).arg( double( pm[2][0] ), 0, 'f', 6 )
		.arg( double( pm[2][1] ), 0, 'f', 6 ).arg( g.unit ).arg( a.red ).arg( timer.elapsed() );

	// WW_CELL_AO_DUMP=<file>: int32 W H hw hh, the opaque pass (W x H RGBA float, bottom row first), the raw
	// (A, key) and the final obscurance (hw x hh, top row first); <file>.txt the numbers
	if ( !a.dump.isEmpty() ) {
		std::vector<float> gb( size_t( W ) * size_t( H ) * 4 ), raw( size_t( hw ) * size_t( hh ) * 2 ),
			fin( size_t( hw ) * size_t( hh ) );
		fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.gbuf.fbo );
		fn->glReadPixels( 0, 0, W, H, GL_RGBA, GL_FLOAT, gb.data() );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.raw.fbo );
		fn->glReadPixels( 0, 0, hw, hh, GL_RG, GL_FLOAT, raw.data() );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.fin.fbo );
		fn->glReadPixels( 0, 0, hw, hh, GL_RED, GL_FLOAT, fin.data() );
		double mean = 0.0;
		for ( float v : fin )
			mean += double( v );
		mean /= double( std::max<size_t>( fin.size(), 1 ) );
		a.last += QStringLiteral( " mean %1" ).arg( mean, 0, 'f', 4 );
		QFile f( a.dump );
		if ( f.open( QIODevice::WriteOnly ) ) {
			const qint32 hd[4] = { W, H, hw, hh };
			f.write( reinterpret_cast<const char *>( hd ), sizeof( hd ) );
			f.write( reinterpret_cast<const char *>( gb.data() ), qint64( gb.size() * sizeof( float ) ) );
			f.write( reinterpret_cast<const char *>( raw.data() ), qint64( raw.size() * sizeof( float ) ) );
			f.write( reinterpret_cast<const char *>( fin.data() ), qint64( fin.size() * sizeof( float ) ) );
		}
		QFile t( a.dump + QStringLiteral( ".txt" ) );
		if ( t.open( QIODevice::WriteOnly | QIODevice::Text ) )
			t.write( ( QStringLiteral( "ao: " ) + a.last + QStringLiteral( "\n" ) ).toUtf8() );
	}

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
}

void wwCellAoDraw( Scene * scene, bool cellProgram )
{
	if ( !scene || !scene->renderer )
		return;
	AoState & a = ao();
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	cellProgram = cellProgram && aoIsCellProgram( prog );
	if ( a.pass ) {
		// the game's opaque (deferred) pass: no blended draw, no effect, nothing that is not cell-lit
		const bool opaque = cellProgram && !glIsEnabled( GL_BLEND );
		glColorMask( opaque, opaque, opaque, opaque );
		if ( !opaque )
			glDepthMask( GL_FALSE );
		if ( cellProgram ) {
			prog->uni1i( "cellProbe", 20 );
			prog->uni1b( "cellAoOn", false );
		}
		return;
	}
	if ( !cellProgram || prog->uniLocation( "cellAoOn" ) < 0 )
		return;
	AoGpu & g = aoGpus()[r];
	const bool on = g.ready && g.doc == scene->nifModel && g.unit >= 0 && !( a.red & 1 ) && !glIsEnabled( GL_BLEND )
		&& wwCellLightsWanted( scene );
	// bound whether or not this draw reads it (a sampler left on unit 0 would sit beside BaseMap)
	GLint prevActive = 0;
	r->fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + std::max( g.unit, 0 ) ) );
	r->fn->glBindTexture( GL_TEXTURE_2D, on ? g.fin.tex : 0 );
	r->fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni1i( "cellAo", std::max( g.unit, 0 ) );
	prog->uni1b( "cellAoOn", on );
	if ( on ) {
		GLint vp[4] = { 0, 0, 1, 1 };
		glGetIntegerv( GL_VIEWPORT, vp );
		prog->uni4f_l( prog->uniLocation( "cellAoRect" ), FloatVector4( float( vp[0] ), float( vp[1] ),
			1.0f / float( std::max( vp[2], 1 ) ), 1.0f / float( std::max( vp[3], 1 ) ) ) );
	}
}
