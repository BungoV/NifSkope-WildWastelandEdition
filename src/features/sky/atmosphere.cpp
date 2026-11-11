#include "features/sky/atmosphere.h"
#include "features/sky/atmomath.h"
#include "features/weather/weatherrecx.h"	// lane WX1: the weather .recx atmosphere block
#include "features/weather/weatherrecxmath.h"	// SHAFTS1b: hazeFromVisibility for the clear-day aerosol
#include "features/clouds/cloudmath.h"	// SHAFTS1b: the WTHR class flags
#include "gl/lookdevstage.h"	// SHAFTS1b: the Lookdev weather's class
#include "features/sky/skydisc.h"
#include "features/sky/moonlight.h"	// lane FIX1101A (moon): the disc's gain through the air
#include "features/exposure/physunits.h"	// lane PHYSUNITS1: real units
#include "features/air/cellvolair.h"	// lane FIX1101A (moon): the air's medium for that gain
#include "wwswitch.h"
#include "gl/cellunits.h"
#include "gl/celluniform.h"	// lane CELLUNI3: the units bound without a unit ask
#include "gl/glscene.h"
#include "gl/renderer.h"
#include "harnesswindow.h"

#include <QHash>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

// lane ATMO1: the physical sky (atmosphere.h). The constants are Hillaire 2020's Earth (tests/features/sky/atmo_model.py).

namespace
{

using namespace WwAtmoMath;	// lane SKYA: the constants and the CPU math, shared with the offline unit test

/* lane WEATHERRECX1 (bungo 11-06: light comes only from the physical sky and its scattering): the Physical Sky (Ultra),
 * Aerial Perspective, Sunlight From Sky (Full), Moon Phase and Stars and GI Sky rows ship ON, under new keys so an old
 * stored OFF does not stick. Red WW_WEATHER_RECORD_RED: sky = the Weather Tint sky colour, sun = Sunlight From Sky
 * off, ambient = GI Sky off (the record's painted light, as before) */
constexpr const char * kKey = "Settings/Render/Scene/Physical Sky 2";
constexpr const char * kApKey = "Settings/Render/Scene/Aerial Perspective 2";
constexpr const char * kSunKey = "Settings/Render/Scene/Sunlight From Sky 2";	// lane SKYA (ATMOSUN1)
constexpr const char * kNightKey = "Settings/Render/Scene/Moon Phase and Stars 2";	// lane SKYA (NIGHT1)
constexpr int kTierDef = 4, kApDef = 1, kSunDef = 2, kNightDef = 1, kGiDef = 1;	// lane WEATHERRECX1
constexpr const char * kExpoKey = "Settings/Render/Scene/Exposure From Sky";	// lane SKYA (EXPO1)
constexpr const char * kTintKey = "Settings/Render/Scene/Sky Color";	// lane SKYA (ATMOCHECK1)
constexpr const char * kGiKey = "Settings/Render/Scene/GI Sky From Physical Sky 2";	// lane SKYA (SKYGI1)
constexpr const char * kBodyKey = "Settings/Render/Scene/Sun and Moon Size";	// MODULES1 R6-12 b
constexpr int kBodyDef = 1;	// Medium (MODULES1 round 10; it was Game, now step 2)
static_assert( kTintN == 17, "WwAtmoInputs::tint and atmo_sky.frag atTint[17]" );
constexpr GLenum kTimestamp = 0x8E28;	// GL_TIMESTAMP
constexpr int kTransW = 256, kTransH = 64;
/* lane SKYA (ATMO2): the aerial-perspective volume's reach. A Fallout 4 worldspace is ~4 km across (64 cells of
 * 58.5 m) and its far LOD a few km more: 16 km covers it with slice 0's centre at 4 m (quadratic slices) */
constexpr float kApMaxKm = 16.0f;
// the disc's display cap (scene units): bloom (post) still sees a bright disc, never an unbounded one
constexpr float kDiscCap = 48.0f;

struct AtState
{
	bool init = false;
	quint64 liveGen = 0;
	int tier = 0;
	bool pinned = false;
	float haze[3] = { 0.0f, 1.0f, 0.0f };	// last drawn: ground extinction /km, scale height km, plateau top km
	float discAirLast[2] = { 1.0f, 1.0f };	// lane FIX1101A (moon): the sun disc's and our globe's gain through the air
	QString last = QStringLiteral( "off" );
	int ap = 0;	// lane SKYA (ATMO2): the Aerial Perspective row, 0 Off / 1 On (pin WW_ATMO_AP)
	bool apPinned = false;
	int sun = 0;	// lane SKYA (ATMOSUN1): the Sunlight From Sky row, 0 Off / 1 Tint / 2 Full (pin WW_ATMO_SUN)
	bool sunPinned = false;
	int night = 0;	// lane SKYA (NIGHT1): the Moon Phase and Stars row, 0 Off / 1 On (pin WW_ATMO_NIGHT)
	bool nightPinned = false;
	float extHaze[4] = { -1.0f, -1.0f, -1.0f, -1.0f };	// NIGHT1: the haze (+ red) the extinction table was built for
	float ext[33][3] = {};	// T(mu) / T(1) at mu = 0, 1/32 .. 1
	int expo = 0;	// lane SKYA (EXPO1): the Exposure From Sky row, 0 Off / 1 On (pin WW_ATMO_EXPO)
	bool expoPinned = false;
	float skyMean = -1.0f;	// EXPO1: the sky's mean luminance over the upper hemisphere, scene units (< 0 = none)
	quint64 frame = 0, skyFrame = 0;	// EXPO1: the frame count (wwAtmoApFrame) and the frame skyMean was read in
	int tint = 0;	// lane SKYA (ATMOCHECK1): the Sky Color row, 0 Physical / 1 Weather Tint (pin WW_ATMO_TINT)
	bool tintPinned = false;
	bool tintDrawn = false;	// SKY2B (SKYTINT2): the last sky drew Weather Tint (the dome resolved): the ambient keeps the weather's color
	bool tintEnv = false;	// LATE 7: WW_ATMO_TINT is set (a pin keeps its value even under a glowing sky)
	bool glowSky = false;	// LATE 7: this hour's weather sky is one the physical model cannot make (wwAtmoSetGlowSky)
	int gi = 0;	// lane SKYA (SKYGI1): the GI Sky From Physical Sky row, 0 Off / 1 On (pin WW_ATMO_GI)
	bool giPinned = false;
	int bodySize = 0;	// MODULES1 R6-12 b: the Sun and Moon Size row (pin WW_ATMO_BODYSIZE)
	bool bodySizePinned = false;
	float gameSun = 0.0f, gameMoon = 0.0f;	// the game's own discs, tan of the half-angle (0 = not handed yet)
};

//! lane SKYA: one row's value from its pin (already read by the caller: the survey needs literal names), a harness
//! run (0, pinned) or its QSettings key
int rowValue( const QString & pin, const char * key, const QStringList & names, bool & pinned, int def = 0 )
{
	const QString e = pin.trimmed().toLower();
	pinned = true;
	if ( !e.isEmpty() ) {
		bool ok = false;
		const int v = e.toInt( &ok );
		if ( ok )
			return std::clamp( v, 0, int( names.size() ) - 1 );
		const int i = int( names.indexOf( e ) );
		return i >= 0 ? i : 0;
	}
	if ( wwHarnessRun() )
		return 0;
	pinned = false;
	return std::clamp( QSettings().value( QLatin1StringView( key ), def ).toInt(), 0, int( names.size() ) - 1 );
}

AtState & as()
{
	static AtState s;
	if ( !s.init || wwSwitchMovedFor( s.liveGen, { "WW_ATMO" } ) ) {
		s.init = true;
		s.liveGen = wwSwitchGen();
		s.pinned = false;
		const QString e = wwEnvStr( "WW_ATMO" ).trimmed().toLower();
		if ( !e.isEmpty() ) {
			bool ok = false;
			const int v = e.toInt( &ok );
			s.tier = ok ? std::clamp( v, 0, 4 )
				: e == QLatin1StringView( "low" ) ? 1 : e == QLatin1StringView( "medium" ) ? 2
				: e == QLatin1StringView( "high" ) ? 3 : e == QLatin1StringView( "ultra" ) ? 4 : 0;
			s.pinned = true;
		} else if ( wwHarnessRun() ) {
			s.tier = 0;	// a harness measures what it pins
			s.pinned = true;
		} else {
			s.tier = std::clamp( QSettings().value( QLatin1StringView( kKey ), kTierDef ).toInt(), 0, 4 );
		}
		// lane SKYA (ATMO2): the Aerial Perspective row (the prefix above re-reads it live)
		const QString a = wwEnvStr( "WW_ATMO_AP" ).trimmed().toLower();
		s.apPinned = true;
		if ( !a.isEmpty() )
			s.ap = ( a == QLatin1StringView( "1" ) || a == QLatin1StringView( "on" ) ) ? 1 : 0;
		else if ( wwHarnessRun() )
			s.ap = 0;
		else {
			s.apPinned = false;
			s.ap = QSettings().value( QLatin1StringView( kApKey ), kApDef ).toInt() ? 1 : 0;
		}
		// lane SKYA: the rows after ATMO2 (the same prefix re-reads them live)
		s.sun = rowValue( wwEnvStr( "WW_ATMO_SUN" ), kSunKey, { "off", "tint", "full" }, s.sunPinned, kSunDef );
		s.night = rowValue( wwEnvStr( "WW_ATMO_NIGHT" ), kNightKey, { "off", "on" }, s.nightPinned, kNightDef );
		s.expo = rowValue( wwEnvStr( "WW_ATMO_EXPO" ), kExpoKey, { "off", "on" }, s.expoPinned );
		s.tint = rowValue( wwEnvStr( "WW_ATMO_TINT" ), kTintKey, { "physical", "weather" }, s.tintPinned );
		s.tintEnv = !wwEnvStr( "WW_ATMO_TINT" ).trimmed().isEmpty();
		s.gi = rowValue( wwEnvStr( "WW_ATMO_GI" ), kGiKey, { "off", "on" }, s.giPinned, kGiDef );
		s.bodySize = rowValue( wwEnvStr( "WW_ATMO_BODYSIZE" ), kBodyKey, { "physical", "medium", "game", "large" }, s.bodySizePinned, kBodyDef );
		// lane WEATHERRECX1: the record reds hand a light channel back to the WTHR record (an unpinned row only)
		const unsigned rr = wwWeatherRecordChannels( wwEnvStr( "WW_WEATHER_RECORD_RED" ) );
		if ( ( rr & WwRecSky ) && !s.tintPinned )
			s.tint = 1;
		if ( ( rr & WwRecSun ) && !s.sunPinned )
			s.sun = 0;
		if ( ( rr & WwRecAmbient ) && !s.giPinned )
			s.gi = 0;
	}
	return s;
}

bool red( const char * word )
{
	return wwEnvStr( "WW_ATMO_RED" ).trimmed().toLower() == QLatin1StringView( word );
}

/* LATE 7 (bungo: the radstorm is not green): the Sky Color the frame draws. A glowing sky (the radstorms, the
 * Glowing Sea: wwAtmoSetGlowSky) takes Weather Tint whatever the row says, so the record's sky, air and ambient
 * colors are drawn, not the physical blue; a WW_ATMO_TINT pin keeps its value. Red WW_ATMO_TINT_RED=noglow. */
int tintNow( const AtState & s )
{
	if ( s.tint > 0 )
		return s.tint;
	const bool glow = s.glowSky && !s.tintEnv
		&& wwEnvStr( "WW_ATMO_TINT_RED" ).trimmed().toLower() != QLatin1StringView( "noglow" );
	return glow ? 1 : 0;
}

//! lane SKYA (ATMO2): the aerial perspective's reds: nosunt (the in-scatter ignores the sun's transmittance), double
//! (the cell fog laid over the full aerial perspective: the haze counted twice)
bool apRed( const char * word )
{
	return wwEnvStr( "WW_ATMO_AP_RED" ).trimmed().toLower() == QLatin1StringView( word );
}

/* SHAFTS1b (manager ruling 11-03, bungo's c3 haze): the fog record is the game's art fog, not a measurement. A clear
 * weather (WTHR class Pleasant only, no precipitation) takes the haze rung from a physical aerosol instead: clear
 * continental noon, 30 km visibility (Mie extinction ~0.11 /km over the clear air's 0.018). A weather .recx
 * atmosphere.visibility (metres, recx/SPEC.md) still wins; every other weather keeps the record's fog. Red
 * WW_CELL_AIR_RED=recordhaze: the record for every weather (today's rule). */
constexpr float kClearDayVisM = 30000.0f;
const char * g_hazeSrc = "record";
float g_airglow = 1.0f;	// lane WEATHERRECX1: atmosphere.airglow (the hazeType's; 1 = the night floor as before)
WwNightSky g_night;	// MODULES1 round 10: the one night sky's factor (atmosphere.h)
float g_nsUnit[WwNsCount] = {}, g_nsLevel[WwNsCount] = {};

void clearDayHaze( float haze[3], bool recordHasFog )
{
	g_hazeSrc = "record";
	const bool recordRed = wwEnvStr( "WW_CELL_AIR_RED" ).toLower().split( QLatin1Char( ',' ) ).contains( QStringLiteral( "recordhaze" ) );
	unsigned f = 0, th = 0;
	int precip = 0;
	if ( !recordRed && wwLookdevWeatherClass( f, th, nullptr, &precip ) && ( f & WwCloudMath::kWxPleasant )
		&& !( f & ( WwCloudMath::kWxCloudy | WwCloudMath::kWxRainy | WwCloudMath::kWxSnow ) ) && precip == 0 ) {
		haze[0] = WwWeatherRecxMath::hazeFromVisibility( kClearDayVisM, kRay[1] + kMieE );
		g_hazeSrc = "clearday";
	}
	wwWeatherRecxHaze( haze, recordHasFog, kRay[1] + kMieE );
	const WwWeatherRecx & r = wwWeatherRecx();
	if ( r.on && r.visibilityM >= 0.0f && !( r.recordFirst && recordHasFog ) )
		g_hazeSrc = "recx";
	/* lane WEATHERRECX1: the haze's make-up from the .recx (hazeType at its humidity; SPEC.md 1.3): albedo per channel,
	 * the Angstrom slope on the extinction (550 nm unchanged: visibility stays the only thickness), g, airglow. None
	 * (row off, red fog): the grey haze before 1.3 */
	HazeOptics & ho = hazeOptics();
	ho = HazeOptics();
	g_airglow = 1.0f;
	float alb[3], ang = 0.0f, g = kMieG, glow = 1.0f;
	if ( wwWeatherRecxHazeOptics( alb, ang, g, glow ) ) {
		static const float kLambda[3] = { 680.0f, 550.0f, 440.0f };
		for ( int c = 0; c < 3; c++ ) {
			ho.albedo[c] = std::clamp( alb[c], 0.0f, 1.0f );
			ho.spec[c] = std::pow( kLambda[c] / 550.0f, -ang );
		}
		ho.g = std::clamp( g, 0.0f, 0.95f );
		g_airglow = std::clamp( glow, 0.0f, 10.0f );
	}
}

//! lane SKYA (NIGHT1): nophase (the moon's light at every phase = the full moon's), noext (the stars and the moon disc
//! not dimmed toward the horizon)
bool nightRed( const char * word )
{
	return wwEnvStr( "WW_ATMO_NIGHT_RED" ).trimmed().toLower() == QLatin1StringView( word );
}

//! lane SKYA (EXPO1): skyzero (the sky counted as nothing), zenithw (the hemisphere mean not weighted by solid angle)
bool expoRed( const char * word )
{
	return wwEnvStr( "WW_ATMO_EXPO_RED" ).trimmed().toLower() == QLatin1StringView( word );
}

struct Target
{
	GLuint tex = 0, fbo = 0;
	int w = 0, h = 0;
};

struct Volume	// lane SKYA (ATMO2): the aerial-perspective froxels, RGBA16F 3D, one FBO a slice at a time
{
	GLuint tex = 0, fbo = 0;
	int w = 0, h = 0, n = 0;
	bool valid = false;	// drawn this frame (wwAtmoApFrame clears it)
	bool layered = false;	// lane PERFFIX3: the last build was one layered draw (false: a draw a slice)
	bool layeredRefused = false;	// lane PERFFIX3: the layered target was incomplete once: a draw a slice from then on
	float view[9] = {};	// the view rotation and projection it was built for: other views (probes) read none
	float proj[4] = {};	// P00, P11, P20, P21
	float kmPerView = 0.0f;
	float camR = 0.0f;	// km from the planet's centre (the sky's air reads the transmittance LUT from here)
};

struct AtGpu
{
	Target trans, ms, sunView, moonView;
	Volume ap;
	QString transKey, msKey;
	GLuint q[4] = { 0, 0, 0, 0 };	// two timestamp pairs, read a frame late
	bool pending[2] = { false, false };
	int cur = 0;
	double ms_ = -1.0;
	int frames = 0;
	// lane SKYA (EXPO1): the sun's and the moon's sky-view tables read back a frame late (no stall), with the scales
	// and the size they were drawn with
	GLuint pbo[2] = { 0, 0 };
	GLsync fence = nullptr;
	int rbW = 0, rbH = 0;
	float rbSunK = 0.0f, rbMoonK = 0.0f, rbCamR = 0.0f;
	float rbLight[2][3] = {};	// SKY2B (EXPO2): the sun's and the moon's directions the queued tables were drawn for
	// EXPO2: CPU copies of the last read-back tables, so the measure can look up the sky pixels actually in view
	std::vector<float> tab[2];
	int tabW = 0, tabH = 0;
	float tabK[2] = { 0.0f, 0.0f }, tabCamR = 0.0f, tabLight[2][3] = {};
	quint64 tabFrame = 0;
};

QHash<Renderer *, AtGpu> & gpus()
{
	static QHash<Renderer *, AtGpu> g;
	return g;
}

AtGpu * g_last = nullptr;
WwAtmoFrame g_frame;	// lane CLOUD2

bool ensure( NifSkopeOpenGLContext::GLFunctions * fn, Target & t, int w, int h )
{
	if ( t.tex && t.w == w && t.h == h )
		return true;
	if ( !t.fbo )
		fn->glGenFramebuffers( 1, &t.fbo );
	if ( t.tex )
		fn->glDeleteTextures( 1, &t.tex );
	fn->glGenTextures( 1, &t.tex );
	fn->glBindTexture( GL_TEXTURE_2D, t.tex );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, t.fbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0 );
	const GLenum one = GL_COLOR_ATTACHMENT0;
	fn->glDrawBuffers( 1, &one );
	t.w = w;
	t.h = h;
	return fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
}

