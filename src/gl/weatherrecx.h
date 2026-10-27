#ifndef WW_WEATHERRECX_H
#define WW_WEATHERRECX_H

/* lane WX1: the Lookdev weather's .recx blocks atmosphere / clouds / night (recx.h; spec FO4-Extension-Formats
 * recx/SPEC.md "atmosphere, clouds, night (Weathers)"). Per key: the file's value > the record's own data > the
 * default; the users (atmosphere.cpp, cloudmap.cpp, lookdevstage.cpp) apply a value only where it is set.
 * Show row "Weather .recx (sky, clouds, night)", ships off, pin WW_WEATHER_RECX; red WW_WEATHER_RECX_RED = ignore
 * (no file read) | recordfirst (the record's fog and cloud cover win over the file). Files re-read on a change
 * (wwRecxScan, 500 ms), no reopen. Census on stderr: "weather recx: ...". */

#include <QString>

struct WwWeatherRecx
{
	bool on = false;	//!< the row is on and a file set at least one value
	bool recordFirst = false;	//!< red recordfirst
	// set values; < 0 = not set (the record and the default decide)
	float visibilityM = -1.0f, hazeHeightM = -1.0f, ozone = -1.0f;
	float coverage = -1.0f, density = -1.0f;
	float layerBase[32], layerTop[32];	//!< metres above sea level; < 0 = not set
	int layers = 0;	//!< how many layers have heights
	float stars = -1.0f, moonlight = -1.0f;
	unsigned long long stamp = 0;	//!< moves whenever a value moves (memo keys)
	QString key;	//!< the values as text (rebuild keys); empty when off
	WwWeatherRecx()
	{
		for ( int i = 0; i < 32; i++ )
			layerBase[i] = layerTop[i] = -1.0f;
	}
};

//! the values for the Lookdev weather at its hour (cheap per call: re-resolved on a change, at most every 100 ms)
const WwWeatherRecx & wwWeatherRecx();

//! atmosphere: the haze rung from visibility / hazeHeight over the record's (atmomath.h hazeFromFog's haze[3])
void wwWeatherRecxHaze( float haze[3], bool recordHasFog, float clearAirPerKm );
//! atmosphere.ozone as a multiple, 1 when not set
float wwWeatherRecxOzone();

#endif
