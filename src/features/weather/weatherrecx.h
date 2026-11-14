#ifndef WW_WEATHERRECX_H
#define WW_WEATHERRECX_H

/* lane WX1: the Lookdev weather's .recx blocks atmosphere / clouds / night (recx.h; spec FO4-Extension-Formats
 * recx/SPEC.md "atmosphere, clouds, night (Weathers)"). Per key: the file's value > the record's own data > the
 * default; the users (atmosphere.cpp, cloudmap.cpp, lookdevstage.cpp) apply a value only where it is set.
 * Show row "Weather .recx (sky, clouds, night)", pin WW_WEATHER_RECX; red WW_WEATHER_RECX_RED = ignore
 * (no file read) | recordfirst (the record's fog and cloud cover win over the file). Files re-read on a change
 * (wwRecxScan, 500 ms), no reopen. Census on stderr: "weather recx: ...".
 *
 * lane WEATHERRECX1 (bungo 11-06, SPEC.md 1.3): the weather .recx OWNS every condition, in physical units: the
 * blocks atmosphere (+ hazeType, humidity, groundAlbedo, angstrom, hazeAnisotropy, airglow, hazeAlbedo,
 * useRecordColors), clouds, night, precipitation, wind, temperature, lightning, transitions, textures. Per key:
 * a file's value > the GENERATED RUNG (weathergen.h, from the record's class; never a colour or fog distance) >
 * none. The row ships ON. While a change blends, both weathers resolve and every number blends by the new one's pct.
 * The record's own path is one red per channel: WW_WEATHER_RECORD_RED = sky,sun,ambient,fog,clouds,imagespace,
 * lightning,precip,wind,transitions (a comma list, or "all"), or per weather atmosphere.useRecordColors. One line per
 * weather open on stderr: "weather recx open: ..." naming each input's source. */

#include <QString>
#include <QStringList>

