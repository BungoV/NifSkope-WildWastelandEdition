/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

#include "cellmodpick.h"

#include "cellmodsource.h"
#include "lodbfile.h"
#include "lodgen.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QSettings>

#include <algorithm>

// see cellmodpick.h

namespace {

QHash<QString, QString> & shownBake()    { static QHash<QString, QString> h; return h; }	// ws (lower) -> .lodb
QHash<QString, QString> & loadedStack()  { static QHash<QString, QString> h; return h; }	// ws (lower) -> comma list
QHash<const void *, QString> & viewNotes() { static QHash<const void *, QString> h; return h; }

QString key( const char * group, const QString & ws )
{
	return QStringLiteral( "CellView/%1/%2" ).arg( QLatin1String( group ), ws.toLower() );
}

QString clean( const QString & p )
{
	return QDir::cleanPath( QDir::fromNativeSeparators( p.trimmed() ) );
}

QString fileOf( const QString & path )
{
	return QFileInfo( path.trimmed() ).fileName();
}

QStringList namesOf( const QStringList & paths )
{
	QStringList n;
	for ( const QString & p : paths )
		n << fileOf( p ).toLower();
	return n;
}

bool isPlugin( const QString & path )
{
	const QString s = QFileInfo( path ).suffix().toLower();
	return s == QLatin1String( "esm" ) || s == QLatin1String( "esp" ) || s == QLatin1String( "esl" );
}

//! The mod folder a plugin path lives in: <mods>/<name>/..., MO2's overwrite, else the game's Data.
QString modOfPath( const QString & path )
{
	const QString p = QDir::fromNativeSeparators( path );
	const int m = p.indexOf( QStringLiteral( "/mods/" ), 0, Qt::CaseInsensitive );
	if ( m >= 0 ) {
		const int s = m + 6;
		const int e = p.indexOf( QLatin1Char( '/' ), s );
		if ( e > s )
			return p.mid( s, e - s );
	}
	if ( p.contains( QStringLiteral( "/overwrite/" ), Qt::CaseInsensitive ) )
		return QStringLiteral( "overwrite" );
	return QStringLiteral( "Data" );
}

bool red( const char * what )
{
	return qgetenv( "WW_MODPICK_RED" ) == what;
}

CellModRow sourceRow( const QString & path, const QString & mod )
{
	CellModRow r;
	r.path = clean( path );
	r.file = fileOf( path );
	r.key = r.file.toLower();
	r.mod = mod;
	r.present = QFileInfo( r.path ).isFile();
	r.locked = r.present && cellModLockedMaster( r.file );
	return r;
}

CellModRow addedRow( const QString & path )
{
	CellModRow r;
	r.path = clean( path );
	r.added = true;
	r.folder = QFileInfo( r.path ).isDir() || !isPlugin( r.path );
	if ( r.folder ) {
		r.file = QFileInfo( r.path ).fileName() + QLatin1Char( '/' );
		r.mod = QStringLiteral( "added: %1 (loose files and archives)" ).arg( QFileInfo( r.path ).fileName() );
		r.present = QFileInfo( r.path ).isDir();
	} else {
		r.file = fileOf( r.path );
		r.mod = QStringLiteral( "added: %1" ).arg( QFileInfo( QFileInfo( r.path ).absolutePath() ).fileName() );
		r.present = QFileInfo( r.path ).isFile();
	}
	r.key = cellModRowKey( r.path, r.folder );
	return r;
}

//! a row's resource root: the folder itself, or the folder its plugin sits in
QString rootOf( const CellModRow & r )
{
	return r.folder ? r.path : clean( QFileInfo( r.path ).absolutePath() );
}

int stackIndexOf( const QStringList & stack, const QString & dir )
{
	int at = -1;
	for ( int k = 0; k < stack.size(); k++ ) {
		const QString e = clean( stack.at( k ) );
		if ( dir.compare( e, Qt::CaseInsensitive ) == 0 || dir.startsWith( e + QLatin1Char( '/' ), Qt::CaseInsensitive ) )
			at = k;
	}
	return at;
}

/*! The stack with each hand-added root placed above the highest root of the plugins listed above it, then every
 *  loaded plugin's own archives on top in plugin order (moved there when already listed). */
QStringList orderedStack( const QStringList & start, const QVector<CellModRow> & rows, const QStringList & kept,
	QStringList * roots )
{
	QStringList st = start;
	for ( int i = 0; i < rows.size(); i++ ) {
		const CellModRow & r = rows.at( i );
		if ( !r.added || !r.present || !r.on )
			continue;
		const QString root = rootOf( r );
		if ( st.contains( root, Qt::CaseInsensitive ) )
			continue;
		/* listed below every source plugin: the top of the stack (above roots no plugin names, such as an unpacked
		 * Data copy); else just above the highest root of the plugins listed above it */
		bool below = false;
		for ( int j = i + 1; j < rows.size() && !below; j++ )
			below = !rows.at( j ).added && rows.at( j ).present && !rows.at( j ).folder;
		int idx = -1;
		for ( int j = 0; j < i; j++ ) {
			const CellModRow & a = rows.at( j );
			if ( a.present && !a.folder )
				idx = std::max( idx, stackIndexOf( st, rootOf( a ) ) );
		}
		st.insert( below ? idx + 1 : int( st.size() ), root );
		*roots << root;
	}
	for ( const QString & p : kept ) {
		for ( const QString & a : cellModPluginArchives( p ) ) {
			for ( int k = st.size() - 1; k >= 0; k-- )
				if ( clean( st.at( k ) ).compare( a, Qt::CaseInsensitive ) == 0 )
					st.removeAt( k );
			st << a;
		}
	}
	return st;
}

QString listOf( const QStringList & names )
{
	return QStringList( names.mid( 0, 3 ) ).join( QStringLiteral( ", " ) )
		+ ( names.size() > 3 ? QStringLiteral( " and %1 more" ).arg( names.size() - 3 ) : QString() );
}

} // namespace

