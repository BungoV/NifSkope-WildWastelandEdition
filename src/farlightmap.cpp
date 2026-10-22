/* lane FARLIGHTMAP1 (2026-10-22): the whole-map far-light pass. The design is in farlightmap.h. */

#include "farlightmap.h"

#include "esmdata.h"
#include "probeincr.h"
#include "version.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QTextStream>
#include <QThread>
#include <QVariantMap>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <memory>
#include <vector>

#ifdef Q_OS_WIN
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#endif

namespace
{
const int kHalf = 2;   // BAKEBLOCK1: an exterior bake loads the 5x5 block round the asked cell

struct Args
{
	QString plugins, world = QStringLiteral( "Commonwealth" ), root, out, dataRoot, exe;
	QString scope = QStringLiteral( "farlightmap" );
	bool region = false;
	int x0 = 0, x1 = 0, y0 = 0, y1 = 0;
	qint64 memcapMB = 0;
	int jobs = 1, port = 14950;
	bool force = false, dry = false, allCells = false;
};

struct Cell
{
	int x = 0, y = 0;
	int lights = 0;
};

QFile g_log;

void logLine( const QString & l )
{
	const QByteArray b = l.toUtf8() + '\n';
	std::fwrite( b.constData(), 1, size_t( b.size() ), stdout );
	std::fflush( stdout );
	if ( g_log.isOpen() ) {
		g_log.write( b );
		g_log.flush();
	}
}

QString fileSha1( const QString & path )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return QString();
	QCryptographicHash h( QCryptographicHash::Sha1 );
	h.addData( &f );
	return QString::fromLatin1( h.result().toHex() );
}

QString sectorBase( int x, int y )
{
	char name[64];
	std::snprintf( name, sizeof name, "%+05d_%+05d", x, y );   // FARVIEW1's sector name (a sector is one cell)
	return QString::fromLatin1( name );
}

//! the files one cell owns in the far-light set
QStringList ownNames( int x, int y )
{
	const QString b = sectorBase( x, y );
	return { QStringLiteral( "lights_%1.wlt" ).arg( b ), QStringLiteral( "relight_%1.wlp" ).arg( b ),
		QStringLiteral( "farlight_%1.fvl" ).arg( b ), QStringLiteral( "farlight_%1.fvg" ).arg( b ),
		QStringLiteral( "farcheck_%1.bin" ).arg( b ) };
}

//! the knobs that move a far-light byte but no .tbk byte (BAKEINCR1 leaves them out of its identity)
QString farKnobs()
{
	QStringList k;
	const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	for ( const QString & n : env.keys() )
		if ( n.startsWith( QLatin1String( "WW_CELL_FARLIGHT_" ) ) )
			k << n + QLatin1Char( '=' ) + env.value( n );
	k.sort();
	return k.isEmpty() ? QStringLiteral( "-" ) : k.join( QLatin1Char( ' ' ) );
}

/* the child's settings scope, seeded with this machine's game manager state (tests/spells/settings_scope_game.py's
 * rule): an empty scope is a first install, whose game folders come out empty */
