#include "cellwater.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "cellhdr.h"
#include "celllights.h"
#include "celllooks.h"	// lane WATERDEPTH1: the Water depth colour row
#include "cellssr.h"
#include "celllooks.h"	// lane WATERSHORE1: the Water soft edge and Shore foam rows
#include "cellwaterinfo.h"	// lane WATERDATA1: the baked water planes
#include "lookdevstage.h"
#include "esmwater.h"
#include "sunshadow.h"	// lane WATERCAUSTICS1: the caustics read the sun's cascades when they are built
#include "cellworldanim.h"	// lane FXPARITY2: the world clock
#include "gl/glscene.h"
#include "gl/gltex.h"
#include "gl/renderer.h"

#include <QFile>
#include <QHash>
#include <QSet>
#include <QSettings>
#include <QTextStream>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace
{

struct WaterBody
{
	WwWaterRecord rec;
	WwWaterMaterial mat;
	QString noise[3];	// as the texture cache takes them: "textures\water\..."
};

struct WaterDoc
{
	QVector<WaterBody> bodies;
	QHash<quint32, int> byForm;
	QHash<int, int> shapeBody;	// written shape's block -> index into bodies
};

struct WaterState
{
	QHash<const void *, WaterDoc> docs;
	bool pinOff = false;	// lane CELLALL1: off (the Water row, or the pin)
	bool pinned = false;
	int red = 0;		// 1 norefl, 2 nofresnel, 4 nosilt, 8 nospec, 16 noshore, 32 nonormal,
				// 64 nodepthfog, 128 nofar, 256 nossr, 512 fogunits (lane WATER2)
				// 1024 hdrraw (WATERHDR1: the water's linear light written back untone-mapped, the old defect)
	int probe = 0;
	int shoreRed = 0;	// WATERSHORE1: 1 nearest (WW_CELL_WATER_SHORE_RED)
	int wavesRed = 0;	// WATERWAVES1: 1 flat, 2 noshore, 4 alias (WW_CELL_WATER_WAVES_RED)
	bool foamOld = false;	// WATERFOAMDETAIL1: WW_CELL_WATER_FOAM_RED=old, the distance band instead of the wave law
	int foamWind = -1;	// WATERFOAMDETAIL1: WW_CELL_WATER_FOAM_WIND=0..255 pins the weather's wind byte (gates)
	bool edgeOld = false;	// WATERFOAMDETAIL1: WW_CELL_WATER_EDGE_RED=old, the baked band only (no soft contact)
	bool linear = false;	// WATERHDR1: the last water draw wrote linear light into the HDR frame
	QString dump;
	QSet<QString> missing;	// noise textures that did not bind (the census says so)
	quint64 dumped = 0;
};

WaterState & st()
{
	static WaterState * s = nullptr;
	static quint64 liveGen = 0;
	// lane TESTDRIVER1: a live set of these switches re-reads them (pins and or-ed reds back to their defaults first)
	if ( !s || wwSwitchMovedFor( liveGen, { "WW_CELL_WATER" } ) ) {
		if ( !s )
			s = new WaterState;
		liveGen = wwSwitchGen();
		const QByteArray pin = wwEnv( "WW_CELL_WATER" ).trimmed();
		s->pinned = !pin.isEmpty();
		s->pinOff = s->pinned ? pin == "0" : !QSettings().value( QStringLiteral( "WW/CellWater" ), true ).toBool();
		const QByteArray r = wwEnv( "WW_CELL_WATER_RED" );
		s->red = r == "norefl" ? 1 : r == "nofresnel" ? 2 : r == "nosilt" ? 4 : r == "nospec" ? 8
			: r == "noshore" ? 16 : r == "nonormal" ? 32 : r == "nodepthfog" ? 64 : r == "nofar" ? 128
			: r == "nossr" ? 256 : r == "fogunits" ? 512 : r == "hdrraw" ? 1024 : 0;
		s->probe = wwEnvInt( "WW_CELL_WATER_PROBE" );
		s->shoreRed = wwEnv( "WW_CELL_WATER_SHORE_RED" ).trimmed() == "nearest" ? 1 : 0;
		const QByteArray wr = wwEnv( "WW_CELL_WATER_WAVES_RED" ).trimmed();
		s->wavesRed = wr == "flat" ? 1 : wr == "noshore" ? 2 : wr == "alias" ? 4 : 0;
		s->foamOld = wwEnv( "WW_CELL_WATER_FOAM_RED" ).trimmed() == "old";
		const QByteArray fw = wwEnv( "WW_CELL_WATER_FOAM_WIND" ).trimmed();
		s->foamWind = fw.isEmpty() ? -1 : std::clamp( fw.toInt(), 0, 255 );
		s->edgeOld = wwEnv( "WW_CELL_WATER_EDGE_RED" ).trimmed() == "old";
		s->dump = QString::fromLocal8Bit( wwEnv( "WW_CELL_WATER_DUMP" ) );
	}
	return *s;
}

// lane WATERFOAMDETAIL1: the foam's drive, the weather's wind (or the pin) as m/s and the sea swell in m
void foamDrive( const WaterState & s, float & windMs, float & swellM )
{
	quint8 wind[4] = { 0, 0, 0, 0 };
	const bool haveWind = s.foamWind >= 0 || wwLookdevWind( wind );
	const float w01 = s.foamWind >= 0 ? float( s.foamWind ) / 255.0f : float( wind[0] ) / 255.0f;
	windMs = haveWind ? 1.0f + 19.0f * w01 : 5.0f;
	swellM = 0.344f * ( haveWind ? 0.6f + 0.8f * w01 : 1.0f );
}

//! NAM2-4 store "data\Textures\Water\X.dds"; the cache resolves paths under the data folder
QString noisePath( const QString & stored )
{
	QString p = QString( stored ).replace( '/', '\\' );
	if ( p.startsWith( QLatin1String( "data\\" ), Qt::CaseInsensitive ) )
		p = p.mid( 5 );
	return p;
}

void writeDump( const WaterDoc & d, const float sky[3][3], bool skyFromWeather, const float sun[3] )
{
	WaterState & s = st();
	QFile f( s.dump );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Text ) )
		return;
	QTextStream o( &f );
	o << "water bodies=" << d.bodies.size() << " shapes=" << d.shapeBody.size() << " red=" << s.red
	  << " probe=" << s.probe << "\n";
	o << "sky " << ( skyFromWeather ? "weather" : "reflection" );
	for ( int r = 0; r < 3; r++ )
		for ( int c = 0; c < 3; c++ )
			o << " " << QString::number( double( sky[r][c] ), 'g', 9 );
	o << "\nsun";
	for ( int c = 0; c < 3; c++ )
		o << " " << QString::number( double( sun[c] ), 'g', 9 );
	o << "\n";
	for ( const WaterBody & b : d.bodies )
		o << wwWaterDescribe( b.rec ) << "\n";
	for ( auto it = d.shapeBody.constBegin(); it != d.shapeBody.constEnd(); ++it )
		o << "shape " << it.key() << " body " << it.value() << " form "
		  << QString::number( d.bodies[it.value()].rec.form, 16 ) << "\n";
}

} // namespace

