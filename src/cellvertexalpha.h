#pragma once

// lane TERRAINBLOCK1: whether a near cell shape's own vertex alpha reaches its drawn vertex (cellview.cpp OutVert
// chan[3]). A ground decal's border vertices fade it out through the alpha test (CoastDune02's grass, 0x9F at the
// border). A tree's vertex alpha is its wind weight, never opacity. `red` (WW_CELL_VERTEXALPHA_RED=1) is the old
// rule: effect shaders and cables only.
// lane GLTFCELL1: FO4 applies it on every coloured shape, SLSF1 Vertex_Alpha set or not (upstream renderer.cpp drops
// it only below NIF version 130). BldChurch01Open's window panes (no flag) carry #ffffff19: texture alpha 80 x 0.1 is
// under the material's alpha ref 10, so the game cuts them. `flagRed` (WW_CELL_VERTEXALPHA_RED=flag) needs the flag.
inline bool wwCellKeepsVertexAlpha( bool ownColor, bool effect, bool cableSway, bool vertexAlphaFlag, bool treeAnim,
	bool red, bool flagRed = false )
{
	if ( !ownColor )
		return false;
	if ( effect || cableSway )
		return true;
	return !red && !treeAnim && ( vertexAlphaFlag || !flagRed );
}
