/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

#include "cellmodsource.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "gamemanager.h"
#include "lodgenloadorder.h"
#include "landlodspec.h"	// lane LANDSPEC8: gridsLine

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

// see cellmodsource.h

namespace {

QString clean( const QString & p )
{
	return QDir::cleanPath( QDir::fromNativeSeparators( p.trimmed() ) );
}

QStringList readLines( const QString & path )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return {};
	QStringList out;
	for ( QString l : QString::fromUtf8( f.readAll() ).split( QLatin1Char( '\n' ) ) ) {
		if ( l.endsWith( QLatin1Char( '\r' ) ) )
			l.chop( 1 );
		out << l;
	}
	return out;
}

//! `key=` anywhere in an MO2 ini (its keys are unique across sections), @ByteArray(...) unwrapped, "\\" undoubled
QString mo2Value( const QStringList & lines, const QString & key )
{
	const QString k = key + QLatin1Char( '=' );
	for ( const QString & l : lines ) {
		if ( !l.startsWith( k, Qt::CaseInsensitive ) )
			continue;
		QString v = l.mid( k.size() ).trimmed();
		if ( v.startsWith( QLatin1String( "@ByteArray(" ) ) && v.endsWith( QLatin1Char( ')' ) ) )
			v = v.mid( 11, v.size() - 12 );
		v.replace( QLatin1String( "\\\\" ), QLatin1String( "\\" ) );
		return v;
	}
	return QString();
}

bool sameDir( const QString & a, const QString & b )
{
	return !a.isEmpty() && clean( a ).compare( clean( b ), Qt::CaseInsensitive ) == 0;
}

//! NifSkope's configured Fallout 4 folder, else the registry's (empty: neither)
QString gameFolder( QString * from )
{
	QString g = clean( Game::GameManager::path( Game::FALLOUT_4 ) );
	if ( !g.isEmpty() && QFileInfo( g + QStringLiteral( "/Data" ) ).isDir() ) {
		*from = QStringLiteral( "NifSkope's Fallout 4 game path" );
		return g;
	}
	const QSettings reg( QStringLiteral( "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Bethesda Softworks\\Fallout4" ),
		QSettings::NativeFormat );
	g = clean( reg.value( QStringLiteral( "Installed Path" ) ).toString() );
	if ( !g.isEmpty() && QFileInfo( g + QStringLiteral( "/Data" ) ).isDir() ) {
		*from = QStringLiteral( "the registry's Fallout 4 Installed Path" );
		return g;
	}
	return QString();
}

struct Mo2Instance
{
	QString ini, base, game, profile, mods;
};

//! a Fallout 4 MO2 instance from its ini (`portableBase`: the folder a portable ini sits in)
bool readInstance( const QString & ini, const QString & portableBase, Mo2Instance * out )
{
	const QStringList lines = readLines( ini );
	if ( lines.isEmpty() )
		return false;
	const QString gameName = mo2Value( lines, QStringLiteral( "gameName" ) );
	if ( !gameName.isEmpty() && gameName.compare( QLatin1String( "Fallout 4" ), Qt::CaseInsensitive ) != 0 )
		return false;
	Mo2Instance m;
	m.ini = clean( ini );
	m.game = clean( mo2Value( lines, QStringLiteral( "gamePath" ) ) );
	if ( gameName.isEmpty() && !QFileInfo( m.game + QStringLiteral( "/Fallout4.exe" ) ).isFile() )
		return false;
	m.base = clean( mo2Value( lines, QStringLiteral( "base_directory" ) ) );
	if ( m.base.isEmpty() )
		m.base = portableBase.isEmpty() ? QFileInfo( ini ).absolutePath() : clean( portableBase );
	auto dir = [&]( const char * key, const char * def ) {
		QString v = mo2Value( lines, QLatin1String( key ) );
		v.replace( QLatin1String( "%BASE_DIR%" ), m.base, Qt::CaseInsensitive );
		return v.isEmpty() ? m.base + QLatin1Char( '/' ) + QLatin1String( def ) : clean( v );
	};
	const QString profiles = dir( "profiles_directory", "profiles" );
	m.mods = dir( "mod_directory", "mods" );
	QString sel = mo2Value( lines, QStringLiteral( "selected_profile" ) );
	if ( sel.isEmpty() )
		sel = QStringLiteral( "Default" );
	m.profile = profiles + QLatin1Char( '/' ) + sel;
	if ( !QFileInfo( m.profile + QStringLiteral( "/modlist.txt" ) ).isFile() )
		return false;
	*out = m;
	return true;
}

