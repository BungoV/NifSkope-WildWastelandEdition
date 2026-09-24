#include "lodgenloadorder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTextStream>

/* See lodgenloadorder.h. Lane LOADORDER1, 2026-09-24. Read only: nothing in
 * this file opens anything for writing. */

namespace
{

QString clean( const QString & p )
{
	return QDir::cleanPath( QDir::fromNativeSeparators( p.trimmed() ) );
}

QString lowerBase( const QString & p )
{
	return QFileInfo( p ).fileName().toLower();
}

//! the text lines of a small file, CR stripped; false when it cannot be opened
bool readLines( const QString & path, QStringList * lines )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return false;
	const QString all = QString::fromUtf8( f.readAll() );
	for ( QString l : all.split( QChar( '\n' ) ) ) {
		if ( l.endsWith( QChar( '\r' ) ) )
			l.chop( 1 );
		lines->append( l );
	}
	return true;
}

//! the value of `key=` in an MO2 ini, @ByteArray(...) unwrapped and "\\" undoubled
QString iniValue( const QStringList & lines, const QString & key )
{
	const QString k = key + QChar( '=' );
	for ( const QString & l : lines ) {
		if ( !l.startsWith( k, Qt::CaseInsensitive ) )
			continue;
		QString v = l.mid( k.size() ).trimmed();
		if ( v.startsWith( QLatin1String( "@ByteArray(" ) ) && v.endsWith( QChar( ')' ) ) )
			v = v.mid( 11, v.size() - 12 );
		v.replace( QLatin1String( "\\\\" ), QLatin1String( "\\" ) );
		return v;
	}
	return QString();
}

//! the stem an archive belongs to: "BNS Trees - Main.ba2" -> "bns trees"
QString archiveStem( const QString & name )
{
	QString s = name;
	const int dash = s.indexOf( QLatin1String( " - " ) );
	if ( dash > 0 )
		s = s.left( dash );
	else if ( s.endsWith( QLatin1String( ".ba2" ), Qt::CaseInsensitive ) )
		s.chop( 4 );
	return s.toLower();
}

QString pluginStem( const QString & path )
{
	QString b = lowerBase( path );
	for ( const char * e : { ".esm", ".esp", ".esl" } )
		if ( b.endsWith( QLatin1String( e ) ) ) {
			b.chop( 4 );
			break;
		}
	return b;
}

} // namespace

QStringList lodgenLoadOrderMasters( const QString & dataDir )
{
	QStringList out;
	const QDir d( clean( dataDir ) );
	// the engine's fixed order (what MO2's own loadorder.txt carries too)
	for ( const char * n : { "Fallout4.esm", "DLCRobot.esm", "DLCworkshop01.esm", "DLCCoast.esm",
			"DLCworkshop02.esm", "DLCworkshop03.esm", "DLCNukaWorld.esm", "DLCUltraHighResolution.esm" } ) {
		const QString p = d.filePath( QLatin1String( n ) );
		if ( QFileInfo( p ).isFile() )
			out.append( QDir::cleanPath( p ) );
	}
	/* The Creation Club files: the game loads the ones Fallout4.ccc names that
	 * are present in Data, in that file's order. The .ccc sits beside Data. */
	QStringList ccc;
	if ( readLines( QDir::cleanPath( d.absolutePath() + QStringLiteral( "/../Fallout4.ccc" ) ), &ccc ) ) {
		QSet<QString> seen;
		for ( const QString & p : out )
			seen.insert( lowerBase( p ) );
		for ( const QString & line : ccc ) {
			const QString n = line.trimmed();
			if ( n.isEmpty() || seen.contains( n.toLower() ) )
				continue;
			const QString p = d.filePath( n );
			if ( QFileInfo( p ).isFile() ) {
				out.append( QDir::cleanPath( p ) );
				seen.insert( n.toLower() );
			}
		}
	}
	return out;
}

