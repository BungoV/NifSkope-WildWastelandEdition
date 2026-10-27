#pragma once

// lane TERRAINBLOCK1: whether a near cell shape's own vertex alpha reaches its drawn vertex (cellview.cpp OutVert
// chan[3]). The game applies it wherever SLSF1 Vertex_Alpha is set, lit shapes too: a ground decal's border
// vertices fade it out through the alpha test (CoastDune02's grass, 0x9F at the border). A tree's vertex alpha is
// its wind weight, never opacity. `red` (WW_CELL_VERTEXALPHA_RED=1) is the old rule: effect shaders and cables only.
inline bool wwCellKeepsVertexAlpha( bool ownColor, bool effect, bool cableSway, bool vertexAlphaFlag, bool treeAnim,
	bool red )
{
	if ( !ownColor )
		return false;
	if ( effect || cableSway )
		return true;
	return !red && vertexAlphaFlag && !treeAnim;
}
