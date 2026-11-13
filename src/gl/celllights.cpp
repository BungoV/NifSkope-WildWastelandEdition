/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "celllights.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "features/lights/cellclustergl.h"
#include "gl/alphabgsm.h"	// the side passes' alpha-test census
#include "features/reflections/cellssr.h"
#include "features/air/cellvolair.h"
#include "features/shadows/shadowset.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/glproperty.h"	// lane GAPFIX1: alpha-tested cube casters read the base map
#include "gl/renderer.h"
#include "features/exposure/cellhdr.h"
#include "features/exposure/exposure.h"	// lane MODULES1 (exposure): the eye, the exposure law, the tone curve red
#include "features/post/cellbloom.h"	// lane BLOOM1b
#include "features/ao/cellaodecalgl.h"	// lane AODECAL1
#include "features/ao/cellaomethod.h"
#include "features/near/cellcull.h"	// wwCellDecalMappedHas
#include "features/shared/cellpasstime.h"	// sub-stage timers
#include "features/near/celldrawstate.h"
#include "gl/lookdevstage.h"
#include "features/water/cellwater.h"	// water casts no cube shadow
#include "features/water/cellunderwater.h"	// the WATR INAM imagespace under water
#include "features/water/esmwater.h"
#include "features/gi/cellwatergi.h"	// water reads no probe GI
#include "features/lights/celllampshadow.h"	// lane LAMPSHADOW1
#include "features/lights/cubefacecull.h"	// lane PERFFIX2 b: a caster draws only into the cube faces it can reach
#include "features/air/airlampatlas.h"	// lane MODULES1 route (a): the air's lamp atlas draws these casters
#include "features/lights/lampslice.h"	// lane PERFFIX2 b: the lamp store's first stores spread over frames
#include "gl/celllooks.h"	// lane GAPLOOK1
#include "features/post/celllensflare.h"	// lane LIGHTS3 (LENSFLARE1): its echo
#include "features/lights/cellfxlit.h"	// lane LIGHTS4: a placement's emittance colour
#include "gamemanager.h"	// lane SUNCELL1: the weather imagespace's LUT
#include "features/weather/esmweather.h"	// lane WEATHERBLEND2: wwLutBlendN
#include "features/gi/probealbedo.h"	// lane LIGHTS5 (GOBO1): a spot's pattern
#include "recx.h"	// lane LIGHTS5 (FOGSCALE1): fog.scale
#include "features/lights/cellnifxlights.h"	// lane NIFXLIGHT1: a fixture mesh's .nifx lights
#include "features/gi/rtddgicell.h"	// lane RTXGI1 M2: the ray-traced GI volume's read (rtddgiCellUniforms)
#include "gl/celluniprof.h"	// lane PERFFIX1: WW_CELL_UNI_PROF
#include "gl/cellunirec.h"	// lane PERFFIX1: the cell-uniform class record
#include "features/clouds/cloudvol.h"	// lane LIGHTNING2: a storm's flash on the exterior ambient (wwLightningGround)
#include "cellworldanim.h"	// lane NIFXLIGHT3: the world clock a .nifx light's dimmer follows
#include "features/sky/atmosphere.h"	// lane FIX1101A (atmo): wwAtmoWbGain, the test white balance
#include "features/sky/cellsunlit.h"	// lane INTSKY1: the sunlit interior rule
#include "features/lights/cellflickclock.h"	// lane PERFFIX2 a: the flicker's phase on the world clock

#include <QDateTime>
#include <QDir>	// lane INTSKY1: the Cells file in the census
#include <QJsonValue>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSettings>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include "cellunits.h"
#include "celluniform.h"	// lane CELLUNI1
#include <QOpenGLContext>
#include <cstring>
#include <map>
#include <vector>
#include <unordered_map>	// lane PERFFIX1: the cell-uniform class records
#include "features/lights/celllights_p.h"	// lane MODULES1 (lights): the state the lamps share
#include "features/exposure/physunits.h"	// lane PHYSUNITS1: real units
#include "gl/dalcfit.h"	// lane SKYLIGHT1: the up / down poles read exactly
#include "features/overlays/debugview.h"	// lane DEBUGVIEW1: a debug view is a probe pass
#include "features/vkframe/vkpresent.h"	// lane VKFAR1 (row G3): the far dots on the Vulkan view
#include "features/vkframe/vkobjpipe.h"	// lane VKOBJ4: the light buffers as stores (objBlTexelFill)
#include "features/near/cellbindless.h"	// lane VKOBJ4: WwBl::vkShadeOn

bool wwFogLeakShaderDefine();	// glcontext.cpp: WW_CELL_FOGLEAK in the cell programs (the Vulkan twin's define)

//! lane PHYSUNITS1: an interior's ambient, directional and fog factor (its lamps' median x Game Light Brightness); 1 else
static float physInteriorK( const WwCellLighting * L )
{
	return ( L && L->interior && L->physAmbK > 0.0f && wwPhysSkyOn() ) ? L->physAmbK * wwPhysGameLights() : 1.0f;
}

using namespace WwCellLightsP;

// lane PRTP6 (cellwatergi.h): the GI switches a water draw zeroed, per program
static WwWaterGiGuard & waterGi()
{
	static WwWaterGiGuard g;
	return g;
}

namespace WwCellLightsP
{

ClState & st()
{
	static ClState s;
	// lane TESTDRIVER1: a live set of these switches re-reads them (pins and or-ed reds back to their defaults first)
	if ( !s.loaded || wwSwitchMovedFor( s.liveGen, { "WW_CELL_" } ) ) {
		s.loaded = true;
		s.liveGen = wwSwitchGen();
		{
			const ClState d {};
			s.pinned = d.pinned;
			s.farPinned = d.farPinned;
			s.isPinned = d.isPinned;
			s.passPinned = d.passPinned;
			s.red = d.red;
			s.passRed = d.passRed;
		}
		const QByteArray pin = wwEnv( "WW_CELL_LIT" );
		if ( !pin.isEmpty() ) {
			s.pinned = true;
			s.on = pin.trimmed() != "0";
		} else {
			// lane DEFAULTS1: ships on (new key so an old OFF does not stick); WW_DEFAULTS1_RED=1 = the old row
			s.on = wwEnvInt( "WW_DEFAULTS1_RED" ) == 1 ? QSettings().value( QStringLiteral( "WW/CellLights" ), false ).toBool()
				: QSettings().value( QStringLiteral( "WW/CellLights2" ), true ).toBool();
		}
		// FARWATER1 item 6 (bungo 11-11): the retired pin WW_CELL_GI and its row are gone; the PRT red draws the old GI whole
		s.giOn = wwCellPrtOn();
		const QByteArray farPin = wwEnv( "WW_CELL_FAR" );
		if ( !farPin.isEmpty() ) {
			s.farPinned = true;
			s.farOn = farPin.trimmed() != "0";
		} else {
			s.farOn = QSettings().value( QStringLiteral( "WW/CellFar" ), false ).toBool();
		}
		const QByteArray flPin = qgetenv( "WW_CELL_FLICKER" ).trimmed();	// lane LIGHTFLICKER1
		s.flickerPinned = !flPin.isEmpty();
		s.flickerOn = s.flickerPinned ? flPin != "0" : wwEnvInt( "WW_DEFAULTS1_RED" ) == 1	// DEFAULTS1
			? QSettings().value( QStringLiteral( "WW/CellFlicker" ), false ).toBool()
			: QSettings().value( QStringLiteral( "WW/CellFlicker2" ), true ).toBool();
		s.flickerRed = qEnvironmentVariableIntValue( "WW_CELL_FLICKER_RED" ) != 0;
		s.flickerStep = std::max( 0.0f, qEnvironmentVariable( "WW_CELL_FLICKER_STEP" ).toFloat() );
		s.flickerClockRed = wwEnvInt( "WW_CELL_FLICKER_CLOCK_RED" ) != 0;
		s.flickerDump = qEnvironmentVariable( "WW_CELL_FLICKER_DUMP" );
		const QByteArray farRed = wwEnv( "WW_CELL_FAR_RED" ).trimmed();
		s.farRed = farRed == "nosurfel" ? 1 : farRed == "noblend" ? 2 : farRed == "flipside" ? 4 : 0;
		s.probe = wwEnvInt( "WW_CELL_LIT_PROBE" );
		const QByteArray isPin = wwEnv( "WW_CELL_IS" );
		if ( !isPin.isEmpty() ) {
			s.isPinned = true;
			s.isOn = isPin.trimmed() != "0";
		} else {
			s.isOn = wwEnvInt( "WW_DEFAULTS1_RED" ) == 1	// DEFAULTS1
				? QSettings().value( QStringLiteral( "WW/CellImageSpace" ), false ).toBool()
				: QSettings().value( QStringLiteral( "WW/CellImageSpace2" ), true ).toBool();
		}
		const QByteArray isRed = wwEnv( "WW_CELL_IS_RED" ).trimmed();
		s.isRed = isRed == "nolut" ? 1 : isRed == "noexp" ? 2 : isRed == "nograde" ? 4 : isRed == "nobloom" ? 8
			: isRed == "nofx" ? 16 : 0;
		const QByteArray shPin = wwEnv( "WW_CELL_SHADOW" ).trimmed();
		s.shadowPinned = !shPin.isEmpty();
		s.shadowOn = s.shadowPinned ? shPin != "0" : QSettings().value( QStringLiteral( "WW/CellShadow" ), true ).toBool();
		s.shadowRed = wwEnv( "WW_CELL_SHADOW_RED" ).trimmed() == "noshadow";
		s.fogProbe = wwEnvInt( "WW_CELL_FOG_PROBE" );
		const QByteArray giAmb = wwEnv( "WW_CELL_GI_AMB" ).trimmed();
		s.giAmb = giAmb == "keep" ? 0 : giAmb == "off" ? 2 : giAmb == "max" ? 3 : giAmb == "asgi" ? 4 : 1;	// unset = replace
		s.giFill = wwEnv( "WW_CELL_GI_FILL" ).trimmed() != "0";
		const QByteArray red = wwEnv( "WW_CELL_LIT_RED" ).trimmed();
		if ( red == "linear" )
			s.red = 1;
		else if ( red == "axis" )
			s.red = 2;
		else if ( red == "nodalc" )
			s.red = 4;
		else if ( red == "lambert" )
			s.red = 8;	// the diffuse back to Lambert
		else if ( red == "normalised" )
			s.red = 16;	// the textbook (normalized) Oren-Nayar azimuth
		else if ( red == "norim" )
			s.red = 32;	// the back-light rim term dropped
		else if ( red == "rimflags" )
			s.red = 64;	// the lights' No Rim / Ignore Roughness flags ignored
		else if ( red == "ambientlit" )
			s.red = 128;	// the Ambient Only lights drawn as ordinary lights
		else if ( red == "hemiomni" )
			s.red = 256;	// hemisphere and box lights drawn as plain omni lights (the cell view applies it)
		else if ( red == "cubeold" )
			s.red = 512;	// the interior cube map at its old scale (squared, x Lambert, default cube)
		else if ( red == "ambientfull" )
			s.red = 1024;	// the Ambient Only lights' ambient adjustment ignored
		else if ( red == "probefx" )
			s.red = 4096;	// effects and refraction shapes drawn into the probe passes again
		else if ( red == "backglow" )
			s.red = 2048;	// MODULES1 R6-1: a material's back light added to the root in the viewport's unit again
		{
			static int backLast = -1;
			const int backNow = ( s.red & 2048 ) ? 1 : 0;
			if ( backNow != backLast ) {
				backLast = backNow;
				std::fprintf( stderr, "cell back light: %s\n", backNow
					? "RED WW_CELL_LIT_RED=backglow: albedo x N.-L x power x the viewport light, added to the root (no exposure, no sun unit)"
					: "the cell sun x albedo x (N.-L x power)^2 x the sun's visibility x a far placement's sky visibility, linear, in the sun's unit" );
			}
		}
		// the lights' Non Specular flag ignored (a bit clear of WW_CELL_LIT_RED's)
		if ( wwEnv( "WW_CELL_SPEC_RED" ).trimmed() == "nonspec" )
			s.red |= 65536;
		else if ( wwEnv( "WW_CELL_SPEC_RED" ).trimmed() == "tint" )
			s.red |= 262144;	// lane GAPFIX1: opaque (deferred) draws tint the lights' specular by the material colour again
		// outdoors the grid's sky takes k x the flat ambient off in sqrt space (the old law)
		if ( wwEnv( "WW_CELL_GISHADE_RED" ).trimmed() == "sqrt" )
			s.red |= 131072;
		// the Pass, by number or by its entry's name
		const QByteArray passPin = wwEnv( "WW_CELL_PASS" ).trimmed();
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
		for ( const QByteArray & r : wwEnv( "WW_CELL_PV_RED" ).split( ',' ) ) {
			const QByteArray t = r.trimmed();
			s.passRed |= t == "direct" ? 1 : t == "nonormal" ? 2 : t == "open" ? 4 : t == "noclamp" ? 8 : t == "nosee" ? 16 : t == "borrow" ? 32 : t == "lendamb" ? 64 : t == "strand" ? 128 : t == "firstslot" ? 256 : t == "tiepick" ? 512 : t == "lonesky" ? 1024 : t == "nofilter" ? 2048 : t == "nolastfill" ? 4096 : 0;
		}
		// lane GIWALL2: a GI voxel with its centre in solid or behind a wall from the surface counts again (GISHADE1's
		// 12 taps short of the centre), in the volumes and the march alike; it picks no march (both paths carry the rule)
		if ( wwEnv( "WW_CELL_GIWALL_RED" ).trimmed().toLower() == "off" )
			s.passRed |= 8192;
	}
	return s;
}

QHash<const void *, Gpu> & gpus()
{
	static QHash<const void *, Gpu> g;
	return g;
}

/* An exterior has no imagespace of its own: the game uses the weather's IMSP at the hour, so isLighting()
 * fills those fields from the Lookdev weather, re-filled when the weather or the hour's keys move. */
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

//! the 256x16 B8G8R8 strip -> 16^3 RGB8, r fastest (as src/cellview.cpp reads it)
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

//! lane WEATHERBLEND2: a weather change re-blends its LUTs every step; decode each file once
bool loadLutCached( const QString & lut, const QString & dataRoot, std::vector<unsigned char> & out )
{
	static QHash<QString, std::vector<unsigned char>> cache;
	const QString k = dataRoot + QLatin1Char( '|' ) + lut;
	auto it = cache.constFind( k );
	if ( it != cache.constEnd() ) {
		out = it.value();
		return !out.empty();
	}
	if ( cache.size() > 32 )
		cache.clear();
	if ( !loadLut( lut, dataRoot, out ) )
		out.clear();
	cache.insert( k, out );
	return !out.empty();
}

//! lane WEATHERBLEND2: the change's up to four LUTs by the game's weights; false when none loaded
bool mixLuts( const WwLookdevIs & w, const QString & dataRoot, std::vector<unsigned char> & out, QString & path )
{
	std::vector<unsigned char> got[4];
	const std::vector<unsigned char> * lut[4] = {};
	float wt[4] = {};
	QStringList parts;
	for ( int k = 0; k < w.lutMix.size() && k < 4; k++ ) {
		if ( !loadLutCached( w.lutMix[k], dataRoot, got[k] ) )
			continue;
		lut[k] = &got[k];
		wt[k] = w.lutW[k];
		parts << QStringLiteral( "%1*%2" ).arg( w.lutMix[k] ).arg( double( w.lutW[k] ), 0, 'f', 4 );
	}
	wwLutBlendN( lut, wt, out );
	path = parts.join( QLatin1Char( '+' ) );
	return !out.empty();
}

const WwCellLighting * isLightingBase( const void * nif, int * stamp )
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
		const bool mix = !w.lutMix.isEmpty();
		QString mixPath;
		const bool gotA = mix ? mixLuts( w, L->dataRoot, a, mixPath ) : loadLut( w.lutA, L->dataRoot, a );
		const bool gotB = !mix && w.t > 0.0f && loadLut( w.lutB, L->dataRoot, b );
		if ( mix && gotA ) {
			e.L.isLut = a;
			e.L.isLutPath = mixPath;
		} else if ( gotA && gotB ) {	// the two keys' LUTs blended: the same as blending their outputs (linear sampling)
			e.L.isLut.resize( a.size() );
			for ( size_t i = 0; i < a.size(); i++ )
				e.L.isLut[i] = (unsigned char) std::lround( float( a[i] ) + ( float( b[i] ) - float( a[i] ) ) * w.t );
			e.L.isLutPath = w.lutA + QStringLiteral( "->" ) + w.lutB;
		} else if ( gotA ) {
			e.L.isLut = a;
			e.L.isLutPath = w.lutA;
		}
		e.stamp++;
		// the whole chain (hdr, grade, LUT blend) for a gate to rebuild
		std::fprintf( stderr, "cell imagespace: exterior=%s hdr=%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g lut=%s%s"
			" cine=%.9g,%.9g,%.9g tint=%.9g,%.9g,%.9g,%.9g lutT=%.9g\n",
			e.L.isName.toLocal8Bit().constData(), double( w.hdr[0] ), double( w.hdr[1] ), double( w.hdr[2] ),
			double( w.hdr[3] ), double( w.hdr[4] ), double( w.hdr[5] ), double( w.hdr[6] ), double( w.hdr[7] ),
			double( w.hdr[8] ), e.L.isLutPath.isEmpty() ? "none" : e.L.isLutPath.toLocal8Bit().constData(),
			( !w.lutA.isEmpty() && !gotA ) ? " (LUT NOT FOUND)" : "", double( w.cine[0] ), double( w.cine[1] ),
			double( w.cine[2] ), double( w.tint[0] ), double( w.tint[1] ), double( w.tint[2] ), double( w.tint[3] ),
			double( ( gotA && gotB ) ? w.t : 0.0f ) );
	}
	if ( stamp )
		*stamp = e.stamp;
	return &e.L;
}

/* lane WATERUNDER3: THE CAMERA UNDER WATER TAKES THE WATR'S INAM IMAGESPACE (the game sets it as the imagespace base
 * data on entering the water). Its tone-map fields, cinematic and tint replace the frame's; Sunlight/Sky Scale (the
 * surfaces' light) and the LUT stay, so the LUT stamp is the base's. Only over a base that has an imagespace. */
struct UwIs
{
	const WwCellLighting * src = nullptr;
	int version = -1, stamp = -1;
	quint32 form = 0;
	WwCellLighting L;
};

