import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/src/esmweather.h'
s = open(P, 'rb').read().decode('utf-8')
cr0 = s.count('\r')

def rep(old, new):
    global s
    assert s.count(old) == 1, (old[:60], s.count(old))
    s = s.replace(old, new)

rep(""" *   hourstuck the hour is ignored, always 12:00 (G7 must fail)
 */
""", """ *   hourstuck the hour is ignored, always 12:00 (G7 must fail)
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
 */
""")

rep("""enum WwNam0Row { WwRowSkyUpper = 0, WwRowFogNear = 1, WwRowAmbient = 3, WwRowSunlight = 4, WwRowSun = 5,
	WwRowSkyLower = 7, WwRowHorizon = 8 };""",
"""enum WwNam0Row { WwRowSkyUpper = 0, WwRowFogNear = 1, WwRowAmbient = 3, WwRowSunlight = 4, WwRowSun = 5,
	WwRowStars = 6, WwRowSkyLower = 7, WwRowHorizon = 8, WwRowSunGlare = 15, WwRowMoonGlare = 16 };""")

rep("""//! the direction TO the light (unit), the engine's biased and floored copy of the disc direction
void wwVanillaSunLightDir( double hour, const unsigned char tnam[4], float dir[3] );
""", """//! the direction TO the light (unit), the engine's biased and floored copy of the disc direction
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
//! elevation / azimuth of a sky vector, degrees (azimuth = atan2(x, -y), the spec's convention)
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
	QString sunTex;	// FNAM, relative to Textures\\ as stored
	QString glareTex;	// GNAM
	bool ok = false;
};
""")

rep("""	float dalcFresnel[8] = {};
	bool hasDalc = false;
};""", """	float dalcFresnel[8] = {};
	bool hasDalc = false;

	// lane PBRWX1: the sky, sun glare and cloud fields
	quint8 sunGlare = 0;	// DATA byte 4 (/255 = the glare strength)
	quint32 imsp[8] = {};	// IMSP: the IMGS per ToD (mapped FormIDs; 4-slot records mapped to 8)
	float skyScale[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };	// IMGS HNAM[7] per ToD (1.0 where absent)
	int skyScaleFound = 0;	// how many ToD slots had an IMGS with HNAM
	QString cloudTex[32];	// x0TX, as stored (relative to Textures\\)
	quint32 cloudLayers = 16;	// LNAM
	bool hasLnam = false;
	quint32 cloudNam1 = 0;	// NAM1 as stored
	quint32 cloudDisabled = 0;	// the engine's effective mask: NAM1 | ~present-texture mask
	unsigned char cloudColor[8][32][3] = {};	// PNAM
	float cloudAlpha[8][32] = {};	// JNAM
	quint8 cloudSpeedX[32] = {};	// QNAM (ONAM b/2+127 when only ONAM is present)
	quint8 cloudSpeedY[32] = {};	// RNAM
};""")

rep("""	bool climate( quint32 formID, unsigned char tnam[4], QString * edid, QString * why );
""", """	bool climate( quint32 formID, unsigned char tnam[4], QString * edid, QString * why );
	//! the whole climate the engine clock needs (TNAM 6 bytes, FNAM, GNAM); 0 = DefaultClimate
	bool climateData( quint32 formID, WwClimateData & out, QString * why );
	//! the sky GMSTs of the loaded plugins, exe defaults where none sets one
	WwSkyGmst gmst();
""")

rep("""	static void blendDalc( const WwWeatherData & w, int axis, const WwTodKeys & k, float rgb[3] );
""", """	static void blendDalc( const WwWeatherData & w, int axis, const WwTodKeys & k, float rgb[3] );
	//! the engine's blend of a NAM0 row: CIELab (Sky::SetColor), 0..255 floats out
	static void blendRowLab( const WwWeatherData & w, int row, const WwTodKeys & k, float rgb[3] );
	/*! one cloud layer at the keys: colour (CIELab, 0..255 floats) and alpha (float blend).
	 *  A layer >= LNAM reads layer 0 (GetCloudColor / GetCloudAlpha clamp). */
	static void blendCloud( const WwWeatherData & w, int layer, const WwTodKeys & k, float rgb[3], float * alpha );
	//! the IMGS Sky Scale at the keys (linear blend, Sky::UpdateHDRValues)
	static float blendSkyScale( const WwWeatherData & w, const WwTodKeys & k );
	//! a cloud speed byte -> uv speed: fWeatherCloudSpeedMax * (2b/254 - 1)
	static float cloudSpeed( quint8 b, const WwSkyGmst & g );
""")

assert s.count('\r') == cr0
open(P, 'wb').write(s.encode('utf-8'))
print('ok header', len(s))
