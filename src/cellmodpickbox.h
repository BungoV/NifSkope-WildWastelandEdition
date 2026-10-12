/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

#ifndef CELLMODPICKBOX_H
#define CELLMODPICKBOX_H

#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/* lane MODPICK1 -- the Cell workspace's Mods band (cellmodpick.h holds the list, the saved choice and the
 * rules; this is only the widget). A checkable list of plugins in load order, each with the mod folder it
 * comes from; a file gone from disk is greyed with a note and cannot be ticked; the worldspace's master
 * (the first row) always loads. Apply saves the ticks for the worldspace and rebuilds the cell; Reset forgets
 * them. Blender's add-on list is the reference (a tick per row, missing ones greyed); it applies a tick at
 * once, this waits for Apply because every apply re-opens the whole cell. */
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
	void setRowOn( int i, bool on );
	QString sourceText() const;
	QString worldspace() const { return ws; }
	bool applyEnabled() const;
	void apply();
	void reset();

private:
	void updateButtons();
	QStringList offNow() const;

	QString ws;
	QStringList offAtRefresh;      //!< the unticked files the list opened with (Apply is live once they change)
	QLabel * source = nullptr;
	QTreeWidget * list = nullptr;
	QPushButton * applyButton = nullptr;
	QPushButton * resetButton = nullptr;
	QLabel * count = nullptr;
	bool filling = false;
};

#endif // CELLMODPICKBOX_H