QString lodgenMo2GameData( const QString & instanceBase, QString * from )
{
	const QString base = clean( instanceBase );
	QStringList inis;
	inis << base + QStringLiteral( "/ModOrganizer.ini" );    // a portable instance
	const QString local = QString::fromLocal8Bit( qgetenv( "LOCALAPPDATA" ) );
	if ( !local.isEmpty() ) {
		const QDir mo( clean( local ) + QStringLiteral( "/ModOrganizer" ) );
		for ( const QString & sub : mo.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name ) )
			inis << mo.filePath( sub + QStringLiteral( "/ModOrganizer.ini" ) );
	}
	for ( int i = 0; i < inis.size(); i++ ) {
		QStringList lines;
		if ( !readLines( inis.at( i ), &lines ) )
			continue;
		if ( i > 0 ) {
			// a global instance: only the one whose base_directory is this folder
			const QString b = clean( iniValue( lines, QStringLiteral( "base_directory" ) ) );
			if ( b.compare( base, Qt::CaseInsensitive ) != 0 )
				continue;
		}
		const QString game = clean( iniValue( lines, QStringLiteral( "gamePath" ) ) );
		if ( game.isEmpty() )
			continue;
		if ( from )
			*from = QDir::cleanPath( inis.at( i ) );
		return game + QStringLiteral( "/Data" );
	}
	return QString();
}