void wwCellWaterBegin( const void * nif )
{
	st().docs.remove( nif );
}

void wwCellWaterShape( const void * nif, int block, const WwWaterRecord & rec )
{
	WaterDoc & d = st().docs[nif];
	auto it = d.byForm.constFind( rec.form );
	int i;
	if ( it == d.byForm.constEnd() ) {
		WaterBody b;
		b.rec = rec;
		b.mat = wwWaterMaterial( rec );
		for ( int k = 0; k < 3; k++ )
			b.noise[k] = noisePath( rec.noise[k] );
		i = d.bodies.size();
		d.bodies.append( b );
		d.byForm.insert( rec.form, i );
	} else {
		i = it.value();
	}
	d.shapeBody.insert( block, i );
}

void wwCellWaterForget( const void * nif, int block )
{
	auto d = st().docs.find( nif );
	if ( d != st().docs.end() )
		d->shapeBody.remove( block );
}

void wwCellWaterRemap( const void * nif, const WwBlockMap & m )
{
	auto d = st().docs.find( nif );
	if ( d != st().docs.end() )
		wwRemapKeys( d->shapeBody, m );
}

bool wwCellWaterOn()
{
	return !st().pinOff;
}

void wwCellWaterSetOn( bool on )
{
	WaterState & s = st();
	if ( s.pinned )
		return;
	s.pinOff = !on;
	QSettings().setValue( QStringLiteral( "WW/CellWater" ), on );
}

