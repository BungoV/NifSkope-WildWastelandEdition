/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_CELLLIGHTS_H
#define WW_CELLLIGHTS_H

/* Cell lights: the cell view drawn with the cell's own lights (placed LIGH point/spot/hemisphere/box,
 * an interior's DALC ambient and directional) via fo4_cell.prog while the Cell lights row is on.
 * Colors are linear; the legacy FO4 program works in sqrt-of-linear, so the cell branch writes the sqrt.
 * WW_CELL_LIT=0|1 pins the row; WW_CELL_LIT_PROBE=<mode> and WW_CELL_LIT_RED=<name> are harness switches. */

#include <QString>
#include <QStringList>
#include <QVector>

#include <cmath>
#include <memory>
#include <string_view>
#include <vector>

#include "features/ao/ao.h"	// lane MODULES1: the AO module's interface (its rows, pass and targets)
#include "features/lights/lights.h"	// lane MODULES1: the lights module's interface (shadows, flicker, XEMI, spheres)

class Scene;
class NodeList;

//! lane BINDLESS2FIX (gl/cellbindless.cpp): WW_BINDLESS_RED=cellname, the bindless twins not taken as cell programs
bool wwBlCellNameRed();

/*! A cell-lit program by name: fo4_cell / pbrm_cell, their sun-cascade variants and the bindless twins
 *  (a program missing here skips every cell uniform: a black, fogless frame). */
inline bool wwIsCellProgramName( std::string_view n )
{
	return n == "fo4_cell.prog" || n == "pbrm_cell.prog" || n == "fo4_cellcsm.prog" || n == "pbrm_cellcsm.prog"
		|| ( ( n == "fo4_cellbl.prog" || n == "fo4_cellcsmbl.prog" ) && !wwBlCellNameRed() )
		|| n == "fo4_cellfgl.prog" || n == "fo4_cellfgc.prog" || n == "fo4_cellcsmfgl.prog" || n == "fo4_cellcsmfgc.prog"	// lane FARDRAW1
		|| n == "fo4_cellfgp.prog" || n == "fo4_cellcsmfgp.prog"	// lane FARPAGES1
		|| n == "fo4_cellfs.prog" || n == "fo4_cellcsmfs.prog";	// lane FARLOOK1101: the far splat twins
}

/*! An effect decal's program by name. The game draws Decal-flagged effects into the surface under them,
 *  so they are lit by the surface's lights rather than self-lit like other effects. */
inline bool wwIsCellDecalProgramName( std::string_view n )
{
	return n == "fo4_effectdecal.prog" || n == "fo4_effectdecalcsm.prog";
}
//! WW_CELL_DECALLIT_RED=unlit: the effect decals self-lit as before
bool wwCellDecalLitRed();
//! WW_CELL_DECALLIT_PROBE=1: the probe passes draw the effect decals white and opaque
bool wwCellDecalLitProbe();

/* lane NIFXLIGHT3: a NiLight's dimmer over time (its NiLightDimmerController's float keys), timed as NifSkope times
 * every controller the World animation row plays (glcontroller.cpp ctrlTime: frequency x T + phase, then the cycle) */
struct WwNifxDimTrack
{
	std::vector<float> t, v, fwd, bwd;	//!< key times and values; quadratic keys' Forward and Backward
	int interp = 1;						//!< 1 linear, 2 quadratic, 5 constant (any other reads linear)
	float start = 0.0f, stop = 0.0f, frequency = 1.0f, phase = 0.0f;
	int cycle = 0;						//!< flags (f & 6) >> 1: 0 loop, 1 reverse, 2 clamp
	bool scaled = true, backwards = false;	//!< flags 0x40 (frequency, phase and cycle apply) and 0x10
	QString sequence;					//!< the looping sequence that plays it (empty: the NiLight's own controller)
};