QString seedScope( const QString & scope )
{
	const QString app = QStringLiteral( "NifSkope " ) + NifSkopeVersion::rawToMajMin( QStringLiteral( NIFSKOPE_VERSION ) )
		+ QLatin1Char( ' ' ) + scope;
	QSettings s( QSettings::NativeFormat, QSettings::UserScope, QStringLiteral( "NifTools" ), app );
	s.clear();
	s.setValue( QStringLiteral( "Settings/Version" ), QStringLiteral( "1" ) );
	s.setValue( QStringLiteral( "Game Manager Version" ), 2 );
	QString game;
	QSettings reg( QStringLiteral( "HKEY_LOCAL_MACHINE\\SOFTWARE\\Bethesda Softworks\\Fallout4" ), QSettings::Registry32Format );
	for ( const char * v : { "Installed Path", "Path" } ) {
		const QString p = reg.value( QString::fromLatin1( v ) ).toString().remove( QLatin1Char( '"' ) );
		if ( !p.isEmpty() && QFileInfo( p ).isDir() ) {
			game = QDir::cleanPath( QDir::fromNativeSeparators( p ) );
			break;
		}
	}
	if ( game.isEmpty() ) {
		s.sync();
		return QStringLiteral( "scope %1: no Fallout 4 install in the registry; Game Manager Version only" ).arg( scope );
	}
	const QString data = game + QStringLiteral( "/Data" );
	QStringList folders;
	for ( const char * f : { "Textures", "Materials" } )
		if ( QFileInfo( data + QLatin1Char( '/' ) + QLatin1String( f ) ).isDir() )
			folders << data + QLatin1Char( '/' ) + QLatin1String( f );
	// get_archive_list()'s order: base game, then DLC and patch, then mods, each by descending lower-cased name
	QVector<QPair<QString, QString>> keyed;
	for ( const QString & n : QDir( data ).entryList( { QStringLiteral( "*.ba2" ), QStringLiteral( "*.bsa" ) }, QDir::Files ) ) {
		const QString low = n.toLower();
		QChar c = QLatin1Char( '2' );
		if ( low.startsWith( QLatin1String( "fallout" ) ) )
			c = ( low.contains( QLatin1String( "update" ) ) || low.endsWith( QLatin1String( "patch.ba2" ) ) ) ? QLatin1Char( '1' ) : QLatin1Char( '0' );
		else if ( low.startsWith( QLatin1String( "dlc" ) ) )
			c = QLatin1Char( '1' );
		keyed.append( { QString( c ) + low, n } );
	}
	std::sort( keyed.begin(), keyed.end(), []( const auto & a, const auto & b ) { return a.first > b.first; } );
	for ( const auto & k : keyed )
		folders << data + QLatin1Char( '/' ) + k.second;
	const QString fo4 = QStringLiteral( "Fallout 4" );
	s.setValue( QStringLiteral( "Game Paths" ), QVariantMap { { fo4, game } } );
	s.setValue( QStringLiteral( "Game Folders" ), QVariantMap { { fo4, folders } } );
	s.setValue( QStringLiteral( "Game Status" ), QVariantMap { { fo4, true } } );
	s.sync();
	return QStringLiteral( "scope %1: Fallout 4 at %2, %3 folder(s)/archive(s)" ).arg( scope, game ).arg( folders.size() );
}

qint64 freePhysMB()
{
#ifdef Q_OS_WIN
	MEMORYSTATUSEX m;
	m.dwLength = sizeof m;
	if ( GlobalMemoryStatusEx( &m ) )
		return qint64( m.ullAvailPhys >> 20 );
#endif
	return -1;
}

//! working set and peak working set of a child, MB (-1 when unreadable)
void childRss( qint64 pid, qint64 * ws, qint64 * peak )
{
	*ws = *peak = -1;
#ifdef Q_OS_WIN
	HANDLE h = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD( pid ) );
	if ( !h )
		return;
	PROCESS_MEMORY_COUNTERS c;
	if ( K32GetProcessMemoryInfo( h, &c, sizeof c ) ) {
		*ws = qint64( c.WorkingSetSize >> 20 );
		*peak = qint64( c.PeakWorkingSetSize >> 20 );
	}
	CloseHandle( h );
#else
	Q_UNUSED( pid );
#endif
}

