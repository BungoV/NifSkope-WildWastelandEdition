/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

/* ---------------------------------------------------------------------------
 * WW_MODPICK_TEST=<report path> -- lane MODPICK1's GUI gate, run INSIDE the
 * running application. It drives the Cell workspace's Mods band through the
 * same calls a click makes:
 *
 *   stage 1 (after the cell opens)  the list is read row by row (source, rows,
 *           gone rows greyed with their note, the master locked), a picture of
 *           the band is saved (WW_MODPICK_SHOT), the plugin named by
 *           WW_MODPICK_UNTICK is unticked and Apply is clicked -- which saves
 *           the choice and re-opens the cell;
 *   stage 2 (after the re-open)     the row reads unticked, the view's note
 *           reads "Mods differ from the far LOD bake", and a second picture of
 *           the band plus the view's own framebuffer are saved.
 *
 * WW_CELL_REFDUMP is copied to <refdump>.stage1.txt after stage 1, so the
 * script counts the near trees before and after from the builder's own
 * reference model. The run needs a scratch WW_SETTINGS_SCOPE: Apply writes it.
 * --------------------------------------------------------------------------- */

#include "cellmodpick.h"
#include "cellmodpickbox.h"
#include "cellworkspace.h"
#include "glview.h"
#include "nifskope.h"

#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QPixmap>
#include <QTextStream>
#include <QTimer>

namespace {

QStringList g_lines;
int g_fail = 0;

void row( bool ok, const QString & what, const QString & detail = QString() )
{
	g_lines << QStringLiteral( "%1  %2%3" ).arg( ok ? QStringLiteral( "PASS" ) : QStringLiteral( "FAIL" ), what,
		detail.isEmpty() ? QString() : QStringLiteral( "   [" ) + detail + QLatin1Char( ']' ) );
	if ( !ok )
		g_fail++;
}

void writeReport( const QString & path )
{
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Text ) )
		return;
	QTextStream s( &f );
	s << "# WW_MODPICK_TEST -- lane MODPICK1\n";
	for ( const QString & l : std::as_const( g_lines ) )
		s << l << "\n";
	s << "rows " << g_lines.size() << " failures " << g_fail << "\n";
}

//! The band as the user sees it: the whole Cell dock (the Mods band inside it), saved to `path`.
QSize shotOfBand( NifSkope * skope, const QString & path )
{
	QDockWidget * dock = skope->findChild<QDockWidget *>( QStringLiteral( "CellWorkspaceDock" ) );
	QWidget * w = dock ? static_cast<QWidget *>( dock ) : nullptr;
	if ( !w || path.isEmpty() )
		return QSize();
	qApp->processEvents();
	const QPixmap p = w->grab();
	QDir().mkpath( QFileInfo( path ).absolutePath() );
	return p.save( path ) ? p.size() : QSize();
}

QString withSuffix( const QString & path, const QString & tag )
{
	const QFileInfo fi( path );
	return fi.absoluteDir().filePath( fi.completeBaseName() + tag + QLatin1Char( '.' ) + fi.suffix() );
}

