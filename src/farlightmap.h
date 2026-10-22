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
 *  One child process a cell: its memory is the OS's again when it exits. The driver itself opens no window.
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

#endif // FARLIGHTMAP_H
