/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_CELLLIGHTS_H
#define WW_CELLLIGHTS_H

/* CELL LIGHTS (lane PRTP3, 2026-10-01; docs/PRTP_PLAN.md 2, docs/PRTP2_LIGHT_MODEL.md).
 *
 * The cell view drawn with the cell's own lights: every placed LIGH (point and spot, the PRTP2
 * curve), and for an interior its XCLL / lighting template directional ambient (DALC) and
 * directional light, in place of the viewport's camera light. The cell view (src/cellview.cpp)
 * publishes one WwCellLighting per document it builds; the renderer swaps fo4_default for
 * fo4_cell.prog (the same shader with WW_CELLLIGHTS compiled in) on every draw of that document
 * while the Cell lights row is on. Every other document, and the row off, draw as before.
 *
 * Units: colors are linear (byte/255)^2.2 x FNAM fade, as PRTP2 section 0. The legacy FO4 program
 * works in sqrt-of-linear (its tonemap squares), so the cell branch sums in linear and writes the
 * square root.
 *
 * The spot axis is MEASURED, not assumed (scratchpad/prtp1_20260930/spot_axis.py over
 * Fallout4.esm): of 2,396 placed spots, local +X aims 1,177 down and 581 up, -Z 608 / 131, every
 * other axis under 6% down. +X it is; the PRTP4 capture is its refuter.
 *
 * Harness pins (read once): WW_CELL_LIT=0|1 (the row), WW_CELL_LIT_PROBE=<mode> (every cell-lit
 * fragment writes raw: 1 the placed lights' irradiance / 4, 2 the world position's high bytes,
 * 3 its low bytes (each over the 65536-unit box centred on the published centre), 4 the world
 * normal * 0.5 + 0.5), WW_CELL_LIT_RED=<red>: "linear" (the radial curve without its 2.2 power),
 * "axis" (spots shine along -Z), "nodalc" (the interior ambient dropped), "ambientlit" (the Ambient Only
 * lights, which add no direct light in game, drawn as ordinary lights; lane AMBO1), "ambientfull" (the Ambient
 * Only lights' ambient adjustment ignored; lane AMBO2). Probe 5 (lane PRTPGI): the bounce's irradiance E(N) / pi,
 * raw. Probe 11 (lane AMBO2): the interior ambient's per-channel affine sum before its 2.2, x 8, clamped to 0..1. */

#include <QString>
#include <QVector>

#include <vector>

class Scene;

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
	bool noRim = false;             //!< lane RIM1: flag 0x80000 (No Rim Lighting), the back-light term dropped
	bool ignoreRoughness = false;   //!< lane RIM1: flag 0x40000, Lambert diffuse and no back-light
	int shadow = 0;                 //!< lane SHADOW1: 0 none, 1 spot (0x400), 2 hemisphere (0x800), 3 omni (0x1000)
	float nearClip = 10.0f;         //!< DATA Near Clip + XLIG Near Clip delta: casters nearer the light cast nothing
	float shadowBias = 0.0f;        //!< XLIG Shadow Depth Bias (read and echoed; its scale is unread, not applied)
};

/* lane AMBO2: an Ambient Only light (LIGH flag 0x100000) as the game draws it: a sphere volume of 1.22077 x its
 * radius (base + XRDS) at the light; the cell ambient of every surface inside it has each channel's affine sum
 * (before the 2.2) scaled by k = pow(byte / 255, 2.2) x fade. The first light in plugin order that holds a
 * point wins; it replaces the cell ambient there, never adds. No fade at the edge, no camera rule. */
struct WwCellAmbientLight
{
	float pos[3] = { 0, 0, 0 };     //!< world
	float volume = 0.0f;            //!< 1.22077 x radius
	float k[3] = { 1, 1, 1 };       //!< per channel, folded into the ambient before its power
};

