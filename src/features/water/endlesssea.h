#ifndef ENDLESSSEA_H
#define ENDLESSSEA_H

#include <QString>

class NifModel;
class QModelIndex;
class Scene;
class Transform;
struct WwWaterRecord;

/*! Lane FARSEA1 (2026-11-09): THE ENDLESS SEA TO THE HORIZON.
 *
 *  Past the baked map (the .lodl's box) the far water stopped and the void showed. One more water shape now hangs
 *  under the far LOD node: a camera-centred disc of rings, fine near (256 u) and coarser by a constant ratio out to
 *  the far sheet's rim (WW_ENDLESS_SEA_FAR, 4194304 u = 59.9 km), at the worldspace's sea level (the .lodl's largest
 *  sea body, else the WRLD default water height). It follows the camera in whole steps of 2048 u (no swim: the
 *  waves are world-anchored in the shader). Rings of one segment count (128) share every edge with the next ring,
 *  so there are no T-junctions and no cracks, flat, curved or tessellated (fo4_waterdisp.tesc).
 *  Prior art: Tessendorf 2001 (world-space wave field), Johanson 2004 (projected grid), Losasso & Hoppe 2004
 *  (camera-centred nested levels snapped to a grid), Crest (MIT, wave-anchored LOD rings + snapping), Unreal Water
 *  (a far-distance mesh around the water zone). notes/farsea1/RESEARCH.md.
 *
 *  It is drawn by the same water programs with the same inputs as the baked ocean (wwCellWaterShape with the
 *  worldspace's default WATR). The baked water stays in charge: the sheet discards every pixel inside the bake's
 *  box (water_info.glsl waterSeamBox; the baked water and the terrain own it), and outside it the water lookup
 *  answers the open sea (the sea body, no shore, deep: depth code 255). The seam: over a band at the box edge
 *  (WW_ENDLESS_SEA_SEAM, 98304 u = 1.4 km) the baked sea's shore distance and depth fade to those open-sea values
 *  and its flow to 0, so no line shows where the bake stops.
 *
 *    WW_ENDLESS_SEA=0|1            the Endless sea row's pin (Settings: Water), ships ON, live
 *    WW_ENDLESS_SEA_RED=off        the old picture: the sheet is not drawn and the baked water keeps its edge (live)
 *    WW_ENDLESS_SEA_SEAM=<u>       the seam band (live; 0 = no fade)
 *    WW_ENDLESS_SEA_FAR=<u>        the far rim (built with the cell)
 *    WW_ENDLESS_SEA_CURVE=1        the earth's curvature: the rim drops d^2 / 2R (R = 6371 km; built with the cell) */

//! Appends the sheet under `parent` and registers it; returns its census line (rings, rim, seam band, coverage).
//! lane STREAMGN1: `drawnCells` (x0, y0, x1, y1, cells, inclusive) = the far water rings' footprint; the sheet starts
//! at its edge, not at the bake box's (past the outer ring the box held no water: a land-coloured wedge).
//! Red WW_ENDLESS_SEA_RED=bakebox: the old inner edge.
QString wwEndlessSeaAppend( NifModel * nif, const QModelIndex & parent, const WwWaterRecord & rec, float defaultZ,
	const QString & lodlPath, const int * drawnCells = nullptr );
//! The Endless sea row and its red.
bool wwEndlessSeaOn();
//! The seam band, world units (0 = no fade).
float wwEndlessSeaSeam();
//! Once a frame before the scene's transform: the sheet moves to the camera's 2048 u step.
void wwEndlessSeaFollow( Scene * scene, const Transform & view );
//! MODULES1 10b-6: round the sheet's draw (bsshape.cpp): the triangles sent and the pixels that passed the depth
//! test, said as `endless sea draw:` when they change. A no-op for every other shape.
void wwEndlessSeaDrawBegin( Scene * scene, int block, long long triangles );
void wwEndlessSeaDrawEnd();
//! The rim's distance from the camera while the sheet draws, else 0 (the far plane reaches it).
float wwEndlessSeaReach( Scene * scene );

#endif
