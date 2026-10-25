/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "celllights.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "gl/cellclustergl.h"
#include "gl/cellssr.h"
#include "gl/cellvolair.h"	// lane VOLAIR1
#include "gl/shadowset.h"	// lane TERRSHADOW1
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/glproperty.h"	// lane GAPFIX1: alpha-tested cube casters read the base map
#include "gl/renderer.h"
#include "gl/cellhdr.h"
#include "gl/cellbloom.h"	// lane BLOOM1b
#include "gl/cellaodecalgl.h"	// lane AODECAL1
#include "gl/cellpasstime.h"	// lane PERF2: sub-stage timers
#include "gl/celldrawstate.h"	// lane PERF3
#include "gl/lookdevstage.h"	// lane SUNCELL1
#include "gl/cellwater.h"	// lane SUNCELL1: water casts no cube shadow
#include "gl/celllampshadow.h"	// lane LAMPSHADOW1
#include "gl/celllooks.h"	// lane GAPLOOK1
#include "gl/celllensflare.h"	// lane LIGHTS3 (LENSFLARE1): its echo
#include "gamemanager.h"	// lane SUNCELL1: the weather imagespace's LUT

#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QSettings>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include "cellunits.h"
#include <cstring>
#include <vector>

namespace
{

constexpr int kTextureUnit = WwCellUnit::Lights;		// TexCache allocates from unit 0 upward; 15 is the CSM map
constexpr int kTexelsPerLight = 8;		// lane SHADOW1 added the 5th: shadow slot, kind, near clip, XLIG bias;
										// lane HEMI1 the 6th to 8th: the box rows (cell_lights.glsl CELL_TPL)
constexpr int kGiUnit = WwCellUnit::Gi;			// lane PRTPGI: the bounce grid (sampler3D)
constexpr int kGiSlotUnit = WwCellUnit::GiSlot;		// lane ROOMCLAMP1: the grid's slot rooms, the fine rooms (sampler3D, R32F); off
constexpr int kRoomUnit = WwCellUnit::Room;		// on a GPU with fewer than 20 units (the blend before the lane)
constexpr int kFarSlotUnit = WwCellUnit::FarSlot;	// lane FARVIEW1: the far table's slots (usamplerBuffer), records (samplerBuffer) and the
constexpr int kFarRecUnit = WwCellUnit::FarRec;		// GI grid's placed share (sampler3D); off on a GPU without FarPlaced (cellunits.h)
constexpr int kFarPlacedUnit = WwCellUnit::FarPlaced;
constexpr int kLutUnit = WwCellUnit::Lut;		// lane IMGS1: the imagespace LUT (sampler3D)
constexpr int kBloomUnit = WwCellUnit::Bloom;		// lane BLOOM1: the imagespace bloom (sampler2D, a quarter of the view)
constexpr int kShadowUnit = WwCellUnit::Shadow;		// lane SHADOW1: the lights' depth cubes (samplerCubeArrayShadow)
constexpr int kShadowSlots = 16;	// shadowed lights at once, the nearest the camera
constexpr int kShadowFace = 512;	// texels a cube face edge (the game's 2048 paraboloid's centre texel at 1024)

/* the frame's exposure: the tonemap PS's clamp( mid / (lum + 0.001), HNAM min, HNAM max ). lane GIBAKE2 (GISHADE1): the
 * Wide exposure range row lifts the max to 16, so a dim GI-lit room opens up (the imagespace caps it at ~3.25). */
float cellExposure( const float * h, float adapted )
{
	const float cap = wwLookOn( WwLook::ExposureWide ) ? std::max( h[4], 16.0f ) : h[4];
	return std::min( std::max( h[8] / ( adapted + 0.001f ), h[5] ), cap );
}
// lane GAPLOOK1 (LIGHTSYS-3): the budget's distance fade, Godot's Light3D shadow fade (40 to 50 m) at 70 units a metre
constexpr float kShadowFadeBegin = 2800.0f, kShadowFadeEnd = 3500.0f;
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
	quint64 liveGen = 0;	// lane TESTDRIVER1: the switch generation last read
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
	QString lampLast = QStringLiteral( "off" );	// lane LAMPSHADOW1: the stored lamp shadows' last frame
	int fogProbe = 0;       // lane FOG2: WW_CELL_FOG_PROBE=6 (alpha, height blend) | 7 (fog colour ^ 1/2.2) | 8 (its d, z), per fragment
	int pass = 0;           // lane PROBEVIEW1: the PRTP band's Pass (WwCellPass); WW_CELL_PASS pins it
	bool passPinned = false;
	int passRed = 0;        // WW_CELL_PV_RED: 1 direct, 2 nonormal (the shader), 4 open (every sky direction open), 8 noclamp, 16 nosee, 32 borrow, 64 lendamb, 128 strand, 256 firstslot, 512 tiepick, 1024 lonesky, 2048 nofilter, 4096 nolastfill
	int giAmb = 1;          // lane GICAL1: WW_CELL_GI_AMB = keep (0) | replace | off | max | asgi (the measure's red); lane GIAMB1: replace by default (his ruling), keep = the gate's red
	bool giFill = true;     // lane GICAL1: WW_CELL_GI_FILL=0 leaves the GI's gaps black (as before the lane)
	// lane LIGHTFLICKER1: the Light flicker row (WW/CellFlicker, pin WW_CELL_FLICKER), its red and gate pins
	bool flickerOn = false, flickerPinned = false, flickerRed = false;
	float flickerStep = 0.0f;	// > 0: WW_CELL_FLICKER_STEP, seconds a frame
	QString flickerDump;
};

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
			s.giPinned = d.giPinned;
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
			s.on = QSettings().value( QStringLiteral( "WW/CellLights" ), false ).toBool();
		}
		const QByteArray giPin = wwEnv( "WW_CELL_GI" );
		if ( !giPin.isEmpty() ) {
			s.giPinned = true;
			s.giOn = giPin.trimmed() != "0";
		} else {
			s.giOn = QSettings().value( QStringLiteral( "WW/CellGi" ), false ).toBool();
		}
		const QByteArray farPin = wwEnv( "WW_CELL_FAR" );	// lane FARVIEW1
		if ( !farPin.isEmpty() ) {
			s.farPinned = true;
			s.farOn = farPin.trimmed() != "0";
		} else {
			s.farOn = QSettings().value( QStringLiteral( "WW/CellFar" ), false ).toBool();
		}
		const QByteArray flPin = qgetenv( "WW_CELL_FLICKER" ).trimmed();	// lane LIGHTFLICKER1
		s.flickerPinned = !flPin.isEmpty();
		s.flickerOn = s.flickerPinned ? flPin != "0" : QSettings().value( QStringLiteral( "WW/CellFlicker" ), false ).toBool();
		s.flickerRed = qEnvironmentVariableIntValue( "WW_CELL_FLICKER_RED" ) != 0;
		s.flickerStep = std::max( 0.0f, qEnvironmentVariable( "WW_CELL_FLICKER_STEP" ).toFloat() );
		s.flickerDump = qEnvironmentVariable( "WW_CELL_FLICKER_DUMP" );
		const QByteArray farRed = wwEnv( "WW_CELL_FAR_RED" ).trimmed();
		s.farRed = farRed == "nosurfel" ? 1 : farRed == "noblend" ? 2 : farRed == "flipside" ? 4 : 0;
		s.probe = wwEnvInt( "WW_CELL_LIT_PROBE" );
		const QByteArray isPin = wwEnv( "WW_CELL_IS" );
		if ( !isPin.isEmpty() ) {
			s.isPinned = true;
			s.isOn = isPin.trimmed() != "0";
		} else {
			s.isOn = QSettings().value( QStringLiteral( "WW/CellImageSpace" ), false ).toBool();
		}
		const QByteArray isRed = wwEnv( "WW_CELL_IS_RED" ).trimmed();
		s.isRed = isRed == "nolut" ? 1 : isRed == "noexp" ? 2 : isRed == "nograde" ? 4 : isRed == "nobloom" ? 8
			: isRed == "nofx" ? 16 : 0;
		const QByteArray shPin = wwEnv( "WW_CELL_SHADOW" ).trimmed();
		s.shadowPinned = !shPin.isEmpty();
		s.shadowOn = s.shadowPinned ? shPin != "0" : QSettings().value( QStringLiteral( "WW/CellShadow" ), true ).toBool();
		s.shadowRed = wwEnv( "WW_CELL_SHADOW_RED" ).trimmed() == "noshadow";
		s.fogProbe = wwEnvInt( "WW_CELL_FOG_PROBE" );
		const QByteArray giAmb = wwEnv( "WW_CELL_GI_AMB" ).trimmed();	// lane GICAL1
		s.giAmb = giAmb == "keep" ? 0 : giAmb == "off" ? 2 : giAmb == "max" ? 3 : giAmb == "asgi" ? 4 : 1;	// lane GIAMB1: unset = replace
		s.giFill = wwEnv( "WW_CELL_GI_FILL" ).trimmed() != "0";
		const QByteArray red = wwEnv( "WW_CELL_LIT_RED" ).trimmed();
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
		if ( wwEnv( "WW_CELL_SPEC_RED" ).trimmed() == "nonspec" )
			s.red |= 65536;
		else if ( wwEnv( "WW_CELL_SPEC_RED" ).trimmed() == "tint" )
			s.red |= 262144;	// lane GAPFIX1: opaque (deferred) draws tint the lights' specular by the material colour again
		// lane GISHADE1: outdoors the grid's sky takes k x the flat ambient off in sqrt space again (the old law)
		if ( wwEnv( "WW_CELL_GISHADE_RED" ).trimmed() == "sqrt" )
			s.red |= 131072;
		// lane PROBEVIEW1: the Pass, by number or by its entry's name
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
			s.passRed |= t == "direct" ? 1 : t == "nonormal" ? 2 : t == "open" ? 4 : t == "noclamp" ? 8 : t == "nosee" ? 16 : t == "borrow" ? 32 : t == "lendamb" ? 64 : t == "strand" ? 128 : t == "firstslot" ? 256 : t == "tiepick" ? 512 : t == "lonesky" ? 1024 : t == "nofilter" ? 2048 : t == "nolastfill" ? 4096 : 0;	// lane GIDARK1: firstslot, tiepick, lonesky; lane WALLSTACK1: nofilter, nolastfill   // lane ROOMCLAMP1: noclamp; lane GISHADE1: nosee; lane GIBLACK1: borrow; lane GIBLACK2: lendamb, strand
		}
	}
	return s;
}

