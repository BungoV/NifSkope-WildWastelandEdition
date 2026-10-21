#ifndef CELLVOLUMEVIEW_H
#define CELLVOLUMEVIEW_H

/* lane VOLFIT1: the Space stage's debug view (src/probevolumes.h). With WW_CELL_VOLUMES_VIEW=1 the cell view
 * draws, over the frame and depth-tested, each room's shapes as wireframes in the room's own color (a hash of its
 * cell and id: neighbors read apart in grayscale too, by the dash of their edges) and each threshold as a
 * translucent quad (doors amber, windows cyan, portals violet, plain openings white) with its normal as a short
 * line. WW_CELL_VOLUMES_VIEW_DUMP=<file> writes what is drawn (rooms, shapes, threshold quads, line count). */

#include "probevolumes.h"

#include <QString>

#include <vector>

class Scene;
class Transform;

struct WwCellVolumeGeometry
{
	std::vector<float> lines;	// 3 floats a point, 2 points a line
	std::vector<float> lineColor;	// 4 floats a point
	std::vector<float> tris;	// 3 floats a point, 3 points a triangle
	std::vector<float> triColor;	// 4 floats a point
	int rooms = 0, shapes = 0, quads = 0;
};

//! The view's geometry (no GL): what wwCellVolumeViewDraw draws and the CLI's --view-dump counts.
WwCellVolumeGeometry wwCellVolumeGeometry( const ProbeVolumes & v );
//! The room's color: from its cell and id (stable across bakes), rgb 0..1.
void wwCellVolumeColor( const QString & cellKey, quint32 id, float rgb[3] );

void wwCellVolumeViewPublish( const void * nif, const ProbeVolumes & v );
//! the overlay, after the scene draw (a no-op without WW_CELL_VOLUMES_VIEW=1 or a published view)
void wwCellVolumeViewDraw( Scene * scene, const Transform & viewTrans );

#endif // CELLVOLUMEVIEW_H