struct WwWeatherRecx
{
	bool on = false;	//!< the row is on and a file set at least one value
	bool recordFirst = false;	//!< red recordfirst
	// set values; < 0 = not set (the record and the default decide)
	float visibilityM = -1.0f, hazeHeightM = -1.0f, ozone = -1.0f;
	float coverage = -1.0f, density = -1.0f;
	float type = -1.0f;	//!< lane CLOUD5: clouds.type 0..1 (or a word: stratus, stratocumulus, cumulus, cumulonimbus)
	float layerBase[32], layerTop[32];	//!< metres above sea level; < 0 = not set
	int layers = 0;	//!< how many layers have heights
	float stars = -1.0f, moonlight = -1.0f;
	// lane WEATHERRECX1: the 1.3 keys (< 0 / empty = not set)
	QString hazeType;	//!< clean, maritime, urban, dust, smoke, glowing
	float humidity = -1.0f, groundAlbedo = -1.0f, angstrom = -1.0f, hazeAnisotropy = -1.0f, airglow = -1.0f;
	float hazeAlbedo[3] = { -1.0f, -1.0f, -1.0f };	//!< single-scattering albedo at 680 / 550 / 440 nm
	/* lane NIGHTHAZE1 (UE's volumetric fog model): hazeGlow = the haze's own light (a thick haze glows at it, in units of
	 * the natural night sky's floor luminance); cloudAlbedo = clouds.albedo; inscatterColor / directionalInscatter (+
	 * exponent, start metres) = the last-resort overrides UE's height fog has, off unless a file sets them. < 0 = not set */
	float hazeGlow[3] = { -1.0f, -1.0f, -1.0f }, cloudAlbedo[3] = { -1.0f, -1.0f, -1.0f };
	float inscatterColor[3] = { -1.0f, -1.0f, -1.0f }, dirInscatter[3] = { -1.0f, -1.0f, -1.0f };
	float dirInscatterExp = -1.0f, dirInscatterStart = -1.0f;
	float windSpeed = -1.0f, windDirection = -1.0f, gusts = -1.0f, turbulence = -1.0f;	//!< m/s, degrees FROM
	float airTemp = -1000.0f;	//!< deg C; <= -999 = not set
	float lightningFreq = -1.0f, cloudToGround = -1.0f, lightningDistance = -1.0f;	//!< flashes / min, share, km
	float transHours = -1.0f, precipStart = -1.0f, precipEnd = -1.0f, lightningStart = -1.0f, lightningEnd = -1.0f;
	float wetting = -1.0f, drying = -1.0f;	//!< minutes
	float rainRate = -1.0f, snowRate = -1.0f;	//!< mm/h, the layers summed per family (rain + hail, snow_*)
	QString precipLayer;	//!< the first layer's type word
	/* lane CLOUDRECX1 (SPEC.md 1.4 clouds; red WW_CLOUD_RECX14_RED=1 = the 1.3 reader: none of these read). < 0 = not set */
	float liquidWater = -1.0f, dropletRadius = -1.0f;	//!< g/m^3, um (the weather's main cloud)
	float iceFraction = -1.0f, evolution = -1.0f, precipitationBase = -1.0f;	//!< 0..1, minutes, metres above sea level
	/* MODULES1 10c: the cloud shadow law's inputs (clouds.shadowOpacity 0..1, shadowStrength 0..8, shadowLayerDensity
	 * percent 25..400 (the exponent on a layer's alpha = 100 / percent), shadowDeck metres 200..20000: the highest
	 * layer's sheet). < 0 = not set: cloudfusionlaw.h's constants. Read by cloudshadow.cpp; red WW_CLOUDSHADOW_RED=norecx */
	float shadowOpacity = -1.0f, shadowStrength = -1.0f, shadowLayerDensity = -1.0f, shadowDeck = -1.0f;
	float layerLwc[32], layerRadius[32], layerIce[32], layerAnvil[32], layerType[32];	//!< per layer; < 0 = not set
	bool densityLegacy = false;	//!< clouds.density came from a file (1.4: the legacy alias, warned)
	float densityGen = -1.0f;	//!< the density the generated rung (or the file's legacy key) gave, before the 1.4 factor
	float densityRatio = 1.0f;	//!< sigma(file) / sigma(type default): density = densityGen x this
	QString textures[9];	//!< SPEC.md textures slots (wwWeatherRecxTextureSlot); empty = the shared default
	QString genClass, genRegion;	//!< the generated rung's class and region for this weather
	unsigned recordRed = 0;	//!< WwRecordChannel bits whose record path is used (env red or useRecordColors)
	/* lane AIRPERF1 A9 (bungo 11-11, SPEC.md 1.6): the block families (WwPhysFamily bits) whose own entry has
	 * physicalWeather on. On: the values written in the file decide alone (the *Alone flags: the users drop build 62's
	 * record terms). Off or absent: the family's physical additions are not read, the record path decides (the same
	 * numbers as a weather with no file); a record part written in the file (wind.direction, wind.turbulence) stays. */
	unsigned phys = 0;
	bool coverAlone = false;	//!< clouds.coverage from a file under the switch: no record layer is needed to draw it
	bool densityAlone = false;	//!< liquidWater / dropletRadius from a file under the switch: the Low band's density = the ratio
	bool heightsAlone = false;	//!< clouds.layers heights under the switch: a band's heights from its file layers only
	bool stormAlone = false;	//!< lightning.frequency from a file under the switch: it says whether the weather flashes
	float lightningFreqOwn = -1.0f;	//!< this weather's own lightning.frequency (not blended over a change)
	bool stormAloneLast = false;	//!< over a change: the leaving weather's stormAlone ...
	float lightningFreqLast = -1.0f;	//!< ... and its own lightning.frequency
	QString blocksState, blocksLine;	//!< the echo per block family: switch, who decides (and the numbers)
	QStringList sources;	//!< "<key>=<source>" per input (file:<path> | generated | record)
	/* lane WEATHERRECX2 (SPEC.md 1.8). source: "override", "override[fog,clouds.layers]", "augment" or "skipped" (the
	 * spec COMMON "override" key of the entries that won); overrideAll / overrideFuncs: what it replaces of the record.
	 * stored: the keys the spec lists as "stored for later" ("atmosphere.rayleighScale" -> value; an rgb as .r/.g/.b,
	 * a bool as 0/1), read, range-checked and echoed, no user yet. recordFields: the WTHR fields the entry's
	 * vanillaRecord block carries (part 8: the file holds the whole record; xEdit's FO4 WTHR names) */
	QString source;
	bool overrideAll = false;
	QStringList overrideFuncs;
	QStringList stored;	//!< "<block>.<key>=<value>", sorted
	QStringList recordFields;
	unsigned long long stamp = 0;	//!< moves whenever a value moves (memo keys)
	QString key;	//!< the values as text (rebuild keys); empty when off
	WwWeatherRecx()
	{
		for ( int i = 0; i < 32; i++ )
			layerBase[i] = layerTop[i] = layerLwc[i] = layerRadius[i] = layerIce[i] = layerAnvil[i] = layerType[i] = -1.0f;
	}
};