void quad( Renderer * r )
{
	static const float q[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = q;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
}

float g_ozone = 1.0f;	// lane WX1: atmosphere.ozone of the drawn sky (1 = the record's air, exact)

void medium( NifSkopeOpenGLContext::Program * p, const float haze[3], bool noOzone )
{
	p->uni3f( "atRay", kRay[0], kRay[1], kRay[2] );
	p->uni1f( "atMieS", kMieS );
	p->uni1f( "atMieE", kMieE );
	const HazeOptics & ho = hazeOptics();	// lane WEATHERRECX1: the weather .recx haze (clearDayHaze sets it)
	p->uni1f( "atG", ho.g );
	p->uni3f( "atHazeAlbedo", ho.albedo[0], ho.albedo[1], ho.albedo[2] );
	p->uni3f( "atHazeSpec", ho.spec[0], ho.spec[1], ho.spec[2] );
	if ( noOzone )
		p->uni3f( "atOzone", 0.0f, 0.0f, 0.0f );
	else
		p->uni3f( "atOzone", kOzone[0] * g_ozone, kOzone[1] * g_ozone, kOzone[2] * g_ozone );
	p->uni3f( "atHaze", haze[0], haze[1], haze[2] );
	// lane FIX1101A (atmo): the MS LUT's sqrt height rows + 40 packed steps (red msold: linear rows, 20 uniform), read
	// and written by every pass that takes this medium; the night floor (red nofloor: none), read by atmo_sky only
	p->uni1f( "atMsOld", red( "msold" ) ? 1.0f : 0.0f );
	const float nsF = g_night.floorGain;	// MODULES1 round 10: the night sky's factor on the floor
	if ( red( "nofloor" ) )
		p->uni3f( "atFloor", 0.0f, 0.0f, 0.0f );
	else if ( g_airglow <= 1.0f || red( "glowgrey" ) )
		p->uni3f( "atFloor", kNightFloor[0] * g_airglow * nsF, kNightFloor[1] * g_airglow * nsF, kNightFloor[2] * g_airglow * nsF );	// WEATHERRECX1: x airglow
	else {	// MODULES1: the natural floor + the excess as the 557.7 nm green line (atmomath.h kGreenLine5577)
		const float ex = kNightFloorLum * ( g_airglow - 1.0f );
		p->uni3f( "atFloor", ( kNightFloor[0] + ex * kGreenLine5577[0] ) * nsF, ( kNightFloor[1] + ex * kGreenLine5577[1] ) * nsF,
			( kNightFloor[2] + ex * kGreenLine5577[2] ) * nsF );
	}
}

void bindTex( NifSkopeOpenGLContext::GLFunctions * fn, NifSkopeOpenGLContext::Program * p, int unit, const char * name, GLuint tex )
{
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_2D, tex );
	p->uni1i( name, unit );
}

// the haze rung, the CPU transmittance and lum(): src/features/sky/atmomath.h (moved there by lane SKYA, unchanged)

void norm( float v[3] )
{
	const float l = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	for ( int c = 0; c < 3; c++ )
		v[c] = l > 1e-9f ? v[c] / l : ( c == 2 ? 1.0f : 0.0f );
}

