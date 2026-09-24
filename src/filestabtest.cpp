/* WW_FILESTAB_TEST: lane FILESTAB's pre-registered gates, run inside the real
   application against the real widget tree.

   It lives in its own translation unit rather than beside the thirty other
   WW_*_TEST harnesses in nifskope_ui.cpp because that file is 31,000 lines and
   another lane is writing into it this session; the whole of this lane's
   footprint there is the one line that calls wwFilesTabHarness().

   WHAT IT MEASURES (the gate letters are the brief's):

     (1) ZERO "NIF" strings in the Files page's user-visible text, READ OFF THE
         WIDGET TREE -- the tab's own text and tooltip, every label, button,
         placeholder, tooltip and accessible name under the page, both models'
         header labels, the tree's group rows and the list's empty message --
         and not off the source, because a rename that missed a runtime-built
         string would still pass a grep.
         FLOOR: the scan is seeded with one known offender (the loaded-files
         placeholder is set back to "Search loaded NIFs...") and must find
         exactly it, then the seed is removed and the count must return to 0. A
         scan that has never found anything has not scanned.

     (2) THE TREE LISTS .hkx AND .btr, counted per extension over the model the
         browser is actually showing, with the first .hkx path printed so its
         archive can be read in the log.
         FLOOR: .nif must also be > 0 -- a walk that returns zero of everything
         would otherwise pass the ">= 1 .hkx" test by being broken in a
         different direction. (It cannot: 0 >= 1 is false. The floor here is the
         .nif count proving the walker reaches leaves at all.)

     (3) OPENING AN .hkx FROM THE TREE plays it on the open model and answers
         with HkxPlayback's own summary line: the pre-registered 78 matched /
         17 unmatched / 4 case-folded of the player skeleton. The row is a real
         row in the browser's model and the open goes through the view's own
         doubleClicked path, not through a private back door.

     (4) UNLOAD RESTORES THE BIND POSE BYTE-IDENTICALLY -- every node's
         Transform, all of it, memcmp, not "close enough".
         FLOOR: while the clip is playing at least one node's bytes must differ,
         or "it restored" is a statement about nothing.

     (5) PANEL STYLE (nifskope-ww-panel-style), counted with floors: no
         QGroupBox on the page, every tool button carries a tooltip AND an
         accessible name, both search fields carry a placeholder, no check-box
         label explains itself after a dash, and the two halves of the page are
         on distinct rows measured as GEOMETRY.
         FLOOR: one tooltip is deliberately blanked, the count must go to 1, and
         it is restored.

     (6) THE REFUSAL. With no model open -- measured as zero named nodes, which
         is the same instrument the summary line uses -- opening an .hkx must
         refuse IN WORDS and load nothing. Runs LAST, because it empties the
         scene to create the condition.

   ENVIRONMENT (tests/spells/files_tab.sh sets all of them):
     WW_FILESTAB_TEST=1              arm the harness
     WW_FILESTAB_RESOURCES=a;b;c     resource roots FORCED for Fallout 4, so the
                                     tree census does not inherit the user's own
                                     Settings > Resources (a harness forces the
                                     state it measures)
     WW_FILESTAB_CLIP=<file.hkx>     the clip opened through the tree
     WW_FILESTAB_EXPECT=78,17,4      gate (3)'s pre-registered counts
     WW_FILESTAB_SHOT=<png>          grab the left dock after the renames
   Log: release/ww_filestab_test.log

   Lane FILESTAB, 2026-09-10. */

#include "filestab.h"

#include "nifskope.h"
#include "glview.h"
#include "gamemanager.h"
#include "hkxplayback.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "model/nifmodel.h"

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSplitter>
#include <QTabBar>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QUndoStack>
#include <QWidget>

