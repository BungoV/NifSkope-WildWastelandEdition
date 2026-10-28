#ifndef CELLBLOOM_H
#define CELLBLOOM_H

#include <QString>

#include <cstdint>

class Renderer;
class Scene;

/*! Lane BLOOM1b (2026-10-13): THE FO4CS BLOOM AND PHYSICAL GLARE on the cell view's linear frame (lane HDR1,
 *  cellhdr.h), as a selectable mode beside the game's imagespace bloom ("Vanilla", the default, untouched).
 *
 *  The maths is FO4CS's (src/Post/Bloom.cpp, res/Post/BloomDownCS.hlsl / BloomUpCS.hlsl), carried over to
 *  fragment passes (no compute shaders in GL 4.1):
 *   - the source: the cell-lit pixels of the linear frame (stencil 1), times the imagespace's exposure -- FO4CS
 *     runs exposure, then bloom, then the tone map ("radiometric order matters"); resolved to one sample;
 *   - the mip plan: each level (dim + 1) / 2 while either side is above 1, at most min(mips, 12) levels;
 *   - the down chain: four bilinear taps at +-0.5 source texel, each sanitised (NaN 0, clamped to 0..65504),
 *     level 0 Karis-weighted (1 / (1 + luma)) and multiplied by the soft-knee threshold weight;
 *   - the up chain: a 9-tap tent (4 / 2 2 2 2 / 1 1 1 1, over 16) at bloom texel x radius, red and blue taps
 *     pushed along the radial by the dispersion, each band weighted by its share of the intensity and the
 *     glare's diffraction band (exp2(-level / width), width from the F-number, normalised over the chain);
 *   - the final combine: the frame + tent(the chain's top) x the bloom weight (intensity + glare strength,
 *     clamped to 10), before the imagespace's curve, grade and LUT (cell_hdr.frag).
 *  The vanilla bloom is not drawn while this one is (FO4CS suppresses the engine bloom the same way).
 *
 *  Deliberate differences from FO4CS (each one written where it lives):
 *   - only the cell-lit pixels feed the bloom (the other programs' values are display values, not light);
 *   - the glow over the pixels the imagespace does not take (the sky, the other programs) is added in display
 *     space, as the tone-mapped difference the bloom makes there (lane SKYHOLE1's additive-glow convention);
 *   - the display peak for the glare is FO4CS's SDR paper white (250 nits): the view is an SDR window.
 *
 *  Rows (Cell workspace, Show > Effects > Bloom): the mode (Vanilla | FO4CS), Threshold, Soft knee, Intensity,
 *  Radius, Mip levels, Physical glare, Glare strength, Glare F-number, Dispersion. Persisted under WW/CellBloom*,
 *  applied on the next paint. Vanilla is the default and draws exactly what it drew before the lane.
 *
 *  Pins (a harness without the mode pin runs Vanilla whatever was saved; the value pins win over the rows):
 *   WW_CELL_BLOOM=vanilla|fo4cs     the mode
 *   WW_CELL_GLARE=0|1               the physical glare
 *   WW_CELL_BLOOM_THRESHOLD, WW_CELL_BLOOM_KNEE, WW_CELL_BLOOM_INTENSITY, WW_CELL_BLOOM_RADIUS,
 *   WW_CELL_BLOOM_MIPS, WW_CELL_GLARE_STRENGTH, WW_CELL_GLARE_FNUMBER, WW_CELL_GLARE_DISPERSION
 *   WW_CELL_BLOOM_RED=nokaris       level 0 is a plain box (no Karis weight): the gate's red, which the numpy
 *                                   rebuild of FO4CS's chain must refuse
 *   WW_CELL_BLOOM_IMPULSE=<value>   the source is the 2x2 block holding (W / 2, H / 2) at this value, every other
 *                                   pixel 0 (no stencil): the energy-spread gate's input. A block, not one pixel:
 *                                   level 0's Karis average (FO4CS's firefly fix) is built to drop a lone pixel
 *   WW_CELL_BLOOM_DUMP=<dir>        the source, every mip and every up level once, raw floats (cellpost.cpp's dump
 *                                   format), plus bloom.txt (the plan and the weights) -- the numpy rebuild's food */

struct WwCellBloomSettings
{
	bool fo4cs = false;			// false: the game's imagespace bloom (Vanilla)
	float threshold = 1.0f;		// FO4CS fBloomThreshold, 0..64
	float softKnee = 0.5f;		// fBloomSoftKnee, 0..1
	float intensity = 0.45f;	// fBloomIntensity, 0..10 (FO4CS's menu sets 0.45 when enabled at 0)
	float radius = 1.0f;		// fBloomUpsampleRadius, 0.5..4
	int mips = 6;				// iBloomMipLevels, 0..12
	bool glare = false;			// bPhysicalGlareEnabled
	float glareStrength = 0.10f;	// fPhysicalGlareStrength, 0..4
	float glareFNumber = 4.0f;	// fPhysicalGlareFNumber, 1..22
	float glareDispersion = 0.15f;	// fPhysicalGlareDispersion, 0..2
};

//! The rows' state (saved, or pinned).
WwCellBloomSettings wwCellBloomSettings();
//! Set the rows (clamped as FO4CS clamps them); a pinned value stays pinned and is not saved.
void wwCellBloomSet( const WwCellBloomSettings & s );
//! True for a pinned setting (its row shows the pin and is greyed).
bool wwCellBloomPinned( const char * which );

//! FO4CS's CPU maths, as named there (Bloom.cpp), for the rows, the passes and the ported checks.
int wwBloomPlanMips( int w, int h, int maxLevels, int dims[][2] );
float wwBloomThresholdWeight( float luminance, float threshold, float softKnee );
float wwBloomExtractLuminance( float luminance, const WwCellBloomSettings & s );
float wwBloomWeight( const WwCellBloomSettings & s );	// ResolveBloomUpsampleParameters().bloomWeight
float wwBloomRadius( const WwCellBloomSettings & s );	// ... .radiusPixels
float wwBloomGlareBand( const WwCellBloomSettings & s, int level, int mipCount, float displayPeakNits = 250.0f );
//! FO4CS's own tests (tests/post_bloom_tests.cpp), ported: "N/N" passed, or the first failure.
QString wwCellBloomSelfTest();

/*! In wwCellHdrEnd, after the depth / stencil went back and before the tone map: the chain, when the mode is
 *  FO4CS. hdrColor is the linear frame (multisampled), ds its depth / stencil renderbuffer (lane TONEMAP1: a
 *  multisample texture when dsTexture). True when the chain
 *  ran and the tone map should take it (wwCellBloomFinal). */
bool wwCellBloomRun( Scene * scene, unsigned hdrColor, unsigned ds, int w, int h, int samples, unsigned dsFormat,
	bool dsTexture = false );
//! True between a successful wwCellBloomRun and wwCellBloomDone: the vanilla bloom stands down.
bool wwCellBloomReplacing();
//! The tone map's uniforms for the final combine, on the current program (cell_hdr.prog), the chain on `unit`.
//! knownActive: the active unit (GL_TEXTURE0 + n) the caller knows, -1 = ask (lane CELLUNI5)
void wwCellBloomFinal( Renderer * r, int unit, int knownActive = -1 );
//! After the tone map: unbinds, ends the frame's replacement.
void wwCellBloomDone( Renderer * r, int unit, int knownActive = -1 );
//! One line for the notes: the mode, the glare values and the mip count used (or why not).
QString wwCellBloomEcho();

#endif
