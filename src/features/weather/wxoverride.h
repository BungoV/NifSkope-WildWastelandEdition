#ifndef WXOVERRIDE_H
#define WXOVERRIDE_H

/* lane MODULES1 (item 12, 11-09): LIVE WEATHER AND FOG OVERRIDES.
 * Every fog / air / haze / light / cloud value the renderer reads can be pinned by a stage-0 switch WW_WX_<NAME>
 * (one number, or "r,g,b" for a colour). Unset = the weather's own value (WTHR record or .recx), which each consumer
 * records here as it reads it, so the Scene window and a Live Panel show it. A set applies on the next frame: the
 * consumers' memos key on wwWxOvStamp(); the sun and ambient reach the probe GI through its relight (no rebuild).
 * Red: WW_WX_RED=1 ignores every override (the weather's values, as before). */

#include <QString>
#include <QStringList>

enum class WwWx {
	FogNear, FogFar, FogPower, FogMax, FogNearHeightMid, FogNearHeightRange, FogFarHeightMid, FogFarHeightRange,
	FogHighDensity,
	FogNearColor, FogFarColor, FogNearHighColor, FogFarHighColor,
	FogNearSunrise, FogNearDay, FogNearSunset, FogNearNight,
	FogFarSunrise, FogFarDay, FogFarSunset, FogFarNight,
	HazeVisibility, HazeHeight,
	AirDensity, AirHeightScale, AirPhaseG, AirTint,
	SunColor, AmbientColor,
	CloudCoverage, CloudDensity,
	Count
};

struct WwWxRow
{
	const char * sw;	// WW_WX_...
	const char * label;
	const char * group;
	int n;	// 1 or 3
	float min, max, step;
	const char * unit;
};

const WwWxRow & wwWxRow( WwWx id );
int wwWxCount();

//! Moves when any override moves (memo keys)
unsigned long long wwWxOvStamp();
//! Is this row pinned (and the red off)?
bool wwWxSet( WwWx id );
//! The value the consumer uses: the pin, else `weather` (recorded as the weather's own value)
float wwWxOv( WwWx id, float weather );
//! The same for a colour, in place
void wwWxOv3( WwWx id, float rgb[3] );
//! The weather's own value as last read (n floats), false when no consumer has read it yet
bool wwWxWeatherValue( WwWx id, float out[3] );
//! The pinned value (n floats), false when unset
bool wwWxPinned( WwWx id, float out[3] );
//! The row of a switch name, -1 when it is not a weather override
int wwWxFind( const QByteArray & sw );
//! The weather's own value as text ("a" or "r,g,b"), empty when no consumer has read it yet
QString wwWxWeatherText( WwWx id );
//! Any per-time-of-day fog colour pinned (currentFog patches a copy of the record then)
bool wwWxAnyTodColor();

//! Writes the current overrides as a new .recx in Fusion Shaders - Assets/Saved Weathers (never overwrites); the reply
//! names the file or starts "refused"
QString wwWxSaveRecx();

#endif
