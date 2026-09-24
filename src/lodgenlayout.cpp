#include "lodgenlayout.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>


/* THE FOLDER NAME EXISTS ONCE, HERE. Everything else in this file is composed
 * from it, and every writer in the generator composes its directory from one of
 * the four functions below. `tests/spells/lodgen_layout.sh` leg (e) greps
 * `src/` for a second spelling and fails on one. */
static const char * const FO4CSLOD = "FO4CSLOD";

QString lodgenFo4csFolderName()
{
	return QString::fromLatin1( FO4CSLOD );
}

QString lodgenFo4csRoot( const QString & modFolder )
{
	return modFolder + QChar( '/' ) + lodgenFo4csFolderName();
}

QString lodgenFo4csWorldDir( const QString & modFolder, const QString & ws )
{
	return lodgenFo4csRoot( modFolder ) + QChar( '/' ) + ws;
}

QString lodgenFo4csCardDir( const QString & modFolder )
{
	return lodgenFo4csRoot( modFolder ) + QStringLiteral( "/Cards" );
}

QString lodgenFo4csGameWorldPath( const QString & ws )
{
	return lodgenFo4csFolderName() + QChar( 92 ) + ws;
}

QString lodgenFo4csGameCardPath()
{
	return lodgenFo4csFolderName() + QChar( 92 ) + QStringLiteral( "Cards" );
}


/* ===== the census clause ================================================= */

static QString g_layoutRoot;        // the root the noted files landed under
static QString g_layoutSecond;      // a SECOND root, if two ever appear
static QString g_layoutStray;       // the first noted file that is not under one
static int     g_layoutUnder  = 0;
static int     g_layoutOutside = 0;
/* ONE ENTRY PER FILE, not per note (lane LAYOUT1, 2026-09-16). Two passes write
 * into the same `Objects/` folder -- the texture arrays and, later, the impostor
 * card arrays -- and each notes the folder when it finishes, so the array sheets
 * were noted twice and the clause read six higher than the tree. A census field
 * states what is on disk or it states nothing (CONSTITUTION 4), so the notes are
 * a SET, keyed on the absolute path folded to lower case because Windows will
 * hand back both spellings of the same file. Gate: leg (f) of
 * tests/spells/lodgen_layout.sh counts the tree and requires the two to agree. */
static QSet<QString> g_layoutSeen;

void lodgenClearLayoutCensus()
{
	g_layoutRoot.clear();
	g_layoutSecond.clear();
	g_layoutStray.clear();
	g_layoutUnder = 0;
	g_layoutOutside = 0;
	g_layoutSeen.clear();
}

void lodgenNoteLayoutFile( const QString & path )
{
	if ( path.isEmpty() )
		return;
	QString abs = QDir::fromNativeSeparators( QFileInfo( path ).absoluteFilePath() );
	if ( g_layoutSeen.contains( abs.toLower() ) )
		return;                      // noted already: one entry per file
	g_layoutSeen.insert( abs.toLower() );
	/* The root is found in the PATH, not assumed from the setting: the segment
	 * has to be there, spelled exactly, and what precedes it is the mod folder
	 * whatever the caller thought it was passing. */
	const QString needle = QChar( '/' ) + lodgenFo4csFolderName() + QChar( '/' );
	const int at = abs.lastIndexOf( needle, -1, Qt::CaseInsensitive );
	if ( at < 0 ) {
		g_layoutOutside++;
		if ( g_layoutStray.isEmpty() )
			g_layoutStray = abs;
		return;
	}
	const QString root = abs.left( at + needle.size() - 1 );
	g_layoutUnder++;
	if ( g_layoutRoot.isEmpty() )
		g_layoutRoot = root;
	else if ( g_layoutRoot.compare( root, Qt::CaseInsensitive ) != 0 && g_layoutSecond.isEmpty() )
		g_layoutSecond = root;
}

void lodgenNoteLayoutDir( const QString & dir )
{
	if ( dir.isEmpty() )
		return;
	const QFileInfoList fs = QDir( dir ).entryInfoList( QDir::Files );
	for ( const QFileInfo & fi : fs )
		lodgenNoteLayoutFile( fi.absoluteFilePath() );
}

QString lodgenLayoutCensusRoot()
{
	return g_layoutRoot;
}

QString lodgenLayoutCensusLine()
{
	if ( g_layoutUnder == 0 && g_layoutOutside == 0 )
		return QStringLiteral( "layout n/a (no FO4CS-target file written)" );
	QString s = QStringLiteral( "layout %1, %2 file(s), %3 outside" )
		.arg( g_layoutRoot.isEmpty() ? QStringLiteral( "(none)" ) : g_layoutRoot )
		.arg( g_layoutUnder ).arg( g_layoutOutside );
	if ( !g_layoutStray.isEmpty() )
		s += QStringLiteral( " (first: %1)" ).arg( g_layoutStray );
	if ( !g_layoutSecond.isEmpty() )
		s += QStringLiteral( " (a SECOND root: %1)" ).arg( g_layoutSecond );
	return s;
}
