/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_SCENELIGHTING_H
#define WW_SCENELIGHTING_H

/* Scene lighting state (lane PBRR2A, docs/NIFSKOPE_PBR_RENDERER.md s5.2, stage R2a).
 *
 * One process-wide state that the Scene window writes and the PBR program reads:
 *   mode            Legacy (today's code) | Studio (linear, EV, view transform)
 *   exposure        EV; the shader multiplies by 2^EV before the view transform
 *   view transform  Standard (clamp + sRGB) | AgX | Khronos PBR Neutral
 *
 * Only pbrm_default reads these (per-program uniforms), so legacy programs are
 * untouched by construction. The mode ALWAYS starts Legacy (ruling Q7) and is
 * never restored; exposure and the view transform are remembered.
 *
 * Harness pins (read once, at the first call): WW_LIGHTING_MODE=legacy|studio|lookdev
 * (WW_LOOKDEV=1 alone also selects Lookdev; lane PBRR2B),
 * WW_EXPOSURE_EV=<float>, WW_VIEW_TRANSFORM=standard|agx|neutral,
 * WW_STUDIO_PROBE=<linear> (the lit result is replaced by this constant before
 * exposure: the output-path gate), WW_STUDIO_CUBE=<texture path>,
 * WW_R2A_RED=<name> (red controls; see wwR2aRed). */

#include <QByteArray>
#include <QString>

class NifModel;
class TexCache;

enum WwSceneMode { WwSceneLegacy = 0, WwSceneStudio = 1, WwSceneLookdev = 2 };	// Lookdev: lane PBRR2B (gl/lookdevstage.h)
enum WwViewTransform { WwViewStandard = 0, WwViewAgX = 1, WwViewPbrNeutral = 2 };

int wwSceneMode();
void wwSetSceneMode( int mode );
float wwSceneExposureEV();
void wwSetSceneExposureEV( float ev );
//! the factor the shader multiplies by: 2^EV (red "ev": 2^(EV/2))
float wwSceneExposureScale();
int wwSceneViewTransform();
void wwSetSceneViewTransform( int vt );
//! < 0 = off
float wwStudioProbe();
//! the Studio environment cube (one for the whole scene until R2b's lookdev)
QString wwStudioCubePath();
//! true when WW_R2A_RED names this red control
bool wwR2aRed( const char * name );
/* Lane PBRR3 harness pins (read once with the others):
 *   WW_STUDIO_SUN=<scale>   the Studio/Lookdev sun times this (0 = the white furnace: env only)
 *   WW_R3_TERM=diffuse|specular|fresnel   the PBR program outputs that one term (gates s1, s2, s3;
 *                           fresnel = the surface Fresnel at N.V, lane PBRR4 gate s1b)
 *   WW_R3_RED=noms|nosplit|fo4csweight|notint|f90scaled|lambert   R3 red controls (pbrm_default.frag
 *                           r3Red bits 1/2/4/8/16/32; f90scaled + lambert are lane PBRR4's s1b/d1 reds)
 * Lane PBRR4:
 *   WW_R4_RED=nodiv|emitmul|notintmask|emitraw|nocomp   R4 red controls (r4Red bits 1/2/4/8/16):
 *     nodiv = tint Normalize never divides, emitmul = the old constant x map emission,
 *     notintmask = no tint mask, emitraw = emission without the /100, nocomp = NIF alpha, not .pbrm */
float wwStudioSunScale();
int wwR3Term();
int wwR3RedBits();
int wwR4RedBits();
//! stored values (exposure, view transform) -> state; pins win
void wwSceneLoadSettings();
void wwSceneSaveSettings();
//! census/log echo: "lighting=studio ev=+1.00 view=standard ..."
QString wwSceneEcho();

/* Studio cube (SFCubeMapCache for FO4, Studio only).
 *
 * wwStudioCubeNormalise rewrites a cube DDS's header so the prefilter accepts it:
 * FO4 ships its cubes with the cube bits in dwCaps and dwCaps2 = 0 (measured on
 * textures/shared/cubemaps/mipblur_DefaultOutside1.dds: caps 0x40FE08, caps2 0),
 * which the prefilter and gli both read as a 2D texture. 8-bit colour formats are
 * tagged sRGB so the filter decodes them to linear (red "cubedecode" keeps UNORM).
 * Returns false with a reason for anything it cannot make a cube of. */
bool wwStudioCubeNormalise( QByteArray & data, QString * why );
//! normalise + prefilter + upload: id[0] = GGX-prefiltered cube, id[1] = 32 px irradiance cube
bool wwStudioCubeLoad( TexCache * tc, const NifModel * nif, const QString & name, QByteArray data,
	unsigned int * id, QString * why );
//! bind the (cached) Studio cube of `fname` on the ACTIVE unit; false = not available
bool wwBindStudioCube( TexCache * tc, const NifModel * nif, const QString & fname, bool irradiance );

#endif
