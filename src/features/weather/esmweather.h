/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_ESMWEATHER_H
#define WW_ESMWEATHER_H

/* EsmWeather -- the W1 weather reader (lane PBRR2B, docs/NIFSKOPE_PBR_RENDERER.md
 * s6.3 + s13 "Weather gates for W1"; research scratchpad/pbrrender0_20260923/
 * research_weather.md).
 *
 * A thin layer on the ONE plugin reader, libfo76utils ESMFile: there is no
 * second parser. What it adds:
 *   - the MISSING-MASTER REFUSAL. ESMFile silently maps an absent master to the
 *     raw byte (esmfile.cpp:216-220), which can alias another plugin's slot and
 *     show the wrong record. Before an ESMFile is built, every file's TES4 MAST
 *     list is read (header only) and each master must appear EARLIER in the load
 *     list (basename, case-folded -- ESMFile's own rule). Otherwise: refused,
 *     naming the master and the plugin that needs it. Also refused: more than
 *     256 files (ESMFile's plugin map), a file that is not a TES4 plugin.
 *   - the WTHR list (each weather once, already resolved to the winning file,
 *     ESMFile keeps the last loaded version) and one WTHR's W1 fields: NAM0 (the
 *     row/ToD count from the FORM VERSION, never the size), DALC x8 (x4 on old
 *     forms), read with the NON-const ESMField (the const one returns nothing for
 *     a compressed record).
 *   - the climate clock: CLMT TNAM (10-minute units), DefaultClimate 0000015F
 *     unless another is named.
 *   - the time-of-day blend (the engine's: 0.5 h extension, four-quarter
 *     ramps; FO4CS docs/RE/solar-daynight.md) and the vanilla sun: the engine's
 *     tent arc (fSunXExtreme 400, fSunYExtreme 25) and the light's bias and
 *     floor (fSunShadowScale -15 as Fallout4.esm sets it, fSunShadowMinAngle 30,
 *     both through the engine's DEG_TO_RAD; FO4CS src/Sky/SolarPosition.h).
 *
 * The same reader serves the Scene window, the harness pins (WW_LOOKDEV_*) and
 * the `-no-gui weather` command the W1 gates call. No GL here.
 *
 * Red controls (WW_LOOKDEV_RED=<name>, gates only):
 *   stride    NAM0 read with a ToD stride of 4 (G1 must fail)
 *   rowswap   NAM0 rows 3 (Ambient) and 4 (Sunlight) swapped (G1 must fail)
 *   todorder  the blend walks the slots in xEdit order 0..3 with no early/late
 *             insert (G3 must fail)
 *   nomaster  the master check is skipped (G5 must fail: the load "succeeds")
 *   hourstuck the hour is ignored, always 12:00 (G7 must fail)
 *
 * The ENGINE CLOCK (lane PBRWX1, weather preview): everything above is the W1
 * reading and stays byte-for-byte as it was. The Sun row of the Scene window
 * switches the lookdev to the engine's own clock instead, built from the
 * PBRPREP1 specs (scratchpad/pbrprep1_20260924/spec_weather_sky.md,
 * spec_clouds.md, spec_moon.md):
 *   - the sky GMSTs read from the loaded plugins by EditorID (last plugin
 *     wins), the game's defaults when none sets them (WwSkyGmst);
 *   - the engine's sun arc: midpoint windows A..D, the night branch, the disc
 *     fade over fSunAlphaTransTime, the light's bias and floor (WwSkyClock);
 *   - the time-of-day colour blend in CIELab, as the engine does it (its own
 *     RGB <-> CIELab conversions and constants);
 *   - the WTHR cloud layers (x0TX, LNAM, NAM1, PNAM, JNAM, QNAM, RNAM, ONAM),
 *     the sun-glare byte (DATA[4]) and the IMGS Sky Scale (IMSP -> HNAM[7]);
 *   - the moon: phase from the game day, alpha by the angle fades, position =
 *     the sun's (same arc).
 * More red controls (the engine clock's gates, tests/spells/pbr_wx1_gates.sh):
 *   exegmst       the plugins' GMSTs ignored, the exe values used
 *   rgbblend      the ToD colour blend on bytes instead of CIELab
 *   nonightbranch the arc's night branch dropped (the day formula runs on)
 *   sunfade2h     fSunAlphaTransTime forced to 2.0 h
 *   colorext05    fDaytimeColorExtension forced to 0.5 h
 *   phasestuck    the moon phase forced to 0 (full)
 *   moonalpha1    the moon alpha forced to 1
 *   nam1ignore    NAM1 ignored (only the missing-texture mask disables)
 *   speedswap     the cloud X and Y speeds swapped
 *   cloudalphaone every cloud alpha forced to 1
 * The fog (lane FOG1, WwFog; gates tests/spells/pbr_fog1_gates.sh):
 *   fogext05      the day weight on fDaytimeColorExtension 0.5 instead of the plugins' value
 *   fogpower1     the fog power forced to 1
 *   fognoblend    no day <-> night blend: the day values at every hour
 *   fognogamma    the fog colours without the pow 2.2
 *   fognonam4     the NAM4 fog colour scale ignored
 *   fognear0      the fog near distance forced to 0
 *   fogmaxclamp   the intensity capped at Max past ramp 0.75 (also in the shader)
 *   fognoescape   the near-escape factor dropped (also in the shader)
 */

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>
#include <vector>

