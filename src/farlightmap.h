#ifndef FARLIGHTMAP_H
#define FARLIGHTMAP_H

/*! THE WHOLE-MAP FAR LIGHT (lane FARLIGHTMAP1, 2026-10-22; RDR2's distant lights, bungo's ruling option a).
 *
 *  `NifSkope -no-gui farlightmap ...` walks a worldspace's exterior cells and gives each one the cell view's OWN
 *  probe GI bake: the same exe, opened on that cell exactly as tools/ww_bake_world.py opens it (WW_CELL_OPEN, the
 *  5x5 block + far soup, WW_CELL_PROBE_BAKE), with WW_CELL_FARLIGHT set, so the relight records its operators and
 *  FARVIEW1's writer (src/farlight.h) writes the cell's far-light layers. Nothing of the bake is forked: a cell's
 *  files are the cell view's files for that cell, byte for byte.
 *
 *  One launch, many cells: a walk process loads the plugins once and opens cell after cell in its window, in a
 *  serpentine over its strip of rows, keeping the worldspace index and the asset bytes between neighbouring cells.
 *  It is respawned only past --memcap or after a failed cell; --jobs N runs N strips. The driver opens no window.
 *
 *  FOLDERS
 *    <root>/cell_X_Y/          the cell's .tbk + bakeincr.txt (BAKEINCR1's record, with a `lights` line)
 *    <root>/cell_X_Y/far/      the child's far-light output (the stage)
 *    <root>/cell_X_Y/farlight.done   the done marker: exe sha1, far-light knobs, sha1 of each file copied out
 *    <out>/                    the far-light set FARVIEW1 reads (farLightLoad): lights_B.wlt, relight_B.wlp,
 *                              farlight_B.fvl, farlight_B.fvg, B = the cell's sector name (+XXXX_+YYYY)
 *    <root>/_runs/cell_X_Y/    the child's notes; <root>/farlightmap.log the census
 *
 *  RESUME: a cell is skipped when BAKEINCR1's check calls it clean (exe sha1, knobs, records, assets, far, and the
 *  lights) AND its done marker matches this exe and the files in <out>. A cell whose 5x5 block holds no placed
 *  light has no far light (the relight's lights are the block's); it is marked done without a bake. */

#include <QStringList>

//! the `farlightmap` CLI command; returns the process exit code
int farLightMapCli( const QStringList & args );

/*! the walk's step, in the window a walk process opened (nifskope.cpp, after each open of a .wwcell): FarLightWalkOff
 *  when no walk was asked (WW_CELL_FARLIGHTWALK unset), FarLightWalkNext when the next cell's env is set and the same
 *  file is to be opened again, else the exit code to quit with (0 done or a memcap respawn, 3 a cell failed) */
enum { FarLightWalkOff = -1, FarLightWalkNext = -2 };
int farLightWalkStep( const QString & fname, bool loaded );

#endif // FARLIGHTMAP_H