bool wwCellWaterWanted( Scene * scene, int block )
{
	const WaterState & s = st();
	if ( s.pinOff || !scene || scene->selecting || !wwCellLightsWanted( scene ) || wwCellProbePass( scene ) )
		return false;
	const auto it = s.docs.constFind( scene->nifModel );
	return it != s.docs.constEnd() && it->shapeBody.contains( block );
}

bool wwCellWaterSsrWanted( Scene * scene )
{
	const WaterState & s = st();
	if ( s.pinOff || !scene || scene->selecting || !wwCellLightsWanted( scene ) || wwCellProbePass( scene ) )
		return false;
	const auto it = s.docs.constFind( scene->nifModel );
	if ( it == s.docs.constEnd() || it->shapeBody.isEmpty() )
		return false;
	for ( const WaterBody & b : it->bodies )
		if ( b.rec.ssr )
			return true;
	return false;
}

bool wwCellWaterIsShape( const void * nif, int block )
{
	const WaterState & s = st();
	const auto it = s.docs.constFind( nif );
	return it != s.docs.constEnd() && it->shapeBody.contains( block );
}

bool wwCellWaterUniforms( Scene * scene, int block, int unit )
{
	WaterState & s = st();
	if ( !scene || !scene->renderer )
		return false;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "waterMat" ) < 0 )
		return false;
	const auto dit = s.docs.constFind( scene->nifModel );
	if ( dit == s.docs.constEnd() )
		return false;
	const auto sit = dit->shapeBody.constFind( block );
	if ( sit == dit->shapeBody.constEnd() )
		return false;
	const WaterBody & b = dit->bodies[sit.value()];
	auto fn = r->fn;

	// the record's constant block, in the order the game's shader reads it (src/esmwater.h)
	const float * rows[13] = { b.mat.shallow, b.mat.deep, b.mat.reflection, b.mat.underwater, b.mat.lightSilt,
		b.mat.darkSilt, b.mat.varAmounts, b.mat.params1, b.mat.params2, b.mat.params3, b.mat.params4,
		b.mat.amplitude, b.mat.uvScale };
	FloatVector4 m[13];
	for ( int k = 0; k < 13; k++ )
		m[k] = FloatVector4( rows[k][0], rows[k][1], rows[k][2], rows[k][3] );
	prog->uni4fv( "waterMat", m, 13 );

	// the water's sky: the weather's horizon, lower and upper rows at the hour the dome uses (linear)
	float sky[3][3];
	const bool fromWeather = wwLookdevWaterSky( sky[0], sky[1], sky[2] );
	if ( !fromWeather )
		for ( int k = 0; k < 3; k++ )
			for ( int c = 0; c < 3; c++ )
				sky[k][c] = b.mat.reflection[c];
	FloatVector4 sk[3];
	for ( int k = 0; k < 3; k++ )
		sk[k] = FloatVector4( sky[k][0], sky[k][1], sky[k][2], 0.0f );
	prog->uni4fv( "waterSky", sk, 3 );
	prog->uni1i( "waterRed", s.red );
	prog->uni1i( "waterProbe", s.probe );
	{	// lane WATERDEPTH1: its red and (WATERCOMBINE1) its row (only fo4_waterlook reads them)
		WW_SWITCH_STATIC( QByteArray, dr, wwEnv( "WW_CELL_WATER_DEPTH_RED" ).trimmed() );
		prog->uni1i( "waterDepthRed", dr == "flat" ? 1 : dr == "noabsorb" ? 2 : dr == "nearest" ? 3 : 0 );
		prog->uni1b( "waterDepthOn", wwLookOn( WwLook::WaterDepth ) );
	}
	if ( prog->uniLocation( "waterCauTime" ) >= 0 ) {	// lane WATERCAUSTICS1: fo4_waterlook only
		prog->uni1b( "waterCauOn", wwLookOn( WwLook::WaterCaustics ) );	// lane WATERCOMBINE1: its own row
		WW_SWITCH_STATIC( QByteArray, cr, wwEnv( "WW_CELL_WATER_CAUSTICS_RED" ).trimmed() );
		prog->uni1i( "waterCauRed", cr == "nofade" ? 1 : cr == "noshadow" ? 2 : cr == "still" ? 3 : cr == "flat" ? 4 : 0 );
		// the water's own clock (its noise scroll's): pinned by WW_CELL_WORLDANIM_TIME, 0 with World animation off;
		// wrapped in double at the shader's 64 s loop (kWwCauLoop) so a long session keeps a float-exact phase
		prog->uni1f( "waterCauTime", wwCellWorldAnimOn() ? float( std::fmod( wwCellWorldSeconds(), 64.0 ) ) : 0.0f );
		prog->uni1b( "waterCauShadow", wwSunShadowWanted( scene ) );
	}
	// the normals' scroll: lane FXPARITY2, wind direction x speed x the world clock (INFERRED); off: time 0 as before
	if ( wwCellWorldAnimOn() ) {
		float s0[4], s1[4];
		wwWaterScroll( b.rec, wwCellWorldSeconds(), s0, s1 );
		prog->uni4f( "waterScroll0", FloatVector4( s0[0], s0[1], s0[2], s0[3] ) );
		prog->uni4f( "waterScroll1", FloatVector4( s1[0], s1[1], s1[2], s1[3] ) );
	} else {
		prog->uni4f( "waterScroll0", FloatVector4( 0.0f ) );
		prog->uni4f( "waterScroll1", FloatVector4( 0.0f ) );
	}

	static const char * const noiseUni[3] = { "waterNoise0", "waterNoise1", "waterNoise2" };
	for ( int k = 0; k < 3; k++ ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
		if ( b.noise[k].isEmpty() || !scene->textures->bind( QStringView( b.noise[k] ), scene->nifModel ) ) {
			if ( !b.noise[k].isEmpty() )
				s.missing.insert( b.noise[k] );
			scene->textures->bind( QStringView( u"#FF8080FF" ), scene->nifModel );	// a flat normal
		}
		/* lane WATER2: the game's sampler for the noise slots 4..6 (its geometry setup): WRAP_S_WRAP_T, filter
		 * TEXTURE_FILTER_MODE_ANISO at the INI's iMaxAnisotropy (16). The cache leaves its default (nearest
		 * within the mip, no anisotropy): the distant ripples average flat and the far glints die. */
		static GLfloat maxAniso = -1.0f;
		if ( maxAniso < 0.0f ) {
			maxAniso = 1.0f;
			glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso );
		}
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, ( s.red & 128 ) ? 1.0f : std::min( 16.0f, maxAniso ) );
		prog->uni1i_l( prog->uniLocation( noiseUni[k] ), unit++ );
	}
	// the scene behind the water (the second pass's copy) and the opaque frame's depth; in the reflections' scene
	// passes (lane WATER2) no copy: the ray pass reads the obscurance pass's linear depth instead
	unsigned int gbuf = 0;
	const bool rayPass = wwCellSsrWaterRayPass( &gbuf );
	const bool inSsr = wwCellSsrInPass();
	const bool haveScene = !inSsr && scene->grabRefractionSource();
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_2D, haveScene ? scene->refractionTexId : 0 );
	prog->uni1i_l( prog->uniLocation( "waterScene" ), unit++ );
	const bool haveDepth = !inSsr && scene->grabEffectDepth();
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_2D, rayPass ? gbuf : haveDepth ? scene->fxDepthTexId : 0 );
	prog->uni1i_l( prog->uniLocation( "waterDepth" ), unit++ );
	prog->uni1b( "waterHaveDepth", haveDepth );
	prog->uni1i( "waterSslr", rayPass ? 1 : 0 );
	/* WATERHDR1: in the linear frame (cellhdr.h) the water writes linear light (fo4_water.frag, cellIsLinear) and
	 * must be sorted with the cell programs, tone-mapped once at the resolve; its probes and its reflection-ray
	 * pass write raw data and are written back as drawn */
	s.linear = wwCellHdrActive() && s.probe == 0 && !rayPass && !( s.red & 1024 );
	// lane WATER2: this frame's reflections (the game reads them only when its WATR's SSR flag is set)
	unsigned int ssrRaw = 0, ssrFin = 0;
	const bool ssrOn = !inSsr && b.rec.ssr && wwCellSsrWaterTextures( scene, ssrRaw, ssrFin );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_2D, ssrOn ? ssrRaw : 0 );
	prog->uni1i_l( prog->uniLocation( "waterSsrRaw" ), unit++ );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_2D, ssrOn ? ssrFin : 0 );
	prog->uni1i_l( prog->uniLocation( "waterSsrFin" ), unit++ );
	prog->uni1b( "waterSsrOn", ssrOn );
	unit += wwCellWaterInfoUniforms( scene, unit );	// lane WATERDATA1: units 7..9 when the program reads them
	if ( prog->uniLocation( "waterShoreOn" ) >= 0 ) {
		/* lane WATERSHORE1 (fo4_waterlook.prog, water_shore.glsl). Engineering choices in world units, no game value
		 * exists (the game draws no shore foam): the field reads 64 at the waterline (half a 128 u texel, a dry texel
		 * counts 0), the edge fades over 256 u, the foam band is 192 u wide with soft 0.25 noise edges; crests 96 u
		 * apart every 6 s, 0.3 of the foam between them, 0.85 opaque (Sea of Thieves 2018, Far Cry 5 2018 shapes) */
		const float waterline = 64.0f, edgeWidth = 256.0f, foamWidth = 192.0f, foamSoft = 0.25f;
		const float waveLength = 96.0f, wavePeriod = 6.0f, foamFloor = 0.3f, foamOpacity = 0.85f;
		prog->uni1i( "waterShoreOn", ( wwLookOn( WwLook::WaterSoftEdge ) ? 1 : 0 ) | ( wwLookOn( WwLook::ShoreFoam ) ? 2 : 0 ) );
		prog->uni1i( "waterShoreRed", s.shoreRed );
		prog->uni4f( "waterShoreK0", FloatVector4( waterline, edgeWidth, foamWidth, foamSoft ) );
		prog->uni4f( "waterShoreK1", FloatVector4( waveLength, wavePeriod, foamFloor, foamOpacity ) );
		// the world clock as the normals' scroll reads it: frozen at 0 with the world's animation off
		prog->uni1f( "waterShoreTime", wwCellWorldAnimOn() ? float( wwCellWorldSeconds() ) : 0.0f );
		/* lane WATERFOAMDETAIL1: the foam follows the wave energy at the shore (water_shore.glsl wwShoreFoamWave).
		 * Wind: the weather's speed byte as 1-20 m/s (the game's meaning of the byte is unknown; 5 m/s with no
		 * weather). Sea swell: WATERWAVES1's sea swell (a k 0.045 at 24 m = 0.34 m crest to trough) times its wind
		 * factor. A river's fetch is its width, 40 m. Full foam at 0.6 m breakers; fresh water keeps 0.25 of it;
		 * breaker index 0.78 (McCowan 1894); the lap line 0.35 opaque at most, from 5 cm waves (ripples run up clear). */
		float windMs = 0.0f, swellM = 0.0f;
		foamDrive( s, windMs, swellM );
		prog->uni1i( "waterShoreLaw", s.foamOld ? 0 : 1 );
		prog->uni4f( "waterShoreK2", FloatVector4( windMs, swellM, 40.0f, 0.6f ) );
		prog->uni4f( "waterShoreK3", FloatVector4( 0.25f, 0.78f, 0.35f, 0.05f ) );
		/* the soft contact: the water path to the scene behind fades the water out over 35 u (0.5 m) looking down to
		 * 105 u (1.5 m) at a grazing view; the bake's distance counts 64 u (half a texel) more for its error */
		prog->uni1i( "waterShoreEdgeLaw", s.edgeOld ? 0 : 1 );
		prog->uni4f( "waterShoreK4", FloatVector4( 35.0f, 105.0f, 64.0f, 0.0f ) );
	}
	if ( prog->uniLocation( "waterWavesOn" ) >= 0 ) {
		/* lane WATERWAVES1 (fo4_waterlook.prog, water_waves.glsl). Engineering choices in game units (70 u = 1 m); the
		 * game draws no swell to match. Wavelength = the body's length (sqrt of its baked area) / a divisor, clamped:
		 * lakes 1.5-8 m (/12), rivers 1-4 m (/16), the sea one 24 m swell. Total steepness a k 0.03 / 0.015 / 0.045:
		 * calm water, far under the 0.44 breaking limit. It fades in from the waterline (64 u) over half a wavelength;
		 * a body flagged tiny or under 4 texels gets none, full from 64 texels. */
		const FloatVector4 cls[3] = { FloatVector4( 1.0f, 1680.0f, 1680.0f, 0.045f ),
			FloatVector4( 16.0f, 70.0f, 280.0f, 0.015f ), FloatVector4( 12.0f, 105.0f, 560.0f, 0.03f ) };
		prog->uni4fv( "waterWavesClass", cls, 3 );
		prog->uni4f( "waterWavesShore", FloatVector4( 64.0f, 0.5f, 4.0f, 64.0f ) );
		// travels along the WATR's first noise layer's wind (the compass, 0 = +Y, as the scroll); height by the
		// weather's wind speed byte, 0.6x calm to 1.4x full (1x with no weather loaded)
		const double a = double( b.rec.windDirection[0] ) * 3.14159265358979323846 / 180.0;
		quint8 wind[4] = { 0, 0, 0, 0 };
		const float amp = wwLookdevWind( wind ) ? 0.6f + 0.8f * float( wind[0] ) / 255.0f : 1.0f;
		prog->uni4f( "waterWavesK", FloatVector4( float( std::sin( a ) ), float( std::cos( a ) ),
			wwCellWorldAnimOn() ? float( wwCellWorldSeconds() ) : 0.0f, amp ) );	// the world clock, 0 with it off
		prog->uni1i( "waterWavesOn", wwLookOn( WwLook::WaterSwell ) ? 1 : 0 );
		prog->uni1i( "waterWavesRed", s.wavesRed );
	}
	fn->glActiveTexture( GL_TEXTURE0 );

	// an opaque surface: it composites the scene behind it itself
	glDisable( GL_BLEND );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDepthMask( GL_TRUE );

	if ( !s.dump.isEmpty() && s.dumped != quint64( quintptr( scene->nifModel ) ) ) {
		s.dumped = quint64( quintptr( scene->nifModel ) );
		const float sun[3] = { 0.0f, 0.0f, 0.0f };	// the shader reads light 0 (the frame's sun); the probes echo it
		writeDump( *dit, sky, fromWeather, sun );
	}
	return true;
}