struct WwCellLighting
{
	bool interior = false;
	QVector<WwCellLight> lights;
	QVector<WwCellAmbientLight> ambientLights;  //!< lane AMBO2, in plugin order (the first that holds a point wins)
	bool hasDalc = false;
	float dalc[6][3] = {};          //!< byte / 255 as stored (PRTP2 powers AFTER the affine sum), X+ X- Y+ Y- Z+ Z-
	bool hasDirectional = false;
	float dirColor[3] = { 0, 0, 0 };    //!< linear
	float dirTo[3] = { 0, 0, 1 };       //!< world, TO the light
	float center[3] = { 0, 0, 0 };      //!< the probe modes' position box centre
	/* lane IMGS1: the cell's imagespace (XCIM -> IMGS, wbDefinitionsFO4 layout), empty when the cell has none.
	 * hdr = HNAM: eye adapt speed, tonemap E, bloom threshold, bloom scale, exposure max, exposure min, sunlight
	 * scale, sky scale, middle gray; cine = CNAM saturation, brightness, contrast; tint = TNAM amount, r, g, b;
	 * lut = the TX00 3D LUT, 16x16x16 RGB8 (r fastest), empty when the strip was not found. */
	bool hasImageSpace = false;
	float isHdr[9] = { 3, 7, 0.6f, 0.5f, 0.15f, 0.15f, 1.8f, 1.5f, 3 };
	float isCine[3] = { 1, 1, 1 };
	float isTint[4] = { 0, 1, 1, 1 };
	QString isName, isLutPath;
	std::vector<unsigned char> isLut;
	/* lane FOG2: the interior's fog, packed as the weather fog (esmweather.h wwFogPackK, lookdev_fog.glsl):
	 * each field from XCLL or, by its Inherits flag, the lighting template (0x4 colours, scales, heights and
	 * high density; 0x8 near; 0x10 far; 0x100 power; 0x200 max). The game's clamps: far <= 0 or > 163840
	 * reads 163840, near <= 0 or > far reads 0.17 far, so an interior always fogs. Colours byte / 255 x their
	 * scale, then pow 2.2. Heights are world z. */
	bool hasFog = false;
	float clipDist = 0.0f;              //!< lane SSR1: an interior's clip distance (XCLL offset 32; Inherits 0x80), 0 = none
	float fogK[6][4] = {};
	QString fogNote;
	QString summary;                    //!< one census line
};

//! the cell view publishes the lighting of the document it just built (replacing that document's last)
void wwCellLightsPublish( const void * nif, const WwCellLighting & lighting );
const WwCellLighting * wwCellLightsFor( const void * nif );

/*! THE BOUNCE (lane PRTPGI, src/probegi.h): the bake relit by these lights, as a voxel grid of
 *  probe ambient cubes. Six slabs (+X -X +Y -Y +Z -Z) of dims[2] each, x fastest, (rgb x valid,
 *  valid) a voxel; the shader adds albedo x E(N) / pi, E blended over the three facing slabs by n^2. */
struct WwCellGi
{
	float origin[3] = { 0, 0, 0 };
	float voxel = 64.0f;
	int dims[3] = { 0, 0, 0 };
	std::vector<float> rgba;
	QString summary;
};
void wwCellGiPublish( const void * nif, const WwCellGi & gi );
const WwCellGi * wwCellGiFor( const void * nif );
//! the GI row (ships off); the pin WW_CELL_GI wins. Draws only while the Cell lights row is on.
bool wwCellGiOn();
void wwCellGiSetOn( bool on );

//! the Cell lights row (ships off); the pin WW_CELL_LIT wins
bool wwCellLightsOn();
void wwCellLightsSetOn( bool on );

