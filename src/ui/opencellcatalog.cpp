/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "opencellcatalog.h"

#include "esmfile.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

#include <algorithm>
#include <functional>

namespace {

constexpr unsigned int kGrup = 0x50555247U;   // "GRUP"

QString fieldText( const ESMFile::ESMField & f )
{
	const char * p = reinterpret_cast<const char *>( f.data() );
	qsizetype n = qsizetype( f.size() );
	while ( n > 0 && p[n - 1] == '\0' )
		n--;
	return QString::fromLatin1( p, n );
}

bool isPluginName( const QString & n )
{
	const QString s = n.toLower();
	return s.endsWith( QLatin1String( ".esm" ) ) || s.endsWith( QLatin1String( ".esp" ) )
		|| s.endsWith( QLatin1String( ".esl" ) );
}

QString settingsKey( const char * k )
{
	return QStringLiteral( "WW/OpenCell/" ) + QLatin1String( k );
}

// A plugin's localized flag decides whether FULL is text or a string-table index.
QVector<bool> localizedFlags( const QStringList & paths )
{
	QVector<bool> out;
	for ( const QString & p : paths ) {
		bool loc = false;
		openCellReadHeader( p, nullptr, &loc, nullptr );
		out.append( loc );
	}
	return out;
}

void readNames( ESMFile & esm, const ESMFile::ESMRecord & r, bool localized, QString & edid, QString & name,
	int * x = nullptr, int * y = nullptr )
{
	ESMFile::ESMField f( esm, r );
	while ( f.next() ) {
		if ( f == "EDID" )
			edid = fieldText( f );
		else if ( f == "FULL" && !localized )
			name = fieldText( f );
		else if ( f == "XCLC" && x && y && f.size() >= 8 ) {
			*x = int( f.readInt32() );
			*y = int( f.readInt32() );
		}
	}
}

// The world-children group (type 1) above an exterior cell, or 0 for a cell under no world.
quint32 worldOf( const ESMFile & esm, const ESMFile::ESMRecord & r )
{
	const ESMFile::ESMRecord * g = esm.findRecord( r.parent );
	while ( g && g->type == kGrup ) {
		if ( g->formID == 1 )
			return g->flags;
		g = esm.findRecord( g->parent );
	}
	return 0;
}

} // namespace

QString OpenCellSources::resolve( const QString & plugin ) const
{
	for ( const QString & d : modDirs ) {
		const QString p = QDir( d ).filePath( plugin );
		if ( QFileInfo::exists( p ) )
			return QDir::cleanPath( p );
	}
	const QString p = QDir( dataDir ).filePath( plugin );
	return QFileInfo::exists( p ) ? QDir::cleanPath( p ) : QString();
}

bool openCellReadHeader( const QString & path, QStringList * masters, bool * localized, QString * error )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		if ( error )
			*error = QStringLiteral( "could not read %1" ).arg( path );
		return false;
	}
	const QByteArray head = f.read( 24 );
	if ( head.size() < 24 || !head.startsWith( "TES4" ) ) {
		if ( error )
			*error = QStringLiteral( "%1 is not a plugin" ).arg( QFileInfo( path ).fileName() );
		return false;
	}
	auto u32 = []( const char * p ) {
		return quint32( quint8( p[0] ) ) | quint32( quint8( p[1] ) ) << 8 | quint32( quint8( p[2] ) ) << 16
			| quint32( quint8( p[3] ) ) << 24;
	};
	const quint32 size = u32( head.constData() + 4 );
	if ( localized )
		*localized = ( u32( head.constData() + 8 ) & 0x80 ) != 0;
	const QByteArray body = f.read( size );
	for ( int pos = 0; pos + 6 <= body.size(); ) {
		const int len = quint8( body[pos + 4] ) | quint8( body[pos + 5] ) << 8;
		if ( pos + 6 + len > body.size() )
			break;
		if ( masters && body.mid( pos, 4 ) == "MAST" ) {
			QByteArray n = body.mid( pos + 6, len );
			while ( n.endsWith( '\0' ) )
				n.chop( 1 );
			masters->append( QString::fromLatin1( n ) );
		}
		pos += 6 + len;
	}
	return true;
}

