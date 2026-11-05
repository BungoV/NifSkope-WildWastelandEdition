/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WATERWINDOW_H
#define WATERWINDOW_H

#include <QString>

class QMainWindow;
class QWidget;

/*! @file waterwindow.h  The water window (lane WATER5): a separate top-level
 *  window -- draggable, resizable, full-screen on F11 -- showing the
 *  worldspace's water map with every tool the marking needs: the curves,
 *  the pins, the body rows, Solve, Reload / Save, the json save / load and
 *  the PNG export / import.
 *
 *  bungo, 2026-09-10, verbatim: *"just make it open a new popup window that
 *  can be set to full screen and you can drag that shows the flowmap"*; *"Add
 *  all the tools needed to mark the rivers and solve it and export import
 *  there, into that new window."*
 *
 *  TWO ENTRY POINTS, on purpose: the dock (watermarkpanel.cpp) keeps one
 *  button that calls waterWindowOpen(), and waterMarkInstall() calls
 *  waterWindowInstall() once so the self-test can run without a click.  Every
 *  other thing this feature adds lives in waterwindow.cpp and watercurves.cpp.
 *
 *  With `WW_WATER_WINDOW_TEST=1` the window runs its self-test after the main
 *  window's first document has loaded and writes
 *  `release/ww_water_window_test.log`; `WW_WATER_WINDOW_FILE=<path.lodl>`
 *  names the land file (its json is written beside it, so give it a COPY);
 *  `WW_WATER_WINDOW_SHOT=<dir>` also grabs the two pictures into that
 *  directory (ABSOLUTE path: a relative one saves nowhere, see the render-shot
 *  skill).  A headless run (any `WW_*` variable set) shows the window at
 *  opacity 0 on a non-primary screen, as every other top level does.
 */

//! Open the window (one per application) on `lodlPath`, or raise it. Returns it.
QWidget * waterWindowOpen( QMainWindow * mw, const QString & lodlPath );

//! Register the Workspaces entry and the self-test trigger. Call once.
void waterWindowInstall( QMainWindow * mw );

#endif // WATERWINDOW_H