// ---- saved choice

QString cellModPickShownBake( const QString & ws )
{
	return shownBake().value( ws.toLower() );
}

QString cellModPickLoaded( const QString & ws )
{
	return loadedStack().value( ws.toLower() );
}

bool cellModPickTouched( const QString & ws )
{
	if ( ws.isEmpty() )
		return false;
	const QSettings s;
	return s.contains( key( "ModPick", ws ) ) || s.contains( key( "ModPickOrder", ws ) ) || s.contains( key( "ModPickAdded", ws ) );
}

CellModChoice cellModPickChoice( const QString & ws )
{
	const QSettings s;
	CellModChoice c;
	for ( const QString & n : s.value( key( "ModPick", ws ) ).toStringList() )
		if ( !n.trimmed().isEmpty() )
			c.off << n.trimmed().toLower();
	for ( const QString & n : s.value( key( "ModPickOrder", ws ) ).toStringList() )
		if ( !n.trimmed().isEmpty() )
			c.order << n.trimmed().toLower();
	for ( const QString & n : s.value( key( "ModPickAdded", ws ) ).toStringList() )
		if ( !n.trimmed().isEmpty() )
			c.added << clean( n );
	return c;
}

void cellModPickSave( const QString & ws, const CellModChoice & c )
{
	QSettings s;
	QStringList off;
	for ( const QString & n : c.off )
		off << n.toLower();
	s.setValue( key( "ModPick", ws ), off );
	if ( c.order.isEmpty() )
		s.remove( key( "ModPickOrder", ws ) );
	else
		s.setValue( key( "ModPickOrder", ws ), c.order );
	if ( c.added.isEmpty() )
		s.remove( key( "ModPickAdded", ws ) );
	else
		s.setValue( key( "ModPickAdded", ws ), c.added );
}

