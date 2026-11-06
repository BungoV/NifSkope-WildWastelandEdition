/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_FEATURES_AO_H
#define WW_FEATURES_AO_H

/*! lane MODULES1: the ambient occlusion module's interface (src/features/ao). Rows for AO (WW/CellAo, WW_CELL_AO);
 *  each env pin wins over its row. The method and quality types: cellaomethod.h. */

class Scene;
class NodeList;
class QMenu;
class QObject;
class QString;

#include <functional>

bool wwCellAoOn();
void wwCellAoSetOn( bool on );
bool wwCellAoPinned();	// lane GAPTOOL1: the Scene window's SSAO row greys on the pin
/*! lane SSAO2: the Ambient Occlusion method (src/features/ao/cellaomethod.h WwAoMethod): WW/CellAoMethod, pin
 *  WW_CELL_AO_METHOD=vanilla|gtao|xegtao|hbao; live (the next frame runs it) */
int wwCellAoMethod();
void wwCellAoSetMethod( int method );
bool wwCellAoMethodPinned();
/*! lane PERFFIX2 (item 9): the AO quality tier 0 Low .. 3 Ultra (cellaomethod.h WwAoTier*; the default Ultra is the
 *  picture before the tiers): WW/CellAoQuality, pin WW_CELL_AO_QUALITY=low|medium|high|ultra; live */
int wwCellAoQuality();
void wwCellAoSetQuality( int tier );
bool wwCellAoQualityPinned();

/*! Ambient obscurance: the game's screen-space AO (INI settings, not the cell's) computed from a pass of the
 *  opaque cell-lit fragments by res/shaders/features/ao/cell_ao.frag; it multiplies the lit color before the fog. WW_CELL_AO=0 skips it. */
void wwCellAoPass( Scene * scene, bool run );
/*! lane PERFHUNT2: the sorted list this paint's obscurance walk collected, appended to out in its collected order (the
 *  same walk, same camera, same paint: what another collectShapes would add). false (out untouched): no walk this paint
 *  for that scene. Taken once; the next obscurance pass forgets it. */
bool wwCellAoSortedTake( Scene * scene, NodeList & out );
//! called with every program setupProgram binds: the opaque pass's masks, and the obscurance a cell-lit draw reads
//! MODULES1: `shape` (the drawn Shape) names the draws the depth pre-pass holds, which the main pass draws EQUAL
void wwCellAoDraw( Scene * scene, bool cellProgram, const void * shape = nullptr );
/*! MODULES1: the main pass takes this paint's depth pre-pass (a depth blit into the bound linear frame, same samples
 *  and format) and draws its draws with depth func EQUAL, writes off, until wwCellAoPrepassEnd. No pre-pass this paint
 *  (red WW_CELL_AO_PREPASS_RED=geom, no linear frame of its size): nothing happens and the main pass draws LEQUAL. */
void wwCellAoPrepassShare( Scene * scene );
//! MODULES1: the main pass is drawn: depth func back to LEQUAL, and the census line ("cell ao prepass: ...")
void wwCellAoPrepassEnd( Scene * scene );
//! MODULES1: the pre-pass's note (msaa, draws, or why it fell back to the geometry pass)
QString wwCellAoPrepassEcho();
//! lane PERF3: the opaque pass is drawing
bool wwCellAoInPass();
//! lane PERF3: the AO opaque pass skips the SSR/SSGI per-draw hooks (the main pass sets them again per draw);
//! WW_CELL_AO_LEAN=0 or the red WW_CELL_AO_LEAN_RED=1 runs them
bool wwCellAoLean();

//! the AO pass's targets of this frame (src/features/ao/ao.cpp); false unless its opaque pass ran for this scene
struct WwCellAoTargets
{
	unsigned int gbuf = 0;		// full size RGBA32F, bottom row first: view normal, linear depth (game units)
	unsigned int depthRb = 0;	// its depth-stencil renderbuffer
	unsigned int mip[5] = {};	// 1..4: min-of-2x2 depth (R32F), top row first
	int w = 0, h = 0, mipW[5] = {}, mipH[5] = {};
};
bool wwCellAoTargets( Scene * scene, WwCellAoTargets & out );

/*! lane MODULES1: the Cell workspace's AO submenus (src/features/ao/aomenu.cpp), built into parent; changed runs after
 *  a pick (the workspace repaints). Object names CellWorkspaceAoMethod / CellWorkspaceAoQuality. */
QMenu * wwAoMethodMenu( QMenu * parent, QObject * ctx, std::function<void()> changed );
QMenu * wwAoQualityMenu( QMenu * parent, QObject * ctx, std::function<void()> changed );

#endif
