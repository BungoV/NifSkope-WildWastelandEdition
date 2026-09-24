/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLWORKSPACE_H
#define CELLWORKSPACE_H

#include "cellview.h"

#include <QStringList>
#include <QVector>
#include <QVector>
#include <QWidget>

class GLView;
class NifModel;
class NifSkope;
class QAction;
class QToolButton;
class QComboBox;
class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

/* ---------------------------------------------------------------------------
 * THE CELL WORKSPACE (lane CELLWORK1, 2026-09-19).
 *
 * bungo, verbatim: "Cell viewing will be a new workspace btw".
 *
 * ===========================================================================
 * WHAT A WORKSPACE IS IN THIS FORK, AND WHY THIS FILE IS SMALL
 * ===========================================================================
 *
 * The mechanism already exists and this lane does not add a second one.  A
 * workspace is an entry in `workspaceNames` and a manager QDockWidget in
 * `managers`, both in NifSkope::initToolBars() (src/nifskope_ui.cpp), switched
 * by NifSkope::activateWorkspace().  Cell is APPENDED LAST, exactly as every
 * workspace before it was, so every stored `UI/Workspace` index keeps pointing
 * at the workspace it pointed at before.
 *
 * What this lane DID have to add to the mechanism is the one thing it could not
 * do: a workspace could show its own dock, but never hide anything else.  The
 * Cell workspace hides the NIF-only editor (`LeftColumnDock` -- block list,
 * block details and header are three PAGES of one dock since the left-column
 * rework) and puts it back untouched on the way out.  That is one field on the
 * workspace table, not a parallel system; see `NifSkope::WorkspaceDef`.
 *
 * ===========================================================================
 * THE PANEL
 * ===========================================================================
 *
 * The reference LIST is Blender's outliner: one flat row per thing that was
 * drawn, filterable by record type and by text, click selects and frames it,
 * and a viewport pick puts the cursor on its row.  Its rows are
 * `cellPickTable()` entries, ONE ROW PER ENTRY, which is the same unit the
 * `WW_CELL_DUMP` file writes a line for -- so "the list shows what the dump
 * counts" is an identity and not a coincidence, and the gate can assert it.
 * (A SCOL that expanded into eleven parts is therefore eleven rows, because
 * eleven boxes are separately pickable.  Blender's outliner would nest those
 * under one parent; this build has no tree column and no nesting anywhere else
 * in the panel, so the divergence is listed in the lane report rather than
 * invented here.)
 *
 * THE LEGEND AND THE CENSUS ARE NOT COMPUTED HERE.  Both are parsed out of the
 * builder's own `notes` string, which is the string `qInfo()` prints and the
 * string tests/spells/cell_legend_colour.py measures against the rendered
 * picture.  A panel that recomputed either would be the second code path that
 * lane CELLVIEW3 already caught printing mauve for a grey bucket.  There is one
 * producer; this reads it.
 *
 * THE VIEW ROWS REBUILD.  An overlay is baked into vertex colours while the
 * scene is welded, so changing one is a rebuild and not a repaint.  A row
 * therefore records its choice in the override block below and asks the window
 * to re-open the same `.wwcell` through the ordinary open path -- the proven
 * path, not a second builder.  The cost is real and the summary line states it
 * in milliseconds; making an overlay switch instant would mean re-running the
 * colour pass over built geometry, which is a change to the builder and is
 * listed as a ruling owed rather than shipped.
 * --------------------------------------------------------------------------- */

/*! The rows' pending choices, applied to the spec the open path just read from
 *  the `.wwcell`.  Called from src/nifskope.cpp between `cellSpecFromFile` and
 *  `nifCreateCellScene`, so the file and the rows are never two sources of
 *  truth: the file is the default, a row that has been touched overrides it,
 *  and nothing overrides anything until a row is touched. */
void cellWorkspaceApplyOverrides( CellSceneSpec & spec );

/*! The open path's read-back: the cell that is now open, the spec it was built
 *  from, and the builder's own notes.  The panel's census, legend and refusal
 *  line all come out of `notes` and out of nothing else. */
void cellWorkspaceNoteOpened( const QString & path, const CellSceneSpec & spec,
	const QString & notes );

