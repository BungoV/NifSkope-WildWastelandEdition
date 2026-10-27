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
#include "cellunits.h"	// lane WATERREFL1: the noise layers' moment textures
#include "cloudmap.h"	// lane WATERREFL1: the weather's clouds in the reflected sky
#include "cellocean.h"	// lane WATERLOOK1: the wind's wave spectrum
#include "cellunderwater.h"	// lane WATERUNDER3: the sun under water
#include "waterplanefit.h"	// lane WATERUNDER5: the placed-water polygon, mean plane and room test
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "model/nifmodel.h"
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
#include <cstdio>
#include <vector>
#include <array>

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
	// lane WATERUNDER1: the shapes' world surfaces, measured once per change of shapeBody (WATERUNDER5: polygons)
	quint64 gen = 1, planesGen = 0;
	std::vector<WwWaterPoly> planes;
	// lane WATERUNDER5: the last room test (eye, surface, shape count) and its answer
	float roofKey[4] = { 1e30f, 1e30f, 1e30f, 1e30f };
	int roofShapes = -1;
	quint64 roofGen = 0;
	bool roofed = false;
};

struct WaterState
{
	QHash<const void *, WaterDoc> docs;
	QHash<const void *, quint32> defaultForm;	// lane WATERUNDER5: the worldspace's default WATR (its water reads as sea)
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

/* lane WATERREFL1: a noise layer's slope second moments for LEAN mapping (Olano and Baker 2010): (t.x^2, t.y^2) of
 * the decoded slope t = 2 texel - 1 at level 0, mipmapped, so the shader gets back the variance its filter averaged
 * away. Built once per renderer and texture (rebuilt if the cache hands the path another texture), bound on unit */
void bindWaterMoment( NifSkopeOpenGLContext::GLFunctions * fn, const void * renderer, const QString & path, GLuint src,
	int unit, float aniso )
{
	struct Moment { GLuint src = 0, tex = 0; };
	static QHash<QPair<const void *, QString>, Moment> cache;
	Moment & m = cache[qMakePair( renderer, path )];
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	if ( !m.tex || m.src != src ) {
		GLint w = 0, h = 0, pack = 0;
		std::vector<float> px;
		if ( src ) {
			fn->glBindTexture( GL_TEXTURE_2D, src );
			fn->glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w );
			fn->glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h );
		}
		if ( w > 0 && h > 0 && w <= 4096 && h <= 4096 ) {
			px.resize( size_t( w ) * size_t( h ) * 4 );
			fn->glGetIntegerv( GL_PIXEL_PACK_BUFFER_BINDING, &pack );
			fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
			fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
			fn->glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, px.data() );
			fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, GLuint( pack ) );
		} else {
			w = h = 1;
			px.assign( 4, 0.5f );	// a flat layer: no variance
		}
		std::vector<float> mo( size_t( w ) * size_t( h ) * 2 );
		for ( size_t i = 0; i < mo.size() / 2; i++ ) {
			const float tx = px[i * 4] * 2.0f - 1.0f, ty = px[i * 4 + 1] * 2.0f - 1.0f;
			mo[i * 2] = tx * tx;
			mo[i * 2 + 1] = ty * ty;
		}
		if ( !m.tex )
			fn->glGenTextures( 1, &m.tex );
		m.src = src;
		fn->glBindTexture( GL_TEXTURE_2D, m.tex );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RG32F, w, h, 0, GL_RG, GL_FLOAT, mo.data() );
		fn->glGenerateMipmap( GL_TEXTURE_2D );
		std::fprintf( stderr, "water moments: %s %dx%d\n", qPrintable( path ), int( w ), int( h ) );
	}
	fn->glBindTexture( GL_TEXTURE_2D, m.tex );
	// sampled as the layer is (lane WATER2's sampler), so the moment's footprint is the normal's
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	fn->glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso );
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
	d.gen++;
}

void wwCellWaterForget( const void * nif, int block )
{
	auto d = st().docs.find( nif );
	if ( d != st().docs.end() ) {
		d->shapeBody.remove( block );
		d->gen++;
	}
}