//! every Fallout 4 MO2 instance on this machine: global ones, then portable ones beside the game
QList<Mo2Instance> findInstances( const QString & game )
{
	QList<Mo2Instance> out;
	const QString local = clean( QString::fromLocal8Bit( wwEnv( "LOCALAPPDATA" ) ) );
	if ( !local.isEmpty() ) {
		const QDir mo( local + QStringLiteral( "/ModOrganizer" ) );
		for ( const QString & sub : mo.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name ) ) {
			Mo2Instance m;
			if ( readInstance( mo.filePath( sub + QStringLiteral( "/ModOrganizer.ini" ) ), QString(), &m ) )
				out << m;
		}
	}
	if ( !game.isEmpty() ) {
		QStringList dirs{ game };
		const QDir g( game );
		for ( const QString & sub : g.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name ) )
			dirs << g.filePath( sub );
		const QDir up( QFileInfo( game ).absolutePath() );
		for ( const QString & sub : up.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name ) )
			dirs << up.filePath( sub );
		for ( const QString & d : dirs ) {
			const QString ini = d + QStringLiteral( "/ModOrganizer.ini" );
			Mo2Instance m;
			if ( QFileInfo( ini ).isFile() && readInstance( ini, d, &m ) ) {
				bool dup = false;
				for ( const Mo2Instance & o : out )
					dup = dup || sameDir( o.profile, m.profile );
				if ( !dup )
					out << m;
			}
		}
	}
	return out;
}

bool fromMo2( const QString & profile, const QString & mods, const QString & dataDir, const QString & label,
	CellModSource * s )
{
	LodgenLoadOrder lo;
	QString err;
	if ( !lodgenLoadOrderFromMo2( profile, mods, dataDir, &lo, &err ) ) {
		s->error = QStringLiteral( "MO2 profile %1: %2" ).arg( QDir::toNativeSeparators( profile ), err );
		return false;
	}
	s->kind = QStringLiteral( "mo2" );
	s->label = label;
	s->from = lo.profileDir;
	s->plugins = lo.plugins;
	for ( int i = 0; i < lo.plugins.size(); i++ ) {
		const QString f = i < lo.pluginFrom.size() ? lo.pluginFrom.at( i ) : QString();
		s->pluginFrom << ( f.isEmpty() || f == QLatin1String( "data" ) ? QStringLiteral( "Data" ) : f );
	}
	s->stack = lo.stack;
	/* Fallout4.ccc's files load under MO2 too: MO2 forces them on and never
	 * writes them to plugins.txt; the reader puts them after the masters. */
	s->alwaysLoads = lo.masters;
	if ( wwEnv( "WW_MODPICK_RED" ) == "noccc" )
		s->alwaysLoads = 0;  // WW_MODPICK_RED=noccc: the Creation Club files unticked
	return true;
}

//! the folder the game's own Fallout4.ini and Fallout4Custom.ini sit in
QString myGames()
{
	const QString o = clean( QString::fromLocal8Bit( wwEnv( "WW_MODPICK_MYGAMES" ) ) );  // a test fixture
	if ( !o.isEmpty() )
		return o;
	return clean( QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) ) + QStringLiteral( "/My Games/Fallout4" );
}

/* sTestFile1..10 in [General]: when any is set the game loads neither
 * Fallout4.ccc nor plugins.txt, only the masters and these files. A later ini
 * overrides a key; `from` names the last ini that set one. */