class ESMFile;

//! time-of-day slot order of every 8-slot WTHR array (xEdit wbDefinitionsCommon.pas:8634-8643)
enum WwTod { WwTodSunrise = 0, WwTodDay = 1, WwTodSunset = 2, WwTodNight = 3,
	WwTodEarlySunrise = 4, WwTodLateSunrise = 5, WwTodEarlySunset = 6, WwTodLateSunset = 7 };
const char * wwTodName( int slot );

//! NAM0 row names (xEdit wbWeatherColors, wbDefinitionsCommon.pas:9707-9757)
enum WwNam0Row { WwRowSkyUpper = 0, WwRowFogNear = 1, WwRowAmbient = 3, WwRowSunlight = 4, WwRowSun = 5,
	WwRowStars = 6, WwRowSkyLower = 7, WwRowHorizon = 8, WwRowEffectLighting = 9, WwRowSunGlare = 15, WwRowMoonGlare = 16,
	WwRowFogFar = 12, WwRowWaterMult = 14, WwRowFogNearHigh = 17, WwRowFogFarHigh = 18 };

//! true when WW_LOOKDEV_RED names this red control
bool wwLookdevRed( const char * name );

struct WwTodKeys
{
	int a = WwTodDay;
	int b = WwTodDay;
	float t = 0.0f;
	QString describe() const;	// "Day", or "Sunrise->LateSunrise 40%"
};

/*! The engine's colour phases for `hour` under climate TNAM bytes rise0, rise1,
 *  set0, set1 (10-minute units). night: hour < rise0 - ext or >= set1 + ext;
 *  the sunrise ramp [rise0 - ext, rise1) in four equal quarters
 *  Night -> EarlySunrise -> Sunrise -> LateSunrise -> Day; day [rise1, set0];
 *  the sunset ramp (set0, set1 + ext) Day -> EarlySunset -> Sunset ->
 *  LateSunset -> Night. Linear t inside a quarter. */
WwTodKeys wwTodKeys( double hour, const unsigned char tnam[4], double ext = 0.5 );

//! the vanilla sun disc position (the engine's tent arc), not normalised; z < 0 below the horizon
void wwVanillaSunPos( double hour, const unsigned char tnam[4], float pos[3] );
//! the direction TO the light (unit), the engine's biased and floored copy of the disc direction
void wwVanillaSunLightDir( double hour, const unsigned char tnam[4], float dir[3] );

/*! The sky game settings (lane PBRWX1). Defaults are the 1.10.155 .data values;
 *  EsmWeather::gmst() overwrites each one a loaded plugin sets (Fallout4.esm:
 *  fSunXExtreme 600, fSunYExtreme -325, fSunAlphaTransTime 0.15,
 *  fDaytimeColorExtension 2.0, fSunShadowScale -15, iSecundaSize 75,
 *  iMasserSize 90, the four moon angle fades 5 / 10). The moon fades have no
 *  measured exe value; 5 / 10 stands in (INFERRED, the ESM's own numbers). */
struct WwSkyGmst
{
	float sunX = 400.0f;		// fSunXExtreme
	float sunY = 25.0f;		// fSunYExtreme
	float alphaTrans = 2.0f;	// fSunAlphaTransTime, hours
	float colorExt = 0.5f;		// fDaytimeColorExtension, hours
	float shadowScale = 0.0f;	// fSunShadowScale, degrees
	float shadowMin = 30.0f;	// fSunShadowMinAngle, degrees
	float sunBase = 425.0f;	// fSunBaseSize (disc quad half-size)
	float sunGlare = 600.0f;	// fSunGlareSize (glare quad half-size)
	float cloudSpeedMax = 0.1f;	// fWeatherCloudSpeedMax
	float secundaSize = 40.0f;	// iSecundaSize
	float masserSize = 94.0f;	// iMasserSize
	float secundaFadeStart = 5.0f, secundaFadeEnd = 10.0f;
	float masserFadeStart = 5.0f, masserFadeEnd = 10.0f;
	QStringList fromEsm;	// the EditorIDs a loaded plugin set
	float dirFogPower = 8.0f;	// fDirectionalFogPower (lane FOG1; ESM and exe agree on 8)
	// lane WEATHERBLEND1: the weather change length, exe defaults (Fallout4.esm sets Max 3 and Accel 16)
	float transMax = 1.0f, transMin = 0.01f, transAccel = 4.0f;	// fWeatherTrans{Max,Min,Accel}
	float durMin = 1.0f, durMax = 23.0f;	// lane WEATHERBLEND5: fWeatherDuration{Min,Max}, hours (exe defaults)
	// lane SKYFULL1: the stars' turn, exe defaults (angle = 2 pi fmod(days, rotDays) / rotDays about the axis)
	float starsRotDays = 4.0f;	// fStarsRotateDays
	float starsAxis[3] = { 0.0f, 0.0f, 1.0f };	// fStarsRotate{X,Y,Z}Axis
	//! "name=value(esm|exe) ..." for the CLI and the census
	QString describe() const;
};