//! one texture buffer per GL context (the renderer), re-uploaded when the published version moves
struct Gpu
{
	GLuint buf = 0, tex = 0;
	bool texAttached = false;	// lane PERF1: the buffer texture keeps its buffer across glBufferData; attached once
	quint64 bufLiveGen = 0;	// lane TESTDRIVER1: a live set (light copies, reds) rebuilds the buffer
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
	bool giSpecSky = false;	// lane SPECOCC1: the GI texture carries the sky grid's slot 0 after its own slabs
	GLuint airTex = 0;		// SKY2 FOGSLICE1: the air volume (WwCellGi::air), uploaded with the GI grid
	const void * airDoc = nullptr;
	int airVersion = 0;
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
	int shKick = 0;	// lane DOORLIVE1: a moving caster (a live door) re-renders every held slot
	int shFace = 0;	// lane LAMPSHADOW1: the face edge shTex was made at (the tier's)
	float shFade[kShadowSlots];	// lane GAPLOOK1: each slot's shadow strength under the budget (1 without it)
	int shBudget = 0;
	float shCut = 0.0f;
	// lane LAMPSHADOW1: the store (a 2D array in blocks of 3 face layers), its maps by light, the movers
	struct LsEntry
	{
		std::vector<int> blocks;	// omni 2 (faces 0-2, 3-5); wall 1 (the front face, then the side halves two a layer)
		int back = -1;			// the face into the wall (-1 omni)
		float dist = 0.0f;
		std::uint64_t used = 0;
	};
	struct LsSig
	{
		float v[20];
		Vector3 c;
		float rad = 0.0f;
	};
	GLuint lsTex = 0, lsReadFbo = 0;
	int lsFace = 0, lsBlocks = 0;
	std::vector<char> lsBlockUsed;
	QHash<int, LsEntry> lsStore;
	QHash<int, WwLampWall> lsWall;
	QHash<int, LsSig> lsHist;
	QHash<int, char> lsMovers;
	std::vector<int> lsSlotMovers[kShadowSlots];
	const void * lsDoc = nullptr;
	int lsVersion = 0;
	std::uint64_t lsFrame = 0;
	std::vector<float> spheres;	// lane VOLAIR1: every buffer light's world centre + radius (copies included), in order
	std::vector<float> texels;		// the light buffer as uploaded, for the cluster lists
	std::uint64_t texelStamp = 0;
	// lane LIGHTFLICKER1: the buffer before the flicker (base) and as patched (live); the flicker stamp last patched
	std::vector<float> flickBase, flickLive;
	std::uint64_t flickStamp = 0;
	bool flickOn = false;
	Gpu()
	{
		std::fill( shSlot, shSlot + kShadowSlots, -1 );
		std::fill( shFade, shFade + kShadowSlots, 1.0f );
	}
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
		// WATERHDR1: + the grade (cine, tint) and the LUT blend, the whole chain for a gate to rebuild
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

/* ---- lane LIGHTFLICKER1: the light flicker (celllights.h) ----
 * Matches the game's flicker / pulse behavior, measured. s = frame seconds x Period; 512-entry sin / cos tables
 * indexed int(x 512 / 2 pi) & 511. Phases p (per ref; start 1, 0, 0) wrap at 2 pi.
 *  flicker (0x8): p += random(1.1, 13.1), (1.2, 13.2), (1.3, 19.3) x s; q = p + (1.7, 0.5, 0.3);
 *    o = Movement x sin q0 x sin q1 / 2 (0 unless o + Movement > 0); local offset o (sin q0, sin q1, sin q2);
 *    v = clamp((sin 1.3 q1 + 1) / 2 x (sin 1.1 q0 + 1) / 2 / 3 + 0.2 sin 3 q2, -1, 1)
 *  pulse (0x80 | 0x100): p0 += s; c, n = cos, sin p0; v = c; local offset Movement (c, n, n c)
 *  dimmer = 1 - A + A v with A = Intensity / 2 (x the fade, which the colour already holds). */
namespace
{
struct FlickPhase
{
	float p[3] = { 1.0f, 0.0f, 0.0f };
	quint32 rng = 0;
};
struct FlickDoc
{
	int version = -1;
	std::vector<int> idx;			// the flickering lights
	std::vector<float> val;			// 4 a light (dimmer, world offset), all lights
	QHash<quint32, FlickPhase> phase;	// by ref, so a streamed republish keeps each light's phase
	std::uint64_t stamp = 1;
	int moving = 0;
	qint64 frames = 0;
};
QHash<const void *, FlickDoc> & flickDocs()
{
	static QHash<const void *, FlickDoc> d;
	return d;
}
float flickTab( float x, bool cosine )
{
	static const std::vector<float> tab = [] {
		std::vector<float> t( 1024 );
		for ( int i = 0; i < 512; i++ ) {
			t[size_t( i )] = float( std::sin( double( i ) * 6.283185307179586 / 512.0 ) );
			t[size_t( 512 + i )] = float( std::cos( double( i ) * 6.283185307179586 / 512.0 ) );
		}
		return t;
	}();
	return tab[size_t( ( int( x * ( 512.0f / 6.2831855f ) ) & 511 ) + ( cosine ? 512 : 0 ) )];
}
float flickRand( quint32 & r, float lo, float hi )	// a random in [lo, hi); xorshift so a gate's frames repeat
{
	r ^= r << 13;
	r ^= r >> 17;
	r ^= r << 5;
	return lo + ( hi - lo ) * float( r >> 8 ) * ( 1.0f / 16777216.0f );
}
bool flickLive( Scene * scene )
{
	const ClState & s = st();
	return s.flickerOn && !s.flickerRed && wwCellLightsWanted( scene );
}
}	// namespace

bool wwCellFlickerOn()
{
	return st().flickerOn;
}

void wwCellFlickerSetOn( bool on )
{
	ClState & s = st();
	if ( s.flickerPinned )
		return;
	s.flickerOn = on;
	QSettings().setValue( QStringLiteral( "WW/CellFlicker" ), on );
}

void wwCellFlickerFrame( Scene * scene )
{
	using Clock = std::chrono::steady_clock;
	static Clock::time_point last = Clock::now();
	const Clock::time_point now = Clock::now();
	float dt = std::clamp( std::chrono::duration<float>( now - last ).count(), 0.0f, 0.25f );
	last = now;
	if ( !flickLive( scene ) )
		return;
	const ClState & s = st();
	if ( s.flickerStep > 0.0f )
		dt = s.flickerStep;
	const WwCellLighting * L = wwCellLightsFor( scene->nifModel );
	FlickDoc & d = flickDocs()[scene->nifModel];
	const int ver = s.version.value( scene->nifModel );
	if ( d.version != ver ) {
		d.version = ver;
		d.idx.clear();
		d.moving = 0;
		for ( qsizetype i = 0; i < L->lights.size(); i++ )
			if ( L->lights.at( i ).flicker ) {
				d.idx.push_back( int( i ) );
				d.moving += L->lights.at( i ).flickerMovement != 0.0f && !L->lights.at( i ).shadow ? 1 : 0;
			}
		d.val.assign( size_t( L->lights.size() ) * 4, 0.0f );
		for ( size_t i = 0; i < d.val.size(); i += 4 )
			d.val[i] = 1.0f;
	}
	if ( d.idx.empty() )
		return;
	constexpr float kTwoPi = 6.2831855f;
	QFile dump( s.flickerDump );
	const bool dumping = !s.flickerDump.isEmpty() && dump.open( QIODevice::Append | QIODevice::Text );
	for ( int i : d.idx ) {
		const WwCellLight & l = L->lights.at( i );
		FlickPhase & ph = d.phase[l.ref];
		if ( !ph.rng )
			ph.rng = 0x9E3779B9u ^ ( l.ref * 2654435761u ) ^ quint32( i + 1 );
		const float sp = dt * l.flickerPeriod;
		const float m = l.flickerMovement, A = l.flickerIntensity * 0.5f;
		float v, local[3];
		if ( l.flicker == 1 ) {
			ph.p[0] += flickRand( ph.rng, 1.1f, 13.1f ) * sp;
			ph.p[1] += flickRand( ph.rng, 1.2f, 13.2f ) * sp;
			ph.p[2] += flickRand( ph.rng, 1.3f, 19.3f ) * sp;
			for ( float & p : ph.p )
				if ( p > kTwoPi )
					p -= kTwoPi;
			const float q0 = ph.p[0] + 1.7f, q1 = ph.p[1] + 0.5f, q2 = ph.p[2] + 0.3f;
			const float s0 = flickTab( q0, false ), s1 = flickTab( q1, false );
			float o = m * s0 * s1 * 0.5f;
			if ( !( o + m > 0.0f ) )
				o = 0.0f;
			local[0] = o * s0;
			local[1] = o * s1;
			local[2] = o * flickTab( q2, false );
			v = ( flickTab( q1 * 1.3f, false ) + 1.0f ) * 0.5f * ( flickTab( q0 * 1.1f, false ) + 1.0f ) * 0.5f * ( 1.0f / 3.0f )
				+ flickTab( q2 * 3.0f, false ) * 0.2f;
			v = std::clamp( v, -1.0f, 1.0f );
		} else {
			ph.p[0] += sp;
			if ( ph.p[0] > kTwoPi )
				ph.p[0] -= kTwoPi;
			const float c = flickTab( ph.p[0], true ), n = flickTab( ph.p[0], false );
			v = c;
			local[0] = m * c;
			local[1] = m * n;
			local[2] = m * n * c;
		}
		float * out = d.val.data() + size_t( i ) * 4;
		out[0] = 1.0f - A + A * v;
		// a shadow-casting light keeps its place: its depth cube is drawn from there, not redrawn each frame
		for ( int k = 0; k < 3; k++ )
			out[1 + k] = l.shadow ? 0.0f
				: l.flickerAxes[0][k] * local[0] + l.flickerAxes[1][k] * local[1] + l.flickerAxes[2][k] * local[2];
		if ( dumping )
			dump.write( QStringLiteral( "%1 %2 %3 %4 %5 %6 %7 %8 %9\n" ).arg( d.frames ).arg( double( dt ), 0, 'g', 6 ).arg( i )
				.arg( l.ref, 8, 16, QLatin1Char( '0' ) ).arg( l.flicker ).arg( double( out[0] ), 0, 'f', 6 )
				.arg( double( out[1] ), 0, 'f', 4 ).arg( double( out[2] ), 0, 'f', 4 ).arg( double( out[3] ), 0, 'f', 4 ).toLatin1() );
	}
	d.frames++;
	d.stamp++;
}

bool wwCellFlickerWantsFrames( Scene * scene )
{
	if ( !flickLive( scene ) )
		return false;
	const auto it = flickDocs().constFind( scene->nifModel );
	// before the first frame has listed them: ask once, the frame lists them
	return it == flickDocs().constEnd() || it->version != st().version.value( scene->nifModel ) || !it->idx.empty();
}

bool wwCellFlickerOf( const void * nif, qsizetype i, float * dimmer, float offset[3] )
{
	*dimmer = 1.0f;
	offset[0] = offset[1] = offset[2] = 0.0f;
	const ClState & s = st();
	if ( !s.flickerOn || s.flickerRed )
		return false;
	const auto it = flickDocs().constFind( nif );
	if ( it == flickDocs().constEnd() || it->version != s.version.value( nif ) || i < 0 || size_t( i ) * 4 + 4 > it->val.size() )
		return false;
	const float * v = it->val.data() + size_t( i ) * 4;
	*dimmer = v[0];
	for ( int k = 0; k < 3; k++ )
		offset[k] = v[1 + k];
	return v[0] != 1.0f || v[1] != 0.0f || v[2] != 0.0f || v[3] != 0.0f;
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
	wwLookdevFogUniforms( scene );	// lane VOLAIR1: the air between the bulb and the camera (cell_fardots.frag)
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
		QStringLiteral( "Surfel color" ), QStringLiteral( "Surfel light" ),
		QStringLiteral( "Brick light" ),	// lane BRICK1: each surfel tile shows its 4 m brick's light
		QStringLiteral( "Probe coverage" ) };	// lane WALLSTACK1: magenta where no probe reaches (the deck's s-placement view)
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
 * upsampled by depth and normal), 1 full (the screen GI at full resolution), 2 inline (no screen pass: every surface
 * reads the volumes itself), 3 red (the screen pass runs, the surfaces read inline: the inline picture at its cost plus
 * the pass) */