void timeRead( NifSkopeOpenGLContext::GLFunctions * fn, AtGpu & g, int slot, bool wait )
{
	if ( !g.pending[slot] )
		return;
	if ( !wait ) {
		GLuint avail = 0;
		fn->glGetQueryObjectuiv( g.q[slot * 2 + 1], GL_QUERY_RESULT_AVAILABLE, &avail );
		if ( !avail )
			return;
	}
	GLuint64 a = 0, b = 0;
	fn->glGetQueryObjectui64v( g.q[slot * 2], GL_QUERY_RESULT, &a );
	fn->glGetQueryObjectui64v( g.q[slot * 2 + 1], GL_QUERY_RESULT, &b );
	g.ms_ = b > a ? double( b - a ) * 1e-6 : 0.0;
	g.pending[slot] = false;
	if ( ++g.frames % 120 == 1 )
		fprintf( stderr, "atmo: gpu %.4f ms\n", g.ms_ );
}

const char * tierName( int t )
{
	static const char * n[5] = { "Off", "Low", "Medium", "High", "Ultra" };
	return n[std::clamp( t, 0, 4 )];
}

/* lane SKYA (ATMO2): the aerial-perspective volume (atmo_ap.frag) over the current view's frustum, every frame (the
 * camera and the sun move). Froxels per tier: Low 16x16x16, Medium 32x32x16, High 32x32x32, Ultra 64x64x32. The
 * caller has saved the framebuffer, the viewport and the state; the transmittance and MS LUTs are this frame's. */
bool buildAp( Scene * scene, AtGpu & g, int msN, float camR, const float sunW[3], const float moonW[3], float sunK,
	float moonK, bool noOzone, bool noMs, const float ( *tint )[3] )
{
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	const AtState & s = as();
	const auto & gu = *r->globalUniforms;
	if ( gu.projectionMatrix[3][3] == 1.0f )
		return false;	// orthographic: no frustum to slice
	const int w = s.tier >= 4 ? 64 : s.tier >= 2 ? 32 : 16;
	const int n = s.tier >= 3 ? 32 : 16;
	Volume & v = g.ap;
	if ( !v.tex || v.w != w || v.h != w || v.n != n ) {
		if ( !v.fbo )
			fn->glGenFramebuffers( 1, &v.fbo );
		if ( v.tex )
			fn->glDeleteTextures( 1, &v.tex );
		fn->glGenTextures( 1, &v.tex );
		fn->glBindTexture( GL_TEXTURE_3D, v.tex );
		fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA16F, w, w, n, 0, GL_RGBA, GL_FLOAT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0 );
		fn->glBindTexture( GL_TEXTURE_3D, 0 );
		v.w = v.h = w;
		v.n = n;
	}
	/* lane PERFFIX3 (item 1): every slice in ONE layered draw (atmo_apl.prog: gl_Layer from the geometry shader, the
	 * whole 3D texture attached layered), not a draw and a glFramebufferTextureLayer re-attach per slice. Same maths per
	 * froxel (the slice index arrives as a flat input instead of a uniform). Red WW_ATMO_AP_RED=slices: a draw a slice;
	 * a refused layered program or target falls back to it too. */
	const bool layeredWanted = !apRed( "slices" ) && !v.layeredRefused;
	auto p = layeredWanted ? r->useProgram( "atmo_apl.prog" ) : nullptr;
	const bool layered = p != nullptr;
	if ( !p )
		p = r->useProgram( "atmo_ap.prog" );
	if ( !p )
		return false;
	v.layered = layered;
	// the frustum's world rays (atmo_sky.frag's: view ray ((x + P20) / P00, (y + P21) / P11, -1), world = vr x viewMatrix)
	const float P00 = gu.projectionMatrix[0][0], P11 = gu.projectionMatrix[1][1];
	const float P20 = gu.projectionMatrix[2][0], P21 = gu.projectionMatrix[2][1];
	auto ray = [&]( float x, float y, float o[3] ) {
		const float vr[3] = { ( x + P20 ) / P00, ( y + P21 ) / P11, -1.0f };
		for ( int c = 0; c < 3; c++ )
			o[c] = vr[0] * gu.viewMatrix[c][0] + vr[1] * gu.viewMatrix[c][1] + vr[2] * gu.viewMatrix[c][2];
	};
	float f0[3], fx[3], fy[3];
	ray( -1.0f, -1.0f, f0 );
	ray( 1.0f, -1.0f, fx );
	ray( -1.0f, 1.0f, fy );
	for ( int c = 0; c < 3; c++ ) {
		fx[c] -= f0[c];
		fy[c] -= f0[c];
	}
	fn->glBindFramebuffer( GL_FRAMEBUFFER, v.fbo );
	glViewport( 0, 0, w, w );
	medium( p, s.haze, noOzone );
	p->uni2f( "atMsSize", float( msN ), float( msN ) );
	p->uni3f( "atApDims", float( w ), float( w ), float( n ) );
	p->uni1f( "atApMaxKm", kApMaxKm );
	p->uni3f( "atFrustum0", f0[0], f0[1], f0[2] );
	p->uni3f( "atFrustumX", fx[0], fx[1], fx[2] );
	p->uni3f( "atFrustumY", fy[0], fy[1], fy[2] );
	p->uni1f( "atCamR", camR );
	p->uni3f( "atSunW", sunW[0], sunW[1], sunW[2] );
	p->uni3f( "atMoonW", moonW[0], moonW[1], moonW[2] );
	p->uni1f( "atSunK", sunK );
	p->uni1f( "atMoonK", moonK );
	p->uni1f( "atMsOn", noMs ? 0.0f : 1.0f );
	p->uni1f( "atApNoSunT", apRed( "nosunt" ) ? 1.0f : 0.0f );
	// SKY2B (SKYTINT2): under Weather Tint the haze in front of distant geometry takes the dome's color like the sky
	p->uni1f( "atTintOn", tint ? 1.0f : 0.0f );
	const GLint tintLoc = p->uniLocation( "atTint" );
	if ( tint && tintLoc >= 0 )
		fn->glUniform3fv( tintLoc, kTintN, &tint[0][0] );
	bindTex( fn, p, 0, "atTransTex", g.trans.tex );
	bindTex( fn, p, 1, "atMsTex", g.ms.tex );
	const GLenum one = GL_COLOR_ATTACHMENT0;
	bool ok = true;
	if ( layered ) {
		fn->glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, v.tex, 0 );
		fn->glDrawBuffers( 1, &one );
		ok = fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
		if ( ok ) {
			quad( r );	// atApDims.z slices, one geometry invocation each
		} else {
			v.layered = false;
			v.layeredRefused = true;	// this frame keeps no volume; the next builds a slice a draw
			fprintf( stderr, "atmo: the layered aerial-perspective target is incomplete; a draw a slice from now on\n" );
		}
	}
	for ( int k = 0; k < n && ok && !layered; k++ ) {
		fn->glFramebufferTextureLayer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, v.tex, 0, k );
		if ( k == 0 ) {
			fn->glDrawBuffers( 1, &one );
			ok = fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
			if ( !ok )
				break;
		}
		p->uni1f( "atApSlice", float( k ) );
		quad( r );
	}
	r->stopProgram();
	if ( !ok )
		return false;
	for ( int c = 0; c < 3; c++ )
		for ( int k = 0; k < 3; k++ )
			v.view[c * 3 + k] = gu.viewMatrix[c][k];
	v.proj[0] = P00;
	v.proj[1] = P11;
	v.proj[2] = P20;
	v.proj[3] = P21;
	const float sc = scene->view.scale != 0.0f ? scene->view.scale : 1.0f;
	v.kmPerView = kUnitKm / sc;
	v.camR = camR;
	v.valid = true;
	return true;
}

}	// namespace

int wwAtmoTier()
{
	return as().tier;
}

void wwAtmoSetTier( int tier )
{
	AtState & s = as();
	if ( s.pinned )
		return;
	s.tier = std::clamp( tier, 0, 4 );
	QSettings().setValue( QLatin1StringView( kKey ), s.tier );
}

bool wwAtmoPinned()
{
	return as().pinned;
}

