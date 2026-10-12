/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

#ifndef CELLMODPICKBOX_H
#define CELLMODPICKBOX_H

#include "cellmodpick.h"

#include <QPoint>
#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/* lane MODPICK1 -- the Cell workspace's Mods band (cellmodpick.h holds the list, the saved choice and the
 * rules; this is only the widget). A checkable list of plugins in load order, each with the mod folder it
 * comes from; a file gone from disk is greyed with a note and cannot be ticked; Fallout4.esm and the DLC
 * masters stay at the top and always load.
 *
 * Reordering follows MO2's plugin pane: drag one or several selected rows, a line shows where they land; a
 * plugin cannot go above one of its own masters (the status bar says why). Files and folders dragged in from
 * Explorer are added at the drop point ("added: <folder>"), refused when a master is missing; Delete or the
 * right-click Remove takes an added row out again. Nothing is written into the dropped folders.
 *
 * Apply saves the ticks, the order and the additions for the worldspace and rebuilds the cell; Reset forgets
 * them all (the source order, no additions). Blender's add-on list is the reference for the ticks; it applies
 * a tick at once, this waits for Apply because every apply re-opens the whole cell. */
class CellModPickBox final : public QWidget
{
public:
	explicit CellModPickBox( QWidget * parent = nullptr );

	//! Re-read the list for `ws` (empty: no exterior cell open -- the band says so).
	void refresh( const QString & ws );
	//! Called after Apply or Reset changed the saved choice: the owner rebuilds the cell.
	std::function<void()> onChoiceChanged;

	// ---- the harness's readers and clicks (the same paths the mouse takes)
	int rowCount() const;
	QString rowFile( int i ) const;
	QString rowMod( int i ) const;
	bool rowOn( int i ) const;
	bool rowPresent( int i ) const;
	bool rowLocked( int i ) const;
	bool rowAdded( int i ) const;
	int rowOf( const QString & file ) const;
	void setRowOn( int i, bool on );
	QString sourceText() const;
	QString worldspace() const { return ws; }
	bool applyEnabled() const;
	void apply();
	void reset();
	//! the last refusal or drop note (also on the status bar)
	QString lastNote() const { return note; }
	QTreeWidget * tree() const { return list; }
	//! a viewport point that drops before row `i` (i == rowCount(): after the last row)
	QPoint dropPointBefore( int i ) const;
	void selectRows( const QStringList & files );
	//! remove the selected hand-added rows (Delete, the right-click Remove)
	void removeSelectedAdded();

	// ---- the tree's drop handlers
	void moveSelectedTo( int before );
	void dropPathsAt( const QStringList & paths, int before );

private:
	void fill();
	void syncTicks();
	void updateButtons();
	void say( const QString & text );
	int firstFree() const;
	CellModChoice choiceNow() const;

	QString ws;
	CellModList listNow;           //!< the list as read at refresh (source, label, natural order)
	QVector<CellModRow> rows;      //!< the rows as they stand (order, ticks, additions)
	QStringList keysAdded;         //!< keys added since refresh, in the order they came
	CellModChoice atRefresh;       //!< the choice the list opened with (Apply is live once it changes)
	QString note;
	QLabel * source = nullptr;
	QTreeWidget * list = nullptr;
	QLabel * hint = nullptr;
	QPushButton * applyButton = nullptr;
	QPushButton * resetButton = nullptr;
	QLabel * count = nullptr;
	bool filling = false;
};

#endif // CELLMODPICKBOX_H