int wwCellGiRate()
{
	WW_SWITCH_STATIC( int, mode, [] {
		const QByteArray v = wwEnv( "WW_CELL_GI_RATE" ).trimmed().toLower();
		return v == "full" ? 1 : v == "inline" || v == "old" || v == "0" ? 2 : v == "red" ? 3 : 0;
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

void wwCellLightsUniforms( Scene * scene )
{
	PassTime::DrawCpuScope cpuT( PassTime::CpuCellUni );	// lane PERF2
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
	const bool flick = L && s.flickerOn && !s.flickerRed;	// lane LIGHTFLICKER1
	if ( L && ( g.doc != scene->nifModel || g.version != s.version.value( scene->nifModel ) || g.bufShStamp != g.shStamp
	            || g.bufLiveGen != wwSwitchGen() || g.flickOn != flick ) ) {
		g.bufLiveGen = wwSwitchGen();
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
		g.bufShStamp = g.shStamp;
		g.spheres.clear();
		for ( size_t o = 0; o + 4 <= t.size(); o += kTexelsPerLight * 4 )
			g.spheres.insert( g.spheres.end(), t.begin() + std::ptrdiff_t( o ), t.begin() + std::ptrdiff_t( o + 4 ) );
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
		g.flickOn = flick;
		g.flickStamp = 0;
		g.flickBase.clear();
		g.flickLive.clear();
		if ( flick && !L->lights.isEmpty() ) {
			g.flickBase = t;
			g.flickLive = t;
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
	/* lane LIGHTFLICKER1: this frame's flicker (wwCellFlickerFrame) patched into the buffer once, as the range of the
	 * flickering lights: the surfaces, the effects and the air read the same texels */
	if ( L && g.flickOn && g.doc == scene->nifModel && !g.flickBase.empty() ) {
		const auto fd = flickDocs().constFind( scene->nifModel );
		if ( fd != flickDocs().constEnd() && fd->version == g.version && fd->stamp != g.flickStamp && !fd->idx.empty() ) {
			g.flickStamp = fd->stamp;
			const size_t per = kTexelsPerLight * 4, n = size_t( L->lights.size() );
			const size_t total = g.flickLive.size() / per;
			size_t lo = total, hi = 0;
			for ( size_t c = 0; c * n < total; c++ )
				for ( int i : fd->idx ) {
					const size_t j = c * n + size_t( i ), o = j * per;
					if ( j >= total || size_t( i ) * 4 + 4 > fd->val.size() )
						continue;
					const float * v = fd->val.data() + size_t( i ) * 4;
					for ( int k = 0; k < 3; k++ ) {
						g.flickLive[o + size_t( k )] = g.flickBase[o + size_t( k )] + v[1 + k];
						g.flickLive[o + 4 + size_t( k )] = g.flickBase[o + 4 + size_t( k )] * v[0];
						if ( j * 4 + 3 < g.spheres.size() )
							g.spheres[j * 4 + size_t( k )] = g.flickLive[o + size_t( k )];
					}
					lo = std::min( lo, j );
					hi = std::max( hi, j );
				}
			if ( lo <= hi && g.buf ) {
				fn->glBindBuffer( GL_TEXTURE_BUFFER, g.buf );
				fn->glBufferSubData( GL_TEXTURE_BUFFER, GLintptr( lo * per * sizeof( float ) ), GLsizeiptr( ( hi - lo + 1 ) * per * sizeof( float ) ),
					g.flickLive.data() + lo * per );
				fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
			}
		}
	}
	/* The sampler is bound whether or not this draw lights: a samplerBuffer left on unit 0
	 * beside BaseMap's sampler2D is a draw-time INVALID_OPERATION. */
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kTextureUnit ) );
	if ( g.tex && g.buf ) {
		fn->glBindTexture( GL_TEXTURE_BUFFER, g.tex );
		if ( !g.texAttached ) {
			fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, g.buf );
			g.texAttached = true;
		}
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
	// lane SPECOCC1: with the sky specular occlusion on, the GI texture carries the sky grid's slot 0 after its own slabs
	const bool specSky = G && wwCellSpecOccMode() > 0 && G->skyLit && G->sky.size() == G->rgba.size();
	if ( G && pass != 2 && !G->rgba.empty()
		&& ( g.giDoc != scene->nifModel || g.giVersion != s.giVersion.value( scene->nifModel ) || g.giSpecSky != specSky ) ) {
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
	}
	const bool skyDraw = G && pass == 2 && g.skyTex && g.skyDoc == scene->nifModel;
	const bool giDraw = skyDraw || ( G && pass != 2 && g.giTex && g.giDoc == scene->nifModel );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kGiUnit ) );
	fn->glBindTexture( GL_TEXTURE_3D, skyDraw ? g.skyTex : giDraw ? g.giTex : 0 );
	{	// SKY2 FOGSLICE1: the air volume (the deck's D2 s55), read by the air pass only (cell_airmed.glsl volCubeAll)
		const bool airUp = giDraw && !skyDraw && G->skyLit && !G->air.empty();
		if ( airUp && ( g.airDoc != scene->nifModel || g.airVersion != s.giVersion.value( scene->nifModel ) ) ) {
			if ( !g.airTex )
				fn->glGenTextures( 1, &g.airTex );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiAir ) );
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
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiAir ) );
		fn->glBindTexture( GL_TEXTURE_3D, airDraw ? g.airTex : 0 );
		prog->uni1i( "cellGiAir", WwCellUnit::GiAir );
		prog->uni1b( "cellGiAirOn", airDraw );
		if ( airDraw ) {
			prog->uni3f( "cellGiAirOrigin", G->airOrigin[0], G->airOrigin[1], G->airOrigin[2] );
			prog->uni1f( "cellGiAirVoxel", G->airVoxel );
			prog->uni3f( "cellGiAirDims", float( G->airDims[0] ), float( G->airDims[1] ), float( G->airDims[2] ) );
		}
	}
	// lane ROOMCLAMP1: the rooms (src/proberooms.h): the grid's slot rooms and the fine rooms, nearest, R32F
	if ( g.units < 0 ) {
		GLint units = 0;
		fn->glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &units );
		g.units = units;
	}
	{	// lane PERF1: the screen GI's samplers (wwCellAoDraw binds them per draw; never left on unit 0); the volume read's
		// loop bound (a uniform: the loop stays one body in every program)
		const bool fits = g.units > WwCellUnit::GiHalfKey;
		const int u = fits ? WwCellUnit::GiHalf : g.units > WwCellUnit::Ao ? WwCellUnit::Ao : 9;
		prog->uni1i( "cellGiHalf", u );
		prog->uni1i( "cellGiHalfKey", fits ? WwCellUnit::GiHalfKey : u );
		prog->uni1i( "cellGiLoop8", 8 );
		prog->uni1b( "cellGiHalfOn", false );	// wwCellAoDraw turns it on, per draw, after this
	}
	const bool roomsUp = giDraw && !G->slotRooms.empty() && !G->rooms.empty() && g.units > kRoomUnit;
	if ( roomsUp && ( g.roomDoc != scene->nifModel || g.roomVersion != s.giVersion.value( scene->nifModel ) ) ) {
		auto up3 = [&]( GLuint & tex, int unit, const int * d, const std::vector<float> & v, bool rgba ) {
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
	prog->uni1f( "cellSkyVis", wwCellSkyVis() );	// lane SKYSTR1: the Sky visibility row, for the AO decals
	// lane SPECOCC1: the sky grid's slabs start after the GI's own (6, or 12 with the rooms' second slot)
	const bool specOccDraw = giDraw && !skyDraw && G->skyLit && g.giSpecSky;
	prog->uni1i( "cellSpecOcc", specOccDraw ? wwCellSpecOccMode() : 0 );
	if ( specOccDraw )
		prog->uni1f( "cellSpecSkyBase", G->slotRooms.empty() ? 6.0f : 12.0f );
	prog->uni1i( "cellGiAmb", s.giAmb );	// lane GICAL1
	prog->uni1b( "cellGiFill", s.giFill && !skyDraw );	// the gap fill is the GI grid's, never the sky share's
	prog->uni1i( "cellPass", pass );	// lane PROBEVIEW1
	prog->uni1i( "cellPassRed", s.passRed & ~4 );	// lane WALLSTACK1: + 2048 nofilter, 4096 nolastfill (was & 251, which dropped them)	// lane ROOMCLAMP1: + 8 noclamp; lane GISHADE1: + 16 nosee; lane GIBLACK1: + 32 borrow; lane GIBLACK2: + 64 lendamb, 128 strand
	if ( giDraw ) {
		prog->uni3f( "cellGiOrigin", G->origin[0], G->origin[1], G->origin[2] );
		prog->uni1f( "cellGiVoxel", G->voxel );
		prog->uni3f( "cellGiDims", float( G->dims[0] ), float( G->dims[1] ), float( G->dims[2] ) );
	}
	{	// lane EMISSIVEGRID1: the fine boxes (their atlas rides the grid's texture, after its layers)
		const bool fineDraw = giDraw && !skyDraw && !G->fine.empty();
		prog->uni1b( "cellGiFineOn", fineDraw );
		if ( fineDraw ) {
			WW_SWITCH_STATIC( int, fineRed, []() {	// lane LIVESET1: follows a live set
				const QByteArray r = qgetenv( "WW_CELL_EMISSIVEGRID_RED" ).trimmed();
				return r == "noseen" ? 1 : r == "nosub" ? 2 : 0;
			}() );
			const int giLayers = G->dims[2] * ( ( G->slotRooms.empty() ? 6 : 12 ) + ( g.giSpecSky ? 6 : 0 ) );	// + SPECOCC1's sky slot
			const int n = std::min( int( G->fineBox.size() / 8 ), 8 );
			prog->uni1i( "cellGiFineN", n );
			prog->uni1i( "cellGiFineRed", fineRed );
			for ( int k = 0; k < n; k++ ) {
				const float * b = &G->fineBox[size_t( k ) * 8];
				prog->uni4f_l( prog->uniLocation( "cellGiFineBox[%d]", k ), FloatVector4( b[0], b[1], b[2], b[3] ) );
				prog->uni4f_l( prog->uniLocation( "cellGiFineDim[%d]", k ), FloatVector4( b[4], b[5], b[6], b[7] + float( giLayers ) ) );
			}
		}
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
	// lane BLOOM1b: the FO4CS bloom stands in for it this frame (FO4CS suppresses the engine bloom the same way)
	const bool bloomDraw = B && B->w > 0 && g.bloomTex && g.bloomDoc == scene->nifModel && !( s.isRed & 8 ) && !wwCellBloomReplacing();
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
	prog->uni1f( "cellShadowTexel", 2.0f / float( g.shFace > 0 ? g.shFace : kShadowFace ) );	// lane LAMPSHADOW1: the tier's face
	// lane GAPLOOK1 (LIGHTSYS-3): the budget's per-slot strengths; off, the shader keeps its old return
	const bool fadeOn = shadowDraw && g.shBudget > 0;
	prog->uni1b( "cellShadowFadeOn", fadeOn );
	if ( fadeOn && prog->uniLocation( "cellShadowFade[0]" ) >= 0 )
		r->fn->glUniform1fv( prog->uniLocation( "cellShadowFade[0]" ), kShadowSlots, g.shFade );
	prog->uni1i( "cellIsLut", kLutUnit );
	prog->uni1b( "cellIsOn", isDraw );
	prog->uni1b( "cellIsLinear", isDraw && wwCellHdrActive() );	// lane HDR1: write linear light, tone-mapped once
	prog->uni1b( "cellIsLutOn", lutDraw );
	if ( isDraw ) {
		const float * h = LI->isHdr;
		// the tonemap PS: max( mid / (lum + 0.001), cb.y ) then min( .., cb.x ); cb (x, y) = HNAM (max, min)
		const float e = cellExposure( h, adapted );
		prog->uni1f( "cellIsExposure", e );
		prog->uni1f( "cellIsE", h[1] );
		prog->uni1f( "cellIsAdapted", adapted );
		prog->uni3f( "cellIsCine", LI->isCine[0], LI->isCine[1], LI->isCine[2] );
		prog->uni4f_l( prog->uniLocation( "cellIsTint" ), FloatVector4( LI->isTint[0], LI->isTint[1], LI->isTint[2], LI->isTint[3] ) );
		prog->uni1i( "cellIsRed", s.isRed );
	}
	prog->uni1i( "cellLights", kTextureUnit );
	// the cluster lists: their sampler bound on every draw, the lists only on a lit one
	wwCellClusterUniforms( scene, L ? &g.texels : nullptr, L ? g.count : 0, g.texelStamp );
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
	 * world, linear, the numbers the viewport light already carries. Only the cell programs take it (the volumetric
	 * fog keeps the old exterior law; a lit effect takes it x Sunlight Scale below); Lookdev off (no weather) = the old path, unchanged. */
	bool hasDir = L->hasDirectional;
	float dirTo[3] = { L->dirTo[0], L->dirTo[1], L->dirTo[2] }, dirColor[3] = { L->dirColor[0], L->dirColor[1], L->dirColor[2] };
	/* lane BOUNCE1: the game's lit surfaces take the sun x the imagespace Sunlight Scale (wwLookdevSunlightScale;
	 * CommonwealthClear noon 4.5), so the sun against the weather's ambient -- and the bounce the probes carry -- is
	 * the game's ratio; the cell imagespace's exposure (adapted luminance) takes the brighter frame. 1 indoors. */
	const float sunScale = ( !L->interior && wwLookdevActive() ) ? wwLookdevSunlightScale() : 1.0f;
	prog->uni1f( "cellSunScale", sunScale );
	/* lane VOLAIR1 (LAMPAIR1): the air takes the surfaces' exterior sun and sky (the weather's light x Sunlight Scale,
	 * its DALC) so it is lit by the light the records give, not the fog-sun glare colour; red WW_CELL_AIR_RED=oldsun */
	WW_SWITCH_STATIC( bool, airOldSun, wwEnvStr( "WW_CELL_AIR_RED" ).split( ',' ).contains( QStringLiteral( "oldsun" ) ) );
	const bool extProg = wwIsCellProgramName( prog->name ) || wwIsCellDecalProgramName( prog->name ) || ( prog->name == "cell_volair.prog" && !airOldSun );
	if ( !L->interior && wwLookdevActive() && extProg ) {
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
		const QString line = !sunIn.changed( sunKey, sizeof( sunKey ) ) ? sunLast : hasDir
			? QStringLiteral( "cell sun: exterior directional=weather to=%1,%2,%3 color=%4,%5,%6 (linear) shadows=%7 sunlight scale=%8" )
				.arg( double( dirTo[0] ), 0, 'f', 5 ).arg( double( dirTo[1] ), 0, 'f', 5 ).arg( double( dirTo[2] ), 0, 'f', 5 )
				.arg( double( dirColor[0] ), 0, 'f', 5 ).arg( double( dirColor[1] ), 0, 'f', 5 ).arg( double( dirColor[2] ), 0, 'f', 5 )
				.arg( prog->uniLocation( "csmMap" ) >= 0 ? 1 : 0 ).arg( double( sunScale ), 0, 'f', 3 )
			: QStringLiteral( "cell sun: exterior refused (no weather loaded, or a black sun)" );
		if ( line != sunLast ) {
			fprintf( stderr, "%s\n", qPrintable( line ) );
			sunLast = line;
		}
	}
	/* lane SUNCELL1 final: a lit effect or lit particle outdoors (fo4_effectcell / particles_cell) takes the game's
	 * base term: the game's lit-effect setup puts pow(directional colour, 2.2) x the light's fade x
	 * the imagespace Sunlight Scale (the imagespace HDR data's 7th float, HNAM's 7th float) in
	 * cb2[12]; the scene's directional outdoors is the sun. No ambient: the lit asm reads cb2 rows 0..14 only
	 * (lights 0..11, base 12, alpha 13..14). Red WW_CELL_FXLIT_EXT_RED=indoors leaves exteriors self-lit.
	 * lane ALLON2 (2026-10-12, the Goodneighbor steam drew near-white): the game puts that same Sunlight Scale on the
	 * SURFACES' sun too (the lit-surface and deferred-light setups both multiply the imagespace Sunlight Scale
	 * in; checked in Todd's treat), so effect and wall keep a 1:1 sun. This
	 * viewer's surfaces take the sun unscaled, so a lit effect takes the surfaces' scale (1): with x4.5 it drew 4.5x
	 * brighter than the wall behind it. Red WW_CELL_FXLIT_SCALE_RED=game puts the Sunlight Scale back (in and out).
	 * lane EFFECTPARITY1: the colour is not the sunlight. The game's per-frame sun update copies the sky's colours into the sun light:
	 * the diffuse (what the surfaces read) from the weather's Sunlight row, and a second colour (the light's
	 * effect colour) from its Effect Lighting row (NAM0 row 9); the effect
	 * shader's lit branch reads that effect colour. CommonwealthClear at noon: Sunlight 225 grey, Effect Lighting 150 grey
	 * (0.41x linear); at night 53,70,87 vs 33,51,67. Red WW_CELL_FXLIT_COLOR_RED=sun takes the sunlight again. */
	{
		WW_SWITCH_STATIC( bool, fxExtRed, wwEnv( "WW_CELL_FXLIT_EXT_RED" ).trimmed() == "indoors" );
		WW_SWITCH_STATIC( bool, fxScaleRed, wwEnv( "WW_CELL_FXLIT_SCALE_RED" ).trimmed() == "game" );
		WW_SWITCH_STATIC( bool, fxColorRed, wwEnv( "WW_CELL_FXLIT_COLOR_RED" ).trimmed() == "sun" );
		const bool fxProg = prog->name == std::string_view( "fo4_effectcell.prog" )
			|| prog->name == std::string_view( "fo4_effectcellfx.prog" )	// lane FXRES1
			|| prog->name == std::string_view( "particles_cell.prog" );
		// lane GIDARK1: the decal programs keep the surfaces' sun and take the lit-effect base as cellFxBase (fxRoomLit)
		const bool roomProg = wwIsCellDecalProgramName( prog->name );
		float dalc[6][3];
		if ( !L->interior && roomProg ) {
			float base[3] = { 0.0f, 0.0f, 0.0f };
			if ( !fxExtRed && wwLookdevActive() && wwLookdevDalc( dalc ) ) {
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
			prog->uni3f( "cellFxBase", base[0], base[1], base[2] );
		}
		if ( !L->interior && fxProg && !fxExtRed && wwLookdevActive() && wwLookdevDalc( dalc ) ) {
			float dif[4] = {}, amb[4] = {};
			wwLookdevLight( dirTo, dif, amb );
			float fxCol[3] = { dif[0], dif[1], dif[2] };
			if ( !fxColorRed )
				wwLookdevEffectLight( fxCol );	// lane EFFECTPARITY1: the Effect Lighting row
			WwLookdevIs is;
			const bool haveIs = wwLookdevImageSpace( is );
			const float isScale = haveIs ? is.hdr[6] : 1.0f;
			const float fxScale = fxScaleRed ? isScale : sunScale;	// lane ALLON2: the surfaces' scale (lane BOUNCE1: the game's)
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
		} else if ( !L->interior && fxProg ) {
			static bool fxSunSaid = false;
			if ( !fxSunSaid ) {
				fprintf( stderr, "cell fx sun: exterior lit effects take no sun (%s)\n",
					fxExtRed ? "RED WW_CELL_FXLIT_EXT_RED=indoors" : "no weather loaded" );
				fxSunSaid = true;
			}
		}
		/* lane SUNCELL1 final: indoors too the base takes the imagespace Sunlight Scale -- the lit-effect setup multiplies
		 * the imagespace Sunlight Scale in unconditionally; indoors that is the cell's XCIM imagespace HNAM[6].
		 * A cell with no imagespace read keeps the base as it was. Red WW_CELL_FXLIT_INT_RED=noscale leaves it out. */
		WW_SWITCH_STATIC( bool, fxIntRed, wwEnv( "WW_CELL_FXLIT_INT_RED" ).trimmed() == "noscale" );
		if ( L->interior && fxProg ) {
			const bool scaled = fxScaleRed && !fxIntRed && L->hasImageSpace;	// lane ALLON2: the surfaces' scale (1)
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
	/* lane SUNCELL1: an exterior's ambient is the weather's directional ambient (the 6 DALC colours blended over
	 * the hour's keys, Lookdev's wwLookdevDalc), uploaded in the interior's form (byte/255, (a - b) / 2 per axis,
	 * the mean) so cellAmbient is one law indoors and out. The axis is the light's TRAVEL direction (Lookdev's
	 * convention: the weathers store the bright sky blue in Z-, so Z- lights an up-facing normal). Only with the
	 * weather's sun (the exterior branch of cellLit). Red WW_CELL_EXTAMB_RED=flat keeps the flat NAM0 Ambient;
	 * =flip swaps the axis convention. */
	bool extDalc = false;
	float ambLum = -1.0f;	// lane SHADOWPAR1: the DALC's mean luminance (linear) for the air tail's ambient share
	if ( !L->interior && hasDir && wwLookdevActive() && extProg ) {
		WW_SWITCH_STATIC( QByteArray, extRed, wwEnv( "WW_CELL_EXTAMB_RED" ).trimmed() );
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
				ambLum = std::max( ambLum, 0.0f ) + mean * mean * ( c == 0 ? 0.2126f : c == 1 ? 0.7152f : 0.0722f );
				const float sgn = flip ? 0.5f : -0.5f;	// travel: n.z = +1 takes Z- (index 5)
				prog->uni4f_l( prog->uniLocation( "cellDalc[%d]", c ), FloatVector4(
					( g[0][c] - g[1][c] ) * sgn, ( g[2][c] - g[3][c] ) * sgn, ( g[4][c] - g[5][c] ) * sgn, mean ) );
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
	if ( ambLum >= 0.0f ) {
		// the share of the air's light that no sun shadow dims: the ambient over it and the sun's isotropic quarter
		const float sunLum = 0.2126f * dirColor[0] + 0.7152f * dirColor[1] + 0.0722f * dirColor[2];
		wwShadowSetAirAmbientShare( ambLum / std::max( ambLum + 0.25f * sunLum, 1e-6f ) );
	}
	prog->uni1b( "cellInterior", L->interior );
	prog->uni3f( "cellCenter", L->center[0], L->center[1], L->center[2] );
	prog->uni1i( "cellProbe", s.measuring ? 6 : s.probe );
	// lane DECALLIT1: a harness reads a dark linear frame through 8 bits; the imagespace's own measure stays x 1
	WW_SWITCH_STATIC( float, probeGain, wwEnvIsSet( "WW_CELL_LIT_PROBE_GAIN" )
		? std::max( wwEnv( "WW_CELL_LIT_PROBE_GAIN" ).toFloat(), 0.0f ) : 1.0f );
	prog->uni1f( "cellProbeGain", s.measuring ? 1.0f : probeGain );
	prog->uni1i( "cellRed", s.red );
	// WW_CELL_CAM_DUMP=<file>: the camera in world units, rewritten when it moves (the fog and diffuse gates measure from it)
	WW_SWITCH_STATIC( QString, camDump, QString::fromLocal8Bit( wwEnv( "WW_CELL_CAM_DUMP" ) ) );
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

// lane SKYSTR1: the Sky visibility row (celllights.h)
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
			r |= t == "legacy" ? 1 : t == "nosoft" ? 2 : t == "nolin" ? 4 : t == "hide" ? 8 : 0;
		return r;
	}() );
	return red;
}

float wwCellImageSpaceAdapted( Scene * scene )
{
	return scene && scene->nifModel ? st().adapted.value( scene->nifModel, -1.0f ) : -1.0f;
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
	const float e = cellExposure( h, a );
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
	WW_SWITCH_STATIC( QString, dump, QString::fromLocal8Bit( wwEnv( "WW_CELL_CUBE_DUMP" ) ) );
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
	if ( s.flickerOn || s.flickerRed ) {	// lane LIGHTFLICKER1
		const auto fd = scene && scene->nifModel ? flickDocs().constFind( scene->nifModel ) : flickDocs().constEnd();
		const bool known = fd != flickDocs().constEnd();
		o += QStringLiteral( " flicker=%1(asked=%2, lights=%3, moving=%4, frames=%5, red=%6, step=%7)" )
			.arg( flickLive( scene ) ? "on" : "off" ).arg( s.flickerOn ? 1 : 0 ).arg( known ? int( fd->idx.size() ) : 0 )
			.arg( known ? fd->moving : 0 ).arg( known ? fd->frames : 0 ).arg( s.flickerRed ? 1 : 0 ).arg( double( s.flickerStep ) );
	}
	o += QStringLiteral( " giamb=%1 gifill=%2" ).arg( QStringList{ "keep", "replace", "off", "max", "asgi" }.value( s.giAmb ) )
		.arg( s.giFill ? 1 : 0 );	// lane GICAL1
	if ( s.red )
		o += QStringLiteral( " red=%1" ).arg( s.red );
	if ( s.pass > 0 )	// lane PROBEVIEW1
		o += QStringLiteral( " pass=%1(asked=%2 %3, sky=%4, pvred=%5)" ).arg( scene ? wwCellPassFor( scene->nifModel ) : 0 )
			.arg( s.pass ).arg( wwCellPassNames().value( s.pass ) ).arg( G && !G->sky.empty() ? 1 : 0 ).arg( s.passRed );
	return o + QLatin1Char( ' ' ) + wwCellShadowEcho( scene )
		+ ( wwVolAirOn() ? QStringLiteral( " " ) + wwVolAirSummary() : QString() )	// lane VOLAIR1
		+ ( wwLookOn( WwLook::LensFlares ) ? QStringLiteral( " " ) + wwCellLensFlareEcho( scene ) : QString() );	// lane LIGHTS3
}

//! WW_CELL_SHADOW_DUMP=<file>: the echo, rewritten whenever the slots change (the gate's light list)
static void shadowDump( Scene * scene )
{
	WW_SWITCH_STATIC( QString, path, QString::fromLocal8Bit( wwEnv( "WW_CELL_SHADOW_DUMP" ) ) );
	if ( path.isEmpty() )
		return;
	QFile f( path );
	if ( f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		f.write( ( wwCellShadowEcho( scene ) + QLatin1Char( '\n' ) ).toUtf8() );
}

//! the depth cube array (kShadowSlots cubes of face x face) and its framebuffer, once
static void shadowAlloc( Renderer * r, Gpu & g, int face )
{
	if ( g.shTex )
		return;
	auto fn = r->fn;
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGenTextures( 1, &g.shTex );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kShadowUnit ) );
	fn->glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, g.shTex );
	fn->glTexImage3D( GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT16, face, face, 6 * kShadowSlots, 0,
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
	g.shFace = face;
}

/* lane LAMPSHADOW1: THE STORED LAMP SHADOWS (gl/celllampshadow.h). A held slot is only drawn when it is new, or a
 * mover in its light's volume moved, or the set of movers in it changed: the light's stored static map is copied in
 * (depth blits, exact), then the movers drawn over it (LEQUAL keeps the nearest: the same depths as one pass). */
namespace {

constexpr float kLsM[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
constexpr float kLsU[6][3] = { { 0, 0, -1 }, { 0, 0, 1 }, { 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { -1, 0, 0 } };
constexpr float kLsV[6][3] = { { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 } };

struct LsCaster
{
	const Shape * sh;
	int id;
	Vector3 c;
	float rad;
};

struct LsRect
{
	int x = 0, y = 0, w = 0, h = 0;
};

//! one face draw: the layer of tex, the viewport's origin (a packed half shifts it), the scissor, clear first or not
struct LsTarget
{
	GLuint tex = 0;
	int layer = 0;
	int vx = 0, vy = 0;
	LsRect cut;
	bool clear = true;
};

bool lsTouch( const WwCellLight & l, const Vector3 & c, float rad )
{
	const float dx = c[0] - l.pos[0], dy = c[1] - l.pos[1], dz = c[2] - l.pos[2], reach = rad + l.radius;
	return dx * dx + dy * dy + dz * dz < reach * reach;
}

Gpu::LsSig lsSigOf( const Shape * sh, const Vector3 & c, float rad )
{
	Gpu::LsSig s;
	const Transform & w = sh->worldTrans();
	int n = 0;
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ )
			s.v[n++] = w.rotation( i, j );
	for ( int i = 0; i < 3; i++ )
		s.v[n++] = w.translation[i];
	s.v[n++] = w.scale;
	// two vertices: a CPU-skinned shape moves its vertices, not its transform
	const Vector3 & a = sh->verts.constFirst();
	const Vector3 & b = sh->verts.at( sh->verts.size() / 2 );
	for ( int i = 0; i < 3; i++ ) {
		s.v[n++] = a[i];
		s.v[n++] = b[i];
	}
	s.v[n++] = float( sh->verts.size() );
	s.c = c;
	s.rad = rad;
	return s;
}

//! side face f of a lamp whose back looks into a wall: the room half and the wall half of it
void lsSideHalves( int f, int back, int S, LsRect & room, LsRect & wall )
{
	const float * fr = kLsM[back ^ 1];
	const float cu = kLsU[f][0] * fr[0] + kLsU[f][1] * fr[1] + kLsU[f][2] * fr[2];
	const float cv = kLsV[f][0] * fr[0] + kLsV[f][1] * fr[1] + kLsV[f][2] * fr[2];
	const int h = S / 2;
	if ( cu != 0.0f ) {
		room = { cu > 0.0f ? h : 0, 0, h, S };
		wall = { cu > 0.0f ? 0 : h, 0, h, S };
	} else {
		room = { 0, cv > 0.0f ? h : 0, S, h };
		wall = { 0, cv > 0.0f ? 0 : h, S, h };
	}
}

//! a wall lamp's side faces (the four off its back axis) with their packed place: layer 1 + pair, member 0/1
void lsSides( int back, int sides[4], int layer[4], int member[4] )
{
	int n = 0, pair = 0;
	for ( int ax = 0; ax < 3; ax++ ) {
		if ( ax == back / 2 )
			continue;
		for ( int j = 0; j < 2; j++ ) {
			sides[n] = 2 * ax + j;
			layer[n] = 1 + pair;
			member[n] = j;
			n++;
		}
		pair++;
	}
}

LsRect lsPacked( const LsRect & room, int member, int S )
{
	const int h = S / 2;
	return room.w == h ? LsRect { member * h, 0, h, S } : LsRect { 0, member * h, S, h };
}

}	// namespace

/* lane GAPFIX1: an alpha-tested shape casts what its alpha test keeps into the light cubes, as the sun's map
 * does (VOLAIR1); the cube passes skipped it, and a barrel's own lamp drew an empty cube (lightbooth1).
 * Red WW_CELL_SHADOW_RED=atskip skips them again. */
static bool cubeCasterAt( const Shape * sh )
{
	return sh->wwAlphaTested() && sh->wwShaderLighting() && !sh->coords.isEmpty()
		&& sh->coords.constFirst().size() == sh->verts.size();
}

static bool cubeSkipsCaster( const Shape * sh )
{
	WW_SWITCH_STATIC( bool, atRed, wwEnv( "WW_CELL_SHADOW_RED" ).trimmed() == "atskip" );	// lane LIVESET3: live
	return sh->wwAlphaTested() && ( atRed || !cubeCasterAt( sh ) );
}

static qint64 g_cubeAtCasters = 0, g_cubeAtDraws = 0;

static void cubeDrawCaster( Renderer * r, NifSkopeOpenGLContext::Program * prog, const Shape * sh )
{
	const bool at = sh->wwAlphaTested();
	const float * attrv[8] = { &( sh->verts.constFirst()[0] ), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	std::uint64_t mask = 3;
	prog->uni1i( "shAlphaTest", at ? 1 : 0 );
	if ( at ) {
		int texunit = 0;
		BSShaderLightingProperty * bsp = sh->wwShaderLighting();
		const BSLightingShaderProperty * lsp = sh->wwLightingShader();
		prog->uniSampler( bsp, "BaseMap", 0, texunit, QString(), bsp->clampMode, QString() );
		prog->uni2f( "uvScale", bsp->uvScale.x, bsp->uvScale.y );
		prog->uni2f( "uvOffset", bsp->uvOffset.x, bsp->uvOffset.y );
		prog->uni1f( "alpha", lsp ? lsp->alpha : 1.0f );
		prog->uni1f( "alphaThreshold", sh->wwAlphaThreshold() );
		attrv[7] = &( sh->coords.constFirst().constFirst()[0] );
		mask = 0x20000003ULL;
		g_cubeAtDraws++;
	}
	r->drawShape( (unsigned int) ( sh->verts.size() ), mask, (unsigned int) ( sh->triangles.size() * 3 ),
		GL_TRIANGLES, GL_UNSIGNED_SHORT, attrv, sh->triangles.constData() );
}

static void lampShadowPass( Scene * scene, Gpu & g, const WwCellLighting & L, std::vector<int> & dirty, const float cam[3] )
{
	ClState & s = st();
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	const int S = wwCellLampShadowFace();
	const int red = wwCellLampShadowRed();
	const int ver = s.version.value( scene->nifModel );
	const std::uint64_t blockBytes = std::uint64_t( 3 ) * std::uint64_t( S ) * std::uint64_t( S ) * 2u;
	QElapsedTimer timer;
	timer.start();
	g.lsFrame++;
	if ( g.lsDoc != scene->nifModel || g.lsVersion != ver || g.lsFace != S ) {
		g.lsStore.clear();
		g.lsWall.clear();
		g.lsHist.clear();
		g.lsMovers.clear();
		for ( std::vector<int> & v : g.lsSlotMovers )
			v.clear();
		std::fill( g.lsBlockUsed.begin(), g.lsBlockUsed.end(), char( 0 ) );
		// the store holds the cell's shadow lamps (2 blocks each at most), up to the tier's budget
		int lamps = 0;
		for ( const WwCellLight & l : L.lights )
			lamps += l.shadow ? 1 : 0;
		const int budget = int( std::uint64_t( wwCellLampShadowBudgetMB() ) * 1048576u / blockBytes );
		const int want = std::max( 2, std::min( budget, 2 * lamps ) );
		if ( g.lsTex && ( g.lsFace != S || g.lsBlocks != want ) ) {
			fn->glDeleteTextures( 1, &g.lsTex );
			g.lsTex = 0;
		}
		g.lsBlocks = want;
		g.lsBlockUsed.assign( size_t( want ), char( 0 ) );
		g.lsDoc = scene->nifModel;
		g.lsVersion = ver;
		g.lsFace = S;
	}

	// the casters, as the cube pass takes them (the water too: SUNCELL1's red keeps it)
	WW_SWITCH_STATIC( bool, waterCastsRed, qgetenv( "WW_CELL_SHADOW_RED" ).trimmed() == "watercasts" );	// fix TDFIX2: follows a live set
	std::vector<LsCaster> casters;
	for ( Node * node : scene->nodes.list() ) {
		const Shape * sh = dynamic_cast<const Shape *>( node );
		if ( !sh || !sh->isVisible() || !sh->wwCastsSunShadow() || cubeSkipsCaster( sh ) )
			continue;
		if ( sh->verts.isEmpty() || sh->triangles.isEmpty() )
			continue;
		if ( !waterCastsRed && wwCellWaterIsShape( scene->nifModel, sh->id() ) )
			continue;
		const BoundSphere b = sh->bounds();
		casters.push_back( { sh, sh->id(), b.center, b.radius } );
	}
	// what moved since the last frame; a first move makes a caster a mover and drops the stored maps it was in
	auto slotOfLight = [&g]( int li ) {
		for ( int k = 0; k < kShadowSlots; k++ )
			if ( g.shSlot[k] == li )
				return k;
		return -1;
	};
	auto markDirty = [&dirty]( int k ) {
		if ( k >= 0 && std::find( dirty.begin(), dirty.end(), k ) == dirty.end() )
			dirty.push_back( k );
	};
	auto freeEntry = [&g]( const Gpu::LsEntry & e ) {
		for ( int b : e.blocks )
			if ( b >= 0 && b < int( g.lsBlockUsed.size() ) )
				g.lsBlockUsed[size_t( b )] = 0;
	};
	QHash<int, char> moved;
	int newMovers = 0;
	for ( const LsCaster & c : casters ) {
		const Gpu::LsSig sig = lsSigOf( c.sh, c.c, c.rad );
		auto it = g.lsHist.find( c.id );
		if ( it != g.lsHist.end() && std::memcmp( it->v, sig.v, sizeof sig.v ) != 0 ) {
			moved.insert( c.id, 1 );
			if ( !g.lsMovers.contains( c.id ) ) {
				g.lsMovers.insert( c.id, 1 );
				newMovers++;
				for ( auto e = g.lsStore.begin(); e != g.lsStore.end(); ) {
					const WwCellLight & l = L.lights.at( e.key() );
					if ( lsTouch( l, it->c, it->rad ) || lsTouch( l, c.c, c.rad ) ) {
						freeEntry( e.value() );
						g.lsWall.remove( e.key() );
						markDirty( slotOfLight( e.key() ) );
						e = g.lsStore.erase( e );
					} else {
						++e;
					}
				}
			}
		}
		g.lsHist.insert( c.id, sig );
	}
	std::vector<const LsCaster *> movers;
	for ( const LsCaster & c : casters )
		if ( g.lsMovers.contains( c.id ) )
			movers.push_back( &c );

	// the slots to draw this frame
	struct Job
	{
		int k;
		std::vector<int> in;
	};
	std::vector<Job> jobs;
	for ( int k = 0; k < kShadowSlots; k++ ) {
		const int li = g.shSlot[k];
		if ( li < 0 || li >= L.lights.size() )
			continue;
		const WwCellLight & l = L.lights.at( li );
		Job j { k, {} };
		bool anyMoved = false;
		for ( const LsCaster * m : movers ) {
			if ( lsTouch( l, m->c, m->rad ) ) {
				j.in.push_back( m->id );
				anyMoved = anyMoved || moved.contains( m->id );
			}
		}
		const bool isDirty = std::find( dirty.begin(), dirty.end(), k ) != dirty.end();
		if ( red == 1 || isDirty || anyMoved || j.in != g.lsSlotMovers[k] )
			jobs.push_back( std::move( j ) );
	}
	auto census = [&]( int stored, int restored, int direct, int moverDraws ) {
		int used = 0, walls = 0;
		for ( char u : g.lsBlockUsed )
			used += u ? 1 : 0;
		for ( const Gpu::LsEntry & e : g.lsStore )
			walls += e.back >= 0 ? 1 : 0;
		const double mb = 1.0 / 1048576.0;
		s.lampLast = QStringLiteral( "%1 face=%2 store=%3(wall %4) storeMB=%5/%6 slotsMB=%7 slotsDrawn=%8 stored=%9 restored=%10"
			" direct=%11 movers=%12(new %13) moverDraws=%14 ms=%15%16" )
			.arg( wwCellLampShadowTierName( wwCellLampShadowTier() ) ).arg( S )
			.arg( g.lsStore.size() ).arg( walls )
			.arg( double( used ) * double( blockBytes ) * mb, 0, 'f', 1 ).arg( double( g.lsBlocks ) * double( blockBytes ) * mb, 0, 'f', 1 )
			.arg( double( blockBytes ) * 2.0 * kShadowSlots * mb, 0, 'f', 1 )
			.arg( jobs.size() ).arg( stored ).arg( restored ).arg( direct ).arg( movers.size() ).arg( newMovers ).arg( moverDraws )
			.arg( timer.elapsed() )
			.arg( red == 1 ? QStringLiteral( " red=live" ) : red == 2 ? QStringLiteral( " red=cube" ) : QString() );
	};
	if ( jobs.empty() ) {
		census( 0, 0, 0, 0 );
		return;
	}

	shadowAlloc( r, g, S );
	if ( !g.lsTex ) {
		fn->glGenTextures( 1, &g.lsTex );
		GLint prevActive = 0;
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kShadowUnit ) );
		GLint prevArray = 0;
		fn->glGetIntegerv( GL_TEXTURE_BINDING_2D_ARRAY, &prevArray );
		fn->glBindTexture( GL_TEXTURE_2D_ARRAY, g.lsTex );
		fn->glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT16, S, S, 3 * g.lsBlocks, 0, GL_DEPTH_COMPONENT,
			GL_UNSIGNED_SHORT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		fn->glBindTexture( GL_TEXTURE_2D_ARRAY, GLuint( prevArray ) );
		fn->glActiveTexture( GLenum( prevActive ) );
	}
	if ( !g.lsReadFbo )
		fn->glGenFramebuffers( 1, &g.lsReadFbo );

	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_shadowdepth.prog" );
	if ( !prog ) {
		s.lampLast = QStringLiteral( "refused(no cell_shadowdepth.prog)" );
		return;
	}
	// save what the pass touches
	GLint prevRead = 0, prevDraw = 0, vp[4], sc4[4], polyMode[2], depthFunc = 0;
	GLboolean depthMask = GL_TRUE;
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_SCISSOR_BOX, sc4 );
	fn->glGetIntegerv( GL_POLYGON_MODE, polyMode );
	fn->glGetIntegerv( GL_DEPTH_FUNC, &depthFunc );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasOffset = fn->glIsEnabled( GL_POLYGON_OFFSET_FILL );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	const bool wasBlend = fn->glIsEnabled( GL_BLEND );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.lsReadFbo );
	fn->glReadBuffer( GL_NONE );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.shFbo );
	fn->glDrawBuffer( GL_NONE );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_POLYGON_OFFSET_FILL );
	fn->glEnable( GL_DEPTH_TEST );
	fn->glDepthFunc( GL_LEQUAL );
	fn->glDepthMask( GL_TRUE );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	fn->glClearDepth( 1.0 );

	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	const Matrix4 viewM = vt.toMatrix4();
	const int lRel = prog->uniLocation( "shRelFromView" ), lClip = prog->uniLocation( "shClipFromRel" );
	const int lRn = prog->uniLocation( "shRadiusNear" );
	bool complete = true;
	auto setLight = [&]( const WwCellLight & l ) {
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
	};
	// one face: target, the face's projection, the casters (and the wall's plane when given)
	auto drawFace = [&]( const WwCellLight & l, int face, const LsTarget & t, const std::vector<const LsCaster *> & list,
		const float * quad ) -> qint64 {
		fn->glFramebufferTextureLayer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, t.tex, 0, t.layer );
		if ( fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
			complete = false;
			return 0;
		}
		fn->glViewport( t.vx, t.vy, S, S );
		fn->glEnable( GL_SCISSOR_TEST );
		fn->glScissor( t.cut.x, t.cut.y, t.cut.w, t.cut.h );
		if ( t.clear )
			fn->glClear( GL_DEPTH_BUFFER_BIT );
		const float n = 1.0f, f = l.radius * 1.01f + 2.0f;
		const float A = ( f + n ) / ( f - n ), B = -2.0f * f * n / ( f - n );
		float clip[16];
		for ( int c = 0; c < 3; c++ ) {
			clip[c * 4 + 0] = kLsU[face][c];
			clip[c * 4 + 1] = kLsV[face][c];
			clip[c * 4 + 2] = A * kLsM[face][c];
			clip[c * 4 + 3] = kLsM[face][c];
		}
		clip[12] = clip[13] = clip[15] = 0.0f;
		clip[14] = B;
		if ( lClip >= 0 )
			fn->glUniformMatrix4fv( lClip, 1, GL_FALSE, clip );
		qint64 drawn = 0;
		for ( const LsCaster * c : list ) {
			prog->uni4m( "modelViewMatrix", c->sh->viewTrans().toMatrix4() );
			cubeDrawCaster( r, prog, c->sh );
			drawn++;
		}
		if ( quad ) {
			static const quint16 idx[6] = { 0, 1, 2, 0, 2, 3 };
			prog->uni1i( "shAlphaTest", 0 );
			prog->uni4m( "modelViewMatrix", viewM );
			r->drawShape( 4u, 3, 6u, GL_TRIANGLES, GL_UNSIGNED_SHORT, &quad, idx );
			drawn++;
		}
		return drawn;
	};
	auto blit = [&]( int srcLayer, const LsRect & a, int slotLayer, const LsRect & b ) {
		fn->glFramebufferTextureLayer( GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, g.lsTex, 0, srcLayer );
		fn->glFramebufferTextureLayer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, g.shTex, 0, slotLayer );
		if ( fn->glCheckFramebufferStatus( GL_READ_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE
			|| fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
			complete = false;
			return;
		}
		fn->glDisable( GL_SCISSOR_TEST );
		fn->glBlitFramebuffer( a.x, a.y, a.x + a.w, a.y + a.h, b.x, b.y, b.x + b.w, b.y + b.h, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
	};
	const LsRect full { 0, 0, S, S };

	int nStored = 0, nRestored = 0, nDirect = 0;
	qint64 moverDraws = 0;
	for ( Job & j : jobs ) {
		if ( !complete )
			break;
		const int k = j.k, li = g.shSlot[k];
		const WwCellLight & l = L.lights.at( li );
		setLight( l );
		std::vector<const LsCaster *> still, mov;
		for ( const LsCaster & c : casters ) {
			if ( !lsTouch( l, c.c, c.rad ) )
				continue;
			if ( red != 1 && g.lsMovers.contains( c.id ) ) {
				if ( std::find( j.in.begin(), j.in.end(), c.id ) != j.in.end() )
					mov.push_back( &c );
			} else {
				still.push_back( &c );
			}
		}
		auto direct = [&]() {
			for ( int f = 0; f < 6 && complete; f++ )
				drawFace( l, f, LsTarget { g.shTex, 6 * k + f, 0, 0, full, true }, still, nullptr );
			nDirect++;
		};
		if ( red == 1 ) {
			direct();
			g.lsSlotMovers[k] = j.in;
			continue;
		}
		// stored already, or store it now (the wall test once per lamp)
		auto e = g.lsStore.find( li );
		if ( e == g.lsStore.end() ) {
			if ( !g.lsWall.contains( li ) ) {
				WwLampWall w;
				if ( red != 2 ) {
					std::vector<const Shape *> shapes;
					for ( const LsCaster & c : casters )
						if ( !g.lsMovers.contains( c.id ) )
							shapes.push_back( c.sh );
					w = wwLampFindWall( l.pos, shapes );
				}
				g.lsWall.insert( li, w );
			}
			const WwLampWall w = g.lsWall.value( li );
			const int need = w.back >= 0 ? 1 : 2;
			std::vector<int> blocks;
			for ( int pass = 0; pass < 2 && int( blocks.size() ) < need; pass++ ) {
				blocks.clear();
				for ( int b = 0; b < g.lsBlocks && int( blocks.size() ) < need; b++ )
					if ( !g.lsBlockUsed[size_t( b )] )
						blocks.push_back( b );
				if ( int( blocks.size() ) >= need || pass == 1 )
					break;
				// full: evict the least recently used maps no slot holds until this one fits
				std::vector<std::pair<std::uint64_t, int>> lru;
				for ( auto it = g.lsStore.cbegin(); it != g.lsStore.cend(); ++it )
					if ( slotOfLight( it.key() ) < 0 )
						lru.emplace_back( it->used, it.key() );
				std::sort( lru.begin(), lru.end() );
				int freed = int( blocks.size() );
				for ( const auto & [used, key] : lru ) {
					if ( freed >= need )
						break;
					freed += int( g.lsStore.value( key ).blocks.size() );
					freeEntry( g.lsStore.value( key ) );
					g.lsStore.remove( key );
				}
			}
			if ( int( blocks.size() ) >= need ) {
				Gpu::LsEntry ne;
				ne.blocks = blocks;
				ne.back = w.back;
				ne.dist = w.dist;
				for ( int b : blocks )
					g.lsBlockUsed[size_t( b )] = 1;
				if ( w.back < 0 ) {
					for ( int f = 0; f < 6 && complete; f++ )
						drawFace( l, f, LsTarget { g.lsTex, 3 * blocks[size_t( f / 3 )] + f % 3, 0, 0, full, true }, still, nullptr );
				} else {
					drawFace( l, w.back ^ 1, LsTarget { g.lsTex, 3 * blocks[0], 0, 0, full, true }, still, nullptr );
					int sides[4], layer[4], member[4];
					lsSides( w.back, sides, layer, member );
					for ( int i = 0; i < 4 && complete; i++ ) {
						LsRect room, wall;
						lsSideHalves( sides[i], w.back, S, room, wall );
						const LsRect dst = lsPacked( room, member[i], S );
						drawFace( l, sides[i], LsTarget { g.lsTex, 3 * blocks[0] + layer[i], dst.x - room.x, dst.y - room.y, dst, true },
							still, nullptr );
					}
				}
				e = g.lsStore.insert( li, ne );
				nStored++;
			}
		}
		if ( e == g.lsStore.end() ) {
			direct();	// over the budget: drawn as before
		} else {
			e->used = g.lsFrame;
			const Gpu::LsEntry & en = e.value();
			if ( en.back < 0 ) {
				for ( int f = 0; f < 6; f++ )
					blit( 3 * en.blocks[size_t( f / 3 )] + f % 3, full, 6 * k + f, full );
			} else {
				blit( 3 * en.blocks[0], full, 6 * k + ( en.back ^ 1 ), full );
				int sides[4], layer[4], member[4];
				lsSides( en.back, sides, layer, member );
				for ( int i = 0; i < 4; i++ ) {
					LsRect room, wall;
					lsSideHalves( sides[i], en.back, S, room, wall );
					blit( 3 * en.blocks[0] + layer[i], lsPacked( room, member[i], S ), 6 * k + sides[i], room );
				}
				// the wall's half: the plane itself, out past the light's reach
				const int ax = en.back / 2, a1 = ( ax + 1 ) % 3, a2 = ( ax + 2 ) % 3;
				const float sg = ( en.back & 1 ) ? -1.0f : 1.0f, ext = l.radius * 2.0f + 64.0f;
				float quad[12];
				for ( int v = 0; v < 4; v++ ) {
					float * q = quad + 3 * v;
					q[ax] = l.pos[ax] + sg * en.dist;
					q[a1] = l.pos[a1] + ( v == 0 || v == 3 ? -ext : ext );
					q[a2] = l.pos[a2] + ( v < 2 ? -ext : ext );
				}
				const std::vector<const LsCaster *> none;
				drawFace( l, en.back, LsTarget { g.shTex, 6 * k + en.back, 0, 0, full, true }, none, quad );
				for ( int i = 0; i < 4 && complete; i++ ) {
					LsRect room, wall;
					lsSideHalves( sides[i], en.back, S, room, wall );
					drawFace( l, sides[i], LsTarget { g.shTex, 6 * k + sides[i], 0, 0, wall, true }, none, quad );
				}
			}
			nRestored++;
		}
		// the movers in this light's volume, over the static map
		if ( !mov.empty() )
			for ( int f = 0; f < 6 && complete; f++ )
				moverDraws += drawFace( l, f, LsTarget { g.shTex, 6 * k + f, 0, 0, full, false }, mov, nullptr );
		g.lsSlotMovers[k] = j.in;
	}
	r->stopProgram();

	// restore
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glScissor( sc4[0], sc4[1], sc4[2], sc4[3] );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GLenum( polyMode[0] ) );
	fn->glDepthFunc( GLenum( depthFunc ) );
	fn->glDepthMask( depthMask );
	wasCull ? fn->glEnable( GL_CULL_FACE ) : fn->glDisable( GL_CULL_FACE );
	wasOffset ? fn->glEnable( GL_POLYGON_OFFSET_FILL ) : fn->glDisable( GL_POLYGON_OFFSET_FILL );
	wasDepth ? fn->glEnable( GL_DEPTH_TEST ) : fn->glDisable( GL_DEPTH_TEST );
	wasScissor ? fn->glEnable( GL_SCISSOR_TEST ) : fn->glDisable( GL_SCISSOR_TEST );
	wasBlend ? fn->glEnable( GL_BLEND ) : fn->glDisable( GL_BLEND );

	if ( !complete ) {
		for ( int k = 0; k < kShadowSlots; k++ )
			g.shSlot[k] = -1;
		g.shStamp++;
		g.lsStore.clear();
		std::fill( g.lsBlockUsed.begin(), g.lsBlockUsed.end(), char( 0 ) );
		s.lampLast = QStringLiteral( "refused(framebuffer incomplete)" );
		return;
	}
	census( nStored, nRestored, nDirect, int( moverDraws ) );
	shadowDump( scene );
}

