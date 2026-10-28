#ifndef CELLVOLUMEVIEW_H
#define CELLVOLUMEVIEW_H

/* lane VOLFIT1: the Space stage's debug view (src/probevolumes.h). The cell view draws, over the frame and
 * depth-tested, each room's shapes as wireframes (neighbors read apart in grayscale too, by the dash of their edges)
 * and each threshold as a translucent quad (doors amber, windows cyan, portals violet, plain openings white) with its
 * normal as a short line. WW_CELL_VOLUMES_VIEW_DUMP=<file> writes what is drawn.
 *
 * lane VOLVIEW2: it is part of the PRTP Debug View (the PRTP band's tick, src/cellworkspace.cpp); WW_CELL_VOLUMES_VIEW=1
 * or 0 pins it on or off (the harness). A room takes its GROUP's color (the building: the rooms its thresholds join,
 * src/probevolumes.h probeVolumesGroups), the same in every cell; each threshold's outline is drawn twice, a hair to
 * each side, in its two rooms' colors (the outdoors white); room and threshold labels are painted by GLView. Each
 * probe gets three rings in its room's color and a line to every probe it works with: the probes whose GI grid reads
 * share a voxel slot by the shader's rule (cellGiRoomBlend in res/shaders/cell_lights.glsl: the room match, the seen
 * filter, both slots across a threshold), solid in the room's color, dashed in both rooms' colors across a threshold. */

#include "probevolumes.h"
#include "data/niftypes.h"

#include <QString>

#include <vector>

class Scene;
class Transform;
struct WwCellGi;

struct WwCellVolumeGeometry
{
	std::vector<float> lines;	// 3 floats a point, 2 points a line
	std::vector<float> lineColor;	// 4 floats a point
	std::vector<float> tris;	// 3 floats a point, 3 points a triangle
	std::vector<float> triColor;	// 4 floats a point
	int rooms = 0, shapes = 0, quads = 0;
};

//! lane VOLVIEW2: a label GLView paints at a world point
struct WwCellVolumeLabel
{
	Vector3 at;
	QString text;
	float rgb[3] = { 1, 1, 1 };
	bool isThreshold = false;	// a threshold's (a smaller type)
};

/*! lane VOLVIEW2: which probes work together. Per probe (gi_probes.bin order) its GI room label pair as the shader
 *  reads it for an up-facing surface at the probe (-1: solid), the voxels the seen filter dropped, and its links
 *  (to[start[p] .. start[p + 1]), thr = 1 when the other probe's room differs: the link runs through a threshold). */
struct WwCellProbeLinks
{
	bool ok = false;
	QString why;	// when not ok
	std::vector<int> room0, room1, dropped, start, to;
	std::vector<char> thr;
};
//! the links (no GL; red = wwCellPassRed(): nosee 16 drops the seen filter, firstslot 256 one slot, noclamp 8 none)
WwCellProbeLinks wwCellProbeLinksBuild( const WwCellGi & gi, const std::vector<float> & probes, int red );

//! The view's geometry (no GL): the rooms and thresholds wwCellVolumeViewDraw draws and the CLI's --view-dump counts.
WwCellVolumeGeometry wwCellVolumeGeometry( const ProbeVolumes & v );
//! A color from a name (a group, a room "<cell>:0x<id>", "gi:<label>"): stable across bakes, rgb 0..1.
void wwCellVolumeColor( const QString & key, float rgb[3] );

//! drivesGi: the GI's room labels are these rooms (WW_CELL_VOLUMES_GI=1); rooms without a group get theirs here
void wwCellVolumeViewPublish( const void * nif, const ProbeVolumes & v, bool drivesGi );
//! the view is on: the PRTP Debug View tick, or the WW_CELL_VOLUMES_VIEW pin
bool wwCellVolumeViewOn( const void * nif );
//! the overlay, after the scene draw (a no-op when off or without a published view)
void wwCellVolumeViewDraw( Scene * scene, const Transform & viewTrans );
//! the last frame's labels near the camera (empty when off), for GLView's painter
std::vector<WwCellVolumeLabel> wwCellVolumeViewLabels( const void * nif );

#endif // CELLVOLUMEVIEW_H