QStringList testFiles( const QStringList & inis, QString * from )
{
	QStringList val( 10 );
	for ( const QString & ini : inis ) {
		bool general = false;
		for ( const QString & raw : readLines( ini ) ) {
			const QString l = raw.trimmed();
			if ( l.startsWith( QLatin1Char( '[' ) ) ) {
				general = l.compare( QLatin1String( "[General]" ), Qt::CaseInsensitive ) == 0;
				continue;
			}
			if ( !general || !l.startsWith( QLatin1String( "sTestFile" ), Qt::CaseInsensitive ) )
				continue;
			const int eq = l.indexOf( QLatin1Char( '=' ) );
			bool ok = false;
			const int n = eq > 9 ? l.mid( 9, eq - 9 ).trimmed().toInt( &ok ) : 0;
			if ( !ok || n < 1 || n > 10 )
				continue;
			val[n - 1] = l.mid( eq + 1 ).trimmed();
			*from = clean( ini );
		}
	}
	QStringList out;
	for ( const QString & v : val )
		if ( !v.isEmpty() && !out.contains( v, Qt::CaseInsensitive ) )
			out << v;
	return out;
}

/*! the folder the game's inis are read from: the MO2 profile's own when it has local inis (a Fallout4.ini or
 *  Fallout4Custom.ini in it), else My Games; `local` says which (lane LANDSPEC9: shared by sTestFile and uGridsToLoad) */
QString iniFolder( const QString & profile, bool * local = nullptr )
{
	const bool p = !profile.isEmpty() && ( QFileInfo( profile + QStringLiteral( "/Fallout4.ini" ) ).isFile()
		|| QFileInfo( profile + QStringLiteral( "/Fallout4Custom.ini" ) ).isFile() );
	if ( local )
		*local = p;
	return p ? profile : myGames();
}

//! the inis the game reads sTestFile from: the profile's own when it has them (MO2), else My Games
QStringList gameInis( const QString & profile )
{
	const QString d = iniFolder( profile );
	return { d + QStringLiteral( "/Fallout4.ini" ), d + QStringLiteral( "/Fallout4Custom.ini" ) };
}

/* The sTestFile exception: the list becomes the masters (no Creation Club
 * files) plus the test files, all forced on. `dirs` = where a file is looked
 * for, highest priority first. */
void applyTestFiles( CellModSource * s, const QStringList & inis, const QStringList & dirs, const QString & data )
{
	QString from;
	const QStringList names = testFiles( inis, &from );
	if ( names.isEmpty() || wwEnv( "WW_MODPICK_RED" ) == "notestfile" )
		return;  // WW_MODPICK_RED=notestfile: the ini ignored
	static const char * masters[] = { "Fallout4.esm", "DLCRobot.esm", "DLCworkshop01.esm", "DLCCoast.esm",
		"DLCworkshop02.esm", "DLCworkshop03.esm", "DLCNukaWorld.esm", "DLCUltraHighResolution.esm" };
	QStringList plugins, pluginFrom, missing;
	for ( const char * m : masters )
		for ( const QString & p : std::as_const( s->plugins ) )
			if ( QFileInfo( p ).fileName().compare( QLatin1String( m ), Qt::CaseInsensitive ) == 0 ) {
				plugins << p;
				pluginFrom << QStringLiteral( "Data" );
			}
	for ( const QString & n : names ) {
		bool have = false;
		for ( const QString & p : std::as_const( plugins ) )
			have = have || QFileInfo( p ).fileName().compare( n, Qt::CaseInsensitive ) == 0;
		if ( have )
			continue;
		QString found;
		for ( const QString & d : dirs )
			if ( found.isEmpty() && QFileInfo( d + QLatin1Char( '/' ) + n ).isFile() )
				found = QDir::cleanPath( d + QLatin1Char( '/' ) + n );
		if ( found.isEmpty() ) {
			missing << n;
			continue;
		}
		const QString dir = QFileInfo( found ).absolutePath();
		plugins << found;
		pluginFrom << ( sameDir( dir, data ) ? QStringLiteral( "Data" ) : QFileInfo( dir ).fileName() );
	}
	s->plugins = plugins;
	s->pluginFrom = pluginFrom;
	s->alwaysLoads = int( plugins.size() );
	s->label += QStringLiteral( " -- test files only (sTestFile in %1): no Fallout4.ccc, no plugins.txt" )
		.arg( QDir::toNativeSeparators( from ) );
	if ( !missing.isEmpty() )
		s->note = QStringLiteral( "%1 sTestFile entries not found: %2" ).arg( missing.size() )
			.arg( missing.join( QStringLiteral( ", " ) ) );
}

