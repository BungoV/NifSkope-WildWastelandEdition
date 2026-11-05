/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WATERMARKPANEL_H
#define WATERMARKPANEL_H

class QMainWindow;

/*! @file watermarkpanel.h  The Water tab of the LOD Generation workspace.
 *
 *  ONE ENTRY POINT, on purpose. Everything this feature adds -- the page, its
 *  canvas, its rows, its tab in the left editor's strip and its self-test --
 *  is built inside watermarkpanel.cpp, so the touch in `NifSkope`'s constructor
 *  is this one call. (Lane BUILD4 was compiling `src/nifskope_ui.cpp`, where
 *  every other dock is created, while this was written; the file rule in the
 *  brief is what shaped the seam, and it turned out to be the better seam.)
 *
 *  LANE WATER7, on bungo's ruling of 2026-09-10 (*"They should be in the LOD
 *  gen workspace"*, then *"You'd access them like this"* over the
 *  Header | Blocks | Files strip): there is no `Water Marking` dock any more,
 *  and no entry for it -- or for the water window -- in the Workspaces menu.
 *  The rows are a fourth page of `LeftColumnStack`, reached by a fourth tab of
 *  `LeftColumnModeSelector`, shown while `LodGenerationDock` is visible.
 *
 *  With `WW_WATER_MARK_TEST=1` in the environment the panel runs the house-style
 *  self-test after the first document finishes loading and writes
 *  `release/ww_water_mark_test.log`; `WW_WATER_MARK_SHOT=<png>` also grabs the
 *  left dock. `WW_WATER_MARK_FILE=<path.lodl>` names the file it opens.
 */
void waterMarkInstall( QMainWindow * mw );

#endif // WATERMARKPANEL_H