namespace
{

struct WwFtState
{
	int stage = 0;
	int checks = 0;
	int fails = 0;
	QString text;
	QString clipPath;
	QString shot;
	int expMatched = 78, expUnmatched = 17, expFolded = 4;
};

void wwCheck( WwFtState & st, const QString & what, bool pass )
{
	st.checks++;
	if ( !pass )
		st.fails++;
	st.text += ( pass ? QStringLiteral( "  ok   " ) : QStringLiteral( "  FAIL " ) ) + what
		+ QStringLiteral( "\n" );
}

void wwSay( WwFtState & st, const QString & line )
{
	st.text += line + QStringLiteral( "\n" );
}

/*
 *  (1) THE STRING SCAN
 */

/*! Is this piece of user-visible text a FILE NAME or a path rather than a label?
 *
 *  The tree's whole point is to list files, and a great many of them are called
 *  something.nif. Counting those as "the word NIF is still in the dock" would
 *  make the gate impossible to pass and would say nothing about the renames, so
 *  a string is skipped when it is a path or ends in one of the extensions the
 *  tab indexes. Everything else -- every label, every sentence -- is scanned.
 */
bool wwLooksLikeAFileName( const QString & s )
{
	if ( s.contains( QLatin1Char( '/' ) ) || s.contains( QLatin1Char( '\\' ) ) )
		return true;
	for ( const QString & ext : wwFilesTabExtensions() ) {
		if ( s.endsWith( ext, Qt::CaseInsensitive ) )
			return true;
	}
	return false;
}

void wwAdd( QStringList & out, const QString & where, const QString & s )
{
	if ( s.trimmed().isEmpty() || wwLooksLikeAFileName( s ) )
		return;
	if ( !s.contains( QStringLiteral( "nif" ), Qt::CaseInsensitive ) )
		return;
	// NifSkope's own product name is not a mention of the file format
	QString probe = s;
	probe.remove( QStringLiteral( "NifSkope" ), Qt::CaseInsensitive );
	if ( !probe.contains( QStringLiteral( "nif" ), Qt::CaseInsensitive ) )
		return;
	out.append( where + QStringLiteral( ": \"" ) + s + QStringLiteral( "\"" ) );
}

//! Every group row of an item model (a row that is not one of the files).
void wwScanModelRows( QStringList & out, const QAbstractItemModel * m,
					  const QModelIndex & parent, const QString & where, int depth )
{
	if ( !m || depth > 3 )
		return;
	const int rows = m->rowCount( parent );
	for ( int r = 0; r < qMin( rows, 4000 ); r++ ) {
		const QModelIndex idx = m->index( r, 0, parent );
		wwAdd( out, where, idx.data( Qt::DisplayRole ).toString() );
		wwAdd( out, where + QStringLiteral( " tooltip" ), idx.data( Qt::ToolTipRole ).toString() );
		if ( m->hasChildren( idx ) )
			wwScanModelRows( out, m, idx, where, depth + 1 );
	}
}

void wwScanActions( QStringList & out, const QList<QAction *> & acts, const QString & where )
{
	for ( QAction * a : acts ) {
		if ( !a )
			continue;
		wwAdd( out, where + QStringLiteral( " action" ), a->text() );
		wwAdd( out, where + QStringLiteral( " action tip" ), a->toolTip() );
		if ( a->menu() )
			wwScanActions( out, a->menu()->actions(), where );
	}
}

/*! Every user-visible string the Files page shows, that still says "NIF".
 *
 *  Read off the LIVE widget tree: a rename that missed a string built at run
 *  time (a header label, a status sentence, a menu item) would pass a grep over
 *  the source and fail here, which is the point.
 */
QStringList wwScanFilesPage( NifSkope * skope )
{
	QStringList out;
	if ( !skope )
		return out;

	if ( auto * tabs = skope->findChild<QTabBar *>( QStringLiteral( "LeftColumnModeSelector" ) ) ) {
		for ( int i = 0; i < tabs->count(); i++ ) {
			// only the page this lane owns: the Header tab really does show a
			// NIF header and its tooltip says so on purpose
			if ( tabs->tabText( i ).compare( QStringLiteral( "Files" ), Qt::CaseInsensitive ) != 0
				 && tabs->tabText( i ).compare( QStringLiteral( "NIFs" ), Qt::CaseInsensitive ) != 0 )
				continue;
			wwAdd( out, QStringLiteral( "tab text" ), tabs->tabText( i ) );
			wwAdd( out, QStringLiteral( "tab tooltip" ), tabs->tabToolTip( i ) );
		}
	}

	auto * page = skope->findChild<QSplitter *>( QStringLiteral( "NifBrowserSplitter" ) );
	if ( !page )
		return out;

	wwAdd( out, QStringLiteral( "page tooltip" ), page->toolTip() );
	wwAdd( out, QStringLiteral( "page accessible" ), page->accessibleName() );
	for ( int h = 0; h < page->count(); h++ ) {
		if ( QWidget * handle = page->handle( h ) ) {
			wwAdd( out, QStringLiteral( "splitter handle tooltip" ), handle->toolTip() );
			wwAdd( out, QStringLiteral( "splitter handle accessible" ), handle->accessibleName() );
		}
	}

	const QList<QWidget *> kids = page->findChildren<QWidget *>();
	for ( QWidget * w : kids ) {
		if ( !w )
			continue;
		const QString where = w->metaObject()->className()
			+ QStringLiteral( " " ) + w->objectName();
		wwAdd( out, where + QStringLiteral( " tooltip" ), w->toolTip() );
		wwAdd( out, where + QStringLiteral( " accessible" ), w->accessibleName() );
		wwAdd( out, where + QStringLiteral( " whatsthis" ), w->whatsThis() );
		if ( auto * le = qobject_cast<QLineEdit *>( w ) )
			wwAdd( out, where + QStringLiteral( " placeholder" ), le->placeholderText() );
		if ( auto * lb = qobject_cast<QLabel *>( w ) )
			wwAdd( out, where + QStringLiteral( " text" ), lb->text() );
		if ( auto * ab = qobject_cast<QAbstractButton *>( w ) )
			wwAdd( out, where + QStringLiteral( " text" ), ab->text() );
		if ( auto * tb = qobject_cast<QToolButton *>( w ); tb && tb->menu() )
			wwScanActions( out, tb->menu()->actions(), where );
		if ( auto * cb = qobject_cast<QComboBox *>( w ) ) {
			for ( int i = 0; i < cb->count(); i++ )
				wwAdd( out, where + QStringLiteral( " item" ), cb->itemText( i ) );
		}
		if ( auto * v = qobject_cast<QAbstractItemView *>( w ) ) {
			if ( QAbstractItemModel * m = v->model() ) {
				for ( int c = 0; c < m->columnCount(); c++ ) {
					wwAdd( out, where + QStringLiteral( " header" ),
						   m->headerData( c, Qt::Horizontal, Qt::DisplayRole ).toString() );
				}
				wwScanModelRows( out, m, QModelIndex(), where + QStringLiteral( " row" ), 0 );
			}
		}
	}
	return out;
}

/*
 *  (2) THE TREE CENSUS
 */

void wwCountExtensions( const QAbstractItemModel * m, const QModelIndex & parent,
						QHash<QString, int> & tally, QString & firstHkx, int depth )
{
	if ( !m || depth > 12 )
		return;
	const int rows = m->rowCount( parent );
	for ( int r = 0; r < rows; r++ ) {
		const QModelIndex idx = m->index( r, 0, parent );
		if ( m->hasChildren( idx ) ) {
			wwCountExtensions( m, idx, tally, firstHkx, depth + 1 );
			continue;
		}
		// column 1 holds the archive-or-loose path; column 0 is the leaf name
		const QModelIndex pathIdx = m->index( r, 1, parent );
		const QString path = pathIdx.isValid()
			? pathIdx.data( Qt::EditRole ).toString() : QString();
		const QString name = idx.data( Qt::DisplayRole ).toString();
		const QString probe = path.isEmpty() ? name : path;
		const int dot = probe.lastIndexOf( QLatin1Char( '.' ) );
		if ( dot < 0 )
			continue;
		const QString ext = probe.mid( dot ).toLower();
		tally[ext] = tally.value( ext ) + 1;
		if ( ext == QStringLiteral( ".hkx" ) && firstHkx.isEmpty() )
			firstHkx = probe;
	}
}

/*
 *  (4) THE BIND-POSE SNAPSHOT
 */

QVector<QByteArray> wwSnapshot( const Scene * sc )
{
	QVector<QByteArray> out;
	if ( !sc )
		return out;
	for ( Node * n : sc->getNodes() ) {
		if ( !n ) {
			out.append( QByteArray() );
			continue;
		}
		const Transform t = n->localTrans();
		out.append( QByteArray( reinterpret_cast<const char *>( &t ), int( sizeof( Transform ) ) ) );
	}
	return out;
}

int wwDiffCount( const QVector<QByteArray> & a, const QVector<QByteArray> & b )
{
	int n = 0;
	const int c = qMin( a.count(), b.count() );
	for ( int i = 0; i < c; i++ ) {
		if ( a.at( i ) != b.at( i ) )
			n++;
	}
	return n + qAbs( a.count() - b.count() );
}

void wwStepTo( NifSkope * skope, Scene * sc, float t )
{
	if ( !skope->getGLView() )
		return;
	skope->getGLView()->setSceneTime( t );
	skope->getGLView()->update();
	qApp->processEvents();
	qApp->processEvents();
	if ( sc )
		sc->transform( sc->view, t );
}

void wwFinish( NifSkope * skope, WwFtState * st )
{
	QFile logf( QApplication::applicationDirPath() + "/ww_filestab_test.log" );
	if ( logf.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
		QTextStream log( &logf );
		log << st->text;
		log << st->checks << " checks, " << st->fails << " failures\n";
		log << ( st->fails == 0 ? "PASS" : "FAIL" ) << "\ndone\n";
		logf.close();
	}
	if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
		n->undoStack->setClean();
	skope->setWindowModified( false );
	delete st;
	QTimer::singleShot( 0, qApp, &QApplication::quit );
}

// ------------------------------------------------------------------ gates ---

void wwGateStrings( NifSkope * skope, WwFtState & st )
{
	wwSay( st, QStringLiteral( "--- (1) the Files page's user-visible text ---" ) );
	const QStringList hits = wwScanFilesPage( skope );
	for ( const QString & h : hits )
		wwSay( st, QStringLiteral( "    still says NIF -> " ) + h );
	wwSay( st, QStringLiteral( "  %1 string(s) in the page still say NIF" ).arg( hits.count() ) );
	wwCheck( st, QStringLiteral( "(1) 0 \"NIF\" strings in the Files page" ), hits.isEmpty() );

	// FLOOR: put one back and prove the scan sees it.
	auto * filter = skope->findChild<QLineEdit *>( QStringLiteral( "LoadedNifsFilter" ) );
	if ( !filter ) {
		wwCheck( st, QStringLiteral( "(1 floor) the loaded-files filter was found" ), false );
		return;
	}
	const QString wasPlaceholder = filter->placeholderText();
	filter->setPlaceholderText( QStringLiteral( "Search loaded NIFs\xE2\x80\xA6" ) );
	const QStringList seeded = wwScanFilesPage( skope );
	wwSay( st, QStringLiteral( "  with one offender seeded: %1 hit(s)" ).arg( seeded.count() ) );
	wwCheck( st, QStringLiteral( "(1 floor) the scan FINDS a seeded \"NIF\" string" ),
			 seeded.count() == hits.count() + 1 );
	filter->setPlaceholderText( wasPlaceholder );
	wwCheck( st, QStringLiteral( "(1 floor) and the seed is gone again" ),
			 wwScanFilesPage( skope ).count() == hits.count() );
}

void wwGateTree( NifSkope * skope, WwFtState & st )
{
	wwSay( st, QStringLiteral( "--- (2) what the Files tree lists ---" ) );
	auto * view = skope->findChild<QTreeView *>( QStringLiteral( "bsaView" ) );
	if ( !view || !view->model() ) {
		wwCheck( st, QStringLiteral( "(2) the Files tree has a model" ), false );
		return;
	}
	QHash<QString, int> tally;
	QString firstHkx;
	wwCountExtensions( view->model(), QModelIndex(), tally, firstHkx, 0 );

	QStringList shown;
	for ( const QString & ext : wwFilesTabExtensions() )
		shown << QStringLiteral( "%1 %2" ).arg( ext ).arg( tally.value( ext ) );
	wwSay( st, QStringLiteral( "  " ) + shown.join( QStringLiteral( " | " ) ) );
	if ( !firstHkx.isEmpty() )
		wwSay( st, QStringLiteral( "  first .hkx: " ) + firstHkx );

	wwCheck( st, QStringLiteral( "(2 floor) the walk reaches leaves at all (.nif > 0)" ),
			 tally.value( QStringLiteral( ".nif" ) ) > 0 );
	wwCheck( st, QStringLiteral( "(2) at least one .hkx is listed" ),
			 tally.value( QStringLiteral( ".hkx" ) ) > 0 );
	wwCheck( st, QStringLiteral( "(2) at least one .btr is listed" ),
			 tally.value( QStringLiteral( ".btr" ) ) > 0 );
}

void wwGateOpenAndUnload( NifSkope * skope, WwFtState & st )
{
	wwSay( st, QStringLiteral( "--- (3) opening an .hkx from the tree, (4) unloading it ---" ) );
	Scene * sc = skope->getGLView() ? skope->getGLView()->getScene() : nullptr;
	if ( !sc || !sc->hkx ) {
		wwCheck( st, QStringLiteral( "(3) the scene has a Havok playback" ), false );
		return;
	}
	if ( st.clipPath.isEmpty() || !QFileInfo::exists( st.clipPath ) ) {
		wwCheck( st, QStringLiteral( "(3) WW_FILESTAB_CLIP names a clip that exists" ), false );
		return;
	}

	const QVector<QByteArray> before = wwSnapshot( sc );
	wwSay( st, QStringLiteral( "  the model offers %1 nodes" ).arg( before.count() ) );
	wwCheck( st, QStringLiteral( "(3) a model is open to play it on" ), before.count() > 0 );

	const int clipsBefore = sc->hkx->count();
#ifdef WW_FILESTAB_HOOKUP
	skope->wwFilesTabOpenRow( st.clipPath );		// the tree's own double-click path
#else
	/* The seam that puts a row in the browser's model and opens it lives in
	 * src/nifskope.cpp, which another lane owns while this one runs; it is
	 * applied by scratchpad/filestab_20260910/hookup.py. Until then this gate
	 * SKIPS BY NAME rather than passing on a route it did not take
	 * (ww-anchored-hookup). */
	wwSay( st, QStringLiteral( "SKIP (3): the tree open seam needs hook-up "
							   "scratchpad/filestab_20260910/hookup.py" ) );
	wwCheck( st, QStringLiteral( "(3) the hook-up is applied" ), false );
	return;
#endif
	qApp->processEvents();

	const QString words = sc->hkx->summary();
	wwSay( st, QStringLiteral( "  summary: " ) + words );
	wwCheck( st, QStringLiteral( "(3) the tree row added a clip" ),
			 sc->hkx->count() == clipsBefore + 1 );

	const QString clipName = sc->hkx->activeName();
	wwSay( st, QStringLiteral( "  active clip: \"%1\"" ).arg( clipName ) );
	wwCheck( st, QStringLiteral( "(3) and it became the playing sequence" ),
			 !clipName.isEmpty() );

	// the pre-registered 78 / 17 / 4, read out of the sentence the panel shows
	const QString expPlay = QStringLiteral( "%1 of " ).arg( st.expMatched );
	const QString expMiss = QStringLiteral( "%1 have no node" ).arg( st.expUnmatched );
	const QString expCase = QStringLiteral( "%1 matched only by ignoring case" ).arg( st.expFolded );
	wwCheck( st, QStringLiteral( "(3) the summary says %1 bones play" ).arg( st.expMatched ),
			 words.contains( expPlay ) );
	wwCheck( st, QStringLiteral( "(3) ...%1 have no node, NAMED" ).arg( st.expUnmatched ),
			 words.contains( expMiss ) );
	wwCheck( st, QStringLiteral( "(3) ...%1 matched only by case" ).arg( st.expFolded ),
			 words.contains( expCase ) );

	// --- (4) the pose really landed, then unload restores it byte for byte ---
	const float mid = 0.5f * ( sc->timeMin() + sc->timeMax() );
	wwStepTo( skope, sc, mid );
	const QVector<QByteArray> posed = wwSnapshot( sc );
	const int moved = wwDiffCount( before, posed );
	wwSay( st, QStringLiteral( "  at t=%1: %2 of %3 nodes differ from the bind pose" )
		.arg( double( mid ), 0, 'f', 3 ).arg( moved ).arg( before.count() ) );
	wwCheck( st, QStringLiteral( "(4 floor) the clip actually posed the rig" ), moved > 0 );

	const bool gone = wwFilesTabUnloadAnimation( skope->getGLView(), clipName );
	qApp->processEvents();
	wwCheck( st, QStringLiteral( "(4) the Loaded-files unload action removed it" ), gone );
	wwCheck( st, QStringLiteral( "(4) ...and the animations list is back to %1" ).arg( clipsBefore ),
			 sc->hkx->count() == clipsBefore );

	const QVector<QByteArray> restored = wwSnapshot( sc );
	const int stillDiff = wwDiffCount( before, restored );
	wwSay( st, QStringLiteral( "  after unload: %1 of %2 nodes differ" )
		.arg( stillDiff ).arg( before.count() ) );
	/* NAME them (lane BUILD9, 2026-09-10). A count cannot be acted on: the
	 * first run of this gate said "1 of 139 nodes differ" and there was no way
	 * to tell a bone the clip moved from the scene root the transport itself
	 * touches. The names cost one loop and turn a red gate into a lead. */
	if ( stillDiff > 0 ) {
		QStringList offenders;
		const QVector<Node *> & nodes = sc->getNodes();
		const int c = qMin( qMin( before.count(), restored.count() ), int( nodes.count() ) );
		for ( int i = 0; i < c; i++ ) {
			if ( before.at( i ) != restored.at( i ) )
				offenders << ( nodes.at( i ) ? nodes.at( i )->getName() : QStringLiteral( "<null>" ) );
		}
		wwSay( st, QStringLiteral( "  nodes that did NOT restore: %1" )
			.arg( offenders.join( QStringLiteral( ", " ) ) ) );
	}
	wwCheck( st, QStringLiteral( "(4) unload restores the bind pose BYTE-identically" ),
			 stillDiff == 0 );
}

void wwGatePanelStyle( NifSkope * skope, WwFtState & st )
{
	wwSay( st, QStringLiteral( "--- (5) panel style (nifskope-ww-panel-style) ---" ) );
	auto * page = skope->findChild<QSplitter *>( QStringLiteral( "NifBrowserSplitter" ) );
	if ( !page ) {
		wwCheck( st, QStringLiteral( "(5) the Files page was found" ), false );
		return;
	}

	const int groupBoxes = page->findChildren<QGroupBox *>().count();
	wwCheck( st, QStringLiteral( "(5) no QGroupBox on the page (0)" ), groupBoxes == 0 );

	const QList<QToolButton *> tools = page->findChildren<QToolButton *>();
	int untipped = 0;
	QStringList untippedNames;
	for ( QToolButton * b : tools ) {
		if ( b->toolTip().trimmed().isEmpty() || b->accessibleName().trimmed().isEmpty() ) {
			untipped++;
			/* NAME the offender (lane BUILD9, 2026-09-10). The first run of this
			 * gate said "2 without a tooltip" and the count alone could not say
			 * whether they were controls this lane put on the row or widgets Qt
			 * creates inside a QLineEdit with setClearButtonEnabled(true). The
			 * class and object name settle it in the log. */
			untippedNames << QStringLiteral( "%1/%2" )
				.arg( QString::fromLatin1( b->metaObject()->className() ),
					  b->objectName().isEmpty() ? QStringLiteral( "<unnamed>" ) : b->objectName() );
		}
	}
	wwSay( st, QStringLiteral( "  %1 tool buttons, %2 without a tooltip or accessible name" )
		.arg( tools.count() ).arg( untipped ) );
	if ( untipped > 0 )
		wwSay( st, QStringLiteral( "  untipped: %1" ).arg( untippedNames.join( QStringLiteral( ", " ) ) ) );
	wwCheck( st, QStringLiteral( "(5) at least 4 tool buttons on the row" ), tools.count() >= 4 );
	wwCheck( st, QStringLiteral( "(5) every tool button explains itself (0 without)" ),
			 untipped == 0 );

	/* FLOOR: blank one and watch the count move.
	 *
	 * The victim must be a button that is CURRENTLY TIPPED (lane BUILD9,
	 * 2026-09-10). It used to be tools.first() unconditionally, and when that
	 * one was already untipped the count could not rise by one, so the floor
	 * went red for a reason that had nothing to do with whether blanking is
	 * counted -- an instrument that cannot fire, reported as a failure. */
	QToolButton * victim = nullptr;
	for ( QToolButton * b : tools ) {
		if ( !b->toolTip().trimmed().isEmpty() && !b->accessibleName().trimmed().isEmpty() ) {
			victim = b;
			break;
		}
	}
	if ( victim ) {
		const QString wasTip = victim->toolTip();
		victim->setToolTip( QString() );
		int again = 0;
		for ( QToolButton * b : tools ) {
			if ( b->toolTip().trimmed().isEmpty() || b->accessibleName().trimmed().isEmpty() )
				again++;
		}
		victim->setToolTip( wasTip );
		wwCheck( st, QStringLiteral( "(5 floor) blanking one tooltip is COUNTED" ),
				 again == untipped + 1 );
	}

	int dashed = 0;
	for ( QCheckBox * c : page->findChildren<QCheckBox *>() ) {
		if ( c->text().contains( QStringLiteral( " - " ) ) )
			dashed++;
	}
	wwCheck( st, QStringLiteral( "(5) no check-box label explains itself after a dash (0)" ),
			 dashed == 0 );

	int placeholderless = 0;
	const QList<QLineEdit *> edits = page->findChildren<QLineEdit *>();
	for ( QLineEdit * e : edits ) {
		if ( e->placeholderText().trimmed().isEmpty() )
			placeholderless++;
	}
	wwSay( st, QStringLiteral( "  %1 search fields, %2 without a placeholder" )
		.arg( edits.count() ).arg( placeholderless ) );
	wwCheck( st, QStringLiteral( "(5) both search fields carry a placeholder" ),
			 edits.count() >= 2 && placeholderless == 0 );

	// The two halves are DISTINCT ROWS, measured as geometry, not as layout class.
	auto * tree = skope->findChild<QTreeView *>( QStringLiteral( "bsaView" ) );
	auto * loaded = skope->findChild<QTreeView *>( QStringLiteral( "LoadedNifsView" ) );
	if ( tree && loaded ) {
		const int treeBottom = tree->mapTo( page, QPoint( 0, tree->height() ) ).y();
		const int loadedTop = loaded->mapTo( page, QPoint( 0, 0 ) ).y();
		wwSay( st, QStringLiteral( "  browser bottom %1, loaded top %2" )
			.arg( treeBottom ).arg( loadedTop ) );
		wwCheck( st, QStringLiteral( "(5) the loaded-files list is BELOW the browser" ),
				 loadedTop >= treeBottom );
	} else {
		wwCheck( st, QStringLiteral( "(5) both views were found" ), false );
	}
}

void wwGateRefusal( NifSkope * skope, WwFtState & st )
{
	wwSay( st, QStringLiteral( "--- (6) with nothing open, an .hkx refuses in words ---" ) );
	Scene * sc = skope->getGLView() ? skope->getGLView()->getScene() : nullptr;
	if ( !sc || !sc->hkx ) {
		wwCheck( st, QStringLiteral( "(6) the scene has a Havok playback" ), false );
		return;
	}
	// create the condition deliberately: no named nodes = no model to play on
	sc->hkx->unloadAll();
	sc->clear();
	qApp->processEvents();
	const int nodes = HkxPlayback::mapNames( sc, QStringList() ).nodesInNif;
	wwSay( st, QStringLiteral( "  the scene now offers %1 named nodes" ).arg( nodes ) );
	wwCheck( st, QStringLiteral( "(6 floor) the scene really is empty" ), nodes == 0 );

	const int clipsBefore = sc->hkx->count();
	const WwAnimOpen r = wwFilesTabOpenAnimation( skope->getGLView(), st.clipPath );
	wwSay( st, QStringLiteral( "  refusal: " ) + r.sentence );
	wwCheck( st, QStringLiteral( "(6) it refuses" ), !r.loaded );
	wwCheck( st, QStringLiteral( "(6) the refusal is IN WORDS and names the reason" ),
			 r.sentence.contains( QStringLiteral( "nothing" ) )
			 && r.sentence.contains( QStringLiteral( "open the rigged NIF" ), Qt::CaseInsensitive ) );
	wwCheck( st, QStringLiteral( "(6) and loads nothing" ), sc->hkx->count() == clipsBefore );
}

void wwGrab( NifSkope * skope, WwFtState & st )
{
	if ( st.shot.isEmpty() )
		return;
	auto * dock = skope->findChild<QDockWidget *>( QStringLiteral( "LeftColumnDock" ) );
	if ( !dock ) {
		wwCheck( st, QStringLiteral( "(shot) the left dock was found" ), false );
		return;
	}
	const bool saved = dock->grab().save( st.shot );
	wwSay( st, QStringLiteral( "  dock grab -> %1 (%2)" )
		.arg( st.shot, saved ? QStringLiteral( "written" ) : QStringLiteral( "REFUSED" ) ) );
	wwCheck( st, QStringLiteral( "(shot) the dock grab was written" ), saved );
}

} // namespace