//! the engine's sky clock at one hour (sun, moon, stars)
struct WwSkyClock
{
	double hour = 12.0;
	double A = 0, B = 0, C = 0, D = 0;	// disc windows: fade in A..B, full B..C, fade out C..D
	float sunPos[3] = {};	// SunPos (x*X, Y, |X| - |x*X|), not normalised; the moon sits on it too
	float lightDir[3] = {};	// TO the light, unit: normalised, z + fSunShadowScale, floored at fSunShadowMinAngle
	float sunAlpha = 0.0f;	// the disc alpha
	float starsAlpha = 0.0f;	// the stars' alpha (the moon shadow disc takes min with it)
	WwTodKeys keys;	// the colour keys, fDaytimeColorExtension
};
WwSkyClock wwSkyClock( double hour, const unsigned char tnam[4], const WwSkyGmst & g );
//! the moon's alpha: fades in after D, out before A, by fAngleFadeStart/End x T/2
float wwMoonAlpha( const WwSkyClock & c, const WwSkyGmst & g, float fadeStart, float fadeEnd );
//! the moon phase: (int(days) mod 8L) / L, L = moons & 0x3F; -1 when L is 0 (no phase change)
int wwMoonPhase( double gameDays, unsigned char moons );
//! the phase's texture suffix: full, three_wan, half_wan, one_wan, new, one_wax, half_wax, three_wax
const char * wwMoonPhaseSuffix( int phase );
//! the cloud scroll offset: fract(speed * 0.1 * seconds), REAL seconds (the engine's constant 0.1)
float wwCloudOffset( float speed, double seconds );
//! elevation / azimuth of a sky vector, degrees (azimuth = atan2(x, y) mod 360: the numbers of spec_moon.md s2 / moon_model_out.txt, 180 at 01:00; its prose says -y, its table does not)
void wwSkyAngles( const float v[3], double * elevDeg, double * azimDeg );

//! the engine's CIELab conversions, 0..1 RGB
void wwRgbToLab( const float rgb[3], float lab[3] );
//! Lab -> RGB with the engine's sRGB encode and clamp
void wwLabToRgb( const float lab[3], float rgb[3] );
//! the engine's color blend: two keys blended in CIELab; bytes in, 0..1 out
void wwLabBlend( const unsigned char a[3], const unsigned char b[3], float t, float rgb[3] );
//! lane WEATHERBLEND1: the same blend over four colours by weight (a weather change: two weathers x two keys)
void wwLabBlend4( const unsigned char * const c[4], const float wt[4], float rgb[3] );

//! the climate the engine clock reads: TNAM 6 bytes (TNAM[5] = moons / phase length), FNAM, GNAM
struct WwClimateData
{
	unsigned char tnam[6] = { 30, 54, 102, 126, 0, 0x44 };
	QString edid;
	QString sunTex;	// FNAM, relative to Textures\ as stored
	QString glareTex;	// GNAM
	bool ok = false;
};

struct WwWeatherEntry
{
	quint32 formID = 0;
	QString edid;
	QString srcFile;	// basename of the file whose version won
	QString ownerFile;	// basename of the file the FormID's load-order byte names
	bool override() const { return srcFile.compare( ownerFile, Qt::CaseInsensitive ) != 0; }
};

/* lane VOLFOG1: one GDRY "God Rays" record (wbDefinitionsFO4 GDRY; the engine's GDRY loader,
 * MEASURED): DATA = back rgb, fwd rgb, intensity, air / back / fwd scale, back phase, air rgb (never read by the
 * engine), fwd phase; a shorter DATA keeps the engine's defaults for its tail. */
struct WwGodRays
{
	quint32 formID = 0;
	QString edid;
	float back[3] = { 1.0f, 1.0f, 1.0f };
	float fwd[3] = { 1.0f, 0.5f, 0.25f };
	float intensity = 1.0f, airScale = 3.0f, backScale = 2.0f, fwdScale = 4.0f, backPhase = 0.0f, fwdPhase = 0.75f;
};