//! fromMo2, then the profile's sTestFile exception
bool fromMo2Tested( const QString & profile, const QString & mods, const QString & dataDir, const QString & label,
	CellModSource * s )
{
	if ( !fromMo2( profile, mods, dataDir, label, s ) )
		return false;
	QStringList dirs;
	for ( int i = int( s->stack.size() ) - 1; i >= 0; i-- )
		dirs << s->stack.at( i );
	applyTestFiles( s, gameInis( s->from ), dirs, s->stack.isEmpty() ? QString() : s->stack.first() );
	return true;
}

//! [Archive] values of the game's inis, the later file overriding a key
QStringList iniArchives( const QString & game )
{
	const QStringList inis{ game + QStringLiteral( "/Fallout4_Default.ini" ),
		myGames() + QStringLiteral( "/Fallout4.ini" ), myGames() + QStringLiteral( "/Fallout4Custom.ini" ) };
	const char * keys[] = { "sResourceIndexFileList", "sResourceStartUpArchiveList", "sResourceArchiveList",
		"sResourceArchiveList2" };
	QStringList val( 4 );
	for ( const QString & ini : inis ) {
		bool inArchive = false;
		for ( const QString & raw : readLines( ini ) ) {
			const QString l = raw.trimmed();
			if ( l.startsWith( QLatin1Char( '[' ) ) ) {
				inArchive = l.compare( QLatin1String( "[Archive]" ), Qt::CaseInsensitive ) == 0;
				continue;
			}
			if ( !inArchive )
				continue;
			for ( int k = 0; k < 4; k++ ) {
				const QString key = QLatin1String( keys[k] ) + QLatin1Char( '=' );
				if ( l.startsWith( key, Qt::CaseInsensitive ) )
					val[k] = l.mid( key.size() );
			}
		}
	}
	QStringList out;
	for ( const QString & v : val )
		for ( const QString & n : v.split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) )
			if ( !n.trimmed().isEmpty() && !out.contains( n.trimmed(), Qt::CaseInsensitive ) )
				out << n.trimmed();
	return out;
}

} // namespace

int cellGameGrids( QString * from, QString * read )
{
	QString gameFrom;
	const QString game = gameFolder( &gameFrom );
	/* lane LANDSPEC9: the open MO2 profile (cellModSource's, the mod picker's) with local inis is read in
	 * place of My Games, as the game run from that profile does; red mygames: My Games always (LANDSPEC8) */
	QString profile;
	if ( wwEnv( "WW_LANDSPEC_RED" ) != "mygames" ) {
		const CellModSource src = cellModSource();
		if ( src.kind == QLatin1String( "mo2" ) )
			profile = src.from;
	}
	bool local = false;
	const QString dir = iniFolder( profile, &local );
	QStringList inis{ dir + QStringLiteral( "/Fallout4.ini" ), dir + QStringLiteral( "/Fallout4Prefs.ini" ),
		dir + QStringLiteral( "/Fallout4Custom.ini" ) };
	if ( !game.isEmpty() && ( local || wwEnv( "WW_MODPICK_MYGAMES" ).isEmpty() ) )	// a My Games fixture stands alone
		inis.prepend( game + QStringLiteral( "/Fallout4_Default.ini" ) );
	if ( read )
		*read = QStringLiteral( "%1 %2" ).arg( local ? QStringLiteral( "the MO2 profile's own inis in" )
			: QStringLiteral( "My Games" ), QDir::toNativeSeparators( dir ) );
	int g = 0;
	for ( const QString & ini : inis ) {
		bool general = false;
		for ( const QString & l : readLines( ini ) ) {
			const int v = LandLodSpec::gridsLine( l.toStdString(), general );
			if ( v < 0 )
				continue;
			g = v;
			if ( from )
				*from = QDir::toNativeSeparators( clean( ini ) );
		}
	}
	return g;
}