const char * wwCellWaterProgram()
{
	// lane WATERCOMBINE1: one twin carries every water look, each switched by its own uniform (wwCellWaterUniforms)
	return wwCellWaterLookOn() ? "fo4_waterlook.prog" : "fo4_water.prog";
}

bool wwCellWaterLookOn()
{
	return wwLookOn( WwLook::WaterDepth ) || wwLookOn( WwLook::WaterCaustics ) || wwLookOn( WwLook::WaterSoftEdge )
		|| wwLookOn( WwLook::ShoreFoam ) || wwLookOn( WwLook::WaterSwell );
}

bool wwCellWaterWritesLinear()
{
	return st().linear;
}

QString wwCellWaterEcho( const void * nif )
{
	const WaterState & s = st();
	const auto it = s.docs.constFind( nif );
	if ( it == s.docs.constEnd() )
		return QStringLiteral( "water: no water surfaces registered" );
	QStringList names;
	for ( const WaterBody & b : it->bodies )
		names << QStringLiteral( "%1 %2" ).arg( b.rec.form, 8, 16, QLatin1Char( '0' ) ).arg( b.rec.editorId );
	return QStringLiteral( "water: %1 shape(s), %2 record(s) [%3]%4; program %5" ).arg( it->shapeBody.size() )
		.arg( it->bodies.size() ).arg( names.join( QLatin1String( ", " ) ) )
		.arg( s.pinOff ? ( s.pinned ? QStringLiteral( ", pinned off (WW_CELL_WATER=0)" ) : QStringLiteral( ", off (row)" ) )
			: QString() )
		.arg( QLatin1String( wwCellWaterProgram() ) )	// lane WATERDEPTH1
		// lane WATERSHORE1: said only when a shore look is on, so the census line with both rows off is as before
		+ ( wwLookOn( WwLook::WaterSoftEdge ) || wwLookOn( WwLook::ShoreFoam )
			? QStringLiteral( ", shore look: edge %1, foam %2%3" )
				.arg( wwLookOn( WwLook::WaterSoftEdge ) ? QLatin1String( "on" ) : QLatin1String( "off" ) )
				.arg( wwLookOn( WwLook::ShoreFoam ) ? QLatin1String( "on" ) : QLatin1String( "off" ) )
				.arg( s.shoreRed ? QLatin1String( " (red nearest)" ) : QLatin1String( "" ) )
			: QString() )
		// lane WATERFOAMDETAIL1: the edge's law
		+ ( wwLookOn( WwLook::WaterSoftEdge )
			? ( s.edgeOld ? QStringLiteral( ", edge law old (red)" ) : QStringLiteral( ", edge law contact 35-105 u" ) )
			: QString() )
		// lane WATERFOAMDETAIL1: the foam's law and what drove it at the last draw
		+ ( wwLookOn( WwLook::ShoreFoam )
			? ( s.foamOld ? QStringLiteral( ", foam law old (red)" )
				: [&s]() {
					float windMs = 0.0f, swellM = 0.0f;
					foamDrive( s, windMs, swellM );
					return QStringLiteral( ", foam law wave: wind %1 m/s%2, sea swell %3 m" ).arg( double( windMs ), 0, 'f', 2 )
						.arg( s.foamWind >= 0 ? QStringLiteral( " (pinned %1)" ).arg( s.foamWind ) : QString() )
						.arg( double( swellM ), 0, 'f', 3 );
				}() )
			: QString() )
		// lane WATERWAVES1: said only while the swell is on, so the census line with the row off is as before
		+ ( wwLookOn( WwLook::WaterSwell )
			? QStringLiteral( ", swell on%1" ).arg( s.wavesRed == 1 ? QLatin1String( " (red flat)" )
				: s.wavesRed == 2 ? QLatin1String( " (red noshore)" ) : s.wavesRed == 4 ? QLatin1String( " (red alias)" )
				: QLatin1String( "" ) )
			: QString() )
		// lane WATERCOMBINE1: which looks the one twin draws this frame
		+ ( wwCellWaterLookOn() ? QStringLiteral( ", looks:%1%2%3%4%5" )
				.arg( wwLookOn( WwLook::WaterDepth ) ? QLatin1String( " depth" ) : QLatin1String( "" ) )
				.arg( wwLookOn( WwLook::WaterCaustics ) ? QLatin1String( " caustics" ) : QLatin1String( "" ) )
				.arg( wwLookOn( WwLook::WaterSoftEdge ) ? QLatin1String( " edge" ) : QLatin1String( "" ) )
				.arg( wwLookOn( WwLook::ShoreFoam ) ? QLatin1String( " foam" ) : QLatin1String( "" ) )
				.arg( wwLookOn( WwLook::WaterSwell ) ? QLatin1String( " swell" ) : QLatin1String( "" ) )
			: QString() );
}
