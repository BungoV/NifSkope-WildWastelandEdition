/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "opencellcatalog.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "lodgenloadorder.h"	// lane OPENCELLFIX1: the masters and the MO2 profile, as the LOD panel reads them

#include "ba2file.hpp"
#include "esmfile.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QStringDecoder>
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

// `strings` is null for a plugin that is not localized; then FULL is the text itself.
void readNames( ESMFile & esm, const ESMFile::ESMRecord & r, const OpenCellStrings * strings, QString & edid,
	QString & name, int * x = nullptr, int * y = nullptr )
{
	ESMFile::ESMField f( esm, r );
	while ( f.next() ) {
		if ( f == "EDID" )
			edid = fieldText( f );
		else if ( f == "FULL" && !strings )
			name = fieldText( f );
		else if ( f == "FULL" && f.size() >= 4 )
			name = strings->value( f.readUInt32() );
		else if ( f == "XCLC" && x && y && f.size() >= 8 ) {
			*x = int( f.readInt32() );
			*y = int( f.readInt32() );
		}
	}
}

quint32 le32( const char * p )
{
	return quint32( quint8( p[0] ) ) | quint32( quint8( p[1] ) ) << 8 | quint32( quint8( p[2] ) ) << 16
		| quint32( quint8( p[3] ) ) << 24;
}

// The game writes UTF-8 for most languages and cp1252 for some; Latin-1 is the near fallback.
QString stringText( const char * p, qsizetype n )
{
	const QByteArray b = QByteArray::fromRawData( p, n );
	QStringDecoder utf8( QStringDecoder::Utf8, QStringDecoder::Flag::Stateless );
	QString s = utf8( b );
	return utf8.hasError() ? QString::fromLatin1( b ) : s;
}

constexpr const char * kStringExt[3] = { "strings", "dlstrings", "ilstrings" };