/* lane VOLFOG1: the god-ray medium the engine builds from a GDRY (checked in Todd's treat, MEASURED):
 * air = (0.18, 0.46, 1) x air scale (Rayleigh), fwd = fwd colour x fwd scale (HG g = fwd phase), back = back colour
 * x back scale (HG g = back phase); post = the intensity. Without a GDRY the engine's fallback constants. */
struct WwGodRayMedium
{
	float air[3] = { 0.60f, 1.52f, 3.31f };
	float fwd[3] = { 2.0f, 2.0f, 2.0f };
	float back[3] = { 1.0f, 1.0f, 1.0f };
	float gFwd = 0.75f, gBack = 0.0f, post = 1.0f;
	bool fallback = true;
};
WwGodRayMedium wwGodRayMediumOf( const WwGodRays & g );
//! a weighted sum of two media (the engine's blend: every term linear in the weights)
WwGodRayMedium wwGodRayMix( const WwGodRayMedium & a, const WwGodRayMedium & b, float t );
//! read one GDRY; false when `formID` is not one
bool wwGodRaysRead( ESMFile & esm, quint32 formID, WwGodRays & out );

/* lane LIGHTS3 (LENSFLARE1): a LENS record (wbDefinitionsFO4 layout). Absent CNAM / DNAM keep the form's own
 * defaults, 0 and 1 (the constructor and loader, read in the exe; vanilla's 35 store neither). */
struct WwLensSprite
{
	QString texture;		//!< FNAM, as stored (under textures/)
	float tint[3] = { 1, 1, 1 };
	float width = 0, height = 0, position = 0, angularFade = 0, opacity = 0;
	quint32 flags = 0;		//!< 1 Rotates, 2 Shrinks When Occluded
};
struct WwLens
{
	quint32 formID = 0;
	QString edid;
	float colorInfluence = 0.0f;	//!< CNAM
	float fadeRadiusScale = 1.0f;	//!< DNAM, Fade Distance Radius Scale
	QVector<WwLensSprite> sprites;
};
//! read one LENS; false when `formID` is not one
bool wwLensRead( ESMFile & esm, quint32 formID, WwLens & out );
/* lane RAINFOG2: one SPGD (Shader Particle Geometry, a weather's precipitation shader): DATA's Type (0 Rain, 1 Snow) and
 * Particle Density (xEdit wbDefinitionsFO4; Fallout4.esm stores 96 bytes, each field followed by 4 unused bytes; a packed
 * 48-byte DATA is read too). false when `formID` is not one */
bool wwSpgdRead( ESMFile & esm, quint32 formID, int & type, float & density, QString & edid );

struct WwWeatherData
{
	quint32 formID = 0;
	QString edid;
	QString srcFile;
	int formVersion = 0;
	int nam0Size = 0;
	int nam0Rows = 0;	// 19 / 17
	int nam0Tods = 0;	// 8 / 4
	QByteArray nam0;	// raw bytes, as stored
	int dalcCount = 0;
	//! [tod][row] RGB bytes, rows past nam0Rows zero; 4-ToD records have Early/Late mapped to their neighbours
	unsigned char color[8][19][3] = {};
	//! [tod][axis 0..5 = X+ X- Y+ Y- Z+ Z-, 6 = specular] RGB bytes
	unsigned char dalc[8][7][3] = {};
	float dalcFresnel[8] = {};
	bool hasDalc = false;

