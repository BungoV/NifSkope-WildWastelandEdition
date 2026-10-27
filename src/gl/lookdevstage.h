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
 * builds the directional-ambient matrix (a Todd's treat candidate is known) -- owed.
 * Red "dalcflip" swaps the sign convention.
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
 * blended in display space (the engine blends in HDR; ruling: no HDR yet).
 *
 * The weather fog (lane FOG1): one Fog row, ships OFF, live. The engine fog of
 * the picked WTHR at the hour (esmweather.h WwFog: FNAM day/night on
 * the engine's fog day weight, the NAM0 fog colours in CIELab x NAM4, pow 2.2),
 * the composite formula in res/shaders/lookdev_fog.glsl, applied to scene
 * geometry (ground, PBR, legacy fo4_default) in LINEAR light before the
 * exposure and the view transform; the dome, moon, sun and clouds are never
 * fogged. Height is measured from the ground plane (groundZ = world z 0).
 * Pins: WW_LOOKDEV_FOG=0|1, WW_LOOKDEV_FOGPROBE=<d>,<z>,<mode> (every fogged
 * fragment writes, raw: mode 1 the alpha, 2 the fog colour / 2, 3 the height
 * blend, all at eye distance d and height z; 5 the geometry it reads, R = d /
 * 4096, G = 0.5 + z / 2000). Reds: fogleak (fog drawn while OFF), fogsky
 * (Fog ON darkens the dome), fogheight0 (shader height forced to 0), plus the
 * esmweather.h fog reds.
 *
 * The cascaded sun shadows (lane CSM1): one Shadows row, ships OFF, live; the
 * pass, fit and receiver are gl/sunshadow.h. Pin: WW_LOOKDEV_SHADOWS=0|1. */

#include <QString>
#include <QStringList>
#include <QVector>
#include <vector>

class Scene;
struct WwWeatherEntry;
struct WwLens;

//! Scene mode Lookdev is the one switch
bool wwLookdevActive();

//! plugin rows: the Data folder's plugins (masters first), and the chosen one (full path)
QStringList wwLookdevAvailablePlugins();
//! lane SKYTOP1: the Plugin row's override (full path); empty = "Load order" (the default)
QString wwLookdevPlugin();
void wwLookdevSetPlugin( const QString & path );
/*! lane SKYTOP1: the open cell's load order (full paths). With the Plugin row on "Load order" the weather (sky, sun,
 *  fog, clouds, imagespace) is the last plugin's record across this list, as in the game; before any cell opens the
 *  list is the active MO2 profile / plugins.txt (cellModSource). */
void wwLookdevSetCellPlugins( const QStringList & plugins );
//! the loaded weathers, "EDID [FormID]" order as the Scene window lists them
QVector<WwWeatherEntry> wwLookdevWeathers();
QString wwLookdevWeatherKey();
/*! lane VOLAIR1 (record extension files, src/recx.h): the loaded weather's identity: its load-order FormID, EditorID,
 *  the plugin that defines it (file name), the loaded plugins (full paths, load order) and the game's Data; false
 *  when no weather is loaded */
bool wwLookdevWeatherId( quint32 & formID, QString & edid, QString & owner, QStringList & files, QString & dataDir );
void wwLookdevSetWeather( const QString & key );
double wwLookdevHour();
void wwLookdevClimateTnam( unsigned char out[4] );	// lane GAPLOOK1: the climate's TNAM sunrise/sunset bytes (10 min units)
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
//! the weather fog row (lane FOG1), OFF by default
bool wwLookdevFog();
void wwLookdevSetFog( bool on );
//! the cascaded sun shadows row (lane CSM1, gl/sunshadow.h), OFF by default
bool wwLookdevShadows();
void wwLookdevSetShadows( bool on );
//! the light as it is shaded (TO the light, floored) and the unfloored disc position, world
void wwLookdevShadowLight( float sunDir[3], float disc[3] );
//! lane LIGHTS4: the weather's sun lens flare (WTHR GNAM), the sun (TO it, world) and its linear colour; false = none
bool wwLookdevSunLens( const WwLens *& lens, float sunDir[3], float sunColor[3] );
//! the ground frame the ground quad sits on; true = the ground draws (so it casts)
bool wwLookdevGroundFrame( Scene * scene, float & z, float xy[2], float & half );
double wwLookdevGameDay();
/*! lane GAPLOOK1 (TERRSHADOWLEFT): FO4CS's moonlit distant-shadow term phase^3 x moon alpha x sky clearness (FarField
 *  EngineSunNight.h ResolveNight, FarFieldMaskPolicy.h sunVisibility); 0 without weather or moons */
float wwLookdevMoonShadowTerm( bool invertClear, QString * why );
void wwLookdevSetGameDay( double d );
//! seconds on the cloud scroll clock (the pin, 0 in a harness run, else real time since Clouds went on)
double wwLookdevCloudSeconds();
//! lane CLOUD1: one weather cloud layer as drawClouds draws it now, for gl/cloudmap.cpp
struct WwCloudLayerIn
{
	int layer = 0;
	QString tex;	//!< the texture as the cache takes it (textures/sky/x.dds)
	float alpha = 0.0f;	//!< JNAM at the colour keys (blendCloud)
	float rgb[3] = {};	//!< PNAM at the keys, 0..255 (CIELab blend)
	float uvOffset[2] = {};	//!< the scroll at the cloud clock
	const float * pos = nullptr;	//!< the Clouds.nif shape, world (dome) space; null without a scene or the mesh
	const float * col = nullptr;	//!< rgba 0..1 (alpha = the edge fade)
	const float * uv = nullptr;
	int nv = 0;
	const unsigned short * idx = nullptr;
	int nIdx = 0;
};
/*! the active layers (not disabled, alpha > 0, a texture), key = plugin|weather|load generation, eye = the view's
 *  world position (game units); false + why when no weather resolved */
bool wwLookdevCloudLayers( Scene * scene, QVector<WwCloudLayerIn> & out, QString & key, float eye[3], QString * why );
//! lane CLOUD1: fills io's mesh pointers for layer io.layer (a previous weather's layer during a transition)
bool wwLookdevCloudMesh( Scene * scene, WwCloudLayerIn & io );
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
/*! Lane EFFECTPARITY1: the weather's Effect Lighting colour (NAM0 row 9) at the hour, linear, blended as the
 *  Sunlight row. The game's sun carries it beside its diffuse and lit effects take it instead of the sunlight.
 *  False (rgb = the sunlight) without a weather. */
bool wwLookdevEffectLight( float rgb[3] );
/*! Lane LIGHTS3 (XEMI-REGN): the emittance colour a REGN gives at the hour: its weather's Effect Lighting row,
 *  CIELab-blended at the clock's keys, as bytes / 255 (what XEMI from a LIGH gives). False when not a REGN or the
 *  plugins are not loaded. */
bool wwLookdevRegionEmit( quint32 regn, float rgb[3] );
/*! Lane SUNCELL1: the weather's imagespace at the hour: the IMSP slots of the two colour keys, their HNAM /
 *  CNAM / TNAM blended linearly at the keys' t (as the Sky Scale), and both LUT paths with that t. False when
 *  no weather is loaded or neither key's IMGS read. `key` changes whenever any of it does. */
struct WwLookdevIs
{
	float hdr[9] = {}, cine[3] = {}, tint[4] = {};
	QString name, lutA, lutB, key;
	float t = 0.0f;	// 0 = all lutA
};
bool wwLookdevImageSpace( WwLookdevIs & out );
/*! Lane BOUNCE1: the hour's imagespace Sunlight Scale (HNAM[6], wwLookdevImageSpace's hdr[6]): the game multiplies it
 *  into the lit surfaces' sun (checked in Todd's treat;
 *  lane SUNCELL1's reading), so the cell view's
 *  exterior surfaces and the probe GI's sun take it too. 1 without a weather or an imagespace. Red
 *  WW_CELL_SUNSCALE_RED=one keeps the sun unscaled (the viewer before lane BOUNCE1). */
float wwLookdevSunlightScale();
//! lane WATER1: the water's sky (linear): the weather's Horizon, Sky-Lower and Sky-Upper rows at the dome's hour;
//! false (the outputs untouched) when no weather is loaded
bool wwLookdevWaterSky( float horizon[3], float lower[3], float upper[3] );
/*! lane GRASSMB1: the depth of field of the weather's two ImageSpace (IMGS) records around the dome's hour: each
 *  DNAM as stored (strength, distance, range, the flags float), the blend t (0 = a, 1 = b) and whether each key
 *  has one; false (the outputs untouched) when no weather is loaded */
bool wwLookdevImageSpaceDof( float a[4], float b[4], float & t, bool & haveA, bool & haveB );

/*! Lane GRASSMB1: the loaded weather's wind bytes (DATA 0 speed, 17 direction, 18 direction range, 19 turbulence);
 *  false (out untouched) when no weather is loaded or its DATA is short */
bool wwLookdevWind( quint8 out[4] );

/*! lane WEATHERBLEND1: true while a weather change blends the weather left behind into the new one (the views ask
 *  for frames until it ends) */
bool wwLookdevWeatherChanging();

//! cube background (replaces drawSkyBox in Lookdev); true = the colour buffer is written
bool wwLookdevDrawBackground( Scene * scene );
//! the ground quad at the lowest visible vertex; no-op when the Ground row is off
void wwLookdevDrawGround( Scene * scene );
/*! lane LIGHTBOOTH1 (harness pin WW_LOOKDEV_GROUND_CELL=1): the ground again inside the cell's HDR frame, after its sky;
 *  no GL call when the pin is unset */
void wwLookdevDrawGroundCellFrame( Scene * scene );
/*! the fog uniforms of the renderer's CURRENT program (lane FOG1): a no-op for a
 *  program without `fogOn`; fogOn = false outside Lookdev, under an orthographic
 *  camera, with the Fog row off, or with no weather */
void wwLookdevFogUniforms( Scene * scene );
/*! true when this draw fogs (the decision behind fogOn above). The renderer then
 *  swaps fo4_default.prog for fo4_fog.prog -- the same shader with WW_FOG defined --
 *  so with Fog off the driver compiles exactly the pre-fog shader */
bool wwLookdevFogWanted( Scene * scene );
/*! lane VOLFOG1: the weather's god-ray medium at the Lookdev hour (esmweather.h WwGodRayMedium packed as air rgb,
 *  fwd rgb, back rgb, fwd g, back g, post); false with no weather */
bool wwLookdevGodRays( float m[12], QString * note );

//! lane SKYPIC1: the sky the Lookdev background draws (the dome, the stars, the cloud layers; not the sun, the moon
//! or the glare: the probes take those as the sun term), captured as light (exposure 1, no view transform) into six
//! n x n float faces. rgb: face (+X -X +Y -Y +Z -Z), row from the bottom, texel from the left, 3 floats. A texel's
//! direction is F + sx R + sy U, sx = 2 (i + 0.5) / n - 1, sy the same for the row (wwSkyPictureFace). Below the
//! dome's rim: 0.
struct WwSkyPicture
{
	int n = 0;
	std::vector<float> rgb;
	QString key, drew;
	double ms = 0.0;
};
void wwSkyPictureFace( int face, float F[3], float R[3], float U[3] );
//! what the capture depends on: plugin, weather, hour, day, the Sky and Clouds rows (not the cloud clock)
QString wwLookdevSkyPictureKey();
//! needs the view's GL context current. flatRgb (a control): a sky of that one colour, nothing drawn.
bool wwLookdevCaptureSky( Scene * scene, int n, const float * flatRgb, WwSkyPicture & out, QString * why );
//! SKY3: the world z the physical sky is captured from (the probes' median height); NaN = the view camera's
void wwLookdevSetSkyCaptureZ( float z );

#endif
