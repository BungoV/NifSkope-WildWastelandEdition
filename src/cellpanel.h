/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLPANEL_H
#define CELLPANEL_H

#include <QWidget>

class QLabel;
class QTreeWidget;

/* ---------------------------------------------------------------------------
 * THE CELL PICK PANEL -- what the reference under the cursor is.
 *
 * A FLAT Name | Value tree and nothing else (the house style,
 * `nifskope-ww-panel-style`, and bungo's standing rule for this fork's panels):
 * no descriptions, no blurbs, no controls.  The rows are exactly
 * `CellPickTable::rowsFor()` in the order that function returns them, so the
 * panel cannot disagree with the harness -- the gate reads the same rows out of
 * the same function without a window.
 *
 * The line under the tree is the SUMMARY OR THE REFUSAL, which is the one thing
 * a panel of this shape must always have: how many boxes the ray entered, or
 * the single reason there is nothing to show.  A pick is by BOX and not by
 * triangle (src/cellpick.h), so "3 under the cursor" is a fact a user needs and
 * a silent single row would be a claim this code cannot make.
 * --------------------------------------------------------------------------- */

class CellPickPanel : public QWidget
{
	Q_OBJECT
public:
	explicit CellPickPanel( QWidget * parent = nullptr );

public slots:
	//! `index` into `cellPickTable()`, -1 for a miss; `candidates` = boxes entered.
	void showPick( int index, int candidates );
	//! A new document is open: drop the rows and say so.
	void onSceneChanged();

private:
	void setRefusal( const QString & why );

	QTreeWidget * rows = nullptr;
	QLabel * summary = nullptr;
};

#endif // CELLPANEL_H