bool lodgenLoadOrderFromMo2( const QString & profileDir, const QString & modsDir,
	const QString & dataDir, LodgenLoadOrder * out, QString * error )
{
	LodgenLoadOrder lo;
	auto refuse = [&]( const QString & why ) {
		if ( error )
			*error = why;
		return false;
	};
	lo.profileDir = clean( profileDir );
	const QString instance = QDir::cleanPath( lo.profileDir + QStringLiteral( "/../.." ) );
	lo.modsDir = modsDir.isEmpty() ? instance + QStringLiteral( "/mods" ) : clean( modsDir );
	if ( !dataDir.isEmpty() ) {
		lo.dataDir = clean( dataDir );
		lo.dataFrom = QStringLiteral( "--data-root" );
	} else {
		lo.dataDir = lodgenMo2GameData( instance, &lo.dataFrom );
	}

	QStringList modlist, pluginsTxt;
	if ( !readLines( lo.profileDir + QStringLiteral( "/modlist.txt" ), &modlist ) )
		return refuse( QStringLiteral( "cannot read %1/modlist.txt (is --mo2-profile a profile folder?)" ).arg( lo.profileDir ) );
	if ( !readLines( lo.profileDir + QStringLiteral( "/plugins.txt" ), &pluginsTxt ) )
		return refuse( QStringLiteral( "cannot read %1/plugins.txt" ).arg( lo.profileDir ) );
	if ( !QFileInfo( lo.modsDir ).isDir() )
		return refuse( QStringLiteral( "no mods folder at %1 (name it with --mo2-mods)" ).arg( lo.modsDir ) );
	if ( lo.dataDir.isEmpty() )
		return refuse( QStringLiteral( "no game Data folder: no ModOrganizer.ini names one for %1; pass --data-root <Fallout 4/Data>" ).arg( instance ) );
	if ( !QFileInfo( lo.dataDir ).isDir() )
		return refuse( QStringLiteral( "the game Data folder %1 (from %2) is not a folder" ).arg( lo.dataDir, lo.dataFrom ) );
	const QString ow = instance + QStringLiteral( "/overwrite" );
	if ( QFileInfo( ow ).isDir() )
		lo.overwriteDir = ow;

	// modlist.txt: top line = highest priority. `enabled` keeps that order.
	QStringList enabled;
	for ( const QString & raw : modlist ) {
		const QString l = raw.trimmed();
		if ( l.isEmpty() || l.startsWith( QChar( '#' ) ) || l.startsWith( QChar( '*' ) ) )
			continue;
		const QChar mark = l.at( 0 );
		const QString name = l.mid( 1 );
		if ( name.endsWith( QLatin1String( "_separator" ) ) ) {
			lo.separators++;
			continue;
		}
		if ( mark == QChar( '-' ) ) {
			lo.modsDisabled++;
			continue;
		}
		if ( mark != QChar( '+' ) )
			continue;
		lo.modsEnabled++;
		const QString dir = lo.modsDir + QChar( '/' ) + name;
		if ( !QFileInfo( dir ).isDir() ) {
			lo.modsMissing << name;
			continue;
		}
		enabled << name;
	}

	// the plugins: masters from Data, then plugins.txt's `*` lines
	lo.plugins = lodgenLoadOrderMasters( lo.dataDir );
	if ( lo.plugins.isEmpty() || lowerBase( lo.plugins.first() ) != QLatin1String( "fallout4.esm" ) )
		return refuse( QStringLiteral( "Fallout4.esm is not in %1" ).arg( lo.dataDir ) );
	lo.masters = int( lo.plugins.size() );
	for ( int i = 0; i < lo.plugins.size(); i++ )
		lo.pluginFrom << QStringLiteral( "data" );
	QSet<QString> have;
	for ( const QString & p : lo.plugins )
		have.insert( lowerBase( p ) );
	int lineNo = 0;
	for ( const QString & raw : pluginsTxt ) {
		lineNo++;
		const QString l = raw.trimmed();
		if ( !l.startsWith( QChar( '*' ) ) )
			continue;       // a comment, or a plugin MO2 has unticked
		const QString name = l.mid( 1 ).trimmed();
		if ( name.isEmpty() || have.contains( name.toLower() ) )
			continue;
		QString found, from;
		if ( !lo.overwriteDir.isEmpty() && QFileInfo( lo.overwriteDir + QChar( '/' ) + name ).isFile() ) {
			found = lo.overwriteDir + QChar( '/' ) + name;
			from = QStringLiteral( "overwrite" );
		}
		for ( int m = 0; found.isEmpty() && m < enabled.size(); m++ ) {
			const QString p = lo.modsDir + QChar( '/' ) + enabled.at( m ) + QChar( '/' ) + name;
			if ( QFileInfo( p ).isFile() ) {
				found = p;
				from = enabled.at( m );
			}
		}
		if ( found.isEmpty() && QFileInfo( lo.dataDir + QChar( '/' ) + name ).isFile() ) {
			found = lo.dataDir + QChar( '/' ) + name;
			from = QStringLiteral( "data" );
		}
		if ( found.isEmpty() )
			return refuse( QStringLiteral( "plugin '%1' (plugins.txt line %2) is in no enabled mod folder, "
				"not in overwrite and not in %3" ).arg( name ).arg( lineNo ).arg( lo.dataDir ) );
		/* The plugin list travels as ONE comma-joined string (EsmWorld, the
		 * load-order hash, the bake record all split it on ','), so a comma
		 * inside a path would cut it in two somewhere downstream. */
		if ( found.contains( QChar( ',' ) ) )
			return refuse( QStringLiteral( "plugin '%1' resolves to %2, and a comma in the path breaks "
				"the comma-joined plugin list" ).arg( name, found ) );
		lo.plugins << QDir::cleanPath( found );
		lo.pluginFrom << from;
		have.insert( name.toLower() );
	}

	// the stack, lowest first: Data, then the mods bottom-up, then overwrite
	lo.stack << lo.dataDir;
	lo.stackFrom << QStringLiteral( "data" );
	for ( int m = int( enabled.size() ) - 1; m >= 0; m-- ) {
		lo.stack << lo.modsDir + QChar( '/' ) + enabled.at( m );
		lo.stackFrom << QStringLiteral( "mod " ) + enabled.at( m );
	}
	if ( !lo.overwriteDir.isEmpty() ) {
		lo.stack << lo.overwriteDir;
		lo.stackFrom << QStringLiteral( "overwrite" );
	}

	/* Archives the game would NOT load: FO4 opens `<plugin> - *.ba2` for a
	 * loaded plugin only. The stack keeps them (a mod folder is one entry);
	 * this list says so rather than letting it pass unseen. */
	QSet<QString> stems;
	for ( const QString & p : lo.plugins )
		stems.insert( pluginStem( p ) );
	for ( const QString & name : enabled ) {
		const QDir d( lo.modsDir + QChar( '/' ) + name );
		for ( const QString & a : d.entryList( QStringList{ QStringLiteral( "*.ba2" ) }, QDir::Files, QDir::Name ) )
			if ( !stems.contains( archiveStem( a ) ) )
				lo.archivesNoPlugin << name + QChar( '/' ) + a;
	}

	*out = lo;
	if ( error )
		error->clear();
	return true;
}