int cellGameGridHalf( QString * note )
{
	QString from, read;
	const int g = cellGameGrids( &from, &read );
	const bool red = wwEnv( "WW_LANDSPEC_RED" ) == "grids5";
	const int h = LandLodSpec::gridHalf( red ? LandLodSpec::kGridsDefault : g );
	if ( note )
		*note = ( red ? QStringLiteral( "RED grids5: 5 (the ini says %1)" ).arg( g )
			: g > 0 ? QStringLiteral( "%1 (%2)" ).arg( g ).arg( from )
			: QStringLiteral( "unset, the game's %1" ).arg( LandLodSpec::kGridsDefault ) )
			+ QStringLiteral( "; read %1" ).arg( read );	// LANDSPEC9: which folder's inis
	return h;
}

QStringList cellModPluginArchives( const QString & pluginPath )
{
	const QFileInfo fi( pluginPath );
	const QString stem = fi.completeBaseName();
	const QDir d( fi.absolutePath() );
	QStringList out;
	for ( const QString & n : d.entryList( { stem + QStringLiteral( ".ba2" ), stem + QStringLiteral( " - *.ba2" ) },
			QDir::Files, QDir::Name ) )
		out << QDir::cleanPath( d.filePath( n ) );
	return out;
}

QStringList cellModDataStack( const QString & dataDir, const QStringList & plugins )
{
	const QString data = clean( dataDir );
	QStringList stack{ data };
	const QString game = QFileInfo( data ).absolutePath();
	for ( const QString & n : iniArchives( game ) ) {
		const QString p = data + QLatin1Char( '/' ) + n;
		if ( QFileInfo( p ).isFile() && !stack.contains( p, Qt::CaseInsensitive ) )
			stack << p;
	}
	for ( const QString & pl : plugins )
		for ( const QString & a : cellModPluginArchives( pl ) )
			if ( !stack.contains( a, Qt::CaseInsensitive ) )
				stack << a;
	return stack;
}