bool wwAtmoDrawSky( Scene * scene, const WwAtmoInputs & in, QString * line )
{
	AtState & s = as();
	if ( s.tier <= 0 || !scene || !scene->renderer )
		return false;
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	AtGpu & g = gpus()[r];
	while ( fn->glGetError() != GL_NO_ERROR ) {
	}
	if ( !g.q[0] )
		fn->glGenQueries( 4, g.q );
	if ( !in.capture ) {	// SKYGI1: the probes' capture is not a frame (its six faces would scramble the timer pairs)
		timeRead( fn, g, 1 - g.cur, false );
		timeRead( fn, g, g.cur, true );
		fn->glQueryCounter( g.q[g.cur * 2], kTimestamp );
	}

	const int vw = s.tier == 1 ? 96 : s.tier == 4 ? 256 : 192;
	const int vh = s.tier == 1 ? 54 : s.tier == 4 ? 144 : 108;
	const int msN = s.tier == 1 ? 16 : 32;
	const bool noOzone = red( "noozone" ), noMs = red( "noms" );
	// lane SKY2B (SKYSTRIPE1): rays that end on the planet see its lit ground; the red "noground" brings back the black
	// earth (a dark band under the horizon from a high camera). The red "perpixel" marches every sky pixel (SKYBLUR1).
	const bool noGround = red( "noground" ), perPixel = red( "perpixel" );
	hazeFromFog( in.fogFar, in.fogMax, in.fogMid, in.fogRange, red( "nohaze" ), s.haze );
	// lane WX1: a weather .recx visibility / hazeHeight wins over the fog-derived haze (Weather .recx row)
	if ( !red( "nohaze" ) )
		clearDayHaze( s.haze, in.fogFar > 0.0f && in.fogMax > 0.0f );	// SHAFTS1b: clear day, then the .recx
	g_ozone = wwWeatherRecxOzone();
	const float hKm = std::clamp( in.camZ * kUnitKm, 0.002f, 80.0f );
	const float camR = kRb + hKm;
	float sunW[3] = { in.sunW[0], in.sunW[1], in.sunW[2] };
	float moonW[3] = { in.moonW[0], in.moonW[1], in.moonW[2] };
	norm( sunW );
	norm( moonW );
	// scene units: displayed = pi k L, k = the Day sunlight luminance / the 45-degree sun's transmittance
	float t45[3];
	cpuTrans( 0.0f, 0.70710678f, s.haze, noOzone, t45, g_ozone );
	float tl = std::max( 1e-4f, lum( t45 ) );
	/* MODULES1 part 23: the scale comes from a clear day's air at most. Dividing by the weather's own hazy T45 lifted a
	 * radstorm's sky and aerial perspective ~230x (white at noon) and the coast rad fog's ~4x (blown). Red
	 * WW_ATMO_RED=hazek: the weather's own T45 again. Weathers as clear as the clear day keep their scale. */
	if ( !red( "hazek" ) ) {
		const float ref[3] = { WwWeatherRecxMath::hazeFromVisibility( kClearDayVisM, kRay[1] + kMieE ), 1.2f, 0.0f };
		float tRef[3];
		cpuTrans( 0.0f, 0.70710678f, ref, noOzone, tRef, g_ozone );
		tl = std::max( tl, lum( tRef ) );
	}
	/* lane PHYSUNITS1: real units. in.sunLum is the top-of-atmosphere sun over pi (lookdevstage.cpp), the sky's LUTs are per
	 * unit sun irradiance, so sunK = E0 and nothing divides by the 45-degree transmittance; the ground's sun is E0 x T(mu). */
	const bool phys = wwPhysSkyOn();
	if ( phys )
		tl = 1.0f;
	const float sunK = kPi * in.sunLum / tl;
	/* MODULES1 round 10: the night sky's factor. By night the moon's unit is the light's own (lightK, the unit the RTXGI
	 * volumes and the ambient axes take), so the drawn sky and the volumes are one sky by construction; x gain. moonKey
	 * keeps the unlifted unit for the readers of the moon's direct light (the volume clouds' key). */
	const WwNightSky & ns = g_night;
	const bool nsOn = ns.mode == 0 && in.moonUp && ns.lightK > 0.0f;
	const float moonKey = !in.moonUp ? 0.0f
		: kPi * ( nsOn ? ( in.moonLum / tl ) * ( 1.0f - ns.share ) + ns.lightK * ns.share : in.moonLum / tl );
	const float moonK = moonKey * ( ns.mode == 0 ? ns.gain : 1.0f );
	wwAtmoNightSkyGot( in.capture ? WwNsProbes : WwNsDrawn, moonK / kPi, moonK / kPi );

	GLint prevDraw = 0, prevRead = 0, prevActive = 0, vp[4] = { 0, 0, 1, 1 };
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasBlend = fn->glIsEnabled( GL_BLEND );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	const bool wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	GLboolean depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDepthMask( GL_FALSE );

	QString why;
	bool ok = ensure( fn, g.trans, kTransW, kTransH ) && ensure( fn, g.ms, msN, msN ) && ensure( fn, g.sunView, vw, vh )
		&& ensure( fn, g.moonView, vw, vh );
	if ( !ok )
		why = QStringLiteral( "a LUT target is incomplete" );
	const QString hazeKey = QString( "%1,%2,%3,%4" ).arg( double( s.haze[0] ), 0, 'g', 6 ).arg( double( s.haze[1] ), 0, 'g', 6 )
		.arg( double( s.haze[2] ), 0, 'g', 6 ).arg( noOzone )
		+ ( g_ozone != 1.0f ? QStringLiteral( ",oz%1" ).arg( double( g_ozone ) ) : QString() )
		+ ( red( "msold" ) ? QStringLiteral( ",msold" ) : QString() );	// lane FIX1101A (atmo): the red's LUT is another table
	// 1. transmittance and multiple scattering: only when the medium moves (weather, hour through the fog keys)
	if ( ok && g.transKey != hazeKey ) {
		auto p = r->useProgram( "atmo_transmittance.prog" );
		ok = p != nullptr;
		if ( p ) {
			fn->glBindFramebuffer( GL_FRAMEBUFFER, g.trans.fbo );
			glViewport( 0, 0, kTransW, kTransH );
			medium( p, s.haze, noOzone );
			quad( r );
			r->stopProgram();
			g.transKey = hazeKey;
			g.msKey.clear();
		} else {
			why = QStringLiteral( "atmo_transmittance.prog did not link" );
		}
	}
	const QString msKey = hazeKey + QString( "|%1" ).arg( msN );
	if ( ok && g.msKey != msKey ) {
		auto p = r->useProgram( "atmo_ms.prog" );
		ok = p != nullptr;
		if ( p ) {
			fn->glBindFramebuffer( GL_FRAMEBUFFER, g.ms.fbo );
			glViewport( 0, 0, msN, msN );
			medium( p, s.haze, noOzone );
			p->uni2f( "atMsSize", float( msN ), float( msN ) );
			bindTex( fn, p, 0, "atTransTex", g.trans.tex );
			quad( r );
			r->stopProgram();
			g.msKey = msKey;
		} else {
			why = QStringLiteral( "atmo_ms.prog did not link" );
		}
	}
	// 2. the sky-view LUTs, every frame (the sun moves): the sun's, and the moon's while it is up
	auto view = [&]( Target & t, const float l[3] ) {
		auto p = r->useProgram( "atmo_skyview.prog" );
		if ( !p )
			return false;
		fn->glBindFramebuffer( GL_FRAMEBUFFER, t.fbo );
		glViewport( 0, 0, vw, vh );
		medium( p, s.haze, noOzone );
		p->uni2f( "atMsSize", float( msN ), float( msN ) );
		p->uni2f( "atViewSize", float( vw ), float( vh ) );
		p->uni1f( "atCamR", camR );
		p->uni1f( "atLightMu", l[2] );
		p->uni1f( "atMsOn", noMs ? 0.0f : 1.0f );
		p->uni1f( "atGround", noGround ? 0.0f : 1.0f );
		bindTex( fn, p, 0, "atTransTex", g.trans.tex );
		bindTex( fn, p, 1, "atMsTex", g.ms.tex );
		quad( r );
		r->stopProgram();
		return true;
	};
	if ( ok && !view( g.sunView, sunW ) ) {
		ok = false;
		why = QStringLiteral( "atmo_skyview.prog did not link" );
	}
	if ( ok && moonK > 0.0f )
		view( g.moonView, moonW );
	// 2a. lane SKYA (EXPO1): the last frame's read-back tables give the sky's mean; this frame's are queued
	if ( g.fence && !in.capture ) {
		fn->glClientWaitSync( g.fence, GL_SYNC_FLUSH_COMMANDS_BIT, GLuint64( 1000000000 ) );
		fn->glDeleteSync( g.fence );
		g.fence = nullptr;
		const GLsizeiptr bytes = GLsizeiptr( size_t( g.rbW ) * size_t( g.rbH ) * 4 * sizeof( float ) );
		float mean = 0.0f;
		bool got = true;
		for ( int t = 0; t < ( g.rbMoonK > 0.0f ? 2 : 1 ); t++ ) {
			fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, g.pbo[t] );
			const auto * px = static_cast<const float *>( fn->glMapBufferRange( GL_PIXEL_PACK_BUFFER, 0, bytes, GL_MAP_READ_BIT ) );
			if ( px ) {
				mean += ( t ? g.rbMoonK : g.rbSunK ) * skyViewMeanLum( px, g.rbW, g.rbH, g.rbCamR, expoRed( "zenithw" ) );
				g.tab[t].assign( px, px + size_t( g.rbW ) * size_t( g.rbH ) * 4 );
			}
			got = got && px;
			fn->glUnmapBuffer( GL_PIXEL_PACK_BUFFER );
		}
		fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
		if ( got ) {
			s.skyMean = mean;
			s.skyFrame = s.frame;
			g.tabW = g.rbW;
			g.tabH = g.rbH;
			g.tabCamR = g.rbCamR;
			g.tabK[0] = g.rbSunK;
			g.tabK[1] = g.rbMoonK;
			std::copy( &g.rbLight[0][0], &g.rbLight[0][0] + 6, &g.tabLight[0][0] );
			g.tabFrame = s.frame;
		}
	}
	if ( ok && s.expo > 0 && !in.capture ) {
		if ( !g.pbo[0] )
			fn->glGenBuffers( 2, g.pbo );
		GLint prevTex = 0;
		fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prevTex );
		fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
		for ( int t = 0; t < ( moonK > 0.0f ? 2 : 1 ); t++ ) {
			fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, g.pbo[t] );
			fn->glBufferData( GL_PIXEL_PACK_BUFFER, GLsizeiptr( size_t( vw ) * size_t( vh ) * 4 * sizeof( float ) ), nullptr,
				GL_STREAM_READ );
			fn->glBindTexture( GL_TEXTURE_2D, t ? g.moonView.tex : g.sunView.tex );
			fn->glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, nullptr );
		}
		fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
		fn->glBindTexture( GL_TEXTURE_2D, GLuint( prevTex ) );
		g.fence = fn->glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
		g.rbW = vw;
		g.rbH = vh;
		g.rbSunK = sunK;
		g.rbMoonK = moonK;
		g.rbCamR = camR;
		std::copy( sunW, sunW + 3, g.rbLight[0] );
		std::copy( moonW, moonW + 3, g.rbLight[1] );
	}
	// 2b. lane SKYA (ATMO2): the aerial-perspective volume over this view's frustum (Hillaire 2020 sec. 5.4)
	QString apLine;	// empty while the row is off: the sky line stays ATMO1's
	if ( !in.capture )	// SKYGI1: a capture face keeps the frame's volume
		g.ap.valid = false;
	if ( ok && s.ap > 0 && !in.capture ) {
		const bool tintAp = tintNow( s ) > 0 && in.tintOk && !wwAtmoTintRedPhysical();
		const bool built = buildAp( scene, g, msN, camR, sunW, moonW, sunK, moonK, noOzone, noMs, tintAp ? in.tint : nullptr );
		apLine = built ? QString( "ap=%1x%2x%3@%4km" ).arg( g.ap.w ).arg( g.ap.h ).arg( g.ap.n ).arg( double( kApMaxKm ), 0, 'f', 0 )
				+ ( g.ap.layered ? QStringLiteral( "/layered" ) : QStringLiteral( "/slices" ) )	// lane PERFFIX3
			: QStringLiteral( "ap=refused(atmo_ap.prog or its target)" );
	}

	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	glViewport( vp[0], vp[1], vp[2], vp[3] );
	/* lane FIX1101A (moon, LATE ITEM 24d(2)): the sun disc and our moon globe end, under the air's composite, dimmed as
	 * their light is (the sky's own air x the ground layer), not by the bands on top (gl/moonlight.h discAirGain).
	 * Red WW_ATMO_DISC_RED=airband: gain 1 (the bands dim the disc again, as before). A capture has no air over it. */
	float discAir[2] = { 1.0f, 1.0f };
	{
		float med[4], layer[4], ground[2];
		if ( !in.capture && !wwAtmoDiscRed( "airband" ) && wwVolAirMediumNow( med, layer, ground ) ) {
			const float * lights[2] = { sunW, moonW };
			for ( int b = 0; b < 2; b++ ) {
				float t[3];
				cpuTrans( hKm, lights[b][2], s.haze, noOzone, t, g_ozone );
				const float tSky = ( t[0] + t[1] + t[2] ) / 3.0f;	// cell_volsky.frag's tSky
				discAir[b] = wwmoon::discAirGain( in.camFogZ, lights[b][2], tSky, med, layer, ground );
			}
		}
		s.discAirLast[0] = discAir[0];
		s.discAirLast[1] = discAir[1];
	}
	// 3. the sky's pixels
	if ( ok ) {
		auto p = r->useProgram( "atmo_sky.prog" );
		if ( p ) {
			medium( p, s.haze, noOzone );
			bindTex( fn, p, 0, "atTransTex", g.trans.tex );
			bindTex( fn, p, 1, "atSunView", g.sunView.tex );
			bindTex( fn, p, 2, "atMoonView", g.moonView.tex );
			bindTex( fn, p, 3, "atMsTex", g.ms.tex );
			p->uni2f( "atMsSize", float( msN ), float( msN ) );
			p->uni1f( "atMsOn", noMs ? 0.0f : 1.0f );
			p->uni1f( "atGround", noGround ? 0.0f : 1.0f );
			p->uni1f( "atPerPixel", perPixel ? 1.0f : 0.0f );
			p->uni2f( "atViewSize", float( vw ), float( vh ) );
			p->uni4f( "atRect", FloatVector4( float( vp[0] ), float( vp[1] ), 1.0f / float( std::max( vp[2], 1 ) ),
				1.0f / float( std::max( vp[3], 1 ) ) ) );
			p->uni1f( "atCamR", camR );
			p->uni3f( "atSunW", sunW[0], sunW[1], sunW[2] );
			p->uni3f( "atMoonW", moonW[0], moonW[1], moonW[2] );
			p->uni1f( "atSunK", sunK );
			p->uni1f( "atMoonK", moonK );
			p->uni1f( "atLimb", ( s.tier >= 3 || in.discs ) ? 1.0f : 0.0f );
			p->uni1f( "atDiscCap", in.disc ? ( phys ? wwphys::kDiscCap : kDiscCap ) : 0.0f );
			p->uni1f( "atSunSize", wwAtmoBodyScale( false ) );	// MODULES1 R6-12 b
			p->uni1f( "atDiscHue", in.discs ? 1.0f : 0.0f );	// SKYDISC1: 0 = the per-channel min() as before
			p->uni1f( "atDiscAir", discAir[0] );	// lane FIX1101A (moon): the disc's gain through the air
			p->uni1f( "sceneExposure", in.exposure );
			p->uni1i( "viewTransform", in.viewTransform );
			// lane SKYA (ATMOCHECK1): the Sky Color row's Weather Tint (atWeatherTint); 0 = the physical color, as before
			const bool tintOn = tintNow( s ) > 0 && in.tintOk;
			s.tintDrawn = tintOn;
			p->uni1f( "atTintOn", tintOn ? 1.0f : 0.0f );
			const GLint tintLoc = p->uniLocation( "atTint" );
			const bool recOn = in.tintOk && in.recNight[1] > 0.0f;	// MODULES1 R7: the record's night sky (lookdevstage.cpp computeLight)
			p->uni2f( "atRecNight", recOn ? in.recNight[0] : 0.0f, recOn ? in.recNight[1] : 0.0f );
			if ( ( tintOn || recOn ) && tintLoc >= 0 )
				fn->glUniform3fv( tintLoc, kTintN, &in.tint[0][0] );
			quad( r );
			r->stopProgram();
		} else {
			ok = false;
			why = QStringLiteral( "atmo_sky.prog did not link" );
		}
	}
	for ( int u = 3; u >= 0; u-- ) {	// SKY2B: unit 3 = the sky pass's MS table
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
	}
	fn->glActiveTexture( GLenum( prevActive ) );
	if ( wasDepth )
		fn->glEnable( GL_DEPTH_TEST );
	if ( wasBlend )
		fn->glEnable( GL_BLEND );
	if ( wasCull )
		fn->glEnable( GL_CULL_FACE );
	if ( wasScissor )
		fn->glEnable( GL_SCISSOR_TEST );
	if ( wasStencil )
		fn->glEnable( GL_STENCIL_TEST );
	fn->glDepthMask( depthMask );
	if ( !in.capture ) {
		fn->glQueryCounter( g.q[g.cur * 2 + 1], kTimestamp );
		g.pending[g.cur] = true;
		g.cur = 1 - g.cur;
	}
	g_last = ok ? &g : nullptr;
	if ( !in.capture ) {	// lane CLOUD2: what the volume clouds light with
		WwAtmoFrame & f = g_frame;
		f.valid = ok;
		std::copy( sunW, sunW + 3, f.sunW );
		std::copy( moonW, moonW + 3, f.moonW );
		f.sunK = sunK;
		f.moonK = moonK;
		f.moonKey = moonKey;
		f.camR = camR;
		std::copy( s.haze, s.haze + 3, f.haze );
		f.trans = g.trans.tex;
		f.sunView = g.sunView.tex;
		f.moonView = moonK > 0.0f ? g.moonView.tex : g.sunView.tex;
		f.viewW = vw;
		f.viewH = vh;
		f.frame = s.frame;
		f.moonDiscAir = discAir[1];	// lane FIX1101A (moon): our globe's gain through the air
		f.tintOn = s.tintDrawn && ok;
		const bool recOn = ok && in.tintOk && in.recNight[1] > 0.0f;	// MODULES1 R7
		f.recNight[0] = recOn ? in.recNight[0] : 0.0f;
		f.recNight[1] = recOn ? in.recNight[1] : 0.0f;
		if ( f.tintOn || recOn )
			std::copy( &in.tint[0][0], &in.tint[0][0] + kTintN * 3, &f.tint[0][0] );
	}

	const float sunEl = std::asin( std::clamp( sunW[2], -1.0f, 1.0f ) ) * 180.0f / kPi;
	QString reds;
	for ( const char * w : { "noozone", "noms", "nohaze", "msold", "nofloor" } )	// FIX1101A (atmo): msold, nofloor
		if ( red( w ) )
			reds = QStringLiteral( ",RED=%1" ).arg( QLatin1StringView( w ) );
	for ( const char * w : { "nosunt", "double", "skyclamp", "apband", "bandfar", "bandfull" } )
		if ( apRed( w ) )
			reds += QStringLiteral( ",AP_RED=%1" ).arg( QLatin1StringView( w ) );
	if ( in.discs )
		reds += wwAtmoDiscRed( "nophase" ) ? QStringLiteral( ",discs=ours,DISC_RED=nophase" ) : QStringLiteral( ",discs=ours" );
	// lane FIX1101A (moon): the sun disc's / our globe's gain through the air (1 = none: no air, or the red airband)
	reds += QStringLiteral( ",disc_air=%1/%2%3" ).arg( double( discAir[0] ), 0, 'f', 3 ).arg( double( discAir[1] ), 0, 'f', 3 )
		.arg( wwAtmoDiscRed( "airband" ) ? QStringLiteral( ",DISC_RED=airband" ) : QString() );
	if ( tintNow( s ) > 0 )	// lane SKYA (ATMOCHECK1); LATE 7: a glowing sky too
		reds += in.tintOk ? ( wwAtmoTintRedFlat() ? QStringLiteral( ",tint=weather,TINT_RED=flat" )
			: wwAtmoTintRedPhysical() ? QStringLiteral( ",tint=weather,TINT_RED=physical" ) : QStringLiteral( ",tint=weather(ap,amb)" ) )
			: QStringLiteral( ",tint=refused(no dome)" );
	if ( in.recNight[1] > 0.0f )	// MODULES1 R7: the record's night sky as drawn
		reds += !in.tintOk ? QStringLiteral( ",nightsky=refused(no dome)" )
			: in.recNight[0] > 0.0f ? QString( ",nightsky=record(x%1)" ).arg( double( in.recNight[1] ), 0, 'f', 3 )
			: QString( ",nightsky=game(+record x%1,zenith +%2)" ).arg( double( in.recNight[1] ), 0, 'f', 3 )
				.arg( double( ( 0.2126f * in.tint[16][0] + 0.7152f * in.tint[16][1] + 0.0722f * in.tint[16][2] ) * in.recNight[1] ), 0, 'g', 3 );
	s.last = ok ? QString( "physical(tier=%1,view=%2x%3,ms=%4,haze=%5/km@%6km,sun_elev=%7,k=%8,moon=%9%10)" )
			.arg( QLatin1StringView( tierName( s.tier ) ) ).arg( vw ).arg( vh ).arg( msN )
			.arg( double( s.haze[0] ), 0, 'f', 3 ).arg( double( s.haze[1] ), 0, 'f', 3 ).arg( double( sunEl ), 0, 'f', 1 )
			.arg( double( sunK / kPi ), 0, 'f', 3 ).arg( moonK > 0.0f ? QStringLiteral( "up" ) : QStringLiteral( "down" ) ).arg( reds )
			+ ( apLine.isEmpty() ? QString() : QStringLiteral( "," ) + apLine )
		: QString( "physical:refused(%1)" ).arg( why );
	if ( line )
		*line = s.last;
	return ok;
}