	// lane PBRWX1: the sky, sun glare and cloud fields
	quint8 sunGlare = 0;	// DATA byte 4 (/255 = the glare strength)
	quint8 transDelta = 0;	// lane WEATHERBLEND1: DATA byte 3, Trans Delta (the change length, wwWeatherTransHours)
	float volMult = 1.0f;	// lane WEATHERBLEND5: VNAM, the volatility multiplier (wwWeatherDurationHours)
	// lane LIGHTS4: GNAM, the Sun Glare Lens Flare (a LENS; wbDefinitionsFO4 WTHR), read with the weather
	quint32 sunLensForm = 0;
	WwLens sunLens;
	// lane GRASSMB1: the wind, DATA bytes 0 (speed), 17 (direction), 18 (direction range), 19 (turbulence); /255 each
	quint8 windSpeed = 0, windDir = 0, windDirRange = 0, windTurb = 0;
	bool hasWind = false;
	// lane CLOUD5: DATA byte 11, the weather's class flags (1 Pleasant, 2 Cloudy, 4 Rainy, 8 Snow; xEdit wbDefinitionsFO4),
	// byte 10, Thunder/Lightning Frequency; the cloud type reads them (gl/cloudmath.h lowTypeOf)
	quint8 classFlags = 0, thunderFreq = 0;
	bool hasClass = false;
	/* lane CLOUD6f: the lightning (gl/cloudlightning.h): DATA bytes 8 / 9 (Thunder/Lightning Begin Fade In / End Fade
	 * Out, /255 of a change), 12..14 (Lightning Color); thunderSounds = the SNAM sounds of type 3 (thunder): a weather with
	 * one storms (15 of 107 vanilla; the thunder byte is 133..255 in all 107 and tells nothing apart) */
	quint8 thunderFadeIn = 0, thunderFadeOut = 0;
	quint8 lightningRgb[3] = {};
	int thunderSounds = 0;
	// lane CLOUD6e: MNAM, the precipitation shader (an SPGD; 0 = none): the rain fog's "this weather rains"
	quint32 precipForm = 0;
	// lane RAINFOG2: that SPGD's DATA Type (0 rain, 1 snow; -1 not read) and Particle Density (-1 not read), its EDID
	int precipType = -1;
	float precipDensity = -1.0f;
	QString precipEdid;
	quint32 imsp[8] = {};	// IMSP: the IMGS per ToD (mapped FormIDs; 4-slot records mapped to 8)
	float skyScale[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };	// IMGS HNAM[7] per ToD (1.0 where absent)
	int skyScaleFound = 0;	// how many ToD slots had an IMGS with HNAM
	/* lane SUNCELL1: the whole IMGS per ToD, for the cell view's exterior imagespace (celllights.h's isHdr
	 * layout: HNAM 9 floats, CNAM saturation/brightness/contrast, TNAM amount/r/g/b, TX00 the LUT strip) */
	bool isFound[8] = {};
	bool isDefault[8] = {};	// lane WEATHERBLEND3: no IMGS in this slot, the game's default (form 0x167) read instead
	float isHdr[8][9] = {};
	float isCine[8][3] = {};
	float isTint[8][4] = {};
	QString isEdid[8], isLut[8];
	float dof[8][4] = {};	// lane GRASSMB1: IMGS DNAM per ToD (strength, distance, range, the flags float); 0 = none
	bool hasDof[8] = {};
	QString cloudTex[32];	// x0TX, as stored (relative to Textures\)
	quint32 cloudLayers = 16;	// LNAM
	bool hasLnam = false;
	quint32 cloudNam1 = 0;	// NAM1 as stored
	quint32 cloudDisabled = 0;	// the engine's effective mask: NAM1 | ~present-texture mask
	unsigned char cloudColor[8][32][3] = {};	// PNAM
	float cloudAlpha[8][32] = {};	// JNAM
	quint8 cloudSpeedX[32] = {};	// QNAM (ONAM b/2+127 when only ONAM is present)
	quint8 cloudSpeedY[32] = {};	// RNAM

	// lane FOG1: the fog fields (scratchpad/pbrprep1_20260924/spec_fog.md 1.1, 1.3)
	//! FNAM as stored, padded with the engine defaults (InitializeData); when FNAM is not
	//! 72 bytes the far height pair [14..17] is a copy of the near pair [8..11]
	float fog[18] = { 0, 0, 0, 0, 1, 1, 1, 1, 0, 10000, 0, 10000, 1, 1, 0, 10000, 0, 10000 };
	int fogFnamSize = 0;	// 72 / 56 / 32 (0 = no FNAM)
	//! NAM4, [k][tod], k = FogNear, FogFar, FogNearHigh, FogFarHigh; 1.0 where absent
	float fogScale[4][8] = { { 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1 },
		{ 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1 } };
	bool hasNam4 = false;

	// lane VOLFOG1: WGDR, the GDRY per ToD (the WGDR order = the IMSP order), and the medium each builds
	quint32 wgdr[8] = {};
	WwGodRayMedium godRay[8];
	int godRayFound = 0;	// slots whose link resolved to a GDRY

	/* lane WEATHERBLEND1: a weather change in progress: the weather the sky is leaving and this weather's share (the
	 * game's linear transition pct). The blends below read both while blending() */
	std::shared_ptr<const WwWeatherData> last;
	float transPct = 1.0f;
	bool blending() const { return last && transPct < 1.0f; }
};

//! lane WEATHERBLEND1: the game's weather change length in game hours, Trans Delta / 255 x (Max - Min) + Min
double wwWeatherTransHours( quint8 transDelta, const WwSkyGmst & g );
//! the new weather's share after `gameHours` of the change, 0..1 (linear, no ease, as the game's)
float wwWeatherTransPct( double gameHours, quint8 transDelta, const WwSkyGmst & g );
//! wall seconds -> game hours at the game's default timescale (20)
double wwWeatherTransGameHours( double seconds );
//! the four colour weights: current keys a, b x pct, then the leaving weather's keys a, b x (1 - pct)
void wwWeatherTransWeights( const WwTodKeys & k, float pct, float wt[4] );
/*! lane WEATHERBLEND2: the game's imagespace slots (up to four LUTs): this weather's keys a, b x pct, the left one's x
 *  (1 - pct); tc / tl = each weather's key t. A slot the game skips weighs 0 (b at t 0, the left ones at pct 1) */
