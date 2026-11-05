#pragma once
// PERFFIX2 (2026-11-01): when the baked AO decals (src/cellaodecal.h) are built and drawn. Pure: tests/aodecalrtx.
//
// The decals scale the old PRT's relit sky term only (res/shaders/features/lights/cell_lights.glsl cellGiE, under WW_CELL_PRT). The
// ray-traced GI volume traces the decal copies themselves (they stay in the probe soup), so without the PRT nothing
// reads a decal; a decal on the volume's term would darken those copies twice.

//! rowOn: the row / WW_CELL_AODECAL; prtOn: the old baked GI drawn (wwCellPrtOn); red: WW_CELL_AODECAL_RTX_RED=1
inline bool aoDecalWorksRule( bool rowOn, bool prtOn, bool red )
{
	return rowOn && ( prtOn || red );
}