/* lane SHADOW1: the depth cubes (celllights.h). Each face: world relative to the light, GL's cube face
 * axes (major m, s = u.r, t = v.r over |m.r|), 90 degrees; depth = distance / radius (the fragment stage). */
namespace {
QHash<const void *, int> & shadowKicks()
{
	static QHash<const void *, int> k;
	return k;
}
}	// namespace

void wwCellShadowKick( const void * nif )
{
	shadowKicks()[nif]++;
}

void wwCellShadowsForget( const void * nif )
{
	for ( Gpu & g : gpus() ) {	// the next pass empties the slots and the lamp store, as a fresh open's first pass
		if ( g.shDoc == nif )
			g.shDoc = nullptr;
		if ( g.lsDoc == nif )
			g.lsDoc = nullptr;
	}
}

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
	// lane LAMPSHADOW1: the tier's face edge; a change remakes the cube array and empties the slots
	const int face = wwCellLampShadowFace();
	if ( g.shTex && g.shFace != face ) {
		r->fn->glDeleteTextures( 1, &g.shTex );
		r->fn->glDeleteFramebuffers( 1, &g.shFbo );
		g.shTex = g.shFbo = 0;
		std::fill( g.shSlot, g.shSlot + kShadowSlots, -1 );
		g.shStamp++;
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
	const int budget = wwCellShadowBudget();	// lane GAPLOOK1 (LIGHTSYS-3): 0 = the nearest, as before
	std::vector<float> fadeOf, rawOf;
	if ( budget > 0 ) {
		const bool worst = wwCellShadowBudgetRedWorst();
		std::vector<char> wasHeld( size_t( L->lights.size() ), 0 );
		for ( int k = 0; k < kShadowSlots; k++ )
			if ( g.shSlot[k] >= 0 && g.shSlot[k] < L->lights.size() )
				wasHeld[size_t( g.shSlot[k] )] = 1;
		fadeOf.assign( size_t( L->lights.size() ), 0.0f );
		rawOf.assign( size_t( L->lights.size() ), 0.0f );
		for ( qsizetype i = 0; i < L->lights.size(); i++ ) {
			const WwCellLight & l = L->lights.at( i );
			if ( !l.shadow )
				continue;
			const float dx = l.pos[0] - cam[0], dy = l.pos[1] - cam[1], dz = l.pos[2] - cam[2];
			const float d = std::sqrt( dx * dx + dy * dy + dz * dz );
			const float t = std::clamp( ( d - kShadowFadeBegin ) / ( kShadowFadeEnd - kShadowFadeBegin ), 0.0f, 1.0f );
			const float dfade = 1.0f - t * t * ( 3.0f - 2.0f * t );
			if ( dfade <= 0.0f )
				continue;	// faded out: takes no slot
			const float lum = 0.2126f * l.color[0] + 0.7152f * l.color[1] + 0.0722f * l.color[2];
			const float raw = std::max( lum, 1e-6f ) * l.radius / std::max( d, 1.0f ) * dfade;
			fadeOf[size_t( i )] = dfade;
			rawOf[size_t( i )] = raw;
			const float eff = raw * ( wasHeld[size_t( i )] ? 1.2f : 1.0f );	// hysteresis: a holder needs beating by 20 %
			cand.emplace_back( worst ? eff : -eff, int( i ) );
		}
	} else {
		for ( qsizetype i = 0; i < L->lights.size(); i++ ) {
			const WwCellLight & l = L->lights.at( i );
			if ( !l.shadow )
				continue;
			const float dx = l.pos[0] - cam[0], dy = l.pos[1] - cam[1], dz = l.pos[2] - cam[2];
			cand.emplace_back( std::max( 0.0f, std::sqrt( dx * dx + dy * dy + dz * dz ) - l.radius ), int( i ) );
		}
	}
	const int want = std::min( int( cand.size() ), budget > 0 ? budget : kShadowSlots );
	std::partial_sort( cand.begin(), cand.begin() + want, cand.end() );
	g.shBudget = budget;
	g.shCut = 0.0f;
	if ( budget > 0 ) {
		// the cut: the best raw score left out; a kept light fades in over 1.2..1.5 x it (past the hysteresis, so
		// neither the light leaving nor the one taking its slot pops)
		for ( size_t k = size_t( want ); k < cand.size(); k++ )
			g.shCut = std::max( g.shCut, rawOf[size_t( cand[k].second )] );
		if ( g.shCut > 0.0f && !wwCellShadowBudgetRedWorst() )
			for ( int k = 0; k < want; k++ ) {
				const size_t li = size_t( cand[size_t( k )].second );
				const float t = std::clamp( ( rawOf[li] / g.shCut - 1.2f ) / 0.3f, 0.0f, 1.0f );
				fadeOf[li] *= t * t * ( 3.0f - 2.0f * t );
			}
	}
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
	{
		const int kick = shadowKicks().value( scene->nifModel );
		if ( g.shKick != kick ) {
			g.shKick = kick;
			for ( int k = 0; k < kShadowSlots; k++ )
				if ( g.shSlot[k] >= 0 && std::find( dirty.begin(), dirty.end(), k ) == dirty.end() )
					dirty.push_back( k );
		}
	}
	if ( changed || !dirty.empty() )
		g.shStamp++;
	for ( int k = 0; k < kShadowSlots; k++ )	// lane GAPLOOK1: the strengths follow the camera every pass
		g.shFade[k] = budget > 0 && g.shSlot[k] >= 0 && size_t( g.shSlot[k] ) < fadeOf.size() ? fadeOf[size_t( g.shSlot[k] )] : 1.0f;
	if ( wwCellLampShadowTier() >= 0 ) {
		s.shadowLast = QStringLiteral( "on shadowLights=%1 face=%2 (lamp)" ).arg( cand.size() ).arg( face );
		lampShadowPass( scene, g, *L, dirty, cam );	// lane LAMPSHADOW1: stored maps, movers redrawn on top
		return;
	}
	if ( g.lsTex ) {	// the row turned Off: the store's memory goes back
		r->fn->glDeleteTextures( 1, &g.lsTex );
		g.lsTex = 0;
		g.lsDoc = nullptr;
		g.lsStore.clear();
	}
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
	shadowAlloc( r, g, face );

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
	WW_SWITCH_STATIC( bool, waterCastsRed, wwEnv( "WW_CELL_SHADOW_RED" ).trimmed() == "watercasts" );
	int waterSkipped = 0;
	for ( Node * node : scene->nodes.list() ) {
		const Shape * sh = dynamic_cast<const Shape *>( node );
		if ( !sh || !sh->isVisible() || !sh->wwCastsSunShadow() || cubeSkipsCaster( sh ) )
			continue;
		if ( sh->verts.isEmpty() || sh->triangles.isEmpty() )
			continue;
		if ( !waterCastsRed && wwCellWaterIsShape( scene->nifModel, sh->id() ) ) {
			waterSkipped++;
			continue;
		}
		g_cubeAtCasters += sh->wwAlphaTested() ? 1 : 0;
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
	// lane MSAABLACK1: bind READ too, so glReadBuffer below sets OUR fbo; on the view's it left TAA's colour blit
	// a silent no-op (MSAA 1 + TAA saved black). WW_MSAABLACK1_RED=1: the old binding
	WW_SWITCH_STATIC( bool, msaaBlackRed, wwEnvInt( "WW_MSAABLACK1_RED" ) == 1 );
	if ( !msaaBlackRed )
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.shFbo );
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
	fn->glViewport( 0, 0, face, face );

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
				cubeDrawCaster( r, prog, c->sh );
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
		.arg( cand.size() ).arg( dirty.size() ).arg( casters.size() ).arg( drawn ).arg( timer.elapsed() ).arg( face )
		.arg( waterCastsRed ? QStringLiteral( "casts(RED)" ) : QStringLiteral( "skipped %1" ).arg( waterSkipped ) );	// lane SUNCELL1
	shadowDump( scene );
}