void wwCellWaterRemap( const void * nif, const WwBlockMap & m )
{
	auto d = st().docs.find( nif );
	if ( d != st().docs.end() ) {
		wwRemapKeys( d->shapeBody, m );
		d->gen++;
	}
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
	return wwLookOn( WwLook::WaterRefl );	// lane WATERREFL1: every body reflects the scene (lakes' WATR flag is 0)
}

bool wwCellWaterIsShape( const void * nif, int block )
{
	const WaterState & s = st();
	const auto it = s.docs.constFind( nif );
	return it != s.docs.constEnd() && it->shapeBody.contains( block );
}

bool wwCellWaterHasShapes( const void * nif )
{
	const WaterState & s = st();
	const auto it = s.docs.constFind( nif );
	return it != s.docs.constEnd() && !it->shapeBody.isEmpty();
}

void wwCellWaterDrive( float & windMs, float & swellM )
{
	foamDrive( st(), windMs, swellM );
}

void wwCellWaterSurfConsts( float surfK[4], float shoreK2[4], float shoreK3[4] )
{
	float windMs = 0.0f, swellM = 0.0f;
	foamDrive( st(), windMs, swellM );
	const float k[4] = { 8.0f, 0.78f, 7.0f, 0.9f };
	const float k2[4] = { windMs, swellM, 40.0f, 0.6f };
	const float k3[4] = { 0.25f, 0.78f, 0.35f, 0.05f };
	for ( int i = 0; i < 4; i++ ) {
		surfK[i] = k[i];
		shoreK2[i] = k2[i];
		shoreK3[i] = k3[i];
	}
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
	if ( prog->uniLocation( "waterClearOn" ) >= 0 ) {	// lane WATERLOOK1 step 8: the Water clear colour row
		prog->uni1b( "waterClearOn", wwLookOn( WwLook::WaterClear ) );
		WW_SWITCH_STATIC( QByteArray, cl, wwEnv( "WW_CELL_WATER_CLEAR_RED" ).trimmed() );
		prog->uni1i( "waterClearRed", cl == "noscatter" ? 1 : cl == "nodown" ? 2 : cl == "fast" ? 3 : 0 );
	}
	if ( prog->uniLocation( "waterSkyVisOn" ) >= 0 ) {	// lane WATERLOOK1 step 10: the Water sky visibility row
		prog->uni1b( "waterSkyVisOn", wwLookOn( WwLook::WaterSkyVis ) );
		WW_SWITCH_STATIC( QByteArray, sv, wwEnv( "WW_CELL_WATER_SKYVIS_RED" ).trimmed() );
		prog->uni1i( "waterSkyVisRed", sv == "one" ? 1 : sv == "nosun" ? 2 : 0 );
	}
	if ( prog->uniLocation( "waterSurfOn" ) >= 0 ) {	// lane WATERLOOK1 step 11: the Surf row
		/* an 8 s swell (the period of a coastal swell; WATERFOAMDETAIL1 gives the height), breaker index 0.78 (McCowan
		 * 1894), 7 waves a group, the foam 0.9 opaque */
		prog->uni1b( "waterSurfOn", wwLookOn( WwLook::WaterSurf ) );
		WW_SWITCH_STATIC( QByteArray, sr, wwEnv( "WW_CELL_WATER_SURF_RED" ).trimmed() );
		prog->uni1i( "waterSurfRed", ( sr.contains( "noshoal" ) ? 1 : 0 ) | ( sr.contains( "nobreak" ) ? 2 : 0 )
			| ( sr.contains( "regular" ) ? 4 : 0 ) | ( sr.contains( "alias" ) ? 8 : 0 ) );
		float sk[4], k2[4], k3[4];
		wwCellWaterSurfConsts( sk, k2, k3 );	// step 14: shared with the land's Wave run-up row
		prog->uni4f( "waterSurfK", FloatVector4( sk[0], sk[1], sk[2], sk[3] ) );
	}
	if ( prog->uniLocation( "waterContactOn" ) >= 0 ) {	// lane WATERLOOK1 step 12: the Contact foam row
		/* the foam band 4 Hs wide (e-fold), each strike's foam lives 2 wave periods, the wind sea's period from its
		 * height at a steepness Hs / L of 0.04 (a developing sea), the foam 0.9 opaque */
		prog->uni1b( "waterContactOn", wwLookOn( WwLook::WaterContact ) );
		WW_SWITCH_STATIC( QByteArray, cr, wwEnv( "WW_CELL_WATER_CONTACT_RED" ).trimmed() );
		prog->uni1i( "waterContactRed", ( cr.contains( "nodepth" ) ? 1 : 0 ) | ( cr.contains( "flat" ) ? 2 : 0 )
			| ( cr.contains( "still" ) ? 4 : 0 ) | ( cr.contains( "noshorecut" ) ? 8 : 0 ) );
		prog->uni4f( "waterContactK", FloatVector4( 4.0f, 2.0f, 0.04f, 0.9f ) );
	}
	if ( prog->uniLocation( "waterRiverFoamOn" ) >= 0 ) {	// lane WATERLOOK1 step 15: the River foam row
		/* foam lives 120 s, covers 12% where the source is full, half of that at 0.4 u/s (the bake's median river);
		 * streaks 24 u wide and 8 times as long, edges 0.3 sd soft, the gathering capped at C T 0.8; 0.85 opaque */
		prog->uni1b( "waterRiverFoamOn", wwLookOn( WwLook::WaterRiverFoam ) );
		WW_SWITCH_STATIC( QByteArray, rr, wwEnv( "WW_CELL_WATER_RIVERFOAM_RED" ).trimmed() );
		prog->uni1i( "waterRiverFoamRed", ( rr.contains( "nodiv" ) ? 1 : 0 ) | ( rr.contains( "still" ) ? 2 : 0 )
			| ( rr.contains( "iso" ) ? 4 : 0 ) | ( rr.contains( "alias" ) ? 8 : 0 ) );
		prog->uni4f( "waterRiverFoamK", FloatVector4( 120.0f, 0.12f, 0.4f, 0.85f ) );
		prog->uni4f( "waterRiverFoamK2", FloatVector4( 24.0f, 8.0f, 0.3f, 0.8f ) );
	}
	if ( prog->uniLocation( "waterUnderOn" ) >= 0 ) {	// lane WATERLOOK1 step 16: the Underwater light row (Snell's window)
		prog->uni1b( "waterUnderOn", wwLookOn( WwLook::WaterUnderLight ) );
		WW_SWITCH_STATIC( QByteArray, ur, wwEnv( "WW_CELL_WATER_UNDERLIGHT_RED" ).trimmed() );
		prog->uni1i( "waterUnderRed", ( ur.contains( "nosnell" ) ? 1 : 0 ) | ( ur.contains( "oldsun" ) ? 2 : 0 ) );
		float to[3], col[3];	// lane WATERUNDER3: the underwater pass's sun, one source (cellunderwater.h)
		wwCellUnderwaterSun( scene, to, col );
		prog->uni3f( "waterUnderSun", col[0], col[1], col[2] );
		prog->uni3f( "waterUnderSunTo", to[0], to[1], to[2] );
	}
	if ( prog->uniLocation( "waterCauTime" ) >= 0 ) {	// lane WATERCAUSTICS1: fo4_waterlook only
		prog->uni1b( "waterCauOn", wwLookOn( WwLook::WaterCaustics ) );	// lane WATERCOMBINE1: its own row
		WW_SWITCH_STATIC( QByteArray, cr, wwEnv( "WW_CELL_WATER_CAUSTICS_RED" ).trimmed() );
		prog->uni1i( "waterCauRed", cr == "nofade" ? 1 : cr == "noshadow" ? 2 : cr == "still" ? 3 : cr == "flat" ? 4
			: cr == "lattice" ? 5 : 0 );	// lane CAUSTICPATTERN1: lattice = WATERCAUSTICS1's four-sine law
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
	GLint noiseTex[3] = { 0, 0, 0 };	// lane WATERREFL1: the layers as bound, for their moments
	float noiseAniso = 1.0f;
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
		noiseAniso = ( s.red & 128 ) ? 1.0f : std::min( 16.0f, maxAniso );
		glGetIntegerv( GL_TEXTURE_BINDING_2D, &noiseTex[k] );
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
	const bool ssrOn = !inSsr && ( b.rec.ssr || wwLookOn( WwLook::WaterRefl ) ) && wwCellSsrWaterTextures( scene, ssrRaw, ssrFin );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_2D, ssrOn ? ssrRaw : 0 );
	prog->uni1i_l( prog->uniLocation( "waterSsrRaw" ), unit++ );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_2D, ssrOn ? ssrFin : 0 );
	prog->uni1i_l( prog->uniLocation( "waterSsrFin" ), unit++ );
	prog->uni1b( "waterSsrOn", ssrOn );
	unit += wwCellWaterInfoUniforms( scene, unit );	// lane WATERDATA1: units 7..9 when the program reads them (WATERFLOWBAKE1's flow: cell units 66/67)
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
		float sk[4], k2[4], k3[4];
		wwCellWaterSurfConsts( sk, k2, k3 );	// lane WATERLOOK1 step 14: shared with the land's Wave run-up row
		prog->uni1i( "waterShoreLaw", s.foamOld ? 0 : 1 );
		prog->uni4f( "waterShoreK2", FloatVector4( k2[0], k2[1], k2[2], k2[3] ) );
		prog->uni4f( "waterShoreK3", FloatVector4( k3[0], k3[1], k3[2], k3[3] ) );
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
	if ( prog->uniLocation( "waterReflOn" ) >= 0 ) {
		/* lane WATERREFL1 (fo4_waterlook.prog, water_refl.glsl). F0 0.0204 = water at IOR 1.333; the glare's shoulder at
		 * 16 x the sun colour (the WATR sun magnitudes run 4.75 creek .. 17.6 ocean); footprint clamp 0.18 (Tokuyoshi and
		 * Kaplanyan 2019); the march stays sharp below alpha 0.015 and is its blur from 0.12 */
		const bool reflOn = wwLookOn( WwLook::WaterRefl ), glintOn = wwLookOn( WwLook::WaterGlint );
		WW_SWITCH_STATIC( QByteArray, rr, wwEnv( "WW_CELL_WATER_REFL_RED" ).trimmed() );
		WW_SWITCH_STATIC( QByteArray, gr, wwEnv( "WW_CELL_WATER_GLINT_RED" ).trimmed() );
		prog->uni1b( "waterReflOn", reflOn );
		prog->uni1b( "waterGlintOn", glintOn );
		prog->uni1i( "waterReflRed", rr == "record" ? 1 : rr == "farcolour" ? 2 : rr == "sharp" ? 4 : 0 );
		prog->uni1i( "waterGlintRed", gr == "game" ? 1 : gr == "nolean" ? 2 : gr == "noshoulder" ? 4
			: gr == "noglitter" ? 8 : gr == "nodisc" ? 16 : gr == "pixel" ? 32 : gr == "nolodblend" ? 64 : 0 );	// WATERLOOK1 step 9
		prog->uni4f( "waterGlintK", FloatVector4( 16.0f, 0.0204f, 0.18f, 0.015f ) );
		static const char * const momentUni[3] = { "waterMoment0", "waterMoment1", "waterMoment2" };
		for ( int k = 0; k < 3; k++ ) {
			const int mu = WwCellUnit::WaterMoment0 + k;
			if ( reflOn || glintOn ) {
				bindWaterMoment( fn, r, b.noise[k].isEmpty() ? QStringLiteral( "#flat" ) : b.noise[k], GLuint( noiseTex[k] ),
					mu, noiseAniso );
			} else {
				fn->glActiveTexture( GLenum( GL_TEXTURE0 + mu ) );
				fn->glBindTexture( GL_TEXTURE_2D, 0 );
			}
			prog->uni1i_l( prog->uniLocation( momentUni[k] ), mu );
		}
		// the latest cloud map (built by the sky pass; never rebuilt here, mid-scene)
		const WwCloudMap & cm = wwCloudMap();
		const bool clouds = reflOn && cm.valid && cm.tex;
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::WaterCloudMap ) );
		fn->glBindTexture( GL_TEXTURE_2D, clouds ? cm.tex : 0 );
		prog->uni1i( "waterCloudMap", WwCellUnit::WaterCloudMap );
		prog->uni4f( "waterCloudGeo", FloatVector4( cm.centerX, cm.centerY, std::max( cm.halfExtent, 1.0f ), clouds ? 1.0f : 0.0f ) );
		prog->uni4f( "waterCloudZ", FloatVector4( cm.band[WwCloudLow].baseZ, cm.band[WwCloudMid].baseZ, cm.band[WwCloudHigh].baseZ, 0.0f ) );
		prog->uni4f( "waterCloudCov", FloatVector4( cm.band[WwCloudLow].coverage, cm.band[WwCloudMid].coverage,
			cm.band[WwCloudHigh].coverage, 0.0f ) );
		prog->uni3f( "waterCloudTint", cm.band[WwCloudLow].tint[0], cm.band[WwCloudLow].tint[1], cm.band[WwCloudLow].tint[2] );
	}
	// lane WATERLOOK1 (water_ocean.glsl): the wind's spectrum, built this frame by wwCellOceanPass; the tiles run along the wind
	if ( prog->uniLocation( "waterOceanOn" ) >= 0 )
		wwCellOceanUniforms( scene, b.rec.windDirection[0] );
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
	// lane WATERLOOK1 step 2: and its displaced form while the spectrum is built (the Ocean waves row, red not old/flat)
	if ( !wwCellWaterLookOn() )
		return "fo4_water.prog";
	return wwCellOceanDispOn() ? "fo4_waterlookd.prog" : "fo4_waterlook.prog";
}