bool parseArgs( QStringList a, Args & A, QString * why )
{
	while ( !a.isEmpty() ) {
		const QString k = a.takeFirst();
		auto val = [&]() { return a.isEmpty() ? QString() : a.takeFirst(); };
		if ( k == QLatin1String( "--plugins" ) ) A.plugins = val();
		else if ( k == QLatin1String( "--world" ) ) A.world = val();
		else if ( k == QLatin1String( "--root" ) ) A.root = val();
		else if ( k == QLatin1String( "--out" ) ) A.out = val();
		else if ( k == QLatin1String( "--dataroot" ) ) A.dataRoot = val();
		else if ( k == QLatin1String( "--exe" ) ) A.exe = val();
		else if ( k == QLatin1String( "--scope" ) ) A.scope = val();
		else if ( k == QLatin1String( "--memcap" ) ) A.memcapMB = val().toLongLong();
		else if ( k == QLatin1String( "--jobs" ) ) A.jobs = qBound( 1, val().toInt(), 8 );
		else if ( k == QLatin1String( "--port" ) ) A.port = val().toInt();
		else if ( k == QLatin1String( "--force" ) ) A.force = true;
		else if ( k == QLatin1String( "--dry" ) ) A.dry = true;
		else if ( k == QLatin1String( "--all-cells" ) ) A.allCells = true;
		else if ( k == QLatin1String( "--region" ) ) {
			// x0..x1,y0..y1 (either may be a single number)
			const QRegularExpression re( QStringLiteral( "^\\s*(-?\\d+)(?:\\.\\.(-?\\d+))?\\s*,\\s*(-?\\d+)(?:\\.\\.(-?\\d+))?\\s*$" ) );
			const QRegularExpressionMatch m = re.match( val() );
			if ( !m.hasMatch() ) {
				*why = QStringLiteral( "--region wants x0..x1,y0..y1" );
				return false;
			}
			const int a0 = m.captured( 1 ).toInt(), a1 = m.captured( 2 ).isEmpty() ? a0 : m.captured( 2 ).toInt();
			const int b0 = m.captured( 3 ).toInt(), b1 = m.captured( 4 ).isEmpty() ? b0 : m.captured( 4 ).toInt();
			A.region = true;
			A.x0 = std::min( a0, a1 );
			A.x1 = std::max( a0, a1 );
			A.y0 = std::min( b0, b1 );
			A.y1 = std::max( b0, b1 );
		} else {
			*why = QStringLiteral( "unknown option %1" ).arg( k );
			return false;
		}
	}
	if ( A.plugins.isEmpty() || A.root.isEmpty() ) {
		*why = QStringLiteral( "--plugins and --root are required" );
		return false;
	}
	A.root = QDir::cleanPath( QFileInfo( A.root ).absoluteFilePath() );
	A.out = A.out.isEmpty() ? A.root + QStringLiteral( "/farlight" ) : QDir::cleanPath( QFileInfo( A.out ).absoluteFilePath() );
	if ( A.exe.isEmpty() )
		A.exe = QCoreApplication::applicationFilePath();
	return true;
}

void usageText()
{
	logLine( QStringLiteral(
		"farlightmap: the cell view's probe GI bake over a worldspace's exterior cells, writing FARVIEW1's far-light set\n"
		"  NifSkope -no-gui farlightmap --plugins <a.esm[,b.esp]> --root <dir> [--world Commonwealth] [--out <dir>]\n"
		"      [--region x0..x1,y0..y1] [--memcap <MB>] [--jobs N] [--port 14950] [--scope farlightmap]\n"
		"      [--dataroot <dir>] [--exe <NifSkope.exe>] [--force] [--dry] [--all-cells]\n"
		"  --out        the far-light folder (default <root>/farlight): point the cell view's WW_CELL_FARLIGHT reader there\n"
		"  --memcap     a child whose working set passes this is stopped and the cell left undone; no cell starts\n"
		"               while free memory is below it\n"
		"  --dry        list the plan (cells, with lights, up to date) and bake nothing\n"
		"  --all-cells  bake cells whose 5x5 block has no placed light too (they write no light)" ) );
}

struct Marker
{
	bool ok = false;
	QString exe, knobs;
	QStringList files;   // sha1 \t name
};

Marker readMarker( const QString & path )
{
	Marker M;
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return M;
	const QStringList lines = QString::fromUtf8( f.readAll() ).split( QLatin1Char( '\n' ), Qt::SkipEmptyParts );
	if ( lines.isEmpty() || lines.first() != QLatin1String( "WWFARLIGHTMAP 1" ) )
		return M;
	for ( const QString & l : lines ) {
		if ( l.startsWith( QLatin1String( "exe " ) ) ) M.exe = l.mid( 4 );
		else if ( l.startsWith( QLatin1String( "knobs " ) ) ) M.knobs = l.mid( 6 );
		else if ( l.startsWith( QLatin1String( "file " ) ) ) M.files << l.mid( 5 );
	}
	M.ok = !M.exe.isEmpty();
	return M;
}

bool markerMatches( const Marker & M, const QString & exeSha, const QString & knobs, const QString & out )
{
	if ( !M.ok || M.exe != exeSha || M.knobs != knobs || M.files.isEmpty() )
		return false;
	for ( const QString & e : M.files ) {
		const int t = e.indexOf( QLatin1Char( '\t' ) );
		if ( t < 0 || fileSha1( QDir( out ).filePath( e.mid( t + 1 ) ) ) != e.left( t ) )
			return false;
	}
	return true;
}

