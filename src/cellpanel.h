/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLPANEL_H
#define CELLPANEL_H

#include <QWidget>

class QLabel;
class QTreeWidget;

/* The cell pick panel: a flat Name | Value tree of `CellPickTable::rowsFor()` for the reference under the
 * cursor, with a pinned line under it giving how many boxes the ray entered or why nothing is shown. */

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