QString wwCellShadowEcho( Scene * scene )
{
	ClState & s = st();
	QString o = QStringLiteral( "shadow=%1" ).arg( s.shadowLast );
	o += QStringLiteral( " atCasters=%1 atDraws=%2" ).arg( g_cubeAtCasters ).arg( g_cubeAtDraws );	// lane GAPFIX1
	if ( wwCellLampShadowTier() >= 0 )
		o += QStringLiteral( " lamp=" ) + s.lampLast;	// lane LAMPSHADOW1
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
	if ( g.shBudget > 0 ) {	// lane GAPLOOK1: the budget, its cut score and the strengths it gives the held slots
		QString f;
		for ( int k = 0; k < kShadowSlots; k++ )
			if ( g.shSlot[k] >= 0 )
				f += QStringLiteral( "%1%2" ).arg( f.isEmpty() ? QString() : QStringLiteral( "," ) ).arg( double( g.shFade[k] ), 0, 'f', 3 );
		o += QStringLiteral( " budget=%1 cut=%2 fade=%3%4" ).arg( g.shBudget ).arg( double( g.shCut ), 0, 'g', 4 )
			.arg( f.isEmpty() ? QStringLiteral( "-" ) : f ).arg( wwCellShadowBudgetRedWorst() ? QStringLiteral( " RED=worst" ) : QString() );
	}
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

constexpr int kAoUnit = WwCellUnit::Ao;			// the obscurance (sampler2D, half the view); 9 on a GPU with only 16 units
constexpr int kAoAngles = 8;		// a still view's mean over the game's per-frame angle in [0, pi)
constexpr float kAoRadius = 108.2f, kAoBias = 0.6f, kAoIntensity = 7.1f;	// the game's INI defaults, game units

struct AoState
{
	bool loaded = false;
	quint64 liveGen = 0;	// lane TESTDRIVER1: the switch generation last read
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
	// lane TESTDRIVER1: a live set of these switches re-reads them (pins and or-ed reds back to their defaults first)
	if ( !a.loaded || wwSwitchMovedFor( a.liveGen, { "WW_CELL_AO" } ) ) {
		a.loaded = true;
		a.liveGen = wwSwitchGen();
		const QByteArray aoPin = wwEnv( "WW_CELL_AO" ).trimmed();
		a.pinned = !aoPin.isEmpty();
		a.on = a.pinned ? aoPin != "0" : QSettings().value( QStringLiteral( "WW/CellAo" ), true ).toBool();
		const QByteArray red = wwEnv( "WW_CELL_AO_RED" ).trimmed();
		a.red = red == "off" ? 1 : red == "radius" ? 2 : red == "noblur" ? 4 : red == "noreset" ? 8 : 0;
		a.dump = QString::fromLocal8Bit( wwEnv( "WW_CELL_AO_DUMP" ) );
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
	bool ready = false;		// the opaque pass + depth pyramid of this frame (the shared g-buffer: SSR, the AO decal)
	bool aoReady = false;	// lane AOOFF1: and the obscurance computed from it (false while the AO row is off)
	int unit = -1;
	GLuint decal = 0;		// lane AODECAL1: this frame's AO decal target (0: none)
	int decalUnit = -1;		// WwCellUnit::AoDecal, or -1 on a GPU without it (lane VOLAIR1: was 20, under the far slots)
	AoTarget giHalf;		// lane PERF1: the screen GI (RGBA32F), its key on the same FBO's attachment 1
	GLuint giKey = 0;
	int giKeyW = 0, giKeyH = 0;
	int giStep = 2;			// 2 half resolution, 1 full
	bool giReady = false;	// this frame's screen GI
	bool volOn = false;		// this frame's volumes' mask was drawn
	AoTarget volMask;		// lane PERF1: full size, the pixel's interior volume's room + 1 (R32F; 0 none)
	GLuint volBuf = 0, volTex = 0, volVao = 0;	// the shapes, 3 RGBA32F texels each (a buffer texture)
	std::vector<float> volData;	// what volBuf holds
	bool giUnits = false;	// the GPU has WwCellUnit::GiHalfKey
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
	return p && ( wwIsCellProgramName( p->name ) || wwIsCellDecalProgramName( p->name ) );	// lane DECALLIT1: a decal reads it
}

/* lane AOOFF1: the units, resolved whether or not the pass ever runs. They were resolved inside the pass only, after
 * its "AO off" return: with the row off every cell-lit draw then bound texture 0 on unit max(-1, 0) = 0, which is
 * BaseMap -- black albedo, the sun "gone", the frame half as bright. */
void aoUnits( NifSkopeOpenGLContext::GLFunctions * fn, AoGpu & g )
{
	if ( g.unit >= 0 )
		return;
	GLint units = 0;
	fn->glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &units );
	g.unit = units > kAoUnit ? kAoUnit : 9;
	g.decalUnit = units > WwCellUnit::AoDecal ? WwCellUnit::AoDecal : -1;
	g.giUnits = units > WwCellUnit::GiHalfKey;
}

//! lane PERF1: the half-res GI's telemetry line, printed when it changes
void giEcho( const QString & line )
{
	static QString last;
	if ( line == last )
		return;
	last = line;
	std::fprintf( stderr, "cell gi rate: %s\n", qPrintable( line ) );
}

/* lane PERF1: the GI volume mask (the Division deck's D4: the volumes drawn into the stencil). Each interior shape's
 * hull, grown by the bias, back faces only, depth-tested against the shared opaque pass (a hull wholly in front of the
 * surface draws nothing); cell_givol.frag tests the surface's point a bias in front against the shape and writes its
 * room + 1, blended by max. The outdoors draws nothing (0). False: no mask this frame. */
bool giVolMaskPass( Scene * scene, AoGpu & g, const WwCellGi * G, int W, int H, float p00, float p11 )
{
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	const int n = G ? int( G->volShapes.size() / 12 ) : 0;
	if ( n <= 0 || !g.gbufDepth )
		return false;
	if ( !g.volBuf ) {
		fn->glGenBuffers( 1, &g.volBuf );
		fn->glGenTextures( 1, &g.volTex );
		fn->glGenVertexArrays( 1, &g.volVao );
		g.volData.clear();
	}
	if ( g.volData != G->volShapes ) {
		fn->glBindBuffer( GL_TEXTURE_BUFFER, g.volBuf );
		fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( G->volShapes.size() * sizeof( float ) ), G->volShapes.data(), GL_STATIC_DRAW );
		fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
		g.volData = G->volShapes;
	}
	aoAlloc( fn, g.volMask, W, H, GL_R32F, GL_RED, false );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.volMask.fbo );
	fn->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g.gbufDepth );
	const GLenum one = GL_COLOR_ATTACHMENT0;
	fn->glDrawBuffers( 1, &one );
	if ( fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
		return false;
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_givol.prog" );
	if ( !prog )
		return false;
	fn->glViewport( 0, 0, W, H );
	const GLfloat zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	fn->glClearBufferfv( GL_COLOR, 0, zero );
	// world -> clip (the projection after posView = sc R world + t), and view -> world (cell_lights.glsl cellRow)
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	const auto & pm = r->globalUniforms->projectionMatrix;
	for ( int row = 0; row < 4; row++ ) {
		double e[4] = { 0, 0, 0, double( pm[3][row] ) };
		for ( int i = 0; i < 3; i++ ) {
			for ( int j = 0; j < 3; j++ )
				e[j] += double( pm[i][row] ) * double( vt.rotation( i, j ) ) * double( sc );
			e[3] += double( pm[i][row] ) * double( vt.translation[i] );
		}
		prog->uni4f_l( prog->uniLocation( "volClip[%d]", row ), FloatVector4( float( e[0] ), float( e[1] ), float( e[2] ), float( e[3] ) ) );
	}
	for ( int k = 0; k < 3; k++ ) {
		float w = 0.0f;
		for ( int j = 0; j < 3; j++ )
			w -= vt.rotation( j, k ) * vt.translation[j];
		prog->uni4f_l( prog->uniLocation( "volRow[%d]", k ), FloatVector4( vt.rotation( 0, k ) / sc, vt.rotation( 1, k ) / sc,
			vt.rotation( 2, k ) / sc, w / sc ) );
	}
	const float bias = 0.75f * G->roomsCell;	// the room grid's read (cell_lights.glsl cellGiVolAt)
	prog->uni1f( "volBias", bias );
	prog->uni1f( "volGrow", bias + 1.0f );
	prog->uni2f( "volProj", p00, p11 );
	fn->glUniform2i( prog->uniLocation( "volFull" ), W, H );
	GLint prevActive = 0, prevCull = GL_BACK, prevFront = GL_CCW, prevDepthFunc = GL_LESS, prevEq = GL_FUNC_ADD;
	GLint prevBlend[4] = { GL_ONE, GL_ZERO, GL_ONE, GL_ZERO };
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGetIntegerv( GL_CULL_FACE_MODE, &prevCull );
	fn->glGetIntegerv( GL_FRONT_FACE, &prevFront );
	fn->glGetIntegerv( GL_DEPTH_FUNC, &prevDepthFunc );
	fn->glGetIntegerv( GL_BLEND_EQUATION_RGB, &prevEq );
	fn->glGetIntegerv( GL_BLEND_SRC_RGB, &prevBlend[0] );
	fn->glGetIntegerv( GL_BLEND_DST_RGB, &prevBlend[1] );
	fn->glGetIntegerv( GL_BLEND_SRC_ALPHA, &prevBlend[2] );
	fn->glGetIntegerv( GL_BLEND_DST_ALPHA, &prevBlend[3] );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiHalf ) );
	fn->glBindTexture( GL_TEXTURE_2D, g.gbuf.tex );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiHalfKey ) );
	fn->glBindTexture( GL_TEXTURE_BUFFER, g.volTex );
	fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, g.volBuf );
	prog->uni1i( "volGbuf", WwCellUnit::GiHalf );
	prog->uni1i( "volShapes", WwCellUnit::GiHalfKey );
	fn->glEnable( GL_DEPTH_TEST );
	fn->glDepthFunc( GL_GEQUAL );	// the hull's back face behind the surface: the surface may be inside
	fn->glDepthMask( GL_FALSE );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glEnable( GL_CULL_FACE );
	fn->glFrontFace( GL_CCW );
	fn->glCullFace( GL_FRONT );
	fn->glEnable( GL_DEPTH_CLAMP );	// a back face past the far plane still covers its pixels
	fn->glEnable( GL_BLEND );
	fn->glBlendEquation( GL_MAX );
	fn->glBlendFunc( GL_ONE, GL_ONE );
	fn->glBindVertexArray( g.volVao );
	fn->glDrawArraysInstanced( GL_TRIANGLES, 0, 36, n );
	fn->glBindVertexArray( 0 );
	fn->glDisable( GL_DEPTH_CLAMP );
	fn->glBlendEquation( GLenum( prevEq ) );
	fn->glBlendFuncSeparate( GLenum( prevBlend[0] ), GLenum( prevBlend[1] ), GLenum( prevBlend[2] ), GLenum( prevBlend[3] ) );
	fn->glDisable( GL_BLEND );
	fn->glCullFace( GLenum( prevCull ) );
	fn->glFrontFace( GLenum( prevFront ) );
	fn->glDisable( GL_CULL_FACE );
	fn->glDepthFunc( GLenum( prevDepthFunc ) );
	fn->glDisable( GL_DEPTH_TEST );
	r->stopProgram();
	fn->glBindTexture( GL_TEXTURE_BUFFER, 0 );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiHalf ) );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	return true;
}

