#ifndef CELLBLOOM_H
#define CELLBLOOM_H

#include <QString>

#include <cstdint>

class Renderer;
class Scene;

/*! FO4CS's bloom and physical glare (src/Post/Bloom.cpp, BloomDownCS/UpCS.hlsl) on the cell view's linear frame,
 *  ported to fragment passes; a selectable mode beside the game's imagespace bloom (Vanilla, the default).
 *  Only cell-lit pixels feed it; glow over non-imagespace pixels is added in display space.
 *  Switches: WW_CELL_BLOOM=vanilla|fo4cs, WW_CELL_GLARE=0|1, WW_CELL_BLOOM_<setting> / WW_CELL_GLARE_<setting> pin
 *  a row; WW_CELL_BLOOM_RED=nokaris, WW_CELL_BLOOM_IMPULSE=<value>, WW_CELL_BLOOM_DUMP=<dir> are debug aids. */

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