//! this scene draws cell-lit: the row on, the scene's document published, a perspective non-picking draw
bool wwCellLightsWanted( Scene * scene );
//! the uniforms of the renderer's CURRENT program (a no-op for a program without `cellOn`)
void wwCellLightsUniforms( Scene * scene );
//! a harness probe pass (WW_CELL_LIT_PROBE or WW_CELL_FOG_PROBE) on a cell-lit scene: effect shapes are not drawn
bool wwCellProbePass( Scene * scene );
//! census echo: "celllit=on lights=N dalc=.. dir=.. probe=.. red=.."
QString wwCellLightsEcho( Scene * scene );
//! the red bits (1 linear, 2 axis, 4 nodalc); the cell view applies "axis" when it publishes
int wwCellLightsRed();
//! lane CUBE1: a material's number for the cube gate's probe (WW_CELL_CUBE_DUMP set), else 0
int wwCellCubeTag( const QString & material );

/*! THE IMAGESPACE (lane IMGS1): the game's own HDR -> display chain, transcribed from the shipped shaders
 *  (Fallout4 - Shaders.ba2, Shaders011.fxp; docs/PRTP_PLAN.md 2j):
 *    adapted  = the frame's mean luminance (0.2125, 0.7154, 0.0721), Inf/NaN read as 0 (the downsample
 *               chain and its adaptation step; a still view is the converged value)
 *    exposure = clamp( middle gray / (adapted + 0.001), exposure min, exposure max )
 *    x        = exposure * hdr;  Hable (A .15, B .5, C .1, D .2, E = tonemap E, F .3), W 11.2, x2 in
 *    grade    = mix(luma, c, saturation) -> mix(.., luma * tint, amount) -> contrast * (brightness * c -
 *               adapted) + adapted
 *    display  = LUT( pow(grade, 1 / 2.2) * 15/16 + 1/32 )
 *  bloom    = added to hdr before the exposure (lane BLOOM1, wwCellImageSpaceSetBloom below)
 *  The measure: the frame drawn raw (probe 6) into a float target at a quarter size, read back. Row ships
 *  off; the pin WW_CELL_IS wins. WW_CELL_IS_DUMP=<file> writes the measure (full size) and the numbers;
 *  WW_CELL_IS_RED=nolut|noexp|nograde|nobloom|nofx its refuters. */
bool wwCellImageSpaceOn();
void wwCellImageSpaceSetOn( bool on );
//! the cell view draws through the imagespace: cell-lit, the row on, the document's cell has one
bool wwCellImageSpaceWanted( Scene * scene );
//! the measure pass's switch: while true every cell-lit fragment writes its raw linear colour
void wwCellImageSpaceMeasuring( bool on );
//! true inside the measure pass: the renderer masks every program that is neither cell-lit nor an effect
//! (sky, debug draw nothing there)
bool wwCellImageSpaceIsMeasuring();
//! lane EXPO1: effect shaders write their colour into the measure, as the game's effects land in the HDR
//! target its adaptation and bloom read (false under WW_CELL_IS_RED=nofx)
bool wwCellImageSpaceMeasuresEffects();
/*! THE EFFECTS (lane EFX2, docs/PRTP_PLAN.md 2q): a cell-lit effect draws with fo4_effectcell.prog, the game's
 *  effect shader (Shaders011.fxp, the effect group) transcribed: no room light (its only "lighting" is a
 *  script-set emit colour, white when unset), colour linear (texture and base colour decoded with 2.2), alpha x the
 *  soft fade x the near fade for a Soft effect, the fog, then the cell's imagespace (no bloom: the frame has it).
 *  WW_CELL_FX_RED=legacy (the viewer's effect shader, as before), nosoft (both fades 1), nolin (no 2.2 decode),
 *  hide (the gate's reference: cell-lit effects draw nothing), comma-separated. tests/spells/cell_fx.sh gates it. */
int wwCellFxRed();
//! the measured mean luminance of the last measure (negative before any)
void wwCellImageSpaceSetAdapted( Scene * scene, float lum, int pixels );
/*! lane BLOOM1: the bloom from the measure's float RGBA (w x h, step 4 when it is full size: box-averaged to a
 *  quarter first), HNAM bloom scale * max(0, c - bloom threshold), blurred 15 taps vertical then horizontal;
 *  the shader adds it (bilinear, a quarter of the view) to the HDR before the exposure */
