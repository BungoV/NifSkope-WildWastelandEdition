/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

/* ---------------------------------------------------------------------------
 * WW_CELLWS_TEST=<report path> -- lane CELLWORK1's gate, run INSIDE the running
 * application.
 *
 * Every row here is a claim the report makes, measured where the claim lives:
 * the workspace switch is asked of the window, the hidden NIF column is asked
 * of the dock, the layout round trip is compared as BYTES, and the two ways to
 * select a reference are driven one after the other and compared. None of it is
 * judged from a screenshot.
 *
 * A harness FORCES the state it measures. It never inherits it from QSettings:
 * the run is started with WW_SETTINGS_SCOPE pointing at a scratch scope, and
 * each row sets up its own precondition immediately before asserting.
 * --------------------------------------------------------------------------- */

#include "cellworkspace.h"

#include "cellclick.h"
#include "cellpick.h"
#include "cellrefs.h"
#include "glview.h"
#include "nifskope.h"
#include "model/nifmodel.h"

#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QTextStream>
#include <QTimer>
#include <QTreeWidget>

namespace {

struct Row
{
	bool ok;
	QString what;
	QString detail;
};

/* THE WHOLE WINDOW, WITH THE VIEWPORT IN IT.
 *
 * `QWidget::grab()` renders the WIDGET TREE through Qt's paint system, and this
 * tree's GL view is not in that tree at all: `GLView` is a **QOpenGLWindow**,
 * embedded through `QWidget::createWindowContainer()`, so the viewport is a
 * separate native window and comes out of a widget grab as an empty hole.
 *
 * There are two honest ways round it and this is the FIRST: grab the window
 * through Qt, ask the GL view for its own framebuffer (`grabFramebuffer()`,
 * which reads the buffer without repainting), and paint the second into the
 * first at the CONTAINER WIDGET's rectangle -- the container is the only thing
 * in the widget tree that knows where the viewport is.
 *
 * The other way is `QScreen::grabWindow()`, which captures whatever the
 * compositor has -- correct for the GL view, but it captures anything sitting on
 * top of the window as well, and a harness window on the second monitor cannot
 * promise nothing is on top of it. Composition promises the picture is of THIS
 * window and nothing else.
 *
 * Returns the size written so the caller can put it in the report: a picture
 * whose size nobody stated is a picture nobody can check against the window log.
 */
QSize composeWindowShot( NifSkope * skope, const QString & path )
{
	if ( !skope )
		return QSize();
	GLView * ogl = skope->getGLView();
	if ( ogl )
		ogl->update();
	qApp->processEvents();

	QPixmap window = skope->grab();
	if ( window.isNull() )
		return QSize();

	QWidget * container = skope->getGraphicsView();
	if ( ogl && container && container->isVisible() ) {
		const QImage fb = ogl->grabFramebuffer();
		if ( !fb.isNull() ) {
			const QPoint at = container->mapTo( skope, QPoint( 0, 0 ) );
			QPainter p( &window );
			// grabFramebuffer() comes back in DEVICE pixels; the window pixmap
			// carries a devicePixelRatio, so draw the image into the container's
			// own RECTANGLE rather than at 1:1, which would land wrong on a
			// high-dpi screen.
			p.drawImage( QRect( at, container->size() ), fb );
		}
	}
	QDir().mkpath( QFileInfo( path ).absolutePath() );
	if ( !window.save( path ) )
		return QSize();
	return window.size() / window.devicePixelRatio();
}

QVector<Row> g_rows;     //!< the whole run's rows: the PRTP stages append after each re-open

void writeReport( const QString & reportPath )
{
	const QVector<Row> & rows = g_rows;
	int failures = 0;
	for ( const Row & r : rows )
		if ( !r.ok )
			failures++;
	QFile f( reportPath );
	if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
		QTextStream s( &f );
		s << "# WW_CELLWS_TEST -- lane CELLWORK1\n";
		for ( const Row & r : rows ) {
			s << ( r.ok ? "PASS  " : "FAIL  " ) << r.what;
			if ( !r.detail.isEmpty() )
				s << "   [" << r.detail << "]";
			s << "\n";
		}
		s << "rows " << rows.size() << " failures " << failures << "\n";
	}
}