bool wwCellWaterLookOn()
{
	return wwLookOn( WwLook::WaterDepth ) || wwLookOn( WwLook::WaterCaustics ) || wwLookOn( WwLook::WaterSoftEdge )
		|| wwLookOn( WwLook::ShoreFoam ) || wwLookOn( WwLook::WaterSwell ) || wwLookOn( WwLook::WaterRefl )
		|| wwLookOn( WwLook::WaterGlint )	// lane WATERREFL1
		|| wwLookOn( WwLook::WaterOcean )	// lane WATERLOOK1
		|| wwLookOn( WwLook::WaterClear )	// lane WATERLOOK1 step 8
		|| wwLookOn( WwLook::WaterSkyVis )	// lane WATERLOOK1 step 10
		|| wwLookOn( WwLook::WaterSurf )	// lane WATERLOOK1 step 11
		|| wwLookOn( WwLook::WaterContact )	// lane WATERLOOK1 step 12
		|| wwLookOn( WwLook::WaterRiverFoam )	// lane WATERLOOK1 step 15
		|| wwLookOn( WwLook::WaterUnderLight );	// lane WATERLOOK1 step 16
}

bool wwCellWaterRecordOf( const void * nif, quint32 form, WwWaterRecord & rec )
{
	const WaterState & s = st();
	const auto it = s.docs.constFind( nif );
	if ( it == s.docs.constEnd() || it->bodies.isEmpty() )
		return false;
	const int i = it->byForm.value( form, 0 );
	rec = it->bodies[i].rec;
	return true;
}

