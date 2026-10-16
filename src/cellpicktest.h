/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLPICKTEST_H
#define CELLPICKTEST_H

#include <QString>

class NifModel;
class QWidget;

/* WW_CELLPICK_TEST=<report path>: in-window self-test of the click path. The pick must equal an independently
 * derived "nearest, ties to the smaller box" answer, the dock must show that entry's rows, and a miss and the
 * master switch off must change nothing. Returns the number of failed rows (0 = pass). */
int cellPickSelfTest( NifModel * nif, QWidget * panel, const QString & reportPath );

#endif // CELLPICKTEST_H