/* lane PERF1: the screen GI. The volume read (cellGiVolume) once per giStep x giStep pixels over the shared opaque
 * g-buffer, in each pixel's volume (the mask above, else the room grid): the sample and its key (world position,
 * packed normal), which the opaque cell-lit draws upsample (cell_lights.glsl cellGiSample). Runs inside wwCellAoPass,
 * after the depth pyramid; the caller restores the state. */
void giHalfPass( Scene * scene, AoGpu & g, int W, int H, float p00, float p11 )
{
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	const WwCellGi * G = wwCellGiFor( scene->nifModel );
	const bool volOn = giVolMaskPass( scene, g, G, W, H, p00, p11 );
	g.volOn = volOn;
	g.giStep = wwCellGiRate() == 1 ? 1 : 2;
	const int hw = ( W + g.giStep - 1 ) / g.giStep, hh = ( H + g.giStep - 1 ) / g.giStep;
	aoAlloc( fn, g.giHalf, hw, hh, GL_RGBA32F, GL_RGBA, false );
	if ( !g.giKey )
		fn->glGenTextures( 1, &g.giKey );
	if ( g.giKeyW != hw || g.giKeyH != hh ) {
		fn->glBindTexture( GL_TEXTURE_2D, g.giKey );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, hw, hh, 0, GL_RGBA, GL_FLOAT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		g.giKeyW = hw;
		g.giKeyH = hh;
	}
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.giHalf.fbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g.giKey, 0 );
	const GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	fn->glDrawBuffers( 2, bufs );
	if ( fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
		giEcho( QStringLiteral( "refused(target incomplete): the full chain" ) );
		return;
	}
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_gihalf.prog" );
	if ( !prog ) {
		giEcho( QStringLiteral( "refused(cell_gihalf.prog did not link): the full chain" ) );
		return;
	}
	wwCellLightsUniforms( scene );	// the surfaces' GI uniforms and grids, on this program
	prog->uni1b( "cellGiHalfOn", false );
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiHalf ) );
	fn->glBindTexture( GL_TEXTURE_2D, g.gbuf.tex );
	prog->uni1i( "giGbuf", WwCellUnit::GiHalf );
	prog->uni1i( "cellGiHalf", WwCellUnit::GiHalf );	// the same sampler type on the unit (never read here)
	prog->uni1i( "cellGiHalfKey", WwCellUnit::GiHalf );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiHalfKey ) );
	fn->glBindTexture( GL_TEXTURE_2D, volOn ? g.volMask.tex : 0 );
	prog->uni1i( "giVolMask", WwCellUnit::GiHalfKey );
	prog->uni1b( "giVolOn", volOn );
	prog->uni1i( "giStep", g.giStep );
	fn->glUniform2i( prog->uniLocation( "giFull" ), W, H );
	prog->uni2f( "giProj", p00, p11 );
	fn->glViewport( 0, 0, hw, hh );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	r->stopProgram();
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiHalf ) );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	const GLenum one = GL_COLOR_ATTACHMENT0;
	fn->glDrawBuffers( 1, &one );
	g.giReady = true;
	giEcho( QStringLiteral( "%1 %2x%3 screen %4x%5; volumes: mask %6 (%7 shapes)" ).arg( wwCellGiRate() == 3
		? QStringLiteral( "red (the screen pass runs, the surfaces read inline)" ) : g.giStep == 1 ? QStringLiteral( "full" )
		: QStringLiteral( "half" ) ).arg( W ).arg( H ).arg( hw ).arg( hh ).arg( volOn ? QStringLiteral( "on" ) : QStringLiteral( "off" ) )
		.arg( G ? qint64( G->volShapes.size() / 12 ) : 0 ) );
}