struct WwCellLight
{
	float pos[3] = { 0, 0, 0 };     //!< world
	float radius = 0.0f;
	float color[3] = { 0, 0, 0 };   //!< linear, fade folded in
	bool spot = false;
	float dir[3] = { 1, 0, 0 };     //!< world, the way a spot shines
	float cosOuter = 0.0f;          //!< cos(FOV / 2)
	float bias = 0.0f, scale = 1.0f, exponent = 2.0f;   //!< DATA Constant, Scalar, Exponent
	float cone = 1.0f;              //!< DATA Falloff Exponent (the spot edge)
	bool noSpecular = false;        //!< flag 0x8000
	bool noRim = false;             //!< flag 0x80000 (No Rim Lighting), the back-light term dropped
	bool ignoreRoughness = false;   //!< flag 0x40000, Lambert diffuse and no back-light
	int shadow = 0;                 //!< 0 none, 1 spot (0x400), 2 hemisphere (0x800), 3 omni (0x1000)
	float nearClip = 10.0f;         //!< DATA Near Clip + XLIG Near Clip delta: casters nearer the light cast nothing
	float shadowBias = 0.0f;        //!< XLIG Shadow Depth Bias (read and echoed; its scale is unread, not applied)
	int shape = 0;                  //!< 0 omni (or spot), 1 hemisphere (dir = its plane normal), 2 box
	float box[3][4] = {};           //!< box local coordinate k = dot(box[k].xyz, P) + box[k].w, inside |k| <= 1
	quint32 ref = 0;                //!< the placed reference (0: none, an effect's light)
	float godRay = 0.0f;            //!< a shaft emitter (a shadow flag AND a WGDR): its GDRY's intensity; 0 none
	quint32 lens = 0;               //!< lane LIGHTS3 (LENSFLARE1): the base's LENS (0 none)
	float byteColor[3] = { 0, 0, 0 };   //!< lane LIGHTS3: DATA colour bytes / 255 (the lens flare's colour influence)
	quint32 xemi = 0;                   //!< lane LIGHTS4: the placement's XEMI code (cellfxlit.h wwCellXemiCode), 0 none
	//! lane LIGHTFLICKER1: 0 steady, 1 flicker (LIGH flag 0x8), 2 pulse (0x80 | 0x100), with DATA's Flicker Effect
	int flicker = 0;
	float flickerPeriod = 0.0f, flickerIntensity = 0.0f, flickerMovement = 0.0f;
	float flickerAxes[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };	//!< the ref's local x, y, z in world, x its scale
	//! lane LIGHTS5 (GOBO1): a spot's LIGH NAM0 pattern, the ref's local y and z in world, tan(FOV / 2)
	//! GOBO2: an omni, hemisphere or box light's pattern too, with its local x (a spot keeps goboX unread)
	QString gobo;
	float goboX[3] = { 1, 0, 0 }, goboY[3] = { 0, 1, 0 }, goboZ[3] = { 0, 0, 1 };
	float goboTan = 0.0f;
	//! lane LIGHTS5 (FOGSCALE1): the .recx keys of fog.scale: the LIGH (plugin file, local id, EditorID) and the placed ref
	QString recxBasePlugin, recxBaseEdid, recxRefPlugin;
	quint32 recxBaseLocal = 0, recxRefLocal = 0;
	//! lane NIFXLIGHT1: a mesh's .nifx light (0 hero, 1 normal, 2 bulb; -1 a placed LIGH) and its fog.scale (-1 none)
	int tier = -1;
	float nifxFog = -1.0f;
	QString nifxName;
	//! lane NIFXLIGHT3: the NiLight dimmer the light follows over time (null: steady) and the dimmer its colour holds
	std::shared_ptr<const WwNifxDimTrack> nifxDim;
	float nifxDimRest = 1.0f;
};

/* lane NIFXLIGHT1: a model whose .nifx sidecar exists, with its named nodes' frames in the model's root space
 * (columns 0-2 the rotation x scale, column 3 the translation; the root's own transform is the placement's) */
