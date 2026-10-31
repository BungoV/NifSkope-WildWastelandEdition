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
class CellModPickBox;
class NifModel;
class NifSkope;
class QAction;
class QActionGroup;
class QMenu;
class QCheckBox;
class QPushButton;
class QToolButton;
class WwDropdown;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QTabBar;
class QTreeWidget;
class QTreeWidgetItem;
class OpenCellPicker;

/* The Cell workspace panel: a reference list (one row per cellPickTable() entry, filterable,
 * click selects and frames), plus the legend and census parsed from the builder's own notes.
 * View rows change baked vertex colors, so they re-open the .wwcell rather than repaint. */

/*! Apply the rows' pending choices to the spec just read from the `.wwcell` (between cellSpecFromFile
 *  and nifCreateCellScene). The file is the default; only touched rows override it. */
void cellWorkspaceApplyOverrides( CellSceneSpec & spec );

/*! The open path's read-back: the open cell, its spec, and the builder's notes (the panel's only source). */
void cellWorkspaceNoteOpened( const QString & path, const CellSceneSpec & spec,
	const QString & notes );

//! The `.wwcell` that is open, or empty. The view rows re-open this path.
QString cellWorkspacePath();

//! lane FAVCAM1: the spec of the cell that is open; false when none is
bool cellWorkspaceOpenSpec( CellSceneSpec & out );

//! lane DUNESAND3: after a live set, rewrite the notes' ground TX07 clause to the switch's value now; empty if unchanged
QString cellWorkspaceNotesLive();

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
	//! The PRTP band: its kind rows as "name count" (empty until probes are placed).
	QStringList probeKindRows() const;
	bool probesShown() const;
	//! Click Place / flip Show probes, exactly as the mouse would.
	void placeProbes();
	//! Click Bake: place, then write the `.tbk` sector files.
	void bakeProbes();
	void setProbesShown( bool on );
	//! The Mods band (the harness drives it through its own clicks).
	CellModPickBox * modPicker() const;
	/*! lane OPENCELLFIX1: the dock's tabs, in a modder's order: "loadorder", "cells", "references", "show".
	 *  The last one shown is remembered; an open switches to References. */
	QString currentTab() const;
	void setCurrentTab( const QString & id );
	//! File > Open Cell...: the Cells tab.
	void showCellsTab() { setCurrentTab( QStringLiteral( "cells" ) ); }
	//! The Load order and Cells tabs' picker (the harness drives it).
	OpenCellPicker * cellPicker() const { return picker; }
	//! The Show tab's switches (built from the Show menu; wwMenuAsPanel).
	QWidget * showTabPanel() const { return showPanel; }
	//! True while the parts that need an open cell (the reference list, Legend, Mods, PRTP) are shown.
	bool cellPartsShown() const;

public slots:
	//! A cell scene was built, or the document changed: rebuild everything.
	void onSceneChanged();
	//! A viewport pick: put the cursor on that row without re-driving the pick.
	void onPicked( int index, int candidates );
	//! "" or "All" shows everything; otherwise a record type as `CellPickTable::typeName`.
	void setTypeFilter( const QString & type );
	//! Matches editor id, form id and model path, case-insensitively.
	void setTextFilter( const QString & text );
	/*! A row click: select the entry exactly as a viewport pick would (`cellPickSelect`), and frame it when `frame`. */
	void selectEntry( int index, bool frame );