//! lane AOOFF1: the telemetry line, printed when it changes
void aoEcho( const QString & line )
{
	static QString last;
	if ( line == last )
		return;
	last = line;
	std::fprintf( stderr, "cell ao: %s\n", qPrintable( line ) );
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

bool wwCellAoPinned()
{
	return ao().pinned;
}

void wwCellAoPass( Scene * scene, bool run )
{
	if ( !scene || !scene->renderer )
		return;
	AoState & a = ao();
	Renderer * r = scene->renderer;
	AoGpu & g = aoGpus()[r];
	g.ready = false;
	g.aoReady = false;
	g.giReady = false;
	if ( !run || !wwCellLightsWanted( scene ) )
		return;
	auto fn = r->fn;
	aoUnits( fn, g );
	g.decal = 0;
	/* lane AOOFF1: the opaque pass below is the frame's SHARED g-buffer (view normal + linear depth, the depth
	 * pyramid): the reflections march it and the AO decal projects into it. The AO row switches off only the
	 * obscurance computed from it (step 2's raw, blurs, final) and its apply, never the pass the others read. */
	const bool forSsr = wwCellSsrWantsTargets( scene );
	const bool forDecal = g.decalUnit >= 0 && wwCellAoDecalWanted( scene->nifModel );
	const bool forContact = wwContactShadowWantsTargets( scene );	// lane TERRSHADOW1: contact shadows march its depth
	// lane PERF1: the half-res GI reads it too (the GI row on, no probe pass, a GPU with the units)
	const int giRate = wwCellGiRate();
	const bool forGi = giRate != 2 && wwCellGiVis() == 0 && g.giUnits && wwCellGiOn() && wwCellPassFor( scene->nifModel ) == 0;
	if ( wwCellGiVis() == 1 )
		giEcho( QStringLiteral( "march (red WW_CELL_GI_VIS or a march-only WW_CELL_PV_RED: the per-pixel chain every call, no screen pass)" ) );
	else if ( giRate == 2 )
		giEcho( QStringLiteral( "inline (no screen pass: every surface reads the volumes)" ) );
	else if ( !g.giUnits )
		giEcho( QStringLiteral( "full (the GPU has no unit %1)" ).arg( WwCellUnit::GiHalfKey ) );
	if ( !a.on ) {
		aoEcho( ( forSsr || forDecal || forContact || forGi ) ? QStringLiteral( "off (row); the shared opaque pass kept for%1%2%3%4" )
			.arg( forSsr ? QStringLiteral( " ssr" ) : QString() ).arg( forDecal ? QStringLiteral( " aodecal" ) : QString() )
			.arg( forContact ? QStringLiteral( " contact" ) : QString() ).arg( forGi ? QStringLiteral( " gihalf" ) : QString() )
			: QStringLiteral( "off (row); no opaque pass (nothing reads it)" ) );
		if ( !forSsr && !forDecal && !forContact && !forGi ) {
			a.last = QStringLiteral( "off" );
			return;
		}
	} else {
		aoEcho( ( a.red & 1 ) ? QStringLiteral( "computed, not applied (RED=off)" ) : QStringLiteral( "on" ) );
	}
	QElapsedTimer timer;
	timer.start();
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
	if ( wwCellSsrPrevStencil() )
		fn->glStencilMask( 0xFF );	// lane PERF3: the clear zeroes the flag bit too
	fn->glClear( GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
	fn->glDisable( GL_BLEND );
	fn->glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );	// wwCellAoDraw opens it for the opaque cell-lit draws
	a.pass = true;
	PassTime::subBegin( PassTime::AoGeometry );	// lane PERF2: the obscurance pass's three parts
	{
		/* lane PERF2: this pass is CPU-bound per draw. memo: the scene-global setters once per program (as the main
		 * pass); opaque: the sorted draws that write nothing here are skipped (PERF2FIX1). =0 or *_RED=1: old */
		WW_SWITCH_STATIC( bool, memo, qgetenv( "WW_CELL_AO_MEMO" ) != "0" && !qEnvironmentVariableIntValue( "WW_CELL_AO_MEMO_RED" ) );
		WW_SWITCH_STATIC( bool, opaqueOnly, qgetenv( "WW_CELL_AO_OPAQUE" ) != "0" && !qEnvironmentVariableIntValue( "WW_CELL_AO_OPAQUE_RED" ) );
		NodeList second;
		wwOpaqueCutNewFrame();
		if ( memo )
			wwSetupMemoArm( true );
		scene->collectShapes( second );
		if ( memo )
			wwSetupMemoArm( false );
		// lane PERF2FIX1: the sorted draws keep their order; the cut skips only those that wrote nothing here last frame
		if ( opaqueOnly )
			wwOpaqueCutArm( true, "ao" );
		Scene::drawDeferredShapes( second );
		scene->drawShapeEffects();
		if ( opaqueOnly )
			wwOpaqueCutArm( false, "ao" );
	}
	PassTime::subEnd( PassTime::AoGeometry );
	a.pass = false;
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glDepthMask( GL_TRUE );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDisable( GL_POLYGON_OFFSET_FILL );
	if ( wwCellSsrPrevStencil() ) {	// lane PERF3: the flag write's stencil state back to the defaults
		fn->glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		fn->glStencilMask( 0xFF );
		fn->glStencilFunc( GL_ALWAYS, 0, 0xFF );
	}
	// lane AODECAL1 (src/gl/cellaodecalgl.h): the decal copies over this opaque pass, into their own target
	if ( g.decalUnit >= 0 && wwCellAoDecalHas( scene->nifModel ) )
		g.decal = GLuint( wwCellAoDecalRun( scene, g.gbuf.tex, g.gbufDepth, W, H ) );

	// 2. the full-screen passes (cell_ao.frag)
	PassTime::subBegin( PassTime::AoStages );
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
	if ( a.on ) {	// lane AOOFF1: the obscurance itself; the pyramid above is shared
		pass( 2, g.raw, nullptr );
		pass( 3, g.across, &g.raw );
		pass( 4, g.fin, &g.across );
	}
	r->stopProgram();
	for ( int u = 0; u < 6; u++ ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
	}
	fn->glActiveTexture( GLenum( prevActive ) );
	PassTime::subEnd( PassTime::AoStages );
	PassTime::subBegin( PassTime::AoGi );
	if ( forGi )	// lane PERF1
		giHalfPass( scene, g, W, H, p00, p11 );
	PassTime::subEnd( PassTime::AoGi );

	g.ready = true;
	g.aoReady = a.on;
	g.doc = scene->nifModel;
	if ( !a.on ) {
		a.last = QStringLiteral( "off (opaque pass %1x%2 kept, ms %3)" ).arg( W ).arg( H ).arg( timer.elapsed() );
	} else {
		a.last = QStringLiteral( "on %1x%2 half %3x%4 radius %5 bias %6 intensity %7 angles %8 p00 %9 p11 %10 p20 %11 p21 %12 "
			"unit %13 red %14 ms %15" ).arg( W ).arg( H ).arg( hw ).arg( hh ).arg( double( radius ), 0, 'f', 2 )
			.arg( double( kAoBias ), 0, 'f', 2 ).arg( double( kAoIntensity ), 0, 'f', 2 ).arg( kAoAngles )
			.arg( double( p00 ), 0, 'f', 6 ).arg( double( p11 ), 0, 'f', 6 ).arg( double( pm[2][0] ), 0, 'f', 6 )
			.arg( double( pm[2][1] ), 0, 'f', 6 ).arg( g.unit ).arg( a.red ).arg( timer.elapsed() );
	}

	// WW_CELL_AO_DUMP=<file>: int32 W H hw hh, the opaque pass (W x H RGBA float, bottom row first), the raw
	// (A, key) and the final obscurance (hw x hh, top row first); <file>.txt the numbers
	if ( !a.dump.isEmpty() && g.aoReady ) {
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

// lane VOLAIR1: the light buffer's spheres (src/gl/celllights.h)
const std::vector<float> * wwCellLightSpheres( Scene * scene )
{
	if ( !scene || !scene->renderer || !gpus().contains( scene->renderer ) )
		return nullptr;
	const Gpu & g = gpus()[scene->renderer];
	if ( g.doc != scene->nifModel || int( g.spheres.size() / 4 ) != g.count )
		return nullptr;
	return &g.spheres;
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
	PassTime::DrawCpuScope cpuT( PassTime::CpuAoDraw );	// lane PERF2
	if ( !scene || !scene->renderer )
		return;
	AoState & a = ao();
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	cellProgram = cellProgram && aoIsCellProgram( prog );
	if ( a.pass ) {
		// the game's opaque (deferred) pass: no blended draw, no effect, nothing that is not cell-lit
		const bool opaque = cellProgram && !wwDrawBlend();
		glColorMask( opaque, opaque, opaque, opaque );
		if ( !opaque )
			glDepthMask( GL_FALSE );
		if ( wwCellSsrPrevStencil() ) {
			// lane PERF3 (cellssr.h): stencil bit 0x01 = the nearest opaque surface's reflection flag (AODECAL1 uses 0x80)
			glEnable( GL_STENCIL_TEST );
			glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
			glStencilMask( opaque ? 0x01u : 0x00u );
			glStencilFunc( GL_ALWAYS, opaque && wwCellSsrFlagged() ? 1 : 0, 0xFF );
		}
		wwOpaqueCutSaw( opaque );	// lane PERF2FIX1 (the cut's check)
		if ( cellProgram ) {
			prog->uni1i( "cellProbe", 20 );
			// lane PERF1: the fragment returns once it has the normal (WW_AO_EARLY=0 shades it all, =red marks the early ones)
			WW_SWITCH_STATIC( int, early, [] { const QByteArray v = wwEnv( "WW_AO_EARLY" ).trimmed(); return v == "0" ? 0 : v == "red" ? 2 : 1; }() );
			prog->uni1i( "cellAoEarly", early );
			prog->uni1b( "cellAoOn", false );
			prog->uni1b( "cellAoDecalOn", false );	// lane AODECAL1
			if ( prog->uniLocation( "cellCtOn" ) >= 0 )
				prog->uni1b( "cellCtOn", false );	// lane GAPLOOK1: this pass writes the depth C8 reads
		}
		return;
	}
	if ( !cellProgram || prog->uniLocation( "cellAoOn" ) < 0 )
		return;
	AoGpu & g = aoGpus()[r];
	aoUnits( r->fn, g );	// lane AOOFF1: never unit 0 (BaseMap), even when the pass has not run
	// lane DECALLIT1: an effect decal is blended, but the game draws it into the opaque surface it lies on
	const bool decalProg = wwIsCellDecalProgramName( prog->name );
	const bool on = g.aoReady && g.doc == scene->nifModel && !( a.red & 1 ) && ( !wwDrawBlend() || decalProg )
		&& wwCellLightsWanted( scene );
	// bound whether or not this draw reads it (a sampler left on unit 0 would sit beside BaseMap)
	const GLint prevActive = wwDrawActiveTexture();	// lane PERF3
	r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + g.unit ) );
	r->fn->glBindTexture( GL_TEXTURE_2D, on ? g.fin.tex : 0 );
	r->fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni1i( "cellAo", g.unit );
	prog->uni1b( "cellAoOn", on );
	/* lane AODECAL1: the decal target multiplies the probe term (cellGiE). Opaque draws only, like the obscurance:
	 * a blended draw is not in the opaque pass the target was made over. */
	const bool decalOn = g.ready && g.doc == scene->nifModel && g.decal && g.decalUnit >= 0 && ( !wwDrawBlend() || decalProg );
	{
		const int du = g.decalUnit >= 0 ? g.decalUnit : g.unit;
		r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + du ) );
		if ( g.decalUnit >= 0 )
			r->fn->glBindTexture( GL_TEXTURE_2D, decalOn ? g.decal : 0 );
		r->fn->glActiveTexture( GLenum( prevActive ) );
		prog->uni1i( "cellAoDecal", du );
		prog->uni1b( "cellAoDecalOn", decalOn );
		if ( decalOn ) {
			int dvp[4];
			wwDrawViewport( dvp );
			prog->uni4f_l( prog->uniLocation( "cellAoDecalRect" ), FloatVector4( float( dvp[0] ), float( dvp[1] ), 0.0f, 0.0f ) );
		}
	}
	// lane GAPLOOK1 (C8): the lamps' contact march reads the shared opaque pass (opaque draws, perspective views only)
	if ( prog->uniLocation( "cellCtOn" ) >= 0 ) {
		const auto & pm = r->globalUniforms->projectionMatrix;
		const bool ctOn = wwLookOn( WwLook::ContactLocal ) && g.ready && g.doc == scene->nifModel && g.gbuf.tex
			&& ( !wwDrawBlend() || decalProg ) && pm[3][3] != 1.0f;
		r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::ContactGbuf ) );
		r->fn->glBindTexture( GL_TEXTURE_2D, ctOn ? g.gbuf.tex : 0 );
		r->fn->glActiveTexture( GLenum( prevActive ) );
		prog->uni1i( "cellCtGbuf", WwCellUnit::ContactGbuf );
		prog->uni1b( "cellCtOn", ctOn );
		if ( ctOn ) {
			prog->uni4f( "cellCtProj", FloatVector4( pm[0][0], pm[1][1], pm[2][0], pm[2][1] ) );
			prog->uni4f( "cellCtRect", FloatVector4( 0.0f, 0.0f, float( g.gbuf.w ), float( g.gbuf.h ) ) );
			prog->uni1i( "cellCtRed", wwEnvStr( "WW_CELL_CONTACT_LOCAL_RED" ) == QLatin1String( "away" ) ? 1 : 0 );	// red: march away from the light
		}
	}
	/* lane PERF1: the half-res GI, read by the opaque draws (and the decals on them), like the obscurance; the
	 * upsample's keys send any pixel that is not the g-buffer's surface to the full chain */
	if ( prog->uniLocation( "cellGiHalfOn" ) >= 0 ) {
		// a probe picture (WW_CELL_LIT_PROBE) reads the volumes' mask at its own pixel instead: the screen pass's read
		const bool probePic = ( st().measuring ? 6 : st().probe ) != 0;
		const bool giOn = g.giReady && g.doc == scene->nifModel && ( wwCellGiRate() == 0 || wwCellGiRate() == 1 ) && wwCellGiVis() == 0
			&& ( !wwDrawBlend() || decalProg ) && ( !probePic || g.volOn );
		if ( g.giUnits ) {
			r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiHalf ) );
			r->fn->glBindTexture( GL_TEXTURE_2D, giOn ? ( probePic ? g.volMask.tex : g.giHalf.tex ) : 0 );
			r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::GiHalfKey ) );
			r->fn->glBindTexture( GL_TEXTURE_2D, giOn ? g.giKey : 0 );
			r->fn->glActiveTexture( GLenum( prevActive ) );
		}
		prog->uni1b( "cellGiHalfOn", giOn );
		if ( giOn ) {
			int vp[4];
			wwDrawViewport( vp );
			prog->uni4f_l( prog->uniLocation( "cellGiHalfRect" ), FloatVector4( float( vp[0] ), float( vp[1] ),
				float( g.giHalf.w ), float( g.giHalf.h ) ) );
			prog->uni1f( "cellGiHalfScale", 1.0f / float( g.giStep ) );
		}
	}
	if ( on ) {
		int vp[4];
		wwDrawViewport( vp );
		prog->uni4f_l( prog->uniLocation( "cellAoRect" ), FloatVector4( float( vp[0] ), float( vp[1] ),
			1.0f / float( std::max( vp[2], 1 ) ), 1.0f / float( std::max( vp[3], 1 ) ) ) );
	}
}

bool wwCellAoInPass()
{
	return ao().pass;
}
