/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

/* WW_MODPICK_TEST=<report path>: an in-app test of the Cell workspace's Mods band, driven through the same
 * calls and real Qt drag events as the mouse. Stage 1 reads the list and applies WW_MODPICK_DRAG/DROP/UNTICK;
 * stage 2 checks the choice after the cell re-opens. Needs a scratch WW_SETTINGS_SCOPE (Apply writes it). */

#include "cellmodpick.h"
#include "cellmodpickbox.h"
#include "cellworkspace.h"
#include "glview.h"
#include "nifskope.h"

#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMimeData>
#include <QPixmap>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>

#include <algorithm>

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

void info( const QString & what )
{
	g_lines << QStringLiteral( "INFO  " ) + what;
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

QString env( const char * name )
{
	return QString::fromLocal8Bit( qgetenv( name ) ).trimmed();
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

QStringList orderOf( const CellModPickBox * box )
{
	QStringList o;
	for ( int i = 0; i < box->rowCount(); i++ )
		o << box->rowFile( i );
	return o;
}

//! enter, move and drop at `at` on the list's viewport, as a mouse drag would
void sendDrop( CellModPickBox * box, QMimeData * mime, const QPoint & at )
{
	QWidget * vp = box->tree()->viewport();
	const Qt::DropActions acts = Qt::CopyAction | Qt::MoveAction;
	QDragEnterEvent enter( at, acts, mime, Qt::LeftButton, Qt::NoModifier );
	QCoreApplication::sendEvent( vp, &enter );
	QDragMoveEvent move( at, acts, mime, Qt::LeftButton, Qt::NoModifier );
	QCoreApplication::sendEvent( vp, &move );
	QDropEvent drop( QPointF( at ), acts, mime, Qt::LeftButton, Qt::NoModifier );
	QCoreApplication::sendEvent( vp, &drop );
	qApp->processEvents();
}

//! "A>B": select A, drag it, drop it just below B (B empty: below the last row). False when A is not listed.
bool dragRow( CellModPickBox * box, const QString & spec )
{
	const QString a = spec.section( QLatin1Char( '>' ), 0, 0 ).trimmed();
	const QString b = spec.section( QLatin1Char( '>' ), 1 ).trimmed();
	const int ia = box->rowOf( a );
	const int ib = b.isEmpty() ? box->rowCount() - 1 : box->rowOf( b );
	if ( ia < 0 || ib < 0 )
		return false;
	box->selectRows( { a } );
	QModelIndexList picked;
	for ( QTreeWidgetItem * it : box->tree()->selectedItems() )
		picked << box->tree()->indexFromItem( it );
	QMimeData * mime = box->tree()->model()->mimeData( picked );
	sendDrop( box, mime, box->dropPointBefore( ib + 1 ) );
	delete mime;
	return true;
}

void dropPaths( CellModPickBox * box, const QStringList & paths )
{
	QMimeData mime;
	QList<QUrl> urls;
	for ( const QString & p : paths )
		urls << QUrl::fromLocalFile( p );
	mime.setUrls( urls );
	sendDrop( box, &mime, box->dropPointBefore( box->rowCount() ) );
}

void stageOne( NifSkope * skope, const QString & report, bool * acted )
{
	*acted = false;
	CellWorkspacePanel * panel = skope->findChild<CellWorkspacePanel *>();
	CellModPickBox * box = panel ? panel->modPicker() : nullptr;
	row( box, QStringLiteral( "the Cell workspace has a Mods band" ) );
	if ( !box ) {
		writeReport( report );
		return;
	}
	QDockWidget * dock = skope->findChild<QDockWidget *>( QStringLiteral( "CellWorkspaceDock" ) );
	row( dock && dock->isVisible(), QStringLiteral( "the Cell dock is up" ) );
	const QString src = box->sourceText();
	row( !src.isEmpty(), QStringLiteral( "source line" ), src );
	const QString expect = env( "WW_MODPICK_EXPECT" ).isEmpty() ? QStringLiteral( "bake" ) : env( "WW_MODPICK_EXPECT" );
	static const QHash<QString, QString> words{ { QStringLiteral( "bake" ), QStringLiteral( "from the far LOD bake" ) },
		{ QStringLiteral( "mo2" ), QStringLiteral( "from the MO2 profile" ) },
		{ QStringLiteral( "data" ), QStringLiteral( "from the game's Data folder and plugins.txt" ) },
		{ QStringLiteral( "masters" ), QStringLiteral( "from the game's Data folder, masters only" ) } };
	row( src.contains( words.value( expect, expect ) ), QStringLiteral( "seeded from: %1" ).arg( expect ) );
	if ( !env( "WW_MODPICK_EXPECT_LABEL" ).isEmpty() )
		row( src.contains( env( "WW_MODPICK_EXPECT_LABEL" ) ), QStringLiteral( "the header names the source" ),
			env( "WW_MODPICK_EXPECT_LABEL" ) );
	int gone = 0, goneNoted = 0, goneTickable = 0, on = 0, locked = 0, lockedOn = 0;
	for ( int i = 0; i < box->rowCount(); i++ ) {
		if ( box->rowLocked( i ) ) {
			locked++;
			lockedOn += box->rowOn( i ) ? 1 : 0;
		}
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
	row( box->rowLocked( 0 ) && locked == lockedOn, QStringLiteral( "the masters are locked at the top and load" ),
		QStringLiteral( "%1 locked, first %2" ).arg( locked ).arg( box->rowFile( 0 ) ) );
	if ( !env( "WW_MODPICK_EXPECT_LOCKED" ).isEmpty() ) {  // no MO2: the masters + Fallout4.ccc's files always load
		int lockedAll = 0;
		for ( int i = 0; i < box->rowCount(); i++ )
			lockedAll += box->rowLocked( i ) && box->rowOn( i ) && box->rowMod( i ).contains( QStringLiteral( "always loads" ) ) ? 1 : 0;
		row( lockedAll == env( "WW_MODPICK_EXPECT_LOCKED" ).toInt() && locked == lockedAll,
			QStringLiteral( "ticked and marked 'always loads' = the expected count" ),
			QStringLiteral( "%1 of %2 expected (%3 locked)" ).arg( lockedAll ).arg( env( "WW_MODPICK_EXPECT_LOCKED" ) ).arg( locked ) );
	}
	row( gone == goneNoted && goneTickable == 0, QStringLiteral( "gone files greyed with a note, never ticked" ),
		QStringLiteral( "%1 gone, %2 noted, %3 took a tick" ).arg( gone ).arg( goneNoted ).arg( goneTickable ) );
	if ( qgetenv( "WW_MODPICK_EXPECT_GONE" ).toInt() > 0 )
		row( gone == qgetenv( "WW_MODPICK_EXPECT_GONE" ).toInt(), QStringLiteral( "gone rows = the files missing on disk" ),
			QStringLiteral( "%1 vs %2" ).arg( gone ).arg( env( "WW_MODPICK_EXPECT_GONE" ) ) );
	if ( !env( "WW_MODPICK_EXPECT_LIST" ).isEmpty() ) {
		QFile f( env( "WW_MODPICK_EXPECT_LIST" ) );
		QStringList want;
		if ( f.open( QIODevice::ReadOnly | QIODevice::Text ) )
			for ( const QString & l : QString::fromUtf8( f.readAll() ).split( QLatin1Char( '\n' ) ) )
				if ( !l.trimmed().isEmpty() )
					want << l.trimmed();
		const QStringList got = orderOf( box );
		int first = -1;
		for ( int i = 0; i < std::max( want.size(), got.size() ) && first < 0; i++ )
			if ( want.value( i ).compare( got.value( i ), Qt::CaseInsensitive ) != 0 )
				first = i;
		row( first < 0 && !want.isEmpty(), QStringLiteral( "the list = the hand-built expectation (count and order)" ),
			first < 0 ? QStringLiteral( "%1 plugins" ).arg( got.size() )
			: QStringLiteral( "%1 vs %2 expected; first difference at %3: %4 vs %5" ).arg( got.size() ).arg( want.size() )
				.arg( first ).arg( got.value( first ), want.value( first ) ) );
	}
	const QString loaded = cellModPickLoaded( box->worldspace() );
	const int loadedN = loaded.split( QLatin1Char( ',' ), Qt::SkipEmptyParts ).size();
	if ( expect == QLatin1String( "bake" ) && !cellModPickTouched( box->worldspace() ) )
		row( on == loadedN, QStringLiteral( "untouched ticks = the plugins the cell loaded" ), QStringLiteral( "%1 ticked, %2 loaded" ).arg( on ).arg( loadedN ) );
	else
		info( QStringLiteral( "%1 ticked, the cell loaded %2" ).arg( on ).arg( loadedN ) );
	row( !box->applyEnabled(), QStringLiteral( "Apply is off until something changes" ) );
	const QString shot = env( "WW_MODPICK_SHOT" );
	const QSize s1 = shotOfBand( skope, shot );
	row( s1.isValid(), QStringLiteral( "picture of the band" ), QStringLiteral( "%1x%2" ).arg( s1.width() ).arg( s1.height() ) );

	const QString refdump = env( "WW_CELL_REFDUMP" );
	if ( !refdump.isEmpty() ) {
		QFile::remove( refdump + QStringLiteral( ".stage1.txt" ) );
		QFile::copy( refdump, refdump + QStringLiteral( ".stage1.txt" ) );
	}

	if ( !env( "WW_MODPICK_DRAG_BAD" ).isEmpty() ) {
		const QStringList was = orderOf( box );
		const bool listed = dragRow( box, env( "WW_MODPICK_DRAG_BAD" ) );
		row( listed && orderOf( box ) == was && box->lastNote().contains( QStringLiteral( "master" ) ),
			QStringLiteral( "a drag above a master is refused, the order kept" ),
			QStringLiteral( "%1: %2" ).arg( env( "WW_MODPICK_DRAG_BAD" ), box->lastNote() ) );
	}
	if ( !env( "WW_MODPICK_DRAG" ).isEmpty() ) {
		const QString spec = env( "WW_MODPICK_DRAG" );
		const QString a = spec.section( QLatin1Char( '>' ), 0, 0 ).trimmed();
		const QString b = spec.section( QLatin1Char( '>' ), 1 ).trimmed();
		const int was = box->rowOf( a );
		const bool listed = dragRow( box, spec );
		const int ia = box->rowOf( a ), ib = b.isEmpty() ? box->rowCount() - 1 : box->rowOf( b );
		row( listed && ( b.isEmpty() ? ia == ib : ia == ib + 1 ) && box->applyEnabled(),
			QStringLiteral( "the drag moved the row; Apply is live" ),
			QStringLiteral( "%1 from row %2 to row %3 (%4 at %5)%6" ).arg( a ).arg( was ).arg( ia ).arg( b ).arg( ib )
				.arg( box->lastNote().isEmpty() ? QString() : QStringLiteral( "; " ) + box->lastNote() ) );
		*acted = true;
	}
	if ( !env( "WW_MODPICK_DROP_BAD" ).isEmpty() ) {
		const int was = box->rowCount();
		dropPaths( box, env( "WW_MODPICK_DROP_BAD" ).split( QLatin1Char( ';' ), Qt::SkipEmptyParts ) );
		row( box->rowCount() == was && box->lastNote().startsWith( QStringLiteral( "Not added" ) ),
			QStringLiteral( "a drop with a missing master is refused" ), box->lastNote() );
	}
	if ( !env( "WW_MODPICK_DROP" ).isEmpty() ) {
		const QStringList paths = env( "WW_MODPICK_DROP" ).split( QLatin1Char( ';' ), Qt::SkipEmptyParts );
		const int was = box->rowCount();
		dropPaths( box, paths );
		int added = 0;
		QStringList mods;
		for ( int i = 0; i < box->rowCount(); i++ )
			if ( box->rowAdded( i ) ) {
				added++;
				mods << box->rowFile( i ) + QStringLiteral( " = " ) + box->rowMod( i );
			}
		row( box->rowCount() == was + paths.size() && added == paths.size()
			&& box->rowMod( box->rowCount() - 1 ).startsWith( QStringLiteral( "added: " ) ),
			QStringLiteral( "the Explorer drop added the rows in the dropped order, marked added" ),
			mods.join( QStringLiteral( "; " ) ) + QStringLiteral( "; " ) + box->lastNote() );
		*acted = true;
	}
	if ( !env( "WW_MODPICK_UNTICK" ).isEmpty() ) {
		const int at = box->rowOf( env( "WW_MODPICK_UNTICK" ) );
		row( at >= 0, QStringLiteral( "the row to untick is listed" ), env( "WW_MODPICK_UNTICK" ) );
		if ( at >= 0 ) {
			box->setRowOn( at, false );
			row( !box->rowOn( at ) && box->applyEnabled(), QStringLiteral( "unticked; Apply is live" ) );
			*acted = true;
		}
	}
	if ( !*acted ) {
		writeReport( report );
		return;
	}
	const QSize s1b = shotOfBand( skope, shot.isEmpty() ? QString() : withSuffix( shot, QStringLiteral( "_edited" ) ) );
	row( s1b.isValid(), QStringLiteral( "picture of the band before Apply" ) );
	{
		QFile f( report + QStringLiteral( ".order1.txt" ) );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) )
			f.write( orderOf( box ).join( QLatin1Char( '\n' ) ).toUtf8() + "\n" );
	}
	writeReport( report );
	box->apply();	// saves the choice and re-opens the cell (stage 2 runs on that load)
}

void stageTwo( NifSkope * skope, const QString & report )
{
	CellWorkspacePanel * panel = skope->findChild<CellWorkspacePanel *>();
	CellModPickBox * box = panel ? panel->modPicker() : nullptr;
	if ( !box ) {
		row( false, QStringLiteral( "the Mods band after the re-open" ) );
		writeReport( report );
		return;
	}
	const QString ws = box->worldspace();
	const CellModChoice c = cellModPickChoice( ws );
	QStringList before;
	{
		QFile f( report + QStringLiteral( ".order1.txt" ) );
		if ( f.open( QIODevice::ReadOnly | QIODevice::Text ) )
			for ( const QString & l : QString::fromUtf8( f.readAll() ).split( QLatin1Char( '\n' ) ) )
				if ( !l.trimmed().isEmpty() )
					before << l.trimmed();
	}
	row( orderOf( box ) == before, QStringLiteral( "after the re-open the rows keep the applied order" ),
		QStringLiteral( "%1 rows" ).arg( box->rowCount() ) );
	if ( !env( "WW_MODPICK_DRAG" ).isEmpty() ) {
		QStringList want;
		for ( const QString & f : before )
			want << ( f.endsWith( QLatin1Char( '/' ) ) ? QString() : f.toLower() );
		QStringList saved;
		for ( const QString & k : c.order )
			saved << ( k.startsWith( QStringLiteral( "dir:" ) ) ? QString() : k );
		row( !c.order.isEmpty() && saved == want, QStringLiteral( "the saved order (CellView/ModPickOrder) = the dragged order" ),
			QStringLiteral( "%1 keys" ).arg( c.order.size() ) );
	}
	if ( !env( "WW_MODPICK_DROP" ).isEmpty() ) {
		QStringList want;
		for ( const QString & p : env( "WW_MODPICK_DROP" ).split( QLatin1Char( ';' ), Qt::SkipEmptyParts ) )
			want << QDir::cleanPath( QDir::fromNativeSeparators( p ) );
		QStringList got;
		for ( const QString & p : c.added )
			got << QDir::cleanPath( p );
		int added = 0;
		for ( int i = 0; i < box->rowCount(); i++ )
			added += box->rowAdded( i ) ? 1 : 0;
		row( got.size() == want.size() && added == want.size(), QStringLiteral( "the saved additions (CellView/ModPickAdded) = the dropped paths" ),
			got.join( QStringLiteral( "; " ) ) );
	}
	if ( !env( "WW_MODPICK_UNTICK" ).isEmpty() ) {
		const int at = box->rowOf( env( "WW_MODPICK_UNTICK" ) );
		row( at >= 0 && !box->rowOn( at ), QStringLiteral( "after the re-open the row reads unticked (the choice was saved)" ),
			env( "WW_MODPICK_UNTICK" ) );
	}
	const QString note = cellModPickViewNote( skope->getNifModel() );
	const QString wantNote = env( "WW_MODPICK_EXPECT_NOTE" );
	if ( !wantNote.isEmpty() ) {
		bool all = note.startsWith( QStringLiteral( "Mods differ from the far LOD bake" ) );
		for ( const QString & part : wantNote.split( QLatin1Char( '|' ), Qt::SkipEmptyParts ) )
			all = all && note.contains( part );
		row( all, QStringLiteral( "the view's note says how the mods differ" ), note );
	} else {
		info( QStringLiteral( "view note: " ) + note );
	}
	const QString shot = env( "WW_MODPICK_SHOT" );
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
				bool acted = false;
				stageOne( skope, report, &acted );
				if ( !acted && qgetenv( "WW_MODPICK_STAY" ).isEmpty() )
					QTimer::singleShot( 0, qApp, &QCoreApplication::quit );
			} else if ( now == 1 ) {
				stageTwo( skope, report );
				if ( qgetenv( "WW_MODPICK_STAY" ).isEmpty() )
					QTimer::singleShot( 0, qApp, &QCoreApplication::quit );
			}
		} );
	} );
}