unsigned wwAtmoTransmittanceTex()
{
	return g_last ? g_last->trans.tex : 0;
}

unsigned wwAtmoMultiScatterTex()
{
	return g_last ? g_last->ms.tex : 0;
}

const WwAtmoFrame & wwAtmoFrame()
{
	return g_frame;
}

const char * wwAtmoHazeSource()
{
	return g_hazeSrc;
}

unsigned wwAtmoSkyViewTex()
{
	return g_last ? g_last->sunView.tex : 0;
}

void wwAtmoTransmittance( float heightKm, float cosZenith, float out[3] )
{
	cpuTrans( heightKm, cosZenith, as().haze, red( "noozone" ), out, g_ozone );
}

int wwAtmoAp()
{
	return as().ap;
}

void wwAtmoSetAp( int on )
{
	AtState & s = as();
	if ( s.apPinned )
		return;
	s.ap = on ? 1 : 0;
	QSettings().setValue( QLatin1StringView( kApKey ), s.ap );
}

bool wwAtmoApPinned()
{
	return as().apPinned;
}

int wwAtmoSun()
{
	return as().sun;
}

void wwAtmoSetSun( int mode )
{
	AtState & s = as();
	if ( s.sunPinned )
		return;
	s.sun = std::clamp( mode, 0, 2 );
	QSettings().setValue( QLatin1StringView( kSunKey ), s.sun );
}

bool wwAtmoSunPinned()
{
	return as().sunPinned;
}

int wwAtmoSunKey()
{
	const AtState & s = as();
	// lane SKY2B (SKYAFIX1): the red nohaze changes the key's haze too, so it moves the key (a live set kept the old one)
	return s.tier > 0 ? s.sun + 3 * ( wwEnvStr( "WW_ATMO_SUN_RED" ).trimmed().toLower() == QLatin1StringView( "flat" ) )
		+ 6 * red( "nohaze" ) : 0;
}

int wwAtmoNight()
{
	return as().night;
}

void wwAtmoSetNight( int on )
{
	AtState & s = as();
	if ( s.nightPinned )
		return;
	s.night = on ? 1 : 0;
	QSettings().setValue( QLatin1StringView( kNightKey ), s.night );
}

bool wwAtmoNightPinned()
{
	return as().nightPinned;
}

bool wwAtmoNightOn()
{
	const AtState & s = as();
	return s.tier > 0 && s.night > 0;
}

int wwAtmoNightKey( int phase )
{
	return wwAtmoNightOn() ? 1 + ( phase & 7 ) + 8 * int( nightRed( "nophase" ) ) : 0;
}

float wwAtmoMoonFactor( int phase )
{
	return wwAtmoNightOn() ? moonPhaseFactor( phase, nightRed( "nophase" ) ) : 1.0f;
}

