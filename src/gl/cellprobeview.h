#ifndef CELLPROBEVIEW_H
#define CELLPROBEVIEW_H

/* Overlays for the PRTP Pass previews that are not a shader: surfel splats (albedo or outgoing light, over a cyan
 *  frame so holes show), probe boxes, and a picked probe's surfel links. WW_CELL_PV_PROBE=<index>,
 *  WW_CELL_PV_DUMP=<file> and WW_CELL_PV_ID=1 (splats colored by surfel index + 1) are debug aids. */

#include <QString>

#include <vector>

class Scene;
class Transform;

struct WwCellProbeView
{
	float surfelCell = 32.0f;	// the bake's surfel cell size (a tile's side)
	bool probesShown = true;	// the PRTP band's Show probes
	std::vector<float> surfels;	// per surfel: pos[3] nrm[3] albedo[3] (linear) B[3]
	std::vector<float> surfelBrick;	// per surfel: its brick's light (rgb, the mean of pass 1's B)
	std::vector<float> probes;	// per probe: pos[3] + E on +X -X +Y -Y +Z -Z (rgb each)
	std::vector<float> probeSky;	// per probe: the sky share on the six axes
	std::vector<int> linkStart;	// probe i links surfels links[linkStart[i] .. linkStart[i + 1])
	std::vector<int> links;
};

void wwCellProbeViewPublish( const void * nif, const WwCellProbeView & v );
const WwCellProbeView * wwCellProbeViewFor( const void * nif );
//! a click in a Pass: picks the probe nearest the ray (within its box); false (nothing changed) otherwise
bool wwCellProbeViewPick( Scene * scene, const float origin[3], const float dir[3] );
//! the overlay, after the scene draw (a no-op in Combined or without a published view)
void wwCellProbeViewDraw( Scene * scene, const Transform & viewTrans );

#endif // CELLPROBEVIEW_H