bool stringsOff()
{
	return qEnvironmentVariable( "WW_OPENCELL_STRINGS" ) == QLatin1String( "0" )
		|| qEnvironmentVariable( "WW_OPENCELL_RED" ) == QLatin1String( "strings" );
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
	// The game loads masters before plugins, and its own master first. A master is the TES4 ESM flag (0x1), not
	// the extension: an ESM-flagged .esp loads with the masters (lane GAPFIX1; .esm/.esl carry it implicitly).
	auto esmFlag = [&]( const QString & n ) {
		QFile f( QDir( dataDir ).filePath( n ) );
		const QByteArray h = f.open( QIODevice::ReadOnly ) ? f.read( 12 ) : QByteArray();
		return h.size() == 12 && h.startsWith( "TES4" ) && ( quint8( h[8] ) & 0x01 ) != 0;
	};
	QHash<QString, int> ranks;
	for ( const QString & n : all ) {
		int r = 1;
		if ( n.compare( QLatin1String( "Fallout4.esm" ), Qt::CaseInsensitive ) == 0 )
			r = 0;
		else if ( n.endsWith( QLatin1String( ".esp" ), Qt::CaseInsensitive ) && !esmFlag( n ) )
			r = 2;
		ranks.insert( n, r );
	}
	std::stable_sort( all.begin(), all.end(), [&]( const QString & a, const QString & b ) {
		return ranks.value( a ) < ranks.value( b );
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
	const bool red = wwEnvStr( "WW_OPENCELL_RED" ) == QLatin1String( "masters" );
	src.resources.clear();
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
		ml.close();
		if ( red )
			return true;
		// lane OPENCELLFIX1: the LOD panel's reader: masters first, Data from ModOrganizer.ini, the folder stack
		const bool haveData = QFileInfo( QDir( src.dataDir ).filePath( QStringLiteral( "Fallout4.esm" ) ) ).isFile();
		LodgenLoadOrder lo;
		QString why;
		if ( lodgenLoadOrderFromMo2( profile.absolutePath(), QString(), haveData ? src.dataDir : QString(), &lo, &why ) ) {
			plugins.clear();
			for ( const QString & p : lo.plugins )
				plugins.append( QFileInfo( p ).fileName() );
			if ( !haveData && !lo.dataDir.isEmpty() )
				src.dataDir = QDir::cleanPath( lo.dataDir );
			src.resources = lo.stack;
			return true;
		}
		if ( !haveData && !src.dataDir.isEmpty() && error )
			*error = why;	// kept for the caller's status; the plain list below still loads
	}
	if ( red )
		return true;
	// The masters the game loads whatever the file says, from the Data folder, ahead of the list.
	QStringList lead;
	QSet<QString> seen;
	for ( const QString & m : lodgenLoadOrderMasters( src.dataDir ) ) {
		const QString n = QFileInfo( m ).fileName();
		lead.append( n );
		seen.insert( n.toLower() );
	}
	for ( const QString & p : plugins )
		if ( !seen.contains( p.toLower() ) )
			lead.append( p );
	plugins = lead;
	return true;
}

bool openCellParseStrings( const QByteArray & bytes, bool lengthPrefixed, OpenCellStrings & out )
{
	// count, data size, count x (id, offset into the data), then the data
	if ( bytes.size() < 8 )
		return false;
	const char * b = bytes.constData();
	const quint32 count = le32( b ), dataSize = le32( b + 4 );
	const qsizetype dataAt = 8 + qsizetype( count ) * 8;
	if ( count > quint32( bytes.size() / 8 ) || dataAt + qsizetype( dataSize ) > bytes.size() )
		return false;
	const char * data = b + dataAt;
	for ( quint32 i = 0; i < count; i++ ) {
		const quint32 id = le32( b + 8 + i * 8 ), off = le32( b + 12 + i * 8 );
		if ( off >= dataSize )
			return false;
		qsizetype at = off, n = 0;
		if ( lengthPrefixed ) {
			if ( qsizetype( off ) + 4 > qsizetype( dataSize ) )
				return false;
			n = le32( data + off );
			at += 4;
			if ( at + n > qsizetype( dataSize ) )
				return false;
		} else {
			while ( at + n < qsizetype( dataSize ) && data[at + n] )
				n++;
		}
		while ( n > 0 && data[at + n - 1] == '\0' )
			n--;
		out.insert( id, stringText( data + at, n ) );
	}
	return true;
}

OpenCellStrings openCellLoadStrings( const QString & pluginPath, const QString & lang, const QString & dataDir,
	bool * found )
{
	OpenCellStrings out;
	if ( found )
		*found = false;
	if ( stringsOff() )
		return out;
	const QFileInfo plugin( pluginPath );
	const QString base = plugin.completeBaseName();
	QStringList dirs{ plugin.absolutePath() };
	if ( !dataDir.isEmpty() && QDir::cleanPath( QFileInfo( dataDir ).absoluteFilePath() ) != QDir::cleanPath( dirs[0] ) )
		dirs.append( dataDir );
	// .STRINGS last, so a FULL id that is in several tables reads the one FULL lives in.
	for ( int t = 2; t >= 0; t-- ) {
		const QString file = QStringLiteral( "%1_%2.%3" ).arg( base, lang, QLatin1String( kStringExt[t] ) );
		QByteArray bytes;
		bool have = false;
		for ( const QString & d : dirs ) {
			QFile f( QDir( d ).filePath( QStringLiteral( "Strings/" ) + file ) );
			if ( f.open( QIODevice::ReadOnly ) ) {
				bytes = f.readAll();
				have = true;
				break;
			}
		}
		for ( int i = 0; !have && i < dirs.size(); i++ ) {
			QStringList archives = QDir( dirs[i] ).entryList( { base + QStringLiteral( " - *.ba2" ) }, QDir::Files,
				QDir::Name | QDir::IgnoreCase );
			// Texture archives hold no tables; the usual homes (Localization, Interface, Main) are opened first.
			archives.removeIf( []( const QString & a ) { return a.contains( QLatin1String( "Textures" ), Qt::CaseInsensitive ); } );
			auto home = []( const QString & a ) {
				for ( const char * k : { "Localization", "Interface", "Main" } )
					if ( a.contains( QLatin1String( k ), Qt::CaseInsensitive ) )
						return 0;
				return 1;
			};
			std::stable_sort( archives.begin(), archives.end(), [&]( const QString & a, const QString & b ) {
				return home( a ) < home( b );
			} );
			const std::string want = ( QStringLiteral( "strings/" ) + file ).toLower().toStdString();
			for ( const QString & a : archives ) {
				try {
					BA2File ba2( QDir( dirs[i] ).filePath( a ).toLocal8Bit().constData(),
						[]( void * w, const std::string_view & n ) { return n == *static_cast<const std::string *>( w ); },
						const_cast<std::string *>( &want ) );
					if ( !ba2.findFile( want ) )
						continue;
					BA2File::UCharArray buf;
					ba2.extractFile( buf, want );
					bytes = QByteArray( reinterpret_cast<const char *>( buf.data ), qsizetype( buf.size ) );
					have = true;
					break;
				} catch ( std::exception & ) {
					// an unreadable archive is skipped like a missing one
				}
			}
		}
		if ( have && openCellParseStrings( bytes, t != 0, out ) && found )
			*found = true;
	}
	return out;
}

QString openCellStringsLanguage()
{
	const QString l = QSettings().value( settingsKey( "Language" ) ).toString().trimmed();
	return l.isEmpty() ? QStringLiteral( "en" ) : l;
}

void openCellSetStringsLanguage( const QString & lang )
{
	QSettings().setValue( settingsKey( "Language" ), lang.trimmed() );
}

quint32 openCellShownForm( quint32 form, int ownerFile, const QVector<bool> & light )
{
	if ( ( form >> 24 ) == 0xFEU || ownerFile < 0 || ownerFile >= light.size() || light[ownerFile] )
		return form;    // a light plugin's FE + slot form is already the game's
	int full = 0;
	for ( int i = 0; i < ownerFile; i++ )
		full += !light[i];
	return ( quint32( full ) << 24 ) | ( form & 0x00FFFFFFU );
}

bool openCellReadCatalog( const QStringList & paths, OpenCellCatalog & out, QString * error,
	const OpenCellReadOptions & opt )
{
	out = OpenCellCatalog();
	if ( paths.isEmpty() ) {
		if ( error )
			*error = QStringLiteral( "no plugin ticked" );
		return false;
	}
	const QVector<bool> localized = localizedFlags( paths );
	const QString lang = opt.lang.isEmpty() ? openCellStringsLanguage() : opt.lang;
	QVector<OpenCellStrings> tables( paths.size() );
	for ( int i = 0; i < paths.size(); i++ ) {
		bool found = false;
		if ( localized[i] )
			tables[i] = openCellLoadStrings( paths[i], lang, opt.dataDir, &found );
		if ( localized[i] && !found )
			out.noStrings.append( QFileInfo( paths[i] ).fileName() );
	}
	auto stringsOf = [&]( const ESMFile::ESMRecord & r ) -> const OpenCellStrings * {
		const bool loc = r.srcFile < unsigned( localized.size() ) && localized[int( r.srcFile )];
		return loc ? &tables[int( r.srcFile )] : nullptr;
	};
	try {
		ESMFile esm( paths.join( QLatin1Char( ',' ) ).toLocal8Bit().constData() );
		for ( const QString & p : paths )
			out.plugins.append( QFileInfo( p ).fileName() );
		// The reader's light slots: FE + slot names file lightFiles[slot]; the first slot past the end reads past the
		// list (or, with no light plugin, the FE byte itself).
		QVector<bool> light( paths.size(), false );
		for ( quint32 s = 0; s < 0x1000U; s++ ) {
			const unsigned int f = esm.getFormIDFile( 0xFE000000U | ( s << 12 ) );
			if ( f >= unsigned( paths.size() ) || light[int( f )] )
				break;
			light[int( f )] = true;
		}
		// One record per form: an override replaces the master's data in place, so `r` is already the winner
		// (its srcFile is the last plugin to edit it) and its names are the ones the CK shows.
		auto stamp = [&]( OpenCellCell & c, const ESMFile::ESMRecord & r ) {
			const unsigned int ownerFile = esm.getFormIDFile( r.formID );
			c.owner = ownerFile < unsigned( paths.size() ) ? int( ownerFile ) : -1;
			c.lastEdit = r.srcFile < unsigned( paths.size() ) ? int( r.srcFile ) : -1;
			c.shownForm = openCellShownForm( r.formID, c.owner, light );
		};
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
					readNames( esm, *r, stringsOf( *r ), w.edid, w.name );
					out.worlds.append( w );
				} else if ( !worldTop && *r == "CELL" ) {
					OpenCellCell c;
					c.form = r->formID;
					readNames( esm, *r, stringsOf( *r ), c.edid, c.name, &c.x, &c.y );
					stamp( c, *r );
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
	if ( wwEnvStr( "WW_OPENCELL_RED" ) == QLatin1String( "writer" ) )
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
