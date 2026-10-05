/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "celllights.h"

#include "gl/cellssr.h"
#include "gl/cellvolfog.h"	// lane VOLFOG1
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"
#include "gl/cellhdr.h"
#include "gl/cellaodecalgl.h"	// lane AODECAL1
#include "gl/lookdevstage.h"	// lane SUNCELL1
#include "gl/cellwater.h"	// lane SUNCELL1: water casts no cube shadow
#include "gamemanager.h"	// lane SUNCELL1: the weather imagespace's LUT

#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{

constexpr int kTextureUnit = 14;		// TexCache allocates from unit 0 upward; 15 is the CSM map
constexpr int kTexelsPerLight = 8;		// lane SHADOW1 added the 5th: shadow slot, kind, near clip, XLIG bias;
										// lane HEMI1 the 6th to 8th: the box rows (cell_lights.glsl CELL_TPL)
constexpr int kGiUnit = 13;			// lane PRTPGI: the bounce grid (sampler3D)
constexpr int kGiSlotUnit = 18;		// lane ROOMCLAMP1: the grid's slot rooms, the fine rooms (sampler3D, R32F); off
constexpr int kRoomUnit = 19;		// on a GPU with fewer than 20 units (the blend before the lane)
constexpr int kFarSlotUnit = 20;	// lane FARVIEW1: the far table's slots (usamplerBuffer), records (samplerBuffer) and the
constexpr int kFarRecUnit = 21;		// GI grid's placed share (sampler3D); off on a GPU with fewer than 23 units
constexpr int kFarPlacedUnit = 22;
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
	bool farOn = false, farPinned = false;	// lane FARVIEW1
	int dotsLast = -1;	// lane FARVIEW1: the dots the last frame drew (-1 none tried)
	int farRed = 0;
	QHash<const void *, WwCellFar> far;
	QHash<const void *, int> farVersion;
	bool isOn = false, isPinned = false, measuring = false;
	int isRed = 0;          // 1 nolut, 2 noexp, 4 nograde, 8 nobloom, 16 nofx
	QHash<const void *, float> adapted;     // lane IMGS1: the last measure per document
	QHash<const void *, int> adaptedPixels;
	QHash<const void *, ClBloom> bloom;     // lane BLOOM1: the last measure's bloom per document
	int nextBloomVersion = 1;
	bool shadowOn = true;   // lane SHADOW1: WW_CELL_SHADOW=0 turns the maps off (the harness's unshadowed pass)
	bool shadowPinned = false;	// lane CELLALL1: the Light shadows row (WW/CellShadow); the pin wins
	bool shadowRed = false; // WW_CELL_SHADOW_RED=noshadow: the maps rendered, every factor read as 1
	QString shadowLast = QStringLiteral( "none yet" );
	int fogProbe = 0;       // lane FOG2: WW_CELL_FOG_PROBE=6 (alpha, height blend) | 7 (fog colour ^ 1/2.2) | 8 (its d, z), per fragment
	int pass = 0;           // lane PROBEVIEW1: the PRTP band's Pass (WwCellPass); WW_CELL_PASS pins it
	bool passPinned = false;
	int passRed = 0;        // WW_CELL_PV_RED: 1 direct, 2 nonormal (the shader), 4 open (every sky direction open)
	int giAmb = 0;          // lane GICAL1: WW_CELL_GI_AMB = keep (0) | replace | off | max | asgi (the measure's red)
	bool giFill = true;     // lane GICAL1: WW_CELL_GI_FILL=0 leaves the GI's gaps black (as before the lane)
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
		const QByteArray farPin = qgetenv( "WW_CELL_FAR" );	// lane FARVIEW1
		if ( !farPin.isEmpty() ) {
			s.farPinned = true;
			s.farOn = farPin.trimmed() != "0";
		} else {
			s.farOn = QSettings().value( QStringLiteral( "WW/CellFar" ), false ).toBool();
		}
		const QByteArray farRed = qgetenv( "WW_CELL_FAR_RED" ).trimmed();
		s.farRed = farRed == "nosurfel" ? 1 : farRed == "noblend" ? 2 : farRed == "flipside" ? 4 : 0;
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
		const QByteArray shPin = qgetenv( "WW_CELL_SHADOW" ).trimmed();
		s.shadowPinned = !shPin.isEmpty();
		s.shadowOn = s.shadowPinned ? shPin != "0" : QSettings().value( QStringLiteral( "WW/CellShadow" ), true ).toBool();
		s.shadowRed = qgetenv( "WW_CELL_SHADOW_RED" ).trimmed() == "noshadow";
		s.fogProbe = qEnvironmentVariableIntValue( "WW_CELL_FOG_PROBE" );
		const QByteArray giAmb = qgetenv( "WW_CELL_GI_AMB" ).trimmed();	// lane GICAL1
		s.giAmb = giAmb == "replace" ? 1 : giAmb == "off" ? 2 : giAmb == "max" ? 3 : giAmb == "asgi" ? 4 : 0;
		s.giFill = qgetenv( "WW_CELL_GI_FILL" ).trimmed() != "0";
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
		else if ( red == "hemiomni" )
			s.red = 256;	// lane HEMI1: hemisphere and box lights drawn as plain omni lights (the cell view applies it)
		else if ( red == "cubeold" )
			s.red = 512;	// lane CUBE1: the interior cube map at its old scale (squared, x Lambert, default cube)
		else if ( red == "ambientfull" )
			s.red = 1024;	// lane AMBO2: the Ambient Only lights' ambient adjustment ignored
		else if ( red == "probefx" )
			s.red = 4096;	// lane FRAT1: effects and refraction shapes drawn into the probe passes again
		// lane POOL1: the lights' Non Specular flag ignored (a bit clear of WW_CELL_LIT_RED's)
		if ( qgetenv( "WW_CELL_SPEC_RED" ).trimmed() == "nonspec" )
			s.red |= 65536;
		// lane PROBEVIEW1: the Pass, by number or by its entry's name
		const QByteArray passPin = qgetenv( "WW_CELL_PASS" ).trimmed();
		if ( !passPin.isEmpty() ) {
			s.passPinned = true;
			const QStringList names = wwCellPassNames();
			const int byName = names.indexOf( QString::fromLatin1( passPin ) );
			s.pass = byName >= 0 ? byName : passPin.toInt();
		} else {
			s.pass = QSettings().value( QStringLiteral( "WW/CellPass" ), 0 ).toInt();
		}
		if ( s.pass < 0 || s.pass >= int( wwCellPassNames().size() ) )
			s.pass = 0;
		for ( const QByteArray & r : qgetenv( "WW_CELL_PV_RED" ).split( ',' ) ) {
			const QByteArray t = r.trimmed();
			s.passRed |= t == "direct" ? 1 : t == "nonormal" ? 2 : t == "open" ? 4 : t == "noclamp" ? 8 : 0;   // lane ROOMCLAMP1: noclamp
		}
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
	GLuint slotTex = 0, roomTex = 0;	// lane ROOMCLAMP1: uploaded with the GI grid (the same document and version)
	const void * roomDoc = nullptr;
	int roomVersion = 0, units = -1;
	GLuint skyTex = 0;		// lane PROBEVIEW1: the sky grid, bound on the GI unit in the Sky visibility pass
	const void * skyDoc = nullptr;
	int skyVersion = 0;
	GLuint lutTex = 0;
	const void * lutDoc = nullptr;
	int lutVersion = 0;
	int lutStamp = -1;	// lane SUNCELL1: the exterior imagespace's stamp the LUT was uploaded at
	GLuint bloomTex = 0;
	GLuint farSlotBuf = 0, farSlotTex = 0, farRecBuf = 0, farRecTex = 0, farPlacedTex = 0;	// lane FARVIEW1
	const void * farDoc = nullptr;
	int farVersion = 0;
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

