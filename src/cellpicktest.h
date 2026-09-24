/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLPICKTEST_H
#define CELLPICKTEST_H

#include <QString>

class NifModel;
class QWidget;

/* ---------------------------------------------------------------------------
 * THE PICK WIRING'S SELF-TEST (`WW_CELLPICK_TEST=<report path>`).
 *
 * The click path is the one part of this lane that a file-and-census gate
 * cannot reach: it starts in `GLView::mouseReleaseEvent`, runs through a ray
 * test, moves a shape in the document and ends in a dock.  So it is measured
 * the way every other panel in this fork is measured -- inside the running
 * window, with the answers written to a file a shell gate greps
 * (`tests/spells/cell_pick.sh`), never by looking at a screenshot.
 *
 * WHAT IT REFUSES TO ACCEPT AS A PASS
 *
 *   - A ray that hits SOMETHING.  The rule this table documents is "nearest,
 *     and ties to the SMALLER box" (src/cellpick.h); the test re-derives that
 *     answer here, from the same dump-able fields, and requires the pick to
 *     equal it.  A pick that merely lands inside some box would pass against a
 *     table that returned its first entry every time.
 *   - A dock that shows rows.  The rows must be the rows OF THE PICKED ENTRY:
 *     the first one carries the reference's own form id.  A panel wired to the
 *     wrong index still fills with plausible text.
 *   - THE MISS, and THE MASTER OFF.  Two red controls in the same run: a ray
 *     pointed away from the scene must report no pick, say so in the summary
 *     line and collapse the highlight; and with the master unticked the click
 *     must return false and change nothing, which is the promise the menu row
 *     makes to everybody who never ticks it.
 *
 * Returns the number of FAILED rows; 0 means every row passed.
 * --------------------------------------------------------------------------- */
int cellPickSelfTest( NifModel * nif, QWidget * panel, const QString & reportPath );

#endif // CELLPICKTEST_H