void stageOne( NifSkope * skope, const QString & report )
{
	CellWorkspacePanel * panel = skope->findChild<CellWorkspacePanel *>();
	CellModPickBox * box = panel ? panel->modPicker() : nullptr;
	row( box, QStringLiteral( "the Cell workspace has a Mods band" ) );
	if ( !box ) {
		writeReport( report );
		QTimer::singleShot( 0, qApp, &QCoreApplication::quit );
		return;
	}
	QDockWidget * dock = skope->findChild<QDockWidget *>( QStringLiteral( "CellWorkspaceDock" ) );
	row( dock && dock->isVisible(), QStringLiteral( "the Cell dock is up" ) );
	const QString src = box->sourceText();
	row( !src.isEmpty(), QStringLiteral( "source line" ), src );
	const bool fromBake = src.contains( QStringLiteral( "far LOD bake" ) );
	const bool wantBake = qgetenv( "WW_MODPICK_EXPECT" ) != "mo2";
	row( fromBake == wantBake, wantBake ? QStringLiteral( "seeded from the shown far LOD bake" )
		: QStringLiteral( "seeded from the MO2 profile (no bake shown)" ) );
	int gone = 0, goneNoted = 0, goneTickable = 0, on = 0;
	for ( int i = 0; i < box->rowCount(); i++ ) {
		if ( box->rowPresent( i ) ) {
			on += box->rowOn( i ) ? 1 : 0;
			continue;
		}
		gone++;
		goneNoted += box->rowMod( i ).contains( QStringLiteral( "gone from disk" ) ) ? 1 : 0;
		box->setRowOn( i, true );	// a gone row refuses a tick
		goneTickable += box->rowOn( i ) ? 1 : 0;
	}
	row( box->rowCount() > 1, QStringLiteral( "rows in load order" ), QStringLiteral( "%1 rows, first %2 (%3), last %4 (%5)" )
		.arg( box->rowCount() ).arg( box->rowFile( 0 ), box->rowMod( 0 ) )
		.arg( box->rowFile( box->rowCount() - 1 ), box->rowMod( box->rowCount() - 1 ) ) );
	row( box->rowLocked( 0 ) && box->rowOn( 0 ), QStringLiteral( "the master row always loads" ), box->rowFile( 0 ) );
	row( gone == goneNoted && goneTickable == 0, QStringLiteral( "gone files greyed with a note, never ticked" ),
		QStringLiteral( "%1 gone, %2 noted, %3 took a tick" ).arg( gone ).arg( goneNoted ).arg( goneTickable ) );
	if ( qgetenv( "WW_MODPICK_EXPECT_GONE" ).toInt() > 0 )
		row( gone == qgetenv( "WW_MODPICK_EXPECT_GONE" ).toInt(), QStringLiteral( "gone rows = the files missing on disk" ),
			QStringLiteral( "%1 vs %2" ).arg( gone ).arg( QString::fromLatin1( qgetenv( "WW_MODPICK_EXPECT_GONE" ) ) ) );
	const QString loaded = cellModPickLoaded( box->worldspace() );
	const int loadedN = loaded.split( QLatin1Char( ',' ), Qt::SkipEmptyParts ).size();
	row( on == loadedN, QStringLiteral( "untouched ticks = the plugins the cell loaded" ), QStringLiteral( "%1 ticked, %2 loaded" ).arg( on ).arg( loadedN ) );
	row( !box->applyEnabled(), QStringLiteral( "Apply is off until a tick changes" ) );
	const QSize s1 = shotOfBand( skope, QString::fromLocal8Bit( qgetenv( "WW_MODPICK_SHOT" ) ) );
	row( s1.isValid(), QStringLiteral( "picture of the band" ), QStringLiteral( "%1x%2" ).arg( s1.width() ).arg( s1.height() ) );

	const QString refdump = QString::fromLocal8Bit( qgetenv( "WW_CELL_REFDUMP" ) );
	if ( !refdump.isEmpty() ) {
		QFile::remove( refdump + QStringLiteral( ".stage1.txt" ) );
		QFile::copy( refdump, refdump + QStringLiteral( ".stage1.txt" ) );
	}
	const QString untick = QString::fromLocal8Bit( qgetenv( "WW_MODPICK_UNTICK" ) ).trimmed();
	int at = -1;
	for ( int i = 0; i < box->rowCount(); i++ )
		if ( box->rowFile( i ).compare( untick, Qt::CaseInsensitive ) == 0 )
			at = i;
	row( at >= 0, QStringLiteral( "the row to untick is listed" ), untick );
	if ( at < 0 ) {
		writeReport( report );
		QTimer::singleShot( 0, qApp, &QCoreApplication::quit );
		return;
	}
	box->setRowOn( at, false );
	row( !box->rowOn( at ) && box->applyEnabled(), QStringLiteral( "unticked; Apply is live" ) );
	writeReport( report );
	box->apply();	// saves the choice and re-opens the cell (stage 2 runs on that load)
}

void stageTwo( NifSkope * skope, const QString & report )
{
	CellWorkspacePanel * panel = skope->findChild<CellWorkspacePanel *>();
	CellModPickBox * box = panel ? panel->modPicker() : nullptr;
	const QString untick = QString::fromLocal8Bit( qgetenv( "WW_MODPICK_UNTICK" ) ).trimmed();
	int at = -1;
	for ( int i = 0; box && i < box->rowCount(); i++ )
		if ( box->rowFile( i ).compare( untick, Qt::CaseInsensitive ) == 0 )
			at = i;
	row( box && at >= 0 && !box->rowOn( at ), QStringLiteral( "after the re-open the row reads unticked (the choice was saved)" ), untick );
	const QString note = cellModPickViewNote( skope->getNifModel() );
	row( note.startsWith( QStringLiteral( "Mods differ from the far LOD bake" ) ), QStringLiteral( "the view says the mods differ" ), note );
	const QString shot = QString::fromLocal8Bit( qgetenv( "WW_MODPICK_SHOT" ) );
	const QSize s2 = shotOfBand( skope, shot.isEmpty() ? QString() : withSuffix( shot, QStringLiteral( "_after" ) ) );
	row( s2.isValid(), QStringLiteral( "picture of the band after Apply" ) );
	if ( GLView * ogl = skope->getGLView() ) {
		if ( !shot.isEmpty() ) {
			ogl->update();
			qApp->processEvents();
			const QImage fb = ogl->grabFramebuffer();
			const bool ok = !fb.isNull() && fb.save( withSuffix( shot, QStringLiteral( "_view" ) ) );
			row( ok, QStringLiteral( "picture of the view after Apply" ), QStringLiteral( "%1x%2" ).arg( fb.width() ).arg( fb.height() ) );
		}
	}
	writeReport( report );
}

} // namespace

void wwCellModPickHarness( NifSkope * skope )
{
	const QString report = QString::fromLocal8Bit( qgetenv( "WW_MODPICK_TEST" ) );
	if ( report.isEmpty() || !skope )
		return;
	// a watchdog: a re-open that never completes still ends the run, with the report saying so
	QTimer::singleShot( 900000, qApp, [report]() {
		row( false, QStringLiteral( "the run finished within 15 minutes (stage 2 never ran)" ) );
		writeReport( report );
		QCoreApplication::exit( 2 );
	} );
	QObject::connect( skope, &NifSkope::completeLoading, skope, [skope, report]( bool, QString & ) {
		static int stage = 0;
		const int now = stage++;
		// after the load's own slots: the panel reads the new notes on the second sceneChanged ring
		QTimer::singleShot( 300, skope, [skope, report, now]() {
			if ( now == 0 ) {
				stageOne( skope, report );
			} else if ( now == 1 ) {
				stageTwo( skope, report );
				if ( qgetenv( "WW_MODPICK_STAY" ).isEmpty() )
					QTimer::singleShot( 0, qApp, &QCoreApplication::quit );
			}
		} );
	} );
}
