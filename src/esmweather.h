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
 *   - the time-of-day blend (engine GetTimes: 0.5 h extension, four-quarter
 *     ramps; FO4CS docs/RE/solar-daynight.md) and the vanilla sun: the tent arc
 *     of Sun::Update (fSunXExtreme 400, fSunYExtreme 25) and the light's bias and
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
 *     wins), the 1.10.155 .data values when none sets them (WwSkyGmst);
 *   - Sun::Update's arc: midpoint windows A..D, the night branch, the disc
 *     fade over fSunAlphaTransTime, the light's bias and floor (WwSkyClock);
 *   - the time-of-day colour blend in CIELab, exactly as Sky::SetColor
 *     0x6510e0@155 does it (ColorRGBtoCIELab 0x657ac0, ColorCIELabToRGB
 *     0x657880, constants read from the exe);
 *   - the WTHR cloud layers (x0TX, LNAM, NAM1, PNAM, JNAM, QNAM, RNAM, ONAM),
 *     the sun-glare byte (DATA[4]) and the IMGS Sky Scale (IMSP -> HNAM[7]);
 *   - the moon: phase from the game day (Moon::UpdatePhase), alpha by the
 *     angle fades (Moon::Update), position = the sun's (same arc).
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

class ESMFile;

//! time-of-day slot order of every 8-slot WTHR array (xEdit wbDefinitionsCommon.pas:8634-8643)
enum WwTod { WwTodSunrise = 0, WwTodDay = 1, WwTodSunset = 2, WwTodNight = 3,
	WwTodEarlySunrise = 4, WwTodLateSunrise = 5, WwTodEarlySunset = 6, WwTodLateSunset = 7 };
const char * wwTodName( int slot );

//! NAM0 row names (xEdit wbWeatherColors, wbDefinitionsCommon.pas:9707-9757)
enum WwNam0Row { WwRowSkyUpper = 0, WwRowFogNear = 1, WwRowAmbient = 3, WwRowSunlight = 4, WwRowSun = 5,
	WwRowStars = 6, WwRowSkyLower = 7, WwRowHorizon = 8, WwRowSunGlare = 15, WwRowMoonGlare = 16,
	WwRowFogFar = 12, WwRowFogNearHigh = 17, WwRowFogFarHigh = 18 };

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

//! the vanilla sun disc position (Sun::Update tent arc), not normalised; z < 0 below the horizon
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
	//! "name=value(esm|exe) ..." for the CLI and the census
	QString describe() const;
};

//! the engine's sky clock at one hour (Sun::Update / Moon::Update / Stars::Update)
struct WwSkyClock
{
	double hour = 12.0;
	double A = 0, B = 0, C = 0, D = 0;	// disc windows: fade in A..B, full B..C, fade out C..D
	float sunPos[3] = {};	// SunPos (x*X, Y, |X| - |x*X|), not normalised; the moon sits on it too
	float lightDir[3] = {};	// TO the light, unit: normalised, z + fSunShadowScale, floored at fSunShadowMinAngle
	float sunAlpha = 0.0f;	// the disc alpha
	float starsAlpha = 0.0f;	// Stars::Update's alpha (the moon shadow disc takes min with it)
	WwTodKeys keys;	// the colour keys, fDaytimeColorExtension
};
WwSkyClock wwSkyClock( double hour, const unsigned char tnam[4], const WwSkyGmst & g );
//! Moon::Update's alpha: fades in after D, out before A, by fAngleFadeStart/End x T/2
float wwMoonAlpha( const WwSkyClock & c, const WwSkyGmst & g, float fadeStart, float fadeEnd );
//! Moon::UpdatePhase: (int(days) mod 8L) / L, L = moons & 0x3F; -1 when L is 0 (no phase change)
int wwMoonPhase( double gameDays, unsigned char moons );
//! the Moon::Phase suffix: full, three_wan, half_wan, one_wan, new, one_wax, half_wax, three_wax
const char * wwMoonPhaseSuffix( int phase );
//! the cloud scroll offset: fract(speed * 0.1 * seconds), REAL seconds (Clouds::Update, 0.1 at 0x2c5442c@155)
float wwCloudOffset( float speed, double seconds );
//! elevation / azimuth of a sky vector, degrees (azimuth = atan2(x, y) mod 360: the numbers of spec_moon.md s2 / moon_model_out.txt, 180 at 01:00; its prose says -y, its table does not)
void wwSkyAngles( const float v[3], double * elevDeg, double * azimDeg );

//! the engine's CIELab conversions, 0..1 RGB (ColorRGBtoCIELab 0x657ac0 / ColorCIELabToRGB 0x657880 @155)
void wwRgbToLab( const float rgb[3], float lab[3] );
//! Lab -> RGB with the engine's sRGB encode and clamp (NiColor::Clamp)
void wwLabToRgb( const float lab[3], float rgb[3] );
//! Sky::SetColor: two keys blended in CIELab; bytes in, 0..1 out
void wwLabBlend( const unsigned char a[3], const unsigned char b[3], float t, float rgb[3] );

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
	quint32 imsp[8] = {};	// IMSP: the IMGS per ToD (mapped FormIDs; 4-slot records mapped to 8)
	float skyScale[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };	// IMGS HNAM[7] per ToD (1.0 where absent)
	int skyScaleFound = 0;	// how many ToD slots had an IMGS with HNAM
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
};

/*! The engine fog at one hour (lane FOG1, spec_fog.md 2.1-2.3): the FNAM floats
 *  blended day <-> night on Sky::UpdateFog's weight, the four NAM0 fog colours
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
//! Sky::UpdateFog's day weight: linear ramps rb..re up, sb..se down (rb, se widened by ext)
float wwFogDayWeight( double hour, const unsigned char tnam[4], double ext );
WwFog wwFogAt( const WwWeatherData & w, double hour, const unsigned char tnam[4], const WwSkyGmst & g );
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
	//! the engine's blend of a NAM0 row: CIELab (Sky::SetColor), 0..255 floats out
	static void blendRowLab( const WwWeatherData & w, int row, const WwTodKeys & k, float rgb[3] );
	/*! one cloud layer at the keys: colour (CIELab, 0..255 floats) and alpha (float blend).
	 *  A layer >= LNAM reads layer 0 (GetCloudColor / GetCloudAlpha clamp). */
	static void blendCloud( const WwWeatherData & w, int layer, const WwTodKeys & k, float rgb[3], float * alpha );
	//! the IMGS Sky Scale at the keys (linear blend, Sky::UpdateHDRValues)
	static float blendSkyScale( const WwWeatherData & w, const WwTodKeys & k );
	//! a cloud speed byte -> uv speed: fWeatherCloudSpeedMax * (2b/254 - 1)
	static float cloudSpeed( quint8 b, const WwSkyGmst & g );

private:
	std::unique_ptr<ESMFile> esm;
	QStringList loaded;
};

//! `NifSkope -no-gui weather ...` (the W1 gates' reader); see esmweather.cpp for the arguments
int cmdWeather( const QStringList & args );

#endif
