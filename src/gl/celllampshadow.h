#ifndef CELLLAMPSHADOW_H
#define CELLLAMPSHADOW_H

/*! lane LAMPSHADOW1: STORED LAMP SHADOWS (notes in WW_CHANGES). With a tier picked, each shadow lamp's depth cube
 *  is rendered once from the still (static) casters and kept in a store; a lamp entering a shadow slot is restored
 *  from it by exact depth copies. A caster that moves (a live door, an animated object, an NPC) becomes a mover:
 *  only the slots whose light volume holds a mover that moved are redrawn (the stored map, then the movers on top).
 *  A lamp whose back faces a wall (short rays find one plane square to a world axis behind it) stores only the
 *  half of its cube in front of the wall; the wall half is redrawn as that plane on restore.
 *  Row: Cell lights > Lamp shadows (Off | Low | Medium | High | Ultra), setting WW/CellLampShadow, pin
 *  WW_CELL_LAMPSHADOW=off|low|medium|high|ultra. Off = the shadow pass as before. Medium keeps today's 512 face.
 *  Reds: WW_CELL_LAMPSHADOW_RED=live (every held slot redrawn every frame from every caster) | cube (no wall half). */

#include <QString>

#include <vector>

class Shape;

//! the tier: -1 off, 0 Low, 1 Medium, 2 High, 3 Ultra
int wwCellLampShadowTier();
void wwCellLampShadowSetTier( int tier );
bool wwCellLampShadowPinned();
//! the cube face edge in texels (512 when off: the shadow pass's own)
int wwCellLampShadowFace();
//! the store's budget in MB for the tier (0 when off)
int wwCellLampShadowBudgetMB();
//! 1 live (redraw every frame), 2 cube (no wall half), 0 none
int wwCellLampShadowRed();
QString wwCellLampShadowTierName( int tier );

//! a wall behind a lamp: the cube face (0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z) that looks into it, its distance
struct WwLampWall
{
	int back = -1;
	float dist = 0.0f;
};

/*! Short rays from the lamp along the six world axes against the casters given (world space): 8 rays parallel to
 *  an axis, 24 and 64 units off it, must all hit one plane square to that axis within 64 units. back -1: none. */
WwLampWall wwLampFindWall( const float pos[3], const std::vector<const Shape *> & casters );

#endif // CELLLAMPSHADOW_H