void cellModPickReset( const QString & ws )
{
	QSettings s;
	s.remove( key( "ModPick", ws ) );
	s.remove( key( "ModPickOrder", ws ) );
	s.remove( key( "ModPickAdded", ws ) );
}

// ---- rows, masters, drops

QString cellModRowKey( const QString & path, bool folder )
{
	return folder ? QStringLiteral( "dir:" ) + clean( path ).toLower() : fileOf( path ).toLower();
}

bool cellModLockedMaster( const QString & fileName )
{
	static const QStringList locked{ QStringLiteral( "fallout4.esm" ), QStringLiteral( "dlcrobot.esm" ),
		QStringLiteral( "dlcworkshop01.esm" ), QStringLiteral( "dlccoast.esm" ), QStringLiteral( "dlcworkshop02.esm" ),
		QStringLiteral( "dlcworkshop03.esm" ), QStringLiteral( "dlcnukaworld.esm" ),
		QStringLiteral( "dlcultrahighresolution.esm" ) };
	return locked.contains( fileName.toLower() );
}

QStringList cellModPluginMasters( const QString & path )
{
	static QHash<QString, QStringList> cache;
	const QFileInfo fi( path );
	if ( !fi.isFile() )
		return {};
	const QString ck = clean( path ).toLower() + QLatin1Char( '|' ) + QString::number( fi.size() );
	auto it = cache.constFind( ck );
	if ( it != cache.constEnd() )
		return *it;
	QStringList out;
	QFile f( path );
	if ( f.open( QIODevice::ReadOnly ) ) {
		const QByteArray head = f.read( 24 );
		if ( head.size() == 24 && head.startsWith( "TES4" ) ) {
			const quint32 size = quint32( uchar( head[4] ) ) | quint32( uchar( head[5] ) ) << 8
				| quint32( uchar( head[6] ) ) << 16 | quint32( uchar( head[7] ) ) << 24;
			const QByteArray body = f.read( qint64( std::min<quint32>( size, 16u << 20 ) ) );
			int o = 0;
			while ( o + 6 <= body.size() ) {
				const QByteArray t = body.mid( o, 4 );
				const int s = int( uchar( body[o + 4] ) ) | int( uchar( body[o + 5] ) ) << 8;
				if ( t == "MAST" && o + 6 + s <= body.size() ) {
					QByteArray n = body.mid( o + 6, s );
					while ( n.endsWith( '\0' ) )
						n.chop( 1 );
					out << QString::fromLatin1( n );
				}
				o += 6 + s;
			}
		}
	}
	cache.insert( ck, out );
	return out;
}

QVector<CellModRow> cellModDropRows( const QStringList & paths, QString * skipped )
{
	QVector<CellModRow> out;
	QStringList skip;
	for ( const QString & raw : paths ) {
		const QString p = clean( raw );
		const QFileInfo fi( p );
		if ( fi.isFile() && isPlugin( p ) ) {
			out << addedRow( p );
		} else if ( fi.isDir() ) {
			QStringList plugins = QDir( p ).entryList( { QStringLiteral( "*.esm" ), QStringLiteral( "*.esl" ),
				QStringLiteral( "*.esp" ) }, QDir::Files, QDir::Name );
			auto rank = []( const QString & n ) {
				const QString s = QFileInfo( n ).suffix().toLower();
				return s == QLatin1String( "esm" ) ? 0 : ( s == QLatin1String( "esl" ) ? 1 : 2 );
			};
			std::stable_sort( plugins.begin(), plugins.end(), [&]( const QString & a, const QString & b ) {
				return rank( a ) < rank( b );
			} );
			if ( plugins.isEmpty() )
				out << addedRow( p );
			for ( const QString & n : plugins )
				out << addedRow( QDir( p ).filePath( n ) );
		} else {
			skip << fi.fileName();
		}
	}
	if ( skipped )
		*skipped = skip.join( QStringLiteral( ", " ) );
	return out;
}