bool wwAtmoDiscRed( const char * word )
{
	return wwEnvStr( "WW_ATMO_DISC_RED" ).trimmed().toLower() == QLatin1StringView( word );
}

bool wwAtmoDiscs()
{
	return as().tier > 0 && !wwAtmoDiscRed( "vanilla" );
}

float wwAtmoMoonFactorDeg( float alphaDeg )
{
	return ( wwAtmoNightOn() && !nightRed( "nophase" ) ) ? float( wwskydisc::ksFactor( alphaDeg ) ) : 1.0f;
}

void wwAtmoStarExtinction( float mu, float out[3] )
{
	out[0] = out[1] = out[2] = 1.0f;
	AtState & s = as();
	if ( !wwAtmoNightOn() )
		return;
	const float key[4] = { s.haze[0], s.haze[1], s.haze[2], nightRed( "noext" ) ? 1.0f : 0.0f };
	if ( !std::equal( key, key + 4, s.extHaze ) ) {
		std::copy( key, key + 4, s.extHaze );
		for ( int i = 0; i <= 32; i++ )
			starExtinction( float( i ) / 32.0f, s.haze, key[3] > 0.0f, s.ext[i] );
	}
	const float x = std::clamp( mu, 0.0f, 1.0f ) * 32.0f;
	const int i = std::min( int( x ), 31 );
	const float t = x - float( i );
	for ( int c = 0; c < 3; c++ )
		out[c] = s.ext[i][c] * ( 1.0f - t ) + s.ext[i + 1][c] * t;
}

bool wwAtmoKeyLight( float muLight, const float fog[4], float keyLum45, float sun[3], float amb[3], float dalc[6][3] )
{
	const AtState & s = as();
	if ( s.tier <= 0 || ( s.sun <= 0 && !wwPhysSkyOn() ) )	// lane PHYSUNITS1: real units take the air's key always
		return false;
	const bool flat = wwEnvStr( "WW_ATMO_SUN_RED" ).trimmed().toLower() == QLatin1StringView( "flat" );
	float haze[3];
	hazeFromFog( fog[0], fog[1], fog[2], fog[3], red( "nohaze" ), haze );
	if ( !red( "nohaze" ) )
		clearDayHaze( haze, fog[0] > 0.0f && fog[1] > 0.0f );	// lane WX1; SHAFTS1b: clear day, then the .recx
	/* lane PHYSUNITS1: keyLum45 is the top-of-atmosphere light over pi; the key is that x T(mu) (Full), so sunLight's
	 * division by the 45-degree transmittance is undone here */
	const bool phys = wwPhysSkyOn();
	if ( phys ) {
		float t45[3];
		cpuTrans( 0.0f, 0.70710678f, haze, false, t45 );
		keyLum45 *= std::max( 1e-4f, lum( t45 ) );
	}
	float rows[7][3];
	std::copy( amb, amb + 3, rows[0] );
	for ( int a = 0; a < 6; a++ )
		std::copy( dalc[a], dalc[a] + 3, rows[1 + a] );
	const float was[3] = { sun[0], sun[1], sun[2] };
	// SKY2B (SKYTINT2): under Weather Tint the ambient (and the DALC) keeps the weather's own color; the sun is still tinted
	const bool tintAmb = tintNow( s ) > 0 && s.tintDrawn && !wwAtmoTintRedPhysical();
	skyKeyLight( phys ? 2 : s.sun, muLight, haze, keyLum45, flat, sun, rows, tintAmb ? 0 : 7 );
	std::copy( rows[0], rows[0] + 3, amb );
	for ( int a = 0; a < 6; a++ )
		std::copy( rows[1 + a], rows[1 + a] + 3, dalc[a] );
	// stderr: every change of the light's inputs (SKY2B: every 240th call printed a stale mu beside a live hour change)
	static float last[4] = { -9.0f, -9.0f, -9.0f, -9.0f };
	const float now[4] = { muLight, float( s.sun ) + ( flat ? 4.0f : 0.0f ) + ( tintAmb ? 8.0f : 0.0f ), haze[0], keyLum45 };
	if ( !std::equal( now, now + 4, last ) ) {
		std::copy( now, now + 4, last );
		fprintf( stderr, "atmo sun: mode=%s%s%s mu=%.3f key %.4f,%.4f,%.4f -> %.4f,%.4f,%.4f amb -> %.4f,%.4f,%.4f\n",
			s.sun == 2 ? "full" : "tint", flat ? ",RED=flat" : "", tintAmb ? ",amb=weather" : "", double( muLight ), double( was[0] ), double( was[1] ),
			double( was[2] ), double( sun[0] ), double( sun[1] ), double( sun[2] ), double( amb[0] ), double( amb[1] ),
			double( amb[2] ) );
	}
	return true;
}

void wwAtmoWbGain( float out[3] )
{
	out[0] = out[1] = out[2] = 1.0f;
	const QString e = wwEnvStr( "WW_CELL_WB" ).trimmed().toLower();
	if ( e.isEmpty() || e == QLatin1StringView( "off" ) || e == QLatin1StringView( "0" ) )
		return;
	QString how;
	if ( e == QLatin1StringView( "day45" ) ) {
		const AtState & s = as();
		float t45[3];
		cpuTrans( 0.0f, 0.70710678f, s.haze, false, t45, g_ozone );	// the last drawn sky's air (clear air before one)
		const float l = lum( t45 );
		for ( int c = 0; c < 3; c++ )
			out[c] = t45[c] > 1e-6f ? l / t45[c] : 1.0f;
		how = QStringLiteral( "day45(haze=%1/km@%2km)" ).arg( double( s.haze[0] ), 0, 'f', 3 ).arg( double( s.haze[1] ), 0, 'f', 3 );
	} else {
		const QStringList p = e.split( QChar( ',' ) );
		if ( p.size() == 3 )
			for ( int c = 0; c < 3; c++ )
				out[c] = std::clamp( p.at( c ).toFloat(), 0.05f, 20.0f );
		how = QStringLiteral( "explicit" );
	}
	static float last[3] = { -1.0f, -1.0f, -1.0f };
	if ( !std::equal( out, out + 3, last ) ) {
		std::copy( out, out + 3, last );
		fprintf( stderr, "cell wb: %s gain=%.4f,%.4f,%.4f\n", qPrintable( how ), double( out[0] ), double( out[1] ), double( out[2] ) );
	}
}

void wwAtmoApFrame()
{
	for ( AtGpu & g : gpus() )
		g.ap.valid = false;
	g_frame.valid = false;	// lane CLOUD2: this frame's sky refills it
	as().frame++;
}

int wwAtmoGi()
{
	return as().gi;
}

// ---- MODULES1 R6-12 b: the Sun and Moon Size row ----
int wwAtmoBodySize()
{
	return as().bodySize;
}

void wwAtmoSetBodySize( int v )
{
	AtState & s = as();
	if ( s.bodySizePinned )
		return;
	s.bodySize = std::clamp( v, 0, 3 );
	QSettings().setValue( QLatin1StringView( kBodyKey ), s.bodySize );
}

float wwAtmoBodyScale( bool moon )
{
	const AtState & s = as();
	const float game = moon ? s.gameMoon : s.gameSun;
	if ( s.bodySize <= 0 || !( game > 0.0f ) )
		return 1.0f;
	const float real = float( std::tan( ( moon ? wwskydisc::kMoonHalfDeg : wwskydisc::kSunHalfDeg ) * wwskydisc::kRad ) );
	// never under the true size; the bound keeps a broken record from filling the sky (a disc 30 degrees across)
	const float full = std::clamp( game * ( s.bodySize == 3 ? 1.5f : 1.0f ) / real, 1.0f, 58.0f );
	if ( s.bodySize != 1 )
		return full;
	/* MODULES1 round 10 (bungo 11-11: "ours was way too small", the game's "too big"): Medium. The moon 4 degrees
	 * across (never over the game's own); the sun the same share, in log size, of the way from its true size to the
	 * game's (moon x7.70 of x27.44 = 0.616 of the way; the sun x4.72 ^ 0.616 = x2.60, 1.39 degrees). */
	const float moonGame = s.gameMoon > 0.0f
		? std::clamp( s.gameMoon / float( std::tan( wwskydisc::kMoonHalfDeg * wwskydisc::kRad ) ), 1.0f, 58.0f ) : 1.0f;
	const float moonMed = std::min( float( std::tan( 2.0 * wwskydisc::kRad ) / std::tan( wwskydisc::kMoonHalfDeg * wwskydisc::kRad ) ), moonGame );
	if ( moon )
		return moonMed;
	const float p = moonGame > 1.0001f ? std::log( moonMed ) / std::log( moonGame ) : 1.0f;
	return std::pow( full, p );
}

void wwAtmoSetBodyGame( float sunTanHalf, float moonTanHalf )
{
	AtState & s = as();
	s.gameSun = sunTanHalf;
	s.gameMoon = moonTanHalf;
	static QString said;
	const char * names[4] = { "Physical", "Medium (moon 4 deg)", "Game", "Large (Game x 1.5)" };
	const double ks = wwAtmoBodyScale( false ), km = wwAtmoBodyScale( true );
	const double ts = std::tan( wwskydisc::kSunHalfDeg * wwskydisc::kRad ), tm = std::tan( wwskydisc::kMoonHalfDeg * wwskydisc::kRad );
	const QString line = QStringLiteral( "sky bodies size: row %1%2; sun disc drawn %3 deg across (true %4, x%5), moon %6 deg (true %7, x%8); the game's own: sun %9 deg, moon %10 deg (the record's quad x the art's disc share); disc and glare only, the light reads the true sizes (red: the row's Physical, WW_ATMO_BODYSIZE=physical)" )
		.arg( QLatin1String( names[std::clamp( s.bodySize, 0, 3 )] ), s.bodySizePinned ? QStringLiteral( " (PINNED)" ) : QString() )
		.arg( 2.0 * std::atan( ts * ks ) / wwskydisc::kRad, 0, 'f', 2 ).arg( 2.0 * wwskydisc::kSunHalfDeg, 0, 'f', 3 ).arg( ks, 0, 'f', 2 )
		.arg( 2.0 * std::atan( tm * km ) / wwskydisc::kRad, 0, 'f', 2 ).arg( 2.0 * wwskydisc::kMoonHalfDeg, 0, 'f', 3 ).arg( km, 0, 'f', 2 )
		.arg( 2.0 * std::atan( double( sunTanHalf ) ) / wwskydisc::kRad, 0, 'f', 2 ).arg( 2.0 * std::atan( double( moonTanHalf ) ) / wwskydisc::kRad, 0, 'f', 2 );
	if ( line != said ) {
		said = line;
		fprintf( stderr, "%s\n", qPrintable( line ) );
	}
}

void wwAtmoSetGi( int on )
{
	AtState & s = as();
	if ( s.giPinned )
		return;
	s.gi = on ? 1 : 0;
	QSettings().setValue( QLatin1StringView( kGiKey ), s.gi );
}

bool wwAtmoGiPinned()
{
	return as().giPinned;
}

