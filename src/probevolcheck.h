#ifndef PROBEVOLCHECK_H
#define PROBEVOLCHECK_H
/*! lane VOLRAY1 (2026-10-27): `volcheck`, an independent check of the Space stage's room shapes against the soup's
 *  real triangles (not the fitter's voxel grid). A jittered lattice over the rooms is joined by short rays (an edge
 *  is open when no triangle cuts it); each lattice point casts K rays for its sky share, openness and back-face share.
 *  Each room floods its air from its anchor (stopping at thresholds, outdoor air and other rooms) and the report
 *  counts per room and per cell: gaps (enclosed air no shape covers), wall pokes (shape volume on the far side of
 *  geometry, by a ray back to the room's own air), cross-wall overlaps, spills through openings, leaks (shape volume
 *  in outdoor air) and missing thresholds. Rays go through bakert::Scene::cast (RT cores) or the CPU walk on every
 *  core (the reference); `--rays both` runs both and compares every answer. */
#include <QStringList>

int probeVolCheckCli( const QStringList & args );

#endif // PROBEVOLCHECK_H