void wwFilesTabHarness( NifSkope * skope )
{
	if ( !skope || !qEnvironmentVariableIsSet( "WW_FILESTAB_TEST" ) )
		return;

	auto * st = new WwFtState;
	st->clipPath = qEnvironmentVariable( "WW_FILESTAB_CLIP" );
	st->shot = qEnvironmentVariable( "WW_FILESTAB_SHOT" );
	const QStringList exp = qEnvironmentVariable( "WW_FILESTAB_EXPECT",
		QStringLiteral( "78,17,4" ) ).split( QLatin1Char( ',' ) );
	if ( exp.count() == 3 ) {
		st->expMatched = exp.at( 0 ).toInt();
		st->expUnmatched = exp.at( 1 ).toInt();
		st->expFolded = exp.at( 2 ).toInt();
	}

	/* A HARNESS FORCES THE STATE IT MEASURES (nifskope-ww-build-verify).
	 *
	 * The tree census would otherwise be a measurement of whatever bungo has in
	 * Settings > Resources on the day, which is not a gate. The roots are given
	 * on the command line and written into the game manager before the first
	 * populate; his own settings are never saved over, because this only calls
	 * update_folders() in memory.
	 */
	const QString roots = qEnvironmentVariable( "WW_FILESTAB_RESOURCES" );
	if ( !roots.isEmpty() ) {
		const QStringList list = roots.split( QLatin1Char( ';' ), Qt::SkipEmptyParts );
		Game::GameManager::update_folders( Game::FALLOUT_4, list );
		Game::GameManager::update_status( Game::FALLOUT_4, true );
	}

	QObject::connect( skope, &NifSkope::completeLoading, skope, [skope, st]( bool ok, QString & ) {
		// 1.5 s, like the other WW harnesses: the scene is built on the load
		// signal but the first paint, and so the first transform walk, is not.
		QTimer::singleShot( 1500, skope, [skope, st, ok]() {
			if ( st->stage != 0 )
				return;
			st->stage = 1;
			wwCheck( *st, QStringLiteral( "the rigged NIF loaded" ), ok );

			// show the Files page and build its tree from the forced roots
#ifdef WW_FILESTAB_HOOKUP
			skope->wwFilesTabShowAndRebuild();
#else
			wwSay( *st, QStringLiteral( "SKIP: the page-and-rebuild seam needs hook-up "
										"scratchpad/filestab_20260910/hookup.py" ) );
#endif
			qApp->processEvents();

			wwGateStrings( skope, *st );
			wwGateTree( skope, *st );
			wwGatePanelStyle( skope, *st );
			wwGrab( skope, *st );
			wwGateOpenAndUnload( skope, *st );
			wwGateRefusal( skope, *st );		// LAST: it empties the scene
			wwFinish( skope, st );
		} );
	} );
}