//! The `.wwcell` that is open, or empty. The view rows re-open this path.
QString cellWorkspacePath();

//! True once a view row has been touched this session (the overrides are live).
bool cellWorkspaceHasOverrides();

//! Forget the overrides and the open cell (a NIF was opened; the cell is gone).
void cellWorkspaceForget();

class CellWorkspacePanel final : public QWidget
{
	Q_OBJECT

public:
	explicit CellWorkspacePanel( QWidget * parent = nullptr );

	void setNif( NifModel * nif );
	void setGLView( GLView * view );

	// ---- readers for the harness (object names cover the widgets)
	//! Rows the list holds, before the filter hides any.
	int totalRows() const;
	//! Rows the filter leaves showing.
	int visibleRows() const;
	//! `cellPickTable()` index of the selected row, or -1.
	int selectedEntry() const;
	//! The pinned one-line census, or empty when no cell is open.
	QString censusText() const;
	//! The summary line, or the refusal.
	QString noteText() const;
	bool noteIsRefusal() const;
	//! Legend rows shown (one per bucket the builder printed).
	int legendRows() const;
	//! The record types the type filter offers, "All" first.
	QStringList typeFilterNames() const;
	//! The Show popover's rows, in menu order -- label and state, for the gate.
	int showRowCount() const;
	QString showRowLabel( int i ) const;
	bool showRowChecked( int i ) const;
	//! Flip a Show row by index, exactly as clicking it would.
	void setShowRow( int i, bool on );

public slots:
	//! A cell scene was built, or the document changed: rebuild everything.
	void onSceneChanged();
	//! A viewport pick: put the cursor on that row without re-driving the pick.
	void onPicked( int index, int candidates );
	//! "" or "All" shows everything; otherwise a record type as `CellPickTable::typeName`.
	void setTypeFilter( const QString & type );
	//! Matches editor id, form id and model path, case-insensitively.
	void setTextFilter( const QString & text );
	/*! What a row click does, and what the harness calls: select the entry the
	 *  same way a viewport pick on it would (one function, `cellPickSelect`),
	 *  and frame it when `frame`. */
	void selectEntry( int index, bool frame );

signals:
	//! A view row changed: re-open this `.wwcell` through the ordinary path.
	void reopenRequested( const QString & path );

private slots:
	void rowClicked();
	void viewRowChanged();
	void filterChanged();

private:
	void buildUi();
	void rebuildList();
	void applyFilter();
	void rebuildLegendAndCensus();
	void say( const QString & text, bool refusal );
	void frameEntry( int index );

	NifModel * nif = nullptr;
	GLView * glView = nullptr;
	bool syncing = false;        //!< a selection we caused; do not echo it back

	QLineEdit * filterText = nullptr;
	QComboBox * filterType = nullptr;
	QTreeWidget * list = nullptr;
	QComboBox * overlayBox = nullptr;
	/*! ONE HOME FOR THE CELL'S VIEW TOGGLES (bungo 2026-09-19: "a lot of visual
	 *  / visibility toggles that will only live in the cell editor workspace,
	 *  that are specific to cell stuff"). Blender's viewport Overlays popover
	 *  and the Creation Kit's View menu are the references: label + control, no
	 *  descriptions. The actions are built from `cellShowRows()` in order, so a
	 *  later lane adds a toggle by adding a ROW, not by adding a widget, a
	 *  member, a connect and a settings key in four places. */
	QVector<QAction *> showActions;
	/*! THE JOIN, kept where the list was built: row i of the reference model
	 *  draws as pick entry `pickForRef[i]`, or -1 when nothing drew it. One
	 *  place, so the mouse and a row click cannot disagree about which
	 *  reference is which. */
	QVector<int> pickForRef;
	QTreeWidget * legend = nullptr;
	QLabel * census = nullptr;
	QLabel * note = nullptr;
	bool noteRefusal = false;
};

/*! WW_CELLWS_TEST=<report path>: lane CELLWORK1's gate, run inside the real
 *  application (src/cellworkspacetest.cpp). Does nothing unless it is set. */
void wwCellWorkspaceHarness( NifSkope * skope );

#endif // CELLWORKSPACE_H