void wwCellImageSpaceSetBloom( Scene * scene, const float * rgba, int w, int h, int step );
QString wwCellImageSpaceEcho( Scene * scene );

/*! THE SHADOWS (lane SHADOW1, docs/PRTP_PLAN.md 2k): every shadow-casting light (LIGH flags 0x400 spot,
 *  0x800 hemisphere, 0x1000 omni) within reach, up to kShadowSlots of them nearest the camera, gets a depth
 *  cube: its distance to the nearest opaque caster over its radius, 6 faces of kShadowFace texels, rendered
 *  from the document's own shapes (opaque, depth-writing, not alpha-tested; casters nearer the light than its
 *  near clip cast nothing). The shader compares its own distance - a slope bias, 3x3 taps a texel apart, /9;
 *  a hemisphere light lights nothing behind its plane (its local +X, like a spot). Drawn before the frame;
 *  re-rendered only when the document, its lights or the slot set change. The game's map is a (dual)
 *  paraboloid with its resolution halved per 750 units of distance; ours is a cube face of 512 (the
 *  paraboloid's centre texel at 1024) at any distance.
 *  Part of the Cell lights row (no row of its own: a light's shadow is part of the light).
 *  Pins: WW_CELL_SHADOW=0 (the harness's unshadowed pass), WW_CELL_SHADOW_RED=noshadow (every
 *  factor 1: the gate's refuter). Probe 7: the shadow factors of slots 0, 1, 2 in r, g, b (0.5 / 255 marks a
 *  pixel out of that light's reach). */
void wwCellShadowPass( Scene * scene );
QString wwCellShadowEcho( Scene * scene );

/*! THE AMBIENT OBSCURANCE (lane AO1, docs/PRTP_PLAN.md "ambient obscurance"): the game's screen-space obscurance, from its own
 *  shaders. Its settings are the INI's, never the cell's (radius 108.2, bias 0.6, intensity 7.1 game units, on
 *  in every cell). The game computes it at half the view from the opaque pass's depth and normals: 5 taps over
 *  2 turns, a disc of radius x 100 / depth pixels, the depth read from a min-of-2x2 mip by the tap's reach,
 *  1 - intensity x sum f^3 max((v.n - bias') / (v.v + 0.01), 0) / r^6 with f = max(r^2 - v.v, 0), the bias
 *  growing past 0.3 of 7000 units and toward the screen edge; turned by a random angle each frame and kept
 *  0.99 of the history, which starts over from a frame at 0.95 or more when it is under 0.7 (a still view =
 *  that history's time average, over kAoAngles angles here); then a 7-tap bilateral blur across and down. It multiplies EVERYTHING its deferred composite writes (the lights, the
 *  ambient, the specular, emissive, reflections) before the fog; nothing drawn blended (forward) or as an effect.
 *  Here: wwCellAoPass draws the frame's opaque cell-lit fragments once more (probe 20: view normal, linear
 *  depth) into a full-size float target, runs res/shaders/cell_ao.frag over it, and the cell programs multiply
 *  their lit colour by the bilinear sample before their fog. Part of the Cell lights row.
 *  Pins: WW_CELL_AO=0 (none computed), WW_CELL_AO_RED=off (computed, not applied) | radius (half the radius)
 *  | noblur (the raw value applied) | noreset (the plain mean over the angles), WW_CELL_AO_DUMP=<file> (the
 *  opaque pass, the raw and the final obscurance, the numbers; tests/spells/cell_ao.sh). */
void wwCellAoPass( Scene * scene, bool run );
//! called with every program setupProgram binds: the opaque pass's masks, and the obscurance a cell-lit draw reads
void wwCellAoDraw( Scene * scene, bool cellProgram );

#endif