bool lodgenLoadOrderFromPluginsTxt( const QString & dataDir, const QStringList & names,
	QStringList * resolved, QString * error )
{
	QStringList r = lodgenLoadOrderMasters( dataDir );
	QSet<QString> have;
	for ( const QString & p : r )
		have.insert( lowerBase( p ) );
	for ( const QString & n : names ) {
		if ( have.contains( n.toLower() ) )
			continue;
		const QString full = QDir( clean( dataDir ) ).filePath( n );
		if ( !QFileInfo( full ).isFile() ) {
			if ( error )
				*error = QStringLiteral( "plugin '%1' is not in %2: a mod's plugin sits in its MO2 mod folder, "
					"not in Data -- use --mo2-profile <profile folder> to read his load order off disk" )
					.arg( n, clean( dataDir ) );
			return false;
		}
		r << QDir::cleanPath( full );
		have.insert( n.toLower() );
	}
	*resolved = r;
	return true;
}

bool lodgenApplyMo2Profile( const QString & profileDir, const QString & modsDir,
	const QString & dataRoot, const QStringList & extraResources, bool otherSource,
	QStringList * stack, QString * file, QTextStream & out, QTextStream & err )
{
	if ( otherSource ) {
		err << "error: --mo2-profile is a whole load order; it does not combine with --plugins-txt or --mo2"
			<< Qt::endl;
		return false;
	}
	LodgenLoadOrder lo;
	QString why;
	if ( !lodgenLoadOrderFromMo2( profileDir, modsDir, dataRoot, &lo, &why ) ) {
		err << "error: --mo2-profile refused: " << why << Qt::endl;
		return false;
	}
	/* One fact a line, keyword first, so a harness reads it with a grep. The
	 * `plugin N:` lines are the same words --plugins-txt has always printed. */
	out << "mo2-profile: " << lo.profileDir << Qt::endl;
	out << "mo2-mods: " << lo.modsDir << Qt::endl;
	out << "mo2-data: " << lo.dataDir << " (from " << lo.dataFrom << ")" << Qt::endl;
	out << "mo2-overwrite: " << ( lo.overwriteDir.isEmpty() ? QStringLiteral( "(none)" ) : lo.overwriteDir ) << Qt::endl;
	out << "mo2-mods-enabled: " << lo.modsEnabled << Qt::endl;
	out << "mo2-mods-disabled: " << lo.modsDisabled << Qt::endl;
	out << "mo2-separators: " << lo.separators << Qt::endl;
	out << "mo2-mods-missing: " << lo.modsMissing.size() << Qt::endl;
	for ( const QString & m : lo.modsMissing )
		out << "mo2-mod-missing: " << m << Qt::endl;
	out << "mo2-archives-without-plugin: " << lo.archivesNoPlugin.size() << Qt::endl;
	for ( const QString & a : lo.archivesNoPlugin )
		out << "mo2-archive-without-plugin: " << a << Qt::endl;
	out << "mo2-masters: " << lo.masters << Qt::endl;
	out << "plugins: " << lo.plugins.size() << Qt::endl;
	for ( int i = 0; i < lo.plugins.size(); i++ )
		out << "plugin " << i << ": " << lo.plugins.at( i ) << Qt::endl;
	for ( int i = 0; i < lo.plugins.size(); i++ )
		out << "plugin-from " << i << ": " << lo.pluginFrom.at( i ) << Qt::endl;
	for ( int i = 0; i < lo.stack.size(); i++ )
		out << "mo2-stack " << i << ": " << lo.stackFrom.at( i ) << Qt::endl;

	*stack = lo.stack + extraResources;
	*file = lo.plugins.join( QChar( ',' ) );
	return true;
}