CellModSource cellModSource()
{
	CellModSource s;
	QString gameFrom;
	const QString game = gameFolder( &gameFrom );
	QStringList looked;
	const bool noMo2 = wwEnv( "WW_MODPICK_NOMO2" ) == "1";

	// 1. the LOD panel's MO2 profile
	const QString set = QSettings().value( QStringLiteral( "LodGeneration/mo2Profile" ) ).toString().trimmed();
	if ( noMo2 ) {
		looked << QStringLiteral( "MO2 hidden (WW_MODPICK_NOMO2)" );
	} else if ( !set.isEmpty() ) {
		const QString mods = QSettings().value( QStringLiteral( "LodGeneration/mo2Mods" ) ).toString().trimmed();
		if ( fromMo2Tested( set, mods, QString(), QStringLiteral( "the MO2 profile %1 (the LOD panel's)" )
				.arg( QFileInfo( clean( set ) ).fileName() ), &s ) )
			return s;
		looked << s.error;
	} else {
		// 2. an MO2 instance on this machine
		const QList<Mo2Instance> found = findInstances( game );
		int pick = -1;
		for ( int i = 0; i < found.size() && pick < 0; i++ )
			if ( sameDir( found.at( i ).game, game ) )
				pick = i;
		if ( pick < 0 && !found.isEmpty() )
			pick = 0;
		if ( pick >= 0 ) {
			const Mo2Instance & m = found.at( pick );
			const QString data = m.game.isEmpty() ? QString() : m.game + QStringLiteral( "/Data" );
			if ( fromMo2Tested( m.profile, m.mods, data, QStringLiteral( "the MO2 profile %1 (found: %2%3)" )
					.arg( QFileInfo( m.profile ).fileName(), QDir::toNativeSeparators( m.ini ) )
					.arg( found.size() > 1 ? QStringLiteral( ", 1 of %1 Fallout 4 instances" ).arg( found.size() ) : QString() ),
					&s ) )
				return s;
			looked << s.error;
		} else {
			looked << QStringLiteral( "no Mod Organizer instance for Fallout 4 (%LOCALAPPDATA%\\ModOrganizer, beside the game)" );
		}
	}
	s = CellModSource();
	if ( wwEnv( "WW_MODPICK_RED" ) == "nofallback" ) {
		s.error = QStringLiteral( "no MO2 profile (RED nofallback: the Data folder is not read)" );
		return s;
	}

	// 3. the game's own Data folder and plugins.txt
	if ( game.isEmpty() ) {
		s.note = QStringLiteral( "looked: %1; NifSkope's Fallout 4 game path (not set); the registry's Installed Path (none)" )
			.arg( looked.join( QStringLiteral( "; " ) ) );
		s.error = QStringLiteral( "no Fallout 4 install found -- %1" ).arg( s.note );
		return s;
	}
	const QString data = game + QStringLiteral( "/Data" );
	s.from = data;
	s.plugins = lodgenLoadOrderMasters( data );
	s.alwaysLoads = int( s.plugins.size() );  // the game loads Fallout4.ccc's files like the DLC masters
	if ( wwEnv( "WW_MODPICK_RED" ) == "noccc" )
		s.alwaysLoads = 0;  // WW_MODPICK_RED=noccc: the Creation Club files become ordinary rows again
	const QString ptxt = clean( QString::fromLocal8Bit( wwEnv( "LOCALAPPDATA" ) ) ) + QStringLiteral( "/Fallout4/plugins.txt" );
	const QStringList lines = readLines( ptxt );
	if ( !lines.isEmpty() ) {
		QStringList missing;
		for ( const QString & raw : lines ) {
			const QString l = raw.trimmed();
			if ( !l.startsWith( QLatin1Char( '*' ) ) )
				continue;
			const QString name = l.mid( 1 ).trimmed();
			const QString p = QDir::cleanPath( data + QLatin1Char( '/' ) + name );
			bool have = false;
			for ( const QString & q : std::as_const( s.plugins ) )
				have = have || QFileInfo( q ).fileName().compare( name, Qt::CaseInsensitive ) == 0;
			if ( have )
				continue;
			if ( QFileInfo( p ).isFile() )
				s.plugins << p;
			else
				missing << name;
		}
		s.kind = QStringLiteral( "data" );
		s.label = QStringLiteral( "the game's Data folder and plugins.txt (no Mod Organizer found)" );
		if ( !missing.isEmpty() )
			s.note = QStringLiteral( "%1 plugins.txt entries are not in Data: %2" ).arg( missing.size() )
				.arg( missing.mid( 0, 4 ).join( QStringLiteral( ", " ) ) + ( missing.size() > 4 ? QStringLiteral( " ..." ) : QString() ) );
	} else {
		// 4. nothing says which plugins: the masters in Data
		s.kind = QStringLiteral( "masters" );
		s.label = QStringLiteral( "the game's Data folder, masters only" );
		s.note = QStringLiteral( "looked: %1; %2 (%3); no plugins.txt at %4" ).arg( looked.join( QStringLiteral( "; " ) ),
			QDir::toNativeSeparators( data ), gameFrom, QDir::toNativeSeparators( ptxt ) );
	}
	for ( int i = 0; i < s.plugins.size(); i++ )
		s.pluginFrom << QStringLiteral( "Data" );
	applyTestFiles( &s, gameInis( QString() ), { data }, data );
	s.stack = cellModDataStack( data, s.plugins );
	return s;
}