/* lane SUNCELL1: AN EXTERIOR'S IMAGESPACE IS THE WEATHER'S. An interior keeps its XCIM imagespace (lane IMGS1); an
 * exterior has none of its own, the game uses the weather's IMSP at the hour (the two colour keys, blended).
 * isLighting() hands the imagespace sites the document's lighting with those fields filled from the Lookdev
 * weather (wwLookdevImageSpace), re-filled when the weather or the hour's keys move. No Lookdev weather: the
 * document's own lighting, so the exterior draws as before (no imagespace). */
struct ExIs
{
	QString key;
	WwCellLighting L;
	int stamp = 0;
};

QHash<const void *, ExIs> & exIs()
{
	static QHash<const void *, ExIs> m;
	return m;
}

//! the 256x16 B8G8R8 strip -> 16^3 RGB8, r fastest (src/cellview.cpp's reader, lane IMGS1)
bool decodeLutStrip( const QByteArray & dds, std::vector<unsigned char> & out )
{
	auto u32 = [&]( int o ) { quint32 v = 0; std::memcpy( &v, dds.constData() + o, 4 ); return v; };
	if ( !( dds.size() >= 128 + 256 * 16 * 3 && dds.startsWith( "DDS " ) && u32( 12 ) == 16 && u32( 16 ) == 256
		&& ( u32( 80 ) & 0x40 ) && u32( 88 ) == 24 && u32( 92 ) == 0xff0000 ) )
		return false;
	const unsigned char * px = reinterpret_cast<const unsigned char *>( dds.constData() ) + 128;
	out.resize( 16 * 16 * 16 * 3 );
	for ( int b = 0; b < 16; b++ )
		for ( int g = 0; g < 16; g++ )
			for ( int r = 0; r < 16; r++ ) {
				const unsigned char * q = px + ( g * 256 + b * 16 + r ) * 3;
				unsigned char * o = &out[size_t( ( ( b * 16 + g ) * 16 + r ) * 3 )];
				o[0] = q[2];
				o[1] = q[1];
				o[2] = q[0];
			}
	return true;
}

bool loadLut( const QString & lut, const QString & dataRoot, std::vector<unsigned char> & out )
{
	if ( lut.isEmpty() )
		return false;
	QByteArray dds;
	bool got = Game::GameManager::get_file( dds, Game::FALLOUT_4, lut, "textures", ".dds" );
	if ( !got && !dataRoot.isEmpty() ) {
		QFile lf( dataRoot + QStringLiteral( "/Textures/" ) + QString( lut ).replace( QLatin1Char( '\\' ), QLatin1Char( '/' ) ) );
		got = lf.open( QIODevice::ReadOnly ) && !( dds = lf.readAll() ).isEmpty();
	}
	return got && decodeLutStrip( dds, out );
}