bool wwAtmoGiActive()
{
	const AtState & s = as();
	return s.gi > 0 && s.tier > 0;
}

/* FIX1101A (LATE ITEM 15d): the physical sky's irradiance on the six world axes for the ray-traced GI's sky (sun toward
 * sunTo, sun irradiance 1, the last drawn haze; kept while sun, haze and height stand).
 * GISKYTWIN: the drawn sky's own model (atmomath skyIrradianceAxesDrawn: its transmittance + MS tables at this tier's
 * size, its 30-step march with MS) from hKm (the probes' height), within 0.6 % of the GPU sky (tests/features/sky/gisky twin).
 * The old single-scatter twin read +Z R/B 0.87 where the drawn sky reads 0.53: launch key WW_CELL_RTDDGI_SKYTWIN_RED=1
 * keeps it (skyIrradianceAxes, from the ground, the exact haze). The twin takes the haze in 2 % steps, so its tables
 * (~70-300 ms on 8 threads) rebuild only on a step; a new sun or height alone is ~3 ms.
 * False while Physical Sky is off or the sun is under the horizon (the weather's ambient stays the GI's sky then). */
/* the drawn sky's CPU tables (atmomath skyTablesBuild), one set for the GI's sky and the fog's horizon (MODULES1 M8):
 * both key them on the same 2 % haze steps and tier, so they rebuild once per step, not once per caller */
static SkyTables & drawnTables()
{
	static SkyTables t;
	return t;
}

bool wwAtmoGiSkyTwinRed()
{
	static const bool red = wwEnvStr( "WW_CELL_RTDDGI_SKYTWIN_RED" ).trimmed() == QLatin1StringView( "1" );
	return red;
}

bool wwAtmoGiSkyAxes( const float sunTo[3], float out[6][3], float * trans, float hKm )
{
	const AtState & s = as();
	if ( s.tier <= 0 || sunTo[2] <= 0.0f )
		return false;
	const bool twinRed = wwAtmoGiSkyTwinRed();
	const int msN = s.tier == 1 ? 16 : 32;
	auto q2 = []( float x ) { return x > 1e-4f ? std::exp( std::round( std::log( x ) * 50.0f ) / 50.0f ) : x; };
	const float hq[3] = { twinRed ? s.haze[0] : q2( s.haze[0] ), twinRed ? s.haze[1] : q2( s.haze[1] ),
		twinRed ? s.haze[2] : std::round( s.haze[2] * 100.0f ) / 100.0f };
	const float h = twinRed ? 0.0f : std::round( std::clamp( hKm, 0.002f, 80.0f ) * 1000.0f ) / 1000.0f;
	static float key[10] = { -9.0f, -9.0f, -9.0f, -9.0f, -9.0f, -9.0f, -9.0f, -9.0f, -9.0f, -9.0f };
	static float memo[6][3], memoT[3];
	SkyTables & tables = drawnTables();
	const float now[10] = { sunTo[0], sunTo[1], sunTo[2], hq[0], hq[1], hq[2], h, g_ozone, float( msN ), twinRed ? 1.0f : 0.0f };
	if ( !std::equal( now, now + 10, key ) ) {
		std::copy( now, now + 10, key );
		if ( twinRed )
			skyIrradianceAxes( sunTo, s.haze, memo );
		else
			skyIrradianceAxesDrawn( sunTo, hq, h, g_ozone, tables, memo, msN );
		cpuTrans( 0.0f, sunTo[2], s.haze, false, memoT );   // GITRUTH1: the sun's own transmittance (the absolute level)
	}
	if ( trans )
		std::copy( memoT, memoT + 3, trans );
	for ( int a = 0; a < 6; a++ )
		for ( int c = 0; c < 3; c++ )
			out[a][c] = memo[a][c];
	return true;
}

/* MODULES1 M8 (bungo 11-10, 3 AM: a hard bright-blue horizon line against a black sky): the drawn sky's colour just
 * above the horizon (elevation 0.57 deg, the mean of 16 azimuths) in scene units, as atmo_sky.frag draws it: the sun's
 * and the moon's scatter (sunK, moonK of the last drawn frame), the night floor seen slant, the Weather Tint. The fog's
 * far colours take it (lookdevstage.cpp wwLookdevFogUniforms). False before a sky is drawn or with the sky off. */
void wwAtmoSetNightSky( const WwNightSky & n )
{
	g_night = n;
}

const WwNightSky & wwAtmoNightSky()
{
	return g_night;
}

void wwAtmoNightSkyGot( int who, float unit, float level )
{
	if ( who < 0 || who >= WwNsCount )
		return;
	g_nsUnit[who] = unit;
	g_nsLevel[who] = level;
	// the line follows the DRAWN sky's report (once a frame, after the light was computed); 2 % steps
	if ( who != WwNsDrawn )
		return;
	auto q = []( float x ) { return x > 1e-12f ? int( std::lround( std::log( x ) * 50.0f ) ) : -99999; };
	const WwNightSky & n = g_night;
	static int key[2 * WwNsCount + 4] = { -1 };
	int now[2 * WwNsCount + 4];
	for ( int i = 0; i < WwNsCount; i++ ) {
		now[2 * i] = q( g_nsUnit[i] );
		now[2 * i + 1] = q( g_nsLevel[i] );
	}
	now[2 * WwNsCount] = n.mode;
	now[2 * WwNsCount + 1] = q( n.gain );
	now[2 * WwNsCount + 2] = q( n.floorGain );
	now[2 * WwNsCount + 3] = int( std::lround( n.share * 50.0f ) );
	if ( std::equal( now, now + 2 * WwNsCount + 4, key ) )
		return;
	std::copy( now, now + 2 * WwNsCount + 4, key );
	const double dv = g_nsUnit[WwNsVolumes] > 0.0f ? double( g_nsUnit[WwNsDrawn] / g_nsUnit[WwNsVolumes] ) : 0.0;
	fprintf( stderr, "night sky: %s factor x%.4g at this light x%.4g (%.3g stops under the Day key, the moon's phase share %.3g cancelled; night share %.2f, floor x%.4g); "
		"unit x gain each reader took: drawn sky %.5g, probes' capture %.5g, RTXGI miss sky %.5g (level +Z %.5g), ambient axes %.5g (level +Z %.5g), "
		"night air %.5g, far horizon %.5g, 2D clouds %.5g; drawn : RTXGI %.4f (bar 0.95..1.05; 0 = no volume yet); "
		"reds WW_NIGHTSKY_RED=physical (no factor) / record (the record dome's crossfade), WW_NIGHTSKY_STOPS (live)\n",
		n.mode == 0 ? "physical sky x one factor" : n.mode == 1 ? "RED WW_NIGHTSKY_RED=physical:" : "RED WW_NIGHTSKY_RED=record:",
		double( n.factor ), double( n.gain ), double( n.stops ), double( n.phase ), double( n.share ), double( n.floorGain ),
		double( g_nsUnit[WwNsDrawn] ), double( g_nsUnit[WwNsProbes] ), double( g_nsUnit[WwNsVolumes] ), double( g_nsLevel[WwNsVolumes] ),
		double( g_nsUnit[WwNsAmbient] ), double( g_nsLevel[WwNsAmbient] ), double( g_nsLevel[WwNsAir] ), double( g_nsLevel[WwNsFar] ),
		double( g_nsLevel[WwNsClouds] ), dv );
}

bool wwAtmoHorizon( float out[3] )
{
	const WwAtmoFrame & f = g_frame;
	const AtState & s = as();
	if ( !f.valid || s.tier <= 0 || f.camR <= 0.0f )
		return false;
	const int msN = s.tier == 1 ? 16 : 32;
	auto q2 = []( float x ) { return x > 1e-4f ? std::exp( std::round( std::log( x ) * 50.0f ) / 50.0f ) : x; };
	const float hq[3] = { q2( f.haze[0] ), q2( f.haze[1] ), std::round( f.haze[2] * 100.0f ) / 100.0f };
	static float key[14] = { -9.0f };
	static float memo[3];
	const float now[14] = { f.sunW[0], f.sunW[1], f.sunW[2], f.moonW[0], f.moonW[1], f.moonW[2], f.sunK, f.moonK, f.camR,
		hq[0], hq[1], hq[2], g_ozone * 64.0f + float( msN ), g_night.floorGain };
	if ( !std::equal( now, now + 14, key ) ) {
		std::copy( now, now + 14, key );
		SkyTables & T = drawnTables();
		skyTablesBuild( hq, g_ozone, msN, T );
		constexpr float vz = 0.01f;
		const float ch = std::sqrt( 1.0f - vz * vz );
		double acc[3] = { 0, 0, 0 };
		constexpr int NA = 16;
		for ( int a = 0; a < NA; a++ ) {
			const float az = ( float( a ) + 0.5f ) * 2.0f * float( kPi ) / float( NA );
			const float v[3] = { ch * std::cos( az ), ch * std::sin( az ), vz };
			double L[3];
			skyMarch( T, hq, g_ozone, f.camR, v, f.sunW, L );
			for ( int c = 0; c < 3; c++ )
				acc[c] += double( f.sunK ) * L[c];
			if ( f.moonK > 0.0f ) {
				skyMarch( T, hq, g_ozone, f.camR, v, f.moonW, L );
				for ( int c = 0; c < 3; c++ )
					acc[c] += double( f.moonK ) * L[c];
			}
		}
		float fl[3] = { 0.0f, 0.0f, 0.0f };
		if ( !red( "nofloor" ) ) {	// atmosphere.cpp medium()'s atFloor, atmo_sky.frag's slant shell
			if ( g_airglow <= 1.0f || red( "glowgrey" ) ) {
				for ( int c = 0; c < 3; c++ )
					fl[c] = kNightFloor[c] * g_airglow;
			} else {
				const float ex = kNightFloorLum * ( g_airglow - 1.0f );
				for ( int c = 0; c < 3; c++ )
					fl[c] = kNightFloor[c] + ex * kGreenLine5577[c];
			}
		}
		float tr[3];
		cpuTrans( f.camR - kRb, vz, hq, red( "noozone" ), tr, g_ozone );
		const float vr = 1.0f / std::sqrt( std::max( 1.0f - 0.97226f * ( 1.0f - vz * vz ), 0.02f ) );
		for ( int c = 0; c < 3; c++ )
			memo[c] = float( acc[c] / NA ) + f.sunK * fl[c] * g_night.floorGain * vr * tr[c];
	}
	std::copy( memo, memo + 3, out );
	if ( f.tintOn || f.recNight[1] > 0.0f ) {	// atmo_sky.frag atRecNight, then atWeatherTint, at the same elevation
		const float t = std::sqrt( std::asin( 0.01f ) * 57.29578f / 90.0f ) * 16.0f;
		const int i = std::min( int( t ), 15 );
		float w[3];
		for ( int c = 0; c < 3; c++ )
			w[c] = f.tint[i][c] + ( f.tint[i + 1][c] - f.tint[i][c] ) * ( t - float( i ) );
		for ( int c = 0; c < 3; c++ )	// MODULES1 R7: the record's night sky
			out[c] = out[c] * ( 1.0f - f.recNight[0] ) + w[c] * f.recNight[1];
		const float lw = 0.2126f * w[0] + 0.7152f * w[1] + 0.0722f * w[2];
		const float lc = 0.2126f * out[0] + 0.7152f * out[1] + 0.0722f * out[2];
		if ( f.tintOn && lw > 1e-8f )
			for ( int c = 0; c < 3; c++ )
				out[c] = w[c] * lc / lw;
	}
	return true;
}

