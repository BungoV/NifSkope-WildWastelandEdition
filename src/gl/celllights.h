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
#include <string_view>
#include <vector>

class Scene;

/*! A cell-lit program by name: fo4_cell / pbrm_cell and their sun-cascade variants. */
inline bool wwIsCellProgramName( std::string_view n )
{
	return n == "fo4_cell.prog" || n == "pbrm_cell.prog" || n == "fo4_cellcsm.prog" || n == "pbrm_cellcsm.prog";
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
	bool showSky = false;	//!< an interior whose CELL DATA has bit 7 (Show Sky); the Lookdev sky draws there
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
	QString summary;                    //!< one census line
};

//! the cell view publishes the lighting of the document it just built (replacing that document's last)
void wwCellLightsPublish( const void * nif, const WwCellLighting & lighting );
const WwCellLighting * wwCellLightsFor( const void * nif );

/*! The bounce (src/probegi.h): a voxel grid of probe ambient cubes, six slabs (+X -X +Y -Y +Z -Z) of
 *  dims[2] each, x fastest, rgba per voxel; the shader adds albedo x E(N) / pi. */
struct WwCellGi
{
	float origin[3] = { 0, 0, 0 };
	float voxel = 64.0f;
	int dims[3] = { 0, 0, 0 };
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
};
void wwCellGiPublish( const void * nif, const WwCellGi & gi );

/*! The PRTP band's Pass: 0 Combined, 1 GI, 2 Sky visibility, 3 Surfel color, 4 Surfel light, 5 Brick light,
 *  6 Probe coverage. Stored as WW/CellPass; WW_CELL_PASS (number or name) wins; WW_CELL_PV_RED is a harness switch. */
QStringList wwCellPassNames();
int wwCellPass();
//! the Pass this document draws: the stored one once its bake is relit (a GI grid published), else Combined
int wwCellPassFor( const void * nif );
void wwCellSetPass( int pass );
int wwCellPassRed();
const WwCellGi * wwCellGiFor( const void * nif );
//! the GI row (ships off); the pin WW_CELL_GI wins. Draws only while the Cell lights row is on.
bool wwCellGiOn();
void wwCellGiSetOn( bool on );
/*! Rows for AO (WW/CellAo, WW_CELL_AO), light shadows (WW/CellShadow, WW_CELL_SHADOW) and GPU relight
 *  (WW/CellGiGpu, WW_CELL_GI_GPU, read when a cell opens). Each env pin wins over its row. */
bool wwCellAoOn();
void wwCellAoSetOn( bool on );
bool wwCellShadowOn();
void wwCellShadowSetOn( bool on );
bool wwCellGiGpuOn();
void wwCellGiGpuSetOn( bool on );
/*! The Sky visibility row, 0-100 % (WW/CellSkyVis, default 100; pin WW_CELL_SKYSTR). As FO4CS's Skylighting
 *  strength: each sky visibility v becomes 1 + s (v - 1), so 0 % is the open sky everywhere, 100 % the bake. */
int wwCellSkyVisPercent();
float wwCellSkyVis();
bool wwCellSkyVisPinned();
void wwCellSkyVisSetPercent( int pct );

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
void wwCellLightsSetOn( bool on );

//! this scene draws cell-lit: the row on, the scene's document published, a perspective non-picking draw
bool wwCellLightsWanted( Scene * scene );
//! the uniforms of the renderer's CURRENT program (a no-op for a program without `cellOn`)
void wwCellLightsUniforms( Scene * scene );
//! a harness probe pass (WW_CELL_LIT_PROBE or WW_CELL_FOG_PROBE) on a cell-lit scene: effects are not drawn
bool wwCellProbePass( Scene * scene );
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
 *  fade, fog, imagespace). WW_CELL_FX_RED=legacy|nosoft|nolin|hide: harness switches. */
int wwCellFxRed();
//! the measured mean luminance of the last measure (negative before any)
void wwCellImageSpaceSetAdapted( Scene * scene, float lum, int pixels );
/*! Bloom from the measure's float RGBA: HNAM scale * max(0, c - threshold), blurred 15 taps each way; the
 *  shader adds it to the HDR before the exposure */
void wwCellImageSpaceSetBloom( Scene * scene, const float * rgba, int w, int h, int step );
QString wwCellImageSpaceEcho( Scene * scene );

/*! Shadows: each shadow-casting light near the camera (up to kShadowSlots) gets a depth cube rendered from the
 *  document's opaque shapes, re-rendered only when the document, lights or slots change. WW_CELL_SHADOW=0 skips it. */
void wwCellShadowPass( Scene * scene );
QString wwCellShadowEcho( Scene * scene );

/*! Ambient obscurance: the game's screen-space AO (INI settings, not the cell's) computed from a pass of the
 *  opaque cell-lit fragments by res/shaders/cell_ao.frag; it multiplies the lit color before the fog. WW_CELL_AO=0 skips it. */
void wwCellAoPass( Scene * scene, bool run );
//! called with every program setupProgram binds: the opaque pass's masks, and the obscurance a cell-lit draw reads
void wwCellAoDraw( Scene * scene, bool cellProgram );

#endif