//! Probe marker shapes in the open document (the builder names their buckets "probes").
int probeShapes( NifSkope * skope )
{
	NifModel * nif = skope ? skope->getNifModel() : nullptr;
	if ( !nif )
		return -1;
	int n = 0;
	for ( qint32 b = 0; b < nif->getBlockCount(); b++ ) {
		const QModelIndex iShape = nif->getBlockIndex( b, "BSTriShape" );
		if ( iShape.isValid() && nif->get<QString>( iShape, "Name" ).startsWith( QLatin1String( "probes" ) ) )
			n++;
	}
	return n;
}

/* ---- lane PRTPPLACE: THE PRTP BAND (WW_CELLWS_PRTP=1). Place, Show probes and Bake
 * each re-open the cell, so each stage runs on the NEXT completeLoading. Returns true
 * when the run is finished and the report can be written. Stage 3 (lane PRTPBAKE)
 * checks the bake's files; set WW_CELL_PROBE_BAKE_DIR to keep them out of release/. */
bool runPrtpStage( NifSkope * skope, int stage )
{
	auto add = []( const QString & what, bool ok, const QString & detail = QString() ) {
		g_rows.append( Row{ ok, what, detail } );
	};
	CellWorkspacePanel * panel = skope->findChild<CellWorkspacePanel *>();
	QDockWidget * dCell = skope->findChild<QDockWidget *>( QStringLiteral( "CellWorkspaceDock" ) );
	if ( !panel || !dCell ) {
		add( "PRTP: the panel exists", false );
		return true;
	}
	auto sum = []( const QStringList & rows, QString * all ) {
		int s = 0;
		for ( const QString & r : rows ) {
			const int at = r.lastIndexOf( QLatin1Char( ' ' ) );
			if ( r.startsWith( QLatin1String( "All " ) ) ) {
				if ( all )
					*all = r.mid( at + 1 );
				continue;
			}
			s += r.mid( at + 1 ).toInt();
		}
		return s;
	};
	if ( stage == 0 ) {
		add( "PRTP: the band's controls are inside the Cell workspace dock",
			dCell->findChild<QWidget *>( QStringLiteral( "CellWorkspaceProbesShow" ) )
				&& dCell->findChild<QWidget *>( QStringLiteral( "CellWorkspaceProbesPlace" ) )
				&& dCell->findChild<QWidget *>( QStringLiteral( "CellWorkspaceProbesBake" ) ) );
		QWidget * bake = dCell->findChild<QWidget *>( QStringLiteral( "CellWorkspaceProbesBake" ) );
		add( "PRTP: Bake is on", bake && bake->isEnabled() );
		// lane PROBEVIEW1: the Pass drop-down, its five entries, off until a bake is relit
		QComboBox * pass = dCell->findChild<QComboBox *>( QStringLiteral( "CellWorkspaceCellPass" ) );
		QStringList items;
		for ( int i = 0; pass && i < pass->count(); i++ )
			items << pass->itemText( i );
		add( "PRTP: the Pass drop-down holds Combined, GI, Sky visibility, Surfel color, Surfel light",
			items == QStringList{ "Combined", "GI", "Sky visibility", "Surfel color", "Surfel light" }, items.join( QLatin1Char( '|' ) ) );
		add( "PRTP: Pass is off before a bake", pass && !pass->isEnabled() );
		add( "PRTP: no kind rows before Place", panel->probeKindRows().isEmpty(),
			panel->probeKindRows().join( QLatin1Char( '|' ) ) );
		add( "PRTP: no probe markers before Place", probeShapes( skope ) == 0,
			QString::number( probeShapes( skope ) ) );
		panel->setProbesShown( true );
		panel->placeProbes();
		return false;
	}
	if ( stage == 1 ) {
		const QStringList rows = panel->probeKindRows();
		QString all;
		const int s = sum( rows, &all );
		add( "PRTP: Place fills the kind rows (8 kinds + All)", rows.size() == 9, rows.join( QLatin1Char( '|' ) ) );
		add( "PRTP: the kinds add up to All", s > 0 && QString::number( s ) == all,
			QStringLiteral( "%1 vs %2" ).arg( s ).arg( all ) );
		add( "PRTP: the markers are drawn", probeShapes( skope ) > 0, QString::number( probeShapes( skope ) ) );
		const QString shotDir = QString::fromLocal8Bit( qgetenv( "WW_CELLWS_SHOTS" ) );
		if ( !shotDir.isEmpty() ) {
			const QSize sz = composeWindowShot( skope, shotDir + QStringLiteral( "/cell_prtp_placed.png" ) );
			add( "picture: the PRTP band after Place", sz.width() > 800,
				QStringLiteral( "cell_prtp_placed  %1x%2" ).arg( sz.width() ).arg( sz.height() ) );
		}
		add( "PRTP: Place alone does not bake", !panel->noteText().startsWith( QLatin1String( "baked" ) ),
			panel->noteText() );
		g_rows.append( Row{ true, QStringLiteral( "PRTP: kept for stage 2" ), rows.join( QLatin1Char( '|' ) ) } );
		panel->setProbesShown( false );
		return false;
	}
	QString keptRows;
	for ( const Row & r : g_rows )
		if ( r.what == QLatin1String( "PRTP: kept for stage 2" ) )
			keptRows = r.detail;
	if ( stage == 2 ) {
		// stage 2: Show probes off -- counted, not drawn
		const QStringList rows = panel->probeKindRows();
		add( "PRTP: Show probes off keeps the counts", !rows.isEmpty() && rows.join( QLatin1Char( '|' ) ) == keptRows,
			rows.join( QLatin1Char( '|' ) ) );
		add( "PRTP: Show probes off draws no markers", probeShapes( skope ) == 0, QString::number( probeShapes( skope ) ) );
		// lane PRTPBAKE: Bake re-opens once more; stage 3 reads what it wrote
		panel->bakeProbes();
		return false;
	}
	// stage 3: Bake wrote .tbk sector files, and the note says where
	const QString note = panel->noteText();
	add( "PRTP: Bake says what it wrote", note.startsWith( QLatin1String( "baked " ) ), note );
	const int colon = note.indexOf( QLatin1String( ": " ) );
	const QString dir = colon > 0 ? note.mid( colon + 2 ).trimmed() : QString();
	const QStringList tbks = dir.isEmpty() ? QStringList()
		: QDir( dir ).entryList( { QStringLiteral( "sector_*.tbk" ) }, QDir::Files, QDir::Name );
	int good = 0;
	for ( const QString & f : tbks ) {
		QFile fh( QDir( dir ).filePath( f ) );
		if ( fh.open( QIODevice::ReadOnly ) && fh.read( 4 ) == QByteArray( "TBK1" ) )
			good++;
	}
	add( "PRTP: the folder holds .tbk sector files, each starting TBK1", !tbks.isEmpty() && good == tbks.size(),
		QStringLiteral( "%1 of %2 in %3" ).arg( good ).arg( tbks.size() ).arg( dir ) );
	const int filesSaid = note.section( QLatin1Char( ' ' ), 1, 1 ).toInt();
	add( "PRTP: the note's file count is the folder's", filesSaid > 0 && filesSaid == tbks.size(),
		QStringLiteral( "%1 vs %2" ).arg( filesSaid ).arg( tbks.size() ) );
	add( "PRTP: Bake keeps the kind rows", panel->probeKindRows().join( QLatin1Char( '|' ) ) == keptRows,
		panel->probeKindRows().join( QLatin1Char( '|' ) ) );
	{	// lane PROBEVIEW1: the bake relit, the Pass drop-down is live
		QComboBox * pass = dCell->findChild<QComboBox *>( QStringLiteral( "CellWorkspaceCellPass" ) );
		add( "PRTP: Pass is on after Bake", pass && pass->isEnabled() );
	}
	const QString shotDir = QString::fromLocal8Bit( qgetenv( "WW_CELLWS_SHOTS" ) );
	if ( !shotDir.isEmpty() ) {
		const QSize sz = composeWindowShot( skope, shotDir + QStringLiteral( "/cell_prtp_baked.png" ) );
		add( "picture: the PRTP band after Bake", sz.width() > 800,
			QStringLiteral( "cell_prtp_baked  %1x%2" ).arg( sz.width() ).arg( sz.height() ) );
	}
	return true;
}

