#ifndef CELLPROBEVIEW_H
#define CELLPROBEVIEW_H

/* lane PROBEVIEW1: the PRTP band's Pass previews that are not a shader (Division deck, s18 / s40 / s65).
 *
 *  Surfel color / Surfel light   every surfel of the bake as a splat (0.6 of a surfel cell) on its own surface, colored by its albedo
 *                                (sRGB) or by its outgoing light B / pi (the GI pass's curve); the frame under
 *                                them is magenta, so a surface no surfel covers reads as a hole.
 *  probes                        with Show probes on, in any Pass but Combined, each probe as a box whose six
 *                                faces carry its own value on that axis (GI: E / pi, Sky visibility: the share).
 *  links                         a probe picked by a click (or the pin WW_CELL_PV_PROBE=<index>, the bake's
 *                                probe order) draws a line to every surfel it links.
 *
 *  All of it lives in the scene's world (the cell's game units) and is drawn over the frame, depth-tested.
 *  WW_CELL_PV_DUMP=<file> writes the drawn state (pass, tiles, boxes, picked probe, its links and their ends)
 *  for the gate (tests/spells/cell_pass.sh); WW_CELL_PV_ID=1 colors each splat by its surfel's index + 1 (24 bits). */

#include <QString>

#include <vector>

class Scene;
class Transform;

struct WwCellProbeView
{
	float surfelCell = 32.0f;	// the bake's surfel cell size (a tile's side)
	bool probesShown = true;	// the PRTP band's Show probes
	std::vector<float> surfels;	// per surfel: pos[3] nrm[3] albedo[3] (linear) B[3]
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
