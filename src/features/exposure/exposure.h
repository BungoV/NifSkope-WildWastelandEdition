#ifndef WW_FEATURES_EXPOSURE_EXPOSURE_H
#define WW_FEATURES_EXPOSURE_EXPOSURE_H

// lane MODULES1 (exposure): the module's interface the light hub calls (src/features/exposure/exposure.cpp)

//! the eye's adaptation limit (lane MUSGI1): WW_CELL_EYE=a,L0 pins it, WW_CELL_EYE_RED=1 = full adaptation
struct WwCellEye
{
	bool red = false;
	float a = 0.4f, l0 = 0.14f;
};

WwCellEye wwCellEye();
//! the full-adaptation exposure: clamp( mid / (adapted + 0.001), HNAM min, HNAM max ) (h = the imagespace's HNAM)
float wwCellExposureFull( const float * h, float adapted );
//! the exposure with the eye's limit under L0 (echoed on a 2 % move)
float wwCellExposure( const float * h, float adapted );
//! WW_TONEMAP_RED=vanilla: the game's filmic curve instead of the Physical one (live, echoed once per change)
bool wwCellTonemapVanilla();

class Scene;
#include <QString>

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

#endif