signals:
	//! A view row changed: re-open this `.wwcell` through the ordinary path.
	void reopenRequested( const QString & path );
	//! lane OPENCELLFIX1: the Cells tab's Open wrote this `.wwcell`; open it through the ordinary path.
	void openCellRequested( const QString & path );

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
	WwDropdown * filterType = nullptr;	// lane OPENCELLFIX2: the dock's one dropdown
	QTreeWidget * list = nullptr;
	WwDropdown * overlayBox = nullptr;
	/*! The cell's view toggles, built from `cellShowRows()` in order: add a toggle by adding a row. */
	QVector<QAction *> showActions;
	/*! Row i of the reference model draws as pick entry `pickForRef[i]`, or -1 when nothing drew it. */
	QVector<int> pickForRef;
	QTreeWidget * legend = nullptr;
	/*! The PRTP band: Place re-opens the cell with probes placed, Bake places and bakes in one rebuild;
	 *  kind rows are read from the builder's notes. */
	QCheckBox * probesShow = nullptr;
	QAction * cellLights = nullptr;   // light the view with the cell's own lights
	QAction * cellGi = nullptr;       // add the bake's bounce of those lights
	QAction * cellFar = nullptr;      // past the band, the surfels' baked light in place of the lights
	QAction * cellIs = nullptr;       // the cell's imagespace (exposure, curve, grade, LUT)
	QAction * cellCull = nullptr;     // per-placement culling (gl/cellcull.h); ships off
	QAction * cellPrevis = nullptr;   // occlusion planes / boxes, rooms and portals (gl/cellcull.h); ships off
	QAction * cellFarLod = nullptr;   // the far field around the block (cellfarlod.h)
	QMenu * cellFarLodType = nullptr;
	QActionGroup * cellFarLodTypeGroup = nullptr;   // lane FARLOD1: Vanilla | FO4CS
	QMenu * cellFarGround = nullptr;   // lane FARDRAW1: Baked | Live, shader decals | Live, cached pages
	QActionGroup * cellFarGroundGroup = nullptr;
	QMenu * cellFarGroundQ = nullptr;   // lane FARDRAW1: the cached pages' sharpness
	QActionGroup * cellFarGroundQGroup = nullptr;
	QAction * cellTaa = nullptr;      // lane MOTION1: the game's temporal AA (one state with the Scene popup's row)
	QMenu * cellModernTaa = nullptr;	// lane TAA2: Off | Low | Medium | High | Ultra
	QActionGroup * cellModernTaaGroup = nullptr;
	// lane CELLALL1: the second row, the features that were env-only (their defaults unchanged; new ones ship off)
	QAction * cellAo = nullptr;
	QAction * cellShadow = nullptr;
	QAction * cellFlicker = nullptr;	// lane LIGHTFLICKER1: the lamps' LIGH flicker (ships off)
	QMenu * cellLampShadow = nullptr;	// lane LAMPSHADOW1: Off | Low | Medium | High | Ultra
	QActionGroup * cellLampShadowGroup = nullptr;
	QMenu * cellShadowBudget = nullptr;	// lane GAPLOOK1 (LIGHTSYS-3): Off | 4 | 8 | 16
	QActionGroup * cellShadowBudgetGroup = nullptr;
	QMenu * cellLightCurve = nullptr;	// lane LIGHTPHYS1: Off | 0.75 | 1.25 | 1.5 | 2
	QActionGroup * cellLightCurveGroup = nullptr;
	QMenu * cellLightCalib = nullptr;	// lane LIGHTPHYS3: 0.5 | 0.75 | 1 | 1.5 | 2
	QActionGroup * cellLightCalibGroup = nullptr;
	QList<QAction *> lookActions;	// lane GAPLOOK1: one tick a WwLook (gl/celllooks.h)
	QAction * cellSsr = nullptr;	// lane OPENCELLFIX2 note 20: indoors and outdoors alike
	QAction * cellReflProbe = nullptr;	// lane REFLPROBE1
	QAction * cellSsgi = nullptr;	// lane SSGI1
	QMenu * cellSsgiQ = nullptr;
	QActionGroup * cellSsgiQGroup = nullptr;
	QAction * cellAoDecal = nullptr;
	QAction * cellSpecOcc = nullptr;	// lane SPECOCC1: specular occlusion from the baked sky visibility; ships off
	QAction * cellGiGpu = nullptr;
	QAction * cellEmissiveGrid = nullptr;	// lane EMISSIVEGRID1
	QAction * cellParticles = nullptr;	// placed models' particle systems (cellview.h); ships off
	QAction * cellGrass = nullptr;    // the game's grass (placed at cell open)
	QAction * cellDof = nullptr;      // the game's depth of field
	QAction * cellMblur = nullptr;    // the game's motion blur
	QMenu * cellBloom = nullptr;      // Vanilla | FO4CS, FO4CS's rows and its physical glare
	QActionGroup * cellBloomGroup = nullptr;
	QAction * cellBloomGlare = nullptr;
	QMenu * cellPass = nullptr;
	QActionGroup * cellPassGroup = nullptr;  // Combined / GI / Sky visibility / surfel previews
	QPushButton * probesPlace = nullptr;
	QPushButton * probesBake = nullptr;
	QTreeWidget * probeKinds = nullptr;
	void rebuildProbeKinds();
	CellModPickBox * modPick = nullptr;	// the Mods band
	// lane OPENCELLFIX1: the tabs, the picker they hold, and the parts hidden until a cell is open
	QTabBar * tabs = nullptr;
	QStackedWidget * tabStack = nullptr;
	OpenCellPicker * picker = nullptr;
	QWidget * showPanel = nullptr;
	QWidget * refsBox = nullptr;
	QWidget * modsBox = nullptr;
	QWidget * prtpBox = nullptr;
	QString openedPath;	//!< the cell the tabs last switched to References for
	void showCellParts( bool on );
	QLabel * census = nullptr;
	QLabel * note = nullptr;
	bool noteRefusal = false;
};

/*! WW_CELLWS_TEST=<report path>: the workspace self-test inside the real application. */
void wwCellWorkspaceHarness( NifSkope * skope );

/*! WW_CELL_WSRESTORE=<report path>: drives workspace switches and a real close from inside the window
 *  (steps in WW_CELL_WSRESTORE_STEPS). Refuses without a scratch WW_SETTINGS_SCOPE. */
void wwWorkspaceRestoreHarness( NifSkope * skope );
//! True when this run may persist its layout: the driver above, in a scratch scope.
bool wwWorkspaceRestoreSavesUi();

#endif // CELLWORKSPACE_H