QStringList openCellWithMasters( const QStringList & plugins, const OpenCellSources & src, QStringList * missing )
{
	QStringList out;
	QSet<QString> seen;     // lower-case names, to stop a master cycle as well as duplicates
	std::function<void( const QString & )> add = [&]( const QString & name ) {
		if ( seen.contains( name.toLower() ) )
			return;
		seen.insert( name.toLower() );
		const QString path = src.resolve( name );
		if ( path.isEmpty() ) {
			if ( missing )
				missing->append( name );
			return;
		}
		QStringList masters;
		openCellReadHeader( path, &masters, nullptr, nullptr );
		for ( const QString & m : masters )
			add( m );
		out.append( name );
	};
	for ( const QString & p : plugins )
		add( p );
	return out;
}

QStringList openCellDataPlugins( const QString & dataDir )
{
	QStringList all = QDir( dataDir ).entryList( QDir::Files, QDir::Name | QDir::IgnoreCase );
	all.erase( std::remove_if( all.begin(), all.end(), []( const QString & n ) { return !isPluginName( n ); } ),
		all.end() );
	// The game loads masters before plugins, and its own master first.
	auto rank = []( const QString & n ) {
		if ( n.compare( QLatin1String( "Fallout4.esm" ), Qt::CaseInsensitive ) == 0 )
			return 0;
		return n.endsWith( QLatin1String( ".esp" ), Qt::CaseInsensitive ) ? 2 : 1;
	};
	std::stable_sort( all.begin(), all.end(), [&]( const QString & a, const QString & b ) {
		return rank( a ) < rank( b );
	} );
	return all;
}

QStringList openCellParsePluginsTxt( const QString & text )
{
	QStringList active, every;
	bool starred = false;
	for ( QString line : text.split( QLatin1Char( '\n' ) ) ) {
		line = line.trimmed();
		if ( line.isEmpty() || line.startsWith( QLatin1Char( '#' ) ) )
			continue;
		const bool on = line.startsWith( QLatin1Char( '*' ) );
		starred |= on;
		const QString name = on ? line.mid( 1 ).trimmed() : line;
		if ( !isPluginName( name ) )
			continue;
		every.append( name );
		if ( on )
			active.append( name );
	}
	return starred ? active : every;
}

QStringList openCellParseModlist( const QString & text )
{
	QStringList out;
	for ( QString line : text.split( QLatin1Char( '\n' ) ) ) {
		line = line.trimmed();
		if ( line.startsWith( QLatin1Char( '+' ) ) )
			out.append( line.mid( 1 ) );
	}
	return out;
}

bool openCellImportLoadOrder( const QString & pluginsTxt, QStringList & plugins, OpenCellSources & src,
	QString * error )
{
	QFile f( pluginsTxt );
	if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		if ( error )
			*error = QStringLiteral( "could not read %1" ).arg( pluginsTxt );
		return false;
	}
	plugins = openCellParsePluginsTxt( QString::fromUtf8( f.readAll() ) );
	// An MO2 profile keeps modlist.txt beside plugins.txt; its mods live in <MO2>/mods.
	const QDir profile = QFileInfo( pluginsTxt ).absoluteDir();
	QFile ml( profile.filePath( QStringLiteral( "modlist.txt" ) ) );
	if ( ml.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		QDir root = profile;
		root.cdUp();
		root.cdUp();
		src.modDirs.clear();
		src.modDirs.append( root.filePath( QStringLiteral( "overwrite" ) ) );
		for ( const QString & m : openCellParseModlist( QString::fromUtf8( ml.readAll() ) ) )
			src.modDirs.append( root.filePath( QStringLiteral( "mods/" ) + m ) );
	}
	return true;
}