QString cellModOrderProblem( const QVector<CellModRow> & before, const QVector<CellModRow> & after )
{
	// problem keys: "missing|<file>|<master>" and "above|<file>|<master>"
	auto problems = []( const QVector<CellModRow> & rows ) {
		QHash<QString, int> at;
		for ( int i = 0; i < rows.size(); i++ )
			if ( !rows.at( i ).folder )
				at.insert( rows.at( i ).key, i );
		QStringList p;
		for ( int i = 0; i < rows.size(); i++ ) {
			const CellModRow & r = rows.at( i );
			if ( r.folder || !r.present )
				continue;
			for ( const QString & m : cellModPluginMasters( r.path ) ) {
				const int j = at.value( m.toLower(), -1 );
				if ( j < 0 )
					p << QStringLiteral( "missing|%1|%2" ).arg( r.file, m );
				else if ( j > i )
					p << QStringLiteral( "above|%1|%2" ).arg( r.file, m );
			}
		}
		return p;
	};
	const QStringList was = problems( before );
	QStringList now;
	for ( const QString & p : problems( after ) )
		if ( !was.contains( p ) )
			now << p;
	if ( now.isEmpty() )
		return QString();
	QStringList missing;
	QString file;
	for ( const QString & p : std::as_const( now ) ) {
		const QStringList f = p.split( QLatin1Char( '|' ) );
		if ( f.at( 0 ) == QLatin1String( "missing" ) && ( file.isEmpty() || file == f.at( 1 ) ) ) {
			file = f.at( 1 );
			missing << f.at( 2 );
		}
	}
	if ( !missing.isEmpty() )
		return QStringLiteral( "%1 needs %2, not in the list" ).arg( file, missing.join( QStringLiteral( ", " ) ) );
	const QStringList f = now.first().split( QLatin1Char( '|' ) );
	return QStringLiteral( "%1 cannot go above its master %2" ).arg( f.at( 1 ), f.at( 2 ) );
}

// ---- the list

CellModList cellModPickList( const QString & ws, const QString & lodb )
{
	CellModList L;
	L.ws = ws;
	if ( !lodb.isEmpty() ) {
		LodgenLedger led;
		QString err;
		if ( lodgenReadLedger( lodb, &led, &err ) && !led.plugins.isEmpty() ) {
			L.source = QStringLiteral( "bake" );
			L.from = lodb;
			L.label = QStringLiteral( "the far LOD bake %1" ).arg( QFileInfo( lodb ).fileName() );
			for ( const LodbPlugin & p : std::as_const( led.plugins ) ) {
				CellModRow r = sourceRow( p.path, modOfPath( p.path ) );
				if ( r.file.isEmpty() ) {
					r.file = p.name;
					r.key = p.name.toLower();
				}
				L.rows << r;
			}
		} else {
			L.error = QStringLiteral( "the far LOD bake's record %1 could not be read%2" )
				.arg( QDir::toNativeSeparators( lodb ) ).arg( err.isEmpty() ? QString() : QStringLiteral( ": " ) + err );
		}
	}
	if ( L.source.isEmpty() ) {
		const CellModSource s = cellModSource();
		if ( !s.kind.isEmpty() ) {
			L.source = s.kind;
			L.label = s.label;
			L.from = s.from;
			L.note = s.note;
			L.sourceStack = s.stack;
			L.error.clear();
			for ( int i = 0; i < s.plugins.size(); i++ ) {
				CellModRow r = sourceRow( s.plugins.at( i ), i < s.pluginFrom.size() ? s.pluginFrom.at( i ) : QStringLiteral( "Data" ) );
				r.locked = r.locked || ( r.present && i < s.alwaysLoads );  // Fallout4.ccc's files in a plain Data
				L.rows << r;
			}
		} else if ( L.error.isEmpty() ) {
			L.error = s.error;
			L.note = s.note;
		}
	}
	for ( const CellModRow & r : std::as_const( L.rows ) )
		L.naturalKeys << r.key;

	const CellModChoice c = cellModPickChoice( ws );
	for ( const QString & p : c.added ) {
		const CellModRow r = addedRow( p );
		if ( L.naturalKeys.contains( r.key ) )
			continue;
		L.rows << r;
		L.naturalKeys << r.key;
	}
	if ( !c.order.isEmpty() ) {
		auto pos = [&]( const CellModRow & r ) {
			if ( r.locked )
				return -1000000 + int( L.naturalKeys.indexOf( r.key ) );
			const int i = int( c.order.indexOf( r.key ) );
			return i >= 0 ? i : int( c.order.size() + L.naturalKeys.indexOf( r.key ) );
		};
		std::stable_sort( L.rows.begin(), L.rows.end(), [&]( const CellModRow & a, const CellModRow & b ) {
			return pos( a ) < pos( b );
		} );
	}
	// the ticks: the saved choice, else what the last build of this worldspace loaded
	const bool touched = cellModPickTouched( ws );
	const QStringList loaded = namesOf( cellModPickLoaded( ws ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) );
	for ( CellModRow & r : L.rows ) {
		if ( r.locked )
			r.on = true;
		else if ( r.added )
			r.on = r.present && !c.off.contains( r.key );
		else
			r.on = r.present && ( touched ? !c.off.contains( r.key ) : ( loaded.isEmpty() || loaded.contains( r.key ) ) );
	}
	return L;
}