void wwCellWaterSetDefault( const void * nif, quint32 form )
{
	st().defaultForm.insert( nif, form );
}

bool wwCellWaterPlaneOver( Scene * scene, float x, float y, float z, float maxDepth, float & height, WwWaterRecord & rec,
	bool oldRule, bool * sea, bool * roofed )
{
	WaterState & s = st();
	if ( sea )
		*sea = false;
	if ( roofed )
		*roofed = false;
	if ( !scene || !scene->nifModel )
		return false;
	const auto it = s.docs.find( scene->nifModel );
	if ( it == s.docs.end() || it->shapeBody.isEmpty() )
		return false;
	WaterDoc & d = *it;
	if ( d.planesGen != d.gen ) {
		d.planes.clear();
		bool whole = true;
		std::vector<float> tri;
		for ( auto b = d.shapeBody.constBegin(); b != d.shapeBody.constEnd(); ++b ) {
			auto * sh = dynamic_cast<Shape *>( scene->findNode( scene->nifModel, scene->nifModel->getBlockIndex( b.key() ) ) );
			if ( !sh || sh->verts.isEmpty() ) {
				whole = false;	// not built yet: measured again next frame
				continue;
			}
			const Transform & w = sh->worldTrans();
			tri.clear();
			for ( const Triangle & t : sh->triangles ) {
				if ( t[0] >= sh->verts.size() || t[1] >= sh->verts.size() || t[2] >= sh->verts.size() )
					continue;
				for ( int k = 0; k < 3; k++ ) {
					const Vector3 q = w * sh->verts.at( t[k] );
					tri.insert( tri.end(), { q[0], q[1], q[2] } );
				}
			}
			d.planes.push_back( wwWaterPolyFrom( tri, b.value() ) );
		}
		if ( whole )
			d.planesGen = d.gen;
	}
	float h = 0.0f;
	const int best = wwWaterPolyPick( d.planes, x, y, z, maxDepth, oldRule, h );
	if ( best < 0 )
		return false;
	if ( !oldRule ) {
		// the room test: a non-water surface between the camera and the water above it = a room below the water
		const float key[4] = { x, y, z, h };
		if ( d.roofGen != d.planesGen || d.roofShapes != int( scene->shapes.size() ) || !std::equal( key, key + 4, d.roofKey ) ) {
			std::copy( key, key + 4, d.roofKey );
			d.roofGen = d.planesGen;
			d.roofShapes = int( scene->shapes.size() );
			d.roofed = false;
			std::vector<float> tri;
			for ( Shape * sh : scene->shapes ) {
				if ( d.roofed )
					break;
				if ( !sh || sh->isHidden() || sh->verts.isEmpty() || d.shapeBody.contains( sh->id() ) )
					continue;
				const BoundSphere bs = sh->bounds();
				const float dx = bs.center[0] - x, dy = bs.center[1] - y;
				if ( dx * dx + dy * dy > bs.radius * bs.radius || bs.center[2] - bs.radius >= h || bs.center[2] + bs.radius <= z )
					continue;
				const Transform & w = sh->worldTrans();
				tri.clear();
				for ( const Triangle & t : sh->triangles ) {
					if ( t[0] >= sh->verts.size() || t[1] >= sh->verts.size() || t[2] >= sh->verts.size() )
						continue;
					for ( int k = 0; k < 3; k++ ) {
						const Vector3 q = w * sh->verts.at( t[k] );
						tri.insert( tri.end(), { q[0], q[1], q[2] } );
					}
				}
				d.roofed = wwWaterRoofOver( tri.data(), tri.size() / 9, x, y, z, h );
			}
		}
		if ( d.roofed ) {
			if ( roofed )
				*roofed = true;
			return false;
		}
	}
	height = h;
	rec = d.bodies[d.planes[size_t( best )].body].rec;
	if ( sea )
		*sea = !oldRule && rec.form != 0 && rec.form == s.defaultForm.value( scene->nifModel, 0 );
	return true;
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
				+ ( wwLookOn( WwLook::WaterRefl ) ? QLatin1String( " reflections" ) : QLatin1String( "" ) )
				+ ( wwLookOn( WwLook::WaterGlint ) ? QLatin1String( " glare" ) : QLatin1String( "" ) )
				+ ( wwLookOn( WwLook::WaterOcean ) ? QLatin1String( " ocean" ) : QLatin1String( "" ) )
				+ ( wwLookOn( WwLook::WaterClear ) ? QLatin1String( " clear" ) : QLatin1String( "" ) )
				+ ( wwLookOn( WwLook::WaterSkyVis ) ? QLatin1String( " skyvis" ) : QLatin1String( "" ) )
				+ ( wwLookOn( WwLook::WaterSurf ) ? QLatin1String( " surf" ) : QLatin1String( "" ) )
				+ ( wwLookOn( WwLook::WaterContact ) ? QLatin1String( " contact" ) : QLatin1String( "" ) )
				+ ( wwLookOn( WwLook::WaterRiverFoam ) ? QLatin1String( " riverfoam" ) : QLatin1String( "" ) )
				+ ( wwLookOn( WwLook::WaterUnderLight ) ? QLatin1String( " underlight" ) : QLatin1String( "" ) )
			: QString() )
		// lane WATERLOOK1: what the spectrum built (said only while its row is on)
		+ ( wwCellOceanEcho().isEmpty() ? QString() : QStringLiteral( "; " ) + wwCellOceanEcho() );
}
