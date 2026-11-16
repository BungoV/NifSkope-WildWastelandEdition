#pragma once
/* lane VKSMALL5 (2026-11-16, census row 77): the soft-effect layer's two full-screen passes (cellfxres.cpp
 * wwCellFxResDraw step 1 cell_fxres_down, step 3 cell_fxres_comp) on the present device, behind WW_VKVIEW=1 (red
 * WW_VKVIEW_FXRES_RED=1 keeps GL's quads). The layer's five targets are shared twins GL attaches (z R32F, depth D32F,
 * c / t RGBA16F, cov R8); GL draws the soft draws into them between the two passes, so each pass is its own crossing and
 * GL waits on Vulkan after each. The composite draws into the shared tone frame (colour + stencil), dual-source blended. */
#include "features/vkframe/vkpresent.h"

#include <cstdint>
#include <string>

namespace WwVkPresent
{
struct FxresTargets
{
	unsigned z = 0, depth = 0, c = 0, t = 0, cov = 0;  //!< GL names (the route owns them: never glDeleteTextures)
};
/* GL current: the layer's five shared targets at w x h for owner (any stable pointer: one set an owner; a new size makes
 * them again). false: why, nothing shared (GL makes its own). */
bool fxresTargets( const void * owner, int w, int h, FxresTargets & out, std::string & why );
//! owner's targets are still these live shared names (a view shutdown drops every shared target)
bool fxresTargetsAre( const void * owner, const FxresTargets & t );
void fxresTargetsDrop( const void * owner );

struct FxresDown
{
	const void * owner = nullptr;  //!< fxresTargets' owner: the run draws into its z and depth
	int W = 0, H = 0;              //!< the full-resolution frame (fxrSrcSize)
	int scale = 0;                 //!< fxrScale (1, 2 or 4)
	unsigned src = 0;              //!< fxrSrc: the opaque depth (Scene::fxDepthTexId), a 2D depth texture W x H
	unsigned srcFmt = 0;           //!< its internal format (GL_DEPTH24_STENCIL8, GL_DEPTH32F_STENCIL8, GL_DEPTH_COMPONENT32F...)
	std::uintptr_t srcKey = 0;     //!< src's shared-target key when it is one (read in place), else 0 (copied into a twin)
	int red = 0;                   //!< gates only: 4 max instead of min (the SPIR-V's FMin patched to FMax in a copy)
};
//! GL current, fxresTargets made: z and depth drawn on Vulkan, GL waits. false: why (GL draws its quad)
bool fxresDownRun( const FxresDown & r, std::string & why );

struct FxresComp
{
	const void * owner = nullptr;  //!< fxresTargets' owner: c, t, cov, z read
	unsigned color = 0;            //!< the draw target's colour: must be the shared tone frame's
	int W = 0, H = 0;              //!< the frame
	int scale = 0;                 //!< fxrScale
	unsigned src = 0, srcFmt = 0;  //!< fxrDepth: the opaque depth, as FxresDown
	std::uintptr_t srcKey = 0;
	float proj[2] = {};            //!< fxrProj: the projection's [2][2], [3][2]
	float edge = 0.0f;             //!< fxrEdge
	int mode = 0;                  //!< fxrMode (1: WW_CELL_FXRES_RED=bilinear)
	int vp[4] = {};                //!< GL's viewport
	int red = 0;                   //!< gates only: 1 stencil write mask 0 (no stencil written), 2 blend dst ONE (no SRC1)
};
//! GL current, GL's draw framebuffer the tone frame: the composite drawn on Vulkan, GL waits. false: why (GL draws it)
bool fxresCompRun( const FxresComp & r, std::string & why );

//! what the route cannot take on this device (empty: none)
std::string fxresRefusal();
void fxresOnGl( const char * why );
FxStat takeFxresStat();  //!< runs = crossings (down + comp), copies = fxrSrc copies into its twin
//! SPIR-V words patched by the last red-4 module (0: the red changed nothing)
int fxresRedPatches();
}  // namespace WwVkPresent
