/* WW_ARCHLOCK_TEST: lane ARCHLOCK1's refuter, run inside the real application
   against the real widget tree.

   WHAT IT MEASURES

     The archive index's lock is ONE static QReadWriteLock in Recursive mode
     (src/gamemanager.h, archiveLock()). Recursive grants read-after-read and
     write-after-write to the same thread, and never a read -> write UPGRADE:
     lockForWrite waits for the reader count to reach zero, and the reader it
     waits for is the calling thread itself.

     GameResources::get_file and ::find_file took the READ lock and then, on a
     miss, recursed `return parent->...` while STILL HOLDING IT. When the parent
     (the shared per-game index) had no BA2File yet, its own get_file called
     init_archives(), whose first line is a QWriteLocker on that same lock. The
     GUI thread then slept on itself forever, and nothing could wake it
     (bungo 2026-09-17: "When I click on anything from 'files', it freezes
     nifskope").

     THE CONDITION HAS TWO HALVES, and both are FORCED here rather than hoped
     for, because either one missing turns the gate green on a broken build:

       1. the document's own GameResources has an EMPTY data path, so it never
          builds an index of its own and every lookup falls through to the
          parent. A loose .nif given on argv does NOT do this -- NifModel::load
          derives a data path from the file name and builds the parent from its
          own init_archives() with no lock held, which is why the first probe of
          this bug could not reproduce it. The Files tab's CONFIGURED-RESOURCE
          row does: it loads the bytes through a QBuffer, the model never sees a
          file name, and the data path comes out empty. That is bungo's route.

       2. the parent index is NOT BUILT YET. A fresh process, nothing opened.
          The harness closes it explicitly and then CHECKS it is null, twice --
          before and after the Files tree is indexed -- because the whole gate
          is meaningless if something already warmed it.

   THE LEGS (tests/spells/gamemanager_archlock.sh is the driver)

     (a) the refuter. On release/NifSkope.before_archlock1.exe this harness
         HANGS at the row marked `opening` below and the spell's watchdog kills
         it; on the fixed exe it opens the file, paints one frame and exits 0.
         The log is flushed after every line for exactly that reason: the rung's
         log stops at `opening`, which is what makes "it hung HERE" a
         measurement instead of a story.
     (c) find_file is covered by the same run: after the paint the harness asks
         the document -- the one with the empty data path -- to RESOLVE a
         texture through findResourceFile(), which is find_file's own route, and
         a miss is a failure. A fix that made every lookup answer "not found"
         would pass (a) and fail this.

     FLOORS
       - the parent index really was unbuilt when the row was opened (checked
         twice, and the check is the same pointer the deadlock depended on)
       - the document really did end up with an EMPTY data path (read back off
         GameResources::dataPaths, not assumed from the route)
       - the model really loaded (block count > 0): an open that failed early
         would never reach a material lookup and would pass by doing nothing
       - the texture lookup really resolves: see (c)

   ENVIRONMENT (tests/spells/gamemanager_archlock.sh sets all of them)
     WW_ARCHLOCK_TEST=1            arm the harness
     WW_ARCHLOCK_RESOURCES=a;b;c   resource roots FORCED into the game manager,
                                   in memory; the user's own Settings >
                                   Resources are never read and never written
     WW_ARCHLOCK_NIF=meshes/...    the CONFIGURED-RESOURCE path to open, as the
                                   first document of the process
     WW_ARCHLOCK_TEXTURE=tex.dds   a texture that lives only in those archives
     WW_ARCHLOCK_SHOT=<png>        grab the viewport after the paint
     WW_ARCHLOCK_WINDOW_SHOT=<png> grab the whole window, Files tab and all
   Log: release/ww_archlock_test.log

   Lane ARCHLOCK1, 2026-09-17. */

#include "nifskope.h"
#include "glview.h"
#include "gamemanager.h"
#include "model/nifmodel.h"

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QPixmap>
#include <QTextStream>
#include <QTimer>
#include <QUndoStack>