struct Running
{
	Cell c;
	std::unique_ptr<QProcess> p;
	QElapsedTimer t;
	qint64 peakMB = 0;
	bool capped = false;
	int port = 0;
	QString cellDir, runDir, stage, notes;
};

QString num( double v, int prec = 1 ) { return QString::number( v, 'f', prec ); }
} // namespace

int farLightMapCli( const QStringList & args )
{
	Args A;
	QString why;
	if ( args.contains( QStringLiteral( "--help" ) ) || args.contains( QStringLiteral( "-h" ) ) ) {
		usageText();
		return 0;
	}
	if ( !parseArgs( args, A, &why ) ) {
		logLine( QStringLiteral( "farlightmap REFUSED: %1" ).arg( why ) );
		usageText();
		return 2;
	}
	QDir().mkpath( A.root + QStringLiteral( "/_runs" ) );
	QDir().mkpath( A.out );
	g_log.setFileName( A.root + QStringLiteral( "/farlightmap.log" ) );
	g_log.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text );
	QElapsedTimer clock;
	clock.start();
	const QString exeSha = fileSha1( A.exe );
	const QString knobs = farKnobs();
	logLine( QStringLiteral( "farlightmap start %1: world %2, plugins %3, root %4, out %5, exe %6 (sha1 %7), far knobs %8, jobs %9, memcap %10 MB" )
		.arg( QDateTime::currentDateTime().toString( Qt::ISODate ), A.world, A.plugins, A.root, A.out, A.exe, exeSha, knobs )
		.arg( A.jobs ).arg( A.memcapMB ) );
	if ( exeSha.isEmpty() ) {
		logLine( QStringLiteral( "farlightmap REFUSED: cannot read %1" ).arg( A.exe ) );
		return 2;
	}

	// 1. the worldspace's exterior cells (region or whole map), and the placed lights each one's block holds
	QString wsError, loadError;
	quint32 wsForm = 0;
	for ( const QPair<quint32, QString> & w : EsmWorld::listWorldspaces( A.plugins, &wsError ) )
		if ( w.second.compare( A.world, Qt::CaseInsensitive ) == 0 )
			wsForm = w.first;
	EsmWorld world;
	if ( !wsForm || !world.load( A.plugins, wsForm, &loadError ) ) {
		logLine( QStringLiteral( "farlightmap REFUSED: cannot load %1 from %2: %3%4" ).arg( A.world, A.plugins, wsError, loadError ) );
		return 3;
	}
	if ( !A.region ) {   // the whole map: every grid cell the worldspace has (FO4's grid is well inside +-128)
		A.x0 = A.y0 = -128;
		A.x1 = A.y1 = 127;
	}
	std::vector<Cell> cells;
	int withLights = 0;
	for ( int y = A.y0; y <= A.y1; y++ )
		for ( int x = A.x0; x <= A.x1; x++ ) {
			if ( !world.hasCell( x, y ) )
				continue;
			Cell c { x, y, 0 };
			ProbeIncr::lightsDigest( world, x, y, kHalf, &c.lights );
			withLights += c.lights > 0 ? 1 : 0;
			cells.push_back( c );
		}
	logLine( QStringLiteral( "cells: %1 in %2..%3,%4..%5 (%6 with a placed light in their 5x5 block), listed in %7 s" )
		.arg( cells.size() ).arg( A.x0 ).arg( A.x1 ).arg( A.y0 ).arg( A.y1 ).arg( withLights ).arg( num( clock.elapsed() / 1000.0 ) ) );

	const QString scopeNote = seedScope( A.scope );
	logLine( scopeNote );
	auto childEnv = [&]() {
		QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
		env.insert( QStringLiteral( "WW_SETTINGS_SCOPE" ), A.scope );
		if ( !A.dataRoot.isEmpty() )
			env.insert( QStringLiteral( "WW_CELL_DATAROOT" ), A.dataRoot );
		return env;
	};

	// 2. BAKEINCR1's check over the cells a marker says were done: identity, records, assets, far, lights
	QHash<QPair<int, int>, QString> verdict;   // cell -> "clean" or the dirty reason
	int bx0 = INT_MAX, by0 = INT_MAX, bx1 = INT_MIN, by1 = INT_MIN;
	for ( const Cell & c : cells ) {
		if ( A.force || ( c.lights == 0 && !A.allCells ) )
			continue;
		if ( !QFileInfo::exists( A.root + QStringLiteral( "/cell_%1_%2/farlight.done" ).arg( c.x ).arg( c.y ) ) )
			continue;
		bx0 = std::min( bx0, c.x );
		bx1 = std::max( bx1, c.x );
		by0 = std::min( by0, c.y );
		by1 = std::max( by1, c.y );
	}
	if ( bx0 <= bx1 ) {
		const QString vpath = A.root + QStringLiteral( "/_runs/verdict.txt" );
		QFile::remove( vpath );
		QProcess chk;
		QProcessEnvironment env = childEnv();
		env.insert( QStringLiteral( "WW_CELL_BAKEINCR_CHECK" ), QStringLiteral( "%1|%2|%3,%4,%5,%6|%7|%8" )
			.arg( A.plugins, A.world ).arg( bx0 ).arg( by0 ).arg( bx1 ).arg( by1 ).arg( A.root, vpath ) );
		chk.setProcessEnvironment( env );
		chk.setProcessChannelMode( QProcess::MergedChannels );
		chk.setStandardOutputFile( A.root + QStringLiteral( "/_runs/check.notes" ) );
		QElapsedTimer ct;
		ct.start();
		chk.start( A.exe, {} );
		chk.waitForFinished( -1 );
		QFile vf( vpath );
		QString census;
		if ( chk.exitStatus() == QProcess::NormalExit && chk.exitCode() == 0 && vf.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
			for ( const QString & l : QString::fromUtf8( vf.readAll() ).split( QLatin1Char( '\n' ), Qt::SkipEmptyParts ) ) {
				const QStringList w = l.split( QLatin1Char( ' ' ) );
				if ( l.startsWith( QLatin1String( "cell " ) ) && w.size() >= 4 )
					verdict.insert( { w[1].toInt(), w[2].toInt() }, w[3] == QLatin1String( "clean" ) ? w[3] : w.mid( 4 ).join( QLatin1Char( ' ' ) ) );
				else if ( l.startsWith( QLatin1String( "incremental:" ) ) )
					census = l;
			}
			logLine( QStringLiteral( "check %1..%2,%3..%4: %5; %6 s" ).arg( bx0 ).arg( bx1 ).arg( by0 ).arg( by1 ).arg( census )
				.arg( num( ct.elapsed() / 1000.0 ) ) );
		} else {
			logLine( QStringLiteral( "check FAILED rc %1 (%2/_runs/check.notes): every cell rebakes" ).arg( chk.exitCode() ).arg( A.root ) );
		}
	}

	// 3. the plan
	std::vector<Cell> todo;
	int upToDate = 0, dark = 0;
	QMap<QString, int> dirtyWhy;
	for ( const Cell & c : cells ) {
		if ( c.lights == 0 && !A.allCells ) {
			dark++;
			if ( !A.dry )   // a light that went away leaves no stale layer behind
				for ( const QString & n : ownNames( c.x, c.y ) )
					QFile::remove( QDir( A.out ).filePath( n ) );
			continue;
		}
		const QString v = verdict.value( { c.x, c.y } );
		const QString cellDir = A.root + QStringLiteral( "/cell_%1_%2" ).arg( c.x ).arg( c.y );
		if ( !A.force && v == QLatin1String( "clean" )
			&& markerMatches( readMarker( cellDir + QStringLiteral( "/farlight.done" ) ), exeSha, knobs, A.out ) ) {
			upToDate++;
			continue;
		}
		dirtyWhy[A.force ? QStringLiteral( "forced" ) : v.isEmpty() ? QStringLiteral( "never done" )
			: v == QLatin1String( "clean" ) ? QStringLiteral( "marker or far files" ) : v.section( QLatin1Char( ' ' ), 0, 0 )]++;
		todo.push_back( c );
	}
	QStringList dw;
	for ( auto it = dirtyWhy.constBegin(); it != dirtyWhy.constEnd(); ++it )
		dw << QStringLiteral( "%1 %2" ).arg( it.value() ).arg( it.key() );
	logLine( QStringLiteral( "plan: %1 to bake (%2), %3 up to date, %4 without a light in their block%5" ).arg( todo.size() )
		.arg( dw.isEmpty() ? QStringLiteral( "none" ) : dw.join( QStringLiteral( ", " ) ) ).arg( upToDate ).arg( dark )
		.arg( A.allCells ? QStringLiteral( " (baked anyway: --all-cells)" ) : QString() ) );
	if ( A.dry || todo.empty() ) {
		logLine( QStringLiteral( "farlightmap end: nothing baked%1" ).arg( A.dry ? QStringLiteral( " (--dry)" ) : QString() ) );
		return 0;
	}

	// 4. the bakes, one child a cell (its memory is freed when it exits)
	const QString fixture = A.root + QStringLiteral( "/_runs/farlightmap.wwcell" );
	{
		QFile ff( fixture );   // the file a child opens; WW_CELL_OPEN overrides its spec
		if ( ff.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
			ff.write( "# farlightmap: the cell comes from WW_CELL_OPEN\n" );
	}
	const QRegularExpression reRec( QStringLiteral( "([+-]\\d{4}_[+-]\\d{4}): (\\d+) lights in (\\d+) groups, (\\d+) pairs over (\\d+) surfels" ) );
	const QRegularExpression reFar( QStringLiteral( "farlight \\(FARVIEW1\\): (\\d+) groups \\((\\d+) switched\\)[^\\n]*?; (\\d+) sectors, (\\d+) records, "
		"(\\d+) switched entries, (\\d+) B, (\\d+) ms" ) );
	std::vector<std::unique_ptr<Running>> run;
	size_t next = 0;
	int baked = 0, failed = 0, finished = 0;
	qint64 bytesOut = 0;
	QElapsedTimer bakeClock;
	bakeClock.start();
	QStringList failedCells;
	qint64 lastWait = -1;

	auto finish = [&]( Running & R ) {
		finished++;
		const double wall = R.t.elapsed();
		const QString tag = QStringLiteral( "cell %1 %2" ).arg( R.c.x ).arg( R.c.y );
		QString notes;
		{
			QFile nf( R.notes );
			if ( nf.open( QIODevice::ReadOnly ) )
				notes = QString::fromUtf8( nf.readAll() );
		}
		auto fail = [&]( const QString & w ) {
			failed++;
			failedCells << QStringLiteral( "%1,%2" ).arg( R.c.x ).arg( R.c.y );
			logLine( QStringLiteral( "%1 FAILED: %2 (rc %3, wall %4 s, peak rss %5 MB; notes %6)" ).arg( tag, w ).arg( R.p->exitCode() )
				.arg( num( wall / 1000.0 ) ).arg( R.peakMB ).arg( R.notes ) );
		};
		if ( R.capped )
			return fail( QStringLiteral( "memcap %1 MB passed, stopped" ).arg( A.memcapMB ) );
		if ( R.p->exitStatus() != QProcess::NormalExit || R.p->exitCode() != 0 )
			return fail( QStringLiteral( "the bake did not end cleanly" ) );
		if ( !QFileInfo::exists( R.cellDir + QStringLiteral( "/bakeincr.txt" ) ) )
			return fail( QStringLiteral( "no bake record (the probe bake was refused?)" ) );
		const QStringList own = ownNames( R.c.x, R.c.y );
		if ( !QFileInfo::exists( QDir( R.stage ).filePath( own[2] ) ) ) {
			const int at = notes.indexOf( QLatin1String( "far light" ) );
			return fail( at >= 0 ? notes.mid( at, notes.indexOf( QLatin1Char( '\n' ), at ) - at ).trimmed()
				: QStringLiteral( "no %1 written" ).arg( own[2] ) );
		}
		// the cell's own files into the shared set; anything else the child wrote stays in the stage, counted
		int foreign = 0;
		for ( const QString & n : QDir( R.stage ).entryList( QDir::Files ) )
			foreign += own.contains( n ) ? 0 : 1;
		QStringList fileLines;
		qint64 bytes = 0;
		int layers = 0;
		for ( const QString & n : own ) {
			const QString dst = QDir( A.out ).filePath( n ), src = QDir( R.stage ).filePath( n );
			QFile::remove( dst );
			if ( !QFileInfo::exists( src ) )
				continue;
			const QString tmp = dst + QStringLiteral( ".part" );
			QFile::remove( tmp );
			if ( !QFile::copy( src, tmp ) || !QFile::rename( tmp, dst ) )
				return fail( QStringLiteral( "cannot write %1" ).arg( dst ) );
			fileLines << QStringLiteral( "file %1\t%2" ).arg( fileSha1( dst ), n );
			bytes += QFileInfo( dst ).size();
			layers += n.endsWith( QLatin1String( ".fvl" ) ) || n.endsWith( QLatin1String( ".fvg" ) ) ? 1 : 0;
		}
		QFile mf( R.cellDir + QStringLiteral( "/farlight.done" ) );
		if ( !mf.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
			return fail( QStringLiteral( "cannot write the done marker" ) );
		mf.write( ( QStringList { QStringLiteral( "WWFARLIGHTMAP 1" ), QStringLiteral( "cell %1 %2" ).arg( R.c.x ).arg( R.c.y ),
			QStringLiteral( "exe %1" ).arg( exeSha ), QStringLiteral( "knobs %1" ).arg( knobs ) } + fileLines ).join( QLatin1Char( '\n' ) ).toUtf8() + "\n" );
		mf.close();
		baked++;
		bytesOut += bytes;
		// the census: the lights and surfels of this cell's sector, the layers written, the times
		QString lights = QStringLiteral( "?" ), groups = QStringLiteral( "?" ), surfels = QStringLiteral( "?" );
		for ( auto it = reRec.globalMatch( notes ); it.hasNext(); ) {
			const QRegularExpressionMatch m = it.next();
			if ( m.captured( 1 ) == sectorBase( R.c.x, R.c.y ) ) {
				lights = m.captured( 2 );
				groups = m.captured( 3 );
				surfels = m.captured( 5 );
			}
		}
		const QRegularExpressionMatch fm = reFar.match( notes );
		logLine( QStringLiteral( "%1 baked: lights %2 (block %3, groups %4), surfels %5, records %6, switched entries %7, layers %8, "
			"%9 B, far ms %10, wall %11 s, peak rss %12 MB%13" ).arg( tag, lights ).arg( R.c.lights ).arg( groups, surfels )
			.arg( fm.hasMatch() ? fm.captured( 4 ) : QStringLiteral( "?" ), fm.hasMatch() ? fm.captured( 5 ) : QStringLiteral( "?" ) )
			.arg( layers ).arg( bytes ).arg( fm.hasMatch() ? fm.captured( 7 ) : QStringLiteral( "?" ) ).arg( num( wall / 1000.0 ) )
			.arg( R.peakMB ).arg( foreign ? QStringLiteral( ", %1 other files left in the stage" ).arg( foreign ) : QString() ) );
	};

	while ( next < todo.size() || !run.empty() ) {
		// start: a free slot, and (with a cap) room for one more child
		while ( next < todo.size() && int( run.size() ) < A.jobs ) {
			const qint64 freeMB = freePhysMB();
			if ( A.memcapMB > 0 && freeMB >= 0 && freeMB < A.memcapMB ) {
				if ( lastWait < 0 || bakeClock.elapsed() - lastWait > 60000 ) {
					logLine( QStringLiteral( "waiting: %1 MB free, below the memcap %2 MB" ).arg( freeMB ).arg( A.memcapMB ) );
					lastWait = bakeClock.elapsed();
				}
				break;
			}
			std::vector<int> used;
			for ( const auto & r : run )
				used.push_back( r->port );
			int port = A.port;
			while ( std::find( used.begin(), used.end(), port ) != used.end() )
				port++;
			auto R = std::make_unique<Running>();
			R->c = todo[next++];
			R->port = port;
			R->cellDir = A.root + QStringLiteral( "/cell_%1_%2" ).arg( R->c.x ).arg( R->c.y );
			R->runDir = A.root + QStringLiteral( "/_runs/cell_%1_%2" ).arg( R->c.x ).arg( R->c.y );
			R->stage = R->cellDir + QStringLiteral( "/far" );
			R->notes = R->runDir + QStringLiteral( "/shot.notes" );
			for ( const QString & d : { R->cellDir, R->runDir } ) {
				QDir( d ).removeRecursively();
				QDir().mkpath( d );
			}
			// tools/ww_bake_world.py's bake, plus WW_CELL_FARLIGHT: the cell view's own bake, relight and far layers
			QProcessEnvironment env = childEnv();
			env.insert( QStringLiteral( "WW_CELL_OPEN" ), QStringLiteral( "%1|%2|%3,%4|1" ).arg( A.plugins, A.world ).arg( R->c.x ).arg( R->c.y ) );
			env.insert( QStringLiteral( "WW_CELL_PROBES" ), QDir::toNativeSeparators( R->runDir + QStringLiteral( "/probes.tsv" ) ) );
			env.insert( QStringLiteral( "WW_CELL_PROBES_HIDE" ), QStringLiteral( "1" ) );
			env.insert( QStringLiteral( "WW_CELL_PROBE_BAKE" ), QDir::toNativeSeparators( R->cellDir ) );
			env.insert( QStringLiteral( "WW_CELL_FARLIGHT" ), QDir::toNativeSeparators( R->stage ) );
			env.insert( QStringLiteral( "WW_RENDER_SHOT" ), QDir::toNativeSeparators( R->runDir + QStringLiteral( "/shot.png" ) ) );
			env.insert( QStringLiteral( "WW_RENDER_SIZE" ), QStringLiteral( "320x200" ) );
			env.insert( QStringLiteral( "WW_RENDER_VIEW" ), QStringLiteral( "1" ) );
			env.insert( QStringLiteral( "WW_RENDER_CLEAN" ), QStringLiteral( "1" ) );
			R->p = std::make_unique<QProcess>();
			R->p->setProcessEnvironment( env );
			R->p->setProcessChannelMode( QProcess::MergedChannels );
			R->p->setStandardOutputFile( R->notes );
			R->t.start();
			R->p->start( A.exe, { QStringLiteral( "--port" ), QString::number( port ), fixture } );
			logLine( QStringLiteral( "cell %1 %2 start (port %3, %4 lights in the block, %5 MB free)" ).arg( R->c.x ).arg( R->c.y )
				.arg( port ).arg( R->c.lights ).arg( freeMB ) );
			run.push_back( std::move( R ) );
		}
		QCoreApplication::processEvents();
		QThread::msleep( 500 );
		for ( size_t i = 0; i < run.size(); ) {
			Running & R = *run[i];
			qint64 ws = -1, peak = -1;
			if ( R.p->state() != QProcess::NotRunning ) {
				childRss( R.p->processId(), &ws, &peak );
				R.peakMB = std::max( R.peakMB, peak );
				if ( A.memcapMB > 0 && ws > A.memcapMB && !R.capped ) {   // our own child, past the cap: the memory guard
					R.capped = true;
					logLine( QStringLiteral( "cell %1 %2: working set %3 MB passed the memcap %4 MB, stopping it" ).arg( R.c.x ).arg( R.c.y )
						.arg( ws ).arg( A.memcapMB ) );
					R.p->kill();
				}
				i++;
				continue;
			}
			R.p->waitForFinished( 0 );
			finish( R );
			const double hours = bakeClock.elapsed() / 3.6e6;
			const double cph = hours > 0 ? finished / hours : 0.0;
			const size_t left = todo.size() - next + run.size() - 1;
			logLine( QStringLiteral( "progress: %1 of %2 (baked %3, failed %4), %5 cells/hour, %6 MB written, eta %7 h" ).arg( finished )
				.arg( todo.size() ).arg( baked ).arg( failed ).arg( num( cph ) ).arg( bytesOut >> 20 )
				.arg( cph > 0 ? num( left / cph ) : QStringLiteral( "?" ) ) );
			run.erase( run.begin() + std::ptrdiff_t( i ) );
		}
	}
	const double hours = bakeClock.elapsed() / 3.6e6;
	logLine( QStringLiteral( "farlightmap end: baked %1, failed %2%3, up to date %4, without lights %5; %6 h, %7 cells/hour, %8 MB written to %9" )
		.arg( baked ).arg( failed ).arg( failedCells.isEmpty() ? QString() : QStringLiteral( " (%1)" ).arg( failedCells.join( QLatin1Char( ' ' ) ) ) )
		.arg( upToDate ).arg( dark ).arg( num( hours, 2 ) ).arg( num( hours > 0 ? finished / hours : 0.0 ) ).arg( bytesOut >> 20 ).arg( A.out ) );
	return failed ? 1 : 0;
}