void wwImageSpaceSlots( float tc, float tl, float pct, bool last, float w[4] );
//! up to four 16^3 RGB8 LUTs by weight, over the ones given (renormalised); empty when none
void wwLutBlendN( const std::vector<unsigned char> * const lut[4], const float w[4], std::vector<unsigned char> & out );
//! fWeatherTransAccel: once accelerated at pct `from` (>= 0), a change's pct runs (Accel + 1) times as fast from there
float wwWeatherTransAccelPct( float raw, float from, const WwSkyGmst & g );
//! what a weather pick does: the game's Sky::UpdateWeather / ForceWeather order
enum WwWeatherPick { WwPickSnap, WwPickStart, WwPickDefer, WwPickKeep };
int wwWeatherPickRule( bool changing, bool sameAsCurrent, bool interior, bool force );
/*! lane WEATHERBLEND3: the Sky flags a pick meets (UpdateWeather): fast travel applies a waiting pick at once and snaps;
 *  wait / teleport (PassTime, PositionPlayer) snap a pick that would start a change */
enum WwWeatherSnapFlag : unsigned { WwSnapFastTravel = 0x10u, WwSnapPassTime = 0x1000000u };
int wwWeatherSnapRule( int rule, unsigned flags );
//! the game hours a wait from hour `from` to hour `to` passes (time only runs forward)
double wwWeatherWaitHours( double from, double to );
//! the integral of the change's pct over its first `secs` real seconds (a wait's hours ahead), with the accel
double wwWeatherTransPctSeconds( double secs, double waitHours, quint8 transDelta, const WwSkyGmst & g, float accelFrom );
//! a layer's ONE scroll offset (the game's): base + old speed x secs + (new - old) x the pct integral, wrapped
float wwCloudScroll( double base, float vOld, float vNew, double seconds, double pctSeconds );
//! the wind speed / turbulence during a change: floats on pct, unrounded (the game's)
float wwWeatherWindBlend( quint8 cur, quint8 last, float pct );
//! the game's random range offset (degrees): the nearest to 0 of 4 draws in +- range / 2; seeded, so a view repeats
float wwWindRangeOffset( quint8 dirByte, quint8 rangeByte, quint32 seed );
/*! lane WEATHERBLEND4: an hour box move as a step round the clock, the shortest way, in (-12, 12]: a forward step is a
 *  wait of that many hours, a backward drag (or none) waits nothing */
double wwWeatherHourStep( double from, double to );
//! lane WEATHERBLEND4: one RDWT entry {weather, chance}; the chance is its global's value when the entry names one
struct WwRegionWeatherEntry
{
	quint32 weather = 0;
	qint32 chance = 0;
};
/*! the game's region list pick (GetRandomWeather): r % the chances' sum, walked over the entries' bins in order; a
 *  chance <= 0 holds no bin. 0 = nothing to pick */
quint32 wwRegionWeatherPick( const WwRegionWeatherEntry * e, int n, quint32 r );
//! a region with weather data the point is inside: its RDAT override flag and priority
struct WwRegionRank
{
	bool overrides = false;
	int priority = 0;
};
/*! the game's weather region (PlayerRegionState): the first wins, unless a later one overrides and it does not, or
 *  both override alike and the later has the higher priority. -1 = none */
int wwRegionChoose( const WwRegionRank * c, int n );
//! a seed's draw for wwRegionWeatherPick (xorshift32 stands in for the game's BSRandom)
quint32 wwRegionDraw( quint32 seed );
//! lane WEATHERBLEND4: a REGN's weather data (RDAT type 3, RDWT) and whether a point is inside its areas (RPLD)
struct WwRegionWeather
{
	bool hasWeather = false, inside = false;
	WwRegionRank rank;
	QVector<WwRegionWeatherEntry> list;
	QString edid;
	// lane WEATHERBLEND5: the record's deleted flag (0x20), its worldspace (WNAM, 0 = none), whether it has areas, its form
	bool deleted = false, areas = false;
	quint32 world = 0, form = 0;
};
//! lane WEATHERBLEND5: the region's reds (WW_WEATHER_BLEND_RED): no area counts as inside; deleted / other world kept
enum WwRegionRed : int { WwRegionRedNoAreaInside = 1, WwRegionRedNoSkip = 2 };
/*! lane WEATHERBLEND5: whether a region takes part in the weather choice (PlayerRegionState): weather data, the point
 *  inside one of its areas (none = never inside), not deleted, and no worldspace or the cell's (cellWorld 0 = any) */
bool wwRegionCounts( const WwRegionWeather & g, quint32 cellWorld, int red = 0 );
/*! the weather's duration in game hours (Sky::GetCurrentWeatherDuration): v = clamp(climate volatility (TNAM byte 4)
 *  x VNAM, 0, 255); Min + (Max - Min)(255 - v)/255, at least 0.1 */