namespace
{

struct WwAlState
{
	int checks = 0;
	int fails = 0;
	QString logPath;
	QString nifPath;
	QString texture;
	QString shot;
	QString windowShot;
	bool done = false;
};

//! Every line goes to disk AS IT IS WRITTEN. The rung hangs inside this
//! harness, so a log buffered to the end would be an empty file and the gate
//! would have no evidence of WHERE it stopped.
void wwSay( WwAlState & st, const QString & line )
{
	QFile f( st.logPath );
	if ( f.open( QIODevice::Append | QIODevice::Text ) ) {
		QTextStream out( &f );
		out << line << "\n";
		out.flush();
		f.close();
	}
}

void wwCheck( WwAlState & st, const QString & what, bool pass )
{
	st.checks++;
	if ( !pass )
		st.fails++;
	wwSay( st, ( pass ? QStringLiteral( "  ok   " ) : QStringLiteral( "  FAIL " ) ) + what );
}

void wwFinish( NifSkope * skope, WwAlState * st )
{
	if ( st->done )
		return;
	st->done = true;
	wwSay( *st, QStringLiteral( "%1 checks, %2 failures" ).arg( st->checks ).arg( st->fails ) );
	wwSay( *st, st->fails == 0 ? QStringLiteral( "PASS" ) : QStringLiteral( "FAIL" ) );
	wwSay( *st, QStringLiteral( "done" ) );
	if ( skope ) {
		if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
			n->undoStack->setClean();
		skope->setWindowModified( false );
	}
	delete st;
	/* EXIT EXPLICITLY; do not leave it to qApp->quit().
	 *
	 * Qt 6.11's QGuiApplicationPrivate::quit() closes every top-level window
	 * first and exits the loop only if they all go (the same mechanism the
	 * headless guard in NifSkope::closeEvent is written against). THIS route
	 * leaves a SECOND, invisible NifSkope window behind -- the configured
	 * resource open builds an off-screen document container beside the visible
	 * window -- and in that state the quit never reaches the exit. Measured
	 * 2026-09-17: the log ended `done` after 3 s and the process was still in
	 * its main event loop 45 s later with the loaded window on screen, so the
	 * run only ever ended when the gate's watchdog killed it -- which is the
	 * SAME ending the refuter rung is supposed to be the only one to have.
	 * Nothing was vetoing a close: a WM_CLOSE posted to that window from
	 * outside exited the process at once, so a close IS an ending this state
	 * accepts -- and one the harness can take itself, which is what lets leg
	 * (a) gate on `rc 0` and mean it.
	 *
	 * The exit is taken the way the external WM_CLOSE took it: this window is
	 * closed, the headless guard in NifSkope::closeEvent answers its questions
	 * and takes the invisible container down with it, and the last visible
	 * window closing ends the loop through quitOnLastWindowClosed. The blunter
	 * pair -- closeAllWindows() then QCoreApplication::exit(0) -- was tried
	 * first and SEGFAULTED on the way out (rc 139, 2026-09-17 03:36), after
	 * the log had already written PASS: exit(0) leaves the loop while the
	 * closes it just started are still pending. A crash after a pass is still
	 * a red leg (a), so it is not used. */
	QTimer::singleShot( 0, qApp, [skope]() {
		if ( skope )
			skope->close();
		/* AND THEN END THE LOOP OURSELVES, 200 ms later, through the loop
		 * rather than from inside this slot: the close above is graceful (it
		 * runs saveUi() and takes the workspace's invisible container with
		 * it) but it does NOT end the run -- measured, every NifSkope window
		 * was gone and the process was still in exec(). The delay is what the
		 * pending deleteLater()s need; exit(0) fired in the same turn as the
		 * closes segfaulted on the way out. */
		QTimer::singleShot( 200, qApp, []() { QCoreApplication::exit( 0 ); } );
	} );
}

void wwRun( NifSkope * skope, WwAlState * st )
{
	using namespace Game;

	wwSay( *st, QStringLiteral( "--- WW_ARCHLOCK_TEST: the configured-resource route, fresh process ---" ) );

	/* (1) FORCE THE STATE MEASURED. The roots go into the game manager in
	 * memory only -- insert_folders / insert_status touch `dataPaths` and
	 * `gameStatus`, nothing is saved -- so the gate is not a measurement of
	 * whatever bungo has in Settings > Resources today.
	 *
	 * OTHER as well as FALLOUT_4: the Files tree indexes the folders of
	 * get_game(nif), and the empty document a fresh NifSkope starts with has
	 * bsVersion 0, which is OTHER. The row carries FALLOUT_4 explicitly, so the
	 * parent the loaded document falls through to is archives[FALLOUT_4]. The
	 * other-games fallback is turned OFF so the two lists cannot be merged and
	 * make the parent's dataPaths non-empty for the wrong reason. */
	const QStringList roots =
		qEnvironmentVariable( "WW_ARCHLOCK_RESOURCES" ).split( QLatin1Char( ';' ), Qt::SkipEmptyParts );
	wwCheck( *st, QStringLiteral( "(setup) WW_ARCHLOCK_RESOURCES names at least one root" ),
			 !roots.isEmpty() );
	GameManager::update_other_games_fallback( false );
	GameManager::update_folders( OTHER, roots );
	GameManager::update_status( OTHER, true );
	GameManager::update_folders( FALLOUT_4, roots );
	GameManager::update_status( FALLOUT_4, true );
	for ( const QString & r : roots )
		wwSay( *st, QStringLiteral( "  root: " ) + r );

	/* (2) FORCE THE PRECONDITION and then MEASURE it. close_archives() takes the
	 * WRITE lock with nothing else held, which is legal from here and is exactly
	 * what "a fresh process" means for this object. */
	GameManager::GameResources & parent = GameManager::getGameResources( FALLOUT_4 );
	parent.close_archives();
	wwCheck( *st, QStringLiteral( "(floor) the shared Fallout 4 index is NOT built yet" ),
			 parent.ba2File == nullptr );

	// (3) the Files page, indexed from the forced roots. This builds the tab's
	// OWN BA2File (currentArchive), which is a different object from the game
	// manager's -- the next check proves it did not warm the one under test.
	QElapsedTimer indexTimer;
	indexTimer.start();
	skope->wwFilesTabShowAndRebuild();
	qApp->processEvents();
	wwSay( *st, QStringLiteral( "  Files tree indexed in %1 ms" ).arg( indexTimer.elapsed() ) );
	wwCheck( *st, QStringLiteral( "(floor) ...and indexing the tree did NOT build it either" ),
			 parent.ba2File == nullptr );

	/* (4) THE OPEN. On the rung the process stops HERE and never returns: the
	 * paint that follows the load reaches BSShaderLightingProperty::setMaterial
	 * -> Material::openFile -> NifModel::getResourceFile -> get_file, which took
	 * the read lock, missed in the document's (empty) index, and recursed into
	 * the parent's init_archives() and its write lock. The log's last line is
	 * the one below. */
	wwSay( *st, QStringLiteral( "opening %1 as a configured resource "
								"(the rung stops here and never writes another line)" ).arg( st->nifPath ) );
	QElapsedTimer openTimer;
	openTimer.start();
	skope->wwFilesTabOpenConfiguredRow( int( FALLOUT_4 ), st->nifPath );
	qApp->processEvents();
	wwSay( *st, QStringLiteral( "  the open returned after %1 ms" ).arg( openTimer.elapsed() ) );

	NifModel * doc = skope->getNifModel();
	wwCheck( *st, QStringLiteral( "(open) the configured row loaded a model" ),
			 doc && doc->getBlockCount() > 0 );
	if ( doc )
		wwSay( *st, QStringLiteral( "  the document holds %1 block(s)" ).arg( doc->getBlockCount() ) );

	/* (5) THE CONDITION, READ BACK. "The document has an empty data path" is the
	 * half of the bug the route is supposed to create; reading it off the object
	 * is the only way to know the route still creates it. */
	if ( doc ) {
		GameManager::GameResources & r = doc->getGameResources();
		wwSay( *st, QStringLiteral( "  the document's own data paths: [%1]" )
			.arg( r.dataPaths.join( QStringLiteral( ", " ) ) ) );
		wwCheck( *st, QStringLiteral( "(floor) the loaded document's data path is EMPTY" ),
				 r.dataPaths.isEmpty() );
		wwCheck( *st, QStringLiteral( "(floor) ...and it falls through to the shared index" ),
				 r.parent == &parent );
	}

	/* (6) ONE FRAME. The lookup is not on the load path, it is on the PAINT
	 * path, so a harness that stopped at the load would never touch the lock.
	 * GLView is a QOpenGLWindow, not a widget: it has no repaint(), and
	 * grabFramebuffer() reads the CURRENT buffer without painting a new one.
	 * A fresh frame is pumped through the event loop instead, which is the
	 * pattern the render probes in src/nifskope_ui.cpp already use. The paint
	 * still runs on THIS thread -- the one that would be holding the read
	 * lock -- which is the only property this leg needs. */
	QElapsedTimer paintTimer;
	paintTimer.start();
	if ( GLView * ogl = skope->getGLView() ) {
		/* Frame the thing first, the way View > Center does: the arm is a few
		 * units across and the default camera looks at an empty grid, so a grab
		 * without this is a picture of the grid with a two-pixel sliver in it
		 * -- true, and useless as the lane's evidence that the file rendered. */
		ogl->center();
		ogl->update();
		QCoreApplication::processEvents( QEventLoop::AllEvents, 250 );
		QCoreApplication::processEvents( QEventLoop::AllEvents, 250 );
		wwSay( *st, QStringLiteral( "  one frame painted in %1 ms" ).arg( paintTimer.elapsed() ) );
		wwCheck( *st, QStringLiteral( "(a) the first frame after the open completed" ), true );
		if ( !st->shot.isEmpty() ) {
			const QImage img = ogl->grabFramebuffer();
			const bool saved = !img.isNull() && img.save( st->shot );
			wwSay( *st, QStringLiteral( "  viewport grab -> %1 (%2, %3x%4)" )
				.arg( st->shot, saved ? QStringLiteral( "written" ) : QStringLiteral( "REFUSED" ) )
				.arg( img.width() ).arg( img.height() ) );
			wwCheck( *st, QStringLiteral( "(picture) the viewport grab was written" ), saved );
		}
		/* AND THE WHOLE WINDOW. A headless WW_* run is shown at opacity 0 on a
		 * non-primary screen (nifskope-ww-render-shot), so a desktop capture of
		 * this window would photograph whatever is behind it. QWidget::grab()
		 * renders the widget tree itself, which is the only honest picture of
		 * 'the Files tab open, with the file it opened on screen'. */
		if ( !st->windowShot.isEmpty() ) {
			const QPixmap win = skope->grab();
			const bool saved = !win.isNull() && win.save( st->windowShot );
			wwSay( *st, QStringLiteral( "  window grab -> %1 (%2, %3x%4)" )
				.arg( st->windowShot, saved ? QStringLiteral( "written" ) : QStringLiteral( "REFUSED" ) )
				.arg( win.width() ).arg( win.height() ) );
			wwCheck( *st, QStringLiteral( "(picture) the window grab was written" ), saved );
		}
	} else {
		wwCheck( *st, QStringLiteral( "(a) the viewport exists" ), false );
	}

	/* (7) find_file, through the same document. get_file is what the paint took;
	 * find_file has the identical shape and the identical bug, and it is the one
	 * the texture cache uses. A "fix" that answered every lookup with a miss
	 * would sail through (a) and die here. */
	if ( doc && !st->texture.isEmpty() ) {
		QElapsedTimer findTimer;
		findTimer.start();
		const QString found = doc->findResourceFile( st->texture, "textures", ".dds" );
		wwSay( *st, QStringLiteral( "  find_file( %1 ) -> \"%2\" in %3 ms" )
			.arg( st->texture, found ).arg( findTimer.elapsed() ) );
		wwCheck( *st, QStringLiteral( "(c) find_file answers through the parent, and FINDS it" ),
				 !found.isEmpty() );

		QByteArray bytes;
		QElapsedTimer getTimer;
		getTimer.start();
		const bool got = doc->getResourceFile( bytes, st->texture, "textures", ".dds" );
		wwSay( *st, QStringLiteral( "  get_file( %1 ) -> %2 byte(s) in %3 ms" )
			.arg( st->texture ).arg( bytes.size() ).arg( getTimer.elapsed() ) );
		wwCheck( *st, QStringLiteral( "(a) get_file answers through the parent, with bytes" ),
				 got && bytes.size() > 0 );
	}

	// and the index the whole thing turned on really did get built in the end
	wwCheck( *st, QStringLiteral( "(floor) the shared index is built NOW" ),
			 parent.ba2File != nullptr );

	wwFinish( skope, st );
}

} // namespace

void wwArchLockHarness( NifSkope * skope )
{
	if ( !skope || !qEnvironmentVariableIsSet( "WW_ARCHLOCK_TEST" ) )
		return;

	auto * st = new WwAlState;
	st->logPath = QApplication::applicationDirPath() + QStringLiteral( "/ww_archlock_test.log" );
	st->nifPath = qEnvironmentVariable( "WW_ARCHLOCK_NIF" );
	st->texture = qEnvironmentVariable( "WW_ARCHLOCK_TEXTURE" );
	st->shot = qEnvironmentVariable( "WW_ARCHLOCK_SHOT" );
	st->windowShot = qEnvironmentVariable( "WW_ARCHLOCK_WINDOW_SHOT" );
	QFile::remove( st->logPath );

	/* No document is opened on the command line, so there is no completeLoading
	 * to hang the run on: the timer starts from the window being built. 1500 ms,
	 * like the other WW harnesses, so the first paint and the first transform
	 * walk are behind us before anything is measured. */
	QTimer::singleShot( 1500, skope, [skope, st]() {
		wwRun( skope, st );
	} );
}