struct WwNifxNode
{
	QString name;
	float m[3][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } };
	bool niLight = false;
	//! lane NIFXLIGHT2: a NiLight's dimmer in the state drawn (the On sequence's last dimmer key, else its Dimmer)
	//! and the NiControllerSequences whose dimmer blocks name it
	float dimmer = 1.0f;
	int sequences = 0;
	std::shared_ptr<const WwNifxDimTrack> track;	//!< lane NIFXLIGHT3: the dimmer over time (null: none plays)
};
struct WwNifxModel
{
	QString model, sidecar;	//!< the Data path, the .nifx file found (WW_NIFX_ROOT, then the data root)
	QVector<WwNifxNode> nodes;
};
//! one placed fixture: the placement in world space (P = pos + rot (scale p)) and its reference's .recx key
struct WwNifxFixture
{
	std::shared_ptr<const WwNifxModel> model;
	float pos[3] = { 0, 0, 0 };
	float rot[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	float scale = 1.0f;
	quint32 ref = 0;
	QString refPlugin;
	quint32 refLocal = 0;
	//! lane NIFXLIGHT3: every placed model is kept (model null: no .nifx yet), so one made later is found live
	QString modelPath, dataRoot;
};

//! World point (x, y, z) lies inside light l's volume (always for an omni or spot light)
inline bool wwCellLightShapeIn( const WwCellLight & l, double x, double y, double z )
{
	if ( l.shape == 1 )
		return ( x - l.pos[0] ) * l.dir[0] + ( y - l.pos[1] ) * l.dir[1] + ( z - l.pos[2] ) * l.dir[2] >= 0.0;
	if ( l.shape == 2 )
		for ( int k = 0; k < 3; k++ )
			if ( std::abs( l.box[k][0] * x + l.box[k][1] * y + l.box[k][2] * z + l.box[k][3] ) > 1.0 )
				return false;
	return true;
}

/* An Ambient Only light (LIGH flag 0x100000): inside a sphere of 1.22077 x radius (or its linked box) it
 * replaces the cell ambient, scaling each channel's affine sum by k; the first light in plugin order wins. */
struct WwCellAmbientLight
{
	float pos[3] = { 0, 0, 0 };     //!< world
	float volume = 0.0f;            //!< 1.22077 x radius
	float k[3] = { 1, 1, 1 };       //!< per channel, folded into the ambient before its power
	bool hasBox = false;            //!< the volume is `box`, not the sphere
	float box[3][4] = {};           //!< as WwCellLight::box, inside |k| < 1
};

struct WwCellLighting
{
	bool interior = false;
	QVector<quint32> regions;	//!< lane WEATHERBLEND4: an exterior's centre cell's regions (XCLR), the weather pick's
	float regionAt[2] = { 0, 0 };	//!< the centre cell's middle (world x, y), the point the regions' areas are tested at
	quint32 regionSeed = 0;	//!< the centre cell's form: the pick's seed
	// lane WEATHERBLEND5: the worldspace (a region naming another is skipped) and the spawn eye, the player's point the
	// game tests the areas at (red WW_WEATHER_BLEND_RED=regioncentre: regionAt)
	quint32 regionWorld = 0;
	float regionEye[2] = { 0, 0 };
	bool regionEyeOk = false;
	bool showSky = false;	//!< an interior whose CELL DATA has bit 7 (Show Sky); the Lookdev sky draws there
	/*! lane SKYINT1 (10-29): Show Sky AND Use Sky Lighting (DATA bits 7 + 8). The game runs such an interior's sky in its
	 *  full (exterior) mode: the weather and the hour move every frame, and the sky's ambient (DALC), fog colours and
	 *  directional colour stay the CURRENT weather's (the cell's XCLL ones are not taken). wwCellSkyLitNow() */
	bool skyLighting = false;
	/*! lane INTSKY1 (bungo 11-01, LATE ITEMs 30-30e): DATA bit 8 (Use Sky Lighting) of the WINNING CELL record alone, and
	 *  the interior's .recx Cells key (its defining plugin, local id, EditorID) for the `sunlight` override.
	 *  wwCellSunlit() decides; the flag alone is the vanilla switch */
	bool useSkyLighting = false;
	QString cellPlugin, cellEdid;
	quint32 cellLocal = 0;
	QVector<WwCellLight> lights;
	QVector<WwCellAmbientLight> ambientLights;  //!< in plugin order (the first that holds a point wins)
	/*! Lights that start off, kept only with WW_CELL_GI_GPU so the relight can turn them on live */
	QVector<WwCellLight> offLights;
	bool hasDalc = false;
	float dalc[6][3] = {};          //!< byte / 255 as stored (PRTP2 powers AFTER the affine sum), X+ X- Y+ Y- Z+ Z-
	bool hasDirectional = false;
	float dirColor[3] = { 0, 0, 0 };    //!< linear
	float dirTo[3] = { 0, 0, 1 };       //!< world, TO the light
	float center[3] = { 0, 0, 0 };      //!< the probe modes' position box centre
	/* The cell's imagespace (XCIM -> IMGS); hdr = HNAM, cine = CNAM, tint = TNAM, lut = the TX00 3D LUT
	 * (16x16x16 RGB8, r fastest; empty when not found). */
	bool hasImageSpace = false;
	float isHdr[9] = { 3, 7, 0.6f, 0.5f, 0.15f, 0.15f, 1.8f, 1.5f, 3 };
	float isCine[3] = { 1, 1, 1 };
	float isTint[4] = { 0, 1, 1, 1 };
	QString isName, isLutPath;
	std::vector<unsigned char> isLut;
	QString dataRoot;	//!< the loose Data folder (an exterior's weather LUT is looked up there too)
	/* The interior's fog, packed as the weather fog (wwFogPackK), each field from XCLL or the lighting
	 * template per its Inherits flag; the game's clamps mean an interior always fogs. */
	bool hasFog = false;
	float clipDist = 0.0f;              //!< an interior's clip distance (XCLL offset 32; Inherits 0x80), 0 = none
	float fogK[6][4] = {};
	QString fogNote;
	/* The interior's god-ray medium (WwGodRayMedium), from XGDR, else the template's WGDR, else the fallback */
	float godRay[12] = { 0.60f, 1.52f, 3.31f, 2, 2, 2, 1, 1, 1, 0.75f, 0, 1 };
	QString godRayNote = QStringLiteral( "fallback" );
	QStringList recxPlugins;			//!< lane LIGHTS5 (FOGSCALE1): the load order's files (recx.h roots and merge order)
	QString recxData;					//!< the game's Data folder
	//! lane NIFXLIGHT1: the fixtures with a .nifx (src/features/lights/cellnifxlights.cpp applies them while Mesh lights is on),
	//! the start-off placed lights the list above skipped (their state carries over), and the bulb-tier lights
	QVector<WwNifxFixture> nifxFixtures;
	QVector<WwCellLight> nifxOffPlaced, nifxBulbs;
	QVector<WwCellLight> nifxOffBulbs;	//!< lane NIFXLIGHT3: bulb-tier lights dark with the start-off light they took
	/*! lane MUSGI1 r5 (11-05): a sunlit interior's lamp scale (cellview.cpp cellPublishLighting; 1 elsewhere) as the .nifx
	 *  lights take it (red WW_CELL_NIFX_SCALE_RED=1: 1) and as the emissive surfaces and their RT GI glow take it (red
	 *  WW_CELL_GLOW_SCALE_RED=1: 1) */
	float nifxScale = 1.0f, glowScale = 1.0f;
	QString summary;                    //!< one census line
};

//! the cell view publishes the lighting of the document it just built (replacing that document's last)
void wwCellLightsPublish( const void * nif, const WwCellLighting & lighting );
const WwCellLighting * wwCellLightsFor( const void * nif );
//! lane NIFXLIGHT2: moves on each Mesh lights (.nifx) publish for the document (0: the lights as built)
int wwCellLightsNifxStamp( const void * nif );

/*! The bounce (src/probegi.h): a voxel grid of probe ambient cubes, six slabs (+X -X +Y -Y +Z -Z) of
 *  dims[2] each, x fastest, rgba per voxel; the shader adds albedo x E(N) / pi. */
struct WwCellGi
{
	float origin[3] = { 0, 0, 0 };
	float voxel = 64.0f;
	int dims[3] = { 0, 0, 0 };
	int pad = 5;	//!< lane GIEDGE1: edge voxels each side past the probes' reach (probegi kPad); the shader fades x/y over them
	std::vector<float> rgba;
	std::vector<float> sky;	// the same layout, rgb = the probes' open-sky share, a = valid
	QString summary;
	bool skyLit = false;   //!< the grid holds the weather's sky; it stands in for the weather's ambient
	/*! With rooms (src/proberooms.h), rgba and sky hold 12 slabs (slot 0's six, then slot 1's);
	 *  slotRooms = per voxel the packed rooms of its two slots, rooms = per fine cell its packed rooms */
	std::vector<float> slotRooms, rooms;	// (Qt reserves "slots")
	float roomsOrigin[3] = { 0, 0, 0 };
	float roomsCell = 16.0f;
	int roomsDims[3] = { 0, 0, 0 };
	/*! lane PERF1 (src/proberooms.h GiVolumeSet): the interior volumes' shapes, 12 floats each (center, type; size,
	 *  room; rotation degrees, 0), and per voxel its slots' thresholds (the packed rooms across, slot 0's weight,
	 *  slot 1's; the slot texture's g, b, a) */
	std::vector<float> volShapes, slotThresh;
	QString volCensus;
	/*! lane EMISSIVEGRID1 (src/probegifine.h): the fine boxes round bright glow, 8 floats a box, and their atlas (dims[0] x
	 *  dims[1] a layer, fineLayers layers, rgba), uploaded after the grid's own layers; empty: none (the row off) */
	std::vector<float> fineBox, fine;
	int fineLayers = 0;
	/*! SKY2 FOGSLICE1 (src/probegi.h ProbeGiResult::air): the air's own volume, six slabs of airDims[2] each, x fastest,
	 *  rgba (a = 1), every voxel filled; empty: none (the air reads the grid) */
	std::vector<float> air;
	float airOrigin[3] = { 0, 0, 0 };
	float airVoxel = 0.0f;
	int airDims[3] = { 0, 0, 0 };
};
void wwCellGiPublish( const void * nif, const WwCellGi & gi );
/*! lane GIPROBE2: Probe light smoothing. The published grid's rgba, to change in place (null: none), then Edited:
 *  only the grid slabs are uploaded again (rooms, sky and fine boxes stay); the publish version is kept. */
std::vector<float> * wwCellGiRgbaEdit( const void * nif );
void wwCellGiRgbaEdited( const void * nif );
int wwCellGiVersion( const void * nif );   //!< bumped by every publish (0: none)
//! lane PERFFIX2 a: moves on each wwCellLightsPublish (and nifx / edit republish) of the document, from the one
//! process-wide counter the GI versions share, so a rebuilt document never repeats an old value (0: none)
int wwCellLightsVersion( const void * nif );
/*! lane PERFFIX2 a: what a relight of the cell's lights reads beside its uniforms, as one key: the light buffer as
 *  uploaded and as patched (flicker, .nifx dimmers), the light shadow slots, their strengths and every cube face
 *  written since the start (a time-sliced cache filling in counts), the document. 0 before the first upload. */
quint64 wwCellLightsStateKey( Scene * scene );
/*! lane EMISSIVEGRID1: the Emissive GI grid row (ships off): WW/CellEmissiveGrid, pin WW_CELL_EMISSIVEGRID (read when a
 *  cell opens: the row reopens it). Red WW_CELL_EMISSIVEGRID_RED=1|old: nothing built or drawn (the old path and cost);
 *  =noseen|nosub: the shader's refuters (no seen test; no coarse share taken out). */
bool wwCellEmissiveGridOn();
void wwCellEmissiveGridSetOn( bool on );

/*! The PRTP band's Pass: 0 Combined, 1 GI, 2 Sky visibility, 3 Surfel color, 4 Surfel light, 5 Brick light,
 *  6 Probe coverage. Stored as WW/CellPass; WW_CELL_PASS (number or name) wins; WW_CELL_PV_RED and
 *  WW_CELL_GIWALL_RED are harness switches. */
QStringList wwCellPassNames();
int wwCellPass();
//! the Pass this document draws: the stored one once its bake is relit (a GI grid published), else Combined
int wwCellPassFor( const void * nif );
void wwCellSetPass( int pass );
int wwCellPassRed();
const WwCellGi * wwCellGiFor( const void * nif );
//! lane LIGHTPHYS4: moves on each wwCellGiPublish for the document (0: none yet)
int wwCellGiVersion( const void * nif );
//! the GI row (ships off); the pin WW_CELL_GI wins. Draws only while the Cell lights row is on.
bool wwCellGiOn();
/*! lane PRTOFF1 (bungo 10-29): the old baked probe GI (PRT) draws only under the red WW_CELL_PRT_RED=1 (read once, at
 *  launch). Off: wwCellGiOn() is false whatever the row or WW_CELL_GI say, WW_CELL_GI_FROM loads no bake, nothing
 *  relights one, the march is never compiled and the shaders hold no grid read (WW_CELL_PRT, src/gl/glcontext.cpp) */
bool wwCellPrtOn();
/*! lane PERF1: the GI's visibility, WW_CELL_GI_VIS: 0 the volumes (default: the pixel's volume, the threshold blend),
 *  1 march (red: the per-pixel chain as before the volumes, compiled in by WW_CELL_GI_MARCH, src/gl/glcontext.cpp) */
int wwCellGiVis();
/*! lane PERF1: the GI's rate, WW_CELL_GI_RATE: 0 half (default: the screen GI at half resolution, upsampled), 1 full
 *  (the screen GI at full resolution), 2 inline (no screen pass: each surface reads the volumes), 3 red (the screen
 *  pass runs, the surfaces read inline). With WW_CELL_GI_VIS=march: no screen pass. */
int wwCellGiRate();
void wwCellGiSetOn( bool on );
//! the GPU relight row (WW/CellGiGpu, WW_CELL_GI_GPU, read when a cell opens); the env pin wins over its row
bool wwCellGiGpuOn();
void wwCellGiGpuSetOn( bool on );
/*! The Sky visibility row, 0-100 % (WW/CellSkyVis, default 100; pin WW_CELL_SKYSTR). As FO4CS's Skylighting
 *  strength: each sky visibility v becomes 1 + s (v - 1), so 0 % is the open sky everywhere, 100 % the bake. */
int wwCellSkyVisPercent();
float wwCellSkyVis();
/*! Lane SKYINT1 (10-29): a Use Sky Lighting interior lit by the current Lookdev weather and hour (WwCellLighting
 *  skyLighting): its DALC and fog colours in place of XCLL's (celllights.cpp), and the ray-traced GI volume's sky
 *  (rtddgicell.cpp). WW_CELL_SKYLIT_RED (live): fixed = both as before (XCLL ambient and fog, the volume's sky held at
 *  the one its first build took), xcll = the ambient and fog only, sky = the volume's sky only, ground = the volume's
 *  sky from below as the weather's again (default: none from below). A comma list. Bits: 1 xcll, 2 sky, 4 ground. */
int wwCellSkyLitRed();
//! true: L is a Use Sky Lighting interior that takes the weather's ambient and fog colours now (Lookdev on, no red)
//! lane INTSKY1: = the interior takes the live weather (wwCellSunlit().weather), its sun or not
bool wwCellSkyLitNow( const WwCellLighting * L );

/*! lane INTSKY1 (bungo 11-01, LATE ITEMs 30-30e): a SUNLIT interior gets the real sky and sun through its openings and
 *  the live exterior weather, as the game's full sky mode runs it ("as outdoors"). The rule (the same in Fusion Shaders):
 *    .recx Cells `sunlight` { enabled, weather } on the cell (Plugins/<Provider>/Cells/<EditorID>.recx, key `cell`)
 *    absent: enabled = weather = the WINNING CELL record's DATA bit 8 (Use Sky Lighting); a block's missing `weather` = its
 *    `enabled`. enabled = the sun + sky light through the openings (shadows from both faces, the GI's sky + sun);
 *    weather = the live weather inside (ambient, fog colours, clouds, rain under open sky, the weather change blends).
 *  Exteriors: on = weather = true. WW_INTSKY_RED (live, comma list): off = no sunlit interior (the weather-only SKYINT1
 *  rule stays: the batch's own look), norecx = the Cells files ignored, onesided = the casters keep their culling,
 *  decals = decal casters kept, rain = rain falls indoors without the roof test. */
struct WwCellSunlit
{
	bool on = false, weather = false;
	QString why;	//!< "flag 0x1181", "recx <file> (<rung>)", "red off", "exterior"
};
WwCellSunlit wwCellSunlit( const WwCellLighting * L );
//! sunlit and Lookdev on (a weather to light it): the cell is lit as outdoors now
bool wwCellSunlitNow( const WwCellLighting * L );
//! !interior or sunlit now: the exterior sun / sky / weather paths run (the shaders' cellInterior = false)
bool wwCellAsOutdoor( const WwCellLighting * L );
int wwIntSkyRed();	//!< bits: 1 off, 2 norecx, 4 onesided, 8 decals, 16 rain
bool wwCellSkyVisPinned();
void wwCellSkyVisSetPercent( int pct );

/*! Lane SPECOCC1: specular occlusion from the baked sky visibility (FO4CS's horizon specular occlusion), on top of the
 *  material AO: outdoors the cube reflection keeps the sky grid's visibility along the reflection. WW/CellSpecOcc, ships
 *  off; pin WW_CELL_SPECOCC=0|1. Red WW_CELL_SPECOCC_RED=ao: the material AO alone (today's), hemi: FO4CS's literal
 *  read (the normal's sky visibility faded out by smoothness). Mode: 0 off, 1 on, 2 hemi. */
bool wwCellSpecOccOn();
bool wwCellSpecOccPinned();
void wwCellSpecOccSetOn( bool on );
int wwCellSpecOccMode();

/*! Distant light from the surfels (src/farlight.h): past the band's near end a surface's placed-light diffuse
 *  fades from the real lights to the bake's surfels; `placed` is subtracted so the far bounce is not counted twice. */
struct WwCellFar
{
	std::vector<float> slotTab;	// (Qt reserves "slots")
	std::vector<float> recs;
	int bits = 0, maxProbe = 0, records = 0;
	float cell = 70.0f;
	float band[2] = { 1024.0f, 5120.0f };
	std::vector<float> placed;
	float placedOrigin[3] = { 0, 0, 0 };
	float placedVoxel = 64.0f;
	int placedDims[3] = { 0, 0, 0 };
	std::vector<float> dots;
	QString summary;
};
void wwCellFarPublish( const void * nif, const WwCellFar & far );
const WwCellFar * wwCellFarFor( const void * nif );
//! the Far light row (ships off); the pin WW_CELL_FAR wins. Draws only while the Cell lights row is on.
bool wwCellFarOn();
void wwCellFarSetOn( bool on );
//! WW_CELL_FAR_RED: 1 nosurfel, 2 noblend, 4 flipside (harness switches)
int wwCellFarRed();
/*! The far bulbs as dots on the skyline, added into the linear frame after the scene draw. Returns the dots drawn. */
int wwCellFarDotsDraw( Scene * scene );

//! the Cell lights row (ships off); the pin WW_CELL_LIT wins
bool wwCellLightsOn();
//! WW_CELL_CAM_DUMP=<file>: the eye in world units, rewritten when it moves; GLView::paintGL calls it each frame
void wwCellCamDump( Scene * scene );
void wwCellLightsSetOn( bool on );

//! this scene draws cell-lit: the row on, the scene's document published, a perspective non-picking draw
bool wwCellLightsWanted( Scene * scene );
//! the uniforms of the renderer's CURRENT program (a no-op for a program without `cellOn`)
void wwCellLightsUniforms( Scene * scene );
//! a harness probe pass (WW_CELL_LIT_PROBE or WW_CELL_FOG_PROBE) on a cell-lit scene: effects are not drawn
bool wwCellProbePass( Scene * scene );
//! lane FO4CELLGI1: a probe (WW_CELL_LIT_PROBE) or a Pass is asked, or WW_CELL_VIEWS_RED=1: the shaders compile the views
bool wwCellViewsShaderDefine();
//! probe 80 (the albedo as drawn): also leaves out the second-pass shapes (alpha blend and decals)
bool wwCellAlbedoProbePass( Scene * scene );
//! census echo: "celllit=on lights=N dalc=.. dir=.. probe=.. red=.."
QString wwCellLightsEcho( Scene * scene );
//! the red bits (1 linear, 2 axis, 4 nodalc); the cell view applies "axis" when it publishes
int wwCellLightsRed();
//! a material's number for the cube probe (WW_CELL_CUBE_DUMP set), else 0
int wwCellCubeTag( const QString & material );

/*! The imagespace: the game's HDR -> display chain (exposure from mean luminance, Hable tonemap, grade, LUT),
 *  transcribed from the shipped Shaders011.fxp. Row ships off; WW_CELL_IS pins it, WW_CELL_IS_DUMP=<file> dumps it. */
bool wwCellImageSpaceOn();
void wwCellImageSpaceSetOn( bool on );
//! the cell view draws through the imagespace: cell-lit, the row on, the document's cell has one
bool wwCellImageSpaceWanted( Scene * scene );
//! the measure pass's switch: while true every cell-lit fragment writes its raw linear colour
void wwCellImageSpaceMeasuring( bool on );
//! true inside the measure pass: every program that is neither cell-lit nor an effect is masked
bool wwCellImageSpaceIsMeasuring();
//! effects write their color into the measure, as the game's effects feed its adaptation and bloom
bool wwCellImageSpaceMeasuresEffects();
/*! A cell-lit effect draws with fo4_effectcell.prog, the game's effect shader transcribed (linear color, soft
 *  fade, fog, imagespace). WW_CELL_FX_RED=legacy|nosoft|nolin|hide|prewater: harness switches. */
int wwCellFxRed();
//! the measured mean luminance of the last measure (negative before any)
void wwCellImageSpaceSetAdapted( Scene * scene, float lum, int pixels );
//! lane PERF1: the document's adapted value (negative before any measure)
float wwCellImageSpaceAdapted( Scene * scene );
/*! Bloom from the measure's float RGBA: HNAM scale * max(0, c - threshold), blurred 15 taps each way; the
 *  shader adds it to the HDR before the exposure */
void wwCellImageSpaceSetBloom( Scene * scene, const float * rgba, int w, int h, int step );
//! lane PERFFIX1: the bloom's threshold and scale (the imagespace HDR data's 3rd, 4th floats); false = no imagespace
bool wwCellImageSpaceBloomParams( Scene * scene, float & thresh, float & scale );
//! lane PERFFIX1: the vanilla bloom blurred on the GPU (cellhdr.cpp), w x h RGBA32F, sampled in place of the CPU's
void wwCellImageSpaceSetBloomGpu( Scene * scene, unsigned tex, int w, int h );
QString wwCellImageSpaceEcho( Scene * scene );

/*! lane PERFHUNT2: the obscurance and SSR passes' sorted draws run under the setup memo as the main sorted pass does
 *  (scene globals once per program, the cell-uniform class record replayed). Red WW_CELL_SORTMEMO_RED=1: unarmed. */
bool wwCellSortedMemo();
/*! lane PRTP6: a cell water shape (cellwater.h) drawn through a cell program reads no probe GI (water is dynamic);
 *  called per draw after wwCellAoDraw. Red WW_CELL_WATER_PRTP_RED=1: it reads the GI as before. */
void wwCellGiWaterDraw( Scene * scene, int block, bool cellProgram );

#endif