const WwCellLighting * isLighting( const void * nif, int * stamp )
{
	int bs = 0;
	const WwCellLighting * L = isLightingBase( nif, &bs );
	if ( stamp )
		*stamp = bs;
	const WwWaterRecord * w = L && L->hasImageSpace ? wwCellUnderwaterImageSpace( nif ) : nullptr;
	if ( !w )
		return L;
	static QHash<const void *, UwIs> m;
	UwIs & u = m[nif];
	const int ver = st().version.value( nif, 0 );
	if ( u.src != L || u.version != ver || u.stamp != bs || u.form != w->imageSpace ) {
		u.src = L;
		u.version = ver;
		u.stamp = bs;
		u.form = w->imageSpace;
		u.L = *L;
		for ( int i : { 0, 1, 2, 3, 4, 5, 8 } )
			u.L.isHdr[i] = w->uwIsHdr[i];
		std::copy( w->uwIsCine, w->uwIsCine + 3, u.L.isCine );
		std::copy( w->uwIsTint, w->uwIsTint + 4, u.L.isTint );
		u.L.isName = QStringLiteral( "underwater:" ) + w->uwIsName;
		std::fprintf( stderr, "cell imagespace: underwater=%s over %s hdr=%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g"
			" cine=%.9g,%.9g,%.9g tint=%.9g,%.9g,%.9g,%.9g\n", w->uwIsName.toLocal8Bit().constData(),
			L->isName.toLocal8Bit().constData(), double( u.L.isHdr[0] ), double( u.L.isHdr[1] ), double( u.L.isHdr[2] ),
			double( u.L.isHdr[3] ), double( u.L.isHdr[4] ), double( u.L.isHdr[5] ), double( u.L.isHdr[6] ),
			double( u.L.isHdr[7] ), double( u.L.isHdr[8] ), double( u.L.isCine[0] ), double( u.L.isCine[1] ),
			double( u.L.isCine[2] ), double( u.L.isTint[0] ), double( u.L.isTint[1] ), double( u.L.isTint[2] ),
			double( u.L.isTint[3] ) );
	}
	return &u.L;
}

}	// namespace

void wwCellLightsPublish( const void * nif, const WwCellLighting & lighting )
{
	ClState & s = st();
	s.docs.insert( nif, lighting );
	s.version.insert( nif, s.nextVersion++ );
	s.nifxStamp.remove( nif );	// lane NIFXLIGHT2
	if ( lighting.nifxFixtures.isEmpty() ) {	// lane NIFXLIGHT1: the tick applies the fixtures to a copy
		s.nifx.remove( nif );
	} else {
		ClNifx & x = s.nifx[nif];
		x.base = lighting;
		x.applied = false;
		x.key.clear();
		x.liveInit = false;	// lane NIFXLIGHT3: the tick lines the new fixtures up with what it has seen
		x.ticked = false;
	}
}