double wwWeatherDurationHours( int climVolatility, float volMult, const WwSkyGmst & g );
//! the hours since the weather began (Sky::GetCurrentWeatherTimeElapsed: +24 when the clock went past midnight)
double wwWeatherElapsedHours( double hourNow, double startHour );
/*! the region weather the game applies (Sky::UpdateWeather): the same region listing cur keeps it until its duration
 *  ran out; otherwise the list's pick (draw), and a new region listing cur keeps it when u01 > elapsed / duration.
 *  0 = nothing to apply */
quint32 wwRegionWeatherFollow( const WwRegionWeatherEntry * e, int n, quint32 cur, bool sameRegion, double elapsed,
	double duration, quint32 draw, double u01 );
/*! lane WEATHERBLEND6: the region walk. A streamed cell view's camera moves like the player; the region choice runs
 *  again when the camera's cell or the set of regions counting under it changes, sampled at most once per
 *  kWwRegionWalkMs and only after the camera moved (never per frame, never per region polygon each frame) */
const qint64 kWwRegionWalkMs = 250;
//! lane WEATHERBLEND6: WW_CELL_REGION_RED=spawnonly: the spawn pick only, no re-pick on a border (the old view)
enum WwRegionWalkRed : int { WwRegionWalkRedSpawnOnly = 1 };
struct WwRegionWalk
{
	bool started = false;	//!< a sample set the baseline below
	int cx = 0, cy = 0;	//!< the camera's cell at the last sample
	float eye[2] = { 0, 0 };	//!< the eye (world x, y) at the last sample
	qint64 lastMs = 0;
	QVector<quint32> under;	//!< the regions counting at the eye (wwRegionCounts), sorted
	quint32 region = 0;	//!< the region whose weather is current: the spawn pick's or the last re-pick's (0 none)
	int samples = 0, repicks = 0;
};
//! whether the walk samples now: the first time, or kWwRegionWalkMs after the last and the cell changed or the eye moved
bool wwRegionWalkDue( const WwRegionWalk & w, qint64 nowMs, int cx, int cy, const float eye[2] );
/*! one sample: the regions counting at the eye and their winner (wwRegionChoose; 0 none). It becomes the baseline
 *  (the first sample's winner the current region, never a re-pick); true = a re-pick: the cell or the set changed and
 *  the winner is not the current region (it becomes current). Red spawnonly: never */
bool wwRegionWalkSample( WwRegionWalk & w, qint64 nowMs, int cx, int cy, const float eye[2], QVector<quint32> under,
	quint32 chosen, int red = 0 );
/*! a re-pick's weather: the follow law on entering a new region (sameRegion false; 0 = nothing to apply), and why it
 *  kept the current weather or changed it (the list, the duration rule, u01 against elapsed / duration) */
quint32 wwRegionWalkFollow( const WwRegionWeatherEntry * e, int n, quint32 cur, double elapsed, double duration,
	quint32 draw, double u01, QString * why = nullptr );

/*! The engine fog at one hour (lane FOG1, spec_fog.md 2.1-2.3): the FNAM floats
 *  blended day <-> night on the engine's fog day weight, the four NAM0 fog colours
 *  (rows 1 / 12 / 17 / 18) blended in CIELab on the colour keys, times the NAM4
 *  scale blended on the same keys, then pow 2.2; and the cb12 packing the
 *  shader reads (K = cb12[41..46]). */
struct WwFog
{
	double hour = 12.0;
	float ext = 0.5f;	// fDaytimeColorExtension the weight used
	float w = 1.0f;	// the day weight (NOT the colour keys' four-quarter ramp)
	float fogNear = 0, fogFar = 0, power = 1, maxv = 1, hds = 1;
	float nMid = 0, nRange = 10000, fMid = 0, fRange = 10000;
	WwTodKeys keys;	// the colour keys (engine clock, fDaytimeColorExtension)
	float scale[4] = { 1, 1, 1, 1 };	// the NAM4 scale at the keys: near, far, nearHigh, farHigh
	float nearLow[3] = {}, farLow[3] = {}, nearHigh[3] = {}, farHigh[3] = {};	// linear
	bool effOff = false;	// near == far == 0: packed as 1e8 / 1e9 (fog effectively off)
	float K[6][4] = {};	// cb12[41..46]
	QString describe() const;
};
//! the engine's fog day weight: linear ramps rb..re up, sb..se down (rb, se widened by ext)
float wwFogDayWeight( double hour, const unsigned char tnam[4], double ext );
WwFog wwFogAt( const WwWeatherData & w, double hour, const unsigned char tnam[4], const WwSkyGmst & g );
//! K from the scalars and the (linear) colours above; wwFogAt's tail, shared with the interior fog (lane FOG2)
void wwFogPackK( WwFog & o );
//! one fragment through the engine fog formula (spec_fog.md 2.4, before the sun term), from the packed K
struct WwFogSample
{
	float ramp = 0, f = 0, hb = 0, intensity = 0, alpha = 0;
	float color[3] = {};
};
WwFogSample wwFogSample( const WwFog & fog, float d, float z );