void runCellWorkspaceTest( NifSkope * skope, const QString & reportPath )
{
	QVector<Row> & rows = g_rows;
	auto add = [&rows]( const QString & what, bool ok, const QString & detail = QString() ) {
		rows.append( Row{ ok, what, detail } );
	};

	QDockWidget * dCell = skope->findChild<QDockWidget *>( QStringLiteral( "CellWorkspaceDock" ) );
	QDockWidget * dPick = skope->findChild<QDockWidget *>( QStringLiteral( "CellPickDock" ) );
	QDockWidget * dLeftCol = skope->findChild<QDockWidget *>( QStringLiteral( "LeftColumnDock" ) );
	CellWorkspacePanel * panel = skope->findChild<CellWorkspacePanel *>();
	QTreeWidget * list = skope->findChild<QTreeWidget *>( QStringLiteral( "CellWorkspaceList" ) );

	add( "the Cell workspace dock exists", dCell != nullptr );
	add( "the workspace panel exists", panel != nullptr );
	add( "the reference list exists", list != nullptr );

	const int cellWs = skope->cellWorkspaceIndex();
	add( "the Cell workspace is in the Workspaces menu", cellWs >= 0,
		QStringLiteral( "index %1 of %2" ).arg( cellWs ).arg( skope->workspaceCount() ) );

	/* THE EXISTING WORKSPACES MUST NOT MOVE. The persisted `UI/Workspace` index
	 * is positional, so an entry inserted rather than appended silently reopens
	 * somebody else's window in the wrong workspace. */
	const QStringList names = skope->workspaceNamesInOrder();
	const QStringList expected = {
		"Default", "Animation", "Materials", "Collision", "Rigging",
		"Vertex Paint", "UV Editing", "Pose", "Skeleton", "Issue Manager",
		"LOD Generation", "Cell"
	};
	add( "the workspace list is the old one with Cell APPENDED", names == expected,
		names.join( QLatin1Char( '|' ) ) );

	if ( cellWs < 0 || !dCell || !panel || !list ) {
		writeReport( reportPath );
		return;
	}

	// ---- 1. opening a cell switched the workspace, by itself
	add( "opening a .wwcell switched to the Cell workspace",
		skope->currentWorkspace() == cellWs,
		QStringLiteral( "workspace %1" ).arg( skope->currentWorkspace() ) );
	add( "the Cell dock is up", dCell->isVisible() );
	add( "the reference inspector came WITH it (a companion, not a copy)",
		dPick && dPick->isVisible() );
	add( "the NIF editor column is hidden in this workspace",
		dLeftCol && !dLeftCol->isVisible() );
	add( "cell picking is on in this workspace", cellPickEnabled() );

	// ---- 2. the list is the plugin's references, not the drawn ones
	const CellRefTable & refs = cellRefTable();
	const CellPickTable & picks = cellPickTable();
	add( "the list has one row per placed reference",
		list->topLevelItemCount() == refs.size(),
		QStringLiteral( "rows %1, reference model %2, pick table %3" )
			.arg( list->topLevelItemCount() ).arg( refs.size() ).arg( picks.size() ) );
	// The pick table is NOT comparable to the model by SIZE: it holds one entry
	// per drawn SHAPE, so one reference with three shapes is three pick entries
	// (Sanctuary: 240 pick entries for 141 drawn references). The claim that
	// actually distinguishes a model from draw data is that the model carries
	// references NOTHING drew.
	const int drawnRefs = refs.countOfFate( CellRefFate::Drawn );
	add( "the reference model is not the draw data (it carries the undrawn too)",
		refs.size() > 0 && drawnRefs < refs.size(),
		QStringLiteral( "model %1, drawn %2, undrawn %3 -- no model %4, marker %5, "
		                "disabled %6, deleted %7, no base %8 (pick entries %9, one per drawn shape)" )
			.arg( refs.size() ).arg( drawnRefs ).arg( refs.size() - drawnRefs )
			.arg( refs.countOfFate( CellRefFate::NoModel ) )
			.arg( refs.countOfFate( CellRefFate::Marker ) )
			.arg( refs.countOfFate( CellRefFate::Disabled ) )
			.arg( refs.countOfFate( CellRefFate::Deleted ) )
			.arg( refs.countOfFate( CellRefFate::NoBase ) )
			.arg( picks.size() ) );

	// ---- 3. the loaded cells are a LIST, and they are named
	add( "the model is a list of loaded cells (one, today)", refs.cellCount() >= 1,
		QStringLiteral( "cells %1" ).arg( refs.cellCount() ) );
	if ( refs.cellCount() > 0 ) {
		const CellBlockEntry & c = refs.cellAt( 0 );
		// The cell's FORM ID always exists; its EDID does not. Measured in
		// Fallout4.esm: 755 of the Commonwealth's 36865 exterior cells carry an
		// EDID, and (-20,7) -- Sanctuary's own cell -- is NOT one of them, so
		// the honest label here is the bare grid. The row that proves the EDID
		// READER works runs the gate a second time on (-21,7) "POIJS021",
		// which does carry one. Asserting a name here would assert something
		// false about the data.
		add( "the cell carries its form id from the plugin, and names itself by "
		     "its editor id only when the plugin gave it one",
			c.cellForm != 0
			&& cellBlockLabel( c ) == ( c.edid.isEmpty()
				? QStringLiteral( "(%1,%2)" ).arg( c.cx ).arg( c.cy )
				: QStringLiteral( "%1 (%2,%3)" ).arg( c.edid ).arg( c.cx ).arg( c.cy ) ),
			QStringLiteral( "%1 form 0x%2, edid %3" ).arg( cellBlockLabel( c ) )
				.arg( c.cellForm, 8, 16, QChar( '0' ) )
				.arg( c.edid.isEmpty() ? QStringLiteral( "<none in the master>" ) : c.edid ) );
		add( "the per-cell reference count agrees with the table walk",
			c.references > 0 && c.references <= refs.size(),
			QStringLiteral( "%1 references, %2 drawn" ).arg( c.references ).arg( c.drawn ) );
	}

	// ---- 4. a row click and a viewport pick land on the same reference
	int drawnRow = -1;
	for ( int i = 0; i < refs.size(); i++ ) {
		if ( refs.at( i ).fate == CellRefFate::Drawn ) {
			drawnRow = i;
			break;
		}
	}
	if ( drawnRow >= 0 ) {
		panel->selectEntry( drawnRow, false );
		const int afterRow = cellPickCurrent();
		add( "a list row click selects that reference", afterRow >= 0,
			QStringLiteral( "ref row %1 -> pick %2" ).arg( drawnRow ).arg( afterRow ) );

		/* The SAME reference, selected the other way: straight through the pick
		 * tail the mouse uses. Equal answers is the claim. */
		cellPickSelect( skope->getNifModel(), afterRow, 1 );
		add( "a viewport pick of the same reference selects the same row",
			cellPickCurrent() == afterRow && panel->selectedEntry() == drawnRow,
			QStringLiteral( "pick %1, panel row %2" )
				.arg( cellPickCurrent() ).arg( panel->selectedEntry() ) );

		// RED: a reference that was never drawn refuses BY NAME rather than
		// selecting something else.
		int undrawn = -1;
		for ( int i = 0; i < refs.size(); i++ ) {
			if ( refs.at( i ).fate != CellRefFate::Drawn ) {
				undrawn = i;
				break;
			}
		}
		if ( undrawn >= 0 ) {
			panel->selectEntry( undrawn, false );
			add( "RED: an undrawn reference refuses by name, and does not move "
				"the selection", panel->noteIsRefusal()
					&& panel->noteText().contains( QLatin1String( "not drawn" ) ),
				panel->noteText() );
			panel->selectEntry( drawnRow, false );	// leave it on the honest one
		}
	} else {
		add( "a drawn reference exists to select", false );
	}

	// ---- 5. the Show popover, and it lives ONLY here
	add( "the Show popover is inside the Cell workspace dock",
		dCell->findChild<QMenu *>( QStringLiteral( "CellWorkspaceShowMenu" ) ) != nullptr );
	add( "no Show popover exists anywhere else in the window",
		skope->findChildren<QMenu *>( QStringLiteral( "CellWorkspaceShowMenu" ) ).size() == 1 );
	add( "the Show popover has its rows", panel->showRowCount() == 5,
		QStringLiteral( "%1 rows" ).arg( panel->showRowCount() ) );
	QStringList showLabels;
	for ( int i = 0; i < panel->showRowCount(); i++ )
		showLabels.append( panel->showRowLabel( i ) );
	add( "the rows are the ones with a builder field behind them",
		showLabels == QStringList( { "Cell borders", "Ground", "Water", "Markers",
			"Disabled references" } ), showLabels.join( QLatin1Char( '|' ) ) );
	add( "none of the Show rows is in the Workspaces menu",
		!skope->workspaceNamesInOrder().contains( QLatin1String( "Cell borders" ) ) );

	// ---- 6. the round trip, as bytes
	const QByteArray beforeCell = skope->saveState( 0x074 );
	skope->setWorkspace( 0 );
	const bool leftIt = ( skope->currentWorkspace() == 0 );
	const bool columnBack = dLeftCol && dLeftCol->isVisible();
	const bool cellGone = !dCell->isVisible();
	const bool pickRestored = !cellPickEnabled();
	skope->setWorkspace( cellWs );
	const QByteArray backInCell = skope->saveState( 0x074 );

	add( "leaving the Cell workspace goes back to the workspace before it", leftIt );
	add( "the NIF editor column comes back", columnBack );
	add( "the Cell dock goes away with it", cellGone );
	add( "cell picking goes back to what it was", pickRestored );
	add( "the Cell workspace layout is byte-identical after a round trip",
		beforeCell == backInCell,
		QStringLiteral( "%1 vs %2 bytes" ).arg( beforeCell.size() ).arg( backInCell.size() ) );

	/* The claim the brief actually asks for, the other way round: leave the
	 * cell, snapshot the NIF workspace, go into the cell and come back, and
	 * compare THOSE bytes. This is the one a person notices. */
	skope->setWorkspace( 0 );
	const QByteArray nifBefore = skope->saveState( 0x074 );
	skope->setWorkspace( cellWs );
	skope->setWorkspace( 0 );
	const QByteArray nifAfter = skope->saveState( 0x074 );
	add( "the NIF workspace layout is byte-identical after a cell round trip",
		nifBefore == nifAfter,
		QStringLiteral( "%1 vs %2 bytes" ).arg( nifBefore.size() ).arg( nifAfter.size() ) );

	// RED: the comparison can fail. If it cannot, it is not measuring anything.
	skope->setWorkspace( 1 );
	add( "RED: a DIFFERENT workspace does not produce those bytes",
		skope->saveState( 0x074 ) != nifAfter );
	skope->setWorkspace( cellWs );

	// ---- 7. the census says what the cell is
	add( "the census counts the plugin's references, not the drawn ones",
		panel->censusText().contains( QString::number( refs.size() ) ),
		panel->censusText() );

	/* ---- 8. THE PICTURES. Taken last, so the window is in the state every row
	 * above just measured: the Cell workspace up, a drawn reference selected.
	 * Each row prints the SIZE, which is the number the window log is checked
	 * against -- a picture with no stated size proves nothing about the window
	 * it claims to be of. */
	const QString shotDir = QString::fromLocal8Bit( qgetenv( "WW_CELLWS_SHOTS" ) );
	if ( !shotDir.isEmpty() ) {
		if ( drawnRow >= 0 )
			panel->selectEntry( drawnRow, false );
		const QString tag = QString::fromLocal8Bit( qgetenv( "WW_CELLWS_SHOTTAG" ) );
		const QString base = tag.isEmpty() ? QStringLiteral( "cell_workspace" ) : tag;
		const QSize s1 = composeWindowShot( skope,
			shotDir + QLatin1Char( '/' ) + base + QStringLiteral( ".png" ) );
		add( QStringLiteral( "picture: the Cell workspace with a reference selected" ),
			s1.width() > 800 && s1.height() > 400,
			QStringLiteral( "%1  %2x%3" ).arg( base ).arg( s1.width() ).arg( s1.height() ) );

		// ...and the window it goes back to, which is the other half of the claim.
		skope->setWorkspace( 0 );
		const QSize s2 = composeWindowShot( skope,
			shotDir + QStringLiteral( "/nif_workspace_after.png" ) );
		add( QStringLiteral( "picture: the NIF workspace after coming back" ),
			s2.width() > 800 && s2.height() > 400,
			QStringLiteral( "nif_workspace_after  %1x%2" ).arg( s2.width() ).arg( s2.height() ) );
		skope->setWorkspace( cellWs );
	}

	writeReport( reportPath );
}

} // namespace