// ---- the build

CellModApplied cellModPickApply( const QString & ws, const QString & plugins, const QString & lodb,
	const QStringList & bakePresent )
{
	CellModApplied r;
	r.plugins = plugins;
	const QStringList asked = plugins.split( QLatin1Char( ',' ), Qt::SkipEmptyParts );
	shownBake().insert( ws.toLower(), lodb );
	QStringList roots;
	bool orderTouched = false;
	if ( !cellModPickTouched( ws ) ) {
		r.line = QStringLiteral( "mods: the picker is untouched -- %1 plugins as the cell asked" ).arg( asked.size() );
	} else if ( red( "inert" ) ) {
		r.line = QStringLiteral( "mods: the saved choice for %1 is NOT applied (RED inert) -- %2 plugins as the cell asked" )
			.arg( ws ).arg( asked.size() );
	} else {
		CellModList L = cellModPickList( ws, lodb );
		const CellModChoice c = cellModPickChoice( ws );
		QStringList reds;
		if ( red( "noadded" ) ) {
			L.rows.erase( std::remove_if( L.rows.begin(), L.rows.end(), []( const CellModRow & x ) { return x.added; } ),
				L.rows.end() );
			reds << QStringLiteral( "RED noadded: hand-added entries ignored" );
		}
		if ( red( "noorder" ) ) {
			std::stable_sort( L.rows.begin(), L.rows.end(), [&]( const CellModRow & a, const CellModRow & b ) {
				return L.naturalKeys.indexOf( a.key ) < L.naturalKeys.indexOf( b.key );
			} );
			reds << QStringLiteral( "RED noorder: the saved order ignored" );
		} else {
			orderTouched = !c.order.isEmpty();
		}
		QStringList kept, dropped;
		bool anyAdded = false;
		for ( const CellModRow & x : std::as_const( L.rows ) ) {
			anyAdded = anyAdded || ( x.added && x.on && x.present );
			if ( x.folder || !x.present )
				continue;
			if ( x.on )
				kept << x.path;
			else
				dropped << x.file;
		}
		if ( kept.isEmpty() ) {
			r.line = QStringLiteral( "mods: the saved choice for %1 leaves no plugin -- NOT applied, %2 plugins as the cell asked" )
				.arg( ws ).arg( asked.size() );
		} else {
			r.plugins = kept.join( QLatin1Char( ',' ) );
			const QStringList start = ( L.source == QLatin1String( "bake" ) || L.source.isEmpty() ) ? lodgenResources()
				: L.sourceStack;
			QStringList stack = start;
			if ( orderTouched || anyAdded )
				stack = orderedStack( start, L.rows, kept, &roots );
			if ( stack != lodgenResources() )
				r.resources = stack;
			QStringList what;
			if ( !dropped.isEmpty() )
				what << QStringLiteral( "unticked: " ) + dropped.join( QStringLiteral( ", " ) );
			if ( !roots.isEmpty() )
				what << QStringLiteral( "hand-added roots: " ) + roots.join( QStringLiteral( ", " ) );
			if ( orderTouched )
				what << QStringLiteral( "his order; plugin archives stacked in plugin order" );
			what << reds;
			r.line = QStringLiteral( "mods: the saved choice for %1 -- %2 plugins from %3%4" )
				.arg( ws ).arg( kept.size() ).arg( L.label.isEmpty() ? QStringLiteral( "the cell's own list" ) : L.label )
				.arg( what.isEmpty() ? QString() : QStringLiteral( "; " ) + what.join( QStringLiteral( "; " ) ) );
		}
		// the hand-added names, for the note
		for ( const CellModRow & x : std::as_const( L.rows ) )
			if ( x.added && x.on && x.present )
				roots << QStringLiteral( "@" ) + x.file;
	}
	loadedStack().insert( ws.toLower(), r.plugins );
	// one stack: say when the near cells' mods are not the shown bake's
	if ( !lodb.isEmpty() && !bakePresent.isEmpty() ) {
		const QStringList nearN = namesOf( r.plugins.split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) );
		const QStringList bakeN = namesOf( bakePresent );
		QStringList addedNames;
		for ( const QString & x : std::as_const( roots ) )
			if ( x.startsWith( QLatin1Char( '@' ) ) )
				addedNames << x.mid( 1 );
		QStringList off, extra;
		for ( const QString & p : bakePresent )
			if ( !nearN.contains( fileOf( p ).toLower() ) )
				off << fileOf( p );
		for ( const QString & p : r.plugins.split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) ) {
			const QString n = fileOf( p );
			if ( !bakeN.contains( n.toLower() ) && !addedNames.contains( n, Qt::CaseInsensitive ) )
				extra << n;
		}
		QStringList nearCommon, bakeCommon;
		for ( const QString & n : nearN )
			if ( bakeN.contains( n ) )
				nearCommon << n;
		for ( const QString & n : bakeN )
			if ( nearN.contains( n ) )
				bakeCommon << n;
		QStringList what;
		if ( !off.isEmpty() )
			what << QStringLiteral( "%1 off" ).arg( listOf( off ) );
		if ( !addedNames.isEmpty() )
			what << QStringLiteral( "added: %1" ).arg( listOf( addedNames ) );
		if ( !extra.isEmpty() )
			what << QStringLiteral( "%1 not in the bake" ).arg( listOf( extra ) );
		if ( nearCommon != bakeCommon )
			what << QStringLiteral( "order changed" );
		if ( !what.isEmpty() ) {
			r.viewNote = QStringLiteral( "Mods differ from the far LOD bake: %1" ).arg( what.join( QStringLiteral( "; " ) ) );
			r.line += QStringLiteral( "\n  mods: DIFFER from the far LOD bake -- %1 (the view says so)" ).arg( what.join( QStringLiteral( "; " ) ) );
		} else {
			r.line += QStringLiteral( "\n  mods: same as the far LOD bake (%1 plugins)" ).arg( bakeN.size() );
		}
	}
	return r;
}

QString cellModPickViewNote( const void * doc )
{
	return viewNotes().value( doc );
}

void cellModPickSetViewNote( const void * doc, const QString & note )
{
	if ( note.isEmpty() )
		viewNotes().remove( doc );
	else
		viewNotes().insert( doc, note );
}