int wwCellLightsNifxStamp( const void * nif )
{
	return st().nifxStamp.value( nif, 0 );
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

std::vector<float> * wwCellGiRgbaEdit( const void * nif )
{
	ClState & s = st();
	auto it = s.gi.find( nif );
	return it == s.gi.end() ? nullptr : &it->rgba;
}

void wwCellGiRgbaEdited( const void * nif )
{
	ClState & s = st();
	s.giFadeVersion.insert( nif, s.nextVersion++ );
}

int wwCellGiVersion( const void * nif )
{
	return st().giVersion.value( nif, 0 );
}

int wwCellLightsVersion( const void * nif )
{
	return st().version.value( nif, 0 );
}

quint64 wwCellLightsStateKey( Scene * scene )
{
	if ( !scene || !scene->renderer )
		return 0;
	const auto it = gpus().constFind( scene->renderer );
	if ( it == gpus().constEnd() )
		return 0;
	const Gpu & g = *it;
	quint64 h = 0xcbf29ce484222325ULL;
	auto mix = [&h]( quint64 v ) { h ^= v + 0x9e3779b97f4a7c15ULL + ( h << 6 ) + ( h >> 2 ); };
	mix( quint64( reinterpret_cast<std::uintptr_t>( g.doc ) ) );
	mix( quint64( g.version ) );
	mix( g.texelStamp );
	mix( g.flickStamp );
	mix( quint64( g.shStamp ) );
	mix( g.shWrites );
	for ( int k = 0; k < kShadowSlots; k++ ) {
		std::uint32_t u;
		std::memcpy( &u, &g.shFade[k], 4 );
		mix( u );
		mix( quint64( std::int64_t( g.shSlot[k] ) ) );
	}
	return h;
}

bool wwCellGiOn()
{
	return st().giOn;
}

bool wwCellPrtOn()
{
	static const bool red = qEnvironmentVariableIntValue( "WW_CELL_PRT_RED" ) == 1;	// lane PRTOFF1: the old draw back (launch)
	return red;
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
	return st().farOn && ( st().farPinned || wwGroupLets( WwGroup::CellLights ) );	// lane OPENCELLFIX2: the group's master
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


/* lane VKFAR1 (row G3): the far dots on the present device's tone frame, cell_fardots' uniforms already set on prog.
 * false: GL draws its points (the reason is in the "vk far:" line). The far cards and the far ground stay on GL. */
static bool farDotsOnVulkan( Renderer * r, NifSkopeOpenGLContext::Program * prog, const WwCellFar * Fr, const std::vector<float> & pos,
	const std::vector<float> & I, const float * row, const float * cam, const float * proj, const GLint * vp, float focal )
{
	auto fn = r->fn;
	GLint drawFb = 0;
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &drawFb );
	const GLuint color = wwCellHdrVkToneColor( GLuint( drawFb ) );
	if ( !color )
		return false;	// not the present device's tone frame: GL's, as before
	static QString said;
	static auto at = std::chrono::steady_clock::now();
	auto say = [&]( const QString & line ) {
		if ( line == said )
			return;
		said = line;
		at = std::chrono::steady_clock::now();
		qInfo().noquote() << line;
	};
	if ( !WwVkPresent::shareWanted( "WW_VKVIEW_FAR_RED" ) ) {
		say( QStringLiteral( "vk far: dots on GL (red WW_VKVIEW_FAR_RED=1 or the view not on Vulkan)" ) );
		return false;
	}
	static WwVkPresent::FarDotsRun t;
	t = WwVkPresent::FarDotsRun();
	std::string why;
	bool drew = false;
	if ( !wwFogLeakShaderDefine() ) {
		WwVkPresent::farDotsOnGl( "WW_CELL_FOGLEAK=0: GL's program is not the Vulkan pass's twin" );
	} else {
		wwCellHdrFrameSize( t.w, t.h );
		std::memcpy( t.vp, vp, sizeof( t.vp ) );
		t.color = color;
		t.n = int( pos.size() / 3 );
		t.pos = pos.data();
		t.value = I.data();
		std::memcpy( t.dotRow, row, sizeof( t.dotRow ) );
		std::memcpy( t.dotProj, proj, sizeof( t.dotProj ) );
		std::memcpy( t.dotCam, cam, sizeof( t.dotCam ) );
		t.dotBand[0] = Fr->band[0];
		t.dotBand[1] = Fr->band[1];
		t.dotFocal = focal;
		t.dotBulb = 4.0f;
		t.dotRed = 0;
		// cell_fardots_ps's t0..t4 (vkframe_spirv.sh); the air's integrated volume is read where it is shared
		static const char * const kName[5] = { "atApTex", "volAirTerr", "volAirRainMap", "volTex", "volTile" };
		static const int kType[5] = { 1, 2, 0, 1, 0 };
		static char twin[5];
		bool ok = true;
		for ( int k = 0; k < 5 && ok; k++ ) {
			ok = WwVkPresent::fxTexOf( fn, prog->id, kName[k], kType[k], t.tex[k], why );
			t.tex[k].twin = reinterpret_cast<std::uintptr_t>( &twin[k] );
		}
		if ( !ok ) {
			WwVkPresent::farDotsOnGl( why.c_str() );
		} else {
			WwVkPresent::fxPack( fn, prog->id, WwVkPresent::kT_fardots, int( std::size( WwVkPresent::kT_fardots ) ), t.loose );
			drew = WwVkPresent::farDotsRun( t, why );
		}
	}
	const WwVkPresent::FxStat st = WwVkPresent::takeFarDotsStat();
	static WwVkPresent::FxStat sum;
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
	if ( std::chrono::duration<double>( std::chrono::steady_clock::now() - at ).count() >= 5.0 || !said.startsWith( QStringLiteral( "vk far: dots on V" ) ) ) {
		QString line = sum.runs > 0
			? QStringLiteral( "vk far: dots on Vulkan %1 frames (%2 dots, cell_fardots as instanced quads into the tone frame + stencil, one crossing; "
				"%3 pipelines built in %4 ms; %5 input copies; drawing thread %6 ms, max %7)" ).arg( sum.runs ).arg( t.n ).arg( sum.pipelines )
				.arg( sum.buildMs, 0, 'f', 1 ).arg( sum.copies ).arg( sum.cpuMs / sum.runs, 0, 'f', 3 ).arg( sum.maxMs, 0, 'f', 3 )
			: QStringLiteral( "vk far: dots on Vulkan wanted, 0 frames drawn" );
		if ( sum.gl > 0 )
			line += QStringLiteral( ", on GL %1 frames (last: %2)" ).arg( sum.gl ).arg( QString::fromStdString( sum.glWhy ) );
		if ( sum.fails > 0 )
			line += QStringLiteral( ", refused %1 (last: %2)" ).arg( sum.fails ).arg( QString::fromStdString( sum.failWhy ) );
		line += QStringLiteral( "; far cards + far ground on GL (blocked: BC textures, cube-array shadows, texel buffers not shared)" );
		said.clear();
		say( line );
		sum = WwVkPresent::FxStat();
	}
	return drew;
}

/* The far bulb dots: intensity I spread over a normalized Gaussian, I (f / d)^2 G / (2 pi sigma^2), so the summed
 * dot equals the bulb's own emissive at any size. Depth-tested, not written; the stencil marks them as linear light. */
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
	float cam[3], row[12];
	for ( int j = 0; j < 3; j++ ) {
		prog->uni4f_l( prog->uniLocation( "dotRow[%d]", j ), FloatVector4( vt.rotation( j, 0 ) * sc, vt.rotation( j, 1 ) * sc,
			vt.rotation( j, 2 ) * sc, vt.translation[j] ) );
		row[j * 4 + 0] = vt.rotation( j, 0 ) * sc;
		row[j * 4 + 1] = vt.rotation( j, 1 ) * sc;
		row[j * 4 + 2] = vt.rotation( j, 2 ) * sc;
		row[j * 4 + 3] = vt.translation[j];
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
	wwLookdevFogUniforms( scene );	// lane VOLAIR1: the air between the bulb and the camera (cell_fardots.frag)
	if ( farDotsOnVulkan( r, prog, Fr, pos, I, row, cam, &pm[0][0], vp, pm[1][1] * 0.5f * float( vp[3] ) ) ) {
		r->stopProgram();	// lane VKFAR1: drawn on the Vulkan view, GL's state untouched
		s.dotsLast = int( n );
		return int( n );
	}
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
	QSettings().setValue( QStringLiteral( "WW/CellLights2" ), on );
}

bool wwCellLightsWanted( Scene * scene )
{
	if ( !scene || !scene->renderer || !scene->nifModel || scene->selecting )
		return false;
	// a Pass other than Combined draws through the cell program with the Cell lights row off too
	if ( !( st().on || wwCellPassFor( scene->nifModel ) > 0 ) || !wwCellLightsFor( scene->nifModel ) )
		return false;
	// an orthographic camera has no eye position to measure light distances from
	return scene->hasOption( Scene::DoLighting ) && scene->renderer->globalUniforms->projectionMatrix[3][3] != 1.0f;
}

/* The effect program takes no cell uniforms, so in a probe pass a see-through effect would write its own color
 * over the surface the probes measure. Probe passes are harness-only. */
bool wwCellProbePass( Scene * scene )
{
	const ClState & s = st();
	if ( s.red & 4096 )
		return false;	// WW_CELL_LIT_RED=probefx: the effects write over the probes
	// MODULES1 R6-8: a shadow debug view is a probe pass too (no effects over it, no linear frame, no post chain)
	// DEBUGVIEW1: so is a debug view (the pixel is a term, not light: no linear frame, no effects over it, no post)
	return ( s.probe > 0 || s.fogProbe > 0 || ( scene && wwCellPassFor( scene->nifModel ) > 0 ) || wwShadowViewPass( scene )
	         || wwDebugViewId() != 0 )
		&& wwCellLightsWanted( scene );
}

/* lane FO4CELLGI1: the loader's WW_CELL_VIEWS (fo4_default.frag's probe and pass views in the programs with the GI volume
 * read). Not static: read at every shader load; a probe or Pass set reloads the shaders (the Pass menu, the live driver) */
bool wwCellViewsShaderDefine()
{
	const ClState & s = st();
	return s.probe > 0 || s.pass > 0 || wwEnvInt( "WW_CELL_VIEWS_RED" ) != 0;
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
		QStringLiteral( "Surfel color" ), QStringLiteral( "Surfel light" ),
		QStringLiteral( "Brick light" ),	// each surfel tile shows its 4 m brick's light
		QStringLiteral( "Probe coverage" ) };	// magenta where no probe reaches
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

/* lane PERF1: the GI's visibility (WW_CELL_GI_VIS, read once): 0 the volumes (the default), 1 march (red: the per-pixel
 * chain as before, compiled in by src/gl/glcontext.cpp's WW_CELL_GI_MARCH, no screen pass) */
int wwCellGiVis()
{
	if ( !wwCellPrtOn() )
		return 0;	// lane PRTOFF1: the march is the PRT's chain (and a march red would turn the ray-traced GI off)
	WW_SWITCH_STATIC( int, mode, [] {
		const QByteArray v = wwEnv( "WW_CELL_GI_VIS" ).trimmed().toLower();
		if ( v == "march" || v == "red" || v == "old" )
			return 1;
		// a red of the march's own rules (WW_CELL_PV_RED) restores the march: the volumes have none of those rules
		for ( const QByteArray & r : wwEnv( "WW_CELL_PV_RED" ).split( ',' ) ) {
			const QByteArray t = r.trimmed().toLower();
			if ( t == "noclamp" || t == "nosee" || t == "borrow" || t == "lendamb" || t == "strand" || t == "firstslot"
				|| t == "tiepick" || t == "lonesky" || t == "nolastfill" )
				return 1;
		}
		return 0;
	}() );
	return mode;
}

/* lane PERF1: the GI's rate (WW_CELL_GI_RATE, read once): 0 half (the default: the screen GI at half resolution,
 * upsampled by depth and normal; PERFFIX2: full for the ray-traced volume's read), 4 half (`half`: half for both), 1 full (the screen GI at full resolution), 2 inline (no screen pass: every surface
 * reads the volumes itself), 3 red (the screen pass runs, the surfaces read inline: the inline picture at its cost plus
 * the pass) */
int wwCellGiRate()
{
	WW_SWITCH_STATIC( int, mode, [] {
		const QByteArray v = wwEnv( "WW_CELL_GI_RATE" ).trimmed().toLower();
		return v == "full" ? 1 : v == "inline" || v == "old" || v == "0" ? 2 : v == "red" ? 3 : v == "half" ? 4 : 0;
	}() );
	return mode;
}

namespace
{
/* lane PERF1: the telemetry lines in wwCellLightsUniforms are formatted only when their inputs' bytes change; the
 * formatting ran for every shape and was most of the function's time. The line printed, and when, is unchanged:
 * a changed input still formats the line and compares it with the last one printed. */
struct InputsLast
{
	QByteArray last;
	bool changed( const void * p, size_t n )
	{
		if ( size_t( last.size() ) == n && std::memcmp( last.constData(), p, n ) == 0 )
			return false;
		last = QByteArray( static_cast<const char *>( p ), qsizetype( n ) );
		return true;
	}
};
}


/* WW_CELL_CAM_DUMP=<file>: the camera in world units, rewritten when it moves (the fog and diffuse gates measure from it).
 * called by GLView::paintGL every frame, so a cell drawn with the Cell lights row off (fo4_default.prog,
 * no cell uniforms) still says where its eye is; the CELLSPAWN1 hold read "drawn camera none" on every row. */
void wwCellCamDump( Scene * scene )
{
	WW_SWITCH_STATIC( QString, camDump, QString::fromLocal8Bit( wwEnv( "WW_CELL_CAM_DUMP" ) ) );
	if ( camDump.isEmpty() || !scene )
		return;
	const ClState & s = st();
	const Transform & cv = scene->view;
	const float csc = cv.scale != 0.0f ? cv.scale : 1.0f;
	float camW[3];
	for ( int k = 0; k < 3; k++ ) {
		float w = 0.0f;
		for ( int j = 0; j < 3; j++ )
			w -= cv.rotation( j, k ) * cv.translation[j];
		camW[k] = w / csc;
	}
	static QString camLast;
	const QString line = QStringLiteral( "cam=%1,%2,%3 fogprobe=%4 probe=%5\n" ).arg( double( camW[0] ), 0, 'f', 2 )
		.arg( double( camW[1] ), 0, 'f', 2 ).arg( double( camW[2] ), 0, 'f', 2 ).arg( s.fogProbe ).arg( s.probe );
	QFile f( camDump );
	if ( line != camLast && f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
		f.write( line.toUtf8() );
		camLast = line;
	}
}

quint64 wwGlPaintCount();	// glview.cpp
bool wwFrameLog();	// gl/cellcull.cpp

bool wwCellUniRed()
{
	WW_SWITCH_STATIC( bool, red, wwEnvInt( "WW_CELL_UNI_RED" ) == 1 );
	return red;
}

// lane CELLUNI6: one name per leftover (celluniform.h)
bool wwCellUni6Red( const char * name )
{
	WW_SWITCH_STATIC( QByteArray, list, wwEnv( "WW_CELL_UNI6_RED" ).trimmed() );
	return wwCellUniRed() || WwCellUni::redHas( list.constData(), name );
}

// lane CELLUNI2: totals, then per site (lights = this file's scope, draw = the per-draw setters, globals = fog + sun)
void wwCellUniFrame()
{
	if ( !WwCellUni::frameTick( wwGlPaintCount(), 60 ) )
		return;
	WwUniProf::print();	// lane PERFFIX1
	if ( !wwFrameLog() )
		return;
	const WwCellUni::State & cs = WwCellUni::state();
	const WwCellUni::Dsa * dsa = wwCellUniDsa();
	QByteArray line = QByteArray( "cell uni: calls=" ) + QByteArray::number( cs.last.calls ) + " uploads="
		+ QByteArray::number( cs.last.uploads ) + " kept=" + QByteArray::number( cs.last.kept ) + " unused="
		+ QByteArray::number( cs.last.unused ) + " queries=" + QByteArray::number( cs.last.queries ) + " dsa="
		+ QByteArray::number( dsa && dsa->bindMulti ? 1 : 0 ) + " vpstale=" + QByteArray::number( cs.vpStale )
		+ " blendstale=" + QByteArray::number( cs.blendStale ) + " knownstale=" + QByteArray::number( cs.knownStale );
	static const char * const names[WwCellUni::SiteCount] = { "lights", "draw", "globals", "tail" };
	for ( int i = 0; i < WwCellUni::SiteCount; i++ ) {
		const WwCellUni::Counts & c = cs.sitesLast[i];
		line += QByteArray( " | " ) + names[i] + " calls=" + QByteArray::number( c.calls ) + " uploads="
			+ QByteArray::number( c.uploads ) + " kept=" + QByteArray::number( c.kept ) + " queries=" + QByteArray::number( c.queries );
	}
	std::fprintf( stderr, "%s%s\n", line.constData(), cs.red ? " RED" : "" );
}

// lane PERFFIX1 (celluniprof.h): the parts' sums, one line per site every 60 painted frames
WwUniProf::Table & WwUniProf::table( int site )
{
	static Table t[kSites];
	return t[site < 0 || site >= kSites ? 0 : site];
}

bool WwUniProf::on()
{
	WW_SWITCH_STATIC( bool, v, wwEnvInt( "WW_CELL_UNI_PROF" ) == 1 );
	return v;
}

void WwUniProf::print()
{
	static const char * const sites[kSites] = { "cell-uni", "fog-uni" };
	for ( int s = 0; s < kSites; s++ ) {
		Table & T = table( s );
		if ( T.calls <= 0 )
			continue;
		double total = 0.0;
		for ( int k = 0; k < kParts; k++ )
			total += T.us[k];
		QByteArray line = QByteArray( "uni prof: " ) + sites[s] + " calls=" + QByteArray::number( T.calls ) + " programs="
			+ QByteArray::number( T.nProgs ) + " us/call=" + QByteArray::number( total / double( T.calls ), 'f', 2 );
		for ( int k = 0; k < kParts; k++ )
			if ( T.name[k] )
				line += QByteArray( " | " ) + T.name[k] + " " + QByteArray::number( T.us[k] / double( T.calls ), 'f', 2 ) + " ("
					+ QByteArray::number( total > 0.0 ? 100.0 * T.us[k] / total : 0.0, 'f', 1 ) + "%)";
		std::fprintf( stderr, "%s\n", line.constData() );
		T = Table();
	}
}

const WwCellUni::Dsa * wwCellUniDsa()
{
	static const QOpenGLContext * last = nullptr;
	static WwCellUni::Dsa d;
	const QOpenGLContext * c = QOpenGLContext::currentContext();
	if ( !c )
		return nullptr;
	if ( c != last ) {	// every GL context of the app is one driver: resolved again only when the context changes
		d = WwCellUni::Dsa();
		if ( c->hasExtension( QByteArrayLiteral( "GL_EXT_direct_state_access" ) ) ) {
			d.bindMulti = reinterpret_cast<WwCellUni::BindMultiFn>( c->getProcAddress( "glBindMultiTextureEXT" ) );
			d.texBuffer = reinterpret_cast<WwCellUni::TexBufferFn>( c->getProcAddress( "glTextureBufferEXT" ) );
		}
		last = c;
	}
	return &d;
}

/* lane PERFFIX1 (item 1, bungo 11-01: the per-frame globals were worked out again for every program in every pass,
 * ~96 us a call x ~675 calls a frame at Concord all-on). What wwCellLightsUniforms sets depends on the scene, the pass
 * and a few facts of the program only: its CLASS (the program names the body tests, and the locations it tests). Inside
 * an armed pass (the setup memo's epoch: one pass, one view) the first call of a class records every set and bind in
 * order; a later call of that class, scene and epoch replays the record onto its own program (each set through the
 * same setter, so its cache and the draw packets see what they saw), and calls the program-specific setters live at
 * their places (the ray-traced volume, the water's GI hand-back, the shadow fades, the clusters). Uploads ran in the
 * recording call. WW_CELL_UNI_CLASS_RED=1: every call works it all out (the old path). WW_CELL_UNI_CLASS_CHECK=1: a
 * replayed call works it out as well and counts every op that differs (`cell uni class:` line, must say 0). */
namespace
{
using CuOp = WwUniRec::Op;
using CuRec = WwUniRec::Rec<NifSkopeOpenGLContext::Program, FloatVector4>;

bool cuClassRed()
{
	WW_SWITCH_STATIC( bool, red, wwEnvInt( "WW_CELL_UNI_CLASS_RED" ) == 1 );
	return red;
}

bool cuClassCheck()
{
	WW_SWITCH_STATIC( bool, check, wwEnvInt( "WW_CELL_UNI_CLASS_CHECK" ) == 1 );
	return check;
}

struct CuStats
{
	long long recorded = 0, replayed = 0, checked = 0, differ = 0, opsDiffer = 0;
};

CuStats & cuStats()
{
	static CuStats s;
	return s;
}

// the facts of a program the body tests: its names (ext / fx / decal / volair) and the locations it asks for
std::uint32_t cuClassOf( NifSkopeOpenGLContext::Program * prog )
{
	WW_SWITCH_STATIC( bool, airOldSun, wwEnvStr( "WW_CELL_AIR_RED" ).split( ',' ).contains( QStringLiteral( "oldsun" ) ) );
	const bool cellName = wwIsCellProgramName( prog->name ), decal = wwIsCellDecalProgramName( prog->name );
	const bool volair = prog->name == "cell_volair.prog" && !airOldSun;
	const bool fx = prog->name == std::string_view( "fo4_effectcell.prog" ) || prog->name == std::string_view( "fo4_effectcellfx.prog" )
		|| prog->name == std::string_view( "particles_cell.prog" );
	return ( cellName ? 1u : 0u ) | ( decal ? 2u : 0u ) | ( volair ? 4u : 0u ) | ( fx ? 8u : 0u )
		| ( prog->uniLocation( "fogOn" ) >= 0 ? 16u : 0u ) | ( prog->uniLocation( "csmMap" ) >= 0 ? 32u : 0u )
		| ( prog->uniLocation( "cellShadowFade[0]" ) >= 0 ? 64u : 0u );
}

// a record replayed onto another program of its class (see wwCellLightsUniforms; the core in gl/cellunirec.h)
void cuReplay( Scene * scene, Renderer * r, NifSkopeOpenGLContext::Program * prog, const CuRec & c )
{
	auto fn = r->fn;
	WwCellUni::Units<std::remove_pointer_t<decltype( fn )>> u( fn, wwCellUniDsa() );
	u.section();
	WwUniRec::replay( c, prog, u, [&]( const CuOp & o ) {
		if ( o.kind == CuOp::Cluster ) {
			wwCellClusterUniforms( scene, c.clTexels, c.clCount, c.clStamp );
		} else if ( o.arg == 1 ) {
			rtddgiCellUniforms( scene );
		} else if ( o.arg == 2 ) {
			waterGi().passSet( prog );
		} else if ( o.arg == 3 && o.i && prog->uniLocation( "cellShadowFade[0]" ) >= 0 ) {
			Gpu & g = gpus()[r];
			r->fn->glUniform1fv( prog->uniLocation( "cellShadowFade[0]" ), kShadowSlots, g.shFade );
			prog->wwUni.forget( prog->uniLocation( "cellShadowFade[0]" ), kShadowSlots );	// lane CELLUNI1: a raw write
		}
	} );
	u.done();
}

//! the end of a call (any return): a recording becomes valid; a checked call's ops are compared with the record
struct CuEnd
{
	CuRec * rec = nullptr;
	const CuRec * cmp = nullptr;
	const CuRec * mine = nullptr;
	~CuEnd()
	{
		CuStats & st = cuStats();
		if ( rec ) {
			rec->on = false;
			rec->valid = true;
			st.recorded++;
		}
		if ( cmp && mine ) {
			st.checked++;
			long long d = cmp->ops.size() == mine->ops.size() ? 0 : 1;
			for ( size_t k = 0; k < cmp->ops.size() && k < mine->ops.size(); k++ )
				if ( !cmp->ops[k].same( mine->ops[k] ) ) {
					if ( st.opsDiffer < 12 )
						std::fprintf( stderr, "cell uni class: MISMATCH op %d kind %d %s[%d] i %d -> %d f %g -> %g\n", int( k ),
							int( cmp->ops[k].kind ), cmp->ops[k].name ? cmp->ops[k].name : "-", cmp->ops[k].arg, cmp->ops[k].i,
							mine->ops[k].i, double( cmp->ops[k].f[0] ), double( mine->ops[k].f[0] ) );
					d++;
				}
			st.differ += d ? 1 : 0;
			st.opsDiffer += d;
		}
		const long long n = st.recorded + st.replayed;
		if ( n > 0 && n % 4000 == 0 )
			std::fprintf( stderr, "cell uni class: recorded %lld replayed %lld checked %lld differ %lld ops differ %lld%s\n",
				st.recorded, st.replayed, st.checked, st.differ, st.opsDiffer, cuClassCheck() ? " (check: must be 0)" : "" );
	}
};
}	// namespace

quint64 wwSetupMemoEpochArmed();	// gl/renderer.cpp: the armed pass's epoch, 0 outside one

namespace
{
WwCellToneInputs & toneInputs()
{
	static WwCellToneInputs t;
	return t;
}
}	// namespace

const WwCellToneInputs & wwCellToneInputs()
{
	return toneInputs();
}

void wwCellLightsUniforms( Scene * scene )
{
	PassTime::DrawCpuScope cpuT( PassTime::CpuCellUni );	// lane PERF2
	WwUniProf::Marks prof( 0 );	// lane PERFFIX1: WW_CELL_UNI_PROF=1, where this call's CPU goes
	if ( !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "cellOn" ) < 0 )
		return;
	WwCellUni::state().red = wwCellUniRed();	// lane CELLUNI1
	wwCellUniFrame();
	const WwCellUni::Scope uniScope;
	// lane PERFFIX1 (item 1): a later call of this program's class in the same armed pass replays the first one's record
	static CuRec idle;	// outside an armed pass, or the red: records nothing
	CuRec scratch;
	CuRec * recp = &idle;
	CuEnd endOf;
	if ( const quint64 epoch = cuClassRed() ? 0 : wwSetupMemoEpochArmed() ) {
		static std::unordered_map<std::uint32_t, CuRec> recs;
		static quint64 recEpoch = 0;
		static const void * recScene = nullptr;
		if ( recEpoch != epoch || recScene != scene ) {
			for ( auto & kv : recs )
				kv.second.valid = false;
			recEpoch = epoch;
			recScene = scene;
		}
		CuRec & c = recs[cuClassOf( prog )];
		if ( c.valid ) {
			cuReplay( scene, r, prog, c );
			cuStats().replayed++;
			if ( !cuClassCheck() ) {
				prof.at( 21, "replay" );
				return;
			}
			scratch.on = true;	// the check: the body runs too (its sets win) and its ops are compared at the end
			recp = &scratch;
			endOf.cmp = &c;
			endOf.mine = &scratch;
		} else {
			c.ops.clear();
			c.on = true;
			recp = &c;
			endOf.rec = &c;
		}
	}
	CuRec & rec = *recp;
	const bool on = wwCellLightsWanted( scene );
	rec.b( prog, "cellOn", on );
	prof.prog( prog );
	auto fn = r->fn;
	ClState & s = st();
	Gpu & g = gpus()[r];
	if ( on && !s.nifx.isEmpty() )
		nifxTick( scene->nifModel );	// lane NIFXLIGHT1
	if ( on )
		physTick( scene->nifModel );	// lane PHYSUNITS1: Game Light Brightness, live
	const WwCellLighting * L = on ? wwCellLightsFor( scene->nifModel ) : nullptr;
	const bool flick = L && s.flickerOn && !s.flickerRed;	// lane LIGHTFLICKER1
	// lane NIFXLIGHT3: the .nifx dimmer tracks play on the world clock (the buffer keeps a copy to patch, as for flicker)
	const bool dimAnim = L && wwLookOn( WwLook::NifxLights ) && wwCellWorldAnimOn() && !( wwNifxLightsRed() & WwNifxRedStaticDim );
	const wwlp::Params phys = wwLightPhysParams();	// lane LIGHTPHYS1
	const int goboOn = wwLookOn( WwLook::LightPatterns ) ? 1 : 0;	// lane LIGHTS5 (GOBO1)
	if ( L )
		fogScaleTick( g, scene->nifModel, s.version.value( scene->nifModel ), *L );	// lane LIGHTS5 (FOGSCALE1)
	prof.at( 1, "head" );
	if ( L && ( g.doc != scene->nifModel || g.version != s.version.value( scene->nifModel ) || g.bufShStamp != g.shStamp
	            || g.bufLiveGen != wwSwitchGen() || g.flickOn != flick || g.dimOn != dimAnim || g.bufXemiStamp != xemiStampOf( scene->nifModel )
	            || g.bufCurve != phys.curve || g.bufCurveRed != ( phys.red & 8 )
	            || g.bufGobo != goboOn || g.bufFogStamp != g.fogStamp ) ) {
		g.bufLiveGen = wwSwitchGen();
		g.bufCurve = phys.curve;
		g.bufCurveRed = phys.red & 8;
		g.bufGobo = goboOn;
		g.bufFogStamp = g.fogStamp;
		// lane LIGHTS4: this frame's XEMI factors (wwCellXemiLightsFrame), folded into each light's colour
		const auto xd = xemiDocs().constFind( scene->nifModel );
		const std::vector<float> * xm = xd != xemiDocs().constEnd() && xd->version == s.version.value( scene->nifModel )
			&& xd->mul.size() == size_t( L->lights.size() ) * 3 ? &xd->mul : nullptr;
		g.bufXemiStamp = xd != xemiDocs().constEnd() ? xd->stamp : 0;
		// the slot each light holds (the slots belong to the document the shadow pass last saw)
		std::vector<float> slotOf( size_t( L->lights.size() ), -1.0f );
		if ( g.shDoc == scene->nifModel && g.shVersion == s.version.value( scene->nifModel ) )
			for ( int k = 0; k < kShadowSlots; k++ )
				if ( g.shSlot[k] >= 0 && g.shSlot[k] < L->lights.size() && g.shPend[k] != g.shSlot[k] )	// lane PERFFIX2 b: a waiting store reads unshadowed
					slotOf[size_t( g.shSlot[k] )] = float( k );
		std::vector<float> t;
		t.reserve( size_t( L->lights.size() ) * kTexelsPerLight * 4 + 20 );
		for ( qsizetype i = 0; i < L->lights.size(); i++ ) {
			const WwCellLight & l = L->lights.at( i );
			float dir[3] = { l.dir[0], l.dir[1], l.dir[2] };
			// texel 1.w = cos(FOV / 2) for a spot, else the shape: -2 omni, -3 hemisphere, -4 box
			const float shape = l.spot ? l.cosOuter : l.shape == 1 ? -3.0f : l.shape == 2 ? -4.0f : -2.0f;
			float col[3] = { l.color[0], l.color[1], l.color[2] };
			if ( xm && l.xemi )
				for ( int k = 0; k < 3; k++ )
					col[k] *= ( *xm )[size_t( i ) * 3 + size_t( k )];
			if ( phys.curve != 1.0f ) {	// lane LIGHTPHYS1: the intensity curve, once here for every consumer
				const float ks = wwlp::intensityScale( phys, col );
				for ( int k = 0; k < 3; k++ )
					col[k] *= ks;
			}
			const float tex[kTexelsPerLight * 4] = {
				l.pos[0], l.pos[1], l.pos[2], l.radius,
				col[0], col[1], col[2], shape,
				dir[0], dir[1], dir[2], l.cone,
				l.bias, l.scale, l.exponent,
				float( ( l.noSpecular ? 1 : 0 ) | ( l.noRim ? 2 : 0 ) | ( l.ignoreRoughness ? 4 : 0 ) | ( flick && l.flicker ? 8 : 0 ) ),
				slotOf[size_t( i )], float( l.shadow ), l.nearClip, l.shadowBias,
				l.box[0][0], l.box[0][1], l.box[0][2], l.box[0][3],
				l.box[1][0], l.box[1][1], l.box[1][2], l.box[1][3],
				l.box[2][0], l.box[2][1], l.box[2][2], l.box[2][3] };
			t.insert( t.end(), tex, tex + kTexelsPerLight * 4 );
		}
		/* WW_CELL_LIGHT_COPIES=n: every light n times at 1/n its color (same picture, n times the light loop cost) */
		WW_SWITCH_STATIC( int, copies, std::clamp( wwEnvInt( "WW_CELL_LIGHT_COPIES" ), 1, 64 ) );
		if ( copies > 1 && !t.empty() ) {
			const size_t one = t.size();
			t.reserve( one * size_t( copies ) );
			for ( size_t o = 0; o < one; o += kTexelsPerLight * 4 )
				for ( int k = 4; k < 7; k++ )
					t[o + size_t( k )] /= float( copies );
			for ( int c = 1; c < copies; c++ )
				t.insert( t.end(), t.begin(), t.begin() + std::ptrdiff_t( one ) );
		}
		/* lane LIGHTS5: data after the lights (the cluster lists and spheres never read it). GOBO1: each pattern once
		 * (header w h, mean, rows), a spot's texel 5.w = 1 + its header; FOGSCALE1: the scales, 4 a texel. */
		/* lane NIFXLIGHT2: the bulb-tier lights (.nifx) sit right after the lights, index cellBulbBase + j: only the air
		 * walks them (the fog glow), the surfaces stop at cellLightCount. No shadow, no pattern (a 0 in the table). */
		const size_t nB = size_t( L->nifxBulbs.size() );
		if ( nB && t.empty() )
			t.assign( kTexelsPerLight * 4, 0.0f );	// the dummy light first, so the bulbs keep index 1 + j
		std::vector<float> x;
		const size_t per = kTexelsPerLight * 4, nL = size_t( L->lights.size() ), lightTexels = t.size() / 4;
		g.bulbBase = int( t.size() / per );
		g.bulbs = int( nB );
		for ( size_t b = 0; b < nB; b++ ) {
			const WwCellLight & l = L->nifxBulbs.at( qsizetype( b ) );
			const float shape = l.spot ? l.cosOuter : l.shape == 1 ? -3.0f : -2.0f;
			float col[3] = { l.color[0], l.color[1], l.color[2] };
			if ( phys.curve != 1.0f ) {
				const float ks = wwlp::intensityScale( phys, col );
				for ( int k = 0; k < 3; k++ )
					col[k] *= ks;
			}
			const float tex[kTexelsPerLight * 4] = {
				l.pos[0], l.pos[1], l.pos[2], l.radius,
				col[0], col[1], col[2], shape,
				l.dir[0], l.dir[1], l.dir[2], l.cone,
				l.bias, l.scale, l.exponent, 0.0f,
				-1.0f, 0.0f, l.nearClip, l.shadowBias };
			x.insert( x.end(), tex, tex + kTexelsPerLight * 4 );
		}
		int goboSpots = 0, goboUnread = 0, goboOmni = 0;
		QHash<QString, int> goboAt;
		/* lane GOBO2: an omni, hemisphere or box light's pattern: a record of 3 texels (local x + 1 + its header, y, z);
		 * pin WW_CELL_GOBO_OMNI=off keeps spots only (LIGHTS5) */
		WW_SWITCH_STATIC( bool, omniOff, wwEnv( "WW_CELL_GOBO_OMNI" ).trimmed() == "off" );
		std::vector<float> omniRec( nL, 0.0f );
		for ( size_t i = 0; goboOn && i < nL; i++ ) {
			const WwCellLight & l = L->lights.at( qsizetype( i ) );
			if ( l.gobo.isEmpty() || ( !l.spot && omniOff ) )
				continue;
			const QString key = l.gobo.toLower();
			int at = goboAt.value( key, -2 );
			if ( at == -2 ) {
				const GoboPat & p = goboPattern( L->dataRoot, l.gobo );
				at = p.w > 0 ? int( lightTexels + x.size() / 4 ) : -1;
				goboUnread += at < 0 ? 1 : 0;
				if ( at >= 0 ) {
					const float hd[8] = { float( p.w ), float( p.h ), 0, 0, p.mean[0], p.mean[1], p.mean[2], 0 };
					x.insert( x.end(), hd, hd + 8 );
					for ( size_t k = 0; k + 3 <= p.rgb.size(); k += 3 ) {
						x.insert( x.end(), p.rgb.begin() + std::ptrdiff_t( k ), p.rgb.begin() + std::ptrdiff_t( k + 3 ) );
						x.push_back( 1.0f );
					}
				}
				goboAt.insert( key, at );
			}
			if ( at < 0 )
				continue;
			if ( !l.spot ) {
				goboOmni++;
				omniRec[i] = float( lightTexels + x.size() / 4 + 1 );
				const float rec[12] = { l.goboX[0], l.goboX[1], l.goboX[2], float( at + 1 ), l.goboY[0], l.goboY[1], l.goboY[2], 0,
					l.goboZ[0], l.goboZ[1], l.goboZ[2], 0 };
				x.insert( x.end(), rec, rec + 12 );
				continue;
			}
			goboSpots++;
			const float t56[8] = { l.goboY[0], l.goboY[1], l.goboY[2], float( at + 1 ), l.goboZ[0], l.goboZ[1], l.goboZ[2], l.goboTan };
			for ( size_t o = i * per; o + per <= t.size(); o += nL * per )	// every copy
				std::copy( t56, t56 + 8, t.begin() + std::ptrdiff_t( o + 20 ) );
		}
		g.goboTbl = 0;
		if ( goboOmni ) {	// lane GOBO2: 1 + each light's record (every copy), 4 a texel
			g.goboTbl = int( lightTexels + x.size() / 4 );
			for ( size_t j = 0; j < t.size() / per; j++ )
				x.push_back( omniRec[j % nL] );
			x.insert( x.end(), nB, 0.0f );	// lane NIFXLIGHT2: the bulbs have no pattern
			while ( x.size() % 4 )
				x.push_back( 0.0f );
		}
		g.goboAny = goboSpots + goboOmni > 0;
		{
			const QByteArray echo = QStringLiteral( "cell gobo: %1 spots with a pattern, %2 textures (%3 unread)%4; omni/box %5%6" )
				.arg( goboSpots ).arg( goboAt.size() - goboUnread ).arg( goboUnread )
				.arg( goboOn ? QString() : QStringLiteral( " (Light patterns off)" ) )
				.arg( goboOmni ).arg( omniOff ? QStringLiteral( " (pin off)" ) : QString() ).toUtf8();
			static QByteArray said;
			if ( echo != said ) {
				said = echo;
				std::fprintf( stderr, "%s\n", echo.constData() );
			}
		}
		// red WW_CELL_FOGSCALE_RED=surfaces: the scale on the light itself, so the surfaces change too
		WW_SWITCH_STATIC( bool, fogRed, wwEnvStr( "WW_CELL_FOGSCALE_RED" ) == QLatin1String( "surfaces" ) );
		g.fogBase = -1;
		if ( nL + nB && g.fogVals.size() == nL + nB ) {
			if ( fogRed ) {
				for ( size_t o = 0, j = 0; o + per <= t.size(); o += per, j++ )
					for ( int k = 4; k < 7; k++ )
						t[o + size_t( k )] *= g.fogVals[j % nL];
			} else {
				g.fogBase = int( lightTexels + x.size() / 4 );
				for ( size_t j = 0; j < t.size() / per; j++ )
					x.push_back( nL ? g.fogVals[j % nL] : 1.0f );
				x.insert( x.end(), g.fogVals.begin() + std::ptrdiff_t( nL ), g.fogVals.end() );	// lane NIFXLIGHT2: the bulbs
				while ( x.size() % 4 )
					x.push_back( 1.0f );
			}
		}
		g.bufShStamp = g.shStamp;
		g.spheres.clear();
		for ( size_t o = 0; nL && o + 4 <= t.size(); o += kTexelsPerLight * 4 )
			g.spheres.insert( g.spheres.end(), t.begin() + std::ptrdiff_t( o ), t.begin() + std::ptrdiff_t( o + 4 ) );
		if ( t.empty() )
			t.assign( kTexelsPerLight * 4, 0.0f );	// a buffer texture must have a store
		if ( !g.tex )
			fn->glGenTextures( 1, &g.tex );
		// lane VKOBJ4: under WW_VKOBJ_SHADE a store (exported Vulkan memory GL writes; the Vulkan runs read it in place)
		bool moved = false;
		const bool stored = WwVkPresent::objBlTexelFill( g.buf, g.bufStored, WwBl::vkShadeOn(),
			{ { t.data(), t.size() * sizeof( float ) }, { x.data(), x.size() * sizeof( float ) } }, moved );
		if ( moved )
			g.texAttached = false;
		if ( !stored ) {
			if ( !g.buf )
				fn->glGenBuffers( 1, &g.buf );
			fn->glBindBuffer( GL_TEXTURE_BUFFER, g.buf );
			if ( x.empty() ) {
				fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( t.size() * sizeof( float ) ), t.data(), GL_STATIC_DRAW );
			} else {	// lane LIGHTS5: the lights, then the patterns and fog scales
				fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( ( t.size() + x.size() ) * sizeof( float ) ), nullptr, GL_STATIC_DRAW );
				fn->glBufferSubData( GL_TEXTURE_BUFFER, 0, GLsizeiptr( t.size() * sizeof( float ) ), t.data() );
				fn->glBufferSubData( GL_TEXTURE_BUFFER, GLintptr( t.size() * sizeof( float ) ), GLsizeiptr( x.size() * sizeof( float ) ), x.data() );
			}
			fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
		}
		g.doc = scene->nifModel;
		g.version = s.version.value( scene->nifModel );
		g.count = int( t.size() / ( kTexelsPerLight * 4 ) ) - ( L->lights.isEmpty() ? 1 : 0 );
		g.flickOn = flick;
		g.dimOn = dimAnim;
		g.flickStamp = 0;
		g.flickBase.clear();
		g.flickLive.clear();
		bool tracks = false;	// lane NIFXLIGHT3: a dimmer track or a flickering bulb needs the patch copy too
		for ( const QVector<WwCellLight> * v : { &L->lights, &L->nifxBulbs } )
			for ( const WwCellLight & l : *v )
				tracks |= ( dimAnim && l.nifxDim ) || ( flick && v == &L->nifxBulbs && l.flicker );
		if ( ( flick && !L->lights.isEmpty() ) || tracks ) {
			g.flickBase = t;
			// lane NIFXLIGHT3: the bulb-tier lights follow the lights in the buffer (x starts with them)
			g.flickBase.insert( g.flickBase.end(), x.begin(), x.begin() + std::ptrdiff_t( std::min( x.size(), nB * per ) ) );
			g.flickLive = g.flickBase;
		}
		if ( flick && !L->lights.isEmpty() ) {
			// lane LIGHTFLICKER1: the cluster lists see a moving light's whole reach (|offset| <= 0.87 Movement x scale)
			const size_t per = kTexelsPerLight * 4, n = size_t( L->lights.size() );
			for ( size_t j = 0; j * per < t.size(); j++ ) {
				const WwCellLight & l = L->lights.at( qsizetype( j % n ) );
				if ( l.flicker && l.flickerMovement != 0.0f && !l.shadow ) {
					float sc = 0.0f;
					for ( int a = 0; a < 3; a++ )
						sc = std::max( sc, std::sqrt( l.flickerAxes[a][0] * l.flickerAxes[a][0] + l.flickerAxes[a][1] * l.flickerAxes[a][1]
							+ l.flickerAxes[a][2] * l.flickerAxes[a][2] ) );
					t[j * per + 3] += 0.87f * std::abs( l.flickerMovement ) * sc;
				}
			}
		}
		g.texels = std::move( t );
		g.texelStamp++;
	}
	prof.at( 2, "lightbuf" );
	/* lane LIGHTFLICKER1: this frame's flicker (wwCellFlickerFrame) patched into the buffer once, as the range of the
	 * flickering lights: the surfaces, the effects and the air read the same texels */
	if ( L && ( g.flickOn || g.dimOn ) && g.doc == scene->nifModel && !g.flickBase.empty() ) {
		const auto fd = flickDocs().constFind( scene->nifModel );
		if ( fd != flickDocs().constEnd() && fd->mode && fd->version == g.version && fd->stamp != g.flickStamp && !fd->idx.empty() ) {
			g.flickStamp = fd->stamp;
			const size_t per = kTexelsPerLight * 4, n = size_t( L->lights.size() );
			const size_t total = g.flickLive.size() / per;
			// lane NIFXLIGHT3: the lights (and their copies) end at the bulbs; bulb j sits at cellBulbBase + j, no copies
			const size_t lightTotal = n ? std::min( size_t( g.bulbBase ), total ) : 0;
			size_t lo = total, hi = 0;
			auto patch = [&]( size_t j, const float * v, bool sphere ) {
				const size_t o = j * per;
				for ( int k = 0; k < 3; k++ ) {
					g.flickLive[o + size_t( k )] = g.flickBase[o + size_t( k )] + v[1 + k];
					g.flickLive[o + 4 + size_t( k )] = g.flickBase[o + 4 + size_t( k )] * v[0];
					if ( sphere && j * 4 + 3 < g.spheres.size() )
						g.spheres[j * 4 + size_t( k )] = g.flickLive[o + size_t( k )];
				}
				lo = std::min( lo, j );
				hi = std::max( hi, j );
			};
			for ( int i : fd->idx ) {
				if ( size_t( i ) * 4 + 4 > fd->val.size() )
					continue;
				const float * v = fd->val.data() + size_t( i ) * 4;
				if ( size_t( i ) < n ) {
					for ( size_t c = 0; c * n < lightTotal; c++ )
						if ( c * n + size_t( i ) < lightTotal )
							patch( c * n + size_t( i ), v, true );
				} else if ( size_t( g.bulbBase ) + ( size_t( i ) - n ) < total ) {
					patch( size_t( g.bulbBase ) + ( size_t( i ) - n ), v, false );
				}
			}
			std::string storeWhy;	// lane VKOBJ4: a store takes the patch as GL's write into it
			if ( lo <= hi && g.buf && g.bufStored ) {
				WwVkPresent::objBlTexelStoreWrite( g.buf, std::size_t( lo * per * sizeof( float ) ), std::size_t( ( hi - lo + 1 ) * per * sizeof( float ) ),
					g.flickLive.data() + lo * per, storeWhy );
			} else if ( lo <= hi && g.buf ) {
				fn->glBindBuffer( GL_TEXTURE_BUFFER, g.buf );
				fn->glBufferSubData( GL_TEXTURE_BUFFER, GLintptr( lo * per * sizeof( float ) ), GLsizeiptr( ( hi - lo + 1 ) * per * sizeof( float ) ),
					g.flickLive.data() + lo * per );
				fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
			}
		}
	}
	prof.at( 3, "flicker" );
	/* The sampler is bound whether or not this draw lights: a samplerBuffer left on unit 0
	 * beside BaseMap's sampler2D is a draw-time INVALID_OPERATION. */
	// lane CELLUNI1: bound by DSA, the active unit untouched (and never asked for); uploads move it and put it back
	WwCellUni::Units<std::remove_pointer_t<decltype( fn )>> u( fn, wwCellUniDsa() );
	u.section();
	if ( g.tex && g.buf ) {
		rec.bind( u, kTextureUnit, GL_TEXTURE_BUFFER, g.tex );
		if ( !g.texAttached ) {
			u.texBuffer( g.tex, GL_RGBA32F, g.buf );
			g.texAttached = true;
		}
	} else {
		rec.bind( u, kTextureUnit, GL_TEXTURE_BUFFER, 0 );
	}
	prof.at( 4, "lightbind" );
	// the bounce grid, bound (like the buffer above) whether or not this draw uses it; a Pass reads it with the
	// GI row off too, and Sky visibility binds the sky grid in its place
	const int pass = wwCellPassFor( scene->nifModel );
	const WwCellGi * G = on && ( s.giOn || pass > 0 ) ? wwCellGiFor( scene->nifModel ) : nullptr;
	if ( G && pass == 2 && !G->sky.empty() && ( g.skyDoc != scene->nifModel || g.skyVersion != s.giVersion.value( scene->nifModel ) ) ) {
		std::vector<float> sky = G->sky;
		if ( s.passRed & 4 )	// red "open": every direction open to the sky
			for ( size_t i = 0; i + 3 < sky.size(); i += 4 )
				sky[i] = sky[i + 1] = sky[i + 2] = sky[i + 3];
		if ( !g.skyTex )
			fn->glGenTextures( 1, &g.skyTex );
		u.raw( kGiUnit );
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
	// lane SPECOCC1: with the sky specular occlusion on, the GI texture carries the sky grid's slot 0 after its own slabs
	// lane WATERLOOK1 step 10: the Water sky visibility row reads the same slot
	// lane FOGGI1: so does the air's sun gate (cellSpecOcc stays the row's: the surfaces read the slot only with it on)
	const bool specSky = G && ( wwCellSpecOccMode() > 0 || wwLookOn( WwLook::WaterSkyVis ) || wwVolAirSunGateWanted() )
		&& G->skyLit && G->sky.size() == G->rgba.size();
	if ( G && pass != 2 && !G->rgba.empty()
		&& ( g.giDoc != scene->nifModel || g.giVersion != s.giVersion.value( scene->nifModel ) || g.giSpecSky != specSky ) ) {
		if ( !g.giTex )
			fn->glGenTextures( 1, &g.giTex );
		u.raw( kGiUnit );
		fn->glBindTexture( GL_TEXTURE_3D, g.giTex );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
		const int slabs = G->slotRooms.empty() ? 6 : 12;
		const int giLayers = G->dims[2] * slabs;
		const int skyLayers = specSky ? G->dims[2] * 6 : 0;	// lane SPECOCC1: the sky grid's slot 0, right after the grid's slabs
		if ( G->fine.empty() && !specSky ) {
			fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA16F, G->dims[0], G->dims[1], giLayers, 0, GL_RGBA, GL_FLOAT,
				G->rgba.data() );
		} else {	// lane EMISSIVEGRID1: the fine boxes' atlas in the layers after the grid's (and the sky's; read by texelFetch only)
			fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA16F, G->dims[0], G->dims[1], giLayers + skyLayers + G->fineLayers, 0, GL_RGBA,
				GL_FLOAT, nullptr );
			fn->glTexSubImage3D( GL_TEXTURE_3D, 0, 0, 0, 0, G->dims[0], G->dims[1], giLayers, GL_RGBA, GL_FLOAT, G->rgba.data() );
			if ( specSky )
				fn->glTexSubImage3D( GL_TEXTURE_3D, 0, 0, 0, giLayers, G->dims[0], G->dims[1], skyLayers, GL_RGBA, GL_FLOAT,
					G->sky.data() );
			if ( !G->fine.empty() )
				fn->glTexSubImage3D( GL_TEXTURE_3D, 0, 0, 0, giLayers + skyLayers, G->dims[0], G->dims[1], G->fineLayers, GL_RGBA,
					GL_FLOAT, G->fine.data() );
		}
		g.giSpecSky = specSky;
		g.giDoc = scene->nifModel;
		g.giVersion = s.giVersion.value( scene->nifModel );
		g.giFadeVersion = s.giFadeVersion.value( scene->nifModel );
	} else if ( G && pass != 2 && !G->rgba.empty() && g.giTex && g.giFadeVersion != s.giFadeVersion.value( scene->nifModel ) ) {
		// lane GIPROBE2: a smoothing step changed the grid's values only: its slabs again, nothing else
		u.raw( kGiUnit );
		fn->glBindTexture( GL_TEXTURE_3D, g.giTex );
		fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
		const int slabs = G->slotRooms.empty() ? 6 : 12;
		fn->glTexSubImage3D( GL_TEXTURE_3D, 0, 0, 0, 0, G->dims[0], G->dims[1], G->dims[2] * slabs, GL_RGBA, GL_FLOAT, G->rgba.data() );
		g.giFadeVersion = s.giFadeVersion.value( scene->nifModel );
	}
	const bool skyDraw = G && pass == 2 && g.skyTex && g.skyDoc == scene->nifModel;
	const bool giDraw = skyDraw || ( G && pass != 2 && g.giTex && g.giDoc == scene->nifModel );
	rec.bind( u, kGiUnit, GL_TEXTURE_3D, skyDraw ? g.skyTex : giDraw ? g.giTex : 0 );
	prof.at( 5, "gi" );
	{	// SKY2 FOGSLICE1: the air volume (the deck's D2 s55), read by the air pass only (cell_airmed.glsl volCubeAll)
		const bool airUp = giDraw && !skyDraw && G->skyLit && !G->air.empty();
		if ( airUp && ( g.airDoc != scene->nifModel || g.airVersion != s.giVersion.value( scene->nifModel ) ) ) {
			if ( !g.airTex )
				fn->glGenTextures( 1, &g.airTex );
			u.raw( WwCellUnit::GiAir );
			fn->glBindTexture( GL_TEXTURE_3D, g.airTex );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
			fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
			fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA16F, G->airDims[0], G->airDims[1], G->airDims[2] * 6, 0, GL_RGBA, GL_FLOAT,
				G->air.data() );
			g.airDoc = scene->nifModel;
			g.airVersion = s.giVersion.value( scene->nifModel );
		}
		const bool airDraw = airUp && g.airTex && g.airDoc == scene->nifModel;
		rec.bind( u, WwCellUnit::GiAir, GL_TEXTURE_3D, airDraw ? g.airTex : 0 );
		rec.i( prog, "cellGiAir", WwCellUnit::GiAir );
		rec.b( prog, "cellGiAirOn", airDraw );
		if ( airDraw ) {
			rec.f3( prog, "cellGiAirOrigin", G->airOrigin[0], G->airOrigin[1], G->airOrigin[2] );
			rec.f( prog, "cellGiAirVoxel", G->airVoxel );
			rec.f3( prog, "cellGiAirDims", float( G->airDims[0] ), float( G->airDims[1] ), float( G->airDims[2] ) );
		}
	}
	prof.at( 6, "air" );
	// the rooms (src/features/gi/proberooms.h): the grid's slot rooms and the fine rooms, nearest, R32F
	if ( g.units < 0 ) {
		GLint units = 0;
		fn->glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &units );
		g.units = units;
	}
	{	// lane PERF1: the screen GI's samplers (wwCellAoDraw binds them per draw; never left on unit 0); the volume read's
		// loop bound (a uniform: the loop stays one body in every program)
		const bool fits = g.units > WwCellUnit::GiHalfKey;
		const int u = fits ? WwCellUnit::GiHalf : g.units > WwCellUnit::Ao ? WwCellUnit::Ao : 9;
		rec.i( prog, "cellGiHalf", u );
		rec.i( prog, "cellGiHalfKey", fits ? WwCellUnit::GiHalfKey : u );
		rec.i( prog, "cellGiLoop8", 8 );
		rec.b( prog, "cellGiHalfOn", false );	// wwCellAoDraw turns it on, per draw, after this
	}
	const bool roomsUp = giDraw && !G->slotRooms.empty() && !G->rooms.empty() && g.units > kRoomUnit;
	if ( roomsUp && ( g.roomDoc != scene->nifModel || g.roomVersion != s.giVersion.value( scene->nifModel ) ) ) {
		auto up3 = [&]( GLuint & tex, int unit, const int * d, const std::vector<float> & v, bool rgba ) {
			if ( !tex )
				fn->glGenTextures( 1, &tex );
			u.raw( unit );
			fn->glBindTexture( GL_TEXTURE_3D, tex );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
			fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
			fn->glTexImage3D( GL_TEXTURE_3D, 0, rgba ? GL_RGBA32F : GL_R32F, d[0], d[1], d[2], 0, rgba ? GL_RGBA : GL_RED,
				GL_FLOAT, v.data() );
		};
		// lane PERF1: with the volumes, the slots' thresholds in g, b, a (the packed rooms across, slot 0's, slot 1's weight)
		if ( wwCellGiVis() == 0 && G->slotThresh.size() == G->slotRooms.size() * 3 ) {
			std::vector<float> v( G->slotRooms.size() * 4 );
			for ( size_t i = 0; i < G->slotRooms.size(); i++ ) {
				v[i * 4] = G->slotRooms[i];
				for ( int c = 0; c < 3; c++ )
					v[i * 4 + 1 + size_t( c )] = G->slotThresh[i * 3 + size_t( c )];
			}
			up3( g.slotTex, kGiSlotUnit, G->dims, v, true );
		} else {
			up3( g.slotTex, kGiSlotUnit, G->dims, G->slotRooms, false );
		}
		up3( g.roomTex, kRoomUnit, G->roomsDims, G->rooms, false );
		g.roomDoc = scene->nifModel;
		g.roomVersion = s.giVersion.value( scene->nifModel );
	}
	const bool roomsDraw = roomsUp && g.roomDoc == scene->nifModel;
	if ( g.units > kRoomUnit ) {
		rec.bind( u, kGiSlotUnit, GL_TEXTURE_3D, roomsDraw ? g.slotTex : 0 );
		rec.bind( u, kRoomUnit, GL_TEXTURE_3D, roomsDraw ? g.roomTex : 0 );
		rec.i( prog, "cellGiSlots", kGiSlotUnit );
		rec.i( prog, "cellRooms", kRoomUnit );
	} else {	// two sampler3D may share a unit; a sampler3D on a 2D unit fails the draw
		rec.i( prog, "cellGiSlots", kGiUnit );
		rec.i( prog, "cellRooms", kGiUnit );
	}
	rec.b( prog, "cellGiRooms", roomsDraw );
	if ( roomsDraw ) {
		rec.f3( prog, "cellRoomsOrigin", G->roomsOrigin[0], G->roomsOrigin[1], G->roomsOrigin[2] );
		rec.f( prog, "cellRoomsCell", G->roomsCell );
		rec.f3( prog, "cellRoomsDims", float( G->roomsDims[0] ), float( G->roomsDims[1] ), float( G->roomsDims[2] ) );
	}
	prof.at( 7, "rooms" );
	/* the far tables and the GI grid's placed share, bound (the sampler types fixed) whether or not
	 * this draw uses them; a GPU with fewer than 23 units never draws far light */
	const WwCellFar * Fr = on && s.farOn && pass == 0 && g.units > kFarPlacedUnit ? wwCellFarFor( scene->nifModel ) : nullptr;
	if ( Fr && !Fr->slotTab.empty() && ( g.farDoc != scene->nifModel || g.farVersion != s.farVersion.value( scene->nifModel ) ) ) {
		auto upBuf = [&]( GLuint & buf, GLuint & tex, GLuint & att, bool & stored, const void * data, size_t bytes ) {
			if ( !tex )
				fn->glGenTextures( 1, &tex );
			bool moved = false;	// lane VKOBJ4: under WW_VKOBJ_SHADE a store (the Vulkan runs read it in place)
			const bool inStore = WwVkPresent::objBlTexelFill( buf, stored, WwBl::vkShadeOn(), { { data, bytes } }, moved );
			if ( moved )
				att = 0;	// re-attached below (a reused name is another buffer)
			if ( inStore )
				return;
			if ( !buf )
				fn->glGenBuffers( 1, &buf );
			fn->glBindBuffer( GL_TEXTURE_BUFFER, buf );
			fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( bytes ), data, GL_STATIC_DRAW );
			fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
		};
		upBuf( g.farSlotBuf, g.farSlotTex, g.farSlotAtt, g.farSlotStored, Fr->slotTab.data(), Fr->slotTab.size() * sizeof( float ) );
		upBuf( g.farRecBuf, g.farRecTex, g.farRecAtt, g.farRecStored, Fr->recs.data(), Fr->recs.size() * sizeof( float ) );
		if ( !Fr->placed.empty() ) {
			if ( !g.farPlacedTex )
				fn->glGenTextures( 1, &g.farPlacedTex );
			u.raw( kFarPlacedUnit );
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
		// lane CELLUNI1: with DSA attached once per buffer (the old path attaches at every call)
		rec.bind( u, kFarSlotUnit, GL_TEXTURE_BUFFER, g.farSlotTex );
		if ( g.farSlotTex && ( !u.dsa || g.farSlotAtt != g.farSlotBuf ) ) {
			u.texBuffer( g.farSlotTex, GL_RGBA32F, g.farSlotBuf );
			g.farSlotAtt = g.farSlotBuf;
		}
		rec.bind( u, kFarRecUnit, GL_TEXTURE_BUFFER, g.farRecTex );
		if ( g.farRecTex && ( !u.dsa || g.farRecAtt != g.farRecBuf ) ) {
			u.texBuffer( g.farRecTex, GL_RGBA32F, g.farRecBuf );
			g.farRecAtt = g.farRecBuf;
		}
		rec.bind( u, kFarPlacedUnit, GL_TEXTURE_3D, farPlacedDraw ? g.farPlacedTex : 0 );
		rec.i( prog, "cellFarSlots", kFarSlotUnit );
		rec.i( prog, "cellFarRecs", kFarRecUnit );
		rec.i( prog, "cellFarPlaced", kFarPlacedUnit );
	} else {	// never read (cellFarOn false): the far samplers ride units of their own types (two samplers of one type may share)
		rec.i( prog, "cellFarSlots", kTextureUnit );
		rec.i( prog, "cellFarRecs", kTextureUnit );
		rec.i( prog, "cellFarPlaced", kGiUnit );
	}
	rec.b( prog, "cellFarOn", farDraw );
	rec.b( prog, "cellFarPlacedOn", farPlacedDraw );
	rec.i( prog, "cellFarRed", s.farRed );
	if ( farDraw ) {
		rec.i( prog, "cellFarBits", Fr->bits );
		rec.i( prog, "cellFarProbe", Fr->maxProbe );
		rec.f( prog, "cellFarCell", Fr->cell );
		rec.f2( prog, "cellFarBand", Fr->band[0], Fr->band[1] );
	}
	if ( farPlacedDraw ) {
		rec.f3( prog, "cellFarPlacedOrigin", Fr->placedOrigin[0], Fr->placedOrigin[1], Fr->placedOrigin[2] );
		rec.f( prog, "cellFarPlacedVoxel", Fr->placedVoxel );
		rec.f3( prog, "cellFarPlacedDims", float( Fr->placedDims[0] ), float( Fr->placedDims[1] ), float( Fr->placedDims[2] ) );
	}
	u.done();
	prof.at( 8, "far" );
	rec.i( prog, "cellGi", kGiUnit );
	rec.b( prog, "cellGiOn", giDraw );
	rec.live( 1 );	rtddgiCellUniforms( scene );	// lane RTXGI1 M2: the ray-traced GI volume (src/features/gi/rtddgicell.h); no-op in an off program
	prof.at( 9, "rtddgi" );
	// without the PRT a weather-lit exterior's sky is the ray-traced volume's (its misses: cell_lights.glsl CELL_GI_UP)
	const bool ddgiSky = !wwCellPrtOn() && L && wwCellAsOutdoor( L ) && wwLookdevActive();	// + sunlit interiors
	rec.b( prog, "cellGiSky", ( giDraw && !skyDraw && G->skyLit ) || ddgiSky );	// lane SKY1 (never on the sky-share grid)
	rec.f( prog, "cellSkyVis", wwCellSkyVis() );	// the Sky visibility row, for the AO decals
	// the sky grid's slabs start after the GI's own (6, or 12 with the rooms' second slot)
	const bool specOccDraw = giDraw && !skyDraw && G->skyLit && g.giSpecSky;
	rec.i( prog, "cellSpecOcc", specOccDraw || ddgiSky ? wwCellSpecOccMode() : 0 );	// or the volume's sky visibility
	rec.f( prog, "cellSpecSkyBase", specOccDraw ? ( G->slotRooms.empty() ? 6.0f : 12.0f ) : 0.0f );	// 0: no sky slot (the water reads it)
	rec.i( prog, "cellGiAmb", s.giAmb );
	rec.b( prog, "cellGiFill", s.giFill && !skyDraw );	// the gap fill is the GI grid's, never the sky share's
	rec.i( prog, "cellPass", pass );
	rec.i( prog, "cellPassRed", s.passRed & ~4 );	// + 8192 WW_CELL_GIWALL_RED=off	// + 2048 nofilter, 4096 nolastfill (was & 251, which dropped them)	// + 8 noclamp; + 16 nosee; + 32 borrow; + 64 lendamb, 128 strand
	if ( giDraw ) {
		rec.f3( prog, "cellGiOrigin", G->origin[0], G->origin[1], G->origin[2] );
		rec.f( prog, "cellGiVoxel", G->voxel );
		rec.f3( prog, "cellGiDims", float( G->dims[0] ), float( G->dims[1] ), float( G->dims[2] ) );
		// lane GIEDGE1: the x/y edge fade; red WW_CELL_GI_RED=clampedge = the old CLAMP_TO_EDGE read past the box
		WW_SWITCH_STATIC( bool, giClampEdge, wwEnvStr( "WW_CELL_GI_RED" ).split( ',' ).contains( QStringLiteral( "clampedge" ) ) );
		rec.f( prog, "cellGiPad", giClampEdge ? 0.0f : float( G->pad ) );
	}
	{	// lane EMISSIVEGRID1: the fine boxes (their atlas rides the grid's texture, after its layers)
		const bool fineDraw = giDraw && !skyDraw && !G->fine.empty();
		rec.b( prog, "cellGiFineOn", fineDraw );
		if ( fineDraw ) {
			WW_SWITCH_STATIC( int, fineRed, []() {	// lane LIVESET1: follows a live set
				const QByteArray r = qgetenv( "WW_CELL_EMISSIVEGRID_RED" ).trimmed();
				return r == "noseen" ? 1 : r == "nosub" ? 2 : 0;
			}() );
			const int giLayers = G->dims[2] * ( ( G->slotRooms.empty() ? 6 : 12 ) + ( g.giSpecSky ? 6 : 0 ) );	// + SPECOCC1's sky slot
			const int n = std::min( int( G->fineBox.size() / 8 ), 8 );
			rec.i( prog, "cellGiFineN", n );
			rec.i( prog, "cellGiFineRed", fineRed );
			for ( int k = 0; k < n; k++ ) {
				const float * b = &G->fineBox[size_t( k ) * 8];
				rec.f4l( prog, "cellGiFineBox[%d]", k, FloatVector4( b[0], b[1], b[2], b[3] ) );
				rec.f4l( prog, "cellGiFineDim[%d]", k, FloatVector4( b[4], b[5], b[6], b[7] + float( giLayers ) ) );
			}
		}
	}
	rec.live( 2 );	waterGi().passSet( prog );	// lane PRTP6: the values just set are the ones a water draw gives back
	prof.at( 10, "gitail" );
	// the imagespace, once this document has a measure; its LUT bound like the grid above
	const float adapted = s.adapted.value( scene->nifModel, -1.0f );
	int isStamp = 0;	// an exterior's imagespace is the weather's (isLighting)
	const WwCellLighting * LI = isLighting( scene->nifModel, &isStamp );
	const bool isDraw = L && LI && !s.measuring && adapted >= 0.0f && wwCellImageSpaceWanted( scene );
	if ( isDraw && LI->isLut.size() == 16 * 16 * 16 * 3
		&& ( g.lutDoc != scene->nifModel || g.lutVersion != s.version.value( scene->nifModel ) || g.lutStamp != isStamp ) ) {
		if ( !g.lutTex )
			fn->glGenTextures( 1, &g.lutTex );
		u.leave( kLutUnit );	// the old code left the unit here, and the binds below put it back to here
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
		toneInputs().lutGen++;
	}
	const bool lutDraw = isDraw && g.lutTex && g.lutDoc == scene->nifModel && LI->isLut.size() == 16 * 16 * 16 * 3;
	toneInputs().lut = lutDraw ? LI->isLut.data() : nullptr;	// VKFRAME1 round 11: the same bytes, for the Vulkan tone pass
	u.section();
	rec.bind( u, kLutUnit, GL_TEXTURE_3D, lutDraw ? g.lutTex : 0 );
	u.done();
	prof.at( 11, "islut" );
	// the measure's bloom, a quarter of the view, sampled at the fragment's place in the view
	const ClBloom * B = nullptr;
	if ( isDraw ) {
		auto it = s.bloom.constFind( scene->nifModel );
		if ( it != s.bloom.constEnd() )
			B = &*it;
	}
	if ( B && B->w > 0 && !B->gpuTex && ( g.bloomDoc != scene->nifModel || g.bloomVersion != B->version ) ) {
		if ( !g.bloomTex )
			fn->glGenTextures( 1, &g.bloomTex );
		u.raw( kBloomUnit );
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
	// the FO4CS bloom stands in for it this frame (FO4CS suppresses the engine bloom the same way)
	// lane PERFFIX1: or the GPU's blur of it (cellhdr.cpp), nothing uploaded
	const GLuint bloomTex = B && B->gpuTex ? GLuint( B->gpuTex ) : g.bloomTex;
	const bool bloomDraw = B && B->w > 0 && bloomTex && ( B->gpuTex || g.bloomDoc == scene->nifModel ) && !( s.isRed & 8 )
		&& !wwCellBloomReplacing();
	rec.bind( u, kBloomUnit, GL_TEXTURE_2D, bloomDraw ? bloomTex : 0 );
	u.done();
	{	// VKFRAME1 round 11: the Vulkan tone pass copies it when it changes (the GPU's blur: every call)
		WwCellToneInputs & ti = toneInputs();
		static const void * tiDoc = nullptr;
		static int tiVersion = -1;
		const unsigned m = bloomDraw ? unsigned( bloomTex ) : 0u;
		if ( m && ( B->gpuTex || m != ti.measure || tiDoc != scene->nifModel || tiVersion != B->version ) )
			ti.measureGen++;
		ti.measure = m;
		ti.mw = m ? B->w : 0;
		ti.mh = m ? B->h : 0;
		tiDoc = scene->nifModel;
		tiVersion = B ? B->version : -1;
	}
	rec.i( prog, "cellIsBloom", kBloomUnit );
	rec.b( prog, "cellIsBloomOn", bloomDraw );
	if ( bloomDraw ) {
		GLint vp[4] = { 0, 0, 1, 1 };
		// lane PERFHUNT2: this runs outside the globals memo window (vpEpoch 0) yet inside the armed pass, whose viewport
		// the memo holds already (it re-arms on a change; wwDrawViewport trusts the same). WW_CELL_UNI6_RED=viewport asks.
		WwCellUni::State & cu = WwCellUni::state();
		const quint64 vpWas = cu.vpEpoch;
		if ( !vpWas && !wwCellUni6Red( "viewport" ) )
			cu.vpEpoch = wwSetupMemoEpochArmed();
		WwCellUni::viewport( fn, vp );
		cu.vpEpoch = vpWas;
		rec.f4l( prog, "cellIsBloomRect", -1, FloatVector4( float( vp[0] ), float( vp[1] ),
			1.0f / float( std::max( vp[2], 1 ) ), 1.0f / float( std::max( vp[3], 1 ) ) ) );
	}
	prof.at( 12, "bloom" );
	// the depth cubes, bound (a sampler left on unit 0 beside BaseMap would be a type clash)
	const bool shadowDraw = L && s.shadowOn && !s.shadowRed && g.shTex && g.shDoc == scene->nifModel;
	rec.bind( u, kShadowUnit, GL_TEXTURE_CUBE_MAP_ARRAY, shadowDraw ? g.shTex : 0 );
	u.done();
	rec.i( prog, "cellShadow", kShadowUnit );
	rec.b( prog, "cellShadowOn", shadowDraw );
	rec.f( prog, "cellShadowTexel", 2.0f / float( g.shFace > 0 ? g.shFace : kShadowFace ) );	// lane LAMPSHADOW1: the tier's face
	// lane GAPLOOK1 (LIGHTSYS-3): the budget's per-slot strengths; off, the shader keeps its old return
	const bool fadeOn = shadowDraw && g.shBudget > 0;
	rec.b( prog, "cellShadowFadeOn", fadeOn );
	rec.live( 3, fadeOn );	if ( fadeOn && prog->uniLocation( "cellShadowFade[0]" ) >= 0 ) {
		r->fn->glUniform1fv( prog->uniLocation( "cellShadowFade[0]" ), kShadowSlots, g.shFade );
		prog->wwUni.forget( prog->uniLocation( "cellShadowFade[0]" ), kShadowSlots );	// lane CELLUNI1: a raw write
	}
	rec.i( prog, "cellIsLut", kLutUnit );
	rec.b( prog, "cellIsOn", isDraw );
	rec.b( prog, "cellIsLinear", isDraw && wwCellHdrActive() );	// write linear light, tone-mapped once
	rec.b( prog, "cellIsLutOn", lutDraw );
	/* lane PHYSUNITS1: unlit effects: additive ones in cd/m2 (the emissive ratio x G), the others on the light's scale
	 * (0 = x 1: the red, or no cell HDR camera) */
	{
		const bool ps = wwPhysSkyOn();
		rec.f( prog, "cellFxEmitK", ps ? wwPhysEmitScale() : 0.0f );
		rec.f( prog, "cellFxUnlitK", !ps ? 0.0f : ( L && L->interior && !wwCellAsOutdoor( L ) ) ? physInteriorK( L ) : wwLookdevPhysLight() );
	}
	if ( isDraw ) {
		const float * h = LI->isHdr;
		if ( wwCellHdrActive() )
			wwPhysNoteCellFrame();	// lane PHYSUNITS1: real units while the cell's HDR camera draws
		// the tonemap PS: max( mid / (lum + 0.001), cb.y ) then min( .., cb.x ); cb (x, y) = HNAM (max, min)
		// lane PHYSUNITS1: the physical camera (EV100) on real units; the red the old law
		wwPhysNoteOutdoor( L && ( !L->interior || wwCellAsOutdoor( L ) ) );	// MODULES1 item 14: the incident meter's daylight
		const float e = wwPhysSkyOn() ? wwPhysExposure( h, adapted ) : wwCellExposure( h, adapted );
		rec.f( prog, "cellIsExposure", e );
		rec.f( prog, "cellIsE", h[1] );
		rec.f( prog, "cellIsAdapted", adapted );
		rec.f3( prog, "cellIsCine", LI->isCine[0], LI->isCine[1], LI->isCine[2] );
		rec.f4l( prog, "cellIsTint", -1, FloatVector4( LI->isTint[0], LI->isTint[1], LI->isTint[2], LI->isTint[3] ) );
		rec.i( prog, "cellIsRed", s.isRed );
		float wb[3];	// lane FIX1101A (atmo): WW_CELL_WB (off = 1, 1, 1: the uniform 0, the shader x 1 exactly)
		wwAtmoWbGain( wb );
		rec.f3( prog, "cellIsWbD", wb[0] - 1.0f, wb[1] - 1.0f, wb[2] - 1.0f );
		rec.b( prog, "cellIsVanCurve", wwCellTonemapVanilla() );	// lane FIX1101A T: the red, the game's filmic curve
	}
	prof.at( 13, "shadow+is" );
	rec.i( prog, "cellLights", kTextureUnit );
	// lane TIMING1: WW_CELL_PASSTIME_LIGHTLOOP=1's unlit frames walk no light (the light loop's GPU timer)
	const int loopCount = PassTime::lightLoopOff() ? 0 : g.count;
	// the cluster lists: their sampler bound on every draw, the lists only on a lit one
	rec.cluster( L ? &g.texels : nullptr, L ? loopCount : 0, g.texelStamp );	wwCellClusterUniforms( scene, L ? &g.texels : nullptr, L ? loopCount : 0, g.texelStamp );
	prof.at( 14, "cluster" );
	if ( !L )
		return;
	rec.i( prog, "cellLightCount", loopCount );
	rec.i( prog, "cellBulbBase", g.bulbBase );	// lane NIFXLIGHT2: the air's bulb-tier lights
	rec.i( prog, "cellBulbCount", g.bulbs );
	{	// lane LIGHTS5: GOBO1's pattern (pin WW_CELL_GOBO_ROT turns it, red WW_CELL_GOBO_RED=flat its mean) and FOGSCALE1
		WW_SWITCH_STATIC( int, goboRot, wwEnvInt( "WW_CELL_GOBO_ROT" ) & 7 );
		WW_SWITCH_STATIC( bool, goboFlat, wwEnvStr( "WW_CELL_GOBO_RED" ) == QLatin1String( "flat" ) );
		WW_SWITCH_STATIC( bool, goboGam, wwEnv( "WW_CELL_GOBO_GAMMA" ).trimmed() == "2.2" );	// GOBO3 red
		rec.b( prog, "cellGoboOn", g.goboAny );
		rec.i( prog, "cellGoboRot", goboRot );
		rec.b( prog, "cellGoboFlat", goboFlat );
		rec.b( prog, "cellGoboGam", goboGam );
		// lane GOBO2: the omni/box records and the mapping (WW_CELL_GOBO_OMNI 0-15; GOBO3: 0 = the game's, Todd's treat)
		WW_SWITCH_STATIC( int, goboMap, wwEnvInt( "WW_CELL_GOBO_OMNI" ) & 15 );
		rec.i( prog, "cellGoboTbl", g.goboTbl );
		rec.i( prog, "cellGoboMap", goboMap );
		rec.b( prog, "cellFogScaleOn", g.fogBase >= 0 );
		rec.i( prog, "cellFogScaleBase", std::max( g.fogBase, 0 ) );
	}
	// world = R^T (posView - t) / sc  (lookdevstage.cpp's fog uses the same inverse)
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	rec.f( prog, "fxDistScale", 1.0f / sc );	// view units -> game units for the soft fades
	for ( int k = 0; k < 3; k++ ) {
		float w = 0.0f;
		for ( int j = 0; j < 3; j++ )
			w -= vt.rotation( j, k ) * vt.translation[j];
		rec.f4l( prog, "cellRow[%d]", k, FloatVector4( vt.rotation( 0, k ) / sc,
			vt.rotation( 1, k ) / sc, vt.rotation( 2, k ) / sc, w / sc ) );
	}
	prof.at( 15, "gobo+row" );
	// PRTP2 section 4: per channel, ((p - n) / 2 per axis, mean of the six), gamma; the shader powers it
	/* lane SKYINT1 (10-29): a Use Sky Lighting interior takes the CURRENT weather's DALC at the hour, as the game's
	 * full-mode sky does there (the cell's XCLL DALC is not read); the same six as the exterior sky (probesky.cpp's
	 * order, its red dalcflip), encoded back to the stored gamma so the shader's law is unchanged */
	const float ( *D )[3] = L->dalc;
	float skyD[6][3];
	bool skyLitAmb = false;
	if ( wwCellSkyLitNow( L ) ) {
		float lin[6][3];
		if ( wwLookdevDalc( lin ) ) {
			const bool flip = wwLookdevRed( "dalcflip" );
			for ( int a = 0; a < 6; a++ )
				for ( int c = 0; c < 3; c++ )
					skyD[a][c] = std::pow( std::max( lin[flip ? a : ( a ^ 1 )][c], 0.0f ), 1.0f / 2.2f );
			D = skyD;
			skyLitAmb = true;
		}
	}
	const bool dalc = ( L->hasDalc || skyLitAmb ) && !( s.red & 4 );
	rec.b( prog, "cellHasDalc", dalc );
	// lane PHYSUNITS1: an interior's own DALC on its lamps' scale (stored gamma: the shader's 2.2 power takes k^(1/2.2))
	const float dk = skyLitAmb ? 1.0f : std::pow( physInteriorK( L ), 1.0f / 2.2f );
	for ( int c = 0; c < 3 && dalc; c++ ) {
		float mean = 0.0f;
		for ( int a = 0; a < 6; a++ )
			mean += D[a][c] / 6.0f;
		rec.f4l( prog, "cellDalc[%d]", c, FloatVector4(
			( D[0][c] - D[1][c] ) * 0.5f * dk, ( D[2][c] - D[3][c] ) * 0.5f * dk,
			( D[4][c] - D[5][c] ) * 0.5f * dk, mean * dk ) );
	}
	if ( L->interior && L->skyLighting ) {	// telemetry, once per change
		static QString skyLitLast;
		const QString line = skyLitAmb
			? QStringLiteral( "cell sky lighting: Use Sky Lighting interior: ambient (DALC) and fog colours from %1 hour %2" )
				.arg( wwLookdevWeatherKey() ).arg( wwLookdevHour(), 0, 'f', 2 )
			: QStringLiteral( "cell sky lighting: Use Sky Lighting interior: the cell's own ambient and fog (%1)" )
				.arg( !wwLookdevActive() ? QStringLiteral( "Lookdev off" ) : ( wwCellSkyLitRed() & 1 )
					? QStringLiteral( "WW_CELL_SKYLIT_RED" ) : QStringLiteral( "no weather" ) );
		if ( line != skyLitLast ) {
			fprintf( stderr, "%s\n", qPrintable( line ) );
			skyLitLast = line;
		}
	}
	// the Ambient Only volumes, in plugin order (the shader takes the first that holds a point)
	const int ambo = ( dalc && !( s.red & 1024 ) ) ? std::min( int( L->ambientLights.size() ), kAmbientSlots ) : 0;
	rec.i( prog, "cellAmboCount", ambo );
	for ( int i = 0; i < ambo; i++ ) {
		const WwCellAmbientLight & a = L->ambientLights[i];
		rec.f4l( prog, "cellAmbo[%d]", i, FloatVector4( a.pos[0], a.pos[1], a.pos[2], a.volume ) );
		// K.w = 1 marks a box volume, its three rows in cellAmboBox
		rec.f4l( prog, "cellAmboK[%d]", i, FloatVector4( a.k[0], a.k[1], a.k[2], a.hasBox ? 1.0f : 0.0f ) );
		for ( int k = 0; k < 3 && a.hasBox; k++ )
			rec.f4l( prog, "cellAmboBox[%d]", i * 3 + k,
				FloatVector4( a.box[k][0], a.box[k][1], a.box[k][2], a.box[k][3] ) );
	}
	prof.at( 16, "dalc+ambo" );
	/* An exterior's directional is the weather's sun (or moon): Lookdev's light, TO it, world, linear.
	 * Only the cell programs take it; Lookdev off (no weather) = the old path. */
	/* lane INTSKY1: a sunlit interior is lit as outdoors (the game's full sky mode): the weather's sun x Sunlight Scale with
	 * its shadows, the weather's DALC, the exterior lit-effect base; the cell's own lights, imagespace and fog distances
	 * stay. Unflagged interiors take every interior path as before */
	const bool outdoor = wwCellAsOutdoor( L );
	bool hasDir = L->hasDirectional;
	float dirTo[3] = { L->dirTo[0], L->dirTo[1], L->dirTo[2] }, dirColor[3] = { L->dirColor[0], L->dirColor[1], L->dirColor[2] };
	for ( int c = 0; c < 3 && !outdoor; c++ )
		dirColor[c] *= physInteriorK( L );	// lane PHYSUNITS1: an interior's directional on its lamps' scale
	/* The game's lit surfaces take the sun x the imagespace Sunlight Scale, so the sun-to-ambient ratio (and the
	 * bounce the probes carry) matches the game. 1 indoors. */
	const float sunScale = ( outdoor && wwLookdevActive() ) ? wwLookdevSunlightScale() : 1.0f;
	rec.f( prog, "cellSunScale", sunScale );
	/* lane VOLAIR1 (LAMPAIR1): the air takes the surfaces' exterior sun and sky (the weather's light x Sunlight Scale,
	 * its DALC) so it is lit by the light the records give, not the fog-sun glare colour; red WW_CELL_AIR_RED=oldsun */
	WW_SWITCH_STATIC( bool, airOldSun, wwEnvStr( "WW_CELL_AIR_RED" ).split( ',' ).contains( QStringLiteral( "oldsun" ) ) );
	/* MODULES1 B9: the far cards (impostor_cell*) take the weather's sun and DALC too. An exterior has no XCLL
	 * directional, so without this their cellHasDir was 0 and impostor_oct.frag lit them by the viewport light, which
	 * follows no hour: white at noon, glowing at night. Red WW_CELL_FARLOD_RED=cardsun: the viewport light again */
	WW_SWITCH_STATIC( bool, cardSunRed, wwEnv( "WW_CELL_FARLOD_RED" ) == "cardsun" );
	const bool cardProg = !cardSunRed && std::string_view( prog->name ).substr( 0, 13 ) == "impostor_cell";
	const bool extProg = wwIsCellProgramName( prog->name ) || wwIsCellDecalProgramName( prog->name ) || ( prog->name == "cell_volair.prog" && !airOldSun )
		|| cardProg;
	if ( outdoor && wwLookdevActive() && extProg ) {
		float dalc[6][3], dif[4] = {}, amb[4] = {};
		if ( wwLookdevDalc( dalc ) ) {
			wwLookdevLight( dirTo, dif, amb );
			for ( int c = 0; c < 3; c++ )
				dirColor[c] = dif[c] * sunScale;
			hasDir = dirColor[0] > 0.0f || dirColor[1] > 0.0f || dirColor[2] > 0.0f;
		}
		// telemetry: what the exterior sun uploaded (or why not), once per change
		static QString sunLast;
		static InputsLast sunIn;
		const float sunKey[9] = { hasDir ? 1.0f : 0.0f, dirTo[0], dirTo[1], dirTo[2], dirColor[0], dirColor[1], dirColor[2],
			prog->uniLocation( "csmMap" ) >= 0 ? 1.0f : 0.0f, sunScale };
		const QString line = cardProg || !sunIn.changed( sunKey, sizeof( sunKey ) ) ? sunLast : hasDir
			? QStringLiteral( "cell sun: %9 directional=weather to=%1,%2,%3 color=%4,%5,%6 (linear) shadows=%7 sunlight scale=%8" )
				.arg( double( dirTo[0] ), 0, 'f', 5 ).arg( double( dirTo[1] ), 0, 'f', 5 ).arg( double( dirTo[2] ), 0, 'f', 5 )
				.arg( double( dirColor[0] ), 0, 'f', 5 ).arg( double( dirColor[1] ), 0, 'f', 5 ).arg( double( dirColor[2] ), 0, 'f', 5 )
				.arg( prog->uniLocation( "csmMap" ) >= 0 ? 1 : 0 ).arg( double( sunScale ), 0, 'f', 3 )
				.arg( L->interior ? QStringLiteral( "sunlit interior" ) : QStringLiteral( "exterior" ) )
			: QStringLiteral( "cell sun: exterior refused (no weather loaded, or a black sun)" );
		if ( line != sunLast ) {
			fprintf( stderr, "%s\n", qPrintable( line ) );
			sunLast = line;
		}
	}
	prof.at( 17, "sun" );
	/* MODULES1 R7-1 (bungo: the fog cards take "either darkness or colored lights" around them): a blended lit effect
	 * card or particle outdoors takes the surfaces' sun / moon (vpSun, shadowed in the shader) and the weather's
	 * ambient (cellDalc) in place of the game's one Effect Lighting colour: at Foggy 22:00 that base was 0.08-0.13
	 * against a moon of 0.002, a white bank in a dark street. Additive cards keep the base (cellDirColor).
	 * Red WW_CELL_FXLOCAL_RED=base (live): the game's base for every lit effect again. */
	WW_SWITCH_STATIC( bool, fxLocalRed, wwEnvStr( "WW_CELL_FXLOCAL_RED" ).split( ',' ).contains( QStringLiteral( "base" ) ) );
	/* MODULES1 R8-1 (bungo: "the fog here should be dark and match the environment lighting"): the card keeps only the
	 * share of the weather's ambient a surface there keeps (1 - the GI volume's sky share) and takes the volume's
	 * light for the rest (cell_fxlaw.glsl fxLocalSky). Red word `ambient` (live): round 7's whole ambient again. */
	WW_SWITCH_STATIC( bool, fxLocalAmbRed, wwEnvStr( "WW_CELL_FXLOCAL_RED" ).split( ',' ).contains( QStringLiteral( "ambient" ) ) );
	// the law's lit band is a receiver: weight = the band, not band x lighting influence. Red word `influence` (live)
	WW_SWITCH_STATIC( bool, fxLocalInfRed, wwEnvStr( "WW_CELL_FXLOCAL_RED" ).split( ',' ).contains( QStringLiteral( "influence" ) ) );
	/* MODULES1 round 9 (bungo: a mist card "is a slab of denser air"): a blended lit mesh card on the volumetric law
	 * takes the air's own scatter at its pixel (fo4_effectshader.frag fxAirCard). Red word `lump` (live): the lump's
	 * surface share + the GI surface term again. */
	WW_SWITCH_STATIC( bool, fxLocalLumpRed, wwEnvStr( "WW_CELL_FXLOCAL_RED" ).split( ',' ).contains( QStringLiteral( "lump" ) ) );
	/* FARWATER1 late item 4 (bungo b71: the barrel fire does not light its smoke): an air card's lamps take the dense
	 * plume's multiple scatter x its opacity (cell_volpart.glsl vpLampMs). Red word `thinlamps` (live): thin share only. */
	WW_SWITCH_STATIC( bool, fxLocalThinLampsRed, wwEnvStr( "WW_CELL_FXLOCAL_RED" ).split( ',' ).contains( QStringLiteral( "thinlamps" ) ) );
	bool fxLocalHere = false;
	float fxSunLocal[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	/* lane SUNCELL1 final: a lit effect or lit particle outdoors (fo4_effectcell / particles_cell) takes the game's
	 * base term: the game's lit-effect setup puts pow(directional colour, 2.2) x the light's fade x
	 * the imagespace Sunlight Scale (the imagespace HDR data's 7th float, HNAM's 7th float) in
	 * cb2[12]; the scene's directional outdoors is the sun. No ambient: the lit asm reads cb2 rows 0..14 only
	 * (lights 0..11, base 12, alpha 13..14).
	 * the game puts that same Sunlight Scale on the
	 * SURFACES' sun too (the lit-surface and deferred-light setups both multiply the imagespace Sunlight Scale
	 * in; checked in Todd's treat), so effect and wall keep a 1:1 sun. This
	 * viewer's surfaces take the sun unscaled, so a lit effect takes the surfaces' scale (1): with x4.5 it drew 4.5x
	 * brighter than the wall behind it. Red WW_CELL_FXLIT_SCALE_RED=game puts the Sunlight Scale back (in and out).
	 * the colour is not the sunlight. The game's per-frame sun update copies the sky's colours into the sun light:
	 * the diffuse (what the surfaces read) from the weather's Sunlight row, and a second colour (the light's
	 * effect colour) from its Effect Lighting row (NAM0 row 9); the effect
	 * shader's lit branch reads that effect colour. CommonwealthClear at noon: Sunlight 225 grey, Effect Lighting 150 grey
	 * (0.41x linear); at night 53,70,87 vs 33,51,67. Red WW_CELL_FXLIT_COLOR_RED=sun takes the sunlight again. */
	{
		WW_SWITCH_STATIC( bool, fxScaleRed, wwEnv( "WW_CELL_FXLIT_SCALE_RED" ).trimmed() == "game" );
		WW_SWITCH_STATIC( bool, fxColorRed, wwEnv( "WW_CELL_FXLIT_COLOR_RED" ).trimmed() == "sun" );
		const bool fxProg = prog->name == std::string_view( "fo4_effectcell.prog" )
			|| prog->name == std::string_view( "fo4_effectcellfx.prog" )	// lane FXRES1
			|| prog->name == std::string_view( "particles_cell.prog" );
		// the decal programs keep the surfaces' sun and take the lit-effect base as cellFxBase (fxRoomLit)
		const bool roomProg = wwIsCellDecalProgramName( prog->name );
		float dalc[6][3];
		if ( outdoor && roomProg ) {
			float base[3] = { 0.0f, 0.0f, 0.0f };
			if ( wwLookdevActive() && wwLookdevDalc( dalc ) ) {
				float dif[4] = {}, amb[4] = {};
				wwLookdevLight( dirTo, dif, amb );
				float fxCol[3] = { dif[0], dif[1], dif[2] };
				if ( !fxColorRed )
					wwLookdevEffectLight( fxCol );
				WwLookdevIs is;
				const float fxScale = fxScaleRed ? ( wwLookdevImageSpace( is ) ? is.hdr[6] : 1.0f ) : sunScale;
				for ( int c = 0; c < 3; c++ )
					base[c] = std::max( fxCol[c], 0.0f ) * fxScale;
			}
			rec.f3( prog, "cellFxBase", base[0], base[1], base[2] );
		}
		if ( outdoor && fxProg && wwLookdevActive() && wwLookdevDalc( dalc ) ) {
			float dif[4] = {}, amb[4] = {};
			wwLookdevLight( dirTo, dif, amb );
			float fxCol[3] = { dif[0], dif[1], dif[2] };
			for ( int c = 0; c < 3; c++ )
				fxSunLocal[c] = std::max( dif[c], 0.0f ) * sunScale;	// R7-1: the surfaces' sun, before the effect row
			fxLocalHere = !fxLocalRed;
			fxSunLocal[3] = fxLocalHere ? 1.0f : 0.0f;
			if ( !fxColorRed )
				wwLookdevEffectLight( fxCol );	// the Effect Lighting row
			WwLookdevIs is;
			const bool haveIs = wwLookdevImageSpace( is );
			const float isScale = haveIs ? is.hdr[6] : 1.0f;
			const float fxScale = fxScaleRed ? isScale : sunScale;	// the surfaces' scale
			for ( int c = 0; c < 3; c++ )
				dirColor[c] = std::max( fxCol[c], 0.0f ) * fxScale;
			hasDir = dirColor[0] > 0.0f || dirColor[1] > 0.0f || dirColor[2] > 0.0f;
			static QString fxSunLast;
			static InputsLast fxSunIn;
			const float fxKey[11] = { dirColor[0], dirColor[1], dirColor[2], fxCol[0], fxCol[1], fxCol[2], fxScale, isScale,
				haveIs ? 1.0f : 0.0f, sunScale, ( fxScaleRed ? 1.0f : 0.0f ) + ( fxColorRed ? 2.0f : 0.0f ) };
			const QString line = !fxSunIn.changed( fxKey, sizeof( fxKey ) ) ? fxSunLast : QStringLiteral( "cell fx sun: exterior lit effects base=%1,%2,%3 (%10 %4,%5,%6 x %7; "
					"imagespace Sunlight Scale %8%9, surfaces' sun scale %11)" )
				.arg( double( dirColor[0] ), 0, 'f', 5 ).arg( double( dirColor[1] ), 0, 'f', 5 ).arg( double( dirColor[2] ), 0, 'f', 5 )
				.arg( double( fxCol[0] ), 0, 'f', 5 ).arg( double( fxCol[1] ), 0, 'f', 5 ).arg( double( fxCol[2] ), 0, 'f', 5 )
				.arg( double( fxScale ), 0, 'f', 3 ).arg( double( isScale ), 0, 'f', 3 )
				.arg( haveIs ? ( fxScaleRed ? QStringLiteral( ", RED WW_CELL_FXLIT_SCALE_RED=game" ) : QString() )
					: QStringLiteral( ", no imagespace: 1" ) )
				.arg( fxColorRed ? QStringLiteral( "sunlight, RED WW_CELL_FXLIT_COLOR_RED=sun," )
					: QStringLiteral( "effect lighting" ) ).arg( double( sunScale ), 0, 'f', 3 );
			if ( line != fxSunLast ) {
				fprintf( stderr, "%s\n", qPrintable( line ) );
				fxSunLast = line;
			}
			static QString fxLocLast;
			const QString loc = QStringLiteral( "cell fx local: blended lit effect cards and particles take %1 (surfaces' sun %2,%3,%4 linear, "
					"shadowed; the game's Effect Lighting base %5,%6,%7 stays on additive cards)" )
				.arg( fxLocalHere ? ( fxLocalAmbRed
						? QStringLiteral( "the surfaces' sun + the WHOLE weather ambient at the card + its lamps, RED WW_CELL_FXLOCAL_RED=ambient" )
						: QStringLiteral( "the surfaces' sun + the weather ambient x (1 - the GI volume's sky share) + the volume's light + "
							"its lamps, as a surface there (share and lamp shadows per pixel on the GPU)" ) )
						+ ( fxLocalInfRed ? QStringLiteral( "; weight = band x lighting influence, RED WW_CELL_FXLOCAL_RED=influence" )
							: QStringLiteral( "; a lit-band material is a receiver: weight = the band, the lighting influence reads 1" ) )
						+ ( fxLocalLumpRed ? QStringLiteral( "; volumetric mesh cards are lumps: the surface share + the GI surface term, RED WW_CELL_FXLOCAL_RED=lump" )
							: QStringLiteral( "; a volumetric mesh card is a slab of air: the air's scatter at its pixel (lamps x phase in their "
								"own colours + the sky share under the record's cap) x its opacity, no surface share, no GI surface term" )
							+ ( fxLocalThinLampsRed ? QStringLiteral( "; its lamps thin only (1/4 phase), RED WW_CELL_FXLOCAL_RED=thinlamps" )
								: QStringLiteral( "; its lamps mix( thin, the multiple scatter's diffuse wrap( N.L, 1 ), opacity ) (FARWATER1)" ) ) )
					: QStringLiteral( "the game's base + lamps, no ambient, RED WW_CELL_FXLOCAL_RED=base" ) )
				.arg( double( fxSunLocal[0] ), 0, 'f', 5 ).arg( double( fxSunLocal[1] ), 0, 'f', 5 ).arg( double( fxSunLocal[2] ), 0, 'f', 5 )
				.arg( double( dirColor[0] ), 0, 'f', 5 ).arg( double( dirColor[1] ), 0, 'f', 5 ).arg( double( dirColor[2] ), 0, 'f', 5 );
			if ( loc != fxLocLast ) {
				fprintf( stderr, "%s\n", qPrintable( loc ) );
				fxLocLast = loc;
			}
		} else if ( outdoor && fxProg ) {
			static bool fxSunSaid = false;
			if ( !fxSunSaid ) {
				fprintf( stderr, "cell fx sun: exterior lit effects take no sun (no weather loaded)\n" );
				fxSunSaid = true;
			}
		}
		/* Indoors the game's lit-effect setup also multiplies in the cell imagespace's Sunlight Scale (HNAM[6]);
		 * here only under the scale red, since the surfaces take scale 1. */
		WW_SWITCH_STATIC( bool, fxIntRed, wwEnv( "WW_CELL_FXLIT_INT_RED" ).trimmed() == "noscale" );
		if ( !outdoor && fxProg ) {
			const bool scaled = fxScaleRed && !fxIntRed && L->hasImageSpace;
			if ( scaled )
				for ( int c = 0; c < 3; c++ )
					dirColor[c] *= L->isHdr[6];
			static QString fxIntLast;
			const QString line = scaled
				? QStringLiteral( "cell fx sun: interior lit effects base x Sunlight Scale %1 (%2)" )
					.arg( double( L->isHdr[6] ), 0, 'f', 3 ).arg( L->isName )
				: QStringLiteral( "cell fx sun: interior lit effects base unscaled (%1)" )
					.arg( fxIntRed ? QStringLiteral( "RED WW_CELL_FXLIT_INT_RED=noscale" )
						: !fxScaleRed ? QStringLiteral( "the surfaces' sun scale 1" ) : QStringLiteral( "no imagespace read" ) );
			if ( line != fxIntLast ) {
				fprintf( stderr, "%s\n", qPrintable( line ) );
				fxIntLast = line;
			}
		}
	}
	if ( prog->uniLocation( "vpSun" ) >= 0 )
		rec.f4( prog, "vpSun", FloatVector4( fxSunLocal[0], fxSunLocal[1], fxSunLocal[2], fxSunLocal[3] ) );
	if ( prog->uniLocation( "fxLocalRed" ) >= 0 )
		rec.i( prog, "fxLocalRed", ( fxLocalAmbRed ? 1 : 0 ) | ( fxLocalInfRed ? 2 : 0 ) | ( fxLocalLumpRed ? 4 : 0 )
			| ( fxLocalThinLampsRed ? 8 : 0 ) );	// R8-1, round 9
	prof.at( 18, "fxsun" );
	/* An exterior's ambient is the weather's DALC, uploaded in the interior's form so cellAmbient is one law
	 * indoors and out. The axis is the light's TRAVEL direction (Z- lights an up-facing normal). */
	bool extDalc = false;
	float ambLum = -1.0f;	// lane SHADOWPAR1: the DALC's mean luminance (linear) for the air tail's ambient share
	if ( outdoor && ( hasDir || fxLocalHere ) && wwLookdevActive() && ( extProg || fxLocalHere ) ) {	// R7-1: the cards' ambient
		WW_SWITCH_STATIC( QByteArray, extRed, wwEnv( "WW_CELL_EXTAMB_RED" ).trimmed() );
		float w[6][3];
		if ( extRed != "flat" && wwLookdevDalc( w ) ) {
			/* lane LIGHTNING2 (LIGHTNINGSKY1): a storm's flash lights the terrain, the objects and the air through the
			 * one ambient they share, the lit cloud base above integrated over the ground (cloudlightning.h
			 * groundDalc); all 0 between flashes (the weather's DALC as before), WW_LIGHTNING_SKY_RED = none */
			float flash[6][3];
			if ( wwLightningGround( flash ) )
				for ( int a = 0; a < 6; a++ )
					for ( int c = 0; c < 3; c++ )
						w[a][c] += flash[a][c];
			const bool flip = extRed == "flip";
			WW_SWITCH_STATIC( bool, fitRed, wwEnvStr( "WW_CELL_DALC_FIT_RED" ).trimmed() == QLatin1StringView( "1" ) );
			float g[6][3];
			for ( int a = 0; a < 6; a++ )
				for ( int c = 0; c < 3; c++ )
					g[a][c] = std::pow( std::max( w[a][c], 0.0f ), 1.0f / 2.2f );
			for ( int c = 0; c < 3; c++ ) {
				float mean = 0.0f;
				for ( int a = 0; a < 6; a++ )
					mean += g[a][c] / 6.0f;
				ambLum = std::max( ambLum, 0.0f ) + mean * mean * ( c == 0 ? 0.2126f : c == 1 ? 0.7152f : 0.0722f );
				const float sgn = flip ? 0.5f : -0.5f;	// travel: n.z = +1 takes Z- (index 5)
				const float gc[6] = { g[0][c], g[1][c], g[2][c], g[3][c], g[4][c], g[5][c] };
				float e[4];
				WwDalcFit::encode( gc, sgn, fitRed, e );
				rec.f4l( prog, "cellDalc[%d]", c, FloatVector4( e[0], e[1], e[2], e[3] ) );
			}
			extDalc = true;
			static QString ambLast;
			static InputsLast ambIn;
			float ambKey[19];
			std::memcpy( ambKey, g, sizeof( g ) );
			ambKey[18] = flip ? 1.0f : 0.0f;
			const QString line = !ambIn.changed( ambKey, sizeof( ambKey ) ) ? ambLast : QStringLiteral( "cell ambient: exterior=weather DALC up=%1,%2,%3 down=%4,%5,%6 (byte/255)%7" )
				.arg( double( flip ? g[4][0] : g[5][0] ), 0, 'f', 4 ).arg( double( flip ? g[4][1] : g[5][1] ), 0, 'f', 4 )
				.arg( double( flip ? g[4][2] : g[5][2] ), 0, 'f', 4 ).arg( double( flip ? g[5][0] : g[4][0] ), 0, 'f', 4 )
				.arg( double( flip ? g[5][1] : g[4][1] ), 0, 'f', 4 ).arg( double( flip ? g[5][2] : g[4][2] ), 0, 'f', 4 )
				.arg( flip ? QStringLiteral( " RED=flip" ) : QString() )
				+ ( fitRed ? QStringLiteral( " fit=RED mean of six (WW_CELL_DALC_FIT_RED=1)" )
				           : QStringLiteral( " fit=up/down poles exact (red WW_CELL_DALC_FIT_RED=1)" ) );
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
	if ( fxLocalHere ) {	// R7-1: cellDalc is uploaded for the card's own read; the surface-only paths and the air's share stay as they were
		extDalc = false;
		ambLum = -1.0f;
	}
	prof.at( 19, "extamb" );
	rec.b( prog, "cellExtDalc", extDalc );
	rec.b( prog, "cellHasDir", hasDir );
	if ( std::string_view( prog->name ).substr( 0, 13 ) == "impostor_cell" && outdoor ) {	// MODULES1 B9's line, once per change
		static int cardLast = -1;
		const int key = ( hasDir ? 1 : 0 ) | ( extDalc ? 2 : 0 ) | ( cardSunRed ? 4 : 0 );
		if ( key != cardLast ) {
			fprintf( stderr, "far cards light: sun %s, ambient %s%s\n", hasDir ? "weather" : "none (the viewport light)",
				extDalc ? "weather DALC" : "flat NAM0", cardSunRed ? " RED=cardsun" : "" );
			cardLast = key;
		}
	}
	rec.f3( prog, "cellDirColor", dirColor[0], dirColor[1], dirColor[2] );
	rec.f3( prog, "cellDirTo", dirTo[0], dirTo[1], dirTo[2] );
	if ( ambLum >= 0.0f ) {
		// the share of the air's light that no sun shadow dims: the ambient over it and the sun's isotropic quarter
		const float sunLum = 0.2126f * dirColor[0] + 0.7152f * dirColor[1] + 0.0722f * dirColor[2];
		wwShadowSetAirAmbientShare( ambLum / std::max( ambLum + 0.25f * sunLum, 1e-6f ) );
	}
	rec.b( prog, "cellInterior", !outdoor );	// lane INTSKY1: a sunlit interior shades as outdoors
	rec.f3( prog, "cellCenter", L->center[0], L->center[1], L->center[2] );
	rec.i( prog, "cellProbe", s.measuring ? 6 : s.probe );
	if ( const int dv = wwDebugViewId() )
		rec.i( prog, "wwDebugView", dv );	// lane DEBUGVIEW1: only while a view is on (off: no call)
	// a harness reads a dark linear frame through 8 bits; the imagespace's own measure stays x 1
	WW_SWITCH_STATIC( float, probeGain, wwEnvIsSet( "WW_CELL_LIT_PROBE_GAIN" )
		? std::max( wwEnv( "WW_CELL_LIT_PROBE_GAIN" ).toFloat(), 0.0f ) : 1.0f );
	rec.f( prog, "cellProbeGain", s.measuring ? 1.0f : probeGain );
	rec.i( prog, "cellRed", s.red );
	// lane LIGHTPHYS1: all zero = the game's curve and point lights (cell_lights.glsl cellPhysFalloff, cellAreaSpec)
	rec.f4l( prog, "cellLightPhys", -1, FloatVector4( float( phys.mode ), phys.anchor, phys.sourceFraction, phys.advNumerator ) );
	rec.f4l( prog, "cellLightPhysB", -1, FloatVector4( phys.advMinMetres, phys.areaWorld, float( phys.red ), 0.0f ) );
	// WW_CELL_CAM_DUMP: wwCellCamDump, once a frame from GLView::paintGL (lane CELLSPAWN2)
	/* An interior's fog in place of the Lookdev weather fog: the same formula, height = world z. No fog sun
	 * indoors (inferred: the game's directional fog term follows the sun). */
	if ( L->hasFog && prog->uniLocation( "fogOn" ) >= 0 ) {
		rec.b( prog, "fogOn", true );
		/* lane SKYINT1 (10-29): a Use Sky Lighting interior's fog colours (K rows 1..4) are the current weather's, as the
		 * game's full-mode sky keeps them; the distances, power and max stay the cell's (the engine's fog distances there
		 * were not read: unknown) */
		float fK[6][4];
		std::memcpy( fK, L->fogK, sizeof fK );
		WwFog wf;
		const bool skyFog = wwCellSkyLitNow( L ) && wwLookdevFogNow( wf );
		if ( skyFog )
			for ( int k = 1; k <= 4; k++ )
				for ( int c = 0; c < 3; c++ )
					fK[k][c] = wf.K[k][c];
		// lane PHYSUNITS1: the fog's colours in the light's units; fogUnit lets the gray weight read the old scale
		const float fu = !wwPhysSkyOn() ? 1.0f : skyFog ? wwLookdevPhysLight() : physInteriorK( L );
		for ( int k = 1; k <= 4; k++ )
			for ( int c = 0; c < 3; c++ )
				fK[k][c] *= fu;
		rec.f( prog, "fogUnit", fu );
		for ( int k = 0; k < 6; k++ )
			rec.f4l( prog, "fogK[%d]", k,
				FloatVector4( fK[k][0], fK[k][1], fK[k][2], fK[k][3] ) );
		float zr[4] = { 0, 0, 0, 0 };
		for ( int k = 0; k < 3; k++ ) {
			zr[k] = vt.rotation( k, 2 ) / sc;
			zr[3] -= vt.rotation( k, 2 ) * vt.translation[k] / sc;
		}
		rec.f4( prog, "fogView", FloatVector4( zr[0], zr[1], zr[2], zr[3] ) );
		rec.f( prog, "fogDistScale", 1.0f / sc );
		rec.f4( prog, "fogSun", FloatVector4( 0.0f, 0.0f, 1.0f, 0.0f ) );
		rec.f4( prog, "fogSunColour", FloatVector4( 0.0f, 0.0f, 0.0f, 1.0f ) );
		rec.f4( prog, "fogProbe", FloatVector4( 0.0f, 0.0f, float( s.fogProbe ), 0.0f ) );
		rec.i( prog, "fogRed", 0 );
	}
	prof.at( 20, "tail" );
}

int wwCellSkyLitRed()
{
	WW_SWITCH_STATIC( int, red, [] {
		int bits = 0;	// a comma list
		for ( const QString & v : wwEnvStr( "WW_CELL_SKYLIT_RED" ).trimmed().toLower().split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) )
			bits |= v == QLatin1String( "fixed" ) ? 3 : v == QLatin1String( "xcll" ) ? 1 : v == QLatin1String( "sky" ) ? 2
				: v == QLatin1String( "ground" ) ? 4 : 0;
		return bits;
	}() );
	return red;
}

bool wwCellSkyLitNow( const WwCellLighting * L )
{
	// lane INTSKY1: the interior takes the live weather (its sun or not); SKYINT1's rule (bits 7 + 8) without a file
	// MODULES1 M1 (11-10 audit, physical reason): interior-only by nature, an exterior always takes the live weather
	return L && L->interior && wwCellSunlit( L ).weather && wwLookdevActive() && !( wwCellSkyLitRed() & 1 );
}

int wwIntSkyRed()
{
	WW_SWITCH_STATIC( int, red, [] {
		int bits = 0;	// a comma list
		for ( const QString & v : wwEnvStr( "WW_INTSKY_RED" ).trimmed().toLower().split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) )
			bits |= v == QLatin1String( "off" ) ? 1 : v == QLatin1String( "norecx" ) ? 2 : v == QLatin1String( "onesided" ) ? 4
				: v == QLatin1String( "decals" ) ? 8 : v == QLatin1String( "rain" ) ? 16 : 0;
		return bits;
	}() );
	return red;
}

/* lane INTSKY1: the decision, memoised per lighting and re-made when the Cells files or the red move (wwRecxScan is
 * rate-limited to half a second inside). Census on stderr once per change. */
WwCellSunlit wwCellSunlit( const WwCellLighting * L )
{
	WwCellSunlit r;
	if ( !L )
		return r;
	if ( !L->interior ) {
		r.on = r.weather = true;
		r.why = QStringLiteral( "exterior" );
		return r;
	}
	struct Memo
	{
		const WwCellLighting * L = nullptr;
		quint64 gen = ~0ULL;
		int red = -1;
		QString edid;	// a republish can reuse the address: the cell and its flag key the memo too
		bool flag = false;
		WwCellSunlit r;
		QString said;
	};
	static Memo m;
	/* every program setup asks: the roots are made once per cell and the Cells files' generation is asked at most
	 * every half second (wwRecxScan's own rate), so an interior pays a compare per call */
	static QStringList roots;
	static const WwCellLighting * rootsOf = nullptr;
	static QString rootsEdid;
	static QElapsedTimer scanClock;
	static quint64 gen = 0;
	const int red = wwIntSkyRed();
	const bool same = m.L == L && m.edid == L->cellEdid && m.flag == L->useSkyLighting;
	if ( rootsOf != L || rootsEdid != L->cellEdid ) {
		rootsOf = L;
		rootsEdid = L->cellEdid;
		roots = L->cellPlugin.isEmpty() ? QStringList() : wwRecxRoots( L->recxPlugins, L->recxData );
		scanClock.invalidate();
	}
	if ( !( red & 2 ) && !roots.isEmpty() && ( !scanClock.isValid() || scanClock.elapsed() >= 500 ) ) {
		gen = wwRecxScan( roots, QStringLiteral( "Cells" ) );
		scanClock.start();
	}
	const quint64 g = ( red & 2 ) || roots.isEmpty() ? 0ULL : gen;
	if ( same && m.gen == g && m.red == red )
		return m.r;
	m.L = L;
	m.edid = L->cellEdid;
	m.flag = L->useSkyLighting;
	m.gen = g;
	m.red = red;
	// the rule (gl/cellsunlit.h, tests/features/sky/cellsunlit): the block's keys over the winning record's flag, weather = enabled
	r.why = QStringLiteral( "flag 0x%1 Use Sky Lighting %2" ).arg( L->useSkyLighting ? QStringLiteral( "100" ) : QStringLiteral( "0" ) )
		.arg( L->useSkyLighting ? QStringLiteral( "on" ) : QStringLiteral( "off" ) );
	bool haveBlock = false;
	int enabled = wwsunlit::Absent, weather = wwsunlit::Absent;
	if ( !( red & 2 ) && !roots.isEmpty() ) {
		const WwRecxHit h = wwRecxFind( roots, L->recxPlugins, QStringLiteral( "Cells" ), QStringLiteral( "cell" ), L->cellPlugin,
			L->cellLocal, L->cellEdid, QStringLiteral( "sunlight" ), true );
		if ( h.ok ) {
			haveBlock = true;
			const QJsonValue en = wwRecxBlockValue( h.block, QStringLiteral( "enabled" ), true );
			const QJsonValue we = wwRecxBlockValue( h.block, QStringLiteral( "weather" ), true );
			QString warn;
			if ( en.isBool() )
				enabled = en.toBool() ? wwsunlit::True : wwsunlit::False;
			else if ( !en.isUndefined() )
				warn += QStringLiteral( " (enabled not a bool: the flag's)" );
			if ( we.isBool() )
				weather = we.toBool() ? wwsunlit::True : wwsunlit::False;
			else if ( !we.isUndefined() )
				warn += QStringLiteral( " (weather not a bool: = enabled)" );
			r.why = QStringLiteral( "recx %1 (%2)%3" ).arg( QDir::toNativeSeparators( h.file ), h.rung, warn );
		}
	}
	const wwsunlit::Decision dd = wwsunlit::decide( L->useSkyLighting, haveBlock, enabled, weather, ( red & 1 ) != 0 );
	r.on = dd.on;
	r.weather = dd.weather;
	if ( red & 1 )
		r.why += QStringLiteral( ", RED WW_INTSKY_RED=off" );
	m.r = r;
	const QString line = QStringLiteral( "cell sunlit: %1 sun+sky %2, live weather %3 by %4" ).arg( L->cellEdid.isEmpty() ? QStringLiteral( "?" ) : L->cellEdid )
		.arg( r.on ? QStringLiteral( "on" ) : QStringLiteral( "off" ) ).arg( r.weather ? QStringLiteral( "on" ) : QStringLiteral( "off" ) ).arg( r.why );
	if ( line != m.said ) {
		std::fprintf( stderr, "%s\n", qPrintable( line ) );
		m.said = line;
	}
	return r;
}

bool wwCellSunlitNow( const WwCellLighting * L )
{
	return L && L->interior && wwLookdevActive() && wwCellSunlit( L ).on;
}

bool wwCellAsOutdoor( const WwCellLighting * L )
{
	return L && ( !L->interior || wwCellSunlitNow( L ) );
}

bool wwCellGiGpuOn()
{
	const QByteArray pin = wwEnv( "WW_CELL_GI_GPU" ).trimmed();
	if ( !pin.isEmpty() )
		return pin != "0";
	return QSettings().value( QStringLiteral( "WW/CellGiGpu" ), false ).toBool();
}

void wwCellGiGpuSetOn( bool on )
{
	if ( !wwEnv( "WW_CELL_GI_GPU" ).trimmed().isEmpty() )
		return;
	QSettings().setValue( QStringLiteral( "WW/CellGiGpu" ), on );
}

bool wwCellEmissiveGridOn()
{
	const QByteArray pin = qgetenv( "WW_CELL_EMISSIVEGRID" ).trimmed();
	if ( !pin.isEmpty() )
		return pin != "0";
	return QSettings().value( QStringLiteral( "WW/CellEmissiveGrid" ), false ).toBool();
}

void wwCellEmissiveGridSetOn( bool on )
{
	if ( !qgetenv( "WW_CELL_EMISSIVEGRID" ).trimmed().isEmpty() )
		return;
	QSettings().setValue( QStringLiteral( "WW/CellEmissiveGrid" ), on );
}

// the Sky visibility row (celllights.h)
namespace
{
int & skyVisCache()
{
	static int pct = -1;
	static quint64 liveGen = 0;	// lane LIVESET1: a live set of the pin reads it (or the saved row) again
	if ( pct < 0 || wwSwitchMovedFor( liveGen, { "WW_CELL_SKYSTR" } ) ) {
		const QByteArray pin = wwEnv( "WW_CELL_SKYSTR" ).trimmed();
		pct = pin.isEmpty() ? QSettings().value( QStringLiteral( "WW/CellSkyVis" ), 100 ).toInt() : pin.toInt();
		pct = std::clamp( pct, 0, 100 );
	}
	return pct;
}
}

bool wwCellSkyVisPinned()
{
	return !wwEnv( "WW_CELL_SKYSTR" ).trimmed().isEmpty();
}

int wwCellSkyVisPercent()
{
	return skyVisCache();
}

float wwCellSkyVis()
{
	WW_SWITCH_STATIC( bool, ignore, wwEnv( "WW_CELL_SKYSTR_RED" ).trimmed() == "ignore" );
	return ignore ? 1.0f : float( skyVisCache() ) / 100.0f;
}

void wwCellSkyVisSetPercent( int pct )
{
	if ( wwCellSkyVisPinned() )
		return;
	skyVisCache() = std::clamp( pct, 0, 100 );
	QSettings().setValue( QStringLiteral( "WW/CellSkyVis" ), skyVisCache() );
}

// lane SPECOCC1: the sky specular occlusion row (celllights.h)
namespace
{
int & specOccCache()
{
	static int on = -1;
	static quint64 liveGen = 0;	// fix TDFIX2: a live set of WW_CELL_SPECOCC re-reads it
	if ( on < 0 || wwSwitchMovedFor( liveGen, { "WW_CELL_SPECOCC" } ) ) {
		liveGen = wwSwitchGen();
		const QByteArray pin = qgetenv( "WW_CELL_SPECOCC" ).trimmed();
		on = pin.isEmpty() ? ( QSettings().value( QStringLiteral( "WW/CellSpecOcc" ), false ).toBool() ? 1 : 0 ) : ( pin == "0" ? 0 : 1 );
	}
	return on;
}
}

bool wwCellSpecOccPinned()
{
	return !qgetenv( "WW_CELL_SPECOCC" ).trimmed().isEmpty();
}

bool wwCellSpecOccOn()
{
	return specOccCache() != 0;
}

void wwCellSpecOccSetOn( bool on )
{
	if ( wwCellSpecOccPinned() )
		return;
	specOccCache() = on ? 1 : 0;
	QSettings().setValue( QStringLiteral( "WW/CellSpecOcc" ), on );
}

int wwCellSpecOccMode()
{
	WW_SWITCH_STATIC( QByteArray, red, qgetenv( "WW_CELL_SPECOCC_RED" ).trimmed() );	// fix TDFIX2: follows a live set
	if ( !wwCellSpecOccOn() || red == "ao" )
		return 0;
	return red == "hemi" ? 2 : 1;
}

bool wwCellDecalLitRed()
{
	WW_SWITCH_STATIC( bool, red, wwEnv( "WW_CELL_DECALLIT_RED" ).trimmed() == "unlit" );
	return red;
}

bool wwCellDecalLitProbe()
{
	WW_SWITCH_STATIC( bool, on, wwEnvInt( "WW_CELL_DECALLIT_PROBE" ) == 1 );
	return on;
}

int wwCellFxRed()
{
	WW_SWITCH_STATIC( int, red, [] {
		int r = 0;
		for ( const QByteArray & t : wwEnv( "WW_CELL_FX_RED" ).split( ',' ) )
			r |= t == "legacy" ? 1 : t == "nosoft" ? 2 : t == "nolin" ? 4 : t == "hide" ? 8 : t == "prewater" ? 16
				: t == "unbound" ? 32 : 0;	// MODULES1 item 2: 32 = no finite guard on a lit effect's light (the black shards)
		return r;
	}() );
	return red;
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
	const WwCellFar * Fr = scene && scene->nifModel ? wwCellFarFor( scene->nifModel ) : nullptr;
	if ( s.farOn || Fr )
		o += QStringLiteral( " far=%1(asked=%2%3, red=%4)" ).arg( wwCellLightsWanted( scene ) && s.farOn && Fr ? "on" : "off" )
			.arg( s.farOn ? 1 : 0 ).arg( Fr ? QStringLiteral( ", %1" ).arg( Fr->summary ) : QStringLiteral( ", none published" ) ).arg( s.farRed );
	if ( s.dotsLast >= 0 )	// the bulb dots the last frame drew
		o += QStringLiteral( " fardots=%1" ).arg( s.dotsLast );
	if ( s.probe )
		o += QStringLiteral( " probe=%1" ).arg( s.probe );
	if ( s.flickerOn || s.flickerRed ) {	// lane LIGHTFLICKER1
		const auto fd = scene && scene->nifModel ? flickDocs().constFind( scene->nifModel ) : flickDocs().constEnd();
		const bool known = fd != flickDocs().constEnd();
		o += QStringLiteral( " flicker=%1(asked=%2, lights=%3, moving=%4, frames=%5, red=%6, step=%7)" )
			.arg( flickLive( scene ) ? "on" : "off" ).arg( s.flickerOn ? 1 : 0 ).arg( known ? int( fd->idx.size() ) : 0 )
			.arg( known ? fd->moving : 0 ).arg( known ? fd->frames : 0 ).arg( s.flickerRed ? 1 : 0 ).arg( double( s.flickerStep ) );
	}
	o += QStringLiteral( " giamb=%1 gifill=%2" ).arg( QStringList{ "keep", "replace", "off", "max", "asgi" }.value( s.giAmb ) )
		.arg( s.giFill ? 1 : 0 );
	if ( s.red )
		o += QStringLiteral( " red=%1" ).arg( s.red );
	if ( s.pass > 0 )
		o += QStringLiteral( " pass=%1(asked=%2 %3, sky=%4, pvred=%5)" ).arg( scene ? wwCellPassFor( scene->nifModel ) : 0 )
			.arg( s.pass ).arg( wwCellPassNames().value( s.pass ) ).arg( G && !G->sky.empty() ? 1 : 0 ).arg( s.passRed );
	return o + QLatin1Char( ' ' ) + wwCellShadowEcho( scene )
		+ ( wwVolAirOn() ? QStringLiteral( " " ) + wwVolAirSummary() : QString() )
		+ ( wwLookOn( WwLook::LensFlares ) ? QStringLiteral( " " ) + wwCellLensFlareEcho( scene ) : QString() );
}


/* ---- the ambient obscurance (celllights.h, res/shaders/features/ao/cell_ao.frag) ---- */


void wwCellGiWaterDraw( Scene * scene, int block, bool cellProgram )
{
	if ( !scene || !scene->renderer )
		return;
	NifSkopeOpenGLContext::Program * prog = scene->renderer->getCurrentProgram();
	// lane PRTOFF1: without the PRT a program reads no grid (no cellGiOn), the ray-traced volume's cellDdgiOn instead
	if ( !prog || ( prog->uniLocation( "cellGiOn" ) < 0 && prog->uniLocation( "cellDdgiOn" ) < 0 ) )
		return;
	WW_SWITCH_STATIC( bool, red, wwEnv( "WW_CELL_WATER_PRTP_RED" ).trimmed() == "1" );	// red: water reads the GI as before
	const bool water = cellProgram && !red && wwCellWaterIsShape( scene->nifModel, block );
	static const char * const names[WwWaterGiGuard::kN] = { "cellGiOn", "cellGiSky", "cellSpecOcc", "cellGiFineOn", "cellDdgiOn" };
	auto fn = scene->renderer->fn;
	switch ( waterGi().draw( prog, water ) ) {
	case WwWaterGiGuard::Zero: {
		int live[WwWaterGiGuard::kN] = {};
		for ( int k = 0; k < WwWaterGiGuard::kN; k++ ) {
			const int l = prog->uniLocation( names[k] );
			if ( l >= 0 )
				fn->glGetUniformiv( prog->id, l, &live[k] );
		}
		waterGi().save( prog, live );
		for ( int k = 0; k < WwWaterGiGuard::kN; k++ )
			if ( prog->uniLocation( names[k] ) >= 0 )
				prog->uni1i( names[k], 0 );
		break;
	}
	case WwWaterGiGuard::Restore: {
		const int * v = waterGi().restore( prog );
		for ( int k = 0; k < WwWaterGiGuard::kN; k++ )
			if ( prog->uniLocation( names[k] ) >= 0 )
				prog->uni1i( names[k], v[k] );
		break;
	}
	case WwWaterGiGuard::None:
		break;
	}
	if ( water && prog->uniLocation( "cellGiHalfOn" ) >= 0 )
		prog->uni1b( "cellGiHalfOn", false );	// the half-res GI is set per draw (wwCellAoDraw), so no restore
}


// lane MODULES1: the probe-picture test the AO module's draw hook asks (src/features/ao/ao.cpp)
bool wwCellLitProbePicture()
{
	return ( st().measuring ? 6 : st().probe ) != 0;
}