//! lane CLOUDRECX1: WW_CLOUD_RECX14_RED=1 (on / true / yes): the 1.3 clouds block (the 1.4 keys unknown, density not legacy)
bool wwCloudRecx14Red();

//! lane WEATHERRECX1: the record channels a red (or useRecordColors) can hand back to the WTHR record
enum WwRecordChannel : unsigned
{
	WwRecSky = 1u << 0, WwRecSun = 1u << 1, WwRecAmbient = 1u << 2, WwRecFog = 1u << 3, WwRecClouds = 1u << 4,
	WwRecImagespace = 1u << 5, WwRecLightning = 1u << 6, WwRecPrecip = 1u << 7, WwRecWind = 1u << 8,
	WwRecTransitions = 1u << 9, WwRecAll = ( 1u << 10 ) - 1u
};
//! lane AIRPERF1 A9: the block families that follow the entry's physicalWeather switch (WwWeatherRecx::phys)
enum WwPhysFamily : unsigned
{
	WwPhysClouds = 1u << 0, WwPhysNight = 1u << 1, WwPhysWind = 1u << 2, WwPhysLightning = 1u << 3, WwPhysPrecip = 1u << 4
};
/*! lane AIRPERF1 A9: red WW_WEATHER_RECX_RED=noswitch (every family) or noswitch-<family> (clouds, night, wind,
 *  lightning, precipitation; a comma list), live: that family as build 62 (a file replaces with or without the switch,
 *  the record terms back) */
bool wwWeatherRecxSwitchRed( const char * family );
//! channel words -> bits ("all" = every channel); unknown words warned once
unsigned wwWeatherRecordChannels( const QString & words );
//! true when the record's own path is used for this channel (WW_WEATHER_RECORD_RED or the file's useRecordColors)
bool wwWeatherRecordRed( unsigned channel );

//! the values for the Lookdev weather at its hour (cheap per call: re-resolved on a change, at most every 100 ms)
const WwWeatherRecx & wwWeatherRecx();

//! atmosphere: the haze rung from visibility / hazeHeight over the record's (atmomath.h hazeFromFog's haze[3])
void wwWeatherRecxHaze( float haze[3], bool recordHasFog, float clearAirPerKm );
//! atmosphere.ozone as a multiple, 1 when not set
float wwWeatherRecxOzone();
/*! lane WEATHERRECX1: the haze's optical make-up at the file's humidity (hazeType preset, hazeAlbedo / angstrom /
 *  hazeAnisotropy / airglow overrides, Haenel's growth); false when the row is off or no hazeType resolves */
bool wwWeatherRecxHazeOptics( float albedo[3], float & angstrom, float & g, float & airglow );
/*! lane NIGHTHAZE1: the air's colour filters and overrides as the file (or its hazeType) gives them; false when the row
 *  is off. glow[0] < 0 = not set (the hazeType's airglow law stands); cloudAlbedo = the file's, else the hazeType's, else
 *  0.99 grey; inscatter / dir = 0 unless set; dirExp 4 and dirStart 0 m unless set */
bool wwWeatherRecxAirFilters( float glow[3], float cloudAlbedo[3], float inscatter[3], float dir[3], float & dirExp,
	float & dirStart );
/*! lane WEATHERRECX1: the wind on the WTHR byte scale the consumers already take (lookdevstage wwLookdevWind):
 *  [0] speed byte = 255 * m/s / 20, [1] direction byte (the record's), [3] turbulence byte; false when the wind is the
 *  record's (red wind, row off) */
bool wwWeatherRecxWindBytes( float & speed, float & dir, float & turb );
//! lane WEATHERRECX1: the texture slot names (SPEC.md textures), index into WwWeatherRecx::textures
int wwWeatherRecxTextureSlot( const char * name );
//! lane WEATHERRECX1: the slot's texture, the weather's or the shared default (Textures\FO4CS\<slot>\...)
QString wwWeatherRecxTexture( const char * slot );

/*! lane WEATHERRECX2: the weather functions an entry's "override" list may name (spec, Weathers) */
QStringList wwWeatherRecxFunctions();
/*! lane WEATHERRECX2: one weather resolved with no window (the offline gates): `haveRecord` false = a weather that
 *  exists only as a file (formID 0, no owner); the generated rung's inputs are the record's when given. Returns the
 *  values; `line` = the source line. Not used by the application. */
WwWeatherRecx wwWeatherRecxResolveForTest( const QString & edid, const QString & owner, unsigned formID, bool haveRecord,
	const void * genIn, const QStringList & roots, double hour, QString & line );

#endif
