/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_LOOKDEVSTAGE_H
#define WW_LOOKDEVSTAGE_H

/* LookdevStage (lane PBRR2B, docs/NIFSKOPE_PBR_RENDERER.md s6.1-6.3, row R2b).
 *
 * The ONE lookdev object: the weather (plugin + WTHR + hour, read by
 * EsmWeather), the sun it gives (vanilla tent arc, NAM0 Sunlight), the ambient
 * (DALC 6-axis; NAM0 Ambient flat for legacy shapes), the cube (stage 1:
 * cube-only background, untinted until W2 -- ruling Q8), and the ground plane.
 * It is active only in Scene mode Lookdev (sceneMode 2); every other mode never
 * reaches it, so `WW_LOOKDEV` unset keeps every existing shot byte-identical.
 *
 * Units (docs s5.2): the sun goes to lightSourceDiffuse[0] LINEAR ((byte/255)^2.2),
 * so the PBR program's sunE = diffuse x PI and a legacy shape sees sqrt(linear),
 * about the stored byte. Legacy ambient = linear NAM0 Ambient / 0.140625, so the
 * legacy A.rgb^2 (= sqrt(amb) x 0.375, squared) is the linear ambient.
 *
 * DALC axis orientation is an ASSUMPTION from physics, not a measurement: the
 * axis name is the light's travel direction, so the Z- colour (the bright sky
 * blue) lights UP-facing normals. The discriminator is the engine function that
 * builds the directional-ambient matrix (Todd's treat candidate Sky::SetDirectionalAmbientBlend,
 * 1.10.155 RVA 0x652F30) -- owed. Red "dalcflip" swaps the sign convention.
 *
 * Harness pins (read once): WW_LOOKDEV=1 (mode Lookdev), WW_LOOKDEV_PLUGINS=<a,b,..>
 * (load list; bare names resolve in the Data folder), WW_LOOKDEV_PLUGIN=<one plugin,
 * masters added>, WW_LOOKDEV_WEATHER=<EditorID|FormID>, WW_LOOKDEV_HOUR=<h>,
 * WW_LOOKDEV_GROUND=0|1, WW_LOOKDEV_GROUNDPASS=0 (the ground pass never runs: the
 * no-ground reference), WW_LOOKDEV_CUBE=<texture path>, WW_LOOKDEV_DATA=<Data dir>,
 * WW_LOOKDEV_RED=<red> (see esmweather.h; here also "groundleak": the OFF path
 * still draws a faint ground, which the ground gate must refuse). Pins win over
 * the stored settings; nothing is stored while a pin holds the value.
 *
 * The weather preview (lane PBRWX1): four rows of the Scene window, each ships
 * OFF and acts live -- Sky (the Atmosphere.nif dome in the WTHR sky colours,
 * CIELab-blended, x IMGS Sky Scale), Sun (the engine clock: the arc, the disc
 * and glare, and the lighting switched to it), Clouds (the WTHR layers on
 * Clouds.nif, scrolling in real seconds) and Moon (position, phase, texture;
 * no light), plus Game day (the moon phase). Pins: WW_LOOKDEV_SKY / _SUN /
 * _CLOUDS / _MOON = 0|1, WW_LOOKDEV_DAY=<days>, WW_LOOKDEV_CLOUDTIME=<s> (the
 * scroll clock; a harness run without it uses 0), WW_LOOKDEV_CLOUDPROBE=
 * <layer>,<u>,<v> (the cloud pass draws one texel probe instead of the
 * layers: left half the shaded colour, right half the alpha as grey).
 * Reds (WW_LOOKDEV_RED): skyswap, skygamma, skyscale, and the toggle leaks
 * skyleak, sunleak, cloudleak, moonleak (the OFF path still draws at 0.02).
 * Nothing here is HDR: every element goes out through studioOutput and is
 * blended in display space (the engine blends in HDR; ruling: no HDR yet). */

#include <QString>
#include <QStringList>
#include <QVector>

class Scene;
struct WwWeatherEntry;

//! Scene mode Lookdev is the one switch
bool wwLookdevActive();

//! plugin rows: the Data folder's plugins (masters first), and the chosen one (full path)
QStringList wwLookdevAvailablePlugins();
QString wwLookdevPlugin();
void wwLookdevSetPlugin( const QString & path );
//! the loaded weathers, "EDID [FormID]" order as the Scene window lists them
QVector<WwWeatherEntry> wwLookdevWeathers();
QString wwLookdevWeatherKey();
void wwLookdevSetWeather( const QString & key );
double wwLookdevHour();
void wwLookdevSetHour( double h );
bool wwLookdevGround();
void wwLookdevSetGround( bool on );
//! the weather preview rows (lane PBRWX1), all OFF by default
bool wwLookdevSky();
void wwLookdevSetSky( bool on );
bool wwLookdevSun();
void wwLookdevSetSun( bool on );
bool wwLookdevClouds();
void wwLookdevSetClouds( bool on );
bool wwLookdevMoon();
void wwLookdevSetMoon( bool on );
double wwLookdevGameDay();
void wwLookdevSetGameDay( double d );
//! seconds on the cloud scroll clock (the pin, 0 in a harness run, else real time since Clouds went on)
double wwLookdevCloudSeconds();
//! the scene cube (the material's own EnvmapTexture wins per shape, in the renderer)
QString wwLookdevCubePath();
//! stored plugin/weather/hour/ground -> state; pins win
void wwLookdevLoadSettings();

//! one line: the resolved weather, keys, sun, ambient source, cube source, ground; or the refusal
QString wwLookdevSummary();
//! census echo: "lookdev=on(asked=1) weather=... keys=... cube=..."
QString wwLookdevEcho();

/*! Replace the viewport light with the weather's (called by GLView after its own
 *  light, only when active and lighting is on). lightDirWorld = direction TO the
 *  light, world (Z up); the caller turns it into view space. */
void wwLookdevLight( float lightDirWorld[3], float diffuse[4], float ambient[4] );
//! the DALC ambient (linear) the PBR program's Lookdev branch reads: [X+,X-,Y+,Y-,Z+,Z-]
bool wwLookdevDalc( float rgb[6][3] );

//! cube background (replaces drawSkyBox in Lookdev); true = the colour buffer is written
bool wwLookdevDrawBackground( Scene * scene );
//! the ground quad at the lowest visible vertex; no-op when the Ground row is off
void wwLookdevDrawGround( Scene * scene );

#endif