const WwCellLighting * isLighting( const void * nif, int * stamp = nullptr )
{
	if ( stamp )
		*stamp = 0;
	const WwCellLighting * L = nif ? wwCellLightsFor( nif ) : nullptr;
	if ( !L || L->interior || !wwLookdevActive() )
		return L;
	WwLookdevIs w;
	if ( !wwLookdevImageSpace( w ) )
		return L;
	ExIs & e = exIs()[nif];
	if ( e.key != w.key ) {
		e.key = w.key;
		e.L = *L;
		e.L.hasImageSpace = true;
		e.L.isName = QStringLiteral( "weather:" ) + w.name;
		std::copy( w.hdr, w.hdr + 9, e.L.isHdr );
		std::copy( w.cine, w.cine + 3, e.L.isCine );
		std::copy( w.tint, w.tint + 4, e.L.isTint );
		e.L.isLut.clear();
		e.L.isLutPath.clear();
		std::vector<unsigned char> a, b;
		const bool gotA = loadLut( w.lutA, L->dataRoot, a );
		const bool gotB = w.t > 0.0f && loadLut( w.lutB, L->dataRoot, b );
		if ( gotA && gotB ) {	// the two keys' LUTs blended: the same as blending their outputs (linear sampling)
			e.L.isLut.resize( a.size() );
			for ( size_t i = 0; i < a.size(); i++ )
				e.L.isLut[i] = (unsigned char) std::lround( float( a[i] ) + ( float( b[i] ) - float( a[i] ) ) * w.t );
			e.L.isLutPath = w.lutA + QStringLiteral( "->" ) + w.lutB;
		} else if ( gotA ) {
			e.L.isLut = a;
			e.L.isLutPath = w.lutA;
		}
		e.stamp++;
		std::fprintf( stderr, "cell imagespace: exterior=%s hdr=%g,%g,%g,%g,%g,%g,%g,%g,%g lut=%s%s\n",
			e.L.isName.toLocal8Bit().constData(), double( w.hdr[0] ), double( w.hdr[1] ), double( w.hdr[2] ),
			double( w.hdr[3] ), double( w.hdr[4] ), double( w.hdr[5] ), double( w.hdr[6] ), double( w.hdr[7] ),
			double( w.hdr[8] ), e.L.isLutPath.isEmpty() ? "none" : e.L.isLutPath.toLocal8Bit().constData(),
			( !w.lutA.isEmpty() && !gotA ) ? " (LUT NOT FOUND)" : "" );
	}
	if ( stamp )
		*stamp = e.stamp;
	return &e.L;
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

void wwCellFarPublish( const void * nif, const WwCellFar & far )
{
	ClState & s = st();
	s.far.insert( nif, far );
	s.farVersion.insert( nif, s.nextVersion++ );
}

const WwCellFar * wwCellFarFor( const void * nif )
{
	ClState & s = st();
	auto it = s.far.constFind( nif );
	return it == s.far.constEnd() ? nullptr : &*it;
}

bool wwCellFarOn()
{
	return st().farOn;
}

void wwCellFarSetOn( bool on )
{
	ClState & s = st();
	if ( s.farPinned )
		return;
	s.farOn = on;
	QSettings().setValue( QStringLiteral( "WW/CellFar" ), on );
}

int wwCellFarRed()
{
	return st().farRed;
}

/* lane FARVIEW1: the bulb dots. A bulb's intensity I = Le x area / 4 (farLightDots); seen from d it covers
 * (area / 4) / d^2 sr, a pixel (1 / f)^2 sr, so the frame value it adds, spread over a normalised Gaussian, is
 * I (f / d)^2 G / (2 pi sigma^2): the summed dot equals the bulb's own emissive the near view draws, at any size.
 * The hardware depth test (the frame's depth, not written) hides a dot behind a nearer surface; the stencil marks
 * its pixels as linear light (tone-mapped once, as the cell programs' are). */
int wwCellFarDotsDraw( Scene * scene )
{
	ClState & s = st();
	if ( !s.farOn || !scene || !wwCellLightsWanted( scene ) || !wwCellHdrActive() )
		return 0;
	const WwCellFar * Fr = wwCellFarFor( scene->nifModel );
	if ( !Fr || Fr->dots.size() < 8 )
		return 0;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_fardots.prog" );
	if ( !prog ) {
		s.dotsLast = 0;
		return 0;
	}
	auto fn = r->fn;
	const size_t n = Fr->dots.size() / 8;
	std::vector<float> pos( n * 3 ), I( n * 3 );
	std::vector<quint32> idx( n );
	for ( size_t i = 0; i < n; i++ ) {
		for ( int k = 0; k < 3; k++ ) {
			pos[i * 3 + k] = Fr->dots[i * 8 + k];
			I[i * 3 + k] = Fr->dots[i * 8 + 4 + k];
		}
		idx[i] = quint32( i );
	}
	// world -> view: posView = sc R world + t (the inverse of cellRow)
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	float cam[3];
	for ( int j = 0; j < 3; j++ ) {
		prog->uni4f_l( prog->uniLocation( "dotRow[%d]", j ), FloatVector4( vt.rotation( j, 0 ) * sc, vt.rotation( j, 1 ) * sc,
			vt.rotation( j, 2 ) * sc, vt.translation[j] ) );
		float w = 0.0f;
		for ( int k = 0; k < 3; k++ )
			w -= vt.rotation( k, j ) * vt.translation[k];
		cam[j] = w / sc;
	}
	const auto & pm = r->globalUniforms->projectionMatrix;
	fn->glUniformMatrix4fv( prog->uniLocation( "dotProj" ), 1, GL_FALSE, &pm[0][0] );
	GLint vp[4] = { 0, 0, 1, 1 };
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	prog->uni3f( "dotCam", cam[0], cam[1], cam[2] );
	prog->uni2f( "dotBand", Fr->band[0], Fr->band[1] );
	prog->uni1f( "dotFocal", pm[1][1] * 0.5f * float( vp[3] ) );
	prog->uni1f( "dotBulb", 4.0f );	// a bulb's radius, game units (a household bulb is ~3-5)
	prog->uni1i( "dotRed", 0 );
	GLboolean depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasBlend = fn->glIsEnabled( GL_BLEND ), wasCull = fn->glIsEnabled( GL_CULL_FACE );
	fn->glEnable( GL_PROGRAM_POINT_SIZE );
	fn->glEnable( GL_DEPTH_TEST );
	fn->glDepthFunc( GL_LEQUAL );
	fn->glDepthMask( GL_FALSE );
	fn->glDisable( GL_CULL_FACE );
	fn->glEnable( GL_BLEND );
	fn->glBlendFunc( GL_ONE, GL_ONE );
	fn->glEnable( GL_STENCIL_TEST );
	fn->glStencilMask( 0x03 );
	fn->glStencilFunc( GL_ALWAYS, 1, 0xFF );
	fn->glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
	const float * attrs[2] = { pos.data(), I.data() };
	r->drawShape( unsigned( n ), 0x33, unsigned( n ), GL_POINTS, GL_UNSIGNED_INT, attrs, idx.data() );
	r->stopProgram();
	fn->glDisable( GL_PROGRAM_POINT_SIZE );
	fn->glDepthMask( depthMask );
	if ( !wasBlend )
		fn->glDisable( GL_BLEND );
	if ( wasCull )
		fn->glEnable( GL_CULL_FACE );
	s.dotsLast = int( n );
	return int( n );
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
	// lane PROBEVIEW1: a Pass other than Combined draws through the cell program with the Cell lights row off too
	if ( !( st().on || wwCellPassFor( scene->nifModel ) > 0 ) || !wwCellLightsFor( scene->nifModel ) )
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
	if ( s.red & 4096 )
		return false;	// lane FRAT1: WW_CELL_LIT_RED=probefx, the effects write over the probes (as before lane EFX1)
	return ( s.probe > 0 || s.fogProbe > 0 || ( scene && wwCellPassFor( scene->nifModel ) > 0 ) ) && wwCellLightsWanted( scene );
}

bool wwCellAlbedoProbePass( Scene * scene )
{
	return st().probe == 80 && wwCellProbePass( scene );
}

int wwCellPassFor( const void * nif )
{
	const int p = st().pass;
	return p > 0 && nif && wwCellGiFor( nif ) ? p : 0;
}

QStringList wwCellPassNames()
{
	return { QStringLiteral( "Combined" ), QStringLiteral( "GI" ), QStringLiteral( "Sky visibility" ),
		QStringLiteral( "Surfel color" ), QStringLiteral( "Surfel light" ) };
}

int wwCellPass()
{
	return st().pass;
}

void wwCellSetPass( int pass )
{
	ClState & s = st();
	if ( s.passPinned || pass < 0 || pass >= int( wwCellPassNames().size() ) )
		return;
	s.pass = pass;
	QSettings().setValue( QStringLiteral( "WW/CellPass" ), pass );
}

int wwCellPassRed()
{
	return st().passRed;
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
			// lane HEMI1: texel 1.w = cos(FOV / 2) for a spot, else the shape: -2 omni, -3 hemisphere, -4 box
			const float shape = l.spot ? l.cosOuter : l.shape == 1 ? -3.0f : l.shape == 2 ? -4.0f : -2.0f;
			const float tex[kTexelsPerLight * 4] = {
				l.pos[0], l.pos[1], l.pos[2], l.radius,
				l.color[0], l.color[1], l.color[2], shape,
				dir[0], dir[1], dir[2], l.cone,
				l.bias, l.scale, l.exponent,
				float( ( l.noSpecular ? 1 : 0 ) | ( l.noRim ? 2 : 0 ) | ( l.ignoreRoughness ? 4 : 0 ) ),	// lane RIM1
				slotOf[size_t( i )], float( l.shadow ), l.nearClip, l.shadowBias,
				l.box[0][0], l.box[0][1], l.box[0][2], l.box[0][3],
				l.box[1][0], l.box[1][1], l.box[1][2], l.box[1][3],
				l.box[2][0], l.box[2][1], l.box[2][2], l.box[2][3] };
			t.insert( t.end(), tex, tex + kTexelsPerLight * 4 );
		}
		/* lane FARVIEW1: WW_CELL_LIGHT_COPIES=n (the flat-cost gate's knob): every light n times at 1/n its colour, the
		 * picture unchanged, the light loop n times as long */
		static const int copies = std::clamp( qEnvironmentVariableIntValue( "WW_CELL_LIGHT_COPIES" ), 1, 64 );
		if ( copies > 1 && !t.empty() ) {
			const size_t one = t.size();
			t.reserve( one * size_t( copies ) );
			for ( size_t o = 0; o < one; o += kTexelsPerLight * 4 )
				for ( int k = 4; k < 7; k++ )
					t[o + size_t( k )] /= float( copies );
			for ( int c = 1; c < copies; c++ )
				t.insert( t.end(), t.begin(), t.begin() + std::ptrdiff_t( one ) );
		}
		g.bufShStamp = g.shStamp;
		if ( t.empty() )
			t.assign( kTexelsPerLight * 4, 0.0f );	// a buffer texture must have a store
		if ( !g.buf ) {
			fn->glGenBuffers( 1, &g.buf );
			fn->glGenTextures( 1, &g.tex );
		}
		fn->glBindBuffer( GL_TEXTURE_BUFFER, g.buf );
		fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( t.size() * sizeof( float ) ), t.data(), GL_STATIC_DRAW );
		fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
		g.doc = scene->nifModel;
		g.version = s.version.value( scene->nifModel );
		g.count = int( t.size() / ( kTexelsPerLight * 4 ) ) - ( L->lights.isEmpty() ? 1 : 0 );
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
	// lane PROBEVIEW1: a Pass reads the grid with the GI row off too; Sky visibility binds the sky grid in its place
	const int pass = wwCellPassFor( scene->nifModel );
	const WwCellGi * G = on && ( s.giOn || pass > 0 ) ? wwCellGiFor( scene->nifModel ) : nullptr;
	if ( G && pass == 2 && !G->sky.empty() && ( g.skyDoc != scene->nifModel || g.skyVersion != s.giVersion.value( scene->nifModel ) ) ) {
		std::vector<float> sky = G->sky;
		if ( s.passRed & 4 )	// red "open": every direction open to the sky
			for ( size_t i = 0; i + 3 < sky.size(); i += 4 )
				sky[i] = sky[i + 1] = sky[i + 2] = sky[i + 3];
		if ( !g.skyTex )
			fn->glGenTextures( 1, &g.skyTex );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kGiUnit ) );
		fn->glBindTexture( GL_TEXTURE_3D, g.skyTex );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
		fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA16F, G->dims[0], G->dims[1], G->dims[2] * ( G->slotRooms.empty() ? 6 : 12 ), 0, GL_RGBA, GL_FLOAT,
			sky.data() );
		g.skyDoc = scene->nifModel;
		g.skyVersion = s.giVersion.value( scene->nifModel );
	}
	if ( G && pass != 2 && !G->rgba.empty() && ( g.giDoc != scene->nifModel || g.giVersion != s.giVersion.value( scene->nifModel ) ) ) {
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
		fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA16F, G->dims[0], G->dims[1], G->dims[2] * ( G->slotRooms.empty() ? 6 : 12 ), 0, GL_RGBA, GL_FLOAT,
			G->rgba.data() );
		g.giDoc = scene->nifModel;
		g.giVersion = s.giVersion.value( scene->nifModel );
	}
	const bool skyDraw = G && pass == 2 && g.skyTex && g.skyDoc == scene->nifModel;
	const bool giDraw = skyDraw || ( G && pass != 2 && g.giTex && g.giDoc == scene->nifModel );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kGiUnit ) );
	fn->glBindTexture( GL_TEXTURE_3D, skyDraw ? g.skyTex : giDraw ? g.giTex : 0 );
	// lane ROOMCLAMP1: the rooms (src/proberooms.h): the grid's slot rooms and the fine rooms, nearest, R32F
	if ( g.units < 0 ) {
		GLint units = 0;
		fn->glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &units );
		g.units = units;
	}
	const bool roomsUp = giDraw && !G->slotRooms.empty() && !G->rooms.empty() && g.units > kRoomUnit;
	if ( roomsUp && ( g.roomDoc != scene->nifModel || g.roomVersion != s.giVersion.value( scene->nifModel ) ) ) {
		auto up3 = [&]( GLuint & tex, int unit, const int * d, const std::vector<float> & v ) {
			if ( !tex )
				fn->glGenTextures( 1, &tex );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
			fn->glBindTexture( GL_TEXTURE_3D, tex );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
			fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
			fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_R32F, d[0], d[1], d[2], 0, GL_RED, GL_FLOAT, v.data() );
		};
		up3( g.slotTex, kGiSlotUnit, G->dims, G->slotRooms );
		up3( g.roomTex, kRoomUnit, G->roomsDims, G->rooms );
		g.roomDoc = scene->nifModel;
		g.roomVersion = s.giVersion.value( scene->nifModel );
	}
	const bool roomsDraw = roomsUp && g.roomDoc == scene->nifModel;
	if ( g.units > kRoomUnit ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kGiSlotUnit ) );
		fn->glBindTexture( GL_TEXTURE_3D, roomsDraw ? g.slotTex : 0 );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kRoomUnit ) );
		fn->glBindTexture( GL_TEXTURE_3D, roomsDraw ? g.roomTex : 0 );
		prog->uni1i( "cellGiSlots", kGiSlotUnit );
		prog->uni1i( "cellRooms", kRoomUnit );
	} else {	// two sampler3D may share a unit; a sampler3D on a 2D unit fails the draw
		prog->uni1i( "cellGiSlots", kGiUnit );
		prog->uni1i( "cellRooms", kGiUnit );
	}
	prog->uni1b( "cellGiRooms", roomsDraw );
	if ( roomsDraw ) {
		prog->uni3f( "cellRoomsOrigin", G->roomsOrigin[0], G->roomsOrigin[1], G->roomsOrigin[2] );
		prog->uni1f( "cellRoomsCell", G->roomsCell );
		prog->uni3f( "cellRoomsDims", float( G->roomsDims[0] ), float( G->roomsDims[1] ), float( G->roomsDims[2] ) );
	}
	/* lane FARVIEW1: the far tables and the GI grid's placed share, bound (the sampler types fixed) whether or not
	 * this draw uses them; a GPU with fewer than 23 units never draws far light */
	const WwCellFar * Fr = on && s.farOn && pass == 0 && g.units > kFarPlacedUnit ? wwCellFarFor( scene->nifModel ) : nullptr;
	if ( Fr && !Fr->slotTab.empty() && ( g.farDoc != scene->nifModel || g.farVersion != s.farVersion.value( scene->nifModel ) ) ) {
		auto upBuf = [&]( GLuint & buf, GLuint & tex, const void * data, size_t bytes ) {
			if ( !buf ) {
				fn->glGenBuffers( 1, &buf );
				fn->glGenTextures( 1, &tex );
			}
			fn->glBindBuffer( GL_TEXTURE_BUFFER, buf );
			fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( bytes ), data, GL_STATIC_DRAW );
			fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
		};
		upBuf( g.farSlotBuf, g.farSlotTex, Fr->slotTab.data(), Fr->slotTab.size() * sizeof( float ) );
		upBuf( g.farRecBuf, g.farRecTex, Fr->recs.data(), Fr->recs.size() * sizeof( float ) );
		if ( !Fr->placed.empty() ) {
			if ( !g.farPlacedTex )
				fn->glGenTextures( 1, &g.farPlacedTex );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kFarPlacedUnit ) );
			fn->glBindTexture( GL_TEXTURE_3D, g.farPlacedTex );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
			fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
			fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA16F, Fr->placedDims[0], Fr->placedDims[1], Fr->placedDims[2] * 6, 0,
				GL_RGBA, GL_FLOAT, Fr->placed.data() );
		}
		g.farDoc = scene->nifModel;
		g.farVersion = s.farVersion.value( scene->nifModel );
	}
	const bool farDraw = Fr && !Fr->slotTab.empty() && g.farSlotTex && g.farDoc == scene->nifModel;
	const bool farPlacedDraw = farDraw && giDraw && !Fr->placed.empty() && g.farPlacedTex;
	if ( g.units > kFarPlacedUnit ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kFarSlotUnit ) );
		fn->glBindTexture( GL_TEXTURE_BUFFER, g.farSlotTex );
		if ( g.farSlotTex )
			fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, g.farSlotBuf );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kFarRecUnit ) );
		fn->glBindTexture( GL_TEXTURE_BUFFER, g.farRecTex );
		if ( g.farRecTex )
			fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, g.farRecBuf );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kFarPlacedUnit ) );
		fn->glBindTexture( GL_TEXTURE_3D, farPlacedDraw ? g.farPlacedTex : 0 );
		prog->uni1i( "cellFarSlots", kFarSlotUnit );
		prog->uni1i( "cellFarRecs", kFarRecUnit );
		prog->uni1i( "cellFarPlaced", kFarPlacedUnit );
	} else {	// never read (cellFarOn false): the far samplers ride units of their own types (two samplers of one type may share)
		prog->uni1i( "cellFarSlots", kTextureUnit );
		prog->uni1i( "cellFarRecs", kTextureUnit );
		prog->uni1i( "cellFarPlaced", kGiUnit );
	}
	prog->uni1b( "cellFarOn", farDraw );
	prog->uni1b( "cellFarPlacedOn", farPlacedDraw );
	prog->uni1i( "cellFarRed", s.farRed );
	if ( farDraw ) {
		prog->uni1i( "cellFarBits", Fr->bits );
		prog->uni1i( "cellFarProbe", Fr->maxProbe );
		prog->uni1f( "cellFarCell", Fr->cell );
		prog->uni2f( "cellFarBand", Fr->band[0], Fr->band[1] );
	}
	if ( farPlacedDraw ) {
		prog->uni3f( "cellFarPlacedOrigin", Fr->placedOrigin[0], Fr->placedOrigin[1], Fr->placedOrigin[2] );
		prog->uni1f( "cellFarPlacedVoxel", Fr->placedVoxel );
		prog->uni3f( "cellFarPlacedDims", float( Fr->placedDims[0] ), float( Fr->placedDims[1] ), float( Fr->placedDims[2] ) );
	}
	fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni1i( "cellGi", kGiUnit );
	prog->uni1b( "cellGiOn", giDraw );
	prog->uni1b( "cellGiSky", giDraw && !skyDraw && G->skyLit );	// lane SKY1 (never on the sky-share grid)
	prog->uni1i( "cellGiAmb", s.giAmb );	// lane GICAL1
	prog->uni1b( "cellGiFill", s.giFill && !skyDraw );	// the gap fill is the GI grid's, never the sky share's
	prog->uni1i( "cellPass", pass );	// lane PROBEVIEW1
	prog->uni1i( "cellPassRed", s.passRed & 11 );	// lane ROOMCLAMP1: + 8 noclamp
	if ( giDraw ) {
		prog->uni3f( "cellGiOrigin", G->origin[0], G->origin[1], G->origin[2] );
		prog->uni1f( "cellGiVoxel", G->voxel );
		prog->uni3f( "cellGiDims", float( G->dims[0] ), float( G->dims[1] ), float( G->dims[2] ) );
	}
	// lane IMGS1: the imagespace, once this document has a measure; its LUT bound like the grid above
	const float adapted = s.adapted.value( scene->nifModel, -1.0f );
	int isStamp = 0;	// lane SUNCELL1: an exterior's imagespace is the weather's (isLighting)
	const WwCellLighting * LI = isLighting( scene->nifModel, &isStamp );
	const bool isDraw = L && LI && !s.measuring && adapted >= 0.0f && wwCellImageSpaceWanted( scene );
	if ( isDraw && LI->isLut.size() == 16 * 16 * 16 * 3
		&& ( g.lutDoc != scene->nifModel || g.lutVersion != s.version.value( scene->nifModel ) || g.lutStamp != isStamp ) ) {
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
		fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_RGB8, 16, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, LI->isLut.data() );
		fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
		g.lutDoc = scene->nifModel;
		g.lutVersion = s.version.value( scene->nifModel );
		g.lutStamp = isStamp;
	}
	const bool lutDraw = isDraw && g.lutTex && g.lutDoc == scene->nifModel && LI->isLut.size() == 16 * 16 * 16 * 3;
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
		const float * h = LI->isHdr;
		// the tonemap PS: max( mid / (lum + 0.001), cb.y ) then min( .., cb.x ); cb (x, y) = HNAM (max, min)
		const float e = std::min( std::max( h[8] / ( adapted + 0.001f ), h[5] ), h[4] );
		prog->uni1f( "cellIsExposure", e );
		prog->uni1f( "cellIsE", h[1] );
		prog->uni1f( "cellIsAdapted", adapted );
		prog->uni3f( "cellIsCine", LI->isCine[0], LI->isCine[1], LI->isCine[2] );
		prog->uni4f_l( prog->uniLocation( "cellIsTint" ), FloatVector4( LI->isTint[0], LI->isTint[1], LI->isTint[2], LI->isTint[3] ) );
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
		// lane HEMI1: K.w = 1 marks a box volume, its three rows in cellAmboBox
		prog->uni4f_l( prog->uniLocation( "cellAmboK[%d]", i ), FloatVector4( a.k[0], a.k[1], a.k[2], a.hasBox ? 1.0f : 0.0f ) );
		for ( int k = 0; k < 3 && a.hasBox; k++ )
			prog->uni4f_l( prog->uniLocation( "cellAmboBox[%d]", i * 3 + k ),
				FloatVector4( a.box[k][0], a.box[k][1], a.box[k][2], a.box[k][3] ) );
	}
	/* lane SUNCELL1: an exterior's directional is the weather's sun (or moon at night): Lookdev's light, TO it,
	 * world, linear, the numbers the viewport light already carries. Only the cell programs take it (a lit effect
	 * and the volumetric fog keep the old exterior law); Lookdev off (no weather) = the old path, unchanged. */
	bool hasDir = L->hasDirectional;
	float dirTo[3] = { L->dirTo[0], L->dirTo[1], L->dirTo[2] }, dirColor[3] = { L->dirColor[0], L->dirColor[1], L->dirColor[2] };
	if ( !L->interior && wwLookdevActive() && wwIsCellProgramName( prog->name ) ) {
		float dalc[6][3], dif[4] = {}, amb[4] = {};
		if ( wwLookdevDalc( dalc ) ) {
			wwLookdevLight( dirTo, dif, amb );
			std::copy( dif, dif + 3, dirColor );
			hasDir = dirColor[0] > 0.0f || dirColor[1] > 0.0f || dirColor[2] > 0.0f;
		}
		// telemetry: what the exterior sun uploaded (or why not), once per change
		static QString sunLast;
		const QString line = hasDir
			? QStringLiteral( "cell sun: exterior directional=weather to=%1,%2,%3 color=%4,%5,%6 (linear) shadows=%7" )
				.arg( double( dirTo[0] ), 0, 'f', 5 ).arg( double( dirTo[1] ), 0, 'f', 5 ).arg( double( dirTo[2] ), 0, 'f', 5 )
				.arg( double( dirColor[0] ), 0, 'f', 5 ).arg( double( dirColor[1] ), 0, 'f', 5 ).arg( double( dirColor[2] ), 0, 'f', 5 )
				.arg( prog->uniLocation( "csmMap" ) >= 0 ? 1 : 0 )
			: QStringLiteral( "cell sun: exterior refused (no weather loaded, or a black sun)" );
		if ( line != sunLast ) {
			fprintf( stderr, "%s\n", qPrintable( line ) );
			sunLast = line;
		}
	}
	/* lane SUNCELL1: an exterior's ambient is the weather's directional ambient (the 6 DALC colours blended over
	 * the hour's keys, Lookdev's wwLookdevDalc), uploaded in the interior's form (byte/255, (a - b) / 2 per axis,
	 * the mean) so cellAmbient is one law indoors and out. The axis is the light's TRAVEL direction (Lookdev's
	 * convention: the weathers store the bright sky blue in Z-, so Z- lights an up-facing normal). Only with the
	 * weather's sun (the exterior branch of cellLit). Red WW_CELL_EXTAMB_RED=flat keeps the flat NAM0 Ambient;
	 * =flip swaps the axis convention. */
	bool extDalc = false;
	if ( !L->interior && hasDir && wwLookdevActive() && wwIsCellProgramName( prog->name ) ) {
		static const QByteArray extRed = qgetenv( "WW_CELL_EXTAMB_RED" ).trimmed();
		float w[6][3];
		if ( extRed != "flat" && wwLookdevDalc( w ) ) {
			const bool flip = extRed == "flip";
			float g[6][3];
			for ( int a = 0; a < 6; a++ )
				for ( int c = 0; c < 3; c++ )
					g[a][c] = std::pow( std::max( w[a][c], 0.0f ), 1.0f / 2.2f );
			for ( int c = 0; c < 3; c++ ) {
				float mean = 0.0f;
				for ( int a = 0; a < 6; a++ )
					mean += g[a][c] / 6.0f;
				const float sgn = flip ? 0.5f : -0.5f;	// travel: n.z = +1 takes Z- (index 5)
				prog->uni4f_l( prog->uniLocation( "cellDalc[%d]", c ), FloatVector4(
					( g[0][c] - g[1][c] ) * sgn, ( g[2][c] - g[3][c] ) * sgn, ( g[4][c] - g[5][c] ) * sgn, mean ) );
			}
			extDalc = true;
			static QString ambLast;
			const QString line = QStringLiteral( "cell ambient: exterior=weather DALC up=%1,%2,%3 down=%4,%5,%6 (byte/255)%7" )
				.arg( double( flip ? g[4][0] : g[5][0] ), 0, 'f', 4 ).arg( double( flip ? g[4][1] : g[5][1] ), 0, 'f', 4 )
				.arg( double( flip ? g[4][2] : g[5][2] ), 0, 'f', 4 ).arg( double( flip ? g[5][0] : g[4][0] ), 0, 'f', 4 )
				.arg( double( flip ? g[5][1] : g[4][1] ), 0, 'f', 4 ).arg( double( flip ? g[5][2] : g[4][2] ), 0, 'f', 4 )
				.arg( flip ? QStringLiteral( " RED=flip" ) : QString() );
			if ( line != ambLast ) {
				fprintf( stderr, "%s\n", qPrintable( line ) );
				ambLast = line;
			}
		} else {
			static bool saidFlat = false;
			if ( !saidFlat ) {
				fprintf( stderr, "cell ambient: exterior=flat NAM0 Ambient (%s)\n",
					extRed == "flat" ? "RED=flat" : "no weather DALC" );
				saidFlat = true;
			}
		}
	}
	prog->uni1b( "cellExtDalc", extDalc );
	prog->uni1b( "cellHasDir", hasDir );
	prog->uni3f( "cellDirColor", dirColor[0], dirColor[1], dirColor[2] );
	prog->uni3f( "cellDirTo", dirTo[0], dirTo[1], dirTo[2] );
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