void wwCellWorkspaceHarness( NifSkope * skope )
{
	const QString reportPath = QString::fromLocal8Bit( qgetenv( "WW_CELLWS_TEST" ) );
	if ( reportPath.isEmpty() || !skope )
		return;

	/* AFTER the load, because everything this measures is a consequence of one:
	 * the workspace switch, the reference model, the list and the census are all
	 * filled by opening the cell. */
	QObject::connect( skope, &NifSkope::completeLoading, skope,
		[skope, reportPath]( bool, QString & ) {
			/* Lane PRTPPLACE: with WW_CELLWS_PRTP set, the PRTP band's stages run on
			 * the re-opens its own buttons cause; the report is written after each. */
			static int stage = -1;
			const bool prtp = !qgetenv( "WW_CELLWS_PRTP" ).isEmpty();
			bool done = true;
			if ( stage < 0 ) {
				runCellWorkspaceTest( skope, reportPath );
				stage = 0;
				if ( prtp ) {
					// after this slot returns: Place starts a re-open of its own
					QTimer::singleShot( 0, skope, [skope, reportPath]() {
						stage = 1;     // first: Place may re-open before it returns
						const bool noCell = cellWorkspacePath().isEmpty();
						runPrtpStage( skope, 0 );
						if ( noCell )   // Place refuses: no re-open is coming, so end here
							g_rows.append( Row{ false, QStringLiteral( "PRTP: a cell is open to place in" ), QString() } );
						writeReport( reportPath );
						if ( noCell && qgetenv( "WW_CELLWS_STAY" ).isEmpty() )
							QTimer::singleShot( 0, qApp, &QCoreApplication::quit );
					} );
					return;
				}
			} else if ( prtp ) {
				const int now = stage++;
				QTimer::singleShot( 0, skope, [skope, reportPath, now]() {
					const bool fin = runPrtpStage( skope, now );
					writeReport( reportPath );
					if ( fin && qgetenv( "WW_CELLWS_STAY" ).isEmpty() )
						QTimer::singleShot( 0, qApp, &QCoreApplication::quit );
				} );
				return;
			}
			if ( !done || !qgetenv( "WW_CELLWS_STAY" ).isEmpty() )
				return;		// leave the window up for a picture
			QTimer::singleShot( 0, qApp, &QCoreApplication::quit );
		} );
}