class EsmWeather
{
public:
	EsmWeather();
	~EsmWeather();

	/*! Load `files` (full paths, load order). Refuses, with a named reason and
	 *  nothing loaded, for a missing or out-of-order master, > 256 files, or a
	 *  file that is not a TES4 plugin. */
	bool load( const QStringList & files, QString * refusal );
	/*! lane SKYTOP1: the same load from the whole load order, reading of each plugin only its TES4 header and
	 *  the top groups the lookdev reads (WTHR CLMT IMGS GMST GDRY LENS REGN), never the whole file. Every plugin
	 *  stays in the list (the FormID masters map by position), and the last plugin's version of a record wins,
	 *  as in the game. Red WW_LOOKDEV_RED=firstwins drops each later plugin's overrides (the first one wins). */
	bool loadGroups( const QStringList & files, QString * refusal );
	//! after loadGroups: bytes held in memory, bytes the files hold in all, milliseconds the read took
	qint64 groupBytes() const { return imgBytes; }
	qint64 fileBytes() const { return allBytes; }
	qint64 loadMs() const { return ms; }
	bool isLoaded() const { return bool( esm ); }
	QStringList files() const { return loaded; }
	QString loadSummary() const;

	QVector<WwWeatherEntry> list();
	//! EditorID or hex FormID (with or without 0x); 0 = not found
	quint32 find( const QString & key );
	bool read( quint32 formID, WwWeatherData & out, QString * why );
	//! CLMT TNAM of `formID` (0 = DefaultClimate 0000015F); false + why if absent
	bool climate( quint32 formID, unsigned char tnam[4], QString * edid, QString * why );
	//! the whole climate the engine clock needs (TNAM 6 bytes, FNAM, GNAM); 0 = DefaultClimate
	bool climateData( quint32 formID, WwClimateData & out, QString * why );
	/*! lane LIGHTS3 (XEMI-REGN): the weather a REGN's emittance follows: its RDAT type 3 (RDWT) list, the highest
	 *  chance (the game picks by chance at random); none listed = DefaultWeather 0000015E. 0 = not a REGN. */
	quint32 regionWeather( quint32 regn, int * listed = nullptr );
	//! lane WEATHERBLEND4: the region's weather list, rank and whether world (x, y) is inside it; false = not a REGN
	bool regionWeatherAt( quint32 regn, float x, float y, WwRegionWeather & out );
	//! the sky GMSTs of the loaded plugins, exe defaults where none sets one
	WwSkyGmst gmst();

	//! header-only master check of a load list; empty = fine
	static QString checkMasters( const QStringList & files );
	//! the masters one plugin names (TES4 MAST), empty on any read failure
	static QStringList mastersOf( const QString & file, QString * why = nullptr );
	/*! The load list for ONE chosen plugin: its masters (recursively, in MAST
	 *  order, looked up beside it and then in `dataDir`), then itself. A master
	 *  that is found nowhere stays in the list under its bare name, so the load
	 *  refuses naming it. */
	static QStringList loadListFor( const QString & plugin, const QString & dataDir );
	//! the first Data folder the game manager serves that holds Fallout4.esm (empty if none)
	static QString gameDataDir();

	//! blend helper: linear-interpolated bytes (0..255 floats) of a colour row
	static void blendRow( const WwWeatherData & w, int row, const WwTodKeys & k, float rgb[3] );
	static void blendDalc( const WwWeatherData & w, int axis, const WwTodKeys & k, float rgb[3] );
	//! the engine's blend of a NAM0 row: CIELab, 0..255 floats out
	static void blendRowLab( const WwWeatherData & w, int row, const WwTodKeys & k, float rgb[3] );
	/*! one cloud layer at the keys: colour (CIELab, 0..255 floats) and alpha (float blend).
	 *  A layer >= LNAM reads layer 0 (the engine clamps the index). */
	static void blendCloud( const WwWeatherData & w, int layer, const WwTodKeys & k, float rgb[3], float * alpha );
	//! the IMGS Sky Scale at the keys (linear blend, as the engine's)
	static float blendSkyScale( const WwWeatherData & w, const WwTodKeys & k );
	//! a cloud speed byte -> uv speed: fWeatherCloudSpeedMax * (2b/254 - 1)
	static float cloudSpeed( quint8 b, const WwSkyGmst & g );

private:
	std::vector<QByteArray> images;	// lane SKYTOP1: loadGroups' plugin images; declared before esm (it outlives them)
	qint64 imgBytes = 0, allBytes = 0, ms = 0;
	std::unique_ptr<ESMFile> esm;
	QStringList loaded;
};

//! `NifSkope -no-gui weather ...` (the W1 gates' reader); see esmweather.cpp for the arguments
int cmdWeather( const QStringList & args );

#endif