bool wwCellShadowOn()
{
	return st().shadowOn;
}

void wwCellShadowSetOn( bool on )
{
	ClState & s = st();
	if ( s.shadowPinned )
		return;
	s.shadowOn = on;
	QSettings().setValue( QStringLiteral( "WW/CellShadow" ), on );
}

bool wwCellGiGpuOn()
{
	const QByteArray pin = qgetenv( "WW_CELL_GI_GPU" ).trimmed();
	if ( !pin.isEmpty() )
		return pin != "0";
	return QSettings().value( QStringLiteral( "WW/CellGiGpu" ), false ).toBool();
}

void wwCellGiGpuSetOn( bool on )
{
	if ( !qgetenv( "WW_CELL_GI_GPU" ).trimmed().isEmpty() )
		return;
	QSettings().setValue( QStringLiteral( "WW/CellGiGpu" ), on );
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
	if ( !st().isOn || !scene || wwCellPassFor( scene->nifModel ) > 0 || !wwCellLightsWanted( scene ) )
		return false;	// lane PROBEVIEW1: a Pass shows the probes' own values, not the imagespace's grade
	const WwCellLighting * L = isLighting( scene->nifModel );	// lane SUNCELL1: an exterior's is the weather's
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
	const WwCellLighting * L = isLighting( scene->nifModel );	// lane SUNCELL1
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
	const WwCellLighting * L = scene && scene->nifModel ? isLighting( scene->nifModel ) : nullptr;	// lane SUNCELL1
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
	const WwCellFar * Fr = scene && scene->nifModel ? wwCellFarFor( scene->nifModel ) : nullptr;	// lane FARVIEW1
	if ( s.farOn || Fr )
		o += QStringLiteral( " far=%1(asked=%2%3, red=%4)" ).arg( wwCellLightsWanted( scene ) && s.farOn && Fr ? "on" : "off" )
			.arg( s.farOn ? 1 : 0 ).arg( Fr ? QStringLiteral( ", %1" ).arg( Fr->summary ) : QStringLiteral( ", none published" ) ).arg( s.farRed );
	if ( s.dotsLast >= 0 )	// lane FARVIEW1: the bulb dots the last frame drew
		o += QStringLiteral( " fardots=%1" ).arg( s.dotsLast );
	if ( s.probe )
		o += QStringLiteral( " probe=%1" ).arg( s.probe );
	o += QStringLiteral( " giamb=%1 gifill=%2" ).arg( QStringList{ "keep", "replace", "off", "max", "asgi" }.value( s.giAmb ) )
		.arg( s.giFill ? 1 : 0 );	// lane GICAL1
	if ( s.red )
		o += QStringLiteral( " red=%1" ).arg( s.red );
	if ( s.pass > 0 )	// lane PROBEVIEW1
		o += QStringLiteral( " pass=%1(asked=%2 %3, sky=%4, pvred=%5)" ).arg( scene ? wwCellPassFor( scene->nifModel ) : 0 )
			.arg( s.pass ).arg( wwCellPassNames().value( s.pass ) ).arg( G && !G->sky.empty() ? 1 : 0 ).arg( s.passRed );
	return o + QLatin1Char( ' ' ) + wwCellShadowEcho( scene )
		+ ( wwVolFogOn() ? QStringLiteral( " " ) + wwVolFogSummary() : QString() );	// lane VOLFOG1
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
		s.shadowLast = s.shadowPinned ? QStringLiteral( "off(WW_CELL_SHADOW=0)" ) : QStringLiteral( "off(row)" );
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
	// lane SUNCELL1: a cell's water surface casts no light shadow either (the game's water is no caster; a pond's
	// plane under a lamp shadowed its own bed). Red WW_CELL_SHADOW_RED=watercasts keeps it.
	static const bool waterCastsRed = qgetenv( "WW_CELL_SHADOW_RED" ).trimmed() == "watercasts";
	int waterSkipped = 0;
	for ( Node * node : scene->nodes.list() ) {
		const Shape * sh = dynamic_cast<const Shape *>( node );
		if ( !sh || !sh->isVisible() || !sh->wwCastsSunShadow() || sh->wwAlphaTested() )
			continue;
		if ( sh->verts.isEmpty() || sh->triangles.isEmpty() )
			continue;
		if ( !waterCastsRed && wwCellWaterIsShape( scene->nifModel, sh->id() ) ) {
			waterSkipped++;
			continue;
		}
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
	s.shadowLast = QStringLiteral( "on shadowLights=%1 rendered=%2 casters=%3 draws=%4 ms=%5 face=%6 water=%7" )
		.arg( cand.size() ).arg( dirty.size() ).arg( casters.size() ).arg( drawn ).arg( timer.elapsed() ).arg( kShadowFace )
		.arg( waterCastsRed ? QStringLiteral( "casts(RED)" ) : QStringLiteral( "skipped %1" ).arg( waterSkipped ) );	// lane SUNCELL1
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
	bool pinned = false;	// lane CELLALL1: the AO row (WW/CellAo); the pin wins
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
		const QByteArray aoPin = qgetenv( "WW_CELL_AO" ).trimmed();
		a.pinned = !aoPin.isEmpty();
		a.on = a.pinned ? aoPin != "0" : QSettings().value( QStringLiteral( "WW/CellAo" ), true ).toBool();
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
	GLuint decal = 0;		// lane AODECAL1: this frame's AO decal target (0: none)
	int decalUnit = -1;		// 20, or -1 on a GPU with 20 units or fewer
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
	return p && ( wwIsCellProgramName( p->name ) );
}

}	// namespace