bool openCellReadCatalog( const QStringList & paths, OpenCellCatalog & out, QString * error )
{
	out = OpenCellCatalog();
	if ( paths.isEmpty() ) {
		if ( error )
			*error = QStringLiteral( "no plugin ticked" );
		return false;
	}
	const QVector<bool> localized = localizedFlags( paths );
	auto isLocalized = [&]( const ESMFile::ESMRecord & r ) {
		return r.srcFile < unsigned( localized.size() ) && localized[int( r.srcFile )];
	};
	try {
		ESMFile esm( paths.join( QLatin1Char( ',' ) ).toLocal8Bit().constData() );
		QSet<quint32> seen;
		// Group ids are dense from 0x80000000: every group of every plugin, linked or not.
		for ( quint32 k = 0;; k++ ) {
			const ESMFile::ESMRecord * g = esm.findRecord( 0x80000000U | k );
			if ( !g )
				break;
			if ( g->type != kGrup )
				continue;
			const quint32 gtype = g->formID;
			const bool worldTop = gtype == 0 && FileBuffer::checkType( g->flags, "WRLD" );
			const bool interiorBlock = gtype == 2 || gtype == 3;
			const bool exteriorBlock = gtype == 4 || gtype == 5;
			if ( !worldTop && !interiorBlock && !exteriorBlock )
				continue;
			for ( quint32 id = g->children; id; ) {
				const ESMFile::ESMRecord * r = esm.findRecord( id );
				if ( !r )
					break;
				id = r->next;
				if ( r->type == kGrup || seen.contains( r->formID ) )
					continue;
				if ( worldTop && *r == "WRLD" ) {
					OpenCellWorld w;
					w.form = r->formID;
					readNames( esm, *r, isLocalized( *r ), w.edid, w.name );
					out.worlds.append( w );
				} else if ( !worldTop && *r == "CELL" ) {
					OpenCellCell c;
					c.form = r->formID;
					readNames( esm, *r, isLocalized( *r ), c.edid, c.name, &c.x, &c.y );
					if ( interiorBlock )
						out.interiors.append( c );
					else
						out.cells[worldOf( esm, *r )].append( c );
				} else {
					continue;
				}
				seen.insert( r->formID );
			}
		}
	} catch ( std::exception & e ) {
		if ( error )
			*error = QString::fromLatin1( e.what() );
		return false;
	}
	for ( auto it = out.cells.begin(); it != out.cells.end(); ++it )
		std::sort( it->begin(), it->end(), []( const OpenCellCell & a, const OpenCellCell & b ) {
			return a.y != b.y ? a.y < b.y : a.x < b.x;
		} );
	std::sort( out.interiors.begin(), out.interiors.end(), []( const OpenCellCell & a, const OpenCellCell & b ) {
		return a.edid.compare( b.edid, Qt::CaseInsensitive ) < 0;
	} );
	return true;
}

QString openCellExteriorLine( const QStringList & paths, const QString & world, int x, int y, int n )
{
	// WW_OPENCELL_RED=writer swaps x and y: the unit test's red must catch it.
	if ( qEnvironmentVariable( "WW_OPENCELL_RED" ) == QLatin1String( "writer" ) )
		std::swap( x, y );
	return QStringLiteral( "%1|%2|%3,%4|%5" ).arg( paths.join( QLatin1Char( ',' ) ), world ).arg( x ).arg( y ).arg( n );
}

QString openCellInteriorLine( const QStringList & paths, const QString & cell )
{
	return QStringLiteral( "%1|interior|%2" ).arg( paths.join( QLatin1Char( ',' ) ), cell );
}

static QString safeName( QString s )
{
	for ( QChar & c : s )
		if ( !c.isLetterOrNumber() && c != QLatin1Char( '-' ) && c != QLatin1Char( '_' ) )
			c = QLatin1Char( '_' );
	return s;
}

QString openCellFileName( const QString & world, int x, int y, int n )
{
	return QStringLiteral( "%1_%2_%3_%4.wwcell" ).arg( safeName( world ) ).arg( x ).arg( y ).arg( n );
}

QString openCellInteriorFileName( const QString & cell )
{
	return QStringLiteral( "interior_%1.wwcell" ).arg( safeName( cell ) );
}

bool openCellWriteSpec( const QString & path, const QString & line, QString * error )
{
	QDir().mkpath( QFileInfo( path ).absolutePath() );
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
		if ( error )
			*error = QStringLiteral( "could not write %1" ).arg( path );
		return false;
	}
	// No Text mode: the file is LF on every platform, so a test can pin its bytes.
	QTextStream out( &f );
	out << line << '\n';
	out.flush();
	return f.error() == QFile::NoError;
}

QStringList openCellRecent()
{
	QStringList l = QSettings().value( settingsKey( "Recent" ) ).toStringList();
	l.erase( std::remove_if( l.begin(), l.end(), []( const QString & p ) { return !QFileInfo::exists( p ); } ),
		l.end() );
	return l.mid( 0, openCellRecentMax );
}

void openCellRecentAdd( const QString & path )
{
	const QString p = QFileInfo( path ).absoluteFilePath();
	QStringList l = QSettings().value( settingsKey( "Recent" ) ).toStringList();
	l.removeAll( p );
	l.prepend( p );
	QSettings().setValue( settingsKey( "Recent" ), l.mid( 0, openCellRecentMax ) );
}

QString openCellSpecDir()
{
	const QStringList r = openCellRecent();
	if ( !r.isEmpty() )
		return QFileInfo( r.first() ).absolutePath();
	return QStandardPaths::writableLocation( QStandardPaths::AppDataLocation ) + QStringLiteral( "/cells" );
}