bool wwAtmoGiRedDome()
{
	return wwEnvStr( "WW_ATMO_GI_RED" ).trimmed().toLower() == QLatin1StringView( "dome" );
}

bool wwAtmoGiRedCamZ()
{
	return wwEnvStr( "WW_ATMO_GI_RED" ).trimmed().toLower() == QLatin1StringView( "camz" );
}

int wwAtmoTint()
{
	return as().tint;
}

void wwAtmoSetTint( int mode )
{
	AtState & s = as();
	if ( s.tintPinned )
		return;
	s.tint = mode ? 1 : 0;
	QSettings().setValue( QLatin1StringView( kTintKey ), s.tint );
}

bool wwAtmoTintPinned()
{
	return as().tintPinned;
}

bool wwAtmoTintRedFlat()
{
	return wwEnvStr( "WW_ATMO_TINT_RED" ).trimmed().toLower() == QLatin1StringView( "flat" );
}

void wwAtmoSetGlowSky( bool glow )
{
	as().glowSky = glow;
}

int wwAtmoTintDrawn()
{
	return tintNow( as() );
}

bool wwAtmoTintRedPhysical()
{
	return wwEnvStr( "WW_ATMO_TINT_RED" ).trimmed().toLower() == QLatin1StringView( "physical" );
}

int wwAtmoExpo()
{
	return as().expo;
}

void wwAtmoSetExpo( int on )
{
	AtState & s = as();
	if ( s.expoPinned )
		return;
	s.expo = on ? 1 : 0;
	QSettings().setValue( QLatin1StringView( kExpoKey ), s.expo );
}

bool wwAtmoExpoPinned()
{
	return as().expoPinned;
}

float wwAtmoAdapted( float sceneMean, int counted, int total )
{
	const AtState & s = as();
	// the sky read within the last two frames only (an interior, or the Physical Sky just turned off: the scene alone)
	if ( s.tier <= 0 || s.expo <= 0 || s.skyMean < 0.0f || total <= 0 || s.frame > s.skyFrame + 2 )
		return sceneMean;
	const float a = adaptedWithSky( sceneMean, float( counted ) / float( total ), s.skyMean, expoRed( "skyzero" ) );
	// stderr: when the cover or the sky's mean moves by 1 % (SKY2B: every 600th call hid the live set's own value)
	static float lastCover = -1.0f, lastSky = -1.0f;
	const float cover = float( counted ) / float( std::max( total, 1 ) );
	if ( std::fabs( cover - lastCover ) > 0.01f || std::fabs( s.skyMean - lastSky ) > 0.01f * std::max( lastSky, 1e-6f ) ) {
		lastCover = cover;
		lastSky = s.skyMean;
		fprintf( stderr, "atmo expo: scene %.5f cover %.3f sky %.5f -> %.5f%s\n", double( sceneMean ),
			double( counted ) / double( total ), double( s.skyMean ), double( a ),
			expoRed( "skyzero" ) ? ",RED=skyzero" : expoRed( "zenithw" ) ? ",RED=zenithw" : "" );
	}
	return a;
}

float wwAtmoAdaptedInView( Scene * scene, float sceneMean, const float * px, int qw, int qh )
{
	double cover = 0.0;
	for ( size_t i = 0; i < size_t( qw ) * size_t( qh ); i++ )
		cover += px[i * 4 + 3] > 0.0f ? double( px[i * 4 + 3] ) : 0.0;
	const int counted = int( std::lround( cover ) ), total = qw * qh;
	const AtState & s = as();
	// the red "hemi" (and no table, no camera) keeps EXPO1: the sky at its whole upper hemisphere's mean
	if ( expoRed( "hemi" ) || !scene || !scene->renderer || s.tier <= 0 || s.expo <= 0 || total <= 0 )
		return wwAtmoAdapted( sceneMean, counted, total );
	auto it = gpus().find( scene->renderer );
	const auto & gu = *scene->renderer->globalUniforms;
	if ( it == gpus().end() || it->tab[0].empty() || s.frame > it->tabFrame + 2 || gu.projectionMatrix[3][3] == 1.0f )
		return wwAtmoAdapted( sceneMean, counted, total );
	const AtGpu & g = *it;
	// the frustum's corner rays (buildAp's): the measure's texel (x, y) looks along f0 + fx x/qw + fy y/qh
	const float P00 = gu.projectionMatrix[0][0], P11 = gu.projectionMatrix[1][1];
	const float P20 = gu.projectionMatrix[2][0], P21 = gu.projectionMatrix[2][1];
	auto ray = [&]( float x, float y, float o[3] ) {
		const float vr[3] = { ( x + P20 ) / P00, ( y + P21 ) / P11, -1.0f };
		for ( int c = 0; c < 3; c++ )
			o[c] = vr[0] * gu.viewMatrix[c][0] + vr[1] * gu.viewMatrix[c][1] + vr[2] * gu.viewMatrix[c][2];
	};
	float f0[3], fx[3], fy[3];
	ray( -1.0f, -1.0f, f0 );
	ray( 1.0f, -1.0f, fx );
	ray( -1.0f, 1.0f, fy );
	for ( int c = 0; c < 3; c++ ) {
		fx[c] -= f0[c];
		fy[c] -= f0[c];
	}
	std::vector<float> share( static_cast<size_t>( total ) );
	for ( size_t i = 0; i < share.size(); i++ )
		share[i] = 1.0f - std::clamp( px[i * 4 + 3], 0.0f, 1.0f );
	const float * tabs[2] = { g.tab[0].data(), g.tabK[1] > 0.0f && !g.tab[1].empty() ? g.tab[1].data() : nullptr };
	const float inView = skyMeanInView( share.data(), qw, qh, f0, fx, fy, tabs, g.tabK, g.tabLight, g.tabW, g.tabH, g.tabCamR );
	const float sky = inView >= 0.0f ? inView : 0.0f;
	const float a = adaptedWithSky( sceneMean, float( counted ) / float( total ), sky, expoRed( "skyzero" ) );
	static float lastCover = -1.0f, lastSky = -1.0f;
	const float cv = float( counted ) / float( total );
	if ( std::fabs( cv - lastCover ) > 0.01f || std::fabs( sky - lastSky ) > 0.01f * std::max( lastSky, 1e-6f ) ) {
		lastCover = cv;
		lastSky = sky;
		fprintf( stderr, "atmo expo: scene %.5f cover %.3f sky in view %.5f (hemisphere %.5f) -> %.5f%s\n", double( sceneMean ),
			double( cv ), double( sky ), double( s.skyMean ), double( a ), expoRed( "skyzero" ) ? ",RED=skyzero" : "" );
	}
	return a;
}

bool wwAtmoApActive()
{
	const AtState & s = as();
	if ( s.ap <= 0 || s.tier <= 0 )
		return false;
	for ( const AtGpu & g : gpus() )
		if ( g.ap.valid )
			return true;
	return false;
}

void wwAtmoApUniforms( Scene * scene )
{
	if ( !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "atAp" ) < 0 )
		return;
	const AtState & s = as();
	auto it = gpus().find( r );
	const Volume * v = it != gpus().end() ? &it->ap : nullptr;
	bool on = v && v->valid && s.ap > 0 && s.tier > 0 && !scene->selecting;
	if ( on ) {
		// only the view the volume was built for (a reflection probe's face or a capture reads none)
		const auto & gu = *r->globalUniforms;
		for ( int c = 0; c < 3 && on; c++ )
			for ( int k = 0; k < 3 && on; k++ )
				on = v->view[c * 3 + k] == gu.viewMatrix[c][k];
		on = on && v->proj[0] == gu.projectionMatrix[0][0] && v->proj[1] == gu.projectionMatrix[1][1]
			&& v->proj[2] == gu.projectionMatrix[2][0] && v->proj[3] == gu.projectionMatrix[2][1];
	}
	// bound whether or not this draw reads it (a sampler left on unit 0 would sit beside BaseMap)
	WwCellUni::Units<std::remove_pointer_t<decltype( r->fn )>> u( r->fn, wwCellUniDsa() );	// lane CELLUNI3: no ask with DSA
	u.section();
	u.bind( WwCellUnit::AtmoAp, GL_TEXTURE_3D, on ? v->tex : 0 );
	u.done();
	prog->uni1i( "atApTex", WwCellUnit::AtmoAp );
	prog->uni4f( "atAp", FloatVector4( on ? 1.0f : 0.0f, kApMaxKm, on ? v->kmPerView : 0.0f, on ? float( v->n ) : 1.0f ) );
	if ( prog->uniLocation( "atSkyTransTex" ) >= 0 ) {
		// the sky's air (cell_volsky.frag): the physical sky already holds the air along its ray to space, so the
		// Volumetric Air over it keeps only its residual (atmo_apfog.glsl); the ray's transmittance from this LUT
		u.bind( WwCellUnit::AtmoTrans, GL_TEXTURE_2D, on ? it->trans.tex : 0 );
		u.done();
		prog->uni1i( "atSkyTransTex", WwCellUnit::AtmoTrans );
		prog->uni1f( "atSkyCamR", on ? v->camR : 0.0f );
		// lane HORIZONBAND1: the sky's horizon band (atmo_skyband.glsl); the red skyclamp = the clamp before it (live)
		// lane FIX1101A (atmo, the night horizon stripe): rule 2 (shipped) colours the band with what a black surface at
		// the far plane gets through the surfaces' own fog path; red apband = rule 1 (the aerial perspective's colour alone)
		/* MODULES1 item 18: rule 3 (shipped) ends the band's fog path where a ray under the horizon meets the ground,
		 * not at the far plane under it (a high camera saw black under the horizon); red WW_ATMO_AP_RED=bandfar = rule 2 */
		prog->uni1i( "atApSkyRule", apRed( "skyclamp" ) ? 0 : apRed( "apband" ) ? 1 : apRed( "bandfar" ) ? 2 : apRed( "bandfull" ) ? 3 : 4 );
		/* MODULES1 round 10: rule 4 (shipped) scales the band's cover by the air the far land under it has
		 * (1 - the far fog path's transmittance) and takes that path's own light: no fog, no band; red bandfull = rule 3 */
	}
	if ( !on )
		return;
	prog->uni4f( "atApProj", FloatVector4( v->proj[0], v->proj[1], v->proj[2], v->proj[3] ) );
	prog->uni1f( "atApDouble", apRed( "double" ) ? 1.0f : 0.0f );
}

QString wwAtmoSummary()
{
	const AtState & s = as();
	return s.tier <= 0 ? QStringLiteral( "off" ) : s.last;
}