bool wwCellAoOn()
{
	return ao().on;
}

void wwCellAoSetOn( bool on )
{
	AoState & a = ao();
	if ( a.pinned )
		return;
	a.on = on;
	QSettings().setValue( QStringLiteral( "WW/CellAo" ), on );
}

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
		g.decalUnit = units > 20 ? 20 : -1;
	}
	g.decal = 0;
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
	// lane AODECAL1 (src/gl/cellaodecalgl.h): the decal copies over this opaque pass, into their own target
	if ( g.decalUnit >= 0 && wwCellAoDecalHas( scene->nifModel ) )
		g.decal = GLuint( wwCellAoDecalRun( scene, g.gbuf.tex, g.gbufDepth, W, H ) );

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

// lane SSR1: this frame's opaque pass and depth pyramid, for the reflections' march (src/gl/cellssr.h)
bool wwCellAoTargets( Scene * scene, WwCellAoTargets & out )
{
	if ( !scene || !scene->renderer || !aoGpus().contains( scene->renderer ) )
		return false;
	const AoGpu & g = aoGpus()[scene->renderer];
	if ( !g.ready || g.doc != scene->nifModel )
		return false;
	out.gbuf = g.gbuf.tex;
	out.depthRb = g.gbufDepth;
	out.w = g.gbuf.w;
	out.h = g.gbuf.h;
	for ( int m = 1; m < 5; m++ ) {
		out.mip[m] = g.mip[m].tex;
		out.mipW[m] = g.mip[m].w;
		out.mipH[m] = g.mip[m].h;
	}
	return true;
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
			prog->uni1b( "cellAoDecalOn", false );	// lane AODECAL1
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
	/* lane AODECAL1: the decal target multiplies the probe term (cellGiE). Opaque draws only, like the obscurance:
	 * a blended draw is not in the opaque pass the target was made over. */
	const bool decalOn = g.ready && g.doc == scene->nifModel && g.decal && g.decalUnit >= 0 && !glIsEnabled( GL_BLEND );
	{
		const int du = g.decalUnit >= 0 ? g.decalUnit : std::max( g.unit, 0 );
		r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + du ) );
		if ( g.decalUnit >= 0 )
			r->fn->glBindTexture( GL_TEXTURE_2D, decalOn ? g.decal : 0 );
		r->fn->glActiveTexture( GLenum( prevActive ) );
		prog->uni1i( "cellAoDecal", du );
		prog->uni1b( "cellAoDecalOn", decalOn );
		if ( decalOn ) {
			GLint dvp[4] = { 0, 0, 1, 1 };
			glGetIntegerv( GL_VIEWPORT, dvp );
			prog->uni4f_l( prog->uniLocation( "cellAoDecalRect" ), FloatVector4( float( dvp[0] ), float( dvp[1] ), 0.0f, 0.0f ) );
		}
	}
	if ( on ) {
		GLint vp[4] = { 0, 0, 1, 1 };
		glGetIntegerv( GL_VIEWPORT, vp );
		prog->uni4f_l( prog->uniLocation( "cellAoRect" ), FloatVector4( float( vp[0] ), float( vp[1] ),
			1.0f / float( std::max( vp[2], 1 ) ), 1.0f / float( std::max( vp[3], 1 ) ) ) );
	}
}
